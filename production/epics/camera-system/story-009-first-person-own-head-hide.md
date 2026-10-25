# Story 009: First-person own-head hide

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-???` (no registry entry: CS-R2/CS-R6/CS-R7 own-body presentation is not among the eight TR-IDs)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0013 for the shared AnimBP final-pose hook)
**ADR Decision Summary**: In 1P and close 3P the own head, neck and hair bones are scaled to 0 on the local pose only (a local-only AnimInstance flag read by the shared AnimBP's final pose, ADR-0013); the mesh stays rendered and ticking, and observers and the server never see it. `EFirstPersonPrimitiveType` is not used.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; AnimBP final-pose bone scaling is pre-cutoff but touches the shared AnimBP)
**Engine Notes**: The hide flag is a local-only AnimInstance variable read in the shared AnimBP's final pose (a Control Rig or Modify Bone node scaling head/neck/hair to 0). **The AnimBP is a sample-derived Blueprint: do not save it from the editor; make the change through a C++ `UAnimInstance` subclass variable, a C++ anim node, or a new asset, and never touch `Content/Blueprints/Cameras/*`** (production/never-resave-assets.md). The prototype's `HideBoneByName(head)` approach scales a bone on the component and is not the promoted path. No ADR-0015 verification item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Own-head hide is local pose only, mesh stays rendered and ticking — source: ADR-0015
- Forbidden: Never select an animation asset, layer or graph by controller, team or participant state; never use `EFirstPersonPrimitiveType` — source: ADR-0013, ADR-0015
- Guardrail: No per-frame allocation; one bool read in the final pose

The player sees a body without a head in 1P while observers see the full model with its head pitched no lower than −21.5°. The close-third-person hide (boom under 60 cm, 15 cm hysteresis, shows again only above 75 cm) shares the same mechanism and is asserted here for its head clause; the matching mask-off clause of AC-CS-41 is in story 013.

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-17, AC-CS-41 head clause), scoped to this story:*

- [ ] **AC1** (AC-CS-17, [P][E] `FirstPersonHeadHidden`): **GIVEN** 1P looking down at −20° and at −60° **WHEN** captured locally and on an observer **THEN** the owner sees the body without a head (feet visible at −60°), and the observer sees the full model with the head pitched no lower than −21.5°; all screenshots are retained.
- [ ] **AC2** (AC-CS-41, [U] `CloseThirdPerson`, head clause): **GIVEN** the free boom length swept 120 → 50 → 15 → 50 → 90 cm, one step per 10 frames **THEN** the own head, neck and hair hide at the first step below 60 cm and stay hidden through the 15 cm hysteresis (they show again only above 75 cm); **AND** no frame renders from inside geometry (the 2 cm guard ray is clear on every frame).

---

## Implementation Notes

*Derived from ADR-0015 Decision §7.4 and GDD CS-R6, CS-R7:*

Flag is set only on the local client for the possessed pawn (`IsLocallyControlled` on the owner side is allowed here: the camera reads its own possessed pawn only, AC-CS-39). The bone set is head, neck and hair; the character pool's merged single-slot meshes (ADR-0012) must expose those bones; check on the pool meshes, not only the mannequin. In 1P the lens sits at pivot + `FpForwardOffsetCm` (8 cm) with near clip 5 cm, and the own head hides from u ≥ 0.85 of the toggle blend (story 008). The prototype shows a sliver of the player's own shoulder at the screen edge in first person (first-playable-report known issue 10): AC1's screenshot at −20° must show that it is gone; if not, tune `FpForwardOffsetCm` within its safe range 5-12 cm.

Evidence: retained screenshots in `production/qa/evidence/` named `<date>_camera_fp-head-hidden-<angle>-<owner|observer>.png`, plus sign-off in `production/qa/evidence/camera-first-person-head-hidden-evidence.md`. A parse check is not a run: the screenshots come from a launched PIE session (`.claude/docs/run-and-observe.md`).

Prototype replaced: `StealthCharacter.cpp` `ToggleCameraMode` first-person branch (`HideBoneByName(FirstPersonSocket)`, `FirstPersonCamera`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-008-toggle-blend-reduce-motion-remembered-mode.md`: the toggle that requests the hide at u ≥ 0.85.
- `story-013-mask-live-world-is-eye-mask-visible.md`: the mask-off clause of AC-CS-41.
- character-pool-appearance epic: bone names present on every pool mesh.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthLocalPoseFlags.h` (local-only AnimInstance flag)
- `Source/StealthGameplay/Private/Core/Camera/StealthOwnHeadHide.cpp` (close-3P hysteresis, 1P request)
- `Source/StealthGameplay/Private/Core/Camera/StealthCameraCore.cpp` (close-3P thresholds 60 / 75 cm)
- `Source/StealthGameplay/Private/Tests/Camera/CloseThirdPersonHeadTest.cpp` (class `FCameraCloseThirdPersonHeadTest`, name `StealthGame.Camera.CloseThirdPersonHead`)
- `production/qa/evidence/camera-first-person-head-hidden-evidence.md` and the screenshots

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- AC2: the numeric parts of AC-CS-41 (hide below 60 cm, 15 cm hysteresis, show above 75 cm, guard ray clear every frame) cannot be pass/failed from a screenshot, and the GDD tags the criterion [U]. The story's Files list and Test Evidence already carry a Logic test (`CloseThirdPersonHeadTest`) for those parts; the screenshot part is the ten-capture sweep below. The author confirms the split or reclassifies AC2.
- Manual check AC1: 1P with a local and an observer client, looking down at −20° and −60° with a debug overlay of replicated head pitch. Pass: four screenshots retained in `production/qa/evidence/`, observer overlay reads no lower than −21.5°, the local capture shows a body with no head and feet visible at −60°, lead sign-off.
- Manual check AC2 (screenshots): sweep free boom 120 → 50 → 15 → 50 → 90 cm, 10 frames per step, local and observer captures at each step. Pass: local head, neck and hair absent at 50 (down), 15 and 50 (up) and present at 90, observer sees them at every step, ten screenshots retained, lead sign-off. Logic test: hide flag false at 120, true at 50 and stays true at 15 and 50, false at 90; mask apply off at 15 and on at 90 (that clause is story 013); guard ray clear every frame.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained screenshots in `production/qa/evidence/` (owner and observer, −20° and −60°) + sign-off in `production/qa/evidence/camera-first-person-head-hidden-evidence.md`; plus the AC2 unit test

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-006-eye-pivot-boom-collision.md`, `story-008-toggle-blend-reduce-motion-remembered-mode.md`; cross-epic: shared-character-locomotion (`story-005-animbp-islocallycontrolled-removal.md`, the AnimBP parity rules), character-pool-appearance (bones on pool meshes)
- Unlocks: `production/epics/camera-system/story-013-mask-live-world-is-eye-mask-visible.md` (close-3P clause), `story-023-playtests-scout-read-own-pawn.md`
