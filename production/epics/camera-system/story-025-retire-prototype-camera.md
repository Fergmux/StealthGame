# Story 025: Retire the prototype camera

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (Migration Plan 1; ADR-0007 §8 prototype promotion rule; ADR-0009 §4 never-resave)
**ADR Decision Summary**: The prototype's debug camera (`StealthDebugCam` exec), the sample `CBP_SandboxCharacter` camera setup and its Gameplay Cameras assets are not promoted; the production pawn is created without camera components; the sample's camera assets stay in the project unused and on the never-resave list.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: **Never let the editor resave the sample camera assets** (`production/never-resave-assets.md`: `Content/Blueprints/Cameras/CameraAsset_SandboxCharacter`, `CameraDirector_SandboxCharacter`, `CameraRigPrefab_BasicThirdPersonBehavior`, `CameraRig_BaseDefaults`, `CameraRig_CollisionOffset`): a 5.8 resave broke the CameraDirector import on 2026-10-24. This story therefore removes camera references through C++ and config only, and never opens, locks or saves those assets. The Gameplay Cameras plugin stays enabled (sample content references it) but no production class references it. Deleting prototype source files is an always-ask category; the user's blanket approval for this run covers it, and the deletion is logged.

**Control Manifest Rules (this layer)**:
- Required: Promotion is a rewrite, not a file move; prototype classes keep compiling until replaced and are deleted only with user approval — source: ADR-0007 §8
- Forbidden: Never put a `UCameraComponent`, `USpringArmComponent` or Gameplay Cameras component on the Shared Pawn or its Blueprints; never resave the sample camera assets — source: ADR-0015, ADR-0009
- Guardrail: A pre-commit check rejects a commit that modifies a never-resave asset (tuning-data-config `story-026-never-resave-commit-check.md`)

This story closes the epic's "Prototype Disposition": once the production pawn (SCL) and the camera manager (stories 004-008) exist, the prototype camera code is removed so only one camera path exists.

---

## Acceptance Criteria

*Derived from the epic's Prototype Disposition, ADR-0015 Migration Plan 1 and `production/never-resave-assets.md` (no GDD criterion):*

- [ ] **AC1** (derived): **GIVEN** the production pawn exists and the camera manager is live **THEN** `AStealthCharacter` has no `UCameraComponent`/`FirstPersonCamera`, no `ToggleCameraMode`, no `bFirstPerson` and no camera-mode V binding, and no class in the production modules (`StealthFoundation`, `StealthGameplay`, `StealthPresentation`) references the sample's Gameplay Cameras assets or components.
- [ ] **AC2** (derived): **GIVEN** the `StealthDebugCam` console command **THEN** it is either removed, or kept only as a non-Shipping developer command that changes the view through the camera manager's `RequestViewTarget` (never a direct `SetViewTarget`).
- [ ] **AC3** (derived, ADR-0009 §4): **GIVEN** the SHA-256 of the five assets in `production/never-resave-assets.md` recorded at story start (baseline below) **WHEN** the hashes are recomputed at story close **THEN** all five are unchanged and the pre-commit never-resave check passes (no sample camera asset was modified or resaved). *(Revised at create-stories: the `_snapshots/2026-10-24_sample-blueprints-pre-resave/Cameras/` folder holds only four of the five assets, with no `CameraDirector_SandboxCharacter` copy, and none of the four snapshot files hashes equal to the current files, so a diff against the snapshot cannot pass. The baseline is the current files.)*

Baseline SHA-256, recorded 2026-10-25 by `/create-stories` (recompute at story start; if any differs, stop and investigate before editing anything):

| Asset | SHA-256 |
|---|---|
| `Content/Blueprints/Cameras/CameraAsset_SandboxCharacter.uasset` | `441575e22a7ea2a788544b5a5ea8e9f25e43d815675ff8a7ff772cb6efad9dce` |
| `Content/Blueprints/Cameras/CameraDirector_SandboxCharacter.uasset` | `e63ad200b3de410cff7fd1c3a3fa3a983d2cc12a8203d7f56adea3d302e2f7fd` |
| `Content/Blueprints/Cameras/CameraRigPrefab_BasicThirdPersonBehavior.uasset` | `b2c51ea5772c76ec5aa685479d9d04c3bc30f91871a6e4af3f0085beda294330` |
| `Content/Blueprints/Cameras/CameraRig_BaseDefaults.uasset` | `325acbb87559356f9f7e8dca5875a92a274d1cc8c6b3e319268cfb099dd73cb3` |
| `Content/Blueprints/Cameras/CameraRig_CollisionOffset.uasset` | `64a4aac3a78c742c075419560e4f1c7f8894ffadccb68c988ff3a17fa31abec6` |

