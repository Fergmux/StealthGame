# Story 004: Build-time include and grep check for the HUD module (AC-HUD-11) plus the ADR-0021 lints

> **Epic**: HUD
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/hud.md`
**Requirement**: `TR-hud-001`, `TR-hud-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (with ADR-0008, ADR-0021, and ADR-0007 lint (h))
**ADR Decision Summary**: Presentation reads exactly its own controller's carriers, the public GameState structs and client-local queries; it never iterates actors, never reads a pawn's `PlayerState`/`Controller`/`Owner`, never calls `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, never includes a `Public/Server/` header and writes nothing replicated (ADR-0021 §9). ADR-0021 adds lints: no `AddToViewport`, no `AHUD` canvas drawing, no `UWidgetComponent` on a pawn, no `SetApplicationScale`, and world projection only inside `SStealthWorldAnchorLeaf::OnPaint`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine surface: none (the checks are build-time include and grep rules over source and over UI Blueprint reference lists; no UWorld/UObject at run time). The module still builds against 5.8.4 / C++20. The lints encode ADR-0021 engine facts: E1 (`AddToViewport` widgets sit above every `AddToPlayerScreen` layer whatever the ZOrder, so one stray widget breaks the HUD-R4 order), E5 (`SetApplicationScale` is process-wide and would scale the editor in PIE), E3/E4 (projection belongs in a volatile leaf's `OnPaint`). `UE_LOGF` is preferred over `UE_LOG` in 5.8 for any lint diagnostics emitted from C++.

**Control Manifest Rules (this layer)**:
- Required: `AStealthHUD` and all client-side wiring live in `StealthPresentation` — source: ADR-0007
- Forbidden: Never use `TActorIterator`, `TActorRange`, `GetAllActorsOfClass`, `GetAllActorsWithInterface`, `GetPlayerControllerIterator`, `GetGameMode`/`GetAuthGameMode`, or `GetPlayerState`/`GetController` on a non-local pawn in `StealthPresentation` (lint h) — source: ADR-0007
- Guardrail: HUD ≤ 0.3 ms CPU (AC-HUD-22), ≤ 0.5 ms with markers; UI GPU ≤ 0.5 ms at 1080p on the reference GPU — source: ADR-0021

This story makes the HUD's leak boundary mechanical, so no later HUD story can compile a violation. It extends the TDC lint framework (rule (h) already forbids the actor-search identifiers under `Source/StealthPresentation`) with the HUD-specific rules.

---

## Acceptance Criteria

*From GDD `design/gdd/hud.md` AC-HUD-11 (verbatim) and ADR-0021 lints (derived items marked), scoped to this story:*

- [ ] **AC1** (AC-HUD-11, `StealthGame.HUD.IncludeCheck`): **GIVEN** the HUD module, **WHEN** the build-time include and grep check runs, **THEN** it includes no Policy, Roster, Round, LSC, Penalty or Objectives server header, calls no `CanViewerKnow`, `GetPlayerState`, `IsPlayerControlled`, `GetController` on any pawn other than the local one, reads the raw own C-TeamView `State` only inside the `PresentedOwnState` owner, and declares no per-figure widget class (extends AC-IVP-17 and art bible 7.7 test 3). Made mechanical: the four identifiers appear in no HUD-module file except a named allowlist file (the local-pawn accessor), and no class derived from `UUserWidget`/`SCompoundWidget`/`SLeafWidget` in the HUD module has an `AActor`/`APawn`/public-handle member or constructor parameter (the allowlisted `SStealthWorldAnchorLeaf` registration excepted).
- [ ] **AC2** (derived, ADR-0021 lints, `StealthGame.HUD.ForbiddenApiLint`): **GIVEN** a planted violation of each of ADR-0021's lints (`AddToViewport`, `AHUD` canvas drawing, `UWidgetComponent` on a pawn, `SetApplicationScale`, `TActorIterator`/`GetAllActorsOfClass`/`GetPlayerControllerIterator`/`GetGameMode`, a `Server/` header include, a UI Blueprint referencing a class off the reference allowlist, `ProjectWorldLocationToScreen` or `GetProjectionData` called outside `SStealthWorldAnchorLeaf::OnPaint`) **WHEN** the lint runs **THEN** each fails the build, and the clean tree passes.
- [ ] **AC3** (derived, GDD Edge Cases "a removal line could reach an opponent" and ADR-0021 §9, `StealthGame.HUD.IncludeCheck`): **GIVEN** the HUD module **WHEN** a compile-time test instantiates feed-line creation with a non-roster type and a grep runs **THEN** the only feed-line creation entry point takes the own-team roster type (any other type fails to compile), and the grep finds no write to a `Replicated` UPROPERTY under `StealthPresentation` (including writes through setters).

---

## Implementation Notes

*Derived from ADR-0021 §1, §2, §6, §9 (lints), ADR-0007 lint (h) and ADR-0008:*

Extend, do not duplicate, the TDC lint family: rule (h) is `tools/ci/lint/include-lint-gh.sh` (`production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`) and already flags `TActorIterator`, `TActorRange`, `GetAllActorsOfClass`, `GetAllActorsWithInterface`, `GetPlayerControllerIterator`, `GetGameMode`, `GetAuthGameMode` and non-local `GetPlayerState`/`GetController` under `Source/StealthPresentation`. This story adds a HUD rule script, `tools/ci/lint/hud-lint.sh`, that reuses the T0 lint emitter and gate of `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`, and one allowlist file, `tools/ci/lint/hud-allowlist.txt`. Rules:

1. **Include boundary** (AC-HUD-11): no `#include` of a header under `Public/Server/` or of the Policy, Roster, Round, LSC, Penalty or Objectives query headers from any file under `Source/StealthPresentation`. The identifiers `CanViewerKnow`, `GetPlayerState`, `IsPlayerControlled`, `GetController` appear in no HUD-module file except the allowlisted local-pawn accessor file (the one place that reads the local pawn; name it in the allowlist, and adding a file to the allowlist needs review). Raw own C-TeamView `State` is read only inside the `PresentedOwnState` owner file (story 014 creates it; the allowlist names the path `Private/Hud/HudPresentedOwnState.cpp` in advance).
2. **No per-figure widget class**: reflect or grep every class derived from `UUserWidget`, `SCompoundWidget` or `SLeafWidget` in the HUD module and fail if it has an `AActor`, `APawn` or public-handle member or constructor parameter. `SStealthWorldAnchorLeaf` registration (story 011) is the named exception.
3. **ADR-0021 forbidden calls**: `AddToViewport` (any), `AHUD` canvas drawing (`DrawText`, `DrawRect`, `DrawLine`, `DrawTexture`, `Canvas->` in an `AHUD` subclass), `UWidgetComponent` in a pawn or any component added to a pawn for presentation, `SetApplicationScale`, and `ProjectWorldLocationToScreen` / `GetProjectionData` outside `SStealthWorldAnchorLeaf::OnPaint`.
4. **UI Blueprint reference allowlist** (ADR-0007): a DataValidation audit fails the build if a UI Blueprint references a class or function off the checked-in reference allowlist (no `GetGameMode`, `GetAllActorsOfClass`/`WithInterface`, `GetPlayerState` on a pawn, `Get Player Controller` with a non-literal-0 index, no `Server/` class). The allowlist content comes from ADR-0021 and ADR-0007; if the Blueprint audit commandlet does not exist yet, register the gap with TDC and test the rule against a seeded fixture.
5. **Feed-line entry point** (compile-time): the only function that creates a feed line takes the own-team roster entry type (the C-TeamView roster element, for example `FRosterTeamViewEntry`) by const reference; a `static_assert`/`requires`-based test shows a non-roster type fails to compile (C++20 concepts are available, 5.8 minimum). The feed itself is story 035; here ship the signature stub and the test.
6. **No replicated write**: grep finds no assignment to a property declared `Replicated`/`ReplicatedUsing` under `Source/StealthPresentation`, and also none through a setter or `Set*` call on a replicated member (follow the setter names of the carrier components).

