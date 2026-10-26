# Story 062: URosterTeamViewComponent and the Roster push step write C-TeamView change-only, once per frame

> **Epic**: Participant State & Roster
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-006`, `TR-roster-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also governed by**: ADR-0005 (store events drive the push), ADR-0007 (component placement, `Public/View/`)
**ADR Decision Summary**: ADR-0001 delivers team data only through C-TeamView: replicated `UActorComponent`s derived from `UTeamViewComponentBase` on each `APlayerController`, relevant only to its owner. The Roster part (`URosterTeamViewComponent`) carries the owner block `{PublicHandle, Team, State, RoundEpoch, WrongTargetTally, LastResultSeq, T_due}` and at most four teammate entries `{PublicHandle, State, PawnRef, IsConcealed, RetireAtServerTime, bReconnecting, ReconnectEndCause}`. The Roster push step is the only writer; it pushes on change only and at most once per server frame.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: the component replicates only if the owning controller sets `bReplicateUsingRegisteredSubObjectList` (default false in 5.8.4; ADR-0020 E2). Push-model properties must be marked dirty on every write.

**Context paragraph**: Story 037 defines the wire struct and its size; story 038 the owner-view feed; story 041 the controller-swap re-push. No story owned the component itself and the push step that turns Roster events into C-TeamView writes, including the D23 rule for `bReconnecting` (set when a teammate's projection becomes `Disconnected` during Round phase `Active`; cleared only by an event: rejoin -> `Rejoined`, grace expiry -> `GraceExpired`, removal/elimination/round end or a kick (`bKicked`) -> `Cleared`; never by a timer; a superseding rejoin keeps the flag until the rejoin completes, D24). Added by the main session under decision D43.

**Control Manifest Rules (this layer)**:
- Required: The Roster part carries owner `{Handle, Team, State, RoundEpoch, WrongTargetTally, LastResultSeq, T_due}` and per teammate `{Handle, State, PawnRef, bIsConcealed, RetireAtServerTime}` plus Team-audience `bReconnecting` and `ReconnectEndCause`; the Roster push step is the only writer (ADR-0001, ADR-0019)
- Required: The Roster teammate entry's `bReconnecting` is derived only by the push step from Session's `Connection` projection and cleared only by a Roster event (ruling D23)
- Required: C-TeamView carries own-team Roster data only; no opponent count (ADR-0005, ADR-0001)
- Guardrail: C-TeamView pushes change-only, at most 1 per frame (ADR-0001)

---

## Acceptance Criteria

- [ ] **AC-PSR-62a** [Integration] `StealthGame.Roster.TeamViewPushStep`: GIVEN a 2v2 store with one `URosterTeamViewComponent` per controller, WHEN several Roster events for one team land in one server frame, THEN each owner's component is written once in that frame (coalesced), carries only own-team data, and an unchanged frame writes nothing.
- [ ] **AC-PSR-62b** [Integration] `StealthGame.Roster.TeamViewReconnectFlag`: GIVEN phase Active, WHEN a teammate's projection becomes `Disconnected`, then (separately) rejoins, expires its grace, is kicked (`bKicked`), or the round ends, THEN `bReconnecting` is set on the disconnect and cleared only by that event with `ReconnectEndCause` `Rejoined` / `GraceExpired` / `Cleared` / `Cleared`; no timer clears it; outside phase Active it stays false.

---

## Implementation Notes

- The push step subscribes to the store's events (story 061) and keeps a per-team dirty set; it flushes once per server frame (end of frame tick or the queue drain, ADR-0002) and writes only changed entries.
- `RetireAtServerTime` is non-zero only in the retiring update (manifest rule); the value comes from the binding row (`FRosterBindingEntry::RetireAtServerTimeMs`, written by `RequestTransition`, story 014).
- `PawnRef` entries clear at `InvalidateAllBindings` (R9).

---

## Out of Scope

- Story 037 (wire struct and size), Story 038 (owner tally and `T_due` feed), Story 041 (rebind re-push), Story 051 (PIE six-case teammate flag test)

---

## Files


- Source/StealthFoundation/Public/View/StealthRosterTeamView.h (new: EReconnectEndCause, FTeamViewOwnerPart, FTeamViewTeammateEntry, FTeamView, UTeamViewComponentBase, URosterTeamViewComponent)
- Source/StealthFoundation/Private/Roster/StealthRosterTeamView.cpp (new: COND_OwnerOnly push-model replication)
- Source/StealthFoundation/Private/Roster/StealthRosterPushStep.h/.cpp (new: the only writer; dirty set per team; D23 reconnect flag)
- Source/StealthGame/Public/StealthPlayerController.h, Private/StealthPlayerController.cpp (TeamView default subobject, registered subobject list)
- Source/StealthFoundation/Private/Tests/Roster/StealthRosterPushStepTest.cpp (new, 2 tests)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterPushStepTest.cpp` — must exist and pass (`qa.level` is full).

**Status**: [x] Created and passing (`StealthGame.Roster.TeamViewPushStep`, `StealthGame.Roster.TeamViewReconnectFlag`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-037-team-view-serialized-size.md`, `production/epics/participant-state-roster/story-061-roster-store-facade-and-match-lifetime.md`
- Unlocks: `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`, `production/epics/participant-state-roster/story-051-pie-teammate-reconnecting-flag.md`

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 2/2 passing (62a TeamViewPushStep, 62b TeamViewReconnectFlag)
**Deviations**: Depends on psr-037 (Ready): the wire types follow 037's notes but use the default property serializer; 037 still owns the fixed-width NetSerialize and the <= 96 B budget. The round-end clear is exercised by delivering RosterRoundEnded to the push step directly (EndRoundAll is psr-023). bIsConcealed stays false until Concealment writes it; WrongTargetTally/LastResultSeq/T_due stay 0 until psr-038.
**Test Evidence**: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterPushStepTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: OBSERVED: production/qa/evidence/integration-spine-2026-10-26/02-client1-active.png (client dev line: own team view team 1, 0 teammates)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
