# Story 017: Shared visual system: art-director display face, plate style, contrast and high contrast

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-006`, `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§4 dumb widgets and string tables; §1 layers), with art bible section 7.4 for the type scale
**ADR Decision Summary**: Widgets are dumb: each layer widget receives its view-model sub-struct and applies it (visibility, text, brush, render opacity); no UMG property bindings, no Blueprint graph logic and no widget animation assets, so widget Blueprints carry layout and style only. All player-facing text comes from a string table (`ST_Hud`) and dynamic values are formatted locally. Every layer widget applies a value only when it differs from the last applied one.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 verification item UI-V4 (DPI) governs the pixel sizes measured here: the 18/20/22/28 px floors are 1080p-equivalent and scaled by `H/1080` through `UStealthDpiScalingRule` (story 009), never lowered by UI scale, and the DPI rule result is cached per viewport size (E5), so capture at the 1080p viewport and at the 200 % text scale from a fresh viewport size. The display face is a licensed font asset imported into the project: confirm its licence permits embedding in a shipped game before import and keep the licence text with the evidence. Capture uses the unified Common UI / Enhanced Input build of 5.8.4 but touches no input. Use `UE_LOGF` for any measurement logging in C++.

**Control Manifest Rules (this layer)**:
- Required: Widgets are dumb (no UMG property bindings, no Blueprint logic beyond layout and style); `FHudViewModel::Dump()` is deterministic — source: ADR-0021
- Forbidden: Never port the prototype `AStealthHUD` drawing code — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This is the first UI implementation story that draws pixels, so it carries three decisions that every later UI story inherits and that D16 explicitly deferred to it: which display face the HUD uses (HUD-UX-8, chosen by the art-director, not by the programmer), the shared plate style (Obsidian Ink opaque plate, stepped 4 px corners, 2 px stroke, no shadow, gradient or glow), and the measured contrast and effective-size floors (HUD-R16). It creates style assets and the measurement procedure, not any cluster widget: the brace indicator, tally, plates and card (stories 019, 021, 022, 025 and later) are built on the style asset this story ships, so they cannot drift from it.

---

## Acceptance Criteria

*From `design/ux/hud.md` (UXA-HUD-22, UXA-HUD-25), GDD `design/gdd/hud.md` (HUD-R16, decision D16) and the art bible 7.4, scoped to this story:*

- [ ] **AC1** (HUD-UX-8 deferred by D16, manual check, `production/qa/evidence/hud-visual-system-1080p.png`): **GIVEN** the art-director picks a licensed display face recorded in the UX spec open-question table **WHEN** strings of 27, 48 and 43 characters are rendered in the stage-card title, line and clue widgets at 1080p/100 % and 200 % text **THEN** there is no wrap or truncation in any widget, the text widths are re-measured, and the re-run `LayoutResolver` test (story 007) is green with updated constants.
- [ ] **AC2** (UXA-HUD-22, manual check, contrast log in `production/qa/evidence/hud-visual-system-evidence.md`): **GIVEN** every text element captured at 1080p **WHEN** text and plate fill pixels are sampled **THEN** text below 28 px has at least 4.5:1 contrast on its plate and larger text and graphics at least 3:1 (Bone Dim included); effective sizes are at least 18 px (20 px for actionable text = prompts, strike hint, rejection plate and brace label; 22 px for changing numerals; 28 px for Cochineal Bright text) at 1080p-equivalent; contrast is the WCAG ratio between sampled text pixels and the plate fill pixel, logged per element.
- [ ] **AC3** (UXA-HUD-25, manual check, `production/qa/evidence/hud-high-contrast-1080p.png`): **GIVEN** High contrast on **THEN** every stroke and keyline (plate stroke, keylines, ring tracks, empty-square outlines, brackets, empty-step outlines; the marker keyline excepted) goes from 2 to 3 px inward without changing any footprint rect, Bone Dim is promoted to Bone, and the view-model dump shows the same facts as with it off.

---

## Implementation Notes

*Derived from ADR-0021 Decision §1 and §4, GDD HUD-R16, art bible 7.2 and 7.4, and the UX spec "Shared visual system" table:*

