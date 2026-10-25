# Story 003: Stick look response and no aim assist

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-???` (no registry entry: CS-R9 and CS-R14 are not among the eight TR-IDs)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: Stick response (F6) and Reduce camera motion are local settings applied before `EvaluateViewEnvelope`; the camera adds no lock-on, snapping, magnetism, slowdown, zoom, shake or FOV punch (Implementation Guideline 12, CS-R9).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this story is plain C++ math and a replay assertion, pre-cutoff)
**Engine Notes**: No post-cutoff API. The stick values reach the code as local user settings (Enhanced Input value after the IIF look binding), not as tuning keys. No ADR-0015 verification item gates it.

**Control Manifest Rules (this layer)**:
- Required: Stick response is the scaled radial dead zone then `((|x| − dz)/(outer − dz))^StickCurveExp` along the stick direction, before the envelope — source: ADR-0015 (GDD F6)
- Forbidden: Never add lock-on, snapping, magnetism or aim assist — source: ADR-0011, ADR-0015
- Guardrail: Pure function, no allocation

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-15, AC-CS-14), scoped to this story:*

- [ ] **AC1** (AC-CS-15, [U] `StickCurve`): **GIVEN** F6 with the defaults **THEN** pure-right deflection 0.565, 0.10 and 1.0 give yaw rates of 63.6 ± 0.5 °/s, 0 and 180 °/s; **AND** a diagonal of magnitude 0.565 gives 45.0 ± 0.5 °/s yaw and 30.0 ± 0.5 °/s pitch.
- [ ] **AC2** (AC-CS-14, [U] `NoAimAssist`): **GIVEN** an identical injected Look input sequence **WHEN** replayed once with a dummy under screen centre and once with none **THEN** the view rotation trajectories are identical to 0.01°.

---

## Implementation Notes

*Derived from GDD CS-R9, CS-R14 and F6, ADR-0015 §7.5:*

`StickLookResponse(FVector2D Raw, FStealthStickSettings)` returns yaw and pitch rates in °/s: radial dead zone `StickDeadZoneFrac` (0.15), outer zone `StickOuterZoneFrac` (0.98), exponent `StickCurveExp` (1.5, presets 1.0/1.5/2.0), full-deflection rates `StickYawRateDegPerSec` 180 and `StickPitchRateDegPerSec` 120. The dead zone is scaled radial: compute on the magnitude and apply along the direction so diagonals match every implementation. Mouse: raw input times sensitivity, no smoothing, no acceleration. No acceleration ramp, no filtering.

The NoAimAssist test drives the story-002 envelope through the real look pipeline function with a registry that contains a dummy pawn under screen centre and one that does not, and compares per-frame view rotation. It passes trivially only if nothing in the look path reads the pawn registry; that is the point (CS-R9, IIF R15). Pair it with the IIF assertion `input-interaction-framework/story-064-no-ally-attack-no-aim-assist.md`, which covers the control-rotation side.

Prototype replaced: none (the prototype uses stock `AddControllerYawInput`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-view-envelope-pure-function.md`: the envelope that consumes these rates.
- `story-005-look-wiring-control-rotation.md`: binding to `IA_Look` and the controller.
- Settings UI (sensitivity, invert Y, presets): Settings & Accessibility (VS).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthLookResponse.h`
- `Source/StealthGameplay/Private/Core/Camera/StealthLookResponse.cpp`
- `Source/StealthGameplay/Private/Tests/Camera/StickCurveTest.cpp` (class `FCameraStickCurveTest`, name `StealthGame.Camera.StickCurve`)
- `Source/StealthGameplay/Private/Tests/Camera/NoAimAssistTest.cpp` (class `FCameraNoAimAssistTest`, name `StealthGame.Camera.NoAimAssist`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `StickCurve` (AC-CS-15)
  Given: F6 defaults.
  When: pure-right deflections 0.565, 0.10 and 1.0, then a diagonal of magnitude 0.565, are evaluated.
  Then: yaw 63.6 ± 0.5 °/s, 0 (inside the dead zone) and 180 °/s (clamped at the 0.98 outer zone); diagonal 45.0 ± 0.5 °/s yaw and 30.0 ± 0.5 °/s pitch.
  Edge cases: exactly 0.15 gives 0; sensitivity ×2 doubles the rate.

Test: AC2 `NoAimAssist` (AC-CS-14)
  Given: one fixed Look sequence, replayed once with a registered pawn under screen centre and once with none.
  When: both runs go through `StepEnvelope` (a pure-function-only version cannot fail).
  Then: view and control-target trajectories match to 0.01° on every frame.
  Edge cases: the dummy sweeps across the centre; the dummy is inside strike range.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `StickCurveTest.cpp` and `NoAimAssistTest.cpp` under `Source/StealthGameplay/Private/Tests/Camera/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md`, `story-002-view-envelope-pure-function.md` (NoAimAssist replays through it)
- Unlocks: `production/epics/camera-system/story-005-look-wiring-control-rotation.md`
