# Story 006: Shipping-symbol check: no harness module and no test-seam symbols in Shipping

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 sections 4 and 7-8; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Test-only hooks are wrapped in `WITH_STEALTH_TEST_SEAMS` and the harness module is excluded from Shipping, and a Shipping-symbol check in the nightly job proves that a Shipping build contains no harness module and no seam symbols.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `WITH_STEALTH_TEST_SEAMS` is a preprocessor macro and leaves no symbol in a binary, so the check scans for a unique sentinel string literal compiled only under the macro (the same technique story 088 uses for the tuning back doors). Observing the harness module's exclusion from a cooked build is ADR-0007 Verification Required item (2) and is still [U]; this story's nightly run is where it is first seen for Shipping. ADR-0008 Verification Required item (7) (whether `WITH_DEV_AUTOMATION_TESTS` is defined in cooked Development game builds) is not touched. The binary file-name pattern of the harness module in a packaged build is not stated by the inputs: record it when first observed.

**Control Manifest Rules (this layer)**:
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) (source: ADR-0008)
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects (source: ADR-0008; here the Development-build positive control)
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script (source: ADR-0008)
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), as revised in `tdc-plan-amendments.md`:*

- [ ] `WITH_STEALTH_TEST_SEAMS` code carries a unique sentinel string literal (`STEALTH_TEST_SEAM_SENTINEL_*`, referenced once at registration so it is not stripped), and the shipping-symbol check finds no StealthTestHarness module binary and no seam sentinel (ASCII and UTF-16LE) in a Shipping build, while the same scan finds both in a Development build (positive control). Deferred to a packaged build; the check script is runnable in the nightly job.

---

## Implementation Notes

*Derived from ADR-0008 sections 4, 7 and the Validation Criteria (item 6); ADR-0007 section 1:*

