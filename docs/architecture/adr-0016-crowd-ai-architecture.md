# ADR-0016: Crowd AI architecture

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It cannot be Accepted before ADR-0001, ADR-0002, ADR-0006, ADR-0010, ADR-0011 and ADR-0014 are Accepted and the 50-character spike (AC-CNB-26 capture on the reference host) has reported; the cost rule (§10) stays provisional until then.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (behaviour clocks run on a per-frame due-time list, not the decision tick; Halt only latches and clears intent, holds and stopping stay Round's/ADR-0010's; continuous draws name AS241 and retry-extended tuples; possession only through ADR-0010's `RequestHandOver` and hand-over pass, names reconciled with ADR-0010; virtual camera integrated every controller tick; schedule cancels by revalidation; 70-pawn and virtual-input cost added to the spike). Engine validation (unreal-specialist lens): BLOCKING → REVISED (AI possession creates a replicated `UGameplayTasksComponent` on AI-driven pawns only — now a default, non-replicated subobject on every pawn via ADR-0010; StateTree schedule re-imposed in an overridden `TickComponent`; StateTree tick moved to `TG_PrePhysics`; `StateTree.Component.ScheduledTickEnabled` asserted; path results ≥ 2 frames; follower stepped before `Super::Tick`; Halt/handover by direct calls, not only events).

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims re-read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; design against `design/gdd/crowd-npc-behaviour.md` as revised and reconciled 2026-10-24)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

## Summary
About 50 NPCs (`NpcCount` 50 since the user decision 2026-10-25) must share the players' pawn class and command path and still act like a learnable crowd within 1.5 ms of listen-host game-thread time. A server-only **Crowd Director** world subsystem owns the population ledger, the ambient schedule (as queue timer items), perception, path budgeting, counter-based draws and **Halt**. Each NPC has a crowd AIController running a scheduled-tick **StateTree** for decisions (~4 Hz, event interrupts) and a **follower** that turns paths into human-shaped `Move`/`Look` through a virtual input layer and the pawn command interface (ADR-0011). Nothing drives the pawn through path following, `MoveTo`, RVO or a direct velocity write.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | AI / Navigation (with Core gameplay framework constraints) |
| **Layer** | Feature |
| **Knowledge Risk** | MEDIUM (architecture.md: AI and Navigation MEDIUM). StateTree is post-cutoff and has **no page** in `docs/engine-reference/unreal/`, so every StateTree claim below was read from 5.8.4 source |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/navigation.md`, `breaking-changes.md`, `deprecated-apis.md`, `PLUGINS.md` (Mass status); `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Feature layer, Data Flow §1, API Boundaries, Performance budget, Required ADRs, Open Questions); ADR-0001, ADR-0002, ADR-0003, ADR-0006 (read); engine source listed in the facts table below; prototype `Source/StealthGame/Private/StealthNpcController.cpp` (evidence only) |
| **Post-Cutoff APIs Used** | `UStateTreeAIComponent` / `UStateTreeComponent::ScheduleTickFrame` / `FStateTreeScheduledTick` / `UStateTreeComponentSchema::ScheduledTickPolicy` (GameplayStateTree, 5.x) [V]; `UE_LOGF` (5.8) for diagnostic logging [D]. Everything else predates the cutoff and is re-verified below |
| **Verification Required** | (1) 60-character spike (`NpcCount` 50 + 10 players): AC-CNB-26 Insights capture of the six `Crowd.*` scopes ≤ 1.5 ms p99 / ≤ 0.8 ms mean on the reference host, multi-process, 60 fps locked. (2) Crowd controller built **without** a `PathFollowingComponent` runs with the gameplay debugger and visual logger enabled (E3: `AIController.cpp` null-checks every use [V]; debugger/vislog [U]). (3) Phase-aligned StateTree schedule survives events (E12) and spreads decisions as AC-CNB-33 requires. (4) AC-CNB-38: no old-epoch command and no non-`Move(0)` motion after Halt. (5) AC-CNB-27 pipeline parity through the recorded virtual-input stream, at the default and at every cut-order step. (6) Async path results arrive ≥ 2 frames after the request (E7 [V]); latency under load [U]. (7) Replicated-subobject inventory of an AI-driven, a player-driven and a just-handed-over pawn is identical (E13; leak suite, ADR-0008). (8) The 70-pawn / 30 fps run and the virtual-input-layer cost (including `EvaluateViewEnvelope` per controller tick) are captured in the spike |

**Engine facts this ADR relies on** ([V] read in the 5.8.4 source this session; [D] engine-reference; [U] unverified, owned by the named check):

| # | Fact | Tag |
|---|---|---|
| E1 | `AAIController::Tick` calls `UpdateControlRotation(DeltaTime)` every frame; `UpdateControlRotation` is virtual, computes rotation toward the focal point (or pawn orientation) and **zeroes pitch unless the focus actor is a pawn**, then `SetControlRotation` and, if `bUpdatePawn`, `FaceRotation` | [V] `AIModule/Private/AIController.cpp:58-63, 445-481`; `AIController.h:378` |
| E2 | On possession `AController::AddPawnTickDependency` makes the pawn's movement component tick (and the pawn tick, if needed) a prerequisite-after of the controller's tick, so a controller tick runs before the CMC that frame | [V] `Engine/Private/Controller.cpp:495-515, 539` |
| E3 | `AAIController` creates its `PathFollowingComponent` with `CreateOptionalDefaultSubobject`, so a subclass can suppress it with `DoNotCreateDefaultSubobject(TEXT("PathFollowingComponent"))`; `SetPathFollowingComponent` exists. `AIController.cpp` null-checks every `PathFollowingComponent` use and the StateTree AI schema never references it; the gameplay debugger and visual logger are unchecked | [V] `AIController.cpp:44, 45, 110, 122, 157, 502, 556, 595, 615, 681, 789-829, 954-981`, `AIController.h:134, 444, 457` (engine gate) / [U] debugger, vislog: verification (2) |
| E4 | `AAIController::bWantsPlayerState` and `bStartAILogicOnPossess` exist and already default to false (the prototype sets the first again) | [V] `AIController.h:98, 120`; `AIController.cpp:51, 54` (engine gate); prototype `StealthNpcController.cpp:10` |
| E5 | `UStateTreeAIComponent : UStateTreeComponent : UBrainComponent`; `SendStateTreeEvent` is public; `StartLogic`/`StopLogic`/`SetStartLogicAutomatically` are public; `ScheduleTickFrame(const FStateTreeScheduledTick&)` is **protected** (a subclass may call it); `FStateTreeScheduledTick::MakeSleep / MakeEveryFrames / MakeNextFrame / MakeCustomTickRate(float)` exist | [V] `GameplayStateTree/.../Components/StateTreeAIComponent.h:15-21`, `StateTreeComponent.h:39, 54-56, 113-120, 141, 161`; `StateTree/.../StateTreeExecutionTypes.h:815-832` |
| E6 | Scheduled ticks are honoured only when the schema allows them: `UStateTreeComponentSchema::ScheduledTickPolicy` ∈ {Default, Allowed, Denied}; Default reads cvar `StateTree.Component.DefaultScheduledTickAllowed` (default true). A second cvar, `StateTree.Component.ScheduledTickEnabled`, overrides the policy and forces every-frame ticks when off. The allowed flag is cached on the **compiled** tree, so assets must be recompiled after a schema change. `ScheduledTickPolicy` is a protected editable property; `UStateTreeAIComponentSchema` is exported, not final, inherits it, and has an `AIControllerClass` property; the component accepts any schema derived from `UStateTreeComponentSchema` | [V] `StateTreeComponentSchema.h:18-22, 86-89`; `StateTreeComponentSchema.cpp:24-28, 58-66`; `StateTree.h:329-331, 635`; `StateTreeComponent.cpp:19-24, 302-309, 461-462`; `StateTreeAIComponentSchema.cpp:20-32` (engine gate) |
| E7 | `UNavigationSystemV1::FindPathAsync` only queues the query; the navigation system's tick dispatches queued queries as one task-graph task and dispatches the **previous** frame's completed results on the game thread through the query delegates. The navigation tick runs **before** `TG_PrePhysics`, so a query issued in a `TG_PrePhysics` tick is triggered next frame and its result dispatched the frame after: **≥ 2 frames** | [V] `NavigationSystem/Private/NavigationSystem.cpp:1813-1829, 1971-1990, 2018-2027, 2039`; `LevelTick.cpp:1647` vs `1750` (engine gate) |
| E8 | The CMC has `bUseRVOAvoidance` (RVO rewrites the requested velocity) and `RequestDirectMove` (path following's velocity entry) | [V] `CharacterMovementComponent.h:1065, 1348` |
| E9 | GameplayStateTree ships a `StateTreeMoveToTask` (path-following based) | [V] `GameplayStateTree/.../Tasks/StateTreeMoveToTask.h` |
| E10 | Mass Gameplay / Mass Crowd are flagged Experimental in 5.8 | [D] `PLUGINS.md` |
| E11 | Recast async queries return string-pulled corner paths for the default filter | [U] (follower test AC-CNB-04 covers it; if not, the follower funnels the returned path itself) |
| E12 | After every tree tick `UStateTreeComponent` overwrites its schedule with `ScheduleTickFrame(GetNextScheduledTick())`; events wake the tree with `MakeNextFrame`; `SetComponentTickIntervalAndCooldown` resets the cooldown — so a phase offset set once is lost after the first event. `TickComponent` is virtual | [V] `StateTreeComponent.cpp:30-35, 146, 202, 288, 335-338, 348-352`; `StateTreeComponent.h:50` (engine gate) |
| E13 | `AAIController::OnPossess` creates a `UGameplayTasksComponent` on the pawn if it has none, and that component replicates by default — so without countermeasure only AI-driven pawns would carry an extra replicated component, and it stays after a takeover | [V] `AIController.cpp:519`; `GameplayTasksComponent.cpp:57` (engine gate) |
| E14 | A brain component has no tick group of its own, so the StateTree component ticks in the default `TG_DuringPhysics` (after the CMC) unless set | [V] `ActorComponent.cpp:548`; `BrainComponent.cpp:191-195` (engine gate) |
| E15 | `SendStateTreeEvent` on a tree that is not running logs a warning and does nothing | [V] `StateTreeComponent.cpp:581` (engine gate) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (identity invariants: one pawn class, one command path, AI writes `Look` into the control rotation; must be Accepted). ADR-0002 (queue, server-local mailbox, due-time heap, round-end step that calls `ICrowdControl::Halt()`; this ADR **requests an amendment** adding `ETimerKind::CrowdSchedule`, §6). ADR-0006 (`ESeedDomain::CrowdBehaviour` = 13, `NpcRoutes` = 5, `AmbientSchedule` = 9, `FSecretStream::Draw` counter tuple). ADR-0010 (pawn factory `SpawnSharedPawn`, server spawn ordinal, `AStealthCrowdController` class, `RequestHandOver` + hand-over pass as the only possession path, server-only control epoch `GetControlEpoch(Pawn)` on the factory, stop-and-blend, last-command hold, `HandoverHoldSec`; this ADR **requests an ADR-0010 amendment**: a default, non-replicated `UGameplayTasksComponent` subobject on every `AStealthPawn`, E13). ADR-0011 (`UPawnCommandComponent` entry points, control-epoch stamping at ingress and `Voided(StaleControl)`, `CanBegin` dry run, held-intent `Move`, token buckets, null reply sink). ADR-0014 (frozen navmesh at `OnLayoutReady`, `CrowdRoute` graph and pre-baked edge paths in the Manifest, `RequestEmergence`, `PickRespawnPawn`). ADR-0004 (Crowd `_Server` tuning sub-struct). ADR-0007 (module placement, composition-root registration). ADR-0008 (harness for the statistical, parity and cost gates) |
| **Enables** | ADR-0017 (NPC bush use through the one Hide path; Concealment implements the crowd perception filter, §5); ADR-0019 (informant takeover consumes `ITakeoverEligibilitySource` and `TryReleaseNpc`); ADR-0023 (telemetry: Tell Matrix calibration counters; the recorded virtual-input stream is this ADR's §4 virtual input layer captured by an ADR-0008 T3 harness run, not part of ADR-0023's export — amendment 2026-10-24) |
| **Blocks** | Crowd & NPC Behaviour epic (all MVP-core crowd stories); the Round disconnect hand-over and Halt stories that call `ICrowdControl`; the informant takeover story (eligibility supply) |
| **Ordering Note** | First in the Feature build order (architecture.md "Layer rationale"). The `CrowdSchedule` timer kind needs an ADR-0002 amendment before the ambient-cycle story starts; until then the cycle story is blocked, not worked around with a local timer. The cost rule is provisional until the 50-character spike reports |

## Context

### Problem Statement

Pillar 1 holds only if a player has no motion an NPC never shows and an NPC can do nothing a player could not. Unreal's standard AI stack breaks both: `MoveTo` and the path-following component write velocity through `RequestDirectMove` (E8), outside the input path a human's movement takes; `AAIController::UpdateControlRotation` throws away pitch unless the AI focuses a pawn (E1), so AI heads could never look down at goods the way a player's can; RVO avoidance rewrites velocity for whoever has it on; and the engine timer manager and `FMath::Rand` (both used by the prototype's `AStealthNpcController`) are neither ordered by the round's queue nor reproducible from a secret seed (ADR-0002, ADR-0006). At the same time the crowd must be cheap: about 50 AI-driven pawns on a listen host whose game-thread budget allocates 1.5 ms to Crowd AI and navigation (architecture.md CPU table), with no cut allowed that depends on distance, visibility or who is watching (CNB-R15). The decision has to be made before any crowd code is written, because the controller class, tick placement, random streams and the command boundary shape every behaviour.

