# Story 021: Per-commit gate script: T0 lint, build, prefix-filtered automation run

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 8 (gate wiring); traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: The per-commit gate runs the T0 lint, builds `StealthGameEditor` Win64 Development and runs `Automation RunTests ^StealthGame.` (all T1 and T2) within 10 minutes of wall time; a suite that exceeds the budget moves to nightly by amendment, never by disabling. Lints and analysis scripts emit observations only; pass/fail lives in the gate script.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `Automation RunTests` accepts `+`-separated filters and `^`/`StartsWith:` prefix filters [V `AutomationCommandline.cpp:135-227`]; a plain `RunTests StealthGame.` is a substring match [V]; only one `RunTests` request is allowed per `-ExecCmds` (`:618-622`); `Automation RunFilter` selects by flag across all tests including engine tests and is never used. The editor exits 255 both when a test fails and when nothing matched (note in `.github/workflows/tests.yml`), so the verdict is taken from the output lines, not the exit code. No unverified engine claim is introduced; the 10-minute budget is a measurement taken on the dev machine by this story.

**Control Manifest Rules (this layer)**:
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: runs select with `^StealthGame.` or `Group:` — source: ADR-0008
- Forbidden: never select tiers with `Automation RunFilter`; never disable, skip or reseed a failing test to pass a gate — source: ADR-0008
- Guardrail: per-commit pipeline at most 10 minutes — source: ADR-0008

---

## Acceptance Criteria

*From the story plan (bootstrap story, no GDD AC), scoped to this story:*

- [ ] `tools/ci/gate-per-commit` runs the T0 lint, builds StealthGameEditor Win64 Development, runs `Automation RunTests ^StealthGame.` headless, and reports NOT ASSESSED (nonzero) when "No automation tests matched", FAIL on any `Result={Fail}`, and PASS only with at least one `Test Completed` and no failure; the script finishes within the 10 minute per-commit budget on the dev machine.

---

## Implementation Notes

*Derived from ADR-0008 section 8 and Implementation Guidelines, plus the existing repo scripts:*

- **Script.** `tools/ci/gate-per-commit.ps1` (the plan's `TOOLS/` is `tools/ci/`). Stage order: (1) T0 lint through the story 011 gate (`tools/ci/lint/gate.sh`); a lint failure stops the run before the build; (2) build `StealthGameEditor Win64 Development` (a build failure stops the run); (3) run `UnrealEditor-Cmd.exe <uproject> -nullrhi -nosound -ExecCmds="Automation RunTests ^StealthGame.; Quit" -unattended -stdout -FullStdOutLogOutput`; (4) parse the output. Exact exit-code values are not specified (only "NOT ASSESSED nonzero"): choose and document three distinct nonzero/zero codes, and keep the verdict word in the printed summary.
- **Log parsing.** The strings already used in the repo: `No automation tests matched`, `Test Completed`, `Result={Fail}`, and the per-test line `Result={...} Name={...} Path={...}` parsed in `tools/dev/cycle.ps1`. Rules from the spec: no match is NOT ASSESSED; any `Result={Fail}` is FAIL, including a log that also holds passes; PASS needs at least one `Test Completed` and no failure.
- **Build step.** `tests.yml` calls `UnrealBuildTool.exe` directly because `Build.bat` fails from bash on an engine path with spaces; `cycle.ps1` uses `Build.bat ... -WaitMutex` from PowerShell. Honour `UE_ROOT` when set (as `tests.yml` does) and fall back to `C:\Program Files\Epic Games\UE_5.8`.
- **Prototype.** Replaces the filter in `tools/dev/cycle.ps1`: its test run currently uses the substring `StealthGame.` and must become the prefix `^StealthGame.` (`project.yaml` `commands.test` already uses the prefix and is read-only here). The 12 prototype tests (6 `StealthGame.Rules.*`, 6 across `StealthGame.EditorTools.*` specs) run inside the gate unchanged.
- **Compile probes.** ADR-0008 section 8 also lists compile probes per commit when a `Build.cs`, the guard header or any `Public/Server/` header changed, but the plan's AC for this story omits them and puts probes in the nightly (story 023). Leave a documented hook point; whether the per-commit trigger is built here is open.
- **G2 (script story).** Ship (a) a selftest under `tools/ci/selftest/` that feeds canned automation logs (no tests matched; one `Result={Fail}`; one or more `Test Completed` with no failure; a log with both pass and fail) and asserts the verdicts, and (b) the automation wrapper `StealthGame.Build.GatePerCommit` in `Source/StealthFoundation/Private/Tests/Build/` that runs the script's selftest mode through `FPlatformProcess` and asserts exit code and observations. The wrapper must run only the canned-log selftest, never the full gate (the full gate runs automation and would recurse). Locating bash/PowerShell from the wrapper on Windows is open.
- **Budget.** Measure the full run on the dev machine and record the time in this story; over 10 minutes means a documented move of the slowest suite to nightly by ADR amendment, never disabling.
- **No MCP.** The gate must not depend on the MCP toolsets or Python editor scripting.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: the lint emitter and lint gate this script calls
- `production/epics/tuning-data-config/story-022-ci-workflow-t0-lint-step.md`: wiring the same logic into `.github/workflows/tests.yml`
- `production/epics/tuning-data-config/story-023-nightly-workflow.md`: compile probes, T3 suites and the Shipping-symbol check
- `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`: running the lint from a git hook

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/gate-per-commit.ps1`
- `tools/ci/selftest/` (canned-log selftest for the gate)
- `tools/dev/cycle.ps1` (filter `StealthGame.` becomes `^StealthGame.`)
- `Source/StealthFoundation/Private/Tests/Build/GatePerCommitTest.cpp` (G2 wrapper `StealthGame.Build.GatePerCommit`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the gate reports NOT ASSESSED, FAIL or PASS from the automation log and finishes within 10 minutes
  - Given: injected log contents: no tests matched; one `Result={Fail}`; one or more `Test Completed` with no failure
  - When: the gate parses each
  - Then: NOT ASSESSED (nonzero) for "No automation tests matched"; FAIL for any `Result={Fail}`; PASS only with at least one `Test Completed` and no failure; the real run finishes within 10 minutes on the dev machine; the `cycle.ps1` filter becomes `^StealthGame.`
  - Edge cases: lint failure stops before the build; build failure; both a pass and a fail in one log is FAIL

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the gate selftest with canned logs under `tools/ci/selftest/` plus the wrapper `StealthGame.Build.GatePerCommit` (`Source/StealthFoundation/Private/Tests/Build/GatePerCommitTest.cpp`) must exist and pass, and one real run with its measured time recorded

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`, `production/epics/tuning-data-config/story-003-split-prototype-composition-root.md`
- Unlocks: `production/epics/tuning-data-config/story-022-ci-workflow-t0-lint-step.md`, `production/epics/tuning-data-config/story-023-nightly-workflow.md`
- Open verification claims: the plan has no Note line for this story. Open: the 10-minute budget on the dev machine is a measurement not yet taken; the G2 wrapper lives in the Foundation module (story 001), which is only transitively a dependency through story 003.
