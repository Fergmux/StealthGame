# Story 034: Each of the 13 abort codes returns to Lobby with no result, no pawns, PreparingFailed archived and nothing released

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 7; rules R6 (all steps, P5 `ParamsMismatch` note), R8, R10.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, no host migration. ADR-0014: `UStealthLayoutService` world subsystem with provider registry, integer `FLayoutManifest` with `LayoutHash`, server navmesh built once and frozen at `OnLayoutReady`. This story applies the Preparing exits: an abort logs the code, runs the `Reset` cleanup with reason `PreparingFailed(code)` and returns everyone to the lobby by `RequestLobbyTravel`; the Event Log slice is archived server-side and never released (ADR-0002 sections 8.6 to 8.8).
**ADR Version**: ADR-0005 2026-10-24; ADR-0014 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk; ADR-0014 is MEDIUM)
**Engine Notes**: None touched in this unit test: all 13 cases run against fakes (Session, Roster, TDC, Character Pool, Level Socket) on the fake clock. The real abort path touches ADR-0005 `RequestLobbyTravel` (seamless travel S3) and ADR-0014's `OnLayoutAborted` / seed-mismatch outcomes, verified in their own epics.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation and Core Layer Rules):
- Required: Round and every feature reach layout only through `UStealthLayoutService` (Core Layer Rules, ADR-0014); a layout abort therefore arrives as `OnLayoutAborted`.
- Forbidden: Never toggle `bIsInactive` or remove a PlayerState from the array between `Preparing` and the next travel (Foundation Layer Rules, ADR-0005); the abort-to-lobby path must not do either.
- Guardrail: Travel load bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005); `Reset` never waits on anything unbounded (GDD R10).

**Prototype disposition**: Replaces `AbortRound` and `CleanupRoundActors` in `Source/StealthGame/Private/StealthGameMode.cpp`; the production abort returns to Lobby through `RequestLobbyTravel` and archives the Event Log slice.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 7, scoped to this story:*

- [ ] GIVEN Preparing, WHEN each of `BadParams`, TDC `Failed`, `TuningSnapshotFault` (a server self-check fault only; any number of client `ClientHash`/`PoolHash` mismatches never aborts, each only disconnects that client `ParamsMismatch`, R6 P5), `DeckInvalid`, `Error_DeckTooSmall`, `AllocatorMisuse`, `LayoutAborted`, `LayoutSeedMismatch`, `TeamAbsent`, `BindFailed`, `PlacementInvariant`, `PreparingWatchdog` and host abort is injected (one test case per code), THEN after lobby travel: phase `Lobby`, `RoundsPlayedCount` and `MatchScore` unchanged, no `FRoundResult` in the public struct, 0 pawns, the archived slice holds `PreparingFailed(code)`, `bCalledOff` is true only for the host abort, and nothing is released.

---

## Implementation Notes

*Derived from ADR-0002 sections 8.6 to 8.8, ADR-0005 and GDD R6, R8, R10:*

