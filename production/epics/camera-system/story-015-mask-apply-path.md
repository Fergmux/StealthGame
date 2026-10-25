# Story 015: Mask apply path

> **Epic**: Camera System
> **Status**: Blocked — BLOCKED: spike story 011 (ADR-0015 OQ5/OQ10 verification items 1-2 and ADR-0022 RC-3) has not reported; story 011 is Ready and releases this one
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-003`, `TR-camera-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0022 for the shared masked-dither character master and ADR-0017 for Concealment's latch)
**ADR Decision Summary**: The camera never touches a pawn's components. It writes `MaskAlpha` into the pawn's client-local presentation state, and the Shared Pawn's single apply function (ADR-0010) combines it with Concealment's client latch and the local head-hide. 0 < α < 1 dithers through custom primitive data slot `CPD_EyeMaskAlpha` via `SetCustomPrimitiveDataFloat`; α = 0 calls `SetVisibility(false, EVisibilityPropagation::Propagate)` with `bCastHiddenShadow` and `bAffectIndirectLightingWhileHidden` false so the pawn leaves the scene; `CastShadow` is never toggled.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Verified in source (ADR-0015 §4.4): `SetVisibility(bool, EVisibilityPropagation)` (`SceneComponent.h:887-907`), `SetCustomPrimitiveDataFloat` (`PrimitiveComponent.h:1185`), a hidden component leaves the scene (`SceneComponent.cpp:3574-3576`, `PrimitiveComponent.cpp:2118-2121`), no RT or Lumen footprint (`RayTracing.cpp:1368`, `RendererScene.cpp:6195`). Unverified: Nanite skinned meshes and Substrate with CPD; proxy re-add cost and first-frame pose after show. **Story 011 must report these before this story is built; its outcome selects among the ADR-0015 fallbacks (R-3 CPD-only fade, R-4 hard cut at α thresholds).** Do not edit or resave the sample material or AnimBP; the shared character master is created or extended per ADR-0022 RC-3 through new assets.

**Control Manifest Rules (this layer)**:
- Required: α = 0 hides via `SetVisibility(false, Propagate)`; pawn primitives keep `bCastHiddenShadow` and `bAffectIndirectLightingWhileHidden` false; the dither uses the single `CPD_EyeMaskAlpha` slot; the camera writes `MaskAlpha` to local presentation state and the Shared Pawn's apply function arbitrates with Concealment's latch — source: ADR-0015
- Forbidden: Never toggle `CastShadow` per frame; never create a `UMaterialInstanceDynamic` or set a material parameter per pawn (ADR-0012 lint); the camera never calls `SetVisibility` on a pawn directly — source: ADR-0015, ADR-0012
- Guardrail: Masked-master GPU delta ≤ 0.3 ms at 50 characters; hidden pawns cost nothing

This story makes the mask visible: pawns fade out of third-person view when the eye cannot see them and leave no shadow, reflection or RT trace. It also proves the host simulation is not reduced by the mask.

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-09, AC-CS-28) and ADR-0015 §4.4 / Guidelines 6-8 (derived items marked), scoped to this story:*

- [ ] **AC1** (AC-CS-09, [P][E] `MaskedCastsNothing`): **GIVEN** a masked dummy in sunlight whose shadow would fall into the 3P frame, a reflective floor in frame and TSR jitter off **WHEN** the frame is captured and captured again with the dummy deleted **THEN** the maximum per-channel difference between the two captures is ≤ 2/255; both captures are retained.
- [ ] **AC2** (AC-CS-28, [N] `HostMaskedSimParity`): **GIVEN** the listen-server host in 3P with NPC dummy A masked behind a wall and mirrored dummy B in view, on the same scripted path with a strike montage and footsteps **THEN** on the server A and B have identical root-motion displacement (within 0.1 cm) and notify sequences (footsteps, strike windows), and montage time advances every frame for both (pose-evaluation counts may differ only as SCL F7 tiering prescribes); **AND** a client observing both receives identical data; **AND** "not rendered" for IIF R10 and SCL R11 equals the mask's α = 0 flag on every frame.
- [ ] **AC3** (derived, ADR-0015 §4.4, Guideline 8): **GIVEN** the camera writes α **THEN** it goes only to the pawn's client-local presentation state, the camera code never calls `SetVisibility` on a pawn, and the Shared Pawn's apply function arbitrates with Concealment's client latch and the local head-hide.
- [ ] **AC4** (derived, ADR-0015 §4.4, Guidelines 6-7): **GIVEN** a pawn **THEN** α = 0 hides it via `SetVisibility(false, Propagate)`, 0 < α < 1 dithers through `CPD_EyeMaskAlpha`, `bCastHiddenShadow` and `bAffectIndirectLightingWhileHidden` are false on every pawn primitive, and `CastShadow` is never toggled at run time.

---

## Implementation Notes

