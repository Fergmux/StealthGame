# Story 004: StealthTestHarness module excluded from Shipping and Test

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0007 section 1 and ADR-0008 section 1/7; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries (with ADR-0008: Test and automation strategy for leak and parity tests)
**ADR Decision Summary**: The multi-client and multi-process leak harness lives in a `Runtime` module `StealthTestHarness` whose `.uproject` entry carries `TargetConfigurationDenyList: ["Shipping", "Test"]`, which is not in any Target.cs `ExtraModuleNames` and which no runtime module depends on; this keeps negative-control types and test seams out of Shipping.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0007 Verification Required item (2): the deny list is honoured by UBT and the runtime module loader [V `ModuleDescriptor.cs:737`, `UEBuildTarget.cs:5559`, `Runtime/Projects/Private/ModuleDescriptor.cpp:567, 635-638`], but the cooked-Development load has still to be observed once [U]; that observation is story 006/020, not this story. `ExtraModuleNames` in a Target.cs is added with no configuration check [V `UEBuildTarget.cs:5030-5041`], which is why the module must not be listed there. `UncookedOnly` and `DeveloperTool` are rejected for the harness because neither builds into a cooked Development game [V `ModuleDescriptor.cs:790-797`].

**Control Manifest Rules (this layer)**:
- Required: `TargetConfigurationDenyList: ["Shipping", "Test"]` on the `StealthTestHarness` `.uproject` entry; `ExtraModuleNames` has no configuration check (source: ADR-0007)
- Forbidden: Never list `StealthTestHarness` in a Target.cs `ExtraModuleNames`; no runtime module depends on `StealthTestHarness` or `StealthGameEditor` (source: ADR-0007)
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) (source: ADR-0008)
- Required: T2 and T3 code lives in `StealthTestHarness` (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), as revised in `tdc-plan-amendments.md`:*

- [ ] the test `StealthGame.Build.HarnessModuleExclusions` passes, asserting statically: the `StealthGame.uproject` StealthTestHarness entry carries `TargetConfigurationDenyList` containing exactly Shipping and Test; no `Source/*.Target.cs` lists StealthTestHarness in ExtraModuleNames; no runtime module's Build.cs names StealthTestHarness or StealthGameEditor; and a Development editor load lists the module as loaded (`StealthGame.Harness.ModuleLoadsInDevelopment`).

---

## Implementation Notes

*Derived from ADR-0007 section 1 (module table row for the harness) and ADR-0008 sections 1 and 3:*

