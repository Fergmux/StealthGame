# Story 012: Navmesh lock lifecycle and freeze

> **Epic**: Level Socket Contract
> **Status**: Blocked — BLOCKED: spike R4 (story 011) has not reported; agent size, cell/tile size and the graybox cook-fallback verdict that this story applies come from it
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
**ADR Decision Summary**: The navmesh is built once per accepted attempt (non-blocking `Rebuild` behind the service's `Custom` lock), then frozen at `OnLayoutReady` by re-adding the lock; nothing spawned afterwards may affect navigation, so the navmesh crowd NPCs walk is byte-for-byte the one Validate measured.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: [V] in 5.8.4: `Build()` blocks and refuses under any non-editor lock; `RemoveNavigationBuildLock(..., Rebuild)` starts an asynchronous full rebuild; while any lock is held the tick skips `RebuildDirtyAreas`; `SetNavigationAutoUpdateEnabled` is editor-only; `IsNavigationDirty()` does not see areas queued under a lock (use `GetNumDirtyAreas()`). The engine removes `InitialLock` on hot-reload, harmless while the `Custom` lock is held. Saved `ARecastNavMesh` actors may override the project generation mode [U].

**Why this story**: This turns the spike's findings into the production lock lifecycle and the freeze rule every later system relies on. Read first: R4 item 3 (service waits for the server navmesh); ADR-0014 sections 4.1-4.5, Risks R-2/R-2b and the 2026-10-24 amendment (config applied, `bInitialBuildingLocked` deferred to this story).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Everything spawned after `OnLayoutReady` has `CanEverAffectNavigation` false on every component; `bAllowClientSideNavigation=false` (ADR-0014)
- Forbidden: Never call `UNavigationSystemV1::Build()` at runtime; no code other than the layout service adds or removes a navigation build lock; never use a fully dynamic navmesh (ADR-0014)
- Guardrail: kit spawn + collision + navmesh <= 3.0 s wall time (graybox navmesh <= 1 s); no navmesh rebuild in `Active` (ADR-0014)

---

## Acceptance Criteria

*From ADR-0014 4.2 and Validation Criteria (Freeze), with GDD R4.3 (the service waits for the server navmesh), scoped to this story:*

- [ ] GIVEN world start, THEN only the service's `Custom` lock gates building (the initial lock is released with `NoRebuild`); the rebuild is started with `RemoveNavigationBuildLock(Custom, Rebuild)`; completion waits for `OnNavigationGenerationFinishedDelegate` and `!IsNavigationBuildInProgress()` and `GetNumRemainingBuildTasks() == 0`, bounded by the attempt watchdog (`NavBuildTimeout`); a source lint finds no `UNavigationSystemV1::Build()` call and no Add/RemoveNavigationBuildLock outside the service (`Nav.LockSequence`)
- [ ] GIVEN a frozen navmesh at `OnLayoutReady`, WHEN 60 s of play with kills, emergences and takeovers runs, THEN the tile-build counter, `GetNumDirtyAreas() == 0` and the navmesh data hash are unchanged to `Ended`, and a reflection test asserts every runtime-spawned class and pawn component has `CanEverAffectNavigation` false (`Nav.FreezeHolds`)
- [ ] GIVEN navigation data whose `RuntimeGeneration` is not `Dynamic`, or a layout map carrying a saved `ARecastNavMesh`, THEN the service fails `NavConfigInvalid` at startup and the round aborts to Lobby with level 3 (`Nav.ConfigInvalid`)

---

## Implementation Notes

*Derived from ADR-0014 4.1-4.5:*

- **Config applied by this story** (Config/ is outside the stories' write scope until now): `[/Script/NavigationSystem.NavigationSystemV1] bInitialBuildingLocked=True`, plus the agent sizing (SCL capsule radius/height, max step 45 cm = `TraversalMinHeightCm`, walkable slope) and `[/Script/NavigationSystem.RecastNavMesh]` cell size, tile size, `bDoFullyAsyncNavDataGathering`, `MaxSimultaneousTileGenerationJobsCount` **from the spike 011 report**. `bAutoCreateNavigationData=True`, `bAllowClientSideNavigation=False` and `RuntimeGeneration=Dynamic` are already applied (2026-10-25 amendment). `L_Prototype`'s per-map `RuntimeGeneration=Dynamic` override is superseded by the project setting.
- Lifecycle: (1) world start: add `Custom`, release initial lock `NoRebuild`; (2) `Building(k)`: geometry registers into the octree, no tile generated; (3) on geometry complete and first collision trace valid: `RemoveNavigationBuildLock(Custom, Rebuild)`; (4) wait for delegate + not in progress + 0 remaining tasks, polled once per tick, bounded by the watchdog; (5) freeze: re-add `Custom`. A failed attempt tears down geometry under the lock and step 3 repeats.
- Freeze discipline: `AStealthPawn` components, corpses, Objectives' server-only actors, Concealment's hide volumes and every runtime actor default `CanEverAffectNavigation` false; non-Shipping `ensure(GetNumDirtyAreas() == 0)` once per second after the freeze.
- Startup assert: default nav data reports `GetRuntimeGenerationMode() == Dynamic` or the service fails `NavConfigInvalid`.
- **Prototype replaced, not promoted:** `AStealthGameMode` waits on `IsNavigationBuildInProgress` in `SpawnCrowdWhenNavReady` and `AStealthNpcController` wanders on a fully dynamic navmesh; both are superseded by the frozen-navmesh contract. The prototype keeps compiling until the Layout epic replaces it; deletion needs user approval (ADR-0014 Migration Plan item 1). Applying `bInitialBuildingLocked=True` stops prototype crowd navigation, so land it together with this lifecycle.
- Removing a saved `ARecastNavMesh` from a map needs the editor (do not byte-edit `.umap`); track under `production/session-state/active.md` "Needs follow-up (editor)".

---

## Out of Scope

- Story 011: measurement spike
- Story 015: path/sight measurement on the frozen navmesh
- Story 028: authoring the graybox map's `ANavMeshBoundsVolume` and removing its saved nav actor

---

## Files

- `Source/StealthGameplay/Private/Core/LayoutNavLifecycle.cpp`, `LayoutNavLifecycle.h`
- `Source/StealthGameplay/Private/Core/StealthLayoutService.cpp`
- `Config/DefaultEngine.ini` (nav keys from spike 011)
- `Source/StealthGameplay/Private/Tests/Layout/NavLifecycleTest.cpp`
- `Source/StealthTestHarness/Private/NavFreezeTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE; BLOCKED on story 011 is a dependency, not a criterion defect).*

- **AC-1** `Nav.LockSequence`
  - Given: world start with the service's `Custom` lock and an instrumented navigation system.
  - When: the lifecycle runs to `OnLayoutReady`.
  - Then: only the `Custom` lock gates building (initial lock released with `NoRebuild`); rebuild starts via `RemoveNavigationBuildLock(Custom, Rebuild)`; completion waits for `OnNavigationGenerationFinishedDelegate` and `!IsNavigationBuildInProgress` and `GetNumRemainingBuildTasks() == 0`, bounded by the attempt watchdog; the source lint finds 0 `UNavigationSystemV1::Build()` calls and 0 Add/RemoveNavigationBuildLock outside the service.
  - Edge cases: a never-finishing build yields `NavBuildTimeout`. Split the four claims into separate asserts.
- **AC-2** `Nav.FreezeHolds`
  - Given: a frozen navmesh at `OnLayoutReady`.
  - When: 60 s of fixed-step play with kills, emergences and takeovers.
  - Then: tile-build counter, `GetNumDirtyAreas() == 0` and the navmesh data hash are unchanged to `Ended`; every runtime-spawned class and pawn component has `CanEverAffectNavigation` false.
  - Edge cases: define the navmesh data hash in the fixture; the class list comes from a registered list of spawned classes.
- **AC-3** `Nav.ConfigInvalid` (proposed)
  - Given: nav data with `RuntimeGeneration != Dynamic`; separately a map with a saved `ARecastNavMesh`.
  - When: the service starts.
  - Then: it fails `NavConfigInvalid` and the round aborts to Lobby with `LayoutFallbackLevel` 3.
  - Edge cases: both conditions together give one abort.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `NavLifecycleTest.cpp` and `NavFreezeTest.cpp` — `StealthGame.Layout.Nav.LockSequence`, `.FreezeHolds`, `.ConfigInvalid` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-011-spike-r4-graybox-navmesh-timing.md` (must report), `production/epics/level-socket-contract/story-003-attempt-loop-timeout-budget-reseed.md`
- Cross-epic: `production/epics/shared-character-locomotion/EPIC.md` (agent capsule, `TraversalMinHeightCm`)
- Unlocks: `production/epics/level-socket-contract/story-015-layout-measure-backend-world.md`, `production/epics/level-socket-contract/story-020-crowd-route-prebaked-paths.md`
