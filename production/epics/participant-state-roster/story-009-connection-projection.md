# Story 009: Session connection states project onto Connection fields only

> **Epic**: Participant State & Roster
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0005 makes Session's `ConnectionState` the single source of truth for liveness and has Session report facts to the Roster through `NotifyConnectionChanged`; the Roster only projects them. Every `Connection` change bumps the participant's `ConnectionGeneration` (including a Lobby rejoin's `Rejoining` projection and a supersede), and Round's grace expiry carries that generation as `ExpectedConnectionGeneration` so a stale timer can be rejected. Session never calls `RequestTransition`: it reports facts and Round decides.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 (HIGH, networking): no new post-cutoff APIs here (`DispatchPostLogin` is deprecated since 5.6; this story has no login code). Spike S4 (reconnect: retained PlayerState, same-machine keys, supersede) and S5 (host loss) have not reported; they affect the facade and travel harnesses, not the core projection implemented here.

**Context paragraph**: Session & Travel owns `ConnectionState`; the Roster's `Connection` is a three-valued projection (`Connected`, `Disconnected`, `Dormant`) written only through `NotifyConnectionChanged(Id, Projection, ServerTimeMs [, bKicked])`. This story implements that notify in `FRosterCore`: it changes only `Connection`, `DisconnectedAt` and `ConnectionGeneration`, bumps the generation once per change of the projection into `Disconnected`, never touches the lifecycle state, and emits `OnConnectionChanged`. It realises GDD R3 (`Connection` field), R11 (mapping table, projection details, supersede rule D24) and R12.

**Control Manifest Rules (this layer)**:
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005)
- Required: Ack records, kick list, silence clocks and connection maps are keyed by `ParticipantId` (or `UNetConnection` for the live mapping) and never replicated (ADR-0005)
- Required: Every wait is bounded by a server-measured clock (ADR-0005)
- Forbidden: Never let Session spawn, free, hide or possess a pawn, or call `RequestTransition` (ADR-0005)
- Forbidden: Never add a teammate connection field other than the one `bReconnecting` bit (no ping, time, reason or address), and never send it to a non-teammate (ADR-0001, ADR-0005)
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005)
- Required (manifest 2026-10-25f, ADR-0005 Technical Setup 2026-10-25b): `FRosterCore::RequestTransition` is the only writer of lifecycle state with the R8 precedence; mutations on the server game thread with FIFO dispatch after the outermost call and EventSeq +1 per event; O(1) allocation-free queries through a pre-sized binding table; client queries return 0/false/NotTargetable(Stale) and fault (re-read at implementation; 2026-10-25g adds nothing for the Roster)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [x] **AC-PSR-46** [Core] `StealthGame.Roster.ConnectionProjection`: GIVEN each Session `ConnectionState` in R11's mapping table, WHEN `NotifyConnectionChanged` delivers its projection, THEN:
  - only `Connection`, `DisconnectedAt` and `ConnectionGeneration` change;
  - `ConnectionGeneration` increases by 1 only on entry to `Disconnected`;
  - the lifecycle state is unchanged;
  - exactly one `OnConnectionChanged` fires, consuming one `EventSeq`;
  - a Lobby departure sets `Dormant`, the record keeps its id, and `Rejected_RosterFull` ignores it (R4, R11).

---

## Implementation Notes

*Derived from ADR-0005 section 5 (Disconnect, Supersede) and GDD R3, R11, R12:*

