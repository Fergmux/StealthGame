# Story 006: A batch swap of two players across full teams applies atomically

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
**ADR Decision Summary**: ADR-0005 keeps match-scoped team state in the Roster store (a plain-C++ `FRosterCore` behind `URosterStore`) and nowhere else, so Session's team requests reach the Roster as a single batch call and every other system holds only a `ParticipantId`. The Roster enforces capacity and the lock; Round owns size and balance rules. Because teams are authoritative only in the store, a batch must be validated against the final state so a full-roster swap is expressible.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 (HIGH, networking): no new post-cutoff APIs; S3, S4 and S5 verification is outstanding but this story is pure core logic with no engine call.

**Context paragraph**: This story implements the valid path of `AssignTeams(Batch[(Id, Team)])`, validated against the final state per formula F5, so that swapping A (team 0) with F (team 1) in a full 5/5 roster succeeds in one batch while the same two moves sent one at a time fail on the first. It realises GDD R5 (atomic batch) and F5 (`Valid(B)`, `Free(T)`).

**Control Manifest Rules (this layer)**:
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Forbidden: Never put team, state, connection, appearance or pawn data on a PlayerState; never give NPCs a PlayerState (ADR-0001)
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them (ADR-0005)
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-08** [Core] `StealthGame.Roster.AssignTeamsAtomicSwap`: GIVEN teams 5/5, WHEN one batch swaps A (team 0) with F (team 1), THEN `Applied` and still 5/5. WHEN the same two moves are sent one at a time, THEN the first returns `Rejected_TeamFull` (F5).

---

## Implementation Notes

*Derived from ADR-0005 section 2 and GDD R5, F5:*

- **Validate against the final state, then apply.** Compute `Team'(p)` for every non-Dormant record by overlaying the batch on the current teams, check `|{p : Team'(p) = T}| <= TeamSize` for both teams on that final view (F5), and only then write. Never mutate while validating; a rejected batch changes nothing (story 007 asserts this).
- **Check order** (R5): `Rejected_TeamsLocked`, `Rejected_UnknownParticipant`, `Rejected_InvalidBatch` (duplicate id, team not in {0, 1}, or an empty batch per F5), `Rejected_TeamFull`, else `Applied`. This story needs only the capacity branch and the success path; stories 005 and 007 own the lock and the other rejections, but implement the whole order once here so the order cannot drift.
- **Single-move case.** A one-entry batch moving A to team 1 while team 1 is full evaluates to 6 on team 1 and returns `Rejected_TeamFull` (the stepwise failure the AC requires). The same code path, no special case.
- **Dormant records** are excluded from the capacity count (F5 counts non-Dormant records).
- **Fixture.** `BeginMatch(5)`, ten registered participants, teams assigned 5/5 and unlocked. Build the 5/5 assignment through `AssignTeams` itself when it fits (first batch assigns all ten), or through the core test seam from story 005; a rejected move leaves the 5/5 fixture intact, so the stepwise run must start from a fresh 5/5 fixture, not the post-swap state.
- **No event assertion here.** `OnTeamsChanged` for an `Applied` batch is story 008 (the event queue); do not add listener plumbing in this story.
- **Prototype disposition:** replaces the `SwitchTeam` and per-team count bookkeeping in `Source/StealthGame/Private/StealthGameMode.cpp` (`AStealthGameMode::SwitchTeam`, `PlayersPerTeam` counts). `FStealthRules::ChooseTeamForNewPlayer` (StealthRules.cpp) is reference logic only; team choice policy belongs to Round and Session, the Roster only enforces capacity and the lock.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005 (A locked roster rejects registration, removal and team changes but reclaims known keys): the lock and the `Rejected_TeamsLocked` branch of `AssignTeams`
- Story 007 (An invalid team batch changes nothing and fires no event): the rejection cases and the no-change guarantee
- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): `OnTeamsChanged` emission
- Story 025 (Undersized rosters count correctly and lock and reset preconditions hold): counts for 2v2, 4v3 and 1v0 rosters

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (changed: `AssignTeams` valid path, final-state validation helper)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (changed)
- `Source/StealthFoundation/Private/Tests/Roster/StealthAssignTeamsAtomicSwapTest.cpp` (new: class `FRosterAssignTeamsAtomicSwapTest`, test name `StealthGame.Roster.AssignTeamsAtomicSwap`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: batch swap across full teams applies atomically.
  - Given: BeginMatch(5), 10 participants assigned 5/5, teams unlocked.
  - When: one batch swaps A (team 0) with F (team 1). On a fresh 5/5 fixture, the same two moves sent as two single-entry batches.
  - Then: batch returns Applied, counts stay 5/5. First single move returns Rejected_TeamFull.
  - Edge cases: stepwise run starts from 5/5 not post-swap; a rejected move leaves store unchanged.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthAssignTeamsAtomicSwapTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-005-locked-roster-is-frozen.md`
- Unlocks: `production/epics/participant-state-roster/story-007-assign-teams-all-or-nothing.md`
