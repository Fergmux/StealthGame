# Story 098: Derived indicators, preset scope (AC-18 scope clauses)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004`; GDD AC-18 (F-3, I-4), preset-scope clauses
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Invariants I-3, I-4 and I-NKP-1 reject only when the preset is `bIsPublic` (`Default` or `HostSelectable`) and warn otherwise, via `FTuningCheckContext`. The F-3 indicators are computed in double precision from the clamped set, and `BlindGuessWarnPct` is a schema constant, never a preset field.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story; plain-C++ test over the pipeline core, logging via `UE_LOGF` (5.8). No [U] claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair — source: ADR-0004
- Required: Every reject/warn invariant carries its GDD ID — source: ADR-0004
- Required: Every Logic story ships T1 tests — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-18, scope clauses), as split out by the amendments (story 098 NEW):*

- [ ] **AC-18 (scope clauses)**: for NpcCount 15 and NpcKillStrikeLimit 3 (BlindGuessSuccess 0.578) a non-selectable preset is accepted with a warning while a HostSelectable preset and Default are rejected; and for TeamSize 5 and NpcKillStrikeLimit 3 in a HostSelectable preset against the AC-17 fixture schema NpcCount 20 passes and 19 is rejected. (`StealthGame.Tuning.DerivedIndicatorsPresetScope`)

---

## Implementation Notes

*Derived from ADR-0004 section 3 and Key Interfaces, and the GDD AC-18 text:*

Story 047 (revised) covers the indicator maths and the reflection clause of AC-18; this story covers the preset-scope clauses that need `bIsPublic` context and `Resolve()`. The amendments give 098 only a slug, so the title above is derived from the AC. Per G7, 098 slots after 097 in the build order, but it depends on stories 047, 048 and 050 only.

- Test: `StealthGame.Tuning.DerivedIndicatorsPresetScope`, class `FTuningDerivedIndicatorsPresetScopeTest`, file `Source/StealthFoundation/Private/Tests/Tuning/DerivedIndicatorsPresetScopeTest.cpp`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- Scope clause 1: with `NpcCount` 15 and `NpcKillStrikeLimit` 3, `BlindGuessSuccess` is about 0.578 (story 047 asserts the number). A non-selectable preset (`bIsPublic` false) is accepted and the report holds a warning that names the I-4 indicator rule; a `HostSelectable` preset and `Default` (both `bIsPublic` true) are rejected, so `Resolve()` falls back (the rejected requested preset gives `FallbackLevel` 1; a rejected `Default` request falls to the schema-defaults rung, `FallbackLevel` 2 with `PresetId` `SchemaDefaults`, per ADR-0004 s3). The AC states only "rejected"; asserting the specific `FallbackLevel` for the rejected `Default` case follows ADR-0004 s3 and should be confirmed against story 051 when writing the test.
- Scope clause 2: with `TeamSize` 5 and `NpcKillStrikeLimit` 3 in a `HostSelectable` preset, validated against the AC-17 fixture schema (story 048's I-1 to I-5-only fixture), `NpcCount` 20 passes and `NpcCount` 19 is rejected. The GDD notes that with the full schema I-LSC-1 rejects below 35, which is why the fixture schema is required here; do not run this clause against the live schema.
- Seams: inject the fixture schema and the preset source through `FTuningResolveInput` (story 050: `TConstArrayView<FTuningSchemaRow> Schema`, `ITuningPresetSource` whose `Load` returns the `bIsPublic` flag). Use the log-capture seam (story 009) to assert the warning in the accepted case names the rule (spec edge case) and the fault sink (story 008) to declare expected rejections.
- The `BlindGuessWarnPct` threshold (50) is a schema constant in `StealthTuningConstants.h` (story 038), not a preset field; the test must not set it through a preset.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-047-derived-indicators.md`: the F-3 indicator values at defaults and at NpcCount 15, and the `BlindGuessWarnPct` reflection check.
- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: the I-1 to I-5 invariant table and the AC-17 fixture schema.
- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: `Resolve()` and the preset-source and schema seams.
- `production/epics/tuning-data-config/story-097-invariants-reject-whole-preset.md`: whole-preset rejection for the AC-17 fixtures.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthFoundation/Private/Tests/Tuning/DerivedIndicatorsPresetScopeTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-18 preset-scope clauses
  - Given: `NpcCount` 15, `NpcKillStrikeLimit` 3 (`BlindGuessSuccess` 0.578); `TeamSize` 5, `NpcKillStrikeLimit` 3 in a `HostSelectable` preset against the AC-17 fixture schema.
  - When: validated.
  - Then: the non-selectable preset is accepted with a warning; the `HostSelectable` preset and `Default` are rejected; `NpcCount` 20 passes and 19 is rejected.
  - Edge cases: the warning names the indicator rule.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation test `StealthGame.Tuning.DerivedIndicatorsPresetScope` in `Source/StealthFoundation/Private/Tests/Tuning/DerivedIndicatorsPresetScopeTest.cpp` must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-047-derived-indicators.md`, `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`, `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`
- Unlocks: None
- Open verification claims: the expected `FallbackLevel` when `Default` itself is rejected follows ADR-0004 s3 and is not stated in the AC (confirm against story 051).
