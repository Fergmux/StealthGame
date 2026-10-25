# Smoking Mirror — Master Architecture

## Document Status

- Version: 1 (first issue; revised in place the same day after the Phase 7b gates, before any ADR cited it)
- Last Updated: 2026-10-24
- Engine: Unreal Engine 5.8 (installed 5.8.4, CL 58902052, `C:/Program Files/Epic Games/UE_5.8`), C++ primary, Blueprint for presentation and tuning assets
- Authoring: `/create-architecture full` (autonomous run; rigor full, docs.density thorough; every choice point logged in `production/session-logs/decision-log.md` under `create-architecture`)
- GDDs Covered (18 MVP): tuning-data-config, participant-state-roster, input-interaction-framework, character-pool-appearance, session-travel, round-match-framework, information-visibility-policy, level-socket-contract, shared-character-locomotion, camera-system, crowd-npc-behaviour, concealment-bushes, assassination-defence-reversal, npc-kill-penalty-informants, objectives-chain, procedural-city-generator, teammate-markers, hud. Also read: game-concept.md (Technical Considerations), systems-index.md, gdd-cross-review-2026-10-24.md (verdict after fixes: blockers resolved, warnings open), design/registry/entities.yaml (247 cross-GDD entries, used as the knob/name baseline), design/art/art-bible.md §8.x (budgets).
- GDD state caveat: all 18 GDDs are "In Review" (one review + fix pass each, independent re-reviews pending, none Approved). This architecture is written against that state; a GDD change after re-review goes through `/propagate-design-change`.
- ADRs Referenced: none exist yet (Phase 5 audit found no `docs/architecture/adr-*.md`). The 25 required ADRs are listed under **Required ADRs**; ADR-001 is first.
- TR-IDs: the TR registry (`docs/architecture/tr-registry.yaml`) is empty, so every ID in Appendix A is **provisional**; `/architecture-review` registers them and assigns permanent IDs.
- Evidence policy: the prototype in `Source/StealthGame` and `production/first-playable-report.md` are cited as feasibility evidence only. They are not this design (the prototype replicates `APawn::PlayerState`, `Team` and `bConcealed` to everyone and hides bushes client-side; report known issue 2).
- TD-ARCHITECTURE: 2026-10-24 — REVISED after CONCERNS (8 concerns: dependency inversion for Crowd/Concealment edges, ADR-001 narrowed with S1/S14 fallback and bootstrap order, client server-time estimator owned by ADR-002, compile guard + Blueprint audit, queue keeps latch storage only with typed staged writes, Iris default verified and pinned, harness module type, audio scope)
- LP-FEASIBILITY: 2026-10-24 — REVISED after CONCERNS (12 findings: per-service View facades, wire structs moved to Foundation `View/`, core value types defined in ADR-002 scope, registration/lifetime rules, custom drain `FTickFunction`, plain-C++ testable cores, metadata-driven tuning schema, four-module layout, CPU budget table and target-hardware spike, Roster-first build, seed in replicated GameState, pawn-factory subsystem)
- Gate re-review: not re-run on the revised text (autonomous run); the next `/architecture-review` is the check.

---

## Engine Knowledge Gap Summary

**Engine:** Unreal Engine 5.8 (5.8.4 installed). **LLM training covers:** reliably to about 5.3. **Post-cutoff versions:** 5.4, 5.5 (pre-cutoff but unreliable), 5.6, 5.7, 5.8. `docs/engine-reference/unreal/` was written against 5.7 with a 5.8 delta pass; its module pages are tutorial-level and carry **no** material on motion matching (PoseSearch), the significance manager, the animation budget allocator, Iris filtering, seamless travel, StateTree or PlayerState replication. Every engine claim below is tagged:

- **[V]** read from the installed 5.8.4 headers/source during this session or by a GDD specialist (file cited in the GDD);
- **[D]** stated by `docs/engine-reference/unreal/` (5.7 text plus 5.8 delta);
- **[U]** unverified: from memory or inference; must be confirmed in the ADR that relies on it.

### Verified this session against the 5.8.4 install

| Claim | Evidence |
|---|---|
| `APawn::PlayerState` replicates unconditionally (`DOREPLIFETIME(APawn, PlayerState)`) | [V] `Engine/Private/Pawn.cpp:1307` |
| `APlayerState` is always relevant; `AGameStateBase::PlayerArray` is a non-replicated `UPROPERTY(Transient)` rebuilt on each client | [V] `PlayerState.cpp:26`, `GameStateBase.h:54-55` |
| `RESET_REPLIFETIME_CONDITION` / `ResetReplicatedLifetimeProperty` exist (condition reset in a subclass) | [V] `Public/Net/UnrealNetwork.h:455-485` |
| `APlayerController::PawnLeavingGame()` is virtual | [V] `PlayerController.h:2054` |
| `APawn` calls `ForceNetUpdate()` on possession paths | [V] `Pawn.cpp:434, 678, 717` |
| `RemoteViewPitch16` is current; 8-bit `RemoteViewPitch` deprecated in 5.6 | [V] `Pawn.h:129-134` |
| `AActor::bReplicateUsingRegisteredSubObjectList` and the registered sub-object list exist | [V] `Actor.h:627, 970` |
| `AGameModeBase::bUseSeamlessTravel`, `GetSeamlessTravelActorList`, `HandleSeamlessTravelPlayer` exist | [V] `GameModeBase.h:239, 262, 579` |
| Iris per-connection group filters: `AddExclusionFilterGroup`, `SetGroupFilterStatus` | [V] `Net/Iris/Public/Iris/ReplicationSystem/ReplicationSystem.h:660-668` |
| Iris is **off by default**: `net.Iris.UseIrisReplication` defaults to 0 ("0 will fallback to legacy"); `BaseEngine.ini` marks GameNetDriver `bCanUseIris=true`, so a command line `-UseIrisReplication=1` (or the cvar) flips the game driver to Iris; UBT rule sources ship (`ModuleRules.cs:1747 SetupIrisSupport`) | [V] `Net/Iris/Private/Iris/IrisConfig.cpp:15-16`; `Config/BaseEngine.ini:346`; `Programs/UnrealBuildTool/Configuration/Rules/ModuleRules.cs:1747` (found by the TD gate, re-checked) |
| `DISABLE_REPLICATED_PROPERTY` exists next to `RESET_REPLIFETIME_CONDITION`; `APawn` replicates `PlayerState` and `Controller` unconditionally | [V] `UnrealNetwork.h`; `Pawn.cpp:1307-1308` (LP gate spot check) |
| Plugins present: PoseSearch, MotionWarping, AnimationLocomotionLibrary, AnimationBudgetAllocator (needs `USkeletalMeshComponentBudgeted`), SignificanceManager, StateTree, GameplayStateTree, CommonUI | [V] `Engine/Plugins/...`; `IAnimationBudgetAllocator.h:27` |
| Project config: `r.SkinCache.DefaultBehavior=0` (Exclusive); no Iris switch, no `r.Substrate` key in `DefaultEngine.ini`; the `.uproject` enables `Mover` and `NetworkPrediction` (inherited from the Game Animation Sample) | [V] `Config/DefaultEngine.ini`, `StealthGame.uproject` |

### HIGH RISK domains (verify against the engine before deciding)

- **Networking.** Iris is "production-ready" per the 5.8 release notes but its plugin is still flagged Beta [D]; it is off unless `net.Iris.UseIrisReplication=1` [V], so the project pins the cvar to its chosen value in `DefaultEngine.ini` and the leak harness asserts the active replication system at startup (ADR-001/ADR-008). Owner-only resets of `APawn::PlayerState/Controller` and `AActor::Owner` without breaking possession or RPC routing (spikes S1, S14) are unproven [U]. The legacy driver's relevancy close lags up to `min(RelevantTimeout, 1 s)` + 0.5 s jitter, so the concealment cull needs an explicit per-connection channel close (S9) [V per IVP R7]. Seamless travel behaviour in PIE may not match standalone (S3) [U].
- **Animation.** Motion matching (PoseSearch) cost per search at 50 characters is unknown (spike R3) [U]; Nanite skeletal exists in 5.8.4 but its readiness and motion-matching interaction are unknown (R6) [V exists / U status]; 5.8 removed deprecated animation symbols (Game Animation Sample code paths must compile) [D].
- **UI and input.** Enhanced Input and Common UI are unified in 5.8 ("no duplicate data assets") [D]; how CommonUI action routing composes with gameplay mapping contexts under that unification is not documented in the reference [U]. `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (5.8.4 name [V] by ADR-0011; the engine reference calls it `IgnoreHeldKeysOnFlush`) defaults true [D] (affects hold edges after a context flush; IIF R4 depends on clean edges).
- **Rendering.** Substrate state for this project is unknown (no `r.Substrate` key; existing-project default applies) [U]; skin cache is Exclusive in config [V], costly with VSM at 50 characters (spike R2); Nanite + WPO status contradictory between reference and source (R9).

### MEDIUM RISK domains (verify key APIs)

- **Navigation.** Runtime navmesh build after seeded assembly, time-budgeted (≤ 3 s server, spike R4); 5.8 per-mesh nav areas [D]; `FindPathAsync` on the task graph [V per Crowd CNB-R15].
- **AI.** StateTree AI component and scheduled ticks (`UStateTreeComponent::ScheduleTickFrame`, `FStateTreeScheduledTick::MakeSleep`, `UStateTreeAIComponent`) [V per Crowd GDD].
- **Movement.** CharacterMovementComponent is pre-cutoff and stable; Mover/ChaosMover are Experimental and PhysicsMover was removed in 5.8 [D]. The sample's replicated traversal (`Traversal_ServerImplementation` / `OnRep_TraversalResult`) is unproven under loss (net-emulation spike).
- **Audio.** WASAPI default and relative-render-cost voice limit on by default [D] (footstep parity at 50 characters must be checked against voice limiting).
- **Build.** C++20 minimum; VS 2022 17.14 present [V active.md]; `UE_LOGF` preferred [D].

### LOW RISK domains

Core UObject/GC, gameplay framework classes (GameMode/GameState/PlayerController), DeveloperSettings, automation tests (`Automation RunTests` headless with `-nullrhi`, 12/12 passing on 5.8.4 per the first playable report), `FRandomStream`-style integer streams.

### Systems touching HIGH/MEDIUM domains

| System | Domain | Risk |
|---|---|---|
| Information Visibility Policy, Participant State & Roster, Session & Travel | Networking (pointer suppression, PlayerState trimming, travel) | HIGH |
| Shared Character & Locomotion | Networking (parity config, possession), Animation (motion matching, significance) | HIGH |
| Concealment (Bushes) | Networking (cull), Rendering (hidden shadow/Lumen/RT leaks) | HIGH |
| Assassination & Defence Reversal | Networking (fixed-width serialiser, forced updates, contact timing) | HIGH |
| Character Pool & Appearance | Rendering (residency, PSO precache, skin cache) | HIGH |
| Camera System | Rendering (per-pawn dither, hide incl. shadow/RT), Physics (trace channel) | HIGH |
| HUD, Teammate Markers | UI (UMG layering, CommonUI/EI unification) | HIGH |
| Crowd & NPC Behaviour | AI (StateTree), Navigation | MEDIUM |
| Level Socket Contract, Procedural City Generator | Navigation (runtime build), Rendering (ISM/Nanite/Lumen) | MEDIUM (R1/R4 spikes) |
| Input & Interaction Framework | Input (Enhanced Input edges), Networking (CMC saved moves) | MEDIUM |

---

## System Layer Map

```
┌────────────────────────────────────────────────────────────────────────────────┐
│ PRESENTATION  (client-local; reads only carriers: owner data, C-TeamView,      │
│               FRoundPublicState, tuning client snapshot)                       │
│   HUD (18) · Teammate Markers (17)                                             │
├────────────────────────────────────────────────────────────────────────────────┤
│ FEATURE       (gameplay meaning; registers queue resolvers; server-authoritative)
│   Crowd & NPC Behaviour (11) · Concealment (12) · Assassination & Defence (13) │
│   NPC-Kill Penalty & Informants (14) · Objectives Chain (15)                   │
│   Procedural City Generator (16, built last; an LSC provider)                  │
├────────────────────────────────────────────────────────────────────────────────┤
│ CORE          (the shared body, its commands, its look, its world, its round)  │
│   Shared Character & Locomotion (9) · Input & Interaction Framework (3)        │
│   Character Pool & Appearance (4) · Level Socket Contract / Layout Service (8) │
│   Round & Match Rules (6: phases, wins, disconnect policy, host commands)      │
│   Camera System (10, client-local)                                             │
├────────────────────────────────────────────────────────────────────────────────┤
│ FOUNDATION    (contracts every other layer builds on; no gameplay meaning)     │
│   Tuning Data & Config (1) · Participant State & Roster (2)                    │
│   Session & Travel (5) · Information Visibility Policy (7: net identity        │
│   boundary, carriers, allowlist) · Resolution Queue & Server Clock (Round-owned│
│   contract: queue, mailboxes, due-time heap, pending-write staging)            │
│   Seeds & RNG service (Session F5) · Game-instance store container · Fault sink│
├────────────────────────────────────────────────────────────────────────────────┤
│ PLATFORM      (not project code)                                               │
│   UE 5.8.4: CharacterMovementComponent, legacy NetDriver (Iris optional),      │
│   Recast navigation, PoseSearch/Chooser/MotionWarping (Game Animation Sample), │
│   StateTree, AnimationBudgetAllocator, Enhanced Input, UMG/CommonUI,           │
│   Lumen/Nanite/VSM, Chaos, Automation framework, OnlineSubsystem Null          │
└────────────────────────────────────────────────────────────────────────────────┘
```

Numbers are the systems-index `#`. VS/Alpha systems (19-30) are out of this document's build scope; their hooks are noted where an MVP module must leave room (Event Log release classes, informant ping slot, settings hooks).

### Layer rationale (thorough)

**Foundation.** The systems-index places Tuning and Roster in Foundation and everything else from 3 to 11 in Core. This architecture moves three more contracts down, because every gameplay module resolves *through* them and none of them carries gameplay meaning:

1. **Session & Travel** owns the connection table, seeds and the single readiness acknowledgement. Roster, Tuning and Round consume its events; it consumes only the Roster's registration API (a peer at the same layer; Session calls Roster, never the reverse).
2. **Information Visibility Policy** becomes the *net identity boundary*: `CanViewerKnow`, the four carrier kinds, the replicated-property allowlist and the default overrides of R5. It is a rule set and a framework, not a feature; it reads Roster facts and Round's phase only through an injected phase provider, so the dependency on Round is a callback, not a layer inversion.
3. **Resolution Queue & Server Clock** is Round's *contract*, split from Round's *rules*. IIF, Assassination, Objectives, Penalty and Crowd all submit items and register resolvers, and the systems-index already notes that "the interface owner is built first, as a header/contract". The queue (Submit, Key, Compare, mailboxes, due-time heap, drain tick, the pending-write staging queue that ASN-R21 and Round R9 share) sits in Foundation; phases, win latch policy, team rules and disconnect policy stay in Core.

*Alternative considered:* keep the index's layers verbatim (Round entirely Core). Rejected: IIF (Core) would then depend on Round (Core) and Round on IIF (the mutual edge the index records), and the queue contract would be built at the same time as its consumers. Splitting contract from rules removes the cycle from the build order.

**Core.** The single Shared Pawn, the pawn command interface, the appearance allocator, the layout service, the round rules and the camera. These are the pieces the MVP-core checkpoint (week ~6, graybox, 2-client PIE) needs before any feature. Camera is client-local but stays in Core because the IIF consumes its aim ray (`GetAimRay`, CS-R8) and SCL's tiering consumes its eye-mask flag.