- **Module files** under `Source/StealthTestHarness/`: `StealthTestHarness.Build.cs` plus a module cpp and header (keep all of them in `Private/`: the harness exposes no public API, so there is no `Public/` folder to trip include-lint rule (a)). Log category `LogStealthHarness` (ADR-0007 section 6), `UE_LOGF` only. Module type `Runtime`.
- **Build.cs dependencies**: the ADR lists "Private: all runtime modules", but at this story only `StealthFoundation` is guaranteed to exist (story 002 is not a dependency, `StealthGame` joins in story 019 with the GameMode). List only what exists; later stories extend the list and add the CQTest dependency guarded by `Target.bBuildEditor` (story 019).
- **.uproject entry**: `{"Name": "StealthTestHarness", "Type": "Runtime", "LoadingPhase": "Default", "TargetConfigurationDenyList": ["Shipping", "Test"]}`, exactly those two values. Do not add `StealthTestHarness` to `Source/StealthGame.Target.cs` or `Source/StealthGameEditor.Target.cs`.
- **Static check** `StealthGame.Build.HarnessModuleExclusions`: class `FBuildHarnessModuleExclusionsTest` in `Source/StealthFoundation/Private/Tests/Build/HarnessModuleExclusionsTest.cpp` (Foundation already has the Json private dependency from story 001; the test reads `StealthGame.uproject`, every `Source/*.Target.cs` and every `Source/*/*.Build.cs` as text/JSON). Assertions: the deny list is exactly the set {Shipping, Test}; no Target.cs mentions the module in an `ExtraModuleNames` statement; no Build.cs other than the harness's own names `StealthTestHarness`; no runtime-module Build.cs (every module except `StealthGameEditor`) names `StealthGameEditor`. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`; EditorContext only because it reads source files.
- **Load check** `StealthGame.Harness.ModuleLoadsInDevelopment`: class `FHarnessModuleLoadsInDevelopmentTest` in `Source/StealthTestHarness/Private/Tests/HarnessModuleLoadsTest.cpp`, asserting `FModuleManager::Get().IsModuleLoaded("StealthTestHarness")` in a Development editor. Note for the implementer: a test hosted inside the harness can only be discovered if the module loaded, so by itself it cannot observe a load failure. Keep it as the plan names it, and rely on the static check plus the nightly cooked observation for the stronger guarantee; if you prefer a non-vacuous check, host the `IsModuleLoaded` assertion from a Foundation test instead and keep the same test name.
- **Negative control** (spec edge case): feed the static check text with a seeded dependency (a Build.cs naming `StealthTestHarness`, and a Target.cs listing it) and assert it is detected, using an inline control (the `FStealthNegativeControl` helper, story 010, is not available yet).
- **Cooked behaviour**: whether UBT and the runtime loader honour the deny list in a cooked Development game stays [U]; record the observation in story 006/020 evidence, not here. Verify nothing about Shipping here.
- Prototype: none (new module).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`: Foundation module and the Json dependency this test uses
- `production/epics/tuning-data-config/story-006-shipping-symbol-check.md`: scanning a Shipping build for the module binary and the seam sentinel; the cooked-Development load observation
- `production/epics/tuning-data-config/story-018-replication-system-assertion.md`: harness scenario runner and result JSON
- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: CQTest dependency, StealthGame dependency, first T2 test
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: cooked-Development launch of the harness

---

## Files

- `Source/StealthTestHarness/StealthTestHarness.Build.cs`
- `Source/StealthTestHarness/Private/StealthTestHarnessModule.cpp`
- `Source/StealthTestHarness/Private/StealthTestHarnessModule.h`
- `Source/StealthTestHarness/Private/Tests/HarnessModuleLoadsTest.cpp`
- `Source/StealthFoundation/Private/Tests/Build/HarnessModuleExclusionsTest.cpp`
- `StealthGame.uproject`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: `StealthGame.Build.HarnessModuleExclusions` and `StealthGame.Harness.ModuleLoadsInDevelopment` pass
  - Given: the StealthTestHarness Runtime module; the `.uproject`, Target.cs and Build.cs files
  - When: the static checks run, then the Development editor module list is read
  - Then: the uproject entry `TargetConfigurationDenyList` is exactly Shipping and Test; no `Source/*.Target.cs` lists it in ExtraModuleNames; no runtime Build.cs names StealthTestHarness or StealthGameEditor; the module is loaded in the Development editor
  - Edge cases: a seeded dependency is detected; the cooked honouring of the deny list stays [U], observed in 006/020

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the named automation tests `StealthGame.Build.HarnessModuleExclusions` (`Source/StealthFoundation/Private/Tests/Build/HarnessModuleExclusionsTest.cpp`) and `StealthGame.Harness.ModuleLoadsInDevelopment` (`Source/StealthTestHarness/Private/Tests/HarnessModuleLoadsTest.cpp`) must exist and pass (qa.level is full, so tests are required)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`
- Unlocks: `production/epics/tuning-data-config/story-006-shipping-symbol-check.md`, `production/epics/tuning-data-config/story-018-replication-system-assertion.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`
- Open verification claims from the plan's Note lines: whether UBT and the runtime module loader honour `TargetConfigurationDenyList` in a cooked Development game is [U] (ADR-0007 Engine Compatibility Verification 2); the cooked observation belongs to stories 006 and 020.
