# Story 002: BeginMatch and registration assign ids, handles and reject a full roster

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
**Also governed by**: ADR-0006: Seeds and deterministic/secret random streams (random source for `MatchId` and `PublicHandle`)
**ADR Decision Summary**: ADR-0005 puts the Roster in a game-instance store whose `BeginMatch` runs when the host opens the session (`Roster.BeginMatch(TeamSize)`) and whose `EndMatch` runs on every exit path, so a former host keeps nothing. Every mutating API asserts authority and the game thread. ADR-0006 supplies the server-only random streams: `PublicHandle` is a secret-domain draw (domain 15, counter-as-Attempt, a draw equal to 0 or to an already-issued handle is discarded) and tests inject entropy through an interface rather than OS entropy.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 (HIGH, networking): no new post-cutoff APIs; S3, S4 and S5 verification is outstanding but this story is pure core logic with no engine call. ADR-0006 (LOW): plain integer arithmetic; the core consumes a random source interface, never `FMath::Rand` or `FRandomStream`.

**Context paragraph**: This story gives `FRosterCore` its match lifecycle entry points: `BeginMatch(TeamSize)` clears the store and draws a fresh `MatchId`, and `RegisterParticipant(IdentityKey, DisplayName)` creates records with monotonic ids and unique, unordered `PublicHandle`s, rejecting registration once `2 x TeamSize` non-Dormant records exist. It realises GDD R3 (field semantics of `ParticipantId`, `PublicHandle`, `MatchId`, `EventSeq`) and R4 (registration, check order, ids never reused).

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Tests inject entropy through `IEntropySource` (ADR-0006)
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Forbidden: Never use `FMath::Rand`, `FMath::FRand`, `FMath::RandRange`, `FRandomStream`, `std::rand`, `<random>` engines or time-seeded randomness in gameplay or server code (ADR-0006)
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests (ADR-0008)
- Guardrail: Random streams: hidden draws well under 0.05 ms/frame (ADR-0006)
- Required (manifest 2026-10-25f, ADR-0005 Technical Setup 2026-10-25b): `FRosterCore::RequestTransition` is the only writer of lifecycle state with the R8 precedence; mutations on the server game thread with FIFO dispatch after the outermost call and EventSeq +1 per event; O(1) allocation-free queries through a pre-sized binding table; client queries return 0/false/NotTargetable(Stale) and fault (re-read at implementation; 2026-10-25g adds nothing for the Roster)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [x] **AC-PSR-05** [Core] `StealthGame.Roster.BeginMatchAndRegister`: GIVEN `BeginMatch(5)`, WHEN keys K1..K10 then K11 register, THEN:
  - ids are 0..9 in order;
  - each new record is `RoundEnded` / `None` / `Connected`;
  - K11 gets `Rejected_RosterFull`;
  - after `RemoveParticipant(3)` and a new key, the new id is 10;
  - a second `BeginMatch` resets `ParticipantId`, `EventSeq` and `RoundEpoch` to 0 and draws a different `MatchId`;
  - every `PublicHandle` is unique, and with the seeded random source the handles are not in id order (R3, R4).

---

## Implementation Notes

*Derived from ADR-0005 section 2, ADR-0006 (PublicHandle domain) and GDD R3, R4:*

