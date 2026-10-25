# ADR-0007: Code module layout, composition root and include boundaries

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. (Bootstrap order, architecture.md "Required ADRs": ADR-0009 → ADR-0007 + ADR-0008 → spikes S1/S2/S14 → ADR-0001. This ADR is accepted before ADR-0001 because the spike harness needs the module skeleton.)

## Date
2026-10-24

## Last Verified
2026-10-24 (against UE 5.8.4 UnrealBuildTool and engine headers; see Engine Compatibility)

## Decision Makers
Autonomous design pipeline (`/architecture-decision --review full`), validated by technical-director (TD-ADR) and the engine-specialist check; user acceptance pending.

Review record (2026-10-24): engine check **MINOR NOTES** (guard define and Shipping deny list verified in UBT/loader source; `ExtraModuleNames` has no configuration check; server types are Blueprint-reachable by default). TD-ADR **CONCERNS** (7) → all revised in this draft: client composition root moved into `StealthPresentation`; positive `NotBlueprintType`/`NotBlueprintable`/`CannotImplementInterfaceInBlueprint` rule; Event Log entries plain C++ and crash-dump policy; harness kept out of `ExtraModuleNames`, `Test` also denied; allowlist-first DataValidation audit; unique header names; verification item (1) now [V]. Re-review of the revised text: not run (autonomous); `/architecture-review` is the check.

