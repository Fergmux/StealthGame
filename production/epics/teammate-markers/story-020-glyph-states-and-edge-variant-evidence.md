# Story 020: Glyph states and edge variant evidence

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0021 verification item UI-V4 (DPI rule and root scaler, owned by the hud epic) has not reported, and the painted glyphs this story reviews come from story 019 (itself Blocked on UI-V1, UI-V3, UI-V4).
> **Layer**: Presentation
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The marker's three fills are the only way a player reads an ally's state, and opacity encodes nothing, so shape and pattern must separate in greyscale at the smallest size (24 px). This story is the evidence pass for the glyphs: the filled, hatched and outline pentagons and the edge variant in each of the three fills, captured at 24 px and 28 px over the brightest and darkest worst-case grounds, in colour, greyscale and protan, deutan and tritan simulation. The outline state's hollow interior at the 24 px floor is only 6 px at its widest (24 minus 9 px per side), which is why the keyline is checked here: if the outline state fails with its inner keyline (CR-7), the art-director thins the inner keyline rather than raising the 24 px floor. Any accent colour proposed for the Vertical Slice (warm white, magenta, ultramarine) must pass the same simulations before adoption.

**GDD**: `design/gdd/teammate-markers.md` (VA-1, VA-4, AC-TMK-21); `design/art/art-bible.md` 3.4, 7.2, 7.5
**Requirement**: `TR-tmk-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 2: hard-step material, nearest filtering, no mips)
**ADR Decision Summary**: Glyph brushes are authored at integer sizes or drawn by a hard-step UI material with nearest filtering and no mips so stair bands stay crisp at non-integer scales.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Captures come from the painted path (story 019) on cooked or standalone builds; greyscale and colour-vision simulations are applied to the captured images offline or with the engine's colour-vision-deficiency preview (post-process, editor only), whichever the art lead prefers; record which.

**Control Manifest Rules (this layer)**:
- Required: Glyphs use integer-size brushes or a hard-step UI material with nearest filtering — source: ADR-0021
- Forbidden: Never encode a state in opacity; never rely on hue alone — source: art bible 3.4, GDD VA-4
- Guardrail: The 24 x 28 floors are absolute and never raised to rescue a glyph; the outline state's inner keyline is what gets thinned

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-21, VA-4), scoped to this story:*

- [ ] **AC1** (AC-TMK-21, [Vis] `GlyphStates`, revised): **GIVEN** captures of the filled, hatched, outline and edge glyphs (the edge variant in each of the three fills) at 24 px and 28 px over the brightest and darkest worst-case grounds, in colour, greyscale and protan, deutan and tritan simulation **WHEN** reviewed blind and in random order by at least 3 testers who did not build the glyphs *(the count is a proposal, see the gap note)* **THEN** every tester names all three states correctly in greyscale and in each simulation, every time (100 % correct).
- [ ] **AC2** (AC-TMK-21, revised): **GIVEN** the same captures **THEN** the dual-edge test passes for keyline and fill (contrast of at least 3 : 1 for graphics on the brightest and darkest ground, art bible 7.2) and the outline state passes with its inner keyline (CR-7); on a failure the art-director thins the inner keyline, and the 24 px floor is not raised.
- [ ] **AC3** (VA-4): **GIVEN** each proposed accent colour (warm white, magenta, ultramarine) **THEN** it passes the AC1 protocol before adoption and changes only the local fill colour, never shape, keyline or the allies-only rule.

*(Revised after QL-STORY-READY: the tester protocol and the dual-edge threshold are now stated; the threshold comes from art bible 7.2, the 3-tester count is a proposal logged in the decision log.)*

**Known gap (accepted):** neither the GDD nor the art bible fixes a tester count; the minimum of 3 is a proposal and the art lead may raise it.

---

## Implementation Notes

*Derived from VA-1, VA-2 and AC-TMK-21:*

State construction (VA-1): filled is solid Turquoise inside the keyline; hatched is a solid fill with two Obsidian stair bands of height `b` across the lower half, band ends offset by one 4 px step (built from the two-step stair, never a free diagonal, art bible 7.5); outline is a 3 px Turquoise stroke, hollow, with the Obsidian keyline outside and inside; the edge variant is the same pentagon in the current fill with a stepped base, no rotation, no arrow. The outline's hollow at the floor is `24 - 2 * 9 = 6` px at its widest, so the inner keyline is the first thing to fail.

Capture matrix: 6 glyph variants (filled, hatched, outline, and the edge variant in each of the three fills) x 2 sizes x 2 grounds x 5 renderings (colour, greyscale, protan, deutan, tritan) in a fixed naming scheme `tmk-glyph-<glyph>-<size>-<ground>-<rendering>.png` under `production/qa/evidence/`. Record the testers (initials), the order and the answers in `production/qa/evidence/tmk-glyph-evidence.md` with the art lead's sign-off. If a glyph fails, the fix goes back to the glyph construction in story 019's `MarkerGlyphs` and this story is re-run.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-paint-markers-in-world-anchor-leaf.md`: producing the painted glyphs.
- `story-021-hatch-on-body-hide-frame.md`: the timing of the hatch.
- Vertical Slice: the accent colour setting UI (this story only vets the proposed colours).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/tmk-glyph-*.png` and `production/qa/evidence/tmk-glyph-evidence.md`
- `Source/StealthPresentation/Private/Markers/MarkerGlyphs.cpp` (only if a glyph needs a fix)
- `Source/StealthTestHarness/Private/Markers/GlyphCaptureTest.cpp` (a capture helper, `StealthGame.Markers.PIE.GlyphCapture.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Setup: the capture matrix above. Verify: each blind tester names the state of every capture. Pass condition: 100 % correct in greyscale and in each of the three simulations, with at least 3 testers. (Gap: tester count is a proposal.)
- **AC2**: Setup: the brightest and darkest worst-case grounds at 24 px and 28 px. Verify: the dual-edge contrast of keyline and fill (at least 3 : 1) and the outline state with its inner keyline. Pass condition: both pass; on failure the inner keyline is thinned, the floor is not raised.
- **AC3**: Setup: each accent colour (warm white, magenta, ultramarine) in the three simulations. Verify: the AC1 protocol per colour, plus a check that only the local fill colour changes. Pass condition: all three pass before adoption.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (protocol and contrast threshold named); the 3-tester count is a proposal; not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures `production/qa/evidence/tmk-glyph-*.png` and the sign-off `production/qa/evidence/tmk-glyph-evidence.md` with art lead sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-019-paint-markers-in-world-anchor-leaf.md`, `story-010-marker-size-f1.md`; the `/asset-spec system:teammate-markers` output (three fill brushes, edge variant) if brushes are used
- Unlocks: Vertical Slice accent settings (outside this epic)