- **BeginMatch.** `BeginMatch(TeamSize)` clears all records, draws a new 64-bit `MatchId` through the injected random source, and resets the id counter, `EventSeq` and `RoundEpoch` to 0. `TeamSize` is injected by the caller (Round owns the key); the core reads no config. Values outside 1-5 return `Rejected_InvalidTeamSize` (R4); tests use 1 through a test preset. `SetTeamSize` itself belongs to a later story.
- **RegisterParticipant result type.** Return a result code struct/enum carrying `Registered(Id)`, `Reclaimed(Id)`, `Rejected_TeamsLocked`, `Rejected_RosterFull` and `Rejected_NotAuthority` (R4). Check order is fixed: `Rejected_NotAuthority`, then the reclaim match, then `Rejected_TeamsLocked`, then `Rejected_RosterFull`. This story implements the new-key path and `Rejected_RosterFull`; the locked and reclaim branches get their behaviour in stories 005 and 029 (leave clearly marked extension points, do not stub wrong behaviour).
- **Roster capacity** is `2 x TeamSize` **non-Dormant** records; Dormant records do not hold a slot (R4, Session F1). Dormant records cannot exist until story 009, so the count here is simply the record count.
- **Id rules.** `ParticipantId` is monotonic per match and never reused (R3): after `RemoveParticipant(3)` the next new key receives id 10, not 3. `RemoveParticipant(Id)` is only needed here to the extent AC-PSR-05 calls it while teams are unlocked; its locked behaviour is story 005.
- **New record defaults.** `State = RoundEnded`, `Team = None`, `Connection = Connected` (R4).
- **PublicHandle.** A random 32-bit value drawn from the injected random source, unique within the match, never reused, carrying no order; per ADR-0006 a draw equal to 0 or to any handle already issued in the match is discarded and the registration counter advances. The test needs a seed whose first 10 handles are not ascending, so the unordered assertion is meaningful.
- **No event on rejection; no EventSeq consumed.** Registration events belong to story 008; here only assert that a rejected call consumes no `EventSeq`.
- **Injected dependencies** come from `StealthRosterDeps.h` (story 001): the random source is `IRosterRandom` (test fixture wraps a deterministic entropy source), and rejections report to the fault sink only where the GDD says (none here).
- **Open point:** ADR-0006 registers `PublicHandle` as secret domain 15 but lists no domain row for the random 64-bit `MatchId` (R3); the GDD only requires "a server-only random source". Treat `IRosterRandom` as the single seam for both draws and flag a possible ADR-0006 amendment for `MatchId` when the facade is wired.
- **Prototype disposition:** replaces the participant bookkeeping in `Source/StealthGame/Private/StealthGameMode.cpp` (`PostLogin` team and slot choice via `FStealthRules::ChooseTeamForNewPlayer`, which is reference logic only; team assignment itself is story 006). No code is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001 (Participant record fields equal the R3 whitelist exactly): the record type, dependency interfaces and id types
- Story 003 (Mutation from a worker thread is rejected and faulted) and Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): wrong-thread and not-authority behaviour
- Story 005 (A locked roster rejects registration, removal and team changes but reclaims known keys): locked behaviour of registration and removal, `SetTeamsLocked`
- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): `OnParticipantRegistered` and the event queue
- Story 029 (Reclaim restores the same record for active and informant participants): the reclaim path

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
- `Source/StealthFoundation/Private/Tests/Roster/StealthBeginMatchAndRegisterTest.cpp` (test `StealthGame.Roster.BeginMatchAndRegister`)
- `docs/architecture/adr-0006-seeds-random-streams.md` (amendment: domain 17 `MatchId`, decision D46)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: BeginMatch(5) and registration assign ids and handles, reject a full roster.
  - Given: BeginMatch(5), random source seeded per test, fault sink, a seed whose first 10 handles are not ascending.
  - When: K1..K10 then K11 register; then RemoveParticipant(3) and a new key register; then BeginMatch runs a second time.
  - Then: ids 0..9 in order, each record RoundEnded/None/Connected. K11 gets Rejected_RosterFull. New key after the removal gets id 10. Second BeginMatch resets ParticipantId, EventSeq, RoundEpoch to 0 and draws a MatchId different from the first. All handles unique and not in id order.
  - Edge cases: EventSeq non-zero before the second BeginMatch so the reset is observable; the rejection consumes no EventSeq.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthBeginMatchAndRegisterTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [x] Created and passing (`StealthGame.Roster.BeginMatchAndRegister`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-001-record-schema-whitelist.md`
- Unlocks: `production/epics/participant-state-roster/story-004-client-calls-rejected.md`, `production/epics/participant-state-roster/story-005-locked-roster-is-frozen.md`

---

## Completion Notes
**Completed**: 2026-10-25 (participant-state-roster batch 001-014: one core built and closed together)
**Criteria**: 1/1 passing (AC-PSR-05: ids 0..9; RoundEnded/None/Connected; K11 Rejected_RosterFull with no EventSeq; id 10 after RemoveParticipant(3); second BeginMatch resets id/EventSeq/RoundEpoch and draws a different MatchId; handles unique, non-zero and not in id order with the seeded source; BeginMatch(0)/(6) Rejected_InvalidTeamSize)
**Decisions**: `IRosterRandom::Draw(Domain, Counter)` is the single seam for both draws; `ERosterRandomDomain` values equal ADR-0006 `ESeedDomain` (15 PublicHandle; 17 MatchId added to ADR-0006 per decision D46). Handle draws discard 0 and repeats (counter advances); MatchId draws discard 0 and the previous match's id with a lifetime counter.
**Deviations**: One shared `ERosterResult` enum for every call instead of per-call result enums. Manifest Version updated to 2026-10-25g.
**Test Evidence**: `production/qa/evidence/story-002-begin-match-and-register/full-suite.txt` (build `Result: Succeeded`; 84/84 `StealthGame.` tests Success, 70 existing + 14 Roster) and `stamp.txt`
**Run result**: N/A — plain-C++ server core with no world, actor or screen; nothing a player can see until the facade (story 061) and the C-TeamView push step (story 062) exist
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed (StrikeLimit release aliasing, story 014); `TryReclaim`, `DisposeLivePawnOnElimination` and `EmitStateChanged` extracted to keep methods short; `RequestTransition` kept as one function by design (the R8 precedence in one place)
