# Story 002: Gameplay and Presentation module skeletons with the presentation guard define

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0007 section 1-3; traced to architecture.md bootstrap order / epic Exit A) and `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`; this story delivers the private guard define that makes the client side of the compile-time split enforceable; GDD CR-10 / AC-28)

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: `StealthPresentation` declares `STEALTH_PRESENTATION_MODULE=1` as a `PrivateDefinitions` entry (never public), so any presentation translation unit that transitively includes a `Public/Server/` header fails to compile through the guard header, while the composition root that legitimately includes both faces is unaffected.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: This story owns ADR-0007 Verification Required item (1): the `PrivateDefinitions` define must reach every TU of StealthPresentation under unity build and shared or private PCH, and must not reach dependents. The ADR marks it [V] from source (`UEBuildModuleCPP.cs:2804`, shared-PCH `Definitions.<Module>.h` force-include `:2260-2300`, private PCH `:2233-2241`) but asks to keep the CI self-test anyway; the self-test is this story's test. `ModuleHostType Runtime` is used for both modules; `ClientOnly`/`ServerOnly` give no MVP protection on the listen-server Game target [V `ModuleDescriptor.cs:805-809`].

**Control Manifest Rules (this layer)**:
- Required: `StealthPresentation` declares `STEALTH_PRESENTATION_MODULE=1` as a `PrivateDefinitions` entry, never a `PublicDefinitions` entry (source: ADR-0007)
- Required: `PrivateDefinitions` reach every TU of the module under unity build and shared/private PCH; keep the CI self-test (source: ADR-0007)
- Forbidden: Never split layers into seven runtime modules, keep the single prototype module, use Game Feature plugins per feature, or mark Presentation `ClientOnly` now (source: ADR-0007)
- Forbidden: Never let a file under `StealthGameplay/*/Core/` include a `Features/` path (source: ADR-0007; the lint is story 013, this story creates the folders)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [ ] StealthGameplay and StealthPresentation exist as `Runtime` modules; StealthPresentation declares `PrivateDefinitions.Add("STEALTH_PRESENTATION_MODULE=1")` and never a PublicDefinitions entry; StealthGameplay has `Core/` and `Features/` source folders; both modules load and the test `StealthGame.Build.PresentationGuardDefineIsPrivate` passes (a Presentation TU sees the define, a dependent module does not).

---

## Implementation Notes

*Derived from ADR-0007 sections 1-3 and Migration Plan step 1:*

