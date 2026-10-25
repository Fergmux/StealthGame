# Story 086: A second round ended by debugeliminate gives EliminationWin, MatchScore [2,0] and a queue restarted at Seq 1

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel), `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 1c; rules R7, R15, R21, R22.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: ADR-0005: the four game-instance stores are the only authoritative match state, so `MatchScore` and `RoundsPlayedCount` persist across travel while the queue, `Seq` and the latch reset per round. ADR-0002: one queue per round keyed (`ServerTickIndex`, `Seq`): each round's queue restarts at `Seq` 1 while `ServerTickIndex` continues; a strike or debug elimination decides the round through the latch and the round-end step (for a strike the step is bound to the deciding slot's death write).
**ADR Version**: ADR-0005 2026-10-24; ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: Touches ADR-0005 S3 (two consecutive seamless round travels with the game-instance stores intact; seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`, so PIE results are labelled non-representative per guideline 10) and ADR-0002 Verification items (1) and (3) as in story 084. `stealth.round.debugeliminate` submits a `DebugRequestTransition(Informant, Assassinated)` through the real queue, so ADR-0002's strike round-end binding by `CauseKey` (section 8.3) may stage a death write; if the debug path stages none, the round-end step runs inline. Open: confirm against ADR-0002 section 8.3 and story 053 which of the two paths a debug elimination takes. No post-cutoff API is called directly.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules):
- Required: Round steps `Preparing` on the server game thread in order P1 params, P2 tuning, P3 start work, P4 layout, P5 barrier, P5b gate, P6 placement and spawn, P7 world-ready then `ActiveAtServerTime = ServerNow() + StartLeadSec`; a disconnect alone never aborts `Preparing` (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never re-submit or re-stamp an outcome (ADR-0002).
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002, ADR-0022).

**Prototype disposition**: This story is the acceptance run for code that REPLACES `CheckEliminationVictory` and `EndRound` in `StealthGameMode.cpp`; the reference tests `FStealthRules::EvaluateEliminationWinner` and the elimination tests in `StealthRulesTests.cpp` are re-expressed against the queue-driven latch by the Core win stories (043-048). This story edits no prototype file.

**Where it sits**: Exit B (Core), MVP lifecycle test part 3 of 3. It starts from the Lobby that story 085 returns to, runs a second full round and ends it by elimination; it is the cross-round proof that `MatchScore` accumulates and the queue restarts at `Seq` 1 with `ServerTickIndex` continuing.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 1c, scoped to this story:*

- [ ] GIVEN 1b's lobby, WHEN a second round starts and the host runs `stealth.round.debugeliminate <client>`, THEN `FRoundResult{EliminationWin, Winner 0, RoundIndex 2}`, `MatchScore = [2, 0]`, and round 2's queue restarted at `Seq` 1 while `ServerTickIndex` continued.

---

## Implementation Notes

*Derived from ADR-0002 sections 1, 5 and 8, ADR-0005 section 2 and GDD R7, R15, R21, R22:*

- **Second round.** From the Lobby of story 085 (teams unchanged, `RoundsPlayedCount` 1), run `stealth.round.start` again; travel completes and `RoundsPlayedCount` becomes 2 at Active entry (so `RoundIndex` 2). `ResetAll(RoundsPlayedCount + 1)` runs at this start and increments `RoundEpoch`.
- **The elimination.** `stealth.round.debugeliminate <client>` submits a `DebugRequestTransition(Informant, Assassinated)` for the client's participant through the real queue (flagged `Debug`, excluded from F7). With team 1's only Active member converted, the F2 zeroing event gives `EliminationWin(0)`: `FRoundResult{EliminationWin, Winner 0, RoundIndex 2}`, `MatchScore[0]` incremented to 2 in the same `FRoundPublicState` write as the result. Read both clients' replicated copies.
- **Queue restart.** Round 2's queue opens at `Seq` 1 at Active entry (R7). Compare `ServerTickIndex` across the two rounds: it continued and was not reset. Record the `ServerTickIndex` at round 1's end and at round 2's first drain and assert the second is greater; assert the first key of round 2 has `Seq` 1. The key is `(ServerTickIndex, Seq)` (ADR-0002 section 1).
- **Match scope.** `MatchScore` and `RoundsPlayedCount` live in the game-instance Round store (R21) and so survive both travels; the GameState mirror is re-seeded from it (story 029/030 prove the mechanism, this run proves it across a real second round).
- **Debug exclusion.** The debug item consumes a `Seq`, carries `Debug` and is excluded from F7 pacing metrics (story 027); do not assert on F7 here.
- **Test shape.** CQTest `NETWORK_TEST_CLASS(PIELifecycleElimination, "StealthGame.Round.PIE")`, continuing the shared harness session of stories 084 and 085, [PIE-2]. Label single-process PIE travel results non-representative (ADR-0005 guideline 10).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 084 (`production/epics/round-match-framework/story-084-pie-lifecycle-start.md`) and story 085 (`production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`): round 1 start, `debugwin` and reset.
- Story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the F2 zeroing-event truth table this run exercises once.
- Story 053 (`production/epics/round-match-framework/story-053-strike-win-staging.md`): the staging of a strike win (latch first, result at the death write).
- Story 030 (`production/epics/round-match-framework/story-030-match-scope-survives-travel.md`): the unit-level persistence of `MatchScore` and `RoundsPlayedCount`.
- GDD AC 2 (`PIELifecycleReal`: real objectives and a real assassination) is [DEFERRED] to the Objectives and Assassination epics and has no story.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/PIELifecycleEliminationTest.cpp` (CQTest `NETWORK_TEST_CLASS(PIELifecycleElimination, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.PIELifecycleElimination.<Method>`; the GDD scenario name is `StealthGame.Round.PIELifecycleElimination`)
- `Source/StealthGame/Private/StealthGameMode.cpp`, `Source/StealthGame/Private/StealthRules.cpp` (prototype `CheckEliminationVictory` and `EvaluateEliminationWinner`: retired/replaced or promoted as reference tests by the Core win stories; not edited by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A second round ended by `debugeliminate` gives `EliminationWin`, `MatchScore [2,0]` and a queue restarted at `Seq` 1.
  - Given: story 085's lobby (the "1b" lobby), and a second round started.
  - When: the host runs `stealth.round.debugeliminate <client>`.
  - Then: `FRoundResult{EliminationWin, Winner 0, RoundIndex 2}`.
  - Then: `MatchScore = [2, 0]`.
  - Then: round 2's queue restarted at `Seq` 1 while `ServerTickIndex` continued.
  - Edge cases: `ServerTickIndex` is compared across the two rounds, not reset.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/PIELifecycleEliminationTest.cpp` (CQTest `NETWORK_TEST_CLASS(PIELifecycleElimination, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-085-pie-lifecycle-objective-and-reset.md`, `production/epics/round-match-framework/story-044-elimination-truth-table.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (seamless round travel and lobby travel; stories not yet created), `production/epics/participant-state-roster` (`RequestTransition`, `DebugRequestTransition` target, `ResetAll`; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
