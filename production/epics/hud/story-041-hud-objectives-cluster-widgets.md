# Story 041: Stage card, own steps and rival steps widgets at the top-centre cluster

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-005` (cluster surfaces), `TR-hud-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering and Common UI
**ADR Decision Summary**: Every HUD layer is a UMG widget added with `AddToPlayerScreen` at its HUD-R4 ZOrder; the objectives cluster lives on L1 (ZOrder 30), `HitTestInvisible`, created once at HUD init and toggled by visibility. Widgets are dumb: each receives its `FHudViewModel` cluster and applies a value only when it differs from the last applied value, with no UMG property bindings, no Blueprint logic and no widget animation assets; step fills and the card turn are sampled in C++ from `now − t0`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0021 engine fact E1: `AddToPlayerScreen` widgets sit in the Player slot (100), below `AddToViewport` widgets (200), and a camera fade is applied inside the scene render and can never cover UMG, which is why the fade is its own HUD layer at ZOrder 20 (story 013) and L1 stays readable in the neutral view. This cluster has no world anchoring, so `ForceVolatile` is not used here (it belongs to the L0 leaf). Rects below are measured at 1080p where the DPI factor `H/1080` is 1.0; the DPI rule itself (UI-V4, story 008) is not exercised by this story. `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Widgets are dumb (no UMG property bindings, no Blueprint logic beyond layout and style); `FHudViewModel::Dump()` is deterministic — source: ADR-0021
- Forbidden: Never create or destroy a widget during a round in response to gameplay — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story draws what story 039 decides: the top-centre objectives cluster (UX E03, E04, E05) in its zone-table positions, in the shared visual system from story 017, with its first capture at the Self-Active worst case.

---

## Acceptance Criteria

*From UX spec `design/ux/hud.md` (E03, E04, E05, UXA-HUD-02) and GDD `design/gdd/hud.md` HUD-R11, scoped to this story:*

- [ ] **AC1** (HUD-R11, UX E03-E05, retained screenshot): **GIVEN** a Self-Active viewer at 1080p with every string at its budget **WHEN** the cluster is captured, with a step fill and a card turn also captured **THEN** the stage card (title, one line, clue "A court [ClueTag phrase]", Bone on Obsidian, 22 px title), own steps ("Your house", 3, Marigold fill) and rival steps ("Rival house", 2) sit in the zone table positions; a step fills in 500 ms and a counted completion turns the card in 500 ms.
- [ ] **AC2** (UXA-HUD-02 top-centre part): **GIVEN** 1080p at 200 % text and every other UXA-HUD-01 case **WHEN** the cluster is measured **THEN** the top-centre cluster ends at or above y = 432 and the centred 20 % x 20 % box holds no persistent element at any case.

---

## Implementation Notes

*Derived from ADR-0021 Decision §1 and §4 and `design/ux/hud.md` E03, E04, E05, Layout Zones:*

**What to build.** One C++ widget base class for the top-centre cluster, for example `UStealthHudObjectivesWidget` in `Source/StealthPresentation/Private/Hud/Widgets/` (a `UUserWidget` subclass whose Widget Blueprint carries layout and style only), created once at HUD init by the L1 root (story 006) and shown or collapsed from the `Objectives` cluster of `FHudViewModel` (story 039). It has three parts, each a dumb view of a view-model field:

- **Stage card (E03)**: one plate 460 x 92 at (730, 128) under the steps row, centred on x = 960. Three left-aligned lines: title (Primary 22 px Semibold), one line (Minimum 18 px, Bone Dim), clue "A court [ClueTag phrase]" (Body 20 px Bone). Budgets at 100 %: title at most 27, line at most 48, clue at most 43 characters; the screenshot uses strings exactly at these lengths and must show no wrapping or truncation. The turn-over takes `HudObjectiveStepSec` (500 ms): the old text fades over the first half and the new over the second, the plate unchanged; the widget applies the `turn_phase` the presenter samples from `now − t0`, it runs no widget animation asset. Never a waypoint, compass, arrow, distance read-out, map or world plate (OC-15).
- **Own steps (E04)**: plate 161 x 56 at (730, 66) on Obsidian Raised (`#2B2622`), label "Your house" (Minimum 18 px), exactly 3 steps of 45 x 18 with a 5 px gap; Marigold (`#E3A72F`) fill with a 2 px Obsidian keyline; empty steps a 2 px Bone Dim outline. Filled count from the view-model; the Final step fills only at `Ended` by the win, over `HudObjectiveStepSec`, as a 500 ms ease-out step fill.
- **Rival steps (E05)**: plate 108 x 56 at (1082, 66), right-aligned to the card, plain Obsidian, label "Rival house", exactly 2 steps of 36 x 14 with a 4 px gap. A rise of 2 in one update is one 500 ms fill with one tick (the tick cue is story 051's). Never a final step, a sub-stage fill or a time. No "n/5" label and no Turquoise bar. The empty gap between the two plates is deliberate: the rival bar is never adjacent to the own bar.

Positions come from the layout resolver (story 007) and the zone table, not literals in the widget; text uses the shared plate style and display face chosen in story 017 (the text widths are re-measured there, HUD-UX-8). Strings come from `ST_Hud` (`Hud.Step.Own`, `Hud.Step.Rival`); the stage card strings come from the objectives carrier (the clue phrase is owned by the objectives-chain epic and is not invented here). Apply a value only when it differs from the last applied value.

**Centre-empty rule (UXA-HUD-02).** At 1080p and 200 % text the cluster grows down and outward but must end at or above y = 432 and leave the centred 20 % x 20 % box (the central band of the screen) empty of persistent elements in every UXA-HUD-01 case. Measure this cluster's bottom edge in each UXA-HUD-01 case (the full resolution and aspect evidence set is story 056's) and record the measured values in the sign-off.

