# Story 008: IStealthFaultSink and the WITH_STEALTH_TEST_SEAMS define

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 4; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Every invariant violation, dead registration, rejected tuning or stale epoch reports to an `IStealthFaultSink`, and every T1-T3 test asserts zero unexpected faults at teardown while declaring expected ones explicitly; test-only hooks sit behind `WITH_STEALTH_TEST_SEAMS`, defined `!UE_BUILD_SHIPPING` by StealthFoundation as a public definition.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff API. `EAutomationTestFlags` is an enum class and every test needs an application-context flag (`AutomationTest.h:93-137, 4127`). Whether `WITH_DEV_AUTOMATION_TESTS` is defined in cooked Development game builds is open [U] (ADR-0008 Verification 7), so tests use `EditorContext | ProductFilter` only. The Shipping value of the define is proven by story 006, not here.

**Control Manifest Rules (this layer)**:
- Required: Test-only hooks sit behind `WITH_STEALTH_TEST_SEAMS`; never compile negative-control types or test seams into Shipping (source: ADR-0008)
- Required: Every test asserts zero unexpected faults at teardown and declares expected ones explicitly (source: ADR-0008)
- Required: Registrations are weak; a dead registration is a fault-sink report, never a crash (source: ADR-0007)
- Required: Every `Public/Server/` header includes `StealthServerOnlyGuard.h` first (source: ADR-0007)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [ ] `IStealthFaultSink::Report(EStealthFault, FName Source, int64 Detail)` exists in `FND/Public/Server/`, `WITH_STEALTH_TEST_SEAMS` is defined as `!UE_BUILD_SHIPPING` by StealthFoundation as a public definition, and a test helper asserts zero unexpected faults at teardown while allowing explicitly declared expected ones (test `StealthGame.Harness.FaultSinkTeardownAssertion`).

---

## Implementation Notes

*Derived from ADR-0008 sections 4, Key Interfaces; ADR-0007 section 4; amendment G1:*

- **Interface** `Source/StealthFoundation/Public/Server/StealthFaultSink.h`: guard include first, `enum class EStealthFault`, and a plain C++ `IStealthFaultSink` with `virtual void Report(EStealthFault Fault, FName Source, int64 Detail)`. ADR-0008 also names a test-side accessor `GetUnexpectedFaults()` available "via `WITH_STEALTH_TEST_SEAMS`": put it on the recording implementation (below), not on the production interface, so no seam symbol leaks into Shipping.
- **`EStealthFault` members**: the inputs do not enumerate them. ADR-0008 section 4 names the categories that report here (invariant violation, dead registration, rejected tuning, stale epoch); declare members for those and let later stories append. Never reuse or renumber a value once published.
- **Define**: in `StealthFoundation.Build.cs` add a public definition `WITH_STEALTH_TEST_SEAMS` that is 1 in every configuration except Shipping and 0 in Shipping (the ADR wording is `!UE_BUILD_SHIPPING`, which also yields 1 in the Test configuration; the harness is excluded from Test, so the story 006 scan covers Shipping only). It is a public definition so dependents (harness, editor module, tests) see it.
- **Test support** (under `#if WITH_STEALTH_TEST_SEAMS`, guard first), in `Source/StealthFoundation/Public/Server/Testing/StealthFaultSinkTestSupport.h` (file name proposed; the plan names no file for the helper, and amendment G1 puts shared test support under `Public/Server/Testing/`): a recording sink implementing `IStealthFaultSink` that stores every report, and a teardown assertion helper that takes the declared expected faults and returns a result object (unexpected faults, declared-but-never-raised faults). Return a result rather than failing the test directly so the self-test can assert on the failing case without failing itself (spec case (b)).
- **Test** `StealthGame.Harness.FaultSinkTeardownAssertion`: class `FHarnessFaultSinkTeardownAssertionTest` in `Source/StealthFoundation/Private/Tests/Harness/FaultSinkTest.cpp`, flags `EditorContext | ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. Cases: (a) no faults reported: teardown passes; (b) an undeclared fault reported: the helper's result lists it as unexpected; (c) a declared expected fault reported: passes. Spec edge cases: an expected fault declared but never raised (decide and test one behaviour: the helper reports it as missing); the same fault reported twice. Also assert `WITH_STEALTH_TEST_SEAMS` is 1 in this (non-Shipping) build.
- **Sequencing**: stories 007, 009 and 010 place their helpers under this macro and do not list this story as a dependency. Land the define first (it is one line in Build.cs) so they compile.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`: the module the define and header live in
- `production/epics/tuning-data-config/story-006-shipping-symbol-check.md`: proving the define is absent in Shipping (sentinel scan)
- `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`: the clock seam
- `production/epics/tuning-data-config/story-009-log-capture-hook.md`: the log-capture seam
- `production/epics/tuning-data-config/story-010-negative-control-helper.md`: the negative-control helper
- `production/epics/tuning-data-config/story-099-selfcheck-and-disconnect-seams.md`: self-check forcing hook and connection event recorder

