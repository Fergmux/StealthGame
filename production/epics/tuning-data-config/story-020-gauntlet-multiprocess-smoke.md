# Story 020: Gauntlet host plus client smoke scenario with legacy-driver assertion

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 1 (T3), section 5 and Verification Required 3; traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: T3 is multi-process: Gauntlet (UAT) launches one cooked Development listen host plus N client processes, the in-game `StealthTestHarness` runs a named scenario and writes a result JSON, and the Gauntlet node asserts it; OQ-11 (Gauntlet versus a custom launcher) is answered: Gauntlet.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Gauntlet ships with 5.8 and UAT discovers project scripts under `<Project>/Build/` [V `EpicGames.Build/System/CompileScriptModules.cs:694`]. Unverified and touched here: Verification Required (3) the topology of one cooked Development listen host plus a client on one machine [U]; Verification Required (7) whether `WITH_DEV_AUTOMATION_TESTS` is defined in cooked Development game builds [U; `WITH_AUTOMATION_WORKER = !UE_BUILD_SHIPPING`, `Misc/Build.h:127`]; and the ADR-0007 Verification 2 item carried over from story 004, whether UBT and the module loader honour the `.uproject` `TargetConfigurationDenyList` in a cooked Development game [U]. The plan names the switch `-UseIrisReplication=1` for the negative control; that this switch makes a 5.8.4 game use Iris is not verified in the ADRs: open.

**Control Manifest Rules (this layer)**:
- Required: T2/T3 tests are bounded by frames or server time and assert and record the active replication system — source: ADR-0008
- Forbidden: never prove multi-process criteria in in-process PIE only; never select tiers with `Automation RunFilter` — source: ADR-0008
- Forbidden: never switch to Iris now (off by default in 5.8.4, Beta-flagged, deferred to ADR-0024) — source: ADR-0001, ADR-0003
- Guardrail: nightly cook plus T3 is estimated at 1-2 hours [U] — source: ADR-0008

---

## Acceptance Criteria

*From the story plan and amendments (bootstrap story, no GDD AC), scoped to this story:*

- [ ] A Gauntlet node under `Build/Scripts/` launches one cooked Development listen host and one client, the harness runs a named scenario (`-StealthHarnessScenario=<Name>`), both write result JSON with `replicationSystem=legacy`, the node asserts it, and the same node fails when started with `-UseIrisReplication=1` while the expectation is legacy (ADR-0008 Validation 4); per G4 the story closes PASS, or closes FAIL with a written amendment request and the named fallback, never by skipping or disabling the check.

---

## Implementation Notes

*Derived from ADR-0008 sections 1 (T3 row), 5 and Key Interfaces, and the G4 spike rule:*

