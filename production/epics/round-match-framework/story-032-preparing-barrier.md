# Story 032: Layout, a client acknowledgement or world-ready each alone hold Preparing; releasing reaches Active

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 8b; rules R6 (P4, P5, P7), R7.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, no host migration. ADR-0014: `UStealthLayoutService` world subsystem with provider registry, integer `FLayoutManifest` with `LayoutHash`, server navmesh built once and frozen at `OnLayoutReady`. This story applies the P4/P5/P7 barrier: Round waits on `OnLayoutReady(LayoutId, Seed)`, Session's single round acknowledgement (`OnParticipantTravelComplete`), and `OnParticipantWorldReady` (until `WorldReadyTimeoutSec`), each bounded by a server-measured clock.
**ADR Version**: ADR-0005 2026-10-24; ADR-0014 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk; ADR-0014 is MEDIUM)
**Engine Notes**: None touched in this test: the barrier runs against fake Session and layout gateways and the fake clock. The real wiring touches ADR-0005 E19 (remote controllers arrive through `ServerNotifyLoadedWorld` -> `HandleSeamlessTravelPlayer` after world `BeginPlay`) and the two-stage acknowledgement (`ServerRoundAck`, `ServerWorldReady(FAckEpoch)`), plus ADR-0014's `FOnLayoutReady(uint64 LayoutId, int32 Seed)` server delegate; verification of those lives with ADR-0005 S3 and the Session and Level Socket Contract epics, not here.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules, section Session and travel):
- Required: Every wait is bounded by a server-measured clock (ADR-0005).
- Forbidden: Never let a client message start, extend or restart a deadline (ADR-0005).
- Guardrail: Travel load is bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005); the layout service's own server-side budget is at most 45 s worst case including reseeds (ADR-0014).

**Prototype disposition**: Replaces the prototype readiness logic in `Source/StealthGame/Private/StealthGameMode.cpp` (`TryScheduleRoundStart`, `StartRound`); the production barrier waits on Session's single round acknowledgement, layout and world-ready.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 8b, scoped to this story:*

- [ ] GIVEN Preparing, WHEN any one of layout, a client's single round acknowledgement or P7 world-ready (until `WorldReadyTimeoutSec`) is withheld alone, THEN the phase stays `Preparing`; releasing it reaches `Active`. A client that misses Session's deadline leaves the wait set and Active is reached without it.

---

## Implementation Notes

*Derived from ADR-0002 section 8.6, ADR-0005 (round travel and the single acknowledgement) and ADR-0014 section 8.4, and GDD R6:*

