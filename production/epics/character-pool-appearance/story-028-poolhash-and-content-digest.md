# Story 028: PoolHash and ContentDigest

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load. The `PoolHash` rides in Session's single round acknowledgement, CP-18.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: `PoolHash` = ADR-0004's FNV-1a-64 over the canonical byte stream `PoolSchemaVersion || RowCount || per row in AppearanceId order: (AppearanceId, ModelIndex, VariantKind, GarmentSwatchIndex, TrimSwatchIndex, UTF-8 mesh path, UTF-8 material path, ContentDigest)`, fixed-width little-endian, computed once per process; `ContentDigest` = FNV-1a-64 over the sorted hard-dependency closure of the row's mesh and material using `GetPackageSavedHash`, written by the editor validator.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: `FAssetPackageData::GetPackageSavedHash()` and the Asset Registry dependency closure exist (E7). Hash only authored data and editor-written digests, never runtime pointers or cooked offsets (risk R7); cross-process identity is the spike in story 029.

**Why this story**: A client whose local pool differs must be caught by Session's acknowledgement (CP-18, AC-CP-19). Read GDD CP-18 and ADR-0012 §1 (`PoolHash` bullet), ADR-0004 for the FNV-1a-64 helper.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `PoolHash` is public (identifies content, not a secret), computed once per process by `UCharacterPoolSubsystem`, identically on server, listen host and clients (ADR-0012)
- Forbidden: Never feed a runtime pointer or cooked offset into the hash (ADR-0012 R7)
- Guardrail: computed once; negligible

---

## Acceptance Criteria

*From ADR-0012 §1 (`PoolHash`, `ContentDigest`) and Validation Criteria:*

- [ ] **AC-1** GIVEN a fixed synthetic pool (committed fixture), THEN `PoolHash` equals a committed golden FNV-1a-64 value over the canonical stream (`PoolSchemaVersion`, `RowCount`, per row in id order: id, `ModelIndex`, `VariantKind`, swatches, UTF-8 mesh path, UTF-8 material path, `ContentDigest`; fixed-width little-endian), the golden produced by an independent reference implementation.
- [ ] **AC-2** GIVEN the synthetic pool, WHEN each row field is changed in turn (including a path differing only in case) and the rows are stored in a different order, THEN every field change alters the hash and re-ordering does not.
- [ ] **AC-3** GIVEN a fixture package set with known saved hashes, WHEN the editor validator writes `ContentDigest`, THEN it equals FNV-1a-64 over the sorted hard-dependency closure of `GetPackageSavedHash` (a dependency added to the closure changes it) and `LINT-CPA-NoHashFromRuntimePtr` finds no runtime pointer or cooked offset feeding the hash.

---

## Implementation Notes

- Test name proposed by the qa-lead: `StealthGame.CharacterPool.Pool.PoolHashGolden`.
- `PoolHash` is computed in `UCharacterPoolSubsystem` after load (story 025); the pure hash function lives in `StealthGameplay/Public/View/CharacterPool/PoolHash.h` so tests need no subsystem.
- Register `LINT-CPA-NoHashFromRuntimePtr` with story 042's harness.

**Prototype disposition**: none.

---

## Out of Scope

- Story 029: cross-process identity (CPA-V5); Story 033: the acknowledgement integration

---

## Files

- `Source/StealthGameplay/Public/View/CharacterPool/PoolHash.h`
- `Source/StealthGameEditor/Private/CharacterPool/CharacterPoolValidator.cpp` (`ContentDigest` writer)
- `Source/StealthGameplay/Private/Tests/CharacterPool/PoolHashGoldenTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead).*

**Test: golden hash**
- Given: a fixed synthetic pool.
- When: `PoolHash` is computed over the canonical stream.
- Then: it equals the committed golden FNV-1a-64 value.
- Edge cases: the golden comes from an independent reference implementation.

**Test: sensitivity and order**
- Given: the synthetic pool.
- When: each row field is changed in turn, and rows are stored in a different order.
- Then: every field change alters the hash; re-ordering does not.
- Edge cases: a path differing only in case.

**Test: content digest**
- Given: a fixture package set with known saved hashes.
- When: the validator writes `ContentDigest`.
- Then: it equals FNV-1a-64 over the sorted dependency closure of `GetPackageSavedHash`; the lint finds no runtime pointer or cooked offset feeding the hash.
- Edge cases: a dependency added to the closure changes the digest.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/PoolHashGoldenTest.cpp` — automation test `StealthGame.CharacterPool.Pool.PoolHashGolden` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-005-pool-row-schema-and-lookup.md`, `production/epics/character-pool-appearance/story-006-swatch-rule-f7-validator.md`
- Unlocks: `production/epics/character-pool-appearance/story-029-spike-cpa-v5-poolhash-cross-process.md`, `production/epics/character-pool-appearance/story-043-pool-evidence-freshness-ci.md`
