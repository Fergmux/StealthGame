# Story 010: Field of view

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-???` (no registry entry: CS-R13 FOV and comfort is not among the eight TR-IDs)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: FOV (F5) is Hor+ with the `FovHMaxDeg` (108°) cap for every aspect, a per-aspect slider top, an ease over 0.15 s (instant under Reduce camera motion), and the eye cone never follows FOV. FOV changes send nothing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this story is plain C++ math, pre-cutoff)
**Engine Notes**: Output goes to `FMinimalViewInfo::FOV` and `AspectRatio` constraint handling in the manager's `UpdateViewTarget`; verify aspect-ratio handling (`bConstrainAspectRatio`) for 21:9 and 32:9 in 5.8.4. No ADR-0015 verification item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Horizontal FOV is capped at `FovHMaxDeg` for every aspect so no display gives more periphery than the best setting every player can choose — source: ADR-0015
- Forbidden: FOV never changes with gait, traversal or strike; no FOV punches — source: ADR-0015
- Guardrail: Pure function, no allocation

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-18, AC-CS-31), scoped to this story:*

- [ ] **AC1** (AC-CS-18, [U] `FovCap`): **GIVEN** aspects 16:9, 21:9 and 32:9 at slider values of 75° and 60° **WHEN** F5 runs **THEN** the horizontal FOV is ≤ 108° in every case; at 32:9 and 60° VFOV_eff is 42.3° ± 0.2; the slider top is 75° at 16:9 and 61.1° ± 0.2 at 21:9, and the slider is disabled at 32:9.
- [ ] **AC2** (AC-CS-31, [U] `FovEase`): **GIVEN** a mid-round FOV change from 60° to 70° **THEN** the FOV eases over 0.15 s (instantly under Reduce camera motion), the mask cone is unchanged, and zero RPCs are sent.

---

## Implementation Notes

*Derived from ADR-0015 Decision §7.2 and GDD F5, CS-R13:*

Keys: `FpFovVDeg`/`TpFovVDeg` (user settings, default 60 vertical, hard range 55-75), `FovVMinDeg`/`FovVMaxDeg` (55/75), `FovHMaxDeg` (108). F5 converts the vertical slider to horizontal at the display aspect, caps horizontal at `FovHMaxDeg`, and reports VFOV_eff (the vertical that remains after the cap). The slider is shown to the player in horizontal degrees; the slider UI is Settings (VS), so this story exposes the per-aspect top and a disabled flag through a function. Invariant I-CS-3 (eye cone ≥ half of the FOV maximum) is enforced in story 001; this story must not read the cone. The ease runs in the manager over 0.15 s and reads the Reduce camera motion setting from story 008. Zero RPCs: the unit test asserts no network call path exists from the FOV setter (compile-level: the setter is in a non-RPC class).

Prototype replaced: the sample camera's fixed FOV; none promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-001-camera-tuning-keys-and-invariants.md`: I-CS-3.
- `story-012-eye-mask-core.md`: the fixed eye cone that FOV must not move.
- Settings & Accessibility (VS): the slider UI and extra options.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthFov.h`
- `Source/StealthGameplay/Private/Core/Camera/StealthFov.cpp`
- `Source/StealthGameplay/Private/Tests/Camera/FovCapTest.cpp` (class `FCameraFovCapTest`, name `StealthGame.Camera.FovCap`)
- `Source/StealthGameplay/Private/Tests/Camera/FovEaseTest.cpp` (class `FCameraFovEaseTest`, name `StealthGame.Camera.FovEase`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `FovCap` (AC-CS-18)
  Given: aspects 16/9, 21/9 (2.3333) and 32/9, at slider values 75 and 60.
  When: F5 runs.
  Then: HFOV ≤ 108.0 in all six cases; at 32:9 and 60° VFOV_eff is 42.3 ± 0.2; the slider top is 75.0 at 16:9 and 61.1 ± 0.2 at 21:9; at 32:9 the slider is disabled (the raw top of 42.3 is below `FovVMinDeg` 55).
  Edge cases: aspects 1.25 and 3.6; a saved 75 clamps to 61.1 when switching to 21:9.

Test: AC2 `FovEase` (AC-CS-31)
  Given: a change from 60° to 70°.
  When: the ease runs, with and without Reduce camera motion.
  Then: FOV is monotonic and exactly 70.0 at t0 + 0.15 s (±1 frame) with no overshoot; under Reduce camera motion it is 70 on the same frame; the eye-cone values stay 55/38 every frame; zero RPCs, checked structurally (the FOV core has no net dependency; the end-to-end check is story 021).
  Edge cases: the easing curve is unspecified, so assert only monotonic and complete; the author confirms the curve (smoothstep suggested).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `FovCapTest.cpp` and `FovEaseTest.cpp` under `Source/StealthGameplay/Private/Tests/Camera/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md`, `story-004-camera-manager-shell-remote-pov.md`, `story-008-toggle-blend-reduce-motion-remembered-mode.md` (Reduce camera motion setting)
- Unlocks: `production/epics/camera-system/story-021-camera-mode-not-observable.md` (FOV changes in the 3P run)
