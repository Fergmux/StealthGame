# Story 021: Tally widget: squares, numeral, danger triangle and informant frozen state

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
**ADR Decision Summary**: The tally plate lives on L1 (`UStealthHudPersistentLayer`, ZOrder 30, `HitTestInvisible`), created once at HUD init and toggled by visibility. The widget applies the tally sub-struct of `FHudViewModel` only when a value differs from the last applied one, and carries no logic, UMG property binding or widget animation asset: the stepped fill is a step index sampled by the presenter from `now − t0`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: UMG widget on the L1 layer added by `AddToPlayerScreen` (E1), `HitTestInvisible`, never a `UCommonActivatableWidget` (E6). Sizes are 1080p-equivalent and scale by `H/1080` through the DPI rule (UI-V4, story 009; the rule result is cached per viewport size, E5). Squares and the triangle are flat opaque brushes with nearest filtering and no mips (the shared rendering rule); the fill is four discrete brush steps, so nothing depends on widget animation timing. Capture with TAA off and neutral tonemap; a frame-stepped capture needs the 60 Hz fixed step. Use `UE_LOGF` for any native diagnostics.

**Control Manifest Rules (this layer)**:
- Required: Widgets apply a view-model value only when it differs from the last applied value — source: ADR-0021
- Forbidden: Never create or destroy a widget during a round in response to gameplay — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story puts the wrong-target tally (UX element E02) on screen: `L` stepped squares, the line "Wrong targets: {n}" and the static upward danger triangle, on the shared plate style (story 017). The values come from `FHudTallyModel` (story 020), whose timing guarantees the widget only ever changes at a plate's show time; the widget's job is to draw those values exactly, with the four-step fill, and to hold still when the owner is an informant.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (HUD-R7, F6) and `design/ux/hud.md` (UXA-HUD-06 and UXA-HUD-07 tally parts, E02), scoped to this story:*

- [ ] **AC1** (derived, F6 and E02, manual check, `production/qa/evidence/hud-tally-states-1080p.png`): **GIVEN** the tally **THEN** it shows L squares filling in 300 ms stepped with a one-step numeral change and the static upward danger triangle per F6; the limit is never written as a number; screenshots at n = 0, 1, 2 (triangle) and 3.
- [ ] **AC2** (UXA-HUD-06 and UXA-HUD-07 tally parts, ruling D4, manual check): **GIVEN** an informant **THEN** the squares and numeral stay frozen and visible without the triangle; UXA-HUD-07: an informant shows the frozen tally **without** the danger triangle, and the tally does not move.

---

## Implementation Notes

*Derived from UX spec E02, GDD HUD-R7, F6, D4 and ADR-0021 §1 and §4:*

**What to build.** `UStealthTallyWidget` in `Source/StealthPresentation/Private/Hud/Widgets/`, a child of the L1 persistent layer's tally slot (Z1, positioned by the layout resolver of story 007), with a C++ base `UStealthTallyBase::ApplyView(const FHudTallyView&)` that applies each field only on a difference. Structure per UX E02: one plate 244 x 80, fixed width regardless of `L` (so nothing reflows between presets). Row 1 (24 px): `L` stepped squares 24 x 24 with gap 6, left-aligned (`L` from 2 to 5, default 3), then the static upward danger triangle 24 x 24 at the row's right end, its space reserved even when absent. Row 2: the string `Hud.Tally.Line` ("Wrong targets: {n}"), Primary 22 px Semibold tabular. The limit is never written as a number. Squares are created for the maximum `L` (5) once at HUD init and shown or collapsed per `SquareCount`: the widget tree is fixed and never rebuilt during a round (a preset change, `L` latched per round, only changes visibility).

**Look.** Empty square: 2 px Bone outline. Filled: Cochineal Bright fill inside the same Bone outline, `min(n, L)` filled. Triangle: Cochineal Bright, 2 px Obsidian keyline, stepped base; visible from `n >= L - 1` while `PresentedOwnState = Active`, kept for `n > L`; static, never pulses; removed when the owner becomes an informant (D4). High contrast (story 017) thickens strokes inward without moving any rect.

**Update.** The value changes only at the show time of the plate that carries the increment (story 020 guarantees this): one square fills in a 300 ms stepped fill (four 75 ms steps, from `FillStepIndex` 0-4 in the view struct), the numeral changes in one step (no roll-up), one tally tick (cue stream, story 051) and a threshold tick once `n >= L - 1`. Reduce motion makes the fill instant (the timeline value arrives already quantised, story 016). A held re-push is never shown early and never lowers the value, which is the model's rule; the widget has no notion of either.

**As an informant.** Squares and numeral frozen at their last shown value as a record, no triangle, no animation: the view struct has `bFrozen` and the widget stops applying fill-step changes and hides the triangle slot. Frames across 2 s must be identical (the evidence run records them).

**Contrast and size.** The numeral line is a changing numeral: 22 px minimum, at least 4.5:1 on its plate; Cochineal Bright is graphics only here (about 4.4:1 against Obsidian is acceptable for graphics at 3:1). Story 017's probe logs the element; append it to `hud-visual-system-evidence.md`.

**Out of this story:** the tally logic, re-push gate and cache (story 020), the plate that triggers an increment (story 022), the leak parity PIE (story 027), the tally tick audio (story 051).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`: F6 values, the HUD-R7 hold, re-push gate and local store.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: the plate style and display face.
- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: the result plate that carries `TallyAfter`.
- `production/epics/hud/story-027-hud-tally-moves-with-plate-and-informant-parity-pie.md`: PIE proof that the tally moves with its plate and informant parity.
- `production/epics/hud/story-051-hud-cue-event-stream-and-owner-only-audio-hooks.md`: the tally and threshold ticks.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthTallyBase.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthTallyBase.cpp`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudPersistentLayer.cpp` (add the tally slot child and apply `FHudTallyView`)

The tally widget Blueprint (layout and style only) is named in prose only; no `Content/` paths are planned here. There is no automated test file: the evidence is the retained screenshots, the frame-step capture and the sign-off.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Tally widget states
  - Setup: L=3, Active, inject results to reach n=0,1,2,3. Capture at 1080p, plus a frame-stepped capture of one fill.
  - Verify: n=0 shows 0 filled squares (2 px Bone outline) and no triangle. n=1 shows 1 square filled Cochineal Bright. n=2 and n=3 show 2 and 3 filled plus the upward triangle. The fill runs in four 75 ms steps and the numeral changes in one step. The limit appears nowhere as a number.
  - Pass condition: all four captures match and the frame-step shows four steps.
- **AC-2**: Informant frozen tally
  - Setup: exile at n=3, then capture and record 2 s of frames.
  - Verify: 3 squares filled, "Wrong targets: 3", no triangle, no animation.
  - Pass condition: frames are identical across the 2 s.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshot `production/qa/evidence/hud-tally-states-1080p.png` (n = 0, 1, 2, 3 and the informant frozen frame), the frame-step capture of one fill, and the sign-off `production/qa/evidence/hud-wrong-target-tally-widget-evidence.md`. Gate level BLOCKING (UI evidence is not waived at `qa.level: minimal`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-020-hud-wrong-target-tally-logic.md`
- Unlocks: `production/epics/hud/story-034-hud-informant-presentation.md`, `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`