**Feature.** Gameplay meaning: Crowd, Concealment, Assassination, Penalty/Informants, Objectives, and the Generator (a second LSC provider). *Change from the index:* Crowd moves from Core to Feature. It depends on Round, IIF, SCL, LSC and CPA (all Core). Three Core consumers need something from Crowd at run time (Round asks it to drive a disconnected pawn and to Halt; LSC asks which crowd pawns are eligible for takeover; the Shared Pawn's hand-over gives a released pawn to it), and the Visibility Policy (Foundation) needs Concealment's commit events. Those edges are **inverted** (TD-ARCHITECTURE concern 1): the lower layer defines a small consumer-owned interface (`ICrowdControl`, `ITakeoverEligibilitySource` in Core; `IConcealmentCullSource` push API in the Visibility Policy), the feature implements it, and the composition root registers the implementation at world start. No lower layer names a feature type. Crowd is first in the Feature build order.

**Presentation.** HUD and Teammate Markers read only carriers and client-local queries (Camera view state, Concealment's client latch, IIF reply sink). They write nothing replicated.

**Engine awareness (Core and Foundation on HIGH/MEDIUM domains).** Visibility Policy, Roster, Session → Networking HIGH (spikes S1-S15, verified facts above). Shared Pawn → Networking HIGH + Animation HIGH. Camera → Rendering HIGH (hide must remove mesh, shadow and ray-tracing presence: `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden`, `bVisibleInRayTracing` exist [V art bible 8.x.11]). Layout Service → Navigation MEDIUM. Tuning → LOW (data assets, `UGameInstanceSubsystem` [U name verified by usage in prototype only]).

---

## Module Ownership

### Code module layout (proposal; ADR-007 decides)

*Revised after LP-FEASIBILITY concern 8 and TD-ARCHITECTURE concerns 1, 4, 7.* The first draft proposed seven runtime modules (Foundation, Core, Features, Presentation, root). UBT forbids module cycles, so every Core→Feature run-time edge would have needed an interface plus `*_API` exports across a module line, which is heavy for 1-2 programmers. The recommended layout is **four runtime modules**; the Core/Feature layering inside `StealthGameplay` is enforced by folders, the include-lint and the consumer-owned interfaces below, not by UBT.

| UE module | Type | Contents | May depend on |
|---|---|---|---|
| `StealthFoundation` | Runtime | Tuning service + validator; Roster store; Session connection table + seeds; Visibility Policy + carrier bases + allowlist data; Resolution Queue + server clock + staged writes; Event Log store; fault sink; **wire and view structs shared across layers** (`FActionState`, `FDeathState`, `FStrikeResult`, `FDefenceResult`, `FRoundPublicState`, ids and tokens) in `Public/View/` | Engine, NetCore, DeveloperSettings |
| `StealthGameplay` | Runtime | `Core/` folder: Shared Pawn + movement + AnimInstance base + significance, pawn factory subsystem, Pawn Command Component + action hub, Character Pool + allocator, Layout Service + Graybox provider, Round rules, Camera (client). `Features/` folder: Crowd, Concealment, Assassination, Penalty, Objectives, City Generator; feature team-view components (e.g. `UObjectiveTeamViewComponent`) declared in `Public/View/` | `StealthFoundation`, EnhancedInput, AIModule, NavigationSystem, PoseSearch, MotionWarping, StateTree, GameplayStateTree |
| `StealthPresentation` | Runtime (client code) | HUD view-models and widgets; Teammate Markers; owner presentation helpers | `StealthFoundation` and `StealthGameplay` **`View/` headers only** (compile guard below), UMG, CommonUI |
| `StealthGame` | Runtime (composition root) | `AStealthGameMode` (`AGameModeBase`), `AStealthGameState`, `AStealthPlayerController`, `AStealthPlayerState`; registers resolvers, providers and interface implementations from `InitGame` / `OnWorldBeginPlay` | all of the above |
| `StealthGameEditor` | Editor | MCP toolsets (existing), tuning lint, allowlist generator, widget-Blueprint server-reference audit, asset audits, golden-seed tools | all runtime modules |
| `StealthTestHarness` | **Runtime, excluded from Shipping targets** (TD concern 7: an UncookedOnly module would not load in the cooked multi-process Development runs that S3 and the raw-bunch leak suites need) | multi-client and multi-process leak harness, raw-bunch capture, net-emulation profiles, replication-system assertion, test seams | all runtime modules |

**Include boundary (IVP R10, TDC CR-10; TD concern 4, LP concern 1).** UBT exposes every `Public/` header of a dependency, so a lint alone is not a boundary. The architecture therefore uses three layers of enforcement:
1. **Compile guard.** `StealthPresentation.Build.cs` defines `STEALTH_PRESENTATION_MODULE=1`; every `Public/Server/*.h` header includes `StealthServerOnlyGuard.h`, which `#error`s under that define.
2. **Facade split per service.** A UCLASS lives in one header, so a service that has both a server face and a client face (Tuning, Roster views, Layout, Round) is split into a small `View/` facade (interface or UCLASS) and a `Server/` implementation. Example: `IStealthClientTuning` (View) vs `UStealthTuningService` (Server).
3. **Blueprint reach.** No `BlueprintCallable`/`BlueprintType` on any server-only type or function; an editor audit scans widget Blueprints for references to server classes (ADR-007/ADR-008).
The tuning client face is a distinct struct type, so a server-only key cannot be named in client C++.

**Testable cores (LP concern 6, 10).** The queue, the Roster, the tuning validator, the seed mixer and the appearance deck are written as plain C++ classes with no `UWorld` dependency, wrapped by thin subsystem facades, so their specs run headless without a world. Roster is built first, as a pure core with property tests (largest effort risk: eleven result codes, epoch tokens, tombstones, re-entrant FIFO event flush).

**Lifetime and registration rules (LP concern 4, 5, 12).** Resolvers, providers and interface implementations register from `AStealthGameMode::InitGame` / `OnWorldBeginPlay` (never from module startup, when no world subsystem exists), using `Collection.InitializeDependency` for subsystem ordering. Registrations are weak (`TWeakInterfacePtr` / `TWeakObjectPtr`), cleared in `Deinitialize`; game-instance stores bind to world objects only with `AddUObject`-style weak bindings and re-bind after each seamless travel. Stores that refer to pawns hold `TWeakObjectPtr`. The queue drain is a custom `FTickFunction` registered in `TG_PostPhysics` against the persistent level in `OnWorldBeginPlay` (a `UWorldSubsystem` has no tick-group control). Pawn creation lives on a pawn-factory world subsystem (`UStealthPawnFactory`), not free functions.

### Consumer-owned interfaces (dependency inversion, TD concern 1)

| Interface | Defined in (layer) | Implemented by | Used by | Purpose |
|---|---|---|---|---|
| `ICrowdControl` | Core (`StealthGameplay/Core`) | Crowd Director (Feature) | Round (disconnect hand-over, Halt in the round-end step), Shared Pawn hand-over (`ReleaseToCrowd` adoption) | drive / release / adopt a pawn, Halt |
| `ITakeoverEligibilitySource` | Core | Crowd Director | Layout Service `PickRespawnPawn` | per-pawn eligibility reason for informant takeover |
| `IConcealmentCullSource` (push API) | Foundation (Visibility Policy) | Concealment (Feature) pushes `SetCullState(Pawn, bConcealed, CommitServerTime)` | Visibility Policy cull hook | the Policy never names a feature type |
| `IQueueResolver` | Foundation (Queue) | IIF, Assassination, Objectives, Penalty, Round | Queue drain | typed item resolution |
| `ILayoutProvider` | Core (Layout Service) | Graybox provider (Core), City Generator (Feature) | Layout Service | layout build |
| `ITerminalDecision` | Foundation (Queue) | Round rules (Core) | Queue latch storage | Round decides termination; the queue only stores the latch (TD concern 5) |

The composition root wires every row at world start. Round still reaches Crowd for *Halt* through `ICrowdControl` rather than the `OnRoundEnded` event alone, because R9 requires the halt inside the same server step as the publication (an event subscriber would satisfy that too; ADR-002 picks one and records it).

**Prototype promotion rule.** Nothing in the current `Source/StealthGame` is promoted until the ADR that governs it is Accepted (concept Scope Risks). The prototype's `UStealthSettings` (`Config` properties in `DefaultGame.ini`) violates TDC CR-13 and is replaced, not migrated.

### Foundation layer

| Module | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Tuning (TDC)** | Tuning asset class (per-owner sub-structs split by audience: `FXxxTuning_Public` / `FXxxTuning_Server`; range, latch and audience in UPROPERTY metadata read by a reflection validator, so the schema is declared once — LP concern 7; the cooked-build metadata-stripping caveat of CR-3 is met by generating a compiled schema table from that metadata at build time [U]), catalog, preset store (game-instance), `ValidatedTuning` per round, `TuningHash`, `ClientHash`, `ValidationReport` | `BeginResolve()`, `GetServerTuning()`, `GetClientTuning()`, `OnTuningReady(TuningHash)`, `OnTuningChanged` (PIE), `SchemaVersion` | Round's `BeginResolve` call; Session's ack echo; host `?Tuning=` (non-Shipping) | `UPrimaryDataAsset` / soft refs, `UGameInstanceSubsystem`, `UWorldSubsystem`, replicated snapshot on GameState (LOW); cook rules for un-referenced presets [U, AC-27] |
| **Roster** | `ParticipantRecord` store (match/round scope), pawn bindings + tombstones, `MatchId`, `RoundEpoch`, `EventSeq` | `RegisterParticipant`, `AssignTeams`, `SetTeamsLocked`, `ResetAll`, `EndRoundAll`, `RequestTransition`, `BindPawn`/`RetireBinding`/`ReleaseToCrowd`/`ClearBinding`/`InvalidateAllBindings`, queries (`ResolveVictimKind`, `GetRelation`, `GetLegalityClass`, `IsPawnBound`, counts), events (`OnParticipantStateChanged`, `OnRosterRoundEnded`, `OnRosterReset`, `OnBindingLost`, ...) | Session (`NotifyConnectionChanged`, registration), Round (batches), features (transitions) | Game-instance store; C-Public on `APlayerState` (field trimming: `RESET_REPLIFETIME_CONDITION` [V], `bShouldUpdateReplicatedPing` [U]); C-TeamView on `APlayerController` (owner-only relevancy [V per Roster R6]) — HIGH |
| **Session & Travel** | Connection table (`ConnectionState`, generations), IdentityKey mapping, admission mode, seed + sub-seed derivation, travel deadline, single round acknowledgement | `RequestRoundTravel`, `RequestLobbyTravel`, `RequestFallbackTravel`, `SetAdmissionMode`, `GetConnectionState`, `KickParticipant`, `PublicSubSeed`, `GetServerSubSeed`, events (`OnParticipantUnresponsive/Disconnected/Rejoined`, `OnParticipantTravelComplete`, `OnSessionEnding`) | Roster registration API; Round's `ShouldAdmitExpired`; TDC `ClientHash`; LSC `LayoutHash`; CPA `PoolHash` | `AGameModeBase::PreLogin` / `Login` / `PostLogin`, seamless `ServerTravel` (`bUseSeamlessTravel`, `GetSeamlessTravelActorList` [V]), net driver timeouts [U], OnlineSubsystem Null — HIGH (S3, S4, S5) |
| **Visibility Policy (IVP)** | `CanViewerKnow` matrix (7 viewer classes), fact table, carrier rules, replicated-property allowlist file, leak catalogue, concealment cull timing | `CanViewerKnow(Viewer, Subject, Fact)`, `GetViewerClass`, cull hook contract (`ShouldCullFor(Pawn, Connection)`), `IConcealmentCullSource` push API, allowlist data + audit, team position fallback feed builder | Roster facts, Round phase (injected provider), cull state pushed by Concealment through `IConcealmentCullSource` | `IsNetRelevantFor` override + explicit channel close (legacy, S9) or Iris exclusion group filters [V]; `GetLifetimeReplicatedProps` reflection for audit — HIGH |
| **Resolution Queue & Server Clock** | Per-round queue, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake, due-time heap, drain tick (custom `FTickFunction`, `TG_PostPhysics`), **latch storage only** (the terminate decision is Round's, injected through `ITerminalDecision`), **typed staged-write items** (server-time-scheduled, epoch-stamped, logged), server clock; the **client server-time estimator** and its accuracy budget (TD concern 3) | `Submit(Source, Item)`, `RegisterResolver(EQueueItemType, Resolver)`, `ScheduleAt(ServerTime, Item, ExpectedEpoch)`, `Compare(KeyA, KeyB)`, `StageWrite(FStagedWrite)`, `IsLatched()`, `ServerNow()`; client `EstimatedServerNow()` | Resolver registrations from IIF, ASN, OC, PEN, Round; Roster event flush | `FTickFunction` in `TG_PostPhysics` [V tick order per Round R11]; spawn/destroy inside drain [S, deferred-teardown fallback] — MEDIUM |
| **Seeds & RNG** | Public sub-seed mixer, `ServerSecret`, keyed sub-seed derivation per purpose, DRBG primitive choice | `PublicSubSeed(Purpose, Index)`, `GetServerSubSeed(Purpose, Index)` (server assert if called from Generator), integer stream type — *ADR-0006 (Proposed): `(ESeedDomain Domain, uint32 Attempt)`, SipHash-2-4 keyed sub-seeds, `FSecretStream` (SipHash counter mode) and PCG32 `FIntStream`* | Session (secret at session open) | none beyond platform crypto/hash [primitive decided by ADR-0006] — LOW |
| **Event Log & fault sink** | Round Event Log ring + header (server memory only), concealment log slice, fault sink | `Append(Entry)`, `ArchiveSlice`, release-class gate (via IVP), `ReportFault(Code)` | All server systems | Must not route through `UE_LOG`, output log or trace channels (Round R19) — LOW |

### Core layer

| Module | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Shared Pawn (SCL)** | `AStealthPawn : ACharacter` (one class), movement component config, pawn-factory world subsystem (`UStealthPawnFactory`: spawn, takeover), possession parity, net parity config, smoothing, speed tiers, traversal set, footstep clock, published state (`IsRunning`, `IsInTraversal`, `CommandedDisplacementCm`) | `SpawnSharedPawn`, `TakeOverCrowdPawn`, `HandOver(ToAi/ToPlayer)`, published state reads, replicated `HeadLook`, `TraversalState`, `AppearanceId` application | IIF (commands), Roster (bind/clear), CPA (appearance), Tuning (SCL keys), IVP (allowlist, cull hook), `ICrowdControl` (hand-over to AI) | `ACharacter`/CMC [D], `PreReplication` override writing `RemoteViewPitch16` [V], `ForceNetUpdate` override [V calls], `PawnLeavingGame` override [V], `NetUpdateFrequency`/adaptive frequency/dormancy config [U values], Game Animation Sample traversal RPCs [U under loss] — HIGH |
| **Animation & significance (SCL R10-R12)** | One AnimBP class, motion-matching database set, chooser set, look-at solver, tier function (screen height only), TierTieKey | `ComputeTier(Pawn, View)`, tier caps, `ActionPose`-driven states, contact-time clip seeking | Camera eye-mask flag (rendered state), `FActionState`, `FDeathState` | PoseSearch [V present, U cost], URO `bEnableUpdateRateOptimizations` [V], `VisibilityBasedAnimTickOption` [V], AnimationBudgetAllocator (`USkeletalMeshComponentBudgeted`) [V present] or SignificanceManager [V] — HIGH. *ADR-0013 (Proposed) rules out engine URO (player-keyed in 5.8.4), the AnimationBudgetAllocator and the SignificanceManager; update-rate skips come from the project's own external tick-rate control.* |
| **Pawn Command Interface (IIF)** | `UPawnCommandComponent` (non-replicated), ingress checks, token buckets, slot arbitration, hold timers (as queue items), release debounce, replies, `FActionState` writes, action hub | `Move`, `Look`, `Traverse`, `InteractBegin/End`, `PrepareBegin/End`, `Attack`, `Hide(Enter/Exit)`; `UActionHub` events `OnActionStarted/Cancelled/Completed`; `CanBegin` predicate registration per feature; `ClientCommandRejected` owner reply | Queue (submit/resolve), Roster `GetLegalityClass`, SCL state, Camera `GetAimRay` (client), features' predicates | Enhanced Input `Started/Completed/Canceled` edges [D], CMC saved moves (`ServerMove`) [D/U 5.8 detail], server RPCs reliable/unreliable [D] — MEDIUM |
| **Character Pool (CPA)** | `UCharacterPoolData` rows (AppearanceId 0-23), deck per round, card ledger, swatch rule | `AllocateAppearances(Players, NpcCount)`, `DrawReplacement()`, `TakeNpcCard()`, `GetAppearanceRow(Id)`, `PoolHash`, `DealDigest` (server-only) | Tuning (deck keys), Seeds (`AppearanceDeal`, `Replace`, `Take` streams), Round (caller) | Hard references + residency, PSO precache [U], texture streaming [U], skin-cache per-mesh opt-in [V config Exclusive] — HIGH |
| **Layout Service (LSC)** | Layout service world subsystem, provider registry, `FLayoutManifest`, `LayoutHash`, validation (V-*), Selection (`FSiteAssignment`, TeamSpawn, server-only), emergence sockets, respawn pick | `RegisterProvider`, `RequestLayout(Seed, TuningHash)`, `OnLayoutReady`, `RequestStartPlacement`, `PickRespawnPawn(Team, RequestSeq, ExcludePawn)`, server query API for sites; Graybox provider | Tuning (LSC keys), Session (seed), Roster (`GetTeam`), `ITakeoverEligibilitySource` (implemented by Crowd), navigation | `UWorldSubsystem` [U name], Recast runtime build [D], `FindPathSync`/`ProjectPointToNavigation` [D] — MEDIUM |
| **Round & Match Rules** | Phase machine (game-instance), team rules, start validity, Preparing sequence, win conditions + latch policy, round-end step, disconnect policy, host commands, `FRoundPublicState`, Event Log entries | `StartRound`, `ResetRound`, `AbortRound`, `DeclareObjectiveWin(Team, Key)`, `TryTerminate`, phase events (`OnRoundActive`, `OnRoundEnded`), `ShouldAdmitExpired` | Queue (implements `ITerminalDecision`), Roster, Session, Tuning, LSC, CPA, `ICrowdControl` (Halt, hand-over), staged writes queued by Assassination | GameState replicated struct with RepNotify [D]; console commands on game instance [U] — MEDIUM |
| **Camera (client)** | 1P/3P modes, view envelope, boom, eye-origin visibility mask, aim ray, view state, listener placement | `GetAimRay()`, `GetViewState()`, `IsEyeMaskVisible(Pawn)` (also feeds SCL tiering), owner presentation hooks | SCL (eye pivot, body facing), IIF (look input), Concealment (own `IsConcealed`), Tuning (client keys) | `APlayerCameraManager` (custom) [D], dedicated `ViewBlock` trace channel [U], per-pawn dither via custom primitive data or per-pawn MID on skeletal mesh [U] (*ADR-0015 (Proposed) chose the custom primitive data slot `CPD_EyeMaskAlpha` and rate bands at every range; no per-pawn MID*), `SetVisibility(false)` removes shadow + RT [U, CS-R5] — HIGH |

### Feature layer

| Module | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Crowd** | Crowd Director (ledger, ambient schedule, density, stimulus fan-out, Halt), per-NPC StateTree + follower, 2 m spatial hash, eligibility reasons, Baseline & Tell Matrix rates, counter-based draws | implements `ICrowdControl` (`Halt()`, `DriveBoundPawn`, `ReleaseControl`, `AdoptReleasedPawn`) and `ITakeoverEligibilitySource`; replacement scheduling | IIF entry points (only path), SCL factory, LSC emergence, CPA `DrawReplacement`, Round phase, action hub stimuli, Seeds (`CrowdBehaviour`) | `UStateTreeAIComponent`, scheduled ticks [V], `FindPathAsync` [V], AIController with path-following not ticking + `UpdateControlRotation` override [U] — MEDIUM |
| **Concealment** | Per-pawn component + server record (`Outside/Entering/Concealed/Exiting/Emerging`), bush hide volumes built from Manifest, client latch `IsAllyBodyHidden`, concealment log | `CanBegin(Hide)`, `IsConcealedFrom(Viewer, Pawn)`, `OnConcealmentCommitted/Broken/Exited`, client `IsAllyBodyHidden(Pawn)` | IIF, IVP (pushes cull state via `IConcealmentCullSource`), LSC Manifest, Roster `GetRelation`, Tuning | Root-motion warp to anchor with owner-side prediction (S-CON-1) [U], hidden-primitive flags [V art bible], collision profiles — HIGH |
| **Assassination** | Attack resolver, victim resolution (F1), defence state component (server-only), encounter ordering by key compare, `FActionState`/`FDeathState` writes (struct types live in Foundation `View/`, LP concern 2), owner results | `OnPawnKilled(Victim, Killer, VictimKind, Cause)`, `FStrikeResult` / `FDefenceResult` owner replies, defence predicates for IIF | Queue (slot, key compare, pending writes), Roster (`ResolveVictimKind`, `GetRelation`, transitions), Concealment (`IsConcealedFrom`, spot zones), SCL | Custom fixed-width `NetSerialize` for `FActionState` [U Iris serializer], forced net updates [V API] — HIGH |
| **Penalty & Informants** | Strike counts (server), WrongTarget tally (owner), exile request, informant respawn timers, Absent/Awaiting/Picking states, takeover commit | owner data (tally, `bExiled` via `FStrikeResult`; tally re-push and `T_due` on the C-TeamView owner part, user decision 2026-10-25), `OnInformantEmbodied` | ASN `OnPawnKilled`, Roster, Queue due-time heap, LSC `PickRespawnPawn`, Crowd eligibility, SCL `TakeOverCrowdPawn`, CPA `TakeNpcCard` | none beyond Core APIs — MEDIUM (inherits S1/S14 for possession parity) |
| **Objectives** | Chain data asset, per-team progress, court binding from `FSiteAssignment`, `FObjectiveTeamView`, completion gate, `RivalPrelimCount` publication schedule, stall-pressure rules | `CanBegin(Interact(Court))`, completion follow-on, `DeclareObjectiveWin` call into Round, team view sub-struct | IIF hold timers, LSC Selection, Roster, Queue, Seeds (`StageSignal`), Crowd decoy rate | none beyond Core APIs — LOW |
| **City Generator** | 12-stage pipeline, integer grid pre-validator, module selection/anti-repetition, kit placement, client decoration, golden seeds | `ILayoutProvider` implementation (kind `Generated`) | LSC service, Tuning (`CityGen*`), Seeds (`PublicSubSeed(Layout, Attempt)` only), kit data | ISM per module type (Nanite) [D], batch `AddInstances` [U], static mobility, Recast build [D] — MEDIUM (R1, R4) |

### Presentation layer

| Module | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Teammate Markers** | Client-local marker set keyed by `PublicHandle`, anchors, fill states, name label | `IsMarkedAlly(Pawn)`, `OnAllyMarkerRetired(PublicHandle)` | C-TeamView (+ fallback feed), Camera `GetViewState`, Concealment client latch, tuning client snapshot | UMG screen-space overlay after post-process, no depth test [U ZOrder vs camera fade] — HIGH |
| **HUD** | View-models (plain `USTRUCT`), widget stack (HUD-R4 ZOrders), timing of plates/cards/feed, `PresentedOwnState` | none (leaf) | Owner data on PC, C-TeamView, `FRoundPublicState`, tuning client snapshot, IIF replies, Camera view state, Markers events | UMG (CommonUI for menus only), Queue module's client `EstimatedServerNow()` (not raw `GetServerWorldTimeSeconds`, which has no RTT compensation [U]), DPI scale [U] — HIGH |

### Dependency diagram

```
                        ┌───────────────────────────┐
                        │  StealthGame (composition │
                        │  root: GameMode/State/PC/ │
                        │  PS; registers resolvers) │
                        └─────────────┬─────────────┘
          ┌───────────────────────────┼────────────────────────────┐
          ▼                           ▼                            ▼
 ┌─────────────────┐      ┌───────────────────────┐     ┌──────────────────────┐
 │ PRESENTATION    │      │ FEATURES              │     │ (VS/Alpha hooks)     │
 │ HUD ──► Markers │      │ Crowd ◄── Concealment │     │ Event Log release,   │
 │  │        │     │      │   ▲  ▲        ▲       │     │ ping slot, settings  │
 │  ▼        ▼     │      │   │  └ Penalty ◄ ASN ──┘     └──────────────────────┘
 │ View/ headers   │      │   │        ▲    ▲     │
 └───────┬─────────┘      │ Objectives  Generator │
         │                └───────┬──────────┬────┘
         │ (client views only)    ▼          ▼ (provider)
         │        ┌──────────────────────────────────────────────┐
         │        │ CORE                                         │
         └───────►│ Camera ─► IIF ◄─ Shared Pawn ◄─ CPA          │
                  │            │         ▲          ▲            │
                  │ Round Rules ─────────┴── LSC (Layout Service)│
                  └──────────────────────┬───────────────────────┘
                                         ▼
                  ┌──────────────────────────────────────────────┐
                  │ FOUNDATION                                   │
                  │ Queue&Clock ◄─ (all resolvers)               │
                  │ Visibility Policy ─► Roster ◄─ Session ─► Seeds
                  │ Tuning ◄─ (all)        Event Log / fault sink│
                  └──────────────────────────────────────────────┘
```

Arrows point from consumer to provider; the boxes are logical layers (Core and Features share the `StealthGameplay` module). Upward run-time needs (Round/LSC/Shared Pawn → Crowd, Visibility Policy ← Concealment) go through the consumer-owned interfaces above, so no arrow points up. The only same-layer mutual edges (IIF↔Round, CPA↔Round, Crowd↔features, IVP↔LSC) are broken by interface ownership as the systems-index records: the interface owner ships its header first.

---

## Data Flow

### 1. Server frame (listen host) — the authoritative path

```
 NetDriver dispatch (top of world tick, before tick groups)        [V tick order]
   ├─ Human Move/Look ─► CMC ServerMove (saved moves) ─► pawn movement
   └─ Discrete command RPC ─► IIF ingress (rate limit, payload) ─► per-source MAILBOX
 TG_PrePhysics … TG_EndPhysics
   ├─ Crowd: StateTree decisions (~4 Hz, scheduled) ─► follower ─► IIF Move/Look/commands
   │          (AI discrete commands ─► server-local mailbox)
   └─ CMC simulates every pawn (human and AI alike)
 TG_PostPhysics: QUEUE DRAIN (once per frame)
   1. due-time heap: TimerFired items (HoldTimer, InformantRespawn, Grace, RivalSignal)
   2. mailboxes stamped Seq in a server-secret permutation  (Round R12)
   3. for each item in Key order: Resolver.Resolve(Item, Key)
        └─ follow-ons inline, depth-first (Roster transition, OnPawnKilled, Penalty,
           elimination check, OnActionStarted fan-out to Crowd/Concealment)
        └─ Roster events dispatched FIFO before the next item
        └─ if TryTerminate succeeds: LATCH STEP (server-only; queue closes)
   4. post-drain deferred actor teardown (if spike shows CMC destroy in drain unsafe)
 PENDING-WRITE STAGING (same frame, after drain)
   └─ writes whose ServerTime ≤ now: FDeathState/Disabled/Counter at ContactServerTime −
      StrikeNetLeadSec; ROUND-END STEP when the deciding item was a strike (phase Ended,
      FRoundResult, staged MatchScore, EndRoundAll + forced updates, held NotNow replies,
      OnRoundEnded, Crowd Halt + stop-and-blend of every pawn)
 NetDriver replication (end of frame)
   ├─ PreReplication: RemoteViewPitch16 from Look() source for every pawn
   ├─ relevancy: every pawn relevant to every connection EXCEPT concealment cull
   │   (CanViewerKnow(V,S,Position) false ⇒ cull at first boundary ≥ t0+ConcealCullDelayMs;
   │    legacy: IsNetRelevantFor + explicit channel close; Iris: exclusion group filter)
   ├─ fixed 15 Hz pawn cadence; forced updates only for FActionState/TraversalState
   │   writes and death writes, same cap for every pawn kind
   └─ owner-only PlayerController: C-TeamView (Roster part + feature team sub-structs),
      owner data, replies; GameState: FRoundPublicState, tuning snapshot, FLayoutManifest
```

Data in this path: commands (client → server, RPC/saved move), queue items (server-local), Roster state (server store), replicated state (server → clients: pawn movement, `FActionState`, `FDeathState`, `HeadLook`, `TraversalState`, `AppearanceId`; PC carriers; GameState structs). **Thread boundaries:** all mutation on the game thread (Roster I7). `FindPathAsync` runs on the task graph and returns results to the game thread before use; animation evaluation runs on worker threads and reads only replicated/pawn state; Generator work is time-sliced on the game thread (no worker threads in the MVP; ADR-014 may revisit). **ADR-0014 (Proposed) revisits this:** the Generator's pure-data stages run on one worker task that touches no engine state and returns results to the game thread; engine-facing assembly stays on the game thread. This sentence is replaced when ADR-0014 is Accepted.

### 2. Client frame — presentation path

```
 Enhanced Input ─► IIF client builder: edges → Begin/End RPC (Started/Completed/Canceled),
                   Attack payload from Camera.GetAimRay(); Move/Look into CMC saved moves
 Receive replication ─► RepNotify handlers ─► ONE state-derived RefreshPresentation(ServerNow)
                         (idempotent; arrival order between actors unspecified)
 Camera: view envelope, boom, eye-origin mask (per pawn α) ─► "rendered" flag
 Animation: tier = f(screen height, rendered flag) ─► update-rate skip (external tick-rate control; engine URO off, ADR-0013)/search interval ─► pose
 Concealment client latch: hide body at swap instant from server start time (fail-closed)
 Presentation: HUD + Markers read PC carriers, FRoundPublicState, tuning snapshot;
               timing from the client server-time estimator EstimatedServerNow()
               (RTT-compensated, smoothed, monotonic; accuracy budget in ADR-002),
               plates at local contact frame
 Render (Lumen/Nanite/VSM) ─► UMG layers after post-process
```

The listen host runs the same client path for its local player: it does not receive RepNotify for its own writes, so it calls the same handlers directly (ASN-R21, Round R20), and its presentation reads only C-TeamView and public structs (Roster R6, Session rule 3).

### 3. Event and signal path (no tight coupling)

| Event | Producer | Consumers | Mechanism |
|---|---|---|---|
| `OnTuningReady(TuningHash)` | Tuning | Round, Session (ack), every feature (cache keys) | server delegate, game thread |
| `OnLayoutReady` | Layout Service | Round, Objectives (bind), Crowd (freeze nav, density), Concealment (bush volumes) | server delegate |
| Roster events (`OnParticipantStateChanged`, `OnRosterRoundEnded`, `OnRosterReset`, `OnBindingLost`, registration/connection/team events) | Roster | Round (elimination), Penalty, Objectives, Concealment, Session | queued, FIFO after outermost call; `EventSeq` idempotency |
| `OnActionStarted/Cancelled/Completed(Pawn, Action)` | IIF action hub | Crowd (stimuli), Concealment (break), Objectives, Assassination | one world-level hub, server-only multicast delegate |
| `OnPawnKilled(Victim, Killer, VictimKind, Cause)` | Assassination | Penalty, Crowd (replacement), Round (log) | inline in the Attack's queue slot |
| `OnRoundActive`, `OnRoundEnded(Result)` | Round | Crowd (start/Halt), IVP (PostRound class), Session (admission) | server delegate in the Active-entry / round-end step |
| Session connection events | Session | Round (policy), Roster (projection via Session call) | server delegate |
| Owner replies (`ClientCommandRejected`, `FStrikeResult`, `FDefenceResult`) | IIF, Assassination | owning client's HUD | owner-only reliable client RPC on the PlayerController [U ordering vs replication; HUD F2 interim rule] |
| Client-local: `OnAllyMarkerRetired`, view-state changes | Markers, Camera | HUD | client delegates |

**Rule:** no multicast RPC on any pawn; every observable cue is replicated state presented by a state-derived handler (SCL R13).

### 4. Save/load and persistence path

There is **no save game** in the MVP. Persistence is in three scopes:

| Scope | What | Owner | Mechanism |
|---|---|---|---|
| Install (client) | 128-bit IdentityKey; client-local settings (sensitivity, FOV, volume, UI scale, accessibility) | Session (key), Camera/HUD/Settings (prefs) | local file / `UGameUserSettings`-style config [U]; never reaches the server as tuning (TDC CR-14) |
| Match (server) | Roster records, Session connection table, Round match data (score, `RoundsPlayedCount`, Event Log archives), Tuning preset name | each owner, in game-instance stores | `UGameInstanceSubsystem` lifetime (survives seamless and hard travel); cleared by `BeginMatch`/`EndMatch` |
| Round (server) | Deck, bindings, strikes, tallies, objectives progress, layout selection, queue | each owner | reset on `OnRosterReset` / Reset step; never carried across rounds |

The Event Log is in memory only and never written through `UE_LOG`, the output log, crash dumps or trace channels (Round R19).

### 5. Initialisation order

```
Process start
 └─ GameInstance subsystems: Tuning preset store, Roster store, Session table,
    Round match state, Seeds (ServerSecret drawn at session open), Event Log
Host opens lobby map (listen)        → Session BeginMatch(TeamSize from Tuning lobby value)
Clients join (PreLogin gates, IdentityKey) → Roster RegisterParticipant → Round AssignTeams
Host StartRound (R5 checks)
 1. SetTeamsLocked(true) → ResetAll(RoundIndex) → Session RequestRoundTravel(Seed)
 2. seamless ServerTravel ?Seed= ; InvalidateAllBindings ; admission Closed
    (the URL option is a server option; clients receive the Seed in a replicated
     GameState struct, FRoundParams, never by parsing the URL — LP concern 11, S3 verifies)
New world (server)
 3. World subsystems: Queue&Clock, Visibility Policy, Layout Service, Crowd Director
 4. Tuning BeginResolve → validate → freeze → OnTuningReady (snapshot to GameState)
 5. Layout RequestLayout(Seed, TuningHash) → provider build → server nav build
    → Validate → Select → OnLayoutReady → FLayoutManifest on GameState
 6. CPA AllocateAppearances → Round single shuffled spawn list (one frame;
    deferred spawn → BindPawn → finish spawn → possess; AppearanceId before first rep)
 7. Session single acknowledgement barrier (Seed, LayoutHash, ClientHash, PoolHash)
 8. ActiveAtServerTime published (P7) → first drain ≥ that time: Active entry step
    (zero ActiveAtServerTime, push team views, OnRoundActive → Crowd starts)
Clients
 a. load map → read FRoundParams.Seed + FLayoutManifest from GameState → build geometry
    from Manifest (no RNG) → recompute LayoutHash
 b. receive tuning snapshot → ClientHash ; ack via Session
 c. lift loading screen at ActiveAtServerTime when ExpectedPawnCount pawns held
    OR RevealPawnWaitMaxSec elapsed (with own-pawn condition), always on Ended
```

---

## API Boundaries

Pseudocode in the project's C++ conventions. Types marked `[U]` need verification in the governing ADR. All server APIs assert authority and game thread; every call that can be stale carries an epoch token.

### Foundation

```cpp
// --- Tuning (TDC) -------------------------------------------------------------
struct FStealthServerTuning;   // all keys (server-only header: Public/Server/)
struct FStealthClientTuning;   // public keys only (Public/View/); distinct type
class IStealthClientTuning {                                      // Public/View facade
public: virtual const FStealthClientTuning* GetClientTuning() const = 0; };  // replicated snapshot
class UStealthTuningService : public UWorldSubsystem, public IStealthClientTuning { // Public/Server
public:                                                           // [U] world vs GI face
    void BeginResolve();                                          // Round only, on Preparing
    const FStealthServerTuning* GetServerTuning() const;          // null before Frozen
    FOnTuningReady OnTuningReady;                                 // (TuningHash)
};
// Invariants: no pawn/team/viewer parameter exists (CR-5). Values never change inside
// a round except a PIE hot-reload publish (Revision+1). Consumers honour latch classes.
// Guarantee: a frozen set passed hard-range clamp + every invariant; an invariant
// failure rejects the whole preset (never repaired).

// --- Roster -------------------------------------------------------------------
enum class EParticipantState : uint8 { Active, Informant, RoundEnded };
enum class ETransitionResult : uint8 { Applied, NoOp_AlreadyInState, Rejected_NotAuthority,
    Rejected_WrongThread, Rejected_UnknownParticipant, Rejected_StaleEpoch,
    Rejected_BatchOnlyReason, Rejected_IllegalReason, Rejected_IllegalTransition,
    Rejected_NotDisconnected, Rejected_StaleConnection };
class URosterStore : public UGameInstanceSubsystem {
public:
    ETransitionResult RequestTransition(FParticipantId, EParticipantState, ETransitionReason,
        TOptional<FEpochToken> ExpectedEpoch = {}, TOptional<uint32> ExpectedConnGen = {});
    EBindResult BindPawn(FParticipantId, APawn*, TOptional<FEpochToken> = {}); // stores TWeakObjectPtr
    void RetireBinding(APawn*); void ReleaseToCrowd(APawn*); void ClearBinding(APawn*, EEndPlayReason::Type);
    EVictimKind ResolveVictimKind(const APawn*) const;        // Player|Informant|Npc|NotTargetable(...)
    ERelation  GetRelation(const APawn* A, const APawn* B) const;
    EParticipantState GetLegalityClass(const APawn*) const;   // Unbound pawn => Active
    // Batch: ResetAll(RoundIndex), EndRoundAll(); queries return 0/false on clients + fault.
};
// Invariants: callers never cache answers across frames; listeners are idempotent per
// EventSeq; only Session writes Connection; only RequestTransition/batches write State.

// --- Core value types (Foundation Public/View; defined in ADR-002, LP concern 3) -----
struct FParticipantId { uint32 Value; };                  // server-only meaning; never replicated
struct FEpochToken    { uint64 MatchId; uint32 RoundEpoch; };
struct FViewerId      { FParticipantId Participant; };    // derived from the server connection
struct FSubjectRef    { /* tagged: Pawn (weak ptr) | Participant | CourtFixtureId */ };
struct FMailboxId     { uint16 Value; };                  // one per participant + one server-local
enum class EQueueItemType : uint8 { Command, HoldTimer, InformantRespawn, GraceExpiry,
                                    RivalSignal, HostCommand, /* closed set */ };
struct FQueueItem {                                       // tagged, ID-only payloads:
    EQueueItemType Type;                                  // no raw UObject* in the item
    TVariant<FCommandPayload, FHoldTimerPayload, FRespawnPayload, FGracePayload,
             FSignalPayload, FHostPayload> Payload;
    FEpochToken Expected;
};
enum class ETransitionReason : uint8 { Assassinated, ReversedAgainst, StrikeLimit,
                                       Disconnect, RoundEnd, Reset };

// --- Resolution Queue & Server Clock -----------------------------------------
struct FQueueKey { uint64 ServerTickIndex; uint32 Seq; };    // total order
class IQueueResolver { public: virtual void Resolve(const FQueueItem&, FQueueKey) = 0; };
class ITerminalDecision { public: virtual void OnLatched(FQueueKey) = 0; };  // Round implements
enum class EStagedWriteType : uint8 { DeathWrite, CounterPose, RoundEndStep, HeldReplies };
struct FStagedWrite { EStagedWriteType Type; double ServerTime; FEpochToken Expected;
                      TVariant<FDeathWritePayload, FCounterPayload, FRoundEndPayload,
                               FHeldRepliesPayload> Payload; };       // typed, loggable
class FResolutionQueueCore { /* plain C++: testable without a UWorld */ };
class UResolutionQueue : public UWorldSubsystem {         // facade; drain = custom FTickFunction
public:                                                   // in TG_PostPhysics (OnWorldBeginPlay)
    void Submit(FMailboxId Source, FQueueItem Item);              // stamped at drain start
    void RegisterResolver(EQueueItemType, TWeakInterfacePtr<IQueueResolver>);
    FTimerItemHandle ScheduleAt(double ServerTime, FQueueItem);   // due-time heap
    int32 Compare(FQueueKey A, FQueueKey B) const;
    void StageWrite(const FStagedWrite&);                         // pending-write queue
    bool Latch(FQueueKey);   // storage only; called by Round's TryTerminate (Round owns the rule)
    bool IsLatched() const; double ServerNow() const;             // double precision
};
// Client side (same module, View header): smoothed, RTT-compensated, monotonic estimate.
double EstimatedServerNow(const UWorld*);   // accuracy budget + net-emulation test in ADR-002
// Invariants: resolvers re-validate against live state and run follow-ons inline;
// nothing outside the drain mutates gameplay state; after the latch every item returns
// Rejected_Latched; staged writes run in server-time order, are logged by type, and are
// dropped on Reset (epoch mismatch).

// --- Visibility Policy ---------------------------------------------------------
enum class EFact : uint8 { IsPlayer, IsInformant, Team, State, Position, IsConcealed,
                           SiteKnownToTeam, /* ... per IVP R3 */ };
class UVisibilityPolicy : public UWorldSubsystem {
public:
    bool CanViewerKnow(FViewerId Viewer, FSubjectRef Subject, EFact Fact) const; // default deny
    EViewerClass GetViewerClass(FViewerId, FSubjectRef) const;
    bool ShouldCullForConnection(const APawn*, const UNetConnection*) const;    // concealment only
};
// Invariants: Viewer derives from the server connection, never client input. No
// consumer re-derives a rule. A new fact or audience is a change request to IVP.

// --- Session & Seeds -----------------------------------------------------------
class USessionService : public UGameInstanceSubsystem {
public:
    bool RequestRoundTravel(int32 RoundIndex);   // draws Seed, ?Seed= URL, ack barrier
    bool RequestLobbyTravel(); bool RequestFallbackTravel(FName GrayboxMap);
    void SetAdmissionMode(EAdmissionMode);       // Open | RejoinOnly | Closed
    EConnectionState GetConnectionState(FParticipantId) const;
    uint32 PublicSubSeed(EPublicPurpose, uint32 Index) const;           // geometry only
    uint64 GetServerSubSeed(EServerPurpose, uint32 Index) const;        // never replicated
};
```

### Core

```cpp
// --- Pawn Command Interface (one per Shared Pawn, non-replicated) ---------------
UCLASS() class UPawnCommandComponent : public UActorComponent {
public: // server entry points; AI and human call the same functions
    void Move(FVector2D Input);  void Look(FRotator Rotation);
    void Traverse(ETraversalKind);
    void InteractBegin(FCourtFixtureId); void InteractEnd();
    void PrepareBegin(); void PrepareEnd();
    void Attack(const FAttackPayload& Payload);   // aim ray, WorldPoint, optional PawnRef
    void Hide(const FHideCommand& Cmd);           // Enter(BushId) | Exit(SideIndex)
    void RegisterPredicate(ECommandKind, ICommandPredicate*);           // features
};
// Invariants (IIF R2): no parameter, event or listener carries IsAI/IsPlayer, a
// controller, a PlayerState or a team; never calls IsPlayerControlled/IsLocallyControlled.
// Guarantees: one action slot per pawn (Attack > Prepare > Interact > Hide); hold ends
// resolve only as queue items; a resolution-voided command yields exactly one owner reply.

// --- Shared Pawn factory (world subsystem, LP concern 12) ----------------------
class UStealthPawnFactory : public UWorldSubsystem {
public:
    AStealthPawn* SpawnSharedPawn(const FTransform&, FAppearanceId, EControlKind,
                                  TOptional<FParticipantId>, FEpochToken);  // Round, Crowd only
    bool TakeOverCrowdPawn(AStealthPawn*, FParticipantId, FEpochToken);     // Penalty only
};
// Invariants: deferred spawn → BindPawn → finish → possess, in one call stack; possession
// never changes class, components, net config, velocity, HeadLook, AnimBP or mesh state.
// [V] ForceNetUpdate is called by APawn possession paths: the Shared Pawn overrides it.

// --- Character Pool ------------------------------------------------------------
class UAppearanceAllocator : public UWorldSubsystem {
public:
    FDealResult AllocateAppearances(const TSet<FParticipantId>& Players, int32 NpcCount); // once/round
    FCardTicket DrawReplacement();     // same server tick as the replacement spawn; CommitCard(Ticket, Pawn) after spawn (ADR-0012)
    void TakeNpcCard(FParticipantId, AStealthPawn*);   // informant adoption: NpcLive -> BoundLive (ADR-0012, GDD form)
};
// Players carry no team, order or kind; the deck is conserved for the round.

// --- Layout Service ------------------------------------------------------------
class ILayoutProvider { public: virtual void BuildAttempt(uint32 AttemptSeed, const FLayoutParams&,
                                 FLayoutBuildSink&) = 0; };       // Graybox, Generated
class ULayoutService : public UWorldSubsystem {
public:
    void RegisterProvider(ELayoutProviderKind, ILayoutProvider*);
    void RequestLayout(uint32 Seed, FTuningHash);   // Preparing only
    FOnLayoutReady OnLayoutReady;
    FPlacementResult RequestStartPlacement(...);    // shared emergence vocabulary
    AStealthPawn* PickRespawnPawn(ETeam, uint32 RequestSeq, AStealthPawn* Exclude = nullptr);
    const FSiteAssignment& GetSiteAssignment_Server() const;   // Public/Server/ only
};
// The contract spawns zero replicated actors; FLayoutManifest carries no role data.

// --- Round rules ---------------------------------------------------------------
class URoundService : public UGameInstanceSubsystem {   // phase machine survives travel
public:
    FStartRefusal StartRound(); void ResetRound(); bool AbortRound();
    void DeclareObjectiveWin(ETeam, FQueueKey);           // Objectives resolver only
    bool ShouldAdmitExpired(FParticipantId) const;
};

// --- Camera (client) -----------------------------------------------------------
FAimRay GetAimRay() const;            // one function for IIF Attack and the strike hint
EViewState GetViewState() const;      // Live, Dying, Awaiting, ... (CS-R15)
bool IsEyeMaskVisible(const APawn*) const;   // geometry-only; α > 0
```

### Feature

```cpp
// Assassination (wire structs below are declared in StealthFoundation Public/View so IIF,
// SCL animation and Presentation can read them without depending on a feature)
DECLARE_MULTICAST_DELEGATE_FourParams(FOnPawnKilled, AStealthPawn* /*Victim*/,
    AStealthPawn* /*Killer*/, EVictimKind, EKillCause);         // inside the Attack slot
USTRUCT() struct FActionState { EActionPose Pose; uint16 Seq; uint8 Variant;
    double StartServerTime; double ContactServerTime; FFixedWarpOffset WarpPoint; // fixed-width NetSerialize, 208 bits [U Iris]
};                                                              // StartServerTime: user decision 2026-10-25
USTRUCT() struct FDeathState { double DeathServerTime = -1.0; bool bDiedConcealed = false; };  // user decision 2026-10-25
USTRUCT() struct FStrikeResult { uint16 ClientCommandSeq; EStrikeResult Result;
    uint8 TallyAfter; bool bExiled; double ContactServerTime; }; // tally confirmed; ContactServerTime added 2026-10-25

// Concealment
bool IsConcealedFrom(FViewerId, const AStealthPawn*) const;     // server-only
bool IsAllyBodyHidden(const AStealthPawn*) const;               // client-local latch

// Crowd
void Halt();                                                    // round-end step
void DriveBoundPawn(AStealthPawn*); void ReleaseControl(AStealthPawn*);
ETakeoverEligibility GetTakeoverEligibility(const AStealthPawn*) const;

// Objectives
USTRUCT() struct FObjectiveTeamView { /* live courts for this team, stage card, RivalPrelimCount */ };
```

### Presentation contract

Presentation code reads exactly: the owner part of C-TeamView and owner data on its own `APlayerController`, the teammate entries of C-TeamView (incl. `FObjectiveTeamView` and the fallback feed), `FRoundPublicState` and `FLayoutManifest` on the GameState, `GetClientTuning()`, IIF/ASN owner replies, and the client-local queries listed above. It may not call `IsPlayerControlled()`, `IsBotControlled()`, read `APawn::PlayerState`, iterate world pawns (Markers), or include any `Public/Server/` header (compile guard). C-TeamView is built as the Roster's owner/teammate struct plus **per-feature replicated sub-object components on the PlayerController** (registered sub-object list [V]); Presentation finds them with `FindComponentByClass<>()` on its own controller, so it never includes the root module's `AStealthPlayerController` header.

**Engine-type flags.** `UWorldSubsystem`/`UGameInstanceSubsystem` (GI subsystems survive travel; world-bound delegates must be re-bound per world — S3 confirms in multi-process standalone); `TVariant` payloads and `TWeakInterfacePtr` [U usage under UHT for UPROPERTY-free plain structs]; `UNetConnection` in `ShouldCullForConnection` [legacy only; Iris uses group handles, V]; `NetSerialize` custom serialiser [U under Iris]; `FTickFunction` in `TG_PostPhysics` [V tick-group order per Round GDD]; `USkeletalMeshComponentBudgeted` if the budget allocator is adopted [V], which must then be the mesh class of every pawn.

---

## ADR Audit

*Refreshed 2026-10-25 after the first `/architecture-review` and its fix pass. The 2026-10-24 text (no ADRs, 0/170 covered) is superseded.*

### ADR quality check

ADR-0001 to ADR-0023 are written and all are **Proposed**, awaiting user acceptance. ADR-0024 (Dedicated Server / Iris migration) and ADR-0025 are deferred (Alpha) and do not exist yet. Every ADR has the Engine Compatibility, ADR Dependencies and GDD Requirements Addressed sections. Full findings are in [`architecture-review-2026-10-25.md`](architecture-review-2026-10-25.md) and the per-requirement matrix is in [`requirements-traceability.md`](requirements-traceability.md).

| ADR | Title | Status | Engine Compat | GDD Linkage | Conflicts (review 2026-10-25 → fix pass) |
|---|---|---|---|---|---|
| ADR-0001 | Crowd identity rules | Proposed | ✅ 5.8.4 | ✅ | C3, C5 fixed |
| ADR-0002 | Server resolution queue, server clock and round-end staging contract | Proposed | ✅ 5.8.4 | ✅ | C1 (blocking) fixed: Foundation-owned payloads, `FPawnHandle`, `RegisterStagedWriteApplier` |
| ADR-0003 | Replication, relevancy and the concealment cull hybrid | Proposed | ✅ 5.8.4 (listen-tick row [V]) | ✅ | C2 fixed; listen-host tick restated |
| ADR-0004 | Tuning data pipeline and validator | Proposed | ✅ 5.8.4 | ✅ | ADR-0024 edge made conditional |
| ADR-0005 | Session, seamless travel and match-scoped state stores | Proposed | ✅ 5.8.4 | ✅ | C8 fixed; `OnViewRebound` mapped |
| ADR-0006 | Seeds and deterministic/secret random streams | Proposed | ✅ 5.8.4 | ✅ | — |
| ADR-0007 | Code module layout, composition root and include boundaries | Proposed | ✅ 5.8.4 | ✅ | dependency cycle fixed (content inputs moved out of Depends On); C1 module table |
| ADR-0008 | Test and automation strategy for leak and parity tests | Proposed | ✅ 5.8.4 | ✅ | dependency cycle fixed; G-DECK row fixed |
| ADR-0009 | Version control and binary asset workflow | Proposed | ✅ | ✅ | — (user decision pending) |
| ADR-0010 | Shared Pawn: CMC locomotion, spawn factory and possession parity | Proposed | ✅ 5.8.4 (MotionWarping Beta noted) | ✅ | C9 note; header folders |
| ADR-0011 | Pawn command interface and input pipeline | Proposed | ✅ 5.8.4 | ✅ | C4 fixed (`Public/Server/Core/`) |
| ADR-0012 | Character pool data and appearance allocation | Proposed | ✅ 5.8.4 | ✅ | C8 fixed |
| ADR-0013 | Animation: motion matching and significance budget | Proposed | ✅ 5.8.4 | ✅ | — |
| ADR-0014 | Layout service, seeded generation and runtime navmesh | Proposed | ✅ 5.8.4 | ✅ | — |
| ADR-0015 | Camera system and LOS envelope | Proposed | ✅ 5.8.4 | ✅ | — |
| ADR-0016 | Crowd AI architecture | Proposed | ✅ 5.8.4 (StateTree plugins not yet enabled: pending user) | ✅ | C9 note |
| ADR-0017 | Concealment component and per-viewer presentation | Proposed | ✅ 5.8.4 | ✅ | C1, C5, C6, C7 fixed; acceptance pair with ADR-0018 |
| ADR-0018 | Encounter data and outcome presentation timing | Proposed | ✅ 5.8.4 (MotionWarping Beta noted) | ✅ | C1, C2, C6 fixed; acceptance pair with ADR-0017 |
| ADR-0019 | Informant lifecycle and crowd-pawn takeover | Proposed | ✅ | ✅ | C1, C8 fixed; C9 note |
| ADR-0020 | Objectives data and team-view sub-structs | Proposed | ✅ 5.8.4 | ✅ | C9 note; rebind event named |
| ADR-0021 | UI/HUD layering, CommonUI and Enhanced Input | Proposed | ✅ 5.8.4 | ✅ | C9 note |
| ADR-0022 | Rendering configuration and performance budgets | Proposed | ✅ 5.8.4 | ✅ | — |
| ADR-0023 | Playtest telemetry hooks (pre-Telemetry GDD) | Proposed | ✅ | ✅ | aligned with Round R19 (TR-round-013) |

`docs/registry/architecture.yaml` holds the stances registered from ADR-0001..0023 (merged 2026-10-24, revised 2026-10-25 by the fix pass). Declared ADR dependencies are acyclic after the fix pass (`adr-dep-graph.sh`: no CYCLE line). The review verdict was **FAIL** (one blocking conflict, C1). Its fixes are listed in the report's "Fixes applied after review (2026-10-25)" section, and the verdict is left for the independent re-check. While every ADR is Proposed, the verdict is capped at CONCERNS.

### Traceability coverage check

**177 requirements** are registered in `docs/architecture/tr-registry.yaml`: the 170 Appendix A IDs, now permanent, plus 7 added by the review (TR-roster-012, TR-iif-013, TR-round-013, TR-ivp-012, TR-scl-014, TR-con-011, TR-asn-011). Coverage per the review: **174/177 covered** (by Proposed ADRs), **3 partial**, 0 gaps. TR-con-005 and TR-pen-006 are partial because they wait on pending user decisions. TR-round-013 was partial against ADR-0023; the fix pass aligned ADR-0023 with Round R19, and the re-check confirms it. The per-requirement matrix is [`requirements-traceability.md`](requirements-traceability.md). The table below is the original assignment of baseline TRs to ADRs.

| Required ADR | TRs it covers (baseline assignment) |
|---|---|
| ADR-001 | TR-roster-006..009, TR-roster-011, TR-ivp-001..003, TR-ivp-008, TR-ivp-011, TR-scl-005, TR-cpa-006, TR-crowd-004, TR-pen-005, TR-pen-006, TR-tmk-001, TR-hud-001, TR-session-008, TR-session-010 |
| ADR-002 | TR-round-002..005, TR-round-007, TR-iif-005..007, TR-asn-001, TR-asn-002, TR-asn-006, TR-asn-010, TR-pen-003, TR-oc-005, TR-roster-003 |
| ADR-003 | TR-ivp-004..007, TR-ivp-009, TR-ivp-010, TR-scl-003, TR-scl-009, TR-scl-010, TR-con-005, TR-con-008, TR-asn-003, TR-asn-004, TR-asn-009, TR-round-009, TR-camera-001, TR-lsc-003, TR-iif-009 |
| ADR-004 | TR-tdc-001..011 |
| ADR-005 | TR-session-001..003, TR-session-005..007, TR-session-009, TR-roster-001, TR-roster-002, TR-roster-010, TR-round-001, TR-round-006, TR-round-010 |
| ADR-006 | TR-session-004, TR-cpa-003, TR-citygen-002, TR-crowd-005, TR-round-011, TR-oc-006 |
| ADR-007 | TR-roster-009 (compile-time half), TR-tdc-006 (typed client face), TR-ivp-011 (include check), TR-round-008 (Event Log never through logs/traces) |
| ADR-008 | TR-ivp-006, TR-ivp-007, TR-ivp-010, TR-hud-002, TR-cpa-007, TR-tdc-002, TR-citygen-002 (goldens) |
| ADR-009 | process prerequisite for every TR (no requirement text) |
| ADR-010 | TR-scl-001..004, TR-scl-010, TR-scl-012, TR-scl-013, TR-roster-004, TR-roster-005, TR-iif-002 |
| ADR-011 | TR-iif-001..004, TR-iif-008, TR-iif-010..012 |
| ADR-012 | TR-cpa-001..006 |
| ADR-013 | TR-scl-006..008, TR-scl-011, TR-asn-007 |
| ADR-014 | TR-lsc-001..009, TR-citygen-001, TR-citygen-003..008, TR-crowd-003 |
| ADR-015 | TR-camera-001..008 |
| ADR-016 | TR-crowd-001..010 |
| ADR-017 | TR-con-001..010 |
| ADR-018 | TR-asn-003, TR-asn-005, TR-asn-007, TR-asn-008, TR-hud-004 |
| ADR-019 | TR-pen-001..008, TR-lsc-006 |
| ADR-020 | TR-oc-001..007 |
| ADR-021 | TR-hud-003..007, TR-tmk-002..006, TR-camera-006, TR-iif-003 |
| ADR-022 | TR-cpa-005, TR-scl-008, TR-citygen-004, TR-con-006, TR-camera-003 |
| ADR-023 | TR-round-012, TR-crowd-010 |

Count (2026-10-25 review): **174/177 covered by Proposed ADRs, 3 partial, 0 gaps**; every requirement has a named ADR (several TRs are deliberately shared by two ADRs where a mechanism ADR and a feature ADR meet, e.g. TR-asn-003 in ADR-003 and ADR-018).

---

## Required ADRs

Numbering is the creation order the orchestrator should follow with `/architecture-decision`. ADR-001 is first by mandate; the rest are Foundation-first, then Core, Feature, Presentation, then cross-cutting and deferred. "Priority" uses the skill's three bands: **Must** (before any coding of the layer), **Should** (before the relevant system is built), **Defer** (to implementation or a later tier).

**ADR authoring status (2026-10-24).** ADR-0001 to ADR-0023 are written (`docs/architecture/adr-0001-*.md` … `adr-0023-*.md`), all **Proposed** and awaiting user acceptance; an inter-ADR amendment pass applied their cross-requests the same day (each target ADR lists them under "Amendments (2026-10-24 sync)"). ADR-0024 and ADR-0025 are deferred (Alpha). The table rows above are the scopes the ADRs were written against; where a Proposed ADR narrowed or replaced a candidate mechanism (ADR-0013: engine URO and the AnimationBudgetAllocator ruled out; ADR-0015: CPD dither slot and rate bands at every range instead of "time-sliced beyond 60 m" / "per-pawn parameter"; ADR-0006: `(Domain, Attempt)` naming), the ADR governs and the row is updated on acceptance.

### Must have before coding starts (Foundation and Core)

**Bootstrap order (TD concern 2).** ADR-001 is written first but cannot be *Accepted* first, because its central mechanism needs spike evidence and the spikes need the harness. The acceptance order is: ADR-009 (VCS, blocked on the user) → ADR-007 and ADR-008 (enough module skeleton and harness to run spikes) → spikes S1, S2, S14 (and S11 if Iris is considered) → ADR-001 Accepted → ADR-002 … in table order. Until then ADR-001 stays Proposed and stories that depend on it are blocked by design.

| # | Title | Layer | Scope (one line) | GDD requirements covered | Priority |
|---|---|---|---|---|---|
| **ADR-001** | **Crowd identity rules** | Foundation | The **identity invariants and the carrier model**, not their mechanisms (narrowed after TD concern 2): one pawn class for players and NPCs; AI drives it through the same pawn command interface (movement and input path); no AI-keyed animation LOD or budget (significance-only); the PlayerState link hidden from non-team connections (`APawn::PlayerState`, `APawn::Controller`, `AActor::Owner` owner-only with identical raw-bunch class for both kinds; PlayerState allowlist; frozen `PlayerArray`; spectator state banned); teammate markers only on teammates' clients via the **C-TeamView carrier** (owner-only PlayerController: Roster part + **feature team sub-structs ride C-TeamView** as per-feature owner-only sub-object components, C-W6); **owner-only `T_due` carrier: the C-TeamView owner part (user decision 2026-10-25, C-W5 closed)**; net-driver choice (legacy recommended; `net.Iris.UseIrisReplication` pinned in `DefaultEngine.ini`, harness asserts it). **Written fallback if S1/S14 fail:** never replicate `APawn::PlayerState`/`Controller` to any client other than the owner by disabling them for non-owners on *every* pawn (`DISABLE_REPLICATED_PROPERTY` / condition reset [V]) and give the owner its own link through its PlayerController; the ally mapping already comes only from C-TeamView `PawnRef`. Mechanisms belong to ADR-003 (replication), ADR-010 (pawn/possession), ADR-011 (commands), ADR-013 (animation). **Status stays Proposed until S1, S2, S14 report.** | Roster R6, R10; IVP R1-R5, R13-R14; SCL R1, R8, R11, R13 (invariants only); IIF R2, R17; CPA CP-16; Crowd CNB-R2/R3; PEN-R14/R15; TMK TM-R1/R2; HUD-R1; Session rules 3, 22, 25 | Must (Accepted only after spikes S1, S2, S14; S7 and S11 only if Iris is considered — matches the bootstrap order above)
| ADR-002 | Server resolution queue, server clock and round-end staging contract | Foundation | One queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes with server-secret permutation, drain once per frame in `TG_PostPhysics`, Round-owned due-time heap, resolver registration, inline follow-ons with Roster event flush, terminal latch, and the **Round R9 two-step** (latch step server-only; round-end step at the deciding strike's death write through the shared pending-write queue: phase `Ended`, staged `MatchScore`, `EndRoundAll` + forced updates, held `NotNow` replies, Crowd Halt and stop-and-blend for every pawn); deferred actor teardown if CMC destroy inside the drain is unsafe; **core value types** (`FQueueItem` as a tagged `TVariant` with ID-only payloads, `EQueueItemType` enum, `FEpochToken`, `FParticipantId`, `FMailboxId`, `FViewerId`, `FSubjectRef`, `ETransitionReason`); the drain as a custom `FTickFunction`; latch *storage* in the queue with the terminate rule injected by Round (`ITerminalDecision`); typed staged writes; **client server-time estimator** (RTT-compensated, smoothed, monotonic) with an accuracy budget and a net-emulation acceptance test (TD concern 3); Round→Crowd Halt via `ICrowdControl` or an in-step event subscriber | Round R9, R11-R16; IIF R5, R8, R9; ASN-R2, R5, R13, R21-R23; PEN-R8; OC-10; Roster R12; TMK TM-R5 / HUD state rule for `RoundEnded` before `Ended` | Must |
| ADR-003 | Replication, relevancy and the concealment cull hybrid | Foundation | Pawn net parity config (fixed 15 Hz, adaptive off, priority 3.0, `DORM_Never`, no cull distance, no multicast), possession `ForceNetUpdate` deferral, forced-update policy (FActionState/TraversalState/death writes only, same cap for all kinds), `RemoteViewPitch16` from `Look`, smoothing parity; concealment cull: quantised delay, legacy `IsNetRelevantFor` replacement + explicit channel close (S9) / Iris exclusion filter, re-open behaviour (S10), un-cull in the same step; fixed-width serialisers; replicated-property allowlist ownership and audit; team fallback position feed (per-entry sequence pending, TMK OQ14). Once ADR-001 picks legacy, the ADR specifies **one** cull path only (the Iris filter path moves to ADR-024) (TD concern 6) | IVP R5-R8, R11; SCL R13-R14; Roster R6 parity; CON-R6a, CON-R10; ASN-R21; LSC R2.6; Round R20; Camera CS-R1 | Must |
| ADR-004 | Tuning data pipeline and validator | Foundation | One data-asset class of per-owner sub-structs, catalog with soft refs vs Asset Manager always-cook (AC-27), C++ schema rows (type, default, hard range, audience, latch), validation ladder: NaN/enum → default, out-of-hard-range → clamp + warn, **cross-field invariants reject or warn, never clamp or repair** (CR-6), fallback ladder; schema declared once (audience by sub-struct `_Public`/`_Server`, range/latch/audience in UPROPERTY metadata, compiled into a schema table so cooked builds keep it; LP concern 7); **safe-range lint: every owner safe range must satisfy all invariants with the other keys at their defaults** (cross-review note; C-W2, C-W17, C-W18, C-W19, C-W22); freeze + `TuningHash`/`ClientHash`; typed client face; latch classes; Shipping resolves `Default`; PIE hot-reload | TDC CR-1..CR-15, I-1..I-5, AC-3a/3b; all 18 owner knob tables (~400 keys) | Must |
| ADR-005 | Session, seamless travel and match-scoped state stores | Foundation | `AGameModeBase` without the match-state machine and without inactive-PlayerState reuse; game-instance stores for Roster, Session, Round, Tuning preset; seamless travel with `?Seed=` and `RebindView`; admission gates at PreLogin; single round acknowledgement; liveness clock and timeouts; rejoin/supersede; disabled default login pawn spawn; no host migration; one named player-pawn spawn site | Session rules 3-25; Roster R3, R4, R9, R11, OQ3/OQ4; Round R4, R6, R17, R18, R22; TDC CR-4, CR-9 | Must (spikes S3, S4, S5) |
| ADR-006 | Seeds and deterministic/secret random streams | Foundation | Public seed → `PublicSubSeed(Purpose, Index)` for geometry only; `ServerSecret` → keyed `GetServerSubSeed` for every hidden decision (deal, spawn order, intake permutation, crowd behaviour, stage signal); DRBG-class primitive for CPA Replace/Take streams; integer-only stream type (multiply-shift with rejection, no `mod`, no floats) for the Generator; counter-based crowd draws; server assert when the Generator touches server sub-seeds | Session F5, rule 14; CPA CP-8, CP-9, CP-12; Generator R-G2/R-G3; Crowd CNB-R14; Round R12, R23; OC-12 | Must |
| ADR-007 | Code module layout, composition root and include boundaries | Foundation | Four runtime modules + editor + harness (recommended; seven-module variant recorded as the alternative); `Public/Server/` vs `Public/View/` with the `STEALTH_PRESENTATION_MODULE` `#error` guard, View facades per service, no `BlueprintCallable`/`BlueprintType` on server-only types, widget-Blueprint audit; consumer-owned interfaces and composition-root registration from `InitGame`/`OnWorldBeginPlay`; weak registrations; plain-C++ cores behind subsystem facades; prototype promotion rule; `UE_LOGF` logging policy with Event Log exclusion | IVP R10; TDC CR-10; Roster R6 host parity; Round R19 | Must |
| ADR-008 | Test and automation strategy for leak and parity tests | Foundation | Unit tests per module (`StealthGame.[System].[Scenario]`, headless `-nullrhi`), test seams and fault sink; multi-client PIE and multi-process standalone harness; net-emulation profiles (`LeakNetProfile`: PktLag 100, PktLoss 1-2 %); raw-bunch capture for pointer parity (S1); reflection allowlist audit on class defaults and live instances; leak suites (AC-IVP, AC-HUD, AC-TMK, possession canary per commit); determinism goldens for the Generator; statistical gates (G-DECK, F5 posterior); CI gate wiring; `StealthTestHarness` as a Runtime module excluded from Shipping so cooked multi-process Development runs load it; startup assertion of the active replication system | IVP R11, R12, AC-IVP-01/29/33/37; HUD-R3; TMK ACs; CPA F-8; Generator R-G11; TDC AC-3a | Must |
| ADR-009 | Version control and binary asset workflow | Foundation (process) | **Pending user decision: no VCS exists; do not init git.** Options to record: Git + LFS (recommended in the concept and first-playable report), Perforce, Plastic/Unity VCS; lock policy for `.uasset`/`.umap`; snapshot workaround (`_snapshots/`) until decided | Concept Technical Considerations (TD-FEASIBILITY VCS concern); coding standards (commit references) | Must (blocked on user) |
| ADR-010 | Shared Pawn: CMC locomotion, spawn factory and possession parity | Core | Commit to CharacterMovementComponent for MVP (not Mover; the sample-enabled `Mover`/`NetworkPrediction` plugins stay unused or are disabled); `SpawnSharedPawn`/`TakeOverCrowdPawn` as the only creators; `PawnLeavingGame` override (S8); stop-and-blend hand-over; speed tiers and restrictions; traversal replication behind the net-emulation spike; input starvation; one capsule/collision/physics asset; published server state | SCL R1-R7, R14-R17, R19; Roster R9, R11; Round R17; IIF R3 | Must (spike: traversal under 100 ms / 1 % loss) |
| ADR-011 | Pawn command interface and input pipeline | Core | `UPawnCommandComponent` entry points and two-stage validation; Enhanced Input edge mapping (no Hold trigger for server meaning; `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (5.8.4 name, verified by ADR-0011) behaviour), human Move/Look via CMC saved moves, AI via the same entry points; token buckets for AI too; slot priority; action hub; reply tiers; input contexts; latency budget F5 | IIF R1-R17, F1-F6 | Must |
| ADR-012 | Character pool data and appearance allocation | Core | `UCharacterPoolData` asset class; deck/allocator implementation; `AppearanceId` applied before first replication and immutable; conservation and replacement; residency (hard refs), PSO and texture precache before leaving Preparing; one look path, no keyed material branches; `PoolHash` in the ack | CPA CP-1..CP-19, F-1..F-9 | Must (before CPA) |
| ADR-013 | Animation: motion matching and significance budget | Core | One AnimBP / PoseSearch database set / chooser set; tiers A-D by screen height and the eye-mask "rendered" flag only, caps 8/14/28, client-local TierTieKey; URO and search intervals; AnimationBudgetAllocator (requires `USkeletalMeshComponentBudgeted` on every pawn) vs SignificanceManager vs custom; server always-ticks pose; the 4 ms definition (game thread + worker sum); fallbacks that lower fidelity for every pawn equally; **audio footstep parity**: one step clock and identical concurrency/voice-limit treatment for every pawn under 5.8's relative-render-cost voice limiting (TD concern 8) | SCL R10-R12, R18, F7, F8; art bible 8.x.8; ASN-R17 | Must (spike R3 **on 3060-class target hardware** before Accepted, LP concern 9) |
| ADR-014 | Layout service, seeded generation and runtime navmesh | Core | Layout service world subsystem and provider interface; Manifest format and `LayoutHash`; zero replicated actors; validation/selection time budgets and time slicing; navmesh built once on the server after assembly and frozen at `OnLayoutReady` (Recast runtime generation mode; prototype rebuilds dynamically — to be replaced); reseed and graybox fallback travel; Generator as second provider: ISM-per-module (Nanite), batch add, static mobility, instance/shape caps | LSC R1-R11; Generator R-G1..R-G12; Crowd CNB-R15 (frozen nav) | Must for the service + graybox; Should for the Generator (built last) |

### Should have before the relevant system is built

| # | Title | Layer | Scope (one line) | GDD requirements covered | Priority |
|---|---|---|---|---|---|
| ADR-015 | Camera system and LOS envelope | Core | Custom `APlayerCameraManager` (GameplayCameras is Experimental in 5.8 — not adopted) ; view envelope; 3P boom geometry; eye-origin visibility mask (eye cone, 5 probes on a `ViewBlock` channel, time-sliced beyond 60 m) that hides mesh, shadow and RT presence without toggling `CastShadow`; dither fade via per-pawn parameter; aim ray; view state; listener at eye; trace cost budget | Camera CS-R1..CS-R15, F2, F3 | Should (before the camera-toggle milestone; OQ5 spike) |
| ADR-016 | Crowd AI architecture | Feature | Crowd Director subsystem; StateTree per NPC (~4 Hz, scheduled ticks, event interrupts); follower emitting `Move`/`Look` through the IIF; no path-following component driving, no `MoveTo` tasks; `UpdateControlRotation` override; async path queries ≤ 2 starts/frame; spatial hash; 1.5 ms budget with global-only cuts; Halt; participant-bound driving and takeover supply | Crowd CNB-R1..CNB-R16, F1-F5 | Should (before MVP-core crowd; 50-character spike) |
| ADR-017 | Concealment component and per-viewer presentation | Feature | Per-pawn component and server record; hide volumes from the Manifest; owner-side warp (S-CON-1); clip scheduling from replicated server start time (`FActionState.StartServerTime`, adopted 2026-10-25); client latch fail-closed; hidden-primitive flags (shadow, Lumen, RT); blind-strike spot zones; rustle parity; NPC use of the same path; concealment log | CON-R1..CON-R16; IVP R7 (with ADR-003) | Should (spike R5, S-CON-1) |
| ADR-018 | Encounter data and outcome presentation timing | Feature | `FActionState` fixed-width struct (incl. `WarpPoint`), `FDeathState`, outcome-blind cue at the slot and outcome writes at `ContactServerTime − StrikeNetLeadSec`; owner-only `FStrikeResult`/`FDefenceResult` and their ordering vs replication (HUD F2; `ContactServerTime` in `FStrikeResult` adopted 2026-10-25); idempotent state-derived presentation; one body sequence | ASN-R16..R23; HUD-R6, HUD-R6a, HUD-R7; PEN-R14 | Should |
| ADR-019 | Informant lifecycle and crowd-pawn takeover | Feature | Due-time-heap respawn at `T_due`, Absent/Awaiting/Picking states, `PickRespawnPawn` + `TakeOverCrowdPawn` + `BindPawn` + `TakeNpcCard` in one call stack, retries and widening, exile release to crowd, informant restrictions enforcement map; consumes ADR-001's `T_due` carrier decision | PEN-R1..R17; LSC R5b; Crowd CNB-R11; CPA CP-12b/CP-13 | Should |
| ADR-020 | Objectives data and team-view sub-structs | Feature | Chain data asset class (`FObjectiveDef`), court binding at `OnLayoutReady`, `FObjectiveTeamView` riding C-TeamView, completion gate inside the hold-timer slot, `RivalPrelimCount` publication on a global release grid | OC-1..OC-17, SP-1..SP-8 | Should |
| ADR-021 | UI/HUD layering, CommonUI and Enhanced Input | Presentation | UMG overlay layers at HUD-R4 ZOrders drawn after post-process; plain `USTRUCT` view-models; pre-created collapsed widgets; CommonUI for menus only and its input routing under the 5.8 EI/CommonUI unification; server-time smoothing for timed plates; DPI/scale rule; teammate-marker rendering (screen space, no depth test, edge clamp) | HUD-R1..R17, F1-F10; TMK TM-R3..R14; Camera CS-R15; IIF R13 | Should |
| ADR-022 | Rendering configuration and performance budgets | Presentation / cross-cutting | Substrate on/off for this project; skin cache default (Exclusive → per-mesh opt-in or Inclusive, R2); VSM; Lumen software SDF; MegaLights off; Nanite for kit, not for characters/bushes; draw-call and memory ceilings; frame-budget table and pre-decided fallbacks; profiling spikes R1-R9 as gates; the **CPU game-thread budget table by owner** (≤ 12 ms on the listen host, see Performance budget) | Art bible 8.x.2-8.x.16; SCL R12; Generator R-G9; CPA CP-18; Camera CS-R5; LP concern 9 | Should (gates mass production) |
| ADR-023 | Playtest telemetry hooks (pre-Telemetry GDD) | cross-cutting | Event Log extensions for brace timing, run share, final-court camping and team-size bucketing (D-W1..D-W4); Tell Matrix calibration counters; export only after `Ended`, never through `UE_LOG` | Round R19 F7; Crowd CNB-R16; cross-review "Notes for /create-architecture" | Should (before the first 10-player test) |

### Can defer to implementation or a later tier

| # | Title | Layer | Scope (one line) | Priority |
|---|---|---|---|---|
| ADR-024 | Dedicated server and Iris migration path | Foundation (Alpha) | Source-built engine, server-only code boundary, Iris re-evaluation with the AC-IVP-29 rerun gate, headless evidence playtests | Defer (Alpha) |
| ADR-025 | Server-side view filtering and anti-cheat | Feature (Alpha) | Whether any server fog-of-war beyond the concealment cull is worth its uniform cost; RPC abuse detection | Defer (Alpha) |

---

## Architecture Principles

1. **Kind-blind by construction (Pillar 1).** There is one pawn class, one command path, one replication configuration and one animation tiering function. No code path outside the Roster's and the Visibility Policy's server-side queries may branch on player vs NPC, team or participant state. Presentation reads only its carriers. A rule that would need a kind-keyed branch is redesigned, not special-cased.
2. **One authoritative total order, presented by the server clock** (a single client estimator, `EstimatedServerNow`, owned by ADR-002). Every interacting state change resolves inside Round's queue on the game thread; consequences run inline in the causing slot. Anything observable about an outcome is staged to a server time and presented by idempotent, state-derived client handlers, so arrival order and jitter never encode meaning.
3. **Default-deny information flow.** Every replicated property is on a checked-in allowlist with one audience and one carrier kind; a new fact or audience is a change request to the Visibility Policy, never a local decision. Leak tests are build gates, and every CRITICAL leak row with a pending spike blocks builds that leave the team.
4. **Data-driven, validated, frozen.** Every gameplay number is a schema-validated tuning key, frozen and hashed per round, identical for everyone; cross-field invariants reject or warn and never repair. Client-local preferences never become tuning.
5. **Determinism where shared, secrecy where hidden, evidence before commitment.** The public seed drives only geometry (integer RNG, Manifest, recomputed hash); server sub-seeds drive every hidden decision. Rules tagged with a spike (S1-S15, R1-R9, traversal, S-CON-1) stay provisional until the spike reports, and prototype code is promoted only after its ADR is Accepted.

### Performance budget (reference: RTX 3060-class, 1080p60; ASSUMED until spikes report)

| Budget | Value | Owner / source |
|---|---|---|
| Frame | 16.6 ms | technical-preferences |
| Animation, all characters (GT + worker sum) | ≤ 4 ms at 60 characters (`NpcCount` 50 since 2026-10-25; working value, re-verify in R3) | SCL R12, art 8.x.8 (ADR-0013, Proposed: summed GT + workers ≤ 4.0 ms with a GT share ≤ 2.0 ms) |
| CMC simulated proxies | 1.5-3 ms, reported separately | SCL R12 |
| Server AI + nav (listen host) | ≤ 1.5 ms steady | Crowd F5 |
| Per-client bandwidth | ≤ 50 KB/s (pawns ≤ 30 KB/s worst case) | SCL F9 |
| GPU allocation | 14.4 ms allocated + ≥ 2.0 ms headroom | art 8.x.16 |
| Layout | ≤ 7 s/attempt, ≤ 45 s total, ≤ 33 ms frames; server spawn + nav ≤ 3 s; client build ≤ 3 s p95 | LSC R4, Generator R-G9 |
| Draw calls (non-Nanite) / texture pool / package | ≤ 1,500 / ≤ 1.0 GB / ≤ 3 GB | art 8.x.2 (proposed; not yet in technical-preferences) |

**CPU game-thread budget on the listen host (LP concern 9; ASSUMED, owner-allocated, must sum to ≤ 12 ms so ≥ 4.6 ms remains for render-thread sync, GC and spikes):**

| Owner | GT budget (ms) |
|---|---|
| Animation, GT share (the 4 ms anim line counts GT + workers) | 2.0 |
| CMC: 40 server-simulated NPCs + simulated proxies | 3.0 |
| Crowd AI + nav (server) | 1.5 |
| Queue drain, resolvers, Roster, staged writes | 0.5 |
| Replication (NetDriver, 10 connections, ~60 pawns, cull) | 1.5 |
| Camera eye-mask traces (3P) + concealment/IIF/objectives misc | 1.0 |
| UMG HUD + markers | 0.5 |
| Engine/world tick, other | 2.0 |
| **Total** | **12.0** |

Prototype evidence (not target hardware): 1 player + 40 NPCs GT 9.5 ms, RT 4.0, GPU 2.1 on an RTX 5090; 50 characters 89.6 fps avg, GT 11.2 ms, 0 % misses at 60; 10-instance in-process PIE 5v5 functional at 20 NPCs. GT 11.2 ms on a 16-core desktop with motion matching but without crowd AI cost, the queue, concealment probes or 10 real connections already consumes most of the 12 ms line, so spike R3 and a 10-connection listen-host capture **on 3060-class hardware** are prerequisites for ADR-013 and ADR-022.

---

## Open Questions

Decisions deferred; each must be resolved before the named layer is built.

| ID | Question | Resolves in | Blocks |
|---|---|---|---|
| AQ-01 | Legacy NetDriver or Iris for the MVP (S11)? Recommendation: legacy (`net.Iris.UseIrisReplication` defaults to 0 [V]; specialists' recommendation; Iris plugin flagged Beta in 5.8.4; pointer and PlayerState trimming unverified under Iris). Whichever is chosen is pinned in `DefaultEngine.ini` (project config change, outside this run's write scope) and asserted by the harness. | ADR-001 | Foundation |
| AQ-02 | Carrier for the owner-only informant countdown end `T_due` (C-W5). **Closed 2026-10-25 (user decision): the C-TeamView owner part** (ADR-0001 §6 option (a)); HUD shows a live countdown. | user → ADR-001 → ADR-019 | Penalty, HUD |
| AQ-03 | Version control (no repository exists). **Pending user decision**; Git + LFS recommended. | user → ADR-009 | all coding |
| AQ-04 | Can `APawn::PlayerState/Controller` and `AActor::Owner` be made owner-only with identical raw-bunch class for both kinds and no broken possession/RPC routing (S1, S14)? | spikes before MVP-core | Pillar-1 network claim |
| AQ-05 | Concealment cull latency and channel-close behaviour, re-open (NetGUID reuse, ABP init), cull delay vs lag (S9, S10, S12); blind-attack probing as an occupancy oracle (S15). | ADR-003 / ADR-017 spikes | Concealment |
| AQ-06 | CMC cadence and correction parity, AI- vs PC-possessed, adaptive frequency off (S13); traversal replication under 100 ms / 1 % loss. | ADR-003 / ADR-010 spikes | Shared Pawn |
| AQ-07 | Motion-matching cost at 60 characters under the tier table (R3; `NpcCount` 50 since 2026-10-25); skin cache mode with VSM (R2); kit Lumen/Nanite cost at 5,000 instances (R1); Chaos bodies + nav build time (R4); concealment shadow/Lumen/RT leak (R5). | ADR-013 / ADR-022 / ADR-014 | mass production gate |
| AQ-08 | Substrate on or off for this existing project (no `r.Substrate` key)? *ADR-0022 (Proposed): off (`r.Substrate=0` pin pending user).* | ADR-022 | material authoring |
| AQ-09 | The sample-enabled `Mover` and `NetworkPrediction` plugins: disable (project file change, outside this run's write scope) or leave unused? Recommendation: disable when ADR-010 is accepted. | ADR-010 | Shared Pawn |
| AQ-10 | Do game-instance subsystems and the C-TeamView components survive seamless travel as assumed, in multi-process standalone (S3)? Does PIE seamless travel represent it? | ADR-005 | Session |
| AQ-11 | Owner reply RPCs vs replicated state ordering: `ContactServerTime` in `FStrikeResult`, `RetireAtServerTime` on the C-TeamView retire, a per-entry `Seq` in the fallback feed, `FActionState.StartServerTime` and `FDeathState.bDiedConcealed`. **Closed 2026-10-25: all approved by the user** and specified in ADR-0001/0003/0017/0018. | user → ADR-003 / ADR-017 / ADR-018 / ADR-021 | Concealment, HUD, Markers |
| AQ-12 | Tuning-schema items: duplicate latency keys (C-W14: `LatencyWarnRttSec` retired, `HudLatencyWarnRttSec` kept), TDC CR 10 key renames (C-W21: applied), `NpcKillStrikeLimit` hard minimum 2 with a test-preset flag (OQ-12), host-selectable presets never ship (TDC OQ-6), new keys `LayoutExactRefineMaxCount` and the three Crowd asset keys, `NpcCount` 50. **Closed 2026-10-25 by user decisions** (ADR-0004 amendments). | user → ADR-004 | TDC AC-3a |
| AQ-13 | Round-start world-ready second ack stage (Round CR-13(d)). **Closed 2026-10-25 (user decision): adopted** as `ServerWorldReady` → `OnParticipantWorldReady` (Session rule 11, ADR-0005); `RevealPawnWaitMaxSec` stays as the client backstop. | user → ADR-005 | Session/Round |
| AQ-14 | Listen-server host holds all ServerOnly facts (IVP R10): trusted playtests only until the Dedicated Server (Alpha); headless evidence server deferred to Alpha by the user 2026-10-25 (non-competing-host rule stands). | ADR-024 | external playtests |
| AQ-15 | CommonUI scope: menus only, or also the in-round HUD input layer, given the 5.8 EI/CommonUI unification? *ADR-0021 (Proposed): menus only (the menu root); the in-round HUD has no input layer.* | ADR-021 | HUD |
| AQ-17 | Does the client receive the seed only through the replicated `FRoundParams` (not the `?Seed=` server URL option)? S3 verifies. | ADR-005 | Session, Layout |
| AQ-18 | All performance numbers come from an RTX 5090 / 16-core desktop; no 3060-class capture exists. Who runs R3 and the 10-connection listen-host capture on target-class hardware, and when? | ADR-013 / ADR-022 | mass production, ADR-013 acceptance |
| AQ-19 | Module count: four runtime modules (recommended, LP) or the seven-module variant (stronger UBT-enforced layering, more interface/export work)? | ADR-007 | all code |
| AQ-16 | Event Log memory/eviction under the 10-player worst case (Round F8) and whether any VS Post-Round Reveal needs a persisted archive. *ADR-0023 (Proposed) §5 answers the memory bound; it found that a rejected-command flood can evict the kill timeline (Round R19 change request).* | ADR-023 / VS | Telemetry, Post-Round Reveal |

---

## Appendix A — Technical Requirements Baseline

Extracted from 18 GDDs | 170 baseline requirements | all 170 **registered as permanent** (2026-10-25) | 177 in the registry
(Status refreshed 2026-10-25: `/architecture-review` registered every ID below unchanged in `docs/architecture/tr-registry.yaml` (version 2) and added 7 IDs that are not listed here: TR-roster-012, TR-iif-013, TR-round-013, TR-ivp-012, TR-scl-014, TR-con-011, TR-asn-011. Their wording and current coverage are in the registry and in [`requirements-traceability.md`](requirements-traceability.md). The ADR column below is the baseline assignment, and ADR-001..023 = ADR-0001..0023, all written and Proposed. Review: [`architecture-review-2026-10-25.md`](architecture-review-2026-10-25.md).)

Method: requirements were extracted from each GDD's Summary, Core Rules (headlines and the rules marked "-> ADR"), Interactions, Dependencies and the cross-review "Notes for /create-architecture"; no GDD contributed nothing. System slugs: `tdc`, `roster`, `iif`, `cpa`, `session`, `round`, `ivp`, `lsc`, `scl`, `camera`, `crowd`, `con`, `asn`, `pen`, `oc`, `citygen`, `tmk`, `hud`.

| Req ID | GDD | System | Requirement | Domain | ADR |
|---|---|---|---|---|---|
| TR-tdc-001 | tuning-data-config.md | Tuning | Every gameplay number is a key in one cooked data-asset class of per-owner sub-structs; presets listed by a catalog | Data | 004 |
| TR-tdc-002 | tuning-data-config.md | Tuning | C++ schema row per key (type, default, hard range, audience, latch); editor metadata is a mirror; schema defaults pinned to GDD defaults by test | Data | 004, 008 |
| TR-tdc-003 | tuning-data-config.md | Tuning | Server-only resolve/validate/freeze at `Preparing` with the CR-6 ladder and fallback rungs (`FallbackLevel`, `ValidationReport`) | Core | 004 |
| TR-tdc-004 | tuning-data-config.md | Tuning | Cross-field invariants reject or warn, never clamp; safe ranges linted against all invariants at other keys' defaults | Data | 004 |
| TR-tdc-005 | tuning-data-config.md | Tuning | Immutable `ValidatedTuning` per round; `TuningHash` (server-only) and `ClientHash` (public keys) | Core | 004 |
| TR-tdc-006 | tuning-data-config.md | Tuning | ServerOnly/All audience split; client snapshot replicated; client face typed to the public struct (compile-time guarantee) | Networking | 004, 007 |
| TR-tdc-007 | tuning-data-config.md | Tuning | World-scoped accessor correct in multi-client PIE; no per-pawn/team/viewer parameter | Core | 004 |
| TR-tdc-008 | tuning-data-config.md | Tuning | Latch classes Round/Action/Use honoured by every consumer | Core | 004 |
| TR-tdc-009 | tuning-data-config.md | Tuning | No back doors: no `Config` properties, no ini/CVar overrides in Shipping; Shipping always resolves `Default` | Security | 004 |
| TR-tdc-010 | tuning-data-config.md | Tuning | PIE-only hot-reload re-validation, debounced, routed to the PIE server world | Tools | 004 |
| TR-tdc-011 | tuning-data-config.md | Tuning | Client tuning check rides Session's single acknowledgement (no TDC RPC/timer) | Networking | 004 |
| TR-roster-001 | participant-state-roster.md | Roster | Game-instance store of `ParticipantRecord`s surviving seamless and hard travel; `BeginMatch`/`EndMatch` | Core | 005 |
| TR-roster-002 | participant-state-roster.md | Roster | Single writer of lifecycle state through `RequestTransition` with the legal-transition table and result precedence; batch-only reset/end | Core | 005 |
| TR-roster-003 | participant-state-roster.md | Roster | Game-thread-only mutation; events queued and dispatched FIFO after the outermost call; `EventSeq` +1 per event | Core | 002 |
| TR-roster-004 | participant-state-roster.md | Roster | Pawn binding states with bind-before-visible spawn and crowd-adoption paths; tombstones | Core | 010 |
| TR-roster-005 | participant-state-roster.md | Roster | O(1) allocation-free server queries; on clients return 0/false and report to the fault sink | Core | 010 |
| TR-roster-006 | participant-state-roster.md | Roster | Carriers C-Public (PlayerState allowlist), C-TeamView (owner-only PlayerController), C-Server (never replicated) | Networking | 001 |
| TR-roster-007 | participant-state-roster.md | Roster | Engine PlayerState defaults disabled or constant; frozen in-round `PlayerArray`; no PlayerState byte change on a transition | Networking | 001 |
| TR-roster-008 | participant-state-roster.md | Roster | Opponent counts never leave the server before `Ended` | Security | 001 |
| TR-roster-009 | participant-state-roster.md | Roster | All presentation, incl. the listen host, reads only C-TeamView and public structs | Security | 001, 007 |
| TR-roster-010 | participant-state-roster.md | Roster | Disconnect/reclaim keyed by IdentityKey; epoch token and connection generation guard every timer | Networking | 005 |
| TR-roster-011 | participant-state-roster.md | Roster | Pawn replication configuration identical for NPC and player and invariant under possession | Networking | 001 |
| TR-iif-001 | input-interaction-framework.md | IIF | One non-replicated command component per pawn with the seven entry points; no caller identity anywhere | Core | 011 |
| TR-iif-002 | input-interaction-framework.md | IIF | Human Move/Look through CMC saved moves; AI through the same entry points; one control rotation, body-yaw rule and pitch path | Networking | 010, 011 |
| TR-iif-003 | input-interaction-framework.md | IIF | Discrete commands as Enhanced Input edges; the Hold trigger never carries server meaning | Input | 011, 021 |
| TR-iif-004 | input-interaction-framework.md | IIF | Two-stage server validation (ingress with token buckets; command check at the queue slot) | Security | 011 |
| TR-iif-005 | input-interaction-framework.md | IIF | Hold timers as queue items from server time; 0.10 s release debounce for every pawn | Core | 002 |
| TR-iif-006 | input-interaction-framework.md | IIF | One action slot per pawn, priority Attack > Prepare > Interact > Hide | Core | 002 |
| TR-iif-007 | input-interaction-framework.md | IIF | Exactly one owner reply per resolution-voided command; ingress rejections silent | Networking | 002 |
| TR-iif-008 | input-interaction-framework.md | IIF | Server-only action hub with `OnActionStarted/Cancelled/Completed` in one world-level hub | Core | 011 |
| TR-iif-009 | input-interaction-framework.md | IIF | Replicated fixed-size `FActionState` with one RepNotify; nothing else per action replicated except owner data | Networking | 003 |
| TR-iif-010 | input-interaction-framework.md | IIF | Attack payload from the camera aim ray; no lock-on, no target-dependent presentation | Input | 011 |
| TR-iif-011 | input-interaction-framework.md | IIF | Input contexts swap on owner state (UX only); AI subject to the same token buckets | Input | 011 |
| TR-iif-012 | input-interaction-framework.md | IIF | Input latency budget (F5) met for human commands | Performance | 011 |
| TR-cpa-001 | character-pool-appearance.md | CPA | Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId` | Data | 012 |
| TR-cpa-002 | character-pool-appearance.md | CPA | `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates | Core | 012 |
| TR-cpa-003 | character-pool-appearance.md | CPA | Server-private streams from `GetServerSubSeed(AppearanceDeal)`; DRBG-class primitive for Replace/Take | Security | 006, 012 |
| TR-cpa-004 | character-pool-appearance.md | CPA | Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption | Networking | 012 |
| TR-cpa-005 | character-pool-appearance.md | CPA | All pool assets resident and precached before leaving `Preparing`; no first-draw load | Rendering | 012, 022 |
| TR-cpa-006 | character-pool-appearance.md | CPA | One look path for every viewer and pawn kind; no keyed material parameters | Rendering | 001, 012 |
| TR-cpa-007 | character-pool-appearance.md | CPA | Statistical indistinguishability gate G-DECK automated | Testing | 008 |
| TR-session-001 | session-travel.md | Session | Listen-server direct join; ordered admission gates at PreLogin; capacity `2 × TeamSize` | Networking | 005 |
| TR-session-002 | session-travel.md | Session | Persistent 128-bit IdentityKey per install with launch/PIE override | Core | 005 |
| TR-session-003 | session-travel.md | Session | Round travel by seamless `ServerTravel ?Seed=` with the Roster reset ordering | Networking | 005 |
| TR-session-004 | session-travel.md | Session | Public seed vs server-secret keyed sub-seeds | Security | 006 |
| TR-session-005 | session-travel.md | Session | Single round acknowledgement (seed, layout hash, tuning hash, pool hash) within the travel deadline | Networking | 005 |
| TR-session-006 | session-travel.md | Session | Liveness silence clock, rejoin window, supersede, client auto-rejoin | Networking | 005 |
| TR-session-007 | session-travel.md | Session | Session never spawns/frees pawns or PlayerStates on connection events; default login pawn disabled | Core | 005 |
| TR-session-008 | session-travel.md | Session | Host parity: the host's view reads the replicated view path | Security | 001 |
| TR-session-009 | session-travel.md | Session | No host migration; session-ending notice; client host-loss detection | Networking | 005 |
| TR-session-010 | session-travel.md | Session | Leak-surface checklist (PlayerState fields, ping, join timing) enforced via the allowlist | Security | 001 |
| TR-round-001 | round-match-framework.md | Round | Five-phase machine in a game-instance store surviving travel | Core | 005 |
| TR-round-002 | round-match-framework.md | Round | One queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake | Core | 002 |
| TR-round-003 | round-match-framework.md | Round | Drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item | Core | 002 |
| TR-round-004 | round-match-framework.md | Round | Single-assignment latch and the two-step round end (latch step; round-end step at the deciding strike's death write) | Networking | 002 |
| TR-round-005 | round-match-framework.md | Round | Objective and elimination wins; no draw, no clock | Core | 002 |
| TR-round-006 | round-match-framework.md | Round | Preparing readiness sequence, spawn gate and watchdog | Core | 005 |
| TR-round-007 | round-match-framework.md | Round | Disconnect policy: blend hand-over, guarded grace timer, forfeit/void | Core | 002 |
| TR-round-008 | round-match-framework.md | Round | Server-only Event Log ring with release classes; never through logs or traces | Security | 007 |
| TR-round-009 | round-match-framework.md | Round | `FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal | Networking | 003 |
| TR-round-010 | round-match-framework.md | Round | Host commands on the game instance with a net-mode check | Tools | 005 |
| TR-round-011 | round-match-framework.md | Round | Single shuffled spawn list in one frame, server-secret order | Security | 006 |
| TR-round-012 | round-match-framework.md | Round | Playtest instrumentation (brace timing, run share, final camping, team-size bucket) in the Event Log | Data | 023 |
| TR-ivp-001 | information-visibility-policy.md | IVP | `CanViewerKnow` server query, default deny, seven viewer classes | Security | 001 |
| TR-ivp-002 | information-visibility-policy.md | IVP | One carrier kind per audience | Networking | 001 |
| TR-ivp-003 | information-visibility-policy.md | IVP | UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator) | Networking | 001 |
| TR-ivp-004 | information-visibility-policy.md | IVP | Concealment hybrid: quantised server cull for non-allies + client presentation hide | Networking | 003 |
| TR-ivp-005 | information-visibility-policy.md | IVP | Relevancy varies by concealment and relation only | Networking | 003 |
| TR-ivp-006 | information-visibility-policy.md | IVP | Single allowlist file, reflection audit and net-config audit on defaults and live instances | Testing | 003, 008 |
| TR-ivp-007 | information-visibility-policy.md | IVP | Leak catalogue with CRITICAL ship gate; per-commit canaries | Testing | 003, 008 |
| TR-ivp-008 | information-visibility-policy.md | IVP | Engine spectator state banned; client-local no-pawn view | Core | 001 |
| TR-ivp-009 | information-visibility-policy.md | IVP | Teammate fallback position feed on C-TeamView | Networking | 003 |
| TR-ivp-010 | information-visibility-policy.md | IVP | Iris rerun gate if Iris is enabled | Testing | 003, 008 |
| TR-ivp-011 | information-visibility-policy.md | IVP | Presentation modules cannot include server query headers | Security | 001, 007 |
| TR-lsc-001 | level-socket-contract.md | LSC | Layout service world subsystem with registered providers; Round talks only to it | Core | 014 |
| TR-lsc-002 | level-socket-contract.md | LSC | Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash` | Networking | 014 |
| TR-lsc-003 | level-socket-contract.md | LSC | Zero replicated actors; bush/court components built locally | Networking | 003, 014 |
| TR-lsc-004 | level-socket-contract.md | LSC | Server nav build, Validate and Select within attempt/total budgets; ≤ 33 ms slices | Navigation | 014 |
| TR-lsc-005 | level-socket-contract.md | LSC | Reseed and graybox fallback travel | Core | 014 |
| TR-lsc-006 | level-socket-contract.md | LSC | Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn | Core | 014, 019 |
| TR-lsc-007 | level-socket-contract.md | LSC | Round-start placement and spawn gate | Core | 014 |
| TR-lsc-008 | level-socket-contract.md | LSC | Pillar-1 placement constraints (no area type for spawns/objectives) | Design | 014 |
| TR-lsc-009 | level-socket-contract.md | LSC | Server-only query API for site assignment and team spawns | Security | 014 |
| TR-scl-001 | shared-character-locomotion.md | SCL | One Shared Pawn class; one spawn factory; takeover function | Core | 010 |
| TR-scl-002 | shared-character-locomotion.md | SCL | Disconnect never destroys a pawn (`PawnLeavingGame` override, same-frame AI possession) | Networking | 010 |
| TR-scl-003 | shared-character-locomotion.md | SCL | Possession parity and stop-and-blend hand-over; `ForceNetUpdate` deferral | Networking | 003, 010 |
| TR-scl-004 | shared-character-locomotion.md | SCL | CMC locomotion, speed tiers, state restrictions, replicated traversal set (spike) | Physics | 010 |
| TR-scl-005 | shared-character-locomotion.md | SCL | One look path; `RemoteViewPitch16` written from `Look` for every pawn | Networking | 001 |
| TR-scl-006 | shared-character-locomotion.md | SCL | One AnimBP, motion-matching database, chooser and pose set; `ActionPose`-driven states | Animation | 013 |
| TR-scl-007 | shared-character-locomotion.md | SCL | Significance-only tiers by screen height; caps; client-local tie key; server always-ticks pose | Animation | 013 |
| TR-scl-008 | shared-character-locomotion.md | SCL | ≤ 4 ms animation at 60 characters; ≤ 36 KB/s pawn replication per client (working values since `NpcCount` 50, 2026-10-25) | Performance | 013, 022 |
| TR-scl-009 | shared-character-locomotion.md | SCL | Net parity config (15 Hz fixed, priority 3.0, `DORM_Never`, no cull distance, no pawn multicast) | Networking | 003 |
| TR-scl-010 | shared-character-locomotion.md | SCL | Smoothing parity and input-starvation rule | Networking | 003, 010 |
| TR-scl-011 | shared-character-locomotion.md | SCL | Footstep path parity from one step clock | Audio | 013 |
| TR-scl-012 | shared-character-locomotion.md | SCL | Published server state (`IsRunning`, `IsInTraversal`, commanded displacement) | Core | 010 |
| TR-scl-013 | shared-character-locomotion.md | SCL | One capsule, collision profile and physics asset | Physics | 010 |
| TR-camera-001 | camera-system.md | Camera | Local-only toggle; nothing replicated about camera mode | Networking | 003, 015 |
| TR-camera-002 | camera-system.md | Camera | View envelope −60/+40 in both modes relative to body facing | Input | 015 |
| TR-camera-003 | camera-system.md | Camera | 3P line-of-sight rule: boom geometry + eye-origin visibility mask hiding mesh, shadow and RT | Rendering | 015, 022 |
| TR-camera-004 | camera-system.md | Camera | Mask reads geometry only; α = 0 counts as not rendered for IIF, SCL and the strike hint | Core | 015 |
| TR-camera-005 | camera-system.md | Camera | One shared `GetAimRay` | Input | 015 |
| TR-camera-006 | camera-system.md | Camera | `GetViewState` for HUD/markers; no-pawn view states | UI | 015, 021 |
| TR-camera-007 | camera-system.md | Camera | Audio listener at the eye pivot | Audio | 015 |
| TR-camera-008 | camera-system.md | Camera | Eye-mask trace cost bounded at 60 pawns (time slicing beyond 60 m) | Performance | 015 |
| TR-crowd-001 | crowd-npc-behaviour.md | Crowd | Server-only Crowd Director subsystem | AI | 016 |
| TR-crowd-002 | crowd-npc-behaviour.md | Crowd | StateTree per NPC (~4 Hz) + follower emitting `Move`/`Look` through the IIF; no path-following driving | AI | 016 |
| TR-crowd-003 | crowd-npc-behaviour.md | Crowd | Async path queries ≤ 2 starts/frame; navmesh frozen at `OnLayoutReady`; 2 m spatial hash | Navigation | 014, 016 |
| TR-crowd-004 | crowd-npc-behaviour.md | Crowd | Identity blindness: no crowd code reads player/binding/team/state | Security | 001 |
| TR-crowd-005 | crowd-npc-behaviour.md | Crowd | Counter-based draws from the crowd server sub-seed | Core | 006 |
| TR-crowd-006 | crowd-npc-behaviour.md | Crowd | ≤ 1.5 ms AI + nav; global-only cuts; never changes replication | Performance | 016 |
| TR-crowd-007 | crowd-npc-behaviour.md | Crowd | Participant-bound AI driving and takeover eligibility supply | AI | 016 |
| TR-crowd-008 | crowd-npc-behaviour.md | Crowd | Ambient emergence and replacement via shared sockets and `DrawReplacement` | AI | 016 |
| TR-crowd-009 | crowd-npc-behaviour.md | Crowd | Dormant until `OnRoundActive`; Halt in the round-end step | AI | 016 |
| TR-crowd-010 | crowd-npc-behaviour.md | Crowd | Baseline and Tell Matrix rates as data with calibration counters | Data | 016, 023 |
| TR-con-001 | concealment-bushes.md | Concealment | One component per pawn; server-only record and state machine | Core | 017 |
| TR-con-002 | concealment-bushes.md | Concealment | Entry/exit legality through IIF predicates; no occupancy check | Core | 017 |
| TR-con-003 | concealment-bushes.md | Concealment | Hide volumes block non-hiding pawns | Physics | 017 |
| TR-con-004 | concealment-bushes.md | Concealment | Owner-side warp for the enter transition (S-CON-1) | Animation | 017 |
| TR-con-005 | concealment-bushes.md | Concealment | Hide clips scheduled from replicated server start time | Networking | 003, 017 |
| TR-con-006 | concealment-bushes.md | Concealment | Fail-closed client latch; hide removes body, attachments, shadow, Lumen and RT | Rendering | 017, 022 |
| TR-con-007 | concealment-bushes.md | Concealment | `IsConcealedFrom` server-only; blind-strike spot zones | Core | 017 |
| TR-con-008 | concealment-bushes.md | Concealment | Server cull per the hybrid; un-cull in the same server step | Networking | 003, 017 |
| TR-con-009 | concealment-bushes.md | Concealment | Rustle parity; NPCs through the same path | Audio | 017 |
| TR-con-010 | concealment-bushes.md | Concealment | Concealment log in the Event Log | Data | 017 |
| TR-asn-001 | assassination-defence-reversal.md | Assassination | Attack resolver re-checks live state at its slot; victim resolution on server capsules | Core | 002 |
| TR-asn-002 | assassination-defence-reversal.md | Assassination | Defence sampled at Attack execution by key compare; server-only defence component | Core | 002 |
| TR-asn-003 | assassination-defence-reversal.md | Assassination | Outcome-blind `FActionState` at the slot; outcome writes at contact via the pending-write queue | Networking | 003, 018 |
| TR-asn-004 | assassination-defence-reversal.md | Assassination | Uniform forced net updates for every outcome and victim kind | Networking | 003 |
| TR-asn-005 | assassination-defence-reversal.md | Assassination | Owner-only `FStrikeResult`/`FDefenceResult`; idempotent state-derived presentation | Networking | 018 |
| TR-asn-006 | assassination-defence-reversal.md | Assassination | `OnPawnKilled` emitted inside the slot | Core | 002 |
| TR-asn-007 | assassination-defence-reversal.md | Assassination | One body sequence for every victim; possession kept through the fall | Animation | 013, 018 |
| TR-asn-008 | assassination-defence-reversal.md | Assassination | Nothing outcome-caused presented before contact (server clock scheduling) | UI | 018 |
| TR-asn-009 | assassination-defence-reversal.md | Assassination | No replicated effect actors, no multicast | Networking | 003 |
| TR-asn-010 | assassination-defence-reversal.md | Assassination | Clients present contact-timed outcomes from one RTT-compensated estimate of server time with a stated accuracy budget (ASN-R21/R23, HUD-R6; added after TD-ARCHITECTURE concern 3) | Networking | 002 |
| TR-pen-001 | npc-kill-penalty-informants.md | Penalty | Server strike count and owner tally keyed by `ParticipantId`, round-scoped | Core | 019 |
| TR-pen-002 | npc-kill-penalty-informants.md | Penalty | Exile on the Nth NPC kill through the Roster; body released to the crowd | Core | 019 |
| TR-pen-003 | npc-kill-penalty-informants.md | Penalty | Respawn as a due-time-heap item at `T_due`; Absent path for disconnected victims | Core | 002, 019 |
| TR-pen-004 | npc-kill-penalty-informants.md | Penalty | Takeover of an unseen crowd pawn in one call stack; retries and widening | Core | 019 |
| TR-pen-005 | npc-kill-penalty-informants.md | Penalty | Informant-kill invariant: perception identical to an NPC kill | Security | 001, 019 |
| TR-pen-006 | npc-kill-penalty-informants.md | Penalty | Owner-only data (tally, `bExiled`, `T_due` — `T_due` and the tally re-push on the C-TeamView owner part, user decision 2026-10-25) | Networking | 001, 019 |
| TR-pen-007 | npc-kill-penalty-informants.md | Penalty | Informant restrictions enforced by their owners; Tell row T7 | Core | 019 |
| TR-pen-008 | npc-kill-penalty-informants.md | Penalty | Event Log rows | Data | 019 |
| TR-oc-001 | objectives-chain.md | Objectives | Chain data asset of `FObjectiveDef` rows | Data | 020 |
| TR-oc-002 | objectives-chain.md | Objectives | Court binding from `FSiteAssignment` at `OnLayoutReady`, server-only | Core | 020 |
| TR-oc-003 | objectives-chain.md | Objectives | `FObjectiveTeamView` rides C-TeamView; pushed at Active entry | Networking | 020 |
| TR-oc-004 | objectives-chain.md | Objectives | Tend predicate reads only site, public occupancy and legality class | Security | 020 |
| TR-oc-005 | objectives-chain.md | Objectives | Completion gate inside the hold-timer slot; `DeclareObjectiveWin` | Core | 002, 020 |
| TR-oc-006 | objectives-chain.md | Objectives | `RivalPrelimCount` with fresh per-publication delay and one global release grid | Networking | 006, 020 |
| TR-oc-007 | objectives-chain.md | Objectives | Stall-pressure rules (no timer, no reveal) | Design | 020 |
| TR-citygen-001 | procedural-city-generator.md | Generator | Pure function of attempt seed, kit version and params; asserts on server sub-seeds | Core | 014 |
| TR-citygen-002 | procedural-city-generator.md | Generator | Integer-only RNG and canonical iteration; golden seeds stable across CPUs/compilers | Testing | 006, 008 |
| TR-citygen-003 | procedural-city-generator.md | Generator | Twelve-stage pipeline with integer pre-validator; time-sliced | Performance | 014 |
| TR-citygen-004 | procedural-city-generator.md | Generator | ISM per module (Nanite), batch add, static mobility; ≤ 5,000 instances, ≤ 20,000 shapes | Rendering | 014, 022 |
| TR-citygen-005 | procedural-city-generator.md | Generator | Deterministic client decoration without collision | Rendering | 014 |
| TR-citygen-006 | procedural-city-generator.md | Generator | Server spawn + nav ≤ 3 s; client build ≤ 3 s p95 | Navigation | 014 |
| TR-citygen-007 | procedural-city-generator.md | Generator | Manifest ≤ 3,500 entries | Networking | 014 |
| TR-citygen-008 | procedural-city-generator.md | Generator | Failure modes → reseed or graybox fallback | Core | 014 |
| TR-tmk-001 | teammate-markers.md | Markers | Marker set only from the local C-TeamView (+ feed); no world iteration | Security | 001 |
| TR-tmk-002 | teammate-markers.md | Markers | Anchor on the resolved `PawnRef`'s visual root; null semantics | UI | 021 |
| TR-tmk-003 | teammate-markers.md | Markers | Fill states from C-TeamView state and phase (`RoundEnded` before `Ended` = no change) | UI | 021 |
| TR-tmk-004 | teammate-markers.md | Markers | Screen-space overlay after post-process, no depth test, constant size, edge clamp | UI | 021 |
| TR-tmk-005 | teammate-markers.md | Markers | `IsMarkedAlly` and `OnAllyMarkerRetired` client-local API | UI | 021 |
| TR-tmk-006 | teammate-markers.md | Markers | View gate through Camera `GetViewState` | UI | 021 |
| TR-hud-001 | hud.md | HUD | Reads only the three carriers | Security | 001 |
| TR-hud-002 | hud.md | HUD | Subject-kind parity rule and automated leak suite | Testing | 008 |
| TR-hud-003 | hud.md | HUD | Layer/ZOrder stack after post-process; safe-margin anchors | UI | 021 |
| TR-hud-004 | hud.md | HUD | Server-clock timing: plates at local contact frame, tally with plate, `PresentedOwnState` | UI | 018, 021 |
| TR-hud-005 | hud.md | HUD | Round surfaces from `FRoundPublicState`; rebuild on `RoundEpoch` change | UI | 021 |
| TR-hud-006 | hud.md | HUD | Client-local settings hooks (scale, reduce motion, high contrast) | UI | 021 |
| TR-hud-007 | hud.md | HUD | Latency warning threshold (`HudLatencyWarnRttSec`; duplicate retired 2026-10-25, C-W14) | UI | 021 |

## Change log — user decisions 2026-10-25
Data/schema and confirmation decisions applied across the GDDs, ADRs (Amendments sections), registries and control manifest: wrong-target tally confirmed; `T_due`, `{WrongTargetTally, LastResultSeq}` on the C-TeamView owner part; teammate `RetireAtServerTime`; feed `Seq`; `FActionState.StartServerTime`; `FDeathState.bDiedConcealed`; `FStrikeResult.ContactServerTime`; `InformantKilled`; `NpcKillStrikeLimit` hard minimum 2; world-ready ack stage; `LayoutExactRefineMaxCount`; Crowd asset keys; pre-baked route-edge paths; `InteractSecOverride` removed; `LatencyWarnRttSec` retired; TDC CR-10 renames; `NpcCount` 50 (60 characters at 5v5; budgets re-verified in R3). AQ-02, AQ-11, AQ-12 and AQ-13 closed; AQ-14's headless evidence server deferred to Alpha.