**What to build.** (1) A plate style asset class, `UStealthHudPlateStyle` (a `UDataAsset` in `Source/StealthPresentation/Private/Hud/Widgets/`), holding: plate fill Obsidian Ink `#1C1917` 100 % opaque, 2 px stroke, stepped 4 px corners (two steps), no shadow, gradient or glow; the palette (Bone `#EFE8D6`, Bone Dim `#B9B09C` for 18 px labels only, Marigold `#E3A72F`, Cochineal Bright `#E04A62` on dark for glyphs, text never below 28 px, Cochineal Deep `#B01E3C` for bars with Bone text, Obsidian Raised `#2B2622` for the own objective bar; Turquoise only on ally chrome); the keyline rule (2 px Obsidian outside every coloured glyph; 3 px on the marker and the ping, 4 px beyond 20 m for the ping); the type scale (Display 48 and Heading 28 in the display accent with a functional Semibold fallback, Primary 22 Semibold tabular, Body 20 Regular, Minimum 18 Regular with Bone Dim allowed; tracking +2 %, line height 1.3; no Light or Thin; text only on an opaque plate); and the rendering rules (flat, resolution-scaled, opaque materials, nearest filtering, no mips, no bloom or emissive, drawn after post-process). (2) The high-contrast variant as a second set of values in the same asset (stroke 3 px inward, Bone Dim mapped to Bone), selected by the client-local `bHighContrast` flag that the presenter already carries in the settings snapshot (story 052 wires the setting; this story reads a flag in `FHudInputs`, which is off in MVP, and exposes a test hook to turn it on). (3) The measurement procedure for UXA-HUD-22 and UXA-HUD-25, implemented as a repeatable capture script or test-only command that logs, per element, sampled text colour, plate fill colour, WCAG ratio and effective pixel height.

**The display face (HUD-UX-8, D16).** The programmer does not choose it. The art-director picks a licensed face and records it in the UX spec open-question table (`design/ux/hud.md`, item HUD-UX-8); that edit is the art-director's, outside this story's code. After import, re-measure the text widths: `FHudLayoutResolver` (story 007) holds the string-width budgets (title at most 27, line at most 48, clue at most 43 characters at 100 %) and the 8 % cap arithmetic of F1 (D16: reserved plate rectangles, cap 8 %, Self-Active 7.12 %, informant 6.68 %); re-run the story 007 `LayoutResolver` test with the updated width constants and confirm the cap still holds. If the chosen face is wider than the budget allows, the fix is a layout-resolver change reviewed with the UX owner, not a smaller font (floors are fixed by HUD-R16).

**Floors and contrast (HUD-R16, ruling D7).** Any text 18 px; text the player acts on 20 px (prompts, the strike hint, the rejection plate and the brace label); changing numerals 22 px; Cochineal Bright text 28 px, all at 1080p scaled by `H/1080` and never lowered by UI scale. Text below 28 px needs at least 4.5:1 on its plate; larger text and non-text graphics at least 3:1, Bone Dim included. Expected ratios from the art bible (Bone about 14:1, Bone Dim about 8:1, Marigold about 8:1, Cochineal Bright about 4.4:1 for graphic use only, Bone on Cochineal Deep about 5.5:1 to be re-measured on the real plate, art bible OI-3): the measured numbers go in the evidence log, and any element under its threshold is a defect against the style asset, not an exception.

**What gets measured now.** The cluster widgets mostly do not exist yet: measure the style asset on a plate fixture and on whichever persistent elements stories 005-007 and 011 produced (the string table of story 005 supplies the strings, the layout of story 007 supplies the rectangles). Each later UI story re-runs the contrast log for the elements it adds and appends to `hud-visual-system-evidence.md`; story 054 does the final pass over every element. The widths for the 27/48/43 character fixtures are test strings of those lengths, not the shipped copy.

