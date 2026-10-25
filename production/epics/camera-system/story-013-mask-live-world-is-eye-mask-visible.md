# Story 013: Mask on the live world and IsEyeMaskVisible

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: Targets are every `AStealthPawn` except the local player's own possessed pawn, plus eye-masked elements, registered through a client-local `UStealthPawnRegistry` world subsystem (shared with ADR-0013's tiering) and `IEyeMaskTargetRegistry`. For every in-cone probe one `AsyncLineTraceByChannel(EAsyncTraceType::Test, P_m → probe, ViewBlock)` is submitted; the result arrives the next frame. In first person and below 25 cm free boom the evaluator runs flag-only so `IsEyeMaskVisible` is one geometric definition of "rendered" in both modes.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `UWorld::AsyncLineTraceByChannel` is verified in 5.8.4 (`Engine/World.h:2522`); result callbacks run on the next frame. ADR-0015 verification item (3) (worst-case 295 traces) and the sync-near-band fallback (§4.3.6) come from story 011; build the backend behind `IEyeMaskTraceBackend` (story 012) so the fallback is a second implementation, not a rewrite. `LineTraceTestByChannel` is the sync variant. Server side: no mask and no view check (CS-R5c).

**Control Manifest Rules (this layer)**:
- Required: Every sight test uses the `ViewBlock` channel; pawns register through one client-local `UStealthPawnRegistry` (no kind, team or controller data); distance changes only the test rate — source: ADR-0015, ADR-0013
- Forbidden: Never ship the development-only `WasRecentlyRendered` rendered-flag fallback once `IsEyeMaskVisible` exists; never use engine `VisibilityBasedAnimTickOption` for tiering — source: ADR-0013, ADR-0015
- Guardrail: Mask game-thread ≤ 0.5 ms mean at 59 near pawns (measured in story 022); no work on a non-local controller

This story connects the headless core (012) to the world: the registry, the real async traces, and the published `IsEyeMaskVisible` that IIF (Strike hint, `PawnRef`) and SCL tiering read. It also owns the mode switch (flag-only in first person and below 25 cm free boom, applying again above 35 cm).

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-05, AC-CS-36, AC-CS-41 mask clause) and ADR-0015 §4.5 (derived), scoped to this story:*

- [ ] **AC1** (AC-CS-05, [P] `LOSMultiPosition`, the core test): **GIVEN** the functional test map (L-corner, T-junction, pillar, door frame, 100 cm wall, a wall whose top lies between eye height and eye + 25 cm, 120 cm alley, dead end, 5 cm fence, window slit, bush), 24 dummies (12 NPC-controlled, 12 bound to fake participants), and ≥ 40 player positions swept in 15° yaw steps at pitch −60, −20, 0, +20, +40 in 3P on both shoulders, each sampled after 30 settled frames **THEN** at every sample the 2 cm guard ray is unblocked, the 14 cm sphere does not overlap geometry, mask-visible (α > 0) implies the 25-point `ViewBlock` oracle from `P_m` sees at least one point (zero false reveals), and false hides (α = 0 while the oracle sees ≥ 5 of 25 points) are ≤ 5 % of samples.
- [ ] **AC2** (AC-CS-36, [P] `BeyondRangeLOS`): **GIVEN** dummies at 70 m and 90 m, one visible from the eye and one behind a wall **THEN** the visible one fades in within 0.32 s of becoming visible and the occluded one is never drawn over 60 s of yaw sweeps.
- [ ] **AC3** (AC-CS-41 mask clause, derived ADR-0015 §4.5): **GIVEN** the free boom length swept 120 → 50 → 15 → 50 → 90 cm **THEN** `IsEyeMaskVisible` equals α > 0 in both modes, and the mask output is not applied (flag-only) at 15 cm and applied again at 90 cm, with the 25 cm off / 35 cm on hysteresis.

---

## Implementation Notes

*Derived from ADR-0015 Decision §4.2-§4.5, §4.7 and Implementation Guidelines 4-5, 11:*

`UStealthPawnRegistry` is a client-local world subsystem: pawns add themselves at `BeginPlay` and remove at `EndPlay`; iteration is by insertion index. ADR-0013's tiering reads the same registry, so coordinate the type with `shared-character-locomotion/story-037-tier-formula.md` (whoever lands first creates it). The trace backend submits all probes of a target together and polls the next frame; results feed `FEyeMaskCore`. The oracle in AC1 is the test's own: 25 points per dummy, a superset of the 5 mask probes, traced on `ViewBlock` from `P_m` and restricted to the eye cone. The functional test map is built in the test harness (a map asset: create a new one, never edit a sample asset; if a map file cannot be produced without the editor, generate the layout from C++ with ISM/box actors at test setup, as the generator tests do).

