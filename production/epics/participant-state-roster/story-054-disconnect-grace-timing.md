# Story 054: Disconnect grace expires on the injected clock and stale-connection timers are rejected

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: ADR-0005 makes every `Connection` change bump the participant's `ConnectionGeneration` (including a Lobby `Rejoining` projection) and requires Round's grace expiry to carry that generation as `ExpectedConnectionGeneration`; a kick that closes an `Expired` connection is a legal Disconnect. Session reports facts and Round decides: Session never calls `RequestTransition`. ADR-0002 schedules the grace expiry as a `TimerFired(GraceExpiry)` item in the due-time heap (never `FTimerManager`), stamps every timed item with an `FEpochToken`, and resolves a stale token as `Rejected_StaleEpoch` with nothing changed.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No new post-cutoff APIs; the 5.6 `DispatchPostLogin` deprecation applies to login overrides (use `PostLogin`/`OnPostLogin`); `UE_LOGF` for logging. Verification required: S3 (seamless and hard travel), S4 (reconnect: retained PlayerState re-attached, two clients on one machine, supersede), S5 (listen-host loss). This story is a stub-Round core test on an injected millisecond clock, so none of those spikes gate it.

**Control Manifest Rules (this layer)**:
- Required: gameplay timers are `ScheduleAt` items in the due-time heap — source: ADR-0002
- Required: every wait is bounded by a server-measured clock; ack records, kick list, silence clocks and connection maps are keyed by `ParticipantId` and never replicated — source: ADR-0005
- Required: queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores with no `UWorld` dependency; the clock is injected (server clock in integer milliseconds) — source: ADR-0007
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer — source: ADR-0002
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` — source: ADR-0005
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line — source: ADR-0005

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-32** [Int][Dep:Round] `StealthGame.Roster.DisconnectGraceTiming` (Round, injected ms clock):
  - GIVEN a drop at 100 000 ms during Active, WHEN the player rejoins at 129 900 ms, THEN they are Active.
  - WHEN there is no rejoin, THEN at 130 000 ms the transition is `Applied`, `ReleaseToCrowd` runs in the same call stack, and the pawn resolves `Npc`.
  - WHEN the drop is before `ActivePhaseStart`, THEN expiry = `ActivePhaseStartMs + 30 000`.
  - WHEN the player rejoins at 129 900 ms and drops again at 129 950 ms, THEN the first timer's request returns `Rejected_StaleConnection` and the state stays Active until 159 950 ms (F6).

---

## Implementation Notes

*Derived from ADR-0005 section 5, ADR-0002 section 4, and GDD F6 and R8, R11 ("Expiry", "Rejoin"):*

- F6: `AcceptDisconnect(p) = p.Connection = Disconnected AND p.State = Active AND ExpectedEpoch = (MatchId, RoundEpoch) AND ExpectedConnectionGeneration = p.ConnectionGeneration`; Round's provisional timing in integer milliseconds is `GraceExpiryMs(p) = max(p.DisconnectedAtMs, ActivePhaseStartMs) + G_eff_Ms`, with `G_eff = min(DisconnectGraceSec, RejoinWindowSec - 30)` (Round F4); the timer fires when `NowMs >= GraceExpiryMs`. The test uses `G_eff = 30 000 ms` (default `DisconnectGraceSec = 30`, `RejoinWindowSec` 120).
- The four scenarios of the AC map to the F6 worked example: (1) drop at 100 000 ms (generation 1), rejoin at 129 900 ms: the participant stays Active (rejoin and expiry in the same tick resolve rejoin-first because Session processes receives before window expiry, so the expiry would return `Rejected_NotDisconnected`); (2) no rejoin: at 130 000 ms the stub Round's timer requests `Active -> Informant (Disconnect)` with the captured `ExpectedEpoch` and `ExpectedConnectionGeneration`, the result is `Applied`, and in the same call stack Round calls `ReleaseToCrowd` so the pawn resolves `Npc` (R11: a rejoin can never land in between); (3) a drop before `ActivePhaseStart` expires at `ActivePhaseStartMs + 30 000`; (4) rejoin at 129 900 ms then drop again at 129 950 ms (generation 2): the first timer at 130 000 ms returns `Rejected_StaleConnection`, the state stays Active, and only the new timer, expiring at 159 950 ms, applies.
- Roster rules asserted (R8 precedence steps 9 and 10): `Rejected_NotDisconnected` when Reason is `Disconnect` and `Connection = Connected`; `Rejected_StaleConnection` when `ExpectedConnectionGeneration` is missing or differs. A later `ResetAll` would make the request `Rejected_StaleEpoch` (Story 024 covers it; do not duplicate).
- [Dep:Round]: use a stub Round grace timer driven by the injected millisecond clock until `production/epics/round-match-framework/EPIC.md` lands; the stub implements the `max(DisconnectedAtMs, ActivePhaseStartMs) + G_eff` formula, so the "drop before `ActivePhaseStart`" case checks the stub's formula against the GDD example and the Roster's accept/reject only. Real timer scheduling through the resolution queue is Round's and ADR-0002's; the shared stub is `StealthRosterRoundStub.h` (Story 052).
- The reclaim path (`RegisterParticipant` with a known key returning `Reclaimed`, Story 029) and the connection projection with generation bump on every entry into `Disconnected` (Story 009) are the inputs; the supersede and kick generation rules (R11, D24, D9) are not re-tested here.
- Prototype disposition: no prototype counterpart (the prototype had no grace window; a dropped player's body was simply destroyed).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029 (Reclaim restores the same record for active and informant participants): the reclaim core.
- Story 024 (Calls carrying an old round epoch or match id are rejected): `Rejected_StaleEpoch`.
- Story 015 (A Disconnect transition is rejected while the participant is connected): the core `Rejected_NotDisconnected` case in isolation.
- Story 009 (Session connection states project onto Connection fields only): projection and generation bump.
- Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): real multi-process rejoin.
- Story 051 (A teammate's reconnecting flag and end cause follow the event-only clear rules): the teammate flag.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterDisconnectGraceTimingTest.cpp` (class `FRosterDisconnectGraceTimingTest`, test name `StealthGame.Roster.DisconnectGraceTiming`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRoundStub.h` (stub Round grace timer on the injected clock; created by Story 052, extended here)
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (injected server clock interface; read-only here)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if `Rejected_StaleConnection` handling is incomplete; shared file)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: disconnect grace expires on the injected clock; stale timers are rejected.
  - Given: a stub Round grace timer, injected ms clock, G_eff 30,000, a Disconnected participant.
  - When: a drop at 100,000 ms and rejoin at 129,900 ms; no rejoin until 130,000 ms; a drop before ActivePhaseStart; a rejoin at 129,900 ms with a second drop at 129,950 ms.
  - Then: the first case stays Active. At 130,000 ms the transition is Applied and ReleaseToCrowd runs in the same call stack so the pawn resolves Npc. The pre-start case expires at ActivePhaseStartMs + 30,000. In the last case the first timer returns Rejected_StaleConnection and the state stays Active until 159,950 ms.
  - Edge cases: rejoin wins over expiry in the same tick.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthRosterDisconnectGraceTimingTest.cpp` (`StealthGame.Roster.DisconnectGraceTiming`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-029-reclaim-core.md`, `production/epics/participant-state-roster/story-024-stale-epoch-rejected.md`
- Cross-epic: `production/epics/round-match-framework/EPIC.md` ([Dep:Round]); the test runs against a stub Round until it lands
- Unlocks: `production/epics/participant-state-roster/story-059-mp-rejoin-within-grace.md`
