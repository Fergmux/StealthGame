# Story 019: CQTest listen server with three clients runs headless under -nullrhi

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0008 section 1 (T2) and Verification Required 1; traced to the epic's Exit A)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time; this story has no entry there.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: T2 is in-process multi-client PIE through CQTest (`FPIENetworkComponent`: listen server plus N clients, `StealthGame` game mode, fixture map), run headless under `UnrealEditor-Cmd -nullrhi`; whether that works headless is the first [U] to be settled by the first harness test, which is this story.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: This story IS the ADR-0008 Verification Required (1) check: CQTest PIE network tests run under `-nullrhi` with a listen server and at least 3 clients [U: the component calls `LevelEditorModule.GetFirstActiveViewport()`, `PIENetworkComponent.cpp:121-127`, which may be null headless]. Verified facts it builds on [V]: `NETWORK_TEST_CLASS(_ClassName, _TestDir)` with flags `EditorContext | ProductFilter`, compiled only under `ENABLE_PIE_NETWORK_TEST` = `WITH_EDITOR && (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)` (`PIENetworkComponent.h:6, 591-594`); full test name `<TestDir>.<ClassName>.<Method>` (CQTest adds no prefix); `Build()` stops PIE and creates a new empty map before every test and the GameMode defaults to `AGameModeBase` unless `WithGameMode` is set; PIE always runs in one process; CQTest step timeouts are wall-clock (30 s default for network steps) and are set through `TestFramework.CQTest.CommandTimeout.Network` to a generous ceiling. A Runtime module may depend on CQTest (`Source/Developer`) in every target except Shipping non-editor [V `ModuleRules.cs:1882-1890`].

**Control Manifest Rules (this layer)**:
- Required: T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map; T1 tests live in the owning module's `Private/Tests/<System>/`, T2 and T3 code lives in `StealthTestHarness` — source: ADR-0008
- Forbidden: never use wall-clock time as a test pass/fail predicate (only as a failure ceiling) — source: ADR-0008
- Forbidden: never disable, skip or reseed a failing test to pass a gate — source: ADR-0008
- Guardrail: per-commit pipeline at most 10 minutes (a suite that exceeds it moves to nightly by amendment, never by disabling) — source: ADR-0008

---

## Acceptance Criteria

*From the story plan and amendments (bootstrap story, no GDD AC), scoped to this story:*

- [x] A `NETWORK_TEST_CLASS(ListenServerThreeClients, "StealthGame.Harness.PIE")` using `FPIENetworkComponent` (`AsListenServer().WithClients(3).WithGameMode(AStealthGameMode::StaticClass())`, loading a fixture map) completes under `UnrealEditor-Cmd -nullrhi` with all steps bounded by frames/server time (wall clock only as a failure ceiling), and per G4 the story closes PASS, or closes FAIL with a written ADR-0008 amendment request naming the fallback (T2 tests run in a non-headless editor on the dev machine), never by skipping or disabling the test; the outcome of ADR-0008 Verification 1 is recorded in the story as an amendment note.

---

## Implementation Notes

*Derived from ADR-0008 sections 1, 2 and Key Interfaces (T2 declaration shape), Implementation Guidelines, and the G4 spike rule:*

