# Story 013: HUD fade layer renders Camera's published fade; AC-HUD-24 LayerOrder

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spike UI-V3 (story 012) has not reported
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§1 Layers, §3 The screen fade is a HUD layer that Camera drives)
**ADR Decision Summary**: Every HUD layer is a `UUserWidget` added with `AddToPlayerScreen` at its HUD-R4 ZOrder (L0 10, Fade 20, L1 30, L2 40, L3 50, L4 60, Menus 100); every layer below 100 is `HitTestInvisible`, non-focusable and never a `UCommonActivatableWidget`. Camera keeps all fade timing and publishes `FStealthScreenFade GetScreenFade() const` (`{Alpha, Color}`); the HUD's fade layer at ZOrder 20 only renders that value, because `StartCameraFade` darkens the scene only and can never cover L0.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This story proves ADR-0021 Verification Required UI-V3 (layer order) as a PIE test once spike story 012 has reported. Engine facts it rests on: E1 (`AddToPlayerScreen` widgets sit in the Player slot 100, below any `AddToViewport` widget), E2 (a camera fade is the scene view's `OverlayColor`, so it can never cover UMG), E11 (`FadeAmount` animates only while `bEnableFading`, which is why Camera computes the alpha itself). If the spike reports that the `GetScreenFade()` amendment was refused, ADR-0021 §3 gives the fallback (the layer also draws `FadeAmount`/`FadeColor` each frame and the world is darkened twice mid-fade); this story follows whatever the spike report records. The `StealthTestHarness` multi-client PIE harness comes from the information-visibility-policy epic (ADR-0008 T2).

**Control Manifest Rules (this layer)**:
- Required: The screen fade is drawn by the HUD fade layer at ZOrder 20 from Camera's published fade — source: ADR-0021
- Forbidden: Never use `AddToViewport` or the `AHUD` canvas for game UI (lint) — the canvas cannot be ordered against the UMG fade — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story makes the layer order an executable rule. Story 006 builds the layer stack and story 012 measured, on probe widgets, that a published fade covers L0 and leaves L1 and L3 readable. Here the real fade layer is built and the GDD's AC-HUD-24 is turned into an automated PIE test over the real layer stack, so any later change to a ZOrder, a layer's hit-test visibility or the fade ownership fails a test instead of a playtest. The HUD decides nothing about fade timing: Camera's `GetScreenFade()` is the only input.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-24) and `design/ux/hud.md` (UXA-HUD-30), scoped to this story:*

- [ ] **AC1** (derived, ADR-0021 §3, integration test `StealthGame.HUD.PIE.LayerOrder` fade method): **GIVEN** the fade widget at ZOrder 20 **WHEN** Camera's published fade is at alpha 1 and a frame is captured **THEN** the widget renders `GetScreenFade()` (alpha, colour) every frame and decides nothing about timing; the world pixels are <= 2/255 of the fade colour and the L1-L4 probe pixels match their no-fade capture +/- 2/255.
- [ ] **AC2** (AC-HUD-24, `StealthGame.HUD.PIE.LayerOrder`): **GIVEN** an own-elimination fade and the neutral view, **WHEN** a teammate marker, an interact ring, a feed line and a card overlap one screen point, **THEN** the fade covers the marker and ring and leaves the feed line and card fully visible; the HUD layers' ZOrders are 10/20/30/40/50/60/100; no HUD layer changes the input mapping context; every widget on a layer below ZOrder 100 (L4 included) is `HitTestInvisible` with no button or click handler. "Covers" and "fully visible" use the +/- 2/255 pixel tolerance of story 012. (The Session status overlay split clause of AC-HUD-24 is verified once, in story 057.)
- [ ] **AC3** (UXA-HUD-30, retained screenshot `production/qa/evidence/hud-layer-order-1080p.png`): **GIVEN** the AC2 overlap at 1080p **WHEN** the frame is captured **THEN** the camera fade covers an ally marker, an interact ring and a ping ring and leaves the feed line, the card and the countdown fully visible; the layers sit at ZOrder 10, 20, 30, 40, 50, 60 and 100.

---

## Implementation Notes

*Derived from ADR-0021 Decision §1 (layers), §3 (fade) and Implementation Guidelines:*

