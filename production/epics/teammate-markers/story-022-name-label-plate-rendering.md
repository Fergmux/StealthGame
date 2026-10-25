# Story 022: Name label plate rendering

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0021 verification item UI-V4 (text floors and DPI, owned by the hud epic) has not reported, and the plate is painted by the leaf of story 019 (itself Blocked on UI-V1, UI-V3, UI-V4).
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The ally name label is an opaque Bone-on-Obsidian plate beside one marker at a time (the focus marker of story 013): an Obsidian plate with stepped 4 px corners, Bone `#EFE8D6` text at 22 px (Primary, Semibold), never Turquoise, on the screen-inward side of its marker (right of a marker in the left half, left otherwise) and clamped inside the safe rectangle. It enters as a non-urgent HUD plate (400 ms fade with an 8 px stepped settle) and leaves over 600 ms; while the focus moves, the old label's exit and the new label's entrance overlap, the one permitted moment two labels are visible. Text scale grows the plate and never goes below 100 %; long names ellipsise at the plate's width cap, and duplicate names already carry Session's sanitised suffix. This is the UI evidence for UI-2 and the visible half of AC-TMK-23.

**GDD**: `design/gdd/teammate-markers.md` (TM-R11, VA-5, UI-2, Game Feel); `design/ux/hud.md` item (17b)
**Requirement**: `TR-tmk-???` (TM-R11 and UI-2 have no registry entry; nearest `TR-tmk-004`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (sections 2, 4 and 6)
**ADR Decision Summary**: Labels are painted by the L0 leaf (text for labels and the prompt); text size follows F1 (`max(floor_px * H/1080, base_px * u * s_text)` in physical pixels, converted by the geometry scale) so the 18/22/28 px floors ignore `s_ui`; strings come from string tables.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Text is drawn by the leaf with Slate font measurement (`FSlateFontMeasure`); the width cap and the settle are leaf-side. UI-V4 (text floors hold, AC-HUD-15) is the [U] check.

**Control Manifest Rules (this layer)**:
- Required: Player-facing text comes from string tables; the leaf draws labels in `OnPaint` — source: ADR-0021
- Forbidden: Never colour the label Turquoise; never sample plate timelines from widget animation assets — source: art bible 7.2 item 4, ADR-0021
- Guardrail: Labels are re-laid-out only when the name or the scale changes (GDD performance budget)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (VA-5, UI-2, AC-TMK-23) and `design/ux/hud.md` (17b), scoped to this story:*

- [ ] **AC1** (VA-5, UI-2): **GIVEN** a focused marker **THEN** an opaque Obsidian plate with stepped 4 px corners carries the name in Bone `#EFE8D6` at 22 px (never Turquoise) on the screen-inward side of the marker, clamped inside the safe rectangle, and a name longer than the plate's width cap ellipsises, keeping Session's sanitised duplicate suffix.
- [ ] **AC2** (UX 17b): **GIVEN** focus entering and leaving **THEN** the entrance is 400 ms with an 8 px stepped settle, the exit is 600 ms, both labels are visible only during the overlapping fades, and text scale grows the plate without going below its 100 % size.
- [ ] **AC3** (AC-TMK-23, [UI][Vis] evidence): **GIVEN** focus, a hysteresis handover, the reconnecting suffix and a truncated name **THEN** a retained screenshot of each is in `production/qa/evidence/` (`tmk-label-*.png`).

**Known gap (accepted):** the plate's width cap in pixels is not stated in the GDD or the UX spec (the UX spec says names "ellipsise at the plate cap"); AC1's truncation clause is verified against whatever cap the UX spec settles on, recorded in the evidence file.

---

## Implementation Notes

*Derived from VA-5, F8 placement and UX 17b:*

Plate: opaque Obsidian (`#1C1917`; the plate fill may use Obsidian Raised `#2B2622` per the UX shared visual system, whichever the art bible gives for plates), stepped 4 px corners, Bone text 22 px Semibold tabular. Placement from F8: the screen-inward side of the marker, clamped inside the safe rectangle. Timing: entrance as a non-urgent plate (400 ms fade, 8 px stepped settle; art bible 7.6), exit 600 ms; sampled from `now - t0` in C++ (ADR-0021 §4). The text is the string from story 013 (`{Name}` or `Hud.Marker.Reconnecting`). At text scale 200 % the plate grows with its text and never truncates except by ellipsising a name at the cap; geometry (the marker) keeps its 100 % size.

Evidence names: `tmk-label-focus-*.png`, `tmk-label-handover-*.png`, `tmk-label-reconnecting-*.png`, `tmk-label-truncated-*.png`; record the viewport, text scale and build in `production/qa/evidence/tmk-label-evidence.md`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-013-label-focus-and-reconnecting-text.md`: the focus choice and text selection.
- `story-019-paint-markers-in-world-anchor-leaf.md`: the leaf itself.
- Vertical Slice: the `ShowTeamNames` hold and the streamer alias.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerLabelPaint.h` and `MarkerLabelPaint.cpp` (plate drawing and settle)
- `Source/StealthPresentation/Private/WorldAnchor/StealthWorldAnchorLeaf.cpp` (calls it)
- `production/qa/evidence/tmk-label-*.png` and `production/qa/evidence/tmk-label-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Setup: a focused marker with a short name, a 40-character name and a duplicate name. Verify: plate fill, corner step, text colour and size, side of the marker, clamp inside the safe rectangle, ellipsis at the cap with the suffix kept. Pass condition: opaque Obsidian plate, stepped 4 px corners, Bone `#EFE8D6` 22 px, never Turquoise, inward side, inside the safe rectangle. (Gap: the cap value is taken from the UX spec once fixed.)
- **AC2**: Setup: focus enters and leaves, frame-stepped capture, with text scale above 100 %. Verify: entrance duration, 8 px stepped settle, exit duration, overlap of the two labels, plate growth. Pass condition: entrance 400 ms, exit 600 ms, both labels visible only during overlapping fades, and the plate never shrinks below the 100 % size.
- **AC3**: Setup: capture focus, a hysteresis handover, the reconnecting suffix and a truncated name. Verify: files present. Pass condition: one retained screenshot of each, `production/qa/evidence/tmk-label-*.png`.

QL-STORY-READY: GAPS, accepted as written for AC1's width cap (no cap value exists in the GDD or UX spec); AC2 and AC3 specs are final.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: a retained screenshot of each label state in `production/qa/evidence/tmk-label-*.png`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-013-label-focus-and-reconnecting-text.md`, `story-019-paint-markers-in-world-anchor-leaf.md`, `story-009-fade-draw-order-no-motion.md`; cross-epic: the hud epic's shared visual system and string table `ST_Hud` (not yet created)
- Unlocks: `production/epics/teammate-markers/story-031-ally-disconnect-marker-unchanged.md` (visible half)
