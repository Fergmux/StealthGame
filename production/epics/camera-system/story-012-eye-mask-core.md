# Story 012: Eye mask core

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-004`, `TR-camera-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: `FEyeMaskCore` decides visibility per target from geometry only: a fixed eye cone (±55° × ±38°, independent of FOV) filters probes with no trace, in-cone probes are traced together on `ViewBlock`, `Visible = Pass ∨ (previous ∧ fails < 2)`, α moves over 0.10 s, the test always runs at every range with rate bands 60/15/5 Hz, and a per-frame cap defers excess targets round-robin by registry index.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; the core itself is plain C++ with an injected trace and clock backend)
**Engine Notes**: The core never calls the engine: results arrive through an injected trace-result interface so AC-CS-07/08 run headless with the async "+1 frame" timing simulated. Real `AsyncLineTraceByChannel` is story 013. ADR-0015 verification item (3) (cost) is measured in story 011 and may change the constants `EyeMaskTracesPerFrameMax` (initial 160) and the sync-near-band fallback; keep both configurable in the core.

**Control Manifest Rules (this layer)**:
- Required: The mask reads only geometry (positions, capsules, `ViewBlock` traces); no pawn is drawn in third person without a passing eye-origin trace; distance changes only the test rate — source: ADR-0015
- Forbidden: Never read team, binding, participant state, controller kind or a server-only header in mask or rig code; never defer by kind, team or distance-to-crosshair — source: ADR-0015 (Guideline 4, AC-CS-07)
- Guardrail: Round-robin by registry index is the only deferral order; worst case 295 traces drains in at most two frames

This story is the decision logic of Pillar 2's guard: the third-person camera sees nothing the body's line of sight does not. It is built and tested headless and kind-blind (an NPC-controlled dummy and a participant-bound dummy behind the same wall produce identical α curves).

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-08, AC-CS-07) and ADR-0015 §4.3 (derived items marked), scoped to this story:*

- [ ] **AC1** (AC-CS-08, [U] `MaskTiming`): **GIVEN** a near dummy with async trace results landing one frame after the test **WHEN** its probe first passes **THEN** the fade-in starts on the frame the result lands and completes 0.10 s later; **WHEN** it fails **THEN** the fade-out starts after the 2nd consecutive failed result (≤ 50 ms).
- [ ] **AC2** (AC-CS-07, [U] `MaskParityAndInputs`): **GIVEN** an NPC-controlled dummy and a participant-bound dummy mirrored behind the same wall **WHEN** the player walks past the corner **THEN** their α curves are identical frame for frame; **AND** a static scan finds no read of team, binding, Roster state or controller class in the mask or rig code.
- [ ] **AC3** (derived, ADR-0015 §4.3.1-4): **GIVEN** targets in and out of the cone **THEN** probes outside the ±55° × ±38° eye cone cost no trace and do not count as visible; every in-cone probe gets one trace and all probes of a target are submitted together; the test runs at every range at 60 Hz within `EyeMaskNearCm`, 15 Hz to `EyeMaskRangeCm`, and 5 Hz beyond.
- [ ] **AC4** (derived, ADR-0015 §4.3.5; TR-camera-008): **GIVEN** more due targets than `EyeMaskTracesPerFrameMax` traces **THEN** at most that many traces are submitted per frame and the remainder is deferred one frame in round-robin order of registry index, never by kind, team or distance.

---

## Implementation Notes

*Derived from ADR-0015 Decision §4.2-§4.3:*

Mask origin `P_m` = eye pivot with z = `min(lagged pivot z, real eye-socket z)` (from story 006). Probes for pawns: head, chest, pelvis and two lateral points derived from the capsule, never bones. The cone test transforms probes to view space at `P_m`; the cone is fixed (`EyeConeHalfHDeg` 55, `EyeConeHalfVDeg` 38), never the FOV (I-CS-3 keeps it ≥ half the FOV maximum). State per target (~64 bytes): visible flag, consecutive fail count, α, last test time, pending trace handles. `EyeMaskHideFailCount` 2, `EyeMaskFadeSec` 0.10, rate keys `EyeMaskNearHz/FarHz/BeyondHz` 60/15/5 with `EyeMaskNearCm` 2000 / `EyeMaskRangeCm` 6000. Iteration order is the registry's insertion index (client-local, kind-blind). A target with no probe in the cone is not visible and costs no trace. Eye-masked elements (attachments, bush rustle emitters, objective-site effects) register through `IEyeMaskTargetRegistry::Register(Root, LocalProbes)` with their own probes; the core treats them like pawns.

