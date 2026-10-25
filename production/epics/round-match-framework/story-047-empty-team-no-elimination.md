# Story 047: A 1v0 solo player never triggers an elimination or timer end; the final objective gives ObjectiveWin

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 18; rules R15 (`RosterCount(e.Team) >= 1` and `RosterCount(W) >= 1`), R16 (4) (no timer ends an Active round), F2 Output Range item 6 (a 1v0 dev round has only `ObjectiveWin`), F3 (`bAllowEmptyTeamDev`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies section 8.1 (Round owns the terminate decision and its predicates) to the empty-team case: a team with `RosterCount = 0` is never eliminated and never wins by elimination.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. A fake clock jumps once and a plain C++ phase core and queue core are drained; no tick function, `UWorld` or post-cutoff API is exercised, so ADR-0002 verification items (1) to (3) do not apply.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Round phase flow and Forbidden Approaches):
- Required: forfeit and void run through R15 `TryTerminate` with the precondition `ActiveCountAfter[team] = 0`, `RosterCount(team) >= 1`, phase `Active`, not `Terminated`; batch events never count (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer (ADR-0002); no timer anywhere may end an Active round (GDD R16 (4)).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); this headless test asserts no timing.

**Prototype disposition**: Promotes the prototype test "A single-team test session never ends by elimination" (`Source/StealthGame/Private/Tests/StealthRulesTests.cpp`, against `FStealthRules::EvaluateEliminationWinner` in `Source/StealthGame/Private/StealthRules.cpp`) as the reference for the empty-rival clause, re-expressed against the F2 event predicate; the prototype's `CheckEliminationVictory` and win-reason code are retired by stories 043 and 044.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 18, scoped to this story:*

- [ ] GIVEN a 1v0 dev round, WHEN the solo player becomes an Informant and the fake clock advances 10 x `TargetRoundMinutes`, THEN no outcome, phase `Active`, empty queue; WHEN instead the solo player completes the final objective, THEN `ObjectiveWin`.

---

## Implementation Notes

*Derived from GDD R15, R16, F2, F3:*

- **Setup.** A 1v0 dev round needs `bAllowEmptyTeamDev` true (F3: 1v0 is invalid otherwise; with the flag, imbalance 1 is within `MaxTeamImbalanceCount`). Team 0 holds one solo player, team 1 has `RosterCount = 0`. The flag is a dev-only key, compiled out of Shipping and forced false in `Default`.
- **Why no elimination.** When the solo player becomes an `Informant`, team 0's `ActiveCountAfter` is 0 (the zeroing clause holds), but the winner would be team 1, whose `RosterCount(W)` is 0, so `EliminationWin(W, e)` is false (F2: "a team with `RosterCount = 0` is never eliminated and never wins by elimination, so a 1v0 dev round has only `ObjectiveWin`; if its solo player becomes an informant the round cannot be won and the host aborts it"). The empty-rival clause is what suppresses the elimination, not the Active count.
- **No timer end (R16 (4)).** Round adds no dwell timer, sudden-death rule or countdown; after the solo player becomes an Informant nothing ends the round. Advance the fake clock by one jump of 10 x `TargetRoundMinutes` (a tuning key, read from the fixture), not a tick loop, drain once, and assert phase `Active`, no outcome and an empty queue.
- **Second run.** With the same setup, the solo player (human, `Connected`, `Active`) completes the final objective: `DeclareObjectiveWin(0, Key)` gives `ObjectiveWin`. Use a stub final completion (stories 004 and 008), not the real Objectives resolver (AC 2 is deferred).
- **Uses stories 043 and 044.** The latch and the F2 predicates exist; this story is a scenario over them with the empty-team fixture from the constants file (story 005). No new production code is expected except a fixture helper in test support.
- **Where the code goes.** Test-only: `Source/StealthGameplay/Private/Tests/Round/EmptyTeamNoEliminationTest.cpp`.
- Manifest 2026-10-25f: the forfeit/void precondition requires `RosterCount(team) >= 1` and ignores batch events, so an empty team never counts as eliminated.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the clause-by-clause truth table (including both `RosterCount` flips); this story is the scenario.
- Story 043 (`production/epics/round-match-framework/story-043-no-draw-property.md`): the latch and uniqueness property.
- Story 051 (`production/epics/round-match-framework/story-051-no-timer-ends-active-round.md`): the 24 h advance leaving only grace and pawn-loss timers (a different timer assertion).
- Story 022: Start validity for 1v0 (`TooSmall` without the flag, accepted with it).
- Story 057: the host abort that ends a stuck 1v0 round.
- Roster internals (`RosterCount`, `ActiveCountAfter`) belong to the participant-state-roster epic.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/EmptyTeamNoEliminationTest.cpp` (class `FRoundEmptyTeamNoEliminationTest`, test name `StealthGame.Round.EmptyTeamNoElimination`)
- `Source/StealthGame/Private/Tests/StealthRulesTests.cpp` (prototype single-team test: reference only, retired when the prototype rules file is removed)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A 1v0 solo player never triggers an elimination or timer end, and the final objective gives `ObjectiveWin`.
  - Given: a 1v0 dev round (`bAllowEmptyTeamDev` true) in `Active` with the fake clock.
  - When: the solo player becomes an Informant and the clock advances 10 x `TargetRoundMinutes`.
  - Then: there is no outcome, the phase is `Active`, and the queue is empty.
  - When (second run): the solo player completes the final objective.
  - Then: the outcome is `ObjectiveWin`.
  - Edge cases: team 0's Active count hits 0 here, so the empty-rival clause is what suppresses the elimination; a single clock jump, not a tick loop.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/EmptyTeamNoEliminationTest.cpp` (automation test `StealthGame.Round.EmptyTeamNoElimination`, class `FRoundEmptyTeamNoEliminationTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-044-elimination-truth-table.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (the solo player's transition event and `RosterCount`, via the fake Roster gateway; stories not yet created), `production/epics/tuning-data-config` (`TargetRoundMinutes`, `bAllowEmptyTeamDev` and the StealthTestHarness bootstrap; stories not yet created)