- **Sentinel**: add a header `Source/StealthFoundation/Public/Server/Testing/StealthTestSeams.h` (server-only: guard include first; the whole content under `#if WITH_STEALTH_TEST_SEAMS`) declaring the sentinel and one registration function that references the literal `STEALTH_TEST_SEAM_SENTINEL_<unique suffix>` exactly once in a way the optimiser cannot remove (it must survive a Development build; "sentinel optimised out" is a spec edge case). Call the registration from code that exists in every non-Shipping build, for example the Foundation module startup; it registers nothing else. Flag: the plan lists this header as `FND/Public/StealthTestSeams.h`, which would sit directly in `Public/` and violate ADR-0007's header-placement rule (a); it goes under `Public/Server/Testing/` instead, consistent with amendment G1 (shared test support under `Public/Server/Testing/`).
- **Define**: `WITH_STEALTH_TEST_SEAMS` itself is defined by story 008 (`StealthFoundation.Build.cs`, public definition, `!UE_BUILD_SHIPPING`). This story depends on 008 for that reason.
- **Script** `tools/ci/shipping-symbol-check.sh`: takes the packaged Shipping directory and the packaged Development directory as arguments. In the Development build it must find both the harness module binary and the sentinel (the positive control); in the Shipping build it must find neither. Scan the sentinel as ASCII and as UTF-16LE (a `TEXT()` literal is wide on Windows). The script emits observations (found or not found, per build and encoding); the verdict is decided in one gate step: FAIL when the Shipping scan finds anything or when the Development control finds nothing; NOT ASSESSED (nonzero, distinguishable text) when a build directory is missing or empty. Follow the story 011 emitter/gate conventions when available (this story does not depend on 011, keep the contract simple).
- **G2**: ship a selftest under `tools/ci/selftest/` that builds small synthetic files at run time (one containing the sentinel as ASCII, one as UTF-16LE, one clean, and a stand-in module-named file) instead of committing binaries (ADR-0009: no binaries in plain Git), proving a clean pass, a seeded violation detected, the control-finds-nothing failure and the missing-directory NOT ASSESSED; and the wrapper automation test `StealthGame.Build.ShippingSymbolCheck` (class `FBuildShippingSymbolCheckTest`, `Source/StealthFoundation/Private/Tests/Build/ShippingSymbolCheckTest.cpp`; proposed name) running it through `FPlatformProcess::ExecProcess`. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- **Deferred part**: the scan of real Shipping and Development packages needs a packaged build of each and runs in the nightly job (story 023). Record the first real observation (including the harness binary name pattern) in the story when it exists. This story does not close the cooked-Development load observation of the deny list; story 020 does.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`: the harness module, its deny list and the static exclusion checks
- `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`: the `WITH_STEALTH_TEST_SEAMS` definition
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: launching a cooked Development build and observing the harness loaded
- `production/epics/tuning-data-config/story-023-nightly-workflow.md`: wiring this check into the nightly job
- `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md` (and 089): the analogous sentinel scan for the tuning back doors

---

## Files

- `tools/ci/shipping-symbol-check.sh` (observations per build/check + one gate step; 0 PASS / 1 FAIL / 2 NOT ASSESSED)
- `tools/ci/selftest/shipping-symbol-check/run-selftest.sh` (synthetic packages built at run time, 10 cases)
- `Source/StealthFoundation/Public/Server/Testing/StealthTestSeams.h` (sentinel registration API, under `WITH_STEALTH_TEST_SEAMS`; replaces the plan's `Public/StealthTestSeams.h`)
- `Source/StealthFoundation/Private/Testing/StealthTestSeams.cpp` (sentinel literals, ASCII + UTF-16, stored in volatile globals)
- `Source/StealthFoundation/Private/StealthFoundationModule.cpp` (StartupModule calls the registration under the macro)
- `Source/StealthFoundation/Private/Tests/Build/ShippingSymbolCheckTest.cpp` (test `StealthGame.Build.ShippingSymbolCheck`)

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: the shipping-symbol check finds no harness binary and no seam sentinel in Shipping, and finds both in Development
  - Given: code under `WITH_STEALTH_TEST_SEAMS` carrying a unique `STEALTH_TEST_SEAM_SENTINEL_*` literal referenced once at registration
  - When: the scan runs on a Development build and on a Shipping build
  - Then: Development: the harness module binary and the seam sentinel are both found (ASCII and UTF-16LE); Shipping: neither is found; the check fails when the Development control finds nothing
  - Edge cases: the sentinel optimised out; missing build output is NOT ASSESSED; the script is runnable in the nightly job

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the selftest under `tools/ci/selftest/shipping-symbol-check/` plus the wrapper `StealthGame.Build.ShippingSymbolCheck` (`Source/StealthFoundation/Private/Tests/Build/ShippingSymbolCheckTest.cpp`) must exist and pass per commit (canned inputs, G2); the real Shipping and Development package scan is DEFERRED to a packaged build and the nightly job, with its observation recorded in the story (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`StealthGame.Build.ShippingSymbolCheck` + selftest, 2026-10-25); packaged-build scan DEFERRED to the nightly job (story 023)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`, `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`
- Unlocks: `production/epics/tuning-data-config/story-023-nightly-workflow.md`
- Open verification claims from the plan's Note lines: none (no Note line on 006). Related [U] items from the ADRs: cooked honouring of the harness deny list (ADR-0007 Verification 2) and the harness binary name pattern in packaged builds.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing at the per-commit level (G2), packaged part DEFERRED as the criterion states. The sentinel `STEALTH_TEST_SEAM_SENTINEL_7F3A9C2E` is stored once as UTF-16 (`TEXT`) and once as ASCII in volatile globals by `StealthRegisterTestSeamSentinel()`, called from StealthFoundation startup under `WITH_STEALTH_TEST_SEAMS`; the test asserts it was registered. `shipping-symbol-check.sh` selftest: clean pass (x2), seeded Shipping sentinel ASCII / UTF-16LE / harness file / harness name -> FAIL, blind control (nothing / no sentinel) -> FAIL, missing or empty dir -> NOT ASSESSED; all 10 as expected.
**Decisions**: monolithic packages have no separate harness binary, so the harness counts as present when a file is named `*StealthTestHarness*` OR the module name appears inside a binary (IMPLEMENT_MODULE registers it); only binaries (*.exe/*.dll/*.so/*.dylib) are scanned. UTF-16LE detection strips NUL bytes and counts the extra matches (Git Bash `grep -P` rejected the C locale). Development control passes when the harness is found and the sentinel is found in at least one encoding.
**Partial real observation**: run on the editor Development DLLs (`UnrealEditor-Stealth*.dll`) vs a clean stand-in: harness file `UnrealEditor-StealthTestHarness.dll` and both sentinel encodings found, gate PASS (see test-run.txt). Still to record from the nightly packaged scan: the packaged harness binary/name pattern and the Shipping result (ADR-0007 Verification 2 for Shipping).
**Deviations**: Story 008 (its dependency, outside the 001-006 batch) was implemented first in this run. Sentinel header placed under `Public/Server/Testing/` as the story directs; a Private cpp was added for the definitions. Foundation startup now makes one call (the sentinel), an explicit exception to "StartupModule registers nothing", as this story asks. Manifest Version updated to 2026-10-25f.
**Test Evidence**: `production/qa/evidence/story-006-shipping-symbol-check/test-run.txt` (build `Result: Succeeded`; 19/19 `StealthGame.` tests Success; partial real observation) and `stamp.txt`
**Run result**: N/A — CI tooling and a build-time sentinel; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no defects found
