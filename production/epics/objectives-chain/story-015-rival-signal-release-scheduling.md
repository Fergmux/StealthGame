# Story 015: Rival signal release scheduling and idle behaviour

> **Epic**: Objectives Chain
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-28

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-12 steps 3-5, SP-3, SP-5, SP-6; Edge Cases on releases and the latch; AC-OC-09, AC-OC-16)
**Requirement**: `TR-oc-006` (`RivalPrelimCount` releases), `TR-oc-007` (stall pressure: no timer, no reveal while idle)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 7 steps 3-6)
**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging (`ScheduleAt`, `TimerFired`, `FTimerPayload`, `Rejected_Latched`, epoch drop on Reset)
**ADR Decision Summary**: One `ScheduleAt(t_rel_ms / 1000.0, TimerFired{StageSignal, ...}, Expected = round epoch)` item is scheduled per pending release, as ADR-0002 section 2a's `FTimerPayload {Kind = StageSignal, Subject = none, Generation = 0, Seq = k, Phase = viewing team}` (no court is named). Nothing is scheduled when nothing is pending. The `StageSignal` resolver increments the viewing team's released count, appends the server-only `StageSignalReleased(viewingTeam, count)` and writes that team's view; two releases due on one grid tick resolve in the same drain and reach clients as one change. After the latch the queue returns `Rejected_Latched` for pending releases; `Reset` drops them by epoch; `OnRosterReset` clears the pending list. `bRivalSignalEnabled == false`, or a 1v0 dev round, schedules nothing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 MEDIUM; logic is plain C++ over a stub queue)
**Engine Notes**: The resolver is registered by the composition root with `RegisterTimerResolver(ETimerKind::StageSignal, ...)` (story 005). The payload mapping is the Technical Setup 2026-10-25 / review N3 correction: `Subject` = none, `Phase` = viewing team (the opponent of the completer), superseding the earlier "Subject = court fixture" wording.

**Control Manifest Rules (this layer)**:
- Required: `TimerFired(StageSignal)` fills `FTimerPayload` as `Subject` = none (no court), `Generation` = 0, `Seq` = round-global ordinal `k`, `Phase` = viewing team — source: ADR-0020, ADR-0002
- Forbidden: Never give Objectives a tick function, `FTimerManager` timer or latent action (only `TimerFired(StageSignal)` items); never reschedule a `Rejected_Latched`/`Rejected_StaleEpoch` item — source: ADR-0020, ADR-0016
- Forbidden: Never use per-team timers, a fixed delay or the engine timer manager for the rival signal — source: ADR-0020
- Guardrail: at most about 4 releases per round, no steady-state cost — source: ADR-0020

The count is what the signal is: `RivalPrelimCount` in {0, 1, 2}, the number of the opponent's counted preliminaries that have been released. It only rises, may rise by 2 in one tick, carries no identity, site, order, time or sub-stage, and is never published after the latch. The idle rule is the stall-pressure rule (SP-3, SP-6): with nothing pending no Objectives timer exists, so waiting changes nothing. This story is the scheduler and resolver around story 014's pure draw.

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-09 remaining clauses, AC-OC-16) and ADR-0020 section 7 steps 3-6, scoped to this story:*

