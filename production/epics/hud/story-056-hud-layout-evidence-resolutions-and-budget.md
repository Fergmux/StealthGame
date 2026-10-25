# Story 056: Layout captures at every resolution and scale, and the A_persist from the real widgets

> **Epic**: HUD
> **Status**: Blocked — BLOCKED: spike UI-V4 (story 008) has not reported
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (§6 DPI and scale, persistent-area lint)
**ADR Decision Summary**: One custom DPI rule, `UStealthDpiScalingRule`, returns `ViewportHeight / 1080` only, so one Slate unit is one 1080p reference pixel; `s_ui` is applied once by the layer roots and the L0 leaf; text size follows F1 with absolute floors that ignore `s_ui`. The persistent-area lint (F1) runs over the UX spec footprints at build time and the layout resolver computes `A_persist` against `HudPersistentAreaLimitPct`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Blocked on ADR-0021 verification item UI-V4 (the custom scaling rule gives Slate DPI scale `H/1080` at 720p to 4320p and in PIE without scaling editor UI; the root scaler applies `s_ui` once and reacts to a settings change without a resize; text floors hold), engine fact E5 (the custom rule's result is cached per viewport size outside the editor; `SetApplicationScale` is process-wide). The captures are meaningful only on the production rule from story 009 and only if the spike has passed; if it fails, the DPI mechanism changes and these captures are redone. 3440x1440 and 1920x1200 have different aspect ratios from 16:9, which the safe-rectangle rule (5 % of each dimension) and the 16:10 budget case cover. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: DPI comes only from `UStealthDpiScalingRule` (`H/1080`); `s_ui` is applied once by the layer roots and the L0 leaf; `ApplicationScale` stays 1.0 — source: ADR-0021
- Forbidden: Never call `SetApplicationScale`, apply `s_ui` inside the DPI rule, or use the engine DPI curve — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

Story 007 proves the layout resolver numerically from the UX footprints; this story proves the real widgets match the resolver at every supported resolution and scale, and that the persistent-area budget holds when computed from them.

---

## Acceptance Criteria

*From UX spec `design/ux/hud.md` (UXA-HUD-01, -02, -03, -11, -12) and GDD `design/gdd/hud.md` (AC-HUD-15 UI part), scoped to this story:*

- [ ] **AC1** (UXA-HUD-01 and AC-HUD-15 UI part, retained captures): At 1920 x 1080, 1920 x 1200, 2560 x 1440, 3440 x 1440, 1280 x 720 and 3840 x 2160, at UI scale 75, 100 and 150 % and text scale 100 and 200 %, no L1, L2 or L3 element crosses the safe rectangle (96 x 54 px at 1080p; 5 % of each dimension elsewhere). Screenshots retained.
- [ ] **AC2** (UXA-HUD-02 and UXA-HUD-03): At every case in UXA-HUD-01 the centred 20 % x 20 % box holds no persistent element, with every stage-card string at its budget (title 27, line 48, clue 43 characters); at 1080p and 200 % text the top-centre cluster ends at or above y = 432. At 1080p, 100 % scales, each zone rectangle is within 2 px of the Layout Zones table: bottom-left cluster `(108, 870)` to `(408, 1014)`; stage card `(730, 128)`, 460 x 92; feed `(1392, 114)`, 420 x 100; latency plate `(1772, 66)`; Result slot centre `(960, 756)`; Notice slot top `y = 800`.
- [ ] **AC3** (UXA-HUD-11 and UXA-HUD-12): `A_persist` computed by the layout resolver from the real widgets at 100 % scales and 16:10 is at most 8 % for the Self-Active state and for the informant state, and the lint `HudPersistentAreaLimitPct >= A_persist` passes (with the reference footprints it is 7.12 % and 6.68 %). No state shows more than 12 plate-class elements in L1 and L2, and a lower-centre column never shows more than two text blocks (Result and Notice); after a removal card entrance, exactly one text block is on screen in the central region once 0.25 s have passed.

---

## Implementation Notes

*Derived from GDD F1 (layout scale, safe rectangle, corner-area budget) and ADR-0021 §6:*

**What this story is.** Evidence over the shipped widgets, plus the glue that dumps real widget rectangles. It does not change layout constants; a failing case is a defect against the widget story that owns the element, or a GDD/UX change (a layout constant change is a creative-director and GDD matter, D16).

**Matrix.** 6 resolutions x 3 UI scales (75/100/150 %) x 2 text scales (100/200 %) = 36 cases. For each: set the viewport size, `s_ui` and text scale through the settings object of story 052, drive the HUD to a state that shows every L1/L2/L3 element at its worst size (Self-Active with brace cooling, a three-line feed with removal lines, a stage card at 27/48/43 characters, the Result slot with a two-line plate, the Notice slot, banner with summary), dump each element's resolved rectangle from the live widgets (the Slate geometry, not the resolver), capture a screenshot, and test containment in the safe rectangle `SafeRect = [m·W, (1 − m)·W] × [m·H, (1 − m)·H]` with `m = HudSafeMarginPct / 100` (5 %, 96 x 54 px at 1080p). L0 world-anchored elements and L4 are exempt from the containment rule (markers clamp to the safe rectangle by their own F3 rule).

**Centre box and zones (AC2).** The centred 20 % x 20 % box (centre of the screen) must contain no persistent element in any of the 36 cases; ping rings, markers and the interact ring/prompt are exempt (D20). At 1080p/100 % compare each zone rectangle with the Layout Zones table within 2 px: bottom-left cluster (108, 870)-(408, 1014), stage card (730, 128) 460 x 92, feed (1392, 114) 420 x 100, latency plate (1772, 66), Result slot centre (960, 756) (`y_result = 0.70 · H`), Notice slot top y = 800 (`y_notice,top = y_result + 0.5 · h_result,actual · u + g · u`).

**Budget (AC3).** `A_persist` is the sum of reserved plate rectangles of the persistent clusters (F1 counts reserved rectangles, D16: cap 8 %, `HudPersistentAreaLimitPct`) at 100 % scales and the narrowest supported aspect (16:10) for the Self-Active state and the informant state, computed by the layout resolver (story 007) fed with footprints measured from the real widgets (display face of story 017). Log the two figures against the reference 7.12 % (Self-Active) and 6.68 % (informant), and run the lint. Count plate-class elements in L1 and L2 per state (at most 12) and the text blocks in the lower-centre column (at most two: Result and Notice); trigger a removal card and verify that 0.25 s after entrance exactly one text block is in the central region (preempt, HUD-R13).

**Traps.** Text at 200 % grows clusters upward (bottom-left) and downward (top-right feed); check both at 3440x1440 where the safe margin is 5 % of each dimension (not 96 x 54 px). A rect that is inside at 100 % may cross at 150 % UI scale: that case is the point of the matrix. Capture from the production DPI rule (story 009), never an editor-scaled viewport. Re-run the matrix when story 017's face constants change.

**Decisions that apply.** D16 (reserved-rectangle area count, cap 8 %, display face from the first UI story, text widths re-measured there). Out of this story: the resolver itself (007), the DPI rule (009), settings (052), per-cluster widgets.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`: the numeric resolver and the lint.
- `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`: the DPI rule and root scaler.
- `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`: the settings that the matrix drives.
- `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `story-021-hud-wrong-target-tally-widget.md`, `story-041-hud-objectives-cluster-widgets.md`, `story-042-hud-end-banner-summary-called-off-lobby-widgets.md`, `story-047-hud-latency-icon.md`: the widgets being measured.
- `production/epics/hud/story-057-hud-menu-coexistence-and-l4-status-host.md`: menu clearance of the bottom-left cluster.
- `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`: the 2v2 screenshot set.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/HudWidgetRectDump.h` and `HudWidgetRectDump.cpp` (dumps resolved widget rectangles for the matrix; Development builds only)
- `tools/dev/hud_layout_matrix.ps1` (drives the 36 cases and captures; planned)
- Evidence: `production/qa/evidence/hud-layout-<resolution>-<scale>.png` (36 captures named per case) and `production/qa/evidence/hud-layout-budget-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: safe rectangle
  - Setup: capture at 1920x1080, 1920x1200, 2560x1440, 3440x1440, 1280x720 and 3840x2160, at UI scale 75/100/150 % and text 100/200 %, with a widget-rect dump per case.
  - Verify: no L1, L2 or L3 element crosses the safe rectangle (96x54 at 1080p, 5 % elsewhere).
  - Pass condition: every rect is inside in all 36 cases, and captures are retained.
- **AC-2**: centre box and zones
  - Setup: 1080p/100 % with every stage-card string at budget.
  - Verify: the centred 20 % x 20 % box holds no persistent element, and each zone rect is within 2 px of the zone table.
  - Pass condition: all measured within tolerance.
- **AC-3**: area budget
  - Setup: compute `A_persist` from the real widgets at 100 % and 16:10, for Self-Active and informant.
  - Verify: <= 8 % each (reference 7.12 % and 6.68 %), the lint passes, no state shows more than 12 plate-class elements in L1 and L2, and the lower-centre column never has more than two text blocks.
  - Pass condition: figures logged in `hud-layout-budget-evidence.md`. Blocked until spike UI-V4 reports.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots `production/qa/evidence/hud-layout-<resolution>-<scale>.png` for each of the 36 cases, plus sign-off and measurements `production/qa/evidence/hud-layout-budget-evidence.md`
- Gate: BLOCKING (UI default; not waived at `qa.level: minimal`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`, `production/epics/hud/story-009-hud-dpi-rule-and-root-scaler.md`, `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`, `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`, `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`, `production/epics/hud/story-047-hud-latency-icon.md`, `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`
- Unlocks: `production/epics/hud/story-058-hud-performance-capture.md`, `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
