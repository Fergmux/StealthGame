# Story 057: Asset Manager primary types with AlwaysCook

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-001` (text lives in `docs/architecture/tr-registry.yaml`); GDD CR-2 (cooked data assets, presets listed by a catalog)

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. A preset that the catalog references only through a soft reference could be left out of a cooked build, and then a host selecting it in a cooked Development build would silently fall back to `Default`. ADR-0004 section 6 closes this by registering the catalog and every preset with the Asset Manager as primary asset types `StealthTuningCatalog` and `StealthTuningPreset` with cook rule `AlwaysCook`, so the cooker keeps them regardless of references. Story 039 gives both classes a `GetPrimaryAssetId()` override returning those type names (without it a `UPrimaryDataAsset` reports its class name and the rules would not match). This story is the configuration half: the `Config/DefaultGame.ini` entries and an editor test that reads the Asset Manager settings back. The cooked-build proof is AC-27 (story 064).

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The catalog and every preset are registered as Asset Manager primary types with cook rule `AlwaysCook`; the store async-loads them at session open and keeps strong references.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: `EPrimaryAssetCookRule::AlwaysCook` ("always be cooked, in both production and development") exists [V `Engine/Classes/Engine/AssetManagerTypes.h:28-53`]. ADR-0004 Verification Required (2), that presets referenced only by the catalog are actually cooked with `AlwaysCook`, is [U] until story 064 observes it in a cooked build; this story does not close it. The ADR also tags "soft references held by an always-cooked asset are followed by the cooker" as [U] and does not rely on it, because the presets are registered as `AlwaysCook` themselves.

**Control Manifest Rules (this layer)**:
- Forbidden: never mark tuning types `Config`/`DefaultConfig`/`GlobalUserConfig`/`PerObjectConfig`, and never read a gameplay value from ini: the ini entries added here are Asset Manager rules only, no tuning value — source: ADR-0004
- Required: before editing `Config/` until a repository exists, copy it to `_snapshots/` (the batch includes `Config/` and `StealthGame.uproject`) — source: ADR-0009
- Required: test names `StealthGame.<System>.<Scenario>`; editor-metadata tests live in the editor module's `Private/Tests/<System>/` — source: ADR-0008
- Guardrail: tuning memory under 100 KB — source: ADR-0004

---

## Acceptance Criteria

*From the story plan (no single GDD criterion; GDD AC-27 is the cooked proof in story 064), scoped to this story:*

- [ ] `Config/DefaultGame.ini` registers primary asset types `StealthTuningCatalog` and `StealthTuningPreset` with cook rule `AlwaysCook` (and their base class/directories) so a preset referenced only by the catalog is cooked; verified by an editor test that reads the Asset Manager settings.

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Runtime objects and lifetime, Loading):*

- **Config.** Add to `Config/DefaultGame.ini` the `[/Script/Engine.AssetManagerSettings]` primary-asset-type entries for the two types. Each entry carries the type name, the asset base class (`UStealthTuningCatalog` for the first, `UStealthTuningData` for the second), the directory (`/Game/StealthGame/Tuning` is where story 056 puts the assets) and `Rules` with `CookRule=AlwaysCook`. The exact ini struct syntax of `FPrimaryAssetTypeInfo` in 5.8.4 is not in the inputs and the ADR cites only the cook-rule enum: read `AssetManagerTypes.h` in the 5.8.4 install (and the existing `AssetManagerSettings` section, if any) before writing the line, and treat the syntax as to be confirmed. If the editor's Project Settings UI writes the section, prefer that to hand-typing, with the single-writer rule below.
- **Only Asset Manager rules.** No tuning value goes into any ini (manifest forbids it). Leave the prototype section `[/Script/StealthGame.StealthSettings]` in `DefaultGame.ini` untouched; story 095 removes the eight canonical entries from it.
- **Type names must match story 039.** The primary type strings must equal what the `GetPrimaryAssetId()` overrides return (`StealthTuningPreset`, `StealthTuningCatalog`); a mismatch makes the rules silently not apply, which is exactly what the editor test must catch.
- **Editor test.** `Source/StealthGameEditor/Private/Tests/Tuning/AssetManagerRulesTest.cpp`, inside `#if WITH_DEV_AUTOMATION_TESTS`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`. The QA spec names the test only as `AssetManagerRulesTest (editor)`; a name following ADR-0008 is proposed: class `FTuningAssetManagerRulesTest`, name `StealthGame.Tuning.AssetManagerRules`. The test reads `UAssetManagerSettings` (the live settings object) and asserts: both primary types are present; each has cook rule `AlwaysCook`; the base class and directory are set and the directory covers `Content/StealthGame/Tuning`; and the ini holds no tuning value. Cross-check the `GetPrimaryAssetId()` of the CDO of each class (story 039) against the registered type name.
- **Single-writer / snapshot.** `Config/DefaultGame.ini` is text and mergeable, but ADR-0009 section 6 still requires a `_snapshots/<YYYY-MM-DD>_<label>/` copy of `Config/` before an edit batch while no repository exists.
- **Cook effect.** Not asserted here. Story 064 (AC-27) proves a catalog-only preset loads in a cooked Development build and records ADR-0004 Verification Required (2).
- **Prototype.** `DefaultGame.ini` is shared with the prototype section; only the Asset Manager section is changed.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-039-preset-asset-classes.md`: the classes and their `GetPrimaryAssetId()` overrides
- `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`: the preset and catalog assets in the directory registered here
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store that async-loads the types (`UAssetManager::LoadPrimaryAssetsWithType`)
- `production/epics/tuning-data-config/story-064-presets-cooked.md`: the cooked Development build check (AC-27)
- `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`: removing the prototype ini entries

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Config/DefaultGame.ini` (Asset Manager settings section only)
- `Source/StealthGameEditor/Private/Tests/Tuning/AssetManagerRulesTest.cpp`
- `production/qa/smoke-<date>.md` (smoke record)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Asset Manager primary types and cook rules (editor test `AssetManagerRulesTest`; Config/Data, so Setup/Verify/Pass condition)
  - Setup: `Config/DefaultGame.ini` `AssetManagerSettings`
  - Verify: the test reads the Asset Manager settings: primary asset types `StealthTuningCatalog` and `StealthTuningPreset` are registered with cook rule `AlwaysCook` and base class/directories set; no tuning value is in the ini
  - Pass condition: all of the above hold; the cooked effect is story 064, not asserted here

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass in `production/qa/smoke-*.md`; the editor test `Source/StealthGameEditor/Private/Tests/Tuning/AssetManagerRulesTest.cpp` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-039-preset-asset-classes.md`
- Unlocks: `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-064-presets-cooked.md`
- Open verification claims: the plan Note says AC-27 (story 064) is the cooked-build check and that the cook-rule semantics are [V `AssetManagerTypes.h:28-53`]. ADR-0004 Verification Required (2) stays [U] until story 064. Open items found while writing: the exact `FPrimaryAssetTypeInfo` ini syntax in 5.8.4, and the editor-test name (proposed `StealthGame.Tuning.AssetManagerRules`).
