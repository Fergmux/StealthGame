# Story 026: Server kit spawn from the Manifest

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0022 for the Nanite/Lumen budgets the kit lives in)
**ADR Decision Summary**: Server and clients run the same builder, `FLayoutGeometryBuilder`, turning Manifest entries into a non-replicated local actor `AStealthLayoutGeometry`: one `UInstancedStaticMeshComponent` per (kit mesh asset x spatial quarter), batch `AddInstances` in local space on an identity-transform actor with `bUpdateNavigation` false before `RegisterComponent` where possible, Static mobility once the last instance is added, per-module simple collision (<= 4 primitives, stacks <= 2, slabs 1 box), never one actor per module. The Generator on the server spawns exactly the Manifest it emitted, so the server validates what clients will build.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; rendering items inherit HIGH through spike R1, owned by ADR-0022)
**Engine Notes**: `UInstancedStaticMeshComponent::AddInstances(Transforms, bShouldReturnIndices, bWorldSpace, bUpdateNavigation)` is [V] in 5.8.4 (`InstancedStaticMeshComponent.h:275`); `AddInstances` before registration defers the navigation update to `OnRegister`; `AddInstancesById` is Preliminary and not used; HISM is not used (Nanite) [U, spike R1]. Runtime-created Static ISM components with Lumen/VSM caching are [U] until R1 reports (ADR-0014 verification item 5): this story implements the ADR as written; the R1 report may force the merged-run-box collision fallback, which is data in `GeneratorKitVersion`. No `.uasset` is created by this story.

**Control Manifest Rules (this layer)**:
- Required: Kit instances are added with batch `AddInstances(..., bUpdateNavigation = false)` before component registration where possible; `AStealthLayoutGeometry` and every layout-derived component are `bReplicates = false`, created locally — source: ADR-0014
- Forbidden: Never add kit instances with per-instance `AddInstance` loops or one actor per module; never use complex-as-simple collision; never spawn a replicated actor from the layout contract — source: ADR-0014
- Guardrail: <= 5,000 instances, about 48 ISM components, <= 20,000 collision shapes; server kit spawn + collision + navmesh <= 3.0 s (measured in story 043) — source: ADR-0014

This story is the Generator-specific half of assembly: it feeds the shared LSC builder with the kit mesh table and proves the server-side rules. The client side and decoration are LSC story 013 and story 029 here.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G7, R-G9) and ADR-0014 sections 3.1-3.2, scoped to this story:*

- [ ] **AC1** (R-G7 / ADR-0014 3.2, `Assembly.ServerBuilderUsage`): **GIVEN** an accepted candidate **THEN** the server spawns exactly its emitted Manifest through the LSC `FLayoutGeometryBuilder`: component count equals the number of distinct (mesh, non-empty spatial quarter) pairs in the Manifest (about 48 at the current kit); the first slice of each component is added before `RegisterComponent` and later slices after it; every slice is one batch `AddInstances` call in local space on an identity-transform actor with `bUpdateNavigation` false; mobility is Static; `CanEverAffectNavigation` is true on server components.
- [ ] **AC2** (R-G7 collision, `Assembly.CollisionStrategy`): **GIVEN** the kit **THEN** per-module simple collision is used (modules <= 4 primitives, stacks <= 2, slabs 1 box) on the pawn and `ViewBlock` channels, complex-as-simple is absent, and the collision strategy is part of `GeneratorKitVersion`.
- [ ] **AC3** (R-G7, `Assembly.NoReplicatedActors`): **GIVEN** a spawned layout **THEN** `AStealthLayoutGeometry` and every component have `bReplicates` false, no per-instance `AddInstance` loop or one-actor-per-module spawn exists on the path (lint), and the listen host builds the kit once.

---

## Implementation Notes

*Derived from ADR-0014 sections 3.1-3.4 and 9.2:*

The Generator adds only what is Generator-specific: the kit mesh table (module id and variant -> `UStaticMesh` asset, resolved on the game thread at `BeginAttempt`, story 025) and the spatial-quarter assignment. Everything else is the LSC builder (`level-socket-contract/story-013-client-geometry-builder-and-decoration.md`) used on the server: spawn `AStealthLayoutGeometry` at the identity transform (layout origin = world origin), add instances in local space with one `AddInstances(Transforms, false, false, false)` call per component per slice, all before `RegisterComponent()` where the slice plan allows (the first slice of a component), set `Static` mobility before registration once the last instance of the attempt is added, `SetCanEverAffectNavigation(true)` on server components (false on client ones). Per-module simple collision on the pawn and `ViewBlock` channels; complex-as-simple is forbidden (art 8.x.10, shape cap <= 20,000); the collision strategy is part of `GeneratorKitVersion` and its fallback if R4 fails is merged run boxes derived from the Manifest on both sides. The listen host builds the kit once (its server geometry is also its view); its client path recomputes `LayoutHash` and builds only decoration (story 029). Server spawn is sliced on the game thread by `LayoutSliceWorkUnitsLimit` (R-G9: frames <= 33 ms). Run AC1 on at least 3 seeds so the expected component count varies; a quarter with 0 instances yields no component, and a single-slice component has only the pre-register call.

