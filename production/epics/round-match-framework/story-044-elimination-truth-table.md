# Story 044: The F2 zeroing event yields an outcome only when all clauses hold; flipping any one yields none

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 48; rules R15 (`EliminationWin`, the Reason partition), F2 (`Zero(e)`, `Decision(e)`, `Expiry(e)`, `Kick(e)`, `EliminationWin(W, e)`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies the rule that Round owns the terminate decision (win predicates, Reason partition, void codes; Round R15) and evaluates every applied single-participant transition event inside the Roster flush of the causing slot (section 5), where `TryTerminate` normally fires.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. The predicate is a pure function over an event record (`OnParticipantStateChanged` payload) and counts supplied by a fake Roster gateway; no `UWorld`, tick function or post-cutoff API is involved. ADR-0002 verification items (1) to (3) are not exercised.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Round phase flow and Forbidden Approaches):
- Required: forfeit and void run through R15 `TryTerminate` with the precondition `ActiveCountAfter[team] = 0`, `RosterCount(team) >= 1`, phase `Active`, not `Terminated`; batch events never count (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source (ADR-0002).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); the truth-table test is pure logic with no timing assertion.

**Prototype disposition**: Replaces `AStealthGameMode::CheckEliminationVictory` in `Source/StealthGame/Private/StealthGameMode.cpp` (called on the next tick from a timer, first-come). Promotes `FStealthRules::EvaluateEliminationWinner` (`Source/StealthGame/Private/StealthRules.cpp`) and its tests in `Source/StealthGame/Private/Tests/StealthRulesTests.cpp` ("Team 1 wins when team 0 has no active players", "Team 0 wins when team 1 is wiped out", "No winner while both teams have active players") as reference rows of this table, re-expressed against the F2 event predicate.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 48, scoped to this story:*

- [ ] GIVEN the F2 zeroing event with all clauses true, THEN an outcome; flipping one clause at a time (Old, New, count, either `RosterCount`, phase, terminated, batch event) gives no outcome; a preliminary completion never ends the round.

---

## Implementation Notes

*Derived from GDD R15 and F2 (text read from the GDD; see the Open items):*

