# Story 019: Paint markers in the L0 world-anchor leaf

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0021 conditional gates have not reported. UI-V1 (zero-lag projection) is story 018 in this epic; UI-V3 (layer order, AC-HUD-24) and UI-V4 (DPI rule and root scaler) are owned by the hud epic and have not reported. Unblock when story 018 reports PASS and the hud epic's UI-V3 and UI-V4 stories report.
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

This is the story where a marker first appears on screen. `SStealthWorldAnchorLeaf::OnPaint` takes the per-frame marker list from the model, projects each anchor from the rendered frame's view, applies F2 (anchor height and pixel gap), F3 (edge clamp with hysteresis) and F4 (de-overlap), and draws the glyph for each state at the F1 size, always on top of the 3D scene with no depth test and no change with distance, field of view, wall or camera mode. The leaf is one volatile Slate leaf in the L0 layer (ZOrder 10), shared with the interact ring, the context prompt and the Pings rings (the hud and informant-pings epics own those), and the markers draw beneath nothing but those rings. There is no per-marker widget class and no component on any pawn. The glyph is a downward pentagon with a centred two-step notch in the top edge, Turquoise `#0FC2B8` with a 3 px Obsidian `#1C1917` keyline, flat (no gradient, glow, bevel, shadow or bloom), built as integer-size brushes or a hard-step UI material with nearest filtering and no mips so stair bands stay crisp.

**GDD**: `design/gdd/teammate-markers.md` (TM-R9, F1-F4, Visual/Audio VA-1 and VA-2); `design/ux/hud.md` item (17)
**Requirement**: `TR-tmk-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (sections 1, 2 and 6)
**ADR Decision Summary**: L0 (ZOrder 10) is one `UStealthWorldAnchorLayer` hosting one volatile `SStealthWorldAnchorLeaf` that projects markers inside `OnPaint` from `ULocalPlayer::GetProjectionData` and applies F2-F4; sizes are physical pixels divided by the leaf geometry scale, so the DPI scale is applied once; `s_ui` is applied by the layer roots and the leaf.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E1 (`AddToPlayerScreen`, Player slot 100 below Viewport 200), E3 (frame order), E4 and E12 (`ForceVolatile`, `ReleaseSlateResources`) are [V]; the `UDPICustomScalingRule` result is cached per viewport size outside the editor (E5), which is why `s_ui` lives in the root scaler. UI-V1, UI-V3, UI-V4 are the [U] checks. A camera fade can never cover UMG (E2), hence the HUD fade layer.

**Control Manifest Rules (this layer)**:
- Required: World-anchored drawing in `SStealthWorldAnchorLeaf::OnPaint` from the rendered frame's camera view; the leaf is volatile and never inside an invalidation or retainer box — source: ADR-0021
- Required: DPI comes only from `UStealthDpiScalingRule` (`H/1080`); `s_ui` is applied once by the layer roots and the L0 leaf; `ApplicationScale` stays 1.0 — source: ADR-0021
- Forbidden: Never use `AddToViewport` or the `AHUD` canvas for game UI; never one widget per marker; never port the prototype `AStealthHUD` drawing code — source: ADR-0021

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-14, VA-1, VA-2) and ADR-0021 §2 and §6, scoped to this story:*

- [ ] **AC1** (AC-TMK-14, [PIE-2][1P/3P][Vis] `AlwaysOnTopConstantSize`): **GIVEN** an ally at 3 m, 20 m and 60 m, in the open and fully behind a wall **WHEN** captured in 1P and 3P (3P with the eye mask hiding the ally's body) **THEN** the marker is drawn at every distance at the F1 size (+/-0 px) above the anchor, identical in 1P and 3P, with no opacity change.
- [ ] **AC2** (ADR-0021 §2, VA-2, derived): **GIVEN** the leaf **THEN** `SStealthWorldAnchorLeaf::OnPaint` projects from `ULocalPlayer::GetProjectionData`, the leaf is `ForceVolatile` with `ComputeDesiredSize` and released in `ReleaseSlateResources`, glyphs use integer-size brushes or a hard-step UI material with nearest filtering and no mips, and no per-marker widget class exists.
- [ ] **AC3** (ADR-0021 §6, derived): **GIVEN** a resize, a DPI change and an ultrawide viewport **THEN** sizes, the margin and the safe rectangle are recomputed every frame, `s_ui` is applied exactly once, DPI comes only from `UStealthDpiScalingRule`, and sizes are physical pixels divided by the leaf geometry scale.

---

## Implementation Notes

*Derived from ADR-0021 §1, §2, §6 and VA-1, VA-2:*

The hud epic owns the layer widgets and the `UStealthWorldAnchorLayer` shell; this story supplies the marker paint path inside the shared leaf (if the hud epic's leaf shell is not yet present, build the leaf class as story 018's spike prototyped it and let the hud epic adopt it). `OnPaint` order: read the projection data; for each marker in far-to-near order (story 009) project `A = R + (0, 0, h_a)`, compute `q = p - (0, g + H/2)` (story 004), apply F3 then F4 (stories 011 and 012), choose the glyph by fill (story 003) and the edge variant when clamped, apply the entrance and exit alpha (story 009), and emit the draw element. Inputs are the per-frame `FWorldAnchorViewModel`; the leaf reads world positions only for resolved `PawnRef`s of its own C-TeamView entries (the pawn's skeletal-mesh component location) and fallback-feed positions, and never reaches any other actor.

Glyph (VA-1): downward pentagon with a centred two-step notch in the top edge, Turquoise `#0FC2B8` fill with a 3 px Obsidian `#1C1917` keyline (F1 scales it), flat; the edge variant is the same pentagon in the current fill with a stepped base, not rotated and not an arrow. Hatched: solid fill with two Obsidian stair bands of height `b` across the lower half, band ends offset by one 4 px step. Outline: 3 px Turquoise stroke, hollow, Obsidian keyline outside and inside. Glyph size includes the keyline. Per the asset spec (`/asset-spec system:teammate-markers`) there are three fill brushes, the edge variant and the label plate; if the brushes are not yet authored, use the hard-step UI material and record it. No `.uasset` is edited or created by hand in this story run; asset authoring follows the project's never-resave rule.

