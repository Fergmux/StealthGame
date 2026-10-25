# Story 071: A stale-generation grace timer is Rejected_StaleConnection and logged Voided, and the matching one converts

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 49; rules R17.2 (grace record with `ExpectedEpoch` and `ExpectedConnectionGeneration`), R17.4 (stale answers are logged `Voided(TimerFired, code)`, never `GraceExpired`), R17.3.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, with Roster `ConnectionGeneration` identifying one connection of a participant. ADR-0002: one total-order queue per round, due-time heap for timers, resolvers that re-validate at their slot. This story applies ADR-0002 section 8.7 (4): the grace timer is guarded by the epoch token and the captured generation; a stale answer changes nothing and is logged `Voided(TimerFired, code)`.
**ADR Version**: ADR-0005 2026-10-24; ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. The test uses the plain-C++ queue core with a fake clock and a fake Roster; no engine API, post-cutoff API or verification item (spikes S3, S4, S5; ADR-0002 [U] items) is exercised.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules):
- Required: `FRosterCore::RequestTransition` applies R8 result precedence in order (`NotAuthority`, `WrongThread`, `UnknownParticipant`, `StaleEpoch`, `BatchOnlyReason`, `IllegalReason`, `NoOp_AlreadyInState`, `IllegalTransition`, `NotDisconnected`, `StaleConnection`, `Applied`); a stale grace expiry changes nothing and is logged `Voided(TimerFired, code)`, never `GraceExpired` (ADR-0005, ADR-0002).
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer (ADR-0002).
- Guardrail: Round Event Log: entry 64-96 B, about 400 KB at the default 4,096 entries (ADR-0002); `Voided` entries are rate-limited to one per source per second.

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 49, scoped to this story:*

- [ ] GIVEN a disconnect at generation g, a rejoin and a second disconnect, WHEN the first timer fires, THEN `Rejected_StaleConnection`, logged `Voided` (not `GraceExpired`), the participant stays Active, and the second timer converts at `DisconnectedAt2 + G_eff`; a matching generation gives `Applied`; a stale epoch is rejected before the generation is checked.

---

## Implementation Notes

*Derived from GDD R17.2 to R17.4 and ADR-0002 section 8.7 (2), (3) and (4):*

- **Record and item.** The grace timer is the queue item `TimerFired(GraceExpiry)` (`Subject` = participant, `Generation` = the Roster `ConnectionGeneration` captured when the timer starts, `Expected` = the round epoch token), scheduled with `ScheduleAt(GraceExpiry, ...)`. Round also keeps the server record `{P, GraceExpiry, ExpectedEpoch = (MatchId, RoundEpoch), ExpectedConnectionGeneration, Kicked}`, which the resolver reads. At most one timer exists per participant.
- **The sequence under test.** Disconnect at generation g starts timer 1. A rejoin inside grace cancels it (story 061); a second disconnect later starts timer 2 with the generation current then (g+1 as the Roster bumped it). In the test, timer 1 is made to fire anyway (a stale timer whose cancellation raced, which the guard exists for).
- **Stale generation.** At timer 1's slot the resolver calls `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)` with generation g; the Roster answers `Rejected_StaleConnection`; Round changes nothing, the participant stays `Active`, and the Event Log gets `Voided(TimerFired, StaleConnection)` through `IRoundLogWriter::Append`, never `GraceExpired`. The `Voided` rate limit (story 015) applies.
- **Matching generation.** Timer 2 fires at `DisconnectedAt2 + G_eff`, the request carries the current generation, the Roster returns `Applied`, `ReleaseToCrowd` runs in the same call stack and R15 is evaluated on the event.
- **Epoch before generation.** An item whose epoch token is stale (`RoundEpoch` incremented by `ResetAll`) must be rejected before the generation is looked at: the queue's epoch check on `FQueueItem.Expected` returns `Rejected_StaleEpoch`, and in the Roster `StaleEpoch` precedes `StaleConnection` in R8 precedence. Give it its own case (an item carrying both a stale epoch and a stale generation yields `Rejected_StaleEpoch`).
- **Other stale answers.** `Rejected_NotDisconnected` and `NoOp_AlreadyInState` follow the same logged-no-op rule (R17.4); only the two required by the AC are asserted here.
- **Helper from story 060.** Reuse the grace record and resolver from story 060; this story adds no new timer kind.
- Manifest 2026-10-25f: R8 precedence is applied in the manifest's stated order and `RequestTransition` is Roster's only `State` writer, so a stale grace expiry is judged only by the returned code.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and resolver (this story's dependency).
- Story 061 (`production/epics/round-match-framework/story-061-rejoin-inside-grace.md`): the rejoin that cancels the timer.
- Story 010 (`production/epics/round-match-framework/story-010-stale-epoch-item.md`): the queue-level `Rejected_StaleEpoch` for items stamped before `ResetAll`.
- Story 015 (`production/epics/round-match-framework/story-015-voided-rate-limit.md`): the one-per-second `Voided` limit and kill timeline protection.
- Roster internals (generation bump, R8 precedence) and Session's connection-generation source.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Round/Disconnect/GraceExpiryResolver.cpp` (created by story 060; extended here only if the guard is not already complete)
- `Source/StealthGameplay/Private/Tests/Round/GraceGenerationGuardTest.cpp` (class `FRoundGraceGenerationGuardTest`, test name `StealthGame.Round.GraceGenerationGuard`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A stale-generation grace timer is Rejected_StaleConnection and logged Voided, and the matching one converts.
  - Given: a disconnect at generation g, a rejoin and a second disconnect.
  - When: the first timer fires, then the second.
  - Then: the first is `Rejected_StaleConnection`, logged `Voided` (not `GraceExpired`), and the participant stays Active.
  - Then: the second converts at `DisconnectedAt2 + G_eff`, `Applied`.
  - Then: a stale epoch is rejected before the generation is checked.
  - Edge cases: the epoch-before-generation order has its own case.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/GraceGenerationGuardTest.cpp` (automation test `StealthGame.Round.GraceGenerationGuard`, class `FRoundGraceGenerationGuardTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (`RequestTransition`, `ConnectionGeneration`; stories not yet created), `production/epics/session-travel` (connection facts; stories not yet created)
