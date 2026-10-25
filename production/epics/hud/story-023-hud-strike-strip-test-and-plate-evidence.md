# Story 023: AC-HUD-07 strike strip capture test and per-outcome plate screenshots

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: UI
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md` · UX spec `design/ux/hud.md`
**Requirement**: `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (§1 tiers, visual and UI evidence stays as the coding standards define), with ADR-0021 (§1 identical widget tree for every viewer and outcome)
**ADR Decision Summary**: Leak and parity tests run as multi-client PIE (T2) and capture tests; visual and UI evidence (screenshots in `production/qa/evidence/`) is not replaced by any automated tier. ADR-0021 requires the widget tree to be identical for every viewer and outcome (all layers created once and collapsed until needed), which is what makes a pixel diff between two strike runs meaningful.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The HUD-layer-only capture renders the UMG layers to a render target, because UMG is composited after the scene and its post-process (E2), so a scene capture cannot isolate it; verify the render-target route (for example drawing the player screen widgets through `FWidgetRenderer` or the viewport's Slate capture) in the editor before relying on it, and record which route was used. TAA off, neutral tonemap and the 60 Hz fixed step are required for pixel-identical frames. DPI is `H/1080` through the custom rule and is cached per viewport size (E5), so the 16:10 capture needs its own viewport size. HIGH networking domain (ADR-0018) applies to the two strike runs: the contact tick is taken from the anim-notify on that client, never from HUD data. Use `UE_LOGF` for capture-run logging.

**Control Manifest Rules (this layer)**:
- Required: All HUD widgets are created once at HUD init and toggled by visibility — source: ADR-0021
- Forbidden: Never create or destroy a widget during a round in response to gameplay — source: ADR-0021
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This is the visual proof of the HUD's strike-strip parity rule. Story 022 shows the right plate at the right time; this story proves, pixel for pixel, that two strikes that look the same in the world (an NPC strike and an enemy-player strike under identical seeds) produce identical HUD pixels until the owner's contact frame, and differ afterwards only where the result plate and tally plate live. It also retains one screenshot per plate outcome at two aspect ratios, because UI stories must close on retained screenshots, not assertions. The capture test is an executable gate, but its classification is UI/Visual: its evidence is the retained diff image and screenshots reviewed against UXA-HUD-18 and UXA-HUD-33.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` (AC-HUD-07, amended per the QA-gate finding) and `design/ux/hud.md` (UXA-HUD-33 plate part), scoped to this story:*

- [ ] **AC1** (AC-HUD-07 as amended by the QA-gate finding (GDD owner to align AC-HUD-07 and HUD-R3 with F6), manual check with retained `production/qa/evidence/hud-strike-strip-diff.png`): **GIVEN** AC-HUD-01's two runs under the determinism rule with TAA off, the HUD widget layers rendered to a render target (HUD-layer-only capture) **WHEN** captures at ticks `t_show - 1`, `t_show` and `t_show + 1` are diffed **THEN** the `t_show - 1` captures are pixel-identical and later captures differ only inside the union of the result-plate rectangle and the tally-plate rectangle (the tally changes only in the WrongTarget run). Evidence retained.
- [ ] **AC2** (UXA-HUD-33 plate part, UXA-HUD-18, manual check, `production/qa/evidence/hud-plate-<outcome>-1080p.png`): **GIVEN** each result plate outcome (Target eliminated, Wrong target, No contact, Reversed) and the defence plate **THEN** retained screenshots exist at 1080p and 16:10, each showing its approved text and glyph with the same size and slot.

---

## Implementation Notes

*Derived from GDD AC-HUD-01 and AC-HUD-07, HUD-R3, UX E07/E08 and ADR-0008 §1:*

**What to build.** A capture run, not a new widget. (1) A HUD-layer capture helper in the test harness: render the HUD widget layers (L0 to L4, not the scene) to a render target at a named tick and write it to disk as a lossless image; it must produce identical bytes for identical frames (no timestamps embedded, no dithering). (2) The two-run driver of AC-HUD-01: a listen host plus the owning client, fixed step 60 Hz, identical seeds; run A strikes an NPC, run B strikes an enemy player, with the owner's strike-clip contact tick `t_contact` taken from the strike anim-notify on that client; `t_show` is the tick the HUD shows the result plate, which is the contact tick for the attacker's plate (F2 never earlier than contact). (3) The diff tool: compare captures at `t_show - 1`, `t_show` and `t_show + 1` per run pair, emit a diff image and the bounding rectangles of differing pixels, and check them against the result-plate rectangle and the tally-plate rectangle from the layout resolver (story 007) as a union. Output goes to `production/qa/evidence/hud-strike-strip-diff.png` plus a short log in the sign-off file.

**The amended criterion.** AC-HUD-07 originally allowed differences only inside the result plate's rectangle. The QA-gate finding amends it: F6 and HUD-R7 move the tally with the plate, so in the WrongTarget run the tally plate also changes at `t_show`. The allowed region is therefore the union of the result-plate rectangle and the tally-plate rectangle, and the tally rectangle may differ only in the WrongTarget run. The GDD owner aligns AC-HUD-07 and HUD-R3 text with F6; this story follows the amended form and does not edit the GDD. If the capture shows a difference outside the union (for example the brace indicator, a marker, or the strike hint), that is a leak defect to raise against the owning story, not a tolerance to widen. The `t_show - 1` captures must be pixel-identical between the two runs: no tolerance at all, which depends on the widget tree being identical for both runs (the layers are created once and collapsed until needed, ADR-0021 §1).

**Determinism.** Fixed step 60 Hz, identical seeds, dumps keyed by tick index with wall-clock fields excluded, TAA off, the HUD-layer-only render target; "unchanged" means equal to a control run with the same seed and no strike. Run the same pair twice to confirm the capture is repeatable before trusting a diff.

**Plate screenshots.** Drive each outcome through the story 022 pipeline and capture the Result slot at its fully-in frame (alpha 1, after the 250 ms entrance): Target eliminated, Wrong target, No contact, Reversed, and Strike reversed for the defender. At 1080p and at 16:10 (for example 1920 x 1200), 10 captures in all, named `hud-plate-<outcome>-1080p.png` and `hud-plate-<outcome>-16x10.png` with `<outcome>` in {target-eliminated, wrong-target, no-contact, reversed, strike-reversed}. Check each against UX E07/E08: approved text from the string table, glyph slot content, same plate size and position in every capture (overlay a grid or compare plate rectangles from the layout resolver).

**Why this is a UI story.** The proof here is retained images reviewed by a person against the UX spec, with an automated capture driver as the means, so the closing evidence is screenshots and a sign-off; the automated leak suites over many runs are stories 028 and 029.

**Out of this story:** the plate pipeline (story 022), the tally logic and widget (020, 021), the repeated leak suite and statistics (028, 029), the other UXA-HUD-33 screenshots (stories 054, 056, 060).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`: the plate flow being captured.
- `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`: the AC-HUD-01 and look-parity leak suite (the two-run pair this story reuses for a capture).
- `production/epics/hud/story-029-hud-leak-suite-no-warning-before-contact.md`: the no-warning-before-contact suite.
- `production/epics/hud/story-056-hud-layout-evidence-resolutions-and-budget.md`: layout evidence across resolutions.
- `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`: the full 2v2 run-and-observe screenshots.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/HUD/HudLayerCapture.h`
- `Source/StealthTestHarness/Private/HUD/HudLayerCapture.cpp` (HUD-layer-only render-target capture and lossless writer)
- `Source/StealthTestHarness/Private/HUD/StrikeStripDiffTest.cpp` (CQTest capture run, `StealthGame.HUD.PIE.StrikeStripDiff.*`; writes the capture set and the diff, the human reviews the evidence)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Strike strip diff (AC-HUD-07 as amended) (manual check)
  - Setup: run AC-HUD-01's two runs (NPC strike, enemy-player strike) at fixed 60 Hz with the same seed and TAA off. Render the HUD widget layers to a render target and capture ticks `t_show - 1`, `t_show` and `t_show + 1`.
  - Verify: diff the two runs' captures at each tick.
  - Pass condition: the `t_show - 1` captures are pixel-identical. Later captures differ only inside the union of the result-plate rectangle and the tally-plate rectangle (the tally changes only in the WrongTarget run). `hud-strike-strip-diff.png` is retained.
- **AC-2**: Per-outcome plate screenshots (manual check)
  - Setup: drive Target eliminated, Wrong target, No contact, Reversed and the defence plate. Capture at 1080p and 16:10.
  - Verify: each plate shows its approved text and glyph, with the same size and slot.
  - Pass condition: all 10 captures retained (`hud-plate-<outcome>-1080p.png` and the 16:10 set).

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: UI
**Required evidence**:
- UI: retained `production/qa/evidence/hud-strike-strip-diff.png`, the ten plate screenshots `production/qa/evidence/hud-plate-<outcome>-1080p.png` and `hud-plate-<outcome>-16x10.png`, and the sign-off `production/qa/evidence/hud-strike-strip-test-and-plate-evidence-evidence.md`. Gate level BLOCKING (UI evidence is not waived at `qa.level: minimal`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-022-hud-result-and-defence-plate-flow.md`, `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`
- Unlocks: `production/epics/hud/story-060-hud-run-and-observe-2v2-evidence.md`
