# Story 045: Retire the prototype generator

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md` (Overview, R-G7; `production/first-playable-report.md` prototype disposition)
**Requirement**: `TR-citygen-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. Retiring the replicated-seed client generator is what makes "clients run no generator" true.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The prototype's model (clients run the generator from a replicated server seed, dynamic navmesh rebuild) is Alternative 1 and 4 in ADR-0014, rejected: clients build only from the Manifest, the navmesh is built once and frozen, and the only runtime build trigger is the layout service's lock removal.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: No post-cutoff API. Removing `StealthCityGenerator` also removes its references from the prototype game mode and map (`/Game/StealthGame/Maps/L_Prototype`); editing the map is a Content step outside this planning (no `.uasset` change is made by the epic definition), so the dev work does it in the editor and keeps the kit assets untouched. Prototype tests (`StealthGame.Rules.*`, 6) keep running in the per-commit gate until the prototype is retired (ADR-0008 section 10); they are not promoted.

**Control Manifest Rules (this layer)**:
- Required: Round and every feature reach layout only through `UStealthLayoutService`; the only runtime navmesh build trigger is the service's lock removal — source: ADR-0014
- Forbidden: Never use a fully dynamic navmesh; never let clients run the Generator or draw geometry randomness — source: ADR-0014
- Guardrail: the retirement must not break the per-commit gate (the retained prototype tests keep passing or are retired with their code under ADR-0007 section 8)

**Prototype replaced (the whole story)**: `Source/StealthGame/Private/StealthCityGenerator.cpp` and `Public/StealthCityGenerator.h` (428 + 133 lines: a grid city of houses, halls, temple, stalls, gardens and low walls built identically on every machine from a replicated server seed, objectives and bushes moved to seeded sites, dynamic nav rebuild, reachability logged). Kept (assets): the Meshy kit pieces `SM_Kit_House`, `SM_Kit_Temple`, `SM_Kit_Stall` under `/Game/StealthGame/Environment/Kit/` (sources in `assets/environment/kit/`) and the Blender kit export procedure.

---

## Acceptance Criteria

*From the epic's Prototype Disposition and GDD R-G7 (no GDD AC id), scoped to this story:*

- [ ] **AC1** (`Retire.PrototypeGeneratorGone`): **GIVEN** the game module **THEN** `StealthCityGenerator.cpp/.h` and its replicated-seed client build path are removed, and no remaining file references them.
- [ ] **AC2** (`Retire.NoDynamicNavRebuild`): **GIVEN** a round **WHEN** it runs **THEN** the prototype's dynamic navmesh rebuild is not present and the navmesh is built once by the layout service.
- [ ] **AC3** (`Retire.KitAssetsKept`): **GIVEN** the path list `Content/StealthGame/Environment/Kit/SM_Kit_House.uasset`, `SM_Kit_Temple.uasset`, `SM_Kit_Stall.uasset` and the source folder `assets/environment/kit/` **THEN** a content-hash comparison against the pre-story snapshot shows them unchanged.

---

## Implementation Notes

*Derived from `production/first-playable-report.md` and ADR-0014 Alternatives 1 and 4:*

Do this story only when the provider path works end to end (stories 026, 028, 032 Done) and the graybox provider (LSC story 026, 028) supplies `L_Prototype`-equivalent rounds, so the game mode has a layout source before the prototype generator goes. AC1: a T0 lint over source, config and Blueprint/asset references finds 0 hits for `StealthCityGenerator`, `AStealthCityGenerator` and the replicated-seed client build symbols (include Blueprint subclasses and redirectors, and ini references); the module builds clean. AC2 (`StealthGame.CityGen.PIE.Retire.NoDynamicNavRebuild`, CQTest in `StealthTestHarness`): a 2-client round to `Active` shows the navmesh build counter at exactly 1 build, by the layout service, and no dynamic-rebuild call path exists in the Round or Generator sources; the navigation runtime-generation config is not `Dynamic` after hand-off (the service asserts `Dynamic` only during the build window, ADR-0014 4.1). AC3: the project is not a git repository in this environment, so use a hash-manifest snapshot (committed with the story) rather than a git diff; an editor re-save or redirector fix-up changes a `.uasset` hash and counts as a failure (ADR-0009: never re-save binary assets casually).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Retiring the prototype crowd: `crowd-npc-behaviour/story-039-retire-prototype-crowd.md`.
- Retiring prototype settings fields: `tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`.
- The graybox map and provider: `level-socket-contract/story-026-graybox-provider-markers-and-bake.md`, `story-028-graybox-map-authoring.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Private/StealthCityGenerator.cpp` (removed)
- `Source/StealthGame/Public/StealthCityGenerator.h` (removed)
- `Source/StealthGame/Private/StealthGameMode.cpp` (references removed)
- `tools/ci/citygen-prototype-retired-check.ps1` (T0 lint and kit-asset hash check)
- `tools/ci/citygen-kit-assets.sha256` (pre-story snapshot)
- `Source/StealthTestHarness/Private/CityGen/NoDynamicNavRebuildPieTest.cpp` (CQTest PIE)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 and AC2 ADEQUATE; AC3 revised and ADEQUATE).*

- **AC1** `StealthGame.CityGen.Build.Retire.PrototypeGeneratorGone`
  - Given: the game module. Use a T0 lint, because T1 does no file I/O.
  - When: the tree is scanned.
  - Then: `StealthCityGenerator.cpp/.h` are absent and a repo-wide search for the class and the replicated-seed client build symbols returns 0 hits in source, config and Blueprint/asset references. The module builds clean.
  - Edge cases: Blueprint subclasses or redirectors, and ini references.
- **AC2** `StealthGame.CityGen.PIE.Retire.NoDynamicNavRebuild`
  - Given: a 2-client PIE round.
  - When: the round runs to Active.
  - Then: the navmesh build counter shows exactly 1 build, by the layout service, and 0 runtime rebuilds. No dynamic-rebuild call path exists in the Round or Generator sources.
  - Edge cases: navigation runtime-generation config is not dynamic after hand-off.
- **AC3** `StealthGame.CityGen.Build.Retire.KitAssetsKept`
  - Given: a hash manifest of `SM_Kit_House.uasset`, `SM_Kit_Temple.uasset`, `SM_Kit_Stall.uasset` and the recursive hash of `assets/environment/kit/`, snapshotted before the story starts.
  - When: the same hashes are recomputed after the story (T0 script).
  - Then: every hash matches and no file is added or removed under the source folder.
  - Edge cases: the project is not a git repository here, so use a hash manifest, not a git diff. Store the snapshot somewhere committed. An editor re-save or redirector fix-up changes a `.uasset` hash, and that counts as a failure.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the T0 lint script, the T2 PIE test and the hash check OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-026-server-kit-spawn-from-manifest.md`, `story-028-navigation-integration-water-stairs-client-none.md`, `story-032-manifest-fixture-provider.md`; cross-epic `level-socket-contract/story-026-graybox-provider-markers-and-bake.md`, `story-028-graybox-map-authoring.md`
- Unlocks: `story-046-ten-connection-gauntlet-integration-pass.md`
