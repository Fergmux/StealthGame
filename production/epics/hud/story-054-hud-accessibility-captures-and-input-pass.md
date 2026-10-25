# Story 054: Greyscale and colour-vision captures plus the keyboard-only and gamepad-only round

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (with art bible 4.7, no meaning by colour alone)
**ADR Decision Summary**: Every layer below ZOrder 100 is `HitTestInvisible`, non-focusable, has no input bindings and is never a `UCommonActivatableWidget`; the in-round HUD has no input layer at all, so a round can be played on keyboard only or gamepad only with no HUD element capturing input. Colour is never the sole carrier of meaning; the shapes, glyphs, labels and fill states are the primary channel (art bible 4.7), which this story checks on captured pixels.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The colour-vision captures use the engine's colour-vision-deficiency simulation (protan, deutan, tritan, severity 1.0), applied to the captured viewport; UMG is composited after the scene (engine fact E2), so the simulation must be applied to the final composited frame, not only the scene, or HUD colours would be unsimulated: record in the evidence note how it was enabled. The input pass relies on the ADR-0021 input rule (no HUD focus, `IMC_Menu` only while a menu is open, Common UI for menus only, Common UI routing needs `UCommonGameViewportClient`, Enhanced Input / Common UI unified in 5.8). No UI-V item is required (UI-V2 menu edge safety is IIF story 011). `UE_LOGF` is preferred over `UE_LOG` in 5.8.

