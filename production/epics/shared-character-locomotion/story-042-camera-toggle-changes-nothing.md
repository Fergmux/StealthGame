# Story 042: Toggling first and third person changes nothing an observer can see

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R9 Camera mode never changes the body; AC-SCL-30)
**Requirement**: `TR-scl-005`, `TR-scl-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (identity blindness; camera mode is local) with ADR-0013 (tiering inputs) and ADR-0015 (camera is computed only in the camera manager, nothing replicated about mode)
**ADR Decision Summary**: First/third-person toggling is local: it changes no replicated property, no component, no pose, no footstep, no movement rule and no tiering input other than the local camera's own FOV. Owner-local presentation allowed: hiding the owner's own head mesh in first person and head, neck and hair in close third person; the camera itself.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `bUseClientSideCameraUpdates = false` and `bAutoManageActiveCameraTarget = false` on the player controller (ADR-0015); no `UCameraComponent` on the pawn. The observer comparison is against a no-toggle control run, so run-to-run network jitter sets a noise floor that must be measured first.

**Control Manifest Rules (this layer)**:
- Required: the camera is computed only in `AStealthPlayerCameraManager`; owner-local head hide is the only allowed pawn-side presentation change.
- Forbidden: never put a `UCameraComponent`, `USpringArmComponent` or Gameplay Cameras component on the Shared Pawn; never replicate camera mode; never read local control in animation.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-30** [N] `CameraToggleChangesNothing`: GIVEN a player toggling first/third person 20 times, WHEN an observer records the pawn's replicated properties, pose, tier and step events, THEN the replicated property set and values, the step-event stream (count and fields) and the tier at each frame are identical to a no-toggle control run, and every bone rotation of the observed pose is within 0.5° of the control at equal server timestamps.

*QA clarification (create-stories 2026-10-25, not a GDD change):* a control-vs-control baseline (two no-toggle runs) measures the noise floor first; the noise floor must be under 0.5°, otherwise the result is INCONCLUSIVE, not Pass; the toggle run must then stay within 0.5° of the control.

---

## Implementation Notes

*Derived from GDD R9 and ADR-0015:*

- Build the observer capture on the harness: third client records replicated properties, pose bone rotations, tier and step events for a player toggling 20 times and for a control run without toggles, plus the second control run for the noise floor.
- The pawn-side behaviour is mostly absence: nothing in the pawn, movement component or step clock may read camera mode. The owner-local head hide is the allowed exception and must not change anything replicated.
- **Prototype replaced**: the prototype's first/third-person toggle (`2026-10-24_client1-first-person-toggle.png`) lives in `AStealthCharacter`/player controller; production camera is the Camera epic's.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The camera itself and its mask (`production/epics/camera-system/EPIC.md`).
- Story 039: tier application. Story 043: footsteps.

---

## Files

- `Source/StealthTestHarness/Private/Locomotion/CameraToggleChangesNothingTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.PIE.CameraToggleChangesNothing.TwentyToggles`.*

Test: camera toggle changes nothing (AC-SCL-30)
  Given: a player toggling first/third person 20 times, a no-toggle control run, and a second no-toggle control run for the noise floor
  When: an observer records replicated properties, pose, tier and step events
  Then: the control-vs-control noise floor is < 0.5° (if it is 0.5° or more the result is INCONCLUSIVE, not Pass); the replicated property set and values, the step-event stream (count and fields) and the tier at each frame are identical to the control; every bone rotation is within 0.5° of the control at equal server timestamps
  Edge cases: the network profile is stated in the harness config and identical across the three runs

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/CameraToggleChangesNothingTest.cpp` — must exist and pass (multi-client PIE); retained screenshots of the toggle on the owner and of the unchanged pawn on the observer in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-022-remote-view-pitch-clamped.md`, `story-039-tier-parity-ab.md`; `production/epics/camera-system/EPIC.md` (toggle)
- Unlocks: None
