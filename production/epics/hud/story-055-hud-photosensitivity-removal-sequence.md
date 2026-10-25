# Story 055: Photosensitivity analysis of the exile and assassinated-victim sequences (AC-HUD-18)

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input
**ADR Decision Summary**: The HUD draws the screen fade as its own layer at ZOrder 20 from the alpha Camera publishes (Camera owns all fade timing), plates and cards sit above it at ZOrder 30 to 50, and UMG is composited after the scene and its post-process. The removal sequence a victim sees is therefore a composite of Camera's grade and fade, the result plate, the preempt, the removal card and the fade, which is exactly what the photosensitivity analysis must examine.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The capture must contain the final composited frame: the camera fade is applied inside the scene render and can never cover UMG (engine fact E2), so the HUD fade layer (UI-V3, story 012 and story 013) is what the viewer actually sees; the scene-only grade comes from Camera. Capture the PIE viewport at 60 Hz after composition. No new engine API is used. `UE_LOGF` is preferred over `UE_LOG` in 5.8 for the capture script's logging.

**Control Manifest Rules (this layer)**:
- Required: The screen fade is drawn by the HUD fade layer at ZOrder 20 from Camera's published fade — source: ADR-0021
- Forbidden: Never sample plate or card timelines from widget animation assets — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

A removal is the most violent thing the HUD shows: a grade, a plate, a preempt, a card and a fade follow within about two seconds. This story runs the photosensitivity analysis on the real sequences and retains the report and the capture script. At MVP the gate is ADVISORY, and it becomes BLOCKING at the Vertical Slice gate, so the script must be reusable.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-18, VA-HUD-4, OQ-NKP-6), scoped to this story:*

- [ ] **AC1** (AC-HUD-18, `production/qa/evidence/hud-photosensitivity-report.md`): **GIVEN** the exile and the assassinated-victim sequences (grade, plate, preempt, card, fade) **WHEN** analysed **THEN** they pass a photosensitivity analysis tool (for example PEAT or Harding FPA: ≤ 3 flashes/s, 10 % large-area luminance) at default settings and with Reduce screen effects; the report and tool name are retained.
- [ ] **AC2** (derived, `hud-photosensitivity-report.md`): **GIVEN** the report **WHEN** reviewed **THEN** it names the capture method (60 Hz capture of the PIE viewport with the HUD and Camera grade composited), lists flash count and large-area luminance per sequence, and the capture script is retained so the BLOCKING re-run at the VS gate reuses it.

---

## Implementation Notes

*Derived from GDD AC-HUD-18, art bible 7.6 rule 7 (photosensitivity budget) and ADR-0021 §3:*

**What to do.** Record the two sequences from the victim's viewpoint, each at default settings and with Reduce screen effects on, giving four captures: (1) exile, an NPC-kill removal after the third wrong target: Result plate "Target eliminated"/exile plate, the removal card preempt after `HudPreemptExitSec` (0.25 s), the card ("Informant", subtitle "Too many wrong targets") entering over `HudRemovalCardInSec` (0.6 s) after `HudRemovalCardDelaySec` (0.8 s, equal to `ExileBeatSec`, I-HUD-3), the card hold (4.0 s) and out (0.8 s), and Camera's fade; (2) assassinated victim: the strike resolves on the victim, the card ("Informant", subtitle "Eliminated for the round") at the F4 time, the exile `ViewRelease` and the fade. Use the real HUD from stories 022 to 026 and Camera's published fade through the HUD fade layer (story 013); do not substitute placeholder plates.

**Capture method.** Capture the PIE viewport at 60 Hz fixed step after the HUD and Camera grade are composited (for example the engine's high-resolution capture or a movie-render capture of the game viewport, or an external capture of the standalone window), as video or a frame sequence in the format the chosen tool ingests. Run PEAT or the Harding FPA (name and version in the report). Thresholds: no more than 3 flashes in any one second, and no large-area (the tool's area definition, roughly a quarter of the screen at 10 degrees of visual angle) luminance change beyond the 10 % general-flash threshold. Report per sequence and per setting: flash count, worst one-second window, large-area luminance change, pass or fail.

**Retention.** Keep the capture script in `tools/dev/` (for example `tools/dev/hud_photosensitivity_capture.ps1`) and name it in the report with its parameters (seed, resolution, fixed step), so the VS-gate re-run is a repeat, not a rewrite. Retain the report at `production/qa/evidence/hud-photosensitivity-report.md` (the file also serves as the sign-off).

**Gate.** ADVISORY at MVP (a failing analysis is recorded as a defect against the owning sequence story and does not block `/story-done`), BLOCKING at the Vertical Slice gate. Do not weaken the thresholds to pass; if a sequence fails, the fix belongs to the plate/card timing stories (016, 025) or to Camera's fade story, with HUD constants changed only through the GDD.

**Traps.** Reduce screen effects must be tested separately (it steps the ping exit and removes the soft grade/fade easing where specified), and the reviewer must check that Reduce motion alone is not used as a stand-in. Do not analyse an uncomposited capture (scene only): it would miss the HUD plate flashes.

**Decisions that apply.** D11 (removal wording on the card), D29 (plate wording "Target eliminated"), I-HUD-3 and I-HUD-6 (timings above). Out of this story: changing any timing, Camera's grade.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`: the fade layer the capture includes.
- `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`, `story-026-hud-removal-card-pie-timing.md`: the card, preempt and the timing proof.
- `production/epics/hud/story-016-hud-plate-timeline-f3-and-reduce-motion.md`: plate timelines.
- `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`: the Reduce screen effects setting hook.
- Camera System epic: the grade and fade timing itself (`production/epics/camera-system/story-017-view-state-neutral-view-fade.md`).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/dev/hud_photosensitivity_capture.ps1` (capture script, retained for the VS-gate re-run; planned)
- Evidence: `production/qa/evidence/hud-photosensitivity-report.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: photosensitivity analysis
  - Setup: capture the exile and assassinated-victim sequences (grade, plate, preempt, card, fade) at 60 Hz, at default settings and with Reduce screen effects. Run PEAT or Harding FPA.
  - Verify: flashes <= 3 per second and large-area luminance change within the 10 % threshold, per sequence and per setting.
  - Pass condition: the tool reports pass for all four runs, and the report and tool name are retained.
- **AC-2**: report content
  - Setup: write the report.
  - Verify: it names the capture method (60 Hz PIE viewport with HUD and Camera grade composited), lists flash count and large-area luminance per sequence, and keeps the capture script.
  - Pass condition: all three items are present in `hud-photosensitivity-report.md`. ADVISORY at MVP, BLOCKING at the VS gate.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: the retained report `production/qa/evidence/hud-photosensitivity-report.md` (tool name, per-sequence figures, capture method) and the capture script; there is no per-screen screenshot beyond what the report indexes
- Gate: ADVISORY at MVP, BLOCKING at the Vertical Slice gate (the plan's override of the UI default)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-013-hud-fade-layer-and-layer-order-test.md`, `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`, `production/epics/hud/story-026-hud-removal-card-pie-timing.md`
- Unlocks: None within this epic (see the EPIC Definition of Done)
