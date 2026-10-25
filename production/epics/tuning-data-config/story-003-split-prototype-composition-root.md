# Story 003: Split the prototype module into the ADR-0007 composition root

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0007 section 1 and section 8; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: `StealthGame` stays the primary game module as the server composition root and depends privately on Foundation, Gameplay and Presentation; the Editor module depends privately on all runtime modules; prototype classes keep compiling in `StealthGame` and nothing is promoted before its governing ADR is built against.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: UBT forbids module cycles and exposes every dependency's `Public/` folder to dependents (hence the header rules); `IMPLEMENT_PRIMARY_GAME_MODULE` stays in `StealthGame`. Targets use `DefaultBuildSettings = BuildSettingsVersion.Latest` and `IncludeOrderVersion = Latest` today; C++20 is the floor in 5.8 (`CppStandardVersion.Cpp14`/`Cpp17` no longer compile). No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Code lives in one of the six modules; a new module needs an ADR amendment (source: ADR-0007)
- Forbidden: Never promote prototype code (`Source/StealthGame`) into a production module before its governing ADR is Accepted; promotion is a rewrite, not a file move (source: ADR-0007, ADR-0001, ADR-0010, ADR-0011, ADR-0020, ADR-0021)
- Forbidden: Never list `StealthTestHarness` in a Target.cs `ExtraModuleNames`; no runtime module depends on `StealthTestHarness` or `StealthGameEditor` (source: ADR-0007)
- Required: Every Integration story ships a T2 or T3 test or a documented playtest (source: ADR-0008; here the evidence is the build plus the unchanged prototype tests, which ADR-0008 section 10 keeps in the per-commit gate until the prototype is retired)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), from `tdc-plan.md`:*

- [ ] After StealthGame depends privately on StealthFoundation, StealthGameplay and StealthPresentation (composition root, keeps IMPLEMENT_PRIMARY_GAME_MODULE) and StealthGameEditor depends privately on all runtime modules, the editor target builds and the 12 prototype tests (6 `StealthGame.Rules.*`, 6 across `StealthGame.EditorTools.*` specs) still pass unchanged.

---

## Implementation Notes

*Derived from ADR-0007 sections 1, 8 and Migration Plan steps 1 and 3:*

- **StealthGame.Build.cs**: add `StealthFoundation`, `StealthGameplay`, `StealthPresentation` to `PrivateDependencyModuleNames`. Leave the existing public dependency list (Core, CoreUObject, Engine, InputCore, EnhancedInput, NetCore, AIModule, GameplayTasks, NavigationSystem, DeveloperSettings) as it is. The module keeps its name and `IMPLEMENT_PRIMARY_GAME_MODULE`.
- **StealthGameEditor.Build.cs**: private deps on all runtime modules (`StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthGame`; `StealthGame` and `UnrealEd` are there already; story 002 may already have added two). The Editor module stays `Editor` / `PostEngineInit`. The Editor module may name runtime modules; no runtime module may name `StealthGameEditor` or `StealthTestHarness`.
- **Target.cs files** (`Source/StealthGame.Target.cs`, `Source/StealthGameEditor.Target.cs`): the plan lists them, but the inputs do not state what must change in them. The `.uproject` module entries added by stories 001, 002 and 004 are what make UBT build the new modules; touch a Target.cs only if the build proves it necessary, and never add `StealthTestHarness` to `ExtraModuleNames`.
- **Nothing is promoted or moved**: all prototype classes (`StealthCharacter`, `StealthGameMode`, `StealthRules`, `UStealthSettings`, and the rest) keep compiling in `StealthGame` unchanged (ADR-0007 section 8). The MCP toolsets in `Source/StealthGameEditor` are kept as the seed of the production Editor module. Deleting any prototype file is an always-ask action; this story deletes none.
- **Regression check** (spec): the 12 prototype test sources (`Source/StealthGame/Private/Tests/StealthRulesTests.cpp` and the `Source/StealthGameEditor/Private/Tests/*Spec.cpp` files) must be byte-unchanged against the pre-story commit (`git diff --stat` on those paths shows nothing). Run the per-commit prefix `^StealthGame.` and count the `Test Completed` lines whose name starts with `StealthGame.Rules.` or `StealthGame.EditorTools.`; the count must be exactly 12, all `Result={Pass}`. Other `StealthGame.*` tests added by earlier stories are expected in the same run and are not counted. A run that prints "No automation tests matched" is NOT ASSESSED, not a pass; the editor exits 255 for both a failure and a no-match, so read the output, not the exit code.
- **Build and run**: `tools/dev/cycle.ps1 -Tests -NoLaunch` builds `StealthGameEditor Win64 Development` and runs the tests (its current filter is the substring `StealthGame.`; story 021 turns it into the `^StealthGame.` prefix). A module-dependency change needs the editor closed and a full rebuild.
- Prototype: this story keeps every prototype class compiling in StealthGame; nothing is promoted; the Source/StealthGameEditor MCP toolsets are kept as the seed of the new editor module.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`: the StealthFoundation module itself
- `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`: the Gameplay and Presentation modules and the guard define
- `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`: the positive control that compiles a Server header from StealthGame (needs this story)
- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: the harness dependency on StealthGame and the GameMode
- `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`: replacing `UStealthSettings` fields (the prototype classes stay until then)

---

## Files

- `Source/StealthGame/StealthGame.Build.cs`
- `Source/StealthGameEditor/StealthGameEditor.Build.cs`
- `Source/StealthGame.Target.cs` (only if the build requires a change)
- `Source/StealthGameEditor.Target.cs` (only if the build requires a change)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: after the dependency split, the editor target builds and the 12 prototype tests pass unchanged
  - Given: StealthGame privately depends on Foundation/Gameplay/Presentation and keeps IMPLEMENT_PRIMARY_GAME_MODULE; StealthGameEditor privately depends on all runtime modules
  - When: StealthGameEditor Win64 Development builds and the prototype tests run (prefix `StealthGame.Rules.` and `StealthGame.EditorTools.`)
  - Then: the build succeeds; exactly 12 tests pass (6 `StealthGame.Rules.*`, 6 across `StealthGame.EditorTools.*` specs); the 12 test source files are byte-unchanged versus the pre-story commit
  - Edge cases: a count other than 12 (a test silently dropped) fails; "No automation tests matched" is NOT ASSESSED, not a pass; no prototype class is moved

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a build plus automation regression run of the existing prototype tests (no new test file is named by the plan); record the run output (12 pass lines) and the clean `git diff --stat` of the test sources in the story's completion note (qa.level is full)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`
- Unlocks: `production/epics/tuning-data-config/story-005-compile-probe-runner-and-guard-probe.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`, `production/epics/tuning-data-config/story-021-per-commit-gate-script.md`, `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md`
- Open verification claims from the plan's Note lines: none (no Note line on 003).
