# Story 001: Participant record fields equal the R3 whitelist exactly

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
**Also governed by**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: ADR-0005 makes four game-instance stores (Session, Roster, Round, Tuning preset) the only authoritative match state, each a plain-C++ core behind a `UGameInstanceSubsystem` facade; the Roster's core is `FRosterCore` and owns the `ParticipantRecord`s, bindings, tombstones, `MatchId`, `RoundEpoch` and `EventSeq`. Nothing authoritative may live on a PlayerController, PlayerState, GameMode or GameState because seamless travel replaces them. ADR-0007 requires the Roster to be a plain C++ core with no `UWorld` dependency whose inputs arrive through constructor arguments or small interfaces, with its specs in the owning module's `Private/Tests/` running headless.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 (HIGH, networking): no new post-cutoff APIs; the 5.6 deprecation of `DispatchPostLogin` and the 5.8 `UE_LOGF` logging guidance apply to later facade work, not to this story. Verification still required by the ADR (S3 seamless/hard travel, S4 reconnect and same-machine keys, S5 host loss) has not been reported; this story touches none of it because `FRosterCore` is plain C++. ADR-0007 (MEDIUM): C++20 is the floor in 5.8, so the core may use C++20 features.

**Context paragraph**: This is the first story of the epic. It scaffolds the plain-C++ `FRosterCore`, the `FParticipantRecord` type, the distinct `FParticipantId` and `FIdentityKey` struct types and the injected dependency interfaces the whole epic's `[Core]` tests use, then proves that the record carries exactly the fields GDD rule R3 lists and nothing else (R1: the Roster stores no feature state). It exists so every later story adds behaviour to a schema that is already pinned and that cannot silently grow a strike, defence, concealment or objective field.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Required: Project header file names are unique across all modules (`Stealth` prefix plus system name); every header in a `Public/` folder sits under `View/` (client-safe) or `Server/` (server-only) (ADR-0007)
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them (ADR-0005)
- Forbidden: Never promote prototype code (`Source/StealthGame`) into a production module before its governing ADR is Accepted; promotion is a rewrite, not a file move (ADR-0007)
- Forbidden: Never let a `Public/View/` header include a `Server/` header (ADR-0007)
- Guardrail: Tests: per-commit pipeline <= 10 min (ADR-0008)
- Required (manifest 2026-10-25f, ADR-0005 Technical Setup 2026-10-25b): `FRosterCore::RequestTransition` is the only writer of lifecycle state with the R8 precedence; mutations on the server game thread with FIFO dispatch after the outermost call and EventSeq +1 per event; O(1) allocation-free queries through a pre-sized binding table; client queries return 0/false/NotTargetable(Stale) and fault (re-read at implementation; 2026-10-25g adds nothing for the Roster)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [x] **AC-PSR-01** [Core] `StealthGame.Roster.RecordSchemaWhitelist`: GIVEN the participant record type, WHEN its reflected fields are enumerated, THEN they equal R3's field set exactly, with no strike, defence, concealment or objective field (R1, R3).

---

## Implementation Notes

*Derived from ADR-0005 section 2 and ADR-0007 section 5 (Implementation Guidelines), and GDD R1 and R3:*

