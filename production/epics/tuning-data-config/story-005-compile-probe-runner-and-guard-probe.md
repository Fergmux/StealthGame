# Story 005: Compile-probe runner with the ADR-0007 guard probe and its positive control

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 7 and ADR-0007 section 3; traced to architecture.md bootstrap order / epic Exit A) and `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`; the probe mechanism proves the compile-time half of the client/server face split; GDD CR-10 / AC-28)

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (with ADR-0007 section 3 for the guard being probed)
**ADR Decision Summary**: Compile-fail probes are files under `tools/ci/compile-probes/` with an expected-error header, compiled alone with UBT `-SingleFile=` inside a disposable full-project copy; each probe needs a positive control that compiles, and the runner emits observations while a separate gate script decides pass or fail.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0008 Verification Required item (4): UBT `-SingleFile=` compiles a file placed under a module directory with that module's environment, even if it is not in the normal build, while a file outside every module errors [V `UnrealBuildTool/Modes/BuildMode.cs:880-943`]. The ADR-0007 guard define reaching Presentation TUs is story 002's check. Not yet observed in this project: a real `-SingleFile` run against the new modules (this story is that first observation).

**Control Manifest Rules (this layer)**:
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects (source: ADR-0008; for probes the control is the positive compile of the same include from `StealthGame`)
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script (source: ADR-0008)
- Forbidden: Never mark a gate Done that was never watched to fail (source: ADR-0008)
- Forbidden: Never make a gate depend on the MCP toolsets or Python editor scripting (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [ ] `tools/ci/compile-probes` holds probe files with `// EXPECT-ERROR:` and `// TARGET-MODULE:` headers and a runner script that, on a disposable full-project copy, compiles each probe alone with UBT `-SingleFile=`, requires failure containing the expected substring for the guard probe (a StealthPresentation file including a `Public/Server/` header) and requires its positive control (the same include from StealthGame) to compile; the runner emits observations and its gate script decides pass/fail.

---

## Implementation Notes

*Derived from ADR-0008 sections 3, 7, 8 and Key Interfaces; ADR-0007 section 3:*

- **Probe file** `tools/ci/compile-probes/ServerHeaderInPresentation.cpp`: header comments `// EXPECT-ERROR: "<substring>"` and `// TARGET-MODULE: StealthPresentation` (the exact header form is in ADR-0008 Key Interfaces). The body includes the placeholder Server header below. The expected substring is a piece of the guard's `#error` text from ADR-0007 section 2, for example `Server-only header included from StealthPresentation`; keep it in sync with `StealthServerOnlyGuard.h` (story 001), and make the gate fail if the probe fails with a different message.
- **Placeholder server header**: the guard probe needs a real header under `Source/StealthFoundation/Public/Server/` that includes `StealthServerOnlyGuard.h` first. The plan does not name it: use a `Stealth`-prefixed unique name, for example `StealthGuardProbeTarget.h` (name proposed here, not fixed by the inputs), containing only the guard include and a trivial declaration, so include-lint rules (a)/(b) pass on it (story 012).
- **Runner** `tools/ci/compile-probes/run-probes.ps1` (or `.sh`; the plan allows either; pick one that the CI shell can run, the workflows use bash and a Windows self-hosted runner). Per ADR-0008 section 7, in the disposable copy: build `StealthGameEditor` once; for each probe copy it under `Source/<TARGET-MODULE>/Private/CompileProbes/`, compile it alone with `UnrealBuildTool.exe StealthGameEditor Win64 Development -Project=<copy>/StealthGame.uproject -SingleFile=<path>`, require failure whose output contains the expected substring; then compile the positive control from `Source/StealthGame/Private/CompileProbes/` and require success; then delete the probe from the copy. The runner prints one observation per outcome (probe failed with expected text, probe compiled unexpectedly, wrong error text, control compiled or not) and decides nothing.
- **Gate** `tools/ci/compile-probes/gate-probes.sh`: exit 0 only when the probe failed with the expected substring and its control compiled; nonzero when the probe compiles (control not detected), when it fails with a different message, or when the positive control fails; NOT ASSESSED (nonzero, distinguishable text) when no probe files are found. This follows the T0 emitter/gate split of story 011; share its output conventions where it is available (story 011 is not a dependency of this story, so keep the contract simple: observations on stdout, verdict only in the gate).
- **Pairing convention**: the ADR does not say how a probe names its positive control. Decide it here (for example a `// CONTROL-FILE:` header or a sibling file), because stories 054 and 068 reuse the runner with different probe and control bodies (a const read compiles, a client-face read of a server-only key does not).
- **Disposable copy**: ADR-0008 section 7 says a git worktree once a repository exists, never the developer tree; the repository exists now. Open point (the inputs are silent): a worktree holds only committed files, so uncommitted changes under test need to be overlaid into the copy, and whether the copy needs `Content/` for a code-only UBT compile is not stated; resolve both at implementation and record them. Create the worktree outside the project tree or under `.claude/worktrees/` (git-ignored).
- **G2**: ship a selftest under `tools/ci/compile-probes/selftest/` (a clean pass AND a seeded violation detected: for the gate, feed canned observation files for the four outcomes plus the empty-input case, so no UBT run is needed per commit) and the wrapper automation test `StealthGame.Build.CompileProbes` (class `FBuildCompileProbesTest`, `Source/StealthFoundation/Private/Tests/Build/CompileProbesTest.cpp`; proposed name) that runs the selftest through `FPlatformProcess::ExecProcess` and asserts exit code and observations. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. The full UBT-driven run is DEFERRED to the nightly job (story 023): it is slow and needs a worktree.
- **Prototype**: none replaced.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`: the guard header whose `#error` text is probed
- `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`: the Presentation module and its private define
- `production/epics/tuning-data-config/story-003-split-prototype-composition-root.md`: StealthGame depending on Foundation (needed for the positive control)
- `production/epics/tuning-data-config/story-023-nightly-workflow.md`: running the probes nightly in CI
- `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`: the mutation probe that reuses this runner
- `production/epics/tuning-data-config/story-068-server-key-not-on-client.md`: the server-key-on-client-face probe that reuses this runner

---

## Files

- `tools/ci/compile-probes/ServerHeaderInPresentation.cpp` (probe; headers EXPECT-ERROR, TARGET-MODULE, CONTROL-MODULE)
- `tools/ci/compile-probes/run-probes.ps1` (runner: disposable worktree, overlay, baseline build, `-SingleFile` per probe and control; observations only)
- `tools/ci/compile-probes/gate-probes.sh` (gate: 0 PASS / 1 FAIL / 2 NOT ASSESSED)
- `tools/ci/compile-probes/selftest/run-selftest.sh` and `tools/ci/compile-probes/selftest/fixtures/*.obs` (12 canned cases)
- `Source/StealthFoundation/Public/Server/StealthGuardProbeTarget.h` (placeholder Server header, guard first)
- `Source/StealthFoundation/Private/Tests/Build/CompileProbesTest.cpp` (test `StealthGame.Build.CompileProbes`)
- `Source/StealthFoundation/Private/Tests/Build/StealthBuildFileTestUtils.h` (added `FindBash`, `RunBashScript`)

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: the probe fails with the expected substring, its positive control compiles, and the gate decides from the observations
  - Given: probe `ServerHeaderInPresentation.cpp` (TARGET-MODULE StealthPresentation, EXPECT-ERROR substring) and its positive control (the same include from StealthGame)
  - When: `run-probes` emits observations and `gate-probes` decides
  - Then: the probe compile fails with the expected substring; the positive control compiles; the gate exits 0 only when both hold; the gate fails if the probe compiles (control not detected) or fails with a different message
  - Edge cases: the expected substring must be the text of the guard `#error`; never run on the developer tree; no probe files found is NOT ASSESSED

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the wrapper automation test `StealthGame.Build.CompileProbes` (`Source/StealthFoundation/Private/Tests/Build/CompileProbesTest.cpp`) and the selftest under `tools/ci/compile-probes/selftest/` must exist and pass per commit (G2); the real UBT `-SingleFile` run on a disposable worktree is DEFERRED to the nightly job and its observation is recorded in the story (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`StealthGame.Build.CompileProbes` + selftest, 2026-10-25); real UBT `-SingleFile` run observed locally (gate PASS)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`, `production/epics/tuning-data-config/story-003-split-prototype-composition-root.md`
- Unlocks: `production/epics/tuning-data-config/story-023-nightly-workflow.md`, `production/epics/tuning-data-config/story-054-validated-tuning-frozen-immutable.md`, `production/epics/tuning-data-config/story-068-server-key-not-on-client.md`
- Open verification claims from the plan's Note lines: UBT `-SingleFile` behaviour is verified from source [V `BuildMode.cs:880-943`] but not yet observed in this project; the disposable copy is a git worktree, never the developer tree (open details: overlaying uncommitted changes, whether `Content/` is needed).

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing. Per-commit: `StealthGame.Build.CompileProbes` runs the gate selftest (12 cases: clean pass; probe compiled, wrong error text, probe not compiled, control failed/not compiled/missing, mixed -> FAIL; no probes, empty/missing file, baseline build failed -> NOT ASSESSED) and checks the EXPECT-ERROR text is the guard #error text. Real run (first observation of UBT `-SingleFile` in this project, ADR-0008 Verification 4): probe in StealthPresentation failed with `C1189 #error "Server-only header included from StealthPresentation ..."`, positive control compiled in StealthGame, gate PASS (`probe-run.txt`).
**Decisions recorded**: pairing convention = optional `// CONTROL-MODULE:` (default StealthGame) and optional `// CONTROL-FILE:` (relative to tools/ci/compile-probes/, default the probe itself), for stories 054/068. Disposable copy = `git worktree add --detach` of HEAD outside the tree (`%TEMP%`) with `GIT_LFS_SKIP_SMUDGE=1`, the developer `Source/` and `.uproject` overlaid with robocopy (uncommitted code under test); `Content/` is not needed (LFS pointers suffice for a code-only compile). Runner is PowerShell; gate and selftest are bash.
**Engine findings (new)**: (1) `UnrealBuildTool.exe` run directly fails on this machine (needs .NET 10; only <=9 installed): use `Build.bat`, which selects the bundled .NET. (2) UBT compiles a `-SingleFile` only when the file is in the makefile source list; the "source directory added" check did not reliably notice the new CompileProbes/ folder ("Target is up to date", 0 actions, exit 0), so the runner touches the module Build.cs before each compile and treats "exit 0 without a Compile action" as `not-compiled` (FAIL). Both failure modes were watched to fail the gate before the fix.
**Deviations**: Nightly wiring is story 023 (DEFERRED as planned). Manifest Version updated to 2026-10-25f.
**Test Evidence**: `production/qa/evidence/story-005-compile-probe-runner-and-guard-probe/test-run.txt` (build `Result: Succeeded`; 17/17 `StealthGame.` tests Success), `probe-run.txt` (real probe run and gate output), `stamp.txt`
**Run result**: N/A — CI tooling and a build self-test; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): two runner defects found and fixed during the real run (direct UBT call, stale makefile false pass)
