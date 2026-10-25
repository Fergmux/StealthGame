# Story 060: AC-2b PresetCaseInsensitive

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-2b, CR-4

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-4 says the stored preset name is matched case-insensitively against the catalog. A host typing `?Tuning=playtest` should get the `Playtest` preset rather than a fallback, because launch options are typed by hand and a case slip must not silently put a playtest on `Default` values. This story adds case-insensitive matching to `ResolveRoundPreset` in the store (story 058). It sits beside the stricter rule of story 061 (padded or path-bearing values are not matched), so the match must be a case-fold of the whole token and nothing looser.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: `?Tuning=`/`ResolveRoundPreset` matching is case-insensitive against catalog names; anything with padding, path or separator characters resolves `Default` with a warning.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Pre-cutoff APIs only. `FName` comparison in UE is case-insensitive by default, but whether the project's `PresetId` handling relies on that or on explicit `FString::Equals(..., ESearchCase::IgnoreCase)` is not stated in the inputs: open, and the test must not depend on either by accident (see the mixed-case edge case below). None of ADR-0004's three Verification Required items is touched.

**Control Manifest Rules (this layer)**:
- Required: `?Tuning=` code sits inside `!UE_BUILD_SHIPPING` blocks — source: ADR-0004
- Required: new code logs with `UE_LOGF` and a `LogStealth...` category — source: ADR-0007
- Forbidden: never use wall-clock time as a test pass/fail predicate — source: ADR-0008
- Guardrail: validation under 1 ms at `Preparing` — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-2b (CR-4)**: GIVEN `Playtest` is `HostSelectable`, WHEN the host opens with `?Tuning=playtest`, THEN `Playtest` is active. (`StealthGame.Tuning.PresetCaseInsensitive`)

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Loading, `?Tuning=` matching):*

- **Where.** `ResolveRoundPreset` in `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`. Compare the option value against each catalog `PresetId` ignoring case, then recheck `HostSelectable` (story 059 owns the not-selectable branch). The active `PresetId` reported afterwards is the catalog's spelling (`Playtest`), not the option's.
- **Test shape.** Playtest HostSelectable (the real catalog from story 056 qualifies; it makes `Playtest` HostSelectable) or an in-memory test catalog if story 059's injection seam exists. The three observable outcomes per the QA spec: `Playtest` active, `FallbackLevel` 0, and no warning (log capture, story 009). Note that the real `Playtest` equals `Default` in value, so the test must assert on `PresetId`, not on values.
- **Edge case: mixed case.** Run the resolve with `playtest`, `PLAYTEST` and `pLaYtEsT`; all must activate `Playtest`. A string compare that happens to be case-insensitive for only one of these would be caught.
- **Not a loosening.** Case-folding must not turn a padded value (`" playtest"`) into a match; that stays story 061's rejection. Do not trim.
- **Test.** `Source/StealthFoundation/Private/Tests/Tuning/PresetCaseInsensitiveTest.cpp`, class `FTuningPresetCaseInsensitiveTest`, name `StealthGame.Tuning.PresetCaseInsensitive`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. Assert zero unexpected faults at teardown (story 008).
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store and the no-option path
- `production/epics/tuning-data-config/story-059-preset-not-selectable.md`: a preset that is not HostSelectable
- `production/epics/tuning-data-config/story-061-preset-malformed.md`: padded, path-bearing and separator-bearing values (no trimming)
- `production/epics/tuning-data-config/story-064-presets-cooked.md`: loading `Playtest` in a cooked build

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/PresetCaseInsensitiveTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `?Tuning=playtest` activates `Playtest` (test `StealthGame.Tuning.PresetCaseInsensitive`)
  - Given: `Playtest` HostSelectable; the log-capture hook (story 009)
  - When: the host opens with `?Tuning=playtest`
  - Then: `Playtest` is active, `FallbackLevel` 0, no warning
  - Edge cases: mixed case

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/PresetCaseInsensitiveTest.cpp` must exist and pass (`StealthGame.Tuning.PresetCaseInsensitive`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-009-log-capture-hook.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. Open item found while writing: whether case-insensitivity comes from `FName` semantics or an explicit ignore-case compare.
