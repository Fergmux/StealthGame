# Story 008: Registration, team, removal and reclaim each fire exactly one sequenced event

> **Epic**: Participant State & Roster
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0002 fixes one server-side total order for every interacting state change, drained once per frame on the game thread, and counts "Roster flush" inside its 0.5 ms listen-host budget; events are ordered only by keys, never by feature-local clocks or tick order. ADR-0005 makes the Roster store the only authoritative match state and says other systems learn of changes through Roster events keyed by `ParticipantId`. This story builds the Roster's own FIFO event queue so that listeners always see applied events in one strict `EventSeq` order, dispatched after the outermost mutating call returns.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0002 (MEDIUM): no post-cutoff APIs; `UE_LOGF` is preferred for diagnostics, but Event Log content never passes through logging. The ADR's verification items (drain spawn, client estimator, drain once per frame on the listen host) concern the queue and clock, not the plain-C++ Roster event queue. ADR-0005 (HIGH): S3, S4 and S5 outstanding, untouched here.

**Context paragraph**: This story owns the Roster's event infrastructure: the FIFO event queue, the roster-wide `EventSeq` counter (+1 per emitted event of any type, never for no-ops or rejections) and dispatch after the outermost mutating call. It then uses that infrastructure for the first four events, `OnParticipantRegistered`, `OnTeamsChanged`, `OnParticipantRemoved` and `OnParticipantReclaimed`. It realises GDD R12 (events, FIFO, outermost-call dispatch), R3 (`EventSeq`), R4 and R5 (which calls emit), and invariant I6.

**Control Manifest Rules (this layer)**:
- Required: Events are ordered only with `Compare`/`GetKey`/`CurrentKey` (ADR-0002); the Roster's own events carry `EventSeq` as their only ordering key (GDD R12, I6)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Registrations are weak (`TWeakInterfacePtr`/`TWeakObjectPtr`), cleared in `Deinitialize`/`EndPlay`; a dead registration is a fault-sink report, never a crash (ADR-0007)
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager` (ADR-0002)
- Forbidden: Never route queue Event Log entries through `UE_LOG`/`UE_LOGF`, the output log or trace channels (ADR-0002)
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002)
- Required (manifest 2026-10-25f, ADR-0005 Technical Setup 2026-10-25b): `FRosterCore::RequestTransition` is the only writer of lifecycle state with the R8 precedence; mutations on the server game thread with FIFO dispatch after the outermost call and EventSeq +1 per event; O(1) allocation-free queries through a pre-sized binding table; client queries return 0/false/NotTargetable(Stale) and fault (re-read at implementation; 2026-10-25g adds nothing for the Roster)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [x] **AC-PSR-55** [Core] `StealthGame.Roster.RegistrationAndTeamEvents`: GIVEN `BeginMatch(5)`, WHEN a key registers, a team batch applies, `RemoveParticipant` runs and a Dormant key reclaims, THEN exactly one `OnParticipantRegistered`, one `OnTeamsChanged`, one `OnParticipantRemoved` and one `OnParticipantReclaimed` fire respectively, each consuming exactly one `EventSeq`, and a rejected call of each kind fires none (R4, R5, R12, I6).

---

## Implementation Notes

*Derived from ADR-0002 Implementation Guidelines, ADR-0005 section 2 and GDD R12, R3, I6:*

- **Event queue.** Add a FIFO queue of event records to `FRosterCore` (plain C++, no `UObject`). Every mutating call increments an outermost-call depth counter on entry and decrements on exit; only when the depth returns to 0 does the core drain the queue and invoke listeners. State is mutated **immediately** inside the call, so a listener (or any re-entrant request) sees live state; its own events queue behind the current ones (re-entrancy behaviour is asserted in story 028, but build the depth counter and FIFO drain correctly now).
- **EventSeq assignment.** Assign `EventSeq` at **emission time** (when the event is enqueued, in call order), +1 per event of any type; rejections and no-ops enqueue nothing and consume nothing (I6, R3). `BeginMatch` resets it to 0 (story 002).
- **Event types here.** `OnParticipantRegistered` (on `Registered`), `OnTeamsChanged` (on an `Applied` `AssignTeams` batch), `OnParticipantRemoved` (on a successful `RemoveParticipant`), `OnParticipantReclaimed` (on `Reclaimed`). Each carries `{Id, EventSeq, RoundEpoch}` at minimum; add payload fields only as R12 names them. Event payload types are plain structs; `OnParticipantStateChanged`, `OnRosterRoundEnded`, `OnRosterReset`, `OnConnectionChanged` and `OnBindingLost` are added by later stories (014, 023, 022, 009, 012) through the same queue, so make the event type an extensible variant/enum rather than a fixed switch.
- **Listener interface.** Server-only delegate surface declared in the core (a small `IRosterEventListener` or multicast delegate set, planned name); weak/registered pointers per ADR-0007. Listeners act on applied events only and must be idempotent per `EventSeq` (R12).
- **Rejected calls fire nothing.** The four rejection cases to cover: roster full (registration), invalid batch (assign), locked removal, locked Dormant reclaim. Use the codes from stories 002, 005 and 007.
- **Dormant reclaim fixture.** The Dormant record cannot yet be built through the Lobby projection (story 009 depends on this story). Build it through the core test seam from story 005 (`WITH_STEALTH_TEST_SEAMS` only, never in Shipping); story 009 replaces it. A Dormant reclaim in an unlocked Lobby keeps the record's team if that team has room under F5, otherwise `None` (R4); it emits `OnParticipantReclaimed`.
- **Story 007 follow-up.** Re-assert the "no `OnTeamsChanged` on a rejected batch" clause here against the real queue (story 007 used a spy stub).
- **No-op events.** `InvalidateAllBindings`, `RebindView`, `SetOwnerTally` and `SetOwnerRespawnDue` never emit (R12); nothing to add now, but do not route them through the emit helper.
- **Prototype disposition:** No prototype counterpart. The prototype `AStealthGameMode` signalled changes by direct calls and `PostLogin`/`Logout` overrides, with no sequenced events.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005 (A locked roster rejects registration, removal and team changes but reclaims known keys) and Story 007 (An invalid team batch changes nothing and fires no event): the rejection logic being asserted here
- Story 009 (Session connection states project onto Connection fields only): `OnConnectionChanged` and the Lobby Dormant projection
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): `OnParticipantStateChanged` and its payload counts
- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event) and Story 023 (EndRoundAll ends all records once and is idempotent): `OnRosterReset` and `OnRosterRoundEnded`
- Story 028 (Re-entrant listener calls see live state and events dispatch FIFO with strict EventSeq): the re-entrancy and nested-payload assertions
- Story 036 (The dev-only transition log keeps the last N entries and is absent from Shipping): the transition log

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
- `Source/StealthFoundation/Private/Tests/Roster/StealthRegistrationAndTeamEventsTest.cpp` (test `StealthGame.Roster.RegistrationAndTeamEvents`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: each of the four operations fires exactly one sequenced event; rejected calls fire none.
  - Given: BeginMatch(5), a listener recording event type and EventSeq. Dormant record built through a core seam until story 009.
  - When: a key registers, a team batch applies, RemoveParticipant runs, the Dormant key reclaims.
  - Then: exactly one each of OnParticipantRegistered, OnTeamsChanged, OnParticipantRemoved, OnParticipantReclaimed, each consuming exactly +1 EventSeq. A rejected call of each kind (roster full, invalid batch, locked removal, locked Dormant reclaim) fires nothing and leaves EventSeq unchanged.
  - Edge cases: events dispatch after the outermost call; assert EventSeq contiguous.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthRegistrationAndTeamEventsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [x] Created and passing (`StealthGame.Roster.RegistrationAndTeamEvents`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-005-locked-roster-is-frozen.md`, `production/epics/participant-state-roster/story-007-assign-teams-all-or-nothing.md`
