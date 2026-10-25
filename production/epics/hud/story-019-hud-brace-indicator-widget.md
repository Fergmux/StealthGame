# Story 019: Brace indicator widget with persistent label, bound glyph and cooling sweep

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§1 L1 persistent plates, §4 dumb widgets, "apply on change only")
**ADR Decision Summary**: The brace cluster lives on L1 (`UStealthHudPersistentLayer`, ZOrder 30, `HitTestInvisible`), created once at HUD init and toggled by visibility. The widget receives the brace sub-struct of `FHudViewModel` and applies it (visibility, text, brush, render opacity) only when a value differs from the last applied one; it holds no logic, no UMG property bindings and no widget animation assets.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: UMG widget on the L1 layer added by `AddToPlayerScreen` (E1: Player slot 100, below `AddToViewport`); the layer is `HitTestInvisible` and never a `UCommonActivatableWidget` (E6). The 56 px indicator and 22 px numerals are 1080p-equivalent and scale by `H/1080` through the DPI rule (UI-V4, story 009; the rule result is cached per viewport size, E5, so a viewport resize is needed to see a rule change in a capture). The glyph comes from the Enhanced Input binding query as a brush id; in 5.8 Enhanced Input and Common UI are unified, but the HUD reads glyphs through the project's local binding query and takes no Common UI input dependency. Capture with TAA off and neutral tonemap. Use `UE_LOGF` for any native diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Widgets apply a view-model value only when it differs from the last applied value — source: ADR-0021
- Forbidden: Never create or destroy a widget during a round in response to gameplay — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story puts the brace indicator on screen. Everything it shows is already decided by `FHudBraceModel` (story 018); the widget is the dumb applier on the shared plate style (story 017), bottom-left, 56 px, Bone and Bone Dim only (no signal hue) with a persistent short label "Brace" and the currently bound glyph beside it (ruling D3). It is also where the visual half of the "never announce an incoming strike" rule is checked: the Protected frame sequence must never shrink, narrow, close or flash, and no ready-again event may reach any viewer other than the owner.

---

## Acceptance Criteria

*From `design/ux/hud.md` (UXA-HUD-06 brace part), GDD `design/gdd/hud.md` (HUD-R12) and derived items, scoped to this story:*

- [ ] **AC1** (UXA-HUD-06 brace part, UX element E01, manual check, `production/qa/evidence/hud-brace-states-1080p.png`): **GIVEN** the 56 px Bone indicator with the "Brace" label and bound glyph **THEN** it shows Ready (empty outline), pending emphasis, Protected (inner stepped square plus 4 outer segments, no sweep) and Cooling (linear Bone Dim sweep plus "Brace in m:ss"); Pending shows a 3 px track outline with nothing filled; in the Protected frame sequence the filled-segment count never decreases and no element toggles visibility faster than 0.5 Hz (never closes, narrows or flashes); a retained screenshot of each state.
- [ ] **AC2** (derived, HUD-R12 and VA-HUD cue rule, manual check): **GIVEN** a device or binding change **THEN** the glyph swaps without re-animating, and an unbound action shows "Unbound" in the glyph plate; the ready-again tone appears only in the owner's cue stream (story 051) and a teammate's and an opponent's dumps and audio logs contain no ready-again event.

---

## Implementation Notes

*Derived from ADR-0021 §1 and §4, UX spec E01, HUD-R12, F5:*

**What to build.** `UStealthBraceIndicatorWidget` (a `UUserWidget`) in `Source/StealthPresentation/Private/Hud/Widgets/`, a child of the L1 `UStealthHudPersistentLayer`'s brace slot (Z1, bottom-left anchor by the layout resolver of story 007; the widget never positions itself). Structure per UX E01: a 56 x 56 opaque plate holding a circular hollow track (outer diameter 44 px, band 4 px: two concentric 2 px Bone outlines), and to its right a label plate 232 x 40 holding the glyph plate and the text, "[glyph] Brace", and while Cooling "[glyph] Brace in 0:43" (Primary 22 px Semibold tabular numerals, string `Hud.Brace.Cooling`). Layout and style only in the Blueprint; a thin C++ base class `UStealthBraceIndicatorBase` exposes `ApplyView(const FHudBraceView&)`, which compares each field with the last applied value and calls `SetText`, `SetVisibility`, `SetRenderOpacity` or a brush setter only on a difference. No Blueprint logic and no widget animation assets: the 250 ms stepped exit of the Protected square and the one-step Pending outline are produced by the presenter's timeline values (story 016), which arrive in the view struct as step integers.

