# Story 009: UStealthDpiScalingRule (H/1080) and the layer-root scaler applying s_ui once

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spike UI-V4 (story 008) has not reported
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`, `TR-hud-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input §6
**ADR Decision Summary**: `UStealthDpiScalingRule : UDPICustomScalingRule` returns `ViewportHeight / 1080` only, so one Slate unit is one 1080p reference pixel. The client-local UI scale `s_ui` (MVP 1.0, a VS setting; TDC CR-14, never tuning) is applied by the HUD root: every layer's root is a scale container (`UScaleBox` with a user-specified scale, or an equivalent layout-affecting root scaler) set to `s_ui`, and the L0 leaf multiplies its glyph sizes by `s_ui` directly. `u * s_ui` is applied exactly once; `ApplicationScale` stays 1.0 and `SetApplicationScale` is never used.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 UI-V4 (DPI) and engine fact E5: a custom rule's result is cached per viewport size outside the editor, so a runtime-changing factor cannot live in the rule; `ApplicationScale` is multiplied in and platform DPI is divided out by the game layer manager; `SetApplicationScale` is process-wide. Spike story 008 settles the [U] parts (cache behaviour, PIE editor-scale safety, floors at real sizes); this story is Blocked until it reports, and must adopt the report's fallback if a fact failed. Config to add (this story): `[/Script/Engine.UserInterfaceSettings] UIScaleRule=Custom`, `CustomScalingRuleClass=/Script/StealthPresentation.StealthDpiScalingRule`. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: DPI comes only from `UStealthDpiScalingRule` (`H/1080`); `s_ui` is applied once by the layer roots and the L0 leaf; `ApplicationScale` stays 1.0 — source: ADR-0021
- Forbidden: Never call `SetApplicationScale`, apply `s_ui` inside the DPI rule, or use the engine DPI curve — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story turns the spike's probe into the production rule and wires the scaler into the layer roots created by story 006, using the resolver of story 007 as the oracle for real scale values.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` HUD-R16 and F1, `design/ux/hud.md` UXA-HUD-05 and AC-HUD-15 (Logic via the resolver), scoped to this story:*

- [ ] **AC1** (derived, ADR-0021 §6, `StealthGame.HUD.DpiRule`): **GIVEN** viewport sizes from 1280x720 to 4320p and `s_ui` = 1.5 **WHEN** `UStealthDpiScalingRule::GetDPIScaleBasedOnSize` and each layer root's effective scale are read **THEN** the rule returns `Size.Y / 1080` only, root and leaf scale = DPI x `s_ui` (applied exactly once, never squared and never inside the rule), `ApplicationScale` stays 1.0, and a `SetApplicationScale` spy shows 0 calls (including after `s_ui` changes from 1.5 to 0.75 at runtime).
- [ ] **AC2** (UXA-HUD-05, `StealthGame.HUD.PIE.ResizeNoReplay`): **GIVEN** a plate showing and the cooldown sweep running **WHEN** the window is resized, and separately when UI scale changes mid-round **THEN** every element relayouts without replaying any animation or cue ("Resizing the window or changing UI scale mid-round relayouts every element without replaying any animation or cue"), every rect equals the resolver output for the new values, and the cue-event and plate-start counters and `t0` values are unchanged.
- [ ] **AC3** (AC-HUD-15 Logic via the resolver, `StealthGame.HUD.DpiRule`): **GIVEN** `s_ui` 75/100/150 % on real scale values **WHEN** effective text sizes are read through the resolver **THEN** they hold the floors at 1080p-equivalent (>= 18 / 22 / 28 px x H/1080, 20 px for actionable text).

---

## Implementation Notes

*Derived from ADR-0021 §6 and the UI-V4 spike report (`production/qa/spikes/ui-v4-dpi-report.md`, to be read first):*

Read the spike report before starting. If it passed, implement as below; if a fact failed, implement the report's named fallback and amend ADR-0021 §6 in the same change (do not silently diverge). Create:

