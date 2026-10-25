# Story 001: Foundation module skeleton and server-only guard header

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0007 section 1-2; traced to architecture.md bootstrap order / epic Exit A) and `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`; this story supplies only the structural half of its compile-time client/server split, the `Public/View` / `Public/Server` folder contract and the guard header; GDD CR-10 / AC-28 are delivered by stories 040, 066 and 068)

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: Four runtime modules plus an Editor module and a Shipping-excluded harness replace the single prototype module; every public header lives in `Public/View/` (client-safe) or `Public/Server/` (guarded by a compile-time `#error` when included from StealthPresentation).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Build system and Core are pre-cutoff and stable, but 5.8 raised the C++ floor to C++20 and prefers `UE_LOGF` (post-cutoff, [V `LogMacros.h:199`]). `ModuleHostType Runtime` builds for every non-Program target [V `ModuleDescriptor.cs:783-810`]. Verification Required item (1) of ADR-0007 (the private guard define reaching every Presentation TU) is story 002, not this one. No claim in this story is unverified.

**Control Manifest Rules (this layer)**:
- Required: Code lives in one of the six modules (`StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthGame`, `StealthGameEditor`, `StealthTestHarness`); a new module needs an ADR amendment (source: ADR-0007)
- Required: Every header in a `Public/` folder sits under `View/` (client-safe) or `Server/` (server-only), except the module header and `StealthServerOnlyGuard.h` (source: ADR-0007)
- Required: New code logs with `UE_LOGF` and a `LogStealth...` category (source: ADR-0007)
- Forbidden: Never promote prototype code (`Source/StealthGame`) into a production module before its governing ADR is Accepted; promotion is a rewrite, not a file move (source: ADR-0007)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), as revised in `tdc-plan-amendments.md`:*

- [ ] StealthFoundation exists as a `Runtime` module and the test `StealthGame.Build.FoundationLayout` passes, asserting all of: the module is loaded by FModuleManager in the editor; the `StealthGame.uproject` module entry exists with Type Runtime; `StealthFoundation.Build.cs` lists Public deps Core/CoreUObject/Engine/NetCore and Private deps DeveloperSettings/Json; `LogStealthFoundation` is declared; `Public/StealthServerOnlyGuard.h` exists and contains the `#error` for STEALTH_PRESENTATION_MODULE; the folders `Public/View`, `Public/Server`, `Private/Tests` exist (each holds a tracked placeholder file because git cannot hold empty folders).

---

## Implementation Notes

*Derived from ADR-0007 sections 1, 2, 6 and Migration Plan step 1:*

