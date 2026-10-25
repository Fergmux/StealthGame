# Story 014: Concealed and informant view

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope (with ADR-0017 for the foliage collision profile)
**ADR Decision Summary**: While the owner's own `IsConcealed` (Owner audience) is true the boom uses `L = ConcealedBoomCm` (60), `O = 0`, `H = 0` and edge push 0; bush foliage ignores `ECC_Camera` and `ViewBlock` (Concealment owns the profile), so the lens stays inside the dome and the mask from the eye inside the bush shows what a head there sees. Informants get the same two modes, envelope, mask and aim ray, with no free camera, detached view or x-ray.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; this story reuses stock traces and the core)
**Engine Notes**: Behaviour depends on the foliage collision profile from story 007 and Concealment's `IsConcealed` replication (owner audience). No ADR-0015 verification item gates the concealed boom itself; the mask half of AC-CS-11 relies on stories 012/013.

**Control Manifest Rules (this layer)**:
- Required: Concealed boom uses the 60 cm straight-back boom, offset 0 and edge push 0, with the lens inside the 1.8 m leaf dome (I-CS-4) — source: ADR-0015, ADR-0017
- Forbidden: Never give an informant a free camera, detached view or x-ray; never let the camera read team, binding or controller kind — source: ADR-0015, ADR-0001
- Guardrail: None beyond the boom's 2 traces and the mask's existing budget

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-11, AC-CS-21), scoped to this story:*

- [ ] **AC1** (AC-CS-11, [P] `ConcealedView`): **GIVEN** the owner concealed in a bush, in 3P **THEN** the free length is ≤ 60 cm with shoulder and height offsets of 0 and the lens is inside the dome at every sample of a full Look sweep; **AND** the set of mask-visible dummies equals the AC-CS-05 oracle's set for the same pivot (at most the 5 % false-hide budget).
- [ ] **AC2** (AC-CS-21, [P] `InformantSameRules`): **GIVEN** an informant **WHEN** AC-CS-03 and AC-CS-05 are re-run **THEN** the results are identical to an Active player's, and no free-camera input exists.

---

## Implementation Notes

*Derived from ADR-0015 Decision §3 and GDD CS-R11.3, CS-R11.4, I-CS-4:*

Concealed state comes from the owner's own `IsConcealed` (Concealment, Owner audience); `Emerging` counts as concealed for pings but follow Concealment's published flag for the boom (do not infer). Body yaw is locked while concealed (Concealment CON-R7); look is free within the CS-R3 clamp, edge push 0, overshoot 0 (story 002 input). While `Dying`, the camera keeps the concealed value it held before the strike (CS-R12.1 concealed input hold, story 018). The leaf-edge vignette is the art bible's and is driven by `IsConcealed` with Concealment's ease times; it is Concealment/HUD presentation, not built here. The informant case needs the informant state only to select no different rules: the test proves that nothing in the rig or mask branches on informant state (and there is no free-camera action in the input contexts).

Test map: the bush from the story 013 functional map with Concealment's foliage profile; if the Concealment bush actor is not yet built, use a stand-in dome actor with the same collision responses.

Prototype replaced: `StealthBush` concealment camera behaviour in `StealthCharacter.cpp`, if any (the prototype draws no leaf dome).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-eye-pivot-boom-collision.md`: generic boom and collision.
- `story-013-mask-live-world-is-eye-mask-visible.md`: AC-CS-05's oracle and map.
- `story-018-dying-hold-and-view-release.md`: the concealed input hold during `Dying`.
- concealment-bushes epic: dome geometry, foliage profile, vignette.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/Camera/StealthCameraCore.cpp` (concealed boom branch)
- `Source/StealthGameplay/Private/Core/Camera/StealthPlayerCameraManager.cpp` (own `IsConcealed` input)
- `Source/StealthGameplay/Private/Tests/Camera/ConcealedViewTest.cpp` (functional test, name `StealthGame.Camera.ConcealedView`)
- `Source/StealthGameplay/Private/Tests/Camera/InformantSameRulesTest.cpp` (functional test, name `StealthGame.Camera.InformantSameRules`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `ConcealedView` (AC-CS-11)
  Given: the owner concealed in a bush with a HideVolume dome, in 3P with offsets 0, over a full Look sweep.
  When: each sample is taken.
  Then: free length ≤ 60 cm on every sample and the lens is inside the dome bounds; the mask-visible dummy set equals the story 013 oracle set from the same pivot (zero false reveals, false hides ≤ 5 %).
  Edge cases: the foliage ignores `ViewBlock` and `Camera`; concealment starting or ending mid-sweep (boom 180 ↔ 60, H → 0). Needs the bush and HideVolume from the Concealment epic: mark DEFERRED if absent.

Test: AC2 `InformantSameRules` (AC-CS-21)
  Given: an informant-bound pawn and an Active player's pawn on the same inputs.
  When: AC-CS-03 and AC-CS-05 are re-run.
  Then: trajectories and results equal the Active player's; the informant's bound input actions are a subset of Active's, with no free-camera or translate action.
  Edge cases: the takeover frame.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the two functional tests above under `Source/StealthGameplay/Private/Tests/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-005-look-wiring-control-rotation.md`, `story-006-eye-pivot-boom-collision.md`, `story-013-mask-live-world-is-eye-mask-visible.md`; cross-epic: concealment-bushes (`IsConcealed`, foliage profile, dome), npc-kill-penalty-informants (informant state)
- Unlocks: `production/epics/camera-system/story-018-dying-hold-and-view-release.md`
