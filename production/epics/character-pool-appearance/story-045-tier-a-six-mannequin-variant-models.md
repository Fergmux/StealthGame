# Story 045: Tier A: six mannequin-variant models on the UEFN skeleton

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
**ADR Decision Summary**: Tier A (MVP first, Exit A of the epic) is six mannequin-derived "models": recombinations of hair, headwear and garment-cut geometry on the shared body, each baked into one merged single-slot mesh with 4 LODs; the stock mannequin has no face and is outside the skin band, so skin and hair are authored through the master; Tier A is provisional pending the Meshy retarget spike.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: The reference skeleton is `SK_UEFN_Mannequin` (88 bones, 165.6 cm; story 001), not Manny. No extra bone (the prototype's armature-as-bone export is fixed in story 002). Art risk (art-director): variety reads at 20 to 30 m but not at assassination range where faceless twins look like dolls; G-VAR (story 049) exists to catch that.

**Why this story**: This is Exit A's content and does not spend a Meshy credit (the Art Director's first visual stories gate Meshy only; Tier A comes from the mannequin kit on Body_Base). Read GDD CP-5, Visual/Audio "MVP tier honesty", art bible 1.3.1, 1.3.5, 1.3.10 (roster rows), 8.5.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One merged single-slot skeletal mesh per model, one skeleton, zero extra bones (ADR-0012)
- Forbidden: Never build appearances at run time (leader-pose, merge, Mutable); no bare feet (shared cactli sandal piece)
- Guardrail: LOD0 12,000 ± 2,000 triangles, LOD1 at most 6,000, LOD2 at most 2,500, LOD3 at most 900 (art bible 8.5)

---

## Acceptance Criteria

*From GDD CP-5, art bible 1.3.5, 1.3.7, 8.5; AC-2 tolerance and sign-off added per QL-STORY-READY:*

- [ ] **AC-1** GIVEN six merged single-slot meshes on `SK_UEFN_Mannequin` (recombinations of hair, headwear and garment-cut geometry on the Body_Base body; no extra bones), THEN each passes its per-model T1 items (stature 165.6 cm ± 2, no extra bone) and neither Quinn nor Manny builds are counted as model differences.
- [ ] **AC-2** GIVEN each mesh, THEN it has 4 LODs within the art bible 8.5 triangle limits (LOD0 12,000 ± 2,000, LOD1 at most 6,000, LOD2 at most 2,500, LOD3 at most 900), Nanite off and the shared physics asset, and an art-director sign-off per model is recorded.
- [ ] **AC-3** GIVEN the stock mannequin, THEN T1 checklist items 15 and 16 (material slots, UV density, LOD0 triangle count) are measured and recorded (GDD OQ-2), and the values are dated.

---

## Implementation Notes

- Build each model in its own `.blend` from Body_Base (`Body_Proxy`), following the roster rows of art bible 1.3.10 (cut, hair form, accessory set, swatches); six models, `GarmentSwap` variants are material-instance-only (CP-4: variant 2 is `GarmentSwap` only for MVP).
- Mannequin grey is outside the skin band: skin and hair come from the master; the head is a neutral form with sculpted hair (hair dark, L* 10 to 22).
- Chain-length and arm-extent audit with story 008's validator before import.
- Use a single flat cactli sandal piece on all six.

**Prototype disposition**: replaces the four Meshy citizens `SK_Citizen02-05` (throwaway; arms about 17 % short, extra root bone) as the MVP pool; `assets/characters/meshy/` sources are kept as references only.

---

## Out of Scope

- Story 046: assembling `DA_CharacterPool` and the 12 MIs; Story 047: the whole-pool T1 to T4 gate; stories 053 and 054: Meshy models

---

## Files

- `assets/characters/tier_a/<model>/<model>.blend` (six)
- `Content/StealthGame/Characters/Pool/SK_Pool_<Model>` (six; follow `production/never-resave-assets.md`)
- `production/qa/evidence/tier-a-models-evidence.md`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 tolerance and sign-off added with the qa-lead's proposed text (the triangle limits are those of art bible 8.5); not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: per-model T1**
- Setup: run the T1 script on each of the 6 models.
- Verify: stature 163.6 to 167.6 cm, no extra bones; Quinn and Manny not counted as models.
- Pass condition: 6 of 6 pass every T1 item.

**Manual check: LODs and physics**
- Setup: read the mesh asset settings.
- Verify: 4 LODs within the 8.5 limits, Nanite off, the shared physics asset.
- Pass condition: 6 of 6, with the sign-off per model.

**Manual check: stock mannequin items 15 and 16**
- Setup: measure the stock mannequin.
- Verify: material slots, UV density and LOD0 triangle count recorded (OQ-2).
- Pass condition: values recorded and dated.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained screenshots in `production/qa/evidence/` (a six-model line-up) + sign-off in `production/qa/evidence/tier-a-models-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-002-body-base-blender-build-oi10.md`, `production/epics/character-pool-appearance/story-008-pool-mesh-conformance-validator.md`
- Unlocks: `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`