Flag-only mode: in first person and below 25 cm free boom the core still computes `IsEyeMaskVisible` with the same cone and probes but writes no visibility or dither (story 015 reads the mode). The cost is the same in both modes so frame time does not hint at the mode.

Prototype replaced: the prototype's `WasRecentlyRendered`-style or distance checks for "visible" pawns, if any (`StealthHUD.cpp`, `StealthNpcController.cpp`); search before promoting. Nothing in the prototype implements line-of-sight masking.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-eye-mask-core.md`: the decision logic.
- `story-015-mask-apply-path.md`: hide and dither on the pawn.
- `story-014-concealed-and-informant-view.md`: AC-CS-11 (concealed oracle set) and AC-CS-21.
- `story-022-mask-performance-acceptance.md`: AC-CS-13 cost.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthPawnRegistry.h` and `Private/Core/StealthPawnRegistry.cpp`
- `Source/StealthGameplay/Public/View/Core/EyeMaskTargetRegistry.h` (`IEyeMaskTargetRegistry`)
- `Source/StealthGameplay/Private/Core/Camera/StealthEyeMaskTraceBackend.cpp`
- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp` (mask tick, mode switch, `IsEyeMaskVisible`)
- `Source/StealthGameplay/Private/Tests/Camera/LOSMultiPositionTest.cpp` (functional test, name `StealthGame.Camera.LOSMultiPosition`)
- `Source/StealthGameplay/Private/Tests/Camera/BeyondRangeLOSTest.cpp` (name `StealthGame.Camera.BeyondRangeLOS`)
- `Source/StealthGameplay/Private/Tests/Camera/MaskModeSwitchTest.cpp` (class `FCameraMaskModeSwitchTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `LOSMultiPosition` (AC-CS-05)
  Given: the AC-CS-05 functional map (a prerequisite deliverable of this story) with 24 dummies (12 NPC, 12 fake participants) and ≥ 40 positions, yaw swept in 15° steps at pitch −60, −20, 0, +20, +40, on both shoulders.
  When: each sample is taken after rig and mask settle, with the 25-point oracle traced from `P_m` on `ViewBlock` inside the eye cone.
  Then: at every sample the 2 cm guard ray is clear, the 14 cm sphere does not overlap geometry, and there are zero false reveals; false hides (α = 0 while the oracle sees ≥ 5 of 25) are ≤ 5 % of samples. Define the denominator as (sample, dummy) pairs where the oracle sees ≥ 5 of 25 (author confirms).
  Edge cases: the wall top between eye and eye + 25 cm; the 5 cm fence; the window slit; the bush must not block. Large run: schedule in the nightly tier.

Test: AC2 `BeyondRangeLOS` (AC-CS-36)
  Given: dummies at 70 m and 90 m, one visible and one behind a wall (the map needs a ≥ 90 m clear sightline).
  When: 60 s of yaw sweeps run.
  Then: the visible one has α > 0 within 0.32 s of its first clear eye ray; the occluded one has α = 0 on every frame.
  Edge cases: a dummy just past 6000 cm; a dummy emerging from behind a wall.

Test: AC3 `IsEyeMaskVisibleAndModes` (AC-CS-41 mask clause, ADR-0015 §4.5)
  Given: free boom swept 120 → 50 → 15 → 50 → 90 cm, plus a 1P run.
  When: each step is held.
  Then: `IsEyeMaskVisible` equals (α > 0) every frame in both modes; in 1P, or below 25 cm free boom, the evaluator is flag-only (SetVisibility and CPD spy counts are 0 while the flag stays correct); nothing is applied at 15 and it is applied at 90.
  Edge cases: from 15 up, still flag-only at 30 and applied above 35; from 50 down, still applied at 30 until below 25.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three tests above under `Source/StealthGameplay/Private/Tests/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-006-eye-pivot-boom-collision.md`, `story-007-viewblock-channel-collision-profiles.md`, `story-009-first-person-own-head-hide.md`, `story-012-eye-mask-core.md`; cross-epic: shared-character-locomotion (`AStealthPawn`, capsule), level-socket-contract (graybox geometry on `ViewBlock`)
- Unlocks: `production/epics/camera-system/story-014-concealed-and-informant-view.md`, `story-015-mask-apply-path.md`, `story-016-aim-ray.md`, `story-022-mask-performance-acceptance.md`
