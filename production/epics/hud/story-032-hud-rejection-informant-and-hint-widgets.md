# Story 032: Rejection plate (L1, beside the brace row), informant plate, third-person hint and strike hint widgets

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-003`, `TR-hud-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: The rejection plate lives on L1 (`UStealthHudPersistentLayer`, ZOrder 30, ruling D19: ADR-0021 is authoritative); the Result slot (informant plate) and Notice slot (strike hint, third-person hint) live on L2 (`UStealthHudTransientLayer`, ZOrder 40). Each layer is a `UUserWidget` created once at HUD init, collapsed until needed, `HitTestInvisible`, and applies a view-model value only when it differs from the last applied one. DPI comes only from `UStealthDpiScalingRule` (`H/1080`) and `s_ui` is applied once by the layer roots.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 engine facts E1 (`AddToPlayerScreen` is the Player slot 100, below `AddToViewport`; never use `AddToViewport` for game UI) and E5 (a custom DPI rule result is cached per viewport size outside the editor, so `s_ui` and the text scale are applied by the layer root scalers, never inside the rule). Plate geometry is measured at four text scales (100, 125, 150, 200 %), so the plate's x must follow the bottom-left cluster's actual right edge from the layout resolver (story 007), not a constant. UI-V4 (DPI) is observed by story 008; this story does not depend on its report because it uses the resolver's output. The display face is the art-director's pick from story 017. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Layers below ZOrder 100 are `HitTestInvisible`, non-focusable, with no input bindings, and never host a button or a `UCommonActivatableWidget`; a Session status overlay that needs buttons shows its text on L4 (display only) and pushes a Menu-root modal (`UStealthStatusActionModal`) through `UStealthMenuStack::PushMenu` — source: ADR-0021 (closeout 2026-10-25)
- Forbidden: Never create or destroy a widget during a round in response to gameplay — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story draws the four transient notices whose logic is in stories 030 and 031: the rejection plate, the informant plate, the strike hint and the third-person hint. The widgets are dumb; the placement rules are the hard part, because a rejection plate that overlaps the tally or the Notice slot at 200 % text is a real defect.

---

## Acceptance Criteria

*From UX spec `design/ux/hud.md` UXA-HUD-16 and UXA-HUD-35 (D19), and HUD-E14 to E17, scoped to this story:*

- [ ] **AC1** (UXA-HUD-16, UXA-HUD-35; D19; retained screenshots at 100, 125, 150 and 200 %): **GIVEN** a rejection plate shown at text scales 100, 125, 150 and 200 % at 1080p **WHEN** its rectangle is measured **THEN** the plate is on L1 beside the brace row (right of it, outside the persistent footprint), sits 10 px right of the bottom-left cluster's actual right edge (x = 418, 454, 496, 578 at the four scales, y = 966 on the label baseline, within 2 px) and overlaps neither the tally, the brace label plate nor the Notice slot.
- [ ] **AC2** (HUD-E14, E16, E17; retained screenshots of each plate): **GIVEN** the strike hint, the third-person hint and the informant plate shown **WHEN** the screen is captured **THEN** the strike hint "[bound glyph] Strike" and the third-person hint show in the Notice slot, the informant plate "Informants can only observe" shows in the Result slot with 400 ms / 2 s / 600 ms eased timing, there is no reticle and the centred 20 % x 20 % box is empty.

---

## Implementation Notes

*Derived from ADR-0021 §1 and §6, GDD HUD-R5 and HUD-R13, and UX elements E14, E15, E16, E17:*

**What to build.** Four dumb widgets (UMG Widget Blueprints with layout and style only, named in prose; the C++ base classes below) created once at HUD init and toggled by visibility, each applying a view-model sub-struct from `FHudViewModel` (stories 030 and 031 produce the content):

1. Rejection plate (E15) in `UStealthHudPersistentLayer` (L1, ZOrder 30): height 40, fit-to-text, Body 20 px; text "Not now", "Busy", "Too far" from `ST_Hud` (`Hud.Reject.NotNow`, `.Busy`, `.OutOfReach`). Position `(418, 966)` at 100 % text, i.e. the bottom-left cluster's actual right edge plus 10 px at every scale (the layout resolver's right edge, story 007, so 418 / 454 / 496 / 578 at 100 / 125 / 150 / 200 %). Timeline 400 ms / 1.5 s / 600 ms eased (sampled in C++ by story 016, applied as render opacity here). A quiet owner-only 2D tone accompanies it (cue routing is story 051).
2. Informant plate (E16) in `UStealthHudTransientLayer` Result slot (L2, ZOrder 40): "Informants can only observe" (`Hud.Informant.Plate`), 400 ms / 2 s / 600 ms. No sound.
3. Strike hint (E14) in the Notice slot: a plate 40 high centred in the slot, "[glyph] Strike" (`Hud.Hint.Strike`, Body 20 px, Bone); the glyph plate is the shared bound-glyph widget from story 017/019 (shows "Unbound" when the action has no binding). No reticle, outline, highlight, lock-on, colour or sound that differs by target class; the screen centre stays empty.
4. Third-person hint (E17) in the Notice slot: "Third person shows only what your character can see." (`Hud.Hint.ThirdPerson`), one plate, Body 20 px, 4 s hold (story 031 decides when it shows and consumes the profile flag).