**Traps.** (1) Contrast is measured on captured pixels (TAA off, neutral tonemap), not computed from the palette values, because anti-aliasing and the stepped corners change sampled pixels; sample the text stroke centre and the plate fill pixel adjacent to the glyph. (2) High contrast must not move any footprint: strokes grow inward only, and the dump facts must be equal with the flag on and off (`FHudViewModel::Dump()` carries no style data, so equality of dumps is the proof). (3) Plate width for text plates is computed from the widest string of the active locale, never from the shown string, so a style change cannot make a plate outcome-dependent (HUD-R3); keep that rule in the style asset's size helper. (4) Do not create widget animation assets for any style transition.

**Prototype.** The prototype canvas HUD (`Source/StealthGame/Private/StealthHUD.cpp`, 281 + 40 lines) has fonts, colours and rectangles hard-coded in `DrawText`/`DrawRect` calls: none of it is read or promoted. Removal is story 059.

**Out of this story:** every cluster widget, the settings screen that sets High contrast and text scale (VS; story 052 adds the hooks), localisation of the display face, the DPI rule itself (story 009).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-005-hud-string-table-and-language-lint.md`: the string table and the language lint (consumed here for test strings and labels).
- `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`: the layout resolver and area budget (re-run here with new width constants).
- `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`: `UStealthDpiScalingRule` and `s_ui`.
- `production/epics/hud/story-019-hud-brace-indicator-widget.md` and `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`: the first cluster widgets built on this style.
- `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`: the settings hooks (scale, Reduce motion, High contrast) that drive the flags used here.
- `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`: the final contrast pass over every shipped element.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudPlateStyle.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudPlateStyle.cpp`
- `Source/StealthPresentation/Private/Hud/HudLayoutResolver.cpp` (updated width constants for the chosen face, with the story 007 test re-run)
- `Source/StealthPresentation/Private/Tests/HUD/LayoutResolverTest.cpp` (story 007's test, constants updated, not a new test)
- `Source/StealthPresentation/Private/Hud/HudContrastProbe.h`
- `Source/StealthPresentation/Private/Hud/HudContrastProbe.cpp` (capture-sampling and WCAG-ratio logging used for the evidence log; compiled out of Shipping)

The display font asset, the plate style data asset and the plate fixture widget are named in prose only; no `Content/` paths are planned here.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Display face and string fit
  - Setup: the art-director picks a licensed face, recorded in the UX spec open-question table. Render stage-card strings of 27, 48 and 43 characters (title, line, clue) at 1080p/100 % and 200 % text.
  - Verify: no wrap or truncation in any widget. Re-run the story 007 `LayoutResolver` test with updated constants.
  - Pass condition: the face is recorded, all strings fit at both scales, and the 007 test is green.
- **AC-2**: Contrast and effective sizes
  - Setup: capture every text element at 1080p and sample text and plate pixels.
  - Verify: the WCAG ratio is >= 4.5:1 for text below 28 px and >= 3:1 for larger text and graphics (Bone Dim included). Effective sizes are >= 18 px (20 for prompts, strike hint, rejection plate and brace label; 22 for changing numerals; 28 for Cochineal Bright text).
  - Pass condition: the log in `hud-visual-system-evidence.md` shows every element meeting its threshold.
- **AC-3**: High contrast
  - Setup: capture and dump with High contrast off and on.
  - Verify: plate stroke, keylines, ring tracks, empty-square outlines, brackets and empty-step outlines go from 2 to 3 px inward (marker keyline unchanged). Footprint rects are equal. Bone Dim is promoted to Bone. The dump facts are equal.
  - Pass condition: `hud-high-contrast-1080p.png` retained and all checks hold.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots `production/qa/evidence/hud-visual-system-1080p.png` and `production/qa/evidence/hud-high-contrast-1080p.png`, the contrast measurement log `production/qa/evidence/hud-visual-system-evidence.md`, and the sign-off `production/qa/evidence/hud-shared-visual-system-font-face-and-plate-style-evidence.md` (records the chosen face and its licence). Gate level BLOCKING (UI evidence is not waived at `qa.level: minimal`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-005-hud-string-table-and-language-lint.md`, `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`, `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`
- Unlocks: `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`, `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`, `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`, `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`, `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`, `production/epics/hud/story-047-hud-latency-icon.md`, `production/epics/hud/story-048-hud-interact-ring-and-context-prompt.md`, `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`
