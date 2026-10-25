# Story 008: Toggle blend, Reduce camera motion and remembered mode

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0003 for "nothing about camera mode is replicated")
**ADR Decision Summary**: The toggle and shoulder swap are local; the blend is clamped below IIF's `CameraToggleLockoutSec` minus one frame; the mask applies from t = 0 in both directions; the shoulder swap mirrors over 0.25 s with one buffered press. The mode is remembered in local settings (`UGameUserSettings`-style local config, never tuning, TDC CR-14).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; the blend math is plain C++, settings storage is stock config)
**Engine Notes**: F4 blend interpolates lens position and FOV over `CameraToggleBlendSec` (0.20 s). Use `UGameUserSettings` or an equivalent local config class; verify its 5.8.4 header. No ADR-0015 verification item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Camera mode, boom state, FOV and shoulder side are client settings; nothing replicated or sent depends on them — source: ADR-0015, ADR-0003
- Forbidden: Never replicate camera mode; never store the mode as tuning data — source: ADR-0015, ADR-0004 (CR-14)
- Guardrail: Toggle adds zero RPCs; no per-frame allocation

This story gives the player the two views. It implements the F4 blend with the `CameraToggleLockoutSec` interaction (accepted press at 0.31 s, ignored at 0.25 s), the Reduce camera motion variant (0.08 s crossfade, no lens travel, cuts for swap and recovery), and the default and remembered mode (CS-R2). The IIF input binding for V is `input-interaction-framework/story-014-camera-toggle-local.md`; this story is the camera side of that press.

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-16, AC-CS-32) and CS-R2 / ADR-0015 §7.1 (derived item marked), scoped to this story:*

- [ ] **AC1** (AC-CS-16, [U] `ToggleBlend`): **GIVEN** 3P → 1P **WHEN** V is pressed **THEN** the blend completes in 0.20 s (12 frames at 60 fps), the head is hidden from frame 10 (u ≥ 0.85), and at no frame does the near plane intersect the own head bounds; a second press at 0.25 s is ignored and one at 0.31 s is accepted; with the blend set to 0.13 s and the lockout 0.15 s the blend completes in 0.13 s.
- [ ] **AC2** (AC-CS-32, [P] `ReduceMotion`): **GIVEN** Reduce camera motion on **THEN** the toggle is a 0.08 s crossfade with no lens travel, there is no traversal pivot lag (only the speed cap), and the shoulder swap and boom recovery are cuts. (Its last line, AC-CS-01 still passes with the setting on, is story 021.)
- [ ] **AC3** (derived, CS-R2, ADR-0015 §7.1): **GIVEN** a fresh install **THEN** the default mode is ThirdPerson; **WHEN** the player chooses a mode **THEN** it is stored in local settings (not tuning data), restored across rounds and matches, and nothing is sent to the server.

---

## Implementation Notes

*Derived from ADR-0015 Decision §7.1 and GDD F4, CS-R10, CS-R13:*

State machine per GDD "States and Transitions": `FirstPerson`, `ThirdPerson` (normal/squeezed/close/concealed), `Blend`, `Dying`, `NoPawn`; this story owns `FirstPerson`/`ThirdPerson`/`Blend`. The blend is clamped below the lockout minus one frame by invariant I-CS-1 at load (story 001), so there is no runtime clamp; the unit test must load a tuning set with blend 0.13 / lockout 0.15 through the real load path. The head hides from frame 10 of 12 (u ≥ 0.85) using the local-only hide flag (story 009 implements the hide; this story requests it). Mask applies from t = 0 in both directions: the mask mode flag is published by story 013.

Reduce camera motion is a local setting: toggle = 0.08 s crossfade (use the manager's published screen fade, story 017, or a lens-less crossfade); no traversal pivot lag (the vertical speed cap stays); shoulder swap and boom recovery are cuts; FOV changes apply instantly (story 010). It changes nothing sent to the server. The body-driven view turn while steering stays (CS-R3.3).

Prototype replaced: `AStealthCharacter::ToggleCameraMode` and `bFirstPerson` (V key, `StealthCharacter.cpp:455-495`) — retired in story 025.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-009-first-person-own-head-hide.md`: the head-hide mechanism and screenshots.
- `story-006-eye-pivot-boom-collision.md`: the boom, squeeze and swap geometry.
- `story-021-camera-mode-not-observable.md`: the 2-client observation that mode is invisible.
- IIF `story-014-camera-toggle-local.md`: the V binding and the lockout timer.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/Camera/StealthCameraCore.cpp` (F4 blend, reduce-motion paths)
- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp` (toggle request, state)
- `Source/StealthGameplay/Public/View/Core/StealthCameraSettings.h` and `Private/Core/Camera/StealthCameraSettings.cpp` (local mode and Reduce motion settings)
- `Source/StealthGameplay/Private/Tests/Camera/ToggleBlendTest.cpp` (class `FCameraToggleBlendTest`, name `StealthGame.Camera.ToggleBlend`)
- `Source/StealthGameplay/Private/Tests/Camera/ReduceMotionTest.cpp` (class `FCameraReduceMotionTest`, name `StealthGame.Camera.ReduceMotion`)
- `Source/StealthGameplay/Private/Tests/Camera/RememberedModeTest.cpp` (class `FCameraRememberedModeTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC2: "0.08 s crossfade with no lens travel" has no observable in ADR-0015 §6/§7.1: the manager publishes only a fade alpha and colour, so a true two-view crossfade does not exist, and "boom recovery are cuts" does not say whether the 0.15 s hold applies. QA-lead proposed rewrite (unconfirmed; needs a design decision): published fade alpha rises 0 → 1 over 0.04 s, the lens snaps to the target mode at the midpoint, alpha falls 1 → 0 over the next 0.04 s; a recovery cut snaps to the target length in one frame after the hold; a swap flips the offset in one frame; traversal pivot lag is 0 s.
- Scenario sketches: AC1 target reached at frame 12, head not hidden at frame 9 and hidden at frame 10 (u ≥ 0.85), near plane never intersects a head-bounds fixture, press at 0.25 s ignored and 0.31 s accepted, blend 0.13 with lockout 0.15 completes at frame 8 (the load-time rejection is story 001 AC2; the lockout is IIF's, so inject the key; confirm whether a press at exactly 0.30 s is inclusive). AC3 fresh settings give ThirdPerson, a chosen 1P mode survives a round reset, the TDC snapshot hash is unchanged, RPC count 0, a missing or corrupt value falls back to ThirdPerson.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three tests above under `Source/StealthGameplay/Private/Tests/Camera/` (AC-CS-32 runs on a functional test map) OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md`, `story-004-camera-manager-shell-remote-pov.md`, `story-006-eye-pivot-boom-collision.md`; cross-epic: input-interaction-framework `story-014-camera-toggle-local.md`
- Unlocks: `production/epics/camera-system/story-009-first-person-own-head-hide.md`, `story-021-camera-mode-not-observable.md`