**Placement rules.** The rejection plate is on L1 (D19: it is not on L2, and the GDD's earlier "L2" wording is superseded by ADR-0021), outside the persistent footprint, and must overlap neither the tally nor the Notice slot, nor the brace label plate, at any of the four text scales. The persistent-area budget counts the reserved plate rectangles (F1, D16 cap 8 %, Self-Active 7.12 %); a transient plate beside the cluster is outside that budget by design, so do not grow the cluster footprint. Never reuse a persistent element's slot for a different element (no reflow).

**Layer discipline.** All four widgets are children of existing layers, are `HitTestInvisible`, not focusable, and never bind a button or click handler (a lint fails the build otherwise, ADR-0021 §1). Apply on change only: call `SetText`, `SetVisibility`, `SetRenderOpacity` only when the new view-model value differs.

**Text and strings.** All text from `ST_Hud` (story 005); dynamic values are formatted locally; nothing travels on the wire. The language lint (story 005) already covers these strings.

**Traps.**
- The brace label plate is wider at 200 % text; read the cluster's right edge from the resolver each frame rather than caching the 100 % value, because the DPI rule result is cached per viewport size (E5) but the text scale is a runtime setting applied by the layer root.
- A rejection plate replaced by a different code swaps in one step (no cross-fade); the limiter is story 031.
- The strike hint is level-triggered and may flicker if the widget animates; there are no widget animation assets (ADR-0021 forbids them).
- IIF `production/epics/input-interaction-framework/story-076-rejection-plate.md` (AC 67) verifies the plate behaviour on top of this widget; coordinate its `ClientCommandReplies` wiring but do not duplicate it.

**Prototype.** The prototype `Source/StealthGame/Private/StealthHUD.cpp` draws status text on the canvas; none of it is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-030-hud-strike-hint-predicate-and-lockout.md`: when the strike hint is wanted.
- `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`: slot priority, one-shot drops and limiters.
- `production/epics/hud/story-019-hud-brace-indicator-widget.md`: the brace row the plate sits beside.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: plate style, display face and glyph plate.
- `production/epics/hud/story-033-hud-returning-countdown-f7.md`: the countdown that shares the Notice slot.
- `production/epics/hud/story-034-hud-informant-presentation.md`: the informant view contract that shows the informant plate.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: the layout evidence across resolutions.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudPersistentLayer.h` and `.cpp` (rejection plate child; layer created by an earlier HUD story)
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudTransientLayer.h` and `.cpp` (informant plate, strike hint, third-person hint children)
- `Source/StealthPresentation/Private/Hud/Widgets/StealthNoticePlates.h` and `.cpp` (shared dumb plate base classes for the four notices)
- `Source/StealthPresentation/Private/Hud/HudViewModel.h` (notice sub-struct fields, if not already present from stories 030 and 031)
- `production/qa/evidence/hud-notices-100-1080p.png`, `hud-notices-125-1080p.png`, `hud-notices-150-1080p.png`, `hud-notices-200-1080p.png`, `production/qa/evidence/hud-rejection-informant-and-hint-widgets-evidence.md` (retained evidence, not code)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Rejection plate placement (Manual check)
  - Setup: show a rejection plate at text scales 100, 125, 150 and 200 % at 1080p.
  - Verify: plate x is the bottom-left cluster's actual right edge + 10 px (418, 454, 496, 578), y = 966 on the label baseline. It overlaps neither the tally, the brace label plate nor the Notice slot.
  - Pass condition: measured x within 2 px at all four scales, with no overlaps. Capture all four scales; the plan's Evidence names only 100 and 200 %, so the story must list all four.
- **AC-2**: Hints and informant plate (Manual check)
  - Setup: show the strike hint, the third-person hint and the informant plate.
  - Verify: the strike hint reads "[bound glyph] Strike" in the Notice slot. The third-person hint shows in the Notice slot. The informant plate "Informants can only observe" shows in the Result slot. There is no reticle, and the centred 20 % x 20 % box is empty. The informant plate timing is 0.4/2.0/0.6 s (frame-stepped, or by story 016's timeline test).
  - Pass condition: screenshots match and the centre box is empty.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots `production/qa/evidence/hud-notices-100-1080p.png`, `hud-notices-125-1080p.png`, `hud-notices-150-1080p.png` and `hud-notices-200-1080p.png` (the plan names 100 and 200 %; the QA spec requires all four scales), with sign-off in `production/qa/evidence/hud-rejection-informant-and-hint-widgets-evidence.md` (gate BLOCKING)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `production/epics/hud/story-030-hud-strike-hint-predicate-and-lockout.md`, `production/epics/hud/story-031-hud-notice-slot-priority-and-rate-limits.md`
- Unlocks: `production/epics/hud/story-034-hud-informant-presentation.md`
