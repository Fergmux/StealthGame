# ADR-0008: Test and automation strategy for leak and parity tests

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. (Bootstrap order: ADR-0009 → ADR-0007 + ADR-0008 → spikes S1/S2/S14 → ADR-0001. The spikes run on the harness this ADR defines.)

## Date
2026-10-24

## Last Verified
2026-10-24 (against UE 5.8.4 headers: `Misc/AutomationTest.h`, `Developer/CQTest`, `Engine/NetDriver.h`, `AutomationController/Private/AutomationCommandline.cpp`, UBT `BuildConfiguration.cs`)

## Decision Makers
Autonomous design pipeline (`/architecture-decision --review full`), validated by technical-director (TD-ADR) and the engine-specialist check; user acceptance pending.

Review record (2026-10-24): engine check **MINOR NOTES** (CQTest naming/empty-map/default-GameMode/wall-clock-timeout behaviour, `^`/`Group:` filters, `-SingleFile=` semantics, Gauntlet and `Build/` discovery, `-PktEmulationProfile`, legacy net-trace property scopes — all verified in 5.8.4 source and folded in). TD-ADR **CONCERNS** (8) → all revised in this draft: CQTest naming rule and class-name exemption; prefix/`Group:` selection; wall-clock only as failure ceiling; `WithGameMode` + fixture map, travel moved to T3; compile probes in a disposable pre-built project copy; net profiles as engine packet-simulation profiles (int loss, one-way lag stated); cooked-context flag claim made Verification 7; engine facts updated. Two follow-ups sit outside this pass's write scope and are listed in §2: `project.yaml` `commands.test` → `^StealthGame.`; test-standards CQTest exemption. Re-review of the revised text: not run (autonomous).

## Summary
Smoking Mirror's central promise (a player cannot be told from an NPC) is only true if every replicated byte, every HUD frame and every statistical deal stays kind-blind, and the GDDs make dozens of those properties build gates (IVP R11/R12, HUD-R3, TMK AC-01..07, CPA G-DECK, Generator goldens, TDC AC-3a). Decision: a five-tier test system — static lints, headless unit and reflection tests, in-process multi-client PIE tests on CQTest's `FPIENetworkComponent`, multi-process cooked-Development runs driven by Gauntlet with the `StealthTestHarness` module, and statistical/golden gates on plain-C++ cores — with mandatory negative controls, a fault-sink seam, a startup assertion of the active replication system, and per-commit / nightly / release gate wiring that runs locally until version control and CI exist (ADR-0009).

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Core (automation framework) + Networking (harness, net emulation, capture) |
| **Layer** | Foundation |
| **Knowledge Risk** | MEDIUM — the Automation framework is pre-cutoff and stable (LOW); CQTest's PIE network component, `EAutomationTestFlags` as an enum class and the net-trace path are newer than reliable training data and were checked in source |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `current-best-practices.md` (command-line automation, result lines, exit code), `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Module Ownership, Required ADRs, Open Questions AQ-01/04/05/10); `.claude/docs/coding-standards.md` Testing Standards; `.claude/rules/test-standards.md`; engine source read 2026-10-24 (below) |
| **Post-Cutoff APIs Used** | CQTest `FPIENetworkComponent` + `FNetworkComponentBuilder` (`AsListenServer()`, `WithClients(int32)`, `WithPacketSimulationSettings(FPacketSimulationSettings*)`) [V `Developer/CQTest/Public/Components/PIENetworkComponent.h:268, 514-556`]; `EAutomationTestFlags` enum class with `EAutomationTestFlags_ApplicationContextMask` [V `Misc/AutomationTest.h:93-144`]; `UNetDriver::IsUsingIrisReplication()` [V `Engine/NetDriver.h:2014`]; Net trace (`Net/Core/Trace/NetTrace.h`) [V header exists; U property-level content under the legacy driver] |
| **Verification Required** | (1) CQTest PIE network tests run under `UnrealEditor-Cmd -nullrhi` with a listen server and ≥ 3 clients [U — the component calls `LevelEditorModule.GetFirstActiveViewport()`, `PIENetworkComponent.cpp:121-127`; first harness test]; (2) a client-side capture can record per-pawn property handles, order, bit-length class, NetGUID class and RPC set under the legacy NetDriver (AC-IVP-02; spike S1 picks between net trace — legacy property scopes exist at `ENetTraceVerbosity::Trace` [V `RepLayout.cpp:1929, 2799, 3130`; `DataReplication.cpp:1098`], full verbosity needed [U] — and a harness receive hook) [U]; (3) Gauntlet launches one cooked Development listen host + N clients on one machine [V Gauntlet ships in 5.8; U topology]; (4) UBT `-SingleFile=` compiles a file placed under a module directory with that module's environment, even if not in the normal build; a file outside every module errors [V `UnrealBuildTool/Modes/BuildMode.cs:880-943`]; (5) `Automation RunTests` accepts `+`-separated filters, `^`/`StartsWith:` prefix filters and `Group:<name>` [V `AutomationCommandline.cpp:135-227`]; (6) a `Runtime` module may depend on CQTest (`Source/Developer`) in every target except Shipping non-editor [V `ModuleRules.cs:1882-1890`]; (7) whether `WITH_DEV_AUTOMATION_TESTS` is defined in cooked Development game builds, so T1 cores can also run inside T3 processes [U; `WITH_AUTOMATION_WORKER = !UE_BUILD_SHIPPING`, `Misc/Build.h:127`]. |

