# Story 022: CI workflow runs the T0 lint and the same gate

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 8 (gate wiring); traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: CI runs the same per-commit gate as the dev machine (T0 lint, build, `^StealthGame.` automation run); choosing the CI host belongs to the devops story, with a self-hosted runner on the dev machine as the default assumption because builds need the installed engine.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API is used. Unreal Engine cannot run on GitHub-hosted runners, so the job needs a self-hosted Windows runner with 5.8 installed and `UE_ROOT` set (comment in the existing workflow). Whether such a runner is registered is not verifiable from the repo and is a user or devops action outside this story; until then the job stays queued, which is "not run", not a pass.

**Control Manifest Rules (this layer)**:
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: runs select with `^StealthGame.` or `Group:` — source: ADR-0008
- Forbidden: never make a gate depend on the MCP toolsets or Python editor scripting — source: ADR-0008
- Guardrail: per-commit pipeline at most 10 minutes — source: ADR-0008

---

## Acceptance Criteria

*From the story plan (bootstrap story, no GDD AC), scoped to this story:*

- [ ] `.github/workflows/tests.yml` runs the T0 lint before the build and uses the per-commit gate logic (prefix filter, NOT ASSESSED on no match, fail on `Result={Fail}`), still on the self-hosted Windows runner; a queued job without a runner is documented as "not run", never a pass.

---

## Implementation Notes

*Derived from ADR-0008 section 8 and the current workflow file:*

- **Starting point (observed in the repo).** `.github/workflows/tests.yml` already has: `runs-on: [self-hosted, windows]` with a comment that a queued job is "not run, not a pass"; a checkout with `lfs: true`; a Build Editor Target step calling `UnrealBuildTool.exe` through `$UE_ROOT`; and an automation step that runs `Automation RunTests ^StealthGame.` and checks the output inline for `No automation tests matched`, `Test Completed` and `Result={Fail}`; plus a log upload step. All steps use `shell: bash --noprofile --norc -eo pipefail "{0}"` because the runner passes bash its script path unquoted and the runner folder has a space.
- **What changes.** Add a T0 lint step that precedes the build step (the story 011 gate, `tools/ci/lint/gate.sh`), and replace the inline grep logic with the verdict logic of the story 021 gate (`tools/ci/gate-per-commit.ps1`), so CI and the dev machine cannot drift. Whether the workflow calls the whole gate as one step or keeps separate lint, build and test steps that share the gate's parsing is open; the AC requires only the ordering and the shared logic. The gate is PowerShell; the steps are bash, so the call is `powershell -File` (or `pwsh`) from the bash step: confirm on the runner.
- **Verdict mapping.** NOT ASSESSED must fail the job (nonzero), FAIL must fail the job, and no step may map a missing runner or a skipped stage to success. Keep the "not run, never a pass" statement in the file's comments (it is already there; do not remove it).
- **Pinned facts to keep.** Prefix filter `^StealthGame.`; the editor exit code is 255 for both failure and no-match, so the verdict comes from output text.
- **Not in this story.** Registering the self-hosted runner (user/devops action), the nightly job, and any `Group:` automation sections.
- **Prototype.** The workflow already runs the 12 prototype tests through the prefix filter; they keep running inside the gate.
- **Config/Data and G2.** This story edits a workflow file only and ships no new script; the selftest and wrapper required by G2 belong to story 021, whose gate logic this workflow calls.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`: the gate script and its selftest
- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: the lint emitter and gate
- `production/epics/tuning-data-config/story-023-nightly-workflow.md`: `nightly.yml` and its jobs

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `.github/workflows/tests.yml`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the workflow runs the T0 lint before the build and uses the per-commit gate logic on the self-hosted runner
  - Setup: the workflow file on the self-hosted Windows runner
  - Verify: the T0 lint step precedes the build; the gate logic uses the prefix filter `^StealthGame.`, NOT ASSESSED on no match, and failure on `Result={Fail}`; the documentation states a queued job without a runner is "not run", never a pass
  - Pass condition: all four statements hold in the file text; no step maps a missing runner to success. DEFERRED: the real run needs the self-hosted runner, which is a user action.

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass recorded in `production/qa/smoke-*.md` (the four file-text statements above; the real runner run is DEFERRED)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`
- Unlocks: None
- Open verification claims: the plan Note says registering the self-hosted runner is a user/devops action outside this story, so the real CI run is unobserved. Open: whether the bash step can invoke the PowerShell gate on the runner.
