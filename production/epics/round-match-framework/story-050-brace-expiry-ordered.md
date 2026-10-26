# Story 050: A defence-window expiry and a strike in one tick resolve by key order (strike lands or reversal)

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock); GDD AC 21; rules R12 (server-generated items go through the due-time heap, including the defence-window end), R13 (the `TimerFired` row, `DefenceWindowEnd`), R14 (order inside an encounter by Round key), Edge Cases ("If a defender's brace expires (`TimerFired`) and a strike on them arrives in the same tick").
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies section 4 (every timed item, defence window end included, is a `ScheduleAt` item in the due-time heap) and R14's contract: the outcome of an encounter is decided by comparing the Round keys of the items.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. Stub resolvers and forced keys on the plain C++ queue core; no tick function, `UWorld` or post-cutoff API is exercised, so ADR-0002 verification items (1) to (3) do not apply.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Resolution queue and Forbidden Approaches):
- Required: gameplay timers are `ScheduleAt` items in the due-time heap (ADR-0002).
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager`; cross-actor tick order is unspecified and join-order biased (ADR-0002).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); this headless test asserts no timing.

**Prototype disposition**: None (no prototype equivalent: the prototype has no defence window, no encounter keys and no due-time heap).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 21, scoped to this story:*

- [x] GIVEN stub resolvers, a defence-window expiry and a strike on that defender in one tick: expiry key lower -> the strike lands (victim `Informant`, `Assassinated`); strike key lower -> reversal (attacker `Informant`, `ReversedAgainst`).

---

## Implementation Notes

*Derived from ADR-0002 sections 4 and 5 and GDD R12 to R14:*

- **What Round guarantees.** The defence-window end is `TimerFired(DefenceWindowEnd)` (submitted by Assassination through `ScheduleAt`), and the strike is a `PawnCommand(Attack)`. Both are items in the one queue with Round keys, so "brace expires" versus "strike arrives" in one tick is decided by `Compare(Key(expiry), Key(strike))`, never by tick-function order. The lower key acts first (R14 (1)): expiry first means the defender is no longer prepared, so the strike lands (victim `Informant`, reason `Assassinated`); strike first means the defender is still prepared, so the strike is reversed (attacker `Informant`, reason `ReversedAgainst`).
- **Stubs stand in for Assassination.** The real encounter resolver is the Assassination epic's (AC 2 is deferred). Write two stub resolvers (stories 004 and 008) that implement R14's rule with `Compare(GetKey(...))` and emit the Roster transition request for the victim or the attacker with the stated reason; the assertion is on the reason and the transitioned participant.
- **Both orders, explicit.** Run once with the expiry key lower and once with the strike key lower. As in story 045, the keys are forced by the stubs, because this story depends on 008 and 044 and not on the real intake rules (stories 006 and 007 own the due-before-intake stamping and the permutation).
- **Outcomes execute in the resolving slot.** The encounter outcome executes inside the slot of the command that resolves it and inherits that item's key (R14 (2)); it is not re-submitted or re-stamped. Assert the transition carries the resolving key (the contract of story 008).
- **Elimination is a consequence, not the point.** If the victim or attacker was their team's last Active player, the inline follow-on may latch a win (stories 044 to 046); this story's fixtures keep other Active players on each team so only the ordering is under test.
- **Where the code goes.** Test-only: `Source/StealthGameplay/Private/Tests/Round/BraceExpiryOrderedTest.cpp`; the production contract it exercises (`Compare`, `GetKey`, `CurrentKey`) is Foundation queue code from story 008.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): the resolver outcomes, key accessors and `Seq` consumption used here.
- Story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the F2 predicates for when a strike zeroes a team.
- Stories 045 and 046: the same-tick final-versus-elimination and mutual-strike cases.
- Stories 006 and 007: the real intake permutation, due-before-intake stamping and `DueEpsilonMs` rules.
- Assassination internals (Prepare, Attack, the defence window itself, the death write) and Roster internals belong to other epics.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/BraceExpiryOrderedTest.cpp` (class `FRoundBraceExpiryOrderedTest`, test name `StealthGame.Round.BraceExpiryOrdered`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A defence-window expiry and a strike in one tick resolve by key order.
  - Given: stub resolvers, a defence-window expiry and a strike on that defender in one tick.
  - When: the expiry key is lower, then the strike key is lower.
  - Then: with the expiry key lower, the strike lands (victim `Informant`, `Assassinated`).
  - Then: with the strike key lower, the reversal applies (attacker `Informant`, `ReversedAgainst`).
  - Edge cases: both orders are covered explicitly.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/BraceExpiryOrderedTest.cpp` (automation test `StealthGame.Round.BraceExpiryOrdered`, class `FRoundBraceExpiryOrderedTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.BraceExpiryOrdered`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-008-resolver-contract.md`, `production/epics/round-match-framework/story-044-elimination-truth-table.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (the `Assassinated` and `ReversedAgainst` transitions, via the fake Roster gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing (expiry key lower -> victim Informant Assassinated; strike key lower -> attacker Informant ReversedAgainst; outcome carries the strike key)
**Deviations**: None (test-only)
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/BraceExpiryOrderedTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: N/A - Logic, covered headless
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
