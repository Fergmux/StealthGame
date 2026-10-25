# Story 085: debugwin 0 ends the round with ObjectiveWin, every pawn stops within RunStopSec plus one frame, and reset returns to Lobby with teams unchanged

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel), `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 1b plus the speed clauses of AC 12b and AC 64 (revised in `rmf-ac-overrides.md`); rules R9, R10, R15, R22.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: ADR-0005: the four game-instance stores are the only authoritative match state; Reset returns to Lobby by lobby travel and host commands live on the game instance. ADR-0002: a single-assignment latch and the two-step round end; for an objective win the latch step is the round-end step, which publishes the result, increments the score, calls `ICrowdControl::Halt()` and the kind-blind stop-and-blend for every live pawn (`UStealthPawnFactory::StopAllForRoundEnd()`, ADR-0010 section 4.5).
**ADR Version**: ADR-0005 2026-10-24; ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: Touches ADR-0005 S3 (lobby travel after Reset; seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`, so PIE results are labelled non-representative per guideline 10) and ADR-0002 Verification item (3) (the drain runs once per server frame on the listen host, whose frame is its render frame). The stop measurement relies on the ADR-0010 stop path (`WalkStopSec`/`RunStopSec` braking, a pawn mid-traversal finishes the traversal first), whose traversal spike is not gated here but may delay a pawn's stop. No post-cutoff API is called directly.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules):
- Required: The latch step replicates and presents nothing; the round-end step runs in one server step, bound to the deciding slot's latest death write by `CauseKey` (or inline when the slot staged none), and calls `ICrowdControl::Halt()` plus the kind-blind stop-and-blend for every live pawn (ADR-0002).
- Forbidden: Never stop human pawns at round end by input loss (ADR-0002).
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002, ADR-0022); the round-end step's own budget is story 092.

**Prototype disposition**: This story is the acceptance run for code that REPLACES `EndRound(WinningTeam, Reason)`, `OnObjectiveCompleted`, `AbortRound`, `CleanupRoundActors`, the auto-restart and the dev console commands `StealthEndRound`/`StealthSwitchTeam` in `StealthGameMode.cpp`/`.h`; those commands survive only as host commands behind the net-mode check (TR-round-010). This story edits no prototype file.

**Where it sits**: Exit B (Core), MVP lifecycle test part 2 of 3. It continues story 084's Active round. It carries the speed clauses that the unit stories moved here: story 055 (AC 12b) asserts the stop requests with `RunStopSec` as their bound, and story 078 (AC 64) asserts the hand-over stop request; both leave the pawn speed measurement (below 1 cm/s within `RunStopSec` plus one frame) to this PIE run.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 1b, plus the AC 12b and AC 64 speed clauses (revised in `rmf-ac-overrides.md`), scoped to this story:*

- [ ] GIVEN 1a's Active round, WHEN the host runs `stealth.round.debugwin 0`, THEN both clients read `Phase = Ended`, `FRoundResult{ObjectiveWin, Winner 0, RoundIndex 1}`, `MatchScore = [1, 0]`; AND in the same PIE run after the objective win, every pawn has speed below 1 cm/s within `RunStopSec` plus one frame (the AC 12b and AC 64 speed clauses; the measurement point is named in the test; NPC, AI-driven and human-driven pawns are included); WHEN `stealth.round.reset` runs after `EndedMinDwellSec`, THEN both read `Lobby` and the teams are unchanged.

---

## Implementation Notes

*Derived from ADR-0002 section 8 and ADR-0010 section 4.5, ADR-0005 section 8 and GDD R9, R10, R22:*

