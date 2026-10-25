# Story 005: A locked roster rejects registration, removal and team changes but reclaims known keys

> **Epic**: Participant State & Roster
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0005 makes the Roster store the only authoritative match state, with `RosterCount(T)` and the record set stable for the whole round so Session and Round can rely on them across seamless travel. Admission is gated by Session, and the Roster enforces only capacity and the lock; the identity of a returning participant is decided by the Roster key alone (no address-keyed reclaim, no inactive-PlayerState reuse). A locked roster therefore turns away strangers and removals while still letting a known participant back in.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 (HIGH, networking): no new post-cutoff APIs; `DispatchPostLogin` is deprecated since 5.6 (use `PostLogin`/`OnPostLogin`) and `UE_LOGF` is the logging macro. S3, S4 and S5 verification is outstanding but this story is pure core logic.

**Context paragraph**: Round locks teams once per round (`SetTeamsLocked(true)`), and from then on `RosterCount(T)` must not change (I1, I2). This story adds the lock flag, `SetTeamsLocked`, `SetTeamSize` with its capacity checks, `RemoveParticipant` and the locked branches of `RegisterParticipant` and `AssignTeams` so that all of them return `Rejected_TeamsLocked`, while a known non-Dormant key is still reclaimed. It realises GDD R4 (registration check order, `RemoveParticipant` legality, `SetTeamSize`), R5 (lock semantics, `Rejected_UnassignedParticipants`) and F3.

