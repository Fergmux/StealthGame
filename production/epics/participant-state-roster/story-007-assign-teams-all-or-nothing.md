# Story 007: An invalid team batch changes nothing and fires no event

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0005 holds all match-scoped state in the game-instance Roster store and requires every mutating API to assert authority and the game thread, with misuse reported rather than half-applied. For team assignment that means Session's batch either lands whole or is refused whole, so the store never exposes a half-assigned roster to Round or to the C-TeamView push step.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 (HIGH, networking): no new post-cutoff APIs; S3, S4 and S5 verification is outstanding but this story is pure core logic with no engine call.

**Context paragraph**: This story proves the rejection side of `AssignTeams`: a batch with six players on one team, an unknown id, a duplicate id or a team value of 2 returns `Rejected_TeamFull`, `Rejected_UnknownParticipant`, `Rejected_InvalidBatch` and `Rejected_InvalidBatch` respectively, leaves the store byte-for-byte unchanged and fires no `OnTeamsChanged`. It realises GDD R5 (result order and all-or-nothing) and F5 (`Valid(B)`).

**Control Manifest Rules (this layer)**:
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests (ADR-0008)
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail (ADR-0008)
- Guardrail: Tests: per-commit pipeline <= 10 min (ADR-0008)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-09** [Core] `StealthGame.Roster.AssignTeamsAllOrNothing`: GIVEN batches with 6 on one team, an unknown id, a duplicate id or team 2, WHEN each is applied, THEN it returns `Rejected_TeamFull` / `Rejected_UnknownParticipant` / `Rejected_InvalidBatch` respectively, the store is unchanged, and no `OnTeamsChanged` fires (R5, F5).

---

## Implementation Notes

*Derived from ADR-0005 section 2 and GDD R5, F5:*

- **Validate fully, then write.** Story 006 implements the final-state validation; this story hardens it with the invariant "any rejection returns before the first write". If validation is not already a pure function over a view of the records, refactor it into one (a `ValidateTeamBatch` returning a result and no side effects) so the all-or-nothing property is structural, not a rollback.
- **Result order** (R5): locked, unknown id, invalid batch (duplicate id, team not in {0, 1}, empty batch), team full. Each QA batch carries a single defect so the order does not mask the intended code (6 on one team gives TeamFull only because no earlier check fails; a team value of 2 is `Rejected_InvalidBatch`, not TeamFull).
- **Empty batch.** F5 says an empty batch returns `Rejected_InvalidBatch`; it is outside this AC's four cases, so cover it only if cheap, and do not let it replace one of the four.
- **No-event assertion.** The event queue arrives in story 008, which depends on this story. Until then assert "no `OnTeamsChanged`" with a spy listener stub on the core's team-changed notification seam and assert `EventSeq` is unchanged; story 008 re-verifies the same clause against the real queue (the plan notes the "no event fires" clause is verified there too). Keep the spy's hook minimal so story 008 can replace it without touching this test's other assertions.
- **State snapshot.** Compare the full core snapshot (records, teams, counters) before and after each batch; reuse the snapshot helper from story 003.
- **Prototype disposition:** No prototype counterpart. The prototype `SwitchTeam` (StealthGameMode.cpp) applied one move at a time with no batch validation; it is superseded by stories 006 and 007.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 006 (A batch swap of two players across full teams applies atomically): the success path and the swap
- Story 005 (A locked roster rejects registration, removal and team changes but reclaims known keys): `Rejected_TeamsLocked` for `AssignTeams`
- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): the real event queue and `OnTeamsChanged` for applied batches
- Story 025 (Undersized rosters count correctly and lock and reset preconditions hold): team counts for undersized rosters

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (changed: pure `ValidateTeamBatch`, spy-able team-changed notification seam)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (changed)
- `Source/StealthFoundation/Private/Tests/Roster/StealthAssignTeamsAllOrNothingTest.cpp` (new: class `FRosterAssignTeamsAllOrNothingTest`, test name `StealthGame.Roster.AssignTeamsAllOrNothing`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: invalid team batch changes nothing and fires no event.
  - Given: 10 participants, teams unlocked, store snapshot, spy listener on OnTeamsChanged (until story 008's queue exists).
  - When: four batches each with a single defect: 6 on one team, unknown id, duplicate id, team value 2.
  - Then: Rejected_TeamFull, Rejected_UnknownParticipant, Rejected_InvalidBatch, Rejected_InvalidBatch respectively. Snapshot unchanged, spy sees zero calls, EventSeq unchanged.
  - Edge cases: keep each batch single-defect so R5 order (locked, unknown, invalid, full) does not mask it. Empty batch is Rejected_InvalidBatch per F5 but outside this criterion.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthAssignTeamsAllOrNothingTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-006-assign-teams-atomic-swap.md`
- Unlocks: `production/epics/participant-state-roster/story-008-registration-and-team-events.md`
