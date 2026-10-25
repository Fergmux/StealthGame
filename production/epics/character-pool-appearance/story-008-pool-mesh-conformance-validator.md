# Story 008: Pool mesh conformance validator (skeleton, arm geometry, LODs, uniform skin cache)

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`, `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: The validator requires every pool mesh to use the project skeleton, have 4 LODs, Nanite disabled, at most 4 influences, the shared physics asset, an identical `SkinCacheUsage` on every LOD of every pool mesh, one `bSupportRayTracing` value, no optional mips on pool textures, and reference-pose proportions within the art bible tolerance of the reference.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: `ESkinCacheUsage` is per LOD (`FSkeletalMeshLODInfo::SkinCacheUsage`); the project sets `r.SkinCache.DefaultBehavior=0` (Exclusive); `bSupportRayTracing` forces a mesh into the skin cache; `IsFullyStreamedIn()` is never true for textures with optional mips (ADR-0012 E5, E12). Fixtures are committed numeric golden data, not live asset loads, so the unit test needs no editor assets.

**Why this story**: ADR-0012's validator is the automated half of T1 and "the check that rejects the prototype's Meshy citizens (arms about 17 % short)". **Finding to carry:** the 2026-10-25 spike measured `SK_Citizen02-05` as skinned to the UEFN skeleton with identical bone chains (`production/qa/spikes/2026-10-25_S1-S2-S14.md` section 4) and with one extra root bone (89 bones against 88). The 17 % shortfall is therefore in the **mesh geometry**, not the skeleton, so a pure chain-length check would pass them; the discriminating check is the arm-geometry extent below. Read ADR-0012 §1 (validator bullet) and art bible 1.3.2, 1.3.5.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every pawn mesh keeps an empty per-component `SkinCacheUsage` override array; pool meshes use the project's one skeleton with zero extra bones (ADR-0012, ADR-0013)
- Forbidden: Never ship a pool mesh that differs in skin-cache, ray-tracing or LOD policy from the rest (CP-16: one look for everyone)
- Guardrail: Validator runs on save and CI; no runtime cost

---

## Acceptance Criteria

*From ADR-0012 §1 and GDD AC-CP-22; AC-1 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN a skeletal mesh and the reference constants from story 001 (data file), WHEN the validator measures arm extent (the distance along the clavicle-to-hand chain axis between the extreme vertices with weight over 0.5 to that chain) and the nine bone chains, THEN a mesh passes only if the extent and every chain are within the tolerance stated in story 001 (proposed ± 3 %) of the same measurement on `SK_UEFN_Mannequin`'s body mesh; committed golden measurements of `SK_UEFN_Mannequin` pass and committed measurements of `SK_Citizen02` to `05` (arms about 17 % short) fail with a message naming the arm chain.
- [ ] **AC-2** GIVEN a mesh whose skeleton has a bone beyond the reference's 88 (the prototype root bone `SKM_UEFN_Mannequin`), THEN it fails with "extra bone" naming that bone.
- [ ] **AC-3** GIVEN pool mesh descriptors, THEN every pool mesh has 4 LODs, Nanite off, at most 4 influences, the shared physics asset, an identical `SkinCacheUsage` on every LOD of every pool mesh, one `bSupportRayTracing` value across the pool, and no optional mips on any pool texture.

---

## Implementation Notes

*Derived from ADR-0012 §1 and E5/E12:*

- Measure the golden values for the committed fixtures once in the editor (reference + the four citizens) and commit them as text; the unit test reads the text only. Do not invent the citizens' numbers; the 17 % figure is from `production/first-playable-report.md` known issue 3.
- The value of `SkinCacheUsage` itself is ADR-0022's (spike R2); this validator enforces only uniformity.
- The validator also feeds story 003's AC-2 (arm-extent within tolerance) so the Blender-side check and the editor-side check use one definition: write the metric definition in a comment at the top of the header.

**Prototype disposition**: the prototype's `SK_Citizen02-05` (`/Game/StealthGame/Characters/Pool/`) are the negative golden fixtures; they are not promoted and are removed from the pool in story 052.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 006 and 007: swatch, size and material rules; Story 046: running on the real pool
- Story 053: the Body_Base surface-deviation (95th percentile ≤ 1.5 cm) conform test for Meshy bodies

---

## Files

- `Source/StealthGameEditor/Private/CharacterPool/CharacterPoolMeshValidator.cpp`
- `Source/StealthGameEditor/Private/Tests/CharacterPool/MeshConformanceRulesTest.cpp`
- `Source/StealthGameEditor/Private/Tests/CharacterPool/Golden/` (reference and citizen measurements)

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-1 revised with the qa-lead's proposed text; not re-run (unattended; decision logged). Test `StealthGame.CharacterPool.Pool.MeshConformanceRules` (editor module).*

**Test: arm-extent conformance**
- Given: committed golden structs (reference, `SK_Citizen02` to `05`) and the reference constants file.
- When: the validator measures them.
- Then: the reference passes; each citizen fails naming the arm chain; the tolerance window is derived from the data file, not hard-coded.
- Edge cases: exactly at the tolerance; a mesh with correct chains but short geometry (the spike case).

**Test: extra bone**
- Given: a bone list of 89 with a root named `SKM_UEFN_Mannequin`.
- When: validating.
- Then: it fails with "extra bone" naming that bone.
- Edge cases: 87 bones (missing bone, separate error).

**Test: pool mesh settings**
- Given: fixture mesh descriptors.
- When: validating.
- Then: pass needs 4 LODs, Nanite off, at most 4 influences, the shared physics asset, an identical `SkinCacheUsage` on every LOD of every mesh, one `bSupportRayTracing` value, no optional mips.
- Edge cases: 3 LODs; Nanite on; one LOD with a different `SkinCacheUsage`; one mesh with a different `bSupportRayTracing`.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameEditor/Private/Tests/CharacterPool/MeshConformanceRulesTest.cpp` — automation test `StealthGame.CharacterPool.Pool.MeshConformanceRules` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-001-oi1-uefn-reference-measurements-recorded.md`, `production/epics/character-pool-appearance/story-005-pool-row-schema-and-lookup.md`
- Unlocks: `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`, `production/epics/character-pool-appearance/story-053-meshy-first-six-conform-gate.md`