- [x] **AC1** (AC-OC-09 [U] `RivalSignalReleaseBehaviour`): **GIVEN** team 1's second completion draws a delay that releases it first or on the same tick **THEN** the count rises 0 to 1 to 2 or 0 to 2 and never falls; the replicated field carries only the integer; a release due after the latch is never published; with `bRivalSignalEnabled` false or in a 1v0 dev round nothing is published and no `StageSignal` timer is scheduled (same-tick releases publish once).
- [x] **AC2** (AC-OC-16 [U] `WaitingEarnsNothing`): **GIVEN** a round where no pawn tends **WHEN** the fake clock runs 10 x `TargetRoundMinutes` **THEN** no Objectives state, team view or `RivalPrelimCount` changes, no Objectives timer exists (a release timer exists only while a release is pending, and none is) and the round is still `Active`, in coarse and in fine clock steps.
- [x] **AC3** (ADR section 7 steps 3-5 [U] `StageSignalPayloadAndLatch`): the scheduled `TimerFired` payload is `Subject` none, `Generation` 0, `Seq` k, `Phase` = viewing team (the completer's opponent); the resolver increments that team's released count, appends server-only `StageSignalReleased(viewingTeam, count)` and writes the view; after the latch the queue returns `Rejected_Latched` for a pending release; `OnRosterReset` clears the pending list; the count never exceeds 2 (a third release attempt trips the assertion); `Seq` k is distinct per release.

---

## Implementation Notes

*Derived from ADR-0020 Decision section 7 and ADR-0002:*

On each counted preliminary completion (the stage branch of the gate, story 008) the core calls `ScheduleRivalRelease(CompleterTeam, k, t_c)`: increment the round-global ordinal `k`, draw `Delta_ms` and compute `t_rel_ms` with story 014's functions, `ScheduleAt(t_rel_ms / 1000.0, TimerFired{StageSignal, ViewingTeam = opponent, Ordinal = k}, Expected = round epoch)` via `IQueueTimers`, and record the release as pending. One queue item per pending release; nothing is scheduled when nothing is pending, and no repeating timer exists (the GDD "No clock" rule). The final is never counted in the signal.

The resolver (`IQueueResolver` for `ETimerKind::StageSignal`) runs in its own slot: increment the viewing team's released count (asserting it never exceeds 2), remove the pending record, append the server-only `StageSignalReleased(viewingTeam, count)` through the Round Event Log `Append`, and `PushTeamView(viewingTeam)`. Releases are unordered, so there is no `max()` coupling between a team's two steps; whichever releases first raises the opponent's count to 1 and the other to 2. If both of a team's releases fall on the same grid tick the count goes 0 to 2 in one update (one UI tick, OBJ-V2 in story 011). A `TimerFired` already queued at the latch returns `Rejected_Latched` and nothing is published in `Ended` except through the Policy's `PostRound` release; never reschedule it.

`bRivalSignalEnabled` false or a 1v0 dev round (`bAllowEmptyTeamDev` with an empty team): schedule nothing, publish nothing, never write `RivalPrelimCount` (it stays 0). Seeds that make the second completion release first (or on the same tick) are a test setup step: search the pinned seed list for them and pin the pair.

AC2 runs with a stub timer manager to prove the absence of any timer; a pending release would make the assertion differ and is absent in that scenario. The "10 x `TargetRoundMinutes`" figure uses the non-binding Round tuning key (not read by Objectives) only as the length of the fake-clock run.

Prototype replaced: none (no rival signal existed).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-014-rival-signal-draw-and-grid.md`: the draw and grid arithmetic.
- `story-011-team-view-component-and-replication.md`: the one-update arrival at the client (OBJ-V2).
- `story-019-pie2-team-view-ingress-and-informant-parity.md`: informant parity of a release (AC-OC-33).
- Round epoch: the queue, latch and `Rejected_Latched` behaviour itself.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Features/Objectives/Private/ObjectivesService.cpp, Public/Server/ObjectivesService.h` (the service is the scheduler: ScheduleAt TimerFired{StageSignal, Subject none, Generation 0, Seq k, Phase viewing team}; Resolve; 1v0 suppression; test seam ScheduleRivalReleaseNowForTest)
- `Source/StealthGameplay/Features/Objectives/Private/ObjectivesCore.h / .cpp` (pending list, ResolveRivalRelease with the <= 2 invariant, StageSignalReleased)
- `Source/StealthGame/Private/Composition/StealthCompositionRoot.cpp` (RivalRelease left to the service; SecretDraw / Clues wired)
- `Source/StealthGameplay/Private/Tests/Objectives/RivalSignalReleaseBehaviourTest.cpp, WaitingEarnsNothingTest.cpp, StageSignalPayloadAndLatchTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; finding seeds that reverse release order is a setup step). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`RivalSignalReleaseBehaviour`): count monotonic, integer only
  - Given: stub Round queue and fake clock; pinned seeds chosen so team 1's second completion releases first or on the same tick; observer on the replicated count
  - When: both completions release; a release falls due after the latch; `bRivalSignalEnabled` false; 1v0 dev round
  - Then: count goes 0 to 1 to 2 or 0 to 2 and never falls; the replicated field is the integer only; a post-latch release is never published; with the flag false or in 1v0 nothing is published and no `StageSignal` timer is scheduled
  - Edge cases: finding the seeds that reverse release order is a setup step; same-tick releases publish once.
- **AC-2** (`WaitingEarnsNothing`): idle round changes nothing
  - Given: Active round, no pawn tends; fake clock; stub timer manager
  - When: the clock runs 10 x `TargetRoundMinutes`
  - Then: no Objectives state, team view or `RivalPrelimCount` change; no Objectives timer exists; the round is still Active
  - Edge cases: run in coarse and fine steps; a pending release would make the timer assertion differ and is absent here.
- **AC-3** (`StageSignalPayloadAndLatch`): payload and resolver
  - Given: team 1 completes Prelim1 (ordinal k); stub queue
  - When: the release is scheduled, fires, and in a second case fires after the latch; `OnRosterReset` with one pending
  - Then: `TimerFired` payload Subject none, Generation 0, Seq k, Phase = viewing team (the completer's opponent); resolver increments that team's count, appends server-only `StageSignalReleased(viewingTeam, count)`, writes the view; after the latch the queue returns `Rejected_Latched`; reset clears the pending list; count never exceeds 2 (assert)
  - Edge cases: a third release attempt trips the assertion; Seq k distinct per release.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (2026-10-28): StealthGame.Objectives.RivalSignalReleaseBehaviour, WaitingEarnsNothing, StageSignalPayloadAndLatch; evidence production/qa/evidence/objectives-chain-013-023-2026-10-28/

Run result: OBSERVED - "Rival steps: 1 of 2" appears on the rival team's client at a grid tick after a counted preliminary (captures in production/qa/evidence/objectives-chain-013-023-2026-10-28/)

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-004-core-site-state-and-progress.md`, `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`, `production/epics/objectives-chain/story-014-rival-signal-draw-and-grid.md`; cross-epic: round-match-framework (`ScheduleAt`, `RegisterTimerResolver`, latch, Event Log; stories being written in parallel)
- Unlocks: `production/epics/objectives-chain/story-018-stall-pressure-and-cross-system-checks.md`, `production/epics/objectives-chain/story-019-pie2-team-view-ingress-and-informant-parity.md`, `production/epics/objectives-chain/story-023-playtest-advisory-metrics.md`

## Completion Notes
**Completed**: 2026-10-28
**Criteria**: AC1-AC3 passing (on the real queue). RivalPrelimCount is now written for real (it was HOLD at 0 since story 010).
**Deviations**: a 1v0 dev round is detected by the service at Active entry (one team with members, the other none); it suppresses the signal and appends ObjectivePacing(Code 1) so story 017's F5 calculator excludes the round.
**Test Evidence**: production/qa/evidence/objectives-chain-013-023-2026-10-28/
**Reviewed by**: self code review 2026-10-28 (notes in the evidence)
