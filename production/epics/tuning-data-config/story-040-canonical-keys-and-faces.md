# Story 040: The eight canonical keys and the client/server tuning faces

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002`, `TR-tdc-006` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD `### Canonical Tunables`, CR-2, CR-3, CR-8 (audience), CR-15

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 1, asset model and the two faces; section 2, schema declared once on the UPROPERTY)
**ADR Decision Summary**: Each key is declared once on its UPROPERTY (default from the C++ initialiser, hard range `ClampMin`/`ClampMax`, safe range `SafeMin`/`SafeMax`, latch/owner/unit as custom meta) inside a `_Public` or `_Server` sub-struct; audience is structural. `FStealthClientTuning` (all `_Public` structs, in `Public/View/`) is a distinct type from `FStealthServerTuning` (public plus server, in `Public/Server/`), so a server-only key does not compile on the client face.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API. [V] UPROPERTY metadata (`ClampMin`, `SafeMin`, custom meta) exists only with editor-only data (`CoreMiscDefines.h:31`, `Field.h:946`), so the test that reads metadata only works in an editor build and the runtime never reads it (the generated table in story 041 carries the values to cooked builds). No unverified [U] claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct; a hardcoded gameplay literal is a defect (source: ADR-0004)
- Required: A service with both faces is split into a `View/` facade and a `Server/` implementation (source: ADR-0007)
- Forbidden: Never let a `Public/View/` header include a `Server/` header; never let client or presentation code include a `Public/Server/` tuning header (source: ADR-0007, ADR-0004)
- Forbidden: Never mark tuning types `Config`/`DefaultConfig`/`GlobalUserConfig`/`PerObjectConfig` (source: ADR-0004)

---

## Acceptance Criteria

*From the plan (Exit B-2), scoped to this story:*

- [ ] The eight canonical keys (`TeamSize`, `NpcCount` default 50, `AssassinationRangeCm`, `DefenceDurationSec`, `DefenceCooldownSec`, `NpcKillStrikeLimit`, `ObjectiveInteractSec`, `TargetRoundMinutes`) are declared once as UPROPERTYs with the type, default, hard range, safe range, audience, latch, owner and unit of the GDD Canonical Tunables table (hard range via ClampMin/ClampMax, safe via SafeMin/SafeMax, latch/owner/unit custom meta), inside `_Public`/`_Server` sub-structs, and `FStealthClientTuning` (all `_Public`, in `Public/View/`) and `FStealthServerTuning` (public + server, in `Public/Server/`) are distinct types. Test `StealthGame.Tuning.CanonicalKeys`.

---

## Implementation Notes

*Derived from ADR-0004 sections 1 and 2:*