---

## Implementation Notes

*Derived from the epic's Prototype Disposition and ADR-0015 Migration Plan:*

Prototype code to retire: `Source/StealthGame/Private/StealthCharacter.cpp` `FirstPersonCamera` creation (lines 52-55), `ToggleCameraMode` (lines 455-495, including the sample `GameplayCamera` component search and the `SetupCamera` `ProcessEvent` call), the V binding (line 111), `bFirstPerson`/`IsFirstPerson`; `StealthPlayerController.cpp` `StealthDebugCam` (lines 101-142) and `DebugCamera`; `StealthHUD.cpp` line 279 help text ("V camera"). Sequence this after IIF `story-093-static-scan-and-prototype-retirement.md` so the key binding removal is not done twice. The HUD or other prototype code that calls `IsFirstPerson()` must be switched to the camera manager's view state first.

Reparented sample Blueprints: ADR-0007 §8 requires `[CoreRedirects]` and explicit-path resaves for moved classes; this story moves no class, so no Blueprint needs saving. Any Blueprint resave in this work must be by explicit path, one asset, never one of the never-resave list. AC3 is mechanical: `sha256sum` the five files and compare with the baseline table above; run it in the story's evidence step and in the pre-commit check. The snapshot folder is not a valid comparison target (four of five files, none identical to the current ones).

Prototype replaced: this is the retirement of `StealthCharacter.cpp` camera code, `StealthPlayerController.cpp` `StealthDebugCam`, and the HUD key hint.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-camera-manager-shell-remote-pov.md` to `story-008-toggle-blend-reduce-motion-remembered-mode.md`: the production replacement.
- IIF `story-093-static-scan-and-prototype-retirement.md`: removal of prototype per-feature input and RPCs.
- tuning-data-config `story-026-never-resave-commit-check.md`: the commit-time guard.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Private/StealthCharacter.cpp`, `Source/StealthGame/Public/StealthCharacter.h` (camera code removed)
- `Source/StealthGame/Private/StealthPlayerController.cpp`, `Source/StealthGame/Public/StealthPlayerController.h` (`StealthDebugCam`)
- `Source/StealthGame/Private/StealthHUD.cpp` (help text)
- `Source/StealthGameplay/Private/Tests/Camera/NoPrototypeCameraScanTest.cpp` (class `FCameraNoPrototypeCameraScanTest`, name `StealthGame.Camera.NoPrototypeCameraScan`)
- `tools/lint/` never-resave hash comparison for the five camera assets

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS (reviewed before the AC3 revision above; the revision answers the AC3 gap)
- AC3 gap (answered): the snapshot folder holds four of the five assets and none hashes equal to the current files; the criterion now compares against the baseline recorded in the story.
- AC1 sketch: a reflection audit of `AStealthCharacter` and its Blueprint subclasses finds no camera component, no `FirstPersonCamera` or `bFirstPerson` property and no `ToggleCameraMode` function, and no input action maps to the removed handler (check the handler or action, not the V key, because the production toggle also uses V); a grep of `Source/` and the `Build.cs` files finds no Gameplay Cameras includes, module dependencies or `/Game/Blueprints/Cameras` references. At create-stories time there are 17 prototype-symbol matches across `StealthCharacter.h/.cpp` and `StealthPlayerController.h/.cpp`, so the test fails until the story runs; skip, do not pass, until the production pawn exists.
- AC2 sketch: either the `StealthDebugCam` symbol is absent, or it sits inside `#if !UE_BUILD_SHIPPING` and a spy shows its only view change is the manager's `RequestViewTarget` (private per ADR-0015 §1, so a dev-only friend or accessor is needed).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `NoPrototypeCameraScanTest.cpp` under `Source/StealthGameplay/Private/Tests/Camera/` plus the hash comparison output, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-004-camera-manager-shell-remote-pov.md`, `story-008-toggle-blend-reduce-motion-remembered-mode.md`, `story-017-view-state-neutral-view-fade.md`; cross-epic: shared-character-locomotion `story-001-pawn-class-default-subobjects.md` (production pawn), input-interaction-framework `story-093-static-scan-and-prototype-retirement.md`
- Unlocks: None