Apply values only when changed (ADR-0021 §1) for anything that lives on the UMG wrapper; the leaf itself repaints every frame. No `UWidgetComponent`, no component on any pawn. Retire the prototype drawing in story 034, not here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-018-spike-ui-v1-zero-lag-projection.md`: the zero-lag spike this depends on.
- `story-020-glyph-states-and-edge-variant-evidence.md`: greyscale and colour-vision verification of the glyphs.
- `story-022-name-label-plate-rendering.md`: the label plate.
- `story-023-layer-order-and-view-gate-with-fade.md`: the fade layer order.
- the hud epic: the layer root, the DPI rule, ring and prompt painting.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/WorldAnchor/StealthWorldAnchorLeaf.h` and `.cpp` (shared with the hud epic; marker paint path)
- `Source/StealthPresentation/Private/WorldAnchor/StealthWorldAnchorLayer.h` and `.cpp` (the `UWidget` wrapper; shared with the hud epic)
- `Source/StealthPresentation/Private/Markers/MarkerGlyphs.h` and `MarkerGlyphs.cpp` (glyph construction for the three fills and the edge variant)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerLeafStructureTest.cpp` (name `StealthGame.Markers.LeafStructure`, class `FMarkersLeafStructureTest`)
- `Source/StealthTestHarness/Private/Markers/AlwaysOnTopTest.cpp` (CQTest, `StealthGame.Markers.PIE.AlwaysOnTopConstantSize.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Setup: ally at 3, 20 and 60 m, in the open and fully behind a wall; capture in 1P and 3P (eye mask hiding the ally's body). Verify: glyph size against F1, position above the anchor, opacity per capture. Pass condition: F1 size (+/-0 px) at every distance, identical in 1P and 3P, no opacity change; evidence `production/qa/evidence/tmk-ontop-*.png`.
- **AC2**: Setup: an automation test instantiates `SStealthWorldAnchorLeaf` with a spy projection provider, plus lint. Verify: `OnPaint` reads `ULocalPlayer::GetProjectionData`, `ForceVolatile` with `ComputeDesiredSize`, release in `ReleaseSlateResources`, integer-size brushes or a nearest-filter hard-step material with no mips, no per-marker widget class. Pass condition: every check true.
- **AC3**: Setup: resize, DPI change and ultrawide in turn, with captures. Verify: sizes, margin and safe rectangle recomputed each frame, `s_ui` applied once, DPI only from `UStealthDpiScalingRule`, sizes in physical px divided by leaf geometry scale. Pass condition: sizes equal F1 after each change; one retained screenshot per case.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each screen state touched, in `production/qa/evidence/` (`tmk-ontop-*.png` for AC1 and one per resize, DPI and ultrawide case for AC3), taken in 1P and 3P; plus `MarkerLeafStructureTest.cpp` for AC2

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-018-spike-ui-v1-zero-lag-projection.md` (must report PASS), `story-003-fill-precedence-and-roundended-freeze.md`, `story-009-fade-draw-order-no-motion.md`, `story-010-marker-size-f1.md`, `story-011-edge-clamp-f3.md`, `story-012-edge-de-overlap-f4.md`, `story-016-presenter-local-controller-only.md`; cross-epic: the hud epic's layer root, `UStealthDpiScalingRule` and UI-V3/UI-V4 stories (not yet created), `camera-system/story-012-eye-mask-core.md` (the 3P eye mask used in AC1)
- Unlocks: `production/epics/teammate-markers/story-020-glyph-states-and-edge-variant-evidence.md`, `story-021-hatch-on-body-hide-frame.md`, `story-022-name-label-plate-rendering.md`, `story-023-layer-order-and-view-gate-with-fade.md`, `story-032-performance-budget.md`, `story-034-retire-prototype-marker-drawing.md`
