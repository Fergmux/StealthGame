# Story 046: AC-4 ValidateClampAndNan: the F-1 validation stage

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-4, CR-6 (a), (b), F-1

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 3, F-1 per key)
**ADR Decision Summary**: The plain-C++ validator `FTuningPipelineCore` runs F-1 per key first: a non-finite float (tested before clamping) or an undeclared enum value takes the schema default with a warning; otherwise the value is clamped to the key's hard range with a warning naming key, authored and effective value; exactly on a bound there is no warning; safe-range excursions only warn. F-1 is the only code that changes a value.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: `UE_LOGF` (5.8, preferred over `UE_LOG`) for the validator warnings, format literals `"..."` and `%ls` for `TCHAR*` [V `LogMacros.h:291`]; this is the only post-cutoff API in the story. `FMath::IsFinite` / `FMath::IsNaN` are long-standing. No unverified [U] claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range) (source: ADR-0004)
- Required: New code logs with `UE_LOGF` and a `LogStealth...` category (source: ADR-0007)
- Forbidden: Never use `UE_LOG` in new code (source: ADR-0007)
- Guardrail: Validation under 1 ms at `Preparing`; the validator suite runs headless in under 10 s (source: ADR-0004)

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-4, scoped to this story:*

- [ ] **AC-4 (CR-6, F-1)**: GIVEN the F-1 stage in isolation (the clamp-and-finite-test unit, run before the invariants; the whole-preset outcome of this fixture is covered by AC-6a and AC-17, because `NpcCount` 6 with `TeamSize` 5 would fail I-2) and a preset with `ObjectiveInteractSec` NaN, `DefenceDurationSec` 7.5, `AssassinationRangeCm` 50, `NpcKillStrikeLimit` 0 and `NpcCount` 0, WHEN validated, THEN the effective values are 3.0, 5.0, 100, 2 and 6, and the captured log holds one warning per key naming its authored and effective value. AND +Infinity and -Infinity each yield 3.0, AND `NpcKillStrikeLimit` 9 yields 5, AND values exactly on a bound produce no warning, AND `EndedAutoResetSec` 0 stays 0 with no warning while 1 yields 3 with one warning (sentinel rule), AND `NpcKillStrikeLimit` 0 yields 1 only for a preset flagged `bTestAllowsStrikeLimitOne`. (`StealthGame.Tuning.ValidateClampAndNan`)

---

## Implementation Notes

*Derived from ADR-0004 section 3 and GDD F-1:*

