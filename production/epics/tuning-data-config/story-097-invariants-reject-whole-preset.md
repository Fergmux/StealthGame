# Story 097: Invariants reject the whole preset (AC-17 closing clauses)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004`, `TR-tdc-003`; GDD AC-17 (closing bullets)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: A failing `Reject` invariant rejects the whole preset (never a clamp or repair of a dependent key), and the fallback ladder then moves requested to `Default` to schema defaults to `Failed`. `Resolve()` reports the active preset and `FallbackLevel`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story; the work is a plain-C++ test over `FTuningPipelineCore` (no `UWorld`). Logging, if asserted, is `UE_LOGF` on `LogStealthFoundation` (5.8, [V `LogMacros.h`]). No [U] claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair — source: ADR-0004
- Forbidden: Never repair a failed invariant by clamping a dependent key — source: ADR-0004
- Required: Every Logic story ships T1 tests — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-17, closing bullets), as split out by the amendments (story 097 NEW):*

- [ ] **AC-17 (closing bullets)**: for every rejecting fixture of AC-17 (I-1 cooldown 39.9; I-2 NpcCount 9 and 7; I-3 HostSelectable 21; I-5 Min greater than Max), running `Resolve()` rejects the whole preset (no partly repaired value), the active preset is Default and FallbackLevel is 1. (`StealthGame.Tuning.InvariantRejectsWholePreset`)

---

## Implementation Notes

*Derived from ADR-0004 sections 3 and Key Interfaces; amendment G7 (097 slots after 050):*

AC-17 has two halves. Story 048 proves the invariant table returns Pass/Warn/Reject for each listed value. This story proves the end-to-end clause "each rejection rejects the whole preset at `FallbackLevel` 1", which needs `Resolve()` and the fallback ladder from story 050. The plan gives this story no title; the title above is taken from the AC (new story numbers 097 to 100 were appended after the original plan, so G7 places 097 after story 050).

- Test: `StealthGame.Tuning.InvariantRejectsWholePreset`, class `FTuningInvariantRejectsWholePresetTest`, file `Source/StealthFoundation/Private/Tests/Tuning/InvariantRejectsWholePresetTest.cpp`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- Fixtures reuse story 048's I-1 to I-5-only test schema fixture (owner invariants excluded) through the injectable schema view on `FTuningResolveInput` (`TConstArrayView<FTuningSchemaRow> Schema`, amendment 050), and the injectable `ITuningPresetSource` (story 050) to return each rejecting preset with its `bIsPublic` flag. Fixture conditions as in AC-17: non-`HostSelectable` presets with `NpcKillStrikeLimit` 1 and `bTestAllowsStrikeLimitOne` so I-4 cannot interfere; the I-3 case is the exception and uses a `HostSelectable` preset with `NpcKillStrikeLimit` 2 and no flag (a `HostSelectable` preset with the flag is rejected on that ground alone); the I-5 pair is the fixture range pair `FixtureMinSec`/`FixtureMaxSec`.
- Rejecting fixtures (values from GDD AC-17): `DefenceDurationSec` 5 with `DefenceCooldownSec` 39.9 (I-1); `TeamSize` 5 with `NpcCount` 9, and `TeamSize` 4 with `NpcCount` 7 (I-2); `HostSelectable` `TeamSize` 1 with `NpcCount` 21 (I-3); Min greater than Max (I-5).
- Assertions per fixture: the result has `FallbackLevel` 1 (`ETuningFallback::Default`), `PresetId` is `Default`, and the active values equal the `Default` preset's values field by field. "No partly repaired value" is asserted by comparing every key, not only the offending one, so a clamped dependent key or a half-applied preset is detected. The `FValidationReport` names the invariant ID that rejected.
- Valid companion fixture (spec edge case): the passing boundary values (cooldown 40, `NpcCount` 10 with `TeamSize` 5, 22 in the HostSelectable case) resolve at `FallbackLevel` 0 with the requested preset active.
- Faults: each rejection logs expected warnings; declare them through the fault sink and log-capture seams (stories 008 and 009) so zero unexpected faults holds at teardown.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: the invariant table results for each listed value (the AC-17 invariant-table half).
- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: `Resolve()`, the preset-source seam and the Default fallback itself (AC-6a).
- `production/epics/tuning-data-config/story-098-derived-indicators-preset-scope.md`: the AC-18 preset-scope clauses.
- `production/epics/tuning-data-config/story-051-fallback-to-schema.md` and `story-053-fallback-failed.md`: the lower fallback rungs.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthFoundation/Private/Tests/Tuning/InvariantRejectsWholePresetTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-17 whole-preset rejection
  - Given: the AC-17 rejecting fixtures (I-1 cooldown 39.9; I-2 `NpcCount` 9 and 7; I-3 `HostSelectable` 21; I-5 Min greater than Max).
  - When: `Resolve()` runs on each.
  - Then: the whole preset is rejected with no partly repaired value; `Default` is active; `FallbackLevel` is 1.
  - Edge cases: a valid companion fixture stays at `FallbackLevel` 0.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation test `StealthGame.Tuning.InvariantRejectsWholePreset` in `Source/StealthFoundation/Private/Tests/Tuning/InvariantRejectsWholePresetTest.cpp` must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`, `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`
- Unlocks: None
- Open verification claims: none. The title of this story is derived from its AC because the amendments give only a slug.