---

## Files

- `Source/StealthFoundation/Public/Server/StealthFaultSink.h` (`EStealthFault`, `IStealthFaultSink::Report`)
- `Source/StealthFoundation/Public/Server/Testing/StealthFaultSinkTestSupport.h` (recording sink, `GetUnexpectedFaults`, `EvaluateTeardown` result object; under `WITH_STEALTH_TEST_SEAMS`)
- `Source/StealthFoundation/StealthFoundation.Build.cs` (public definition `WITH_STEALTH_TEST_SEAMS` = 0 in Shipping, 1 otherwise)
- `Source/StealthFoundation/Private/Tests/Harness/FaultSinkTest.cpp` (test `StealthGame.Harness.FaultSinkTeardownAssertion`)
- `Source/StealthPresentation/Public/StealthPresentationModule.h`, `Source/StealthPresentation/Private/StealthPresentationModule.cpp` (story 002's guard-define probe switched from `!UE_BUILD_SHIPPING` to `WITH_STEALTH_TEST_SEAMS`, as story 002 asked)

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: `IStealthFaultSink` exists, `WITH_STEALTH_TEST_SEAMS` is `!UE_BUILD_SHIPPING`, and the teardown helper allows only declared faults
  - Given: `IStealthFaultSink::Report(EStealthFault, FName, int64)` and a teardown-assert helper
  - When: (a) no faults are reported; (b) an undeclared fault is reported; (c) a declared expected fault is reported
  - Then: (a) teardown passes; (b) the helper reports the unexpected fault (returned as a result object so the self-test does not itself fail); (c) passes; `WITH_STEALTH_TEST_SEAMS` is 1 when `!UE_BUILD_SHIPPING`
  - Edge cases: an expected fault declared but never raised; the same fault twice; the Shipping value is proven by story 006

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Harness/FaultSinkTest.cpp` (test `StealthGame.Harness.FaultSinkTeardownAssertion`) must exist and pass (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`StealthGame.Harness.FaultSinkTeardownAssertion`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`
- Unlocks: `production/epics/tuning-data-config/story-006-shipping-symbol-check.md`
- Sequencing note (not a plan dependency): stories 007, 009 and 010 need this story's define to compile their `WITH_STEALTH_TEST_SEAMS` helpers.
- Open verification claims from the plan's Note lines: none (no Note line on 008). The `EStealthFault` member list is open (see Implementation Notes).

---

## Completion Notes
**Completed**: 2026-10-25 (implemented ahead of its route position as the prerequisite of story 006, in the 001-006 build batch)
**Criteria**: 1/1 passing (AC-1: `IStealthFaultSink::Report(EStealthFault, FName, int64)` in `Public/Server/`; `WITH_STEALTH_TEST_SEAMS` public definition, asserted 1 in this build; teardown helper cases (a) none -> clean, (b) undeclared -> listed unexpected with source/detail, (c) declared and raised -> clean; edges: declared-but-never-raised -> missing; same fault twice -> covered by one declaration / two unexpected when undeclared; source-scoped declaration)
**Decisions**: `EStealthFault` members InvariantViolation=1, DeadRegistration=2, RejectedTuning=3, StaleEpoch=4 (0 reserved). One declaration covers any number of matching reports; `Source = NAME_None` matches any source. `GetUnexpectedFaults` lives on the recording sink, not the production interface.
**Deviations**: Also switched story 002 probe guard to `WITH_STEALTH_TEST_SEAMS` (requested by story 002). Manifest Version updated to 2026-10-25f.
**Test Evidence**: `production/qa/evidence/story-008-fault-sink-and-test-seams-define/test-run.txt` (build `Result: Succeeded`; 18/18 `StealthGame.` tests Success) and `stamp.txt`
**Run result**: N/A — test-support interface and a build define; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects found
