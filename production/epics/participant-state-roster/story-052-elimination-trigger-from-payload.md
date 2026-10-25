# Story 052: Round declares an elimination win exactly once from the transition payload

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: ADR-0005 puts the Roster in a game-instance store (`URosterStore` facade over the plain-C++ `FRosterCore`) that is the only authoritative match state, and fixes that `RequestTransition(P, Informant, StrikeLimit, ExpectedEpoch)` releases rather than retires the binding inside the call. ADR-0002 makes Round, not the Roster, own the terminate decision: elimination is evaluated inside the Roster event flush (`IEventFlushBarrier::FlushPending()`, FIFO after the outermost call) from the event payload, and the round-end step later calls `Roster.EndRoundAll()` as a batch that is never evaluated as an elimination.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No new post-cutoff APIs; the 5.6 change applies: `DispatchPostLogin` is deprecated (work moved into the protected virtual `OnPostLogin`), so overrides use `PostLogin`/`OnPostLogin`; `UE_LOGF` for logging (ADR-0007). Verification required: S3 (seamless and hard travel), S4 (reconnect), S5 (listen-host loss), none of which this story's stub-Round test depends on.

**Control Manifest Rules (this layer)**:
- Required: queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F...Core`) with no `UWorld` dependency, behind a subsystem facade; stub dependencies are injected — source: ADR-0007
- Required: every interacting state change resolves as an item (or inline follow-on in an item's slot) of the round's single queue; resolvers re-validate against live state and run follow-ons inline — source: ADR-0002
- Required: authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId` — source: ADR-0005
- Forbidden: Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain — source: ADR-0002
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` — source: ADR-0005
- Guardrail: queue drain + staging + teardown + Roster flush at most 0.5 ms on the listen-host game thread — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-30** [Int][Dep:Round] `StealthGame.Roster.EliminationTriggerFromPayload` (with Round): GIVEN team 0 has one Active (A), WHEN A is reversed, THEN the payload's `ActiveCountAfter` = [0, 3] and Round declares `EliminationWin(1)` exactly once. GIVEN `EndRoundAll` or `ResetAll` drives both counts to 0, THEN no win is declared (F4).

---

## Implementation Notes

*Derived from ADR-0002 sections 5 and 8, ADR-0005 section 2, and GDD F4 and R12:*

- F4: `EliminationTrigger(e) = e.Applied AND e.Old = Active AND e.New = Informant AND e.Reason in {Assassinated, ReversedAgainst, StrikeLimit} AND Phase = Active AND NOT EndedLatched AND e.ActiveCountAfter[e.Team] = 0 AND RosterCount(e.Team) >= 1 AND e.ActiveCountAfter[1 - e.Team] >= 1`, edge-triggered: evaluated once per applied event in `EventSeq` order, never re-evaluated from live state. Batch events (`OnRosterRoundEnded`, `OnRosterReset`) are never evaluated, even though they drive both counts to 0.
- The Roster's part is the payload (Story 035 delivers the core-level trigger predicate re-test; this story proves it against a Round consumer): `OnParticipantStateChanged{Id, Team, Old, New, Reason, EventSeq, RoundEpoch, ActiveCountAfter[2], InformantCountAfter[2]}` carries `ActiveCountAfter` as its own snapshot (R12). The example from F4: team 0 has one Active A, A's assassination of a prepared defender is reversed, producing `e{A, team 0, Active to Informant, ReversedAgainst, ActiveCountAfter = [0, 3]}`; the trigger is true and the result is `EliminationWin(1)`.
- [Dep:Round]: use a stub Round that registers as a Roster listener, evaluates the F4 predicate per event and records declared wins, until `production/epics/round-match-framework/EPIC.md` lands. The stub implements `EndedLatched` (set once a win is declared). The stub Round lives in the test tree (`StealthRosterRoundStub.h`), not in production code.
- Two sub-cases: (1) reverse A and observe `ActiveCountAfter = [0, 3]` and one `EliminationWin(1)`; (2) separately, `EndRoundAll` or `ResetAll` drives both counts to 0 and no win is declared.
- Open point (QA): with a stub, the "declares once" part only re-tests the stub; the payload assertion is the real Roster check. Keep the assertion on the payload primary and the declared-once assertion secondary.
- Test seam: if the test needs a Round-phase input the core does not take (the QA report notes the GDD's injected-dependency list has no Round-phase input), use the same phase seam as Story 035 behind `WITH_STEALTH_TEST_SEAMS` (never compiled into Shipping).
- Prototype disposition: re-expresses the elimination decision of `FStealthRules::EvaluateEliminationWinner` (`Source/StealthGame/Private/StealthRules.cpp`, called from `StealthGameMode.cpp`) against the new core payload; the prototype function is reference logic, not promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 035 (The elimination trigger fires only for a strike-out win and never for disconnect or empty sides): the core-level F4 predicate cases (a) to (e).
- Story 053 (Two last-active attacks in one tick latch exactly one elimination win): same-tick no-draw latching.
- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event) and Story 023 (EndRoundAll ends all records once and is idempotent): the batch calls themselves.
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the transition and its payload.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterEliminationTriggerFromPayloadTest.cpp` (class `FRosterEliminationTriggerFromPayloadTest`, test name `StealthGame.Roster.EliminationTriggerFromPayload`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterRoundStub.h` (stub Round listener evaluating F4; shared with Stories 053, 054, 055)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` and `.cpp` (read-only here unless the payload lacks `ActiveCountAfter`; shared file)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: Round declares an elimination win exactly once from the transition payload.
  - Given: a stub Round that evaluates F4, team 0 with one Active (A).
  - When: A is reversed; separately EndRoundAll or ResetAll drives both counts to 0.
  - Then: payload ActiveCountAfter is [0, 3] and the stub declares EliminationWin(1) exactly once. No win on the batch calls.
  - Edge cases: with a stub, the "declares once" part only re-tests the stub; the payload assertion is the real Roster check.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthRosterEliminationTriggerFromPayloadTest.cpp` (`StealthGame.Roster.EliminationTriggerFromPayload`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-035-elimination-trigger-core.md`
- Cross-epic: `production/epics/round-match-framework/EPIC.md` ([Dep:Round]); the test runs against a stub Round until it lands
- Unlocks: None
