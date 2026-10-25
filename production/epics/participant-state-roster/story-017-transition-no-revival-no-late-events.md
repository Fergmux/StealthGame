# Story 017: No revival and no round-ended-to-informant transitions are possible

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story pins the two "never again" rules of the lifecycle: nothing can return to `Active` through `RequestTransition` (the only way back is `ResetAll`), and a queued event that arrives after the round ended cannot turn a `RoundEnded` participant into an `Informant`. It exists so a late or stale event from the resolution queue can never revive a player or exile a participant who is already out of the round. It realises GDD rule R8 (result precedence steps 6 and 8), the legal-transition table rows "any other reason to Active" and "RoundEnded to Informant", and the Edge Case "a queued event resolves after `EndRoundAll`".

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: The Roster is one of four game-instance stores and is the only authoritative owner of `ParticipantRecord`s, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore` behind a thin `URosterStore` facade. Every mutating API asserts authority and the game thread, and match state is keyed by `ParticipantId`, never by controller or PlayerState. `RequestTransition` is the single writer of lifecycle state, so the legal-transition table is enforced in the core and no caller can bypass it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by the ADR is behavioural: spikes S3, S4 and S5, none of which has reported. This story runs entirely inside the engine-free `FRosterCore`, so none of them gates it.

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

- [ ] **AC-PSR-17** [Core] `StealthGame.Roster.Transition.NoRevivalNoLateEvents`: GIVEN an Informant or a RoundEnded participant, WHEN →Active is requested with any non-batch reason, THEN `Rejected_IllegalReason`. WHEN RoundEnded→Informant is requested with any entering reason, THEN `Rejected_IllegalTransition`. In both cases nothing changes and no event fires (table).

---

## Implementation Notes

*Derived from ADR-0005 Decision section 2 and GDD rules R8, the legal-transition table and the Edge Cases "Round boundaries":*

- The behaviour lives in `FRosterCore::RequestTransition` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`), the skeleton from story 014. This story adds no API; it pins precedence steps 6 and 8.
- Step 6 (`Rejected_IllegalReason`): the entering reasons are Informant <- {Assassinated, ReversedAgainst, StrikeLimit, Disconnect}, RoundEnded <- {RoundEnd}, Active <- {Reset}. So `->Active` with any non-batch reason (the four informant reasons) is `Rejected_IllegalReason`, from `Informant` or `RoundEnded` alike, not `IllegalTransition`. Batch reasons (`RoundEnd`, `Reset`) are step 5 and belong to story 019.
- `InformantKilled` (D10) is never an entering reason: it is `Rejected_IllegalReason` for every target state (QA edge case; keep it as an extra row, it does not weaken the AC).
- Step 8 (`Rejected_IllegalTransition`): `RoundEnded -> Informant` with each entering reason, including `Disconnect`. Because step 6 and step 8 come before steps 9 and 10, the `Disconnect` case never reaches the connection checks, so no connection generation needs to be supplied.
- Supply a matching `ExpectedEpoch` so step 4 does not mask the result. Snapshot the store (states, reasons, `EventSeq`, binding table) before each call and assert it is identical afterwards; assert the event spy saw nothing.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink). Build the `Informant` via the story-014 path and the `RoundEnded` participant via the freshly registered state (new records are `RoundEnded`) or via `EndRoundAll` once story 023 exists; use a core test seam otherwise, compiled only under `WITH_STEALTH_TEST_SEAMS`.
- Prototype disposition: no prototype counterpart. The prototype's lifecycle bookkeeping replaced in story 014 had no legality table, and nothing is ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the legal applied path.
- Story 016 (The first entering reason is kept and later reasons are no-ops): Informant to Informant no-ops.
- Story 019 (RoundEnd and Reset reasons are rejected on single transitions): `Rejected_BatchOnlyReason`.
- Story 023 (EndRoundAll ends all records once and is idempotent): the batch that produces `RoundEnded`.
- Story 026 (Late binds and transitions after a round boundary are rejected): the round-boundary scenario built around `EndRoundAll`.
- Story 027 (The transition matrix equals the checked-in CSV fixture for every combination): exhaustive combination coverage.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if steps 6 or 8 need correction)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if steps 6 or 8 need correction)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionNoRevivalNoLateEventsTest.cpp` (class `FRosterTransitionNoRevivalNoLateEventsTest`, test name `StealthGame.Roster.Transition.NoRevivalNoLateEvents`)
- ADR-0007 header naming: no new header is added; any new one uses the `Stealth` prefix and a project-unique file name.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: no revival; no RoundEnded-to-Informant.
  - Given: one Informant and one RoundEnded participant, state snapshot, event spy.
  - When: Active requested with each non-batch reason; RoundEnded->Informant requested with each entering reason.
  - Then: Active requests return Rejected_IllegalReason. RoundEnded->Informant returns Rejected_IllegalTransition, including for Disconnect. Nothing changes, no event.
  - Edge cases: InformantKilled as a reason is Rejected_IllegalReason for every target; RoundEnded to Active is also IllegalReason.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionNoRevivalNoLateEventsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-027-transition-exhaustive-matrix.md`
