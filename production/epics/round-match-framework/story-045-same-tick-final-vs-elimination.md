# Story 045: A final completion and a last-Active elimination in one tick resolve by key order with one WinLatched

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 15; rules R11 (one queue, one total order), R15, R16 (2), F2 example (tick 900, Seq 41 and 42), Edge Cases ("If team 0's final completion and the strike that eliminates team 0's last Active player resolve in the same tick").
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies sections 1, 5 and 8.1: items resolve one at a time in key order, follow-ons run inline in the slot, and after the latch every later item resolves `Rejected_Latched`.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. The test drives the plain C++ queue core with stub resolvers and forced keys; no tick function, `UWorld` or post-cutoff API is exercised, and the ADR-0002 verification items (1) to (3) do not apply.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Resolution queue and Forbidden Approaches):
- Required: events are ordered only with `Compare`, `GetKey` and `CurrentKey` (ADR-0002).
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager`; cross-actor tick order is unspecified and join-order biased (ADR-0002).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); this headless test asserts no timing.

**Prototype disposition**: Replaces the first-come resolution of `AStealthGameMode::OnObjectiveCompleted` against `CheckEliminationVictory` in `Source/StealthGame/Private/StealthGameMode.cpp`, where whichever call ran first won. The production rule is the F1 key; there is no prototype equivalent of a total-order key.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 15, scoped to this story:*

- [ ] GIVEN stub items: team 0's final completion (Seq n) and the strike that eliminates team 0's last Active (Seq n+1) in one tick, THEN `ObjectiveWin(0)` and the strike is `Rejected_Latched`; with the order swapped, THEN `EliminationWin(1)` and the completion is `Rejected_Latched`; exactly one `WinLatched` entry in each case.

---

## Implementation Notes

*Derived from ADR-0002 sections 1, 5, 8.1 and GDD R11, R15, F2:*

- **Two stub items, forced keys.** Use the story 004 stub resolvers and the story 008 resolver contract to put team 0's final completion at `Seq n` and the strike that eliminates team 0's last Active at `Seq n+1` in the same `ServerTickIndex`; the swapped run puts the strike at `Seq n` and the completion at `Seq n+1`. The order is forced by the stub keys, not by the intake permutation (stories 006 and 007 own that).
- **Order 1 (completion first).** The completion's resolver re-validates (`Valid(i)`: completer `Active`, `Connected`, human-controlled, site live), calls `Round.DeclareObjectiveWin(0, Key)`, which calls `TryTerminate(Win(ObjectiveWin(0)), Key)`; the latch closes the queue, so the strike item then resolves `Rejected_Latched` before its resolver runs.
- **Order 2 (strike first).** The strike's slot kills team 0's last Active player: the Roster transition runs inline (`Old = Active`, `New = Informant`, `ActiveCountAfter[0] = 0`), and Round's elimination check, evaluated in the Roster flush of that slot, gives `EliminationWin(1)` (F2 example: "With the Seqs swapped, Seq 41's kill gives `e{team 0, ActiveCountAfter = [0, 3]}`, so `EliminationWin(1)`"). The completion then returns `Rejected_Latched`; either its re-validation (the completer is no longer `Active`) or the latch alone would reject it, and the queue-level latch rejection comes first.
- **Exactly one `WinLatched`.** Each case records one `WinLatched` entry (and the loser one `Rejected_Latched` entry). Count `WinLatched` through a spy on `IRoundLogWriter::Append` (ADR-0002 section 8.9); the real ring and its content are story 011. The loser's rejection is also logged, never executed.
- **Uses the story 043 latch and the story 044 predicates.** Do not re-implement them here; this story only composes them with forced keys.
- **Where the code goes.** No new production file is expected beyond what stories 043 and 044 create; if a small composition helper for the stub scenario is needed it lives in test support. Planned test: `Source/StealthGameplay/Private/Tests/Round/SameTickFinalVsEliminationTest.cpp`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 043 (`production/epics/round-match-framework/story-043-no-draw-property.md`): the latch and outcome uniqueness over random interleavings.
- Story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the F2 clause table.
- Story 046 (`production/epics/round-match-framework/story-046-mutual-last-strike.md`): two last-Active strikes in one tick.
- Story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): the resolver outcomes, `Seq` consumption and key accessors used here.
- Stories 006 and 007: the real intake permutation and ordering rules that decide which of two same-tick commands gets the lower `Seq`.
- Story 052 and 053: the effects after the latch (round-end step, strike staging).
- The real Objectives and Assassination resolvers (AC 2 is deferred; stubs stand in).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/SameTickFinalVsEliminationTest.cpp` (class `FRoundSameTickFinalVsEliminationTest`, test name `StealthGame.Round.SameTickFinalVsElimination`)
- `Source/StealthGameplay/Private/Round/Win/RoundTerminalLatch.cpp` (created by story 043; touched here only if the `DeclareObjectiveWin` entry point lands in this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A final completion and a last-Active elimination in one tick resolve by key order with one `WinLatched`.
  - Given: stub items: team 0's final completion (Seq n) and the strike eliminating team 0's last Active (Seq n+1) in one tick.
  - When: the tick drains, then again with the order swapped.
  - Then: the first order gives `ObjectiveWin(0)` and the strike is `Rejected_Latched`.
  - Then: the swapped order gives `EliminationWin(1)` and the completion is `Rejected_Latched`.
  - Then: there is exactly one `WinLatched` entry in each case.
  - Edge cases: the order is forced by the stub keys, not by intake permutation.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/SameTickFinalVsEliminationTest.cpp` (automation test `StealthGame.Round.SameTickFinalVsElimination`, class `FRoundSameTickFinalVsEliminationTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.SameTickFinalVsElimination`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-043-no-draw-property.md`, `production/epics/round-match-framework/story-008-resolver-contract.md`
- Unlocks: `production/epics/round-match-framework/story-046-mutual-last-strike.md`
- Cross-epic: `production/epics/participant-state-roster` (the strike's transition event, via the fake Roster gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (RMF Exit B batch: stories 030-047, Preparing, spawn order and win rules)
**Criteria**: 1/1 passing (completion first: ObjectiveWin(0), strike Rejected_Latched; swapped: EliminationWin(1), completion Rejected_Latched; one WinLatched each)
**Deviations**: None
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/SameTickFinalVsEliminationTest.cpp`; full suite: `production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/full-suite.txt` (189/189 Success)
**Run result**: N/A - a same-tick ordering that play cannot stage deterministically; covered on the real queue headless
**Reviewed by**: lead-programmer focused review (sonnet, 15 findings, fixes in production/qa/evidence/rmf-exit-b-preparing-wins-2026-10-26/evidence.md) + self-review, RMF Exit B batch