**States, as drawn (UX E01 table).** Ready: empty hollow track, no inner square, label "Brace". Pending: the track outline thickens one step (2 to 3 px), nothing fills, no protection shown, on the press within one frame. Protected: the band fills in 4 stepped Bone segments (count from `ProtectedSegments`), a stepped 16 x 16 Bone square appears at once, no sweep drawn. End: band empties, the square removed with a 250 ms stepped exit. Cooling: the band fills linearly clockwise from 12 o'clock in Bone Dim (the only linear motion; Reduce motion quantises to 1 s steps through story 016) with the label "Brace in m:ss". The Protected frame sequence must be monotone: filled-segment count never decreases, and no element's visibility toggles faster than 0.5 Hz; the evidence frame log lists segment count and visibility per frame so this is checkable.

**Glyph and label.** The glyph plate follows the current device and binding through the `GlyphId` field and swaps in one step without re-running any entrance (no animation state on swap). An unbound action shows the string `Hud.Glyph.Unbound` ("Unbound") in the glyph plate. The label "Brace" is persistent in every state (ruling D3); Onboarding may hide it in Alpha, so give the base class a `bShowLabel` style flag, default true, that nothing sets in this story. Strings come from `ST_Hud` (story 005): no literal text in the Blueprint (the language lint covers the table, and the DataValidation audit covers widget text).

**Hidden.** When the view says `Hidden` (`PresentedOwnState = Informant`) the widget is collapsed and the slot stays empty; it is not destroyed (created once at HUD init).

**No cue to anyone else.** The ready-again tone is owner-only 2D UI-bus audio produced by the cue stream (story 051); the widget makes no sound and the view struct carries no ready-again event. Verify by attaching a teammate and an opponent client during the Cooling-to-Ready transition and checking their view-model dumps and audio logs for the absence of any ready-again entry (the dumps are deterministic, ADR-0021 §4).

**Colour and contrast.** Bone and Bone Dim only. Bone Dim is the Cooling sweep colour and is permitted for 18 px labels only; the label is 20 px actionable text (HUD-R16), so it stays Bone. Contrast is logged by story 017's probe for this element and appended to `hud-visual-system-evidence.md`.

**Prototype.** The prototype canvas HUD drew defence state and cooldown as text in `Source/StealthGame/Private/StealthHUD.cpp`; nothing from it is read or promoted (removal is story 059).

**Out of this story:** the state logic and F5 arithmetic (story 018), the plate style (story 017), the owner cue (story 051), accessibility captures at 200 % text and 16:10 (story 054, UXA-HUD-33 persistent-cluster screenshots in story 056).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-018-hud-brace-indicator-state-logic.md`: the state machine, numeral and sweep values.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: the plate style and display face this widget uses.
- `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`: the owner-only ready-again cue stream.
- `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`: High contrast, 200 % text and Reduce motion captures of this element.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: layout evidence at all resolutions.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthBraceIndicatorBase.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthBraceIndicatorBase.cpp`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudPersistentLayer.cpp` (add the brace slot child and apply `FHudBraceView`)

The brace indicator widget Blueprint (layout and style only) is named in prose only; no `Content/` paths are planned here. There is no automated test file: the evidence is the retained screenshots, the Protected frame log and the sign-off.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Brace states
  - Setup: drive Ready, a press (Pending), `FHoldStatus` (Protected), End and Cooling. Capture each at 1080p, and record the Protected frame sequence.
  - Verify: the 56 px plate shows the "Brace" label and bound glyph in every state. Ready is an empty hollow track. Pending has a 3 px track outline with nothing filled. Protected has the inner stepped square plus 4 segments and no sweep. Cooling is a Bone Dim linear clockwise sweep from 12 o'clock plus "Brace in m:ss".
  - Pass condition: all five captures match. In the Protected sequence the filled-segment count never decreases and no element toggles visibility faster than 0.5 Hz.
- **AC-2**: Glyph swap and ready-again cue
  - Setup: switch input device or binding mid-state, unbind the action, and fire ready-again with a teammate and an opponent client attached.
  - Verify: the glyph swaps without re-animating. Unbound shows "Unbound" in the glyph plate. The ready-again tone appears only in the owner's cue stream, and the teammate's and opponent's dumps and audio logs contain no ready-again event.
  - Pass condition: all three checks hold.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshot `production/qa/evidence/hud-brace-states-1080p.png` (one capture of each of the five states), the Protected frame log, and the sign-off `production/qa/evidence/hud-brace-indicator-widget-evidence.md`. Gate level BLOCKING (UI evidence is not waived at `qa.level: minimal`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-018-hud-brace-indicator-state-logic.md`
- Unlocks: `production/epics/hud/story-032-hud-rejection-informant-and-hint-widgets.md`, `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`, `production/epics/hud/story-058-hud-performance-capture.md`
