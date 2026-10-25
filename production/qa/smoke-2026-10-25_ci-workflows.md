# Smoke record: CI workflows run the per-commit gate and the nightly gate (stories tdc-022 and tdc-023)

> **Stories:** `production/epics/tuning-data-config/story-022-ci-workflow-t0-lint-step.md`, `production/epics/tuning-data-config/story-023-nightly-workflow.md` · **Date:** 2026-10-25 · **Files:** `.github/workflows/tests.yml`, `.github/workflows/nightly.yml`, `tools/ci/nightly.ps1`
> **Result:** **PASS for everything observable without the runner; the real GitHub runs are DEFERRED.** The self-hosted Windows runner is not registered (a user action), so neither workflow has run on GitHub; a queued job is "not run", never a pass. The same scripts the workflows call were run locally, with the exact workflow step script, in Git Bash.

## Story 022: the tests workflow (four file-text statements plus local runs)

| Statement (AC-1) | Where it holds | Result |
|---|---|---|
| The T0 lint runs before the build | `tests.yml` has ONE gate step; the gate (`tools/ci/gate-per-commit.ps1`) runs `tools/ci/lint/gate.sh` first and stops before the build on a finding (`StealthGame.Build.GatePerCommit` cases `lint-failure-stops-before-build`, `build-failure-stops-before-automation`); the old separate `UnrealBuildTool.exe` build step is gone (asserted by `StealthGame.Build.CiWorkflows`) | PASS |
| The gate logic uses the prefix filter `^StealthGame.`, NOT ASSESSED on no match, FAIL on `Result={Fail}` | the workflow no longer re-implements the verdict (no inline `grep -q`, asserted); the gate's verdict function does (selftest cases `no-tests-matched` -> exit 2, `one-failure` -> exit 1, `pass-and-fail-is-fail`) | PASS |
| A queued job without a runner is "not run", never a pass | the comment `Until such a runner is registered, this job stays queued -- that is "not run", not a pass.` is kept in `tests.yml` (asserted) | PASS |
| No step maps a missing runner or a skipped stage to success | the step maps gate exit 1 and 2 and any other code to a failing job (`production/qa/evidence/story-022-ci-workflow-t0-lint-step/verdict-mapping.txt`: stub gate exit 0 -> step 0, 1 -> 1, 2 -> 2, 5 -> 1); a missing tool inside the gate is NOT ASSESSED (exit 2) | PASS |

Local runs (`production/qa/evidence/story-022-ci-workflow-t0-lint-step/`):
- `ci-step-local-run.txt`: the `run:` block of the gate step, extracted from the YAML and run in Git Bash with `bash --noprofile --norc -eo pipefail` (the workflow's shell) against the real tree: lint PASS, build Succeeded, `GATE-PER-COMMIT: PASS [automation] 43 test(s) completed, none failed`, `ELAPSED: total=373s budget=600s`, `per-commit gate: PASS`. This also answers the story's open question: the bash step CAN invoke the PowerShell gate (`powershell` is on the Git Bash PATH) and `${PIPESTATUS[0]}` carries the gate's exit code through `tee`.
- Validation: `python -c "import yaml; yaml.safe_load(...)"` parses both workflows; `actionlint 1.7.7` (downloaded release, run against `.github/workflows/tests.yml` and `nightly.yml`) reports nothing. shellcheck and pyflakes were not installed, so actionlint did not lint the embedded scripts.

## Story 023: the nightly workflow

| Check | Method | Result |
|---|---|---|
| `nightly.yml` is self-hosted, scheduled daily (02:30 UTC) plus manual dispatch, runs `tools/ci/nightly.ps1`, fails the job on FAIL and on NOT ASSESSED, uploads logs with `if: always()` | file text (asserted by `StealthGame.Build.CiWorkflows`); step script run with a stub nightly exiting 0/1/2 (`production/qa/evidence/story-023-nightly-workflow/verdict-mapping.txt`) | PASS |
| All four component groups run; a seeded failing observation from any one fails the job; a component that did not run, cannot decide or crashed is NOT ASSESSED; a clean run passes; logs collected; T3 profile recorded | `nightly.ps1 -SelfTest` (15 cases, `selftest.txt`) through `StealthGame.Build.NightlyDryRun` | PASS |
| Real T3 component under both profiles | `nightly.ps1 -Only t3-StealthLeak2,t3-StealthClean -ReuseStage -DevStageDir Saved/Staged/Dev3` ran the real Gauntlet node twice on this machine: `StealthGame.Harness.MP.Smoke (Win64 Development Client) result=Passed` under `-PktEmulationProfile=StealthLeak2` and under `StealthClean`, `BUILD SUCCESSFUL` each; the profile reached both processes' command lines; the overall verdict was `NOT ASSESSED` because gate, probes and Shipping-symbol were not selected (`real-t3-both-profiles-run.txt`) | PASS |
| Real compile-probe, per-commit-gate and Shipping-symbol components inside the nightly | the scripts themselves were observed in their own stories (005, 021, 006); the nightly's real Shipping and Development packaging (`BuildCookRun -clientconfig=Shipping`) and the full unattended night were NOT run (1-2 hours, needs the runner) | DEFERRED |

## Deferred, to record when the runner exists

1. Register the self-hosted Windows runner (labels `self-hosted`, `windows`, `UE_ROOT` set, Git LFS and PowerShell available) and watch `tests.yml` run once on a push.
2. Run `nightly.yml` once (workflow_dispatch) and record the elapsed time (ADR-0008 estimates 1-2 hours), the packaged harness binary name pattern and the Shipping result for story 006's open observation.
