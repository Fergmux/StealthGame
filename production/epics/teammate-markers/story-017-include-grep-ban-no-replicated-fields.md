# Story 017: Include and grep ban, no replicated fields, no pawn component

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The locked rule "markers exist only on teammates' clients, enemies cannot obtain them" is enforced by the absence of data, and kept that way by build-time checks on the marker module. The prototype's marker code iterated every character and read a team field replicated on every pawn; the production module must not be able to do either, even by a future mistake. This story adds the include check and the symbol grep (no server query header, no actor iteration, no player-state or controller read from a pawn, no `PlayerArray` outside the public-name helper), the reflection and RPC audit (the module and every class it touches declare no replicated property, RPC, actor or component), and the lints that forbid a `UWidgetComponent` or any component on a pawn and the use of `AddToViewport` or the `AHUD` canvas for markers. Each lint needs a seeded violation that proves it fails.

**GDD**: `design/gdd/teammate-markers.md` (TM-R1, TM-R2, TM-R12; AC-TMK-03, AC-TMK-06)
**Requirement**: `TR-tmk-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (and the include and lint boundary of ADR-0007, ADR-0008)
**Also governed by**: ADR-0021 (Presentation may never write anything replicated; no `UWidgetComponent`; `AddToViewport` banned), ADR-0007 (lint rules)
**ADR Decision Summary**: Presentation reads only its own controller's carriers; it never iterates actors, reads a pawn's `PlayerState`/`Controller`/`Owner`, calls `IsPlayerControlled`/`IsBotControlled`/`CanViewerKnow`, includes a `Public/Server/` header, or writes anything replicated; no component is added to a pawn for presentation.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The lints run in the T0 lint framework (tuning-data-config stories 011 to 016); the reflection audit enumerates `UClass` property flags (`CPF_Net`) and `UFunction` flags (`FUNC_Net`) in an editor-context automation test.

**Control Manifest Rules (this layer)**:
- Required: Presentation reads only its own controller's carriers, the GameState public structs and client-local queries — source: ADR-0001, ADR-0021
- Forbidden: Never use `TActorIterator`, `TActorRange`, `GetAllActorsOfClass`, `GetAllActorsWithInterface`, `GetPlayerControllerIterator`, `GetGameMode`/`GetAuthGameMode`, or `GetPlayerState`/`GetController` on a non-local pawn in `StealthPresentation` (lint h) — source: ADR-0007
- Forbidden: Never add a `UWidgetComponent` or any other component to a pawn for presentation; never use `AddToViewport` or the `AHUD` canvas for game UI — source: ADR-0021

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-06, AC-TMK-03) and ADR-0021, scoped to this story:*

- [ ] **AC1** (AC-TMK-06, [U] `IncludeGrepBan`): **GIVEN** the marker module's source **WHEN** the build-time include check and grep run **THEN** no server query header is included and none of `TActorIterator`, `GetAllActorsOfClass`, `GetPlayerControllerIterator`, `GetFirstPlayerController`, pawn `GetPlayerState`/`GetController`/`GetOwner`, `IsPlayerControlled`, `IsBotControlled`, `CanViewerKnow`, or a `PlayerArray` read outside the public-name helper appears (a seeded violation of each fails the check).
- [ ] **AC2** (AC-TMK-03, [U] `NoMarkerReplicatedFields`): **GIVEN** the build **WHEN** the IVP R11 reflection audit and an RPC enumeration run over the marker module's classes and every class it touches **THEN** the module declares no replicated property, no RPC and no replicated actor or component, and accent colour, `TeammateMarkerLargeMult` and `ShowTeamNames` appear in no replicated layout.
- [ ] **AC3** (ADR-0021, derived): **GIVEN** the module **THEN** no `UWidgetComponent` or other component is added to any pawn, and neither `AddToViewport` nor the `AHUD` canvas is used to draw markers (lint; a seeded `AddToViewport` fails it).

---

## Implementation Notes

*Derived from TM-R2, AC-TMK-03, AC-TMK-06 and the control manifest:*

Extend the existing checks rather than writing new infrastructure: AC-IVP-17 and AC-PSR-42 are the include and grep checks this one extends (information-visibility-policy story 034 builds the Presentation include check; tuning-data-config stories 012 to 016 build the lint framework and seeded-violation controls). Add `Source/StealthPresentation/Private/Markers/` to the scanned set and add the symbol list above as the marker rule. The one allowed `PlayerArray` read is the public-name helper (TM-R11); allow-list it by function name, not by file.

The reflection audit reuses the IVP R11 replicated-property audit (information-visibility-policy story 015) over the marker classes plus every class the module touches (`FTeammateMarkerModel`, the presenter, the leaf wrapper). `ShowTeamNames` is a client-local UI action in `IMC_Common` and is VS-deferred; the audit asserts only that it is in no replicated layout. Audio is covered by story 009's lint.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `information-visibility-policy/story-034-presentation-include-check.md`: the framework-level Presentation check this extends.
- `story-027-leak-audit-marker-objects-and-canaries.md` and `story-028-leak-audit-reflection-and-packet-capture.md`: the runtime leak tests.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- the T0 lint rule file for the marker module (path per tuning-data-config story 014; adds the marker symbol list)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerBansTest.cpp` (names `StealthGame.Markers.IncludeGrepBan`, `StealthGame.Markers.NoReplicatedFields`, `StealthGame.Markers.NoPawnComponentNoCanvas`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: the marker module source. When: the build-time include check and grep run. Then: no server query header and none of the banned symbols appear. Edge cases: each seeded violation fails the check, one per symbol.
- **AC2**: Given: the built module. When: the IVP R11 reflection audit and RPC enumeration run over its classes and every class it touches. Then: no replicated property, RPC, or replicated actor or component, and accent colour, `TeammateMarkerLargeMult` and `ShowTeamNames` are in no replicated layout. Edge cases: a seeded replicated property fails.
- **AC3**: Given: the module source and pawn classes. When: lint runs. Then: no `UWidgetComponent` or other component is added to any pawn, and neither `AddToViewport` nor the `AHUD` canvas is used for markers. Edge cases: a seeded `AddToViewport` fails.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerBansTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-002-marker-model-teamview-source-and-validation.md`; cross-epic `production/epics/information-visibility-policy/story-034-presentation-include-check.md`, `story-015-replicated-property-audit.md`, `production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`, `story-016-lint-seeded-violation-controls.md`
- Unlocks: `production/epics/teammate-markers/story-027-leak-audit-marker-objects-and-canaries.md`
