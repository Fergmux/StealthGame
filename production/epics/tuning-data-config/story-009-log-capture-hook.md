# Story 009: Log-capture hook asserting warnings by key

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; TDC AC test seam: log-capture hook; governed by ADR-0008 section 4; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Tests observe behaviour through injected seams compiled only under `WITH_STEALTH_TEST_SEAMS`; the TDC acceptance criteria that demand "exactly one warning naming key K with authored A and effective E" (AC-4, AC-26, AC-2a/2c, AC-1) need a log-capture seam, which this story provides.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `FOutputDevice` and `GLog->AddOutputDevice` are pre-cutoff and stable. `UE_LOGF` (5.8) is the logging macro for new code [V `LogMacros.h:199`]; its format-string syntax was not read for this story, so check it before writing the synthetic warnings. A log device receives lines from any thread: guard the captured list with a lock. Tests use `EditorContext | ProductFilter` only (ADR-0008 Verification 7 is open [U]).

**Control Manifest Rules (this layer)**:
- Required: Test-only hooks sit behind `WITH_STEALTH_TEST_SEAMS`; never compile test seams into Shipping (source: ADR-0008)
- Required: New code logs with `UE_LOGF` and a `LogStealth...` category (source: ADR-0007)
- Forbidden: Never use wall-clock waits, unseeded randomness or shared mutable state in tests (source: ADR-0008)
- Required: Every `Public/Server/` header includes `StealthServerOnlyGuard.h` first (source: ADR-0007)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [x] A test-only `FStealthLogCapture` (an FOutputDevice RAII scope under `WITH_STEALTH_TEST_SEAMS`) records `LogStealthFoundation` warnings and lets a test assert "exactly one warning naming key K with authored value A and effective value E" (test `StealthGame.Harness.LogCaptureAssertsWarningByKey`).

---

## Implementation Notes

*Derived from ADR-0008 section 4 (injected seams) and amendment G1:*

