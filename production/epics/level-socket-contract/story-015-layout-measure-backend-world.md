# Story 015: World measure backend — navmesh paths, nav-grid bound and ViewBlock traces

> **Epic**: Level Socket Contract
> **Status**: Blocked — BLOCKED: spike R4 (story 011) has not reported; the nav-grid bound constant (Risk R-5), agent sizing and graybox navmesh timing it records are inputs here
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; <= 33 ms slices)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: V-* predicates are written once against `ILayoutMeasureBackend`; `FWorldMeasureBackend` measures bounded point-to-point paths with `FindPathSync` and field predicates on a nav-grid (2 m cells projected on the navmesh, octile Dijkstra) using a one-sided bound (octile path never shorter than the string-pulled optimum, at most 8.24% longer) with exact refinement only for in-between pairs, up to `LayoutExactRefineMaxCount`; sight and sun predicates use the `ViewBlock` channel.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `FindPathSync`, `ProjectPointToNavigation` and `NavigationRaycast` are [V]; concurrent Detour queries on a frozen mesh are [U] and not used off the game thread; the 1.0824 bound holds only when the nav-grid's connectivity is a subset of the navmesh's [U] (R-5). The `ViewBlock` trace channel is declared by the camera epic (ADR-0015); until it lands use a project-config channel of the same name.

**Why this story**: It is the single measuring layer under every navmesh-dependent predicate. Read first: R7 preamble ("all path lengths are navmesh paths without traversal"), D2, D3; ADR-0014 sections 5.1-5.3, Risk R-5, Validation Criteria (nav-grid bound).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every V-* predicate exists once, written against `ILayoutMeasureBackend`; every sight test uses the `ViewBlock` channel (ADR-0014, ADR-0015)
- Forbidden: Never fork predicate logic between the grid and world backends; never introduce a second "sight" trace channel (ADR-0014, ADR-0015)
- Guardrail: Validate + Select within about 1.5 s in <= 4 ms GT slices; nav-grid and path matrix <= about 2 MB transient (ADR-0014)

---

## Acceptance Criteria

*From ADR-0014 5.1-5.3 and Validation Criteria, with GDD D2/D3 (revised after QL-STORY-READY: the bound is a pass/fail, not an escape hatch), scoped to this story:*

- [ ] GIVEN the same fixtures run on the grid fixture backend and the world backend, THEN every V-* predicate gives identical pass/fail through one code base (no forked predicate logic) (`Validate.OneCodeBase`)
- [ ] GIVEN 1,000 sampled pairs on the graybox navmesh, THEN every nav-grid / Detour ratio is at least 1.0 and at most `DetourGridBoundMult` (initial 1.0824), pass/fail of every pair matches an all-`FindPathSync` reference, and exceeding `LayoutExactRefineMaxCount` fails `RefineBudgetExceeded` deterministically (same result over 2 runs); if a ratio exceeds the bound this story fails until the constant is re-derived and recorded in the spike 011 report (`Validate.GridBound`)
- [ ] GIVEN a partial path, THEN it fails at any length (D2), and sight, sun and witness traces use only the `ViewBlock` channel (`Validate.PartialPathAndTraceChannel`)

---

## Implementation Notes

*Derived from ADR-0014 sections 5.1-5.4:*

- `ILayoutMeasureBackend`: path length between two points, single-source path-cost field over the 2 m cell lattice, line of sight and sun ray on `ViewBlock`, walkable-cell set, connectivity. `FGridMeasureBackend` (integer grid, used by the Generator's G-PRE) is a fixture backend here; `FWorldMeasureBackend` is the production one.
- Point-to-point predicates with bounded counts (V-REACH, V-SEP path, D6 contact, Selection path matrix, chain legs; a few hundred pairs) use string-pulled `FindPathSync` quantised to cm. Field predicates (V-NAV-1, D3 over about 10^5 pairs, D6b regions, V-ROUTE coverage) use the nav-grid: 8-neighbour edges when `NavigationRaycast` between centres is unobstructed; integer Dijkstra with octile weights 200 / 283 cm.
- A pair whose grid ratio passes a threshold passes exactly; a pair whose ratio exceeds `threshold x 1.0824` fails exactly; only in-between pairs are re-measured. The D3 p95 is bracketed by lower (grid / 1.0824) and upper (grid) values and refined only when the bracket straddles `DetourP95MaxMult`, up to `LayoutExactRefineMaxCount` (default 2,000, registered LSC key); beyond it the attempt fails `RefineBudgetExceeded`.
- All work goes through the story-002 slicing driver (units: rays, relaxations / 64).
- The `ILayoutMeasureBackend` interface and the grid fixture backend are declared in story 014 so the predicate stories (016-019, 021) run headless without this story; this story adds the world implementation.
- Edge: a 1 cm partial path fails; the first 50-seed batch (Generator epic) logs min/max ratios, here use the graybox's 1,000 pairs.

---

## Out of Scope

- Story 012: the navmesh lifecycle itself; stories 016-019: the predicates; story 020: route baking

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/LayoutMeasureBackend.h`
- `Source/StealthGameplay/Private/Core/Layout/WorldMeasureBackend.cpp`
- `Source/StealthGameplay/Private/Core/Layout/GridMeasureBackend.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/MeasureBackendTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision; BLOCKED on story 011 is a dependency, not a criterion defect).*

- **AC-1** `Validate.OneCodeBase`
  - Given: the same fixtures.
  - When: run on the grid backend and the world backend.
  - Then: every V-* predicate gives identical pass/fail via one predicate code base (backend behind an interface).
  - Edge cases: a source lint finds no duplicated predicate bodies.
- **AC-2** `Validate.GridBound`
  - Given: 1,000 sampled pairs on the graybox navmesh.
  - When: nav-grid and Detour path lengths are compared.
  - Then: every ratio in [1.0, 1.0824]; pass/fail matches an all-`FindPathSync` reference; exceeding `LayoutExactRefineMaxCount` (2,000) gives `RefineBudgetExceeded`, identical over 2 runs.
  - Edge cases: a ratio over the bound fails the story until the constant is re-derived in the 011 report.
- **AC-3** `Validate.PartialPathAndTraceChannel`
  - Given: a partial path; sight, sun and witness traces.
  - When: validated; the trace channel is spied.
  - Then: the partial path fails at any length; all those traces use only `ViewBlock`.
  - Edge cases: 1 cm partial path.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Layout/MeasureBackendTest.cpp` — `StealthGame.Layout.Validate.OneCodeBase`, `.GridBound`, `.PartialPathAndTraceChannel` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-011-spike-r4-graybox-navmesh-timing.md` (must report), `production/epics/level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`, `production/epics/level-socket-contract/story-002-service-state-machine-and-time-slicing.md`, `production/epics/level-socket-contract/story-014-socket-records-determinism-required-data.md` (interface and grid backend)
- Cross-epic: `production/epics/camera-system/EPIC.md` (declares the `ViewBlock` channel)
- Unlocks: `production/epics/level-socket-contract/story-017-predicates-navigation-area-sight-route-traversal.md`, `production/epics/level-socket-contract/story-020-crowd-route-prebaked-paths.md`, `production/epics/level-socket-contract/story-021-selection-combos-and-fairness.md`
