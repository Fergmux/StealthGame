# Story 006: Swatch assignment is a rule (F-7), checked and rewritten by the validator

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: The editor-time pool validator (asset save and CI) recomputes swatch indices from F-7 and overwrites them, so hand edits fail CI; ids are unique, 0 to 23 and never 255; for every reachable `K` (5 to 16) at least one valid subset exists or the pool is reported.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); this story is a pure function plus an editor save hook
**Engine Notes**: The validator lives in `StealthGameEditor` (ADR-0007). Keep the F-7 function pure and in `StealthGameplay` so the editor module and tests share it; only the pre-save hook is editor-only.

**Why this story**: CP-3 makes swatch assignment a rule (art bible 4.3) so it is never a hand choice. Read GDD CP-1 to CP-3, Formulas F-7 and AC-CP-21 by section.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Character-pool editor validator tests live in `Source/StealthGameEditor/Private/Tests/CharacterPool/`; swatch and trim indices are written by the validator, never hand-edited (ADR-0012)
- Forbidden: Never hand-edit `GarmentSwatchIndex` / `TrimSwatchIndex`; never derive `AppearanceId` from model, variant or pool size
- Guardrail: Validator runs on asset save and in CI; no runtime cost

---

## Acceptance Criteria

*From GDD AC-CP-21 (CP-1 to CP-3, F-7); AC-3 revised and AC-4 added per QL-STORY-READY:*

- [ ] **AC-1** GIVEN pool tables with `N_models` in {6, 7, 8}, WHEN the validator runs, THEN every `GarmentSwatchIndex` and `TrimSwatchIndex` equals F-7 (`Default` garment S(k), `GarmentSwap` garment S(k+1), trim S(k+3) in every variant, `Accessory` garment S(k); 1-based modulo N; material parameter = swatch minus 1).
- [ ] **AC-2** GIVEN the same tables, THEN every swatch occurs on exactly 3 distinct models across the garment-default, swap and trim roles.
- [ ] **AC-3** GIVEN a descriptor with ids {0..23}, THEN `AppearanceId` values are unique, within 0 to 23 and never 255, and (a) a descriptor with a hand-edited swatch index makes the validator report an error naming the row, (b) the pre-save hook rewrites all indices from F-7, after which the validator passes.
- [ ] **AC-4** GIVEN each of the 6, 7 and 8 model pools, THEN for every `K` from 5 to 16 at least one valid stratified subset exists or the validator reports the pool and `K` (uses the subset table of story 011 through its interface).

---

## Implementation Notes

*Derived from GDD CP-3 and F-7, ADR-0012 §1 (pool validator):*

- F-7 is a pure function `(ModelIndex k, N, VariantKind) -> (GarmentSwatch, TrimSwatch)` with 1-based modulo arithmetic; keep the oracle in the test independent of it (spell the expected table out by hand for N = 6, 7, 8).
- The Accessory-variant garment index S(k) is an **assumption** recorded in CP-3 (art bible 4.3 is silent); do not change it here.
- The pre-save hook (`StealthGameEditor`) calls the pure function and overwrites; CI runs the same validator headless.
- AC-4 depends on the subset table interface from story 011; until it lands, test with a stub table.

**Prototype disposition**: none; the prototype had no swatch rule (`M_CrowdTint` hue tint).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 007: pool size, material and accessory rules; Story 008: mesh and skeleton rules
- Story 011: building the valid-subset table

---

## Files

- `Source/StealthGameplay/Public/View/CharacterPool/SwatchRule.h`
- `Source/StealthGameEditor/Private/CharacterPool/CharacterPoolValidator.cpp` (swatch + id checks, pre-save hook)
- `Source/StealthGameEditor/Private/Tests/CharacterPool/SwatchRuleTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-3 revised and AC-4 added with the qa-lead's proposed text; not re-run (unattended; decision logged). Test `StealthGame.CharacterPool.Pool.SwatchRule` (editor module).*

**Test: F-7 swatch indices**
- Given: in-memory descriptors for N in {6, 7, 8}, expected indices from an independent oracle in the test (1-based modulo N).
- When: the validator runs.
- Then: Default garment S(k), GarmentSwap S(k+1), trim S(k+3) in every variant, Accessory garment S(k); material parameter = swatch minus 1.
- Edge cases: wrap-around at k = N and N-2 to N; adding a 7th model changes no earlier index.

**Test: swatch occurrence count**
- Given: the same descriptors.
- When: roles are tallied per swatch across garment-default, swap and trim.
- Then: every swatch occurs on exactly 3 distinct models.
- Edge cases: N = 6, 7 and 8 separately.

**Test: id validity and hand-edit handling**
- Given: a descriptor with ids {0..23} and a copy with one swatch changed.
- When: validating, then running the save hook.
- Then: ids unique, in 0..23, never 255; the edited copy errors with the row named; after the hook it validates clean.
- Edge cases: duplicate id, id 255, id 24.

**Test: reachable-K sweep**
- Given: the N = 6, 7, 8 pools.
- When: K runs from 5 to 16.
- Then: a valid subset exists or the pool is reported with K.
- Edge cases: K = 16 at N = 6.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameEditor/Private/Tests/CharacterPool/SwatchRuleTest.cpp` — automation test `StealthGame.CharacterPool.Pool.SwatchRule` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-005-pool-row-schema-and-lookup.md`
- Unlocks: `production/epics/character-pool-appearance/story-007-pool-shape-and-material-rules-validator.md`, `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`
