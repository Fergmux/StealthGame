# Story 018: Harness replication-system assertion and result JSON

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 5 and Key Interfaces; traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Every T2/T3 run reads which replication system the game NetDriver uses, compares it with the pinned expectation (legacy, per ADR-0001), fails on mismatch and writes the active system into a result JSON, so no leak or parity result is trusted under the wrong driver.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `UNetDriver::IsUsingIrisReplication()` is post-cutoff and was checked in source [V `Engine/NetDriver.h:2014`]. The Iris-versus-legacy mismatch path can only be reached for real through story 020 (Gauntlet, `-UseIrisReplication=1`); this story proves the comparison logic through an injectable probe, so nothing here is an unverified engine claim. The harness module is a `Runtime` module with `TargetConfigurationDenyList` for Shipping and Test (story 004), so this code never ships.

**Control Manifest Rules (this layer)**:
- Required: T2/T3 tests are bounded by frames or server time and assert and record the active replication system — source: ADR-0008
- Required: the game NetDriver runs the legacy replication system and the harness asserts it at startup — source: ADR-0001
- Forbidden: never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) — source: ADR-0008
- Guardrail: per-commit pipeline at most 10 minutes — source: ADR-0008

---

## Acceptance Criteria

*From the story plan and amendments (bootstrap story, no GDD AC), scoped to this story:*

- [ ] The harness reads the game driver's replication system through an injectable `IStealthReplicationSystemProbe` (production implementation: `UNetDriver::IsUsingIrisReplication()`), fails the run when it differs from the pinned expectation (legacy) -- proven by a fake probe reporting Iris causing failure and a fake reporting legacy passing -- and writes `{scenario, build, replicationSystem, netProfile, passed, negativeControlDetected, faults[], captures[]}` to `-StealthHarnessOut=<path>` (test `StealthGame.Harness.ReplicationSystemAsserted`).

---

## Implementation Notes

*Derived from ADR-0008 section 5 (replication-system assertion), Key Interfaces and Implementation Guidelines:*

- **Seam.** Declare `IStealthReplicationSystemProbe` (one query returning legacy or Iris) in `StealthHarnessScenarioRunner.h`. The production implementation reads `UNetDriver::IsUsingIrisReplication()` for the game net driver. The runner takes the probe through its constructor so tests never need a live world. The plan lists no separate header for the probe; if the developer adds one, `/dev-story` records it in Files.
- **Pinned expectation.** Legacy, from ADR-0001 (never Iris now; deferred to ADR-0024). Keep it a single named constant in the runner so an Iris switch later is one edit; ADR-0008 says the Iris rerun gate (AC-IVP-29) activates automatically if the expectation is switched.
- **Run flow.** On harness start (and in every non-Shipping game process the harness module loads into): read the probe, compare, set `passed` false on mismatch, and always write the active system into `replicationSystem`. The mismatch fails the run; it is not a warning.
- **Result JSON.** `FStealthHarnessResult` (in `StealthHarnessResult.h`) serialises exactly `{scenario, build, replicationSystem, netProfile, passed, negativeControlDetected, faults[], captures[]}` to the path given by `-StealthHarnessOut=<path>` (and the scenario name from `-StealthHarnessScenario=<Name>`, the ADR-0008 harness entry). The value sets for `build` and `netProfile` are not defined in the inputs: open. A proposal for `/dev-story` to confirm: `build` = the build configuration string, `netProfile` = the `-PktEmulationProfile` name or `none`. The JSON writer needs the `Json` module; add it to the harness module's private dependencies if absent (Foundation already lists it privately).
- **Edge cases the spec names.** `-StealthHarnessOut` absent, and an unwritable path. The expected behaviour of each is not stated in the inputs: open. Proposal: absent means no file is written, the run result is still evaluated and logged; unwritable means the run fails (a result that cannot be recorded is a failed run). Record the decision in the test.
- **faults[].** The spec requires `faults[]` to be filled from the fault sink. `IStealthFaultSink` is story 008, which is not a declared dependency of this story (deps are 004 and 007): either add the dependency through `/dev-story` or let the test feed `faults[]` from a test-local list until 008 lands. Open.
- **Negative control.** The fake probe reporting Iris is the negative control: the suite passes only if the mismatch run is detected as failed. `negativeControlDetected` is written from that outcome. The shared `FStealthNegativeControl` helper is story 010 (not a declared dependency); use it if present, otherwise a local check.
- **Logging.** `UE_LOGF` with the harness module's own `LogStealth...` category (manifest: new code logs with `UE_LOGF` and a `LogStealth...` category; the category name is open, `LogStealthHarness` suggested). `LogStealthFoundation` is Foundation's category and is not used here.
- **Test placement and naming (ADR-0008 section 2, `.claude/rules/test-standards.md`).** Class `FHarnessReplicationSystemAssertedTest`, name `StealthGame.Harness.ReplicationSystemAsserted`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file inside `#if WITH_DEV_AUTOMATION_TESTS`. The plan puts it in `Source/StealthTestHarness/Private/Tests/`. No wall-clock predicate anywhere.
- **Shared support (G1).** Any fake that other stories reuse goes under `Source/StealthFoundation/Public/Server/Testing/Stealth<X>.h` (server guard first, content under `WITH_STEALTH_TEST_SEAMS`); a fake used only here stays in the test file.
- **Prototype.** None: no prototype code is replaced or promoted. The 12 prototype tests are not harness tests and keep running unchanged.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: the CQTest listen-server proof under `-nullrhi`
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the real Iris-versus-legacy mismatch through Gauntlet and the scenario that calls this runner in a cooked build
- `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`: creating the harness module and its Shipping exclusion
- `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`: `IStealthFaultSink` itself
- `production/epics/tuning-data-config/story-017-net-emulation-profiles.md`: the packet-simulation profiles whose name goes into `netProfile`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/StealthHarnessScenarioRunner.h`, `Source/StealthTestHarness/Private/StealthHarnessScenarioRunner.cpp` (probe seam, production probe, fixed fake, runner with fault sink, negative control, result writer)
- `Source/StealthTestHarness/Private/StealthHarnessResult.h` (result record and JSON)
- `Source/StealthTestHarness/Private/Tests/ReplicationSystemAssertedTest.cpp`
- `Source/StealthTestHarness/Private/Scenarios/StealthHarnessRun.h`, `Source/StealthTestHarness/Private/Scenarios/StealthHarnessRun.cpp` (the T3 process runner asserts at boot, writes the result at the end, exits 0/1)
- `Source/StealthTestHarness/StealthTestHarness.Build.cs` (added the `Json` module)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the harness reads the replication system through `IStealthReplicationSystemProbe`, fails on a mismatch with the legacy expectation and writes the result JSON (test `StealthGame.Harness.ReplicationSystemAsserted`)
  - Given: `IStealthReplicationSystemProbe` (production: `UNetDriver::IsUsingIrisReplication()`), pinned expectation legacy
  - When: a fake probe reports Iris; a fake probe reports legacy; the run result is written to `-StealthHarnessOut=<temp path>`
  - Then: the Iris run fails; the legacy run passes; the JSON holds `scenario`, `build`, `replicationSystem`, `netProfile`, `passed`, `negativeControlDetected`, `faults[]`, `captures[]`
  - Edge cases: output path absent or unwritable; `faults[]` filled from the fault sink

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthTestHarness/Private/Tests/ReplicationSystemAssertedTest.cpp` must exist and pass (`StealthGame.Harness.ReplicationSystemAsserted`)

