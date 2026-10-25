# Story 053: First six Meshy bodies pass or fail the Body_Base conform test (Tier B versus Tier C)

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: the OI-1/OI-10 test character (stories 001 to 004) has not reported and the art bible's mass-production gate (spikes R1, R2, R3, art bible 8.x.14; ADR-0022 conditional) has not lifted: no Meshy credit is spent on characters 2 to 8 until both do. Credit confirmation with the user is also required.
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: Tier B (Meshy models normalised to the project skeleton) is valid only if normalised bodies conform to Body_Base; if 3 of the first 6 fail, the Tier C fallback (one shared body with Meshy-generated heads, hair and garments) is invoked; the allocator and pool table are identical across tiers (risk R8).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Conform measured by a committed script (see AC-1). Meshy credit costs are confirmed with the user before each generation (project Meshy rule).

**Why this story**: This is the Exit B decision point (GDD OQ-1, art bible 8.7). The **prototype citizens are throwaway** and are not counted among the six. The Meshy normalisation pipeline is the one story 003 proves. **AC-1 and AC-2 revised per QL-STORY-READY** (named script; sign-off). Read art bible 1.3.1, 1.3.9, 8.7.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One merged single-slot mesh per model on the project skeleton; conform check before pool admission (ADR-0012)
- Forbidden: Never rescue a model beyond the 8 h cap (regenerate or cut); never non-uniform scale
- Guardrail: 4 h target and 8 h hard cap per model (art bible 8.7)

---

## Acceptance Criteria

*From art bible 1.3.9 and 8.7 and GDD OQ-1; revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN the first 6 Meshy bodies normalised per art bible 8.7, THEN each conform test (95th percentile surface deviation from Body_Base at most 1.5 cm, mean at most 1.0 cm, measured by the named committed script `tools/asset-pipeline/<name>.py`) result is recorded for 6 of 6.
- [ ] **AC-2** GIVEN the results, THEN if 3 or more fail a Tier C decision (Meshy heads and garments on Body_Base) is recorded, otherwise Tier B continues; GDD OQ-1 is closed in the report, with an art-director sign-off (name, date) on the verdict.
- [ ] **AC-3** GIVEN the per-model time log, THEN each model took at most 8 h, otherwise it was regenerated or cut with the action recorded.

---

## Implementation Notes

- Reuse story 003's normalisation steps and story 008's arm-extent metric in the script; the 95th-percentile surface deviation is a Blender-side measurement against `Body_Proxy`.
- Models 1 to 6 follow the roster rows of art bible 1.3.10; model 1 may be the story 003 test character if it passes.
- A Tier C decision changes content only (story 024 proves tier independence); open a story for the Tier C head/garment generation when decided.

**Prototype disposition**: `SK_Citizen02-05` are not among the six and are not promoted.

---

## Out of Scope

- Story 054: swapping the pool rows; Story 004: the single test character

---

## Files

- `tools/asset-pipeline/conform_check.py` (name to confirm; committed)
- `assets/characters/meshy/<model>/` (six)
- `production/qa/evidence/meshy-first-six-evidence.md`

---

## QA Test Cases

*QL-STORY-READY: GAPS → script named and sign-off added with the qa-lead's proposed text; not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: conform test**
- Setup: the first 6 Meshy bodies normalised per 8.7.
- Verify: 95th-percentile deviation from `Body_Base` at most 1.5 cm and mean at most 1.0 cm, per body.
- Pass condition: results recorded for 6 of 6.

**Manual check: tier decision**
- Setup: the recorded results.
- Verify: 3 or more failures records Tier C, otherwise Tier B continues; OQ-1 closed in the report.
- Pass condition: the decision and OQ-1 closure are recorded and signed.

**Manual check: time**
- Setup: the per-model time log.
- Verify: each at most 8 h, otherwise regenerated or cut.
- Pass condition: none over the cap without a recorded action.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures in `production/qa/evidence/` + sign-off in `production/qa/evidence/meshy-first-six-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-004-test-character-t1-t4-signoff.md`, `production/epics/character-pool-appearance/story-008-pool-mesh-conformance-validator.md`
- Unlocks: `production/epics/character-pool-appearance/story-054-meshy-pool-swap-tier-b-or-c.md`