**What to build.** `UStealthScreenFadeLayer` is the Fade layer of ADR-0021 §1: one `UUserWidget` containing one full-screen opaque image (a `SColorBlock`-style brush, no texture), added with `AddToPlayerScreen(20)` by the layer bootstrap of story 006 and created once at HUD init like every layer. Each frame the presenter reads `IStealthCameraView::GetScreenFade()` (Camera's amendment to ADR-0015 §6; the interface lives in `StealthGameplay/Public/View/Core/`) through `FStealthClientViewHandles` and puts the `{Alpha, Color}` into `FHudViewModel`'s fade field as plain values (the `FHudInputs` field is added by story 003's sampler if it is not there yet). The widget applies the colour and render opacity only when the value differs from the last applied one (ADR-0021 "apply on change only"), collapses at alpha 0 and never animates, eases or times anything. A fade that ends is Camera ending it.

**Why a HUD layer.** `StartCameraFade` is the scene view's `OverlayColor` (E2) and cannot cover UMG; the HUD GDD requires the fade to cover world-anchored L0 content (markers, ring, prompt, ping rings) while the feed line, card and countdown stay readable (HUD-R4, D19 puts the rejection plate on L1 as well). The only way to get that order is a layer between L0 (10) and L1 (30). Keep the fade layer free of any prototype code: the prototype `Source/StealthGame/Private/StealthHUD.cpp` has no fade and nothing is promoted from it.

**The AC-HUD-24 test.** Write one CQTest PIE test, `NETWORK_TEST_CLASS(LayerOrder, "StealthGame.HUD.PIE")`, in `StealthTestHarness`, with these methods: `FadeRendersPublishedValue` (AC1), `OverlapOrder` (AC2 pixel half), `ZOrdersAndHitTest` (AC2 structure half). The structure half reflects the player screen: it asserts the layers' ZOrders are exactly 10/20/30/40/50/60/100 for L0, Fade, L1, L2, L3, L4 and the Menu root, that no layer below 100 derives from `UCommonActivatableWidget`, has a button or click handler, or reports any visibility other than `HitTestInvisible`, and that the active gameplay input mapping contexts are identical before and after the HUD stack is created and after the fade runs (the HUD never adds or removes a context, ADR-0021 §7). The pixel half builds the overlap: put the own pawn in the neutral view (Camera's `Neutral` view state, camera-system story 017, collapses L0 in normal play, so for this test the L0 content is driven by the test through the leaf's view-model path to make the cover-or-not question observable), place a marker, an interact ring, a feed line and a card so they overlap one screen point, publish a fade alpha of 1 through a test fake of `IStealthCameraView` (or Camera's real fade where camera story 017 is done), capture the frame with TAA off and a neutral tonemap as spike 012 did, and sample the overlap point and the probe points. Reuse the capture helpers and the +/- 2/255 oracle that story 012 produced; do not write a second tolerance. Where a production cluster widget does not exist yet (feed line and card widgets arrive in stories 025 and 036), a test-only probe widget on the same layer stands in: what is under test is the layer, not the cluster.

**Determinism.** Fixed step 60 Hz, identical seeds, capture at a named tick (GDD AC preamble). The test makes no wall-clock assertion.

**Traps.** (1) A layer widget left at default visibility is `Visible` and would pass a pixel test while breaking input; the structure half is mandatory. (2) `AddToViewport` would put a widget above all layers whatever its ZOrder (E1): the lint of story 004 covers the source, this test covers the result. (3) The fade layer must still be added when Camera publishes alpha 0 (collapsed, not absent) so the widget tree is identical for every viewer and outcome. (4) `FadeAmount` does not animate unless `bEnableFading` (E11): never read it unless the spike chose the documented fallback.

**Out of this story:** Camera's own fade timing and the `GetScreenFade()` implementation (camera-system story 017), the layer roots and input isolation themselves (story 006), the Session overlay split (story 057).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`: creating the layer widgets, ZOrders and `HitTestInvisible` defaults.
- `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`: the L0 leaf that the fade must cover.
- `production/epics/hud/story-012-spike-ui-v3-layer-order-with-camera-fade.md`: the spike, its capture helpers and its +/- 2/255 oracle.
- `production/epics/hud/story-057-hud-menu-coexistence-and-l4-status-host.md`: the Session-overlay L4 split and the action modal at ZOrder 100.
- `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`: the full 2v2 observation run.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthScreenFadeLayer.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthScreenFadeLayer.cpp`
- `Source/StealthPresentation/Private/Hud/HudViewModel.h` (add the fade field if story 002 left it out)
- `Source/StealthPresentation/Private/Hud/StealthHudPresenter.cpp` (sample `GetScreenFade()` into `FHudInputs`)
- `Source/StealthTestHarness/Private/HUD/LayerOrderTest.cpp` (CQTest, `StealthGame.HUD.PIE.LayerOrder.*`)

The fade widget's Blueprint (layout only) and any probe widgets are named in prose only; no `Content/` paths are planned here.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Fade widget renders Camera's fade
  - Given: the fade widget at ZOrder 20 and Camera's fade at 1.
  - When: a frame is captured.
  - Then: it renders `GetScreenFade()` (alpha, colour) every frame and decides no timing. World pixels are <= 2/255, and L1-L4 probe pixels match their no-fade capture +/- 2/255.
  - Edge cases: blocked until spike 012 reports.
- **AC-2**: AC-HUD-24 `LayerOrder`
  - Given: an own-elimination fade and the neutral view, with a teammate marker, interact ring, feed line and card overlapping one screen point.
  - When: the frame is captured and the widget tree reflected.
  - Then: the fade covers marker and ring, and leaves feed line and card fully visible (both within +/- 2/255 of the story 012 oracle). ZOrders are 10/20/30/40/50/60/100. No HUD layer changes the input mapping context. Every widget below 100 (L4 included) is `HitTestInvisible` with no button.
  - Edge cases: the Session-overlay clause is verified once, in story 057.
- **AC-3**: UXA-HUD-30 layered overlap (manual check)
  - Setup: reproduce the AC2 overlap at 1080p.
  - Verify: marker and ring are covered by the fade, and feed line and card are fully visible.
  - Pass condition: `production/qa/evidence/hud-layer-order-1080p.png` is retained and matches.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest PIE test `Source/StealthTestHarness/Private/HUD/LayerOrderTest.cpp` (`StealthGame.HUD.PIE.LayerOrder`), plus the retained screenshot `production/qa/evidence/hud-layer-order-1080p.png` (UXA-HUD-30). Gate level BLOCKING (`qa.level` does not make this advisory).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`, `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`, `production/epics/hud/story-012-spike-ui-v3-layer-order-with-camera-fade.md`; cross-epic: camera-system `production/epics/camera-system/story-017-view-state-neutral-view-fade.md` (`GetScreenFade()` and the neutral view)
- Unlocks: `production/epics/hud/story-055-hud-photosensitivity-removal-sequence.md`, `production/epics/hud/story-057-hud-menu-coexistence-and-l4-status-host.md`