- **Module files** under `Source/StealthFoundation/`: `StealthFoundation.Build.cs`, `Public/StealthFoundationModule.h`, `Private/StealthFoundationModule.cpp`, `Public/StealthServerOnlyGuard.h`. Use `PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs` as the prototype Build.cs does. Public deps exactly `Core`, `CoreUObject`, `Engine`, `NetCore`; Private deps exactly `DeveloperSettings`, `Json` (the Json dep is for the Event Log telemetry export of ADR-0023, and this story's own test also uses it to read the .uproject).
- **Module header exception**: `StealthFoundationModule.h` is the one header allowed directly in `Public/` besides the guard (include-lint rule (a), story 012). Header file names are project-unique, so keep the `Stealth` prefix.
- **Module class**: an `IModuleInterface` implementation with an empty `StartupModule` (never register implementations from `StartupModule`, ADR-0007 section 4). `LogStealthFoundation` is declared in the module header and defined in the cpp; export it so other modules can log to it (confirm the export macro form when compiling). Log only with `UE_LOGF`. The `UE_LOGF` format-string syntax was not read for this story: check it against `LogMacros.h` before first use.
- **Guard header** `Public/StealthServerOnlyGuard.h`, content as given in ADR-0007 section 2: `#pragma once`, then `#if defined(STEALTH_PRESENTATION_MODULE) && STEALTH_PRESENTATION_MODULE` / `#error "Server-only header included from StealthPresentation (ADR-0007, IVP R10)"` / `#endif`. Do not alter the message text: story 005 uses a substring of it as the expected compile error.
- **Folder contract**: create `Public/View/`, `Public/Server/`, `Private/Tests/` each with a tracked `.gitkeep` (not a header, so rule (a) is not affected). The test asserts the placeholder is present.
- **.uproject**: add one entry `{"Name": "StealthFoundation", "Type": "Runtime", "LoadingPhase": "Default"}` (LoadingPhase matches the existing StealthGame entry; the ADR does not state one). It must appear exactly once. Do not add the module to any `Source/*.Target.cs` `ExtraModuleNames`; Target.cs changes belong to story 003.
- **Prototype**: none replaced (new module). The monolithic `StealthGame` module and `StealthGame.Build.cs` stay untouched until story 003.
- **Test** `StealthGame.Build.FoundationLayout`: class `FBuildFoundationLayoutTest` in `Source/StealthFoundation/Private/Tests/Build/FoundationLayoutTest.cpp` (the plan's earlier name `FoundationModuleLoads` is superseded). Flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` (an application-context flag is mandatory; `EAutomationTestFlags` is an enum class), whole file inside `#if WITH_DEV_AUTOMATION_TESTS`. EditorContext only because the test reads project files from `FPaths::ProjectDir()` (source files are absent from a packaged game). Assertions: `FModuleManager::Get().IsModuleLoaded("StealthFoundation")`; parse `StealthGame.uproject` with `FJsonSerializer`, require exactly one entry named StealthFoundation with Type `Runtime`; read `StealthFoundation.Build.cs` as text and compare its Public and Private dependency lists to the exact sets above; reference `LogStealthFoundation` and check its category name; read `Public/StealthServerOnlyGuard.h` and require the `#error` line and the `STEALTH_PRESENTATION_MODULE` token; check the three folders and their placeholder files.
- **Negative control (spec edge case)**: feed the dependency-list comparison a mutated Build.cs string with one extra and one missing dependency and assert the comparison reports a mismatch. Do this inline: the `FStealthNegativeControl` helper (story 010) depends on this story and is not available yet.
- The guard `#error` actually firing is proven by the story 005 probe, not here.
- Run through `tools/dev/cycle.ps1 -Tests -NoLaunch` (it closes a running editor first; a new module needs a full rebuild).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`: StealthGameplay and StealthPresentation modules and the private `STEALTH_PRESENTATION_MODULE=1` define
- `production/epics/tuning-data-config/story-003-split-prototype-composition-root.md`: StealthGame and StealthGameEditor dependency changes, Target.cs
- `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`: StealthTestHarness module and its deny list
- `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`: proof that the guard `#error` fires
- `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`: the `WITH_STEALTH_TEST_SEAMS` definition
- `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`: the include lint that enforces the folder contract

---

## Files

- `Source/StealthFoundation/StealthFoundation.Build.cs`
- `Source/StealthFoundation/Public/StealthFoundationModule.h`
- `Source/StealthFoundation/Private/StealthFoundationModule.cpp`
- `Source/StealthFoundation/Public/StealthServerOnlyGuard.h`
- `Source/StealthFoundation/Public/View/.gitkeep`
- `Source/StealthFoundation/Public/Server/.gitkeep`
- `Source/StealthFoundation/Private/Tests/.gitkeep`
- `Source/StealthFoundation/Private/Tests/Build/FoundationLayoutTest.cpp`
- `StealthGame.uproject`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: StealthFoundation exists as a `Runtime` module and `StealthGame.Build.FoundationLayout` passes with all listed assertions
  - Given: the StealthFoundation Runtime module is built and the editor is running
  - When: the test queries FModuleManager and statically reads `StealthGame.uproject`, `StealthFoundation.Build.cs` and the module tree
  - Then: the module is loaded; the uproject entry exists with Type Runtime; Public deps are Core/CoreUObject/Engine/NetCore and Private deps are DeveloperSettings/Json; `LogStealthFoundation` resolves; `Public/StealthServerOnlyGuard.h` exists and contains the `#error` for STEALTH_PRESENTATION_MODULE; `Public/View`, `Public/Server` and `Private/Tests` exist and each holds a tracked placeholder file
  - Edge cases: a seeded extra or missing dependency fails the check (negative control); the guard `#error` firing is proven by story 005, not here

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the named automation test `StealthGame.Build.FoundationLayout` in `Source/StealthFoundation/Private/Tests/Build/FoundationLayoutTest.cpp` must exist and pass (qa.level is full, so tests are required)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`, `production/epics/tuning-data-config/story-003-split-prototype-composition-root.md`, `production/epics/tuning-data-config/story-004-test-harness-module-shipping-excluded.md`, `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`, `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`, `production/epics/tuning-data-config/story-008-fault-sink-and-test-seams-define.md`, `production/epics/tuning-data-config/story-009-log-capture-hook.md`, `production/epics/tuning-data-config/story-010-negative-control-helper.md`, `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`, `production/epics/tuning-data-config/story-038-schema-row-types.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`, `production/epics/tuning-data-config/story-100-rpc-counter-and-property-crc-helper.md`
- Open verification claims from the plan's Note lines: none for this story (the plan has no Note line on 001).
