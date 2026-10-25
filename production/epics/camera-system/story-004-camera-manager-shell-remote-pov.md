# Story 004: Camera manager shell, controller defaults and remote-controller POV

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0003 for the camera-mode-not-replicated rule)
**ADR Decision Summary**: `AStealthPlayerCameraManager : APlayerCameraManager` in `StealthGameplay/Core` with `bUseClientSideCameraUpdates = false`, `bClientSimulatingViewTarget = true`, no modifiers, shakes or lens effects; `AStealthPlayerController` sets `PlayerCameraManagerClass`, `bAutoManageActiveCameraTarget = false`, and the manager overrides `SetViewTarget` to accept only targets it requests. On a non-local controller the manager yields only the eye-only POV and runs no boom, collision, mask or trace. The Shared Pawn carries no camera, boom or Gameplay Cameras component.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Verified in 5.8.4 source (ADR-0015 "Verified engine facts"): `bUseClientSideCameraUpdates` false means the client never sends `ServerUpdateCamera` and the server ignores a forged one, but `DoUpdateCamera` then also runs on the server for remote controllers; `bAutoManageActiveCameraTarget=false` does not stop `NotifyLoadedWorld`, `Reset`/`ClientReset`, `OnUnPossess` or `SetSeamlessTravelViewTarget` from calling `SetViewTarget`. ADR-0015 Verification Required items (4) zero `ServerUpdateCamera` calls and server-side camera stays eye-only, and (5) the `SetViewTarget` override across seamless travel and round reset, are settled by this story's tests; the seamless-travel and round-reset first-frame cases of AC-CS-37 are in story 017.

**Control Manifest Rules (this layer)**:
- Required: The camera is computed only in `AStealthPlayerCameraManager`; `bUseClientSideCameraUpdates = false` and `bAutoManageActiveCameraTarget = false` on `AStealthPlayerController`; on a non-local controller the manager computes only the eye-only POV — source: ADR-0015
- Forbidden: Never put a `UCameraComponent`, `USpringArmComponent` or Gameplay Cameras component on the Shared Pawn or its Blueprints; no code may call `ServerUpdateCamera` — source: ADR-0015
- Guardrail: Zero camera traffic on the wire; server cost per remote controller is a few vector operations

This is the structural story of the epic: it creates the manager class and the controller defaults that every later camera story builds on, and proves the "never observable" rule at the engine-flag level. It supersedes the prototype's component-based camera (`FirstPersonCamera` plus the sample's Gameplay Cameras rig) without touching the sample camera assets.

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-27, AC-CS-02) and ADR-0015 §1 (derived items marked), scoped to this story:*

- [ ] **AC1** (AC-CS-27, [N] `NoServerCameraTraffic`): **GIVEN** a client in 3P that toggles, swaps shoulder and changes FOV for 60 s **WHEN** the network is traced on both ends **THEN** zero `ServerUpdateCamera` (or equivalent lens-transform) calls are sent, and the server-side PlayerCameraManager holds no lens transform from the client (5.8 names verified).
- [ ] **AC2** (AC-CS-02, [N] `NoCameraComponentsHostParity`): **GIVEN** the listen-server host in 1P and a client **WHEN** the client compares component lists on the server, owner and simulated proxy **THEN** all are identical, and the Shared Pawn class has no camera or boom component; **AND** the host pawn's head is visible on the client, with root-motion end positions within 1 cm of a 3P-host run.
- [ ] **AC3** (derived, ADR-0015 §1 and Guideline 3): **GIVEN** a non-local controller on the server **WHEN** its camera manager updates **THEN** the POV is the capsule axis at the pawn's base eye height plus the control rotation (or the controller's own location and rotation when it has no pawn), identical for every remote controller, and no boom, collision, mask or trace runs.
- [ ] **AC4** (derived, ADR-0015 §1 and R-5): **GIVEN** an engine path calls `SetViewTarget` with a target the manager did not request **WHEN** the call arrives **THEN** it is ignored and counted (non-Shipping log), and `UpdateViewTarget` still computes the view from the manager's own state.

---

## Implementation Notes

*Derived from ADR-0015 Decision §1 and Implementation Guidelines 1-3:*

