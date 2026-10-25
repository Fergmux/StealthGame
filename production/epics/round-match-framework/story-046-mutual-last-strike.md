# Story 046: Two last-Active strikes in one tick are won by the lower-key attacker in both permutations

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 16; rules R14 (order inside an encounter by Round key), R15, R16 (2), Edge Cases ("If the last Active players of both teams eliminate each other in the same tick").
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies sections 5 and 8.1: a resolver re-validates against live state at its slot and runs its follow-ons inline, and the first terminal item latches, so the second strike resolves `Rejected_Latched`.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. Plain C++ queue core, stub resolvers and forced keys; no tick function, `UWorld` or post-cutoff API is exercised, so ADR-0002 verification items (1) to (3) do not apply.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Resolution queue and Forbidden Approaches):
- Required: resolvers re-validate against live state at the slot and run follow-ons inline; a deferred outcome is a new item with its own key and a logged `CauseKey` (ADR-0002).
- Forbidden: Never re-submit or re-stamp an outcome (ADR-0002).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); this headless test asserts no timing.

**Prototype disposition**: Replaces the first-come tie behaviour of `CheckEliminationVictory` in `Source/StealthGame/Private/StealthGameMode.cpp`. The prototype's `FStealthRules::EvaluateEliminationWinner` sees only head-counts and cannot attribute a mutual kill to the lower-key attacker; the key order of F1 replaces it. No other prototype equivalent.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 16, scoped to this story:*

- [ ] GIVEN both teams' last Active players strike each other in one tick, THEN the winner is the team of the attacker whose strike has the lower key, in both permutations, and the other strike is `Rejected_Latched`.

---

## Implementation Notes

*Derived from ADR-0002 sections 5 and 8.1 and GDD R14, R15, Edge Cases:*

- **Scenario.** Team 0's last Active player and team 1's last Active player each strike the other in the same tick. Each strike is a `PawnCommand(Attack)` item whose resolver (a stub, stories 004 and 008) re-validates at its slot and removes the victim, or reverses onto the attacker when the victim had prepared, per R14. The first strike in key order zeroes one team (the victim's) and latches `EliminationWin` for the other team, which is the attacker's team; the second strike returns `Rejected_Latched` because the queue is closed (its attacker was in any case already out of play in the Edge Cases text, but the latch alone is sufficient).
- **Both permutations explicitly.** Run once with team 0's strike at the lower key and once with team 1's strike at the lower key (keys forced by the stubs, as in story 045). The winner is the team of the lower-key attacker in each, so the result flips with the key and never depends on tick-function order or intake permutation.
- **Exactly one `WinLatched` entry** per run (counted through the `IRoundLogWriter::Append` spy, as in story 045), plus one `Rejected_Latched` entry for the loser's strike.
- **Builds on stories 043 to 045.** The latch (043), the F2 predicates (044) and the composed same-tick fixture (045) are reused; add no new production logic unless the strike stub requires a helper in test support.
- **Where the code goes.** Test-only: `Source/StealthGameplay/Private/Tests/Round/MutualLastStrikeTest.cpp`. Real strike resolution is the Assassination epic's; AC 2 is deferred, so stubs stand in.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 045 (`production/epics/round-match-framework/story-045-same-tick-final-vs-elimination.md`): a final completion against an elimination in one tick.
- Story 043 (`production/epics/round-match-framework/story-043-no-draw-property.md`) and story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the latch and the F2 predicates.
- Story 050: a defence-window expiry against a strike in one tick.
- Story 053: staging of a strike win until the deciding strike's death write.
- Assassination internals (the real Prepare/Attack encounter and its death write) and Roster internals belong to other epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/MutualLastStrikeTest.cpp` (class `FRoundMutualLastStrikeTest`, test name `StealthGame.Round.MutualLastStrike`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Two last-Active strikes in one tick are won by the lower-key attacker in both permutations.
  - Given: both teams' last Active players strike each other in one tick, with stub keys.
  - When: the tick drains in both key permutations.
  - Then: the winner is the team of the attacker whose strike has the lower key.
  - Then: the other strike is `Rejected_Latched`.
  - Edge cases: both permutations are covered explicitly; exactly one `WinLatched` entry.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/MutualLastStrikeTest.cpp` (automation test `StealthGame.Round.MutualLastStrike`, class `FRoundMutualLastStrikeTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-045-same-tick-final-vs-elimination.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (the strike's transition events, via the fake Roster gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
