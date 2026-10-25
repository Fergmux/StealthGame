# Story 005: Pool row schema, UCharacterPoolData and O(1) id lookup

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
**ADR Decision Summary**: The pool is one hard-referenced `UCharacterPoolData : UPrimaryDataAsset` of `FAppearanceRow`s; `AppearanceId` is an authored, stable key 0 to 23 (never derived), 255 is the `Unassigned` sentinel, and `FindRow` is O(1) through a 256-entry index built in `PostLoad`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); this story's surface is plain UObject data
**Engine Notes**: `UPrimaryDataAsset` and `GetPrimaryAssetId` override only (verified E7/E8 in ADR-0012). Hard `TObjectPtr` references are the residency mechanism (loading the asset loads every mesh and material).

**Why this story**: Every other story reads the pool through this schema. It fixes the row fields (CP-1), the stable-id rule, the sentinel and the lookup before the allocator, validators and apply path are written. GDD sections to read: Detailed Design CP-1, CP-2 and the Tuning Knobs paragraph on pool content (`bash .claude/scripts/md-section.sh extract design/gdd/character-pool-appearance.md "Detailed Design"`; the GDD is 130 KB, never read whole).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The pool is one `UCharacterPoolData` asset with hard references to every mesh and static material instance, held by `UCharacterPoolSubsystem` for the session (ADR-0012)
- Forbidden: Never use a `UDataTable` pool, soft references with a per-round load of only the active `K` rows, or build appearances at run time (ADR-0012)
- Guardrail: Pool textures at most 40 MB (ADR-0012 / art bible 8.x)

---

## Acceptance Criteria

*From GDD CP-1, CP-2, AC-CP-21 (id rules) and ADR-0012 §1, scoped to this story:*

- [ ] **AC-1** GIVEN a `UCharacterPoolData` with rows of ids {0, 1, 5, 23}, WHEN `PostLoad` runs, THEN `FindRow` returns the row for each present id and null for 2, 24 and 255 through a 256-entry index (O(1)), and `FAppearanceId::Unassigned` equals 255.
- [ ] **AC-2** GIVEN `FAppearanceRow`, THEN `Mesh` and `Material` are hard `TObjectPtr` (reflection finds no soft-object property on the row) and `GetPrimaryAssetId` returns `FPrimaryAssetId("CharacterPool", GetFName())`.
- [ ] **AC-3** GIVEN a fixed fixture of 6 models, WHEN a 7th model's rows are added on unused ids, THEN every existing row keeps its id (ids are authored, never derived from model, variant or pool size).

---

## Implementation Notes

*Derived from ADR-0012 §1 (Key Interfaces, `FAppearanceRow`, `UCharacterPoolData`):*

- Fields per row: `AppearanceId` (uint8 authored 0 to 23), `ModelIndex` (uint8, 1-based pool-table model order, F-7), `ModelKey` (FName, stable model identity for stratification), `VariantKind` (`Default | GarmentSwap | Accessory`), `Mesh` (hard `TObjectPtr<USkeletalMesh>`), `Material` (hard `TObjectPtr<UMaterialInstanceConstant>`), `GarmentSwatchIndex` and `TrimSwatchIndex` (`VisibleAnywhere`, written by the validator in story 006, never hand-edited), `ContentDigest` (uint64, written by the validator in story 028).
- `Rows` stored sorted by `AppearanceId`; `PoolSchemaVersion` = 1.
- Header lives at `Source/StealthGameplay/Public/View/CharacterPool/CharacterPoolData.h` (client-safe, ADR-0007 `Public/View`). `FAppearanceId { uint8 Value = 255; static constexpr uint8 Unassigned = 255; }`.
- Doc comments on every public type (coding standards). No gameplay value is hard-coded here.

**Prototype disposition**: replaces the prototype pool entries in `Source/StealthGame/Public/StealthSettings.h` and `Config/DefaultGame.ini` (four Meshy citizens + `M_CrowdTint`); nothing is promoted. Removal happens in story 052.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 006 (`story-006-swatch-rule-f7-validator.md`): swatch index rule and id uniqueness validation
- Story 025 (`story-025-pool-subsystem-residency.md`): loading and holding the asset
- Story 028 (`story-028-poolhash-and-content-digest.md`): `PoolHash` and `ContentDigest`

---

## Files

- `Source/StealthGameplay/Public/View/CharacterPool/CharacterPoolData.h`
- `Source/StealthGameplay/Private/View/CharacterPool/CharacterPoolData.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/PoolRowSchemaAndLookupTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Test `StealthGame.CharacterPool.Pool.RowSchemaAndLookup`, class `FCharacterPoolPoolRowSchemaAndLookupTest`.*

**Test: FindRow by id**
- Given: a transient `UCharacterPoolData` with rows {0, 1, 5, 23}.
- When: `PostLoad()` is called.
- Then: `FindRow` returns the row for 0, 1, 5, 23 and null for 2, 24, 255; `FAppearanceId::Unassigned == 255`.
- Edge cases: an empty table; ids at 0 and 255.

**Test: hard references and primary asset id**
- Given: `FAppearanceRow` reflection.
- When: its properties are enumerated.
- Then: no `FSoftObjectProperty` exists; Mesh and Material are `FObjectProperty`; `GetPrimaryAssetId()` equals `FPrimaryAssetId("CharacterPool", GetFName())`.
- Edge cases: the asset is renamed and the id follows `GetFName()`.

**Test: stable ids on model addition**
- Given: a 6-model fixture with fixed ids.
- When: a 7th model's rows are added on unused ids.
- Then: every pre-existing row keeps its id; new rows use free ids.
- Edge cases: gaps in the id range; an added row on a used id is rejected.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/PoolRowSchemaAndLookupTest.cpp` — automation test `StealthGame.CharacterPool.Pool.RowSchemaAndLookup` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md` (ADR-0007 module skeletons)
- Unlocks: `production/epics/character-pool-appearance/story-006-swatch-rule-f7-validator.md`, `production/epics/character-pool-appearance/story-007-pool-shape-and-material-rules-validator.md`, `production/epics/character-pool-appearance/story-025-pool-subsystem-residency.md`