- **The core takes the projection, not the Session state.** `NotifyConnectionChanged(Id, EConnectionProjection, ServerTimeMs, bKicked = false)` accepts only `Connected`, `Disconnected` or `Dormant`. The GDD mapping table (`Connected`/`Unresponsive`/`Traveling` -> `Connected`; `Disconnected`/`Rejoining`/`Expired` -> `Disconnected`; `Dormant` -> `Dormant`; `Rejected` -> unchanged; `Connecting` -> no record) is Session's adapter responsibility. This story's test may hold the table as test data and drive the notify with each row's projection, but the Roster has no `ConnectionState` enum. See the open point.
- **Generation rule.** `ConnectionGeneration` increases by 1 on every change of the projection **into** `Disconnected` (from `Connected` or `Dormant`); a supersede of a live connection is such a change (D24: Session sends `Disconnected` at the supersede instant, so `DisconnectedAt` is that instant and the generation bumps once). A notify of `Disconnected` while already `Disconnected` (the `Rejoining` and `Expired` moves, a failed rejoin) changes neither the generation nor `DisconnectedAt` (R11), so a client that keeps failing its rejoin cannot extend its grace.
- **DisconnectedAt.** Server time in integer milliseconds taken from the notify's `ServerTimeMs`, written only on entry to `Disconnected`.
- **Lifecycle untouched.** The notify never changes `State`, `Team`, bindings, `LastTransitionReason` or `InRound` (R11 Disconnect). Whether and when a disconnected participant leaves active play is Round's policy. The pawn hand-over is ADR-0010's and Round's, never Session's or the Roster's.
- **Dormant.** A Lobby departure sets `Dormant`; the record keeps its id, handle and match-scoped fields and does not hold a roster slot, so `RegisterParticipant` still returns `Rejected_RosterFull` for a new key when `2 x TeamSize` **non-Dormant** records exist (R4). Replace the story 005 and 008 Dormant test seam with this real path in those fixtures where practical.
- **bKicked.** Accept the optional flag but change nothing here; it only affects the `bReconnecting` clear cause in the C-TeamView push step (R6, `ReconnectEndCause = Cleared`), which belongs to the later team-view stories (037, 041, 051).
- **Event.** Emit `OnConnectionChanged{Id, Projection, EventSeq}` through the story 008 queue, consuming one `EventSeq`, for a notify that changes the projection. See the open point for notifies that change nothing.
- **Open point (QL-STORY-READY GAPS):** the criterion iterates "each Session ConnectionState" but `NotifyConnectionChanged` takes only the 3-value projection, and the GDD does not say whether the mapping lives in the Roster. "Exactly one OnConnectionChanged" also conflicts with the table's "unchanged" for `Rejected` and with a Disconnected-to-Disconnected move. Assert at the projection level only, and decide and document whether a non-changing notify emits an event (the provisional reading is: no event, no `EventSeq`, because I6 counts only emitted events and a no-op consumes nothing).
- **Prototype disposition:** replaces the connection bookkeeping in `AStealthGameMode::PostLogin`/`Logout` (`Source/StealthGame/Private/StealthGameMode.cpp`), which treated a logout as a removal and queued an elimination check. The disconnect policy itself belongs to Round (stories 054, 059).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): the event queue this story uses
- Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): `ReleaseToCrowd` (uses this story's `Disconnected`)
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition) and Story 015 (A Disconnect transition is rejected while the participant is connected): the `Disconnect` transition that reads `Connection` and the generation
- Story 029 (Reclaim restores the same record for active and informant participants): reclaim of a Disconnected record
- Story 051 (A teammate's reconnecting flag and end cause follow the event-only clear rules in all six cases): `bReconnecting` derivation
- Story 054 (Disconnect grace expires on the injected clock and stale-connection timers are rejected): grace timers and `Rejected_StaleConnection` behaviour

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
- `Source/StealthFoundation/Private/Tests/Roster/StealthConnectionProjectionTest.cpp` (test `StealthGame.Roster.ConnectionProjection`)
- `docs/architecture/adr-0005-session-seamless-travel-state-stores.md` (amendment: D45 generation rule, D43 Round-phase input)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

**Flagged gap (QL-STORY-READY: GAPS):** the criterion iterates "each Session ConnectionState" but NotifyConnectionChanged takes only the 3-value projection. Whether the mapping lives in the Roster is undefined, so the Connecting and Rejected rows have no testable Roster behaviour. "Exactly one OnConnectionChanged" conflicts with the table's "unchanged" for Rejected and with a Disconnected-to-Disconnected move (Rejoining, Expired). Needs: assert at projection level only; state whether non-changing notifies emit an event.

**Provisional spec (qa-lead, pending gap resolution):**

- **AC-1**: projections change only the connection fields.
  - Given: an Active record, Connected.
  - When: NotifyConnectionChanged delivers Connected, Disconnected, Dormant.
  - Then: only Connection, DisconnectedAt, ConnectionGeneration change. Generation rises by 1 only on entry to Disconnected. State unchanged. A changing notify fires one OnConnectionChanged consuming one EventSeq. A Lobby departure gives Dormant with same id, and Rejected_RosterFull ignores it.
  - Edge cases: Disconnected to Disconnected changes neither generation nor DisconnectedAt (R11); a supersede bumps the generation once; bKicked changes nothing here.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthConnectionProjectionTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [x] Created and passing (`StealthGame.Roster.ConnectionProjection`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-008-registration-and-team-events.md`
- Unlocks: `production/epics/participant-state-roster/story-011-tombstone-generation-and-release.md`, `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `production/epics/participant-state-roster/story-029-reclaim-core.md`, `production/epics/participant-state-roster/story-051-pie-teammate-reconnecting-flag.md`

---

## Completion Notes
**Completed**: 2026-10-25 (participant-state-roster batch 001-014: one core built and closed together)
**Criteria**: 1/1 passing (AC-PSR-46: walk through every notify-sending Session state incl. supersede and kick; only Connection fields change; generation +1 only on entry to Disconnected (4 entries -> 4); DisconnectedAt written only then; state and pawn unchanged; one OnConnectionChanged and one EventSeq per changing notify; Lobby departure -> Dormant, same id, ignored by Rejected_RosterFull)
**Decisions**: Gap resolution: the Roster takes only the 3-value projection; the R11 table is Session's adapter and test data here. A non-changing notify is NoOp_Unchanged with no event and no EventSeq. Decision D45 honoured (ADR-0005 amended). `bKicked` is accepted and unused in the core (C-TeamView clear cause, D23, story 062).
**Deviations**: None. Manifest Version updated to 2026-10-25g.
**Test Evidence**: `production/qa/evidence/story-009-connection-projection/full-suite.txt` (build `Result: Succeeded`; 84/84 `StealthGame.` tests Success, 70 existing + 14 Roster) and `stamp.txt`
**Run result**: N/A — plain-C++ server core with no world, actor or screen; nothing a player can see until the facade (story 061) and the C-TeamView push step (story 062) exist
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed (StrikeLimit release aliasing, story 014); `TryReclaim`, `DisposeLivePawnOnElimination` and `EmitStateChanged` extracted to keep methods short; `RequestTransition` kept as one function by design (the R8 precedence in one place)