- **The predicates (F2, verbatim in substance).** `Zero(e) = e.Applied AND e.Old = Active AND e.New = Informant AND Phase = Active AND NOT L AND e.ActiveCountAfter[e.Team] = 0 AND RosterCount(e.Team) >= 1`, with `W = 1 - e.Team`. The Reason partition is disjoint: `Decision(e)` is `Assassinated`, `ReversedAgainst` or `StrikeLimit`; `Expiry(e)` is `Disconnect` and not kicked; `Kick(e)` is `Disconnect` and kicked. `EliminationWin(W, e) = Zero(e) AND RosterCount(W) >= 1 AND ((Decision(e) AND e.ActiveCountAfter[W] >= 1) OR (Expiry(e) AND ConnectedActiveCount(W) >= 1))`. `ObjectiveWin(t, i)` requires `i.Type = FinalCompletion`, which is why a preliminary (stage) completion never ends the round (R15: preliminary stage completions do not end the round).
- **Where it runs.** Round evaluates every applied single-participant transition event `e`, in `EventSeq` order, inside the Roster flush of the causing slot (ADR-0002 section 5), so no other item interleaves between the cause and its check. Batch events (`OnRosterRoundEnded`, `OnRosterReset`) are never evaluated (F2 proof, item 5), which is why `EndRoundAll` cannot create a second candidate (story 052).
- **Table-driven test.** Build the event fixture with every clause true (use a `Decision` reason such as `Assassinated`, so the `ActiveCountAfter[W] >= 1` clause also applies), assert an `EliminationWin(W)` outcome, then re-evaluate with exactly one clause flipped per row: `Old` not `Active`; `New` not `Informant`; `ActiveCountAfter[e.Team]` not 0; `RosterCount(e.Team)` 0; `RosterCount(W)` 0 (the two `RosterCount` clauses are separate rows); phase not `Active`; `Terminated` true; and the event being a batch event. Each flipped row yields no outcome and no `WinLatched`. A final row submits a stage (preliminary) completion and asserts it never ends the round.
- **Fixture, not inline.** Team layouts, reason names and counts come from the test constants file (story 005). The scope here is the Round predicate; the transition itself and its counts come from the fake Roster gateway (story 003).
- **Relation to story 043.** Story 043 proves the latch and outcome uniqueness over random interleavings against an independent F2 reference; this story pins the clause-by-clause boundary. Do not reuse the story 043 reference model here: the expected outcomes in this table are written by hand from F2.
- **Open: confirm against GDD F2** that the QA clause list ("Old, New, count, either `RosterCount`, phase, terminated, batch event") maps one-to-one onto the F2 predicates above. F2 also names `e.Applied` and the Decision/Expiry partition clauses (`ActiveCountAfter[W] >= 1`, `ConnectedActiveCount(W) >= 1`), which AC 48 does not list; the AC lists the flips to implement, and any further rows need qa-lead approval.
- **Where the code goes.** `Source/StealthGameplay/Public/Round/Win/RoundWinPredicates.h` and `Private/Round/Win/RoundWinPredicates.cpp` (pure functions, plain C++); planned names, `/dev-story` replaces them with the real ones.
- Manifest 2026-10-25f: the forfeit/void precondition is `ActiveCountAfter[team] = 0`, `RosterCount(team) >= 1`, phase `Active`, not `Terminated`, and `ClearBinding` emits no event of its own, so only applied transition events reach the check.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 043 (`production/epics/round-match-framework/story-043-no-draw-property.md`): the latch, outcome uniqueness over 100,000 interleavings and the no-draw enums (this story builds on its latch).
- Stories 045 and 046: same-tick final versus elimination and mutual last strikes (key order).
- Story 047 (`production/epics/round-match-framework/story-047-empty-team-no-elimination.md`): the 1v0 solo round (the empty-rival clause as a scenario).
- Story 048 (`production/epics/round-match-framework/story-048-informants-do-not-count.md`): informant kills and re-informing as scenarios.
- Story 049: the completer validity `Valid(i)` for a final completion.
- Story 064 and 065: the forfeit and void partition for `Disconnect` reasons end to end (here only the predicate rows).
- Roster internals (`ActiveCountAfter` snapshots, `RosterCount`) belong to the participant-state-roster epic.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Win/RoundWinPredicates.h`
- `Source/StealthGameplay/Private/Round/Win/RoundWinPredicates.cpp`
- `Source/StealthGameplay/Private/Tests/Round/EliminationTruthTableTest.cpp` (class `FRoundEliminationTruthTableTest`, test name `StealthGame.Round.EliminationTruthTable`)
- `Source/StealthGame/Private/StealthGameMode.cpp` (`CheckEliminationVictory`: retired/replaced by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: The F2 zeroing event yields an outcome only when all clauses hold, and flipping any one yields none.
  - Given: a zeroing event with every F2 clause true.
  - When: it is evaluated, then re-evaluated with each clause flipped alone: Old, New, count, each `RosterCount`, phase, terminated, batch event.
  - Then: with all clauses true there is an outcome.
  - Then: each single flip gives no outcome.
  - Then: a preliminary completion never ends the round.
  - Edge cases: the clause definitions come from F2 and the cases are table-driven; both `RosterCount` clauses are flipped separately. Open: confirm against GDD F2 that the table covers exactly the clauses AC 48 names (see Implementation Notes).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/EliminationTruthTableTest.cpp` (automation test `StealthGame.Round.EliminationTruthTable`, class `FRoundEliminationTruthTableTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.EliminationTruthTable`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-043-no-draw-property.md`
- Unlocks: `production/epics/round-match-framework/story-047-empty-team-no-elimination.md`, `production/epics/round-match-framework/story-048-informants-do-not-count.md`, `production/epics/round-match-framework/story-049-completion-needs-human.md`, `production/epics/round-match-framework/story-050-brace-expiry-ordered.md`, `production/epics/round-match-framework/story-064-last-disconnect-forfeits.md`, `production/epics/round-match-framework/story-086-pie-lifecycle-elimination.md`
- Cross-epic: `production/epics/participant-state-roster` (the transition event payload with `ActiveCountAfter` and `RosterCount`, via the fake Roster gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (all clauses: EliminationWin(1); each of the 8 listed flips: no outcome; a stage completion never ends the round)
**Deviations**: None
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/EliminationTruthTableTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: OBSERVED: production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/ (elimination win in PIE through the production predicate)
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
