# Story 051: AC-6b FallbackToSchema

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-6b (CR-6)

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 3, fallback ladder)
**ADR Decision Summary**: The fallback ladder is requested preset, then `Default`, then schema defaults (the C++ initialisers, i.e. the generated table's defaults), then `Failed`. Requesting `Default` skips the repeated rung: a failing `Default` goes straight to schema defaults with `FallbackLevel` 2 and `PresetId` `SchemaDefaults`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API beyond `UE_LOGF` (5.8). No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; invariants reject or warn, never repair (source: ADR-0004)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores with no `UWorld` dependency (source: ADR-0007)
- Forbidden: Never repair a failed invariant by clamping a dependent key (source: ADR-0004)
- Guardrail: Validation under 1 ms at `Preparing` (source: ADR-0004)

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-6b, scoped to this story:*

- [ ] **AC-6b (CR-6)**: GIVEN the injectable loader returns a corrupted `Default` and the requested preset fails, THEN schema defaults apply with `FallbackLevel` 2. AND GIVEN `Default` itself is requested and corrupted, THEN `FallbackLevel` is 2 and `PresetId` is `SchemaDefaults`. (`StealthGame.Tuning.FallbackToSchema`)

---

## Implementation Notes

*Derived from ADR-0004 section 3 and the qa-lead spec for this story:*

- Files: `Source/StealthFoundation/Private/Tuning/StealthTuningPipelineCore.cpp` (add the schema-defaults rung to `Resolve()` from story 050) and test `Source/StealthFoundation/Private/Tests/Tuning/FallbackToSchemaTest.cpp`.
- Schema-defaults rung: build the active value set from the schema table's `Default` column (the generated table's defaults, pinned equal to the GDD defaults by `SchemaComplete`, story 044), not from any asset. The result has `FallbackLevel` 2 (`ETuningFallback::SchemaDefaults`) and `PresetId` `SchemaDefaults`. The schema-default set is itself validated; if it fails the next rung is `Failed` (story 053).
- Two cases: (1) the requested preset fails and `Default` is corrupted: schema defaults apply, `FallbackLevel` 2; (2) `Default` itself is requested and corrupted: the repeated `Default` rung is skipped, `FallbackLevel` is 2 and `PresetId` is `SchemaDefaults`.
- "Corrupted" is defined in the story 050 source seam (the spec says to define it there): a load or validity failure (the source returns no value, or a value that fails validation), distinct from out-of-range values, which clamp and do not count as corruption. Use that definition and the fake source of story 050; do not introduce a second one.
- Test class `FTuningFallbackToSchemaTest`, name `StealthGame.Tuning.FallbackToSchema`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`, headless `-nullrhi`. Assert, in both cases, that the active values equal the schema defaults (compare a few canonical keys, or the whole set through the generated accessors) and that the report records the fallback.
- Prototype: none (no `Prototype:` line in the plan).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: `Resolve()`, the source seam and the requested-to-`Default` rung.
- `production/epics/tuning-data-config/story-053-fallback-failed.md`: the `Failed` rung when every rung fails, including the substituted-schema seam use.
- `production/epics/tuning-data-config/story-044-schema-complete.md`: the pin of schema defaults to the GDD Default column.
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the production store that feeds the source.

---

## Files

- `Source/StealthFoundation/Private/Tuning/StealthTuningPipelineCore.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/FallbackToSchemaTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: With a corrupted `Default` schema defaults apply at `FallbackLevel` 2, and when `Default` itself is requested and corrupted the `PresetId` is `SchemaDefaults`.
  - Given: a preset source returning a corrupted `Default` (a load/validity failure, distinct from out-of-range values which clamp) plus a failing requested preset.
  - When: `Resolve()`; then `Resolve()` with `Default` itself requested and corrupted.
  - Then: schema defaults apply with `FallbackLevel` 2; the second case has `FallbackLevel` 2 and `PresetId` `SchemaDefaults`.
  - Edge cases: "corrupted" is defined in the source seam of story 050.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/FallbackToSchemaTest.cpp` (`StealthGame.Tuning.FallbackToSchema`) must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`
- Unlocks: `production/epics/tuning-data-config/story-053-fallback-failed.md`
- Open verification claims from the plan Note lines: none (the plan has no Note line for this story).