- **The command.** `stealth.round.debugwin <team>` is a development-build host command that submits a `DebugObjectiveCompletion` through the real queue (flagged `Debug` in the log, compiled out of shipping), so the real latch runs before Objectives exists. Run it on the host's game instance with the net-mode and host check.
- **What both clients must read.** After the round-end step: `Phase = Ended`, `FRoundResult{ObjectiveWin, Winner 0, RoundIndex 1}` and `MatchScore = [1, 0]` in `FRoundPublicState`. For an objective win the round-end step is the latch step (nothing staged), so all three arrive together. Read each client's replicated copy, not the server's.
- **Speed clause.** After the win every pawn must be below 1 cm/s within `RunStopSec` (a Shared Character key; Crowd R10 bounds the stop by it) plus one frame. The pawn set must include NPC pawns (Crowd Halt), AI-driven participant pawns and human-driven pawns (the kind-blind server stop through `StopAllForRoundEnd()`, never input loss). Name the measurement point in the test: QA specifies the host's pawns on the host's simulation, sampled each frame from the round-end step. Also include one pawn released from Crowd earlier in the same drain (the AC 64 case) if the fixture can stage it. `RunStopSec` is read from the tuning snapshot, not inlined.
- **Reset.** After `EndedMinDwellSec` the host runs `stealth.round.reset`. An earlier reset is held and executed at the dwell's end (R9), so do not run it earlier. Both clients read `Lobby` and the teams are unchanged (R10; `Reset` does not call `ResetAll`, which runs at the next start). Reaching `Lobby` requires every participant terminal or `TravelLoadTimeoutSec` (story 058).
- **Travel evidence caveat.** Reset ends in lobby travel; label single-process PIE results non-representative for travel (ADR-0005 guideline 10). Open: confirm against ADR-0008 the tier this scenario runs in.
- **Test shape.** CQTest `NETWORK_TEST_CLASS(PIELifecycleObjectiveAndReset, "StealthGame.Round.PIE")`, continuing the session set up for story 084 (shared fixture helper in the harness), [PIE-2].

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 084 (`production/epics/round-match-framework/story-084-pie-lifecycle-start.md`): Start through travel to Active.
- Story 086 (`production/epics/round-match-framework/story-086-pie-lifecycle-elimination.md`): the second round ended by `debugeliminate`.
- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`), story 055 (`production/epics/round-match-framework/story-055-ended-hold.md`) and story 078 (`production/epics/round-match-framework/story-078-hand-over-pawn-stops-at-end.md`): the unit-level round-end effects, the stop requests and the hand-over stop; this story only measures the resulting speed.
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`): what Reset clears and when it reaches Lobby.
- Story 092 (`production/epics/round-match-framework/story-092-round-end-step-budget.md`): the round-end step's CPU and byte budget.
- The end banner text (HUD epic).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/PIELifecycleObjectiveAndResetTest.cpp` (CQTest `NETWORK_TEST_CLASS(PIELifecycleObjectiveAndReset, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.PIELifecycleObjectiveAndReset.<Method>`; the GDD scenario name is `StealthGame.Round.PIELifecycleObjectiveAndReset`)
- `Source/StealthGame/Private/StealthGameMode.cpp`, `Source/StealthGame/Public/StealthGameMode.h` (prototype `EndRound`, `OnObjectiveCompleted`, dev commands: retired/replaced by the Core stories this run exercises; not edited by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `debugwin 0` ends the round with `ObjectiveWin`, every pawn stops within `RunStopSec` plus one frame, and `reset` returns to Lobby.
  - Given: story 084's Active round on two clients.
  - When: the host runs `stealth.round.debugwin 0`, then `stealth.round.reset` after `EndedMinDwellSec`.
  - Then: both clients read `Phase = Ended`, `FRoundResult{ObjectiveWin, Winner 0, RoundIndex 1}` and `MatchScore = [1, 0]`.
  - Then: after the objective win, every pawn has speed below 1 cm/s within `RunStopSec` plus one frame (the AC 12b and AC 64 speed clauses).
  - Then: after the reset, both read `Lobby` and the teams are unchanged.
  - Edge cases: include NPC, AI-driven and human-driven pawns, and one pawn released from Crowd earlier in the drain if the fixture can stage it; measure the speed on the host's pawns (the measurement point must be named in the test); `RunStopSec` comes from the Shared Character key.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/PIELifecycleObjectiveAndResetTest.cpp` (CQTest `NETWORK_TEST_CLASS(PIELifecycleObjectiveAndReset, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-084-pie-lifecycle-start.md`, `production/epics/round-match-framework/story-052-ended-latch-effects.md`, `production/epics/round-match-framework/story-058-reset-clears.md`
- Unlocks: `production/epics/round-match-framework/story-086-pie-lifecycle-elimination.md`
- Cross-epic: `production/epics/session-travel` (lobby travel, admission; stories not yet created), `production/epics/participant-state-roster` (`EndRoundAll`, teams; stories not yet created), `production/epics/tuning-data-config` (module skeleton, StealthTestHarness bootstrap, `EndedMinDwellSec`; stories not yet created), `production/epics/shared-character-locomotion` (`RunStopSec`, the kind-blind stop path; stories not yet created), `production/epics/crowd-npc-behaviour` (Halt and the stop-and-blend for NPCs; stories not yet created)