Selftest fixtures (TDC G6 style): one seeded violation per rule plus a clean control, under `tools/ci/lint/selftest/hud/`, so the lint is watched to fail. The Logic tests are thin wrappers that run the script on the fixtures and on the real tree (the real tree must pass; the HUD module is small at this point, so zero findings is expected). Prototype disposition: `Source/StealthGame/Private/StealthHUD.cpp` uses the `AHUD` canvas and reads team fields; it sits outside `StealthPresentation`, so the lint does not fail on it, and its removal is story 059. Do not add `StealthGame` to the lint scope to hide it.

Decisions D19, D20: the interact ring, ping rings and markers are world-anchored, so the projection lint's allowlist is exactly one function (`SStealthWorldAnchorLeaf::OnPaint`).

Out of this story: the camera epic's own include check (`production/epics/camera-system/story-024-presentation-include-check-tell-matrix.md`); the leak suite itself (AC-HUD-01..07).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/camera-system/story-024-presentation-include-check-tell-matrix.md`: the camera epic's own include check.
- `production/epics/hud/story-011-hud-world-anchor-leaf-shell.md`: the leaf whose `OnPaint` is the projection allowlist, and which relies on the projection lint.
- `production/epics/hud/story-035-hud-team-feed-queue-f8-and-removal-lines.md`: the feed queue behind the entry point.
- `production/epics/hud/story-014-hud-presented-own-state-and-viewer-state-machine.md`: the `PresentedOwnState` owner file named in the allowlist.
- `production/epics/hud/story-053-hud-no-clock-no-counts-pie.md`: AC-HUD-12 runtime string collection.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/lint/hud-lint.sh`
- `tools/ci/lint/hud-allowlist.txt`
- `tools/ci/lint/selftest/hud/` (seeded violations, clean controls and the selftest script)
- `Source/StealthPresentation/Private/Hud/HudFeedEntryPoint.h` (the signature stub restricted to the own-team roster type)
- `Source/StealthPresentation/Private/Tests/HUD/IncludeCheckTest.cpp` (class `FHudIncludeCheckTest`, name `StealthGame.HUD.IncludeCheck`)
- `Source/StealthPresentation/Private/Tests/HUD/ForbiddenApiLintTest.cpp` (class `FHudForbiddenApiLintTest`, name `StealthGame.HUD.ForbiddenApiLint`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: `IncludeCheck`, mechanical form
  - Given: the HUD module sources.
  - When: the include and grep check runs.
  - Then: no Policy/Roster/Round/LSC/Penalty/Objectives server header is included. `CanViewerKnow`, `GetPlayerState`, `IsPlayerControlled` and `GetController` appear only in the named allowlist file (the local-pawn accessor). Raw own C-TeamView `State` is read only inside the `PresentedOwnState` owner file. No `UUserWidget`/`SCompoundWidget`/`SLeafWidget` subclass has an `AActor`/`APawn`/public-handle member or constructor parameter (`SStealthWorldAnchorLeaf` registration allowlisted).
  - Edge cases: the story names the allowlist files. Adding a file to the allowlist needs review.
- **AC-2**: `ForbiddenApiLint` planted violations
  - Given: one planted violation per lint: `AddToViewport`, `AHUD` canvas drawing, `UWidgetComponent` on a pawn, `SetApplicationScale`, `TActorIterator`/`GetAllActorsOfClass`/`GetPlayerControllerIterator`/`GetGameMode`, a `Server/` header include, a UI Blueprint referencing a class off the allowlist, and `ProjectWorldLocationToScreen`/`GetProjectionData` outside `SStealthWorldAnchorLeaf::OnPaint`.
  - When: the lint runs on each.
  - Then: each fails the build, and the clean tree passes.
  - Edge cases: the Blueprint reference allowlist comes from ADR-0021. Story 011 AC3 relies on the projection lint.
- **AC-3**: feed entry point and replicated writes
  - Given: the HUD module.
  - When: a compile-time test instantiates feed-line creation with a non-roster type, and a grep runs.
  - Then: the only entry point accepts the own-team roster type (other types fail to compile). The grep finds no write to a `Replicated` UPROPERTY under `StealthPresentation`.
  - Edge cases: the grep must also cover writes through setters.

QL-STORY-READY: ADEQUATE (qa-lead, 2026-10-25; criteria revised after the first pass and re-judged)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/HUD/IncludeCheckTest.cpp` (`StealthGame.HUD.IncludeCheck`) and `ForbiddenApiLintTest.cpp` (`StealthGame.HUD.ForbiddenApiLint`) — must exist and pass (`/story-done` checks that they EXIST; pass/fail is established by `/gate-check` and `/smoke-check`)
- Gate: BLOCKING (Logic default).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/hud/story-002-hud-view-model-and-presenter-core-shell.md`
- Unlocks: `production/epics/hud/story-028-hud-leak-suite-strike-and-look-parity.md`
