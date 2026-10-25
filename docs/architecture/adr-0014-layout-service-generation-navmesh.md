# ADR-0014: Layout service, seeded generation and runtime navmesh

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed (awaiting user acceptance)

## Date
2026-10-24

## Last Verified
2026-10-24 (against UE 5.8.4 headers at `C:/Program Files/Epic Games/UE_5.8/Engine/Source`, the 2026-10-24 revised `level-socket-contract.md` and `procedural-city-generator.md`, and ADR-0001..0010 as present on disk)

## Decision Makers
Design pipeline (autonomous run, user away) with technical-director (TD-ADR) and unreal-specialist validation; user acceptance pending.

**Review record (2026-10-24):** TD-ADR **CONCERNS** + engine check **MINOR NOTES** (one combined gate). Revised in place: (1) the replicated-array "2,048-element cap" evidence was wrong — `DefaultMaxRepArraySize/Memory` are unused constants in 5.8.4; the real wire limit is the 64 KB constructed partial-bunch cap (§8.2, Alternative 5); (2) the freeze check now watches `GetNumDirtyAreas()`, because `IsNavigationDirty()` does not see areas queued while locked (§4.2); (3) saved `ARecastNavMesh` actors in layout maps are removed/reset and the service asserts `Dynamic` at startup (§4.1); (4) the listen host builds the kit once (§3.1); (5) instances are added in local space on an identity-transform actor (§3.2); (6) the service also refuses to start without authority in `OnWorldBeginPlay`, and a cook test covers editor-module marker classes (§1, §6). The engine lock lifecycle of §4.2 was confirmed in source. Cross-document consistency was not re-run by the gate (turn limit); the author checked ADR-0005/0006/0007/0010 and both GDDs while drafting.

