# Story 019: RoundEnd and Reset reasons are rejected on single transitions

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story guarantees that a round can never end or reset for some participants and not others: the reasons `RoundEnd` and `Reset` exist only in their batch forms (`EndRoundAll`, `ResetAll`), and a single-participant `RequestTransition` carrying either reason returns `Rejected_BatchOnlyReason` for any target state and any current state. It realises GDD rules R7 (last bullet) and R8 (result precedence step 5) and the legal-transition table rows "any, Active or RoundEnded, Reset or RoundEnd via RequestTransition" and "any, Informant, Reset / RoundEnd".

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: The Roster is one of four game-instance stores and is the only authoritative owner of `ParticipantRecord`s, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore` behind a thin `URosterStore` facade. Every mutating API asserts authority and the game thread. `RequestTransition` is the single writer of single-participant lifecycle changes, while the round batches are separate calls so Round can reset or end every record in one step.
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

- [ ] **AC-PSR-13** [Core] `StealthGame.Roster.BatchOnlyReasonsRejectedSingly`: GIVEN any state, WHEN `RequestTransition` is called with Reason `RoundEnd` or `Reset` (for any target), THEN the result is `Rejected_BatchOnlyReason` and nothing changes (R7, R8 precedence 5).

---

## Implementation Notes

*Derived from ADR-0005 Decision section 2 and GDD rules R7, R8:*

- The check lives in `FRosterCore::RequestTransition` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`), the skeleton from story 014, as precedence step 5. It runs after authority (1), thread (2), unknown id (3) and epoch (4), and before the illegal-reason (6), no-op (7) and illegal-transition (8) checks.
- Consequently `RoundEnded -> RoundEnded` with `RoundEnd` is `Rejected_BatchOnlyReason`, not `NoOp_AlreadyInState`, and `Informant -> Active` with `Reset` is `Rejected_BatchOnlyReason`, not `IllegalReason`. An unknown id or a stale epoch outranks this result; supply a registered id and a matching `ExpectedEpoch`.
- Test matrix: for each current state in {Active, Informant, RoundEnded} (three registered participants) and each target in {Active, Informant, RoundEnded} and each reason in {RoundEnd, Reset}, that is 18 requests; every one returns `Rejected_BatchOnlyReason`. Snapshot the store before and after each, and assert the event spy saw nothing and `EventSeq` is unchanged.
- No new API is added. The batch calls themselves are stories 022 (`ResetAll`) and 023 (`EndRoundAll`).
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink). Build the three states through the story-014 path or a core test seam compiled only under `WITH_STEALTH_TEST_SEAMS`.
- Prototype disposition: no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event): the batch reset.
- Story 023 (EndRoundAll ends all records once and is idempotent): the batch round end.
- Story 017 (No revival and no round-ended-to-informant transitions are possible): the illegal-reason and illegal-transition results for non-batch reasons.
- Story 018 (Unregistered and retired ids are rejected by every id-taking API): the unknown-id result that outranks this one.
- Story 024 (Calls carrying an old round epoch or match id are rejected): the stale-epoch result that outranks this one.
- Story 027 (The transition matrix equals the checked-in CSV fixture for every combination): exhaustive combination coverage.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if step 5 needs correction)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if step 5 needs correction)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterBatchOnlyReasonsRejectedSinglyTest.cpp` (class `FRosterBatchOnlyReasonsRejectedSinglyTest`, test name `StealthGame.Roster.BatchOnlyReasonsRejectedSingly`)
- ADR-0007 header naming: no new header is added; any new one uses the `Stealth` prefix and a project-unique file name.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: RoundEnd and Reset rejected on single transitions.
  - Given: registered participants in each of Active, Informant, RoundEnded, matching epoch.
  - When: RequestTransition(target, reason) for every target (Active, Informant, RoundEnded) and reason in {RoundEnd, Reset}.
  - Then: Rejected_BatchOnlyReason every time, no state change, no event.
  - Edge cases: RoundEnded->RoundEnded with RoundEnd is still BatchOnly not NoOp. An unknown id or stale epoch outranks it.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterBatchOnlyReasonsRejectedSinglyTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-027-transition-exhaustive-matrix.md`
