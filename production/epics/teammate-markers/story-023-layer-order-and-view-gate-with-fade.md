# Story 023: Layer order and view gate with the fade

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0021 verification item UI-V3 (layer order, AC-HUD-24: the screen fade covers markers and stays under plates, cards and menus), owned by the hud epic, has not reported; the fade layer itself is a hud-epic deliverable that is not yet built.
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

Markers go out with the camera's black: the marker layer (L0, ZOrder 10) sits below the HUD fade layer (ZOrder 20, drawn from Camera's published alpha), which sits below the plates (ZOrder 30 and up) and menus (ZOrder 100). This story verifies on real clients that markers are drawn exactly while the view is `Live`, are covered by the fade while a plate is not, return together when the view goes `Live` again, and are never drawn in the lobby, the `Preparing` briefing or any menu. An informant viewer keeps a concealed ally's hatched marker across the same sequence. It is the PIE half of AC-TMK-16a (the model half is story 015) and the marker half of the UI-V3 check that the hud epic owns.

**GDD**: `design/gdd/teammate-markers.md` (TM-R8, UI Requirements layout, AC-TMK-16a)
**Requirement**: `TR-tmk-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (sections 1 and 3)
**Also governed by**: ADR-0015 (Camera publishes `GetScreenFade()`; `StartCameraFade` not used)
**ADR Decision Summary**: The HUD fade layer (ZOrder 20) renders Camera's published fade so it covers L0 (markers, ring, prompt) while leaving L1-L4 readable; a camera fade applied in the scene can never cover UMG (E2).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E1 (Player slot 100 below Viewport 200), E2 (scene `OverlayColor` is inside the scene render) and E11 (`FadeAmount` animates only while the scene fade is on) are [V]; UI-V3 is the [U] check.

**Control Manifest Rules (this layer)**:
- Required: The screen fade is drawn by the HUD fade layer at ZOrder 20 from Camera's published fade; L0 is at ZOrder 10 and plates at 30 — source: ADR-0021
- Forbidden: Never use `AddToViewport` for game UI; never use `StartCameraFade` for neutral, cut or elimination fades — source: ADR-0021, ADR-0015
- Guardrail: Markers are never focusable or clickable; layers below 100 are `HitTestInvisible`

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-16a, UI Requirements) and ADR-0021 UI-V3, scoped to this story:*

- [ ] **AC1** (AC-TMK-16a, PIE half, [PIE-2] `ViewGateFollowsCamera`): **GIVEN** the scripted view sequence `Neutral` (`Preparing`), `Live`, `Live` (own dying body), `Neutral` (awaiting a pawn), `Live` (informant pawn) **WHEN** markers are sampled each frame **THEN** markers are drawn iff the state is `Live`, go out under the fade to black (cross-checks AC-CS-37), all fade in together on returning to `Live`, and an informant viewer shows a concealed ally's hatched marker.
- [ ] **AC2** (UI-V3, AC-HUD-24 marker part): **GIVEN** the fade at ZOrder 20, L0 at 10 and plates at 30 **THEN** a retained screenshot taken mid-fade shows a marker covered by the black and a HUD plate not.
- [ ] **AC3** (UI Requirements): **GIVEN** the lobby, the `Preparing` briefing and any menu **THEN** no marker is drawn in any of them, even with allies present.

---

## Implementation Notes

*Derived from TM-R8 and ADR-0021 §1, §3:*

Drive `GetViewState()` through the real camera manager's states (camera-system story 017 produces `Neutral` and the published fade; story 018 produces the dying hold) in a 2-client PIE. Sample each frame: the marker fill and alpha from the model dump, plus a frame capture for the layer-order screenshot. The fade alpha comes from `IStealthCameraView::GetScreenFade()` drawn by the hud epic's fade layer; this story must not build that layer. If the fade layer is missing, the test cannot run (that is the Blocked reason).

Evidence names: `tmk-fade-cover-*.png` for the covered-marker and uncovered-plate screenshot; `tmk-gate-*.png` for the lobby, briefing and menu states; record a sign-off in `production/qa/evidence/tmk-gate-evidence.md`. Repeat the sequence once for an informant viewer with a hatched concealed ally.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-015-view-gate-model.md`: the gate logic.
- `camera-system/story-017-view-state-neutral-view-fade.md` and `story-018-dying-hold-and-view-release.md`: producing the view states and the published fade.
- the hud epic: the fade layer, the plates, UI-V3 itself.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/ViewGateFollowsCameraTest.cpp` (CQTest, `StealthGame.Markers.PIE.ViewGateFollowsCamera.*`)
- `production/qa/evidence/tmk-fade-cover-*.png`, `tmk-gate-*.png`, `tmk-gate-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: the scripted sequence `Neutral`, `Live`, `Live` (dying), `Neutral`, `Live` in PIE. When: sampled each frame. Then: markers are drawn iff `Live`, go out under the fade to black (cross-check AC-CS-37), fade in together on `Live`, and an informant viewer shows a concealed ally's hatched marker. Edge cases: `Neutral` for a single frame.
- **AC2**: Setup: fade at ZOrder 20, L0 at 10, plates at 30. Verify: a screenshot during the fade. Pass condition: the marker is covered by the black and a HUD plate is not.
- **AC3**: Given: the lobby, the `Preparing` briefing and a menu in PIE. When: each is shown with allies present. Then: no marker is drawn in any of them. Edge cases: opening a menu mid-round.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `ViewGateFollowsCameraTest.cpp` OR a documented playtest, with the retained screenshots listed above (the layer-order screenshot is a UI-V3 requirement)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-015-view-gate-model.md`, `story-019-paint-markers-in-world-anchor-leaf.md`; cross-epic `production/epics/camera-system/story-017-view-state-neutral-view-fade.md`, `story-018-dying-hold-and-view-release.md`, and the hud epic's fade-layer story (not yet created)
- Unlocks: the hud epic's AC-HUD-24 verification
