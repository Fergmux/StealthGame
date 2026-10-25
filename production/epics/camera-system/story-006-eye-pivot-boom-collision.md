# Story 006: Eye pivot, boom and collision

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: The eye pivot sits on the capsule axis at the eye-socket height (read from the local pawn mesh only), low-passed by `CameraPivotLagSec` (0.12 s in traversal) with vertical speed capped at `PivotMaxVertCmPerSec`; the third-person boom places the ideal point at pivot + R(view yaw)·(−L, s·O_eff, H), and per frame runs one 2 cm guard line trace and one 14 cm sphere sweep on the engine `ECC_Camera` channel, with immediate pull-in and recovery per F2 (hold, exponential, speed cap), shoulder squeeze, and close-3P below 60 cm.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; the boom and pivot math is pre-cutoff, traces are stock physics)
**Engine Notes**: `LineTraceSingleByChannel` and `SweepSingleByChannel` on `ECC_Camera` are pre-cutoff. The math lives in `FStealthCameraCore` with an injected trace backend so AC-CS-06/29/30 run headless. No ADR-0015 verification item gates this story.

**Control Manifest Rules (this layer)**:
- Required: The camera is computed only in `AStealthPlayerCameraManager`; camera math lives in plain C++ cores with injected backends — source: ADR-0015, ADR-0007 §5
- Forbidden: Never read the head bone's rotation or horizontal position for the pivot; no camera components on the pawn — source: ADR-0015
- Guardrail: Boom adds 2 traces per frame (inside the camera's 1.0 ms line)

This story builds the third-person rig geometry: the stable pivot (so animation jitter and climbs do not shake the lens), the boom with collision and recovery, and the squeeze and swap of the shoulder offset. It guarantees no frame renders from inside geometry (2 cm guard ray) and sets up the free-length value that the mask mode switch (story 013) and own-head hide (story 009) read.

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-06, AC-CS-29, AC-CS-30), scoped to this story:*

- [ ] **AC1** (AC-CS-06, [U] `BoomRecovery`): **GIVEN** F2 with the default keys **WHEN** the free length drops from 186 to 52 cm and the obstacle is then removed **THEN** the lens is at 52 cm in the same frame, holds for 0.15 s, reaches ≥ 179.3 cm (95 % of the gap) by 1.15 s, and never exceeds 300 cm/s.
- [ ] **AC2** (AC-CS-29, [U] `PivotCapAndCorrection`): **GIVEN** a 150 cm drop and a 120 cm climb **THEN** the pivot's vertical speed never exceeds 300 cm/s and the mask origin is never above the real eye socket; **AND** for an injected owner correction below `PawnSmoothSnapThresholdCm` the pivot moves smoothly (no step > 5 cm per frame), and above the threshold it snaps with the body on the same frame.
- [ ] **AC3** (AC-CS-30, [U] `SqueezeAndSwap`): **GIVEN** a wall 30 cm from the right shoulder **THEN** the offset reaches 0 within 0.15 s and returns within 0.40 s after clearing, with no automatic flip; **AND** a shoulder swap mirrors the offset over 0.25 s, a second press during the blend is buffered and runs once, and the 2 cm guard ray stays clear on every frame of the blend.

---

## Implementation Notes

*Derived from ADR-0015 Decision §3 and GDD F2:*

Eye pivot (CS-R4): x/y on the capsule axis; height = eye-socket height read from the pawn's mesh socket on the local client only, filtered by `CameraPivotLagSec` (0.12 s while `IsInTraversal`), vertical speed ≤ `PivotMaxVertCmPerSec`, clamped inside the capsule; a network correction above SCL's `PawnSmoothSnapThresholdCm` resets the filter. Expose the mask origin height `min(lagged pivot z, real eye-socket z)` from the core, since story 012 consumes it (D6, 2026-10-25).