## Summary
Every round needs a city that is identical on every machine, validated on the server's own navmesh, and free of any role data, whether it comes from the hand-built graybox or the Procedural City Generator. This ADR fixes the server-only layout service (`UStealthLayoutService`, a tickable world subsystem with the consumer-owned `ILayoutProvider` interface), the chunked integer `FLayoutManifest` replicated on the GameState with its FNV-1a `LayoutHash`, a navmesh built **once** per accepted attempt on the server (Recast `Dynamic` runtime generation behind the initial build lock, non-blocking rebuild, then frozen by a custom build lock at `OnLayoutReady`), time-sliced validation and selection, the graybox socket-data bake, and the Generator's ISM-per-module kit assembly with its pure-data stages on one worker task.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Navigation (primary); Core (subsystems, threading); Networking (Manifest replication); Rendering (ISM/Nanite kit) |
| **Layer** | Core (the layout service, the graybox provider and the client Manifest builder); the City Generator provider it hosts is Feature (architecture.md layer map, systems-index #8 and #16) |
| **Knowledge Risk** | MEDIUM (Navigation; VERSION.md lists no 5.6-5.8 navigation breaking change; the 5.8 change is per-mesh nav areas, not adopted here). Rendering items (ISM + Nanite at 5,000 instances) inherit HIGH risk through spike R1 and are owned by ADR-0022 |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/navigation.md` (5.8 block), `modules/rendering.md`, `modules/networking.md`, `breaking-changes.md`, `deprecated-apis.md`, `current-best-practices.md` (ISM/HISM); engine headers listed under "Verified engine facts" |
| **Post-Cutoff APIs Used** | None introduced after 5.3 is load-bearing. Used APIs that exist pre-cutoff but were re-verified in 5.8.4: `ENavigationBuildLock` / `AddNavigationBuildLock` / `RemoveNavigationBuildLock(…, ELockRemovalRebuildAction)`, `bInitialBuildingLocked`, `ERuntimeGenerationType::Dynamic`, `OnNavigationGenerationFinishedDelegate`, `IsNavigationBuildInProgress`, `UInstancedStaticMeshComponent::AddInstances(…, bUpdateNavigation)`, `UTickableWorldSubsystem`, `UE::Tasks::Launch`, `GetNumDirtyAreas`, `net.MaxConstructedPartialBunchSizeBytes` |
| **Verification Required** | (1) Spike R4: server kit spawn + collision + navmesh build ≤ 3.0 s on the listen host for a 5,000-instance golden seed, graybox build ≤ 1 s; (2) freeze holds: tile-build counter and dirty-area count unchanged from `OnLayoutReady` to `Ended` under kills, emergences and takeovers (AC-G36); (3) chunked Manifest arrives intact under 10 % loss / 200 ms (LSC AC-34), wire size measured; (4) ISM nav export of per-instance simple collision produces the same navmesh as equivalent static actors (spot test on the graybox kit); (5) runtime-created ISM components at Static mobility are accepted by Lumen/VSM caching without a warning (spike R1, ADR-0022); (6) `bAllowClientSideNavigation=false` leaves clients with no navigation data (AC-G13) |

### Verified engine facts (5.8.4 source)

| Claim | Evidence |
|---|---|
| `UNavigationSystemV1::Build()` **blocks** the calling thread until every nav data finishes (`EnsureBuildCompletion`) and refuses while any non-editor build lock is set | [V] `NavigationSystem/Private/NavigationSystem.cpp:4441-4500` |
| `RemoveNavigationBuildLock(Flags, ELockRemovalRebuildAction::Rebuild)` calls the protected `RebuildAll()` (asynchronous, tick-driven tile generation) when the last lock is removed; `ReleaseInitialBuildingLock()` removes `InitialLock` with the default `Rebuild` action | [V] `NavigationSystem.cpp:4814-4836, 4094-4097`; `NavigationSystem.h:1146-1153, 901` |
| While any build lock is set the navigation tick skips `RebuildDirtyAreas` and invoker updates | [V] `NavigationSystem.cpp:1676-1690` |
| `ENavigationBuildLock::{NoUpdateInEditor, NoUpdateInPIE, InitialLock, Custom, AsyncLoadLock}`; `bInitialBuildingLocked` is a config `UPROPERTY` on `UNavigationSystemV1` | [V] `NavigationSystem.h:102-112, 362` |
| `SetNavigationAutoUpdateEnabled` exists only `#if WITH_EDITOR` (not usable in a packaged game) | [V] `NavigationSystem.h:1161-1163` |
| `ANavigationData::SetRebuildingSuspended(true)` caches dirty areas and may trigger a full `RebuildAll()` on resume if the cached count exceeds `ai.navigation.MaxDirtyAreasCountWhileSuspended` | [V] `NavigationData.h:717-729` |
| `ERuntimeGenerationType { Static, DynamicModifiersOnly, Dynamic, LegacyGeneration }`; per-NavData `RuntimeGeneration` | [V] `NavigationData.h:523-531, 583` |
| `bAllowClientSideNavigation` config flag ("if false, will not create nav collision when connecting as a client") | [V] `NavigationSystem.h:329-331, 781-782` |
| `OnNavigationGenerationFinishedDelegate`, `IsNavigationBuildInProgress()`, `GetNumRemainingBuildTasks()`, `IsNavigationDirty()`, `FindPathSync`, `FindPathAsync`, `ProjectPointToNavigation`, `NavigationRaycast` | [V] `NavigationSystem.h:444, 1101, 1106, 809, 650-679, 712-718, 548` |
| `UInstancedStaticMeshComponent::AddInstances(const TArray<FTransform>&, bool bShouldReturnIndices, bool bWorldSpace = false, bool bUpdateNavigation = true)` and `AddInstancesById` | [V] `Engine/Classes/Components/InstancedStaticMeshComponent.h:275, 294` |
| `UActorComponent::SetCanEverAffectNavigation(bool)` | [V] `ActorComponent.h:1396` |
| `UTickableWorldSubsystem : UWorldSubsystem, FTickableGameObject` | [V] `Engine/Public/Subsystems/WorldSubsystem.h:80-91` |
| `UNetworkSettings::DefaultMaxRepArraySize/Memory` (2,048 / 65,535) are declared but **not enforced** anywhere in 5.8.4; the legacy path writes array counts as `uint16`; the binding wire limit is `net.MaxConstructedPartialBunchSizeBytes` = 64 KB per constructed bunch, and a bunch split into ≥ `net.PartialBunchReliableThreshold` (8) partials is sent reliable | [V] `NetworkSettings.h:32-36` (no other reference in `Engine/Source`); `RepLayout.cpp:2805, 3680, 4057` (TD gate); `DataChannel.cpp:92-96, 104-108` |
| `IsNavigationDirty()` does not report dirty areas queued while a build lock is held in game worlds; `GetNumDirtyAreas()` does; queued areas are appended without a cap while locked | [V] `NavigationSystem.cpp:5697-5720`; `NavigationDirtyAreasController.h:95`, `.cpp:226-229`; `NavigationSystem.h:977` (TD gate) |
| `InitialLock` is added at nav-system init when `bInitialBuildingLocked` is set and is removed by the engine only in editor run mode or on hot-reload; nothing else in a game world calls `ReleaseInitialBuildingLock`; while any lock is held the tick skips both `RebuildDirtyAreas` and the async tile builder (octree updates still run) | [V] `NavigationSystem.cpp:1171-1178, 1391, 1453, 5186, 1660-1700` (TD gate) |
| `AddInstances` before registration defers the navigation update to `OnRegister`; navigation export is per instance; `bWorldSpace=true` converts through the possibly stale component transform before registration; `AddInstancesById` is marked Preliminary | [V] `InstancedStaticMesh.cpp:3729-3734, 3752, 5442-5526`; `InstancedStaticMeshComponent.h:287-294` (TD gate) |
| `AActor::bIsEditorOnlyActor` | [V] `Actor.h:563` |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0006 (seeds: `PublicSubSeed(Layout, Attempt)`, `GetServerSubSeed` domains 2, 4, 7, 8, `FIntStream`, `FSecretStream`, `FPublicOnlyScope`); ADR-0005 (`FRoundParams`, `FRoundReadiness`, the single readiness acknowledgement, `RequestFallbackTravel`, per-round seamless travel to a fresh world); ADR-0007 (module and header placement, `ILayoutProvider` in `StealthGameplay/Public/Server/Core/`, composition-root registration); ADR-0004 (frozen server tuning, `TuningHash` feeding `LayoutParamsHash`); ADR-0003 (replicated-property allowlist row for the Manifest; zero replicated layout actors); ADR-0008 (test placement, Generator goldens, multi-client harness). None is Accepted yet; this ADR cannot be Accepted before them. |
| **Enables** | ADR-0016 (Crowd: `CrowdRoute`, emergence API, frozen navmesh), ADR-0017 (Concealment: `BushSite` built from the Manifest), ADR-0019 (Informants: `PickRespawnPawn`), ADR-0020 (Objectives: court binding from `FSiteAssignment` at `OnLayoutReady`), ADR-0022 (kit rendering budgets consume this ADR's instance and component plan) |
| **Blocks** | Layout Service + Graybox epic (Round `Preparing` stories, graybox authoring tool, CI `Graybox.Validates`); Procedural City Generator epic; Crowd navigation stories that assume a frozen navmesh |
| **Ordering Note** | Shares the project's sight trace channel `ViewBlock` with ADR-0015 (written in the same batch; ADR-0015 declares the channel, this ADR uses it for LSC sight predicates so "what blocks sight" has one definition). Spike R4 and the Manifest wire spike must report before Acceptance; the Generator half (§9) may be accepted later than the service half without reopening §1-§8. |

## Context

### Problem Statement
The Level Socket Contract (LSC) and the Procedural City Generator GDDs leave seven implementation choices to an ADR ("-> ADR"): the layout service's engine shape (LSC R4.1), how the server builds and holds its navmesh (LSC R4.3, Generator R-G7, Crowd CNB-R15/CR-18), how the integer Manifest crosses the network (LSC R4.4, OQ2), how the graybox's server-only authoring markers stay out of the client build (LSC R11.2, OQ1, AC-38), how the reseed/fallback travel works (LSC R8.3, OQ3), where Generator work runs (R-G7 "off the game thread", R-G9 33 ms frames) and the kit instancing plan (R-G7, TR-citygen-004). The prototype answered several of these in ways the design now forbids, so the decision must be made before the first Core story (Round `Preparing` cannot be built without the service; Crowd cannot be built without the navmesh contract).

**What the prototype proved and what this ADR changes.** `AStealthCityGenerator` (prototype, `Source/StealthGame`, evidence only) built a seeded grid city with ISM box-kit placeholders and the Meshy house/temple/stall kit, ran a navmesh reachability check (`ProjectPointToNavigation` + `FindPathToLocationSynchronously`), used `RecastNavMesh RuntimeGeneration=Dynamic` in `L_Prototype`, and produced an identical layout on 3 and later 10 PIE instances. That establishes feasibility of runtime ISM assembly, runtime Recast generation and seeded determinism on one build. It differs from this design in five ways this ADR replaces: (1) every machine ran the generator itself from `FRandomStream` — now only the server generates and clients build from the Manifest with no RNG (LSC R2.2); (2) the navmesh rebuilt dynamically whenever objectives or other actors carved it — now it is built once per accepted attempt and frozen at `OnLayoutReady` (Crowd CR-18); (3) objectives and bushes were replicated actors relocated by seed — now the contract spawns zero replicated actors and roles are server-secret (LSC R2.5-2.6, R9); (4) instances were added one at a time with per-call navigation updates — now batch `AddInstances` with navigation updates off; (5) the reachability check was a log line — now validation decides acceptance, reseed and fallback (LSC R7-R8).

### Constraints
- **Pillar 1 / secrecy:** the Manifest carries geometry only; roles, socket records, sub-seeds and the selection are server-only (LSC R1, R2.5, R4.4); the public seed drives geometry only (ADR-0006).
- **Determinism:** same (Seed, GeneratorKitVersion, LayoutParamsHash) gives the same socket set and Manifest bytes everywhere and on every compiler (LSC R2.4, Generator R-G11); clients run no geometry RNG.
- **Listen server:** the host process is also a client, and every install can host (AQ-14, ADR-0007 §7): "server-only data absent from the client build" cannot be literally true until the Dedicated Server (ADR-0024).
- **Frame budget:** no game-thread frame over 33 ms during `Preparing` on host or clients (LSC R4.3, AC-36); steady server AI + nav ≤ 1.5 ms (Crowd F5).
- **Time budget:** each attempt ≤ `LayoutAttemptTimeoutSec` (7 s), all attempts ≤ `LayoutTotalBudgetSec` (45 s); server spawn + nav ≤ 3.0 s; client build ≤ 3.0 s p95 (LSC R4.3, Generator R-G9/F5).
- **Zero replicated actors** from the contract (LSC R2.6, ADR-0003).
- **Team size:** 1-2 programmers; one code path for both providers wherever possible.
- **Write scope of this run:** project config (`Config/`), `.uproject` and sources are not edited here; the config this ADR requires is listed for the implementing story.

### Requirements
- A server-only service with which exactly two provider kinds register; Round talks only to the service; Round and the Generator never reference each other (LSC R4.1, AC-3).
- `RequestLayout(Seed, TuningHash)` → build → server navmesh → Validate (V-*) → Select (R9) → publish Manifest → `FRoundReadiness` → `OnLayoutReady`, with the LSC state table (Idle … Aborted) and its out-of-state rejections.
- Reseed up to `LayoutMaxReseedAttemptsCount`, then fallback travel or abort, with attempt records for the Event Log header (LSC R8).
- The navmesh Validate measured is the navmesh Crowd walks for the whole round (Crowd CNB-R15, Generator R-G7, AC-G36).
- Runtime services: `RequestStartPlacement`, `RequestEmergence`, `PickRespawnPawn`, server queries for sites, zones, routes and bushes (LSC R5, R5b, R6, R4.7).
- Clients build geometry and `BushSite` records from the Manifest only and recompute `LayoutHash` before acknowledging (LSC R4.5, R2.6).

## Decision

### 1. The layout service: `UStealthLayoutService`

A **`UTickableWorldSubsystem`** in `StealthGameplay` (`Public/Server/Core/StealthLayoutService.h`, `NotBlueprintType, NotBlueprintable`, ADR-0007 §3), created only where the world has authority: `ShouldCreateSubsystem` returns false for `NM_Client` worlds and for non-game worlds, and `OnWorldBeginPlay` re-checks authority (net mode may not be final when subsystems are created during seamless travel [U]) and leaves the service inert without it; `Tick` runs only while the service is in a working state (`GetTickableTickType` conditional). It owns a plain-C++ core, **`FLayoutServiceCore`** (state machine, attempt records, validation and selection drivers, runtime emergence/respawn bookkeeping), which takes its world-facing needs through injected interfaces (`ILayoutMeasureBackend`, `ILayoutClock`, `ILayoutPawnQuery`), so the state table, timeouts, reseed and fallback logic run headless in unit tests (ADR-0007 §5, ADR-0008 T1).

- **Registration.** The composition root (`AStealthGameMode::InitGame` / its `OnWorldBeginPlay` handler, ADR-0007 §4) registers the Graybox provider (Core) and, from the Generator milestone, the City Generator provider (Feature) through `RegisterProvider(ILayoutProvider&)`, held weakly. `LayoutProviderMode` (`Graybox` | `Generated` | `GeneratedWithFallback`, TDC key) selects which provider builds; a mode naming an unregistered provider fails `NoProvider` at `RequestLayout` (LSC AC-43).
- **States.** Exactly the LSC States table: `Idle → Building(k) → Validating(k) → Selecting(k) → Published → Live → Released`, with `AttemptFailed(k)`, `FallbackTravel` and `Aborted` (from any state). `RequestStartPlacement` is served only in `Published`; `RequestEmergence` and `PickRespawnPawn` only in `Live`; any other request is rejected with `OutOfState` and logged (LSC AC-29). The service enters `Live` on Round's `OnRoundActive` and `Released` on `OnRoundEnded`; per-round travel (ADR-0005) gives every round a fresh world, so `Released → Idle` happens by world teardown, not by in-place reset.
- **Watchdog.** `ILayoutClock` supplies server time; each attempt is cancelled at `LayoutAttemptTimeoutSec` and the whole sequence at `LayoutTotalBudgetSec`. A cancelled attempt tears down everything it created (§3.4). The Generator's own candidate loop is bounded by work units, never wall-clock (R-G5.5); the watchdog bounds only the attempt.
- **Provider calls run inside `FPublicOnlyScope`** (ADR-0006 §3; `thread_local`, so it also covers the Generator's worker task, which opens its own scope, §9.2). Selection, placement, emergence and respawn draws happen outside that scope, on the game thread, from `GetServerSubSeed`.

### 2. Providers and the build sink

`ILayoutProvider` is consumer-owned by Core (ADR-0007 §4). A provider produces a **candidate** (socket set + Manifest entries, or the graybox's baked records + hash) and spawns its geometry on the server; the service owns everything after that (navmesh, Validate, Select, publish). Providers never see `FSiteAssignment`, Round, Session, tuning outside `FLayoutParams`, or any server sub-seed.

| Provider | Kind | Build | Geometry on the server |
|---|---|---|---|
| `FGrayboxLayoutProvider` (Core) | `Graybox` | Loads the map's baked `UStealthLayoutSocketData` blob (§6) — no RNG, instant | Already in the map package; nothing spawned |
| `UStealthCityGenerator` provider (Feature, §9) | `Generated` | S1-S12 + G-PRE on one worker task from `PublicSubSeed(Layout, k)` | Spawns the local geometry actor from its own Manifest entries (§3), the same builder clients use |

### 3. Geometry from the Manifest: one builder for server and clients

**3.1 One builder.** `FLayoutGeometryBuilder` (Core, client-safe `View/` header because clients run it) turns Manifest entries into a **non-replicated** local actor `AStealthLayoutGeometry` (`bReplicates = false`, `bNetLoadOnClient = false`, spawned locally on every machine; the contract's zero-replicated-actor rule holds, LSC R2.6, ADR-0003). The server (for the Generator) and every client run the same builder, so the server validates exactly what clients build. **The listen host builds the kit once**: its server-side `AStealthLayoutGeometry` is also its view, so its client path skips the kit build, recomputes `LayoutHash` from the published Manifest, and builds only decoration (§3.3).

**3.2 Components.** One `UInstancedStaticMeshComponent` per (kit mesh asset × spatial quarter of the grid) — about 48 components for 12 meshes (Generator R-G7) — never one actor or one `UStaticMeshComponent` per module, and not HISM (Nanite meshes do not need HISM's cluster culling; HISM's tree rebuild is extra cost) [U: confirm no HISM benefit with Nanite in spike R1]. Rules:
- `AStealthLayoutGeometry` is spawned at the **identity transform** (layout origin = world origin), and instances are added in local space with **one `AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false, /*bUpdateNavigation*/ false)` call per component per slice** [V signature; `bWorldSpace=true` would convert through a possibly stale pre-registration transform], all instances added **before** `RegisterComponent()` where the slice plan allows, so the navigation update is deferred to `OnRegister` and the octree sees each component once [V]. `AddInstancesById` (Preliminary in 5.8.4) is not used.
- Kit meshes are Nanite-enabled; per-module **simple** collision (≤ 4 primitives; bay stacks ≤ 2; slabs 1 box) on the pawn and `ViewBlock` channels; complex-as-simple is forbidden (art 8.x.10 shape cap ≤ 20,000).
- Mobility `Static` once the last instance of the attempt is added (set before registration) [U spike R1: runtime-registered static ISM with Lumen/VSM caching].
- `SetCanEverAffectNavigation(true)` on server components, **false** on client components (clients have no navigation at all, §4.5).
- Client slices are ≤ 10 ms each (R-G7), ordered canonically (component index, then entry order) and drain within `ClientLayoutBuildMaxSec` (3 s p95, TR-citygen-006).

**3.3 Decoration** (client, and the host as a client) is built after the kit from `FIntStream(PublicSubSeed(Layout, LayoutAttempt), LayoutHash)` (ADR-0006 §6) by a separate component set with **no collision, no nav relevance** and bounds top ≤ 0.5 m (LSC R2.3, R-G8); non-Shipping builds `ensure` and drop a violating element.

**3.4 Teardown between attempts.** On `AttemptFailed(k)` the service destroys the attempt's `AStealthLayoutGeometry` (instances, collision), re-arms the navigation build lock (§4.2) and discards the attempt's socket set before `Building(k+1)`. The Generator's worker task is cancelled through its atomic flag and its result discarded even if it completes late.

### 4. Navmesh: built once per accepted attempt, frozen at `OnLayoutReady`

**4.1 Configuration** (applied by the implementing story; `Config/` is outside this run's write scope):
- `[/Script/NavigationSystem.NavigationSystemV1]` `bInitialBuildingLocked=True`, `bAllowClientSideNavigation=False`, `bAutoCreateNavigationData=True`, one supported agent sized from SCL's capsule (radius, height, max step 45 cm = `TraversalMinHeightCm`, walkable slope).
- `[/Script/NavigationSystem.RecastNavMesh]` `RuntimeGeneration=Dynamic`; cell size and tile size from spike R4 (Generator OQ2 candidates: cell 16-19 cm, tile 8-10 m); `bDoFullyAsyncNavDataGathering` and `MaxSimultaneousTileGenerationJobsCount` chosen by R4.
- **Saved navmesh actors.** A map can hold a saved `ARecastNavMesh` actor whose per-actor `RuntimeGeneration` and baked tiles override the config default [U]. Layout maps (the graybox, the generated-round persistent map, and `L_Prototype` if it is ever used for layout work) carry **no** saved `ARecastNavMesh` (nav data is auto-created from config), and the service asserts at startup that the default nav data reports `GetRuntimeGenerationMode() == ERuntimeGenerationType::Dynamic` [V accessor], failing `NavConfigInvalid` (round aborts to Lobby, logged) otherwise.
- One `ANavMeshBoundsVolume` per map covering the full grid (the graybox map authors it; the generated map's persistent level carries one sized to the maximum grid `CityGenGridSideMaxCount` × 200 cm plus margin).
- No navigation invokers; no per-mesh nav area cost classes in the MVP (LSC R3 NavArea is `Walkable`/`Blocked` only; the 5.8 per-mesh area feature [D] is not adopted, so validation and crowd pathing use one uniform cost). Water is `Blocked` by exporting no nav geometry from the water surface plus a pawn-only blocking volume (R-G7).

**4.2 Lifecycle per attempt (server only).**
1. World start: `bInitialBuildingLocked` keeps the navigation system from building anything at begin play (the graybox map's static geometry included). The service immediately adds `ENavigationBuildLock::Custom` as its own lock and then releases the initial lock with `ELockRemovalRebuildAction::NoRebuild`, so from here on only the service's lock gates building.
2. `Building(k)`: the provider spawns (Generator) or confirms (graybox) geometry while the service's `Custom` lock is held; components register into the octree but no tile is generated.
3. When the provider reports geometry complete and the first collision trace is valid: `RemoveNavigationBuildLock(ENavigationBuildLock::Custom, ELockRemovalRebuildAction::Rebuild)`, which starts an **asynchronous** full rebuild on the tick-driven tile generator [V]. **`UNavigationSystemV1::Build()` must never be called at runtime**: it blocks the game thread until completion [V] and would break the 33 ms frame cap.
4. The service waits for `OnNavigationGenerationFinishedDelegate` **and** `!IsNavigationBuildInProgress()` **and** `GetNumRemainingBuildTasks() == 0`, polled once per tick, bounded by the attempt watchdog (`NavBuildTimeout` → `AttemptFailed`). A build that leaves an unbuilt tile inside the grid fails `NavBuildFailed` (Generator R-G10; V-NAV-1 would reject it anyway).
5. **Freeze:** the moment the build completes, the service re-adds `ENavigationBuildLock::Custom`. From then until world teardown the navigation tick never processes dirty areas [V], so no tile is rebuilt. Validate and Select run on this frozen navmesh; if the attempt fails, teardown removes geometry while the lock is held and step 3 rebuilds for the next attempt. On `Published` nothing changes: the lock simply stays, so the navmesh Crowd walks is byte-for-byte the navmesh Validate measured.
6. **Freeze discipline (the lock is a safety net, not the mechanism):** nothing spawned or moved after the freeze may affect navigation. `AStealthPawn` components, corpses, Objectives' server-only interaction actors, Concealment's hide volumes and every runtime-spawned actor have `CanEverAffectNavigation` false and are never dynamic obstacles; bush planters, court fixtures and traversal coping are Manifest geometry and exist before the build. Non-Shipping builds `ensure` once per second that **`GetNumDirtyAreas()` stays 0** after the freeze (`IsNavigationDirty()` does not see areas queued while locked [V], and queued areas grow without a cap [V]), and AC-G36 asserts the tile-build counter and `GetNumDirtyAreas()` are unchanged from `OnLayoutReady` to `Ended`. Spike R4 also records `GetNumDirtyAreas()` at each lifecycle step, so redundant dirty-area work on top of the between-attempt `RebuildAll` is measured [U whether `RebuildAll` clears queued areas]. Custom is re-added only after the build completes: while locked the tick also skips the async tile builder [V], so locking early would stall the build.

**Rejected freeze mechanisms:** `SetNavigationAutoUpdateEnabled` (editor-only [V]); `ANavigationData::SetRebuildingSuspended` (queues dirty areas and can trigger a full `RebuildAll` on resume [V]); `RuntimeGeneration=Static`/`DynamicModifiersOnly` (need navmesh data cooked with the geometry, impossible for generated cities); `SetNavigationOctreeLock(true)` as the primary mechanism (it also hides legitimate registrations; kept only as an optional extra guard if the R4 spike shows octree churn after the freeze [U]).

**4.3 Graybox uses the same path.** The graybox map ships no baked navmesh data that the game relies on; its navmesh is built at runtime exactly as a generated one, so CI's `Graybox.Validates`, the editor Validate button and a live round measure the same navmesh. If spike R4 shows the graybox build exceeding its 1 s target, the documented fallback is to cook the graybox's navmesh with the map and skip step 3 for `ProviderKind = Graybox` (the freeze and Validate are unchanged).

**4.4 Queries.** Server code reads the frozen navmesh only through `FindPathSync` (Validate, bounded counts), `FindPathAsync` (Crowd, ≤ 2 starts/frame, ADR-0016), `ProjectPointToNavigation` and `NavigationRaycast`. Off-game-thread Detour queries by layout code are not used in the MVP [U thread safety of concurrent Detour queries against a frozen mesh; revisit only if Validate misses its budget].

**4.5 Clients** hold no navigation data (`bAllowClientSideNavigation=False` [V flag]; client components `CanEverAffectNavigation(false)`; AC-G13). Client-side movement uses collision only.

### 5. Validation and selection (server, time-sliced)

**5.1 One predicate code base, two backends.** The V-* predicates (LSC R7) are written once against **`ILayoutMeasureBackend`** — path length between two points, single-source path-cost field over the 2 m cell lattice, line of sight between two points on the `ViewBlock` channel, sun ray on the `ViewBlock` channel, walkable-cell set, connectivity. The Generator's G-PRE uses the integer grid backend (`FGridMeasureBackend`, R-G5.2); the service uses `FWorldMeasureBackend` on the built world. Thresholds, guard bands and comparisons are integer cm (LSC AC-30).

**5.2 Path measurement on the navmesh (`FWorldMeasureBackend`).**
- **Point-to-point predicates with bounded counts** (V-REACH, V-SEP path, D6 contact paths, the Selection court/anchor path matrix, chain legs; a few hundred pairs per attempt) use **`FindPathSync`** string-pulled lengths, quantised to cm; a partial path is a failure (D2).
- **Field predicates over all pairs or regions** (V-NAV-1 connectivity, D3 detour statistic over ~10^5 pairs, D6b respawn regions, V-ROUTE coverage) use a **nav-grid**: every 2 m cell whose centre projects onto the navmesh (`ProjectPointToNavigation`, vertical tolerance 50 cm) is a node; 8-neighbour edges exist when `NavigationRaycast` between the two centres is unobstructed; single-source integer Dijkstra with octile weights (200 / 283 cm) gives path costs.
- **One-sided bound with exact refinement.** An octile lattice path is never shorter than the navmesh's string-pulled optimum through the same corridor and is at most 8.24 % longer [U: holds when the nav-grid's connectivity is a subset of the navmesh's; the first 50-seed batch measures the empirical ratio]. So a pair whose grid ratio passes a threshold passes exactly, a pair whose grid ratio exceeds `threshold × 1.0824` fails exactly, and only pairs in between are re-measured with `FindPathSync`. For the D3 p95, the statistic is bracketed by its lower (grid ÷ 1.0824) and upper (grid) values; refinement runs only when the bracket straddles `DetourP95MaxMult`, on the pairs whose rank can move the p95, up to `LayoutExactRefineMaxCount` (LSC-owned tuning key, default 2,000; registered by user decision 2026-10-25); exceeding it fails the attempt with `RefineBudgetExceeded` (deterministic, never wall-clock).
- Why not `FindPathSync` for everything: D3 alone needs ~10^5 paths per attempt, far beyond a 7 s attempt; why not grid-only: the LSC defines paths on the navmesh, and grid lengths differ by 5-8 % (Generator R-G5.2).

**5.3 Sight and sun predicates** (V-SIGHT, V-EMERGE witness sets, V-SEP line of sight, V-POOL sun raycast, V-BUSH-3 backdrop) use physics line traces on the **`ViewBlock`** trace channel declared by ADR-0015 (the same "what blocks sight" as the camera eye mask), from the LSC sample heights (1.6 m eyes, 3.2 m climb tops; 1.75 m / 1.0 m targets). Witness sets W(O) are computed once at Validate and stored as cell sets for the runtime clear check (§7.2).

**5.4 Time slicing.** Validate and Select are a sequence of resumable work items driven by the service tick. Each tick processes at most `LayoutSliceWorkUnitsLimit` units (cells, rays, Dijkstra relaxations ÷ 64, combos ÷ 64; unit weights fixed in code and covered by a unit test) **and** yields early if the tick has spent `LayoutSliceMaxMs` (4 ms default, a code constant until spike R4) — the work-unit cap is the deterministic bound AC-36 tests; the millisecond guard only moves work to the next frame and never changes a result.

**5.5 Selection** (LSC R9, D8) runs on the game thread outside `FPublicOnlyScope`: enumerate passing combos against the precomputed path matrix in canonical order (zone pair, zone map, F, P1_0, P2_0, P1_1, P2_1 by SocketId), check V-FAIR (≥ `MinFairCombosCount`, role share, prefix ambiguity, `FinalCandidatesMinCount` 6), then the two-stage uniform draw (usable zone pair, then combo) by rejection sampling from `FSecretStream(GetServerSubSeed(ObjectiveSites, LayoutAttempt), Tag("LSC_SITE"))` (ADR-0006). The mirror-intercept condition uses the skeleton transform the Generator hands over (server-only), inactive for the graybox. `FSiteAssignment` lives only in the service's server memory and the Round Event Log header.

### 6. Graybox socket data: editor-only markers baked into an opaque blob

Resolves LSC R11.2 / OQ1.
- **Marker actors** (`AStealthLayoutMarker` and subclasses for emergence, start cell, court box, bush planter, route spline, nav blocker) live in the **`StealthGameEditor`** module with `bIsEditorOnlyActor = true` [V flag], so neither their class nor their instances are cooked into any package. They carry `ZoneId` (0-3) at most; never a team, stage or role field (LSC R11.2, AC-28). Whether a cooked map that references marker classes from an Editor module strips them cleanly (no load warning, no dangling import) is [U]: a **cook test** (package the graybox map in a Development client, scan its package imports for the marker classes) is added to LSC AC-38's suite before the graybox epic closes.
- **Bake.** The editor Validate button and the validation commandlet (one predicate function, LSC R11.4) bake the markers into a `UStealthLayoutSocketData` data asset referenced from the map's world settings by soft reference: `uint32 FormatVersion`, `uint64 GrayboxLayoutHash` (the D7 cook-time extract hash of collision and sight-relevant placements) and **`TArray<uint8> Records`** — a versioned binary encoding of the socket records, with **no tagged, role-named properties**, so no "Zone", "Spawn", "Emergence" or "Final" token is serialised into the asset (LSC AC-38 word list). The asset's own name is neutral (`DA_<MapName>_LayoutData`). CI fails when the baked hash or record set is stale against the map (the commandlet re-bakes in memory and diffs).
- **Listen-server reality.** Every install can host, so the blob is present in every client package; only the server-authority world loads it (the soft reference is resolved by `UStealthLayoutService`, which does not exist on clients). This is acceptable under LSC R1's data classes: socket records are the *unreplicated* class, derivable from public geometry by a modified client and never encoding a secret; the worst-case leak is "these cells are zone 2" (R11.2). True exclusion from client packages comes with the Dedicated Server (ADR-0024: a server-only cook / `ServerOnly` module). **GDD sync:** LSC AC-38's "contains no marker class or asset" is satisfiable for marker *classes and actors* but not for the baked blob under a listen server (see GDD Sync).
- Clients of the graybox read `GrayboxLayoutHash` from their own packaged blob header (the header is the only part a client reads) and echo it as `LocalLayoutHash`; a client with a different map build has a different hash and fails the ack (LSC R2.7, AC-31).

### 7. Runtime services (`Published` / `Live`)

**7.1 Start placement** (`RequestStartPlacement`, LSC R6): one placement function for players and NPCs; player cells from `FSecretStream(GetServerSubSeed(PlayerPlacement, LayoutAttempt), Tag("LSC_PLCE"))`; zone NPC counts from the same sub-seed with a distinct tag; returns transforms only. Round owns the spawn order and calls `UStealthPawnFactory::SpawnSharedPawn` (ADR-0010) — the service never spawns a pawn.

**7.2 Emergence** (`RequestEmergence(Kind, RequestSeq)`, LSC R5): the clear check reads pawn positions from a server spatial hash (the Crowd Director's 2 m hash through `ILayoutPawnQuery`, ADR-0016) against each socket's precomputed dilated W(O) cell set and ExitPoint radius — no traces at run time; selection uniform among clear sockets from `FSecretStream(GetServerSubSeed(EmergencePick, RequestSeq), Tag("LSC_EMRG"))` (counter-as-Attempt shape, ADR-0006 §4); retries every `EmergenceRetrySec` in request-sequence order. The service returns a socket; Crowd spawns through the pawn factory.

**7.3 Respawn takeover** (`PickRespawnPawn(Team, RequestSeq, ExcludePawn)`, LSC R5b): eligibility pre-filter from the pawn query (Unbound, live, region by the precomputed `RespawnRegion` cell set), then the **unseen** test by `ViewBlock` line traces from every non-team pawn within `EmergenceWitnessRangeCm` (eye 1.6 m / 3.2 m on climb tops; targets head and torso, dilated per R5b.2), rejection-sampled uniform over eligible pawns sorted by server-only stable pawn id from `FSecretStream(GetServerSubSeed(RespawnPick, RequestSeq), Tag("LSC_RSPN"))`; the unseen test re-runs at commit. The service returns a pawn or `None`; `TakeOverCrowdPawn` (ADR-0010 §3.2) owns the final eligibility check and the bind. Cost guard: at most `RespawnPicksPerFrameMax` (code constant 1) picks evaluated per server frame; further requests wait one frame (their order is the request sequence, so no draw changes).
- **Result and request contract** (amendment 2026-10-24, ADR-0019 §4): `FRespawnPick { AStealthPawn* Pawn; ERespawnPickStatus Status ∈ {Picked, None, Deferred}; uint8 WideningLevel; }`. A call the per-frame guard does not evaluate returns `Deferred` immediately (the caller retries next frame; nothing is queued inside the service). The widening level is derived from the elapsed server time since the **first** call carrying that `RespawnRequestSeq` (the service records it per sequence; the record is cleared when the caller reports the request closed or at `Released`), so retries never restart widening. The caller passes an exclusion set (`TConstArrayView<const AStealthPawn*> Exclude`: the participant's own released exile body plus that request's short-term `NotEligible` exclusions) instead of one pawn. The pre-filter also applies Crowd's `ITakeoverEligibilitySource::GetTakeoverEligibility == Eligible` and the factory's side-effect-free `CheckTakeOverEligibility` (ADR-0010 §3.2a), so a returned pawn rarely fails the commit.

**7.4 Server queries.** `GetSiteAssignment_Server()`, `GetTeamSpawn_Server(Team)`, `GetSocketSet_Server()` (routes — each `CrowdRoute` edge with its pre-baked path polyline, baked in Validate after the navmesh freeze, server-only, never in `FLayoutManifest` (Crowd CR 17, user decision 2026-10-25) — sockets, courts, start cells, traversal points) are in `Public/Server/` only; Crowd's density function receives a **filtered view** with no assignment access (an attempted assignment read from the Crowd context fires the server misuse branch, LSC AC-39). **Courts** (amendment 2026-10-24, ADR-0020 §3.1/§4): a court is identified by `FCourtFixtureId` (its SocketId), declared by this ADR in `Public/View/Core/`; when the LSC GDD adds the per-court `ClueTag` record (LSC change request 8), the service exposes it per court in `FLayoutSocketView` (server-side, no Manifest or wire change; Objectives copies it into its own Team-audience view). Until then no clue record exists and Objectives uses its area-only fallback. `BushSite` records are **not** a server query: they are derived from the Manifest by a pure function in `View/` (`DeriveBushSites(const FLayoutManifest&)`) on server and clients alike (LSC R2.6, AC-52).

### 8. The Manifest on the wire

**8.1 Shape.** A single property `FLayoutManifest` on `AStealthGameState`, audience `All`, carrier GameState (IVP; ADR-0003 allowlist row), `ReplicatedUsing=OnRep_LayoutManifest`, **not** `COND_InitialOnly` (late joiners and rejoiners receive it, LSC R4.4, AC-32):

```cpp
// StealthGameplay/Public/View/Core/StealthLayoutManifest.h  (client-safe)
USTRUCT() struct FLayoutManifestEntry {            // 9 bytes on the wire (LSC D7)
    UPROPERTY() int16 CellX = 0; UPROPERTY() int16 CellY = 0; UPROPERTY() uint8 Course = 0;
    UPROPERTY() uint16 ModuleId = 0; UPROPERTY() uint8 VariantId = 0; UPROPERTY() uint8 Yaw = 0; };
USTRUCT() struct FLayoutManifestChunk {           // ≤ LayoutManifestChunkEntriesMax (1,024) entries
    UPROPERTY() uint16 ChunkIndex = 0;
    UPROPERTY() TArray<FLayoutManifestEntry> Entries;
    bool NetSerialize(FArchive&, UPackageMap*, bool& bOutSuccess);   // fixed-width 9-byte entries, count-prefixed
    bool Identical(const FLayoutManifestChunk*, uint32 PortFlags) const; };
template<> struct TStructOpsTypeTraits<FLayoutManifestChunk> : TStructOpsTypeTraitsBase2<FLayoutManifestChunk> {
    enum { WithNetSerializer = true, WithIdentical = true }; };
USTRUCT() struct FLayoutManifest {
    UPROPERTY() uint64 LayoutId = 0;                  // FNV-1a 64 of the LSC R1 LayoutId string form
    UPROPERTY() ELayoutProviderKind ProviderKind = ELayoutProviderKind::Graybox;
    UPROPERTY() uint32 GeneratorKitVersion = 0;
    UPROPERTY() int32 LayoutAttempt = 0;
    UPROPERTY() uint64 LayoutHash = 0;               // LSC D7
    UPROPERTY() uint32 EntryCount = 0;
    UPROPERTY() TArray<FLayoutManifestChunk> Chunks;  // ≤ 4 for 3,500 entries
};
```

**8.2 Why chunks, and the real wire limit.** The binding limit is the **64 KB constructed partial-bunch cap** (`net.MaxConstructedPartialBunchSizeBytes` [V]); the `UNetworkSettings` replicated-array defaults (2,048 / 65,535) are not enforced in 5.8.4 [V], and the legacy path counts array elements as `uint16` [V]. Publishing dirties the whole Manifest at once, so it travels as **one bunch of ~31.5 KB** (3,500 × 9 bytes) plus GameState payload — under the cap with ~2× headroom; a bunch of ≥ 8 partials is sent reliable [V `net.PartialBunchReliableThreshold`]. The Manifest plus everything else replicated on the GameState in that update must therefore stay ≤ 64 KB, which caps the Manifest at about 7,000 entries (`CityGenManifestEntriesMaxCount` 3,500 is well inside). LSC OQ2's "is `TArray<uint8>` fine?" is answered: it would fit the cap, but chunks are chosen because each chunk is one opaque fixed-width element (9 bytes per entry with no per-element/per-field property handles), `Identical` and change detection cost is bounded per chunk, and chunks allow the **fallback**: if the Manifest wire spike or LSC AC-34 (10 % loss / 200 ms) shows the single large bunch stalling, the service publishes chunks over successive net updates (header `EntryCount` and the client's "all chunks present" gate already support it) instead of all at once. Under Iris (ADR-0024 only) the chunk needs an Iris net serializer [U].

**8.3 Client handler.** `OnRep_LayoutManifest` is idempotent and keyed on `LayoutId`: it starts the build only when `Chunks.Num()` chunk indices `0..N-1` are present and the entry total equals `EntryCount`; a repeated or partial notify is ignored. After the build the client recomputes `LayoutHash` over its own entries (D7 canonical sort), then the Session acknowledgement echoes `(Seed, LayoutAttempt, LocalLayoutHash, ClientHash, PoolHash)` (ADR-0005). The handler never reads roles because the Manifest has none (LSC R2.5, AC-3/AC-37).

**8.4 Publish order on the server** (`Selecting → Published`): freeze sockets and selection → write `FLayoutManifest` on the GameState → write `LayoutAttempt` and `LayoutHash` into Session's `FRoundReadiness` (ADR-0005) → broadcast `OnLayoutReady(LayoutId, Seed)` (server delegate, game thread; consumers: Round, Objectives court binding, Crowd density/routes, Concealment hide volumes). Failed attempts are never replicated (LSC R8.1).

**8.5 Fallback travel.** On exhaustion in `GeneratedWithFallback` the service calls Session's `RequestFallbackTravel(GrayboxMapId)` (ADR-0005 rule 11.9: second seamless travel; Round's match store carries the round-scoped Event Log header and attempt records across it), enters `FallbackTravel`, and the new world's service starts `Idle` with `ProviderKind = Graybox`. Level streaming of the graybox into the current world is rejected (LSC R8.3; it would leave generated actors, navmesh tiles and nav bounds to unwind in place). LSC OQ3 is answered by ADR-0005's match-scoped store.

### 9. The City Generator as the second provider (Feature; built last)

**9.1 Pure-data pipeline on one worker task.** S1-S12 and G-PRE (R-G4, R-G5) are pure functions over plain C++ data (integer grid, kit tables copied into plain structs at `Build` entry; no `UObject` access) and run as **one `UE::Tasks::Launch` task per attempt** [V `Core/Public/Tasks/Task.h`]. The task opens its own `FPublicOnlyScope` (the guard is `thread_local`, ADR-0006), uses only `FIntStream` stage streams (`Mix32(AttemptSeed, s, j)`), checks an atomic cancel flag at stage boundaries, and returns `{Manifest entries, socket set, skeleton transform, metrics}` or a failure code to the game thread. This revisits architecture.md's "time-sliced on the game thread, no worker threads in the MVP": the pipeline touches no engine state, so a worker removes ~1 s of slicing complexity at no determinism cost (results never depend on scheduling; AC-G9's 3× slowdown test still applies). Spawning, collision, navmesh, Validate and Select stay on the game thread (§3-§5).
**9.2 Kit assembly** uses the §3 builder on the server from the emitted Manifest (so the server spawns exactly what clients will build), with instance and shape caps checked at emission (`ManifestOverflow` → next candidate; ≤ 5,000 instances including the decoration reserve, ≤ 3,500 Manifest entries, ≤ 20,000 collision shapes; R-G9, F2).
**9.3 Isolation.** The Generator lives in `StealthGameplay/Private/Features/CityGenerator/` with its provider header in `Public/Server/Features/CityGenerator/` (ADR-0006/0007); it includes no Round, Session-secret, Selection, Crowd or Objectives header (include lint + LSC AC-3 `Module.RoundGeneratorIsolated`).
**9.4 Goldens and bump rule** follow ADR-0008 (`CityGenGoldens.generated.inl` `constexpr` records — format amended 2026-10-24 by TD ruling — with `GeneratorKitVersion`, compiler and CPU family; "unbumped drift" fails CI).

### Architecture Diagram

```
 SERVER (listen host)                                                        CLIENTS (host renders as a client)
 Round (Preparing) ──RequestLayout(Seed, TuningHash)──► UStealthLayoutService  (UTickableWorldSubsystem, authority only)
                                                         │ FLayoutServiceCore: Idle→Building(k)→Validating(k)→Selecting(k)→Published→Live→Released
                                                         │ watchdog 7 s / 45 s; reseed k+1 on PublicSubSeed(Layout,k); fallback → Session.RequestFallbackTravel
   ┌──────────── Building(k), inside FPublicOnlyScope ───┤
   │ Graybox provider: load baked blob (records + hash)  │
   │ Generator: UE::Tasks worker S1-S12 + G-PRE ─► Manifest entries + sockets
   │            ─► FLayoutGeometryBuilder (server) ─► AStealthLayoutGeometry (non-replicated, ISM per mesh×quarter)
   └─────────────────────────────────────────────────────┤
   Nav: [Custom lock held] ─ geometry done ─► RemoveNavigationBuildLock(Custom, Rebuild) ─► async tiles
        ─► OnNavigationGenerationFinished & no tasks ─► AddNavigationBuildLock(Custom)  == FROZEN for the world's life
   Validating(k): V-* on FWorldMeasureBackend (FindPathSync | nav-grid Dijkstra + refine | ViewBlock traces), sliced
   Selecting(k):  combos on path matrix ─► FSecretStream(ObjectiveSites) two-stage draw ─► FSiteAssignment (server memory)
   Published: GameState.FLayoutManifest (header + ≤4 chunks) ─► FRoundReadiness ─► OnLayoutReady(LayoutId, Seed)
                │                                                     │
                │ Round: RequestStartPlacement ─► spawn via factory   └──────► OnRep_LayoutManifest (all chunks present)
                │ Objectives: bind courts   Concealment: DeriveBushSites        ─► FLayoutGeometryBuilder (client, ≤10 ms slices)
                │ Crowd: routes, density (filtered view)                        ─► decoration (FIntStream, no collision)
   Live: RequestEmergence (cell sets, EmergencePick) · PickRespawnPawn (ViewBlock traces, RespawnPick)   ─► LayoutHash ─► Session ack
```

### Key Interfaces

```cpp
// ---- StealthGameplay/Public/Server/Core/StealthLayoutProvider.h (consumer-owned, ADR-0007) ----
enum class ELayoutProviderKind : uint8 { Graybox, Generated };
enum class ELayoutFailure : uint8 { None, Timeout, NavBuildTimeout, NavBuildFailed, GPreExhausted, ManifestOverflow,
    GeneratorConfigInvalid, HostTooSlow, PredicateFailed, SelectionFailed, RefineBudgetExceeded, NoProvider, NavConfigInvalid };
struct FLayoutAttemptParams {               // everything a provider may read
    uint32 AttemptSeed;                     // PublicSubSeed(Layout, LayoutAttempt) — the only randomness input
    int32  LayoutAttempt;
    uint32 GeneratorKitVersion;
    uint64 LayoutParamsHash;                // CityGen* + LSC keys from the frozen tuning (ADR-0004)
    const FLayoutParams* Params;            // plain copy of those keys; no tuning service access
};
class ILayoutBuildSink {
public:
    virtual void OnCandidateReady(FLayoutCandidate&& Candidate) = 0;   // sockets + Manifest entries (+ skeleton transform)
    virtual void OnGeometryReady() = 0;                                 // instances + collision registered, first trace valid
    virtual void OnAttemptFailed(ELayoutFailure Reason, FLayoutMetrics&& Metrics) = 0;
};
class ILayoutProvider {
public:
    virtual ~ILayoutProvider() = default;
    virtual ELayoutProviderKind GetKind() const = 0;
    virtual void BeginAttempt(const FLayoutAttemptParams& Params, ILayoutBuildSink& Sink) = 0; // game thread, in FPublicOnlyScope
    virtual void TickAttempt(int32 WorkUnitBudget) = 0;   // game thread; spawning slices
    virtual void CancelAttempt() = 0;                       // idempotent; late results discarded
    virtual void TearDownAttempt() = 0;                     // destroys geometry of the current attempt
};

// ---- StealthGameplay/Public/Server/Core/StealthLayoutService.h ----
UENUM() enum class ELayoutProviderMode : uint8 { Graybox, Generated, GeneratedWithFallback };
enum class ELayoutServiceState : uint8 { Idle, Building, Validating, Selecting, AttemptFailed, Published, Live,
    Released, FallbackTravel, Aborted };
enum class EEmergenceKind : uint8 { Replacement, AmbientEntry, AmbientExit, PawnRecovery };
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLayoutReady, uint64 /*LayoutId*/, int32 /*Seed*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLayoutAborted, ELayoutFailure);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnClientLayoutFailed, FParticipantId, ELayoutClientFailure);

UCLASS(NotBlueprintType, NotBlueprintable)
class UStealthLayoutService final : public UTickableWorldSubsystem {
public:
    void RegisterProvider(ILayoutProvider& Provider);                          // composition root only; weak
    ELayoutRequestResult RequestLayout(int32 Seed, FTuningHash TuningHash, FEpochToken RoundEpoch); // Preparing only
    FOnLayoutReady OnLayoutReady;  FOnLayoutAborted OnLayoutAborted;  FOnClientLayoutFailed OnClientLayoutFailed;
    FPlacementResult RequestStartPlacement(const FStartPlacementRequest& Request);   // Published only
    FEmergenceTicket RequestEmergence(EEmergenceKind Kind, uint32 RequestSeq);       // Live only; Crowd
    FRespawnPick PickRespawnPawn(ETeam Team, uint32 RequestSeq,
                                 TConstArrayView<const AStealthPawn*> Exclude = {});  // Live only; Penalty (§7.3)
    void CloseRespawnRequest(uint32 RequestSeq);                                     // clears the widening record
    const FSiteAssignment& GetSiteAssignment_Server() const;                         // Round, Objectives, IVP PostRound release
    const FTeamSpawnRecord& GetTeamSpawn_Server(ETeam Team) const;
    const FLayoutSocketView& GetSocketView_Server() const;                           // routes, sockets, courts; no assignment
    TConstArrayView<FLayoutAttemptRecord> GetAttemptRecords_Server() const;          // Round Event Log header (R8.5)
    ELayoutServiceState GetState() const;
};

// ---- StealthGameplay/Public/View/Core/ (client-safe) ----
struct FCourtFixtureId { uint16 SocketId; };   // a court's LSC SocketId; public geometry (owned here; ADR-0020 §3.1, ADR-0011)
enum class ERespawnPickStatus : uint8 { Picked, None, Deferred };   // server use; value type only
struct FLayoutManifest;  /* §8.1 */
uint64 ComputeLayoutHash(const FLayoutManifest& Manifest);                 // LSC D7, FNV-1a 64, canonical sort
TArray<FBushSiteRecord> DeriveBushSites(const FLayoutManifest& Manifest);  // pure; server and clients (AC-52)
class IStealthLayoutClientView {                                           // client-local query (ADR-0007 View facade)
public:
    virtual bool IsLocalLayoutBuilt() const = 0;
    virtual uint64 GetLocalLayoutHash() const = 0;
    virtual FSimpleMulticastDelegate& OnLocalLayoutBuilt() = 0;
};
```

### Implementation Guidelines
1. Round and every feature **must** reach layout only through `UStealthLayoutService`; Round and the Generator **must never** include each other's headers (LSC AC-3).
2. Providers **must** read only `FLayoutAttemptParams`; they **must never** call `GetServerSubSeed`, read `FSiteAssignment`, or read world state that is not their own geometry.
3. **Must never** call `UNavigationSystemV1::Build()` at runtime (blocking [V]); the only runtime build trigger is `RemoveNavigationBuildLock(ENavigationBuildLock::Custom, ELockRemovalRebuildAction::Rebuild)` by the layout service.
4. After the freeze, nothing **may** affect navigation: every component on anything spawned after `OnLayoutReady` **must** have `CanEverAffectNavigation` false and **must never** be a dynamic obstacle or nav modifier; no code other than the layout service **may** add or remove a navigation build lock.
5. Clients **must never** run the Generator, draw geometry randomness, or hold navigation data; they build only from a complete `FLayoutManifest`.
6. The contract **must** spawn zero replicated actors; `AStealthLayoutGeometry` and every layout-derived component **must** be `bReplicates = false` and created locally from the Manifest.
7. Kit instances **must** be added with batch `AddInstances(…, bUpdateNavigation = false)` before component registration where possible; per-instance `AddInstance` loops and one-actor-per-module spawning are forbidden in layout code.
8. Every V-* predicate **must** exist once, written against `ILayoutMeasureBackend`; the grid and world backends **must not** fork predicate logic.
9. Time slicing **must** bound work by `LayoutSliceWorkUnitsLimit`; any wall-clock guard **may** only defer work, never change a result; the Generator candidate loop **must never** be wall-clock bounded.
10. `FLayoutManifest` **must** carry only the §8.1 fields; a new field is a Visibility Policy change request and a schema change (always-ask).
11. Draw sites **must** use the ADR-0006 shapes: `ObjectiveSites` and `PlayerPlacement` per `LayoutAttempt`; `EmergencePick` and `RespawnPick` counter-as-Attempt per `RequestSeq`; stream tags `LSC_SITE`, `LSC_PLCE`, `LSC_ZONE`, `LSC_EMRG`, `LSC_RSPN`.
12. Graybox marker classes **must** be editor-only (`StealthGameEditor`, `bIsEditorOnlyActor`); the baked asset **must** store records as an opaque versioned byte blob with no role-named tagged properties.
13. Logging: attempt records, failed predicate ids, `LayoutAttempt`, `LayoutHash` and `Seed` may be logged (`LogStealthLayout`); `FSiteAssignment`, zone → team, respawn picks and every sub-seed value **must never** be logged outside the server-only Event Log header (ADR-0006 §7, ADR-0007 §6).

## Alternatives Considered

### Alternative 1: Keep the prototype's fully dynamic navmesh (rebuild on change all round)
- **Description**: `RuntimeGeneration=Dynamic` with auto-update on; objectives, corpses and props carve the navmesh as they appear; Validate runs on whatever the mesh is at publish time.
- **Pros**: Zero new code; proven on the prototype; tolerant of runtime geometry changes.
- **Cons**: The navmesh Crowd walks can differ from the one Validate measured (reachability, fairness and detour predicates stop being guarantees); tile rebuilds cost game-thread time during `Active` against a 1.5 ms AI + nav budget; dynamic carving around pawns or corpses could make pathing kind- or event-dependent (a Pillar 1 risk); AC-G36 cannot pass.
- **Rejection Reason**: Contradicts Crowd CR-18 / CNB-R15 and Generator R-G7 (frozen navmesh) and makes validation results non-binding.

### Alternative 2: Navmesh built in a blocking `UNavigationSystemV1::Build()` call
- **Description**: After assembly, call `Build()` once on the server, then disable updates.
- **Pros**: One call; the navmesh is guaranteed complete on return.
- **Cons**: `Build()` blocks the game thread until every tile finishes [V]; a 1-3 s stall on the listen host freezes the host player's own briefing screen and every server tick (replication, Session liveness), violating the 33 ms frame cap (LSC R4.3, AC-36).
- **Rejection Reason**: Breaks the frame budget; the async rebuild via lock removal gives the same result without a stall.

### Alternative 3: Static or cooked navmesh for every layout
- **Description**: Ship navmesh data with the map (`RuntimeGeneration=Static`) or use `DynamicModifiersOnly` over a base mesh.
- **Pros**: No runtime build cost; deterministic.
- **Cons**: Impossible for generated cities (geometry does not exist at cook time); two navmesh paths (graybox static, generator dynamic) would let CI validate a different mesh than generated rounds use.
- **Rejection Reason**: Kept only as the graybox-specific fallback if spike R4 shows the runtime graybox build above 1 s (§4.3).

### Alternative 4: Clients run the Generator from the public seed (prototype model)
- **Description**: Replicate only `Seed`/`LayoutAttempt`; every machine generates.
- **Pros**: Tiny wire cost; matches the prototype.
- **Cons**: Every client runs the full pipeline (cost on weak clients), floating-point or compiler drift desyncs geometry, and a client-side generator bug is a desync per machine; LSC R2.2 and Generator R-G7 forbid it; also hands every client the generator's intermediate data (grid, candidate rejections) for free.
- **Rejection Reason**: Contradicts the contract (clients build from the Manifest with no RNG); a 13.5-31.5 KB Manifest once per round is affordable.

### Alternative 5: Manifest as a single `TArray<uint8>` or `TArray<FLayoutManifestEntry>` property
- **Description**: Replicate the entries directly.
- **Pros**: Simplest code; fits the 64 KB constructed-bunch cap at 3,500 entries (the 2,048-element setting is not enforced in 5.8.4 [V]).
- **Cons**: A `TArray<FLayoutManifestEntry>` replicates each element's fields through the rep layout (per-element handles, field-by-field compare of 3,500 × 6 fields); a single `TArray<uint8>` has no structure for the idempotent "all present" check and no way to spread the publish across net updates if the one-bunch send stalls under loss.
- **Rejection Reason**: Chunks give a fixed-width 9-byte wire format, bounded compare cost and a staged-publish fallback for little extra code.

### Alternative 6: Manifest through reliable RPCs to each PlayerController
- **Description**: Send chunks as owner-only reliable client RPCs.
- **Pros**: Explicit ordering; no property-diff cost.
- **Cons**: The LSC requires a state that late joiners and rejoiners receive without a resend protocol; reliable RPC bursts to 10 connections during `Preparing` risk the reliable buffer; per-connection sends of identical public data add code for no audience benefit.
- **Rejection Reason**: Replicated GameState state is the contract's carrier (LSC R4.4) and needs no custom resend logic.

### Alternative 7: Validate every path with `FindPathSync`
- **Description**: No nav-grid; every predicate path is an exact Detour query.
- **Pros**: Exactly "navmesh paths" with no bound argument.
- **Cons**: D3 needs ~10^5 pairs per attempt; at even 0.1 ms per query that is ~10 s, beyond the 7 s attempt.
- **Rejection Reason**: Budget; the one-sided bound plus refinement gives exact pass/fail decisions at a fraction of the cost.

### Alternative 8: Generator time-sliced on the game thread (architecture.md's first plan)
- **Description**: Each stage resumable, a slice per frame.
- **Pros**: No threading; no worker-thread reasoning.
- **Cons**: Twelve stages plus G-PRE must each become resumable state machines; ~1 s of work per attempt spread across ~60 frames adds latency and code; the pipeline touches no engine state, so threading buys simplicity, not risk.
- **Rejection Reason**: A single cancellable worker task over pure data is simpler and equally deterministic; kept as the fallback if a platform forbids the task system for this work.

### Alternative 9: Graybox markers as cooked runtime actors or a server-only data layer
- **Description**: Keep marker actors in the cooked map (hidden) or put them in a data layer loaded only on the server.
- **Pros**: No bake step; editing is "what you see".
- **Cons**: Marker classes and role-ish property names would ship in every package; a data layer still cooks into the package under a listen server; actor names and class names leak tokens AC-38 forbids.
- **Rejection Reason**: Editor-only markers baked to an opaque blob ship the least information possible under a listen server.

## Consequences

### Positive
- One navmesh per round, measured and walked identically: the LSC's fairness, reachability and detour guarantees bind the round actually played (Crowd CR-18).
- One geometry builder for server and clients and one predicate code base for G-PRE and Validate, so the server validates exactly what clients build and the Generator pre-checks exactly what the service decides.
- No runtime navmesh rebuild cost during `Active`; the AI + nav budget is spent on path queries only.
- No frame stall during `Preparing` on the host: async tile generation, sliced validation, worker-thread generation.
- The Manifest fits the engine's replication limits with headroom, reaches late joiners, and carries no role data.
- The graybox ships the minimum authoring information a listen-server build can, with no role tokens in names or tagged properties.
- Headless tests cover the state machine, timeouts, reseed/fallback and every predicate on fixtures (plain-C++ core, measure-backend injection).

### Negative
- A custom navigation build lock held for the whole round is an unusual engine use; any engine path that removes or bypasses it (or any future plugin that calls `Build()`) would silently unfreeze the mesh — mitigated only by the AC-G36 counter test and the non-Shipping dirty `ensure`.
- Runtime navmesh generation on every round costs up to ~2-3 s of worker time during `Preparing` (inside the 7 s attempt, covered by the briefing screen).
- The nav-grid bound and refinement add a second path engine that must be kept consistent with Detour (calibration batch required).
- The baked graybox blob is in every client package until the Dedicated Server; LSC AC-38 needs a wording change (GDD sync).
- Chunked net serialisation is custom code on a replicated property (`NetSerialize`/`Identical`) that must also get an Iris serializer if ADR-0024 adopts Iris.
- The Generator worker task introduces one cross-thread hand-off (cancel flag, result marshal) that must be tested under cancellation.

## Risks
- **R-1 Navmesh build exceeds the 3 s server budget at 5,000 instances** (spike R4). *Mitigation:* tile/cell size and `MaxSimultaneousTileGenerationJobsCount` tuned by R4; the Generator's merged-run collision fallback (R-G7) cuts nav geometry; if still over, `LayoutAttemptTimeoutSec` safe range is revisited (TDC) before any feature is cut.
- **R-2 The freeze leaks** (an engine path removes all locks, a component affects navigation after the freeze, or dirty areas accumulate unboundedly while locked). *Mitigation:* only the service touches build locks (lint grep for `AddNavigationBuildLock`/`RemoveNavigationBuildLock`/`ReleaseInitialBuildingLock`/`Build()` outside the service); AC-G36; non-Shipping `ensure(GetNumDirtyAreas() == 0)` each second after the freeze; pawn and runtime actor classes default `CanEverAffectNavigation` false and a reflection test asserts it; the engine removes `InitialLock` on hot-reload (`OnReloadComplete` [V]), which is harmless here because the service's `Custom` lock is still held, and Live Coding sessions are non-Shipping only.
- **R-2b A saved `ARecastNavMesh` actor in a layout map overrides the project generation mode** [U]. *Mitigation:* no saved nav data actors in layout maps; startup assert on `GetRuntimeGenerationMode()` (§4.1).
- **R-3 ISM nav export or collision creation is slow or differs from static actors** [U]. *Mitigation:* spike R4 measures both; equivalence spot test on the graybox kit (Verification 4); fallback merged collision boxes derived from the Manifest (part of `GeneratorKitVersion`).
- **R-4 The one ~31.5 KB Manifest bunch stalls under loss, or GameState payload in the same update pushes it past the 64 KB cap** [U]. *Mitigation:* LSC AC-34 under 10 % loss / 200 ms; a size assertion at publish (Manifest + GameState pending payload ≤ 48 KB, leaving margin); fallback staged publish of chunks over successive net updates (§8.2); Session's travel deadline bounds the wait.
- **R-5 Nav-grid bound assumption fails** (a navmesh path longer than the octile lattice path, e.g. due to agent-radius erosion making the lattice connectivity a superset) [U]. *Mitigation:* the lattice uses `NavigationRaycast` on the eroded navmesh so its edges are navmesh-walkable; the first 50-seed batch logs the min/max grid ÷ Detour ratio over 1,000 sampled pairs; if any ratio < 1.0, the bound factor becomes the measured one with a 1 % margin (a code constant, `GeneratorKitVersion`-tracked).
- **R-6 Runtime static-mobility ISM rejected or degraded by Lumen/VSM caching** [U]. *Mitigation:* spike R1; fallback `Stationary`-equivalent behaviour is not available for ISM, so the fallback is `Movable` with VSM cache invalidation off (owned by ADR-0022).
- **R-7 Worker-task determinism slip** (a stage reads shared mutable data). *Mitigation:* inputs are deep-copied plain structs; a determinism test runs the pipeline on the game thread and on a worker and compares bytes (AC-G1 variant); `FPublicOnlyScope` misuse branch reports any secret access.
- **R-8 Listen-server host can read every server-only layout fact** (assignment, respawn picks) from its own memory. *Mitigation:* none in MVP beyond IVP R10's trusted-playtest rule; Dedicated Server (ADR-0024).

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| level-socket-contract.md | R1 data classes (public / unreplicated / secret) | Manifest public; socket records server memory + baked blob (unreplicated class); assignment and picks server memory only (§5.5, §6, §7) |
| level-socket-contract.md | R2.1-2.5 integer Manifest, client build with no RNG, decoration rules, purity, no role data | §3, §8.1, §8.3; decoration §3.3 |
| level-socket-contract.md | R2.6 zero replicated actors; bush/court components built locally | §3.1, Guideline 6, `DeriveBushSites` §7.4 |
| level-socket-contract.md | R2.7 graybox hash from cook-time extract | §6 bake (`GrayboxLayoutHash`) |
| level-socket-contract.md | R4.1 layout service world subsystem, provider registry, Round talks only to it | §1, §2, Guideline 1 |
| level-socket-contract.md | R4.2-4.3 Preparing handshake, server navmesh, attempt/total budgets, ≤ 33 ms slices | §1 watchdog, §4.2, §5.4 |
| level-socket-contract.md | R4.4-4.5 Manifest replication (not initial-only), `FRoundReadiness`, ack after local hash | §8 |
| level-socket-contract.md | R4.7 server-only query API | §7.4 |
| level-socket-contract.md | R5 emergence clear check and uniform pick; R5b unseen takeover pick | §7.2, §7.3 |
| level-socket-contract.md | R6 one placement function | §7.1 |
| level-socket-contract.md | R7 predicates on navmesh paths without traversal; sight predicates | §5.1-§5.3 |
| level-socket-contract.md | R8 reseed, fallback travel, attempt records | §1, §3.4, §8.5 |
| level-socket-contract.md | R9 secret two-stage Selection | §5.5 |
| level-socket-contract.md | R11.2 markers out of the client cook (OQ1) | §6 (editor-only markers + opaque blob; residual listen-server exposure stated) |
| level-socket-contract.md | OQ2 Manifest serialisation; OQ3 fallback travel | §8.2; §8.5 |
| procedural-city-generator.md | R-G1/R-G2 provider role, inputs, purity, no server sub-seed | §2, §9.1, §9.3 |
| procedural-city-generator.md | R-G3 integer RNG | ADR-0006 `FIntStream`, used in §9.1 |
| procedural-city-generator.md | R-G5 G-PRE same predicate code as Validate | §5.1 |
| procedural-city-generator.md | R-G7 server/client split, ISM per mesh asset, Nanite, static, navmesh server-only and frozen, clients no nav | §3.2, §4, §9.2 |
| procedural-city-generator.md | R-G8 client decoration | §3.3 |
| procedural-city-generator.md | R-G9 budgets (frame, instances, Manifest, shapes, spawn + nav, client build) | §3.2, §4.2, §5.4, §9.2; Performance Implications |
| procedural-city-generator.md | R-G10 failure modes; R-G11 goldens | `ELayoutFailure`, §3.4; §9.4 |
| crowd-npc-behaviour.md | CNB-R15 / CR-18 navmesh frozen at `OnLayoutReady`; async path queries | §4.2 steps 5-6, §4.4 |

## Performance Implications
- **CPU (server, `Preparing`)**: Generator worker ≤ 1 s per attempt off the game thread (J ≤ 8 candidates × ~0.12 s); server kit spawn + collision + navmesh ≤ 3.0 s wall time (R4 to confirm; tiles on worker threads); Validate + Select ≤ ~1.5 s wall time in ≤ 4 ms game-thread slices; game-thread frames ≤ 33 ms throughout. Graybox: no provider build, navmesh build target ≤ 1 s.
- **CPU (server, `Active`)**: no navmesh rebuild; layout runtime services ≤ 0.1 ms average and ≤ 0.5 ms in a frame that evaluates a respawn pick (≤ 1 pick per frame; ≤ ~400 `ViewBlock` traces per pick), drawn from the "Engine/world tick, other" 2.0 ms line of the CPU table (architecture.md).
- **CPU (clients)**: Manifest build in ≤ 10 ms slices, ≤ 3 s p95; decoration included.
- **Memory**: ~48 ISM components; ≤ 5,000 instances; ≤ 20,000 collision shapes per machine; navmesh server-only (~10-30 MB for 128 m × 128 m at 16-19 cm cells [U, R4]); nav-grid and path matrix ≤ ~2 MB transient during Validate, freed at `Published`.
- **Load Time**: inside `Preparing`, covered by the briefing screen; ≤ 45 s worst case including reseeds.
- **Network**: once per round 13.5-31.5 KB raw Manifest (≤ 4 chunks) on the GameState to each connection, ~0.3-0.7 s of a 50 KB/s budget during `Preparing`; zero layout traffic during `Active`.

## Migration Plan
1. Nothing in `Source/StealthGame` is promoted (ADR-0007 §8): `AStealthCityGenerator`, its replicated objective/bush relocation and its dynamic navmesh are replaced, not migrated. The prototype keeps compiling until the Layout epic replaces it; deletion needs user approval.
2. Build order: (a) `FLayoutServiceCore` + state table + watchdog with a stub provider and stub measure backend (headless tests: LSC AC-6, AC-29, AC-41, AC-43); (b) Manifest structs, `ComputeLayoutHash`, chunked net serialisation, client builder, `DeriveBushSites` (AC-1/2/3, AC-31/32/34); (c) navmesh lifecycle with the lock sequence on a graybox test map (AC-G36 counters, spike R4 graybox timing); (d) `FWorldMeasureBackend` + predicate matrix (AC-22/23) and Selection (AC-24..27); (e) graybox markers + bake commandlet + CI `Graybox.Validates` (AC-28, AC-38 adjusted); (f) runtime placement / emergence / respawn (AC-7..19, with Crowd and Penalty stubs); (g) Generator provider last (worker task, goldens, batch audits).
3. Config changes listed in §4.1 are applied in the story that implements (c); `L_Prototype`'s per-map `RuntimeGeneration=Dynamic` override is superseded by the project setting.

## Validation Criteria
- LSC ACs 1-6, 22-29, 31-38, 41-44, 52 and Generator ACs G1, G9, G13, G18, G22, G24, G33, G36 pass under ADR-0008's harness (test names `StealthGame.Layout.*` and `StealthGame.CityGen.*`, files under `Source/StealthGameplay/Private/Tests/`).
- Spike R4: server spawn + nav ≤ 3.0 s at a 5,000-instance golden seed and graybox nav ≤ 1 s on the listen host; no game-thread frame > 33 ms during `Preparing` (AC-36 advisory half) on the reference machine and, before mass production, on 3060-class hardware (AQ-18).
- Freeze: tile-build counter, `GetNumDirtyAreas()` and navmesh data hash unchanged from `OnLayoutReady` to `Ended` across 60 s of play with kills, emergences and takeovers (AC-G36).
- Manifest: identical `LayoutHash` on server and 3 clients incl. one under 10 % loss / 200 ms (AC-G24, AC-34); wire size recorded.
- Nav-grid bound: on the first 50-seed batch, every sampled grid ÷ Detour ratio lies in [1.0, 1.0824] (or the bound constant is re-set per R-5) and Validate's pass/fail matches an all-`FindPathSync` reference run on 5 seeds.

## Related
- Depends on: ADR-0006 (seeds and streams), ADR-0005 (round travel, readiness, fallback travel), ADR-0007 (modules, header placement, composition root), ADR-0004 (frozen tuning, `TuningHash`), ADR-0003 (allowlist row for `FLayoutManifest`; zero replicated layout actors), ADR-0008 (tests, goldens). Uses `UStealthPawnFactory` from ADR-0010 (placement and takeover never spawn through the service). Shares the `ViewBlock` channel with ADR-0015.
- Enables: ADR-0016, ADR-0017, ADR-0019, ADR-0020, ADR-0022.
- Design: `design/gdd/level-socket-contract.md` (R1-R11, D7, D8, States, OQ1-OQ3), `design/gdd/procedural-city-generator.md` (R-G1..R-G12, F2, F5), `design/gdd/crowd-npc-behaviour.md` (CNB-R15, CR-18), `docs/architecture/architecture.md` (Required ADRs row ADR-014; TR-lsc-001..009, TR-citygen-001, 003..008, TR-crowd-003).
- Supersedes nothing. Revisits architecture.md §Data Flow note "Generator work is time-sliced on the game thread (no worker threads in the MVP; ADR-014 may revisit)": pure-data stages now run on one worker task (§9.1).

### GDD Sync (not applied by this ADR; for the GDD owners)
1. `level-socket-contract.md` AC-38: "contains no marker class or asset" → "contains no marker class or marker actor; the baked layout-data asset is present only as an opaque blob with no role tokens in names or tagged properties (listen-server builds; strict exclusion with the Dedicated Server)" (§6).
2. `level-socket-contract.md` R5b.3: "`ServerSubSeed(RespawnPick, LayoutAttempt)` keyed by `RequestSeq`" → `GetServerSubSeed(RespawnPick, RequestSeq)` (ADR-0006 counter-as-Attempt shape; same intent: counter-based, independent per request).
3. `level-socket-contract.md` Acceptance Criteria preamble: test root `Source/StealthGame/Private/Tests/LevelSocket/` and `StealthGame.LevelSocket.*` → `Source/StealthGameplay/Private/Tests/Layout/` and `StealthGame.Layout.*` (ADR-0007/0008 vocabulary).
4. `level-socket-contract.md` Tuning Knobs: LSC-owned key `LayoutExactRefineMaxCount` (int, default 2,000, hard 200-20,000, ServerOnly, Round latch) for §5.2 — **approved by the user 2026-10-25 and registered** (LSC Tuning Knobs, TDC); until decided it is a code constant.
5. `level-socket-contract.md` OQ1, OQ2, OQ3 answered by §6, §8.2, §8.5; Generator OQ3 (ISM/nav APIs) and OQ4 (threading, RNG) answered by §3.2/§4 and §9.1 + ADR-0006.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §7.3, Key Interfaces: `FRespawnPick {Pawn, Status ∈ {Picked, None, Deferred}, WideningLevel}`, `Deferred` from the per-frame guard, widening keyed by `RespawnRequestSeq` from the first call, exclusion set, Crowd/factory eligibility in the pre-filter — requested by ADR-0019 (item 2).
- §7.4, Key Interfaces: owns the `FCourtFixtureId` (= court SocketId) declaration; per-court `ClueTag` exposed server-side once the LSC GDD delivers it — requested by ADR-0020.
- §9.4: golden file format reference updated to ADR-0008's amended `CityGenGoldens.generated.inl` (follows the technical-director ruling on ADR-0008 §9).
- Config applied 2026-10-25 (phase 2, user-authorised): `Config/DefaultEngine.ini` `[/Script/NavigationSystem.NavigationSystemV1] bAutoCreateNavigationData=True, bAllowClientSideNavigation=False` and `[/Script/NavigationSystem.RecastNavMesh] RuntimeGeneration=Dynamic`. Agent sizing, cell/tile size and async-gathering keys wait for spike R4 (§4.1).
- **Deferred: `bInitialBuildingLocked=True` is NOT applied yet** (2026-10-25). Engine source (5.8.4 `NavigationSystem.cpp:1171-1178, 4094, 5186`) shows that in a game world nothing but `ReleaseInitialBuildingLock()` (or a hot-reload) removes the initial lock, and no project code calls it until the layout service of §4.2 step 1 exists. Setting it now would stop all runtime navmesh generation in the prototype (`AStealthGameMode` waits on `IsNavigationBuildInProgress`, `AStealthNpcController` wanders by `GetRandomReachablePointInRadius`), so crowd NPCs would never move — a player-observable regression the headless tests cannot see. It is applied by the story that implements the layout service's lock handling, as the Migration Plan item 3 already states.
- Removing saved `ARecastNavMesh` actors from layout maps needs the editor (not done by byte-editing `.umap`); tracked in `production/session-state/active.md` "## Needs follow-up (editor)".

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §5.2, Related — user decision 2026-10-25: `LayoutExactRefineMaxCount` is a registered LSC tuning key (no longer a code constant).
- §7.4 — user decision 2026-10-25: `CrowdRoute` edges carry pre-baked path polylines (server-only, with the route graph; the replicated Manifest is unchanged) (Crowd CR 17).
- §7.4 — user decision 2026-10-25: touchable-prop records for Crowd's idle-touch overlay are declined for MVP (Crowd CR 8); the layout publishes none.
- V-AREA — user decision 2026-10-25: `NpcCount` 50 widens the default area band to 8,000-10,800 m² (2,000-2,700 cells); LSC `ZoneDensityMaxMult` returns to 1.5; `LayoutSliceWorkUnitsLimit`/`RespawnPoolMinExpectedCount` renames (TDC CR-10).