- Unlocks: `production/epics/participant-state-roster/story-009-connection-projection.md`, `production/epics/participant-state-roster/story-010-bind-pawn-guards.md`, `production/epics/participant-state-roster/story-028-reentrant-listener-ordering.md`

---

## Completion Notes
**Completed**: 2026-10-25 (participant-state-roster batch 001-014: one core built and closed together)
**Criteria**: 1/1 passing (AC-PSR-55: one OnParticipantRegistered / OnTeamsChanged / OnParticipantRemoved / OnParticipantReclaimed each consuming exactly one EventSeq; roster-full, invalid-batch, locked-removal and locked-Dormant-reclaim rejections fire none; EventSeq contiguous from 1; dispatch after the call's writes)
**Decisions**: FIFO queue with an outermost-call depth (`FCallScope`) and a re-entrancy guard; EventSeq assigned at enqueue. Flat `FRosterEvent` payload tagged by `ERosterEventType` (extensible). Listener interface `IRosterEventListener` (raw pointers in the plain core; the facade, story 061, bridges to weak delegates). A reclaim does not write Connection (R3: only Session's notify does).
**Deviations**: None. Manifest Version updated to 2026-10-25g.
**Test Evidence**: `production/qa/evidence/story-008-registration-and-team-events/full-suite.txt` (build `Result: Succeeded`; 84/84 `StealthGame.` tests Success, 70 existing + 14 Roster) and `stamp.txt`
**Run result**: N/A — plain-C++ server core with no world, actor or screen; nothing a player can see until the facade (story 061) and the C-TeamView push step (story 062) exist
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed (StrikeLimit release aliasing, story 014); `TryReclaim`, `DisposeLivePawnOnElimination` and `EmitStateChanged` extracted to keep methods short; `RequestTransition` kept as one function by design (the R8 precedence in one place)
