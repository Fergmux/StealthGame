# Story 053: Two last-active attacks in one tick latch exactly one elimination win

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-003`, `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0002 resolves every interacting state change in one server-side total order: one resolution queue per round keyed `(ServerTickIndex, Seq)`, drained once per frame, with Roster transitions as inline follow-ons inside the item's slot and the Roster event flush (`IEventFlushBarrier::FlushPending()`) called after each `Resolve`. Round's latch record is authoritative and single-assignment; after the latch the queue is closed and every remaining item resolves `Rejected_Latched`, so a later `TryTerminate` returns false and logs `LateWinSuppressed`. ADR-0005 keeps the Roster as the game-instance store whose events (FIFO, strict `EventSeq`) Round evaluates.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff APIs (tick groups and `FTickFunction` are pre-cutoff and re-verified; `UE_LOGF` preferred for diagnostics). Verification required: spawning a Shared Pawn inside `TG_PostPhysics` from a resolver (spike with ADR-0010), client estimator accuracy under `LeakNetProfile`, and that the drain runs exactly once per server frame after CMC and AI ticks, including on the listen host (frame time at most 1/60 s). This story uses a stub queue and does not exercise those items.

**Control Manifest Rules (this layer)**:
- Required: every interacting state change resolves as an item (or an inline follow-on in an item's slot) of the round's single queue; nothing outside the drain mutates gameplay state — source: ADR-0002
- Required: events are ordered only with `Compare`/`GetKey`/`CurrentKey` — source: ADR-0002
- Required: only Round's `TryTerminate` calls `Latch`; after the latch the queue rejects every item with `Rejected_Latched` except parked `HostCommand`s, the post-latch lane and the `ConcealmentTransition` exemption — source: ADR-0002
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager` — source: ADR-0002
- Forbidden: Never re-submit or re-stamp an outcome — source: ADR-0002
- Guardrail: queue drain + staging + teardown + Roster flush at most 0.5 ms on the listen-host game thread — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-31** [Int][Dep:Round][Dep:Assassination] `StealthGame.Roster.NoDrawSameTick` (with Round): GIVEN the last Active on each team attacks the other in the same tick, WHEN the queue resolves, THEN exactly one `EliminationWin` is latched, for the first in queue order, and the second attack has no effect (F4).

---

## Implementation Notes

*Derived from ADR-0002 sections 1, 5, 8.1 and GDD F4 ("Why no draw is possible"), Edge Cases ("Simultaneous events"):*

- Why no draw is possible (F4): each single transition changes exactly one participant on one team, so one event can drive at most one team to 0. If two independent transitions in the same tick each drive their own team to 0, the second event's payload shows `ActiveCountAfter[1 - e.Team] = 0`, so it awards nothing, and the first event has already latched a valid win. Round evaluates the trigger per event in `EventSeq` order and latches the first win.
- Test shape: a stub Round queue (ordered by `(ServerTickIndex, Seq)`) and a stub Assassination that resolves an attack by calling `RequestTransition(victim, Informant, Assassinated, ...)`; the last Active A (team 0) and last Active B (team 1) attack each other in the same tick; the queue resolves both slots in key order. Assert exactly one `EliminationWin` is latched, for the first in queue order, and that the final state is pinned (see Open point).
- Edge Cases text (R6/Edge Cases): the winner is decided by arrival order, which favours lower latency (accepted limit); the Roster applies calls in the order received and never orders requests (R8).
- [Dep:Round][Dep:Assassination]: stubs until `production/epics/round-match-framework/EPIC.md` and `production/epics/assassination-defence-reversal/EPIC.md` land. The stub Round evaluates F4 per event and latches the first win (`EndedLatched`); it uses the shared `StealthRosterRoundStub.h` from Story 052.
- Open point (QA gap): "the second attack has no effect" has no pinned observable. Pin the second attack's result code and the final state. In the real queue, once Round latches inside the winning slot the queue closes and the second attack's item resolves `Rejected_Latched` before it ever reaches the Roster (ADR-0002 section 8.1; a later `TryTerminate` returns false and logs `LateWinSuppressed`). Where the latch is bound to the round-end step, a second transition that does reach the Roster after `EndRoundAll` is `Rejected_IllegalTransition`; if it reaches the Roster before then it applies but awards nothing because its `ActiveCountAfter[1 - e.Team]` is 0. Decide which of these the stub models and assert it.
- Prototype disposition: re-tests the logic of `FStealthRules::EvaluateEliminationWinner` (`Source/StealthGame/Private/StealthRules.cpp`) for the simultaneous case; the prototype resolved eliminations from live counts in `StealthGameMode.cpp`, which is replaced by the edge-triggered payload rule.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 035 (The elimination trigger fires only for a strike-out win and never for disconnect or empty sides): the core predicate, including case (d) two same-tick events driving both teams to 0.
- Story 052 (Round declares an elimination win exactly once from the transition payload): single-event payload to win path.
- Story 055 (Same-tick assassinations and strike-outs resolve in queue order): same-tick assassination versus strike-out ordering.
- Story 028 (Re-entrant listener calls see live state and events dispatch FIFO with strict EventSeq): FIFO dispatch and `EventSeq` ordering.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterNoDrawSameTickTest.cpp` (class `FRosterNoDrawSameTickTest`, test name `StealthGame.Roster.NoDrawSameTick`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRoundStub.h` (stub Round queue and latch; created by Story 052, extended here)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterAssassinationStub.h` (stub Assassination attack resolver; shared with Story 055)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: "the second attack has no effect" has no pinned observable. After the round-end step the second transition is Rejected_IllegalTransition; otherwise it applies but awards nothing. Needs the second transition's result code and the final state asserted.

**Provisional spec (qa-lead, pending gap resolution)**:

- **AC-1**: two last-active attacks in one tick latch exactly one win.
  - Given: a stub Round queue and Assassination stub, last-Active A (team 0) and B (team 1) attacking each other.
  - When: the queue resolves.
  - Then: exactly one EliminationWin latched, for the first in queue order. The second attack's result is as pinned.
  - Edge cases: no draw is possible.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthRosterNoDrawSameTickTest.cpp` (`StealthGame.Roster.NoDrawSameTick`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-035-elimination-trigger-core.md`
- Cross-epic: `production/epics/round-match-framework/EPIC.md` ([Dep:Round]) and `production/epics/assassination-defence-reversal/EPIC.md` ([Dep:Assassination]); stubbed until they land
- Unlocks: None