- Source of truth is the GDD `### Canonical Tunables` table; re-read it at implementation time, it wins over this snapshot. Snapshot at story creation (type, default, hard range, safe range, audience, latch, owner): `TeamSize` int 5, 1-5, 2-5, All, Round, Round & Match Framework; `NpcCount` int 50, 6-100, 45-50, ServerOnly, Round, Crowd & NPC Behaviour; `AssassinationRangeCm` float 200, 100-250, 150-250, All, Use, Assassination & Defence Reversal; `DefenceDurationSec` float 4.0, 3.0-5.0, 3.0-5.0, All, Action, Assassination; `DefenceCooldownSec` float 60, 30-120, 45-75, All, Action, Assassination; `NpcKillStrikeLimit` int 3, 2-5, 2-4, All, Round, NPC-Kill Penalty & Informants; `ObjectiveInteractSec` float 3.0, 2.0-6.0, 2.9-4.5, All, Action, Objectives Chain; `TargetRoundMinutes` float 12, 5-30, 8-15, ServerOnly, Round, Round & Match Framework. `NpcCount` default is 50 (user decision 2026-10-25). `BlindGuessWarnPct` is NOT a key here (a schema constant, story 038).
- Audience maps to the struct: All and Client keys go in the owner's `_Public` struct, ServerOnly keys in its `_Server` struct. Struct naming follows the ADR pattern `F<Owner>Tuning_Public` / `F<Owner>Tuning_Server` (the ADR names `FRoundTuning`, `FCrowdTuning`, `FAssassinationTuning`); the exact names for the Penalty and Objectives owners are not given by the inputs (open, follow the pattern). The field name is the canonical key; the owner prefix is not part of it.
- Declaration shape (ADR section 2): `UPROPERTY(EditAnywhere, Category = "...", meta = (ClampMin = "...", ClampMax = "...", SafeMin = "...", SafeMax = "...", Latch = "Round", Owner = "...", Unit = "..."))` with the C++ initialiser as the schema default. `NpcKillStrikeLimit` hard range 2-5; the 1 permitted only for a `bTestAllowsStrikeLimitOne` preset is an F-1 rule (story 046), not a metadata value.
- The Canonical Tunables table has no Unit column, but the AC requires a unit. Derive the unit from the CR-15.2 name suffix (`Sec`, `Cm`, `Count`, `Minutes`) and write it in the `Unit` meta; this inconsistency between the AC wording and the table is noted for the story reviewer.
- `StealthTuningData.h` (story 039) gains the owner members for these sub-structs in this story; that edit is part of this story's Files (the plan's file list does not name it).
- The two faces: `FStealthClientTuning` in `Public/View/StealthClientTuning.h` holds only `_Public` sub-structs and never includes a `Server/` header; `FStealthServerTuning` in `Public/Server/StealthServerTuning.h` holds public plus server sub-structs (guard header first). Sub-structs live in `Public/Server/StealthTuningSubstructs.h`; this raises a placement question the inputs do not answer: the client face (View) must name the `_Public` structs without including a `Server/` header (open; either the `_Public` structs get a View header or the ADR-0007 lint exception is raised with the lead). Raise it at implementation start, do not work around the include lint.
- Guard: `static_assert(!std::is_same_v<FStealthClientTuning, FStealthServerTuning>)`, plus the test asserts by reflection that no server-only key is a member of the client type (spec edge case).
- Test placement: the plan puts `CanonicalKeysTest.cpp` under `Source/StealthFoundation/Private/Tests/Tuning/`, but it reads metadata (`ClampMin`, `SafeMin`, `Latch`), and ADR-0004 section 2 places metadata-reading tests in `Source/StealthGameEditor/Private/Tests/`. Follow the plan path and wrap the metadata reads in `#if WITH_METADATA`; if that proves awkward, move the file to the editor module and record the change in the Files list at close. Class `FTuningCanonicalKeysTest`, name `StealthGame.Tuning.CanonicalKeys`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`. The expected values in the test are committed constants copied from the GDD table (the Default pin; full pin-to-schema is story 044).
- Prototype: the prototype `UStealthSettings` values are only a source for the defaults, which are pinned by this test; nothing is migrated (EPIC Prototype Disposition). The prototype fields are removed by story 095.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-039-preset-asset-classes.md`: the preset and catalog classes this story adds members to.
- `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md`: generating schema rows from these declarations.
- `production/epics/tuning-data-config/story-044-schema-complete.md`: the full AC-3a pin of Default preset and C++ initialisers to the GDD Default column, suffix and uniqueness checks.
- `production/epics/tuning-data-config/story-047-derived-indicators.md`: F-3 indicators over these keys.
- `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`: the `DA_Tuning*` preset assets.
- `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`: the replicated snapshot built from the client face.
- Other owners' keys (registered by their epics).

---

## Files

- `Source/StealthFoundation/Public/View/StealthClientTuning.h`
- `Source/StealthFoundation/Public/Server/StealthServerTuning.h`
- `Source/StealthFoundation/Public/Server/StealthTuningSubstructs.h`
- `Source/StealthFoundation/Public/Server/StealthTuningData.h` (edit: owner members; not in the plan list)
- `Source/StealthFoundation/Private/Tests/Tuning/CanonicalKeysTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: Each canonical key is declared once with the table's attributes, in the right sub-struct, and the two faces are distinct types.
  - Given: the GDD Canonical Tunables table (`TeamSize`, `NpcCount` default 50, `AssassinationRangeCm`, `DefenceDurationSec`, `DefenceCooldownSec`, `NpcKillStrikeLimit`, `ObjectiveInteractSec`, `TargetRoundMinutes`).
  - When: reflection reads each UPROPERTY and its metadata.
  - Then: each key is declared once with type, default, hard range (ClampMin/ClampMax), safe range (SafeMin/SafeMax), audience, latch, owner and unit equal to the table row; keys sit in `_Public`/`_Server` sub-structs; `FStealthClientTuning` (Public/View, only `_Public`) and `FStealthServerTuning` (public + server) are distinct types.
  - Edge cases: a server-only key (per Audience column) is absent from the client type; a duplicate declaration fails; `static_assert` on `!is_same`.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/CanonicalKeysTest.cpp` (`StealthGame.Tuning.CanonicalKeys`) must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-039-preset-asset-classes.md`
- Unlocks: `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md`, `production/epics/tuning-data-config/story-047-derived-indicators.md`, `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`, `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`
- Open verification claims from the plan Note lines: none (the plan has no Note line for this story). Open design points: View-side placement of the `_Public` structs, Penalty/Objectives struct names, and the Unit source (no table column).
