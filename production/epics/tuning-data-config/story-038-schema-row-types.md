# Story 038: Schema row struct, type tags, latch enum and schema constants

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002`, `TR-tdc-008` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD CR-3, CR-11 (latch classes) and the Canonical Tunables note on `BlindGuessWarnPct`

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 2, schema; section 8, latch classes)
**ADR Decision Summary**: Each key is declared once on its UPROPERTY and compiled into a generated, checked-in table of `constexpr` rows read by a plain-C++ validator; values are accessed through generated member-pointer accessors, never `offsetof`. Values that invariants read but that are not preset fields live as `constexpr` schema constants, never hashed and never replicated.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API; plain C++ types plus a UENUM-free `enum class`. [V] UPROPERTY metadata exists only with editor-only data (`WITH_METADATA = WITH_EDITORONLY_DATA`, `CoreMiscDefines.h:31`), which is why the runtime reads this table and never metadata. The ADR Verification Required items (1)-(3) are not touched by this story. No unverified [U] claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct; a hardcoded gameplay literal is a defect (source: ADR-0004)
- Forbidden: Never read UPROPERTY metadata at runtime for ranges; `WITH_METADATA` is editor-only, so cooked builds lose the clamp (source: ADR-0004)
- Required: Every `Public/Server/` header includes `StealthServerOnlyGuard.h` first (source: ADR-0007)
- Guardrail: Tuning costs 0 per frame, under 100 KB memory (source: ADR-0004)

This story lays the type vocabulary that every later tuning story uses. It creates the row struct and the schema constants header only. The table content (rows) is produced by the commandlet in story 041, and the empty-table-under-bootstrap behaviour is verified by story 042.

---

## Acceptance Criteria

*From the plan amendment for story 038 (REVISED) with the constants list fixed by Amendments 2, scoped to this story:*

- [x] `FTuningSchemaRow` (Key, TypeTag bool=0/int=1/float=2/enum=3, Get/Set function pointers, Default, HardMin, HardMax, SafeMin, SafeMax, bPublic, Latch, Owner, sentinel), `ETuningLatch {Round, Action, Use, Session}`, `GetTuningSchema()` and `Public/Server/StealthTuningConstants.h` exist, and the constants header defines exactly these schema constants, each with its GDD reference and none a preset field: `BlindGuessWarnPct` = 50 (I-4), `GraceMarginSec` = 30 (Round F4), `BushDomeInnerRadiusCm` = 90 (I-CS-4), `SclCapsuleRadiusCm` = 35 (I-ASN-1), `PingSkewTolSec` = 0.1 (Pings F1), `DueEpsilonMs` = 1 (Round R12); test `StealthGame.Tuning.SchemaRowTypes`. (The empty-table-under-BOOTSTRAP clause is verified by story 042, not here.)

---

## Implementation Notes

*Derived from ADR-0004 sections 2 and 8, Key Interfaces, and Amendments 2:*

- Files and placement (Foundation module, ADR-0007): `Public/Server/StealthTuningSchema.h` (row struct, `ETuningLatch`, `GetTuningSchema()` declaration), `Public/Server/StealthTuningConstants.h`, `Private/Tuning/StealthTuningSchema.cpp`. Both headers are under `Public/Server/` and include `StealthServerOnlyGuard.h` first. Log category `LogStealthFoundation`, `UE_LOGF` only (never `UE_LOG`), though this story logs nothing.
- Row shape from the ADR Key Interfaces: `const ANSICHAR* Key; uint8 TypeTag; double (*Get)(const FStealthServerTuning&); void (*Set)(FStealthServerTuning&, double); double Default, HardMin, HardMax, SafeMin, SafeMax; bool bPublic; ETuningLatch Latch; const ANSICHAR* Owner;` plus the sentinel field the F-1 rule needs (a value outside the continuous range that is valid by itself, per key, for example `EndedAutoResetSec` 0). `FStealthServerTuning` is declared in story 040, so forward-declare it here. The ADR section 2 row list also names `EnumValues` (used by the F-1 undeclared-enum rule and by the story 043 fingerprint); the AC does not list it and the eight canonical keys have no enum, so add it when the first enum key exists (open, settle at implementation).
- TypeTag values are fixed by the AC: bool=0, int=1, float=2, enum=3. The ADR section 1 mentions a `SoftRef` schema type for `*Asset` soft-reference keys (Crowd); no tag value for it is assigned by the inputs (open, owned by the Crowd epic's schema-row story).
- `ETuningLatch` is `enum class ETuningLatch : uint8 { Round, Action, Use, Session }`, in that order (TR-tdc-008; the consumers' latch behaviour is not tested here).
- `GetTuningSchema()` returns `TConstArrayView<FTuningSchemaRow>`. Until story 041 produces `StealthTuningSchema.generated.inl` the table is empty; whether this story ships a placeholder `.inl` or guards the include is not specified (open). Do not implement the `STEALTH_TUNING_SCHEMA_BOOTSTRAP` define handling here (story 042).
- Constants (Amendments 2, exactly these six, name = value, GDD reference): `BlindGuessWarnPct` = 50 (I-4), `GraceMarginSec` = 30 (Round F4), `BushDomeInnerRadiusCm` = 90 (I-CS-4), `SclCapsuleRadiusCm` = 35 (I-ASN-1), `PingSkewTolSec` = 0.1 (Pings F1), `DueEpsilonMs` = 1 (Round R12). Each is a `constexpr` with a comment citing its GDD reference. The IVP `LeakNetProfile` constants and the `T_uncull` inputs (I-CON-a..e) are NOT added here: Amendments 2 assigns them to the Information Visibility Policy and Concealment epics. Constants are never hashed (F-2) and never replicated, and none is a field of any preset struct.
- How the test proves "exactly these constants" is not fixed by the inputs (open): a single `constexpr` list or table in the header that the test iterates and compares with the six expected name/value pairs, with a failing check when an extra entry exists (spec edge case), is the straightforward option.
- Test: `Source/StealthFoundation/Private/Tests/Tuning/SchemaRowTypesTest.cpp`, class `FTuningSchemaRowTypesTest` via `IMPLEMENT_SIMPLE_AUTOMATION_TEST`, name `StealthGame.Tuning.SchemaRowTypes`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file inside `#if WITH_DEV_AUTOMATION_TESTS`; run headless (`UnrealEditor-Cmd -nullrhi`, `Automation RunTests ^StealthGame.Tuning.SchemaRowTypes`). The schema-constants-are-not-preset-fields check is a reflection check against `UStealthTuningData` (story 039) and the sub-structs once they exist; before they exist it asserts over the types available and the check tightens as stories 039/040 land (note this in the test comments).
- Prototype: none (no `Prototype:` line in the plan). The prototype `UStealthSettings` is not touched.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-039-preset-asset-classes.md`: `UStealthTuningData`, `UStealthTuningCatalog` and the no-Config hygiene test.
- `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`: the eight canonical keys, the sub-structs and the two tuning faces.
- `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md`: generating the table rows.
- `production/epics/tuning-data-config/story-042-schema-bootstrap-regeneration.md`: the `STEALTH_TUNING_SCHEMA_BOOTSTRAP=1` empty-table build and its verification.
- `production/epics/tuning-data-config/story-044-schema-complete.md`: schema completeness checks (defaults inside ranges, suffixes, mirrors).
- The IVP and Concealment constants (owned by those epics, Amendments 2).

---

## Files

- `Source/StealthFoundation/Public/Server/StealthTuningSchema.h`
- `Source/StealthFoundation/Public/Server/StealthTuningConstants.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningSchema.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/SchemaRowTypesTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: The row struct, latch enum, accessor and constants header exist with the stated values.
  - Given: `FTuningSchemaRow`, `ETuningLatch`, `GetTuningSchema()`, `StealthTuningConstants.h`.
  - When: the test inspects types and constants.
  - Then: TypeTag bool=0, int=1, float=2, enum=3; `ETuningLatch` is {Round, Action, Use, Session}; the row has Key, Get/Set pointers, Default, HardMin, HardMax, SafeMin, SafeMax, bPublic, Latch, Owner, sentinel; the constants header holds exactly the enumerated constants, each with a GDD reference and none a preset field; `BlindGuessWarnPct` 50, `GraceMarginSec` 30, bush dome inner radius 90 cm, `PingSkewTolSec` 0.1.
  - Edge cases: an extra undeclared constant fails; the BOOTSTRAP empty-table clause is story 042.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/SchemaRowTypesTest.cpp` (`StealthGame.Tuning.SchemaRowTypes`) must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Tuning.SchemaRowTypes`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`
- Unlocks: `production/epics/tuning-data-config/story-039-preset-asset-classes.md`
- Open verification claims from the plan Note lines: none (the plan has no Note line for this story). Open design points: the `EnumValues` field timing, the `SoftRef` tag value, and the "exactly these constants" mechanism.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: `FTuningSchemaRow` (Key, TypeTag, Get/Set, Default, HardMin/HardMax, SafeMin/SafeMax, bPublic, Latch, Owner, Unit, bHasSentinel/Sentinel), type tags bool 0 / int 1 / float 2 / enum 3 (plus SoftRef 4 per the ADR-0004 amendment 2026-10-25b), `ETuningLatch {Round, Action, Use, Session}`, `GetTuningSchema()`, and `StealthTuningConstants.h` with exactly the six constants, each citing its GDD reference and none a preset field, checked by reflection over `UStealthTuningData` and both faces)
**Decisions**: "exactly these constants" = the `StealthTuningConstants::All` list compared with the six expected pairs AND a scan of the header text for every `inline constexpr` declaration (a seeded extra constant is counted, control in the test); `EnumValues` is deferred to the first enum key (the generator rejects enum keys until then); the SoftRef tag value 4 is the ADR amendment's, its `AllowedClass`/`bAllowNull` columns stay with the Crowd schema-row story; a `Unit` column was added to the row (used by the AC-3b doc lint later); the empty-table clause is story 042 (`STEALTH_TUNING_SCHEMA_BOOTSTRAP`)
**Deviations**: Advisory: the row carries `Unit` and `bHasSentinel` beyond the ADR Key Interfaces row (additive)
**Test Evidence**: `Source/StealthFoundation/Private/Tests/Tuning/SchemaRowTypesTest.cpp`; `production/qa/evidence/story-038-schema-row-types/stamp.txt`; full suite 61/61 green (cycle.ps1 -Tests -NoLaunch: Result: Succeeded, tests exit=0)
**Run result**: N/A — type vocabulary and constants only; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects left