- **Node.** `Build/Scripts/StealthMultiProcessSmoke.cs` is a Gauntlet test node (class and base type chosen from the 5.8.4 Gauntlet sources at implementation; not specified in the inputs). It launches a cooked Development host and one client, passes `-StealthHarnessScenario=<Name>` and `-StealthHarnessOut=<path>` to each, waits for both to exit, reads both JSON files and asserts `replicationSystem=legacy`. The T3 test name is `StealthGame.Harness.MP.<Scenario>` (ADR-0008 section 2: `StealthGame.<System>.MP.<Scenario>`).
- **Scenario.** `Source/StealthTestHarness/Private/Scenarios/SmokeScenario.cpp` registers the smoke scenario with the runner from story 018 (`StealthHarnessScenarioRunner`) and returns when both processes have connected. The result JSON is the one defined by story 018: `{scenario, build, replicationSystem, netProfile, passed, negativeControlDetected, faults[], captures[]}`.
- **Negative control (ADR-0008 Validation 4).** Start the same node with `-UseIrisReplication=1` while the expectation is legacy: the node must report a failure, and the suite passes only if that control is detected. If the switch is not honoured (see Engine Notes), the control must use another way to put the driver on Iris; record the choice here.
- **Failure handling.** A missing JSON from either process fails; a process crash fails; neither is a pass by default.
- **Cooked Development.** The harness module is excluded only for Shipping and Test (story 004), so it must be present in the cooked Development game. Story 004's note says the cooked observation of the deny list is recorded in this story: record whether the module loaded in the cooked Development host and client.
- **Build folder.** `Build/` must not be ignored by `.gitignore` (story 024 checks `git check-ignore -v Build/Scripts/x.cs` prints nothing); story 024 is not a declared dependency here: open.
- **Bounds.** Pass/fail uses process exit and JSON content only; wall clock is only a launch/timeout ceiling.
- **G4 closure.** PASS: the node passes with both legacy results and fails on the Iris control; record the observation (command, date, machine, result) in this story. FAIL (for example the topology on one machine does not work, Verification 3): record the FAIL, write an amendment request to ADR-0008 with the named fallback, and close FAIL. Stories 063, 064, 078 and 080-082 depend on this node; none may close by skipping it.
- **Deferred part.** The qa-lead spec marks this DEFERRED: it needs a cooked Development build and two processes on one machine [U]. The story may be implemented before the cook runs, but it is not Done until the observation exists.
- **Prototype.** None; the prototype has no multi-process harness.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-018-replication-system-assertion.md`: the runner, the probe and the result JSON writer
- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: the in-process T2 proof
- `production/epics/tuning-data-config/story-023-nightly-workflow.md`: running this node nightly under `StealthLeak2` and `StealthClean`
- `production/epics/tuning-data-config/story-063-preset-survives-travel.md`, `production/epics/tuning-data-config/story-078-remote-loopback-hash.md`, `production/epics/tuning-data-config/story-080-rejoin-snapshot.md`: tuning scenarios that reuse this node pattern
- `production/epics/tuning-data-config/story-024-vcs-conformance-check.md`: the `.gitignore` rule that keeps `Build/` tracked

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Build/Scripts/StealthMultiProcessSmoke.cs` (the Gauntlet node), `Build/Scripts/StealthGame.Automation.csproj` (UAT project script module)
- `Source/StealthTestHarness/Private/Scenarios/SmokeScenario.cpp`
- `Source/StealthTestHarness/Private/Capture/StealthNetCapture.cpp`, `.h`, `Source/StealthTestHarness/Private/StealthTestHarnessModule.cpp` (fix: the capture auto-install no longer reads the command line in a static initialiser)
- `tools/ci/mp/run-mp.ps1` (new `-CookedExe` switch for the interim launcher)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the Gauntlet node launches a cooked Development host and client, both write `replicationSystem=legacy`, and the node fails under `-UseIrisReplication=1` against the legacy expectation
  - Given: a cooked Development listen host and one client, started with `-StealthHarnessScenario=<Name>`
  - When: the node launches both and reads their result JSON
  - Then: both write `replicationSystem=legacy`; the node asserts it and passes; started with `-UseIrisReplication=1` against the legacy expectation the node fails (negative control, ADR-0008 Validation 4)
  - Edge cases: missing JSON from either process fails; a process crash; `Build/` is not git-ignored

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the Gauntlet node `StealthMultiProcessSmoke` (`Build/Scripts/StealthMultiProcessSmoke.cs`) with `Source/StealthTestHarness/Private/Scenarios/SmokeScenario.cpp`, a recorded run (PASS, or FAIL plus amendment request per G4) with the Iris negative control observed to fail. DEFERRED until a cooked Development build exists.

