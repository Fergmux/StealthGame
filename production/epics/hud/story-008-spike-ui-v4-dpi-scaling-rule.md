# Story 008: Spike UI-V4: custom DPI scaling rule and root scaler on 5.8.4

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`, `TR-hud-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (Verification Required UI-V4, engine fact E5)
**ADR Decision Summary**: One custom rule, `UStealthDpiScalingRule : UDPICustomScalingRule`, returns `ViewportHeight / 1080` only, so one Slate unit is one 1080p reference pixel. The client-local UI scale `s_ui` is applied by the HUD root (every layer's root is a scale container), never inside the rule, because the engine caches the rule's result per viewport size outside the editor, so a runtime change would not apply until a resize. `ApplicationScale` stays 1.0 and `FSlateApplication::SetApplicationScale` is never used (process-wide; it would scale the editor in PIE).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 UI-V4 (DPI): the custom scaling rule gives Slate DPI scale = `H/1080` at 720p to 4320p and in PIE without scaling the editor UI; the root scaler applies `s_ui` once and reacts to a settings change without a resize; the text floors hold (AC-HUD-15). Engine fact E5 [V] (read in the 5.8.4 source): `UUserInterfaceSettings` has `UIScaleRule` (`Custom`), `CustomScalingRuleClass`, `ApplicationScale`; `UDPICustomScalingRule::GetDPIScaleBasedOnSize(FIntPoint)` runs on the class default object; outside the editor its result is cached per viewport size (reset only by editor property edits); `ApplicationScale` is multiplied in and platform DPI is divided out by the game layer manager (`SGameLayerManager.cpp:491-495`). The cache behaviour and the PIE no-editor-scale claim are the [U] parts this spike settles. Config under test: `[/Script/Engine.UserInterfaceSettings] UIScaleRule=Custom`, `CustomScalingRuleClass=...`. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: DPI comes only from `UStealthDpiScalingRule` (`H/1080`); `s_ui` is applied once by the layer roots and the L0 leaf; `ApplicationScale` stays 1.0 — source: ADR-0021
- Forbidden: Never call `SetApplicationScale`, apply `s_ui` inside the DPI rule, or use the engine DPI curve — source: ADR-0021
- Guardrail: UI GPU ≤ 0.5 ms at 1080p on the reference GPU (HUD ≤ 0.3 ms CPU) — source: ADR-0021

A conditioning spike: story 009 (the production rule) is Blocked until this reports. The probe is throwaway and is not promoted; it lives in the test harness (non-Shipping), outside the HUD lint scope, so it may reference the engine DPI classes freely.

---

## Acceptance Criteria

*From ADR-0021 Verification Required UI-V4 and engine fact E5 (derived), scoped to this story:*

- [ ] **AC1** (derived, ADR-0021 UI-V4, spike probe and `production/qa/spikes/ui-v4-dpi-report.md`): **GIVEN** a throwaway `UDPICustomScalingRule` probe **WHEN** the viewport is 720p, 1080p, 1440p, 2160p and 4320p and in PIE **THEN** Slate DPI scale equals `Size.Y / 1080` within 1e-6 and the editor UI is not scaled.
- [ ] **AC2** (derived, ADR-0021 UI-V4 and §6, spike probe): **GIVEN** a root scaler applying `s_ui` **WHEN** the UI scale changes at runtime **THEN** the layout updates without a viewport resize (the rule's per-size cache does not hold a runtime factor) and `ApplicationScale` stays 1.0.
- [ ] **AC3** (derived, ADR-0021 UI-V4 and E5, report review): **GIVEN** the report `production/qa/spikes/ui-v4-dpi-report.md` **WHEN** it is reviewed **THEN** it states pass/fail per fact (E5 cache behaviour, `SetApplicationScale` process-wide) and, on 1080p, 1440p and 2160p, probe text at the 18/22/28 px floors measures floor x H/1080 +/- 0.5 px, with the three captures retained.

---

## Implementation Notes

*Derived from ADR-0021 §6 and Verification Required UI-V4:*

Build a throwaway probe in the test harness (`StealthTestHarness`, never in `StealthPresentation`):

- `UStealthSpikeDpiRule : UDPICustomScalingRule` returning `Size.Y / 1080.f` from `GetDPIScaleBasedOnSize`, set through `UUserInterfaceSettings` for the duration of the test only (record the original `UIScaleRule` and `CustomScalingRuleClass`, restore them in teardown; never leave project config changed, and never edit `Config/DefaultEngine.ini` from the probe). The production class and its config entry are story 009.
- A probe root widget with a `UScaleBox` (user-specified scale) driven by a settable `s_ui`, hosting a label at each text floor (18, 22, 28 px designed size at 1080p) and a 100 x 100 reference square. Drive viewport sizes by forcing the render-target / viewport size (720p, 1080p, 1440p, 2160p, 4320p); if the monitor cannot reach 4320p, force the render-target size rather than skipping the case. Read the Slate DPI scale from the game layer manager's geometry (`FGeometry::Scale` of the player-screen root) and compare with `Size.Y / 1080`.
- PIE case: run in a PIE window and assert (a) the PIE viewport scale equals `Size.Y / 1080` and (b) the editor UI scale is unchanged (read `FSlateApplication` application scale before and after; `SetApplicationScale` must not be called by the probe, and the report records that it is process-wide because a test that called it would also scale the editor, so the probe verifies the fact by reading the engine source and one non-invasive check, not by calling it).
- Runtime `s_ui` case: change `s_ui` 1.0 to 1.5 to 0.75 with no viewport resize and assert the square's painted size changes accordingly and `UUserInterfaceSettings::ApplicationScale` stays 1.0. This proves the rule's per-size cache does not hold the runtime factor (the factor lives in the root scaler).
- Text floor captures: at 1080p, 1440p and 2160p the probe labels at the 18/22/28 px floors measure `floor x H/1080` +/- 0.5 px (measure the glyph-box height from the capture or from the widget geometry plus the font measure service); retain the three captures as `production/qa/evidence/hud-ui-v4-dpi-1080p.png`, `hud-ui-v4-dpi-1440p.png` and `hud-ui-v4-dpi-2160p.png`.

Report `production/qa/spikes/ui-v4-dpi-report.md`: pass/fail per fact (E5 cache behaviour: does the rule result cache per viewport size outside the editor, and does a runtime `s_ui` change in the root scaler apply immediately; `SetApplicationScale` process-wide), the measured table per size, the engine build (5.8.4, CL 58902052), and, if a fact fails, the fallback: apply `s_ui` through a different layout-affecting root scaler, or drive the rule through an invalidated viewport size. A failed fact keeps story 009 Blocked until the ADR is amended; a pass unblocks it (the user or `/story-readiness` flips the status).

Traps: the editor caches the rule result differently from a game build (the cache resets on editor property edits), so run the runtime case in a standalone or cooked Development game process as well as in PIE and record both; do not conclude "the cache is fine" from PIE alone. TAA and screen percentage can blur the measured text box; capture with TAA off.

Prototype disposition: none (the prototype uses raw canvas pixels and no DPI rule). Out of this story: the production rule and the layer-root scaler (story 009), any HUD widget.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`: the production `UStealthDpiScalingRule`, its config entry and the layer-root scaler.
- `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`: the pure-math layout resolver.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: layout screenshots and budget evidence.
- `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`: the layer roots (this spike depends on them existing).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/Spike/SpikeUiV4DpiProbe.h` (throwaway probe: rule, probe widget, size driver; not promoted)
- `Source/StealthTestHarness/Private/HUD/Spike/SpikeUiV4DpiProbe.cpp`
- `Source/StealthTestHarness/Private/HUD/Spike/SpikeUiV4DpiTest.cpp` (CQTest, name `StealthGame.HUD.PIE.SpikeUiV4Dpi.*`)
- `production/qa/spikes/ui-v4-dpi-report.md` (the spike report; written by the spike, not a template)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: DPI scale equals `Size.Y / 1080`
  - Given: a throwaway `UDPICustomScalingRule` probe.
  - When: the viewport is 720p, 1080p, 1440p, 2160p and 4320p, and in PIE.
  - Then: Slate DPI scale equals `Size.Y / 1080` within 1e-6, and the editor UI is not scaled.
  - Edge cases: if the monitor cannot reach 4320p, force the render-target size.
- **AC-2**: runtime `s_ui` without resize
  - Given: a root scaler applying `s_ui`.
  - When: UI scale changes at runtime.
  - Then: the layout updates without a viewport resize, and `ApplicationScale` stays 1.0.
  - Edge cases: the rule's per-size cache must not hold the runtime factor.
- **AC-3**: report content
  - Given: the report `production/qa/spikes/ui-v4-dpi-report.md`.
  - When: it is reviewed.
  - Then: it states pass/fail for E5 cache behaviour and `SetApplicationScale` process-wide. On 1080p, 1440p and 2160p, probe text at the 18/22/28 px floors measures floor x H/1080 +/- 0.5 px, with three captures retained.
  - Edge cases: none.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report `production/qa/spikes/ui-v4-dpi-report.md` plus three retained captures in `production/qa/evidence/` (`hud-ui-v4-dpi-1080p.png`, `hud-ui-v4-dpi-1440p.png`, `hud-ui-v4-dpi-2160p.png`) and the probe test `Source/StealthTestHarness/Private/HUD/Spike/SpikeUiV4DpiTest.cpp`, with sign-off in `production/qa/evidence/spike-ui-v4-dpi-scaling-rule-evidence.md`
- Gate: BLOCKING (Integration default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`
- Unlocks: `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`
