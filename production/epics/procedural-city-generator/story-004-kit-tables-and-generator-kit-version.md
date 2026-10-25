# Story 004: Kit tables and `GeneratorKitVersion`

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Config/Data
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. The version constant is what makes golden seeds stable across builds, R-G11.3.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0008 for the bump rule and goldens)
**ADR Decision Summary**: Kit tables are copied into plain structs at `Build` entry so the worker task holds no `UObject`; `GeneratorKitVersion` covers stage code, kit content, slab and stack tables, the collision strategy and the `ClueTag` rule version, so `LayoutHash` covers them. A hash change without a bump fails CI as "unbumped drift" (ADR-0008 section 9).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: The table is plain C++ data (constexpr or a versioned data table converted at `Build` entry); kit meshes are referenced by soft path and resolved on the game thread (story 025 treats a missing asset as `GeneratorConfigInvalid`). No `.uasset` is created or modified by this story: the Meshy kit pieces `SM_Kit_House`, `SM_Kit_Temple`, `SM_Kit_Stall` are inputs to the production kit, not the G0 modules.

**Control Manifest Rules (this layer)**:
- Required: Golden vectors, including the Generator goldens, are committed and a primitive change replaces its vectors in the same change; `ClueTag` is a pure function of the published geometry and its rule version is part of `GeneratorKitVersion` — source: ADR-0006, ADR-0014
- Forbidden: Never ship a kit table edit without bumping `GeneratorKitVersion`; never let the Generator read kit data from the world during the worker task — source: ADR-0008, ADR-0014
- Guardrail: per-module simple collision <= 4 primitives, stacks <= 2, slabs 1 box; <= 20,000 shapes — source: ADR-0014

Tier G0 (before the mass-production gate, art bible 8.x.14) is the graybox module set plus derived assets the user allowed on 2026-10-25 (baked bay stacks, slab sizes, primitive placeholders). Tier G1 is data that arrives after the gate.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (Visual/Audio "Kit content required", R-G6.3-4, R-G11.3), scoped to this story:*

- [ ] **AC1** (`Kit.TierG0Table`): **GIVEN** the G0 kit table **THEN** it lists the 8 graybox modules at one variant each (M01 blank, M02, M03, M04, M05, M06, M09 civic, M10 edge + water; vault and climb variants flagged as traversal pieces), the 4 derived bay stacks, the slab sizes 2x2, 2x4, 2x8, 4x4 and 8x8, with dimensions on the 2.0 m grid and 1.6 m course and collision primitive counts (modules <= 4, stacks <= 2, slabs 1).
- [ ] **AC2** (R-G11.3, `Kit.VersionBumpRule`): **GIVEN** the table, slab and stack tables, collision strategy and `ClueTag` rule version **THEN** `GeneratorKitVersion` is one constant stored beside their content hash, and a table edit without a version bump fails the test as "unbumped kit table".
- [ ] **AC3** (G0/G1 tiers, `Kit.TierSelectionIsData`): **GIVEN** tier G0 **THEN** canals are disabled (L_c = 0), `Causeway` blocks become Residential and the temple is the stand-in platform; **AND GIVEN** a synthetic G1 kit table supplied as a data asset only **THEN** the same stage code runs to a complete candidate, and a lint finds no tier identifier under `CityGenerator/` stage files.

---

## Implementation Notes

*Derived from GDD Visual/Audio (Kit content required) and R-G6:*

Module ids M01-M12 follow art bible 6.2. G0 data (GDD): M01 blank (vault and climb variants are traversal pieces and need producer approval), M02, M03, M04, M05, M06, M09 civic, M10 edge + water; the 4 derived bay stacks (M01+M04, M02+M04, M01+M03+M04, M02+M03+M04) and the five slab sizes are baked by script from the same module sources (derived assets, allowed before the gate by user decision 2026-10-25; the baking script and the mitred M05 cornice request are art-pipeline work tracked by `/asset-spec system:procedural-city-generator`). Primitive placeholders (planter + bush silhouette with the real outline per LSC VA-5, court fixture, 2-3 ground props) are table entries too. G0 limits: Residential and stall-less Market only, canals disabled (no M11 bridges), temple stand-in = M01 climb + M09 civic + a sealed mass, variety from rhythm only. G1 adds M07, M08, M09 temple, M11 deck and bridge, M12 awning + counter, an M01 frieze variant and a second M03 window variant (up to 6 stacks), the real planter and bush family, the court fixture family (>= 3 decor copies per site), prop families >= 3 per area type and the ground-class decoration set.