**Status**: [x] Recorded run PASS (G4): the Gauntlet node `StealthMultiProcessSmoke` passed on a cooked Development build with both `replicationSystem=legacy` and failed under the Iris control (2026-10-25); evidence in `production/qa/evidence/story-020-gauntlet-multiprocess-smoke/`

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-018-replication-system-assertion.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/tuning-data-config/story-023-nightly-workflow.md`, `production/epics/tuning-data-config/story-063-preset-survives-travel.md`, `production/epics/tuning-data-config/story-064-presets-cooked.md`, `production/epics/tuning-data-config/story-078-remote-loopback-hash.md`, `production/epics/tuning-data-config/story-080-rejoin-snapshot.md`, `production/epics/tuning-data-config/story-081-travel-epoch.md`, `production/epics/tuning-data-config/story-082-local-tamper-ignored.md`
- Open verification claims: ADR-0008 Verification 3 (Gauntlet topology on one machine) [U]; Verification 7 (`WITH_DEV_AUTOMATION_TESTS` in cooked Development) [U]; ADR-0007 Verification 2 (deny list honoured in a cooked Development game) [U], observation recorded here per story 004; `-UseIrisReplication=1` effect unverified; undeclared reliance on story 024 for `Build/` not being ignored.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1, G4 PASS: `RunUAT RunUnreal -test=StealthMultiProcessSmoke -Namespaces=StealthGame.Harness.MP -build=Saved/Staged/Dev3 -platform=Win64 -configuration=Development` launched a cooked Development listen host and one client, both ran the `Smoke` scenario and wrote result JSON, the node asserted `server-result.json: replicationSystem=legacy passed=True negativeControlDetected=True` and the same for `client1-result.json`, `StealthGame.Harness.MP.Smoke result=Passed`, `BUILD SUCCESSFUL`. Negative control: the same node with `-StealthRoleArgs="-StealthHarnessForceReplicationSystem=iris"` recorded `replicationSystem=iris passed=False` and `replicationSystem is 'iris', the pinned expectation is 'legacy'` and ended `BUILD FAILED` (control detected). Missing JSON from either process and a crashed process fail the node (`GetUnrealTestResult` returns Failed with the reason). The interim launcher `run-mp.ps1 -CookedExe` gave the same results on the cooked game and, earlier, on the editor binary)
**Observations (the story's open [U] items)**: (3) Gauntlet topology of one cooked Development listen host plus a client on one machine WORKS (host as a Client role with MapOverride `/Game/StealthGame/Maps/L_Prototype?listen`, second Client role opening 127.0.0.1). (7) `WITH_DEV_AUTOMATION_TESTS` IS defined in the cooked Development game: `Automation List` in the cooked exe reported `Found 1902 Automation Tests`. ADR-0007 Verification 2: the StealthTestHarness module loaded in the cooked Development host and client (the harness boot, replicationSystem and result records were written by it); the Shipping/Test side of the deny list is covered by story 006's symbol check, not by a cooked Shipping run here. `-UseIrisReplication=1`: UE 5.8.4 logs `Cmdline -UseIrisReplication=1 is forcing NetDriver GameNetDriver ... to use the Iris replication system` but the driver stayed legacy (the switch only applies when the net driver config has bCanUseIris and the Iris plugin is on, which ADR-0001 keeps off), so the node PASSED with `-StealthRoleArgs=-UseIrisReplication=1`; the switch is NOT a usable negative control here and the harness seam `-StealthHarnessForceReplicationSystem=<legacy|iris>` is the recorded alternative way to put a process on Iris. Machine: Windows 11, UE 5.8.4, one machine, both processes `-nullrhi`.
**Decisions**: node = `UnrealTestNode<UnrealTestConfiguration>`, two Client roles (host with `?listen`), test name `StealthGame.Harness.MP.Smoke`, result folder passed as `-StealthHarnessOut` (story 018 writes `server-result.json` / `client1-result.json` there); the project script module `Build/Scripts/StealthGame.Automation.csproj` references the engine's Gauntlet/AutomationUtils/UnrealBuildTool/EpicGames.Core DLLs (a ProjectReference to the engine's Gauntlet csproj made the second UAT run crash while validating the build record: `'..' can be only added at the beginning of the pattern` (an ArgumentException from the glob validator)); the smoke scenario ends the run a fixed number of frames (default 60, `-StealthHarnessSettleFrames`) after all clients joined, never by wall clock; `Build/` is not git-ignored (story 024 checks it). Cook: `BuildCookRun ... -pak -IgnoreCookErrors` (the cook reports 1 error from the sample's broken `CameraDirector_SandboxCharacter` import, present before this work); `-nopak` stages no content in 5.8.4 with this project (loose UFS files were not copied), so `-pak` is used
**Defect found and fixed**: the first cooked game exited immediately with code 777006 (crash during static initialisation): `FAutoInstall` in `StealthNetCapture.cpp` read `FCommandLine::Get()` from a static initialiser, which is fine in the modular editor (the DLL loads after the command line exists) but fatal in a monolithic cooked build (`Attempting to get the command line but it hasn't been initialized yet`). Found with a small debugger harness; the install now happens from the harness module's `StartupModule` (`Capture::InstallIfRequested`). No test could have caught it before a cooked run existed
**Deviations**: none; `Build/Scripts/StealthMultiProcessSmoke.cs` also gained the `-StealthScenario` and `-StealthRoleArgs` node parameters
**Test Evidence**: `production/qa/evidence/story-020-gauntlet-multiprocess-smoke/` (`gauntlet-node-legacy-PASS.txt`, `gauntlet-node-control-force-iris-FAIL.txt`, `gauntlet-node-control-UseIrisReplication1-PASS-inert.txt`, `cook-and-stage.txt`, per-run result JSON and records under `tdc020-*`), `stamp.txt`; the editor suite stays green: 37/37 `StealthGame.` tests Success after the fix (`production/qa/evidence/story-018-replication-system-assertion/test-run.txt` is the pre-fix run; the post-fix run is `test-run.txt` in this folder)
**Run result**: OBSERVED — two cooked Development processes (host + client) under Gauntlet; the observation is the result JSON and the node verdict, not a screenshot (nothing player-visible; both `-nullrhi`)
**Reviewed by**: focused code review (self-applied, unattended run): the static-initialiser defect above was found and fixed; no further defects found