*Derived from ADR-0015 Decision §4.4, Risks R-1 to R-4 and ADR-0022 RC-3:*

The apply function is `AStealthPawn`'s single presentation arbiter owned by ADR-0010; this story adds the `MaskAlpha` input and the CPD write; Concealment (ADR-0017) adds its latch. Order when α rises from 0: `SetVisibility(true, Propagate)` first, then dither in. Pose ticking is unaffected: visibility never feeds `VisibilityBasedAnimTickOption` decisions on the server (always-tick pose, ADR-0010), and client tiering reads `IsEyeMaskVisible`, not the engine's was-recently-rendered test (ADR-0001 §3). The CPD slot index `CPD_EyeMaskAlpha` is one project-wide constant reserved in the shared character material and every attachable prop/VFX material (ADR-0022 owns the master). Nothing is read from team, binding or controller (AC-CS-07 scan from story 012 covers this code too).

AC1 evidence: two captures at identical camera state, same seed, retained in `production/qa/evidence/` as `<date>_camera_masked-casts-nothing-{with,without}-dummy.png` plus a pixel-diff output and sign-off `production/qa/evidence/camera-masked-casts-nothing-evidence.md`; run in a launched game with Lumen software GI and VSM on, per ADR-0022. A parse check is not a run (`.claude/docs/run-and-observe.md`).

Prototype replaced: none (no mask in the prototype).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-eye-mask-core.md` and `story-013-mask-live-world-is-eye-mask-visible.md`: the α computation and live traces.
- `story-011-mask-spike-oq5-oq10.md`: the measurements that release this story.
- `story-022-mask-performance-acceptance.md`: cost acceptance.
- concealment-bushes epic: the Concealment latch implementation.
- character-pool-appearance epic: the shared character master material.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnPresentation.cpp` (apply function: `MaskAlpha`, CPD write, `SetVisibility`)
- `Source/StealthGameplay/Public/View/Core/StealthMaskConstants.h` (`CPD_EyeMaskAlpha` index)
- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp` (writes `MaskAlpha`)
- `Source/StealthTestHarness/Private/Camera/HostMaskedSimParityTest.cpp` (CQTest, `StealthGame.Camera.PIE.HostMaskedSimParity.*`)
- `Source/StealthGameplay/Private/Tests/Camera/MaskedCastsNothingTest.cpp` (functional capture test, name `StealthGame.Camera.MaskedCastsNothing`)
- `Source/StealthGameplay/Private/Tests/Camera/MaskApplyContractTest.cpp` (class `FCameraMaskApplyContractTest`)
- `production/qa/evidence/camera-masked-casts-nothing-evidence.md` and the captures

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC2: AC-CS-28 compares root-motion displacement, notify sequences and montage time. Those are data, not a screenshot, so it is Integration evidence under a Visual/Feel type. The Files list and Test Evidence already carry the CQTest PIE test for it.
- AC3 and AC4 are code and flag assertions with no screenshot component (Logic or Integration evidence, carried by `MaskApplyContractTest`). The author may split the story by type at `/story-readiness`.
- Manual check AC1: a masked dummy in sunlight whose shadow falls into frame, a reflective floor, fixed camera, TSR jitter off; take a noise-floor control first (two captures with no change), then capture with the dummy masked and with it deleted after the same number of frames; an image-diff tool reports the per-channel maximum. Pass: maximum ≤ 2/255 and the control is below it; both captures and the diff report retained, lead sign-off.
- AC3 sketch: camera code makes 0 `SetVisibility` calls on a pawn (static scan finds none in the View sources); apply truth table: latch on and α 1 hidden, latch off and α 0 hidden, latch off and α > 0 visible, latch on and α 0 hidden; α and latch changing on the same frame covered. AC4 sketch: α = 0 calls `SetVisibility(false, Propagate)` on the mesh root and attachments hide; α = 0.5 writes 0.5 to `CPD_EyeMaskAlpha` on every primitive with one slot index; 0 per-pawn material instances; `bCastHiddenShadow` and `bAffectIndirectLightingWhileHidden` false before and after a hide/show cycle; `CastShadow` unchanged; visibility is set true before the CPD write on rising α.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures (with and without the dummy) in `production/qa/evidence/` + sign-off in `production/qa/evidence/camera-masked-casts-nothing-evidence.md`; plus the PIE and unit tests above

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-011-mask-spike-oq5-oq10.md` (blocker), `story-013-mask-live-world-is-eye-mask-visible.md`; cross-epic: shared-character-locomotion (single apply function, ADR-0010), character-pool-appearance (masked-dither character master, ADR-0022 RC-3), concealment-bushes (client latch)
- Unlocks: `production/epics/camera-system/story-021-camera-mode-not-observable.md`, `story-022-mask-performance-acceptance.md`, `story-023-playtests-scout-read-own-pawn.md`