- **StealthGameplay** (`Source/StealthGameplay/`): `StealthGameplay.Build.cs` (Runtime; Public dep `StealthFoundation`), module header and cpp, log category `LogStealthGameplay` (ADR-0007 section 6, `UE_LOGF` only). Create `Core/` and `Features/` source folders with tracked placeholders. ADR-0007 lists a Private dependency set (EnhancedInput, AIModule, NavigationSystem, PoseSearch, ...) but says the final list is set by ADR-0010/0011/0013/0016: add none of them here.
- **StealthPresentation** (`Source/StealthPresentation/`): `StealthPresentation.Build.cs` (Runtime), module header and cpp, `LogStealthPresentation`, `PrivateDefinitions.Add("STEALTH_PRESENTATION_MODULE=1")`. The ADR's Private dependency list is `StealthFoundation`, `StealthGameplay`, `UMG`, `CommonUI`, `Slate`, `SlateCore`; add only `StealthFoundation` and `StealthGameplay` now (the UI modules arrive with the HUD work; the inputs do not say otherwise, so treat this as open). No `PublicDefinitions` entry for the define, ever.
- **.uproject**: add the two module entries (Type `Runtime`, LoadingPhase `Default` like StealthGame), each once. No Target.cs `ExtraModuleNames` change (story 003).
- **Test location (deviation from the plan, flagged)**: the plan puts `PresentationGuardDefineTest.cpp` in `Source/StealthFoundation/Private/Tests/Build/` "or HAR". A Foundation test cannot call into Presentation: Foundation must not depend upward (ADR-0007 section 1 table, lint rule (e)), and the harness (story 004) is not a dependency of this story. Place the test in `Source/StealthGameEditor/Private/Tests/Build/PresentationGuardDefineTest.cpp` and add `StealthPresentation` and `StealthGameplay` as Private deps of `StealthGameEditor.Build.cs` (ADR-0007 gives the Editor module all runtime modules as private deps; story 003 completes the list). The spec's "dependent module (StealthGame)" is not usable yet because StealthGame depends on the new modules only after story 003; the Editor module is the dependent module here.
- **Probe design** (spec): Presentation exports a tiny function, for example `bool StealthPresentationSeesGuardDefine()`, defined in a Presentation `.cpp` TU, returning whether `STEALTH_PRESENTATION_MODULE` is 1 in that TU. The dependent probe is evaluated in a TU of the dependent module itself (the test cpp in the Editor module), not in a header inlined from Presentation, so a leaked `PublicDefinitions` entry would make it true. Wrap the Presentation probe in `#if !UE_BUILD_SHIPPING` (the `WITH_STEALTH_TEST_SEAMS` define does not exist until story 008; switch to it then, and keep the probe out of Shipping per ADR-0008 section 4).
- **Test** `StealthGame.Build.PresentationGuardDefineIsPrivate`: class `FBuildPresentationGuardDefineIsPrivateTest`, flags `EditorContext | ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. It also reads `StealthPresentation.Build.cs` as text and asserts there is no `PublicDefinitions` entry for the define, that `StealthGameplay/Core` and `StealthGameplay/Features` exist, and that both modules are loaded via `FModuleManager`.
- **Control (spec edge case)**: a `PublicDefinitions` entry would make the dependent probe true; this is a control by inspection (the Build.cs text check), not a second build.
- **Prototype**: none replaced.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`: StealthFoundation, guard header, folder contract
- `production/epics/tuning-data-config/story-003-split-prototype-composition-root.md`: StealthGame private deps on the new modules, complete Editor module deps, Target.cs
- `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`: proof that an include of a `Public/Server/` header from a Presentation file fails to compile
- `production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md`: lint for the Core/Features direction rule

---

## Files

- `Source/StealthGameplay/StealthGameplay.Build.cs`
- `Source/StealthGameplay/Public/StealthGameplayModule.h`
- `Source/StealthGameplay/Private/StealthGameplayModule.cpp`
- `Source/StealthGameplay/Core/.gitkeep`
- `Source/StealthGameplay/Features/.gitkeep`
- `Source/StealthPresentation/StealthPresentation.Build.cs`
- `Source/StealthPresentation/Public/StealthPresentationModule.h`
- `Source/StealthPresentation/Private/StealthPresentationModule.cpp`
- `Source/StealthGameEditor/StealthGameEditor.Build.cs` (private deps added)
- `Source/StealthGameEditor/Private/Tests/Build/PresentationGuardDefineTest.cpp`
- `StealthGame.uproject`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: both modules exist and load, the guard define is private, and `StealthGame.Build.PresentationGuardDefineIsPrivate` passes
  - Given: StealthPresentation (PrivateDefinitions `STEALTH_PRESENTATION_MODULE=1`) and one dependent module
  - When: each module exports a tiny probe function returning whether the define is 1 in its own TU
  - Then: the Presentation probe returns true and the dependent-module probe returns false; the Presentation Build.cs has no PublicDefinitions entry for the define; StealthGameplay has `Core/` and `Features/` folders; both modules load
  - Edge cases: a PublicDefinitions entry would make the dependent probe true (control by inspection); the probe must live in a TU of the dependent module, not in a header inlined from Presentation

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the named automation test `StealthGame.Build.PresentationGuardDefineIsPrivate` (`Source/StealthGameEditor/Private/Tests/Build/PresentationGuardDefineTest.cpp`) must exist and pass (qa.level is full, so tests are required)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`
- Unlocks: `production/epics/tuning-data-config/story-003-split-prototype-composition-root.md`, `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`, `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`, `production/epics/tuning-data-config/story-092-no-hardcoded-gameplay-scan.md`
- Open verification claims from the plan's Note lines: none (no Note line on 002). The ADR-0007 item (1) check is this story's own test.
