# Story 040: Stack and slab visual equivalence

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md` (Visual/Audio VG-1 to VG-3)
**Requirement**: `TR-citygen-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. Bay stacks and floor slabs exist to hold the instance budget; this story proves they cost the look nothing.)*

**ADR Governing Implementation**: ADR-0022: Rendering configuration and performance budgets (ADR-0014 for the assembly)
**ADR Decision Summary**: Substrate off, Lumen software GI, Virtual Shadow Maps, Nanite for the kit; kit instances <= 5,000 with the kit costing <= 5.0 ms GPU (R1). World-aligned kit materials keep slab and stack joints invisible; slabs keep 0.2 m thickness (Lumen minimum).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0022; rendering is HIGH risk for 5.4-5.8)
**Engine Notes**: Needs the baked stack and slab assets (derived assets, allowed before the mass-production gate by user decision 2026-10-25) and a per-cell-module render switch (render the same seed from per-cell modules instead of stacks and slabs): that switch is a deliverable of this story (a non-Shipping builder option). Lumen/VSM captures of the same scene differ run to run, so a noise floor is measured. No `.uasset` is created by this planning step; the story's dev work creates the baked assets under `Content/StealthGame/Environment/Kit/` through the art pipeline (asset-spec).

**Control Manifest Rules (this layer)**:
- Required: Verify with retained screenshots; Visual/Feel stories need a lead sign-off — source: `.claude/docs/coding-standards.md`
- Forbidden: Never re-save or modify the prototype `SM_Kit_*` assets — source: ADR-0009
- Guardrail: slabs 0.2 m thick; kit GPU <= 5.0 ms (measured by story 043)

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G17, VG-2, VG-3), scoped to this story:*

- [ ] **AC1** (AC-G17, `Visual.StackSlabEquivalence`): **GIVEN** one seed rendered with slabs and stacks and again with per-cell modules, from a fixed camera set (6 named viewpoints, at least 3 at joints), a fixed sun and a 64-frame warm-up, with a noise floor recorded from two same-configuration captures **THEN** on the joint-pixel mask (pixels covered by slab or stack surfaces, from a stencil/object-ID mask) (|mean dL*| minus the noise-pair |mean dL*|, floored at 0) <= 1 and (p99 |dL*| minus the noise-pair p99, floored at 0) <= 3 (provisional, re-set from the first R1 capture); if the noise-pair value alone exceeds the threshold the result is INCONCLUSIVE and the capture protocol is fixed before judging.
- [ ] **AC2** (VG-2, sign-off): **GIVEN** the two captures **THEN** the lead signs off a side-by-side with no structure at joints on all 6 viewpoints, slabs keep 0.2 m thickness (+- 1 mm), and captures are retained in `production/qa/evidence/citygen/`.
- [ ] **AC3** (VG-3, `Visual.NoRepetitionTell`): **GIVEN** a facade strip and a top-down floor capture **THEN** autocorrelation shows no secondary peak at lags of 4-20 m (threshold 0.5, provisional).

---

## Implementation Notes

*Derived from GDD VG-1 to VG-3:*

Slabs and stacks are invisible by construction: world-aligned kit materials (axis from the world normal; never object-aligned, because yaw would break continuity), course and quoin lines aligned across every adjacency, modules abut with zero gap, closed end caps, walls start at Z = 0 below the floor top (no Lumen light leaks); an 8 x 8 m slab's distance-field resolution is checked in R1. Capture protocol: the same seed, the same 6 viewpoints, the same sun, a 64-frame warm-up so VSM page cache and Lumen surface cache settle, a pair of identical-configuration captures to measure the noise floor, then the two configurations. The comparison tool is a T1-style image-diff executable with retained PNGs; "joint pixels" come from an object-ID or stencil mask of slab and stack surfaces. The autocorrelation (VG-3) is mean-removed and normalised, along a facade strip of at least 40 m (metres per pixel recorded) and radially for the floor capture; a deliberately periodic strip (ABABAB) must trip the threshold (recommended positive control). Seams: M05 needs its own mitred cornice (art-bible change request, VG-1: M04 cornices meet at M05 corners and z-fight); track it in the asset spec, not here.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-kit-tables-and-generator-kit-version.md`: stack and slab tables as data.
- `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`: GPU cost of the kit.
- `story-044-tier-g1-validation-visual-tests-and-fairness-playtest.md`: LSC VA tests at tier G1.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenPerCellModuleSwitch.cpp` (non-Shipping render switch)
- `tools/qa/citygen-image-diff/` (image-diff and autocorrelation tool)
- `production/qa/evidence/citygen/stack-slab-[viewpoint]-{stack,cell}.png`, `autocorr-*.png`
- `production/qa/evidence/citygen/stack-slab-equivalence-evidence.md` (measurements and lead sign-off)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC2 and AC3 ADEQUATE; AC1 was GAPS on the noise-floor reading and the camera set, revised above with the qa-lead's wording).*

- **AC1** `Visual.StackSlabEquivalence`
  - Setup: a golden seed, 6 fixed viewpoints (3 at joints), fixed sun, 64-frame warm-up. Capture two same-configuration images per viewpoint (noise pair), then slab+stack and per-cell renders.
  - Verify: compute the joint-pixel mask from the stencil/object-ID pass; compute |mean dL*| and p99 |dL*| on that mask for both comparisons.
  - Pass condition: net of the noise pair, |mean dL*| <= 1 and p99 |dL*| <= 3 on every viewpoint; INCONCLUSIVE (fix the protocol) if the noise pair alone exceeds a threshold.
- **AC2** (VG-2 sign-off)
  - Setup: the golden seed and 6 fixed viewpoints, 3 of them at joints. Capture the slab/stack and per-cell renders and build a side-by-side. Measure the Z thickness of every slab size.
  - Verify: a named lead reviews each viewpoint.
  - Pass condition: lead sign-off of "no structure at joints" on all 6 viewpoints. Slab thickness is 0.20 m (+- 1 mm). PNGs and the sign-off are retained in `production/qa/evidence/citygen/`.
- **AC3** `Visual.NoRepetitionTell`
  - Setup: a facade strip capture (a run of at least 40 m) and a top-down floor capture on one seed. Record the metres-per-pixel scale.
  - Verify: compute mean-removed normalised autocorrelation along the strip, and radially for the floor.
  - Pass condition: the maximum value over lags of 4-20 m is <= 0.5 (provisional). The plot is retained. A deliberately periodic strip (ABABAB) trips the threshold (positive control).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: a retained screenshot set in `production/qa/evidence/citygen/` plus the lead sign-off in `production/qa/evidence/citygen/stack-slab-equivalence-evidence.md` (BLOCKING, not waived at any `qa.level`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-004-kit-tables-and-generator-kit-version.md`, `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`, `story-019-placement-bay-stacks-and-manifest-emission.md`, `story-026-server-kit-spawn-from-manifest.md`
- Unlocks: `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`, `story-044-tier-g1-validation-visual-tests-and-fairness-playtest.md`
