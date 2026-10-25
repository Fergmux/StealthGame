# Story 010: Negative-control helper: the suite passes only if the inner check fails

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 3; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: "A gate you have not watched fail is not a gate": every leak, parity, statistical and lint gate ships a negative control that runs in the same suite and must fail the inner check, so the suite passes only if the control is detected; negative-control types live only in the harness module or in `#if WITH_DEV_AUTOMATION_TESTS` blocks, never in runtime production code.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff API. The helper wraps `FAutomationTestBase` reporting (pre-cutoff). Tests use `EditorContext | ProductFilter` only (ADR-0008 Verification 7 is open [U]). An inner check cannot be allowed to call `check`/`checkf`: those abort the process and cannot be captured.

**Control Manifest Rules (this layer)**:
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects (source: ADR-0008)
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail (source: ADR-0008)
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) (source: ADR-0008)
- Required: Every `Public/Server/` header includes `StealthServerOnlyGuard.h` first (source: ADR-0007)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [x] A `FStealthNegativeControl` helper runs an inner check expected to fail and makes its enclosing test fail when the inner check passes (control not detected); a self-test proves both directions (test `StealthGame.Harness.NegativeControlDetected`).

---

## Implementation Notes

*Derived from ADR-0008 section 3 and amendment G1:*

- **Header** `Source/StealthFoundation/Public/Server/Testing/StealthNegativeControl.h` (amendment G1 replaces the plan's `Private/Tests/Support/StealthNegativeControl.h`): guard include first, whole content under `#if WITH_STEALTH_TEST_SEAMS`. `FStealthNegativeControl` runs an inner check supplied by the caller and reports to the enclosing test: when the inner check fails as expected the control is "detected" and the enclosing test passes; when the inner check passes the helper adds the error "control not detected" (with the control's name) to the enclosing test.
- **Result object**: expose the outcome as a value (detected or not, plus the message) so the self-test can observe the not-detected direction without failing itself (spec: observed by capturing the result). The reporting step that actually fails the enclosing test is a separate, explicit call.
- **Inner check shape** (design suggestion, not fixed by the inputs): the inner check receives a small context object that collects its failures instead of the real `FAutomationTestBase`, so an inner `TestTrue`-style assertion does not itself fail the enclosing test. This also covers the spec edge case "inner check that throws/asserts" for assertion-style failures; process-aborting `check` calls are out of scope and must not appear in inner checks.
- **Reuse**: the helper may be used more than once in one test (spec edge case); keep the state per instance, no statics.
- **Test** `StealthGame.Harness.NegativeControlDetected`: class `FHarnessNegativeControlDetectedTest` in `Source/StealthFoundation/Private/Tests/Harness/NegativeControlTest.cpp`, flags `EditorContext | ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. Direction (a): the inner check fails as expected, so the control is detected and the enclosing test passes; direction (b): the inner check passes, so the result is "control not detected" and the helper would fail the enclosing test (observed via the captured result). Also: use the helper twice in one test.
- **Sequencing flag**: `WITH_STEALTH_TEST_SEAMS` comes from story 008, which is not a listed dependency here; land the define first.
- Many later gates use this helper (lint seeded controls story 016, compile probes, the leak suites); keep its API small and stable.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`: the module this header lives in
- `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`: the `WITH_STEALTH_TEST_SEAMS` define
- `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`: the lint-specific seeded controls (script side, not this C++ helper)
- `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`: the compile-probe positive control mechanism

---

## Files

- `Source/StealthFoundation/Public/Server/Testing/StealthNegativeControl.h`
- `Source/StealthFoundation/Private/Tests/Harness/NegativeControlTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: `FStealthNegativeControl` makes the enclosing test fail when the inner check passes, and passes when the inner check fails
  - Given: `FStealthNegativeControl` wrapping an inner check
  - When: (a) the inner check fails as expected; (b) the inner check passes
  - Then: (a) the enclosing test passes; (b) the enclosing test fails with "control not detected" (observed by capturing the result, not by failing the self-test)
  - Edge cases: an inner check that throws or asserts; the helper used twice in one test

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Harness/NegativeControlTest.cpp` (test `StealthGame.Harness.NegativeControlDetected`) must exist and pass (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`StealthGame.Harness.NegativeControlDetected`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md` (added after review: the `WITH_STEALTH_TEST_SEAMS` define)
- Unlocks: None (no later story lists 010 as a dependency in the plan or amendments; later gate stories use the helper without declaring it)
- Sequencing note (not a plan dependency): needs the `WITH_STEALTH_TEST_SEAMS` define of `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`.
- Open verification claims from the plan's Note lines: none (no Note line on 010).

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: (a) failing inner check -> detected, inner failure kept, `Report` adds no error and the enclosing test has no errors; (b) passing inner check -> result "control not detected: <name>", observed through the returned value (Report would add that error); edges: an assertion-style inner failure (`AddFailure`) is detected; an inner check with no assertions is not detected; the helper used several times in one test, runs independent, `RunAndReport` on a detected control; fault teardown clean)
**Decisions**: API `FStealthNegativeControl(Name)`, `Run(TFunctionRef<void(FStealthCheckContext&)>) -> FStealthNegativeControlResult` (`bDetected`, `InnerFailures`, `Describe()`), explicit `static Report(FAutomationTestBase&, Result)` and `RunAndReport`. The inner check reports into `FStealthCheckContext` (`TestTrue`/`TestFalse`/`TestEqual`/`AddFailure`), never the real test. `check()` in an inner check is out of scope (it aborts the process); documented in the header.
**Deviations**: None
**Test Evidence**: `production/qa/evidence/story-010-negative-control-helper/test-run.txt` (build `Result: Succeeded`; 22/22 `StealthGame.` tests Success) and `stamp.txt`
**Run result**: N/A — test-seam helper; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects found
