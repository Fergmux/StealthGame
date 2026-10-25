# Story 016: The first entering reason is kept and later reasons are no-ops

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story pins the "first reason wins" rule of `RequestTransition`: once a participant is an `Informant`, any later request for the same target state returns `NoOp_AlreadyInState`, keeps the original `LastTransitionReason`, and emits nothing. It exists so that a participant exiled by one cause (for example `Assassinated`) cannot be overwritten by a later racing cause (a strike-out, a reversal or a disconnect expiry), which would corrupt the round record Round and the post-round reveal rely on. It realises GDD rule R8 (first-reason-wins, result precedence step 7), the "Informant to Informant" row of the legal-transition table, and the EventSeq rule that no-ops consume nothing (R3, I6).

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: The Roster is one of four game-instance stores and is the only authoritative owner of `ParticipantRecord`s, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore` behind a thin `URosterStore` facade. Every mutating API asserts authority and the game thread, and match state is keyed by `ParticipantId`, never by controller or PlayerState. `RequestTransition` is the single writer of lifecycle state, so rules such as first-reason-wins are enforced in the core, not by callers.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by the ADR is behavioural: spikes S3 (seamless and hard travel), S4 (reconnect) and S5 (host loss), none of which has reported. This story runs entirely inside the engine-free `FRosterCore`, so none of them gates it.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Forbidden: Never disable, skip or reseed a failing test to pass a gate — source: ADR-0008
- Guardrail: Tests: per-commit pipeline <= 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-16** [Core] `StealthGame.Roster.Transition.FirstReasonWins` (each of the 4 reasons): GIVEN an Informant whose reason was `Assassinated`, WHEN any entering reason is requested, THEN `NoOp_AlreadyInState`, the reason is still `Assassinated`, and `EventSeq` is unchanged (R8).

---

## Implementation Notes

*Derived from ADR-0005 Decision section 2 (plain-C++ cores) and GDD rules R8, R3, R12, I6:*

- The behaviour lives in `FRosterCore::RequestTransition` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`), the skeleton created in story 014 (result-code enum, R8 precedence, legal-transition table). This story adds no new API; it pins and, if needed, corrects the precedence step 7 behaviour with a test.
- R8 precedence step 7 (`NoOp_AlreadyInState`, current state equals `NewState`) runs before step 9 (`Rejected_NotDisconnected`) and step 10 (`Rejected_StaleConnection`). Consequently a `Disconnect` request against an Informant whose `Connection` is `Connected`, or whose `ExpectedConnectionGeneration` is missing, is still `NoOp_AlreadyInState`.
- A no-op writes nothing: `LastTransitionReason` and `LastEventSeq` keep their first values, no event is queued, and `EventSeq` does not advance (R3: "No-ops and rejections consume nothing").
- Parameterise the test over the four entering reasons (`Assassinated`, `ReversedAgainst`, `StrikeLimit`, `Disconnect`), including the same reason that was already recorded (`Assassinated` again).
- Build the Informant through the real story-014 path (`RequestTransition(Active -> Informant, Assassinated)`) so the first reason is genuinely recorded, then record `EventSeq` and the event spy count before each repeat request.
- Test construction: `FRosterCore` takes injected dependencies from `Private/Roster/StealthRosterDeps.h` (pawn-liveness provider, authority provider = true, thread check = game thread, server clock in ms, seeded random source, fault sink). Use fakes; no `UWorld`.
- Prototype disposition: no prototype counterpart. The prototype's lifecycle bookkeeping that story 014 replaces has no first-reason rule, and nothing is ported (ADR-0007 promotion rule).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the transition skeleton, the applied path and the event payload.
- Story 015 (A Disconnect transition is rejected while the participant is connected): `Rejected_NotDisconnected` for an Active participant.
- Story 017 (No revival and no round-ended-to-informant transitions are possible): `Rejected_IllegalReason` and `Rejected_IllegalTransition`.
- Story 027 (The transition matrix equals the checked-in CSV fixture for every combination): the exhaustive combination coverage that includes these rows.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if step 7 needs correction)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if step 7 needs correction)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionFirstReasonWinsTest.cpp` (class `FRosterTransitionFirstReasonWinsTest`, test name `StealthGame.Roster.Transition.FirstReasonWins`)
- ADR-0007 header naming: any new header uses the `Stealth` prefix and a project-unique file name; no header is added in this story.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: first entering reason kept; later reasons are no-ops.
  - Given: an Informant whose LastTransitionReason is Assassinated, EventSeq recorded, run once per requested reason.
  - When: RequestTransition(Informant, r) for r in Assassinated, ReversedAgainst, StrikeLimit, Disconnect.
  - Then: NoOp_AlreadyInState, reason still Assassinated, EventSeq unchanged, no event dispatched.
  - Edge cases: r = Assassinated (same reason) and Disconnect with a Connected record both still NoOp (step 7 precedes 9 and 10).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionFirstReasonWinsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-027-transition-exhaustive-matrix.md`