- `UStealthDpiScalingRule : UDPICustomScalingRule` in `Source/StealthPresentation/Private/Hud/Scaling/StealthDpiScalingRule.h/.cpp`; `GetDPIScaleBasedOnSize(FIntPoint Size)` returns `Size.Y / 1080.f` and nothing else: no `s_ui`, no curve, no min/max clamp beyond guarding `Size.Y <= 0` (return 1). The rule runs on the class default object, so it must hold no state.
- The config entry in `Config/DefaultEngine.ini` (or the config file the implementing story's ADR note names): `UIScaleRule=Custom` and `CustomScalingRuleClass=/Script/StealthPresentation.StealthDpiScalingRule`, with `ApplicationScale` left at 1.0. Editing `DefaultEngine.ini` is an ADR-pinned config change recorded against ADR-0021; do not touch ADR-0022-pinned keys.
- `FHudRootScaler` (plain helper) plus each layer root's scale container: every layer root created in story 006 gains a `UScaleBox` (user-specified scale) as its first child, set to `s_ui` from the client-local settings (story 052 supplies the settings hooks; until then read 1.0 from a settings stub that returns the MVP fixed value). The L0 leaf (story 011) multiplies glyph sizes by `s_ui` directly and divides by the leaf geometry scale so the DPI scale is applied once. `u * s_ui` is therefore applied exactly once.
- `s_ui` change at runtime: the root scaler observes the settings value each frame by value (apply-if-changed, story 002 helper) and calls `SetUserSpecifiedScale`; no relayout from a viewport resize is needed. On a window resize the engine re-queries the rule (new size, new cached value) and the layout follows, with no HUD code involved beyond not holding stale pixel values.
- Replay safety: nothing in the presenter or timelines may depend on viewport size or `s_ui`. Plate timelines are sampled from `now - t0` (story 016), so a resize cannot restart one; the `ResizeNoReplay` test proves cue and plate-start counters and each `t0` are unchanged across a resize and across an `s_ui` change, including a resize during a card entrance (a story-025 element; until that exists use a plate or the cooldown sweep).

Spy for `SetApplicationScale`: the story 004 lint forbids the call statically; this story adds a runtime spy in the test (a counter on a thin wrapper, or a check that `FSlateApplication` application scale equals its value before the test) so a call by a dependency would be caught.

Trap list: (1) `s_ui` squared (applied in the rule and the root) is the classic bug; AC1 asserts DPI x `s_ui`, not `DPI * s_ui^2`. (2) The engine's per-size cache means an editor property edit resets it but a game does not; test in a game process too (see the spike). (3) The 18/22/28 px floors scale with `H/1080` and ignore `s_ui` (HUD-R16), so a smaller `s_ui` shrinks plates but never text below its floor; plates grow to fit (F1). (4) PIE must not scale the editor UI (`SetApplicationScale` is process-wide, E5).

Decisions that apply: D16 (text widths re-measured after the face is chosen, story 017). Out of this story: layout evidence at real resolutions and the area budget on real widgets (story 056), settings UI (VS), the L0 leaf (story 011).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-008-spike-ui-v4-dpi-scaling-rule.md`: the spike this story waits on.
- `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`: the leaf's own `s_ui` multiplication and constant-pixel painting.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: measured layouts, captures and the real-widget budget.
- `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`: the client-local `s_ui` and `s_text` settings hooks.
- `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`: the resolver used as the oracle.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Scaling/StealthDpiScalingRule.h`
- `Source/StealthPresentation/Private/Hud/Scaling/StealthDpiScalingRule.cpp`
- `Source/StealthPresentation/Private/Hud/Scaling/HudRootScaler.h`
- `Source/StealthPresentation/Private/Hud/Scaling/HudRootScaler.cpp`
- `Source/StealthPresentation/Private/Hud/Widgets/HudLayerWidgets.cpp` (edit: the scale container as each root's first child)
- `Config/DefaultEngine.ini` (edit: `[/Script/Engine.UserInterfaceSettings]` `UIScaleRule` and `CustomScalingRuleClass`)
- `Source/StealthPresentation/Private/Tests/HUD/DpiRuleTest.cpp` (class `FHudDpiRuleTest`, name `StealthGame.HUD.DpiRule`)
- `Source/StealthTestHarness/Private/HUD/ResizeNoReplayTest.cpp` (CQTest, `NETWORK_TEST_CLASS(ResizeNoReplay, "StealthGame.HUD.PIE")`, full name `StealthGame.HUD.PIE.ResizeNoReplay.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: DPI rule and single `s_ui` application
  - Given: sizes 1280x720 to 4320p, `s_ui` = 1.5.
  - When: `GetDPIScaleBasedOnSize` and each layer root's effective scale are read.
  - Then: DPI = `Size.Y/1080`. Root and leaf scale = DPI x `s_ui` (not squared). `ApplicationScale` = 1.0. A `SetApplicationScale` spy shows 0 calls.
  - Edge cases: `s_ui` changed from 1.5 to 0.75 at runtime. Blocked until spike 008 reports.
- **AC-2**: `ResizeNoReplay`
  - Given: a plate showing and the cooldown sweep running.
  - When: the window is resized, and separately `s_ui` is changed.
  - Then: every rect equals the resolver output for the new values, and cue-event and plate-start counters and `t0` values are unchanged.
  - Edge cases: resize during a card entrance.
- **AC-3**: floors at real scales
  - Given: `s_ui` 75/100/150 %.
  - When: effective sizes are read through the resolver.
  - Then: sizes >= 18/22/28 x H/1080.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/HUD/ResizeNoReplayTest.cpp` (`StealthGame.HUD.PIE.ResizeNoReplay`, CQTest PIE) and `Source/StealthPresentation/Private/Tests/HUD/DpiRuleTest.cpp` (`StealthGame.HUD.DpiRule`) OR a documented playtest
- Gate: BLOCKING (Integration default). Blocked until spike UI-V4 (story 008) has reported.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`, `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`, `production/epics/hud/story-008-spike-ui-v4-dpi-scaling-rule.md`
- Unlocks: `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`, `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`