Verified engine facts [V]:
- `EAutomationTestFlags` contexts `EditorContext`, `ClientContext`, `ServerContext`, `CommandletContext` and filters `SmokeFilter`, `EngineFilter`, `ProductFilter`, `PerfFilter`, `StressFilter` exist; a test without an application-context flag fails a `static_assert` (`AutomationTest.h:93-137, 4127`).
- `Automation RunFilter <Flag>` exists but selects by flag across **all** tests, engine tests included (`AutomationCommandline.cpp:376-395, 688`); only one `RunTests` request is allowed per `-ExecCmds` (`:618-622`, matches `current-best-practices.md`).
- CQTest `FPIENetworkComponent` supports a listen server, a client count and packet simulation settings (`PIENetworkComponent.h:527-556`); state restore lives in `PIENetworkTestStateRestorer.h`. Tests are declared with `NETWORK_TEST_CLASS(_ClassName, _TestDir)`, flags `EditorContext | ProductFilter`, compiled only under `ENABLE_PIE_NETWORK_TEST` = `WITH_EDITOR && (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)` (`PIENetworkComponent.h:6, 591-594`). The full test name is `<TestDir>.<ClassName>.<Method>` — CQTest adds no prefix (`Impl/CQTest.inl:131-147, 214, 290-296`). `Build()` stops PIE and creates a new empty map before every test, the GameMode defaults to `AGameModeBase` unless `WithGameMode` is set, and PIE always runs in one process (`PIENetworkComponent.cpp:33-36, 119, 124-131`). CQTest step timeouts are wall-clock (`FDateTime::UtcNow`, 30 s default for network steps; `TestCommands.cpp:70-82`, `CQTestSettings.h:20`).
- Packet simulation in non-Shipping builds reads `-PktLag=`, `-PktLagVariance=`, `-PktLoss=` (int percent) and `-PktEmulationProfile=<Name>` from the command line (overriding ini); named profiles come from `[PacketSimulationProfile.<Name>]` in `DefaultEngine.ini` (`Engine/Private/Net/NetEmulationHelper.cpp:384-460, 534-570`; `NetDriver.cpp:785-788`). Lag applies to each side's outgoing packets [U semantics], so the profile is set on one side per the profile definition below.
- Gauntlet ships with 5.8; UAT discovers project scripts under `<Project>/Build/` (`Shared/EpicGames.Build/System/CompileScriptModules.cs:694`) — note ADR-0009's warning that the template `.gitignore` rule `build/` would ignore that folder on Windows.
- `UNetDriver` exposes `PktLoss`, `PktLossMinSize/MaxSize` packet-simulation fields (`NetDriver.h:471-489`); PIE has `FLevelEditorPlayNetworkEmulationSettings` (`UnrealEd/Classes/Settings/LevelEditorPlayNetworkEmulationSettings.h:43`).
- The prototype's 6 `StealthGame.Rules.*` tests use `EditorContext | ProductFilter`; the editor specs `StealthGame.EditorTools.*` use `BEGIN_DEFINE_SPEC`; 12/12 pass headless (`production/first-playable-report.md`).

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0007 (module layout; `StealthTestHarness` module and its Shipping/Test exclusion; `Private/Tests/` locations; compile-fail probe targets). ADR-0009 (CI host and per-commit wiring need a repository — pending user decision; local fallback defined here). |
| **Enables** | Spikes S1, S2, S3, S7, S9-S14, T-1 (ADR-0004), traversal net-emulation spike (ADR-0010); acceptance of ADR-0001; every story whose evidence is an automated test (coding-standards evidence table). |
| **Blocks** | Any Logic or Integration story (no test evidence possible before the tiers exist); any build leaving the team (AC-IVP-44 CRITICAL gate needs the leak suites). |
| **Ordering Note** | Accepted together with ADR-0007. The harness skeleton and the possession canary (AC-IVP-37) are built before any spike so the spikes report through the same harness. **Content inputs, not prerequisites** (architecture-review fix 2026-10-25; moved here from Depends On so the declared graph matches the bootstrap order ADR-0009 → ADR-0007/0008 → spikes → ADR-0001): ADR-0001 (what the leak suites assert: carriers, owner-only pointers, legacy NetDriver), ADR-0004 (tuning schema/lint tests it names), ADR-0006 (seeded integer streams the goldens and G-DECK use). This ADR defines the tiers and mechanisms; those suites land with those ADRs' stories. |

## Context

### Problem Statement
Most of Smoking Mirror's correctness is a *negative* property: nothing observable differs between a player and an NPC, or between teams, for anyone who should not know. Negative properties are invisible to play-testing and to ordinary unit tests; they need audits of everything that *could* differ (every replicated property, every received bunch, every HUD frame, every deal), each with a negative control proving the audit can see a leak. The GDDs specify these as BLOCKING criteria (IVP R11 "fails the build on any property not in the allowlist", R12 "any CRITICAL row … blocks every build that leaves the development team", HUD-R3 view-model diff suite, TMK AC-01..07, CPA F-8 G-DECK, Generator R-G11 goldens, TDC AC-3a). Several need separate processes (in-process PIE shares one asset object and runs editor script guards, TDC edge case; active.md harness artifact), some need network loss, and some need many runs. The prototype has a headless unit runner only. This ADR decides how all of that is structured, named, run and gated, before the spikes that decide ADR-0001.