### Constraints

- **Locked rules and identity invariants** (ADR-0001): one pawn class; AI through `UPawnCommandComponent` only; no AI-keyed LOD or budget; no NPC PlayerState (`bWantsPlayerState = false`).
- **Ordering and timers** (ADR-0002): interacting state changes resolve in the queue; AI discrete commands push to the server-local mailbox; AI `Move`/`Look` are continuous inputs, never items; gameplay timers are due-time-heap items, never the engine timer manager or a feature tick.
- **Randomness** (ADR-0006): every hidden draw from `FSecretStream`; no `FMath::Rand*`/`FRandomStream`; Crowd uses domain 13 with counter tuples.
- **Budget**: Crowd AI + nav ≤ 1.5 ms game thread steady on the listen host (Crowd F5); cuts are global values only; no cut lowers `NpcCount` without a creative-director review.
- **Navigation**: navmesh frozen for the round at `OnLayoutReady` (ADR-0014; CNB-R15); no runtime carving.
- **Team**: 1-2 programmers; the decision layer must be authorable and debuggable without bespoke tooling.
- **Engine**: UE 5.8.4, no engine patches; StateTree and GameplayStateTree plugins present [V architecture.md].

### Requirements

- Identity blindness: no crowd decision reads player-ness, team, binding (beyond driver selection), site assignment, `VictimKind` or objective progress (CNB-R2).
- Every NPC action enters through the IIF entry points under the same legality, slot arbitration, token buckets and debounce (CNB-R3, IIF R17).
- Motion parity: pace set, string-pulled paths with early turns, `Move(0)` at the stop distance, held intent re-applied every movement tick, avoidance expressed as `Move` offsets, and heading/glance statistics matched to logged human input through the same view envelope (CNB-R3.1-R3.4; OQ11).
- Perception ignores concealed pawns, and steering ignores bush-transitioning pawns, identically for every kind (CNB-R2, CON-R4).
- Counter-based, kill-independent, ordinal-keyed draws that survive takeover and release (CNB-R14).
- Ambient cycles, replacements and kill cancels on the F3 schedule, ordered against kills and takeovers (CNB-R9).
- Participant-bound driving (restricted driver), handover order with a control epoch, `ReleaseToCrowd`, takeover eligibility (CNB-R10, R11).
- Phase gating with per-pawn phase offsets; Halt inside Round's round-end step (CNB-R12, Round R9, ADR-0002 §8.4).
- Server cost ≤ 1.5 ms with named Insights scopes; a fixed global cut order (Crowd F5, AC-CNB-26).

## Decision

### 1. Components and placement

All crowd code is **server-only** and lives in `StealthGameplay/Features/Crowd/` (`Public/Server/` headers carry the ADR-0007 server-only guard). Nothing in this ADR replicates anything: observers see only the pawn state the command interface produces.

