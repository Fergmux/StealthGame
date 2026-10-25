# Story 007: Pool shape and material rules validator

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
**ADR Decision Summary**: The validator enforces 6 to 8 models and 12 to 24 appearances, one static `UMaterialInstanceConstant` per row whose parent is `M_Character_Master` with no `Team`, `Role`, `IsPlayer`, `IsNPC` or `Ally` parameter or switch, an opaque master with no WPO and no masked opacity, and accessory variants as separate mesh assets.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); editor-only asset inspection
**Engine Notes**: Fixtures are in-memory descriptors and transient `UMaterial` objects so the tests run before pool assets exist; the real-asset run is story 046. Material blend mode, WPO connection and parameter enumeration are read through standard `UMaterial` / `UMaterialInstance` APIs (no post-cutoff API).

**Why this story**: CP-2 (size limits), CP-4 (accessory variants as separate meshes) and CP-16 (no keyed material parameters, one look path) are enforceable at asset time, and AC-CP-08c wants `PoolBelowMvpFloor` logged. Read GDD CP-2, CP-4, CP-16, AC-CP-08c, AC-CP-22.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One merged single-slot mesh per model/accessory variant and one `UMaterialInstanceConstant` per appearance (ADR-0012)
- Forbidden: Never create a `UMaterialInstanceDynamic`, set a material parameter, scale, tint or add a component per pawn; no parameter or switch named `Team`, `Role`, `IsPlayer`, `IsNPC`, `Ally` (ADR-0012, art bible 8.x.6)
- Guardrail: Validator runs on save and in CI; zero runtime cost

---

## Acceptance Criteria

*From GDD AC-CP-08c and AC-CP-22; AC-2 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN a pool descriptor, THEN a shipping pool needs 6 to 8 models and 12 to 24 appearances; below that `PoolBelowMvpFloor` is logged and the pool gate fails (development builds still run), above 24 appearances or 8 models is an error.
- [ ] **AC-2** GIVEN row fixtures, THEN every row's material is a `UMaterialInstanceConstant` whose parent is `M_Character_Master`, no parameter or switch is named (case-insensitively) `Team`, `Role`, `IsPlayer`, `IsNPC` or `Ally`, and the master check rejects each of three transient `UMaterial` fixtures (masked blend mode, a WPO input connected, a banned parameter name) and accepts an opaque fixture; the real-asset run is story 046.
- [ ] **AC-3** GIVEN an `Accessory` row and a `GarmentSwap` row, THEN the Accessory row references its own mesh asset with its own LODs, and the GarmentSwap row shares the Default row's mesh and differs only in material instance.

---

## Implementation Notes

*Derived from ADR-0012 §1 (pool validator) and GDD CP-2, CP-4:*

- Share the validator class with story 006 (`CharacterPoolValidator`); expose each rule as a small function returning a structured error (rule id, row, message) so CI output names the row.
- The MVP floor (6 models x 2 variants = 12) fails the **pool gate** below it but a development pool may still run (the allocator's `Warn` policy covers it); keep the gate separate from the allocator.
- A runtime MID on a pawn is rejected in story 042's lint and story 040's test, not here; here, a row whose `Material` is not an MIC fails.

**Prototype disposition**: replaces the prototype `M_CrowdTint` hue-tint approach (a per-pawn MID) which CP-16 forbids; nothing promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 006: swatch rule and ids; Story 008: mesh, skeleton and LOD rules
- Story 046: running these validators on the real `DA_CharacterPool`

---

## Files

- `Source/StealthGameEditor/Private/CharacterPool/CharacterPoolValidator.cpp` (shape and material rules)
- `Source/StealthGameEditor/Private/Tests/CharacterPool/ShapeAndMaterialRulesTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 revised with the qa-lead's proposed text; not re-run (unattended; decision logged). Tests `StealthGame.CharacterPool.Pool.ShapeAndMaterialRules` and `StealthGame.CharacterPool.Deck.ValidationLadder.PoolFloor`.*

**Test: pool size gate**
- Given: descriptors of 5, 6, 8 and 9 models and of 11, 12, 24 and 25 appearances.
- When: the validator runs.
- Then: 6 to 8 models and 12 to 24 appearances pass; anything else logs `PoolBelowMvpFloor` (below) or the over-limit error and the pool gate fails; a development allocation on a 5-model pool still runs.
- Edge cases: exact boundaries on both axes.

**Test: material rules**
- Given: an MIC child of the master, an MIC child of another parent, a `UMaterialInstanceDynamic`, and the four master fixtures (opaque, masked, WPO connected, banned parameter name).
- When: validating.
- Then: only the conforming MIC passes; the MID, wrong-parent MIC and the three bad masters each fail with a distinct error code.
- Edge cases: banned names in mixed case; a static-switch parameter named Role.

**Test: accessory versus swap meshes**
- Given: an Accessory row and a GarmentSwap row.
- When: validating.
- Then: the Accessory row has its own mesh asset with its own LODs; GarmentSwap shares the Default row's mesh and differs only in material instance.
- Edge cases: an Accessory sharing the Default mesh (fail); a swap with its own mesh (fail).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameEditor/Private/Tests/CharacterPool/ShapeAndMaterialRulesTest.cpp` — automation tests `StealthGame.CharacterPool.Pool.ShapeAndMaterialRules` and `StealthGame.CharacterPool.Deck.ValidationLadder.PoolFloor` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-005-pool-row-schema-and-lookup.md`, `production/epics/character-pool-appearance/story-006-swatch-rule-f7-validator.md`
- Unlocks: `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`, `production/epics/character-pool-appearance/story-024-tier-c-fixture-tier-independence.md`