**Reduce motion.** Instant fills when Reduce motion is on (settings hook, story 052); the durations of holds are unchanged.

**Does not touch.** The F9 logic (story 039), the banner and its 600 ms cluster fade (story 042), the layout resolver and the 8 % area budget (stories 007 and 056), the layer roots (story 006). Prototype: none of `Source/StealthGame/Private/StealthHUD.cpp` is promoted; there is no prototype objectives cluster.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-039-hud-objectives-cluster-f9-logic.md`: what is filled and when.
- `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`: the banner, and fading this cluster out over 600 ms as it enters.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: palette, type, plate style, the display face.
- `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`: the zone rects and the area budget.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: the full resolution and aspect evidence set.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudObjectivesWidget.h`
- `Source/StealthPresentation/Private/Hud/Widgets/StealthHudObjectivesWidget.cpp`
- `Source/StealthPresentation/Private/Hud/HudLayerRoots.cpp` (register the objectives widget on the L1 root, created once at HUD init)

The Widget Blueprint `WBP_HudObjectivesCluster` (layout and style only) is authored in the editor and is not listed as a file here.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Manual check: top-centre cluster
  - Setup: Self-Active at 1080p/100 % with every string at budget (title 27, line 48, clue 43 characters). Capture, and capture a step fill and a card turn.
  - Verify: own plate (730,66) 161x56 labelled "Your house" with 3 steps in Marigold fill. Rival plate (1082,66) 108x56 labelled "Rival house" with 2 steps. Stage card (730,128) 460x92, Bone on Obsidian, 22 px title, clue "A court [ClueTag phrase]". A step fills in 500 ms and a counted completion turns the card in 500 ms.
  - Pass condition: rects within 2 px, no wrapping or truncation. The clue phrase comes from the objectives epic.
- **AC-2**: Manual check: centre-empty rule
  - Setup: 1080p at 200 % text, and every other UXA-HUD-01 case.
  - Verify: the top-centre cluster's bottom edge is at or above y = 432, and the centred 20 % x 20 % box holds no persistent element.
  - Pass condition: measured y <= 432 and the box is empty in all cases.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshot `production/qa/evidence/hud-objectives-cluster-1080p.png` (Self-Active, every string at its budget), with the step-fill and card-turn frames and the measured centre-empty values recorded in the sign-off `production/qa/evidence/hud-objectives-cluster-widgets-evidence.md`. Gate level: BLOCKING (the UI default; not waived at `qa.level: minimal`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`, `production/epics/hud/story-039-hud-objectives-cluster-f9-logic.md`, `production/epics/hud/story-007-hud-layout-resolver-and-persistent-area-budget.md`
- Unlocks: `production/epics/hud/story-034-hud-informant-presentation.md`, `production/epics/hud/story-054-hud-accessibility-captures-and-input-pass.md`, `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`, `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
