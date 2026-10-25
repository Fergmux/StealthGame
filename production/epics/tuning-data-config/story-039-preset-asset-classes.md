# Story 039: Preset data-asset classes with no Config back door

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-001`, `TR-tdc-009` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD CR-2 (catalog, `Default` mandatory), CR-13 (no back doors), OQ-12 (`bTestAllowsStrikeLimitOne`)

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 1, asset model; section 6, loading)
**ADR Decision Summary**: `UStealthTuningData : UPrimaryDataAsset` is the only preset class (one `UPROPERTY` per owner system, a `_Public`/`_Server` struct pair each), and `UStealthTuningCatalog : UPrimaryDataAsset` lists the presets with `Default` mandatory. No tuning class or struct carries `Config`, `DefaultConfig`, `GlobalUserConfig` or `PerObjectConfig`, and both classes override `GetPrimaryAssetId()` so the Asset Manager rules match.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API. [V] a `UPrimaryDataAsset` otherwise reports its class name as its asset type (`DataAsset.cpp:119-120`, per the ADR), so the `GetPrimaryAssetId()` override is required for the Asset Manager cook rules to match (story 057). The cook rule itself (`AlwaysCook`, `AssetManagerTypes.h:28-53`) is [V] and belongs to story 057. ADR Verification Required (2) (cooking of catalog-only presets) is story 064, not this story. No unverified [U] claim is touched.

**Control Manifest Rules (this layer)**:
- Forbidden: Never mark tuning types `Config`/`DefaultConfig`/`GlobalUserConfig`/`PerObjectConfig`; never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable (source: ADR-0004)
- Forbidden: Never store tuning as `UDataTable` rows or per-domain assets (source: ADR-0004)
- Required: Every UCLASS in a `Server/` header declares `NotBlueprintType, NotBlueprintable` (source: ADR-0007)
- Guardrail: Tuning costs 0 per frame, under 100 KB memory (source: ADR-0004)

---

## Acceptance Criteria

*From the plan (Exit B-2), scoped to this story:*

- [x] `UStealthTuningData : UPrimaryDataAsset` (one per owner `_Public`/`_Server` struct pair, no Config/DefaultConfig/GlobalUserConfig/PerObjectConfig on any tuning class or struct) and `UStealthTuningCatalog : UPrimaryDataAsset` (entries `{PresetId, TSoftObjectPtr Preset, bHostSelectable, bTestAllowsStrikeLimitOne}`, `Default` mandatory) exist, both override `GetPrimaryAssetId()` to return `StealthTuningPreset` / `StealthTuningCatalog`, and a catalog check rejects `bTestAllowsStrikeLimitOne` on `Default` and on any HostSelectable entry. Test `StealthGame.Tuning.PresetClassesHygiene`.

---

## Implementation Notes

*Derived from ADR-0004 sections 1 and 6 and the Implementation Guidelines:*

- Files and placement: `Public/Server/StealthTuningData.h`, `Public/Server/StealthTuningCatalog.h`, `Private/Tuning/StealthTuningData.cpp` in `StealthFoundation`. Server headers include `StealthServerOnlyGuard.h` first and the classes are `UCLASS(NotBlueprintType, NotBlueprintable)`; no member carries a Blueprint specifier (a data asset edited in the editor uses `EditAnywhere` only). Log category `LogStealthFoundation`, `UE_LOGF` only.
- `UStealthTuningData` holds one `UPROPERTY` per owner system as a `_Public`/`_Server` struct pair (field name is the canonical key, CR-2). This story creates the class; the sub-structs and the eight canonical keys arrive with story 040, which adds the first members (listed in that story's Files). The class may therefore be nearly empty at the end of this story.
- `UStealthTuningCatalog` holds an array of `{ FName PresetId; TSoftObjectPtr<UStealthTuningData> Preset; bool bHostSelectable; bool bTestAllowsStrikeLimitOne; }`. `Default` is mandatory (CR-2): a catalog without it fails the check.
- Catalog check: a plain function over the entries (no asset loading, so the test uses constructed fixtures): reject `bTestAllowsStrikeLimitOne` on `Default` and on any `bHostSelectable` entry; accept it on a non-selectable non-Default entry; reject a catalog with no `Default`. The ADR says the flag lowers `NpcKillStrikeLimit`'s hard minimum from 2 to 1 for that preset in F-1 only (story 046) and is "compiled out of Shipping"; whether that means an `#if` around the struct field (which would change the cooked struct layout between configurations) or the flag being ignored in Shipping is not specified by the inputs (open); settle before the struct is serialised into the first catalog asset (story 056).
- `GetPrimaryAssetId()` overrides return `FPrimaryAssetId` with primary asset type `StealthTuningPreset` (preset class) and `StealthTuningCatalog` (catalog class). The asset-name part of the id (the object's name is the natural choice) is not specified (open). The Asset Manager type registration is story 057.
- Hygiene test: reflect over `UStealthTuningData`, `UStealthTuningCatalog` and every tuning struct and assert none has the `CLASS_Config`, `CLASS_DefaultConfig`, `CLASS_GlobalUserConfig` or `CLASS_PerObjectConfig` flag and no property carries `CPF_Config`. The same reflection hygiene is repeated for the full schema by story 044 (AC-3a); keep this test to the classes that exist now and write it so new tuning structs are picked up (walk the sub-structs of the preset class, not a hardcoded list).
- Test: `Source/StealthFoundation/Private/Tests/Tuning/PresetClassesHygieneTest.cpp`, class `FTuningPresetClassesHygieneTest`, name `StealthGame.Tuning.PresetClassesHygiene`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`, headless with `-nullrhi`.
- Prototype: replaces `UStealthSettings` (`Source/StealthGame/Public/StealthSettings.h`) as the home of tuning values. The prototype class stays until story 095 retires its canonical fields; replacement is a rewrite, not a file move, and nothing is promoted (ADR-0007 section 8). Its `Config = Game` properties are the live back door that CR-13 forbids; do not copy that pattern.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-038-schema-row-types.md`: the row struct, latch enum and constants this story builds on.
- `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`: the sub-structs, the eight canonical keys and the client and server faces.
- `production/epics/tuning-data-config/story-046-f1-validate-clamp-nan.md`: the F-1 use of `bTestAllowsStrikeLimitOne` (hard minimum 1).
- `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`: the actual `DA_Tuning*` assets under `Content/StealthGame/Tuning/`.
- `production/epics/tuning-data-config/story-057-asset-manager-primary-types.md`: Asset Manager primary type registration and the `AlwaysCook` rule.
- `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`: removing the prototype's canonical `Config` fields.

---

## Files

- `Source/StealthFoundation/Public/Server/StealthTuningData.h`
- `Source/StealthFoundation/Public/Server/StealthTuningCatalog.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningData.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/PresetClassesHygieneTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: The preset and catalog classes exist, carry no Config flags, report the right primary asset ids, and the catalog check enforces the flag rules.
  - Given: `UStealthTuningData`, `UStealthTuningCatalog` and catalog entries `{PresetId, TSoftObjectPtr Preset, bHostSelectable, bTestAllowsStrikeLimitOne}`.
  - When: reflection walks the tuning classes and structs; `GetPrimaryAssetId` is called; the catalog check runs on fixtures.
  - Then: no Config/DefaultConfig/GlobalUserConfig/PerObjectConfig flag anywhere; the ids are `StealthTuningPreset` / `StealthTuningCatalog`; the check rejects `bTestAllowsStrikeLimitOne` on `Default` and on any HostSelectable entry; it accepts the flag on a non-selectable non-Default entry; `Default` is mandatory (missing is a failure).
  - Edge cases: the flag on both `Default` and a selectable entry; an empty catalog.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/PresetClassesHygieneTest.cpp` (`StealthGame.Tuning.PresetClassesHygiene`) must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Tuning.PresetClassesHygiene`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-038-schema-row-types.md`
- Unlocks: `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`, `production/epics/tuning-data-config/story-057-asset-manager-primary-types.md`
- Open verification claims from the plan Note lines: none (the plan has no Note line for this story). Open design points: whether `bTestAllowsStrikeLimitOne` is compiled out of Shipping by `#if` on the field, and the asset-name part of the primary asset id.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: `UStealthTuningData` and `UStealthTuningCatalog` (`UPrimaryDataAsset`, `NotBlueprintType, NotBlueprintable`) carry no Config/DefaultConfig/GlobalUserConfig/PerObjectConfig flag and no `CPF_Config` property anywhere in the walked struct graph; `GetPrimaryAssetId()` returns `StealthTuningPreset` / `StealthTuningCatalog` with the object name; `UStealthTuningCatalog::CheckEntries` rejects `bTestAllowsStrikeLimitOne` on Default and on HostSelectable entries, accepts it on a non-selectable non-Default entry, and requires Default (also for an empty catalog))
**Decisions**: `bTestAllowsStrikeLimitOne` stays a field in every configuration (one cooked layout) and F-1 ignores it under `UE_BUILD_SHIPPING` (story 046), which is how "compiled out of Shipping" is met; the asset-name part of the primary asset id is the object name; the hygiene walk follows struct members (and arrays of structs) from the classes and both faces, so new sub-structs are picked up
**Deviations**: None
**Test Evidence**: `Source/StealthFoundation/Private/Tests/Tuning/PresetClassesHygieneTest.cpp`; `production/qa/evidence/story-039-preset-asset-classes/stamp.txt`; full suite 61/61 green (cycle.ps1 -Tests -NoLaunch: Result: Succeeded, tests exit=0)
**Run result**: N/A — C++ asset classes with no asset instance yet (assets are story 056); nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects left
