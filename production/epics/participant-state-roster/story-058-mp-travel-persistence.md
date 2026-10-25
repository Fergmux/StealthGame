# Story 058: Roster identity and team state survive seamless and hard travel unchanged

> **Epic**: Participant State & Roster
> **Status**: Ready — unblocked 2026-10-25: spike S3 reported PASS (production/qa/spikes/2026-10-25_S3-seamless-travel.md): controller/PlayerState replaced on seamless arrival (E5), re-bind by key on seamless and hard travel, owner-only team view re-pushed and acknowledged before the first round tick, same-map travel through /Engine/Maps/Entry, all four game-instance stores intact. Note S3 finding 3: Session must treat connection closes during its own hard travel as travel.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0005 keeps authoritative match state only in four game-instance stores (the Roster is `URosterStore` over `FRosterCore`), because default seamless travel replaces every PlayerController and PlayerState (E5); a `UGameInstanceSubsystem` lives as long as the game instance and so survives seamless and hard travel. On arrival the `HandleSeamlessTravelPlayer` override re-binds (`RebindView`) and the Roster rewrites the view fields from the record; Session calls `InvalidateAllBindings` at travel start (it clears every binding and tombstone, never a lifecycle state). Acceptance tests for travel run multi-process standalone because single-process PIE cannot exercise seamless travel by default (E7).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No new post-cutoff APIs; the 5.6 `DispatchPostLogin` deprecation applies (use `PostLogin`/`OnPostLogin`); `UE_LOGF` for logging. Verification required: **S3** (seamless and hard travel in multi-process standalone: PlayerController/PlayerState replacement, re-bind, C-TeamView re-push, same-map travel through the transition map, game-instance stores intact), S4 and S5. Seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`; a PIE seamless result is labelled non-representative. S3 has not reported.

**Control Manifest Rules (this layer)**:
- Required: authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer; `EndMatch` runs on every session exit path — source: ADR-0005
- Required: seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`; travel results come from multi-process standalone — source: ADR-0005
- Required: T2/T3 tests are bound by frames or server time and assert and record the active replication system — source: ADR-0008
- Forbidden: Never hard-travel every round (fallback only if spike S3 fails), never run rounds as level streaming in one persistent world, never keep the PlayerController across seamless travel — source: ADR-0005
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them — source: ADR-0005
- Guardrail: travel load bounded by `TravelLoadTimeoutSec` (60 s) — source: ADR-0005

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-40a** [MP-3][S3] `StealthGame.Roster.MP.TravelPersistence`: GIVEN a match across one seamless and one hard travel, WHEN the new map's first round tick runs, THEN `ParticipantId`, `PublicHandle`, `Team`, `IdentityKey`, `AppearanceHistory`, `MatchId` and `RoundEpoch` equal their pre-travel values, `InvalidateAllBindings` ran once at travel start, and every client's C-TeamView content equals the server's before that tick (R4, R6).

---

## Implementation Notes

*Derived from ADR-0005 sections 2, 4, 5 and Implementation Guidelines 3, 10, 12, and GDD R3, R4, R6, R9 (`InvalidateAllBindings`):*

