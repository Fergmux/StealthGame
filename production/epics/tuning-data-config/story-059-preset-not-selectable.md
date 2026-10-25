# Story 059: AC-2a PresetNotSelectable

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-2a, CR-4

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-4 says that at each `Preparing`, TDC calls `ResolveRoundPreset(Name)` on the stored preset name: it matches case-insensitively against the catalog, rechecks `HostSelectable` and the Shipping rule, and selects `Default` with a warning for anything else. This story covers the `HostSelectable` recheck: a catalog preset that is not HostSelectable must not be chosen by a host's `?Tuning=` option, because a non-selectable preset (for example the test-only `Solo` or a preset with `bTestAllowsStrikeLimitOne`) exists for tests and must never be reachable by a player. The preset store itself is story 058; this story adds the selectability check to its resolve path.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: `?Tuning=`/`ResolveRoundPreset` matching is case-insensitive against catalog names, rechecks `HostSelectable` (honoured only in Development and Test) and resolves `Default` with a warning for anything not selectable; the store, not a client or a travel URL, owns the name.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Pre-cutoff APIs only; warning through `UE_LOGF` on `LogStealthFoundation`. None of ADR-0004's three Verification Required items is touched.

**Control Manifest Rules (this layer)**:
- Required: `?Tuning=` code sits inside `!UE_BUILD_SHIPPING` blocks (Shipping always resolves `Default`) — source: ADR-0004
- Required: new code logs with `UE_LOGF` and a `LogStealth...` category — source: ADR-0007
- Forbidden: never use wall-clock time as a test pass/fail predicate — source: ADR-0008
- Guardrail: validation under 1 ms at `Preparing` — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-2a (CR-4)**: GIVEN a Dev build with test catalog entry `Playtest` not `HostSelectable`, WHEN the host opens with `?Tuning=Playtest`, THEN `Default` is active and one warning names the option. (`StealthGame.Tuning.PresetNotSelectable`)

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Loading, `?Tuning=` matching) and section 9 (HostSelectable):*

- **Where.** `ResolveRoundPreset` in `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp` (created by story 058). After matching the option against the catalog, a non-HostSelectable entry yields `Default` and exactly one warning that names the option. The warning goes to `LogStealthFoundation` through `UE_LOGF` and into the `FValidationReport`.
- **Exactly one warning.** The fallback to `Default` must not add a second warning of its own (the QA spec's edge case). `Default` resolves cleanly, so only the selectability warning is logged.
- **Test catalog.** The criterion needs a catalog in which `Playtest` is not HostSelectable, while the real catalog (story 056) makes it selectable. The store must therefore accept a test catalog or a test `ITuningPresetSource` / catalog view. The injection mechanism is not in the inputs and is open: either a test-only setter under `WITH_STEALTH_TEST_SEAMS` or a catalog-source seam that the store takes in `Initialize` and that story 058 can also use. The test builds the catalog in memory, never as a `.uasset`.
- **Warning content.** The warning must name the option (the `Tuning` option and the offered value). The exact text is defined by the first implementation; `FStealthLogCapture` (story 009) matches on key tokens, so keep the option name as a whole token.
- **Test.** `Source/StealthFoundation/Private/Tests/Tuning/PresetNotSelectableTest.cpp`, class `FTuningPresetNotSelectableTest`, name `StealthGame.Tuning.PresetNotSelectable`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. Capture with `FStealthLogCapture` and assert Default active, `FallbackLevel` per the store's own result, one warning naming the option, zero unexpected faults at teardown (story 008).
- **"Dev build".** As in story 058, the test runs as editor automation in a non-Shipping build; the GDD text is kept unchanged.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store and the no-option path
- `production/epics/tuning-data-config/story-060-preset-case-insensitive.md`: case-insensitive matching of a selectable preset
- `production/epics/tuning-data-config/story-061-preset-malformed.md`: padded, path-bearing or separator-bearing option values
- `production/epics/tuning-data-config/story-062-client-option-ignored.md`: client login options
- `production/epics/tuning-data-config/story-089-no-shipping-back-door-scan.md`: the Shipping behaviour of `?Tuning=Playtest`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp` (`ResolveRoundPreset`)
- `Source/StealthFoundation/Private/Tests/Tuning/PresetNotSelectableTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a non-HostSelectable `Playtest` requested through `?Tuning=Playtest` resolves `Default` with one warning (test `StealthGame.Tuning.PresetNotSelectable`)
  - Given: a test catalog entry `Playtest` with `bHostSelectable` false, the log-capture hook (story 009)
  - When: the host opens with `?Tuning=Playtest`
  - Then: `Default` is active; exactly one warning naming the option
  - Edge cases: no second warning from the fallback

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/PresetNotSelectableTest.cpp` must exist and pass (`StealthGame.Tuning.PresetNotSelectable`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-009-log-capture-hook.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. Open items found while writing: the catalog-injection seam for a test catalog, and the warning text.
