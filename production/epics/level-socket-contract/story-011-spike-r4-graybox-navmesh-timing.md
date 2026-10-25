# Story 011: Spike R4 — graybox runtime navmesh timing and freeze counters

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; <= 33 ms slices)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The navmesh is built once per accepted attempt on the server (Recast `Dynamic` generation behind a build lock, non-blocking `Rebuild`, then frozen by a `Custom` lock). Spike R4 measures the build and sets agent size, cell size, tile size and async-gathering values; the graybox has a documented fallback (cook its navmesh) if it exceeds 1 s.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Navigation has no 5.6-5.8 breaking change listed in VERSION.md; `GetNumDirtyAreas()`, `IsNavigationBuildInProgress()`, `GetNumRemainingBuildTasks()` and `OnNavigationGenerationFinishedDelegate` are [V] in 5.8.4 source (`NavigationSystem.h`). `IsNavigationDirty()` misses areas queued under a lock, so the spike must read `GetNumDirtyAreas()`.

**Why this story**: This is the "navmesh-build spike R4" in the epic's Build Order and Gates. Stories 012 (lifecycle and freeze) and 015 (world measure backend) cannot fix their numbers until it reports. Read first: AC-36; ADR-0014 section 4 (4.1 configuration, 4.2 lifecycle, 4.3 graybox) and Risks R-1, R-2, R-5.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The only runtime navmesh build trigger is `RemoveNavigationBuildLock(ENavigationBuildLock::Custom, ELockRemovalRebuildAction::Rebuild)` by the layout service (ADR-0014)
- Forbidden: Never call `UNavigationSystemV1::Build()` at runtime (blocks the game thread) (ADR-0014)
- Guardrail: Graybox navmesh <= 1 s; game-thread frames <= 33 ms during Preparing; no navmesh rebuild in Active (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-36 (advisory half) and ADR-0014 section 4 / Risks R-1, R-2:*

- [ ] GIVEN the graybox on the listen host, WHEN Preparing runs the runtime navmesh build (median of 3), THEN build time is at most 1 s (target under 0.1 s layout time) and no game-thread frame exceeds 33 ms (`Network.PreparingNoHitch`, advisory half; name the reference machine)
- [ ] THEN the report records `GetNumDirtyAreas()` and the tile-build counter at each lifecycle step (world start, lock release, rebuild start, completion, `OnLayoutReady`, `Ended`) and the recommended agent size, cell size, tile size, async-gathering setting and `MaxSimultaneousTileGenerationJobsCount`
- [ ] THEN the report records the verdict on the cooked-graybox-navmesh fallback of ADR-0014 4.3 (needed or not needed), citing the first criterion's numbers

---

## Implementation Notes

- Use a graybox-sized test map (a copy of `/Game/StealthGame/Maps/L_Prototype` is enough; the real graybox is story 028) with no saved `ARecastNavMesh` actor and one `ANavMeshBoundsVolume`.
- Apply the lock sequence of ADR-0014 4.2 in throwaway spike code (the production version is story 012): add the `Custom` lock, release the initial lock with `NoRebuild`, spawn geometry, then `RemoveNavigationBuildLock(Custom, Rebuild)`, wait for the delegate plus `!IsNavigationBuildInProgress()` plus `GetNumRemainingBuildTasks() == 0`, re-add the lock.
- Note that `bInitialBuildingLocked=True` is deferred in `Config/DefaultEngine.ini` (ADR-0014 amendment) because it freezes the prototype's crowd navigation until the lifecycle exists; the spike sets it per-run via command line or a spike-only config.
- Record the nav-grid / Detour ratio sample (min and max over 1,000 pairs) as an input to story 015 (risk R-5).
- Output: `production/qa/spikes/lsc-r4-graybox-navmesh.md` (machine, cell/tile values tried, timings, counters, verdict). A failed spike is handled by a superseding ADR, not by editing ADR-0014.

---

## Out of Scope

- Story 012: the production lock lifecycle, freeze enforcement and config changes
- The generated-layout 3.0 s / 5,000-instance half of R4 (Generator epic, ADR-0022)

---

## Files

- `Source/StealthGameplay/Private/Spikes/LayoutNavSpike.cpp` (throwaway)
- `Source/StealthGameplay/Private/Tests/Layout/NavTimingAdvisoryTest.cpp`
- `production/qa/spikes/lsc-r4-graybox-navmesh.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Network.PreparingNoHitch` (advisory half)
  - Given: graybox on a listen host; the named reference machine.
  - When: Preparing runs the runtime navmesh build, median of 3.
  - Then: build time <= 1 s (target < 0.1 s layout time) and no game-thread frame > 33 ms.
  - Edge cases: the report must name the reference machine; cold vs warm run.
- **AC-2** spike report data (manual check)
  - Setup: instrument world start, lock release, rebuild start, completion, `OnLayoutReady` and `Ended`.
  - Verify: `GetNumDirtyAreas()` and the tile-build counter are recorded at each step, plus recommended agent size, cell size, tile size, async gathering and `MaxSimultaneousTileGenerationJobsCount`.
  - Pass condition: all 6 steps and 5 values are present. Confirm the API names against the 5.8.4 source.
- **AC-3** fallback verdict (manual check)
  - Verify: the report says "cooked graybox navmesh fallback (ADR-0014 4.3) needed" or "not needed".
  - Pass condition: the verdict cites the AC-1 numbers.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `NavTimingAdvisoryTest.cpp` (`StealthGame.Layout.Network.PreparingNoHitch`, advisory) plus the spike report `production/qa/spikes/lsc-r4-graybox-navmesh.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-001-service-skeleton-provider-registry.md`
- Unlocks: `production/epics/level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`, `production/epics/level-socket-contract/story-015-layout-measure-backend-world.md`
