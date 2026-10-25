# Story 049: AC-26 SafeRangeWarnOnly

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-26 (CR-15.5), Edge Cases ("inside its hard range but outside its safe range")

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 3, F-1: safe-range excursions are a warning only)
**ADR Decision Summary**: The validator keeps a value that is inside its hard range but outside its safe range, logs a warning naming the key, and accepts the preset. Safe ranges are recommendations; only hard ranges clamp.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API beyond `UE_LOGF` (5.8) for the warning. No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range) (source: ADR-0004)
- Required: New code logs with `UE_LOGF` and a `LogStealth...` category (source: ADR-0007)
- Forbidden: Never repair a failed invariant by clamping a dependent key (source: ADR-0004)
- Guardrail: Validation under 1 ms at `Preparing` (source: ADR-0004)

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-26, scoped to this story:*

- [x] **AC-26 (CR-15.5)**: GIVEN a non-selectable preset with `NpcCount` 60 (inside the hard range, outside the safe range), WHEN validated, THEN the value stays 60, one safe-range warning names the key and the preset is accepted. (`StealthGame.Tuning.SafeRangeWarnOnly`)

---

## Implementation Notes

*Derived from ADR-0004 section 3 and GDD CR-15.5:*

- Files: `Source/StealthFoundation/Private/Tuning/StealthTuningValidate.cpp` (extend the F-1 stage of story 046 with the safe-range check, in the same pass: after the hard-range clamp, a value outside `[SafeMin, SafeMax]` but inside the hard range logs one warning naming the key and keeps the value) and test `Source/StealthFoundation/Private/Tests/Tuning/SafeRangeWarnOnlyTest.cpp`.
- Case: `NpcCount` hard range 6 to 100, safe range 45 to 50 (GDD Canonical Tunables, re-read it): 60 is inside hard and outside safe, so it stays 60 with exactly one safe-range warning naming `NpcCount` and the preset is accepted. A value exactly on a safe bound gives no warning; a value outside the hard range still clamps with the clamp warning (spec edge cases), which is story 046's behaviour and must not regress.
- Exactly one warning: the safe-range warning must not duplicate the clamp warning; a value that is clamped is judged on its effective value (a clamped value lands on the hard bound, which may or may not be inside the safe range; decide and test the one choice, the inputs are silent on a clamped value that is also outside safe, so record the chosen behaviour in a comment).
- Use `FStealthLogCapture` (story 009, `Source/StealthFoundation/Public/Server/Testing/StealthLogCapture.h`, G1 path) to count warnings by key. Story 009 is a dependency of story 046, which this story depends on, so it is available.
- Acceptance wording: the spec says "preset accepted (FallbackLevel 0)". `FallbackLevel` and `Resolve()` arrive in story 050, which is not a dependency of this story. At this story, assert acceptance at validation-stage level: the stage reports no rejection and no invariant is evaluated that could reject (the AC fixture is a single-key preset). The `FallbackLevel` 0 half cannot be asserted until `Resolve()` exists; flagged for the story reviewer, and a follow-on assertion belongs in story 050's test run or a later story rather than a hidden skip.
- Test class `FTuningSafeRangeWarnOnlyTest`, name `StealthGame.Tuning.SafeRangeWarnOnly`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`, headless `-nullrhi`.
- Prototype: none (no `Prototype:` line in the plan).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-046-f1-validate-clamp-nan.md`: the F-1 clamp, NaN and sentinel behaviour this story extends.
- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: `Resolve()`, `FallbackLevel`, whole-preset rejection.
- `production/epics/tuning-data-config/story-094-safe-range-lint-baseline.md`: the editor `SafeRangeLint` that probes safe ranges against all invariants (a different thing: lint, not runtime warning).
- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: cross-field invariants.

---

## Files

- `Source/StealthFoundation/Private/Tuning/StealthTuningValidate.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/SafeRangeWarnOnlyTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: A value inside its hard range and outside its safe range is kept with one warning and the preset is accepted.
  - Given: a non-selectable preset with `NpcCount` 60 (inside hard, outside safe), log capture.
  - When: validated.
  - Then: `NpcCount` stays 60; exactly one safe-range warning naming `NpcCount`; the preset is accepted (FallbackLevel 0).
  - Edge cases: a value exactly on a safe bound gives no warning; a hard-range violation still clamps. (Note: the FallbackLevel 0 assertion needs `Resolve()` from story 050; see Implementation Notes.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/SafeRangeWarnOnlyTest.cpp` (`StealthGame.Tuning.SafeRangeWarnOnly`) must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Tuning.SafeRangeWarnOnly`)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-046-f1-validate-clamp-nan.md`
- Unlocks: None
- Open verification claims from the plan Note lines: none (the plan has no Note line for this story). Open points: the `FallbackLevel` 0 assertion depends on story 050 (not a listed dependency), and the behaviour for a clamped value that also lies outside its safe range.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (NpcCount 60 stays 60 with exactly one safe-range warning naming NpcCount and no F-1 change; 45 and 50 on the safe bounds give no warning; NpcCount 0 still clamps to 6 with only its clamp warning; end to end through Resolve() (story 050 landed in the same batch) a non-selectable preset with NpcCount 60 is accepted at FallbackLevel 0 with one warning in the whole resolve, which settles the open FallbackLevel 0 point)
**Decisions**: a key F-1 changed is judged by its clamp warning only, never a second safe-range warning for its effective value (one warning per key; comment in RunF1); sentinel values and bools never warn; the note is recorded in FTuningF1Report::SafeRange. The story 046 test's hard-bound case now asserts no F-1 warning and that every other warning is a safe-range note (a hard bound outside the safe range now legitimately warns, CR-15.5)
**Files changed beyond the list**: `Source/StealthFoundation/Private/Tests/Tuning/ValidateClampAndNanTest.cpp`, `Source/StealthFoundation/Public/Server/StealthTuningPipelineCore.h`
**Deviations**: None
**Test Evidence**: `Source/StealthFoundation/Private/Tests/Tuning/SafeRangeWarnOnlyTest.cpp`; `production/qa/evidence/story-049-safe-range-warn-only/stamp.txt`; full suite 70/70 green (`tools/dev/cycle.ps1 -Tests -NoLaunch`: Result: Succeeded, tests exit=0)
**Run result**: N/A — validator core; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects left