- **P4 layout.** Wait for `OnLayoutReady(LayoutId, Seed)` with `Seed == FRoundParams.Seed`. `OnLayoutAborted` aborts `LayoutAborted` and a seed mismatch aborts `LayoutSeedMismatch` (both are story 034). Round reaches layout only through `UStealthLayoutService` and never includes Generator headers (ADR-0014 guideline 1).
- **P5 barrier.** Every `Connected` participant must have delivered Session's single round acknowledgement (`OnParticipantTravelComplete`, carrying `ClientHash` and `PoolHash`). Stale acknowledgements are dropped (story 033). There is no separate TDC or pool acknowledgement and no quorum abort. The only per-client bound is Session's F7 `Deadline`: a participant that misses it becomes `Disconnected(TravelTimeout)`, leaves the wait set and is a normal disconnect, so Active is reached without it. A participant that rejoins before P6 re-enters the wait set and must acknowledge. Travel-complete is never inferred from a PlayerState existing (PlayerStates survive travel, ADR-0005 E6).
- **P7 world-ready and scheduled start.** Each `Connected` client reports world-ready through the second stage of the acknowledgement (`ServerWorldReady`, forwarded as `OnParticipantWorldReady`). Round waits until every `Connected` client is world-ready or `WorldReadyTimeoutSec` (10 s) passes, then sets `ActiveAtServerTime = ServerNow() + StartLeadSec` (0.25 s). A client not ready at the timeout is not disconnected; the start proceeds. The second stage never fails and never disconnects. Behaviour past `WorldReadyTimeoutSec` is not part of this AC.
- **Release to Active.** When all three are satisfied and the F6 conditions hold, the next drain whose server time is `>= ActiveAtServerTime` enters `Active` (the Active entry step is story 042; the unbound-participant check is story 041).
- **Test shape.** Fakes for layout, client acknowledgement and P7 world-ready, advanced by the fake `IClock` (story 001): withhold each one alone while the other two are satisfied, assert the phase stays `Preparing`, then release and assert `Active`. Add the case in which a client misses Session's deadline and leaves the wait set.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 031 (`production/epics/round-match-framework/story-031-allocate-once.md`): P3 start work and the single deal (this story's dependency).
- Story 033 (`production/epics/round-match-framework/story-033-stale-ack-dropped.md`): old-nonce and replaced-controller acknowledgements.
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): the abort codes (`LayoutAborted`, `LayoutSeedMismatch`, `TeamAbsent`, `PreparingWatchdog`).
- Story 035 (`production/epics/round-match-framework/story-035-watchdog-bound.md`): the F6 watchdog bound.
- Story 041 (`production/epics/round-match-framework/story-041-active-entry-unbound-aborts.md`) and 042: Active entry.
- Story 091 (`production/epics/round-match-framework/story-091-scheduled-reveal.md`): the client-side scheduled reveal (blocked on the OQ11(b) spike).
- Session internals (the acknowledgement RPCs, F7 `Deadline`) and Level Socket Contract internals (layout building and validation).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Preparing/PreparingBarrier.h`
- `Source/StealthGameplay/Private/Round/Preparing/PreparingBarrier.cpp`
- `Source/StealthGameplay/Private/Tests/Round/PreparingBarrierTest.cpp` (class `FRoundPreparingBarrierTest`, test name `StealthGame.Round.PreparingBarrier`)
- `Source/StealthGame/Private/StealthGameMode.cpp` (prototype `TryScheduleRoundStart` readiness: retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Layout, a client's acknowledgement or world-ready each alone hold Preparing, and releasing reaches Active.
  - Given: Preparing with fakes for layout, client acknowledgement and P7 world-ready.
  - When: each of the three is withheld alone while the other two are satisfied, then released.
  - Then: the phase stays `Preparing` while any one is withheld (until `WorldReadyTimeoutSec`).
  - Then: releasing it reaches `Active`.
  - Then: a client that misses Session's deadline leaves the wait set and `Active` is reached without it.
  - Edge cases: each client acknowledges only once; the behaviour past `WorldReadyTimeoutSec` is not stated in this AC (an abort is covered by `PreparingWatchdog` in story 034).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/PreparingBarrierTest.cpp` (automation test `StealthGame.Round.PreparingBarrier`, class `FRoundPreparingBarrierTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.PreparingBarrier`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-031-allocate-once.md`
- Unlocks: `production/epics/round-match-framework/story-033-stale-ack-dropped.md`, `production/epics/round-match-framework/story-034-preparing-aborts.md`, `production/epics/round-match-framework/story-041-active-entry-unbound-aborts.md`
- Cross-epic: `production/epics/session-travel` (round acknowledgement, `OnParticipantTravelComplete`, `OnParticipantWorldReady`, F7 `Deadline`; stories not yet created), `production/epics/level-socket-contract` (`OnLayoutReady`, `OnLayoutAborted`, layout service; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (layout, one acknowledgement and world-ready each hold Preparing alone; release reaches Active; a client that misses Session's deadline leaves the wait set; one acknowledgement per client; world-ready timeout proceeds without disconnecting)
**Deviations**: Session's acknowledgement RPCs do not exist yet; in L_Prototype the composition-root adapter acknowledges every connected player (no travel) and reports world-ready on the tick after P6.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/PreparingBarrierTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: OBSERVED: production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