- **Blocker, stated precisely:** spike S3 has not reported. It must show, in a multi-process standalone run (not single-process PIE), that the store survives one seamless and one hard travel, that `RebindView` re-associates the replaced PlayerController and PlayerState, that C-TeamView is re-pushed to the new controller, and that the same-map lobby to match travel works through the transition map (if S3 fails, `LobbyMap` and `MatchMap` are distinct maps from day one). **Unblocks when** S3 reports; this story depends on Story 041 (re-push on controller swap) in addition.
- What must persist (R3 "Match" fields, survive round changes, seamless travel and disconnect or rejoin): `ParticipantId`, `PublicHandle`, `Team`, `IdentityKey`, `AppearanceHistory`, plus the store's `MatchId` and `RoundEpoch`. The Roster is a `UGameInstanceSubsystem` facade, so it is not touched by GameMode, GameState or PlayerState replacement; `AStealthPlayerState::CopyProperties` carries nothing authoritative.
- `InvalidateAllBindings()` is called by Session at travel start (`OnTravelStarting`); it clears every live binding and tombstone but no lifecycle state, and raises no event. Assert it ran exactly once per travel start, and that no lifecycle state or `Team` changed because of it.
- Test shape (T3, [MP-3]): a standalone multi-process run, a listen server and three clients, one seamless and one hard travel, with `AppearanceHistoryDepth > 0` so `AppearanceHistory` is non-empty. A pre-travel snapshot is taken after `ResetAll`; after the new map's first round tick, every listed field equals the snapshot; every client's C-TeamView content equals the server's projection before that tick. Wait on conditions with timeouts counted in server frames or integer milliseconds; the snapshot is taken through the store dump hook shared with Stories 057, 059, 060.
- Seamless-travel ordering to respect (ADR-0005 section 4, E19): the host's rebind runs in `PostSeamlessTravel` before world `BeginPlay`; remote controllers arrive later through `ServerNotifyLoadedWorld` and `HandleSeamlessTravelPlayer`. `RebindView` writes only view fields and does not depend on world wiring.
- Carried PlayerStates of disconnected participants (E21) are kept on round and fallback travel and rebound by Session in `PostSeamlessTravel`; on lobby travel they are dropped, matching their records becoming `Dormant`. Not asserted here beyond identity fields.
- Prototype disposition: replaces the prototype's hard-travel-every-round approach (ADR-0005 Alternative 2) and the participant/team bookkeeping that lived on PlayerState and GameMode (`StealthPlayerState.h`, `StealthGameMode.cpp`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 041 (A replaced controller or reclaim re-pushes C-TeamView within one server frame): the single-process re-push mechanism.
- Story 060 (Host loss empties every client's roster store): host loss.
- Story 057 (Two client processes on one machine with distinct identity keys get distinct records): same-machine identity.
- Story 059 (A client rejoining within grace reclaims its record, pawn and informant status): rejoin within grace.
- Story 012 (Clearing a live binding during Active faults, but level-teardown clears are silent): teardown-time `ClearBinding` silence.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Roster/StealthRosterTravelPersistenceTest.cpp` (T3 multi-process node, name `StealthGame.Roster.MP.TravelPersistence`)
- `Source/StealthTestHarness/Private/Roster/StealthRosterStoreDumpHook.h` and `.cpp` (store snapshot hook; created by Story 057 or here, shared)
- `Source/StealthFoundation/Private/Roster/StealthRosterStore.cpp` (`RebindView` and `InvalidateAllBindings` wiring only if not already complete; shared file)
- Header names follow ADR-0007: `Stealth` prefix, unique project-wide.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: roster identity and team state survive seamless and hard travel unchanged.
  - Given: a multi-process run with one seamless and one hard travel, depth > 0 so AppearanceHistory is non-empty, a pre-travel snapshot taken after ResetAll.
  - When: the new map's first round tick runs.
  - Then: ParticipantId, PublicHandle, Team, IdentityKey, AppearanceHistory, MatchId, RoundEpoch equal the snapshot. InvalidateAllBindings ran once per travel start. Every client's C-TeamView equals the server's before the tick.
  - Edge cases: wait on a condition with a timeout. Blocked by S3.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: automated test `Source/StealthTestHarness/Private/Roster/StealthRosterTravelPersistenceTest.cpp` (`StealthGame.Roster.MP.TravelPersistence`) OR a documented playtest; `qa.level` is full, so the automated test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-041-team-view-repush-on-controller-swap.md`
- Cross-epic: `production/epics/session-travel/EPIC.md` (travel path, `OnTravelStarting`, transition map); spike S3 must report before this story is unblocked
- Unlocks: `production/epics/participant-state-roster/story-060-mp-host-loss.md`
