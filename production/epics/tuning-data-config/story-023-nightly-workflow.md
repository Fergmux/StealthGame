# Story 023: Nightly workflow: compile probes, T3 suites, Shipping-symbol check

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 8 (nightly gate); traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: The nightly gate runs everything per-commit plus the compile probes, the T3 multi-process leak suites under the leak and clean net profiles and the Shipping-symbol check, and blocks any build leaving the developer's machine the next day.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Touches several unverified items through its components: Gauntlet topology on one machine (Verification 3) and `WITH_DEV_AUTOMATION_TESTS` in cooked Development (Verification 7), both [U] and settled by story 020; `-PktEmulationProfile=<Name>` is the verified command-line switch for named profiles [V `NetEmulationHelper.cpp:384-460, 534-570`], with the profile's lag semantics [U] recorded by story 017; UBT `-SingleFile=` for the probes [V `BuildMode.cs:880-943`] is story 005. The nightly cook plus T3 time is estimated at 1-2 hours [U]. A self-hosted runner and cooked builds are required, so a real run is DEFERRED.

**Control Manifest Rules (this layer)**:
- Required: every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Forbidden: never prove multi-process criteria in in-process PIE only — source: ADR-0008
- Guardrail: nightly cook plus T3 estimated at 1-2 hours [U] — source: ADR-0008

---

## Acceptance Criteria

*From the story plan (bootstrap story, no GDD AC), scoped to this story:*

- [ ] `.github/workflows/nightly.yml` (self-hosted) runs everything per-commit plus the compile probes, the T3 multi-process suites under `StealthLeak2` and `StealthClean` profiles, and the Shipping-symbol check, uploading logs, and fails the job on any observation the gate script marks as a failure.

---

## Implementation Notes

*Derived from ADR-0008 section 8 (Gate wiring), section 5 (net profiles) and section 7 (probes and Shipping-symbol check):*

- **Files.** `.github/workflows/nightly.yml` (a daily schedule trigger on the same self-hosted Windows runner as `tests.yml`) and `tools/ci/nightly.ps1`, which sequences the components and decides the verdict.
- **Components, in order.** (1) the story 021 per-commit gate; (2) the compile probes: story 005's runner on a disposable git worktree copy (never the developer tree) and its gate script; (3) the story 020 T3 smoke node, once under `StealthLeak2` and once under `StealthClean` (`-PktEmulationProfile=<Name>`, the profile name recorded in the log); (4) the story 006 Shipping-symbol check on a packaged Shipping build. Other nightly items in ADR-0008 (Generator 50-seed batch audits, transport-classifier data capture) belong to later epics and are not wired here.
- **Verdicts.** A component that did not run is NOT ASSESSED, never a pass. The job fails on any observation the gate marks as a failure. Lint-style scripts only emit observations; the decision stays in `nightly.ps1` or the shared gate.
- **Logs.** Upload logs as the existing `tests.yml` does (`actions/upload-artifact`, `Saved/Logs/` plus the captured output files), including on failure.
- **G2 (script story).** Ship (a) a dry-run selftest of `nightly.ps1` under `tools/ci/selftest/` in which each component is replaced by a stub that can be told to emit a failing observation, and (b) the automation wrapper `StealthGame.Build.NightlyDryRun` in `Source/StealthFoundation/Private/Tests/Build/` running that dry run through `FPlatformProcess`. The dry run must prove: a seeded failing observation from one component fails the job, all four component groups ran, logs are uploaded (or collected), a clean run passes. A dry-run switch name is open.
- **Deferred.** The real nightly needs the self-hosted runner (user action) and cooked builds. The story can be Done on the dry run plus file review only if the qa-lead's DEFERRED note is honoured: record the real-run observation when it exists.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`: the probe runner and gate
- `production/epics/tuning-data-config/story-006-shipping-symbol-check.md`: the Shipping-symbol check script
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the T3 node being run
- `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`: the per-commit gate being included
- `production/epics/tuning-data-config/story-017-net-emulation-profiles.md`: defining the `StealthLeak2` and `StealthClean` profiles
- `production/epics/tuning-data-config/story-089-no-shipping-back-door-scan.md`: the Shipping back-door scan that later joins the nightly

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `.github/workflows/nightly.yml`
- `tools/ci/nightly.ps1` (sequencing, verdicts, `-DryRun`, `-Only`, `-SelfTest`)
- `tools/ci/selftest/nightly/stub-component.cmd` (stub component for the dry run)
- `Source/StealthFoundation/Private/Tests/Build/NightlyDryRunTest.cpp` (G2 wrapper `StealthGame.Build.NightlyDryRun`), `Source/StealthFoundation/Private/Tests/Build/CiWorkflowsTest.cpp` (workflow file text, shared with story 022)
- `production/qa/smoke-2026-10-25_ci-workflows.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the nightly job runs the per-commit gate, compile probes, T3 suites under both profiles and the Shipping-symbol check, uploads logs and fails on a failing observation
  - Given: the nightly workflow with the per-commit gate, compile probes, T3 suites under `StealthLeak2` and `StealthClean`, the Shipping-symbol check and log upload
  - When: it is run with a seeded failing observation from one component
  - Then: the job fails on any observation the gate marks as a failure; all four component groups ran; logs are uploaded; a clean run passes
  - Edge cases: a component not run is NOT ASSESSED, not a pass; the profile applied to each T3 run is recorded in the log

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the dry-run selftest under `tools/ci/selftest/` plus the wrapper `StealthGame.Build.NightlyDryRun` must exist and pass; the real nightly run is DEFERRED (self-hosted runner, cooked builds)

**Status**: [x] Created and passing (`nightly.ps1 -SelfTest` 15/15 cases; `StealthGame.Build.NightlyDryRun`; real T3 component run under both profiles; the full real nightly is DEFERRED)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`, `production/epics/tuning-data-config/story-006-shipping-symbol-check.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`, `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`
- Unlocks: `production/epics/tuning-data-config/story-089-no-shipping-back-door-scan.md`
- Open verification claims: the plan has no Note line for this story. Inherited [U] items: Gauntlet topology and `WITH_DEV_AUTOMATION_TESTS` in cooked Development (story 020), profile lag semantics (story 017), nightly duration estimate of 1-2 hours. Open: the nightly uses the `StealthLeak2` and `StealthClean` profiles defined by story 017, which is not a declared dependency.

