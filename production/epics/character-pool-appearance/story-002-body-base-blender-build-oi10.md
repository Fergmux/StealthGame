# Story 002: Body_Base built in Blender to the UEFN reference (OI-10)

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: Tier A, B and C pools are all merged single-slot meshes on the one project skeleton; Tier C (Body_Base plus Meshy heads and garments) is the supported fallback if Meshy bodies fail conformity, so Body_Base is the canonical conform target for every model.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); this story is Blender-only and exports nothing to Unreal
**Engine Notes**: Blender-side only (art bible 1.3.9). The reference mesh is `assets/characters/reference/SKM_UEFN_Mannequin.fbx`; the skeleton in the editor is `SK_UEFN_Mannequin` (88 bones). Blender must not export the armature object as a bone (the prototype citizens did, producing a root bone named `SKM_UEFN_Mannequin` and the editor's "Skeleton is missing bones ... They will be added now" message).

**Why this story**: OI-10 (Body_Base not yet built) and OI-1 gate the first Meshy credit (art bible 1.3.9, 8.7 step 0). The Art Director's first visual story is the test character with Body_Base as its conform target, so this comes before any Meshy generation, and long before any mass production (that waits for spikes R1 to R3). Body_Base is the first Blender-only deliverable: one unisex adult bare under-garment body, neutral head form, the reference A-pose, soles at Z = 0, origin between the feet, scale 1.0 applied. Read art bible 1.3.2, 1.3.5, 1.3.7 (T1 items) and 1.3.9 by section.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One merged single-slot skeletal mesh per model/accessory variant on the project's one skeleton (ADR-0012); every pawn shares the one skeleton and the zero-extra-bone rule (ADR-0010/0013)
- Forbidden: Never fix proportion errors by non-uniform scale (art bible 8.7 step 3); never ship a barefoot model
- Guardrail: Pool character LOD0 12,000 ± 2,000 triangles (art bible 8.5); Body_Base is a proxy and is never exported

---

## Acceptance Criteria

*From art bible 1.3.9 and T1 items 1 to 5 and 17 to 18 (1.3.7), scoped to this story:*

- [ ] **AC-1** GIVEN OI-1 closed (story 001), WHEN Body_Base is built in Blender (one unisex adult bare body, stature 165.6 cm ± 2, soles at Z = 0, origin between the feet, scale 1.0 applied, the UEFN reference A-pose, bone chains equal to `SK_UEFN_Mannequin`'s), THEN art bible T1 items 1 to 5 and 17 to 18 pass by script and the report is saved.
- [ ] **AC-2** GIVEN skin weights transferred from the UEFN mannequin body mesh and hand-corrected, WHEN the 1.3.5 pose set is applied, THEN every vertex has at most 4 influences, the armature has exactly the reference's 88 bones (the armature object is not exported as a bone), no intersection exceeds 1.0 cm and no joint twists beyond 15 degrees.
- [ ] **AC-3** GIVEN the accepted file, THEN `Body_Base.blend`, a four-view render and the T1 report are retained and the art-director sign-off (name, date) is recorded in `production/qa/evidence/body-base-evidence.md`; Body_Base is never exported to Unreal (it lives in each model's `.blend` as `Body_Proxy`).

---

## Implementation Notes

*Derived from art bible 1.3.9 and ADR-0012 Implementation Guidelines (content pipeline, R8):*

- Start from `SKM_UEFN_Mannequin.fbx`; sculpt a neutral head form, keep the body to the under-garment surface only. Build to the numbers re-derived in story 001 (head height, shoulder, chest depth, hip, foot), with bone chain lengths equal to the UEFN skeleton's (chains win).
- Weights: nearest-face-interpolated transfer from the UEFN body mesh, then hand-correct elbows, shoulders, hips, knees; limit 4 influences; normalise. Apply the 1.3.5 pose set and measure intersections and twist by script.
- Save as `assets/characters/body_base/Body_Base.blend` (new folder). Keep the T1 script output as text beside it so story 008 and story 003 reuse the same measurements.
- Record the Blender FBX export preset (axes, scale, no armature-as-bone) here once verified on a Manny/UEFN round trip; story 003 depends on it.

**Prototype disposition**: promote `tools/asset-pipeline/blender_bind_to_mannequin.py` (bind procedure) after fixing its armature export so no extra root bone appears; the prototype's four Meshy citizens (`assets/characters/meshy/citizen_*`) are throwaway and are not used as references.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 003 (`story-003-meshy-test-character-generate-normalise.md`): the single Meshy test character
- Story 008 (`story-008-pool-mesh-conformance-validator.md`): the Unreal-side automated conformance check
- Models 2 to 8 and Tier A variants: stories 045 and 052 onward (mass production is gated by R1 to R3)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `assets/characters/body_base/Body_Base.blend`
- `assets/characters/body_base/Body_Base_T1_report.txt`
- `tools/asset-pipeline/blender_bind_to_mannequin.py` (armature-export fix only)
- `production/qa/evidence/body-base-evidence.md`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Visual/Feel: manual verification; sign-off required.*

**Manual check: Body_Base T1 items 1-5 and 17-18**
- Setup: run the T1 script on `Body_Base.blend`.
- Verify: stature 163.6 to 167.6 cm, soles at Z = 0, origin between the feet, scale 1.0 applied, A-pose, chains equal to `SK_UEFN_Mannequin`.
- Pass condition: the script reports pass on every listed item and the report file is saved.

**Manual check: weights and pose set**
- Setup: apply the 1.3.5 pose set, then run the influence, bone-count and intersection scripts.
- Verify: at most 4 influences on every vertex, exactly 88 bones with no armature-object bone, maximum intersection at most 1.0 cm, maximum joint twist at most 15 degrees.
- Pass condition: the scripts report zero violations.

**Manual check: evidence retained**
- Setup: open `production/qa/evidence/body-base-evidence.md`.
- Verify: `Body_Base.blend`, a four-view render and the T1 report exist, with art-director name and date; no Body_Base asset exists under `Content/`.
- Pass condition: all files present and the sign-off is dated.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: a retained four-view render in `production/qa/evidence/` + sign-off in `production/qa/evidence/body-base-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-001-oi1-uefn-reference-measurements-recorded.md`
- Unlocks: `production/epics/character-pool-appearance/story-003-meshy-test-character-generate-normalise.md`, `production/epics/character-pool-appearance/story-045-tier-a-six-mannequin-variant-models.md`