- **Declaration shape.** `NETWORK_TEST_CLASS(ListenServerThreeClients, "StealthGame.Harness.PIE") { FPIENetworkComponent<...> Network{...}; BEFORE_EACH() { Network.AsListenServer().WithClients(3).WithGameMode(AStealthGameMode::StaticClass())...Build(); } TEST_METHOD(...) { ... } }` [V macro names; U exact component template form, per ADR-0008 Key Interfaces]. The full name reads `StealthGame.Harness.PIE.ListenServerThreeClients.<Method>`; the method name states the expected result. T2 classes are exempt from the `F...Test` class-name rule (`.claude/rules/test-standards.md`, CQTest exemption). `Harness` is the ADR-0008 `<System>` for tests of the harness itself.
- **Placement.** `Source/StealthTestHarness/Private/Tests/PIE/ListenServerThreeClientsTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST` (and `WITH_DEV_AUTOMATION_TESTS`). The CQTest dependency goes into `Source/StealthTestHarness/StealthTestHarness.Build.cs` guarded by `Target.bBuildEditor`. `AStealthGameMode` comes from the composition root `StealthGame` (story 003); the harness module depends privately on all runtime modules (ADR-0007 section 1).
- **Fixture map.** Because CQTest otherwise runs in an auto-created empty map with `AGameModeBase`, the first step loads the fixture map. The plan names no map and creates no `.uasset` in this run. The only map in `Content/StealthGame/Maps/` at writing is `L_Prototype.umap`; whether to use it or a dedicated fixture map (a Content change under the ADR-0009 single-writer rule) is open and is decided at implementation.
- **Bounds.** Every step is bounded by frame count or server time. Wall clock appears only as the failure ceiling: set `TestFramework.CQTest.CommandTimeout.Network` generously so a hung step fails instead of hanging the run. Assert one server world and three client worlds are up; assert no assertion depends on elapsed wall time.
- **Run command.** One `Automation RunTests` per `-ExecCmds` (ADR-0008, `current-best-practices.md`), selected with the prefix `^StealthGame.` or the exact test name; `UnrealEditor-Cmd -nullrhi` per the project's `commands.test`.
- **G4 closure (spike/verification story).** PASS: the test completes headless; write the observation into this story as an amendment note (command, date, result). FAIL (for example `GetFirstActiveViewport` is null headless): record the failure, file an ADR-0008 amendment request in this story naming the fallback (T2 tests run in a non-headless editor on the dev machine), and close FAIL. The test is never skipped, disabled or `#if`-ed out to close the story. Stories 062, 067, 073-077, 083, 085-086 and 093 inherit the fallback; story 100 (real-connection RPC positive control, Amendments 2) also depends on this story, and whether it inherits the fallback is not stated: open.
- **Outcome record (filled by `/dev-story`).** Verification 1 result: **PASS**, 2026-10-25. Command: `UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthGame.Harness.PIE; Quit" -unattended -nullrhi -stdout -FullStdOutLogOutput` (isolated: exit 0, 25 s including editor start-up) and inside the per-commit run `tools/dev/cycle.ps1 -Tests -NoLaunch` (`Automation RunTests StealthGame.`, 25/25 Success, 68 s build + tests). Evidence: `production/qa/evidence/story-019-cqtest-headless-proof/`. Amendment request: none needed (no fallback); see the amendment note below.