- **Header** `Source/StealthFoundation/Public/Server/Testing/StealthLogCapture.h` (amendment G1 replaces the plan's `Private/Tests/Support/StealthLogCapture.h`): guard include first; the whole content under `#if WITH_STEALTH_TEST_SEAMS`. `FStealthLogCapture` derives from `FOutputDevice`, registers itself with the global log in its constructor and removes itself in its destructor (RAII scope), and overrides `Serialize(const TCHAR*, ELogVerbosity::Type, const FName& Category)` to record only lines whose category is `LogStealthFoundation` and whose verbosity is Warning. Capture stops at scope exit; nested scopes both record.
- **Assertion API**: a query such as `CountWarnings(Key, Authored, Effective)` or an `ExpectExactlyOneWarning(...)` that returns a result object (so the self-test can observe the failing cases without failing itself) and passes only for exactly one matching warning.
- **Matching rules** (spec edge cases): key names match as whole tokens, not substrings (a warning naming `NpcCountMax` must not satisfy a query for `NpcCount`); warnings from other categories are ignored. The message format of the real validation warnings, including how authored and effective values are rendered (for example `3` versus `3.0`), is defined by story 046 (the F-1 validation stage); the inputs do not fix it. Match on token occurrences of K, A and E in one line and record the format question for story 046 rather than inventing a format here.
- **Test** `StealthGame.Harness.LogCaptureAssertsWarningByKey`: class `FHarnessLogCaptureAssertsWarningByKeyTest` in `Source/StealthFoundation/Private/Tests/Harness/LogCaptureTest.cpp`, flags `EditorContext | ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. Emit a synthetic warning naming key K with authored A and effective E, then variants: one matching warning passes; zero fails; two fails; a warning naming a different key fails; a non-`LogStealthFoundation` warning is ignored; after the scope ends further warnings are not recorded; nested scopes.
- **Sequencing flag**: `WITH_STEALTH_TEST_SEAMS` is defined by story 008 and is not a listed dependency of this story: land the define first or the header compiles to nothing.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`: the module and `LogStealthFoundation`
- `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`: the `WITH_STEALTH_TEST_SEAMS` define and the fault sink
- `production/epics/tuning-data-config/story-046-f1-validate-clamp-nan.md`: the real validation warnings and their message format
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: first user asserting "no warning" with this capture
- `production/epics/tuning-data-config/story-059-preset-not-selectable.md`, `production/epics/tuning-data-config/story-060-preset-case-insensitive.md`, `production/epics/tuning-data-config/story-061-preset-malformed.md`: preset-option warning assertions

---

## Files

- `Source/StealthFoundation/Public/Server/Testing/StealthLogCapture.h`
- `Source/StealthFoundation/Private/Tests/Harness/LogCaptureTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: `FStealthLogCapture` records `LogStealthFoundation` warnings and asserts exactly one warning naming key K with authored A and effective E
  - Given: an `FStealthLogCapture` RAII scope on `LogStealthFoundation`
  - When: a synthetic warning naming key K with authored A and effective E is emitted, then variants
  - Then: the assertion "exactly one warning naming K, A, E" passes for one; fails for zero, for two, and for a warning naming a different key; capture stops at scope exit
  - Edge cases: key-name substring collisions (match the whole key); non-`LogStealthFoundation` warnings are ignored; nested scopes

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Harness/LogCaptureTest.cpp` (test `StealthGame.Harness.LogCaptureAssertsWarningByKey`) must exist and pass (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`StealthGame.Harness.LogCaptureAssertsWarningByKey`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md` (added after review: the `WITH_STEALTH_TEST_SEAMS` define)
- Unlocks: `production/epics/tuning-data-config/story-046-f1-validate-clamp-nan.md`, `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-059-preset-not-selectable.md`, `production/epics/tuning-data-config/story-060-preset-case-insensitive.md`, `production/epics/tuning-data-config/story-061-preset-malformed.md`
- Sequencing note (not a plan dependency): needs the `WITH_STEALTH_TEST_SEAMS` define of `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`.
- Open verification claims from the plan's Note lines: none (no Note line on 009). The rendered format of authored and effective values in warnings is open (story 046).

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: one matching warning passes; zero fails; two fails; a different key fails; `NpcCountMax` does not satisfy `NpcCount` (whole-token keys) and values are whole tokens too (`13` is not `3`, `2.5` is not `2`); a non-`LogStealthFoundation` warning and a `Log`-verbosity line are ignored; nothing is recorded after `Stop()` or scope exit, and a capture opened later does not see earlier warnings; nested scopes both record, the inner one only while open; fault teardown clean)
**Decisions**: `FStealthLogCapture` registers with `GLog` as an unbuffered, lock-guarded device (`CanBeUsedOnMultipleThreads`), armed only after `AddOutputDevice` returns so a backlog replay is never recorded. Query API `ExpectExactlyOneWarning(K, A, E)` returns `FStealthLogMatchResult` (`Passed()`, `Matches`, `Describe()`), plus `CountWarnings`/`FindWarnings`/`ContainsToken`. Token rule: neighbours must not be letters, digits or `_`; a trailing `.` is a boundary unless a digit follows; a numeric token preceded by `-` is a different token.
**Finding for later stories (058-061, 046)**: a warning declared with `AddExpectedMessage` is demoted to `Verbose` by the automation framework's `GWarn` filter before any `GLog` device sees it (`AutomationTest.cpp` `FAutomationTestMessageFilter::Serialize`), so a capture can never record it. Observed: the first run of this test failed with nothing captured. Tests that use the capture must therefore NOT declare expected messages; they derive from `FStealthLogCaptureTestBase` (in the same header; `SuppressLogWarnings()` true) via `IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST`, and the capture's assertion is the check. Log errors still fail the test.
**Open for story 046**: how authored/effective values are rendered in the real validation warnings (`3` vs `3.0`); the capture matches tokens literally.
**Deviations**: Added `FStealthLogCaptureTestBase` to the story's header (needed, see finding). Synthetic warnings use `UE_LOGF` with `%ls` for `TCHAR*` (5.8: `%s` is a UTF-8 string in `UE_LOGF`).
**Test Evidence**: `production/qa/evidence/story-009-log-capture-hook/test-run.txt` (build `Result: Succeeded`; 22/22 `StealthGame.` tests Success; watched-to-fail note) and `stamp.txt`
**Run result**: N/A — test-seam helper; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects found