**Status**: [x] Created and passing (`Source/StealthTestHarness/Private/Tests/ReplicationSystemAssertedTest.cpp`, `StealthGame.Harness.ReplicationSystemAsserted`; plus real two-process runs, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`, `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`
- Unlocks: `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Open verification claims: the plan has no Note line for this story. Open items found while writing: the `build` and `netProfile` value sets, the absent/unwritable output path behaviour, and the missing declared dependency on story 008 for `faults[]`.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: `IStealthReplicationSystemProbe` with the production `FStealthNetDriverReplicationProbe` (`UNetDriver::IsUsingIrisReplication()`; the class default reads legacy; no driver reads Unknown); a fake reporting Iris fails the run (the suite passes only because that failure is detected, via `FStealthNegativeControl`), a fake reporting legacy passes, Unknown fails; the result JSON has exactly the eight keys `scenario, build, replicationSystem, netProfile, passed, negativeControlDetected, faults[], captures[]` read back from `-StealthHarnessOut`; faults[] is filled through the `IStealthFaultSink` face and a fault fails the run; an absent path writes no file and still evaluates; an unwritable path returns Failed and fails the run; a run that never asserted is not a pass. Real processes: `run-mp.ps1 -Scenario Smoke` host + client each wrote `replicationSystem: legacy, passed: true, negativeControlDetected: true` (evidence `live-run/`); a process forced to report Iris wrote `iris`, `passed: false` and exited with status 1 (`RequestExitWithStatus(0, 1, StealthHarness)`))
**Decisions**: named `EHarnessReplicationSystem` (the engine already has a global `EReplicationSystem`); the probe returns Legacy, Iris or Unknown and Unknown never reads as legacy; `build` = the build configuration string (`LexToString(FApp::GetBuildConfiguration())`), `netProfile` = the `-PktEmulationProfile` name or `none`; `-StealthHarnessOut` is the result FILE when it ends in `.json`, else a FOLDER (the interim launcher run-mp.ps1 already passes a folder) and the file is `server-result.json` / `client<N>-result.json`; absent path = no file, still evaluated and logged; unwritable path = failed run; `faults[]` comes from the runner being the process's `IStealthFaultSink` (story 008 is in the repo) and any fault fails the run; the negative control uses `FStealthNegativeControl` (story 010 is in the repo): every run evaluates the comparison against a probe reporting the OTHER system and records `negativeControlDetected`; a control that is not detected fails the run; expected failures log at Warning, not Error, so an automation test exercising them does not fail on log errors; the assertion runs at the first boot record of every harness process (`-StealthHarnessScenario=`), not in processes without a scenario
**Deviations**: `StealthHarnessScenarioRunner.h/.cpp` live in `Private/` (as the story lists) and the process runner is the existing `Scenarios/StealthHarnessRun.*` from the earlier spikes, extended rather than replaced
**Test Evidence**: `production/qa/evidence/story-018-replication-system-assertion/test-run.txt` (build `Result: Succeeded`; 37/37 `StealthGame.` tests Success), `live-run/` (two-process result JSON and records), `stamp.txt`
**Run result**: OBSERVED — two real processes (listen host + client, Development editor binary `-game -nullrhi`) via tools/ci/mp/run-mp.ps1: `production/qa/evidence/story-018-replication-system-assertion/live-run/` (no screenshot: nothing player-visible; the observation is the result JSON and the boot records, including `maxPlayers: 10` from decision D51)
**Reviewed by**: focused code review (self-applied, unattended run): the Error-level log of an expected failure (it failed the automation test) was found and changed to Warning; no further defects found
