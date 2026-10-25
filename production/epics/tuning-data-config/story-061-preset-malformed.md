# Story 061: AC-2c PresetMalformed

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-2c, CR-4

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-4 says anything unknown, padded, or containing path or separator characters resolves `Default` with a warning. The preset name comes from a launch option that ends up in logs, travel URLs (`?Preset=`) and the catalog lookup, so a value carrying whitespace, a path (`/../`) or a URL separator (`;`) must never be trimmed into a valid match or forwarded. This story adds the malformed-value rejection to `ResolveRoundPreset` in the store (story 058), next to the selectability check (059) and the case-insensitive match (060).

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: `?Tuning=`/`ResolveRoundPreset` matching is case-insensitive against catalog names; anything with padding, path or separator characters resolves `Default` with a warning (AC-2c).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Pre-cutoff APIs only; warning through `UE_LOGF` (`%ls` for `TCHAR*` [V `LogMacros.h:291`]). None of ADR-0004's three Verification Required items is touched.

**Control Manifest Rules (this layer)**:
- Required: `?Tuning=` code sits inside `!UE_BUILD_SHIPPING` blocks — source: ADR-0004
- Required: new code logs with `UE_LOGF` and a `LogStealth...` category — source: ADR-0007
- Forbidden: never use wall-clock time as a test pass/fail predicate — source: ADR-0008
- Guardrail: validation under 1 ms at `Preparing` — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-2c (CR-4)**: GIVEN `Playtest` is `HostSelectable`, WHEN the host opens with each of `" Playtest"`, `"Playtest/../x"` and `"Playtest;"`, THEN `Default` is active plus one warning each. (`StealthGame.Tuning.PresetMalformed`)

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Loading, `?Tuning=` matching):*

- **Where.** `ResolveRoundPreset` in `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`. Validate the raw option value before any catalog lookup: leading or trailing whitespace, path characters (`/`, `\`, `..`) and separator characters (`;`, and by the same rule `?`, `&`, `=`) mean malformed, which resolves `Default` with one warning. The ADR and GDD list "padding, path or separator characters" without a closed character set: the exact set is open; the three AC fixtures must be covered and the rule should be an allowlist of identifier characters (letters, digits, underscore) rather than a growing blocklist, which is a proposal for `/dev-story`.
- **No trimming.** A padded value is not trimmed into a match (QA edge case). `" Playtest"` must not select `Playtest`.
- **The warning names the option, not the payload.** The warning names the `Tuning` option and says the value was rejected; it must not echo the injected path text as a name or a log field that something could parse (QA edge case). Whether to include the raw value at all, sanitised, is open; the safe default is to log only that the value was malformed and its length.
- **Three separate resolves.** The test runs three resolves (one per value), each with its own log-capture scope, and asserts exactly one warning and `Default` active for each (QA spec: "three separate resolves, one warning each").
- **Fixture.** `Playtest` HostSelectable: the real catalog (story 056) qualifies, or a test catalog if story 059's injection exists. Assert on `PresetId`, since `Playtest` equals `Default` in value.
- **Test.** `Source/StealthFoundation/Private/Tests/Tuning/PresetMalformedTest.cpp`, class `FTuningPresetMalformedTest`, name `StealthGame.Tuning.PresetMalformed`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`; uses `FStealthLogCapture` (story 009) and the fault-sink teardown helper (story 008).
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store and the no-option path
- `production/epics/tuning-data-config/story-059-preset-not-selectable.md`: a well-formed name that is not HostSelectable
- `production/epics/tuning-data-config/story-060-preset-case-insensitive.md`: case-insensitive matching of a well-formed name
- `production/epics/tuning-data-config/story-063-preset-survives-travel.md`: the travel URL `?Preset=` disagreement warning

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/PresetMalformedTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: malformed option values resolve `Default` with one warning each (test `StealthGame.Tuning.PresetMalformed`)
  - Given: `Playtest` HostSelectable, log capture
  - When: the host opens with each of `" Playtest"`, `"Playtest/../x"` and `"Playtest;"`
  - Then: for each opening `Default` is active and exactly one warning is logged (three separate resolves, one warning each)
  - Edge cases: whitespace is not trimmed into a match; the warning names the option, not the injected path

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/PresetMalformedTest.cpp` must exist and pass (`StealthGame.Tuning.PresetMalformed`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-009-log-capture-hook.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. Open items found while writing: the closed character set that counts as malformed, and whether the warning may echo a sanitised value.
