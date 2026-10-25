# Story 015: A Disconnect transition is rejected while the participant is connected

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0005 has Session report connection facts and Round decide, so a grace-expiry `Disconnect` request reaches the Roster carrying `ExpectedConnectionGeneration`, and the Roster accepts it only when its own `Connection` projection is `Disconnected` (a kick that closes an `Expired` connection counts as `Disconnected`). Session never calls `RequestTransition`. The Roster's guard is what stops a connected player from being converted to an informant by a stale or mistaken timer.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 (HIGH, networking): no new post-cutoff APIs; S3, S4 and S5 verification is outstanding but this story is pure core logic with no engine call.

**Context paragraph**: This story proves two cells of the R8 precedence for the `Disconnect` reason: an `Active` participant who is `Connected` gets `Rejected_NotDisconnected` (step 9) with no event and no change, while an `Informant` who is `Connected` gets `NoOp_AlreadyInState` because step 7 outranks step 9. It realises GDD R8 (precedence steps 7 and 9), R11 (Expiry) and F6 (`AcceptDisconnect`).

**Control Manifest Rules (this layer)**:
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` (ADR-0005)
- Forbidden: Never let a client message start, extend or restart a deadline (ADR-0005)
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-15** [Core] `StealthGame.Roster.Transition.DisconnectRequiresDisconnected`: GIVEN Active and Connected, WHEN `RequestTransition(Informant, Disconnect)` is called, THEN the result is `Rejected_NotDisconnected` and no event fires. GIVEN Informant and Connected, WHEN the same request is made, THEN `NoOp_AlreadyInState` (R8 precedence, F6).

---

## Implementation Notes

*Derived from ADR-0005 section 5 (Disconnect) and GDD R8 precedence 7 and 9, R11, F6:*

- **No new production code expected beyond story 014.** Steps 7 (`NoOp_AlreadyInState`) and 9 (`Rejected_NotDisconnected`) of the R8 precedence skeleton are implemented in story 014; this story pins their behaviour for the `Disconnect` reason and corrects any ordering mistake the test exposes. If a step is wrong, fix it in `RequestTransition`, do not add a special case for `Disconnect`.
- **Precedence under test.** For `Active` + `Connected`: steps 1 to 8 pass (valid epoch, `Disconnect` is an entering reason for `Informant`, current state `Active` differs from `Informant`, `Active -> Informant` is a legal source), step 9 fails, so the result is `Rejected_NotDisconnected`. For `Informant` + `Connected`: step 7 returns `NoOp_AlreadyInState` before step 9 is reached ("an Informant who receives a `Disconnect` request gets `NoOp_AlreadyInState`, whether connected or not", R8). F6's `AcceptDisconnect` being false does not mean a rejection in the Informant case.
- **Valid epoch and generation.** Supply a valid `ExpectedEpoch` and a valid `ExpectedConnectionGeneration` in both calls so steps 4 and 10 cannot be the reason for the result.
- **No event, no change.** Use an event spy on the story 008 queue and the snapshot helper; assert `EventSeq` unchanged and the record unchanged for both calls.
- **Fixtures.** An `Active` participant and an `Informant` participant, both `Connected`; set the lifecycle states through the core test seam (or via story 014's `Applied` path for the Informant) until story 022.
- **Prototype disposition:** No prototype counterpart (the prototype treated `Logout` as an immediate elimination trigger with no connection guard; see `AStealthGameMode::Logout` in `Source/StealthGame/Private/StealthGameMode.cpp`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the `RequestTransition` implementation and the applied `Disconnect` path
- Story 016 (The first entering reason is kept and later reasons are no-ops): first-reason-wins across all reasons
- Story 027 (The transition matrix equals the checked-in CSV fixture for every combination): every connection and generation combination
- Story 054 (Disconnect grace expires on the injected clock and stale-connection timers are rejected): `Rejected_StaleConnection` and the grace timer behaviour

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (changed only if the test exposes a precedence defect in `RequestTransition`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthTransitionDisconnectRequiresDisconnectedTest.cpp` (new: class `FRosterTransitionDisconnectRequiresDisconnectedTest`, test name `StealthGame.Roster.Transition.DisconnectRequiresDisconnected`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: a Disconnect transition is rejected while the participant is connected.
  - Given: one Active+Connected and one Informant+Connected participant, event spy, valid epoch and generation.
  - When: RequestTransition(Informant, Disconnect) for each.
  - Then: Active one returns Rejected_NotDisconnected, no event, no state change. Informant returns NoOp_AlreadyInState.
  - Edge cases: NoOp (step 7) outranks NotDisconnected (step 9).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthTransitionDisconnectRequiresDisconnectedTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Unlocks: `production/epics/participant-state-roster/story-027-transition-exhaustive-matrix.md`
