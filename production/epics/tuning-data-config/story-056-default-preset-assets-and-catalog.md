# Story 056: Default, Playtest and Solo preset assets and the catalog

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-001` (text lives in `docs/architecture/tr-registry.yaml`); GDD CR-2, CR-4 (presets and catalog)

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-2 says tuning lives in cooked data assets of one class and the presets are listed by a catalog with `Default` mandatory. Stories 039 and 040 create the classes (`UStealthTuningData`, `UStealthTuningCatalog`) and the eight canonical keys; this story creates the first real assets so the store (058), the catalog check (065), the schema test (044, which pins the `Default` preset to the Default column) and the cooked-build check (064) have something to load. Three presets are enough for the epic: `Default`, `Playtest` (HostSelectable, needed by AC-2b, AC-2c and AC-27) and `Solo` (non-selectable, `TeamSize` 1). The amendment fixes their values so nothing is invented: `Playtest` is identical to `Default` apart from being HostSelectable, and `Solo` is `Default` with `TeamSize` 1. The prototype's `UStealthSettings` values are not migrated (ADR-0007 section 8); the defaults come from the C++ initialisers pinned to the GDD Canonical Tunables table.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: One preset class with per-owner `_Public`/`_Server` sub-structs, one root asset per preset (one load, one validation unit, one hash), and a `UStealthTuningCatalog` of `{ PresetId, Preset, bHostSelectable, bTestAllowsStrikeLimitOne }` entries with `Default` mandatory.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Data assets and primary data assets are pre-cutoff APIs. Asset creation is an editor operation: the store-layer facts that matter here (primary asset id override, `AlwaysCook`) are stories 039 and 057. Version-control facts from ADR-0009 apply to every `.uasset` written: ADR-0009 Verification Required (1) (lock behaviour in the editor) and (2) (LFS pointer behaviour on `.uasset` files) are [U] and not resolved by this story.

**Control Manifest Rules (this layer)**:
- Required: until a locking-capable remote exists, only one session saves `Content/` assets at a time, and every editor edit batch is preceded by a `_snapshots/` copy including `Config/` and `StealthGame.uproject` — source: ADR-0009
- Forbidden: never use Make Writable; never mass-save; agents save explicit asset paths only (and never an asset on the never-resave list) — source: ADR-0009
- Forbidden: never store tuning as `UDataTable` rows or per-domain assets (one preset is one validation unit) — source: ADR-0004
- Guardrail: tuning memory under 100 KB — source: ADR-0004

---

## Acceptance Criteria

*From the story plan and amendments (the GDD has no single criterion for the asset set), scoped to this story:*

- [ ] `Default` (every value equal to the Default column), `Playtest` (identical values to Default, HostSelectable) and `Solo` (Default values except TeamSize 1, non-selectable) preset assets and the catalog asset exist under `Content/StealthGame/Tuning/`; the catalog check passes; a smoke check records each preset loading (no values invented beyond these rules).

---

## Implementation Notes

*Derived from ADR-0004 section 1 (Asset model), section 6 (Loading) and ADR-0009 sections 4 and 6:*

- **Assets.** `Content/StealthGame/Tuning/DA_TuningDefault.uasset`, `DA_TuningPlaytest.uasset`, `DA_TuningSolo.uasset` (class `UStealthTuningData`) and `DA_TuningCatalog.uasset` (class `UStealthTuningCatalog`). Created in the editor by explicit path (the MCP asset tools may be used for the creation, but no gate may depend on them, ADR-0008).
- **Values.** `Default`: leave every field at its C++ initialiser (the story 040 declarations equal the GDD Canonical Tunables Default column: `TeamSize` 5, `NpcCount` 50, `AssassinationRangeCm` 200, `DefenceDurationSec` 4.0, `DefenceCooldownSec` 60, `NpcKillStrikeLimit` 3, `ObjectiveInteractSec` 3.0, `TargetRoundMinutes` 12). `Playtest`: the same values. `Solo`: the same values with `TeamSize` set to 1. Since `Playtest` equals `Default` in value, tests can tell them apart only by `PresetId`; this is intended by the amendment, and later stories that need a differing preset use a test-created fixture, not this asset.
- **Catalog entries.** `Default`, `Playtest` (`bHostSelectable` true) and `Solo` (`bHostSelectable` false). `bTestAllowsStrikeLimitOne` is false on all three (the catalog check rejects it on `Default` and on any HostSelectable entry; `Solo` has no need for it). The `bHostSelectable` value for the `Default` entry is not stated in the inputs: open. The ADR gives a preset a public scope when it is `Default` or HostSelectable, so `Default` is already strict; a proposal is false.
- **Catalog check.** The check is the validation that story 039's `PresetClassesHygiene` test asserts for `bTestAllowsStrikeLimitOne`; here it is run once over the real catalog. Story 065 runs the full per-preset cleanliness check (zero clamps, zero rejections, zero I-4 warnings for HostSelectable presets and `Default`). Note that `Solo` with `TeamSize` 1 and `NpcCount` 50 raises an I-3 warning in a non-selectable preset and that is expected, not an I-4 finding (QA spec for story 065).
- **Single-writer procedure (ADR-0009 sections 4 and 6).** Before the edit batch copy `Content/StealthGame/`, `Source/`, `Config/` and `StealthGame.uproject` to `_snapshots/<YYYY-MM-DD>_<label>/`. One session writes `Content/`; if a repository with locking exists by then, lock the four explicit paths first. Create and save each asset by path; never mass-save, never Make Writable, and do not touch assets on `production/never-resave-assets.md` when it exists.
- **Smoke check.** Write `production/qa/smoke-<date>.md` recording, for each of the four assets, that it loads (editor load or automation), that `Default` equals the Default column, that `Playtest` equals `Default` in value, that `Solo` differs only in `TeamSize` 1, and the catalog check result. Story 044 later pins `Default` to the Default column by automated test; this smoke check is the manual record until then.
- **Not here.** Registering the primary asset types and the `AlwaysCook` rule is story 057 (needed for the cooked-build check, story 064); the asset classes and their `GetPrimaryAssetId()` overrides are story 039.
- **Prototype.** `UStealthSettings` values are not migrated; the 12 prototype tests and the prototype ini section are untouched.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-039-preset-asset-classes.md`: the preset and catalog classes and the catalog check logic
- `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`: the eight canonical keys and their C++ initialisers
- `production/epics/tuning-data-config/story-057-asset-manager-primary-types.md`: the Asset Manager primary types and the `AlwaysCook` rule
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: loading these assets through the game-instance store
- `production/epics/tuning-data-config/story-065-catalog-clean.md`: per-preset cleanliness in CI, with a seeded dirty fixture
- `production/epics/tuning-data-config/story-044-schema-complete.md`: pinning the `Default` preset and the initialisers to the GDD Default column by test
- `production/epics/tuning-data-config/story-064-presets-cooked.md`: the cooked-build load of `Playtest`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Content/StealthGame/Tuning/DA_TuningDefault.uasset`
- `Content/StealthGame/Tuning/DA_TuningPlaytest.uasset`
- `Content/StealthGame/Tuning/DA_TuningSolo.uasset`
- `Content/StealthGame/Tuning/DA_TuningCatalog.uasset`
- `production/qa/smoke-<date>.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: preset assets and catalog (smoke check)
  - Setup: `Default`, `Playtest` (identical to `Default`, HostSelectable) and `Solo` (`Default` except `TeamSize` 1, non-selectable) presets and the catalog under `Content/StealthGame/Tuning/`
  - Verify: `Default` equals the Default column; `Playtest` equals the `Default` values; `Solo` differs only in `TeamSize` 1; the catalog check passes; each preset loads
  - Pass condition: all four assets load, the catalog check is clean, the single-writer rule was followed

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass in `production/qa/smoke-*.md` (the four assets load, the catalog check is clean)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`
- Unlocks: `production/epics/tuning-data-config/story-044-schema-complete.md`, `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-064-presets-cooked.md`, `production/epics/tuning-data-config/story-065-catalog-clean.md`
- Open verification claims: the plan Note says the ADR-0009 lock and single-writer rule applies and that prototype values are only a source for defaults pinned by test (never migrated). ADR-0009 Verification Required (1) to (3) remain [U]. Open items found while writing: the `bHostSelectable` value of the `Default` catalog entry, and whether a repository with lock support exists when this story runs.