**Control Manifest Rules (this layer)**:
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Forbidden: Never derive from `AGameMode`; never use `AddInactivePlayer`/`FindInactivePlayer` or address-keyed reclaim (ADR-0005)
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` (ADR-0005)
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail (ADR-0008)
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)
- Required (manifest 2026-10-25f, ADR-0005 Technical Setup 2026-10-25b): `FRosterCore::RequestTransition` is the only writer of lifecycle state with the R8 precedence; mutations on the server game thread with FIFO dispatch after the outermost call and EventSeq +1 per event; O(1) allocation-free queries through a pre-sized binding table; client queries return 0/false/NotTargetable(Stale) and fault (re-read at implementation; 2026-10-25g adds nothing for the Roster)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [x] **AC-PSR-06** [Core] `StealthGame.Roster.LockedRosterIsFrozen`: GIVEN teams locked, WHEN a new key registers, or a Dormant record's key registers, or `RemoveParticipant`, `AssignTeams` or `SetTeamSize` is called, THEN each returns `Rejected_TeamsLocked`, `RosterCount(T)` is unchanged, and a known non-Dormant key still returns `Reclaimed` (R4, R5, F3).

---

## Implementation Notes

*Derived from ADR-0005 section 2 and Implementation Guidelines, and GDD R4, R5, F3:*

- **Lock state.** Add the `bTeamsLocked` flag and `SetTeamsLocked(bool)` to `FRosterCore`. `SetTeamsLocked(true)` returns `Rejected_UnassignedParticipants` while any non-Dormant record has `Team = None` (enforces I2; that precondition is asserted in story 025), and is a no-op when already locked. Round unlocks teams only on arrival in `Lobby`.
- **RegisterParticipant check order** (R4, fixed): `Rejected_NotAuthority`, then the reclaim match, then `Rejected_TeamsLocked`, then `Rejected_RosterFull`. Locked means: the key is new, **or** matches a `Dormant` record, returns `Rejected_TeamsLocked`. A known **non-Dormant** key must reach the reclaim branch before the lock check and return `Reclaimed(Id)`. This story implements only the reclaim *match and return* needed for the AC; the full restoration semantics (Connection, pawn, history) are story 029. Do not duplicate reclaim logic: leave one `TryReclaim` seam.
- **RemoveParticipant(Id).** Legal only while teams are unlocked (explicit Lobby removal and `EndMatch`); otherwise `Rejected_TeamsLocked`. The id is retired, never reused (R4; story 002 relies on it).
- **AssignTeams under lock.** Only the first check of `AssignTeams` matters here: when locked it returns `Rejected_TeamsLocked` before any other check (R5 order: locked, unknown, invalid, full). The atomic batch itself is story 006 (which depends on this story), so add `AssignTeams` here with only the locked branch, and let the valid path return `Rejected_InvalidBatch` until story 006 replaces it (say so in the code comment). The 2v2 fixture therefore assigns teams through the same core test seam as the Dormant record, not through `AssignTeams`.
- **SetTeamSize.** While teams are locked the value is frozen and the AC says the result is `Rejected_TeamsLocked`. R4 names no locked result code for `SetTeamSize` (it names `Rejected_TeamSizeBelowRoster` and `Rejected_InvalidTeamSize` only); the AC is authoritative here. Also implement the unlocked checks of R4: a value >= the largest team's non-Dormant record count, else `Rejected_TeamSizeBelowRoster`; values outside 1-5 `Rejected_InvalidTeamSize`.
- **Dormant record fixture.** The AC needs a Dormant record, but the Lobby projection that creates one is story 009. Build the Dormant record through a core test seam (compiled only under `WITH_STEALTH_TEST_SEAMS`, never into Shipping, ADR-0008) until story 009 replaces it. Open point: R4 names no locked result code for `SetTeamSize`; assert exactly as the AC states and record the GDD gap.
- **Locked implies RosterCount stable.** The test compares `RosterCount(T)` (F3) before and after each rejected call; `RosterCount` is the count of `InRound`-agnostic non-Dormant records per team at this stage (the `InRound` refinement arrives with story 022).
- **Prototype disposition:** No prototype counterpart for the lock; the prototype `AStealthGameMode` had no team lock, and its `SwitchTeam` and per-team counts (replaced in story 006) were unguarded.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (BeginMatch and registration assign ids, handles and reject a full roster): the unlocked new-key path and `Rejected_RosterFull`
- Story 006 (A batch swap of two players across full teams applies atomically) and Story 007 (An invalid team batch changes nothing and fires no event): the validity and atomicity of `AssignTeams`
- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): events for these operations
- Story 009 (Session connection states project onto Connection fields only): the real Dormant projection
- Story 025 (Undersized rosters count correctly and lock and reset preconditions hold): the `Rejected_UnassignedParticipants` assertion
- Story 029 (Reclaim restores the same record for active and informant participants): the full reclaim semantics

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (`FRosterCore`, `FParticipantRecord` and its X-macro field list, result/event/call enums, core test seams under `WITH_STEALTH_TEST_SEAMS`)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (pawn handle, `IRosterPawnLiveness`, `IRosterAuthority`, `IRosterThreadCheck`, `IRosterRandom`, `IRosterRoundPhase` (D43), `FRosterCoreDeps`; clock and fault sink reuse `IStealthClock`/`IStealthFaultSink`)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (`FParticipantId`, `FIdentityKey`, `EParticipantState`, `ERosterTeam`)
- `Source/StealthFoundation/Public/Server/StealthFaultSink.h` (appended `RosterNotAuthority`=5, `RosterWrongThread`=6, `RosterLiveBindingCleared`=7, `RosterUnrecognisedState`=8)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterTestFixture.h` (shared fakes and fixture; reuses `FFakeStealthClock`, `FStealthRecordingFaultSink`)
- `Source/StealthFoundation/Private/Tests/Roster/StealthLockedRosterIsFrozenTest.cpp` (test `StealthGame.Roster.LockedRosterIsFrozen`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: locked roster rejects registration, removal, team changes; known non-Dormant keys reclaimed.
  - Given: 2v2 assigned teams, SetTeamsLocked(true), one Dormant record built through a core test seam until story 009.
  - When: a new key registers, the Dormant key registers, RemoveParticipant, AssignTeams, SetTeamSize called.
  - Then: each returns Rejected_TeamsLocked, RosterCount(T) unchanged, a known non-Dormant key returns Reclaimed.
  - Edge cases: R4 names no locked result code for SetTeamSize, only the AC does; assert per the AC. Registration check order: reclaim match, then locked.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthLockedRosterIsFrozenTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [x] Created and passing (`StealthGame.Roster.LockedRosterIsFrozen`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-002-begin-match-and-register.md`
- Unlocks: `production/epics/participant-state-roster/story-006-assign-teams-atomic-swap.md`, `production/epics/participant-state-roster/story-008-registration-and-team-events.md`, `production/epics/participant-state-roster/story-025-undersized-rosters-and-lock.md`

---

## Completion Notes
**Completed**: 2026-10-25 (participant-state-roster batch 001-014: one core built and closed together)
**Criteria**: 1/1 passing (AC-PSR-06: locked: new key, Dormant key, RemoveParticipant, AssignTeams, SetTeamSize each Rejected_TeamsLocked with RosterCount(0/1) unchanged; known non-Dormant key Reclaimed with the same id; plus the unlocked SetTeamSize checks and Rejected_UnassignedParticipants)
**Decisions**: Dormant record built through the real Lobby projection (story 009, same batch) instead of a Dormant seam. The AC wins over R4 for the locked SetTeamSize code (GDD gap: R4 names none). `SetTeamsLocked(true)` when locked is NoOp_Unchanged. Single `TryReclaim` seam for story 029. RemoveParticipant order: UnknownParticipant before TeamsLocked.
**Deviations**: Teams assigned through the real `AssignTeams` (story 006, same batch) instead of the seam. Manifest Version updated to 2026-10-25g.
**Test Evidence**: `production/qa/evidence/story-005-locked-roster-is-frozen/full-suite.txt` (build `Result: Succeeded`; 84/84 `StealthGame.` tests Success, 70 existing + 14 Roster) and `stamp.txt`
**Run result**: N/A — plain-C++ server core with no world, actor or screen; nothing a player can see until the facade (story 061) and the C-TeamView push step (story 062) exist
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed (StrikeLimit release aliasing, story 014); `TryReclaim`, `DisposeLivePawnOnElimination` and `EmitStateChanged` extracted to keep methods short; `RequestTransition` kept as one function by design (the R8 precedence in one place)