## Summary
The project needs a C++ module layout in which the presentation layer (HUD, teammate markers), including the listen-server host's own UI, provably cannot read server-only facts, and in which lower layers never name feature types. Decision: four runtime modules (`StealthFoundation`, `StealthGameplay`, `StealthPresentation`, and `StealthGame` as the composition root) plus `StealthGameEditor` and a Shipping-excluded `StealthTestHarness`; every public header lives in `Public/View/` (client-safe) or `Public/Server/` (guarded by a compile-time `#error` in the presentation module), server-only types are never Blueprint-exposed, consumer-owned interfaces are wired weakly by the composition root at world start, and Event Log content never passes through any log, message or trace path.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Core (build system, module layout, logging) |
| **Layer** | Foundation |
| **Knowledge Risk** | MEDIUM — build/UBT and Core are pre-cutoff and stable, but 5.8 changed the C++ floor (C++20) and the logging guidance (`UE_LOGF`) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `breaking-changes.md` (C++20 minimum), `deprecated-apis.md` (`UE_LOGF`), `current-best-practices.md` (build and automation command lines); `docs/architecture/architecture.md` (Module Ownership, Consumer-owned interfaces, Initialisation order, Required ADRs, AQ-19); engine source read 2026-10-24: `Programs/UnrealBuildTool/Configuration/Descriptors/ModuleDescriptor.cs`, `Runtime/Core/Public/Logging/LogMacros.h`, `Runtime/Engine/Public/Subsystems/SubsystemCollection.h`, `Runtime/CoreUObject/Public/UObject/WeakInterfacePtr.h` |
| **Post-Cutoff APIs Used** | `UE_LOGF` (5.8) [V `LogMacros.h:199`]; C++20 default standard (5.8) [D]. Everything else is pre-cutoff. |
| **Verification Required** | (1) The `PrivateDefinitions` guard define reaches every translation unit of `StealthPresentation` under unity build and shared or private PCH, and does not reach dependents [V: `PrivateDefinitions` enter the module compile environment, `UEBuildModuleCPP.cs:2804`; with a shared PCH the defines the PCH lacks are written to `Definitions.<Module>.h` and force-included first in every file, `:2260-2300`; a module-private PCH re-applies the module's own defines, `:2233-2241`; keep the CI self-test anyway]; (2) `TargetConfigurationDenyList: ["Shipping", "Test"]` on the `StealthTestHarness` `.uproject` entry is honoured by UBT and by the runtime module loader [V: `UnrealBuildTool/.../ModuleDescriptor.cs:737`, `UEBuildTarget.cs:5559`, `Runtime/Projects/Private/ModuleDescriptor.cpp:567, 635-638`] — the cooked-Development load is still to be observed once [U]; (3) the DataValidation-hosted Blueprint audit can walk `K2Node_CallFunction::GetTargetFunction`, cast and variable pin types, "Get Subsystem" nodes and `UWidgetBlueprint` property bindings [V plugin and `UEditorValidatorBase` present; U graph-walk API details]; (4) `[CoreRedirects]` (`ClassRedirects`, `StructRedirects`, `EnumRedirects`) resolve for this project's reparented sample Blueprints when a class moves module [U]. |

Verified engine facts [V] (read from the 5.8.4 install this session):
- `ModuleHostType` includes `Runtime`, `UncookedOnly`, `CookedOnly`, `DeveloperTool`, `Editor`, `ServerOnly`, `ClientOnly`; `Runtime` builds for every non-Program target; `UncookedOnly` only where the build does not require cooked data (so not in a cooked Development game); `ServerOnly` excludes `Client` targets, `ClientOnly` excludes `Server` targets — both are compiled into the listen-server `Game` target (`ModuleDescriptor.cs:783-810`).
- Module descriptors support `TargetAllowList`/`TargetDenyList` and `TargetConfigurationAllowList`/`TargetConfigurationDenyList` (`ModuleDescriptor.cs:199-214`).
- `FSubsystemCollectionBase::InitializeDependency(TSubclassOf<USubsystem>)` and the templated form exist (`SubsystemCollection.h:35-44`).
- `TWeakInterfacePtr` exists (`UObject/WeakInterfacePtr.h`).
- `UE_LOGF(Category, Verbosity, Format, ...)` exists; the header says "Prefer UE_LOGF" (`LogMacros.h:199, 272`).
- `ExtraModuleNames` in a Target.cs is added with no configuration check (`UEBuildTarget.cs:5030-5041`), so a module listed there builds into Shipping regardless of the `.uproject` deny list (TD-ADR engine note).
- Server types are reachable from Blueprint even without a Blueprint specifier: every subsystem gets a "Get <X> Subsystem" node unless it is `NotBlueprintType` (`BlueprintGraph/Private/K2Node_GetSubsystem.cpp:282` → `EdGraphSchema_K2.cpp:1228` falls through to `bAssumeBlueprintType`). `AActor`/`UActorComponent` subclasses inherit `BlueprintType`/`Blueprintable`, and a `UINTERFACE` is implementable in Blueprint by default [U].

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None. (Content and placement inputs are listed in the Ordering Note; they are not acceptance prerequisites.) |
| **Enables** | ADR-0008 (the harness module and the compile-fail probe mechanism live in the layout defined here); spikes S1, S2, S14 (they need a module skeleton); every Core/Feature/Presentation ADR (ADR-0010..ADR-0023) places its types by this ADR's rules; ADR-0024 (Dedicated Server may later tighten `StealthPresentation` to `ClientOnly` and server modules to `ServerOnly`). |
| **Blocks** | Every coding epic (no production module may be created before this is Accepted); the module-skeleton story that precedes the S1/S2/S14 spikes. |
| **Ordering Note** | ADR-0009 (version control) is accepted first per the bootstrap order — the skeleton is the first commit. ADR-0007 and ADR-0008 are accepted together. **Content inputs, not prerequisites** (architecture-review fix 2026-10-25; moved here from Depends On so the declared graph matches architecture.md's bootstrap order ADR-0009 → ADR-0007/0008 → spikes → ADR-0001): ADR-0001 (identity carriers: which types are View vs Server), ADR-0002 (queue value types in `StealthFoundation/Public/View/`, queue payloads and interfaces in `Public/Server/`), ADR-0004 (tuning client/server faces). This ADR defines the placement rule; those ADRs name the types, and their types land with their own stories. |

## Context

### Problem Statement
The game's core claim (Pillar 1, "kind-blind by construction") is that nothing a client can see distinguishes a player from an NPC. On a listen server the host's process holds every ServerOnly fact (IVP R10); the only thing that keeps the host's own HUD honest is code discipline. GDDs ask for that discipline to be *build-enforced*: "presentation modules may not include server query headers (build-time include check)" (IVP R10), "the client face is typed to the public struct, so a server-only key cannot be expressed in client C++" (TDC CR-10, AC-28), "host UI code may include only the public struct" (Round R19), and host parity in Roster R6. UBT exposes every `Public/` header of a dependency to a dependent, so a module line on its own is not a boundary. In addition, three Core consumers need run-time services from Feature systems (Round and the Shared Pawn need Crowd; the Layout Service needs takeover eligibility; the Visibility Policy needs Concealment's commits), and UBT forbids module cycles. The prototype is one module (`StealthGame`) where everything includes everything; it must not silently become production.

This must be decided now: it is the first thing a programmer creates, the spike harness (S1, S2, S14) needs it, and changing a module layout after classes are referenced by Blueprints costs redirects and asset resaves.

### Constraints
- UE 5.8.4, C++20 minimum, VS 2022 17.14 [V active.md]; `DefaultBuildSettings = BuildSettingsVersion.Latest`, `IncludeOrderVersion = Latest` (current targets).
- 1-2 programmers; MVP ships a listen server only (Dedicated Server is Alpha, ADR-0024).
- No module cycles (UBT). Cross-module calls need `*_API` exports.
- Sample content (Game Animation Sample) is reparented onto prototype classes; `CBP_SandboxCharacter` is reparented to `/Script/StealthGame.StealthCharacter` (active.md). Moving a referenced class needs a redirect.
- Pipeline write scope: this ADR changes no file under `Source/`, `Config/` or the `.uproject`; it specifies what a later story creates.

### Requirements
- Presentation code (HUD, markers, owner presentation helpers) on every machine including the listen host must fail to compile if it includes a server-only header (IVP R10, TR-ivp-011, TR-roster-009 compile-time half).
- The tuning client face must be a distinct type so a server-only key cannot be named in client C++ (TDC CR-10, AC-28, TR-tdc-006).
- No lower layer names a feature type; upward run-time needs go through consumer-owned interfaces wired at world start.
- Registrations must survive multi-client PIE (one world per client) and seamless travel without dangling pointers.
- Pure logic (queue, Roster, tuning validator, seed mixer, appearance deck) must be testable headless without a `UWorld`.
- The multi-process leak harness must load in cooked Development builds and never ship.
- Event Log content never reaches `UE_LOG`/`UE_LOGF`, the output log, `ClientMessage`, crash dumps or trace channels (Round R19, TR-round-008).

## Decision

### 1. Module layout (resolves AQ-19: four runtime modules)

| UE module | Host type / descriptor | Contents | Dependencies (Build.cs) |
|---|---|---|---|
| `StealthFoundation` | `Runtime` | Tuning service + validator (ADR-0004); Roster store (ADR-0001/0005); Session connection table + seeds (ADR-0005/0006); Visibility Policy, carrier bases, allowlist data (ADR-0001/0003); Resolution Queue, server clock, staged writes, **every queue and staged-write payload type, `FPawnHandle` and `FSubjectRef`** (`Public/Server/StealthQueuePayloads.h`), and the resolver/applier/terminal/flush interfaces (ADR-0002 §2a, §6; amendment 2026-10-25); Event Log store; fault sink; **wire and view structs shared across layers** in `Public/View/` (`FActionState`, `FDeathState`, `FStrikeResult`, `FDefenceResult`, `FRoundPublicState`, ids, epoch tokens) | Public: `Core`, `CoreUObject`, `Engine`, `NetCore`. Private: `DeveloperSettings`, `Json` (Event Log telemetry export, ADR-0023; amendment 2026-10-24) |
| `StealthGameplay` | `Runtime` | `Core/`: Shared Pawn + movement + AnimInstance base + significance, `UStealthPawnFactory`, Pawn Command Component + action hub, Character Pool + allocator, Layout Service + Graybox provider, Round rules, Camera (client). `Features/`: Crowd, Concealment, Assassination, Penalty, Objectives, City Generator, Telemetry (ADR-0023 probes; amendment 2026-10-24). Feature team-view components in `Public/View/` | Public: `StealthFoundation`. Private: `EnhancedInput`, `AIModule`, `NavigationSystem`, `PoseSearch`, `MotionWarping`, `StateTree`, `GameplayStateTree` (final list set by ADR-0010/0011/0013/0016) |
| `StealthPresentation` | `Runtime` (client code; see §7 for the Dedicated Server path) | HUD view-models and widgets, Teammate Markers, owner presentation helpers, **`AStealthHUD` and the client half of the composition root** (`UStealthPresentationBootstrap`, built only from `View/` interfaces) | Private only: `StealthFoundation`, `StealthGameplay`, `UMG`, `CommonUI`, `Slate`, `SlateCore`. `PrivateDefinitions.Add("STEALTH_PRESENTATION_MODULE=1")` |
| `StealthGame` | `Runtime`, `IMPLEMENT_PRIMARY_GAME_MODULE` (keeps the existing name) | **Server composition root**: `AStealthGameMode` (`AGameModeBase`), `AStealthGameState`, `AStealthPlayerController`, `AStealthPlayerState`; all server registration and wiring. For the client side it only sets `HUDClass` and has the local PlayerController call the presentation bootstrap with `View/` handles | Private: all three runtime modules above |
| `StealthGameEditor` | `Editor`, `PostEngineInit` (existing) | MCP toolsets (existing `StealthPlayToolset`, `StealthAssetToolset`), tuning lint, allowlist generator, widget-Blueprint server-reference audit, asset audits, golden-seed tools | Private: all runtime modules, `UnrealEd`, `ToolsetRegistry`, `UMGEditor`/`Kismet` as the audit needs |
| `StealthTestHarness` | `Runtime` with `"TargetConfigurationDenyList": ["Shipping", "Test"]` in the `.uproject` module entry; **not** listed in any Target.cs `ExtraModuleNames` (that list has no configuration check [V], and the `.uproject` entry already pulls the module in) | Multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, replication-system assertion, test seams (mechanism: ADR-0008) | Private: all runtime modules. **No runtime module depends on it.** |

`UncookedOnly` and `DeveloperTool` are rejected for the harness because neither builds into a cooked Development game, which the S3 and raw-bunch leak suites need [V `ModuleDescriptor.cs:790-797`].

The Core/Feature split inside `StealthGameplay` is enforced by folders, the include lint (§3) and consumer-owned interfaces (§4), not by UBT. The direction rule is: `Features/` may include `Core/`; `Core/` must never include `Features/`.

### 2. Header placement: `Public/View/` and `Public/Server/`

Every runtime module's public headers live in exactly one of:
- `Public/View/` — **client-safe**: wire/view structs, client faces (`IStealthClientTuning`, Roster team view, `FRoundPublicState`), view components that ride C-TeamView, client-local query interfaces (Camera view state, Concealment client latch, IIF reply sink). May be included anywhere.
- `Public/Server/` — **server-only**: services, stores, resolvers, consumer-owned interfaces used by server code, Event Log types, `GetServerTuning()`. Every header here starts with `#include "StealthServerOnlyGuard.h"`.
- The module's root `Public/` folder holds only the module header and `StealthServerOnlyGuard.h` (Foundation); no other header may sit there (lint rule).
- **Header file names are unique across all project modules** (lint rule). UBT adds every dependency's `Public/` folder to the include path, so `#include "Server/Foo.h"` would silently resolve to whichever module's `Server/Foo.h` comes first if two modules had one. Project headers use the `Stealth` prefix plus the system name (`StealthRosterService.h`, not `Service.h`).

`StealthFoundation/Public/StealthServerOnlyGuard.h`:
```cpp
#pragma once
// Included first by every Public/Server/ header (ADR-0007).
#if defined(STEALTH_PRESENTATION_MODULE) && STEALTH_PRESENTATION_MODULE
#error "Server-only header included from StealthPresentation (ADR-0007, IVP R10)"
#endif
```

**Facade split per service.** A UCLASS lives in one header, so a service with both faces is split: a small `View/` facade (interface or USTRUCT) and a `Server/` implementation. Example (ADR-0004's names): `IStealthClientTuning`, `FStealthClientTuning`, `FStealthTuningSnapshot` (View) vs `FStealthServerTuning`, `UStealthTuningStore` and `UStealthTuningService` with `GetServerTuning()` (Server). Presentation holds the service only as an `IStealthClientTuning*` handed over by the bootstrap (§4). The client face is a distinct struct type, so a server-only key is not a member of any type client code can name (TDC CR-10 compile-time guarantee).

Because the guard define is a **private** definition of `StealthPresentation`, the composition root (`StealthGame`), which legitimately includes both faces, is unaffected.

### 3. Three enforcement layers for the include boundary

1. **Compile guard** (above): any presentation translation unit that transitively includes a `Server/` header fails to compile. Covers C++ only.
2. **Include lint** (CI and a pre-commit check; mechanism in ADR-0008): fails when (a) a header sits outside `View/`/`Server/` in a `Public/` folder, (b) a `Public/Server/` header does not include the guard first, (c) a `Public/View/` header includes a `Server/` header (a view header must stay client-safe), (d) a file under `StealthGameplay/*/Core/` includes a `Features/` path, (e) `StealthFoundation` includes anything from `StealthGameplay`/`StealthPresentation`/`StealthGame` (UBT also refuses this, as there is no dependency), (f) two project headers share a file name, (g) a UCLASS/UINTERFACE in a `Server/` header lacks the specifiers in rule 3, (h) `StealthPresentation` source uses `TActorIterator`, `TActorRange`, `GetAllActorsOfClass`, `GetAllActorsWithInterface`, `GetPlayerControllerIterator`, `GetGameMode`/`GetAuthGameMode`, or `APawn::GetPlayerState`/`GetController` on a pawn other than the local one (extends AC-TMK-06, AC-IVP-17 and AC-PSR-42), (i) a log/trace macro argument names an Event Log type or a server-secret identifier (§6), (j) a file under a `CityGenerator/` folder includes anything under `Public/Server/Random/` (Generator isolation, ADR-0006 §3, R-G2.2; amendment 2026-10-24 — ADR-0006 asked for it as "(i)", renumbered because (i) was taken).
3. **Blueprint reach** (exposure is inherited by default in UE, so the rule is positive, not just "no Blueprint specifiers"):
   - Every UCLASS declared in a `Server/` header **must** declare `NotBlueprintType, NotBlueprintable`; every server `UINTERFACE` **must** declare `meta=(CannotImplementInterfaceInBlueprint)`; no member of a server type carries `BlueprintCallable`, `BlueprintPure`, `BlueprintReadOnly`, `BlueprintReadWrite` or `BlueprintAssignable`. `NotBlueprintType` also removes the "Get <X> Subsystem" node for server subsystems [V `K2Node_GetSubsystem.cpp:282`].
   - **Allowlist-first Blueprint audit**, hosted as a DataValidation `UEditorValidatorBase` in `StealthGameEditor` and run in CI through the DataValidation commandlet. For every `UWidgetBlueprint` and every Blueprint under `/Game/StealthGame/UI/`, every referenced class (call targets via `K2Node_CallFunction::GetTargetFunction`, cast targets, variable and pin types, "Get Subsystem" nodes, widget property bindings) must be on a checked-in **UI reference allowlist**: `View/` classes, UMG/CommonUI/Slate classes, and an approved list of engine functions. Explicitly never allowed: `GetGameMode`, `GetAllActorsOfClass`/`WithInterface`, `GetPlayerState` on a pawn, `Get Player Controller` with any index other than a literal 0, any `Server/` class. On the listen host these calls return authoritative objects — the host-parity hole the C++ guard cannot see.

### 4. Composition root and consumer-owned interfaces

- Lower layers declare small interfaces they *consume* (architecture.md table): `ICrowdControl`, `ITakeoverEligibilitySource`, `ILayoutProvider` (in `StealthGameplay/Public/Server/Core/`); `IConcealmentCullSource` push API, `IQueueResolver`, `IStagedWriteApplier`, `ITerminalDecision`, `IEventFlushBarrier` (in `StealthFoundation/Public/Server/`). Features implement them; **no lower-layer file names a feature type**. Corollary (amendment 2026-10-25): a type that a Foundation container or interface names, such as a queue item or staged-write payload, is declared in Foundation, never in the feature that fills it; gameplay modules register resolvers and appliers by item, timer or write type at the composition root.
- **Server wiring** happens only in `AStealthGameMode::InitGame` (services that exist before any actor) and in a `OnWorldBeginPlay` handler the GameMode binds (implementations that need actors or world subsystems). Never from `IModuleInterface::StartupModule` (no world exists then) and never from a feature's own `BeginPlay` reaching down to register itself.
- **Client wiring** (presentation view-models, local query interfaces) is the client half of the composition root and lives **inside `StealthPresentation`**, so the `#error` guard covers it: `AStealthHUD` (declared in `StealthPresentation`) owns a `UStealthPresentationBootstrap` that is built only from `View/` interfaces. The local `AStealthPlayerController` (in `StealthGame`) calls `Bootstrap->Bind(FStealthClientViewHandles)` once its C-TeamView component and the replicated GameState public struct are available; `FStealthClientViewHandles` is a `View/` struct of `View/` interface pointers; it includes `IOwnerCommandClientView` (implemented by `UPawnCommandClient`, ADR-0011/ADR-0021), through which Presentation hands its `IAllyMarkerQuery` implementation back to the IIF client (`SetAllyMarkerQuery`) — amendment 2026-10-24. `StealthGame` never constructs presentation objects beyond setting `HUDClass`. Presentation never discovers services by searching the world.
- **Subsystem ordering** uses `Collection.InitializeDependency<T>()` in `Initialize`, never reliance on creation order.
- **Registrations are weak**: `TWeakInterfacePtr<I…>` / `TWeakObjectPtr<…>`, cleared in `Deinitialize`; call sites check validity and treat a dead registration as a fault (fault sink), never a crash. Game-instance stores bind to world objects with `AddWeakLambda`/`AddUObject` bindings and re-bind after each seamless travel (ADR-0005). Stores that refer to pawns hold `TWeakObjectPtr`.
- Pawn creation goes through `UStealthPawnFactory` (world subsystem, ADR-0010), not free functions.
- The queue drain is a custom `FTickFunction` registered by the composition root in `OnWorldBeginPlay` (ADR-0002 owns its tick group).

### 5. Plain-C++ cores behind thin facades
The queue, the Roster, the tuning validator, the seed mixer and the appearance deck are plain C++ classes (`F…Core`) with no `UWorld`, `UObject` or engine-singleton dependency; inputs come through constructor arguments or small interfaces (dependency injection). A `UWorldSubsystem`/`UGameInstanceSubsystem` facade owns one instance and adapts it. Cores live in the owning module's `Private/` unless another module needs the type (then `Public/Server/`). Their specs live in the same module's `Private/Tests/` and run headless (ADR-0008).

### 6. Logging policy
- New code uses `UE_LOGF` with a `LogStealth…` category: one per module (`LogStealthFoundation`, `LogStealthGameplay`, `LogStealthPresentation`, `LogStealthGame`, `LogStealthHarness`) plus optional per-system categories declared in the owning module (e.g. `LogStealthTuning`, ADR-0004); `UE_LOG` is allowed only in code that pre-dates this ADR until it is rewritten.
- Event Log types (`Server/`, `StealthFoundation`) are **plain C++** — no `USTRUCT`, `UCLASS` or `UPROPERTY` — so reflection-based dumps (`ExportText`, `obj dump`, `DisplayAll`, Blueprint debuggers, the reflection audits themselves) cannot reach them, and they expose no `ToString`, `LexToString`, `operator<<` or `FString` conversion. The only text conversion is the `Ended`/`Reset` disk writer inside the Event Log store. The store is owned by a facade subsystem through a `TUniquePtr` to the plain core.
- **Crash dumps**: non-Shipping builds used in trusted playtests write minidumps without full heap memory (crash-reporter configuration, set by the release/devops story under ADR-0009's process); the residual risk that a minidump's stack captures Event Log fields on the host is **accepted** under IVP R10's trusted-playtest rule until the Dedicated Server (ADR-0024). Event Log entries, their fields, `DealDigest`, mailbox order, server sub-seeds and bindings are never arguments of `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names. The lint (§3.2) greps log-macro arguments for Event Log type names and for the server-secret identifiers.
- Reflective or Blueprint requests for a server-only value on a client return *invalid* and log via `ensure` in development, never `check` (TDC CR-10).
- `stealth.round.dumplog` and similar debug commands are `#if !UE_BUILD_SHIPPING` and refuse unless the phase is `Ended`/`Reset` (Round R19).

### 7. Dedicated Server headroom (not MVP)
On the listen-server `Game` target every module is compiled into the same process, so `ClientOnly`/`ServerOnly` host types give no MVP protection [V `ModuleDescriptor.cs:805-809`]. ADR-0024 may later mark `StealthPresentation` `ClientOnly` and move `Server/` implementations into a `ServerOnly` module for the Server target. The `View/`/`Server/` split is chosen so that move is mechanical.

### 8. Prototype promotion rule
Nothing in the current `Source/StealthGame` prototype is promoted into a production module until the ADR that governs it is Accepted (concept Scope Risks). Promotion is a rewrite into the new layout, not a file move: `UStealthSettings` (`Config` properties in `DefaultGame.ini`) violates TDC CR-13 and is replaced, not migrated; `StealthRules` is replaced by the queue/Round cores. When a production class reaches a path a Blueprint already references (for example a pawn class that sample Blueprints are reparented to), a `[CoreRedirects] +ClassRedirects=(OldName="/Script/StealthGame.X",NewName="/Script/StealthGameplay.Y")` entry is added in the same change and the affected Blueprints are resaved by explicit path (never mass-save; active.md gotcha). The prototype's classes keep compiling in `StealthGame` until each is replaced; they are deleted only with user approval (file deletion is an always-ask category).

### Architecture Diagram

```
                     ┌──────────────────────────────────────────────┐
                     │ StealthGame  (primary module, server         │
                     │ composition root: GameMode/GameState/PC/PS;  │
                     │ wires interfaces in InitGame /               │
                     │ OnWorldBeginPlay; local PC hands View/       │
                     │ handles to the presentation bootstrap)       │
                     └──────┬───────────────┬───────────────┬───────┘
                            │ private       │ private       │ private
                            ▼               ▼               ▼
 ┌──────────────────────────────┐  ┌───────────────────────────────────┐
 │ StealthPresentation          │  │ StealthGameplay                   │
 │ AStealthHUD + bootstrap      │
 │ PrivateDefinitions:          │  │  Public/View/   Public/Server/    │
 │  STEALTH_PRESENTATION_MODULE │──►  Core/  ◄──────── Features/       │
 │ includes View/ only          │  │  (Core never includes Features;   │
 │ (Server/ → #error)           │  │   upward needs via ICrowdControl, │
 └──────────────┬───────────────┘  │   ITakeoverEligibilitySource,     │
                │                  │   ILayoutProvider)                │
                │                  └───────────────┬───────────────────┘
                │                                  │ public
                ▼                                  ▼
 ┌──────────────────────────────────────────────────────────────────────┐
 │ StealthFoundation   Public/View/ (wire structs, client faces)        │
 │                     Public/Server/ (services, IQueueResolver,        │
 │                       ITerminalDecision, IConcealmentCullSource,     │
 │                       Event Log)  Public/StealthServerOnlyGuard.h    │
 └──────────────────────────────────────────────────────────────────────┘
 StealthGameEditor (Editor) ──► all runtime modules
 StealthTestHarness (Runtime, not Shipping) ──► all runtime modules; nothing depends on it
```

### Key Interfaces
- `StealthServerOnlyGuard.h` (Foundation, root `Public/`): the guard above.
- Folder contract: `Source/<Module>/Public/View/**`, `Source/<Module>/Public/Server/**`, `Source/<Module>/Private/**`, `Source/<Module>/Private/Tests/**`.
- Consumer-owned interfaces (declared by the consuming layer; signatures owned by the named ADR): `IQueueResolver`, `IStagedWriteApplier`, `ITerminalDecision`, `IEventFlushBarrier` (ADR-0002), `IConcealmentCullSource` (ADR-0003/0017), `ICrowdControl`, `ITakeoverEligibilitySource` (ADR-0016/0019), `ILayoutProvider` (ADR-0014).
- Registration shape (all services): `void Register<I>(TScriptInterface<I> Impl)` stores `TWeakInterfacePtr<I>`; `Unregister` on `Deinitialize`/`EndPlay`; lookups return `I*` or `nullptr` and report a fault on `nullptr`.
- Composition-root entry points: server — `AStealthGameMode::InitGame`, the GameMode's `OnWorldBeginPlay` handler; client — `UStealthPresentationBootstrap::Bind(const FStealthClientViewHandles&)` (in `StealthPresentation`, called by the local `AStealthPlayerController`), owned by `AStealthHUD` (in `StealthPresentation`).
- Server-type declaration shape: `UCLASS(NotBlueprintType, NotBlueprintable)`; `UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))`.

### Implementation Guidelines
- Code **must** live in one of the six modules above; a new module requires an ADR amendment.
- Every header in a `Public/` folder **must** be under `View/` or `Server/` (exceptions: the module header; `StealthServerOnlyGuard.h`).
- Every `Public/Server/` header **must** include `StealthServerOnlyGuard.h` as its first include.
- `Public/View/` headers **must never** include a `Server/` header.
- `StealthPresentation` **must** declare `STEALTH_PRESENTATION_MODULE=1` as a `PrivateDefinitions` entry, never a `PublicDefinitions` entry.
- Every UCLASS in a `Server/` header **must** declare `NotBlueprintType, NotBlueprintable`; every server UINTERFACE **must** declare `meta=(CannotImplementInterfaceInBlueprint)`; members of server types **must never** carry `BlueprintCallable`, `BlueprintPure`, `BlueprintReadOnly`, `BlueprintReadWrite` or `BlueprintAssignable`.
- UI Blueprints **must** reference only classes and functions on the checked-in UI reference allowlist; they **must never** call `GetGameMode`, `GetAllActorsOfClass`/`WithInterface`, `GetPlayerState` on a pawn, `Get Player Controller` with a non-literal-0 index, or reference a `Server/` class; the DataValidation audit fails the build.
- `AStealthHUD` and all client-side wiring **must** live in `StealthPresentation`; `StealthGame` **must never** construct presentation objects beyond `HUDClass`.
- Project header file names **must** be unique across all modules.
- Event Log entry types **must** be plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`).
- `StealthTestHarness` **must never** appear in a Target.cs `ExtraModuleNames`.
- Files under `StealthGameplay/*/Core/` **must never** include `Features/` paths; lower layers **must never** name a feature type.
- Interface implementations **must** be registered only by the composition root, from `InitGame`, the `OnWorldBeginPlay` handler, or local PC/HUD `BeginPlay`; **never** from `StartupModule`.
- Registrations **must** be weak and **must** be cleared in `Deinitialize`/`EndPlay`.
- Subsystem ordering **must** use `InitializeDependency`.
- Queue, Roster, tuning validator, seed mixer and appearance deck **must** be plain C++ cores with no `UWorld` dependency.
- New logging **must** use `UE_LOGF` with the module category; Event Log content, `DealDigest`, server sub-seeds, bindings and mailbox order **must never** reach a log, message, ensure/check text or trace.
- No runtime module **may** depend on `StealthTestHarness` or `StealthGameEditor`.
- Prototype code **must not** be moved into a production module before its governing ADR is Accepted; a moved Blueprint-referenced class **must** ship with its `[CoreRedirects]` entry in the same change.

## Alternatives Considered

### Alternative 1: Seven runtime modules (Foundation, Core, Features, Presentation, root, plus split server/view modules)
- **Description**: The first architecture draft: one UBT module per layer, so UBT itself refuses Core→Feature and Presentation→Server includes.
- **Pros**: Layering is enforced by the build tool; presentation could depend only on a `…View` module that contains no server headers at all, which is a stronger boundary than a `#error`.
- **Cons**: Every Core→Feature run-time edge and every cross-layer call needs an interface plus `*_API` export; facades split across modules double the header count; reflection (UCLASS in one module, its view in another) multiplies boilerplate; incremental build graph grows. For 1-2 programmers this was judged the largest avoidable cost (LP-FEASIBILITY concern 8).
- **Rejection Reason**: The guard + lint + Blueprint audit deliver the same compile-time guarantee for the boundary the GDDs actually require (presentation vs server) at a fraction of the export work. The Core/Feature direction is protected by the lint and consumer-owned interfaces. Revisit if the lint is repeatedly bypassed or at the Dedicated Server stage (ADR-0024).

### Alternative 2: Keep the single prototype module (`StealthGame`)
- **Description**: All code in one module with folder conventions.
- **Pros**: Zero setup; the prototype already compiles and its tests pass.
- **Cons**: No define-based guard is possible (presentation and server share translation-unit settings), so IVP R10's "build-time include check" degrades to a lint; nothing stops a feature from being named by Core; the prototype would drift into production unreviewed.
- **Rejection Reason**: Fails IVP R10 and TDC CR-10's compile-time requirement.

### Alternative 3: Game Feature plugins per feature (Crowd, Concealment, …)
- **Description**: Each Feature system as a GameFeature plugin activated per experience.
- **Pros**: Hard isolation per feature; activation hooks give a natural composition point.
- **Cons**: All MVP features are always on (one mode); plugin activation adds load-order and seamless-travel lifetime questions (AQ-10 already open); more descriptor and asset-path churn; the boundary that matters (presentation vs server) is not addressed by it.
- **Rejection Reason**: Solves a problem the MVP does not have and adds lifetime risk to the hardest open area (travel).

### Alternative 4: Separate Presentation into a `ClientOnly` module now
- **Description**: Mark `StealthPresentation` `ClientOnly`.
- **Pros**: Signals intent for the Dedicated Server stage.
- **Cons**: On the listen-server `Game` target, `ClientOnly` modules are still compiled and loaded [V], so it adds no MVP protection, and it would exclude presentation from any future `Server` target that hosts a listen-style debug HUD.
- **Rejection Reason**: No MVP benefit; deferred to ADR-0024.

## Consequences

### Positive
- The GDDs' compile-time requirements (IVP R10, TDC CR-10/AC-28, Round R19 host UI, Roster R6 host parity) have a concrete, testable mechanism.
- No module cycles; upward run-time needs are explicit interfaces, visible in one wiring place.
- Multi-client PIE and seamless travel are safe by construction (world-scoped, weak registrations).
- Pure cores make the hardest logic (Roster, queue, validator, deck, seeds) unit-testable headless, matching the coding standard's "dependency injection over singletons".
- The harness ships in Development cooked builds and never in Shipping.
- The Dedicated Server move (ADR-0024) is mechanical.

### Negative
- The guard is per translation unit of `StealthPresentation` only: a server header included by a *view* header compiled in another module would not trip it — lint rule §3.2(c) is the backstop, so lint is load-bearing.
- Every server type carries extra specifiers (`NotBlueprintType, NotBlueprintable`), and designers cannot prototype UI against server data in Blueprint — intended.
- Facade splits add one header per dual-faced service.
- Core/Feature direction is enforced by lint, not UBT; a lint gap is a silent layering breach.
- The Blueprint audit is custom editor tooling to build and maintain.
- Listen-host `ServerOnly` protection is still code discipline, not a security boundary (IVP R10 accepts this until Alpha).

## Risks
- **Guard define regression after a UBT upgrade.** Verified for 5.8.4 (UBT force-includes the module definitions header when a shared PCH lacks them); mitigation: the guard self-test (Validation 1) runs in CI on every engine upgrade.
- **Blueprint reach bypass** through an inherited exposure (actor/component subclasses, Blueprint-implementable interfaces, "Get Subsystem" nodes) or an engine function missing from the review. Mitigation: positive specifiers on every server type (lint rule g), an allowlist-first audit, a seeded-failure test per bypass kind; allowlist reviewed when CommonUI routing is decided (ADR-0021).
- **Include-name collision** across modules resolving to the wrong header. Mitigation: unique-name lint rule (f).
- **Event Log in a crash dump on the host.** Mitigation: minidumps without heap in playtest builds; residual risk accepted under IVP R10 until ADR-0024.
- **Class moves break reparented sample Blueprints.** Mitigation: `[CoreRedirects]` in the same change, explicit-path resave, snapshot before (VCS per ADR-0009; `_snapshots/` until then).
- **Harness accidentally linked into Shipping** via a stray dependency. Mitigation: no runtime Build.cs may name it (lint); a Shipping build in CI asserts the module is absent.
- **Over-strict folder rule slows prototyping.** Mitigation: throwaway work goes to `prototypes/` or the prototype module, never into production modules.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| information-visibility-policy.md | R10: presentation modules may not include server query headers (build-time include check); host reads only its C-TeamView, owner data and `FRoundPublicState`; no debug overlay that reads server data ships | `View/`/`Server/` split, `STEALTH_PRESENTATION_MODULE` `#error` guard, include lint, Blueprint audit; debug commands `!UE_BUILD_SHIPPING` (TR-ivp-011) |
| tuning-data-config.md | CR-10: client face typed to the public struct (compile-time guarantee, AC-28); reflective path returns invalid and logs with `ensure`, never `check`; client code holds no reference to the tuning asset class (AC-8 static check) | Facade split (`IStealthClientTuning`/public struct in `View/`, service in `Server/`); `ensure` policy (§6); asset class header in `Server/` so presentation cannot name it (TR-tdc-006) |
| participant-state-roster.md | R6 host parity: all presentation, incl. the listen host, reads only C-TeamView and public structs | Same guard and audit apply on the host because the host runs the same `StealthPresentation` code (TR-roster-009, compile-time half; run-time half is ADR-0001/0008) |
| round-match-framework.md | R19: Event Log never through `UE_LOG`, output log, `ClientMessage` or trace channels; host UI code may include only the public struct; dump command compiled out of shipping | Logging policy §6: no string conversions on Event Log types, lint on log/trace macro arguments, `UE_LOGF` categories; Event Log in `Server/` (TR-round-008) |

## Performance Implications
- **CPU**: None at run time (layout and interface indirection are one virtual call per cross-layer call; weak-pointer validity checks are negligible against the 0.5 ms queue budget).
- **Memory**: Negligible (a few module images; harness absent in Shipping).
- **Load Time**: Four small modules instead of one; no measurable change.
- **Network**: None.
- **Build time**: More modules improve incremental builds; the private PCH for `StealthPresentation`, if needed, adds one PCH compile.

## Migration Plan
1. (After ADR-0009 gives a repository.) Story "module skeleton": create `StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthTestHarness` with Build.cs files, empty `Public/View/`, `Public/Server/`, `Private/Tests/`, the guard header, module categories; add them to `StealthGame.uproject` (`StealthTestHarness` with `TargetConfigurationDenyList: ["Shipping", "Test"]`, and **not** to any Target `ExtraModuleNames`); move the `AStealthHUD` role into `StealthPresentation` when the HUD is rebuilt; keep `StealthGame` as the primary module and make it depend privately on the new modules.
2. Add the guard self-test, include lint and the Shipping-absence check (ADR-0008 wiring).
3. The prototype keeps building inside `StealthGame` unchanged; each prototype class is replaced by its production counterpart only when the governing ADR is Accepted, with `[CoreRedirects]` for Blueprint-referenced classes.
4. Prototype files are removed only with the user's approval (always-ask).

## Validation Criteria
1. **Guard self-test**: a CI probe compiling a `StealthPresentation` file that includes any `Public/Server/` header fails with the ADR-0007 `#error` text; the same include in `StealthGame` compiles. (Negative-compile probe mechanism: ADR-0008.)
2. **AC-28** (`StealthGame.Tuning.ServerKeyNotOnClient`): naming a server-only key through the client face fails to compile (probe), and the reflective path returns invalid with an expected `ensure`.
3. **AC-8 static half** (`StealthGame.Tuning.NoClientAssetReference`): no `StealthPresentation` source or widget Blueprint references the tuning asset class.
4. Include lint passes with zero exceptions on every commit; a seeded violation of each rule (a)-(e) is watched to fail once.
5. Blueprint audit passes; each seeded bypass is watched to fail once: a `GetGameMode` node in a test widget, a "Get Subsystem" node targeting a server subsystem, a widget property binding to a server class, a `Get Player Controller` with index 1, and a server UCLASS missing `NotBlueprintType` (lint rule g).
6. Shipping and Test builds contain no `StealthTestHarness` module; a cooked Development standalone loads it.
7. Event Log lint: a seeded `UE_LOGF` with an Event Log entry argument is watched to fail once.
8. Multi-client PIE (3 clients) and one seamless travel produce zero dangling-registration faults.

## Related
- ADR-0001 (crowd identity rules: carriers decide which types are View vs Server), ADR-0002 (queue value types and interfaces placed in Foundation), ADR-0003 (allowlist ownership), ADR-0004 (tuning faces), ADR-0005 (travel re-binding), ADR-0006 (seed mixer core), ADR-0008 (tests, lint and probe mechanisms), ADR-0009 (VCS; skeleton is the first commit), ADR-0024 (Dedicated Server tightening).
- `docs/architecture/architecture.md` — Module Ownership, Consumer-owned interfaces, Initialisation order, AQ-19.
- GDDs: `design/gdd/information-visibility-policy.md` R10, `design/gdd/tuning-data-config.md` CR-10/AC-8/AC-28, `design/gdd/participant-state-roster.md` R6, `design/gdd/round-match-framework.md` R19.
- Prototype evidence (read-only): `Source/StealthGame/StealthGame.Build.cs`, `Source/StealthGameEditor/StealthGameEditor.Build.cs`, `StealthGame.uproject` module list.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §3 include lint: rule (j) "no `CityGenerator/` file includes `Public/Server/Random/`" — requested by ADR-0006 as rule (i); lettered (j) because (i) already exists.
- Module table: `StealthFoundation` private `Json` dependency; `StealthGameplay/Features/Telemetry/` feature folder — requested by ADR-0023 (item 6).
- §4: `IOwnerCommandClientView` in `FStealthClientViewHandles`, carrying the `IAllyMarkerQuery` hand-back — requested by ADR-0021.
- Client tuning struct name `FStealthClientTuning` (not `FStealthPublicTuning`) — requested by ADR-0004; already the name used in §2 (facade split); no edit.

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- ADR Dependencies: Depends On is now "None"; ADR-0001/0002/0004 moved to the Ordering Note as content inputs, which breaks the declared Foundation cycles — architecture-review fix 2026-10-25 (dependency cycles).
- Module table, §4, Key Interfaces: `StealthFoundation` holds every queue/staged-write payload type, `FPawnHandle` and `FSubjectRef` in `Public/Server/`, plus `IStagedWriteApplier` and `IEventFlushBarrier`; a type a Foundation container names is declared in Foundation, and gameplay modules register resolvers and appliers by type at the composition root — architecture-review fix 2026-10-25 (Conflict 1, blocking).
- No rule change for Conflict 4: ADR-0011's headers now sit in `StealthGameplay/Public/Server/Core/` and pass lint rule (a); this ADR's rule is unchanged — architecture-review fix 2026-10-25 (Conflict 4).

Technical Setup 2026-10-25 (Technical Director gate findings, `production/gate-checks/2026-10-25_systems-design-to-technical-setup.md`); decision unchanged, Status unchanged (Accepted).
- §1 (AQ-19 confirmed closed): the module set is the four runtime modules of §1 (`StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthGame`) plus `StealthGameEditor` and `StealthTestHarness`; the seven-module variant stays rejected. `architecture.md` AQ-19 is marked closed against this section.
- Module table, §2, §4 (Informant Pings, system 20, `design/gdd/informant-pings-team-comms.md`): Pings is a feature folder `StealthGameplay/Features/Pings/`. Server: `UTeamPingService` (`UWorldSubsystem`, `Public/Server/`) over a headless `FTeamPingCore` (validation order, `ParticipantId`-keyed token bucket, slot writes; plain C++ for `StealthGame.Pings.*` specs). View: `UTeamPingViewComponent` / `FTeamPingView` and the client-local ping-point and bucket-mirror helpers in `Public/View/` (the presenter in `StealthPresentation` reads the part with `FindComponentByClass<>()` on its own controller, never a server header). The composition root binds `IA_Ping` and declares `ServerTeamPing` on `AStealthPlayerController` (ADR-0011 amendment) and forwards it to `UTeamPingService`. Round (Core) clears the parts at the round-end step only through a new Core consumer-owned interface **`ITeamPingReset`** (`UINTERFACE`, `CannotImplementInterfaceInBlueprint`; one method `ClearAllTeamPings()`), implemented by `UTeamPingService` and registered weakly by the composition root from `InitGame`/`OnWorldBeginPlay` beside `ICrowdControl`; Pings clears itself at `OnRosterReset` and `InvalidateAllBindings` by subscribing to the Roster (Feature → Foundation, no inversion needed). `Core/` still never includes `Features/`.