### Constraints
- UE 5.8.4 automation framework; `commands.test` in `project.yaml` runs `UnrealEditor-Cmd … -ExecCmds="Automation RunTests StealthGame.; Quit" -unattended -nullrhi -stdout -FullStdOutLogOutput`; exit code 0 = no failures.
- Test root per engine: `Source/<Module>/Private/Tests/` (directory-structure rule); naming `F[System][Scenario]Test`, test name `StealthGame.[System].[Scenario]` (test-standards).
- Determinism and isolation (coding standards): no random seeds, no wall-clock assertions, each test sets up and tears down its own state; never disable a failing test to pass CI.
- No version control and no CI exist (ADR-0009 pending); one Windows developer machine (RTX 5090, 16 cores), MSVC only.
- Listen-server MVP; Iris off (ADR-0001 pins `net.Iris.UseIrisReplication=0`).

### Requirements
- Reflection audit of every replicated property and condition on CDOs and live instances, including Blueprint replication lists, with a negative control (IVP R11, AC-IVP-01, AC-IVP-42, AC-IVP-45, AC-TMK-03).
- Raw-bunch schema parity per pawn kind across ≥ 3 clients and ≥ 100 lifetimes, and the per-commit possession canary (AC-IVP-02, AC-IVP-37).
- HUD view-model / widget-tree diff parity across worlds that differ only in subject kind or team (HUD-R3, AC-HUD-01..04).
- Opponent-client audits: no marker objects, no team data in received state, packet audit with the listen host on either team, memory audit (AC-TMK-01..07).
- G-DECK statistical gate with pre-registered seeds and negative controls B1-B4 (CPA F-8).
- Generator golden seeds with bump rule and 50-seed batch audits (Generator R-G11).
- Tuning schema completeness and lint (TDC AC-3a, AC-28 compile-fail, AC-8 multi-process tamper).
- Net emulation profile for leak suites (`LeakNetProfile`: PktLag 100 ms, PktLoss 1-2 %).
- Assertion of the active replication system at startup; Iris rerun gate if Iris is ever enabled (AC-IVP-29).

## Decision

### 1. Five test tiers

