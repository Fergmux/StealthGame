# Story 028: Informant respawn takes over an unseen crowd pawn

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R2a Crowd takeover, decided cross-GDD rule C1; AC-SCL-38; OQ6 resolved)
**Requirement**: `TR-scl-001` (takeover function), `TR-scl-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (sections 3.2, 3.2a); also ADR-0019 (informant lifecycle, caller)
**ADR Decision Summary**: `TakeOverCrowdPawn(Pawn, Participant, ExpectedEpoch)` owns the eligibility check and the bind atomically (Unbound, not Disabled, `Pose == None`, not in traversal, not running, not concealed, no pending hand-over, not pending teardown), then calls `ICrowdControl::ReleaseControl` and schedules the AI to player hand-over. No actor, component or NetGUID is created; `AppearanceId`, velocity and `HeadLook` are unchanged. `CheckTakeOverEligibility` is the side-effect-free form. No card move is committed unless `TakeOverCrowdPawn` returned `Applied`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Between the bind (in the queue slot) and the possession (next frame start) the pawn is `BoundLive` but crowd-possessed and issuing nothing; every system must treat that as ordinary. Spike S14 verified takeover pointer redelivery in the prototype (re-delivery to the new owner works; the previous owner's former body keeps stale `PlayerState`/`Owner`, handled by the owner-local cleanup of ADR-0001 section 4.1).

**Control Manifest Rules (this layer)**:
- Required: `TakeOverCrowdPawn` checks eligibility and calls `BindPawn` itself; no card move is committed unless it returned `Applied`.
- Forbidden: never call `BindPawn` from a takeover caller; never destroy-and-respawn on takeover; never add a component on possession.
- Guardrail: none specific.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-38** [P] `InformantRespawnTakesOverUnseenCrowdPawn`: GIVEN a killed participant and 50 pawns, of which one eligible crowd pawn near `TeamSpawn` has been unrendered on all 3 clients for ≥ 5 s, WHEN informant respawn runs, THEN the participant is bound to that same actor (same NetGUID), the pawn count stays 50 after Crowd's replacement for the killed body, `SpawnSharedPawn` is called once (the replacement) and never for the participant, `AppearanceId` and velocity are unchanged, and the head moves ≤ 1 quantum; AND a pick that is running, traversing, braced or concealed returns `NotEligible` with no state change.

*QA clarification (create-stories 2026-10-25, not a GDD change):* "near `TeamSpawn`" is the Level Socket Contract's `InformantRespawnPathCm` key (LSC and Penalty GDDs), and one head quantum is one step of the `HeadLook` yaw byte (0.55° at the 70° clamp); the traversing-pick case is deferred to the traversal stories (Blocked on spike 045) and asserted there. Confirm both key names at `/story-readiness`.

---

## Implementation Notes

*Derived from ADR-0010 sections 3.2-3.2a and 4.3:*

- Implement `TakeOverCrowdPawn` and `CheckTakeOverEligibility` on `UStealthPawnFactory` with one shared predicate (same code for both). Reads `IsRunning`/`IsInTraversal` (story 013), `FActionState.Pose`, Concealment's server query, pending hand-over and `IsPendingTeardown` (ADR-0002).
- On `Applied`: `Roster.BindPawn`, `ICrowdControl::ReleaseControl(Pawn)`, then schedule the AI to player hand-over through story 027's mechanism. On any failure change nothing.
- The test uses stubs for Penalty's pick, Roster's `BindPawn` and Crowd's `ReleaseControl`/replacement, and asserts through the creation-token trace of story 006 that the factory created exactly one pawn (the replacement).
- **Prototype replaced**: the prototype's informant respawn through `RestartPlayer` (spike S14 R7b) is replaced by this takeover.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 027: the hand-over pass itself.
- NPC-Kill Penalty's candidate selection and no-candidate retry, and the card moves (`TakeNpcCard`) (`production/epics/npc-kill-penalty-informants/EPIC.md`).
- Crowd's replacement spawn for the killed body (Crowd epic).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnFactory.cpp` (`TakeOverCrowdPawn`, `CheckTakeOverEligibility`)
- `Source/StealthGameplay/Private/Tests/Locomotion/InformantRespawnTakesOverUnseenCrowdPawnTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (conditional on naming the key at `/story-readiness`; traversing-pick case deferred). Test `StealthGame.Locomotion.PIE.InformantRespawnTakesOverUnseenCrowdPawn.PickedPawn`.*

Test: informant respawn takes over an unseen crowd pawn (AC-SCL-38)
  Given: a killed participant and 50 pawns, one eligible crowd pawn within `InformantRespawnPathCm` of `TeamSpawn` unrendered on all 3 clients for ≥ 5 s
  When: informant respawn runs (stubbed Penalty pick, Roster, Crowd release and replacement)
  Then: the participant is bound to that same actor (same NetGUID); the pawn count stays 50 after Crowd's replacement for the killed body; `SpawnSharedPawn` is called once (the replacement) and never for the participant; `AppearanceId` and velocity are unchanged and the head moves ≤ 1 quantum (0.55°); a pick that is running, braced or concealed returns `NotEligible` with no state change
  Edge cases: the traversing pick is asserted by the traversal stories; confirm `InformantRespawnPathCm` and the 0.55° quantum at `/story-readiness`

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/InformantRespawnTakesOverUnseenCrowdPawnTest.cpp` — must exist and pass; retained screenshot of the participant in the taken-over body in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-006-single-spawn-site.md`, `story-013-isrunning-hysteresis.md`, `story-027-handover-blend.md`; `production/epics/participant-state-roster/EPIC.md`, `production/epics/npc-kill-penalty-informants/EPIC.md`, `production/epics/crowd-npc-behaviour/EPIC.md`
- Unlocks: `production/epics/npc-kill-penalty-informants/EPIC.md` (takeover caller)
