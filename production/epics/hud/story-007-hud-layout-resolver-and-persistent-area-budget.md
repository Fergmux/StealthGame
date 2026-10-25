# Story 007: Layout resolver (F1): safe rectangle, slot positions, text floors and the A_persist lint

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input §6 (the resolver is pure math; the DPI rule is story 009)
**ADR Decision Summary**: One custom rule returns `ViewportHeight / 1080` only, so one Slate unit is one 1080p reference pixel; `s_ui` is applied by the layer roots, and text size follows F1 `max(floor_px * H/1080, base_px * u * s_text)` so the 18/22/28 px floors ignore `s_ui`. The persistent-area lint (F1) runs at build time over the UX spec's footprints. This story builds the pure-C++ resolver those rules describe.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (plain C++ resolver over integers and floats; no UWorld/UObject; the module still builds against 5.8.4 / C++20). It mirrors ADR-0021 engine fact E5 (the DPI rule's result is cached per viewport size outside the editor, so `u = s_ui * H/1080` is split: `H/1080` in the rule, `s_ui` in the roots) without calling any engine DPI API; real scale values from the engine are exercised in story 009 after spike story 008 (UI-V4). `UE_LOGF` is preferred over `UE_LOG` in 5.8 for lint diagnostics.

**Control Manifest Rules (this layer)**:
- Required: DPI comes only from `UStealthDpiScalingRule` (`H/1080`); `s_ui` is applied once by the layer roots and the L0 leaf; `ApplicationScale` stays 1.0 — source: ADR-0021
- Forbidden: Never call `SetApplicationScale`, apply `s_ui` inside the DPI rule, or use the engine DPI curve — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

The resolver is the layout oracle: later widget stories take their rectangles from it, and story 056 compares real widget measurements and screenshots against it. Layout is authoritative here, not in a widget Blueprint.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-15 (Logic part) and `design/ux/hud.md` UXA-HUD-03, UXA-HUD-04, UXA-HUD-11, UXA-HUD-22, scoped to this story:*

- [ ] **AC1** (AC-HUD-15 Logic part, `StealthGame.HUD.LayoutResolver`): **GIVEN** the resolver and, for 1920x1080, 1920x1200, 2560x1440 and 3440x1440 at UI scale 75/100/150 % and text scale 100/200 % (24 cases), the Self-Active and informant element sets **WHEN** layouts are resolved **THEN** the style resolver's effective font sizes are >= 18 px (22 px changing numerals, 28 px Cochineal Bright; 20 px for text the player acts on, UXA-HUD-22) at 1080p-equivalent, no element rect crosses the safe rectangle, a centred 20 % x 20 % box holds no persistent element, the Result and Notice slots do not overlap at 200 % text, `A_persist` at 100 % and 16:10 is <= `HudPersistentAreaLimitPct`, and the lint `HudPersistentAreaLimitPct >= A_persist(16:10)` passes.
- [ ] **AC2** (UXA-HUD-03 and UXA-HUD-04, `StealthGame.HUD.LayoutResolver`): **GIVEN** 1080p at 100 % scales, then 3440x1440 and 5120x1440 **WHEN** zones are resolved **THEN** each zone rectangle is within 2 px of the Layout Zones table (bottom-left cluster `(108, 870)` to `(408, 1014)`; stage card `(730, 128)`, 460 x 92; feed `(1392, 114)`, 420 x 100; latency plate `(1772, 66)`; Result slot centre `(960, 756)`; Notice slot top `y = 800`); at 3440 x 1440 the bottom-left cluster's left edge is at x = 584 (within 2 px) and the informant brackets are at x = 172 (the full safe rectangle); at 5120 x 1440 the corner clusters stay inside the centred 16:9 region.
- [ ] **AC3** (UXA-HUD-11 and D16/D20, `StealthGame.HUD.LayoutResolver`): **GIVEN** the reference footprints (Self-Active 132,920 reference px2 and informant 124,600) at 1920x1200 **WHEN** `A_persist` is computed **THEN** it is 7.12 % for Self-Active and 6.68 % for the informant state at 16:10 (tolerance +/-0.005 points), the feed counts as one 420x100 box, and world-anchored markers and ping rings are excluded from `A_persist` and the centre-empty rule.

---

## Implementation Notes

*Derived from ADR-0021 §6, GDD F1 (`design/gdd/hud.md` lines 170-192) and UX spec Layout Zones (`design/ux/hud.md` line 124 onward):*

Create `FHudLayoutResolver` in `Source/StealthPresentation/Private/Hud/Layout/HudLayoutResolver.h/.cpp`, a plain C++ class with no engine world types. Inputs: viewport `(W, H)`, `s_ui`, `s_text`, `HudSafeMarginPct`, `HudPersistentAreaLimitPct` (client face, story 001), and the viewer's element set (Self-Active, informant, plus the transient slot occupants). Outputs: per-element rectangles in physical pixels and per-text-role effective font sizes.

F1 as specified: `u = s_ui * H / 1080`; `m = HudSafeMarginPct / 100`; `SafeRect = [m*W, (1 - m)*W] x [m*H, (1 - m)*H]` (96 x 54 inset at 1080p with the 5 % default); `y_result = 0.70 * H` (Result-slot centre); `y_notice,top = y_result + 0.5 * h_result,actual * u + g * u` with `h_result,actual` = 64 (one line, 73 at 200 % text per the UX spec) and `g = 12`; `size_text = max(floor_px * H / 1080, base_px * u * s_text)`; `A_persist = Sum_i(w_i * h_i * u^2) / (W * H)` at `s_ui = s_text = 1`, checked at 16:10, with the lint `HudPersistentAreaLimitPct >= A_persist(16:10)`. The floors are 18 px any text, 20 px actionable, 22 px changing numerals, 28 px Cochineal Bright, all scaled by `H/1080` and never lowered by `s_ui`. Each text role carries a `base_px` (UX spec type table: Display 48, Heading 28, Primary 22, Body 20, Minimum 18); the fixtures need one `base_px` per role.