> **Amendment note — ADR-0008 Verification Required (1): PASS, [U] -> [V] (2026-10-25, UE 5.8.4).** CQTest `NETWORK_TEST_CLASS` with `FPIENetworkComponent`, a listen server and 3 clients, `AStealthGameMode` and the fixture map `/Game/StealthGame/Maps/L_Prototype` runs headless under `UnrealEditor-Cmd -nullrhi -unattended`: one `NM_ListenServer` PIE world (`UEDPIE_0_L_Prototype`, `AStealthGameMode`), three distinct `NM_Client` worlds, three server connections matched, each client received the replicated `AStealthGameState`. `LevelEditorModule.GetFirstActiveViewport()` did not block PIE start headless (PIE: "Server logged in", 3x "Client logged in", "Welcomed by server ... StealthGameMode"). Watched to fail: a mutated census assertion (expect 4 client worlds) failed the test with exit code -1. T2 stays headless in the per-commit gate; the fallback (non-headless editor) is not needed, so stories 062, 067, 073-077, 083, 085-086, 093 and 100 inherit nothing.
> Two corrections to ADR-0008's T2 description for the ADR owner (Key Interfaces is marked [U] there): (a) the builder is a separate object: `FNetworkComponentBuilder<State>().AsListenServer().WithClients(3).WithGameMode(...).Build(Network)`, not `Network.AsListenServer()...Build()`; (b) the stock component always runs "Stop PIE -> Create New Map -> Start PIE", so it cannot load a fixture map as "a first step" before PIE; the harness's `TStealthPIENetworkComponent::StartOnFixtureMap(MapPackage)` (`Source/StealthTestHarness/Private/Tests/PIE/StealthPIENetworkComponent.h`) queues the same public steps with the map load in place of the new map, and later T2 stories reuse it.
- **Prototype.** None. The prototype's PIE method (MCP toolsets, ADR-0008 Alternative 1) is not reused; no gate may depend on the MCP toolsets.
- **Shared support (G1).** Any helper reused by later T2 stories goes under `Source/StealthFoundation/Public/Server/Testing/Stealth<X>.h` (guard first, content under `WITH_STEALTH_TEST_SEAMS`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-018-replication-system-assertion.md`: the replication-system assertion and result JSON
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the multi-process (cooked, Gauntlet) tier; travel is never proven in single-process PIE
- `production/epics/tuning-data-config/story-062-client-option-ignored.md`, `production/epics/tuning-data-config/story-067-world-scoped-accessor-pie.md`, `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the first tuning tests that use this T2 shape
- `production/epics/tuning-data-config/story-017-net-emulation-profiles.md`: the packet-simulation profiles (`WithPacketSimulationSettings` is not exercised here)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/StealthTestHarness.Build.cs` (`CQTest` and `UnrealEd` guarded by `Target.bBuildEditor`; `StealthGame` and `NetCore` private dependencies; `SetupIrisSupport(Target)` because CQTest's `PIENetworkComponent.h` includes Iris headers)
- `Source/StealthTestHarness/Private/Tests/PIE/StealthPIENetworkComponent.h` (shared T2 support: `TStealthPIENetworkComponent::StartOnFixtureMap`)
- `Source/StealthTestHarness/Private/Tests/PIE/ListenServerThreeClientsTest.cpp`
- Fixture map reference only: `/Game/StealthGame/Maps/L_Prototype` (no `.uasset` is created by this story)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `NETWORK_TEST_CLASS(ListenServerThreeClients, "StealthGame.Harness.PIE")` runs headless, or the story closes FAIL with an amendment request
  - Given: `FPIENetworkComponent` `AsListenServer().WithClients(3).WithGameMode(AStealthGameMode::StaticClass())`, a fixture map, `UnrealEditor-Cmd -nullrhi`
  - When: the test steps run, bounded by frames/server time (wall clock only as a failure ceiling)
  - Then: PASS: it completes with one server and three client worlds; or a recorded FAIL with an ADR-0008 amendment request naming the fallback (T2 in a non-headless editor on the dev machine) (G4); the outcome is written into the story
  - Edge cases: never closes by skipping or disabling the test; `GetFirstActiveViewport` null headless (the [U]); stories 062, 067, 073-077, 083, 085-086, 093 inherit the fallback

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/ListenServerThreeClientsTest.cpp` (CQTest `StealthGame.Harness.PIE.ListenServerThreeClients`) must exist and pass, OR the recorded FAIL plus ADR-0008 amendment request per G4 (in both cases the Verification 1 outcome note is in this story)

**Status**: [x] Created and passing (`StealthGame.Harness.PIE.ListenServerThreeClients.OneServerAndThreeClientWorldsAreUp`, headless, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-003-split-prototype-composition-root.md`, `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`
- Unlocks: `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`, `production/epics/tuning-data-config/story-062-client-option-ignored.md`, `production/epics/tuning-data-config/story-067-world-scoped-accessor-pie.md`, `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`, `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-085-hot-reload-pie.md`, `production/epics/tuning-data-config/story-093-local-settings-not-tuning.md`, `production/epics/tuning-data-config/story-100-rpc-counter-and-property-crc-helper.md`
- Open verification claims: ADR-0008 Verification 1 (CQTest PIE network under `-nullrhi`, `GetFirstActiveViewport` at `PIENetworkComponent.cpp:121-127`) is [U] and is exactly what this story settles. Open: the fixture map choice and whether story 100 inherits the fallback. **Settled 2026-10-25: PASS (see the amendment note); fixture map `L_Prototype`; no fallback exists for story 100 to inherit.**

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: `NETWORK_TEST_CLASS(ListenServerThreeClients, "StealthGame.Harness.PIE")`, listen server + 3 clients + `AStealthGameMode` + fixture map, completes under `UnrealEditor-Cmd -nullrhi`; waits are conditions ("Set Worlds", "Await Clients Ready", "Client game state replicated") or a frame count (30 frames of `GFrameCounter` after ready); wall clock only as the ceiling `TestFramework.CQTest.CommandTimeout.Network` = 180 s, set in `BEFORE_ALL` and restored in `AFTER_ALL`; Verification 1 outcome PASS recorded above; watched to fail by mutation)
**Decisions**: fixture map = `L_Prototype` (the only map; a dedicated fixture map is a Content change under ADR-0009, left for a later T2 story if L_Prototype's crowd/city start-up ever slows the gate; today the test takes about 2 s inside the suite). Fixture-map loading via the harness's `TStealthPIENetworkComponent` (stock chain suppressed through the component's `IsInitializing` flag, same public steps re-queued). The harness depends on `StealthGame` (composition root, ADR-0007 section 1 "all runtime modules"). Replication-system assertion is story 018's, not added here.
**Deviations**: the ADR's builder shape differs from the engine's (recorded in the amendment note for the ADR owner); a shared support header was added in the harness rather than under `StealthFoundation/Public/Server/Testing/` (G1) because it depends on editor-only CQTest, which Foundation cannot depend on.
**Test Evidence**: `production/qa/evidence/story-019-cqtest-headless-proof/test-run.txt` (build `Result: Succeeded`; 25/25 `StealthGame.` tests Success), `headless-pie-run.txt` (isolated headless run + mutation run), `stamp.txt`
**Run result**: N/A — headless harness proof; the run itself is the evidence (PIE worlds verified by assertions, nothing rendered under `-nullrhi`)
**Reviewed by**: focused code review (self-applied, unattended run): no defects found; no package was saved by the PIE run (no `SavePackage` lines; `git status` shows no Content change)
