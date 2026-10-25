# Story 011: SStealthWorldAnchorLeaf shell with constant-pixel anchors and the neutral-view collapse

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spike UI-V1 (teammate-markers story 018) has not reported
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input §2
**ADR Decision Summary**: `SStealthWorldAnchorLeaf` is a custom `SLeafWidget`, `ForceVolatile(true)`, never inside an invalidation or retainer box; its `UWidget` wrapper builds it in `RebuildWidget` and releases it in `ReleaseSlateResources`, and the leaf implements `ComputeDesiredSize`. In `OnPaint` it reads the frame's view through `ULocalPlayer::GetProjectionData`, projects each anchor, applies the marker offset, edge clamp and draw-order rules and emits draw elements. Sizes are constant in physical pixels (`u * s_ui * k_large`, floors not scaled) then divided by the leaf geometry's scale so the DPI scale is applied once. L0 collapses while `GetViewState() == Neutral`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 UI-V1 (zero-lag projection) is settled by spike story `teammate-markers/story-018`; this story is Blocked until its report exists and must follow its fallback if a fact failed (`FSceneViewExtension` view source). Engine facts E3 (paint sees the rendered frame's camera; `GetProjectionData` returns the view with aspect constraint and sub-rect applied), E4 (`ForceVolatile`), E12 (`ForceVolatile` pushed in `SynchronizeProperties`; the wrapper releases the Slate widget in `ReleaseSlateResources`), E5 (DPI rule `H/1080`, `s_ui` applied once; story 009). E1: the L0 layer sits at ZOrder 10 in the Player slot, below every `AddToViewport` widget. E2: the camera fade cannot cover UMG, which is why the fade is a separate layer at ZOrder 20 (story 013). `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: World-anchored drawing (markers, interact ring, context prompt) happens in `SStealthWorldAnchorLeaf::OnPaint` from the rendered frame's camera view; the leaf is volatile and never inside an invalidation or retainer box — source: ADR-0021
- Forbidden: Never add a `UWidgetComponent` or any other component to a pawn for presentation — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story provides the leaf and its draw-order and registration contract. Marker content and the model behind it belong to the teammate-markers epic; ring and prompt content belong to stories 048 and 049.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` HUD-R2, HUD-R4 and Camera CS-R12.2 (L0 collapses in the neutral view), ADR-0021 §2 (derived items marked), scoped to this story:*

- [ ] **AC1** (derived, HUD-R4 and ADR-0021 §2, `StealthGame.HUD.PIE.WorldAnchorLeaf`): **GIVEN** the leaf on L0 (ZOrder 10) at 5 m and 50 m, `s_ui` 1.0 and 1.5 **WHEN** it paints **THEN** it projects only inside `OnPaint` from the rendered frame's view, draws at constant pixel size (a marker's pixel rect is identical at 5 m and 50 m and scales only with `s_ui`), edge-clamps, and exposes the registration point that teammate markers, the interact ring, the context prompt and ping rings use (draw order: ping rings beneath markers).
- [ ] **AC2** (derived, HUD-R4 and CS-R12.2, `StealthGame.HUD.PIE.WorldAnchorLeaf`): **GIVEN** `GetViewState() = Neutral` **WHEN** a frame renders **THEN** L0 collapses (every world-anchored element hidden) while L1-L4 stay readable on the black view and are unchanged versus the Live view.
- [ ] **AC3** (derived, ADR-0021 §2 and story 004 lints, `StealthGame.HUD.PIE.WorldAnchorLeaf`): **GIVEN** the HUD sources **WHEN** the story 004 lints run **THEN** no world-anchored position is computed in a UMG binding, widget tick or game-thread tick, and no pawn component is added (covered by the `ProjectWorldLocationToScreen`/`GetProjectionData`-outside-`OnPaint` lint and the `UWidgetComponent` lint of story 004).

---

## Implementation Notes

*Derived from ADR-0021 §2, §1 (L0) and the UI-V1 spike report (`production/qa/spikes/ui-v1-projection-report.md`, read first):*

Create in `Source/StealthPresentation/Private/WorldAnchor/`:

- `SStealthWorldAnchorLeaf : SLeafWidget`: `ForceVolatile(true)`, `ComputeDesiredSize` implemented, `OnPaint` const override. It is the only place that calls `ULocalPlayer::GetProjectionData` or projects a world point (story 004 lint: allowlist is exactly `SStealthWorldAnchorLeaf::OnPaint`). In `OnPaint` it (1) reads the frame's projection data for the owning local player, (2) projects each registered anchor, (3) applies the anchor offset, edge clamp with hysteresis, edge de-overlap and far-to-near draw order that the Teammate Markers GDD defines (F2-F4; the full rules and content are the teammate-markers epic, this shell provides the hooks and the clamp-to-safe-rect behaviour), and (4) emits draw elements (nearest-filtered glyph brushes or a hard-step UI material, text for labels and the prompt).
- Registration contract: `FWorldAnchorRegistry` (plain C++, held by the bootstrap, read by the leaf) with typed layers drawn in a fixed order, back to front: ping rings and ping edge rings (beneath), teammate markers and labels, interact ring, context prompt. A producer registers a per-frame `FWorldAnchorViewModel` slice (the `FHudWorldAnchorVm` cluster of story 002: markers: handle, anchor source, fill, phase of fade/hold; ring: target object, fill fraction, colour state; prompt: object, verb, glyph). The registration point accepts only these value structs, never an `AActor`/`APawn` (story 004 lint; the leaf registration is its single allowlisted exception, because world positions come from the producers' resolved values, never from the leaf holding an actor).
- Constant pixel size: sizes are computed in physical pixels from F1 (`u * s_ui * k_large`, absolute floors `W_min`/`H_min` not scaled), then divided by the leaf geometry's scale so the DPI scale (story 009) is applied once. A marker's pixel rect is identical at 5 m and 50 m and scales only with `s_ui` (D20: world-anchored elements are exempt from the centre-empty rule, and from F1's A_persist).
- `UStealthWorldAnchorLayer` (L0 widget shell from story 006): builds the leaf in `RebuildWidget`, calls `ForceVolatile` in `SynchronizeProperties`, releases the leaf in `ReleaseSlateResources` (E12), and is never nested under an invalidation or retainer box. The layer collapses via the apply-if-changed helper while the presenter's view-state input is `Neutral` (CS-R12.2: Camera's neutral view hides every world-anchored element while L1-L4 stay readable on the black view); the registry keeps running while gated (the markers model and `IsMarkedAlly` do not stop, TM-R8).
- View-state source: `FHudInputs.ViewState` from Camera `GetViewState()` (story 003 samples it; `production/epics/camera-system/story-017-view-state-neutral-view-fade.md` publishes it).

Prototype disposition: the prototype's per-frame canvas projection in `AStealthHUD::DrawTeammateMarkers` (`Source/StealthGame/Private/StealthHUD.cpp`) is replaced and not ported; no per-marker widget and no widget tick position is allowed (ADR-0021 Forbidden).

Traps: (1) a marker widget per ally positioned by tick shows one frame of lag; the leaf is one widget drawing all anchors. (2) Never put the leaf inside an invalidation panel (the L1-L3 layers use invalidation panels per cluster, L0 must not). (3) Release resources in `ReleaseSlateResources` (E12) or PIE restarts leak the Slate widget. (4) At Neutral, only L0 collapses; the fade layer (story 013) and L1-L4 are untouched, so a test that asserts "unchanged versus Live" must diff view-models, not pixels. (5) Draw order is asserted from the paint-order list (a recorded sequence of layer ids), not from pixels.

Decisions that apply: D20 (world-anchored markers and ping rings are exempt from the centre-empty rule). Out of this story: marker, ring, prompt and ping content, the markers model, the fade layer (013), DPI rule (009).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/teammate-markers/story-018-spike-ui-v1-zero-lag-projection.md`: the conditioning spike.
- `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md`: the marker paint path; it builds on this shell (its Files list the same `StealthWorldAnchorLeaf` and `StealthWorldAnchorLayer` files, shared with this epic: whichever story lands first creates them, the other edits).
- `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`: the fade layer and layer-order asserts.
- `production/epics/hud/story-048-hud-interact-ring-and-context-prompt.md`: ring and prompt content.
- `production/epics/hud/story-049-hud-team-ping-rings-l0.md`: ping ring content and `FTeamPingRingModel`.
- `production/epics/hud/story-004-hud-include-boundary-and-forbidden-api-lint.md`: the projection and `UWidgetComponent` lints this story relies on.
- Teammate-markers epic (`production/epics/teammate-markers/`): the markers model, marker shapes and `OnAllyMarkerRetired` (TM-R14).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/WorldAnchor/StealthWorldAnchorLeaf.h`
- `Source/StealthPresentation/Private/WorldAnchor/StealthWorldAnchorLeaf.cpp`
- `Source/StealthPresentation/Private/WorldAnchor/WorldAnchorRegistry.h`
- `Source/StealthPresentation/Private/WorldAnchor/WorldAnchorRegistry.cpp`
- `Source/StealthPresentation/Private/WorldAnchor/StealthWorldAnchorLayer.h` (edit or create: the L0 `UWidget` wrapper)
- `Source/StealthPresentation/Private/WorldAnchor/StealthWorldAnchorLayer.cpp`
- `Source/StealthTestHarness/Private/HUD/WorldAnchorLeafTest.cpp` (CQTest, `NETWORK_TEST_CLASS(WorldAnchorLeaf, "StealthGame.HUD.PIE")`, full name `StealthGame.HUD.PIE.WorldAnchorLeaf.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: leaf painting, constant pixel size and registration
  - Given: the leaf on L0 (ZOrder 10), at 5 m and 50 m, `s_ui` 1.0 and 1.5.
  - When: it paints.
  - Then: it projects only inside `OnPaint` from the rendered frame's view. A marker's pixel rect is identical at 5 m and 50 m and scales only with `s_ui`. It edge-clamps. It exposes the registration point used by markers, the interact ring, the context prompt and ping rings, with ping rings drawn beneath markers.
  - Edge cases: assert draw order from the paint-order list. Blocked until spike 010 reports.
- **AC-2**: neutral-view collapse
  - Given: `GetViewState() = Neutral`.
  - When: a frame renders.
  - Then: L0 is collapsed (every world-anchored element hidden), and L1-L4 are present and unchanged versus the Live view.
  - Edge cases: none.
- **AC-3**: no world position outside `OnPaint`
  - Given: the HUD sources.
  - When: the story 004 lints run.
  - Then: there is no `ProjectWorldLocationToScreen`/`GetProjectionData` outside `OnPaint`, no `UWidgetComponent`, and no UMG-binding or tick world position.
  - Edge cases: depends on the projection lint in story 004 AC2.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/WorldAnchorLeafTest.cpp` (`StealthGame.HUD.PIE.WorldAnchorLeaf`, CQTest PIE) OR a documented playtest
- Gate: BLOCKING (Integration default). Blocked until spike UI-V1 (teammate-markers story 018) has reported.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`, `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`, `production/epics/teammate-markers/story-018-spike-ui-v1-zero-lag-projection.md`
- Unlocks: `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`, `production/epics/hud/story-048-hud-interact-ring-and-context-prompt.md`, `production/epics/hud/story-049-hud-team-ping-rings-l0.md`