- **One abort path.** Every R6 failure, `PreparingWatchdog` and the host `AbortRound` in `Preparing` go through one function: log the reason, run the `Reset` cleanup (R10) with reason `PreparingFailed(code)`, and return everyone to the lobby by `RequestLobbyTravel`. It must not call `UResolutionQueue::Latch` (the queue is not open in `Preparing`). No result, no score change, `RoundsPlayedCount` unchanged (it only advances at Active entry).
- **R10 order.** (1) admission `Closed`; (2) cancel every Round timer and invalidate `RoundEpoch`-stamped work; (3) archive the Event Log slice server-side and clear the round log buffer; (4) destroy or release every player pawn of the round (zero pawns exist after an abort in `Preparing`, including after `BindFailed`, where the pawn is destroyed before `FinishSpawning`); (5) clear round-scoped data; (6) `RequestLobbyTravel()`; (7) when Session reports every participant terminal (bounded by `TravelLoadTimeoutSec`), phase `Lobby`, teams unlocked, admission `Open`. Round does not call `ResetAll` here.
- **What each code asserts.** After lobby travel: phase `Lobby`; `RoundsPlayedCount` and `MatchScore` unchanged; no `FRoundResult` in the public struct; 0 pawns; the archived slice holds `PreparingFailed(code)`; `bCalledOff` is true only for the host abort; nothing released (the Event Log slice is never released because there was no `Ended`).
- **The 13 cases, as the AC lists them:** `BadParams`, TDC `Failed`, `TuningSnapshotFault`, `DeckInvalid`, `Error_DeckTooSmall`, `AllocatorMisuse`, `LayoutAborted`, `LayoutSeedMismatch`, `TeamAbsent`, `BindFailed`, `PlacementInvariant`, `PreparingWatchdog`, host abort. Open: confirm against ADR-0002 section 8.6 (Preparing watchdog and abort codes). The ADR's closed abort-code set is `BadParams`, `TuningLoadFailed`, `TuningSnapshotFault`, `DeckInvalid`, `AllocatorMisuse`, `LayoutAborted`, `LayoutSeedMismatch`, `TeamAbsent`, `BindFailed`, `PlacementInvariant`, `PlacementInfeasible`, `NpcCountTooLow`, `PreparingWatchdog`. The AC's "TDC `Failed`" corresponds to the ADR's `TuningLoadFailed`; `Error_DeckTooSmall` is the allocator return that aborts as `DeckInvalid` (GDD R6 P3); the AC list omits `PlacementInfeasible` and `NpcCountTooLow` (both abort "with that code", GDD R6 P6) and counts host abort as the thirteenth case. Do not add test cases beyond the QA spec; flag the difference to the lead before closing the story.
- **Client hash mismatches never abort.** `TuningSnapshotFault` comes only from TDC's server self-check at P2. Any number of `ClientHash` or `PoolHash` mismatches never aborts; each only disconnects that client `ParamsMismatch` (Session), and the participant becomes a normal disconnect (R17).
- **Table-driven.** One fixture row per code, each injected through the corresponding fake gateway (Session `OnTravelFailed(BadParams)`, TDC `Failed`, TDC self-check fault, Character Pool `DeckInvalid` / `Error_DeckTooSmall` / `Error_AlreadyDealt`, layout `OnLayoutAborted` / seed mismatch, Roster `TeamAbsent` via P5b, a rejected `BindPawn`, a failed zone-share check, the watchdog on the fake clock, host abort).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 028 (`production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`): the phase machine and transition table (this story's dependency).
- Story 031 and 032 (`production/epics/round-match-framework/story-031-allocate-once.md`, `production/epics/round-match-framework/story-032-preparing-barrier.md`): the deal and the barrier (dependencies).
- Story 035 (`production/epics/round-match-framework/story-035-watchdog-bound.md`): the F6 value of `PreparingWatchdogEff`.
- Story 038 (`production/epics/round-match-framework/story-038-placement-invariant.md`) and 039: the placement invariant and the spawn that raise `PlacementInvariant` and `BindFailed`.
- Story 057 and 058: host abort in Active and the `Reset` cleanup of an Active round.
- Story 059 (`production/epics/round-match-framework/story-059-session-end-and-abort-during-travel.md`): `OnSessionEnding` and a host abort before P1.
- Story 027 (`production/epics/round-match-framework/story-027-reset-command-matrix.md`): the `reset` / `abort` command routing.
- Level Socket, TDC, Character Pool and Session internals (their failure detection).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Preparing/PreparingAbort.h`
- `Source/StealthGameplay/Private/Round/Preparing/PreparingAbort.cpp`
- `Source/StealthGameplay/Private/Round/Phase/RoundResetCleanup.cpp`
- `Source/StealthGameplay/Private/Tests/Round/PreparingAbortsTest.cpp` (class `FRoundPreparingAbortsTest`, test name `StealthGame.Round.PreparingAborts`)
- `Source/StealthGame/Private/StealthGameMode.cpp` (prototype `AbortRound` and `CleanupRoundActors`: retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Each of the 13 abort codes returns to Lobby with no result, no pawns, PreparingFailed archived and nothing released.
  - Given: Preparing, with one test case per code: `BadParams`, TDC `Failed`, `TuningSnapshotFault`, `DeckInvalid`, `Error_DeckTooSmall`, `AllocatorMisuse`, `LayoutAborted`, `LayoutSeedMismatch`, `TeamAbsent`, `BindFailed`, `PlacementInvariant`, `PreparingWatchdog` and host abort.
  - When: the code is injected.
  - Then: after lobby travel the phase is `Lobby` and `RoundsPlayedCount` and `MatchScore` are unchanged.
  - Then: there is no `FRoundResult` in the public struct, and 0 pawns exist.
  - Then: the archived slice holds `PreparingFailed(code)`.
  - Then: `bCalledOff` is true only for the host abort.
  - Then: nothing is released.
  - Then: any number of client `ClientHash`/`PoolHash` mismatches never aborts and each only disconnects that client with `ParamsMismatch`.
  - Edge cases: `TuningSnapshotFault` is a server self-check only, so a client mismatch must not map to it; the 13 cases are table-driven, one per code; Open: confirm against ADR-0002 section 8.6 the mapping between the AC's code list and the ADR's closed abort-code set (see Implementation Notes).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/PreparingAbortsTest.cpp` (automation test `StealthGame.Round.PreparingAborts`, class `FRoundPreparingAbortsTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.PreparingAborts`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`, `production/epics/round-match-framework/story-031-allocate-once.md`, `production/epics/round-match-framework/story-032-preparing-barrier.md`
- Unlocks: `production/epics/round-match-framework/story-027-reset-command-matrix.md`
- Cross-epic: `production/epics/session-travel` (`RequestLobbyTravel`, `OnTravelFailed`, `ParamsMismatch`; stories not yet created), `production/epics/tuning-data-config` (TDC `Failed`, `TuningSnapshotFault`, module skeleton and StealthTestHarness bootstrap; stories not yet created), `production/epics/character-pool-appearance` (`DeckInvalid`, `Error_DeckTooSmall`, `Error_AlreadyDealt`; stories not yet created), `production/epics/level-socket-contract` (`OnLayoutAborted`, `PlacementInfeasible`, `NpcCountTooLow`; stories not yet created), `production/epics/participant-state-roster` (`BindPawn` rejection, `TeamAbsent` counts; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (14 table rows: the 13 ADR-0002 8.6 codes, Error_DeckTooSmall as a second DeckInvalid injection, and the host abort; Lobby after lobby travel, counts and score unchanged, no FRoundResult, 0 pawns, PreparingFailed(code) archived, bCalledOff only for the host abort, nothing released; client hash mismatches never abort)
**Deviations**: Decision D48: the closed ADR set is used (it adds PlacementInfeasible and NpcCountTooLow to the AC list; the AC's "TDC Failed" is TuningLoadFailed). The host abort archives EndReason Aborted (it has no abort code in the closed set). Lobby travel completes on the next Round tick (no Session travel yet).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/PreparingAbortsTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: N/A - aborts need failure injection; the production abort path is exercised headless with every code
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
