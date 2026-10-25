# Story 050: AC-6a FallbackToDefault: Resolve() and the preset-source seam

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-6a (CR-6)

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 3, fallback ladder and `FTuningResolveInput`)
**ADR Decision Summary**: `FTuningPipelineCore::Resolve` takes the requested preset name, an injectable `ITuningPresetSource` (production: the store's preloaded catalog; tests: a source that can return a missing or corrupted preset), the schema and the session-frozen keys, and walks the ladder requested preset, then `Default`, then schema defaults, then `Failed`. A rejected preset is rejected as a whole, never partly repaired.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API beyond `UE_LOGF` (5.8). `TOptional`, `TSharedPtr` and `TConstArrayView` are long-standing. No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; invariants reject or warn, never repair (source: ADR-0004)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F...Core`) with no `UWorld` dependency, behind a subsystem facade (source: ADR-0007)
- Forbidden: Never repair a failed invariant by clamping a dependent key (source: ADR-0004)
- Guardrail: Validation under 1 ms at `Preparing`; under 100 KB memory (source: ADR-0004)

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-6a, scoped to this story:*

- [ ] **AC-6a (CR-6)**: GIVEN a requested preset violating I-1 (`DefenceDurationSec` 5, `DefenceCooldownSec` 35), WHEN resolved, THEN the whole preset is rejected, `Default` is active and `FallbackLevel` is 1. (`StealthGame.Tuning.FallbackToDefault`)

---

## Implementation Notes

*Derived from ADR-0004 section 3, Key Interfaces and the plan amendment for story 050 (REVISED):*

- Files: `Public/Server/StealthTuningPipelineCore.h` (guard header first; extends the header story 046 created with `FTuningResolveInput`, `FTuningResolveResult`, `FValidationReport`, `ITuningPresetSource`, `ETuningFallback`), `Private/Tuning/StealthTuningPipelineCore.cpp`, test `Private/Tests/Tuning/FallbackToDefaultTest.cpp`, all in `Source/StealthFoundation/`. Plain C++, no `UWorld`. Warnings via `UE_LOGF` on `LogStealthFoundation`.
- Interfaces from the ADR Key Interfaces: `enum class ETuningFallback : uint8 { Requested = 0, Default = 1, SchemaDefaults = 2 };` `class ITuningPresetSource { virtual TOptional<FStealthServerTuning> Load(FName PresetId, bool& bOutIsPublic) const = 0; };` `struct FTuningResolveInput { FName RequestedPreset; const ITuningPresetSource* Source; const FStealthServerTuning* SessionFrozen; };` `class FTuningPipelineCore { FTuningResolveResult Resolve(const FTuningResolveInput&) const; };` `FTuningResolveResult { FallbackLevel, PresetId, FValidationReport, TSharedPtr<const FValidatedTuning> }`. Amendment: `FTuningResolveInput` also carries an injectable schema view (`TConstArrayView<FTuningSchemaRow> Schema`, default the real schema) so a test can substitute a schema (story 053 depends on this seam).
- Per rung: load the preset through the source; run F-1 (story 046) then the invariants (story 048) on the clamped set with the context `bIsPublic` (the `Default` or `HostSelectable` flag the source returns) and `PresetId`; any Reject rejects the whole preset, with no value repaired and no partial application (spec edge case). The first passing rung wins and `FallbackLevel` is 0 for the requested preset, 1 for `Default`. A missing preset (the source returns no value) takes the same rung as a rejected one.
- The seam defines "corrupted" for story 051: document in the `ITuningPresetSource` header comment what a corrupted preset is. The inputs give: a load or validity failure distinct from out-of-range values, which clamp (spec for story 051). Document it once here, in the header, and keep story 051 consistent with it.
- The F-1 flag `bTestAllowsStrikeLimitOne` also has to arrive per rung; the ADR `Load` signature shows only `bOutIsPublic` (open, shared with story 046): settle the carrier here if not already settled there.
- `FValidatedTuning` and the freeze: the result holds a `TSharedPtr<const FValidatedTuning>` (ADR Key Interfaces) and the hash comes from story 045 (the reason for that dependency), but the plan lists `FND/Public/Server/StealthValidatedTuning.h` only under story 054. Create the type here minimally, with const accessors only as in the ADR (`Values()`, `TuningHash()`, `ClientHash()`, `SchemaVersion()`, `PresetId()`, `Revision()`, `FallbackLevel()`, `Report()`), and let story 054 prove immutability (compile probe, held as `TSharedRef<const FValidatedTuning>`, hash unchanged at `Ended`). Add `StealthValidatedTuning.h` to this story's Files; flagged as an inconsistency between the plan's file lists for 050 and 054.
- Test, per the spec: the source returns a preset with `DefenceDurationSec` 5 and `DefenceCooldownSec` 35 (I-1: 35 < 8 x 5 = 40) and a valid `Default`; `Resolve()` runs with the real schema; assert the whole preset is rejected, the report names I-1, `Default` is active, `FallbackLevel` is 1, and that none of the rejected preset's other keys leaked into the active values (no partial application). Also assert a missing requested preset (source returns none) lands on the same rung, and that a substituted schema view is honoured (used by story 053).
- Test class `FTuningFallbackToDefaultTest`, name `StealthGame.Tuning.FallbackToDefault`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`, headless `-nullrhi`. The fake source is a test-local class. Assert zero unexpected faults at teardown if the fault sink (story 008) is wired.
- Prototype: none (no `Prototype:` line in the plan).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-051-fallback-to-schema.md`: the schema-defaults rung (`FallbackLevel` 2, `SchemaDefaults`).
- `production/epics/tuning-data-config/story-053-fallback-failed.md`: the `Failed` rung, `TuningLoadFailed` and the Round stub.
- `production/epics/tuning-data-config/story-097-invariants-reject-whole-preset.md`: the whole-preset rejection for every AC-17 rejecting fixture through `Resolve()`.
- `production/epics/tuning-data-config/story-098-derived-indicators-preset-scope.md`: AC-18 scope clauses through `Resolve()`.
- `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`: immutability proof of `FValidatedTuning`.
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the game-instance store that supplies the production `ITuningPresetSource`.
- `production/epics/tuning-data-config/story-045-hasher-golden.md` and `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: the hasher and invariants this story calls.

---

## Files

- `Source/StealthFoundation/Public/Server/StealthTuningPipelineCore.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningPipelineCore.cpp`
- `Source/StealthFoundation/Public/Server/StealthValidatedTuning.h` (minimal const-only type; not in the plan list for this story)
- `Source/StealthFoundation/Private/Tests/Tuning/FallbackToDefaultTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: A requested preset violating I-1 is rejected as a whole, `Default` is active and `FallbackLevel` is 1.
  - Given: `ITuningPresetSource` returning a preset with `DefenceDurationSec` 5, `DefenceCooldownSec` 35; `FTuningResolveInput` with a schema view (default real schema).
  - When: `Resolve()` runs.
  - Then: the whole preset is rejected naming I-1; `Default` is active; `FallbackLevel` is 1.
  - Edge cases: no partial application; a missing preset takes the same rung; a substituted schema view is honoured (used by story 053).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/FallbackToDefaultTest.cpp` (`StealthGame.Tuning.FallbackToDefault`) must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`, `production/epics/tuning-data-config/story-045-hasher-golden.md`
- Unlocks: `production/epics/tuning-data-config/story-051-fallback-to-schema.md`, `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`, `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-097-invariants-reject-whole-preset.md`, `production/epics/tuning-data-config/story-098-derived-indicators-preset-scope.md`
- Open verification claims from the plan Note lines: none. The plan Note (`ITuningPresetSource` is the injectable loader seam) is a design statement, not an open claim. Open design points: the `bTestAllowsStrikeLimitOne` carrier, the "corrupted" definition (to be written in the seam header), and where `FValidatedTuning` is first declared.
