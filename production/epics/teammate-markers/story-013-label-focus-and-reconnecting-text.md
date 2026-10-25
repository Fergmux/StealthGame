# Story 013: Label focus F8 and the reconnecting-suffix text

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

An ally's name appears as an opaque plate beside one marker at a time, chosen by focus: the marker whose anchor projects nearest the screen centre and within `TeammateLabelFocusSize` px of it (default 64 at 1080p), with hysteresis so the label does not flicker between two near-equal markers. The MVP has focus labels only (the `ShowTeamNames` hold moved to the Vertical Slice by user decision 2026-10-25). While the ally's team-only `bReconnecting` flag is true the label reads "{Name} · reconnecting"; the marker itself never changes (shape, fill and opacity never encode connection). The name comes from the public C-Public `DisplayName` looked up by `PublicHandle`, never through a pawn's PlayerState. This story is the pure focus and text logic; drawing the plate is story 022.

**GDD**: `design/gdd/teammate-markers.md` (TM-R11, F8)
**Requirement**: `TR-tmk-???` (TM-R11 has no registry entry; the registry should gain one)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (sections 4 and 5)
**Also governed by**: ADR-0001 (names come from the C-Public allowlist by handle)
**ADR Decision Summary**: All player-facing text comes from a string table (`ST_Hud`, key `Hud.Marker.Reconnecting`), never from data on the wire; names are the C-Public `DisplayName` by `PublicHandle`; label focus is part of `FTeammateMarkerModel`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `FText` formatting with a string-table key is pre-cutoff API; the model returns the key and name and the leaf formats, so the logic test stays engine-light.

**Control Manifest Rules (this layer)**:
- Required: Player-facing text comes from string tables; names are looked up from C-Public `DisplayName` by `PublicHandle` — source: ADR-0021
- Forbidden: Never carry text on the wire; never read a name through a pawn's PlayerState — source: ADR-0021, ADR-0001
- Guardrail: Looking around sends nothing to the server (no RPC, no property change)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-23 logic half, AC-TMK-26 label text, TM-R11, F8), scoped to this story:*

- [ ] **AC1** (AC-TMK-23 a, [U] `LabelFocusAndHold`): **GIVEN** three on-screen markers at 40, 58 and 120 px from the centre and no input held **THEN** only the 40 px marker is focused, edge markers never take focus, `TeammateLabelFocusSize` 0 disables labels, and the server receives no RPC or property change from looking around.
- [ ] **AC2** (AC-TMK-23 b): **GIVEN** focus on the 40 px marker **WHEN** a second marker moves to 36 px (4 px nearer) **THEN** it does not take focus; **WHEN** it moves to 30 px (at least `round(8 * u * s_ui)` = 8 px nearer at 1080p) **THEN** it does, with both labels live only during the overlapping fades.
- [ ] **AC3** (TM-R11, AC-TMK-26 label text): **GIVEN** an ally entry **THEN** the label text is "{Name}" or "{Name} · reconnecting" as a function of the `bReconnecting` flag alone (string-table key `Hud.Marker.Reconnecting`), the name is the C-Public `DisplayName` looked up by `PublicHandle` and never read through a pawn's PlayerState, a 40-character name and a duplicate name truncate with an ellipsis and keep their suffix, and the marker fill never changes with the flag.

---

## Implementation Notes

*Derived from F8 and TM-R11:*

`focus = argmin_i |q_i - c|` over on-screen markers with `|q_i - c| <= r_f * u * s_ui`; ties by smaller world distance, then lower `PublicHandle`; `q_i` is the unclamped on-screen glyph centre (edge markers never take focus); `r_f` is `TeammateLabelFocusSize` (0 to 200, default 64, 0 disables). Hysteresis: the focus moves from the current marker only when another is at least `round(8 * u * s_ui)` px nearer the centre or the current one leaves the radius; when the focus moves, the old label's exit and the new label's entrance overlap (the one permitted moment two labels are visible). Label placement (story 022) is the inward side of the marker, clamped inside the safe rectangle.

Text: the suffix is a function of `bReconnecting` alone (cross-review S-B1, ruling D23): it stays while the flag stays true (an `Active` ally killed while disconnected, an informant ally that remains disconnected, a superseding rejoin still loading) and is removed on any clear whatever the `ReconnectEndCause`; the label never reads "back" or "disconnected" (those are HUD feed lines) and exists only while a marker does. The HUD string `Hud.Marker.Reconnecting` is "{Name} · reconnecting" (design/ux/hud.md `ST_Hud`). The truncation rule and width cap are the label plate's (story 022); this story asserts that a name longer than the supplied width is ellipsised and that a duplicate name keeps Session's sanitised suffix.

**Known gap (accepted):** neither the GDD nor the UX spec gives the label's width cap in pixels, so the truncation test takes the cap as a test parameter; when the UX spec fixes a value it replaces the parameter.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-022-name-label-plate-rendering.md`: the plate, its timing and its placement.
- `story-031-ally-disconnect-marker-unchanged.md`: the multi-client disconnect scenarios.
- Vertical Slice: the `ShowTeamNames` hold (moved by user decision 2026-10-25).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerLabel.h` and `MarkerLabel.cpp` (focus, hysteresis, text selection)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerLabelFocusTest.cpp` (names `StealthGame.Markers.LabelFocusAndHold`, `StealthGame.Markers.LabelText`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: three on-screen markers at 40, 58 and 120 px from centre, no input. When: focus is evaluated. Then: only the 40 px marker is focused, edge markers never take focus, `TeammateLabelFocusSize` 0 disables labels, and a mock server channel records 0 RPCs or property changes. Edge cases: the nearest marker is an edge marker; a marker at exactly 64 px.
- **AC2**: Given: focus on the 40 px marker. When: a second marker moves to 36 px, then to 30 px. Then: it does not take focus at 36 px (4 px nearer); it does at 30 px (10 px nearer, threshold `round(8*u*s_ui)` = 8 at 1080p), and both labels are live only during the overlapping fades. Edge cases: exactly 8 px nearer.
- **AC3**: Given: an ally entry with `bReconnecting` false then true, a 40-character name, a duplicate name. When: the label is built. Then: text is "{Name}" or "{Name} · reconnecting" from the flag alone, the name comes from C-Public `DisplayName` by handle, an over-width name is ellipsised with its suffix intact, and the marker fill is unchanged by the flag. Edge cases: the flag clears with any `ReconnectEndCause`.

QL-STORY-READY: GAPS, accepted as written for the truncation width (no cap value exists in the GDD or UX spec; the test takes it as a parameter); AC1 and AC2 specs are final.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerLabelFocusTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-001-marker-tuning-keys-and-local-settings.md`, `story-002-marker-model-teamview-source-and-validation.md`, `story-010-marker-size-f1.md`; cross-epic `production/epics/participant-state-roster/story-051-pie-teammate-reconnecting-flag.md` (flag semantics)
- Unlocks: `production/epics/teammate-markers/story-022-name-label-plate-rendering.md`, `story-031-ally-disconnect-marker-unchanged.md`