| Tier | What | Framework / runner | Location | Environment |
|---|---|---|---|---|
| **T0 Static** | Include and Blueprint-specifier lint (ADR-0007 §3 rules a-i), Event Log log-argument lint, unique header names, tuning doc lint (TDC AC-3b), allowlist-file parse | Scripts that **emit observations** (file:line findings); the gate script decides pass/fail (`.claude/scripts` observation rule) | `tools/ci/lint/` | No engine; seconds |
| **T1 Unit & reflection** | Plain-C++ cores (Roster, queue, tuning validator, seed mixer, deck, generator stages), reflection audits on CDOs and on instances spawned in a test-created game world (AC-IVP-01/42, AC-3a, AC-TMK-03), G-DECK (F-8), Generator golden seeds (R-G11.2-3), compile-time contract tests | `IMPLEMENT_SIMPLE_AUTOMATION_TEST` (one scenario) or `BEGIN_DEFINE_SPEC` (many cases), inside `#if WITH_DEV_AUTOMATION_TESTS` | `Source/<OwningModule>/Private/Tests/<System>/`; fixtures in `Source/<OwningModule>/Private/Tests/Data/` | `UnrealEditor-Cmd -nullrhi` headless |
| **T2 In-process multi-client** | Possession canary (AC-IVP-37), net-config runtime differential (AC-IVP-45), HUD parity (AC-HUD-01..04), marker audience (AC-TMK-01/02/07), host-parity tests. **Not** travel (single-process PIE cannot represent seamless travel; ADR-0005) | CQTest `NETWORK_TEST_CLASS` with `FPIENetworkComponent`: `AsListenServer().WithClients(N).WithGameMode(AStealthGameMode::StaticClass())`, optional `WithPacketSimulationSettings` loaded from the engine profile; a first step loads the test fixture map (CQTest otherwise runs in an auto-created empty map with `AGameModeBase` [V]); scenarios that need a project map the component cannot load move to T3 | `Source/StealthTestHarness/Private/Tests/PIE/` (CQTest dependency in `StealthTestHarness.Build.cs` guarded by `Target.bBuildEditor`; files inside `#if ENABLE_PIE_NETWORK_TEST`) | `UnrealEditor-Cmd -nullrhi` headless [U, Verification 1]; rendering-dependent checks are Visual evidence, not T2 |
| **T3 Multi-process** | Raw-bunch schema parity (AC-IVP-02), opponent packet and memory audits (AC-TMK-04/05), tuning tamper (TDC AC-8), seamless travel and game-instance survival (S3, S4, AQ-10), traversal under loss, cull/re-open (S9/S10), transport classifier data capture (AC-IVP-03); crowd virtual-input stream capture for AC-CNB-27 replay (ADR-0016 §4; not part of ADR-0023's telemetry export); **on-demand `PerfReference` scenario** (ADR-0022 §6: scripted camera path + Insights capture on reference hardware, run by hand, never part of the nightly gate) | Gauntlet (UAT) launches one cooked **Development** listen host + N client processes; the in-game `StealthTestHarness` module runs a named scenario (`-StealthHarnessScenario=<Name>`), records captures and writes a JSON result; the Gauntlet node asserts | Harness C++: `Source/StealthTestHarness/Private/Scenarios/`; Gauntlet node scripts: `Build/Scripts/` under the project root (UAT discovery [V]) | Cooked Development, `-PktEmulationProfile=<Name>`, legacy NetDriver asserted |
| **T4 Statistical & batch** | AC-IVP-03 transport classifier AUC over ≥ 60 matches, Generator 50-seed batch audits (R-G11.4), F5 hide-event posterior from evidence playtests (IVP F5, AC-IVP-25/36), Iris rerun gate (AC-IVP-29) if Iris is enabled | Offline analysis tools over T3 captures and session records; fixed, checked-in seed sets and priors | `tools/ci/stats/` (analysis), inputs under `production/qa/` | Nightly / milestone |

Visual and UI evidence (screenshots in `production/qa/evidence/`) stay as the coding standards define them; they are not replaced by any tier above.

### 2. Naming
- Test names: `StealthGame.<System>.<Scenario>` for T1 (class `F<System><Scenario>Test`, test-standards; one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`); T2 is declared `NETWORK_TEST_CLASS(<Scenario>, "StealthGame.<System>.PIE")`, giving `StealthGame.<System>.PIE.<Scenario>.<Method>` (CQTest appends the method [V]; e.g. `StealthGame.HUD.PIE.StrikeTargetParity.NpcVsPlayer`), so T2 classes are named by scenario and are **exempt from the `F…Test` class-name convention** (follow-up: the test-standards rule gets this exemption in the change that adds the first CQTest test — `.claude/rules/` is outside this ADR pass's write scope); `StealthGame.<System>.MP.<Scenario>` for T3 Gauntlet nodes.
- `<System>` vocabulary (fixed list, one per GDD): `Tuning`, `Roster`, `Session`, `Seeds`, `Queue`, `Round`, `Visibility`, `Layout`, `CityGen`, `Pawn`, `Input`, `Pool`, `Camera`, `Crowd`, `Concealment`, `Encounter`, `Penalty`, `Objectives`, `Markers`, `HUD`, `Harness` (tests of the harness itself), `Build` (lint/probe wrappers).
- GDD acceptance-criterion IDs keep their GDD names; a GDD criterion that names only a scenario (e.g. TMK `NoOpponentMarkerObjects`) becomes `StealthGame.Markers.PIE.NoOpponentMarkerObjects`.
- Flags: T1 `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` (adding `ClientContext | ServerContext` so cores also run inside cooked T3 processes is allowed only once Verification 7 confirms `WITH_DEV_AUTOMATION_TESTS` there); T2 gets `EditorContext | ProductFilter` from the CQTest macro [V]; G-DECK and goldens add `StressFilter` for reporting only.
- Selection: by **prefix filter** `^StealthGame.` (a plain `RunTests StealthGame.` is a substring match [V]), never `RunFilter` (it selects engine tests too). Tiers that must be selected separately use automation `Group:` entries in the project's automation ini (`StealthPerCommit`, `StealthNightly`) [V `Group:` syntax; U ini section name — harness story]. Follow-up: `project.yaml` `commands.test` should become `Automation RunTests ^StealthGame.` — `project.yaml` is outside this pass's write scope; the current substring form still selects every project test.

### 3. Mandatory negative controls ("a gate you have not watched fail is not a gate")
Every leak, parity, statistical and lint gate ships with at least one negative control that runs in the same suite and must fail the inner check (the suite passes only if the control is detected):
- AC-IVP-01: a test-only class with one unlisted replicated property. AC-IVP-02/37: a pawn variant with `APawn::PlayerState` at `COND_None`. AC-HUD: an injected kind-dependent widget property. AC-TMK-04: an injected team field. G-DECK: B1-B4 (CPA F-8). Generator: an unbumped stage change ("unbumped drift"). T0 lint: one seeded violation per rule. Compile probes: a positive control (the same include compiles in `StealthGame`).
- Negative-control types live **only** in `StealthTestHarness` or in `#if WITH_DEV_AUTOMATION_TESTS` blocks of test files, never in runtime production code, so they cannot ship (ADR-0007 excludes the harness from Shipping/Test).

### 4. Test seams
- **Fault sink** (`IStealthFaultSink`, `StealthFoundation/Public/Server/`): every invariant violation, dead registration (ADR-0007 §4), rejected tuning or stale epoch reports here. Every T1-T3 test asserts zero unexpected faults at teardown and declares expected ones explicitly.
- **Injected time and randomness**: plain cores take an `IStealthClock` and seed/stream objects (ADR-0006) through constructors; tests never read wall-clock time or unseeded randomness. T2/T3 **pass/fail predicates** use server-time windows and frame-count bounds only. Wall-clock time appears only as a **failure ceiling** (a hung step fails instead of hanging the run): CQTest's `Until` timeouts are wall-clock [V] and are set through `TestFramework.CQTest.CommandTimeout.Network` to a generous ceiling; no assertion may depend on how much wall time passed.
- **Seam compilation**: test-only hooks are wrapped in `WITH_STEALTH_TEST_SEAMS`, defined `!UE_BUILD_SHIPPING` by `StealthFoundation` as a public definition; a Shipping build has no seam symbols (checked by the T0 Shipping-symbol check in §7).
- **Allowlist file**: the single checked-in `PawnReplicationAllowlist` (IVP R11; owner IVP, Shared Pawn rows co-owned by SCL) lives at `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` and is read by the T1 audit and the T2/T3 parity tests.

### 5. Replication-system assertion and net profiles
- On every T2/T3 start (and in every non-Shipping game process the harness module loads into), the harness reads `UNetDriver::IsUsingIrisReplication()` for the game net driver [V] and compares it with the pinned expectation (legacy, per ADR-0001); a mismatch fails the run and the active system is written into every result file. AC-IVP-29 (Iris rerun gate) activates automatically if the expectation is ever switched to Iris.
- Net profiles are **engine packet-simulation profiles**, not a project struct: `[PacketSimulationProfile.StealthLeak1]`, `[PacketSimulationProfile.StealthLeak2]` (PktLag 100 ms, PktLoss 1 % / 2 %) and `[PacketSimulationProfile.StealthClean]` in `Config/DefaultEngine.ini` (config change made by the harness story). **PktLag 100 is one-way on the client's outgoing side, matching ADR-0002's "symmetric PktLag 100 ms" acceptance profile; the harness story measures the resulting RTT once and records it in the profile comment** [U semantics]. T3 applies them with `-PktEmulationProfile=StealthLeak2` [V switch]; T2 loads the same profile into `FPacketSimulationSettings` (`LoadEmulationProfile`) [U name] and passes it to `WithPacketSimulationSettings`. `LeakNetProfile` in GDD/ADR text means `StealthLeak1`/`StealthLeak2`.

### 6. Capture for raw-bunch parity (S1)
The harness provides a **client-side** capture (what a non-owning client receives) and a **server-side** send capture (what the server sends to one connection), recording per actor: class, NetGUID class, property handles and order, bit-length class, condition, components, subobjects and RPC set. Spike S1 chooses the mechanism — net trace analysis (`NetTrace`) or a harness-only `UNetConnection`/replicator receive hook — and records it as an amendment to this ADR. Whatever mechanism wins must first pass its own negative control (`COND_None` PlayerState detected) before any parity result is trusted.

### 7. Compile-fail probes (ADR-0007 guard, TDC AC-28)
Probes are files under `tools/ci/compile-probes/<Name>.cpp`, each with an expected error substring in a header comment (e.g. the ADR-0007 `#error` text, or the missing-member error for a server-only key on `FStealthClientTuning`). The nightly job keeps a **disposable full-project copy** (a git worktree once ADR-0009 gives a repository; a mirrored copy until then — never the developer's working tree), builds `StealthGameEditor` there once per night, then for each probe copies it under `Source/<TARGET-MODULE>/Private/CompileProbes/` in that copy and compiles it alone with UBT `-SingleFile=<path>` (the file must sit under a module directory to receive that module's environment [V `BuildMode.cs:880-943`]); it requires failure containing the expected substring, then compiles the positive control (same include from `Source/StealthGame/Private/CompileProbes/`), then deletes the probe from the copy. Results are observations; the gate script decides. A Shipping-symbol check (no `StealthTestHarness`, no `WITH_STEALTH_TEST_SEAMS` symbols in a Shipping build) runs in the same nightly job.

### 8. Gate wiring

| Gate | When | Contents | Blocking for |
|---|---|---|---|
| **Per-commit** | every commit (locally before each snapshot until ADR-0009) | T0 lint; build `StealthGameEditor` Win64 Development; `Automation RunTests ^StealthGame.` (all T1 and T2; must stay ≤ 10 min wall on the dev machine — a suite that exceeds it moves to nightly by amendment, never by disabling); compile probes when a `Build.cs`, the guard header or any `Public/Server/` header changed | the commit |
| **Nightly** | once per day | everything per-commit; compile probes; T3 multi-process leak suites under `LeakNetProfile` and `CleanNetProfile`; Generator 50-seed batch audits; Shipping-symbol check; transport-classifier data capture | any build leaving the developer's machine the next day |
| **On demand (reference hardware)** | when ADR-0022's gates (R1-R3, RC-1..RC-3) or a performance story call for it | T3 `PerfReference` scenario on the 3060-class reference machine; results recorded in `production/qa/` | ADR-0013/ADR-0022 acceptance and the mass-production gate only; never a per-commit or nightly gate |
| **Release / milestone** | before any build leaves the team; each milestone gate | T4 AUC (AC-IVP-03), F5 posterior from evidence playtests, AC-IVP-29 if Iris, AC-IVP-33/44 doc gates (no CRITICAL row `Spk`/`Req`) | invited, evidence and public builds (IVP R12) |

Until ADR-0009 gives a repository and a CI host, the per-commit gate is run by `tools/dev/cycle.ps1 -Tests` plus the T0 scripts before every `_snapshots/` capture, and its result log is kept in `production/qa/`. Choosing the CI host (self-hosted runner on the dev machine is the default assumption, since builds need the installed engine) belongs to the devops story after ADR-0009.

### 9. Determinism goldens and statistical seed sets
- Generator goldens: `CityGenGoldenSeedCount` seeds with expected `LayoutHash`, candidate index, outcome and predicate metrics, checked in as a generated `Source/StealthGameplay/Private/Tests/Data/CityGenGoldens.generated.inl` of `constexpr` records, rewritten only by the rebaseline tool (amendment 2026-10-24, TD ruling: T1 tests do no file I/O; the replication allowlist stays the one data-file exception because T0 scripts also read it); each record stores `GeneratorKitVersion`, compiler and CPU family. A hash change without a `GeneratorKitVersion` bump fails as "unbumped drift" (R-G11.3). Cross-CPU/cross-compiler stability is claimed only for configurations actually run (MSVC x64 today) [U for others].
- G-DECK: the seed set is a `constexpr` array in `Source/StealthGameplay/Private/Tests/CharacterPool/CharacterPoolTestSeeds.h`, chosen once, never reseeded; a failing seed is a defect (CPA F-8; amendment 2026-10-24, TD ruling on the ADR-0012 conflict).
- Pre-registered priors and thresholds for T4 analyses are checked in beside the tools before the data is collected.

### 10. Prototype tests
The prototype's `StealthGame.Rules.*` (6) and `StealthGame.EditorTools.*` specs remain feasibility evidence and keep running in the per-commit gate until the prototype is retired; they are not promoted (ADR-0007 §8). The MCP toolsets (`StealthPlayToolset`, `StealthAssetToolset`) are developer tools, **not** a CI path: Python toolsets run under the editor script guard and execute client RPCs locally (active.md), so no gate may depend on them.

### Architecture Diagram

```
 commit ─► T0 lint (tools/ci/lint) ─► build ─► UnrealEditor-Cmd -nullrhi
                                              "Automation RunTests ^StealthGame."
                                                ├─ T1 Source/<Module>/Private/Tests/**   (cores, reflection, G-DECK, goldens)
                                                └─ T2 StealthTestHarness/Private/Tests/PIE (CQTest NETWORK_TEST_CLASS:
                                                       listen server + N clients, AStealthGameMode [+ StealthLeak profile])
 nightly ─► cook Development ─► Gauntlet ─► host.exe + N client.exe  (StealthTestHarness scenario,
                                              replication-system assert, capture) ─► JSON results ─► node asserts
          ─► compile probes (tools/ci/compile-probes, UBT -SingleFile)   ─► Shipping-symbol check
 release ─► T4 tools/ci/stats (AUC, F5 posterior, batch audits) + IVP doc gates (AC-IVP-33/44)
 every suite: negative control must be detected; fault sink must be clean
```

### Key Interfaces
- `IStealthFaultSink` (Foundation `Public/Server/`): `Report(EStealthFault, FName Source, int64 Detail)`; tests read `GetUnexpectedFaults()` via `WITH_STEALTH_TEST_SEAMS` accessor.
- `IStealthClock` (Foundation `Public/Server/`): `GetServerTimeSeconds()`, `GetServerTickIndex()`; injected into plain cores.
- Harness scenario entry: `-StealthHarnessScenario=<Name> -StealthHarnessOut=<path>`; result JSON `{scenario, build, replicationSystem, netProfile, passed, negativeControlDetected, faults[], captures[]}`.
- Net profiles: engine `[PacketSimulationProfile.StealthLeak1|StealthLeak2|StealthClean]` sections (`PktLag` int ms, `PktLagVariance` int ms, `PktLoss` int percent) → `-PktEmulationProfile=` (T3) or `FPacketSimulationSettings` (T2).
- T2 declaration shape: `NETWORK_TEST_CLASS(<Scenario>, "StealthGame.<System>.PIE") { FPIENetworkComponent<…> Network{…}; BEFORE_EACH() { Network.AsListenServer().WithClients(N).WithGameMode(AStealthGameMode::StaticClass())…Build(); } TEST_METHOD(<Case>) { … } }` [V macro names; U exact component template form].
- Allowlist file schema: `{ class, property, condition, audience, carrier, owners[] }` rows (IVP R11; ADR-0003 owns the content).
- Probe file header: `// EXPECT-ERROR: "<substring>"` and `// TARGET-MODULE: StealthPresentation`.

### Implementation Guidelines
- Every Logic story **must** ship T1 tests; every Integration story **must** ship a T2 or T3 test or a documented playtest (coding-standards table).
- Every leak, parity, statistical and lint gate **must** include a negative control that the suite detects; a gate never watched to fail **must not** be marked Done.
- Tests **must** be named `StealthGame.<System>.<Scenario>` (T1), `NETWORK_TEST_CLASS(<Scenario>, "StealthGame.<System>.PIE")` (T2, full name `….<Scenario>.<Method>`), `StealthGame.<System>.MP.<Scenario>` (T3) with `<System>` from the fixed list; runs **must** select with `^StealthGame.` or a `Group:`.
- T2 tests **must** set `WithGameMode(AStealthGameMode)` and load their fixture map; travel scenarios **must** be T3.
- Wall-clock time **may** only be a failure ceiling, never a pass/fail predicate.
- T1 tests **must** live in the owning module's `Private/Tests/<System>/`; T2 and T3 code **must** live in `StealthTestHarness`.
- Negative-control types and test seams **must never** be compiled into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only).
- Tests **must never** use wall-clock waits, unseeded randomness or shared mutable state; T2/T3 **must** bound by frames or server time.
- Every T2/T3 run **must** assert the active replication system and record it.
- No gate **may** depend on the MCP toolsets or Python editor scripting.
- A failing test **must never** be disabled, skipped or reseeded to pass a gate; a suite that is too slow moves tier by ADR amendment.
- Lints and analysis scripts **must** emit observations only; pass/fail lives in the gate script.

## Alternatives Considered

### Alternative 1: PIE-only integration through the MCP toolsets (the prototype's method)
- **Description**: Keep driving multi-client PIE through `StealthPlayToolset` / Python toolsets and the `StealthBot` driver, as the first playable did.
- **Pros**: Already works; fast to author; visual inspection possible.
- **Cons**: Requires a live editor and an agent; the Python editor script guard makes client RPCs execute locally (active.md), so results can be false; not runnable headless; no process isolation.
- **Rejection Reason**: Cannot be a build gate and has a known false-result mode.

### Alternative 2: Functional Tests (`AFunctionalTest` actors in test maps) as the integration layer
- **Description**: Place functional-test actors in dedicated maps and run them through the automation framework.
- **Pros**: Mature, map-based, good for gameplay sequences; visible in the editor.
- **Cons**: Multi-client network topologies need extra plumbing; test logic lives in maps (binary assets, merge-hostile before ADR-0009 decides a lock policy); weaker fit for reflection and capture audits.
- **Rejection Reason**: CQTest's PIE network component gives listen server + N clients + packet simulation in C++ directly [V]; functional tests remain allowed for single-world gameplay sequences where a map is the natural fixture.

### Alternative 3: In-process PIE only (no multi-process tier)
- **Description**: Run every leak suite inside one editor process.
- **Pros**: Simpler; no cook; faster.
- **Cons**: One process shares asset objects (TDC: "the client never reads its asset" cannot be proven), editor-only code paths run, seamless travel in PIE may differ from standalone (S3), and the listen-host packet audit (AC-TMK-04) needs real connections.
- **Rejection Reason**: Several BLOCKING criteria are explicitly multi-process; T3 is required.

### Alternative 4: Tier selection by `RunFilter` flags
- **Description**: Select per-commit vs nightly suites with `Automation RunFilter Smoke/Stress`.
- **Pros**: No naming convention needed.
- **Cons**: `RunFilter` selects across all tests including engine tests [V], so the project run would include unrelated engine suites.
- **Rejection Reason**: Selection by the `^StealthGame.` prefix filter and by `Group:` entries instead [V both syntaxes].

## Consequences

### Positive
- Every GDD build gate has a named tier, location, runner and negative control.
- Spikes S1-S14 report through the same harness that will later gate builds, so spike evidence and gate evidence are comparable.
- Headless per-commit gate stays cheap; expensive suites are nightly or release without being disabled.
- Process-isolation and network-loss properties are tested where they actually hold (cooked multi-process).
- Determinism and statistical gates are pre-registered, preventing seed shopping.

### Negative
- Building and maintaining the T3 Gauntlet harness and the capture tool is significant tooling work for 1-2 programmers (largest cost in this ADR).
- Nightly cooks need disk and time on the single dev machine.
- Until ADR-0009, "per-commit" is a local discipline, not an enforced gate.
- CQTest PIE tests in a Developer-module dependency tie T2 to editor targets (by design).

## Risks
- **CQTest PIE network component misbehaves under `-nullrhi` or with ≥ 3 clients.** Mitigation: harness story's first test is a 3-client smoke; fallback to a hand-rolled PIE launcher using `FRequestPlaySessionParams` [U].
- **Client-side capture cannot see property-level detail under the legacy driver.** Mitigation: S1 evaluates both mechanisms; server-side send capture plus client reflection dump as fallback; AC-IVP-02 stays `Spk` (blocking external builds) until one passes its negative control.
- **Per-commit run exceeds 10 min** as T2 grows. Mitigation: profile suites; move by amendment.
- **Gauntlet project-script location / packet-sim switches differ in 5.8.** Mitigation: verified in the harness story before any T3 suite is written [U].
- **Flaky network tests.** Mitigation: frame/server-time bounds, fixed seeds, `CleanNetProfile` runs paired with `LeakNetProfile`; flakiness is triaged with `/test-flakiness`, never by retries that hide failures.
- **Golden instability across CPUs/compilers.** Mitigation: goldens record compiler and CPU; integer-only RNG (ADR-0006); claims limited to configurations run.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| information-visibility-policy.md | R11 reflection audit + net-config audit on defaults and live instances (AC-IVP-01, 42, 45) | T1 reflection audit on CDOs + test-world instances, allowlist file location, T2 runtime differential; negative control (TR-ivp-006) |
| information-visibility-policy.md | R12 leak catalogue, CRITICAL ship gate, per-commit canary (AC-IVP-33, 37, 44) | Per-commit possession canary (T2), release gate wiring for doc gates (TR-ivp-007) |
| information-visibility-policy.md | AC-IVP-02 raw-bunch schema parity; AC-IVP-29 Iris rerun gate | T3 capture with S1-chosen mechanism and negative control; replication-system assertion; Iris gate auto-activates (TR-ivp-010) |
| information-visibility-policy.md | F5 hide-event posterior; AC-IVP-03 classifier AUC | T4 analysis tools with pre-registered priors over evidence playtests and T3 captures |
| hud.md | HUD-R3 subject-kind parity, view-model diff suite (AC-HUD-01..04) | T2 `StealthGame.HUD.PIE.*` with kind/team-swapped worlds and frame-by-frame view-model dumps; injected-leak negative control (TR-hud-002) |
| teammate-markers.md | AC-TMK-01..07 audience, packet, memory, include, host-parity tests | T2 (01, 02, 07), T3 (04 with listen host on either team, 05), T1 (03), T0 include grep (06 via ADR-0007 lint rule h) |
| character-pool-appearance.md | F-8 G-DECK with fixed pre-registered seeds and negative controls B1-B4 | T1 on the plain deck core; pre-registered seeds as a `constexpr` array in `CharacterPoolTestSeeds.h` (§9); B1-B4 in suite; never reseed (TR-cpa-007) |
| procedural-city-generator.md | R-G11 golden seeds, bump rule, batch audits | T1 goldens with `GeneratorKitVersion`, "unbumped drift" failure; nightly 50-seed batch audits (TR-citygen-002) |
| tuning-data-config.md | AC-3a schema completeness; AC-28 compile-fail; AC-8 multi-process tamper; schema defaults pinned by test | T1 `StealthGame.Tuning.SchemaComplete`; compile probes; T3 tamper scenario (TR-tdc-002) |

## Performance Implications
- **CPU**: None in shipped builds (harness and seams excluded from Shipping/Test). Development builds with the harness loaded pay only for active scenarios.
- **Memory**: Capture buffers in T3 runs only.
- **Load Time**: None in Shipping.
- **Network**: Test-only packet simulation; none in shipped builds.
- **Pipeline time**: per-commit ≤ 10 min budget; nightly cook + T3 estimated 1-2 h on the dev machine [U].

## Migration Plan
1. After ADR-0009 and with ADR-0007's skeleton: create `StealthTestHarness` (Runtime, Shipping/Test denied), the fault sink and clock interfaces, the allowlist file, the T0 scripts and the local gate wrapper (`tools/dev/cycle.ps1 -Tests` + lint).
2. First T2 test: 3-client CQTest smoke; then the possession canary AC-IVP-37 with its `COND_None` negative control (runs against the prototype pawn and must fail there — the prototype replicates `PlayerState` to everyone).
3. Gauntlet skeleton with one scenario and the replication-system assertion; then the S1 capture comparison.
4. Prototype tests keep running until the prototype is retired (deletion is the user's call).

## Validation Criteria
1. `Automation RunTests StealthGame.` headless returns exit code 0 with all T1/T2 suites, and each negative control is reported detected.
2. The possession canary fails against the current prototype pawn (watched to fail) and passes against a pawn built per ADR-0001.
3. A seeded unlisted replicated property fails AC-IVP-01; a seeded B1 bias fails G-DECK; an unbumped generator change fails the goldens.
4. A T3 run records `replicationSystem=legacy` and fails when started with `-UseIrisReplication=1` while the expectation is legacy.
5. Each compile probe fails with its expected substring and its positive control compiles.
6. A Shipping build contains no harness module and no seam symbols.
7. Per-commit gate wall time ≤ 10 min on the dev machine.

## GDD Sync Note
`teammate-markers.md` (Acceptance Criteria preamble) and `procedural-city-generator.md` (Acceptance Criteria preamble) state that automation tests live under `Source/StealthGame/Private/Tests/…`. Under ADR-0007/ADR-0008, T1 tests live in the **owning** module (Markers → `Source/StealthPresentation/Private/Tests/Markers/`; CityGen → `Source/StealthGameplay/Private/Tests/CityGen/`) and T2/T3 tests in `Source/StealthTestHarness/Private/`. The test **names** (`StealthGame.<System>.…`) are unchanged, so the `commands.test` filter still finds them. The path wording should be updated in those GDDs at their next revision (not edited by this ADR pass: GDD ownership).

## Related
- ADR-0001 (identity carriers and legacy NetDriver — what the leak suites assert), ADR-0003 (allowlist ownership and cull path), ADR-0004 (tuning tests and lint baseline), ADR-0006 (seeded streams), ADR-0007 (module layout, harness module, compile guard), ADR-0009 (VCS/CI host), ADR-0024 (Iris re-evaluation).
- GDDs: `design/gdd/information-visibility-policy.md` R11, R12, F5, AC-IVP-01/02/03/29/33/37/42/44/45; `design/gdd/hud.md` HUD-R3, AC-HUD-01..04; `design/gdd/teammate-markers.md` AC-TMK-01..07; `design/gdd/character-pool-appearance.md` F-8; `design/gdd/procedural-city-generator.md` R-G11; `design/gdd/tuning-data-config.md` AC-3a, AC-8, AC-28.
- Standards: `.claude/docs/coding-standards.md` Testing Standards; `.claude/rules/test-standards.md`.
- Prototype evidence (read-only): `Source/StealthGame/Private/Tests/StealthRulesTests.cpp`, `Source/StealthGameEditor/Private/Tests/*Spec.cpp`, `production/first-playable-report.md` Test results.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §T3 row and gate table: on-demand `PerfReference` scenario on reference hardware, outside the nightly gate — requested by ADR-0022 (sync item 7).
- §T3 row: the crowd's recorded virtual-input stream is an ADR-0008 T3 harness capture of ADR-0016's virtual input layer, not part of ADR-0023's export — requested by ADR-0023 (item 7).
- §9 (CONFLICT, ruled by technical-director): G-DECK seeds move from `GDeckSeeds.json` to a `constexpr` array in `CharacterPoolTestSeeds.h` (CPA GDD AC preamble and the coding standard's no-file-I/O rule), and by the same ruling the Generator goldens become a generated `constexpr` `.inl`; the replication allowlist stays a JSON file (T0 scripts read it) — raised by ADR-0012 (sync item 4).

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- ADR Dependencies: ADR-0001, ADR-0004 and ADR-0006 moved from Depends On to the Ordering Note as content inputs (Depends On keeps ADR-0007 and ADR-0009), which breaks the declared Foundation cycles — architecture-review fix 2026-10-25 (dependency cycles).
- GDD Requirements table, G-DECK row: "checked-in seed file" replaced by the `constexpr` seed array of §9 — architecture-review fix 2026-10-25 (other finding; CPA GDD sync request).
