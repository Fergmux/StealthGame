# Story 057: Two client processes on one machine with distinct identity keys get distinct records

> **Epic**: Participant State & Roster
> **Status**: Blocked — BLOCKED: spike S4 (reconnect, null-subsystem keys) has not reported. Unblocks when S4 reports (two clients on one machine with `-IdentityKey=` and no online subsystem, retained PlayerState re-attached, supersede).
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0005 fixes `AGameModeBase` with no inactive-PlayerState reuse (the address-keyed `AddInactivePlayer`/`FindInactivePlayer` exist only in `AGameMode`), and keeps the Roster as the only authority on identity: reclaim is decided by the Roster key alone. The `IdentityKey` is a client-side 128-bit key loaded or created in a local file under `Saved/`, honouring `-IdentityKey=` and deriving a per-instance key in PIE, sent only as a login option and never replicated. A rejoin with a known key adopts the retained PlayerState in `InitPlayerState`, so no PlayerState is ever created or lost for a reclaim.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No new post-cutoff APIs; the 5.6 `DispatchPostLogin` deprecation applies (use `PostLogin`/`OnPostLogin`); `UE_LOGF` for logging. Verification required: **S4** (reconnect: retained PlayerState re-attached to a new controller with no player-array change on any client; two clients on one machine with `-IdentityKey=`; supersede; the identity-subsystem API name for the PIE instance index is [U]), S3 and S5. S4 has not reported.

**Control Manifest Rules (this layer)**:
- Required: the game mode derives from `AGameModeBase`; a rejoiner adopts its retained PlayerState in `InitPlayerState`, and `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` — source: ADR-0005
- Required: authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer — source: ADR-0005
- Required: T3 code lives in `StealthTestHarness`; travel and multi-process criteria run multi-process standalone, never in in-process PIE only — source: ADR-0008
- Forbidden: Never derive from `AGameMode`; never use `AddInactivePlayer`/`FindInactivePlayer` or address-keyed reclaim — source: ADR-0005
- Forbidden: Never spawn a fresh PlayerState for a reclaim; never put a secret in a travel URL or login option — source: ADR-0005
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line — source: ADR-0005

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-07** [MP-2][S4] `StealthGame.Roster.MP.SameMachineClientsDistinct`: GIVEN two client processes on one machine with distinct `-IdentityKey=` values and no online subsystem, WHEN both join and one rejoins with its key, THEN there are 2 records, the rejoin reclaims its own record, and the other client's PlayerState still exists (R4).

---

## Implementation Notes

*Derived from ADR-0005 sections 3 and 5 and Implementation Guidelines 1 and 4, and GDD R4 ("No engine inactive-PlayerState reuse"), Edge Cases ("If the dev build has no online subsystem"):*

- **Blocker, stated precisely:** spike S4 has not reported. It must establish that null-subsystem keys supplied by `-IdentityKey=` (or the PIE instance index) are accepted, that two clients on one machine never collapse into one record, and that the retained PlayerState is re-attached to a new controller with no player-array change on any client. **Unblocks when** S4 reports PASS (it also unblocks Story 059). The multi-process harness shares its travel/launch support with S3, which has not reported either.
- Rule being proved (R4): reclaim is decided by the Roster key alone, never by address. The address-only duplicate check that would destroy a second same-machine client's inactive PlayerState lives in `AGameMode::AddInactivePlayer`; the game mode must stay on `AGameModeBase`. With no online subsystem, two PIE clients never collapse into one record, and a restarted instance with the same key reclaims its record.
- Test shape (T3, [MP-2]): a standalone multi-process run, one listen server plus two client processes launched with distinct `-IdentityKey=` values and the null online subsystem, plus a server-side store dump hook. Both clients join; assert 2 records. "Rejoin" is defined here as process restart with the same key (QA report: needs a defined action); the restarted client's `RegisterParticipant` returns `Reclaimed` for its own record (same id, same handle); assert the other client's PlayerState still exists on the server (and in the player array) throughout.
- The store dump hook is a test seam in `StealthTestHarness` (never compiled into Shipping); it must not log identity keys beyond a 4-byte prefix (ADR-0005 section 3: keys are never logged beyond that).
- Waits are conditions with a timeout counted in server frames or integer milliseconds (never wall-clock sleeps); record the active replication system as legacy.
- Open point: the unauthenticated key is an accepted MVP risk (R3, OQ13), so a spoofed key is out of scope here.
- Prototype disposition: no prototype counterpart for the Roster record; the prototype `AStealthGameMode` also stayed on `AGameModeBase` but identified players by controller. `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` is reference only and is never promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029 (Reclaim restores the same record for active and informant participants): the pure-core reclaim behaviour.
- Story 009 (Session connection states project onto Connection fields only): the connection projection.
- Story 002 (BeginMatch and registration assign ids, handles and reject a full roster): registration and ids.
- Story 058 (Roster identity and team state survive seamless and hard travel unchanged): travel persistence.
- Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): rejoin within grace with pawn repossession.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterSameMachineClientsTest.cpp` (T3 multi-process node, name `StealthGame.Roster.MP.SameMachineClientsDistinct`)
- `Source/StealthTestHarness/Private/Roster/StealthRosterStoreDumpHook.h` and `.cpp` (server-side store dump hook; shared with Stories 058, 059, 060)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: two client processes on one machine with distinct identity keys get distinct records.
  - Given: a listen server and two client processes with distinct -IdentityKey= and the null subsystem, a server-side store dump hook.
  - When: both join, then one process is restarted with its same key.
  - Then: 2 records; the rejoin returns Reclaimed for its own record; the other client's PlayerState still exists.
  - Edge cases: blocked by S4; "rejoin" needs a defined action (process restart with the same key).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterSameMachineClientsTest.cpp` (`StealthGame.Roster.MP.SameMachineClientsDistinct`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-029-reclaim-core.md`
- Cross-epic: `production/epics/session-travel/EPIC.md` (identity subsystem, admission and rejoin path used by the multi-process run); spike S4 must report before this story is unblocked
- Unlocks: `production/epics/participant-state-roster/story-059-mp-rejoin-within-grace.md`