Reference zones (1080p, origin top-left, UX Layout Zones): bottom-left cluster `(108, 870)`-`(408, 1014)` (12 px cluster inset inside the safe rectangle), stage card `(730, 128)` 460 x 92, feed `(1392, 114)` 420 x 100, latency plate `(1772, 66)`, Result slot centre `(960, 756)`, Notice slot top `y = 800`, centre box `[768, 1152] x [432, 648]`. Ultrawide: at 3440 x 1440 the corner clusters are clamped to a centred 16:9 region of the safe rectangle (bottom-left left edge x = 584) while the informant brackets hug the full safe rectangle (x = 172); at 5120 x 1440 the clusters stay inside the 16:9 region. Text scale 200 %: plates grow and never shrink text, a plate is capped at 60 % of the safe-rect width (the stage card at 40 % from 150 %) and wraps to at most 2 lines, and the Result plate grows to 450 x 73.

D16 convention (this story's core number rule): `A_persist` counts each plate's reserved rectangle, with the 3-line feed as one 420 x 100 box. Reference footprints: Self-Active 132,920 reference px2 and informant 124,600 (the informant has no brace indicator but adds brackets and the latency icon), giving 7.12 % and 6.68 % at 16:10 (1920x1200) and 6.41 % and 6.01 % at 16:9. D20: world-anchored markers, ping rings, the interact ring and the context prompt are outside the centre-empty rule and outside F1.

Traps: (1) assert the area cap only at 100 % scales; F1 lets `A_persist` exceed the cap at text scale >= 1.25 and on unsupported aspects (4:3, 5:4: no cap is enforced), never the centre-empty rule or the safe rectangle. (2) At `s_ui` 0.75 text stays at its floor while plates shrink, so plates size to their text (grow, never clip). (3) The 16:10 margin is thin (7.12 % against the 8 % default limit, about 0.9 points), so any new persistent element changes the lint result: the lint sums every persistent element of the worst state. (4) String budgets (stage card title 27, line 48, clue 43 characters) are inputs here; text widths are estimates until story 017 re-measures with the chosen display face (HUD-UX-8, D16).

Prototype disposition: replaces the prototype's hard-coded canvas positions in `Source/StealthGame/Private/StealthHUD.cpp`; nothing is promoted. Out of this story: real-widget measurement and screenshots (story 056), the DPI rule (story 009), the widgets themselves.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: real-widget measurement, screenshots of each case and the evidence-based budget.
- `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`: `UStealthDpiScalingRule` and the root scaler on real scale values.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: the display face and the text-width re-measure.
- `production/epics/hud/story-001-hud-tuning-keys-and-invariants.md`: registering `HudSafeMarginPct` and `HudPersistentAreaLimitPct`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Layout/HudLayoutResolver.h`
- `Source/StealthPresentation/Private/Hud/Layout/HudLayoutResolver.cpp`
- `Source/StealthPresentation/Private/Hud/Layout/HudLayoutFootprints.h` (reference footprints, zone table and `base_px` per text role as constants)
- `Source/StealthPresentation/Private/Tests/HUD/LayoutResolverTest.cpp` (class `FHudLayoutResolverTest`, name `StealthGame.HUD.LayoutResolver`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `LayoutResolver` matrix
  - Given: the resolver, viewports 1920x1080, 1920x1200, 2560x1440 and 3440x1440, UI scale 75/100/150 %, text scale 100/200 % (24 cases), Self-Active and informant element sets.
  - When: layouts are resolved.
  - Then: effective font >= the 18/22/28 px floor x H/1080. No rect crosses SafeRect. The centred 20 % x 20 % box holds no persistent element. Result and Notice slots do not overlap at 200 % text. `A_persist` at 100 % and 16:10 <= `HudPersistentAreaLimitPct`. The limit lint passes.
  - Edge cases: assert the area cap only at 100 % scales, since F1 lets it exceed the cap at text scale >= 1.25. The 20 px actionable floor (UXA-HUD-22) is included here. Fixtures need a `base_px` per text role.
- **AC-2**: zone rectangles
  - Given: 1080p at 100 % scales, then 3440x1440 and 5120x1440.
  - When: zones are resolved.
  - Then: each zone is within 2 px of the table: bottom-left (108,870)-(408,1014), stage card (730,128) 460x92, feed (1392,114) 420x100, latency plate (1772,66), Result slot centre (960,756), Notice top y=800. At 3440x1440 the bottom-left left edge is x=584 and the brackets x=172. At 5120x1440 the corner clusters stay inside the centred 16:9 region.
  - Edge cases: margin 5 % gives a 96x54 safe inset at 1080p.
- **AC-3**: `A_persist` convention
  - Given: the reference footprints (Self-Active 132,920 ref px2, informant 124,600) at 1920x1200.
  - When: `A_persist` is computed.
  - Then: 7.12 % and 6.68 % (tolerance +/-0.005 points). The feed counts as one 420x100 box, and world-anchored markers and rings are excluded.
  - Edge cases: 16:9 gives 6.41 % and 6.01 %. 4:3 and 5:4 are unsupported, so no cap is enforced.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/LayoutResolverTest.cpp` (`StealthGame.HUD.LayoutResolver`) — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)
- Gate: BLOCKING (Logic default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`
- Unlocks: `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`, `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`
