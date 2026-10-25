# Story 043: Bush greyscale and colour-swatch identification tests (VA-4)

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md` (AC-45, AC-46; Visual human tests section); art bible `design/art/art-bible.md` (3.6 tests)
**Requirement**: `TR-lsc-???` (no registry entry matches the VA-4 human tests; traced to GDD AC-45 and AC-46)
*(The art bible 3.6 states both tests "are run on the graybox before the vertical slice".)*

**ADR Governing Implementation**: N/A — human perception test of art-bible rules on the graybox; no architectural decision governs (V-BUSH-3's backdrop warning in ADR-0014 5.3 defers to this test)
**ADR Decision Summary**: N/A
**ADR Version**: N/A

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Greyscale captures are produced from the graybox renderer with debug draw off; pass thresholds are set so the one-sided 95% bound on the true rate meets the target; testers are counted as clusters and option positions are randomised.

**Why this story**: A bush must be recognisable at 25 m by shape and value alone while carrying no unique colour (Pillar 1: nothing but shape tells a bush). Read first: GDD "Visual human tests" preamble, AC-45, AC-46; art bible 3.6.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Bushes are identified by shape and value structure, never a unique colour (art bible 3.6)
- Forbidden: Never use debug draw in the evidence captures (GDD Acceptance Criteria preamble)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-45 and AC-46, scoped to this story:*

- [ ] GIVEN a 25 m greyscale bush test with at least 10 testers and at least 20 in-situ captures, THEN at least 92 of 100 bush judgments are correct and at most 5 of 100 decorative-plant judgments are called bushes (evidence: `bush-25m-*.png` plus the results sheet)
- [ ] GIVEN a colour-swatch test, 4-choice, at least 10 testers and at least 200 trials, THEN the bush swatch is picked in at most 58 of 200 (rate bound at most 35%; chance 25%)

---

## Implementation Notes

- Captures: at least 20 in-situ 25 m captures (bushes and decorative plants of the Dome-Ban vocabulary: maguey rosettes, maize stalks, ground cover, canopies), greyscale, randomised order; 100 bush judgments and 100 decorative-plant judgments across at least 10 testers (clusters).
- Swatch test: the bush's colour swatch against three other foliage-family swatches, positions randomised, at least 200 trials; at most 58 picks of the bush swatch.
- Keep raw trial sheets, tester count and randomisation seed in the results sheet; art-director sign-off retained in `production/qa/evidence/level-socket/`.
- A failure feeds back to the bush silhouette/value spec (art bible 3.6) and V-BUSH-3's threshold; this story only records the outcome.

---

## Out of Scope

- Story 042: leg-and-forearm value band (OI-2); story 044: static-figure test (AC-47)

---

## Files

- `production/qa/evidence/level-socket/bush-25m-*.png`
- `production/qa/evidence/level-socket/bush-greyscale-swatch-results.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE, provisional TR).*

- **AC-1** 25 m greyscale test
  - Setup: >= 20 in-situ captures, >= 10 testers, greyscale, randomised.
  - Verify: 100 bush and 100 decorative-plant judgments.
  - Pass condition: >= 92/100 bush correct; <= 5/100 decorative called bushes; evidence `bush-25m-*.png` plus results sheet.
- **AC-2** colour-swatch test
  - Setup: 4-choice, randomised positions, >= 10 testers, >= 200 trials.
  - Verify: bush swatch picks.
  - Pass condition: <= 58 of 200 (rate bound <= 35%). Retain the sheet in `production/qa/evidence/level-socket/` with art-director sign-off.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures and the results sheet in `production/qa/evidence/level-socket/` with art-director sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-042-bush-value-band-leg-forearm-oi2.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
- Cross-epic: `production/epics/concealment-bushes/EPIC.md` (bush asset family)
- Unlocks: Vertical Slice gate (`/vertical-slice`)