**Control Manifest Rules (this layer)**:
- Required: Layers below ZOrder 100 are `HitTestInvisible`, non-focusable, with no input bindings, and never host a button or a `UCommonActivatableWidget`; a Session status overlay that needs buttons shows its text on L4 (display only) and pushes a Menu-root modal (`UStealthStatusActionModal`) through `UStealthMenuStack::PushMenu` — source: ADR-0021 (closeout 2026-10-25)
- Forbidden: Never make a layer below ZOrder 100 a `UCommonActivatableWidget` — source: ADR-0021
- Guardrail: HUD: ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU (inside ADR-0022's frame budget) — source: ADR-0021

This is the manual accessibility pass for the HUD's visual states: after the widgets exist (stories 019 to 048) two reviewers prove from unlabelled captures that every state is distinguishable in greyscale and under colour-vision simulation, and a tester plays a round with each input family.

---

## Acceptance Criteria

*From UX spec `design/ux/hud.md` (UXA-HUD-23, UXA-HUD-29) and GDD `design/gdd/hud.md` (AC-HUD-15 UI part), scoped to this story:*

- [ ] **AC1** (UXA-HUD-23 greyscale, retained captures): from unlabelled greyscale captures two reviewers independently label every HUD state (marker fills, tally, triangle, result outcomes, ring states, steps, brace states, brackets, latency icon); pass requires both at 100 %.
- [ ] **AC2** (UXA-HUD-23 colour-vision and AC-HUD-15 UI part, retained captures): captures use the engine colour-vision-deficiency simulation (protan, deutan, tritan, severity 1.0); two reviewers label the unlabelled captures independently and no two semantic elements receive the same label (confused = same label for different elements); no meaning is carried by colour alone.
- [ ] **AC3** (UXA-HUD-29 manual half, `hud-input-pass-evidence.md`): a 2v2 PIE round is played to `Ended` once with keyboard only and once with gamepad only, with no HUD element taking focus or input.

---

## Implementation Notes

*Derived from art bible 4.7, UX spec Accessibility (A-01 to A-07, T-01, T-02) and ADR-0021 §1:*

**What this story is.** Evidence work, not new widgets. It adds one capture script and one evidence note, and files defects against the owning widget stories when a state fails (never fix inside this story unless trivial). It becomes executable once the cluster widgets it captures exist: brace indicator (story 019), tally (021), removal card (025), informant states (034), objectives cluster (041), end banner (042), latency icon (047), interact ring and prompt (048).

**Capture set.** For each HUD state, one capture at 1080p/100 % scales: marker fills (filled, hatch, outline) and ping ring states (from the leaf; story 049), tally squares, numeral and triangle (E02), the five result outcomes and the defence plate (E07/E08), own and rival steps (E04/E05), brace states (Ready, Cooling, Protected; E01), informant brackets (E16), latency icon (E19). Produce each as a greyscale capture and as protan, deutan and tritan captures at severity 1.0, strip every label from the file names and write the key separately; the reviewers see only the unlabelled images.

**Greyscale (AC1).** Reviewer A and reviewer B (two different people; the author of the widgets is not a reviewer) independently write a label for every capture. Pass is 100 % of labels correct for both. A miss is a defect against the owning widget story (typically a fill state distinguished only by hue).

**Colour-vision (AC2).** Same procedure per simulation. "Confused" means two different semantic elements given the same label by a reviewer; pass is zero confusions across protan, deutan and tritan. Also review the colour-only audit: list each element's non-colour carrier (shape, glyph, label, fill, position, stroke weight) in the evidence note; any element with no carrier fails (art bible 4.7, AC-HUD-15 UI part). Marigold ping rings, Turquoise ally chrome and Cochineal danger text are the palette entries most at risk; the text twins and stepped stroke weights are the intended carriers.

**Input pass (AC3).** Play a 2v2 PIE round to `Ended` twice on the same seed: keyboard and mouse only, then gamepad only. Observe that no HUD element is focused, highlighted or clicked, no HUD layer swallows a key, and the round completes. Record the build, seed, input device and result in `hud-input-pass-evidence.md`. This is DEFERRED until a full round is playable (needs the 2v2 session path); the automated half (no focusable layer, ZOrders 10/20/30/40/50/60) is story 006.

**Traps.** Applying the colour-vision simulation to the scene only leaves the HUD unsimulated (UMG is composited after the scene, E2): verify one capture where a known red HUD element visibly shifts. The greyscale conversion must use luminance, not a single channel. Reviewer labels are written before the key is revealed.

**Decisions that apply.** D16 (display face from story 017 affects legibility captures), D20 (world-anchored markers and rings exempt from the centre-empty rule, but not from this pass). Out of this story: the automated focus lint (story 006), contrast ratios (story 017), the text-scale and reduce-motion captures (story 052).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `story-021-hud-wrong-target-tally-widget.md`, `story-025-hud-removal-card-widget-and-preempt.md`, `story-034-hud-informant-presentation.md`, `story-041-hud-objectives-cluster-widgets.md`, `story-042-hud-end-banner-summary-called-off-lobby-widgets.md`, `story-047-hud-latency-icon.md`, `story-048-hud-interact-ring-and-context-prompt.md`: building the states that are captured.
- `production/epics/hud/story-006-hud-layer-stack-root-and-input-isolation.md`: the automated no-focus and ZOrder checks.
- `production/epics/hud/story-017-hud-shared-visual-system-font-face-and-plate-style.md`: contrast ratio measurement and high contrast.
- `production/epics/hud/story-052-hud-accessibility-settings-hooks.md`: scale, text scale and Reduce motion captures.
- `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`: the run-and-observe screenshot set.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/dev/hud_accessibility_captures.ps1` (capture script: greyscale and colour-vision runs, label-stripped file names; planned)
- Evidence: `production/qa/evidence/hud-greyscale-1080p.png`, `hud-protan-1080p.png`, `hud-deutan-1080p.png`, `hud-tritan-1080p.png`, `hud-input-pass-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: UXA-HUD-23 greyscale
  - Setup: capture every HUD state (marker fills, tally, triangle, result outcomes, ring states, steps, brace states, brackets, latency icon) in greyscale, unlabelled.
  - Verify: two reviewers independently label every state from the captures.
  - Pass condition: both reviewers label 100 % correctly. `hud-greyscale-1080p.png` retained.
- **AC-2**: UXA-HUD-23 colour-vision
  - Setup: capture the same states with the engine colour-vision-deficiency simulation (protan, deutan, tritan, severity 1.0), unlabelled.
  - Verify: two reviewers label the captures independently, and review the colour-only audit.
  - Pass condition: no two semantic elements receive the same label (confused = same label for different elements), and no meaning is carried by colour alone. `hud-protan-1080p.png`, `hud-deutan-1080p.png`, `hud-tritan-1080p.png` retained.
- **AC-3**: UXA-HUD-29 input pass
  - Setup: play a 2v2 PIE round to `Ended` once with keyboard only and once with gamepad only.
  - Verify: no HUD element takes focus or input in either run.
  - Pass condition: both runs complete with no focus or input captured. Notes in `hud-input-pass-evidence.md`. DEFERRED (full round).

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained screenshots `production/qa/evidence/hud-greyscale-1080p.png`, `hud-protan-1080p.png`, `hud-deutan-1080p.png`, `hud-tritan-1080p.png` plus sign-off and input-pass notes `production/qa/evidence/hud-input-pass-evidence.md` (the file `production/qa/evidence/hud-accessibility-captures-and-input-pass-evidence.md` carries the reviewer labels and sign-off)
- Gate: BLOCKING (UI default; not waived at `qa.level: minimal`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-019-hud-brace-indicator-widget.md`, `production/epics/hud/story-021-hud-wrong-target-tally-widget.md`, `production/epics/hud/story-025-hud-removal-card-widget-and-preempt.md`, `production/epics/hud/story-034-hud-informant-presentation.md`, `production/epics/hud/story-041-hud-objectives-cluster-widgets.md`, `production/epics/hud/story-042-hud-end-banner-summary-called-off-lobby-widgets.md`, `production/epics/hud/story-047-hud-latency-icon.md`, `production/epics/hud/story-048-hud-interact-ring-and-context-prompt.md`
- Unlocks: None within this epic (see the EPIC Definition of Done)