| Piece | Kind | Owns | Ticks |
|---|---|---|---|
| `UCrowdDirector` | `UWorldSubsystem` (server only; `ShouldCreateSubsystem` false on clients) facade over a plain C++ `FCrowdDirectorCore` (no `UWorld`, headless-testable, ADR-0007 testable-core rule) | population ledger (live, vacancies V, takeovers B, releases Rel), ambient schedule, per-ordinal draw counters, per-pawn activity mirror, 2 m spatial hash, line-of-sight ray budget, path-start budget, stimulus fan-out, Halt latch, eligibility reasons, Tell Matrix accepted-rate counters | one custom `FCrowdDirectorTickFunction`, game thread, `TG_PrePhysics`, registered on the authority in `OnWorldBeginPlay` (same pattern as ADR-0002's drain, because a world subsystem cannot choose its tick group) |
| `AStealthCrowdController` (class named by ADR-0010) | `AAIController` subclass; **one class** for every AI-driven pawn (NPCs, participant-bound pawns, `PawnLeavingGame` holds) | `bWantsPlayerState = false`, `bStartAILogicOnPossess = false` (engine defaults, asserted, E4); **no** `PathFollowingComponent` (`DoNotCreateDefaultSubobject`, E3); the `Look` control-rotation write (§4); the per-pawn behaviour due-time list (§3) | actor tick `TG_PrePhysics`; prerequisites: ADR-0010's hand-over pass → the Director's tick → the pawn's StateTree component → this controller; it is itself a prerequisite of the pawn's CMC (E2). In `Tick` the follower and due-time list are stepped **before** `Super::Tick`, so `UpdateControlRotation` (called by `Super::Tick`, E1) writes this frame's `Look` |
| `UCrowdStateTreeComponent` | `UStateTreeAIComponent` subclass on the controller, so it can call the protected `ScheduleTickFrame` and override the virtual `TickComponent` (E5, E12) | decisions: the CNB States table as StateTree states and tasks | component tick moved to `TG_PrePhysics` (default would be `TG_DuringPhysics`, after the CMC, E14) on the phase-aligned schedule of §3 |
| `FCrowdFollower` | plain C++ object held by the controller | path corners, carrot, lane offset, avoidance offset, progress monitor, virtual input layer (§4) | virtual camera, input quantisation and `Move`/`Look` emission **every controller tick**; carrot/path recompute at `CrowdSteerPerSec` with a per-pawn phase offset |
| `FCrowdCommandSink` | plain C++ wrapper per driven pawn | the drive token `{TWeakObjectPtr<AStealthPawn>, ControlEpoch}`, driver kind (Full / Restricted), Halt filter | — |

The Director implements the Core-owned consumer interfaces `ICrowdControl` and `ITakeoverEligibilitySource` (architecture.md "Consumer-owned interfaces"); the composition root (`AStealthGameMode`, ADR-0007) registers them weakly at world start. No lower layer names a Crowd type.

**Component parity (E13, engine gate blocking item).** `AAIController::OnPossess` would otherwise add a replicated `UGameplayTasksComponent` only to AI-driven pawns, a wire-visible difference that also survives a takeover. This ADR **requests an ADR-0010 amendment**: `AStealthPawn` creates a `UGameplayTasksComponent` as a default subobject on **every** pawn with replication off, so `OnPossess` finds one and adds nothing; the ADR-0008 leak suite adds a replicated-subobject inventory of an AI-driven, a player-driven and a just-handed-over pawn (verification (7)). Crowd uses no gameplay tasks.

**Possession is ADR-0010's.** Crowd never calls `Possess`/`UnPossess`. Every change of driver goes through `UStealthPawnFactory::RequestHandOver` committed in a queue slot (which bumps the factory-owned control epoch) and executes in ADR-0010's hand-over pass at the next frame start; the crowd controller then asks the Director for the pending driver configuration in `OnPossess` (§7).

### 2. One command boundary: the sink

Every crowd action leaves through `FCrowdCommandSink`, which calls the pawn's `UPawnCommandComponent` (ADR-0011) and nothing else:

- **Continuous:** `Move(FVector2D Direction, EGait)` and `Look(FRotator)`. The command component holds the last `Move` as an intent it re-applies every movement tick, as a held key is (CNB-R3.2; mechanism ADR-0011). AI `Move`/`Look` are never queue items (ADR-0002 §3).
- **Discrete:** `Traverse`, `InteractBegin/End`, `PrepareBegin/End`, `Hide(Enter/Exit)` enter the same component entry points, which push them into the **server-local mailbox** with a **null reply sink** (IIF R2, R5; ADR-0002 §3). `Attack` is **not** in the sink's vocabulary: the sink type has no `Attack` method, so an NPC strike cannot be written (T6, AC-CNB-11).
- **Restricted sink** (participant-bound pawns, CNB-R10.1): a separate type `FCrowdMotionSink` exposing only `Move` and `Look`; the restricted StateTree receives this type, so a bound pawn cannot issue any other command by construction. Outstanding holds of the human end through the IIF release path, not through the sink.
- **Control epoch** (CNB-R10.5): the sink carries the `ControlEpoch` it was created with and compares it with the pawn's current server-only epoch (`UStealthPawnFactory::GetControlEpoch(Pawn)`, ADR-0010 §4.4, bumped by `RequestHandOver` at commit inside the drain) **before every call**; a mismatch drops the call and counts `StaleEpochDrop`. This covers `Move`/`Look`, which are not items; discrete commands are additionally stamped at ingress and voided `StaleControl` at their slot by the IIF (ADR-0011). Async path callbacks, StateTree task completions and follower steps all go through the sink, so no old-driver command reaches the pawn.
- **Dry run** (CNB-R3): before a discrete command the StateTree task calls the IIF's side-effect-free legality predicate (`CanBegin`, ADR-0011) with 10 cm / 10° margins. A rejection that still happens (same-drain contention) abandons the behaviour; the target is not retried for 10 s.
- **Halt filter** (§7): after Halt the sink passes only `Move(0)` and `Look` (from the round-end step every discrete command is rejected `RoundEnded` anyway, ADR-0002 §8.4).
- **No other write.** Crowd code never calls `AddMovementInput`, `RequestDirectMove`, `SetActorRotation`, `SetControlRotation` (except §4's single override), `LaunchCharacter`, the CMC's velocity, or any `MoveTo`; `bUseRVOAvoidance` stays false on the one pawn class for **every** pawn (E8: RVO rewrites velocity outside the input path). A build grep enforces it (AC-CNB-02).

### 3. Decisions: one scheduled-tick StateTree per AI-driven pawn

- Two StateTree assets on one schema (`UStateTreeAIComponentSchema` subclass with `ScheduledTickPolicy = Allowed` and `AIControllerClass = AStealthCrowdController`, so behaviour never depends on the cvar default, E6; assets are recompiled whenever the schema changes, because the flag is cached on the compiled tree). The Director asserts at init that `StateTree.Component.ScheduledTickEnabled` is on (it would silently force every-frame ticks, E6) and the harness boot test checks it: `ST_CrowdFull` (the CNB States table: `Dormant`, `Emerging`, `Routing`, `Pausing`, `Conversing`, `Tending`, `Bracing`, `Hiding`, `Exiting`, `Reacting`, `Halted`) and `ST_CrowdRestricted` (`Routing`, `Pausing`, `Halted`, with C1, C2, C5, C6, G1-G3, G5, G7 only). Interrupt priority `Halted` > `Reacting` > the rest. Assets live under `Content/StealthGame/AI/` and are referenced by soft path from the Crowd tuning sub-struct through the registered keys `CrowdFullStateTreeAsset` and `CrowdRestrictedStateTreeAsset` (ADR-0004; user decision 2026-10-25), so their choice is data.
- **Cadence (phase-aligned grid):** each pawn has a decision grid `t = Phase + k / CrowdDecisionPerSec` (default 4 Hz), `Phase` drawn once from its stream (`ECrowdDecision::TickPhase`). Because the component overwrites its own schedule after every tree tick and events wake it with `MakeNextFrame` (E12), `UCrowdStateTreeComponent::TickComponent` calls `Super` and then **re-imposes** `ScheduleTickFrame(MakeCustomTickRate(NextGridPoint − Now))` unless the tree itself asked to sleep or to tick every frame; an event therefore costs one off-grid tick and the pawn returns to its grid (AC-CNB-33). Fallback if the override proves fragile: the Director calls `ScheduleTickFrame` per pawn on the grid and the tree only sleeps between calls. `CrowdDecisionPerSec` is one global value for every AI-driven pawn.
- **Behaviour clocks run per frame, not per decision.** Each controller keeps a small **due-time list** (pause end, glance-hold end, red-herring due, reaction start at `StimulusTime + Delay`, yield jitter end) checked in every controller tick against `ServerNow()`; an entry that falls due acts at once through the follower or sink and, when a decision is needed, wakes the tree (`ScheduleTickFrame(MakeNextFrame)`). Timing statistics are therefore independent of the decision rate (CNB-R6 G4, R8; AC-CNB-21), not quantised to 250 ms.
- **Event interrupts** (`SendStateTreeEvent`, public, E5): `Crowd.Stimulus.Strike`, `Crowd.Stimulus.Salient`, `Crowd.Path.Blocked`, `Crowd.Path.Failed`, `Crowd.Converse.Faced`. **Halt and handover never rely on events alone**, because an event to a stopped tree is dropped (E15): they are direct calls (§7) that latch the sink first and then stop or switch the tree.
- **Tasks are thin.** StateTree tasks choose (destination, pause length, glance target, red-herring due, bush, court) and hand intents to the follower or the sink; they never move the pawn. No StateTree `MoveTo` task (E9), no `AITask_MoveTo`, no Behavior Tree.
- **Activity mirror.** Each state's enter/exit task writes the pawn's `ECrowdActivity` into the Director's ledger; eligibility (§8) reads authoritative sources first (IIF action slot for Tending/Bracing, Concealment for Hiding, the ledger for Emerging/Exiting/AlleyExit/Bound) and the mirror only for `Reacting` and `Conversing`.
- **Stationary-stretch watchdog** (CNB-R5, R10.2): the Director tracks each driven pawn's current visible stationary stretch from movement state and forces a `Routing` leg at `CrowdMaxIdleSec`; while the pawn is concealed the stretch clock is suspended.

### 4. Motion: the follower and the virtual input layer (OQ11 decided: input emulation)

The follower turns a path into exactly the inputs a human produces:

1. **Path source.** Pre-baked `CrowdRoute` edge paths from the Manifest when the leg follows an edge (no query); otherwise `FindPathAsync` (E7) through the Director's **path-start budget** (≤ `CrowdPathStartsPerFrameLimit`, default 2, starts per frame, FIFO with round-robin deferral; a hitch defers, never drops). Results arrive on the game thread **at least two frames** after the request (E7: the navigation tick runs before `TG_PrePhysics`), during which the follower keeps its current `Move`; they are accepted only through the sink's epoch check and the request's leg id. The navmesh is frozen for the round (ADR-0014); a path the navigation system reports invalid is re-queried, holding the last `Move` ≤ 0.5 s, then `Move(0)`.
2. **Shaping** (CNB-R3.2-R3.3): string-pulled corners (E11; the follower funnels them itself if the query does not) → per-pawn lane offset ±0.5 m → pure-pursuit carrot at `max(150 cm, v · 1 s)` → avoidance offset → early turn to fit the SCL F3 arc → `Move(0)` at the SCL F2 stop distance. Avoidance reads neighbours only from the Director's 2 m spatial hash, built once per frame from public pawn positions and velocities **excluding** every pawn for which the perception filter (§5) returns "not an obstacle". The yield decision is drawn once when a predicted approach is first detected (`ECrowdDecision::Yield`), with the 0.2-0.6 s jitter as a sampled timer. Progress monitor and repath rules exactly as CNB-R3.3. No pawn is ever pushed.
3. **Virtual input layer** (CNB-R3.4). The follower does not emit the world direction it wants. It maintains a per-pawn **virtual camera** (yaw, pitch) and a **device profile** drawn once per pawn from its stream (`ECrowdDecision::DeviceProfile`) out of the measured input-device mix: *keyboard and mouse* (8-way quantised move relative to the virtual camera yaw, mouse-like yaw changes) or *gamepad* (analog stick magnitude and angle with the profile's dead zone and response curve). **Every controller tick** (not at the steering rate, because human `Look` changes every frame and a 30 Hz look stream would itself be a tell) it (a) moves the virtual camera toward the gaze or travel target with push durations, frequencies and turn-rate distributions drawn from the profile, passed through Camera's exported pure function `EvaluateViewEnvelope` (Camera F1, CS-R3), so edge pushes, latches and relaxes are a human's; (b) expresses the desired travel direction as the input a human with that camera would press (an 8-way key combination or a stick vector, quantised as the device would), and converts it to the world 2D `Move` direction exactly as the human client builder does; (c) emits `Look(VirtualCameraRotation)`. A walking glance therefore bends the path as a human's does. Pace magnitude is from the shared set {0.8, 0.9, 1.0}, chosen at leg start or after a stop (interim rule: 1.0 only, CNB-R3.1).
4. **The pitch path** (CNB-R3, SCL R8.1): `AStealthCrowdController::UpdateControlRotation(DeltaTime, bUpdatePawn)` is overridden to `SetControlRotation(LastLook)` — yaw **and pitch** from the most recent `Look` — and never calls `FaceRotation` and never consults a focal point (E1). The engine focus system (`SetFocus`/`SetFocalPoint`) is never used by Crowd. Because the controller tick precedes the CMC and the pawn tick (E2), the pawn's per-tick `HeadLook` filter (SCL R8.3) consumes this frame's control rotation, exactly as it consumes a human's saved-move control rotation; `RemoteViewPitch16` is then written from `HeadLook` for every pawn (ADR-0003 §3).
5. **Profiles are data.** Device mix, per-device quantisation, view-envelope push statistics, turn-rate and glance-hold distributions live in a ServerOnly `UCrowdMotionProfileData` asset referenced from the Crowd tuning sub-struct by the registered key `CrowdMotionProfileAsset` (user decision 2026-10-25) (ADR-0004 pipeline: validated, frozen, hashed per round). Until logged human telemetry exists they hold provisional profiles from internal play (CNB OQ12); replacing them never changes code.
6. **Recordable stream.** The virtual input layer's per-step output `FCrowdVirtualInputFrame {ServerTime, CameraYaw, CameraPitch, DeviceInput, Gait}` can be recorded (non-Shipping) and replayed through a bot client's Enhanced Input path by the ADR-0008 harness; that is how AC-CNB-27 isolates pipeline differences.

*Alternative rejected:* an SCL turn-smoothing filter applied to every pawn (§ Alternatives 3).

### 5. Perception: public facts only, concealment-blind

- Perception inputs are only what every client could see: position, velocity, `ActionPose`, `TraversalState`, body facing, plus server line of sight from the NPC's eye socket. No `UAIPerceptionComponent` (its sense configs and team affiliation are a second, kind-aware perception model); line-of-sight checks are `LineTraceSingleByChannel` on a crowd sight channel from the Director's **ray budget** (≤ 8 per frame, round-robin, deferred rays keep their stimulus time so reaction delays are unaffected).
- **Concealment filter.** Crowd defines the consumer-owned interface `ICrowdPerceptionFilter { bool IsPerceivable(const APawn*); bool IsSteeringObstacle(const APawn*); }`, implemented by Concealment (ADR-0017) and registered by the composition root. `IsPerceivable` is false for every pawn in `Concealed`; `IsSteeringObstacle` is false for every pawn in `Entering`, `Exiting`, `Emerging` or `Concealed`; both are identical for every pawn kind and take no viewer. Bush `HideVolume`s are static steering obstacles read from the Manifest. If Concealment is absent (soft dependency), the filter defaults to "everything perceivable" and bush red herrings are disabled (CNB Dependencies).
- **Glance targets, conversation partners, avoidance obstacles and reaction sources** all come from the spatial hash filtered this way (AC-CNB-36).
- **Stimuli** arrive from the IIF action hub `OnActionStarted(Pawn, Action)` (server-only delegate, ADR-0011); strike stimuli come **only** from the outcome-blind `OnActionStarted(Attack)`, never from `OnPawnKilled` (amendment 2026-10-24, ADR-0018 §3.4); the Director fans a strike out to at most `CrowdStrikeResponderLimit` unbound NPCs within `CrowdStrikeReactRadiusCm` with line of sight, applying the refractory and chain-gap rules. The stimulus never carries an outcome or `VictimKind` that the Director reads.

### 6. Population, emergence and the ambient schedule as queue timer items

- **Ledger.** The Director's ledger holds `NpcCount − V − B + Rel` and the per-socket schedule; it reads only kill times, never who died (CNB-R1, R9.3).
- **Schedule events are queue items.** Ambient exits, ambient entries, replacements, retries and the exile-exit slot are interacting state changes (they spawn and absorb pawns, draw CPA cards and change takeover supply), so they are scheduled in ADR-0002's due-time heap as `TimerFired(CrowdSchedule)` items with the round's `FEpochToken`, and resolved by the Director's registered timer resolver inside the drain. **This needs an ADR-0002 amendment** adding `CrowdSchedule` to the closed `ETimerKind` set (an internal enum; no replicated schema changes). Inside the slot the resolver runs the LSC clear check, calls `RequestEmergence(...)` and the pawn factory (`SpawnSharedPawn(..., Ai, ...)`, with `DrawReplacement()` in the same tick), or absorbs the pawn (`RequestEmergence(AmbientExit)` + `RequestTeardown`, ADR-0002 §7). A failed clear check reschedules the item at +`EmergenceRetrySec`; an item that resolves `Rejected_Latched` or `Rejected_StaleEpoch` is never rescheduled. ADR-0002 has no cancel for a scheduled item, so a **cancelled** exit (kill cancel, exile taking a slot) is recorded in the Director ledger and its item resolves `Voided(Revalidation)` when the resolver re-checks the ledger at the slot.
- **Kills.** `OnPawnKilled` runs inline in the Attack slot (ADR-0002 §5) with `KillServerTime` = `GetScheduledDeathServerTime(Victim)` (the strike's `ContactServerTime`, ADR-0018 §3.4); t_k below is that time. No `OnPawnKilled` effect may be observable before the death write (amendment 2026-10-24, ADR-0018): an NPC victim keeps its driver, follower and stimulus handling unchanged until the death write's freeze flag, and the kill handling may change only exits scheduled at or after t_k. The Director adds the vacancy at t_k, cancels one upcoming exit chosen uniformly among those due in [t_k, t_k + 60 s] (`AmbientSchedule` draw), and binds the replacement to the first scheduled entry at or after t_k + `CrowdReplacementMinDelaySec` (F3), with the `CrowdReplacementMaxDelaySec` fallback.
- **Schedule draws** use ADR-0006's counter-as-Attempt shape on `AmbientSchedule` (domain 9): one `GetServerSubSeed(AmbientSchedule, counter)` per schedule event.
- **AI behaviour clocks are input generation, not gameplay timers.** Pause lengths, gaze holds, red-herring clocks and reaction delays are the AI's private decision timing — the analogue of a human deciding when to press a key — evaluated in the controller's per-frame due-time list (§3) against `ServerNow()` (elapsed server time, never frame counts, never the engine timer manager). They mutate no gameplay state: their only outputs are `Move`/`Look` inputs and commands that enter the IIF and are ordered by the queue like any human input. Holds the AI begins (`Interact`, `Prepare`) end through the IIF's hold-timer items exactly as a human's do. Because ADR-0002 §4's guideline literally bans "a feature tick" for gameplay timers, the `CrowdSchedule` amendment request also asks ADR-0002 to record this definition (AI decision timing is input generation whose outputs enter the command path).

### 7. Phases, Halt and handover

- **Dormant until Active.** Every crowd pawn's StateTree is created stopped. At `OnRoundActive` (inside Round's Active-entry step) the Director starts every AI-driven pawn in that step, each from its own drawn first phase (pause or leg) and decision-tick phase offset (CNB-R12, AC-CNB-33). If the navmesh is not ready the Director asserts `NavNotReady` and keeps them Dormant (development failure only).
- **Latch to round-end step.** Between the latch and the round-end step the crowd keeps running unchanged; its discrete commands resolve `Rejected_Latched` silently (null sink), locomotion continues (ADR-0002 §8.1).
- **Halt** (`ICrowdControl::Halt()`, called by Round inside the round-end step, ADR-0002 §8.4 (f)): synchronously, in that call, the Director (1) sets the **Halt latch** on every sink (only `Move(0)` and `Look` pass from then on, so no due-time entry, StateTree or follower tick later in the same frame can re-issue motion); (2) clears every AI-driven pawn's held `Move` intent (`Move(0)`); (3) stops every tree directly (terminal `Halted` behaviour: no further decisions; the per-frame look keeps the head idle) — not by an event alone (E15); (4) stops the ambient schedule (its `TimerFired(CrowdSchedule)` items are already `Rejected_Latched`), replacements and reactions, abandoning open cycles without emergence. Halt does **not** end holds and does **not** brake pawns: frozen holds are cancelled by Round's step (g) for every pawn kind, and braking/stop-and-blend for every live pawn is ADR-0010's, called by Round in the same step — so no hold-end or stop path exists that only AI pawns take. A pawn released during `Ended` stays halted.
- **Handover order** (CNB-R10.5, one rule for R10 and R11): Round's (or Penalty's) slot calls `UStealthPawnFactory::RequestHandOver`, which bumps the pawn's control epoch at commit (ADR-0010 §4.3-4.4); every crowd sink holding the old epoch is dead from that instant. ADR-0010's hand-over pass possesses at the start of the next server frame, never during a traversal; it runs before the Director's tick (prerequisite), so the new controller is driven in the same frame. Until the new driver's first command or `HandoverHoldSec`, ADR-0010 holds the last `Move`.
- **`ICrowdControl` semantics** (the caller states the driver, so Crowd reads no Roster binding; none of these possesses anything):
  - `DriveBoundPawn(Pawn)` — Round, on Unresponsive/Disconnected, in the same slot as its `RequestHandOver(Pawn, Ai)`: registers a pending **restricted** driver for that pawn. When the hand-over pass possesses it with `AStealthCrowdController` (or the controller already holds it after `PawnLeavingGame`, ADR-0010 §5), `OnPossess` picks up the pending config: `Move(current heading, Walk)` at the nearest pace for `HandoverHoldSec` (or `Move(0)` below the pivot speed or when the hold heading leaves the navmesh within the hold distance), deceleration never above `CrowdStopDecel`, then `ST_CrowdRestricted` with `FCrowdMotionSink`. A concealed pawn stays concealed; no `Hide(Exit)` (Round R17; CNB OQ4).
  - `ReleaseControl(Pawn)` — Round, on rejoin/responsive, in the same slot as its `RequestHandOver(Pawn, Player)`: latches the pawn's sink, clears the held `Move`, stops the tree; the hand-over pass detaches at the next frame start (never mid-traversal); the human's first command shapes from current velocity.
  - `AdoptReleasedPawn(Pawn, const FCrowdAdoptParams&)` — Shared Pawn hand-over on `ReleaseToCrowd`: `FCrowdAdoptParams { double RestrictedUntilServerTime; bool bExitAtNextScheduledSlot; }`. The pawn joins `ST_CrowdFull` at its next leg boundary (nothing visible changes at the release instant), or first finishes its strike lockout under `ST_CrowdRestricted` and then takes the **next scheduled city-wide exit slot** (Penalty CR-4; the slot's original NPC simply stays), reporting `AlleyExit` until absorbed. A concealed released pawn's remaining stay is drawn from the NPC stay distribution conditioned on the time already concealed (uncapped).
  - `Halt()` — above.
- This makes CNB-R2's "one permitted read" of the Roster binding unnecessary: the driver kind arrives as a call parameter, so the Crowd module includes **no** Roster or Objectives query header (AC-CNB-01 tightens to zero call sites; GDD sync below).

### 8. Takeover supply (`ITakeoverEligibilitySource`)

- `ETakeoverEligibility GetTakeoverEligibility(const AStealthPawn*) const` returns one of `Eligible`, `Tending`, `Bracing`, `Hiding`, `Emerging`, `Exiting`, `Reacting`, `Conversing`, `Bound`, `AlleyExit` (CNB-R11), computed from authoritative sources at call time (§3 activity mirror rule); it never reads appearance, team or who is watching.
- `bool TryReleaseNpc(AStealthPawn*, FEpochToken)` is called **only by Penalty**, inside its commit slot, on the pawn `PickRespawnPawn` returned (amendment 2026-10-24, ADR-0019 §4: the Layout Service picks, it never calls Crowd): it re-checks `Eligible` atomically, latches the pawn's sink, stops the StateTree (Penalty's same-slot `TakeOverCrowdPawn`/`RequestHandOver` bumps the control epoch, ADR-0010), removes the pawn from the ledger (B += 1) and leaves the pawn moving as it was for the SCL AI → player handover. On failure Penalty re-picks per ADR-0019 (exclusion set, next-frame retry).
- **Reversal and idempotence** (amendment 2026-10-24, ADR-0019): `AdoptReleasedPawn(Pawn, {})` on a pawn the Director released by `TryReleaseNpc` and that was never bound exactly reverses it (B −= 1, sink unlatched, tree restarted from its current activity); the Director tells this case from a `ReleaseToCrowd` adoption by its own released-for-takeover set, never by a Roster read. `ReleaseControl` on an already-released pawn is a no-op. `DriveBoundPawn` applies to every participant-bound pawn whose participant goes `Unresponsive`, informant-bound pawns included (Round R17.1).

### 9. Random streams (CNB-R14)

- `FCrowdDraws` wraps one `FSecretStream(GetServerSubSeed(CrowdBehaviour, LayoutAttempt), StealthRandom::Tag("CNB_BHVR"))` per round and exposes `Draw(Ordinal, ECrowdDecision)`, which takes the next `DecisionIndex` for that `(Ordinal, Decision)` and returns `FSecretStream::Draw({Ordinal, uint64(Decision), DecisionIndex, Retry = 0})` (ADR-0006 §5, mode 0x02). Ranged and weighted draws (`DrawBelow(Ordinal, Decision, N)`, `DrawWeighted(...)`) apply ADR-0006's Lemire rejection to this random-access form: each rejection retry reads the next tuple `{Ordinal, Decision, Index, Retry + 1}` with the **same** `DecisionIndex`, so retries never shift later decisions. `Ordinal` is the server-only spawn ordinal the pawn factory assigns to **every** pawn, participants included (ADR-0010); `DecisionIndex` counters live in the Director keyed by ordinal (never on the controller, which is destroyed on possession), so streams follow a pawn through takeover and release and a kill shifts no other pawn's draws (AC-CNB-29, AC-CNB-34).
- `ECrowdDecision : uint16` is append-only with fixed values (`TickPhase`, `DeviceProfile`, `LaneOffset`, `Destination`, `PauseKind`, `PauseLength`, `Pace`, `Yield`, `YieldJitter`, `GlanceTarget`, `GlanceHold`, `GazeScan`, `ConverseStart`, `ConverseReply`, `ConverseLength`, `ReactionKind`, `ReactionDelay`, `RunClock`, `VaultClock`, `ClimbClock`, `BraceClock`, `DecoyClock`, `DecoyCourt`, `DecoyRelease`, `BushClock`, `BushStay`, `ReversalClock`, `HeadHoldClock`, `ExitPick`, …).
- **Continuous distributions** (exponential clocks with F2's compensated rates, log-normal bush stays, uniform durations): the 64-bit draw is mapped to a unit fraction `u = ((Bits >> 11) + 0.5) · 2^-53` (never 0 or 1) and through a closed-form inverse CDF in `double` — exponential `−ln(1 − u)/λ`, uniform `a + u(b − a)`, log-normal `exp(μ + σ·Φ⁻¹(u))` with Φ⁻¹ by **Wichura's AS241 (PPND16)** — server-side only. Golden vectors for `u` and AS241 are committed fixtures (ADR-0006 rule 7). Results are reproducible bit-for-bit for one build and seed (AC-CNB-34); cross-platform float identity is not required because no client recomputes them. ADR-0006 rule 5 covers integer ranged draws only, so this ADR **requests an ADR-0006 note** recording this mapping as the sanctioned continuous form, applied only **after** a secret integer draw and never feeding a public stream.
- **Start placement** (CNB-R1, R13) uses `NpcRoutes` (domain 5, layout attempt) inside LSC `RequestStartPlacement`; per-pawn behaviour after spawn uses `CrowdBehaviour` only.
- Rates are constant per round: they are read from the frozen tuning snapshot after `OnTuningReady` and never adapted at runtime (AC-CNB-37).

### 10. Cost rule and budget (provisional pending the 60-character spike)

- Budget: the six named scopes `Crowd.Director`, `Crowd.StateTree`, `Crowd.Follower`, `Crowd.Perception`, `Crowd.NavQuery`, `Crowd.Dispatch` sum to ≤ 1.5 ms game thread per frame at p99 and ≤ 0.8 ms mean on the listen host (AC-CNB-26). The server movement simulation of AI-moved pawns is SCL's CMC line (3.0 ms) and is reported alongside, not counted here. Path work on the task graph (E7) is worker time.
- **Fixed global cut order** (each one value for every AI-driven pawn; never by distance, visibility, viewer or kind; never touching replication settings; never lowering `NpcCount`): steering (carrot/path recompute) 30 → 20 Hz; neighbour candidate cap; decisions 4 → 3 Hz; coarse waypoint A* on the route graph instead of navmesh queries; defer stimulus rays. Timing statistics do not depend on the decision rate (per-frame due-time list) and `Move`/`Look` are emitted every controller tick at every step, but invariance of the F6 motion features under each cut is **tested, not assumed**: AC-CNB-21 and AC-CNB-27 run at the default and at each cut-order step before that step may ship.
- **Load envelope.** F5 allows N ≤ 70 AI-driven pawns (60 NPCs + 10 bound); at 70 pawns on a 30 fps host F5 is already over budget, so the cut order is expected to engage there. The virtual input layer's per-tick cost (including `EvaluateViewEnvelope`) is not in F5's `c_f` term; both the 70-pawn/30 fps run and that cost are added to the spike (verification (8)).
- **Host hitch:** follower and timers use elapsed server time; path starts and rays stay capped per frame with round-robin deferral; no pawn is deferred by type or visibility.

### Architecture Diagram

```
 SERVER FRAME (listen host)
 ┌─ NetDriver dispatch: human Move/Look (ServerMove), human discrete RPCs ─► mailboxes ─────────────────┐
 ├─ Navigation system tick (before TG_PrePhysics): trigger last frame's queued queries; dispatch the ──┤
 │     previous batch's results (game thread) ─► leg-id + epoch check   (request → result ≥ 2 frames)  │
 ├─ TG_PrePhysics ─────────────────────────────────────────────────────────────────────────────────────┤
 │  ADR-0010 FPawnHandoverTickFunction: possess/unpossess committed hand-overs (AStealthCrowdController │
 │     OnPossess picks up the Director's pending driver config)                                         │
 │        │ prerequisite                                                                                │
 │  FCrowdDirectorTickFunction: spatial hash (public pos/vel, filtered by ICrowdPerceptionFilter),      │
 │     stimulus fan-out, ray budget (≤8), path-start budget (≤2 FindPathAsync), watchdog                 │
 │        │ prerequisite                                                                                │
 │  UCrowdStateTreeComponent (×N, TG_PrePhysics, phase-aligned ~4 Hz grid re-imposed in TickComponent) │
 │     ─► intents (destination, pause, glance, red herring, bush, court)                                │
 │        │ prerequisite                                                                                │
 │  AStealthCrowdController::Tick (×N): due-time list (per frame) ─► FCrowdFollower:                    │
 │     [CrowdSteerPerSec, phase-offset] path → carrot → avoidance                                       │
 │     [every tick] virtual camera (EvaluateViewEnvelope) → device-quantised input → world Move, Look   │
 │     then Super::Tick ─► UpdateControlRotation override: SetControlRotation(LastLook) (yaw+pitch)     │
 │                                      FCrowdCommandSink (factory epoch check, Halt filter, no Attack)  │
 │                                        ├─ Move/Look ─► UPawnCommandComponent ─► held intent ─► CMC   │
 │                                        └─ Traverse/Interact/Prepare/Hide ─► server-local mailbox     │
 │        │ prerequisite (E2)                                                                           │
 │  CMC + pawn tick (every pawn): movement, HeadLook filter (SCL R8.3)                                   │
 ├─ TG_PostPhysics: ADR-0002 drain ───────────────────────────────────────────────────────────────────┤
 │  AI discrete items resolved like human ones (null sink; stale control epoch ⇒ Voided)               │
 │  TimerFired(CrowdSchedule) ─► Director resolver: ledger re-check (cancelled ⇒ Voided), clear check, │
 │     RequestEmergence, SpawnSharedPawn + DrawReplacement / absorb + RequestTeardown                   │
 │  OnPawnKilled (inline in Attack slot) ─► vacancy + exit cancel;  OnActionStarted ─► stimuli          │
 │  Round/Penalty: RequestHandOver (epoch++) + ICrowdControl::DriveBoundPawn/ReleaseControl;           │
 │     Penalty ─► TryReleaseNpc                                                                        │
 │  ROUND-END STEP ─► ICrowdControl::Halt(): sink latch, clear held Move, stop trees, schedule stopped; │
 │     Round (g) cancels holds and ADR-0010 stop-and-blend brakes every pawn                            │
 └─ replication: pawn state only (crowd replicates nothing of its own) ────────────────────────────────┘
```

### Key Interfaces

```cpp
// --- StealthGameplay/Core (consumer-owned; Public/Server) -------------------------------------
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UCrowdControl : public UInterface { GENERATED_BODY() };
class ICrowdControl
{   GENERATED_BODY()
public:
    virtual void Halt() = 0;              // Round, inside the round-end step: latch, clear Move, stop trees
    // The following never possess: the caller also calls UStealthPawnFactory::RequestHandOver in
    // the same slot (ADR-0010); these only register the driver the hand-over pass will attach.
    virtual void DriveBoundPawn(AStealthPawn* Pawn) = 0;                  // Round: restricted driver
    virtual void ReleaseControl(AStealthPawn* Pawn) = 0;                  // Round: hand back to the human
    virtual void AdoptReleasedPawn(AStealthPawn* Pawn, const FCrowdAdoptParams& Params) = 0; // ReleaseToCrowd
};
USTRUCT() struct FCrowdAdoptParams
{   GENERATED_BODY()
    double RestrictedUntilServerTime = -1.0;    // strike lockout end; -1 = none
    bool   bExitAtNextScheduledSlot = false;    // exile body (Penalty CR-4)
};
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UTakeoverEligibilitySource : public UInterface { GENERATED_BODY() };
class ITakeoverEligibilitySource
{   GENERATED_BODY()
public:
    virtual ETakeoverEligibility GetTakeoverEligibility(const AStealthPawn* Pawn) const = 0;
    virtual bool TryReleaseNpc(AStealthPawn* Pawn, FEpochToken Expected) = 0;   // inside the commit slot
};
enum class ETakeoverEligibility : uint8 { Eligible, Tending, Bracing, Hiding, Emerging, Exiting,
                                          Reacting, Conversing, Bound, AlleyExit };

// --- StealthGameplay/Features/Crowd (Public/Server) -----------------------------------------
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UCrowdPerceptionFilter : public UInterface { GENERATED_BODY() };
class ICrowdPerceptionFilter                     // implemented by Concealment (ADR-0017)
{   GENERATED_BODY()
public:
    virtual bool IsPerceivable(const APawn* Pawn) const = 0;       // false while Concealed, any kind
    virtual bool IsSteeringObstacle(const APawn* Pawn) const = 0;  // false while Entering/Exiting/Emerging/Concealed
};

UCLASS() class UCrowdDirector : public UWorldSubsystem,
                                public ICrowdControl, public ITakeoverEligibilitySource
{   GENERATED_BODY()
    // facade over FCrowdDirectorCore (plain C++, headless specs); server only.
    // Owns FCrowdDirectorTickFunction (TG_PrePhysics, authority, registered in OnWorldBeginPlay).
    // Registers the TimerFired(CrowdSchedule) resolver with UResolutionQueue (ADR-0002 amendment).
public:
    void SetPerceptionFilter(TWeakInterfacePtr<ICrowdPerceptionFilter> Filter);  // composition root
    void OnRoundActive();                                   // start every AI-driven pawn (phase offsets)
    void OnPawnKilled(AStealthPawn* Victim, double KillServerTime);   // inline in the Attack slot
    void ScheduleExileExit(AStealthPawn* Pawn);             // takes the next scheduled exit slot
};

UCLASS() class AStealthCrowdController : public AAIController
{   GENERATED_BODY()
public:
    AStealthCrowdController(const FObjectInitializer& OI);        // DoNotCreateDefaultSubobject("PathFollowingComponent");
                                                             // bWantsPlayerState=false; bStartAILogicOnPossess=false
    virtual void Tick(float DeltaTime) override;           // due-time list + follower step, THEN Super::Tick
    virtual void OnPossess(APawn* InPawn) override;          // Super, then Director::AttachDriver(pending config)
    virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;
                                                             // SetControlRotation(LastLook): yaw + pitch;
                                                             // no focal point, no FaceRotation
};
UCLASS() class UCrowdStateTreeComponent : public UStateTreeAIComponent
{   GENERATED_BODY()   // TG_PrePhysics; Director tick is a prerequisite; controller depends on it
public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;
                       // Super, then re-impose ScheduleTickFrame(MakeCustomTickRate(NextGridPoint - Now))
                       // unless the tree asked to sleep or tick every frame (E12)
};
// AStealthPawn (ADR-0010 amendment requested): UGameplayTasksComponent default subobject on every
// pawn, SetIsReplicated(false), so AAIController::OnPossess adds nothing kind-specific (E13).

// Sinks (plain C++): the only way crowd code reaches a pawn.
class FCrowdMotionSink                                       // restricted driver: Move/Look only
{ public: bool Move(FVector2D Dir, EGait Gait); bool Look(FRotator Rot);
  protected: TWeakObjectPtr<AStealthPawn> Pawn; uint32 ControlEpoch; bool bHalted; };
class FCrowdCommandSink : public FCrowdMotionSink            // full driver: + discrete, never Attack
{ public: bool Traverse(const FTraversalHint&); bool InteractBegin(FCourtFixtureId); bool InteractEnd();
          bool PrepareBegin(); bool PrepareEnd(); bool Hide(const FHideCommand&); };
// Every call: drop if Pawn stale, if UStealthPawnFactory::GetControlEpoch(Pawn) != ControlEpoch
// (ADR-0010 §4.4), or if bHalted and the call is not Move(0) / Look.

struct FCrowdVirtualInputFrame { double ServerTime; float CameraYaw; float CameraPitch;
                                 FVector2D DeviceInput; EGait Gait; };   // recordable (non-Shipping)

enum class ECrowdDecision : uint16 { TickPhase = 1, DeviceProfile = 2, LaneOffset = 3, Destination = 4,
    /* ... append-only, fixed values, never renumbered ... */ };
class FCrowdDraws   // FSecretStream(GetServerSubSeed(CrowdBehaviour, LayoutAttempt), Tag("CNB_BHVR"))
{ public: uint64 Draw(uint32 Ordinal, ECrowdDecision Decision);       // tuple {Ordinal, Decision, Index++, Retry=0}
          uint32 DrawBelow(uint32 Ordinal, ECrowdDecision Decision, uint32 N);   // Lemire; retries use Retry+1
          int32  DrawWeighted(uint32 Ordinal, ECrowdDecision Decision, TConstArrayView<uint32> CumWeights);
          static double UnitFraction53(uint64 Bits);                  // ((Bits>>11)+0.5)*2^-53, server only
          static double InvNormalAS241(double U); };                  // Wichura PPND16; golden vectors
```

### Implementation Guidelines

- Crowd code must reach a pawn only through `FCrowdCommandSink` / `FCrowdMotionSink`, which call only `UPawnCommandComponent` entry points; it must never call `MoveTo*`, `AITask_MoveTo`, a StateTree MoveTo task, `RequestDirectMove`, `AddMovementInput`, `SetActorRotation`, `LaunchCharacter` or write CMC velocity.
- `AStealthCrowdController` must not create a `PathFollowingComponent` (fallback if verification (2) fails: create it, never tick it, never request a move; the AC-CNB-02 tick counter must read 0).
- `bUseRVOAvoidance` must be false for every pawn; avoidance must be expressed as `Move` offsets.
- The crowd AIController must write `Look` yaw and pitch into the control rotation in its `UpdateControlRotation` override and must never use `SetFocus`/`SetFocalPoint` or `FaceRotation`.
- The NPC sink must have no `Attack` method; the restricted sink must expose only `Move` and `Look`.
- Every sink call must check the pawn's current control epoch; async path results must also match the request's leg id.
- StateTree assets must use the crowd schema with `ScheduledTickPolicy = Allowed` and be recompiled after any schema change; the Director must assert `StateTree.Component.ScheduledTickEnabled` at init; the StateTree component must tick in `TG_PrePhysics` after the Director and before the controller; decision cadence must be one global `CrowdDecisionPerSec` on a per-pawn phase-aligned grid re-imposed after every tree tick; reaction and behaviour delays must live in the controller's per-frame due-time list, never in decision-tick counts or the engine timer manager.
- The controller must step its due-time list and follower before `Super::Tick`; the virtual camera, input quantisation and `Move`/`Look` emission must run every controller tick.
- Crowd code must never call `Possess`/`UnPossess`; driver changes must go through `UStealthPawnFactory::RequestHandOver` and ADR-0010's hand-over pass, with `ICrowdControl` only registering the driver.
- Halt and handover must be direct calls that latch the sink and stop or switch the tree; they must never rely on a StateTree event alone. Halt must not end holds or brake pawns (Round and ADR-0010 do that for every pawn).
- Every `AStealthPawn` must carry a non-replicated default `UGameplayTasksComponent` (ADR-0010 amendment) so AI possession adds no component.
- Perception must use only public facts and server line of sight; it must exclude pawns for which `ICrowdPerceptionFilter` says not perceivable / not an obstacle, identically for every kind; `UAIPerceptionComponent` and team affiliation must not be used.
- Crowd code must never include Roster, Objectives-assignment or Visibility Policy fact headers; the driver kind must come from the `ICrowdControl` call.
- Ambient schedule events must be `TimerFired(CrowdSchedule)` queue items resolved in the drain; emergence spawns must go through `RequestEmergence` and `SpawnSharedPawn`; absorptions through `RequestTeardown`.
- Every behavioural draw must be `FCrowdDraws::Draw`/`DrawBelow`/`DrawWeighted(Ordinal, Decision, ...)` on domain `CrowdBehaviour` (rejection retries extend the tuple, never the index); continuous samples must use `UnitFraction53` and the named inverse CDFs (AS241 for the normal) with golden vectors; schedule draws must use `AmbientSchedule` counter-as-Attempt; start placement must use `NpcRoutes`; `FMath::Rand*` and `FRandomStream` must never appear in Crowd code; decision counters must be keyed by ordinal in the Director.
- Rates and profiles must come from the frozen tuning snapshot and `UCrowdMotionProfileData`; nothing may adapt them during a round.
- Halt must latch every sink synchronously inside the round-end step before any further crowd tick can run.
- A cancelled schedule item must resolve `Voided(Revalidation)` by ledger re-check; `Rejected_Latched`/`Rejected_StaleEpoch` items must never be rescheduled.
- Cost cuts must be global values from the fixed cut order; they must never depend on distance, visibility, viewer or kind, never change replication settings and never lower `NpcCount`.
- Crowd diagnostics must use `UE_LOGF` with no participant, team or binding data; Event Log entries must never go through `UE_LOG`/`UE_LOGF` (ADR-0007).
- No crowd UI, debug draw or indicator may exist in Shipping (AC-CNB-25).

## Alternatives Considered

### Alternative 1: Mass Entity / Mass Crowd (or Detour Crowd) for the crowd
- **Description**: Simulate citizens as Mass entities (or `UCrowdFollowingComponent` with Detour Crowd) and represent visible ones as actors.
- **Pros**: Scales to thousands; built-in crowd avoidance and LOD.
- **Cons**: Mass is Experimental in 5.8 (E10) and models NPCs as a different kind of thing from the 10 `ACharacter` players — a second representation, a second movement path and LOD tied to the AI kind, all forbidden by ADR-0001. Detour Crowd drives velocity through path following (E8), outside the command interface; its avoidance would be NPC-only.
- **Rejection Reason**: Breaks the one-pawn, one-path invariant for a scale (≈ 50 pawns) that does not need it.

### Alternative 2: Behavior Tree (or StateTree) with stock `MoveTo` and path following
- **Description**: The standard UE AI stack: BT/StateTree selects goals; `MoveTo` and `UPathFollowingComponent` move the pawn.
- **Pros**: Least code; mature tooling; the prototype already does it.
- **Cons**: Path following writes velocity via `RequestDirectMove` (E8), so NPC motion bypasses the `Move` entry point and its pace set, shaping, token buckets and commanded-displacement accounting (IIF R17, SCL R16); `MoveTo` stops at corners and accelerates differently from a human (AC-CNB-04); `UpdateControlRotation` drops pitch (E1). Every one of these is a statistical tell a classifier finds (F6).
- **Rejection Reason**: Not kind-blind; the prototype's approach is evidence of feasibility only.

### Alternative 3: Emit the ideal direction and add an SCL turn-smoothing filter on every pawn (OQ11 option b)
- **Description**: The follower emits continuous world directions; the Shared Pawn applies one turn-smoothing filter to every pawn's movement input so AI and human headings look alike.
- **Pros**: Simpler follower; one filter, applied to every pawn, so it is kind-blind by construction.
- **Cons**: Smoothing makes the AI look less robotic but does not make it look like a *human with a device*: keyboard players produce 8-way quantised headings tied to the camera yaw, pad players analog curves with dead zones, and both bend their path when they glance. A smoothing filter erases none of that from human data and adds none of it to AI data, so the F6 heading-change and head-yaw features still separate the kinds. It would also change every human's feel (input latency on turns), which SCL treats as a feel regression.
- **Rejection Reason**: Matches the code, not the motion. Recorded as the fallback only if logged human profiles cannot be obtained and AC-CNB-28 shows heading features are the only failing ones.

### Alternative 4: Crowd behaviour timers and the ambient schedule on the engine timer manager (the prototype)
- **Description**: `FTimerManager` timers per NPC for pauses, wanders and emergence.
- **Pros**: Trivial to write.
- **Cons**: Timer callbacks run outside the queue, so spawns and absorptions would not be ordered against kills and takeovers (ADR-0002); `FMath::FRandRange` breaks determinism and secrecy (ADR-0006); frame-quantised timer firing ties behaviour to host frame rate.
- **Rejection Reason**: Violates ADR-0002 and ADR-0006. Behaviour clocks become sampled server-time timers in the crowd tick; schedule events become queue items.

### Alternative 5: `UAIPerceptionComponent` for sight
- **Description**: Engine perception with a sight sense per NPC.
- **Pros**: Standard; debug tooling.
- **Cons**: Its affiliation model (`IGenericTeamAgentInterface`) invites team-aware perception; per-NPC sense ticks cost more than one shared hash plus a capped ray budget; concealment filtering would need a custom sense anyway.
- **Rejection Reason**: A second, kind-aware perception model and more cost for no needed feature.

## Consequences

### Positive
- NPC motion and player motion share every link from input to the wire: the same command entry points, the same held-intent `Move`, the same control-rotation consumer and the same `HeadLook` filter. Remaining differences are behavioural choices, measured by the F6 gates, not pipeline artefacts.
- The pitch path is fixed by one override (E1), so G3 "look at goods" and every human-like downward glance are possible.
- Determinism and secrecy: every crowd draw is reproducible with the server secret and unpredictable without it; kills and takeovers shift no other pawn's draws.
- Schedule events are totally ordered with kills and takeovers, so the ledger, CPA cards and takeover supply never race.
- The decision layer is authorable in the StateTree editor; the follower and Director cores are plain C++ with headless specs.
- Driver selection by call parameter removes Crowd's only Roster read.

### Negative
- More code than stock AI: a follower, a virtual input layer, a spatial hash, budgets and two sink types.
- Motion parity depends on human telemetry that does not exist yet; until it does, profiles are provisional and AC-CNB-28 is advisory for MVP.
- Three cross-ADR requests before implementation: one more `ETimerKind` plus the input-generation definition (ADR-0002 amendment), one ADR-0006 note (continuous mapping), one ADR-0010 amendment (non-replicated `UGameplayTasksComponent` on every pawn).
- Per-frame virtual camera and due-time evaluation for ~50-60 controllers is a fixed cost not yet in F5.
- Per-controller ticks for ~50-60 controllers (the `c_t` term of F5) are a fixed cost even when StateTrees sleep.
- StateTree's scheduled-tick behaviour is post-cutoff and undocumented in the engine reference; it is verified in source only.

## Risks

- **Cost over budget on 3060-class hosts** (OQ10, AQ-18). Mitigation: named scopes from day one, the fixed global cut order, pre-baked route-edge paths; escalation to the creative-director before any `NpcCount` change.
- **Removing `PathFollowingComponent` breaks the gameplay debugger or visual logger** (E3: `AIController.cpp` is null-safe [V]). Mitigation: verification (2); fallback keeps the component, never ticked or used, asserted by AC-CNB-02.
- **AI possession adds a component only AI-driven pawns carry** (E13 [V]). Mitigation: ADR-0010 amendment (default non-replicated `UGameplayTasksComponent` on every pawn); replicated-subobject inventory in the leak suite (verification (7)). Until the amendment lands this ADR cannot be Accepted.
- **StateTree scheduled ticks lose their phase** (the component overwrites its schedule after every tick and events wake it next frame, E12 [V]). Mitigation: phase-aligned grid re-imposed in the overridden `TickComponent`; AC-CNB-33's per-frame decision bound; fallback: the Director drives `ScheduleTickFrame` per pawn on the grid.
- **A disabled `StateTree.Component.ScheduledTickEnabled` cvar silently forces every-frame ticks** (E6 [V]). Mitigation: Director init assertion and harness boot check; stale compiled assets after a schema change are caught by the same check.
- **Async path results arrive ≥ 2 frames after the request, later under load** (E7 [V]). Mitigation: pre-baked route-edge paths for most legs; the follower keeps its current `Move` while waiting, then holds ≤ 0.5 s and `Move(0)` on an invalid path; leg ids and epochs drop stale results.
- **Virtual input layer still classifiable** (AC-CNB-27/28). Mitigation: profiles are data and re-derived per playtest build; Alternative 3 as a recorded fallback; CNB F6 gate blocks Alpha.
- **ADR-0002 amendment not accepted.** Mitigation: the ambient-cycle story stays blocked; no local timer is permitted as a workaround.
- **Concurrent ADR drift.** ADR-0010's draft was read and its names adopted (`AStealthCrowdController`, `RequestHandOver`, hand-over pass, factory-owned `GetControlEpoch`); ADR-0011's `Voided(StaleControl)` stamping is consistent. Remaining risk: the spawn-ordinal accessor name and the held-intent mechanism are ADR-0010/0011 details; reconciled at `/architecture-review`.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| crowd-npc-behaviour.md | CNB-R1 population, spawn/remove only through `RequestEmergence` and the factory | §6: schedule items resolve emergence/absorption through LSC and `SpawnSharedPawn`/`RequestTeardown` only |
| crowd-npc-behaviour.md | CNB-R2 identity blindness, concealment-blind perception, occupancy-blind bush use | §5 public-fact perception + `ICrowdPerceptionFilter`; §7 driver kind by call parameter (no Roster read); bush choice reads no occupancy (Concealment exposes none) |
| crowd-npc-behaviour.md | CNB-R3 same commands, dry run, pitch path | §2 sinks, dry run, null reply sink; §4.4 `UpdateControlRotation` override |
| crowd-npc-behaviour.md | CNB-R3.1-R3.4 pace, path, avoidance as `Move`, motion parity matched to logged human input via `EvaluateViewEnvelope` | §4 follower and virtual input layer (OQ11 decided: input emulation); profiles as data |
| crowd-npc-behaviour.md | CNB-R4-R8 catalogue, gaze, red herrings, reactions | §3 StateTree states/tasks; sampled reaction timers; §5 stimulus fan-out with responder cap and refractory |
| crowd-npc-behaviour.md | CNB-R9 ambient cycles, replacement, exile exit slot | §6 `TimerFired(CrowdSchedule)` items; §7 `AdoptReleasedPawn` params |
| crowd-npc-behaviour.md | CNB-R10 bound driving, idle bound, hand-back, `ReleaseToCrowd`, handover order with control epoch | §2 restricted sink + epoch; §3 watchdog; §7 `ICrowdControl` semantics |
| crowd-npc-behaviour.md | CNB-R11 takeover supply, atomic release | §8 `ITakeoverEligibilitySource` |
| crowd-npc-behaviour.md | CNB-R12 phase gating, Halt at Round's round-end step (R9 two-step) | §7 Dormant-until-Active, crowd unchanged between latch and step, synchronous Halt latch |
| crowd-npc-behaviour.md | CNB-R14 counter-based, kill-independent, ordinal-keyed draws | §9 `FCrowdDraws` on domain 13; counters in the Director |
| crowd-npc-behaviour.md | CNB-R15 Director + StateTree ~4 Hz + follower; no path-following/MoveTo; ≤ 2 path starts/frame; spatial hash; frozen nav; 1.5 ms; global cuts | §1, §3, §4, §10 |
| crowd-npc-behaviour.md | F5 server AI cost; AC-CNB-26 named scopes | §10 |
| crowd-npc-behaviour.md | F6 pipeline parity; AC-CNB-27 | §4.6 recordable virtual-input stream replayed through a bot client |
| input-interaction-framework.md | R2 no caller identity; R17 AI uses the same entry points; NPCs never `Attack` | §2 (null sink, no `Attack` method) |
| shared-character-locomotion.md | R4 handover / last-command hold; R8.1 AI writes Look pitch into control rotation | §7 handover order; §4.4 |
| round-match-framework.md | R9 round-end step Halt; R17 restricted driver | §7 |
| information-visibility-policy.md | R7.6 NPC bush parity input (F5) | §3 bush red herrings via the one Hide path, rates from frozen tuning |
| session-travel.md | Rule 14 sub-seeds; 25f stall cover | §9; F4 regulator runs in the Director |

## Performance Implications
- **CPU**: Crowd F5 estimate ≈ 0.74 ms/frame mean at 60 pawns, 60 fps (≈ 1.2 ms peak; ≈ 0.62 ms at 50 pawns); budget ≤ 1.5 ms p99 / ≤ 0.8 ms mean, provisional pending the 60-character spike. Path search runs on the task graph. AI-moved CMC simulation is SCL's line.
- **Memory**: ~50-60 controllers + StateTree instance data + followers (≈ tens of KB each) + Director ledger and hash: well under 10 MB.
- **Load Time**: two StateTree assets and one profile asset; negligible.
- **Network**: none of its own. Crowd replicates nothing; NPC motion costs exactly what player motion costs (ADR-0003 parity).

## Migration Plan
The prototype `AStealthNpcController` (`MoveToLocation`, `FMath::FRandRange`, `FTimerManager`, `GetRandomReachablePointInRadius` wander) is evidence that 40 possessed `ACharacter` NPCs on a navmesh are feasible (first-playable report: 50 characters at 89.6 fps on the RTX 5090 host). It is **replaced, not ported**: no code moves into `StealthGameplay` until this ADR is Accepted (ADR-0007 promotion rule). `BP_CrowdCharacter` is retired in favour of the one `AStealthPawn` class (ADR-0001, ADR-0010).

## Validation Criteria
- AC-CNB-01/02/11 (build greps and instrumentation: no Roster includes, no MoveTo/path-following ticks/direct velocity writes, no NPC `Attack`).
- AC-CNB-21, 29, 33, 34, 37 (cadence invariance, kill-independent and deterministic draws, phase spread, rates constant).
- AC-CNB-22, 23, 38 (Halt, atomic release, epoch).
- AC-CNB-26 (cost) on the reference host; AC-CNB-27 (pipeline parity) nightly; AC-CNB-28 (classifier gate) at playtest.
- Verification items (2)-(6) in Engine Compatibility.

## Related
- ADR-0001 (identity invariants; this ADR implements its §2 for AI), ADR-0002 (queue; **amendment requested**: `ETimerKind::CrowdSchedule` and a definition that AI decision timing is input generation whose outputs enter the command path), ADR-0003 (pawn parity, pitch write), ADR-0004 (Crowd `_Server` sub-struct, `UCrowdMotionProfileData` soft ref), ADR-0006 (domains 5, 9, 13; **note requested**: unit-fraction continuous mapping), ADR-0007 (module placement, composition root), ADR-0008 (harness for AC-CNB-26/27), ADR-0010 (factory, spawn ordinal, `AStealthCrowdController`, `RequestHandOver` + hand-over pass, control epoch; **amendment requested**: non-replicated `UGameplayTasksComponent` default subobject on every pawn), ADR-0011 (command component, ingress epoch stamping, `CanBegin`), ADR-0014 (frozen nav, route data, emergence), ADR-0017 (implements `ICrowdPerceptionFilter`), ADR-0019 (consumes eligibility), ADR-0023 (telemetry counters).
- GDD sync items (not edited here; GDDs are owned by their authors): **Crowd** CNB-R2 and AC-CNB-01 — the "one permitted read" of the Roster binding becomes a call parameter (`DriveBoundPawn` vs `AdoptReleasedPawn`), so the include check expects zero Roster call sites; CNB-R15 / OQ11 — resolved as input emulation (§4), OQ8's StateTree verification done here; CNB OQ5 — possession confirmed, executed by ADR-0010's hand-over pass (§7); CNB-R12 and the States table `Halted` row — Halt latches the sink, clears `Move` and stops the tree, while holds are cancelled by Round's round-end step (g) and braking is ADR-0010's stop-and-blend for every pawn (Crowd no longer "ends outstanding holds"); CNB OQ8 — StateTree scheduled-tick behaviour verified (E5, E6, E12, E14, E15). **Round** R13 item table — add `TimerFired(CrowdSchedule)` (with the ADR-0002 amendment). **Tuning** — register `UCrowdMotionProfileData` soft-ref key and the StateTree asset keys in the Crowd ServerOnly sub-struct. **SCL** OQ14 — Alternative 3 rejected; `bUseRVOAvoidance` false for every pawn.
- Design: `design/gdd/crowd-npc-behaviour.md`; `design/gdd/input-interaction-framework.md` R1, R2, R17; `design/gdd/shared-character-locomotion.md` R4, R8; `design/gdd/round-match-framework.md` R9, R17; `design/gdd/camera-system.md` F1.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §5, §6: NPC victim keeps its driver until the death write's freeze flag; no `OnPawnKilled` effect observable before the death write (exit cancellation limited to exits due at or after `KillServerTime`); strike stimuli only from `OnActionStarted(Attack)`; `KillServerTime` = `GetScheduledDeathServerTime(Victim)` — requested by ADR-0018 (item 4).
- §8: Penalty is the only caller of `TryReleaseNpc` (the "via LSC" wording removed); `ReleaseControl` idempotent; `AdoptReleasedPawn(Pawn, {})` reverses `TryReleaseNpc`; `DriveBoundPawn` covers `Unresponsive` informant-bound pawns — requested by ADR-0019 (item 5). Topic owner per architecture.md is ADR-019 (informant takeover sequence), so its caller rule wins over this ADR's earlier wording.
- ADR Dependencies (Enables): recorded virtual-input stream attributed to §4 + an ADR-0008 T3 capture, not ADR-0023's export — requested by ADR-0023 (item 7).
- This ADR's own requests to ADR-0002 (`CrowdSchedule`), ADR-0006 (continuous mapping) and ADR-0010 (`UGameplayTasksComponent`) are applied in those ADRs.

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- Requests answered (2026-10-24): the ADR-0002 `CrowdSchedule` amendment and the AI-timing definition, the ADR-0006 unit-fraction note and the ADR-0010 `UGameplayTasksComponent` request (Related) are applied by their owners. The Status line's prerequisites list no unanswered request — architecture-review fix 2026-10-25 (Conflict 9).
- `TimerFired(CrowdSchedule)` items use ADR-0002 §2a's generic Foundation `FTimerPayload` (`Subject` = pawn or participant, `Generation` = schedule generation, `Seq` = event ordinal, `Phase` = event kind); Crowd declares no queue payload type — architecture-review fix 2026-10-25 (Conflict 1).
- Project file applied 2026-10-25 (phase 2, user-authorised): `StateTree` and `GameplayStateTree` plugins enabled in `StealthGame.uproject` (StateTree was already mounted transitively; GameplayStateTree now mounts). No module dependency added: no project code uses StateTree yet; the Crowd module adds `StateTreeModule`/`GameplayStateTreeModule` when `UCrowdStateTreeComponent` lands.

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §3, §5 — user decision 2026-10-25: the asset keys `CrowdMotionProfileAsset`, `CrowdFullStateTreeAsset`, `CrowdRestrictedStateTreeAsset` are registered in the Crowd ServerOnly tuning sub-struct (TDC; no longer class defaults).
- §4 — user decision 2026-10-25: legs along a `CrowdRoute` edge use the edge's pre-baked path stored with the route graph (LSC, Crowd CR 17).
- §4, Related — user decision 2026-10-25: the touch half of the shared idle overlay stays off in MVP (no touchable-prop records, Crowd CR 8 declined); the sit/browse verb is declined for MVP (CNB OQ2).
- §4 device profiles — user decision 2026-10-25: no client-reported input-device field in MVP; the device mix comes from the coordinator's manual playtest sheet (ADR-0023).
- user decision 2026-10-25: `NpcCount` default 50 (was ~40, user-locked default changed by the user), so 5v5 has 60 characters; Crowd F2 per-NPC rates scale by 40/50 (interval keys × 1.25: run 405 s, vault/climb 1443/3409 s, brace 3000 s, decoy 13,125 s, reversal 395 s, head hold 600 s; bush 315 s); F5 ≈ 0.74 ms mean at 60 pawns; the spike is a 60-character spike.

User scope decisions (2026-10-25: "accept all recommendations"); decision unchanged, Status unchanged (Accepted).
- §4 pace — user decision 2026-10-25: the keyboard walk-slower modifier exists (IIF R15a), so Crowd's interim full-pace rule is lifted: `CrowdFullPacePct` default 55 and C4 group-drift pace matching active (Crowd R3.1, T16 "Yes"). The follower's pace decision (`ECrowdDecision::Pace`) already draws from the shared set; only the tuning default changes.

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- F2 (closeout D7): Crowd's default bush interval is **300 s** (was 315 s after the 40-to-50 rescale) to absorb `r_P` drift; the 300 s figure replaces "bush 315 s" in the 2026-10-25 NpcCount amendment above, and the registry carries it. Crowd's stop deceleration consumes SCL's `StopMinSec` (closeout D2, ADR-0010); no separate pace-set key. `CrowdSchedule` event kinds are unchanged (ADR-0002 confirmation).
