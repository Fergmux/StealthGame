# Story 034: A scripted 200-step sequence keeps F1-F3 and I1-I3 equal to a brute-force recount

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-001`, `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0005 makes the Roster one of four game-instance stores whose plain-C++ core (`FRosterCore`) holds the `ParticipantRecord`s, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`, and is the only authoritative match state. Every mutating API asserts authority and the game thread, and state is keyed by `ParticipantId`, never by controller or PlayerState. That makes the core testable headlessly against a brute-force recount of its own records.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 uses no new post-cutoff API (note the 5.6 change: `DispatchPostLogin` is deprecated, use `PostLogin`/`OnPostLogin`; `UE_LOGF` for logging). Its verification items are spikes S3, S4 and S5 (not yet reported); none gates this pure-core counting story.

**Control Manifest Rules (this layer)**:
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Every Logic story ships T1 tests; T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them — source: ADR-0005
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail — source: ADR-0008
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (count queries are O(roster size, at most 10)) — source: ADR-0002

This story proves, over one fixed scripted sequence of 200 steps, that the Roster's published counts (`CountActive`, `CountInformant`, `AwaitingPawn`, `CountRoster`) always equal an independent brute-force recount of the record snapshots, and that invariants I1 to I3 hold after every step. It exists because the count formulas drive elimination (F4) and the HUD and must never drift from the records. It realises GDD F1, F2, F3 and invariants I1, I2, I3.

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-29** [Core] `StealthGame.Roster.CountsAndConservation`: GIVEN a fixed scripted 200-step sequence (register, assign, transition, bind, retire, disconnect, reset, end round), WHEN each step is applied, THEN:
  - F1–F3 equal a brute-force recount;
  - I1–I3 hold and `AwaitingPawn` ≤ `InformantCount`;
  - a Disconnected Active participant counts in F1;
  - the F1 and F2 worked examples reproduce exactly (3; 2 and 1).

---

## Implementation Notes

*Derived from ADR-0005 section 2, ADR-0007 section 5 and GDD F1-F3, I1-I3:*

- **Counts under test.** F1 `ActiveCount(T)`, F2 `InformantCount(T)` and `AwaitingPawn(T)` (Informant with no live pawn), F3 `RosterCount(T)` = `InRound` records on team T; outside a round (before the first `ResetAll`, all `InRound` false) `RosterCount(T)` counts non-Dormant records with `Team = T`. Connection state is ignored by F1: a Disconnected Active participant counts until Round transitions them. The core exposes these as `CountActive`, `CountInformant`, `AwaitingPawn`, `CountRoster` (R10; story 025 asserts them for undersized rosters, so reuse its implementation rather than writing a second one).
- **Brute-force recount.** The test owns an independent recount that reads record snapshots (a const snapshot accessor on the core, test seam only) and recomputes F1 to F3 and the invariants without calling the production count methods. The two must be written independently, otherwise the test only compares a function with itself.
- **Invariants checked after every step.** I1: record count equals registrations minus removals since `BeginMatch`, one state per record, per team `Active + Informant + RoundEnded = RosterCount`, `RosterCount` constant while locked, a Dormant record never `Active` or `Informant`. I2: while locked, no non-Dormant record has `Team = None`, and no `Team = None` record is `Active` or `Informant`. I3: a participant has at most one live pawn and a pawn is bound to at most one participant. Plus `AwaitingPawn(T) <= InformantCount(T)`.
- **The scripted step list.** Embed a fixed 200-entry step table in the test source (a `constexpr` array of op, participant, argument), never a seeded random generator and never file I/O (determinism and unit-test isolation rules). Cover register, assign teams, lock, `ResetAll`, `RequestTransition` (each entering reason), `BindPawn`, `RetireBinding`, `ReleaseToCrowd`, `NotifyConnectionChanged` (Disconnected and back), `EndRoundAll`, removal in Lobby. Include a stage where one Active participant is Disconnected and verify it still counts in F1.
- **Worked examples.** Reproduce the F1 example exactly (team 0 = A Active, B Active, C Informant, D Active and Disconnected, E Informant gives `ActiveCount(0) = 3`) and the F2 example (`InformantCount(0) = 2`, `AwaitingPawn(0) = 1` after C's binding is retired and E walks) at dedicated steps in the script.
- **Phase seam.** Counts do not need the Round phase; `IsTeamEliminated` (story 025) does and is outside this story.
- **Prototype disposition:** no direct prototype counterpart. The prototype keeps per-team counts in `Source/StealthGame/Private/StealthGameMode.cpp` (per-team player counts and active counts passed to `FStealthRules::EvaluateEliminationWinner`); those ad hoc counts are replaced by the core's F1 to F3 and are not ported.
- **Header naming (ADR-0007).** Stealth prefix, project-unique names: `StealthRosterCore.h`, `StealthRosterTypes.h`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 025 (Undersized rosters count correctly and lock and reset preconditions hold): owns the count implementation's undersized cases (2v2, 4v3, 1v0), `SetTeamsLocked` and `ResetAll` preconditions.
- Story 023 (EndRoundAll ends all records once and is idempotent): owns `EndRoundAll`; this story only uses it as a step.
- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event): owns `ResetAll`.
- Story 035 (The elimination trigger fires only for a strike-out win and never for disconnect or empty sides): owns F4.
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): owns the transition steps used in the script.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (const record snapshot accessor under `WITH_STEALTH_TEST_SEAMS`)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterCountsAndConservationTest.cpp` (class `FRosterCountsAndConservationTest`, test name `StealthGame.Roster.CountsAndConservation`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Logic):**

- **AC-1**: a scripted 200-step sequence keeps counts and invariants equal to a brute-force recount.
  - Given: a checked-in 200-step list (register, assign, transition, bind, retire, disconnect, reset, end round) and a brute-force recount that reads record snapshots.
  - When: each step applied.
  - Then: after every step F1 to F3 equal the recount, I1 to I3 hold, AwaitingPawn <= InformantCount. A Disconnected Active participant counts in F1. The F1 example gives 3; the F2 example gives InformantCount 2 and AwaitingPawn 1.
  - Edge cases: use a fixed step list, not a seeded random generator, per the determinism rule.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterCountsAndConservationTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-025-undersized-rosters-and-lock.md`, `production/epics/participant-state-roster/story-023-end-round-all-batch-idempotent.md`
- Unlocks: None