Boom (F2): ideal point = pivot + R(view yaw)·(−L, s·O_eff, H), pitch rotates the lens in place (no orbit). One thin line trace (`CameraGuardRadiusCm` 2) treated as a ray, and one sphere sweep (`CameraProbeRadiusCm` 14) on `ECC_Camera` from pivot to ideal point; pull-in immediate, recovery by `BoomRecoverHoldSec` 0.15, `BoomRecoverTauSec` 0.30, `BoomRecoverCmPerSec` 300; shoulder squeeze in `ShoulderSqueezeInSec` 0.15, out `ShoulderSqueezeOutSec` 0.40; close 3P below `BoomCloseCm` 60; the mask-off band (below 25 cm, back on above 35 cm, `CameraMaskOffHysteresisCm` 10) is a published output of the core for story 013. First-person lens is pivot + `FpForwardOffsetCm` and is story 008/009. Near clip `CameraNearClipCm` 5 in both modes. Reduce camera motion turns recovery and swap into cuts (story 008). The `Camera` channel responses are story 007.

The shoulder swap input binding is IIF's (R14); this story consumes the swap request and runs the 0.25 s blend with one buffered press. Concealed boom (`ConcealedBoomCm`) is story 014; keep the `bConcealed` input to the core.

Prototype replaced: the sample's spring-arm style camera (`CBP_SandboxCharacter` camera setup, Gameplay Cameras rig) is not promoted (ADR-0015 Migration Plan 1); the sample assets stay unmodified and on the never-resave list.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-viewblock-channel-collision-profiles.md`: `ECC_Camera` and `ViewBlock` responses on profiles.
- `story-008-toggle-blend-reduce-motion-remembered-mode.md`: 1P/3P blend, Reduce motion cuts.
- `story-014-concealed-and-informant-view.md`: concealed boom and dome containment.
- `story-013-mask-live-world-is-eye-mask-visible.md`: mask behaviour below 25 cm.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthCameraCore.h`
- `Source/StealthGameplay/Private/Core/Camera/StealthCameraCore.cpp` (pivot filter, boom, squeeze, swap)
- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp` (adapter: trace backend, socket read)
- `Source/StealthGameplay/Private/Tests/Camera/BoomRecoveryTest.cpp` (class `FCameraBoomRecoveryTest`, name `StealthGame.Camera.BoomRecovery`)
- `Source/StealthGameplay/Private/Tests/Camera/PivotCapAndCorrectionTest.cpp` (class `FCameraPivotCapAndCorrectionTest`)
- `Source/StealthGameplay/Private/Tests/Camera/SqueezeAndSwapTest.cpp` (class `FCameraSqueezeAndSwapTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC3: "returns within 0.40 s after clearing" has no tolerance. QA-lead proposed rewrite (unconfirmed): offset at least 44.5 cm (99 % of 45) at 0.40 s.
- Scenario sketches: AC1 d_max = 186.0 ± 0.1, lens at 52 on the same frame, holds 0.15 s, ≥ 179.3 cm by 1.15 s, never above 300 cm/s (5 cm per frame plus epsilon); an obstacle that returns during recovery pulls in immediately and restarts the hold. AC2 pivot vertical speed ≤ 300 cm/s on every frame, mask origin z ≤ real eye z, correction of `PawnSmoothSnapThresholdCm` − 1 cm moves smoothly (no step above 5 cm per frame), threshold + 1 cm snaps the same frame; ask SCL which side exactly-at-threshold falls on. AC3 offset 0 within 9 frames with no automatic flip, swap mirrors +45 to −45 over 0.25 s, presses 2 and 3 in one blend give one buffered run, guard ray clear on every frame, swap during squeeze covered.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files above — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md`, `story-004-camera-manager-shell-remote-pov.md`, `story-005-look-wiring-control-rotation.md`; cross-epic: shared-character-locomotion (`story-002-body-constants-published.md` capsule and eye socket, `IsInTraversal`, `PawnSmoothSnapThresholdCm`)
- Unlocks: `production/epics/camera-system/story-008-toggle-blend-reduce-motion-remembered-mode.md`, `story-009-first-person-own-head-hide.md`, `story-013-mask-live-world-is-eye-mask-visible.md`, `story-014-concealed-and-informant-view.md`
