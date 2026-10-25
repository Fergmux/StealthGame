# Story 018: Unregistered and retired ids are rejected by every id-taking API

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story makes sure a stale or bogus `ParticipantId` can never reach a record: an id that was never registered, or one that was registered and later removed (ids are retired, never reused), is rejected with `Rejected_UnknownParticipant` and changes nothing. It exists because features and timers hold ids across frames, and a held id can outlive its record after a Lobby removal. It realises GDD rules R8 (result precedence step 3), R4 (`RemoveParticipant` retires the id) and R9 (`BindPawn` precedence), plus the legal-transition table row "any, unknown Id".

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: The Roster is one of four game-instance stores and is the only authoritative owner of `ParticipantRecord`s, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore` behind a thin `URosterStore` facade. Authoritative state and every ack or mailbox record are keyed by `ParticipantId`, never by controller or PlayerState pointer. Every mutating API asserts authority and the game thread before it looks at the id.
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

- [ ] **AC-PSR-18** [Core] `StealthGame.Roster.Transition.UnknownIdRejected`: GIVEN an id that was never registered, and an id that was retired, WHEN each Id-taking API is called, THEN `Rejected_UnknownParticipant` and nothing changes (table).

---

## Implementation Notes

*Derived from ADR-0005 Decision section 2 and GDD rules R4, R8, R9, R5:*

- Result precedence for `RequestTransition` (R8): `Rejected_NotAuthority`, `Rejected_WrongThread`, then `Rejected_UnknownParticipant` (step 3), then `Rejected_StaleEpoch`. The unknown-id check therefore ranks below authority and thread and above the epoch check; keep authority true and the thread check on the game thread in this test.
- `BindPawn` has the same order (R9: NotAuthority, WrongThread, UnknownParticipant, StaleEpoch, WrongState, AlreadyBound, PawnInUse).
- `AssignTeams` checks `Rejected_TeamsLocked` first, then `Rejected_UnknownParticipant` (R5), so run the batch case with teams unlocked and a batch that contains the unknown id.
- "Retired id" means an id that was registered and then removed with `RemoveParticipant` (R4: the id is retired, not reused). The test registers id 3, removes it (teams unlocked), and uses 3 as the retired id; use an id such as 99 as the never-registered case.
- Id-taking calls asserted: `RequestTransition`, `BindPawn`, `RemoveParticipant`, `AssignTeams` (with the id in a batch), and the owner-view feed calls `SetOwnerTally` and `SetOwnerRespawnDue` once they exist. Snapshot the store (records, binding table, `EventSeq`) before and after each call and assert it is unchanged; assert no event fires.
- Open point: `SetOwnerTally` and `SetOwnerRespawnDue` are created by story 038, which comes later than this story's only dependency (story 014). Until they exist, assert the other four calls and add the two owner-feed calls to this test in story 038 (or stub them if the core already exposes them). Do not widen this story's dependencies without a plan change.
- Open point: query APIs (`GetState`, `GetTeam`, `GetPawn`) and the calls that return no result code (`NotifyConnectionChanged`, `RecordAppearance`, `RebindView`) have no GDD-defined unknown-id answer. The criterion says "each Id-taking API"; the qa-lead gap asks for a table of API to unknown-id result. Cover only the result-code-returning calls above and raise the table with the GDD owner.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink).
- Prototype disposition: no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (BeginMatch and registration assign ids, handles and reject a full roster): id allocation and retirement through `RemoveParticipant` for the id counter.
- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): the `Rejected_NotAuthority` behaviour.
- Story 003 (Mutation from a worker thread is rejected and faulted): the `Rejected_WrongThread` behaviour.
- Story 024 (Calls carrying an old round epoch or match id are rejected): the `Rejected_StaleEpoch` behaviour that ranks just below this check.
- Story 038 (SetOwnerTally and SetOwnerRespawnDue feed only the owner view with the full precedence): the owner-feed calls and their own precedence list.
- Story 027 (The transition matrix equals the checked-in CSV fixture for every combination): matrix coverage.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if a call lacks the unknown-id check)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if a call lacks the unknown-id check)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionUnknownIdRejectedTest.cpp` (class `FRosterTransitionUnknownIdRejectedTest`, test name `StealthGame.Roster.Transition.UnknownIdRejected`)
- ADR-0007 header naming: no new header is added; any new one uses the `Stealth` prefix and a project-unique file name.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS)**: "each Id-taking API" is not enumerated. Query APIs (GetState, GetTeam, GetPawn) and calls with no defined result (NotifyConnectionChanged, RecordAppearance, RebindView) have no stated unknown-id result. "A retired id" is undefined without saying it is a removed id. Needs a table of API to unknown-id result.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: unregistered and retired ids rejected by every id-taking call that returns a result code.
  - Given: id 99 (never registered) and id 3 (registered then removed), authority true, game thread.
  - When: RequestTransition, BindPawn, RemoveParticipant, SetOwnerTally, SetOwnerRespawnDue called, and AssignTeams with the id in a batch.
  - Then: Rejected_UnknownParticipant and store snapshot unchanged.
  - Edge cases: unknown id ranks below NotAuthority and WrongThread and above StaleEpoch. AssignTeams returns TeamsLocked first if locked.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTransitionUnknownIdRejectedTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-027-transition-exhaustive-matrix.md`