- **FRosterCore is plain C++ with injected dependencies.** `FRosterCore` lives in `Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp` and has no `UWorld`, `UObject` or engine-singleton dependency. Its constructor takes the seven injected dependencies listed in the GDD AC "Tags" section: an opaque pawn handle `{ObjectKey, Generation}`, a pawn-liveness provider (valid, destroyed, pending kill), an authority provider, a thread check, a server clock in integer milliseconds, a random source for `MatchId` and `PublicHandle`, and a fault sink. Declare the interfaces (planned names `IRosterPawnLiveness`, `IRosterAuthority`, `IRosterThreadCheck`, `IRosterClock`, `IRosterRandom`, `IRosterFaultSink`, and the handle type) in `Private/Roster/StealthRosterDeps.h`. The facade (`URosterStore`) adapts engine objects to these later; this story builds no facade.
- **Distinct id types.** `FParticipantId` and `FIdentityKey` are distinct struct types (not raw integers or strings) in `Public/View/StealthRosterTypes.h`; AC-PSR-04's reflection scan (story 039) is only meaningful because they are distinct. `FParticipantId` is a monotonic per-match value; `FIdentityKey` wraps the client-asserted 128-bit install key (R3). Neither may be given a Blueprint specifier, and neither may appear as an RPC parameter (R2).
- **The R3 field set the test pins** (hand-derive the expected list from R3 and check it in as the expected-name list): per record `ParticipantId`, `PublicHandle`, `IdentityKey`, `DisplayName`, `Team`, `Connection` (with `DisconnectedAt` and `ConnectionGeneration`), `AppearanceHistory`, `InRound`, `State`, `LastTransitionReason`, `LastEventSeq`, `AssignedAppearanceId`, and the pawn binding row. Roster-wide values (`MatchId`, `RoundEpoch`, `EventSeq`) belong to `FRosterCore`, not the record. The owner-view cache (R6 `SetOwnerTally` feed) is explicitly not part of the R3 record and must not appear in it.
- **No feature state (R1).** The record has no strike count, defence state, concealment flag or objective progress field; the test must also check a deny-list of field-name fragments (strike, defence, concealment, objective).
- **Enumeration mechanism.** A pure-core struct has no UObject reflection. The enumeration has to come from a mechanism the record itself generates (for example an X-macro field list that both declares the members and emits the name list, or a USTRUCT with `TFieldIterator`). Do not hand-maintain a second list next to the struct. See the open point below.
- **Open point:** the QL-STORY-READY gap says R3's field set is not enumerated in the AC and that the enumeration mechanism and the placement of roster-wide counters and the binding table (on the expected side or not) are author decisions. Record the decision in the test file header and in the expected-name fixture.
- **Prototype disposition:** replaces the team and life fields of `Source/StealthGame/Public/StealthPlayerState.h` and `Private/StealthPlayerState.cpp` (schema only; the PlayerState replication fix is story 040). Nothing is promoted: the new types are written from the GDD, and `StealthTypes.h` enums stay prototype-only until superseded by Foundation `View/` wire types.
- **Cross-epic note:** the `StealthFoundation` module skeleton, `StealthTestHarness` and CI come from `production/epics/tuning-data-config/EPIC.md`; this story assumes that skeleton exists and adds only Roster files.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (BeginMatch and registration assign ids and handles and reject a full roster): `BeginMatch`, `RegisterParticipant`, id and handle assignment
- Story 003 (Mutation from a worker thread is rejected and faulted): thread-check behaviour and `Rejected_WrongThread`
- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): authority behaviour
- Story 037 (A maximal C-TeamView net-serializes in at most 96 bytes): the `FTeamView` wire struct
- Story 039 (No Server/Client/Multicast RPC parameter contains a participant id or identity key): the reflection scan over RPCs
- Story 040 (PlayerState replicates only the allowlist): the PlayerState replacement

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
- `Source/StealthFoundation/Private/Tests/Roster/StealthRecordSchemaWhitelistTest.cpp` (test `StealthGame.Roster.RecordSchemaWhitelist`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** "R3's field set" is not enumerated. R3 mixes per-record fields with roster-wide counters (MatchId, RoundEpoch, EventSeq), a binding table, and composite Connection (DisconnectedAt, ConnectionGeneration). "Reflected fields" is undefined for a pure-core struct with no UObject. Needs: exact expected field names in the AC and the enumeration mechanism (USTRUCT TFieldIterator or registered field-list macro).

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: record field set equals R3 exactly, no strike/defence/concealment/objective field.
  - Given: FParticipantRecord and a checked-in expected-name list (hand-derived from R3).
  - When: fields enumerated by the agreed mechanism.
  - Then: set equality (names, types where R3 gives them); no field name matches the strike/defence/concealment/objective deny-list; ParticipantId and IdentityKey are distinct struct types.
  - Edge cases: extra/missing/renamed field; owner-view cache fields (R6) must not appear in the record; roster-wide counters placed on expected side or the other per author decision.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthRecordSchemaWhitelistTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [x] Created and passing (`StealthGame.Roster.RecordSchemaWhitelist`, 2026-10-25)

---

## Dependencies

- Depends on: None within this epic. Cross-epic: `production/epics/tuning-data-config/EPIC.md` (StealthFoundation module skeleton, test harness and CI)
- Unlocks: `production/epics/participant-state-roster/story-002-begin-match-and-register.md`, `production/epics/participant-state-roster/story-003-off-game-thread-mutation-rejected.md`, `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`, `production/epics/participant-state-roster/story-039-no-rpc-carries-participant-id.md`, `production/epics/participant-state-roster/story-040-player-state-replicated-allowlist.md`

---

## Completion Notes
**Completed**: 2026-10-25 (participant-state-roster batch 001-014: one core built and closed together)
**Criteria**: 1/1 passing (AC-PSR-01: record fields equal the checked-in R3 expected-name fixture, 13 record fields plus 3 Connection sub-fields, types where R3 gives them; deny-list strike/defence/conceal/objective/MatchId/RoundEpoch/Tally/RespawnDue; `FParticipantId` and `FIdentityKey` distinct class types, 128-bit key)
**Decisions**: Enumeration by an X-macro (`STEALTH_ROSTER_RECORD_FIELDS`) that both declares the members and emits the name/type list; no hand-kept second list. MatchId/RoundEpoch/EventSeq live on `FRosterCore` (deny side). The binding row is the record field `PawnBinding` (live pawn); tombstones live in the core's table. UE naming: `bInRound`, `DisconnectedAtMs`. Clock and fault sink reuse the TDC seams (`IStealthClock`, `IStealthFaultSink`) instead of the planned `IRosterClock`/`IRosterFaultSink`.
**Deviations**: Built together with stories 002-014 in one core (decision D41 shared commit). Manifest Version updated to 2026-10-25g.
**Test Evidence**: `production/qa/evidence/story-001-record-schema-whitelist/full-suite.txt` (build `Result: Succeeded`; 84/84 `StealthGame.` tests Success, 70 existing + 14 Roster) and `stamp.txt`
**Run result**: N/A — plain-C++ server core with no world, actor or screen; nothing a player can see until the facade (story 061) and the C-TeamView push step (story 062) exist
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed (StrikeLimit release aliasing, story 014); `TryReclaim`, `DisposeLivePawnOnElimination` and `EmitStateChanged` extracted to keep methods short; `RequestTransition` kept as one function by design (the R8 precedence in one place)