Prototype replaced: `StealthCityGenerator` spawned one `StaticMeshComponent`/actor per module and rebuilt the navmesh dynamically; both are replaced (story 045 retires the code).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- LSC `FLayoutGeometryBuilder` itself (shared with clients): `level-socket-contract/story-013-client-geometry-builder-and-decoration.md`.
- `story-027-instance-entry-and-shape-budget-audit.md`: the 1,000-seed budget audit (blocked on R1).
- `story-028-navigation-integration-water-stairs-client-none.md`: navmesh, water and stairs.
- `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`: server spawn + nav time and GPU cost.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenKitMeshTable.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/StealthCityGeneratorProvider.cpp` (spawn slicing, `OnGeometryReady`)
- `Source/StealthTestHarness/Private/CityGen/NoReplicatedActorsHostPieTest.cpp` (CQTest PIE, T2)
- `Source/StealthGameplay/Private/Tests/CityGen/ServerBuilderUsageTest.cpp` (class `FCityGenAssemblyServerBuilderUsageTest`, name `StealthGame.CityGen.Assembly.ServerBuilderUsage`)
- `Source/StealthGameplay/Private/Tests/CityGen/CollisionStrategyTest.cpp` (class `FCityGenAssemblyCollisionStrategyTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/NoReplicatedActorsTest.cpp` (class `FCityGenAssemblyNoReplicatedActorsTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC2 and AC3 ADEQUATE; AC1 revised and ADEQUATE).*

- **AC1** `Assembly.ServerBuilderUsage`
  - Given: a server-built kit on a golden seed in an editor world. Compute expected = distinct (mesh, non-empty quarter) pairs from the Manifest.
  - When: components are enumerated and the builder's `AddInstances` and `RegisterComponent` calls are traced.
  - Then: component count == expected. Each component's first slice is added before `RegisterComponent` and later slices after it. Every slice is exactly 1 batch `AddInstances` call, in local space, on an identity-transform actor with `bUpdateNavigation` false. Mobility is Static and `CanEverAffectNavigation` is true.
  - Edge cases: a quarter with 0 instances yields no component. A single-slice component has only the pre-register call. Run on >= 3 seeds so the expected count varies.
- **AC2** `Assembly.CollisionStrategy`
  - Given: a server-built kit on a golden seed in an editor world.
  - When: body setups are enumerated.
  - Then: modules have <= 4 simple primitives, stacks <= 2, and slabs exactly 1 box. No body uses complex-as-simple. Pawn and `ViewBlock` responses are Block. Changing the collision-strategy constant changes `GeneratorKitVersion`.
  - Edge cases: a 3-module stack is still <= 2 primitives. A zero-primitive module fails.
- **AC3** `Assembly.NoReplicatedActors`
  - Given: a spawned layout, plus a 2-client PIE for the host check.
  - When: actors and components are enumerated, and the Assembly sources are linted (T0).
  - Then: `AStealthLayoutGeometry` and every component have `bReplicates` false. The lint finds no per-instance `AddInstance(` loop and no one-actor-per-module spawn. The host's kit-build counter == 1.
  - Edge cases: the lint needs a named rule ID.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` (editor-world fixture; the host check is a CQTest PIE test in `StealthTestHarness`, name `StealthGame.CityGen.PIE.NoReplicatedActors.HostBuildsKitOnce`) OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-019-placement-bay-stacks-and-manifest-emission.md`, `story-024-worker-task-cancellation-and-state-machine.md`; cross-epic `level-socket-contract/story-013-client-geometry-builder-and-decoration.md`, `story-012-navmesh-lifecycle-and-freeze.md`
- Unlocks: `story-025-failure-modes-and-teardown.md`, `story-027-instance-entry-and-shape-budget-audit.md`, `story-028-navigation-integration-water-stairs-client-none.md`, `story-043-kit-gpu-cost-and-server-spawn-nav-time.md`
