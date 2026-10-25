# Story 025: Undersized rosters count correctly and lock and reset preconditions hold

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story verifies that the Roster contains no "full team" assumption: 2v2, 4v3 and 1v0 rosters count correctly under F1 to F4, an empty team is never eliminated, and the two lock preconditions hold (`SetTeamsLocked(true)` refuses while a non-Dormant record has no team, and `ResetAll` refuses while teams are unlocked, so a `Team = None` record can never become `Active`). It realises GDD rules R5 (lock, undersized rosters), F1 to F4 (counts and the elimination predicate), I2, and the Edge Cases for 1v0, 2v2 and 4v3.

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: The Roster is one of four game-instance stores and the only authoritative owner of participant records, bindings, `MatchId`, `RoundEpoch` and `EventSeq`; its logic lives in the plain-C++ `FRosterCore` behind a thin `URosterStore` facade, and `TeamSize` is injected into `BeginMatch` by its caller so the core reads no config. The store survives seamless and hard travel as a game-instance subsystem, and match state is keyed by `ParticipantId`, never by controller or PlayerState.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking domain (HIGH per VERSION.md). No new post-cutoff APIs; new logging uses `UE_LOGF` (ADR-0007). Verification required by ADR-0005 is behavioural: spikes S3, S4 and S5 (not yet reported). This story runs entirely inside the engine-free `FRosterCore` and is not gated by any of them.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: Tests: per-commit pipeline <= 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-10** [Core] `StealthGame.Roster.UndersizedRostersAndLock`: GIVEN 2v2, 4v3 and 1v0 rosters, WHEN F1–F4 are evaluated with everyone `Active`, THEN `(ActiveCount, RosterCount)` per team equals (2,2)/(2,2), (4,4)/(3,3) and (1,1)/(0,0), and `IsTeamEliminated(emptyTeam)` = false. GIVEN one non-Dormant record with `Team = None`, WHEN `SetTeamsLocked(true)` is called, THEN the result is `Rejected_UnassignedParticipants`. GIVEN teams unlocked, WHEN `ResetAll` is called, THEN `Rejected_TeamsUnlocked` and no record is `Active` (R5, R7, F4, I2).

---

## Implementation Notes

*Derived from ADR-0005 section 2 and GDD rules R4, R5, R7, F1 to F4, I2:*

- Counts: `ActiveCount(T)`, `RosterCount(T)` and `IsTeamEliminated(T)` are computed over `InRound` records only (F1, F3, F4); `IsTeamEliminated(T) = (RosterCount(T) >= 1) AND (ActiveCount(T) = 0)` and is false outside Round phase `Active`. Dormant records are excluded; before the first `ResetAll`, `RosterCount(T)` counts non-Dormant records with `Team = T` instead (F3).
- 1v0 uses `BeginMatch(1)` (valid `TeamSize` range is 1 to 5, R4; the test preset uses 1 even though Tuning Data & Config's soft range is 2 to 5). With one participant on team 0 and none on team 1, `IsTeamEliminated(1)` must be false because the team is empty.
- `SetTeamsLocked(true)` returns `Rejected_UnassignedParticipants` while any non-Dormant record has `Team = None` (I2); a Dormant record with `Team = None` does not block it. `ResetAll` while teams are unlocked returns `Rejected_TeamsUnlocked`, leaves every record untouched (none `Active`) and emits no event.
- Open point (cross-cutting qa-lead gap): the GDD's injected dependency list has no Round-phase input, but `IsTeamEliminated` is defined only in phase `Active`. The test needs the phase to be `Active` or the check is trivially false. Provide a phase seam on the core (compiled only under `WITH_STEALTH_TEST_SEAMS`) or an injected phase provider; the same seam serves stories 012, 035 and 050. Do not add a production dependency on Round headers (ADR-0007: lower layers declare the interfaces they consume).
- Fixture: three rosters built through registration (story 002), team assignment (stories 005 to 007), `SetTeamsLocked(true)` and `ResetAll` (story 022). The lock and reset preconditions run on separate fresh fixtures.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink).
- Prototype disposition: `FStealthRules::ChooseTeamForNewPlayer` and the per-team counts in `Source/StealthGame/Private/StealthGameMode.cpp` are reference logic only; the counts are re-expressed in the core (F1 to F3) and re-tested here, nothing is ported verbatim.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event): the `ResetAll` batch effects; this story only asserts its unlocked precondition.
- Story 005 (A locked roster rejects registration, removal and team changes but reclaims known keys): the behaviour of a locked roster.
- Story 006 (A batch swap of two players across full teams applies atomically): `AssignTeams` capacity validity.
- Story 034 (A scripted 200-step sequence keeps F1-F3 and I1-I3 equal to a brute-force recount): count conservation across a long sequence.
- Story 035 (The elimination trigger fires only for a strike-out win and never for disconnect or empty sides): the F4 trigger.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h`
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (count queries, `SetTeamsLocked` precondition, `ResetAll` unlocked rejection)
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (round-phase provider or seam, if added here)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterUndersizedRostersAndLockTest.cpp` (class `FRosterUndersizedRostersAndLockTest`, test name `StealthGame.Roster.UndersizedRostersAndLock`)
- ADR-0007 header naming: `Stealth` prefix and project-unique file names; no new header beyond the shared roster headers.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: undersized rosters count correctly; lock and reset preconditions hold.
  - Given: 2v2, 4v3, 1v0 rosters (BeginMatch(1) for 1v0), all Active after lock and ResetAll, Round phase Active through the phase seam.
  - When: CountActive, CountRoster, IsTeamEliminated(emptyTeam) evaluated. Separately SetTeamsLocked(true) with one non-Dormant Team = None record. Separately ResetAll while unlocked.
  - Then: counts (2,2)/(2,2), (4,4)/(3,3), (1,1)/(0,0); the empty team not eliminated. The lock returns Rejected_UnassignedParticipants. ResetAll returns Rejected_TeamsUnlocked and no record is Active.
  - Edge cases: phase must be Active or the eliminated check is trivially false.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterUndersizedRostersAndLockTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-022-reset-all-batch.md`, `production/epics/participant-state-roster/story-005-locked-roster-is-frozen.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-034-counts-and-conservation.md`
