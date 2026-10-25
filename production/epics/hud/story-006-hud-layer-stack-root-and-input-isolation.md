# Story 006: HUD layer stack at the HUD-R4 ZOrders, created once, hit-test-invisible, with no input effects

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`, `TR-hud-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input §1 (with ADR-0007 for `AStealthHUD` ownership)
**ADR Decision Summary**: Each layer is one `UUserWidget` subclass created once per local player by the bootstrap and added with `AddToPlayerScreen(ZOrder)`: L0 world-anchored 10, Fade 20, L1 persistent plates 30, L2 lower-centre 40, L3 cards and banners 50, L4 status overlays 60, Menus 100. All are created at HUD init, collapsed until needed and never re-created during a round. Every layer below 100 is `HitTestInvisible`, not focusable, has no input bindings and is never a `UCommonActivatableWidget`. `AddToViewport` and the `AHUD` canvas are banned for game UI.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 engine facts: E1 (`AddToPlayerScreen` puts the widget in the local player's overlay in the game layer's Player slot (100), below the Viewport slot (200) of every `AddToViewport` widget whatever its ZOrder; hence the lint of story 004), E2 (UMG is composited after the scene and its post-process, so no grade darkens any layer), E6 (a `UCommonActivatableWidget` adds and removes its `InputMapping` on activation, which could change contexts mid-strike; hence no activatable below 100), E8 (Common UI routing needs `UCommonGameViewportClient`; plugin enabled in `StealthGame.uproject` since 2026-10-25). Verification item UI-V3 (layer order against the camera fade) is asserted by story 013 on top of this stack, after spike story 012. Enhanced Input / Common UI are unified in 5.8; this story touches neither beyond asserting the active context set is unchanged. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Every HUD and marker layer is a UMG widget added with `AddToPlayerScreen` at its HUD-R4 ZOrder — source: ADR-0021
- Forbidden: Never use `AddToViewport` or the `AHUD` canvas for game UI (lint) — the canvas cannot be ordered against the UMG fade — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This is the structural story of the HUD epic: it creates `AStealthHUD` as a bootstrap owner (no drawing), the empty layer widgets and the proof that they exist once, in order, and take no input. Every later widget story fills a layer that already exists.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` HUD-R4, AC-HUD-22 (no widget constructed during `Active`) and `design/ux/hud.md` UXA-HUD-29 (automated half), scoped to this story:*

- [ ] **AC1** (derived, HUD-R4 and ADR-0021 §1; AC-HUD-22 clause, `StealthGame.HUD.PIE.LayerStack`): **GIVEN** a local player **WHEN** the HUD initialises **THEN** `AStealthHUD` (StealthPresentation) adds one UMG root per layer with `AddToPlayerScreen` at ZOrder 10 (L0), 20 (fade), 30 (L1), 40 (L2), 50 (L3), 60 (L4) and the menu root at 100, every widget is created once at init (the widget-construction counter delta is 0 between entering and leaving `Active`, AC-HUD-22 clause; the story names which module creates the ZOrder-100 menu root) and toggled by visibility.
- [ ] **AC2** (derived, ADR-0021 §1 and the L4 input rule, `StealthGame.HUD.PIE.LayerStack`): **GIVEN** every widget on a layer below ZOrder 100 (L4 included) **WHEN** it is reflected **THEN** each is `HitTestInvisible`, non-focusable, has no button or click handler and no input binding, and none is a `UCommonActivatableWidget`.
- [ ] **AC3** (UXA-HUD-29 automated half, `StealthGame.HUD.PIE.LayerStack`): **GIVEN** HUD layers that show, hide and animate **WHEN** any HUD layer shows, hides or animates **THEN** the active input-mapping-context set (ids and priorities) is identical before and after and no HUD widget holds keyboard or gamepad focus (UXA-HUD-29: "No HUD layer can be focused or clicked on keyboard-mouse or gamepad"; the manual keyboard-only and gamepad-only round is story 054).

---

## Implementation Notes

*Derived from ADR-0021 §1, §7 (menu root ownership) and ADR-0007 (presentation wiring):*

Create in `StealthPresentation`:

- `AStealthHUD : AHUD` (`Public/StealthHUD.h` is the prototype name in `StealthGame`; the production class lives at `Source/StealthPresentation/Public/Hud/StealthPresentationHUD.h` or the name the composition-root story fixes, and it keeps the ADR-0021 name `AStealthHUD` once the prototype class is retired in story 059; until then use a distinct C++ class name to avoid a UHT clash and note the rename in story 059). It owns `UStealthPresentationBootstrap` and draws nothing: no `ReceiveDrawHUD`, no canvas calls. `StealthGame` only sets `HUDClass` (ADR-0007 Forbidden: never construct presentation objects in `StealthGame` beyond setting `HUDClass`).
- `UStealthPresentationBootstrap` creates, once per local player at `AStealthHUD::BeginPlay` (guard against re-entry), the seven roots and calls `AddToPlayerScreen` with the HUD-R4 ZOrders held as named constants in one header (`HudZOrder.h`: `L0 = 10`, `Fade = 20`, `L1 = 30`, `L2 = 40`, `L3 = 50`, `L4 = 60`, `Menus = 100`): `UStealthWorldAnchorLayer` (shell; the leaf is story 011), `UStealthScreenFadeLayer` (shell; story 013), `UStealthHudPersistentLayer`, `UStealthHudTransientLayer`, `UStealthHudCardLayer`, `UStealthStatusOverlayLayer`, and the Menu root `UStealthMenuRoot`. Each non-menu layer is a `UUserWidget` subclass whose root panel is `HitTestInvisible` and whose `bIsFocusable` is false, set in C++ (the layer Blueprints carry layout and style only). Elements inside a layer are toggled by `SetVisibility(Collapsed/HitTestInvisible)` through the apply-if-changed helper of story 002, never created or destroyed in response to gameplay.
- Which module creates the ZOrder-100 menu root: the plan requires this story to name it. Decision to record in the story at implementation: `UStealthMenuRoot` is a Common UI `UCommonActivatableWidgetStack` host owned by the Session & Travel / menu work, but the bootstrap in `StealthPresentation` is the creator and adds it with `AddToPlayerScreen(100)`; its content is out of this epic. A persistent menu-root activatable (ADR-0021 §7: `IMC_Menu` is its only `InputMapping`) is not built here; this story adds an inert root so the ZOrder is reserved and the destroy count assertion can include it.
- Widget-construction counter: add a non-Shipping counter (incremented in each layer widget's `NativeOnInitialized` and element-factory) and a destroy counter (`NativeDestruct`), readable by the PIE test, so AC1's delta-0 assertion and the destroy-count-0 assertion have an oracle. The counters never feed gameplay.

Input isolation: no layer binds `OnClicked`, `OnPressed` or any `UButton`; no layer calls `SetUserFocus` or `SetKeyboardFocus`; no layer derives from `UCommonActivatableWidget`. The PIE test captures the Enhanced Input mapping-context set of the local player (`UEnhancedInputLocalPlayerSubsystem` mapping contexts with priorities) before and after a scripted show / hide / animate of each layer, and asserts equality; it also asserts `FSlateApplication::GetUserFocusedWidget` is not a HUD widget. The manual keyboard-only and gamepad-only completion of a round is story 054, which closes UXA-HUD-29.

Prototype disposition: this REPLACES `Source/StealthGame/Private/StealthHUD.cpp` / `Public/StealthHUD.h` (281 + 40 lines, `AHUD` canvas). Nothing is ported (ADR-0021 Forbidden: never port the prototype drawing code). The prototype keeps running until story 059 removes it; both must not draw at once, so the production `HUDClass` swap in `StealthGame` config is made in story 059 (or behind a project setting here) and this story tests the production HUD by spawning it explicitly in PIE.

Traps: (1) one stray `AddToViewport` breaks the order (E1), so the test also asserts no HUD widget is in the Viewport slot. (2) `AddToPlayerScreen` needs a valid owning `ULocalPlayer`; on a listen-server host with a dedicated-look setup, create layers only when `GetOwningPlayer()->IsLocalController()`. (3) Re-creation on controller replacement at seamless travel is a rebuild (ADR-0005 replaces controllers at travel); the bootstrap re-binds and re-creates there, which is a legitimate non-`Active` event, and story 044/045 own the rebuild semantics. (4) Fade and L0 contents are shells in this story; their behaviour is stories 011 and 013.

Decisions that apply: D19 (the rejection plate is layer L1, ADR-0021 authoritative), D20 (world-anchored elements are exempt from the centre-empty rule). Out of this story: fade content (011/013), the leaf (011), DPI (009).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`: `SStealthWorldAnchorLeaf` and the L0 content contract.
- `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`: fade rendering and the UI-V3 layer-order asserts.
- `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`: the DPI rule and the `s_ui` root scaler.
- `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`: the manual keyboard-only and gamepad-only round.
- `production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`: deleting the prototype `AHUD` canvas and message plumbing.
- `production/epics/hud/story-057-hud-menu-coexistence-and-l4-status-host.md`: menu coexistence and the L4 status host content.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Public/Hud/StealthPresentationHUD.h` (the production HUD owner; final class name `AStealthHUD` per story 059)
- `Source/StealthPresentation/Private/Hud/StealthPresentationHUD.cpp`
- `Source/StealthPresentation/Private/Hud/StealthPresentationBootstrap.h`
- `Source/StealthPresentation/Private/Hud/StealthPresentationBootstrap.cpp`
- `Source/StealthPresentation/Private/Hud/HudZOrder.h`
- `Source/StealthPresentation/Private/Hud/Widgets/HudLayerWidgets.h` (the six layer widget shells and the inert `UStealthMenuRoot`)
- `Source/StealthPresentation/Private/Hud/Widgets/HudLayerWidgets.cpp`
- `Source/StealthPresentation/Private/Hud/HudWidgetCounters.h` (non-Shipping construction and destroy counters)
- `Source/StealthTestHarness/Private/HUD/LayerStackTest.cpp` (CQTest, `NETWORK_TEST_CLASS(LayerStack, "StealthGame.HUD.PIE")`, full name `StealthGame.HUD.PIE.LayerStack.*`; UI-V3 asserts are added in story 013)
- Widget Blueprints (`WBP_` layer roots) are Content assets named in prose only

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: layer roots created once
  - Given: a local player and HUD init.
  - When: init runs, then `Active` is entered and left.
  - Then: one UMG root per layer is added with `AddToPlayerScreen` at ZOrder 10, 20, 30, 40, 50, 60, and the menu root at 100. The widget-construction counter delta is 0 between entering and leaving `Active`. Elements are toggled by visibility.
  - Edge cases: the story names the module that creates the ZOrder-100 menu root. Also assert the destroy count is 0.
- **AC-2**: hit-test isolation
  - Given: every widget on a layer below ZOrder 100 (L4 included).
  - When: it is reflected.
  - Then: each is `HitTestInvisible`, non-focusable, has no button, click handler or input binding, and is not a `UCommonActivatableWidget`.
  - Edge cases: none.
- **AC-3**: no input effects
  - Given: HUD layers that show, hide and animate.
  - When: the active input-mapping-context set is captured before and after.
  - Then: ids and priorities are identical, and no HUD widget holds keyboard or gamepad focus.
  - Edge cases: the manual keyboard-only and gamepad-only round is story 054.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/LayerStackTest.cpp` (`StealthGame.HUD.PIE.LayerStack`, CQTest PIE) OR a documented playtest. No screenshot is required beyond the tests (the layers are empty shells here).
- Gate: BLOCKING (Integration default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`, `production/epics/hud/story-003-hud-input-sampling-and-host-parity-path.md`
- Unlocks: `production/epics/hud/story-008-spike-ui-v4-dpi-scaling-rule.md`, `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`, `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`, `production/epics/hud/story-012-spike-ui-v3-layer-order-with-camera-fade.md`, `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`, `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-057-hud-menu-coexistence-and-l4-status-host.md`, `production/epics/hud/story-059-hud-retire-prototype-canvas-and-message-plumbing.md`
