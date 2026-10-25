# Story 012: Spike UI-V3: fade layer order under real plates and probes

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (Verification Required UI-V3, engine facts E1, E2, E11)
**ADR Decision Summary**: `StartCameraFade` darkens only the scene (E2), so it cannot cover L0 while leaving L1-L4 readable. Decision: Camera keeps all fade timing and publishes it through `GetScreenFade()` (`{float Alpha; FLinearColor Color}`, an amendment to ADR-0015 §6 and `IStealthCameraView`), and the HUD's fade layer at ZOrder 20 renders it. Fallback if the amendment is refused: Camera keeps calling `StartCameraFade` and the fade layer also draws `FadeAmount`/`FadeColor` each frame, accepting that the world is darkened twice mid-fade.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 UI-V3 (layer order): AC-HUD-24, the screen fade covers markers, ring and prompt and stays under plates, cards and menus. Engine facts [V] from the 5.8.4 source: E1 (`AddToPlayerScreen` widgets sit in the game layer's Player slot (100), below the Viewport slot (200) where every `AddToViewport` widget goes whatever its ZOrder: `UserWidget.h:342, 351`, `SGameLayerManager.cpp:133-143, 294-299`), E2 (the camera fade is applied as the scene view's `OverlayColor`, inside the scene render, so it can never cover UMG: `LocalPlayer.cpp:813-814`, `SceneView.cpp:895`), E11 (`FadeAmount` animates only while `bEnableFading` is set and the scene `OverlayColor` is applied under the same flag: `PlayerCameraManager.cpp:503, 977-984`, `LocalPlayer.cpp:810-815`). The spike measures the layer order on real pixels (TAA off, neutral tonemap) and states which fade source is usable. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: The screen fade is drawn by the HUD fade layer at ZOrder 20 from Camera's published fade — source: ADR-0021
- Forbidden: Never use `AddToViewport` or the `AHUD` canvas for game UI (lint) — the canvas cannot be ordered against the UMG fade — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

A conditioning spike: story 013 (the production fade layer and the layer-order test) is Blocked until this reports. The probe is throwaway and lives in the test harness (non-Shipping). The probe's deliberate `AddToViewport` widget (to prove E1) lives outside `StealthPresentation`, so the story 004 `AddToViewport` lint does not apply to it.

---

## Acceptance Criteria

*From ADR-0021 Verification Required UI-V3 and engine facts E1, E2, E11 (derived), scoped to this story:*

- [ ] **AC1** (derived, ADR-0021 UI-V3 and E1, spike probe): **GIVEN** probe widgets on L0, L1 and L3 and Camera's published fade at alpha 1 (TAA off, neutral tonemap) **WHEN** a frame is captured **THEN** the captured pixel at the L0 probe centre equals the fade colour +/- 2/255 per channel, at the L1 and L3 probe centres it equals the probe colour +/- 2/255, and `AddToPlayerScreen` widgets sit in the Player slot (100) below any `AddToViewport` widget (fact E1).
- [ ] **AC2** (derived, ADR-0021 UI-V3 and E2/E11, report review): **GIVEN** the report `production/qa/spikes/ui-v3-layer-order-report.md` **WHEN** it is reviewed **THEN** it states pass/fail for E1, E2 and E11 (`FadeAmount` animates only while `bEnableFading`) and the fallback choice if the fade must be driven by the HUD alone.

---

## Implementation Notes

*Derived from ADR-0021 §1, §3 and Verification Required UI-V3:*

Build the probe in the test harness (`Source/StealthTestHarness/Private/HUD/Spike/`), on a throwaway player-screen stack (not story 006's production roots):

- Probe widgets of flat colour with distinct known values: a probe on L0 (ZOrder 10, a plain `UImage`/`SColorBlock` is enough because the question is order, not projection), one on L1 (30) and one on L3 (50), plus the probe fade layer at ZOrder 20 (a full-screen `SColorBlock` driven by the published alpha and colour). Also add one `AddToViewport` widget at a low ZOrder over a known region to prove E1 (it must paint above every `AddToPlayerScreen` widget whatever its ZOrder).
- Fade source: Camera's published fade through `IStealthCameraView::GetScreenFade()` as added by `production/epics/camera-system/story-017-view-state-neutral-view-fade.md` (the ADR-0015 amendment). The probe sets the published fade to alpha 1 (full black, the fade colour) and reads it each frame. If story 017 has not yet shipped `GetScreenFade()`, use a stub implementing the same struct (`FStealthScreenFade { float Alpha; FLinearColor Color }`) and state in the report that the real publisher was not exercised; the E2 question is independent of who publishes. Also exercise the ADR fallback path: call `StartCameraFade` and confirm the world darkens but the L0 probe does not (E2), and that the fade layer drawing `FadeAmount`/`FadeColor` would reach the same black.
- Capture: disable TAA (`r.AntiAliasingMethod 0`), neutral tonemap (no auto exposure, `r.TonemapperGamma` default, `r.Tonemapper.Sharpen 0`, or a captured pre-tonemap path if available), fixed resolution, read the pixel at each probe centre from `FScreenshotRequest` / `ReadSurfaceData`. Pass criteria: pixel at the L0 probe centre equals the fade colour +/- 2/255 per channel; at the L1 and L3 probe centres equals the probe colour +/- 2/255. Also read the Slate layer order (the `SGameLayerManager` Player slot index (100) versus Viewport slot (200)) to state E1 as a fact.
- E11 check: set `bEnableFading` false and show that `FadeAmount` does not animate (the reason Camera computes its own alpha and the fade layer cannot rely on `FadeAmount`).

Capture retained as `production/qa/evidence/hud-ui-v3-layer-order.png`. Report `production/qa/spikes/ui-v3-layer-order-report.md`: pass/fail for E1, E2, E11; the measured pixel table (L0, L1, L3 probe centres versus expected); the build (5.8.4, CL 58902052); and the fallback choice if the fade must be driven by the HUD alone (the ADR's fade-layer-also-draws-`FadeAmount` fallback, with the double-darkening cost stated). A failure amends ADR-0021 §3 (and the ADR-0015 `GetScreenFade()` amendment) before story 013 starts; a pass unblocks 013.

Traps: tonemapper, TAA and exposure move pixel values by more than 2/255, so the capture settings are part of the test, not optional; capture in a development game process, not only the editor viewport (editor overlays and PIE viewport composition can differ); the L1/L3 probe regions must not overlap the L0 probe or the viewport probe.

Prototype disposition: the prototype has no fade; none to replace. Out of this story: the production fade layer (story 013), Camera's fade timing (camera epic story 017).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`: the production `UStealthScreenFadeLayer` and the layer-order test (AC-HUD-24).
- `production/epics/camera-system/story-017-view-state-neutral-view-fade.md`: Camera's fade timing and the published `GetScreenFade()`.
- `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`: the production layer roots.
- `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`: the L0 leaf (the probe here uses a plain image on L0).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/Spike/SpikeUiV3LayerOrderProbe.h` (throwaway probe: layered colour widgets, probe fade layer, pixel reader; not promoted)
- `Source/StealthTestHarness/Private/HUD/Spike/SpikeUiV3LayerOrderProbe.cpp`
- `Source/StealthTestHarness/Private/HUD/Spike/SpikeUiV3LayerOrderTest.cpp` (CQTest, name `StealthGame.HUD.PIE.SpikeUiV3LayerOrder.*`)
- `production/qa/spikes/ui-v3-layer-order-report.md` (the spike report; written by the spike, not a template)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: fade covers L0 only
  - Given: probe widgets on L0, L1 and L3, Camera's published fade at alpha 1, TAA off, neutral tonemap.
  - When: a frame is captured.
  - Then: the pixel at the L0 probe centre equals the fade colour +/- 2/255 per channel. At the L1 and L3 probe centres it equals the probe colour +/- 2/255. `AddToPlayerScreen` widgets sit in the Player slot (100) below any `AddToViewport` widget (fact E1).
  - Edge cases: none.
- **AC-2**: report content
  - Given: `production/qa/spikes/ui-v3-layer-order-report.md`.
  - When: it is reviewed.
  - Then: it states pass/fail for E1, E2 and E11 (`FadeAmount` animates only while `bEnableFading`), and the fallback choice if the fade must be driven by the HUD alone.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report `production/qa/spikes/ui-v3-layer-order-report.md` plus the retained capture `production/qa/evidence/hud-ui-v3-layer-order.png` and the probe test `Source/StealthTestHarness/Private/HUD/Spike/SpikeUiV3LayerOrderTest.cpp`, with sign-off in `production/qa/evidence/spike-ui-v3-layer-order-with-camera-fade-evidence.md`
- Gate: BLOCKING (Integration default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`; `production/epics/camera-system/story-017-view-state-neutral-view-fade.md`
- Unlocks: `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`
