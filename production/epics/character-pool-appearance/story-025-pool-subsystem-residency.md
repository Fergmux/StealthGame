# Story 025: UCharacterPoolSubsystem holds the pool resident for the session

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: `UCharacterPoolSubsystem : UGameInstanceSubsystem` (server and clients) resolves `FPrimaryAssetId("CharacterPool", "DA_CharacterPool")` through `UAssetManager::GetPrimaryAssetPath` (falling back to `LoadObject` on `/Game/StealthGame/Characters/Pool/DA_CharacterPool` if the scan has not finished, risk R9), loads it once at `Initialize` with `RequestSyncLoad` in the front end, and holds it in a `UPROPERTY` for the session so it survives every seamless travel and is never collected mid-match.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Asset Manager scan timing at game-instance `Initialize` is unverified ([U], risk R9): the cooked-build boot assertion is the check. Game-instance subsystems survive seamless travel (ADR-0005). `FStreamableManager::RequestSyncLoad` exists (E8).

**Why this story**: Hard references are the residency mechanism (CP-18): loading `DA_CharacterPool` loads every mesh, material and texture; gameplay code never loads a pool asset. Read GDD CP-18 and ADR-0012 §1, Implementation Guidelines 1.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The pool is one asset with hard references, held by `UCharacterPoolSubsystem` for the session (ADR-0012)
- Forbidden: Never load a pool asset (`LoadSynchronous`, `RequestAsyncLoad`) from gameplay code after the front end; no config, settings or ini path for the pool location (TDC CR-13) (ADR-0012)
- Guardrail: Pool textures at most 40 MB; load is front-end only

---

## Acceptance Criteria

*From GDD CP-18 and ADR-0012 §1 and Guidelines 1; cooked-build assertion DEFERRED to a cooked run per QL-STORY-READY:*

- [ ] **AC-1** GIVEN a front-end boot, WHEN the game instance initialises, THEN `UCharacterPoolSubsystem` loads the pool once with `RequestSyncLoad` via `FPrimaryAssetId("CharacterPool", "DA_CharacterPool")` through `UAssetManager` (the `LoadObject` fallback is forced in a test), holds it in a `UPROPERTY` and `GetPool` is never null after `Initialize`; the boot assertion in a cooked build is DEFERRED to the first cooked run.
- [ ] **AC-2** GIVEN a lobby-to-round seamless travel and a forced garbage collection (run twice) mid-match, THEN the pool pointer is unchanged and every row's mesh, material and texture packages remain loaded with every texture `IsFullyStreamedIn`.
- [ ] **AC-3** GIVEN the CI lint `LINT-CPA-PoolLoadOnlyInSubsystem`, THEN no `LoadSynchronous` or `RequestAsyncLoad` of a pool type appears outside the subsystem (its own fallback `LoadObject` is allowlisted) and no config, ini or settings key names the pool path; a seeded violation fails the lint.

---

## Implementation Notes

- Key Interfaces: `const UCharacterPoolData* GetPool() const; const FAppearanceRow* FindRow(FAppearanceId) const; uint64 GetPoolHash() const;` (the hash comes from story 028).
- The Asset Manager settings entry (primary asset type `CharacterPool`, cook rule `AlwaysCook`) is a config change applied in story 046; here the test creates a transient pool asset and registers it.
- The subsystem asserts `r.PSOPrecache.ProxyCreationStrategy == 1` at startup (story 032 owns the failing behaviour).

**Prototype disposition**: replaces the prototype's `LoadSynchronous` at first draw (`AStealthCharacter::ApplyAppearance`), a pop correlated with first use.

---

## Out of Scope

- Story 028: `PoolHash`; Story 032: warm-up; Story 046: the real asset and Asset Manager entry

---

## Files

- `Source/StealthGameplay/Public/View/CharacterPool/CharacterPoolSubsystem.h`
- `Source/StealthGameplay/Private/View/CharacterPool/CharacterPoolSubsystem.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/PoolSubsystemResidencyTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; cooked boot assertion deferred).*

**Test: pool residency at boot**
- Given: a front-end boot.
- When: the game instance initialises.
- Then: the subsystem loads the pool once by `RequestSyncLoad` of the primary asset id, with the `LoadObject` fallback (forced in a test); `GetPool` is non-null after `Initialize`.
- Edge cases: the cooked boot assertion is DEFERRED to a cooked run.

**Test: residency across travel and GC**
- Given: a lobby-to-round seamless travel.
- When: a forced GC runs mid-match.
- Then: the pool pointer is unchanged; every row's mesh, material and texture packages are loaded and every texture `IsFullyStreamedIn`.
- Edge cases: GC run twice.

**Test: load lint**
- Given: `LINT-CPA-PoolLoadOnlyInSubsystem`.
- When: it scans for `LoadSynchronous` and `RequestAsyncLoad` of pool types outside the subsystem and for any ini or settings key naming the pool path.
- Then: zero hits; a seeded violation fails.
- Edge cases: the subsystem's own fallback is allowlisted.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/CharacterPool/PoolSubsystemResidencyTest.cpp` — automation test `StealthGame.CharacterPool.Pool.SubsystemResidency` (OR documented playtest for the cooked-build boot)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-005-pool-row-schema-and-lookup.md`, `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`
- Unlocks: `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`, `production/epics/character-pool-appearance/story-031-spike-cpa-v2-v3-pso-precache-and-residency.md`, `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`