Interfaces: `IEyeMaskTraceBackend { SubmitTrace(From, To) -> Handle; PollResult(Handle) }` and an injected clock (ADR-0007 §5, ADR-0008 T1; `tuning-data-config/story-007-stealth-clock-interface.md`). Static-scan part of AC2 is a T0 lint (tuning-data-config `story-011-t0-lint-framework-and-gate.md`): grep the camera module for `GetPlayerState`, `GetController`, `IsPlayerControlled`, team and Roster headers.

Prototype replaced: none (no mask exists in the prototype).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-013-mask-live-world-is-eye-mask-visible.md`: the real async trace backend, pawn registry, the LOS oracle test.
- `story-015-mask-apply-path.md`: writing α into pawns.
- `story-011-mask-spike-oq5-oq10.md`: measuring cost and setting `EyeMaskTracesPerFrameMax`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthEyeMaskCore.h`
- `Source/StealthGameplay/Private/Core/Camera/StealthEyeMaskCore.cpp`
- `Source/StealthGameplay/Private/Tests/Camera/MaskTimingTest.cpp` (class `FCameraMaskTimingTest`, name `StealthGame.Camera.MaskTiming`)
- `Source/StealthGameplay/Private/Tests/Camera/MaskParityAndInputsTest.cpp` (class `FCameraMaskParityAndInputsTest`, name `StealthGame.Camera.MaskParityAndInputs`)
- `Source/StealthGameplay/Private/Tests/Camera/MaskConeAndBandsTest.cpp` (class `FCameraMaskConeAndBandsTest`)
- `Source/StealthGameplay/Private/Tests/Camera/MaskTraceCapTest.cpp` (class `FCameraMaskTraceCapTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `MaskTiming` (AC-CS-08)
  Given: a fake async backend that lands results one frame later, a fake clock, and a near dummy with a clear probe.
  When: the probe first passes; later it fails.
  Then: fade-in starts on the frame the result lands and α reaches 1.0 at +0.10 s (6 frames ±1); from visible, fade-out starts after the 2nd consecutive failed result (≤ 50 ms near).
  Edge cases: a single fail followed by a pass causes no fade-out (the fail counter resets).

Test: AC2 `MaskParityAndInputs` (AC-CS-07)
  Given: an NPC-bound and a participant-bound dummy mirrored behind a wall.
  When: the player walks past the corner.
  Then: α curves are exactly equal on every frame; a static scan of mask and rig sources finds none of: team, binding, Roster, controller class, `GetController`, `GetPlayerState`, `IsPlayerControlled` (token list is a proposal for the author to confirm).
  Edge cases: a different registry order still gives equal α. The same scan is story 024's; keep one owner.

Test: AC3 `MaskConeAndRateBands`
  Given: targets in and out of the cone and at 1999, 2000, 2001, 6000, 6001 and 9000 cm.
  When: the core runs.
  Then: probes outside ±55° H × ±38° V, or behind the eye, cost 0 traces and are not visible (test 54.9 and 55.1); each in-cone probe gets exactly 1 trace and all probes of a target are submitted on the same frame; 1999 and 2000 cm test every frame (60 Hz), 2001 and 6000 cm every 4th frame (15 Hz), 6001 and 9000 cm every 12th frame (5 Hz); an in-cone blocked target casts traces at every range (no cutoff).
  Edge cases: none beyond the boundaries listed.

Test: AC4 `MaskTraceCap`
  Given: 59 in-cone blocked targets × 5 probes and a cap of 160.
  When: frames run until the backlog drains.
  Then: ≤ 160 traces per frame and no target is split; deferred targets are tested next frame in round-robin order of registry index, so 295 drains in two frames.
  Edge cases: far targets at low indices are not deprioritised; kind and team metadata does not change the order; a target removed mid-rotation does not skip another; with the cap above demand nothing is deferred.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four test files above under `Source/StealthGameplay/Private/Tests/Camera/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-001-camera-tuning-keys-and-invariants.md`, `story-006-eye-pivot-boom-collision.md` (mask origin); cross-epic: tuning-data-config `story-007-stealth-clock-interface.md`, `story-011-t0-lint-framework-and-gate.md`
- Unlocks: `production/epics/camera-system/story-013-mask-live-world-is-eye-mask-visible.md`
