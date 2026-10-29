# Story 034: The R5 leak matrix passes on the production hide for every LOD and lighting cell

> **Epic**: Concealment (Bushes)
> **Status**: In Review
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R6b, Visual/Audio, AC-CON-23, art bible R5)
**Requirement**: `TR-con-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§6; Verification Required R5), with ADR-0022 (rendering configuration and performance budgets)
**ADR Decision Summary**: A hidden pawn must leave no shadow, VSM, Lumen (software and hardware RT), reflection or ray-traced trace; the three gating flags are class defaults and the hide is `SetVisibility(false, true)`. AC-CON-23 is the production verification that the occupied bush is indistinguishable from the empty one outside the rustle window across the R5 matrix and every LOD.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Runs with `bCreateRenderStateForHiddenComponentsWithCollsion` off. Hardware ray-traced Lumen cells need RT-capable hardware (RTX 3060-class reference, ADR-0022). The noise floor is the 99th percentile of the temporal-mean difference between two empty bushes captured at matched sway phase.

**Control Manifest Rules (this layer)**:
- Required: Hiding applies the full ADR-0017 §6 flag set — source: ADR-0017
- Forbidden: Never toggle the three gating flags at runtime; never use `bHidden` — source: ADR-0017
- Guardrail: Zero visible magenta-proxy pixels from the swap until the crossing at 5-25 m through every LOD

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-23), scoped to this story:*

- [ ] **AC1** (AC-CON-23, [Vis] `StealthGame.Concealment.R5LeakMatrix`, VA-4): **GIVEN** the R5 matrix (low sun, noon, shade × Lumen software and hardware RT × VSM on/off × flag combinations) **WHEN** an occupied and an empty bush are captured with sway phase matched (60-frame temporal means) plus single frames around each swap **THEN** no difference exists outside the rustle window above the noise floor, and the magenta occlusion proxy shows 0 visible pixels from the swap until the crossing at 5-25 m through every LOD. Evidence: `con-r5-*.png` plus the results sheet, with lead sign-off.

---

## Implementation Notes

*Derived from ADR-0017 §6 and GDD AC-CON-23:*

Reuse the capture tooling and noise-floor method from spike R5 (story 021), now against the production hide of story 022. Single frames around each swap catch one-frame flashes that temporal means hide. Every LOD of the pawn is covered, including the farthest, because LOD changes can re-add a proxy. Any difference above the floor outside the rustle window is a defect against story 022, not a reason to widen the floor.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-021-spike-r5-hidden-primitive-leaks.md`: the throwaway-code verification.
- `story-022-presentation-hide-application-and-host-parity.md`: the implementation.
- ADR-0022 spikes R1-R3: frame budgets.

---

## Files

- `Source/StealthTestHarness/Private/Scenarios/R5LeakMatrixCapture.cpp`
- `tools/analysis/r5_matrix.py`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: Manual check: R5LeakMatrix
  - Setup: the R5 matrix with an occupied and an empty bush at matched sway phase (60-frame temporal means), plus single frames around each swap, at 5-25 m through every LOD.
  - Verify: any difference outside the rustle window against the noise floor (p99 between two empty bushes), and the magenta occlusion proxy pixel count from swap to crossing.
  - Pass condition: no difference above the floor and 0 magenta pixels; `con-r5-*.png` and the results sheet retained with lead sign-off.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures `con-r5-*.png` and the results sheet in `production/qa/evidence/` with lead sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-021-spike-r5-hidden-primitive-leaks.md` (blocker), `story-022-presentation-hide-application-and-host-parity.md`, `story-028-enter-swap-hides-body-per-viewer.md`
- Unlocks: `story-035-prototype-retirement-and-regression.md`

## Completion Notes
**Completed**: 2026-10-29 (status In Review)
**Criteria**: PARTIAL (see con-r5-results.md). Built: the production-hide capture scenario Stealth.R5Matrix (Source/StealthTestHarness/Private/Scenarios/R5LeakMatrixCapture.cpp) and tools/analysis/r5_matrix.py; run 4 times in the editor's standalone PIE. Holds: no subject primitive (body, groom, Niagara) keeps a render proxy while hidden in any cell or at the mesh's one LOD; in every converged cell the occupied bush's ROI residual equals the empty-vs-empty noise; the magenta occlusion proxy (run 3) showed 0 body pixels in every frame from the swap to the crossing at 5 and 15 m with valid controls (1,707 / 266 body pixels visible). Open: some cells' lighting did not converge between their two empty captures (sky light / Lumen settling after the sun moves), so those cells are NOT ASSESSED; the 25 m magenta row had no valid control; single frames show a faint temporal-AA ghost of the body for ~2-3 frames after the swap (inside the rustle window, which AC-CON-23 excludes; recorded for the lead). HW-RT cells: N/A by configuration (r.RayTracing=False, ADR-0022).
**Deviations**: ADVISORY: the mesh has a single LOD, so 'every LOD' is LOD0; lead sign-off pending.
**Test Evidence**: production/qa/evidence/concealment-027-034-2026-10-29/
**Gate**: Build Succeeded; full ^StealthGame. suite 594 tests, 593 passed first run (ExitUncullBeforeLeafPlane snap check narrowed to late actors, re-run alone at full counts PASS)
Run result: OBSERVED — production hide captured in standalone PIE across the lighting cells (con-r5-*.png); results sheet con-r5-results.md.
**Reviewed by**: self code review 2026-10-29 (notes in production/qa/evidence/concealment-027-034-2026-10-29/evidence.md)
