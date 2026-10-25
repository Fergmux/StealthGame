# Story 002: Team ping wire types and fixed-slot view component

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R4, PNG-R6, F4)
**Requirement**: `TR-png-011` (also covers the type half of `TR-png-008` and the slot-cost half of `TR-png-021`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (§5 carrier model, feature team sub-structs)
**ADR Decision Summary**: Team-audience data rides C-TeamView, a set of replicated `UActorComponent`s on each `AStealthPlayerController` derived from `UTeamViewComponentBase`; the server copies team data into each teammate's own controller. `UTeamPingViewComponent` carries `FTeamPingView { Entries[TeamSize] {SenderHandle, PingSeq, Point (FVector_NetQuantize10, 0.1 cm), ServerTime} }`, a fixed-size array where slot `i` always belongs to the sender holding Roster team slot `i` (never compacted, reordered or reassigned mid-round), with no epoch of its own.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0001 networking; the legacy NetDriver is pinned, Iris off)
**Engine Notes**: `AController` is `bOnlyRelevantToOwner`, so each part reaches exactly one connection (ADR-0001 E6); sub-objects ride the registered sub-object list (E11). A fixed-size `UPROPERTY` array replicates per element on the legacy driver; a `TArray` without a fast-array serializer would resend every slot and is not allowed (GDD F4). `FVector_NetQuantize10` quantises to 0.1 cm. Verify the per-element delta in the wire-size measurement of this story, not by assumption.

**Control Manifest Rules (this layer)**:
- Required: `UTeamPingViewComponent` (`FTeamPingView`, fixed-size array, one slot per team member, no `RoundEpoch` of its own) is a C-TeamView part written only by the Informant Pings server handler; slot `i` always belongs to the sender with Roster team slot `i`; `Point` is `FVector_NetQuantize10` — source: ADR-0001, ADR-0002
- Required: the view part, its interface and the client helpers sit in `Public/View/` and include no `Public/Server/` header — source: ADR-0007
- Forbidden: Never add a connection field, entry field or carrier that names a pawn, stage, court, site or clue; never use a dynamic array without a fast-array serializer for the slots — source: ADR-0001, GDD F4
- Guardrail: one changed slot at most 32 B on the wire; ping part at most 50 B/s per teammate connection at defaults — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (PNG-R4, PNG-R6, F4), scoped to this story:*

- [ ] **AC1** (TR-png-011): **GIVEN** `FTeamPingView` **WHEN** built for a team of `TeamSize` (at most 5) **THEN** it is a fixed-size array of entries `{SenderHandle, PingSeq uint8, Point FVector_NetQuantize10, ServerTime double}`, slot `i` belongs to the sender with Roster team slot `i` and is never compacted, reordered or reassigned across rebind, takeover or rejoin, and reflection finds no `RoundEpoch`, stage, court, site or clue field.
- [ ] **AC2** (TR-png-008, type half): **GIVEN** `FNetTeamPing` **THEN** reflection finds exactly one property `Point` of type `FVector_NetQuantize10`, and a point (1234.56, -78.91, 5.04) round-trips through serialisation within 0.05 cm per component.
- [ ] **AC3** (TR-png-021, slot cost): **GIVEN** one slot changes **WHEN** serialised for a receiving connection **THEN** only that slot's changed properties are sent (per-element delta, no whole-array resend), at most 32 bytes on the wire for the changed slot.
- [ ] **AC4** (TR-png-011, change-only push): **GIVEN** two slot writes in one server frame **WHEN** replication runs **THEN** one coalesced update carries both, and an unchanged part sends nothing.

---

## Implementation Notes

*Derived from ADR-0001 §5 (carrier model) and ADR-0007 Technical Setup 2026-10-25 / 2026-10-25b:*

Declare in `Source/StealthGameplay/Public/View/Features/Pings/StealthTeamPingView.h`: `FNetTeamPing { FVector_NetQuantize10 Point; }` (the point only; no pawn id, no client sequence — the RPC is reliable and ordered), `FTeamPingEntry`, `FTeamPingView`, and `UTeamPingViewComponent : UTeamViewComponentBase` (Foundation `Public/View/`, supplied by the participant-state-roster / IVP carrier work). The server writes the same view into the controller of every member of the sender's team, the sender included. Sizing: `TeamSize` comes from Round (max 5); the array is allocated at the maximum and slots beyond `TeamSize` stay empty. The slot index is the Roster team slot, a stable per-team index in `0..TeamSize-1` assigned once at team assignment; `SenderHandle` is redundant with the index for a receiver and is kept for the client's own validation. `PingSeq` is a wrapping byte per participant (story 005 advances it). The part is pushed on change only, coalesced once per server frame like every C-TeamView part (use the base class's push path; do not add a second dirtying mechanism).

Measure, do not assume, the 32 B figure: the test serialises one changed slot with the PIE/raw-bunch capture helpers from the tuning-data-config epic (net-emulation and capture stories) and fails above 32 B. Opponent controllers never hold this component (story 003 and 013 own that assertion); this story only provides the type.

Prototype replaced: nothing is promoted. The prototype ping carried a location and a name string over a client RPC (`AStealthPlayerController::ClientReceivePing(FVector_NetQuantize, FString)`); that is retired in story 027.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 003: allowlist row, audit and registration constraints for the component.
- Story 005 and 006: who writes the slots, validation and `PingSeq` advance.
- Story 007: clears at reset, travel and round end.
- Story 023: the 5v5 bandwidth measurement (this story only proves the per-slot cost).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Features/Pings/StealthTeamPingView.h` (`FNetTeamPing`, `FTeamPingEntry`, `FTeamPingView`, `UTeamPingViewComponent`)
- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingView.cpp`
- `Source/StealthGameplay/Private/Tests/Pings/TeamPingWireTypesTest.cpp` (class `FPingsTeamPingWireTypesTest`, name `StealthGame.Pings.WireTypes`)
- `Source/StealthTestHarness/Private/Pings/SlotDeltaWireSizeTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(SlotDeltaWireSize, "StealthGame.Pings.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 fixed-slot view layout and stability**
- Given: `FTeamPingView` built for TeamSize 1..5 with a roster.
- When: reflect the type; run rebind, takeover and rejoin sequences and write slots.
- Then: a fixed-size array of `{SenderHandle, PingSeq uint8, Point FVector_NetQuantize10, ServerTime double}`; slot `i` is always roster team slot `i`; no `RoundEpoch`/stage/court/site/clue property.
- Edge cases: TeamSize 5 (max) and 1; the middle member leaves and rejoins and its slot index is unchanged; no compaction.

**Test: AC2 `FNetTeamPing` quantisation**
- Given: `FNetTeamPing`.
- When: reflect; serialise and deserialise (1234.56, -78.91, 5.04).
- Then: exactly one property `Point` of type `FVector_NetQuantize10`; each component within 0.05 cm.
- Edge cases: negative and near-zero components; a NaN input is not asserted here (story 005 covers it).

**Test: AC3 per-element delta, <= 32 B**
- Given: a replicating team part with a known baseline.
- When: change one slot and serialise for one receiving connection.
- Then: only that slot's changed properties are sent, no whole-array resend, <= 32 B.
- Edge cases: changing slot 0 and slot 4 separately; the same measurement as story 023 AC3, so one shared fixture.

**Test: AC4 coalescing**
- Given: two writes to two slots in one server frame.
- When: replication runs.
- Then: one update carries both; the next frame with no change sends 0 bytes.
- Edge cases: two writes to the same slot in one frame send only the final value.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Pings/TeamPingWireTypesTest.cpp` and `Source/StealthTestHarness/Private/Pings/SlotDeltaWireSizeTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic `production/epics/participant-state-roster/EPIC.md` (Roster team slot index, `PublicHandle`) and `production/epics/information-visibility-policy/story-013-allowlist-content.md` (carrier base class and allowlist file)
- Unlocks: `production/epics/informant-pings-team-comms/story-003-carrier-registration-and-team-only-audit.md`, `story-005-server-validation-core.md`, `story-006-server-team-ping-handler-and-host-parity.md`, `story-016-ring-model-and-age.md`, `story-023-bandwidth-bound.md`