- Files: `Private/Tuning/StealthTuningValidate.cpp` (the F-1 stage), `Public/Server/StealthTuningPipelineCore.h` (guard header first; this story introduces the header, story 050 extends it with `FTuningResolveInput/Result`, `FValidationReport`, `ITuningPresetSource`, `ETuningFallback`), test `Private/Tests/Tuning/ValidateClampAndNanTest.cpp`, all in `Source/StealthFoundation/`. Plain C++, no `UWorld`. Warnings go to `LogStealthFoundation` through `UE_LOGF` and into the validation report structure that story 050 completes (tuning values are not secret, so logging them is allowed).
- F-1 rule per key, in this order: (1) a float that is not finite, or an enum value outside its declared set, takes the schema default (the finite test runs BEFORE clamping, because clamp of NaN is NaN; +Infinity and -Infinity take the default, not a bound); (2) a value equal to a sentinel the schema row declares is accepted unchanged with no warning; (3) otherwise clamp to `[HardMin, HardMax]` with a warning naming key, authored value and effective value; a value exactly on a bound gives no warning; bools pass unchanged. Safe-range excursions are a warning only; they are verified by story 049 and may share this file.
- Schema source: the stage must take the schema as an input view (`TConstArrayView<FTuningSchemaRow>`) rather than calling `GetTuningSchema()` directly, so the spec's test schema fixture (with an `EndedAutoResetSec` sentinel row) can be used; the story 050 amendment adds the same injectable view to `FTuningResolveInput`. Fixture rows for the AC keys come from the real canonical keys (story 040) where they exist and from fixture rows otherwise (`EndedAutoResetSec`: sentinel 0, continuous range 3 to 600; `SessionHeartbeatIntervalSec` is another sentinel key in the GDD, not needed here).
- `bTestAllowsStrikeLimitOne`: the `NpcKillStrikeLimit` hard minimum is 1 instead of 2 only for a preset whose catalog entry sets the flag, never for `Default` or a `HostSelectable` preset. How the flag reaches the F-1 stage is not specified (the ADR `ITuningPresetSource::Load` shows only `bOutIsPublic`); pass it as an explicit stage input and record the carrier choice in the header doc comment (open, shared with story 050). The flag is compiled out of Shipping (ADR-0004 section 1, same open point as story 039).
- Expected numbers (all from the AC): NaN `ObjectiveInteractSec` gives 3.0 (default); `DefenceDurationSec` 7.5 gives 5.0; `AssassinationRangeCm` 50 gives 100; `NpcKillStrikeLimit` 0 gives 2 (or 1 with the flag); `NpcCount` 0 gives 6; `NpcKillStrikeLimit` 9 gives 5; `EndedAutoResetSec` 0 stays 0 and 1 gives 3 with one warning.
- Log capture: use `FStealthLogCapture` from story 009 (`Source/StealthFoundation/Public/Server/Testing/StealthLogCapture.h`, G1 path; an `FOutputDevice` RAII scope under `WITH_STEALTH_TEST_SEAMS`) to assert exactly one warning per key naming authored and effective value, and no warning for a bound or sentinel value. Assert zero unexpected faults at teardown if the fault sink (story 008) is wired (F-1 itself reports no fault).
- Whole-preset outcome is out of scope: this fixture on a full preset would fail I-2 (`NpcCount` 6 with `TeamSize` 5), and AC-6a/AC-17 cover whole-preset behaviour. Keep F-1 free of any invariant evaluation.
- Test class `FTuningValidateClampAndNanTest`, name `StealthGame.Tuning.ValidateClampAndNan`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`, headless `-nullrhi`. No wall-clock use, no randomness; the NaN and Infinity values come from `std::numeric_limits<float>`.
- Prototype: none (no `Prototype:` line in the plan).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-009-log-capture-hook.md`: the log-capture hook this test uses.
- `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md`: the schema table the stage walks.
- `production/epics/tuning-data-config/story-047-derived-indicators.md`: F-3 indicators.
- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: cross-field invariants (reject or warn, never repair).
- `production/epics/tuning-data-config/story-049-safe-range-warn-only.md`: the safe-range warning-only rule.
- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: `Resolve()`, the fallback ladder and whole-preset rejection.
- `production/epics/tuning-data-config/story-039-preset-asset-classes.md`: the catalog flag `bTestAllowsStrikeLimitOne` and its catalog check.

---

## Files

- `Source/StealthFoundation/Private/Tuning/StealthTuningValidate.cpp`
- `Source/StealthFoundation/Public/Server/StealthTuningPipelineCore.h`
- `Source/StealthFoundation/Private/Tests/Tuning/ValidateClampAndNanTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: The F-1 stage clamps, defaults non-finite values, honours sentinels and warns exactly once per changed key.
  - Given: the F-1 stage alone over a test schema fixture (an `EndedAutoResetSec` sentinel row is needed), log capture.
  - When: validated: `ObjectiveInteractSec` NaN, `DefenceDurationSec` 7.5, `AssassinationRangeCm` 50, `NpcKillStrikeLimit` 0, `NpcCount` 0.
  - Then: effective 3.0, 5.0, 100, 2, 6; exactly one warning per key naming authored and effective value; +/-Infinity give 3.0; `NpcKillStrikeLimit` 9 gives 5; exact bounds no warning; `EndedAutoResetSec` 0 stays 0 with no warning, 1 gives 3 with one warning; `NpcKillStrikeLimit` 0 gives 1 only for a `bTestAllowsStrikeLimitOne` preset.
  - Edge cases: NaN tested before clamping; whole-preset outcome is out of scope (AC-6a/17).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/ValidateClampAndNanTest.cpp` (`StealthGame.Tuning.ValidateClampAndNan`) must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md`, `production/epics/tuning-data-config/story-009-log-capture-hook.md`
- Unlocks: `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`, `production/epics/tuning-data-config/story-049-safe-range-warn-only.md`
- Open verification claims from the plan Note lines: none. The plan Note ("finite test runs before clamping; logs via UE_LOGF on LogStealthFoundation") is a rule, not an open claim; the `UE_LOGF` format-literal detail is [V `LogMacros.h:291`]. Open design point: how `bTestAllowsStrikeLimitOne` reaches the stage.
