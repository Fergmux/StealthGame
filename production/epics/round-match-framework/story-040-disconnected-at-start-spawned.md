# Story 040: A participant Disconnected at P6 gets a bound, unpossessed, AI-driven pawn and grace measured from ActivePhaseStart

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 9; rules R6 ("Every Active participant gets a bound pawn, connected or not" and the spawn gate), R17.2 and R17.7, F4.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (also ADR-0005: Session, seamless travel and match-scoped state stores)
**ADR Decision Summary**: One `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator/possessor, kind-blind possession and hand-over pass, disconnect never destroys a pawn. This story applies ADR-0010's rule that the factory forces `ControlKind = Ai` (and reports to the fault sink if `Player` was asked) for a participant Session has not moved to `Connected` for this round, so that pawn is possessed later only by Round's hand-over pass; and ADR-0005's rule that Session reports connection facts while Round decides the consequence. The grace timer itself is the queue item `TimerFired(GraceExpiry)` of ADR-0002 section 8.7.
**ADR Version**: ADR-0010 2026-10-24; ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 knowledge risk; ADR-0005 is also HIGH)
**Engine Notes**: No engine API is called in this [U] test: the P6 spawn goes through the story 003 fake gateways and the fake clock, with no connection and no `UWorld`. The production path it specifies touches ADR-0010 items that stay [U] until their spikes report: `SpawnSharedPawn` inside `TG_PostPhysics` (ADR-0002 E4, verified with ADR-0010), S8 (`PawnLeavingGame` hand to a crowd controller) and H1 (AI to player hold). Per-connection relevancy (the spawn gate) is ADR-0003's and is asserted here only through the gateway call or a pawn flag, never through a real connection.

**Control Manifest Rules (this layer)** (the Round-relevant rules for this story sit in the Core Layer Rules, Shared Pawn and possession, and the Foundation Layer Rules, Round phase flow):
- Required: `UStealthPawnFactory` forces `ControlKind = Ai` (and reports to the fault sink if `Player` was asked) for a participant Session has not moved to `Connected` for this round; such a pawn is possessed later only by Round's hand-over pass (ADR-0010, ADR-0014).
- Forbidden: Never trust a caller's `ControlKind = Player` for a participant that is not `Connected` (ADR-0010).
- Guardrail: the listen-host server frame stays at or below 1/60 s at the profiled load (ADR-0002, ADR-0003); this [U] story adds no per-frame work, and the one-frame P6 spawn cost is measured in story 039.

**Prototype disposition**: None for the disconnect and grace policy (no prototype equivalent; the prototype has no disconnect handling, grace clock or bound-pawn AI drive). The prototype's round-start spawn path in `Source/StealthGame/Private/StealthGameMode.cpp` is replaced by story 039's factory path, not by this story.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 9, scoped to this story:*

- [ ] GIVEN a participant `Disconnected` at P6 (fake clock), THEN its pawn exists, is bound, unpossessed, AI-driven and not net-relevant to its connection; with `DisconnectedAt` before `ActivePhaseStart` the grace expires at `ActivePhaseStart + G_eff`; with `DisconnectedAt = ActivePhaseStart + 10` at `ActivePhaseStart + 10 + G_eff`.

---

## Implementation Notes

*Derived from ADR-0010 Implementation Guidelines, ADR-0002 section 8.7 and GDD R6, R17, F4:*

- **Every Active participant gets a bound pawn, connected or not (R6).** A participant that is `Disconnected` at P6 (a load timeout, a crash during travel) still gets a pawn, placed and bound like everyone else and AI-driven by the Crowd controller exactly as in a mid-round disconnect. This keeps the spawn indistinguishable (Pillar 1: no pawn missing or late) and lets a quick rejoin repossess it. `Players` for the allocator and for P6 is every rostered `ParticipantId`, connected or not.
- **Spawn sequence (ADR-0010).** The P6 step calls `UStealthPawnFactory::SpawnSharedPawn`: deferred spawn, `ApplyAppearance`, `BindPawn` (for a participant), `FinishSpawning`, possess, in one call stack. For a participant not `Connected` Round passes `ControlKind = Ai` (it never passes `Player`), and the factory also forces `Ai` and reports to the fault sink if `Player` was asked. The pawn is bound (`BindPawn` accepted) and unpossessed by any human controller; it is driven by a Crowd controller. Round never calls `Possess`, `RestartPlayer` or `SpawnDefaultPawnFor` itself.
- **Not net-relevant to its connection (spawn gate, R6 and Level Socket Contract R4.8).** No pawn is spawned for, possessed by or made relevant to a client before Session has moved that participant to `Connected` for this round. The relevancy mechanism is ADR-0003's per-connection spawn gate (`AStealthPawn::IsNetRelevantFor`). In [U] there is no connection, so assert the gate through the gateway call (the factory fake records `ControlKind` and the gate-closed request) or a pawn flag. Open: confirm against ADR-0003 and story 039 which of the two the fake exposes; the QA spec allows either.
- **Grace clock (F4, R17.2, R17.7).** `GraceExpiry(p) = max(DisconnectedAt(p), ActivePhaseStart) + G_eff`, with `G_eff = min(DisconnectGraceSec, RejoinWindowSec - GraceMarginSec)` and `GraceMarginSec = 30`. A participant disconnected before `Active` has its timer created at `ActivePhaseStart` with the `ConnectionGeneration` current then, so a rejoin and re-drop in Preparing leaves no stale timer. The timer is the queue item `TimerFired(GraceExpiry)` (`Subject` participant, `Generation` the captured `ConnectionGeneration`, `Expected` the round epoch) scheduled with `ScheduleAt(GraceExpiry, ...)`; Round also keeps the record `{P, GraceExpiry, ExpectedEpoch = (MatchId, RoundEpoch), ExpectedConnectionGeneration, Kicked}`. No `FTimerManager`.
- **Two fixture cases.** (1) `DisconnectedAt` before `ActivePhaseStart`: expiry is `ActivePhaseStart + G_eff` (F4 example: a participant who failed to load and is `Disconnected` at Active start converts at `G_eff`, 30 s at defaults). (2) `DisconnectedAt = ActivePhaseStart + 10`: expiry is `ActivePhaseStart + 10 + G_eff`. `G_eff` comes from a fixture constant, not an inline number. Assert the `GraceExpiry` stored in the record and the `ServerTime` of the `ScheduleAt` call on the fake queue.
- **Active entry in this test.** Dependencies are 001, 003 and 039; story 042 (the Active-entry step) lands later. The test therefore drives the same record-creation entry point that story 042 calls at Active entry, with `ActivePhaseStart` supplied by the fake clock, and asserts through a `ScheduleAt` spy. Open: confirm at /dev-story whether this entry point and the grace record type are created here or in story 060 (the full grace timeline); whichever lands first creates `RoundGraceRecord`, the other reuses it.
- **A disconnect during Preparing never aborts the round by itself (R6).** Only P5b (`TeamAbsent`) does; that is story 034. This story adds no abort path.
- **Where the code goes.** The P6 spawn call extends the step that story 039 creates under `Source/StealthGameplay/Private/Round/Preparing/`. The grace record and the F4 expiry function are plain C++ under `Round/Disconnect/`, behind the consumer-owned Roster, Crowd and Character Pool gateway interfaces injected by the composition root (ADR-0007); tests inject the story 003 fakes and the story 001 fake clock.
- Manifest 2026-10-25f: `UStealthPawnFactory` itself forces `ControlKind = Ai` (reporting to the fault sink if `Player` was asked) for a participant Session has not moved to `Connected` for this round.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 039 (`production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`): spawning all D pawns in one frame in sigma order and the `Connecting` participant's gate (PIE).
- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the Active-entry step itself (stamping, `RoundsPlayedCount`, `OnRoundActive`).
- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): grace timelines for disconnects during Active (silent, quit, informant, debug kill) and the PIE smoke.
- Stories 061 and 062: a rejoin inside or after grace (repossess the same pawn, or the C1 takeover).
- Story 067 (`production/epics/round-match-framework/story-067-bound-ai-move-look-only.md`): the Move-and-Look-only constraint on a bound AI-driven pawn.
- Story 068: the `DisconnectGraceSec` clamp rule and schema range (here `G_eff` is a fixture value).
- Roster internals (the `Disconnected` transition, `ConnectionGeneration`) and Session internals (connection facts, admission) are other epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Disconnect/RoundGraceRecord.h`
- `Source/StealthGameplay/Private/Round/Disconnect/RoundGraceRecord.cpp`
- `Source/StealthGameplay/Private/Round/Preparing/` (the P6 spawn step created by story 039, extended for non-`Connected` participants; Open: confirm file name against story 039)
- `Source/StealthGameplay/Private/Tests/Round/DisconnectedAtStartSpawnedTest.cpp` (class `FRoundDisconnectedAtStartSpawnedTest`, test name `StealthGame.Round.DisconnectedAtStartSpawned`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A participant `Disconnected` at P6 gets a bound, unpossessed, AI-driven pawn, with grace measured from `ActivePhaseStart`.
  - Given: the fake clock and a participant `Disconnected` at P6.
  - When: P6 and Active entry run, once with `DisconnectedAt` before `ActivePhaseStart` and once with `DisconnectedAt = ActivePhaseStart + 10`.
  - Then: the pawn exists and is bound, unpossessed, AI-driven and not net-relevant to its connection.
  - Then: with `DisconnectedAt` before `ActivePhaseStart` the grace expires at `ActivePhaseStart + G_eff`.
  - Then: with `DisconnectedAt = ActivePhaseStart + 10` it expires at `ActivePhaseStart + 10 + G_eff`.
  - Edge cases: in [U] there is no connection, so assert net-relevance through the gateway call or pawn flag; `G_eff` comes from the fixture.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/DisconnectedAtStartSpawnedTest.cpp` (automation test `StealthGame.Round.DisconnectedAtStartSpawned`, class `FRoundDisconnectedAtStartSpawnedTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.DisconnectedAtStartSpawned`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-001-test-seam-fake-clock.md`, `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (`Disconnected` record, `ConnectionGeneration`, `BindPawn`, via the fake Roster gateway; stories not yet created), `production/epics/shared-character-locomotion` (`SpawnSharedPawn` and its `ControlKind` forcing; stories not yet created), `production/epics/crowd-npc-behaviour` (AI drive of a bound pawn, via the fake Crowd gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (bound, Ai-driven, gated pawn; GraceExpiry ActivePhaseStart + G_eff and ActivePhaseStart + 10 + G_eff, observed firing on the real queue at that time)
**Deviations**: Open item resolved: FRoundGraceTable, F4 and a minimal TimerFired(GraceExpiry) resolver are created here; stories rmf-060..071 own the full timeline.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/DisconnectedAtStartSpawnedTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: N/A - needs a participant disconnected at P6, which L_Prototype cannot produce without travel
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