`AStealthPlayerCameraManager` lives in `StealthGameplay/Core` with its header in `Public/View/Core/` (client code with no server facts, ADR-0007 §2). Class defaults as in the ADR (`bAlwaysApplyModifiers = false`, nothing registered). `AStealthPlayerController` (the composition root's controller, ADR-0007) sets `PlayerCameraManagerClass` and `bAutoManageActiveCameraTarget = false`. The manager overrides `SetViewTarget` (virtual) to accept only targets it requests through a private `RequestViewTarget` (its own possessed pawn or its `AStealthNeutralViewActor`, story 017), and keeps the released body as view target during a factory-initiated unpossess (story 018); `AStealthPlayerController::OnUnPossess` skips the engine's `SetViewTarget(this)` for those unpossesses. `UpdateViewTarget` branches on `PCOwner->IsLocalController()`: non-local gets only the eye-only POV (never the eye socket, which only the local client reads). This POV is also the connection's relevancy viewer.

The rig's math is not in this class: `FStealthCameraCore` and `FEyeMaskCore` are plain C++ cores with injected backends and the manager is a thin adapter. This story creates the (empty) adapter wiring; the cores arrive with stories 002, 006, 010 and 012. The asset audit that fails on a camera-type component in the pawn hierarchy is the ADR-0008 reflection audit; add the camera-component class list to it if not present.

Network tests are CQTest PIE tests in the `StealthTestHarness` module (`StealthGame.Camera.PIE.<Scenario>.<Method>`, test-standards CQTest exemption). Never edit or resave the sample camera assets listed in `production/never-resave-assets.md`; the sample's Gameplay Cameras plugin stays enabled and unused.

Prototype replaced: the `AStealthCharacter` camera setup (`FirstPersonCamera`, `ToggleCameraMode`, sample camera activation); actual removal is story 025 once the production pawn exists.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-005-look-wiring-control-rotation.md`: look input into control rotation.
- `story-006-eye-pivot-boom-collision.md`, `story-008-toggle-blend-reduce-motion-remembered-mode.md`: the live rig for the local controller.
- `story-017-view-state-neutral-view-fade.md`: the neutral view actor and the seamless-travel / round-reset first-frame cases.
- `story-021-camera-mode-not-observable.md`: the full ModeNotObservable run.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthPlayerCameraManager.h`
- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp`
- `Source/StealthGame/Private/StealthPlayerController.cpp` (or the production controller named by the composition-root story): `PlayerCameraManagerClass`, `bAutoManageActiveCameraTarget`, `OnUnPossess`
- `Source/StealthTestHarness/Private/Camera/NoServerCameraTrafficTest.cpp` (CQTest, `StealthGame.Camera.PIE.NoServerCameraTraffic.*`)
- `Source/StealthTestHarness/Private/Camera/NoCameraComponentsHostParityTest.cpp` (CQTest, `StealthGame.Camera.PIE.NoCameraComponentsHostParity.*`)
- `Source/StealthGameplay/Private/Tests/Camera/RemotePovTest.cpp` (class `FCameraRemotePovTest`, name `StealthGame.Camera.RemotePov`) and `ViewTargetOwnershipTest.cpp` (class `FCameraViewTargetOwnershipTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC1: AC-CS-27 says "`ServerUpdateCamera` (or equivalent lens-transform)" and carries "[verify 5.8 names]". The RPC list and the counting mechanism (a net-driver RPC hook, or assert-not-declared) are not fixed. QA-lead proposed rewrite (unconfirmed): enumerate the camera and view RPCs from the 5.8.4 `PlayerController.cpp` and count them with a net-driver hook.
- Scenario sketches: AC1 forged `ServerUpdateCamera` from the test client is ignored, server camera POV equals the eye-only POV. AC2 component class lists identical on server, owner and simulated proxy, none in the class or Blueprint hierarchy, host head visible on the client, root-motion end positions within 1 cm of a 3P-host run. AC3 two remote controllers (one with a pawn, one without), run with the client in 1P and in 3P: POV identical, injected trace/boom/mask backend call counts 0, and the host's own local controller still runs the full rig as a positive control. AC4 a decoy `SetViewTarget` (the `OnUnPossess` and `NotifyLoadedWorld` paths) is ignored and counted, `RequestViewTarget` of own pawn and of the neutral actor is accepted, POV unchanged with the decoy 10 km away; the story must define a non-Shipping accessor for the counter.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE tests under `Source/StealthTestHarness/Private/Camera/` and the two unit tests under `Source/StealthGameplay/Private/Tests/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic: tuning-data-config `story-002-gameplay-presentation-module-skeletons.md`, `story-004-test-harness-module-shipping-excluded.md`, `story-019-cqtest-headless-proof.md`; shared-character-locomotion `story-001-pawn-class-default-subobjects.md` (the production `AStealthPawn` with no camera component)
- Unlocks: `production/epics/camera-system/story-005-look-wiring-control-rotation.md`, `story-006-eye-pivot-boom-collision.md`, `story-008-toggle-blend-reduce-motion-remembered-mode.md`, `story-017-view-state-neutral-view-fade.md`
