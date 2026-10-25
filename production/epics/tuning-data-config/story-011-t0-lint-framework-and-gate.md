# Story 011: T0 lint framework: observation emitter and gate script

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 sections 1 and 8; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Tier T0 (static) is a set of scripts under `tools/ci/lint/` that emit observations (`file:line` findings) with no engine and in seconds, while a separate gate script is the only place that decides pass or fail, following the `.claude/scripts` rule that helpers emit observations and never verdicts.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The lint itself uses no engine API and runs without the engine. The G2 wrapper test uses `FPlatformProcess` (pre-cutoff) and `EAutomationTestFlags` (enum class, application-context flag mandatory). Which `bash` the wrapper launches on the Windows dev machine and the self-hosted runner is not stated by the inputs (the CI workflow uses `shell: bash`); resolve it when writing the wrapper.

**Control Manifest Rules (this layer)**:
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script (source: ADR-0008)
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects (source: ADR-0008)
- Required: The include lint (rules a-j) runs in CI and pre-commit (source: ADR-0007; this story is its framework)
- Forbidden: Never make a gate depend on the MCP toolsets or Python editor scripting (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [x] `tools/ci/lint/` has a shared emitter that prints one `file:line rule message` observation per finding and a gate script that is the only place pass/fail is decided (exit 0 with no findings, nonzero with any, NOT ASSESSED nonzero when no input files were scanned); a self-test over a two-file fixture proves each outcome.

---

## Implementation Notes

*Derived from ADR-0008 sections 1 and 8, the `.claude/scripts` observation rule, and amendment G2:*

- **Files** (TOOLS means `tools/ci`): `tools/ci/lint/emit.sh` (the plan allows `.py`; prefer shell, since the repo scripts and the CI steps already use bash, and Python is allowed only as a build-time tool, never as a gate), `tools/ci/lint/gate.sh`, and `tools/ci/lint/selftest/*`.
- **Emitter**: a small shared library that rule scripts source or call to print exactly one line per finding in the form `file:line rule message`, with repo-relative forward-slash paths. It never exits nonzero because of a finding and never prints a verdict.
- **Gate**: the only decider. Exit 0 when files were scanned and there are no findings; nonzero with any finding (print the findings and a FAIL line); nonzero with a distinct NOT ASSESSED line when zero input files were scanned, so an empty scope is never a pass. The two nonzero outcomes must be distinguishable by text (and preferably by exit code).
- **Files-scanned accounting**: how the gate learns the number of files scanned is not specified by the inputs. The emitter has to expose it (a suggested contract: each rule script prints one non-finding `scanned` line the gate counts). Decide it here, record it in the selftest, and keep it stable: stories 012-015, 024, 025, 029 and 092 build on it.
- **Reusable pieces to anticipate** (do not build what is not needed here): repo-root argument handling (story 016 points the rule scripts at a fixture tree), a file lister for the G5 scan scope (`Source/StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthGameEditor`, `StealthTestHarness`; the prototype `Source/StealthGame` only for the cross-module rules), and the G3 shrink-only baseline comparison (working-tree file against its content at `HEAD` from the pre-commit hook, or at the push's before-SHA, else `HEAD~1`, in CI; first-commit creation exempt). Where the G3 comparison lives is open; story 012 is its first user.
- **Selftest** under `tools/ci/lint/selftest/`: a two-file fixture (one clean file, one with a deliberate finding) plus an empty input set. Outcomes proved: (a) clean file only: gate exit 0; (b) fixture with one finding: nonzero and the observation line printed; (c) zero files: nonzero with the NOT ASSESSED text, distinguishable from FAIL. Spec edge cases: the emitter alone never returns a verdict; two findings produce two lines.
- **G2 wrapper**: automation test `StealthGame.Build.LintFramework` (class `FBuildLintFrameworkTest`, `Source/StealthFoundation/Private/Tests/Build/LintFrameworkTest.cpp`; name proposed, the plan fixes only the `StealthGame.Build.<Name>` pattern) that runs the selftest through `FPlatformProcess::ExecProcess` and asserts the exit code and the observation lines. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. The wrapper lives in StealthFoundation, so it builds only after story 001; this story has no declared dependency on 001, so write the scripts first and add the wrapper once the module exists.
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`: rules (a)-(c)
- `production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md`: rules (d)-(f)
- `production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`: rules (g)-(h)
- `production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md`: rules (i)-(j)
- `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`: the seeded fixture tree for rules (a)-(j)
- `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`: running the lint as the first step of the per-commit gate
- `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`: running the lint from a pre-commit hook

---

## Files

- `tools/ci/lint/emit.sh`
- `tools/ci/lint/gate.sh`
- `tools/ci/lint/selftest/` (two-file fixture, empty-input case, selftest script)
- `Source/StealthFoundation/Private/Tests/Build/LintFrameworkTest.cpp` (G2 wrapper)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: the emitter prints one observation per finding and the gate alone decides pass, fail or NOT ASSESSED
  - Given: an emitter printing `file:line rule message`, and a gate script as the sole decider
  - When: the gate runs on (a) a clean file only, (b) a fixture with one finding, (c) an input set with zero files
  - Then: (a) exit 0; (b) nonzero and the observation line printed; (c) nonzero with NOT ASSESSED text distinguishable from FAIL
  - Edge cases: the emitter alone never returns a verdict; two findings produce two lines

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/lint/selftest/` (run by the T0 gate step) and the G2 wrapper `StealthGame.Build.LintFramework` (`Source/StealthFoundation/Private/Tests/Build/LintFrameworkTest.cpp`) must exist and pass (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`tools/ci/lint/selftest/run-framework-selftest.sh` 11/11 cases; `StealthGame.Build.LintFramework`, 2026-10-25)

---

## Dependencies

- Depends on: None
- Unlocks: `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`, `production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md`, `production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`, `production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md`, `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`, `production/epics/tuning-data-config/story-025-pre-commit-hook-framework.md`, `production/epics/tuning-data-config/story-029-knob-table-parser.md`, `production/epics/tuning-data-config/story-092-no-hardcoded-gameplay-scan.md`
- Sequencing note (not a plan dependency): the G2 wrapper test needs the StealthFoundation module of `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`.
- Open verification claims from the plan's Note lines: none. The plan's Note ("follows the `.claude/scripts` rule: helpers emit observations, never verdicts") is a design rule, not a verification claim.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: (a) clean file only -> `GATE: PASS`, exit 0; (b) two-file fixture with one finding -> exit 1, `finding.txt:3 demo seeded violation marker` printed, `GATE: FAIL`; (c) zero files -> exit 2, `GATE: NOT ASSESSED`, distinct from FAIL by text and code; edges: the rule/emitter alone exits 0 with no verdict text; two findings -> two lines; plus contract breaks: malformed line, missing observation file and a crashing rule -> NOT ASSESSED; SCANNED counted per distinct file)
**Contract (stable, for stories 012-016, 024, 025, 029, 092)**: rule scripts source `tools/ci/lint/emit.sh` and print only `<path>:<line> <rule> <message>` findings (repo-relative, forward slashes, no spaces; line 0 = whole file) and `SCANNED <path>` lines on stdout; diagnostics go to stderr. `gate.sh [--root <dir>] <rule-script>...` runs each rule as `bash <rule> <root>`; `gate.sh --obs <file>` gates a recorded stream. Exit 0 PASS / 1 FAIL / 2 NOT ASSESSED. Library also provides `lint_root`, `lint_rel`, `lint_scope_modules` (G5), `lint_prototype_module` and the G3 helper `lint_shrink_only` (first used by story 012).
**Bash on Windows**: the wrapper runs the selftest with Git for Windows' bash (`StealthBuildTest::FindBash`, overridable with `STEALTH_BASH`), never a bare `bash` from PATH (WSL launcher risk); the CI workflow's `shell: bash` is the same Git bash on the self-hosted runner.
**Deviations**: the demonstration rule `selftest/framework/demo-rule.sh` exists only to drive the selftest (no real rule existed yet); a third fixture (`two/`) proves the two-lines edge case; the empty-input case uses a temp directory (git cannot track an empty folder).
**Test Evidence**: `production/qa/evidence/story-011-t0-lint-framework-and-gate/test-run.txt` (build `Result: Succeeded`; 24/24 `StealthGame.` tests Success), `selftest.txt`, `stamp.txt`
**Run result**: N/A — CI tooling; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): one defect fixed before close (a selftest finding-count regex matched the `GATE:` line)