`GeneratorKitVersion` is a manually bumped integer constant stored next to an FNV-1a hash of the canonical serialisation of the tables, the collision-strategy id and the `ClueTag` rule version; the test recomputes the hash and fails if it differs from the stored one while the version is unchanged. Selecting a tier is a data switch: stage code reads capabilities from the table (for example `bCanalsEnabled`), never the tier enum. Table content that is not yet art-approved (traversal variants) carries a `bNeedsProducerApproval` flag the test asserts is set where the GDD says so.

Prototype replaced: the prototype's `SM_Kit_House`, `SM_Kit_Temple`, `SM_Kit_Stall` Meshy pieces and the Blender export procedure remain inputs (story 045 asserts they are untouched); no prototype table is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-019-placement-bay-stacks-and-manifest-emission.md`: placing stacks and slabs and emitting entries.
- `story-025-failure-modes-and-teardown.md`: `GeneratorConfigInvalid` when the table or an asset is missing.
- `story-031-golden-seeds-and-bump-rule.md`: the golden-seed side of the bump rule.
- `story-040-stack-and-slab-visual-equivalence.md`: the baked stack and slab assets and their visual proof.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenKitTable.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenKitTable.cpp` (tier G0 data, `GeneratorKitVersion`)
- `Source/StealthGameplay/Private/Tests/CityGen/TierG0TableTest.cpp` (class `FCityGenKitTierG0TableTest`, name `StealthGame.CityGen.Kit.TierG0Table`)
- `Source/StealthGameplay/Private/Tests/CityGen/VersionBumpRuleTest.cpp` (class `FCityGenKitVersionBumpRuleTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/TierSelectionIsDataTest.cpp` (class `FCityGenKitTierSelectionIsDataTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after the revision of AC3).*

- **AC1** `Kit.TierG0Table`
  - Given: the G0 kit data asset.
  - When: it is validated.
  - Then: the 8 modules are M01 blank, M02, M03, M04, M05, M06, M09 civic and M10 edge+water, one variant each; the vault and climb variants are flagged as traversal pieces; there are 4 derived stacks and the slab set is exactly {2x2, 2x4, 2x8, 4x4, 8x8}; dimensions are multiples of 2.0 m horizontally and 1.6 m vertically; collision primitives are at most 4 per module, at most 2 per stack and exactly 1 per slab.
  - Edge cases: a 9th module, a second variant or an off-grid dimension each fail (fixtures).
- **AC2** `Kit.VersionBumpRule`
  - Given: the tables, slab and stack tables, collision strategy and `ClueTag` rule version.
  - When: the content hash is recomputed.
  - Then: `GeneratorKitVersion` is one constant stored beside a hash that matches the recomputed one.
  - Edge cases: one edited value with no bump fails with "unbumped kit table". The same edit with a bump and new hash passes (negative control). The hash uses canonical serialisation.
- **AC3** `Kit.TierSelectionIsData`
  - Given: tier G0, and separately a synthetic G1 table supplied as a data asset only.
  - When: 50 seeds run at G0, then one seed runs with the G1 asset.
  - Then: G0 gives L_c = 0 on every seed, `Causeway` blocks typed Residential, and the temple as the stand-in platform. The same stage binaries run the G1 asset to a complete candidate, meaning Manifest emission through S12. The lint finds 0 tier identifiers (the tier enum values and "G0"/"G1" literals) under the `CityGenerator/` stage files.
  - Edge cases: a seeded `if (Tier == G1)` in a stage file is caught (negative control). The lint's identifier list comes from the tier enum, not hardcoded. The G1-asset clause needs stories 005-019 to exist; it runs as the last check of the epic's first end-to-end candidate and may be deferred until then.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass — `production/qa/smoke-[date].md` (the three tests above run in the smoke set)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-001-citygen-tuning-keys-invariants-and-params.md`
- Unlocks: `story-005-grid-model-s1-frame-and-f1-walkable-target.md` (tier capabilities), `story-019-placement-bay-stacks-and-manifest-emission.md`, `story-025-failure-modes-and-teardown.md`, `story-031-golden-seeds-and-bump-rule.md`