---

## Completion Notes
**Completed**: 2026-10-25 (the full real nightly DEFERRED: self-hosted runner and cooked Shipping/Development packages)
**Criteria**: 1/1 passing on the dry run plus file review, real run deferred (AC-1: `nightly.yml` is self-hosted, scheduled and runs `tools/ci/nightly.ps1`, which runs the per-commit gate, the compile probes, the T3 smoke under `StealthLeak2` and `StealthClean` and the Shipping-symbol check, uploads logs with `if: always()` and fails on FAIL and on NOT ASSESSED. Dry run (`-DryRun`, each component a stub): a clean run passes with `GROUPS RAN: gate probes t3 shipping-symbol (4 of 4)` and a log per component plus `summary.txt`; a seeded failing observation from EACH of gate, probes, either T3 run and the Shipping check exits 1 while the other four still run; a component that is NOT ASSESSED, has no verdict (did not run) or crashes (exit 7) gives exit 2, never a pass; FAIL wins over NOT ASSESSED; `-Only` can never pass; each T3 run prints `T3 profile=<name>` and writes `PROFILE: <name>` into its log. Real component observed: the T3 node ran for real under `StealthLeak2` and `StealthClean` on this machine (both `result=Passed`, `BUILD SUCCESSFUL`, the profile on both processes' command lines))
**Decisions**: dry-run switch = `-DryRun` with `-StubVerdicts "all=pass,<id>=fail|na|crash"` (stub = `tools/ci/selftest/nightly/stub-component.cmd`, exit 0/1/2/7), so the real exit-code mapping and logging are exercised; component exit convention 0 PASS / 1 FAIL / 2 NOT ASSESSED, any other code is NOT ASSESSED; all components always run, the verdict is decided at the end (FAIL beats NOT ASSESSED beats PASS); the T3 profile reaches both processes through the node's `-StealthRoleArgs=-PktEmulationProfile=<Name>` (ADR-0002's "symmetric" meaning); the T3 verdict needs `StealthGame.Harness.MP.Smoke ... result=Passed` (a run with no such line is NOT ASSESSED, `result=Failed` or `BUILD FAILED` is FAIL); Development and Shipping are staged with `BuildCookRun -pak -IgnoreCookErrors` (as story 020) into `Saved/Staged/NightlyDev` and `NightlyShipping`; relative `-LogDir` and stage paths are resolved against the repo root (RunUAT cannot resolve them: found by the first real T3 run); story 017 is not a declared dependency but the profiles exist in `Config/DefaultEngine.ini` and the real run applied them
**Deferred (record when it exists)**: one real full nightly (workflow_dispatch) on the runner: elapsed time vs the 1-2 hour estimate, the packaged Shipping/Development harness binary name pattern and Shipping result for story 006, and the compile-probe component inside the nightly
**Deviations**: none
**Test Evidence**: `production/qa/smoke-2026-10-25_ci-workflows.md`, `production/qa/evidence/story-023-nightly-workflow/` (`selftest.txt` 15/15, `real-t3-both-profiles-run.txt`, `verdict-mapping.txt`, `test-run.txt` 43/43 Success, `stamp.txt`)
**Run result**: OBSERVED — the T3 component was run for real (two cooked Development processes per profile, Gauntlet); nothing player-visible, the observation is the node verdict, not a screenshot
**Reviewed by**: focused code review (self-applied, unattended run): the relative-path defect above and a function-output pollution (a bare string returned from a component function became part of its return value) were found and fixed; no further defects found
