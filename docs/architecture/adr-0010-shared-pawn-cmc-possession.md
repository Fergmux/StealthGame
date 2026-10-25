# ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It cannot be Accepted before ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0005, ADR-0007 and ADR-0008 are Accepted and the spikes listed under Engine Compatibility → Verification Required have reported (traversal under net emulation, S8, S14, H1, P1).

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims read in the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; design against `shared-character-locomotion.md` in its "NEEDS REVISION, re-review pending; reconciled 2026-10-24" state)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full, review mode full); technical-director (TD-ADR) and unreal-specialist validation (gate record below); acceptance reserved for the user.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (hand-over pass and `PawnLeavingGame` named as apply-only steps with an ADR-0002 amendment request; epoch bumped at commit; no epoch bump in `PawnLeavingGame`, teardown guard, cached last input; BoundLive-but-AI window stated; `AGameNetworkManager` values pinned in config instead of a run-time CDO write; Unresponsive hand-over, participant-bound AI constraint, `Reset`/`OutsideWorldBounds` covered). Engine validation (unreal-specialist lens): BLOCKING on the AI → player hold mechanism and E7 → REVISED (hold simulates with `PerformMovement`, gated on a factory flag, never advancing client timestamps); notes applied (tick-pose override dropped as unsafe, `DetachFromControllerPendingDestroy` override dropped, pace snap on the server for `MoveAutonomous`, starvation 4 Hz stepping as spike ST1, `ReceiveRestarted` audit). Details under Validation gates.

## Summary
Every body in the city is one `AStealthPawn : ACharacter`, and Unreal's defaults make the possession, spawn and disconnect paths differ by controller kind: possession zeroes velocity and resets the movement mode (`APawn::Restart`), a leaving player's pawn is destroyed (`APlayerController::PawnLeavingGame`), and the engine spawns player pawns on login. This ADR commits the MVP to the CharacterMovementComponent (not Mover), makes `UStealthPawnFactory` the only creator and possessor of pawns, defines a kind-blind possession and hand-over pass that keeps velocity, movement mode and head-look, implements the SCL movement model (speed tiers, shaping, restrictions, starvation, traversal) inside one `UStealthMovementComponent`, and publishes the server state the IIF reads.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Core (gameplay framework: pawn, controller, possession) with Networking (CMC client prediction) and Physics (movement) |
| **Layer** | Core |
| **Knowledge Risk** | HIGH — the Shared Pawn sits on the Networking HIGH-risk domain (possession parity, owner-only pointers) and the Movement MEDIUM-risk domain (Mover/ChaosMover Experimental, PhysicsMover removed in 5.8) per `architecture.md` Engine Knowledge Gap Summary. **MotionWarping: Beta in 5.8.4** ([V] `MotionWarping.uplugin` `IsBetaVersion: true`, `VersionName 0.1`), used here for the traversal warp and by `UStealthMotionWarpingComponent` (no replicated `WarpTargets`, ADR-0018 §7.2): its API and the protected `WarpTargets` member may change between engine versions, so the traversal warp and the protected-member disable are on the engine-upgrade checklist |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `breaking-changes.md` (5.8: PhysicsMover removed; `UMotionWarpingComponent::GetCharacterOwner` removed), `deprecated-apis.md`, `PLUGINS.md` (Mover/ChaosMover Experimental), `modules/physics.md`, `modules/networking.md`, `modules/animation.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Module Ownership → Core layer, Data Flow §1/§5, API Boundaries → Core, Required ADRs, Open Questions AQ-06/AQ-09); ADR-0001, ADR-0002, ADR-0003, ADR-0005, ADR-0007, ADR-0008; engine source read this session (E1-E14 below); prototype `Source/StealthGame` (read-only) and `production/first-playable-report.md` |
| **Post-Cutoff APIs Used** | `UMotionWarpingComponent::GetOwnerAdapter()` (5.8 replacement of the removed `GetCharacterOwner`) [V `MotionWarpingComponent.h:134`]; `UE_LOGF` for diagnostics (ADR-0007). Every other API here predates the cutoff and is re-verified in 5.8.4 source |
| **Verification Required** | **Traversal spike** (architecture "spike: traversal under 100 ms / 1 % loss"; AC-SCL-12/13/25): server-authoritative `TraversalState` start, client warp targets from `PointId`, proxy end-position error ≤ `PawnSmoothSnapThresholdCm / 4`, no duplicated clip, the `RepRootMotion` vs follow-replicated-movement choice on proxies (E12), and observer parity of root-motion poses between remotely controlled and server-simulated pawns (E4). **S8**: the `PawnLeavingGame` override keeps the pawn and a crowd AI controller possesses it in the same server frame on a real connection close, including whether spawning a controller is safe while the connection is being cleaned up (E3). **S14** (shared with ADR-0001): owner-only pointers survive every possession path here. **H1 (new)**: AI → player hand-over gap — the server keeps simulating the pawn with the held input until the new owner's first `ServerMove` (E7, §4.3) without touching the CMC's client timestamps, with no dropped first moves, no owner correction above `PawnSmoothSnapThresholdCm` and no visible stop for observers. **ST1 (new, part of the net-emulation spike)**: starvation braking cadence — the engine's forced position updates step the server position once per `MAXCLIENTUPDATEINTERVAL` (E6), so the spike decides between that cadence and a per-frame zero-input update (§6), measured by AC-SCL-23. **P1 (new)**: shaping state (pivot latch, pace hysteresis, starvation flag) is restored on client replay so owner prediction matches the server within 1 frame at 100 ms lag (AC-SCL-06). **ADR-0002 E4 spike**: a CMC pawn spawned by the factory inside the drain (`TG_PostPhysics`) behaves on its first tick |

**Engine facts this ADR relies on** ([V] read in 5.8.4 source this session; [D] engine-reference docs; [U] unverified, owned by the named spike):

| # | Fact | Tag |
|---|---|---|
| E1 | `AController::OnPossess` and `APlayerController::OnPossess` call `InPawn->PossessedBy`, `SetControlRotation(Pawn->GetActorRotation())` and `DispatchRestart(false)` (for a player controller only when it is not local); `APlayerController::OnPossess` also calls `ResetPredictionData_Server()` and `ClientRestart(Pawn)`; `ServerAcknowledgePossession` resets server prediction data a second time; the owning client sends no moves until its `AcknowledgedPawn` is set; AI possession copies the remote role from the class default; `APawn::UnPossessed` consumes the pending input vector. `Possess`/`UnPossess` are `virtual final`; `OnPossess`/`OnUnPossess` are the override points. `DispatchRestart` also fires the Blueprint `ReceiveRestarted` event | [V] `Controller.cpp` (`AController::OnPossess`), `PlayerController.cpp:873-927, 1323-1338` (`OnPossess`, `ServerAcknowledgePossession`), `Pawn.cpp:671-740`, `CharacterMovementComponent.cpp:8914`, `Controller.h:284-306` (TD/engine gate) |
| E2 | `APawn::Restart()` calls only `StopMovementImmediately()`, `ConsumeMovementInputVector()` and `RecalculateBaseEyeHeight()`; `ACharacter::Restart()` also resets jump state, calls `UnCrouch(true)` and `SetDefaultMovementMode()`; `ACharacter::PawnClientRestart()` (owner) calls `StopMovementImmediately()` and `ResetPredictionData_Client()`, then `APawn::PawnClientRestart()`, which calls the virtual `Restart()`. `bInputEnabled` is not touched by either. All are virtual | [V] `Pawn.cpp:496-547, 903-911`, `Character.cpp:1375-1400`, `Pawn.h:415-421`, `Character.h:806-807` |
| E3 | `APlayerController::PawnLeavingGame()` (virtual) destroys the pawn; `APlayerController::Destroyed()` calls it on the authority when `Player == nullptr`, otherwise it calls `UnPossess()`. On a connection close `OnNetCleanup` nulls `Player` and then destroys the controller, so `PawnLeavingGame` runs; `Logout` runs afterwards from `AController::Destroyed`. Whether spawning a controller is safe while the connection is being cleaned up is [U] S8 | [V] `PlayerController.cpp:1467-1488, 1572-1579, 1633-1646`; `Controller.cpp:595-605`; `PlayerController.h:2054` |
| E4 | `ACharacter::UpdateMeshAllowAutonomousTickPose()` (non-virtual; called from `PossessedBy`, `UnPossessed`, `SetReplicateMovement` and CMC `Activate`/`Deactivate`) sets `Mesh->bOnlyAllowAutonomousTickPose = true` on the server for a pawn controlled by a remote autonomous proxy, so its pose and root-motion montages advance with that client's `ServerMove`s instead of the mesh tick; forcing it false would advance root-motion montages twice (mesh tick and `MoveAutonomous`) and drift them ahead of the owner's timeline. `UnPossessed` resets server and client prediction data | [V] `Character.cpp:461, 1403-1421, 2443-2452`; `CharacterMovementComponent.cpp:966, 981, 10705`; `SkeletalMeshComponent.cpp:1910`, `SkeletalMeshComponent.h:880` (engine gate) |
| E5 | `UWorld::SpawnActorDeferred` + `AActor::FinishSpawning` exist; `APawn::AutoPossessAI`, `APawn::AIControllerClass`, `APawn::SpawnDefaultController()` (virtual) exist; `AAIController::bWantsPlayerState` exists | [V] `World.h:3851`, `Actor.h:3117`, `Pawn.h:124, 140, 473`, `AIController.h:120` |
| E6 | Server **forced position updates** (distinct from ADR-0003's forced *net* updates): `APlayerController` (server tick) calls the CMC's `ForcePositionUpdate(DeltaTime)` when no move has arrived for `AGameNetworkManager::MAXCLIENTUPDATEINTERVAL` (default 0.25 s), for at most `MaxClientForcedUpdateDuration` (default 1.0 s, capped at 5 s), skipped after a server hitch; each call is one step over the whole elapsed time, after which the timestamp resets, so a starved pawn's server position steps about once per interval (4 Hz at 0.25 s). These values and the movement-time discrepancy settings are read from the **class default object** of `AGameNetworkManager` (`GlobalConfig`), not from the world's instance; the CMC warns if `MaxMoveDeltaTime > MAXCLIENTUPDATEINTERVAL`. `ForcePositionUpdate` is virtual, simulates with the CMC's **current** `Acceleration` (the last input), advances `CurrentClientTimeStamp`, and `check`s that the remote role is `AutonomousProxy` | [V] `PlayerController.cpp:5527-5618`; `GameNetworkManager.cpp:32-33`; `GameNetworkManager.h:128-134`; `CharacterMovementComponent.cpp:8726-8760, 9449, 12603`; `CharacterMovementComponent.h:2330` (engine gate) |
| E7 | A pawn newly possessed by a remote player: the owner sends no move until it acknowledges the pawn (E1), and its prediction data starts from a reset timestamp; the PlayerController's tick sets a zero `ServerTimeStamp` to now and only then counts towards a forced position update, so for up to `MAXCLIENTUPDATEINTERVAL` after possession the server does **not** simulate the pawn. Advancing `CurrentClientTimeStamp` before the first real move (as `ForcePositionUpdate` does) makes the server reject the owner's first moves (`TimeStamp <= CurrentClientTimeStamp`) | [V] `PlayerController.cpp:5541, 5614-5618`; `CharacterMovementComponent.cpp:8735, 8749-8759, 9401, 9663` (engine gate; replaces the draft's "never simulated until the first move") |
| E8 | CMC override points used here: `GetMaxSpeed`, `GetMaxAcceleration`, `GetMaxBrakingDeceleration`, `ScaleInputAcceleration`, `PhysicsRotation`, `ComputeOrientToMovementRotation`, `SetMovementMode`, `RequestDirectMove`, `RequestPathMove`, `MoveAutonomous`, `UpdateFromCompressedFlags`, `AllocateNewMove`/`FSavedMove_Character` (`GetCompressedFlags`, `FLAG_Custom_0..3`), `SetNetworkMoveDataContainer`; members `bOrientRotationToMovement`, `bUseControllerDesiredRotation`, `bUseRVOAvoidance`, `bRequestedMoveUseAcceleration`, `MaxStepHeight`, `NetworkSmoothingMode`, `NetworkMaxSmoothUpdateDistance`, `ListenServerNetworkSimulatedSmooth*Time` | [V] `CharacterMovementComponent.h` (lines 160, 245, 439, 446, 815-845, 1065, 1073, 1276, 1324-1349, 1505-1584, 1864, 2237, 2459-2462, 2702, 2980-3136, 3257) |
| E9 | `ACharacter::RepRootMotion` (`FRepRootMotionMontage`, `ReplicatedUsing=OnRep_RootMotion`, active-override while root motion plays) replicates a root-motion montage's state to simulated proxies | [V] `Character.h:1159-1172`; `Character.cpp:1786-1805, 2062-2071` |
| E10 | The Game Animation Sample character Blueprint (`CBP_SandboxCharacter`) runs traversal through `TryTraversalAction`, a server RPC `Traversal_ServerImplementation` and a replicated `TraversalResult` with `OnRep_TraversalResult`, switching to `MOVE_Flying` with motion warping; it calls `IsLocallyControlled` (twice) | [V by name only: binary text scan of `Content/Blueprints/CBP_SandboxCharacter.uasset`] / behaviour [U] traversal spike |
| E11 | Mover and ChaosMover are Experimental in 5.8; PhysicsMover was removed; the project `.uproject` enables `Mover` and `NetworkPrediction` (inherited from the sample); a text scan of `Content/` found no asset referencing `/Script/Mover` | [D] `PLUGINS.md`, `breaking-changes.md`; [V] `StealthGame.uproject:83-88`; [U weak] content scan |
| E12 | Whether proxies should play traversal root motion from `RepRootMotion` or ignore root motion and follow replicated movement (SCL R7.6 fallback) | [U] traversal spike |
| E13 | `UMotionWarpingComponent::AddOrUpdateWarpTargetFromLocationAndRotation` and `RemoveWarpTarget` exist; `GetCharacterOwner` was removed in 5.8 (use `GetOwnerAdapter()`) | [V] `MotionWarpingComponent.h:134, 222, 229`; [D] `breaking-changes.md` |
| E14 | Spawning a CMC pawn inside `TG_PostPhysics` ticks it the same frame (`TG_NewlySpawned`) | [V queueing, U behaviour] ADR-0002 E4 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (one pawn class, one command path, owner-only pointers, no kind-keyed branch); ADR-0002 (queue slots, `RequestTeardown`, staging pass, round-end step §8.4(f) that calls this ADR's stop path, spawn-in-drain spike); ADR-0003 (net parity config, `ForceNetUpdate` scoped override with `TraversalStateWrite`, `PreReplication` pitch, `IsNetRelevantFor` hook, smoothing parity — implemented on the class this ADR defines); ADR-0004 (SCL tuning keys, client/server faces, `OnTuningReady` before any spawn); ADR-0005 (`HandleStartingNewPlayer` no-op, no default login pawn, `PawnLeavingGame` hand-over named here); ADR-0007 (`StealthGameplay/Core` placement, composition-root registration of `ICrowdControl`); ADR-0008 (PIE network tests, net-emulation profiles, allowlist audit). All must be Accepted before this ADR is implemented |
| **Enables** | ADR-0011 (the IIF executes `Move`/`Look`/`Traverse` on this pawn and reads its published state); ADR-0012 (`AppearanceId` property and apply-before-first-replication slot in the factory); ADR-0013 (animation reads movement, `HeadLook`, `TraversalState`, the step clock input); ADR-0016 (Crowd drives the pawn only through the IIF and uses the factory for replacements); ADR-0017 (concealment warp-to-anchor and collision re-derivation hooks); ADR-0018 (`Disabled` state at the death write); ADR-0019 (informant takeover through `TakeOverCrowdPawn`) |
| **Blocks** | Shared Character & Locomotion epic; Round & Match Framework round-start spawn and disconnect hand-over stories; Crowd replacement spawning; NPC-Kill Penalty & Informants takeover stories |
| **Ordering Note** | Tenth in table order (Core). The `Mover`/`NetworkPrediction` plugins are disabled only when this ADR is Accepted, through a `.uproject` edit that is outside this pipeline's write scope and already a pending user item (active.md "[create-architecture] Project-config changes", AQ-09). The `AGameNetworkManager` forced-position-update and movement-time-discrepancy values must be pinned in `Config/DefaultGame.ini` (Decision §6) — also outside the write scope, so it is a new pending user item. ADR-0002 §5 lists one exception to "nothing outside the drain mutates gameplay state"; this ADR asks ADR-0002 to name two more **apply-only** steps (the hand-over pass and the `PawnLeavingGame` plumbing, Decision §4.3, §5) — an amendment request recorded under Related, not applied here |

## Context

### Problem Statement

The Shared Pawn is where Pillar 1 meets the engine. ADR-0001 fixed the invariants (one class, one command path, owner-only pointers); this ADR fixes the mechanisms that create, possess, hand over and move that class, and the engine defaults they must defeat. Read in the 5.8.4 source, four defaults would each produce a player tell:

1. **Possession stops the body.** Every `Possess` calls `Restart()`, which zeroes velocity and resets the movement mode, and resets control rotation to the actor rotation (E1, E2). A player taking over a walking crowd pawn, or the AI taking over a disconnected player's running pawn, would stop dead mid-stride on every observer's screen (SCL R3, R4; Round R17).
2. **Disconnect destroys the body.** `PawnLeavingGame` destroys the pawn (E3), so a lost connection makes a citizen vanish (Session rule 22, Roster R9).
3. **The engine spawns player pawns.** Login, `RestartPlayer` and `SpawnDefaultPawnFor` create player pawns outside any NPC path (the prototype still does: `RestartPlayer` at round start, NPCs through `SpawnActor` + a separate controller); ADR-0005 already disables the login path, so pawns need one named creator.
4. **Starvation glides or freezes.** When a client stops sending moves the server forces updates with the **last** acceleration, so the pawn keeps walking for up to a second, then freezes (E6); a pawn newly handed to a remote player is not simulated at all until its first move (E7).

At the same time the movement model must be the SCL's crowd walk for every pawn: two gaits, baseline acceleration and braking, capped turns and pivots, effective-gait clamps by participant state and action, a small traversal set, input starvation, and published server state, all predicted on the owner and identical for AI. The prototype's `UStealthMovementComponent` shows the CMC can host these rules but reads `IsInformant()` from the replicated PlayerState and lets the AI path-following component drive velocity (`bUseAccelerationForPaths`), both of which ADR-0001 forbids.

The decision is needed now because Round, Crowd, Penalty, Concealment and Assassination all create, possess or move pawns, and the movement framework choice (CMC vs Mover) gates the Game Animation Sample's motion matching and traversal.

### Constraints

- ADR-0001: one `AStealthPawn` class, one crowd AIController class without PlayerState, AI and humans through `UPawnCommandComponent` only, no kind-keyed branch, no replicated pawn-owned actor.
- ADR-0002: gameplay state mutates only inside queue slots (and the named round-end step); resolvers never destroy actors inline.
- ADR-0003: net config, forced-update policy, pitch path and relevancy hook are fixed there; this ADR must not add a second configuration path.
- ADR-0005: no engine default pawn spawn; the PlayerController and PlayerState are replaced on travel.
- Engine: UE 5.8.4 binary install, no engine patches; the Game Animation Sample's motion matching and traversal are built on the CMC; Mover is Experimental (E11).
- Team: 1-2 programmers; the movement code must be one component with testable pure shaping functions.
- Write scope: the `.uproject` plugin list and `Config/` are not edited by this pipeline.

### Requirements

- One class, one factory, one takeover function; the factory is the only creator of a Shared Pawn and the only caller of `Possess` on one (SCL R1, R2, R2a; Roster R9; TR-scl-001, TR-roster-004).
- Bind before visible: deferred spawn → `AppearanceId` → `BindPawn` → `FinishSpawning` → possess, one call stack; bind failure destroys the unreplicated pawn (SCL R2; Roster R9).
- Disconnect never destroys a pawn; a crowd AI controller possesses it in the same server frame (SCL R2b; Roster R9; TR-scl-002).
- Possession and hand-over never change class, components, net config, movement mode, velocity, `HeadLook`, AnimBP instance or mesh state, and never produce an off-cadence update (SCL R3, R4; Round R17; TR-scl-003).
- CMC movement with two gaits, shaping, clamps and restrictions identical for every pawn and predicted on the owner; traversal server-authoritative with a replicated `TraversalState` (SCL R5-R7; TR-scl-004, TR-iif-002 execution half).
- Input starvation brakes by the baseline and never glides or catches up (SCL R15, F12; TR-scl-010 starvation half).
- One capsule, collision profile and physics asset (SCL R17; TR-scl-013).
- Published server state `IsRunning`, `IsInTraversal`, `CommandedDisplacementCm` (SCL R16, R19; TR-scl-012).
- Roster queries stay server-only and allocation-free; the pawn never asks them on a client (TR-roster-005).

## Decision

### 1. Movement framework: CharacterMovementComponent for the MVP

The Shared Pawn uses the **CharacterMovementComponent** through one subclass, `UStealthMovementComponent`. Mover 2.0 / ChaosMover is not used in the MVP: it is Experimental in 5.8 (E11), the Game Animation Sample's motion matching and traversal paths used here are CMC-based, and every replication parity rule in ADR-0003 and every engine fact in this ADR were verified against the CMC. The sample-enabled `Mover` and `NetworkPrediction` plugins are **disabled when this ADR is Accepted** (a `.uproject` edit; pending user item AQ-09); until then no project code, Blueprint or asset may reference a Mover or NetworkPrediction type, and the editor audit (ADR-0008) fails on any such reference. Re-evaluating Mover is a superseding ADR, never a per-system choice.

### 2. One class, one controller class, one body

- `AStealthPawn : ACharacter` (ADR-0001 §1) with `UStealthMovementComponent` as its movement component (`ObjectInitializer.SetDefaultSubobjectClass`). The sample's `CBP_SandboxCharacter` is reparented onto it (the prototype shows this works for `AStealthCharacter`); one Blueprint child class carries the AnimBP, databases and mesh defaults and is the class the factory spawns (`UStealthPawnFactory` holds it as a soft class reference resolved before `OnTuningReady` completes). No second Blueprint child exists for NPCs or players.
- **Controllers.** Players: `AStealthPlayerController`. Every AI drive (crowd NPC, participant-bound hand-over, grace): one class `AStealthCrowdController : AAIController` with `bWantsPlayerState = false`; its decision logic is ADR-0016's. `AStealthPawn::AIControllerClass` = that class, `AutoPossessAI = Disabled`, `AutoPossessPlayer = Disabled`: nothing possesses a pawn except the factory's possession paths (§4).
- **Body.** One capsule size, one collision profile, one physics asset, `MaxStepHeight = TraversalMinHeightCm`, walkable floor angle at the engine default, `bUseRVOAvoidance = false`, physics push of pawns off, pawn-pawn capsules block (SCL R5.6, R16, R17). Capsule radius and half-height are the reparented sample character's values, read once in the graybox and recorded in the class defaults (SCL R17.1 [verify]); the values are published to LSC and the Generator from the class default object.
- **Default subobjects identical on every pawn** (amendments 2026-10-24): `AStealthPawn` creates a `UGameplayTasksComponent` as a default subobject with replication **off** on every pawn, so `AAIController::OnPossess` finds one and adds no component (an AI-only replicated component would be a wire-visible tell that survives a takeover; ADR-0016 E13); and its motion-warping component is `UStealthMotionWarpingComponent`, which disables `WarpTargets` replication (`DISABLE_REPLICATED_PROPERTY_FAST`, `COND_Never` audit row) identically for every pawn (ADR-0018 §7.2, E5). Traversal warp targets are therefore derived locally from `TraversalState` on every machine (§7); if the traversal spike shows the sample's Blueprint traversal needs server-set replicated targets, traversal moves to that local pattern, never to replicated targets.
- **Kind-blind code.** Nothing on the pawn, `UStealthMovementComponent`, the AnimBP or the step clock reads `IsPlayerControlled`, `IsBotControlled`, the controller class, the PlayerState or Roster state for anything another client can observe (ADR-0001). `IsLocallyControlled` is allowed only in owner-local presentation (camera, own head hiding, input binding; SCL R9); the sample's two `IsLocallyControlled` calls (E10) are audited against that list before promotion.
- **Inputs the movement rules need about the participant** (the effective-gait clamp needs "is this pawn's participant an Informant?") come through one injected query, `IGaitLegalitySource`, set on the movement component by the composition root: on the server it calls Roster `GetLegalityClass(Pawn)` (Unbound → `Active`, so no binding is exposed); on the owning client it reads the participant `State` from the owner's own C-TeamView owner part (ADR-0001 §5); on every other client it is never called (simulated proxies follow replicated movement). The prototype's `IsInformant()` from the replicated PlayerState is replaced, not ported.

### 3. The pawn factory (`UStealthPawnFactory`, world subsystem, server-only)

`UStealthPawnFactory` (`StealthGameplay/Core`, `Public/Server/` face, ADR-0007) is the **only** code that spawns a Shared Pawn, calls `Possess`/`UnPossess` on one, or changes its controller. It asserts authority and game thread on every call and reports misuse to the fault sink.

**3.1 `SpawnSharedPawn(Transform, AppearanceId, ControlKind, Participant?, ExpectedEpoch)`** — callers: Round (round-start mixed spawn) and Crowd (NPC replacement, including Round R17.9's `PawnRecovery` own-card respawn, which Crowd's replacement path performs with `Participant` set). One call stack:
1. `SpawnActorDeferred<AStealthPawn>(PawnClass, Transform, Owner = nullptr, Instigator = nullptr, AlwaysSpawn)`; then `SetInstigator(Self)` (ADR-0001, IVP R5); auto-possess disabled by class default.
2. `ApplyAppearance(AppearanceId)` — the one property and apply function (CPA CP-10; ADR-0012 owns the meaning). It returns `bool` (ADR-0012: refuses `255`, an id with no pool row and a second write); on `false` the factory destroys the deferred actor before `FinishSpawning`, reports the fault and returns with `bAppearanceRejected` set. Nothing replicates before step 4.
3. If `Participant` is set: `Roster.BindPawn(Participant, Pawn, ExpectedEpoch)`. On any rejection: `Destroy()` before `FinishSpawning` (no client ever receives the actor) and return the code (Round aborts `BindFailed`; Crowd follows its retry rule).
4. `FinishSpawning(Transform)`.
5. Possess **in the same call stack**: `ControlKind = Ai` → spawn one `AStealthCrowdController` and possess; `Player` → the participant's `AStealthPlayerController` possesses (a participant that is not `Connected` gets `Ai`, Round R17.7). Possession runs through the parity wrapper of §4.2.
Result: `FSpawnResult { AStealthPawn* Pawn; EBindResult Bind; }`. Replication begins on the next net tick, so a client's first bunch already carries `AppearanceId` and no binding (SCL R2). Spawns requested inside a queue slot are allowed pending the ADR-0002 E4 spike; if it fails, factory spawns move to ADR-0002's post-drain pass with this sequence unchanged.

**3.2 `TakeOverCrowdPawn(Pawn, Participant, ExpectedEpoch)`** — caller: NPC-Kill Penalty & Informants only (informant respawn and Round R18's awaiting-informant path), inside a queue slot. It **owns the eligibility check and the bind, atomically**:
1. Eligibility (SCL R2a.1): `Unbound`, not `Disabled`, `FActionState.Pose == None`, not `IsInTraversal`, not `IsRunning`, not concealed (Concealment's server query), no pending hand-over, not `IsPendingTeardown` (ADR-0002 §7). Otherwise return `NotEligible` and change nothing.
2. `Roster.BindPawn(Participant, Pawn, ExpectedEpoch)`; on rejection return its code and change nothing.
3. `ICrowdControl::ReleaseControl(Pawn)` (Crowd stops issuing commands for it; the consumer-owned interface of architecture.md).
4. Schedule the AI → player hand-over (§4.3) for the participant's controller.
No actor, component or NetGUID is created; `AppearanceId`, velocity and `HeadLook` are unchanged. Between the bind (in the slot) and the possession (next frame start) the pawn is `BoundLive` but still crowd-possessed, issuing nothing (Crowd released it) and carrying the last-command hold; every Roster query and every system must treat that state as ordinary (the Roster never reads controllers). Card moves (`TakeNpcCard`) and `RecordAppearance` are ADR-0019's, with one constraint fixed here: **no card move is committed unless `TakeOverCrowdPawn` returned `Applied`**. (GDD sync note: Roster R9 and Round R18 name Penalty as the `BindPawn` caller; under this ADR Penalty calls `TakeOverCrowdPawn`, which binds on its behalf — recorded under Related.)

**3.2a `CheckTakeOverEligibility(const AStealthPawn*) const`** (amendment 2026-10-24, ADR-0019): the side-effect-free form of `TakeOverCrowdPawn` step 1 (same predicate, same code), returning `Applied` (eligible) or `NotEligible`. Penalty and the Layout Service's respawn pre-filter call it before committing, so `TakeOverCrowdPawn` → `NotEligible` is unreachable in normal play; `TakeOverCrowdPawn` still re-checks.

**3.2b `ReleaseBody(AStealthPawn*)`** (amendment 2026-10-24, ADR-0018 §6.1): committed inside Assassination's `TimerFired(BodyRelease)` slot and executed apply-only by the hand-over pass. It detaches the body from its controller kind-blindly: a crowd controller is destroyed; a PlayerController is left pawnless in its normal playing state (never `Spectating`, ADR-0001) and keeps its view target (ADR-0015 §1 `SetViewTarget` override; `AStealthPlayerController::OnUnPossess` skips the engine's `SetViewTarget(this)` for factory-initiated unpossess, ADR-0018/ADR-0019). No binding changes; the control epoch is bumped at commit like any control change.

**3.3 Default pawn paths are dead.** `AStealthGameMode` never calls `RestartPlayer`, `SpawnDefaultPawnFor` or `SpawnDefaultPawnAtTransform` (ADR-0005 makes `HandleStartingNewPlayer` a no-op); `DefaultPawnClass` is null; `AStealthPawn::SpawnDefaultController` is overridden to do nothing (possession only through the factory). An automation test asserts that every `AStealthPawn` alive after round start was created by the factory (a factory-stamped server-only creation token; AC-SCL-02).

### 4. Possession parity and the hand-over pass

**4.1 What possession may change.** Only the controller pointer, `Owner`, `PlayerState` (all three `COND_OwnerOnly`, ADR-0001), the control rotation, the owner-only camera and input setup on the owning client, and the server's per-pawn control epoch (§4.4). It never changes class, components, net config (ADR-0003), movement mode, velocity, `HeadLook`, `FActionState`, AnimBP instance or mesh state, and never forwards a forced net update (ADR-0003 §2 drops the engine's possession `ForceNetUpdate`).

**4.2 Defeating the engine's restart (E1, E2, E4).** The factory wraps every `Possess` in `FScopedPossessionParity`, which captures velocity, movement mode, `HeadLook` world rotation and the last commanded input before the call and verifies them after it (development builds fault on any difference). The class overrides make the engine path itself parity-preserving:
- `AStealthPawn::Restart()` does **not** call `Super`: it calls `RecalculateBaseEyeHeight()` only (no `StopMovementImmediately`, no `ConsumeMovementInputVector`, no `UnCrouch`, no `SetDefaultMovementMode`). This is safe because `APawn::Restart` does nothing else (E2); crouch and jump are absent from the MVP (SCL R5.1). The reparented Blueprint is audited for an `Event Restarted` (`ReceiveRestarted`, still fired by `DispatchRestart`, E1) before promotion.
- `AStealthPawn::PawnClientRestart()` (owner) calls `ResetPredictionData_Client()` and then `APawn::PawnClientRestart()` directly (skipping `ACharacter`'s `StopMovementImmediately`); the virtual `Restart()` it calls is the override above. The owner keeps the replicated velocity it already displayed.
- After `Possess` returns, the wrapper sets the new controller's control rotation from the pawn's current `HeadLook` world rotation (SCL R4.2), overriding the engine's `SetControlRotation(GetActorRotation())` (E1), so the head does not move at the switch.
- **Last commanded input survives unpossession.** `APawn::UnPossessed` consumes the pending input vector and a human's last input lived only in the CMC's `Acceleration` from `ServerMove` (E1), so `UStealthMovementComponent` caches the last commanded input (direction, pace, gait) every time it consumes a move, from either source, and the hold of §4.3 re-injects that cached value each tick.
- **Server pose ticking (E4) is left to the engine.** The engine advances a remotely controlled pawn's pose with its `ServerMove`s and an AI pawn's pose with the mesh tick; forcing one behaviour would double-advance root-motion montages (E4). This is a server-internal timing difference applied by engine role, not a branch in project code; the parity requirement is on what observers receive (positions, `FActionState`, `TraversalState`, montage state through `RepRootMotion`), which the traversal spike and AC-SCL-13/14 measure. SCL R11.5's "always-tick, identical setting" is scoped to that observable result (GDD change request under Related): every pawn's pose, montages, root motion and notifies advance on the server whether or not the host renders it.

**4.3 The hand-over pass (SCL R4; Round R17).** A possession change of an existing pawn is **committed** inside a queue slot (Round's disconnect/rejoin policy item, Penalty's takeover) and **executed** at the start of the next server frame by `FPawnHandoverTickFunction` (a custom tick function in `TG_PrePhysics`, registered on the authority in `OnWorldBeginPlay`, with the CMC tick of each affected pawn as a dependent prerequisite). It is the only place a live pawn changes controller, except §5's disconnect hook. The pass is **apply-only**, like ADR-0002's staging pass: every decision (who drives the pawn, from when) was made and logged in the committing slot; the pass makes none, so it does not breach ADR-0002's drain rule (ADR-0002 is asked to list it by name, Related).
- `RequestHandOver(Pawn, EControlTarget {Ai, Player(Participant)}, ExpectedEpoch)` validates, bumps the control epoch (§4.4) **at commit**, records the pending change and returns; the pass executes it next frame. Any command that reaches its slot between the commit and the possession, including during a traversal wait, carries the old epoch and is `Voided(StaleControl)`.
- **Callers and cases**: Round's disconnect policy items (R17.1 `Unresponsive` Player → AI while the controller still exists, and the reverse on `Responsive`/`Rejoined(Superseded)`; R17.3 rejoin inside grace AI → player); Penalty's takeover (§3.2). While a Player → AI hand-over is pending or done, late `ServerMove`s from the old owner are ignored because the pawn is no longer that connection's autonomous proxy. The participant-bound AI constraint (Move and Look only, Round R17.1) is Crowd's behaviour (ADR-0016) and the IIF's entry points (ADR-0011) carry no exception for it.
- **Never during a traversal or a root-motion action pose**: a pending hand-over on a pawn with `IsInTraversal`, or whose `FActionState.Pose` is a root-motion pose (`Strike`, `Counter`, `BushEnter`, `BushExit`; e.g. the exile hand-over waits for the strike lockout, ADR-0019), waits until it ends (at most the clip length), then runs.
- **Pending-request rules** (amendment 2026-10-24, ADR-0019): a later `RequestHandOver` on a pawn **replaces** a pending one (each bumps the epoch at commit); a `Player` target whose controller is invalid when the pass runs is **dropped** (logged; the pawn keeps its current drive and Round's disconnect policy decides next); within one pass, **body releases (§3.2b) execute before possessions**, so a controller released from a body is pawnless before it possesses another.
- Execution order per pawn: (1) the outgoing controller's outstanding holds end through the IIF as a release (IIF R4); (2) unpossess / possess through §4.2's wrapper; (3) control rotation from `HeadLook`; (4) **last-command hold**: the movement component keeps applying the last `Move` it received until the new controller's first command or `HandoverHoldSec`, whichever is first (SCL R4.3, Crowd change request 15b).
- **Player → AI**: the crowd controller is told by `ICrowdControl::DriveBoundPawn` (Round's call, ADR-0016) to issue `Move(current heading, Walk)` for `HandoverHoldSec`, so a running pawn slows through the baseline run-to-walk deceleration.
- **AI → player** (rejoin, takeover): the server keeps simulating the pawn with the held input until the new owner's first `ServerMove` arrives (E7: the engine would otherwise leave it unsimulated for up to `MAXCLIENTUPDATEINTERVAL`, a visible stop). Mechanism: the factory sets a per-pawn `bAwaitingFirstOwnerMove` flag at the possession step, cleared by `UStealthMovementComponent` when it consumes the first `ServerMove` (`ServerTimeStampLastServerMove` advances). While the flag is set and the pawn is remotely controlled, the component's own server tick runs `PerformMovement(DeltaTime)` with the held input **without touching the CMC's client timestamps** (it must not call `ForcePositionUpdate`, which advances `CurrentClientTimeStamp` so the owner's first moves would be rejected, and asserts on the host's own pawn, E6/E7). Position keeps replicating at the fixed cadence. The owner's first prediction starts from the replicated state; any correction must stay below `PawnSmoothSnapThresholdCm` (**spike H1**; fallback: keep the crowd controller driving until the client acknowledges the pawn, then possess). A player who gives no input sees the pawn brake by the baseline once the hold ends (`WalkStopSec`).

**4.4 Control epoch.** Each pawn carries a server-only `uint32 ControlEpoch` (in the factory's per-pawn record, never replicated), bumped inside the deciding slot for every control change: by `RequestHandOver` at commit (§4.3), by `TakeOverCrowdPawn` through it, and by `MarkControlChange(Pawn)` when a slot decides a drive change without a new possession (Round's `ConnectionTransition` after `PawnLeavingGame`, §5); the possession step itself never bumps it. The IIF stamps every command it accepts with the pawn's current control epoch, and a command whose epoch is stale resolves `Voided(StaleControl)` at its slot (Round R17: "commands stamped under the old epoch void at re-validation"; ADR-0002 §2's "control/possession epoch"; ADR-0011 owns the stamping).

**4.5 Round freeze and death (the kind-blind stop path).** `UStealthPawnFactory::StopAllForRoundEnd()` is the stop path ADR-0002 §8.4(f) calls inside the round-end step: it sets the server freeze flag on every live pawn's movement component (AI-driven and human-driven alike), so effective input is zero and every pawn brakes by the baseline (`WalkStopSec`/`RunStopSec`) to idle, never a snap; a pawn mid-traversal finishes the traversal first (SCL States `Disabled`). For human pawns the server zeroes the acceleration taken from each `ServerMove` (`MoveAutonomous` override) while frozen; the owner mirrors it from `FRoundPublicState.Phase == Ended` (All audience, so no new fact). A killed pawn enters `Disabled` at Assassination's death write (ADR-0002 staging pass; ADR-0018) through the same flag. The freeze flag is never replicated on the pawn.

### 5. Disconnect never destroys a pawn (S8)

`AStealthPlayerController::PawnLeavingGame()` is overridden (E3): it does **not** destroy the pawn.
- **Teardown guard**: if the world is tearing down or the controller is being replaced by seamless travel (ADR-0005), it only unpossesses (travel destroys every pawn anyway, and `InvalidateAllBindings` has run).
- Otherwise it ends the outgoing controller's holds as releases (the IIF's End path; their resolution happens in the next drain like any End), unpossesses through the §4.2 wrapper, and in the same server frame spawns an `AStealthCrowdController` that possesses the pawn and applies the last-command hold from the cached input (§4.2, §4.3).
- **No control-epoch bump here**: commands from the departed connection are already dropped by its stale `ConnectionGeneration` (ADR-0002 §3), and Round's `ConnectionTransition` slot performs the epoch bump when it decides the drive (`ICrowdControl::DriveBoundPawn`). Round R17.2's "take over at once" therefore reduces to that `DriveBoundPawn` call plus Round's usual `RequestHandOver(Pawn, Ai)` (ADR-0016 pairs the two in one slot): on a pawn that is already crowd-possessed, `RequestHandOver` to `Ai` only bumps the epoch (it is `MarkControlChange`) and schedules no possession.
- The binding is untouched (Roster R9). No gameplay state changes: the crowd controller issues no command of its own until Round's slot calls `DriveBoundPawn`, so this is apply-only possession plumbing during controller destruction (ADR-0002 is asked to name it, Related). Mid-traversal, the hold continues through the landing.
- **Other engine destroy paths** (engine gate): `APawn::Reset` (destroys controllerless or player-controlled pawns) and `APawn::OutsideWorldBounds` (destroys AI pawns directly) are overridden — `Reset` does nothing to a Shared Pawn (rounds reset by travel, never by `ResetLevel`), `OutsideWorldBounds` routes to the same teardown path as `FellOutOfWorld` (§9). `DetachFromControllerPendingDestroy` is **not** overridden (it runs on the pawn's own destroy and must keep the controller consistent).
No connection event destroys, re-creates or re-spawns a Shared Pawn (Session rule 22; AC-SCL-42).

### 6. The movement model (`UStealthMovementComponent`)

All values come from the frozen SCL tuning keys (ADR-0004): the server face on the server, the client face for owner prediction. Every rule below is **one function for every pawn**, used on the server for AI-driven and human-driven pawns and on the owner for prediction; the shaping maths lives in a plain-C++ `FStealthLocomotionModel` (no `UWorld`) so it is unit-testable headless (ADR-0008).

- **One input path into the CMC.** Human: the owner's `Move` produces the CMC input vector and the gait bit; the gait travels in the saved move as `FLAG_Custom_0` (`WantsRun`). AI: `UPawnCommandComponent::Move` on the server writes the same input vector and the same `bWantsRun` field. The CMC consumes both identically. `RequestDirectMove` and `RequestPathMove` are overridden to **fault and do nothing**: no path-following component, direct velocity request, `SetActorLocation`, launch or teleport may move a visible pawn (SCL R5.7; ADR-0016 owns the Crowd follower that issues `Move` instead). `bRequestedMoveUseAcceleration` stays true as a backstop.
- **Gaits and pace (R5.1).** One quantiser function maps analog magnitude to the shared pace set with ±0.03 hysteresis, applied **where the input is consumed**: in `ScaleInputAcceleration` on the owning client (human) and on the server (AI). `MoveAutonomous` does not call `ScaleInputAcceleration` (engine gate), so for human moves the server snaps the received acceleration magnitude to the nearest pace-set value (validation, no hysteresis needed because the owner already quantised). The owner applies the 0.2 dead zone before sending; the server applies none. `GetMaxSpeed` returns `WalkSpeedCmPerSec × pace` or `RunSpeedCmPerSec`.
- **Effective-gait clamps (R5.2, R6).** Run → Walk if `IGaitLegalitySource` reports `Informant` or `FActionState.Pose == Brace`; while `Brace` Move is ignored for translation and yaw (planted); while `Strike`, `Counter`, `BushEnter`/`BushExit` (`PoseLocked`) Move is zeroed and the pose's root motion or warp moves the pawn; `RoundEnded`/freeze as §4.5. Informants and planted pawns may not enter the traversal movement mode: `SetMovementMode` refuses it as a backstop (the IIF already rejects the command).
- **Acceleration, braking, turning (R5.3, R5.4, F2, F3).** `GetMaxAcceleration`/`GetMaxBrakingDeceleration` give constant acceleration to gait speed in `WalkStartSec`/`RunStartSec`, one walk deceleration set from the slowest pace (`WalkStopSec`), run braking from `RunStopSec`. `bOrientRotationToMovement = true`, `bUseControllerRotationYaw = false`, `bUseControllerDesiredRotation = false` for every pawn; the acceleration direction turns at most `WalkTurnRateDegPerSec`/`RunTurnRateDegPerSec`; the reversal latch (stop-pivot-start above `ReversalAngleDeg`, hysteresis `ReversalHysteresisDeg`, release on `Pivot` exit) and idle pivot (`PivotExitAngleDeg`, `PivotTurnRateDegPerSec`) and idle turn-to-look (R5.5) live in `PhysicsRotation`/`ComputeOrientToMovementRotation` overrides.
- **Prediction state (spike P1).** The pivot latch, the pace-quantiser hysteresis state, the last commanded input and the starvation flag are movement state: they are saved in `FSavedMove_Stealth` (`SetMoveFor`/`PrepMoveFor`) and restored on client replay; replay-only fields need no custom move-data container (engine gate). Only if a server correction must carry them is a custom response container added. P1 measures owner-vs-server divergence at 100 ms lag (AC-SCL-06 ≤ 1 frame).
- **Commanded vs external displacement (R16).** The component accumulates horizontal displacement while the pawn's own `Move` input is non-zero (`CommandedDisplacementCm(Since)`), separately from displacement under zero input (depenetration, bases, root motion).
- **Input starvation (R15, F12).** On the server, the engine's **forced position updates** (E6; not ADR-0003's forced net updates) are made to brake rather than glide: `UStealthMovementComponent::ForcePositionUpdate` zeroes the input acceleration and then calls `Super`, so a starved pawn brakes by the baseline into idle while its position keeps replicating. The engine reads its interval, duration and the movement-time discrepancy switches from the `AGameNetworkManager` **class default object** (E6), which is process-wide (shared by every PIE world in an ADR-0008 multi-world test) and read before any tuning could be applied, so the values are **pinned in `Config/DefaultGame.ini`** `[/Script/Engine.GameNetworkManager]`: `MAXCLIENTUPDATEINTERVAL = InputStarvationSec` (0.25), `MaxClientForcedUpdateDuration ≥ InputStarvationSec + max(WalkStopSec, RunStopSec) + 0.25 s` (≥ 1.25 s at defaults), `bMovementTimeDiscrepancyDetection = true`, `bMovementTimeDiscrepancyResolution = true`, and `MaxMoveDeltaTime ≤ MAXCLIENTUPDATEINTERVAL`. That `.ini` edit is outside this pipeline's write scope (pending user item). The SCL tuning keys stay the design source; a boot test asserts the CDO equals them and fails otherwise, and the TDC invariants carry both inequalities (change request under Related). The engine steps a starved pawn's server position once per interval (E6, about 4 Hz); whether that reads as a stepped brake at 15 Hz is **spike ST1**, whose fallback drives the zero-input forced position update every server frame while starved (with the engine's forced-update bookkeeping mirrored). The owner mirrors starvation (no server acknowledgement for `InputStarvationSec + 0.05 s` → zero its own input). On resume, late moves are consumed with the discrepancy detection and resolution on, and the server caps observed speed at `StarvationCatchUpMaxMult ×` gait speed in `MoveAutonomous`, discarding excess late moves (no catch-up sprint). Server hitches suppress forced position updates for every pawn alike (E6).

### 7. Traversal (SCL R7; provisional pending the traversal spike)

- **Server-authoritative start through the queue.** `Traverse(Hint?)` is a discrete command (ADR-0011) resolved in a queue slot, so two pawns contending for one landing cell are ordered by key. The resolver calls the pawn's `TryStartTraversal`, which re-runs F5 eligibility with the server's own query (never trusting the hint, body facing only) **only after** the server has consumed every saved move whose timestamp is ≤ the command's client timestamp (`ServerData->CurrentClientTimeStamp`); if not yet consumed, the item is re-submitted to the next drain (bounded by input starvation: a forced position update advances the timestamp). AI commands carry no client timestamp and validate immediately.
- **One replicated start record.** On success the server writes `TraversalState {Seq (uint16), TypeId (uint8), PointId (uint32)}` (fixed width, ADR-0003 §4) inside `FStealthForcedNetUpdate(TraversalStateWrite)`, sets the motion-warp targets from the coping record, switches to the traversal movement mode and plays the traversal montage. Every client derives the warp targets locally from `PointId` (public geometry, LSC) and starts the same anticipation lead-in and clip from `OnRep_TraversalState`, offset by `EstimatedServerNow` (ADR-0002) so a late arrival starts at the elapsed position and a clip already finished is skipped.
- **The sample's traversal replication is replaced.** `Traversal_ServerImplementation` and the replicated `TraversalResult` (E10) are removed from the reparented Blueprint; the sample's traversal *query and clip selection* (chooser, motion warping) are kept and fed from the server's validated record. The movement mode during traversal is the sample's (`MOVE_Flying` + root motion + warping) and identical for every pawn. Proxies either play root motion from `RepRootMotion` (E9) or ignore it and follow replicated movement (SCL R7.6 fallback) — **one** choice for every pawn, made by the spike (E12).
- **Owner anticipation.** The owner starts the `TraversalAnticipationSec` lead-in locally at the press and cancels it on the IIF's owner-only rejection reply; observers start it at the server's validation, so a predicted start and an AI start look the same.
- `IsInTraversal` is true from validation to landing; collision with other pawns is kept; Move input is buffered and applied on landing.

### 8. Published server state (SCL R19, R16)

On the server, for every pawn, `UStealthMovementComponent` exposes `IsRunning()` (F4 with hysteresis, from commanded-locomotion horizontal velocity, forced false in traversal, `PoseLocked`, falling or `Disabled`, initialised false at spawn and at every hand-over), `IsInTraversal()` and `CommandedDisplacementCm(Since)`. They are not replicated (observers see the movement itself); the owner computes the same values locally for prompt legality only. The IIF reads them through `ISharedPawnStateView` (Core header, ADR-0011).

### 9. Failure hooks

- **Fell out of world / outside world bounds.** KillZ sits ≥ 10 m below the lowest kit geometry; `AStealthPawn::FellOutOfWorld` and `OutsideWorldBounds` request teardown through ADR-0002 (`RequestTeardown`) identically for every pawn; the Roster's `ClearBinding` at `EndPlay` raises `OnBindingLost(PawnDestroyed)` for Round's `PawnLost` recovery (Round R17.9); an unbound pawn is replaced by Crowd.
- **EndPlay.** `AStealthPawn::EndPlay` calls `Roster.ClearBinding(this, Reason)` on the server and nothing else kind-dependent.

### Architecture Diagram

```
                       server (listen host)
 Round (round start) ─┐                     ┌─ Penalty (informant respawn, in slot)
 Crowd (replacement,  ├─► UStealthPawnFactory┤
   PawnRecovery)     ─┘   SpawnSharedPawn    └─► TakeOverCrowdPawn: eligible? → Roster.BindPawn
                          │ deferred spawn → ApplyAppearance → [BindPawn] → FinishSpawning → possess
                          │                                       → ICrowdControl.ReleaseControl
 Round policy (slot) ────►│ RequestHandOver ──► ControlEpoch++ ──► pending
                          ▼                                         │ next frame
                FPawnHandoverTickFunction (TG_PrePhysics) ◄─────────┘
                  end holds (IIF) → FScopedPossessionParity{ Possess } → ControlRotation := HeadLook
                  → last-command hold (≤ HandoverHoldSec; AI→player: PerformMovement with held input until 1st ServerMove)
 AStealthPlayerController::PawnLeavingGame ─► unpossess → AStealthCrowdController possesses (hold only)

 AStealthPawn : ACharacter (one class)          UStealthMovementComponent (one model)
   Restart(): no stop, no mode reset             Move/Look (human: saved move + FLAG_Custom_0; AI: same fields)
   PawnClientRestart(): keeps velocity           pace quantiser · gait clamps (IGaitLegalitySource) · shaping
   Reset: no destroy; OutsideWorldBounds: teardown RequestDirectMove/RequestPathMove: fault, no-op
   [ADR-0003] ForceNetUpdate scoped · PreReplication pitch · IsNetRelevantFor hook
   TraversalState {Seq, TypeId, PointId} ──► clients: warp from PointId, clip from EstimatedServerNow
   server-only: IsRunning · IsInTraversal · CommandedDisplacementCm ──► IIF (ISharedPawnStateView)
```

### Key Interfaces

```cpp
// --- StealthGameplay/Public/Server/Core/: the factory (server-only, ADR-0007 guard) -------------
enum class EControlKind : uint8 { Ai, Player };
enum class ETakeoverResult : uint8 { Applied, NotEligible, BindRejected, StaleEpoch, NotAuthority };
struct FSpawnResult { AStealthPawn* Pawn = nullptr; EBindResult Bind = EBindResult::Applied;
                     bool bAppearanceRejected = false; };   // ApplyAppearance returned false (defect)

UCLASS(NotBlueprintType, NotBlueprintable)
class UStealthPawnFactory : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** Only creator of a Shared Pawn. Callers: Round (round start), Crowd (replacement, PawnRecovery). */
    FSpawnResult SpawnSharedPawn(const FTransform& Transform, FAppearanceId AppearanceId,
                                 EControlKind ControlKind, TOptional<FParticipantId> Participant,
                                 FEpochToken ExpectedEpoch);
    /** Informant respawn (Penalty only, inside a slot): eligibility + BindPawn + release + scheduled hand-over. */
    ETakeoverResult TakeOverCrowdPawn(AStealthPawn* Pawn, FParticipantId Participant, FEpochToken ExpectedEpoch);
    /** Commit a possession change in a slot; executed by the hand-over pass at the next frame start. */
    bool RequestHandOver(AStealthPawn* Pawn, FControlTarget Target, FEpochToken ExpectedEpoch);
    /** Side-effect-free eligibility pre-check, same predicate as TakeOverCrowdPawn step 1 (ADR-0019). */
    ETakeoverResult CheckTakeOverEligibility(const AStealthPawn* Pawn) const;
    /** Commit a body release in a slot (ADR-0018 BodyRelease); executed by the hand-over pass before possessions. */
    bool ReleaseBody(AStealthPawn* Pawn);
    /** Round-end step (ADR-0002 §8.4 f): kind-blind baseline stop for every live pawn. */
    void StopAllForRoundEnd();
    uint32 GetControlEpoch(const AStealthPawn* Pawn) const;   // server-only; IIF stamps commands
    void   MarkControlChange(AStealthPawn* Pawn);              // in a slot: drive change without re-possession
    bool   IsHandOverPending(const AStealthPawn* Pawn) const;
};

// --- StealthGameplay/Public/View/Core/: the pawn (client-safe; forward-declares UPawnCommandComponent) --
UCLASS()
class AStealthPawn : public ACharacter
{
    GENERATED_BODY()
public:
    AStealthPawn(const FObjectInitializer& ObjectInitializer);   // UStealthMovementComponent subobject
    virtual void Restart() override;                 // RecalculateBaseEyeHeight only (E2)
    virtual void PawnClientRestart() override;       // ResetPredictionData_Client, keep velocity (E2)
    virtual void SpawnDefaultController() override;  // no-op: factory possesses
    virtual void Reset() override;                   // never destroys a Shared Pawn (engine gate)
    virtual void OutsideWorldBounds() override;      // same teardown path as FellOutOfWorld
    virtual void FellOutOfWorld(const UDamageType& DamageType) override;  // RequestTeardown (ADR-0002)
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;     // Roster.ClearBinding
    // ADR-0003 overrides live here too: ForceNetUpdate (scoped), PreReplication (pitch), IsNetRelevantFor.
    bool ApplyAppearance(FAppearanceId Id);          // ADR-0012; before FinishSpawning only; false = refused (255, unknown id, rewrite)
protected:
    UPROPERTY(ReplicatedUsing = OnRep_TraversalState) FTraversalState TraversalState;  // fixed width
    UFUNCTION() void OnRep_TraversalState();
};
USTRUCT() struct FTraversalState { GENERATED_BODY()
    UPROPERTY() uint16 Seq = 0; UPROPERTY() uint8 TypeId = 0; UPROPERTY() uint32 PointId = 0; };

// --- StealthGameplay/Public/View/Core/: movement (client-safe; server queries reach the IIF through a Private adapter) --
class IGaitLegalitySource    // injected by the composition root; server: Roster; owner: own C-TeamView
{ public: virtual EParticipantState GetLegalityClass(const APawn* Pawn) const = 0; };

UCLASS()
class UStealthMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    virtual float GetMaxSpeed() const override;
    virtual float GetMaxAcceleration() const override;
    virtual float GetMaxBrakingDeceleration() const override;
    virtual FVector ScaleInputAcceleration(const FVector& InputAcceleration) const override;
    virtual void PhysicsRotation(float DeltaTime) override;
    virtual void SetMovementMode(EMovementMode NewMode, uint8 NewCustomMode = 0) override;  // traversal backstop
    virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override; // fault, no-op
    virtual void RequestPathMove(const FVector& MoveInput) override;                         // fault, no-op
    virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 Flags, const FVector& NewAccel) override;
    virtual bool ForcePositionUpdate(float DeltaTime) override;   // starvation brake (zero input); never the hand-over hold
    virtual void UpdateFromCompressedFlags(uint8 Flags) override; // FLAG_Custom_0 = WantsRun
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override; // FSavedMove_Stealth
    // Published server state (not replicated):
    bool  IsRunning() const;  bool IsInTraversal() const;
    float CommandedDisplacementCm(double SinceServerTime) const;
    void  SetRoundFreeze(bool bFrozen);   // StopAllForRoundEnd / Disabled
};
class FStealthLocomotionModel { /* plain C++: pace quantiser, gait clamp, F2 accel/brake, F3 turn,
                                   reversal latch, F4 IsRunning, F12 catch-up cap — headless-testable */ };
```

### Implementation Guidelines

- The Shared Pawn must use `UStealthMovementComponent` (a CMC subclass); no project code, Blueprint or asset may reference a Mover, ChaosMover or NetworkPrediction type.
- Every pawn must be created by `UStealthPawnFactory::SpawnSharedPawn`; `RestartPlayer`, `SpawnDefaultPawnFor`, `SpawnDefaultPawnAtTransform`, `SpawnActor` of a pawn class outside the factory and `SpawnDefaultController` must never create or possess a pawn.
- The spawn sequence must be deferred spawn → `ApplyAppearance` → `BindPawn` (if a participant) → `FinishSpawning` → possess, in one call stack; a bind rejection must destroy the pawn before `FinishSpawning`.
- Every spawn must set `Instigator == Self` and must spawn no replicated child actor.
- `TakeOverCrowdPawn` must check eligibility and call `BindPawn` itself; callers must not call `BindPawn` for a takeover and must commit no card move unless it returned `Applied`.
- Every `AStealthPawn` must carry the same default subobjects, including a non-replicated `UGameplayTasksComponent` and `UStealthMotionWarpingComponent` with `WarpTargets` replication disabled; possession must never add a component.
- `Possess`/`UnPossess` on a Shared Pawn must be called only by the factory (spawn, hand-over pass incl. `ReleaseBody`, takeover) and `AStealthPlayerController::PawnLeavingGame`; every call must go through `FScopedPossessionParity`.
- `AStealthPawn::Restart` must never call `StopMovementImmediately`, `SetDefaultMovementMode` or `Super::Restart`; `PawnClientRestart` must never zero velocity.
- After every possession the control rotation must be set from the pawn's `HeadLook` world rotation, never from the actor rotation.
- Project code must never set `bOnlyAllowAutonomousTickPose` or otherwise change server pose ticking per controller; the engine's role-driven behaviour is left intact and observer parity is tested.
- A possession change of a live pawn must be committed in a queue slot, bump the control epoch, wait for landing if `IsInTraversal`, and execute in the hand-over pass at the next frame start; the last-command hold must cover every frame until the new controller's first command or `HandoverHoldSec`.
- `PawnLeavingGame` must never destroy the pawn; it must hand it to an `AStealthCrowdController` in the same frame without issuing a command or changing the binding.
- Movement shaping must be one function per rule for every pawn, read only tuning values, and run identically on the server and in owner prediction; no rule may read controller kind, PlayerState or team.
- `RequestDirectMove`/`RequestPathMove` must never move a pawn; AI movement must enter only through `UPawnCommandComponent::Move`.
- The participant-state input to the gait clamp must come only from `IGaitLegalitySource` (server Roster, owner's own C-TeamView); the pawn must never read a PlayerState for it.
- Starvation must zero acceleration in `ForcePositionUpdate`; the `AGameNetworkManager` forced-position-update and discrepancy values must come from `Config/DefaultGame.ini` and equal the SCL tuning keys (boot test); excess late moves must be discarded, never replayed as a catch-up.
- The AI → player hold must simulate with `PerformMovement` and must never advance the CMC's client timestamps or call `ForcePositionUpdate`.
- Shaping state used by prediction must be saved and restored in `FSavedMove_Stealth`.
- Traversal must start only from a server-validated queue slot, write `TraversalState` inside the `TraversalStateWrite` forced-update scope, and must never use the sample's `Traversal_ServerImplementation`/`TraversalResult` replication or a multicast; one proxy root-motion policy must apply to every pawn.
- `IsRunning`, `IsInTraversal` and `CommandedDisplacementCm` must stay server-side (owner-local copies for prompts only) and must never be replicated.
- No pawn may be destroyed inside a resolver except through `RequestTeardown` (ADR-0002); `FellOutOfWorld` and `OutsideWorldBounds` must use it; `Reset` must never destroy a Shared Pawn.
- `PawnLeavingGame` must only unpossess during world teardown or seamless travel, and must not bump the control epoch.
- Prototype code (`AStealthCharacter`, `UStealthMovementComponent`, `AStealthNpcController`, `AStealthGameMode` spawn paths) must not be promoted until this ADR is Accepted; its `IsInformant()`-from-PlayerState clamp, `bUseAccelerationForPaths` path following, `RestartPlayer` round start and post-possession appearance draw must be replaced, not ported.

## Alternatives Considered

### Alternative 1: Mover 2.0 / ChaosMover instead of the CMC
- **Description**: Build the Shared Pawn on the Mover plugin (already enabled by the sample) with NetworkPrediction or ChaosMover for networked prediction, using Mover's layered moves for traversal.
- **Pros**: Modern, modular movement modes; rollback-style prediction; Epic's stated long-term direction; one simulation model for players and AI by construction.
- **Cons**: Experimental in 5.8 with an API still moving (PhysicsMover removed in 5.8, E11); the Game Animation Sample motion-matching and traversal content used by the project is CMC-based; every replication parity fact (ADR-0001, ADR-0003) and possession fact here was verified against the CMC and would have to be re-proved; Iris/legacy interactions of NetworkPrediction are unverified; 1-2 programmers cannot absorb an Experimental framework's churn.
- **Rejection Reason**: Risk without a Pillar-1 benefit the CMC cannot also deliver. Recorded as a later superseding ADR candidate once Mover leaves Experimental.

### Alternative 2: Keep the engine's possession behaviour and mask it
- **Description**: Let `Restart()` stop the pawn and reset control rotation, and hide it with an animation blend or by re-applying the previous velocity a frame later.
- **Pros**: No engine overrides.
- **Cons**: The server state (velocity zero, movement mode reset) replicates to every observer before any mask runs; the dead stop is exactly the hand-over tell Round R17 and SCL R4 forbid; a re-applied velocity a frame later is a visible hitch at 15 Hz.
- **Rejection Reason**: Masks cannot hide authoritative state. Overriding the four virtuals (E1, E2) is smaller and testable.

### Alternative 3: Destroy and re-spawn on disconnect / takeover
- **Description**: On disconnect destroy the player's pawn (engine default, E3) and spawn an NPC replacement; on informant respawn spawn a fresh pawn near the team spawn.
- **Pros**: Simplest possession model; no hand-over code.
- **Cons**: A pawn vanishing at a disconnect and one appearing mid-round each identify a human (Session rule 22, Roster R9, decided rule C1); new NetGUIDs and channel opens are packet-level tells (IVP).
- **Rejection Reason**: Violates locked rules and Pillar 1.

### Alternative 4: AI path-following drives the CMC directly (the prototype)
- **Description**: Keep `AAIController::MoveTo` and the path-following component with `bUseAccelerationForPaths`, so NPCs accelerate through the CMC like players.
- **Pros**: Engine-standard AI movement, little code; the prototype already shows plausible motion.
- **Cons**: AI input would bypass the IIF (ADR-0001 §2), so NPC pace quantisation, gait clamps, last-command hold, token buckets and commanded displacement would differ from humans; requested velocity is not a `Move` command; path-following corner behaviour differs from human stick input.
- **Rejection Reason**: Breaks the one command path. `RequestDirectMove`/`RequestPathMove` become fault-and-no-op backstops; ADR-0016 supplies a follower that issues `Move`.

### Alternative 5: Execute hand-overs inline in the queue slot
- **Description**: Possess and unpossess inside the resolver that decides the hand-over (TG_PostPhysics), instead of a next-frame pass.
- **Pros**: One frame earlier; no extra tick function.
- **Cons**: The CMC has already simulated the pawn this frame under the old controller; possession in the middle of the drain interleaves engine possession side effects (ClientRestart, prediction resets) with later slots that re-validate against the pawn; SCL R4 and Round R17 both specify the frame start; traversal deferral needs a place to wait anyway.
- **Rejection Reason**: The frame-start pass gives one well-defined point for every possession change, satisfies both GDDs, and keeps the drain free of engine possession side effects.

### Alternative 6: Keep the sample's traversal replication
- **Description**: Use the sample's `Traversal_ServerImplementation` RPC and `OnRep_TraversalResult` as shipped.
- **Pros**: Already works in the prototype; no Blueprint surgery.
- **Cons**: Starts traversal outside the queue (two pawns could both claim a landing cell); unverified under loss (first-playable known issue 4); its fields are not fixed-width or on the allowlist; the start is not keyed to server time for late joiners or re-opened actors (ADR-0003 §5.8).
- **Rejection Reason**: Replaced by `TraversalState`; the sample's query, chooser and warping are kept.

## Consequences

### Positive
- Pillar 1 holds through every possession path: spawn, takeover, disconnect, rejoin and round-end stops look like ordinary crowd motion, because the engine paths that stop, destroy or reset a body are overridden at the class, not masked.
- One creator and one possession point make the bind-before-visible guarantee and the "no mid-round participant pawn" rule enforceable by test (AC-SCL-02, AC-SCL-38, AC-SCL-42).
- The CMC keeps the Game Animation Sample's motion matching and traversal content and the engine facts already verified for replication parity.
- Movement maths in a plain-C++ model is unit-testable headless; the same functions serve server, AI and owner prediction.
- Starvation uses the engine's own forced-position-update path, so no parallel server simulation exists for starved pawns.

### Negative
- Engine virtuals are overridden (`Restart`, `PawnClientRestart`, `Reset`, `OutsideWorldBounds`, `PawnLeavingGame`, CMC `ForcePositionUpdate`/`MoveAutonomous`); an engine upgrade must re-check each (listed in Validation Criteria).
- A next-frame hand-over pass adds one frame of latency to every possession change (covered by the last-command hold).
- Custom saved moves and possibly a custom move-data container add CMC networking code the team must own (P1).
- Starvation depends on `AGameNetworkManager` values pinned in `DefaultGame.ini`, a project-config edit the pipeline cannot make (pending user item); until it is made the engine defaults (0.25 s / 1.0 s) apply, which match `InputStarvationSec` but give a duration shorter than the invariant requires for a running pawn.
- The sample Blueprint must be edited to remove its traversal replication, a binary-asset change that needs the snapshot workflow until ADR-0009 is decided.

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| H1: the AI → player gap shows a stop, dropped first moves or a large owner correction | Medium | High (hand-over tell) | Hold simulation with `PerformMovement` and untouched client timestamps until the first move; spike H1 under `LeakNetProfile`; fallback: keep the crowd controller driving until the client acknowledges the pawn, then possess |
| Overriding `Restart` skips engine setup a later engine version adds there | Low | Medium | Validation lists the four overrides for re-check on every engine upgrade; development-build parity wrapper faults on any velocity/mode/HeadLook change |
| Engine role-driven server pose ticking (E4) makes a remotely controlled pawn's root-motion poses differ observably from an AI pawn's | Low | High | Traversal spike and AC-SCL-13/14 compare observer traces; if a difference shows, ADR-0013 picks a uniform pose-timing fix that keeps montages in step with `ServerMove` (never a per-kind setting in project code) |
| `PawnLeavingGame` runs during controller destruction in a state where spawning and possessing an AI controller misbehaves (S8) | Medium | High | S8 on a real multi-process connection close (ADR-0008 T3); fallback: unpossess only in `PawnLeavingGame` and possess from a hand-over pass scheduled for the same frame's end |
| Starvation forced position updates are suppressed after a server hitch, or step at about 4 Hz (E6), so a starved pawn freezes or brakes in steps | Medium | Medium | Spike ST1 (fallback: per-frame zero-input update while starved); identical for every pawn; logged; AC-SCL-23 runs under emulation without hitches and once with an injected hitch |
| Shaping state not restored on replay causes owner jitter (P1) | Medium | Medium | Saved-move fields first; custom move-data container if corrections must carry them |
| Spawning in `TG_PostPhysics` misbehaves (ADR-0002 E4) | Low | Medium | Spawns move to the post-drain pass with the sequence intact |
| The sample's `IsLocallyControlled` uses include an observable branch | Low | High | Audit both uses before promotion; AC-SCL-01 static scan |
| Disabling `Mover`/`NetworkPrediction` breaks a sample asset that references them | Low | Low | Text scan found none (weak); re-check by loading every sample map after the `.uproject` edit |

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| shared-character-locomotion.md | R1 one class; R17 one capsule/collision/physics asset (TR-scl-001, TR-scl-013) | §2: one `AStealthPawn` + one Blueprint child, one crowd controller class, one body |
| shared-character-locomotion.md | R2 one spawn factory, bind-before-visible sequence; R2a crowd takeover (TR-scl-001) | §3.1, §3.2: `SpawnSharedPawn`, `TakeOverCrowdPawn` with atomic eligibility + bind |
| shared-character-locomotion.md | R2b disconnect never destroys a pawn (TR-scl-002) | §5: `PawnLeavingGame` override, same-frame crowd controller |
| shared-character-locomotion.md | R3 possession parity; R4 stop-and-blend hand-over, last-command hold (TR-scl-003, execution half) | §4.1-§4.3: `Restart`/`PawnClientRestart` overrides, cached last input, parity wrapper, hand-over pass; ADR-0003 owns the forced-update deferral |
| shared-character-locomotion.md | R5, R6 speed tiers, shaping, clamps, restrictions; R16 commanded displacement (TR-scl-004) | §6: `UStealthMovementComponent` + `FStealthLocomotionModel`, `IGaitLegalitySource` |
| shared-character-locomotion.md | R7 traversal set, server-authoritative start, `TraversalState` (TR-scl-004) | §7 (provisional pending the traversal spike) |
| shared-character-locomotion.md | R14/R15 smoothing parity and input starvation (TR-scl-010) | §6 starvation via engine forced position updates (smoothing values are ADR-0003 §3) |
| shared-character-locomotion.md | R19 published state (TR-scl-012) | §8 |
| participant-state-roster.md | R9 one named spawn site, crowd adoption, `PawnLeavingGame` hook (TR-roster-004) | §3, §5 |
| participant-state-roster.md | R10 server-only O(1) queries; never on clients (TR-roster-005) | §2 `IGaitLegalitySource`: server calls Roster, owner reads its own C-TeamView, proxies never ask |
| participant-state-roster.md | R11 hand-over on disconnect; reclaim repossesses the bound pawn | §4.3, §5 |
| round-match-framework.md | R17 hand-over committed in the drain, control epoch bump, next-frame effect, never during traversal; R17.9 own-card `PawnRecovery` | §4.3, §4.4; §3.1 (Crowd's replacement path with `Participant` set) |
| input-interaction-framework.md | R3 human Move/Look through CMC saved moves, AI through the same entry points (TR-iif-002, execution half) | §6 one input path; ADR-0011 owns the command half |

## Performance Implications
- **CPU**: The movement model adds a few pure functions per CMC tick per pawn (pace quantiser, clamp, turn cap); expected well under 5 % of the 3.0 ms CMC game-thread line (architecture CPU table: 40 server-simulated NPCs + proxies). The hand-over pass touches only pawns with a pending change. Forced updates for starved pawns are the engine's own cost.
- **Memory**: One factory record per pawn (control epoch, hand-over state, creation token): bytes per pawn.
- **Load Time**: The pawn Blueprint class is resolved once before `OnTuningReady` completes; no per-spawn load.
- **Network**: `TraversalState` (7 bytes when it changes, forced net update) per traversal; no other new replicated field. Possession adds nothing visible to non-owners (ADR-0001/0003). Within the ≤ 30 KB/s pawn line (SCL F9), which ADR-0003's capture measures.

## Migration Plan
The prototype is evidence, not a base (ADR-0007 promotion rule). When this ADR is Accepted: create `AStealthPawn`, `UStealthMovementComponent`, `UStealthPawnFactory`, `AStealthCrowdController` in `StealthGameplay/Core`; reparent the sample character Blueprint from `AStealthCharacter` to `AStealthPawn` with a CoreRedirect (ADR-0007), using the `_snapshots/` workflow until ADR-0009 is decided; remove the sample's traversal RPC/OnRep from the reparented Blueprint; delete nothing from the prototype without user approval (the unused `BP_StealthCharacter` duplicate stays a pending user item). Replace, not port: `IsInformant()`/`GetTeam()`/`IsNpc()` on the pawn, `bUseAccelerationForPaths` wandering, `RestartPlayer` round start, `SetAppearanceIndex` after possession, the informant `GetMaxSpeed` cap from `UStealthSettings`. The `.uproject` plugin edit (disable `Mover`, `NetworkPrediction`) is applied by the user (pending item AQ-09).

## Validation Criteria
- AC-SCL-01, 02, 03, 04, 05, 06, 07, 08, 10, 11, 12, 13, 14, 15, 16, 23, 24, 25, 38, 40, 42, 44, 45 pass (tests `StealthGame.Locomotion.*`, ADR-0008 layout); AC-SCL-17 with S1/S14.
- Spikes report PASS: traversal under `LeakNetProfile`, S8, S14, H1, P1, ADR-0002 E4.
- Parity wrapper: 0 velocity/mode/HeadLook differences across 1,000 scripted possessions (spawn, takeover, disconnect, rejoin, round-end stop) in development builds.
- Boot test: `AGameNetworkManager` CDO forced-position-update and discrepancy values equal the SCL tuning keys; no Mover/NetworkPrediction type is loaded; no `AStealthPawn` exists without a factory creation token.
- Engine-upgrade checklist: re-read `APawn::Restart`/`PawnClientRestart`/`Reset`/`OutsideWorldBounds`/`UnPossessed`, `ACharacter::Restart`/`PawnClientRestart`/`UpdateMeshAllowAutonomousTickPose`, `AController::OnPossess`, `APlayerController::OnPossess`/`ServerAcknowledgePossession`/`PawnLeavingGame`/`Destroyed`/`OnNetCleanup`, the server forced-position-update block in `APlayerController`'s tick, and `UCharacterMovementComponent::ForcePositionUpdate`/`MoveAutonomous`/`VerifyClientTimeStamp` on every engine version change.

## Related
- **Depends on**: ADR-0001 (identity invariants), ADR-0002 (queue, teardown, round-end step, E4 spike), ADR-0003 (net parity mechanisms on this class), ADR-0004 (SCL keys), ADR-0005 (no default spawn, PlayerController lifecycle), ADR-0007 (module placement), ADR-0008 (tests and harness).
- **Enables**: ADR-0011 (IIF), ADR-0012 (appearance apply slot), ADR-0013 (animation), ADR-0016 (Crowd follower and `ICrowdControl`), ADR-0017 (concealment warp/collision hooks), ADR-0018 (`Disabled` at the death write), ADR-0019 (informant takeover).
- **GDD change requests raised (not applied; recorded for the GDD owners)**:
  1. **Participant State & Roster R9 and Round R18**: Penalty calls `TakeOverCrowdPawn`, which performs the eligibility check and `BindPawn`; Penalty does not call `BindPawn` itself for a takeover, and no card move is committed for a `NotEligible` result (SCL R2a already has the factory binding).
  2. **Shared Character & Locomotion R2a.2**: "one server frame" becomes "committed in one queue slot, possession executed by the hand-over pass at the next frame start" (consistent with R4 and Round R17).
  3. **Tuning Data & Config**: invariants `MaxClientForcedUpdateDuration ≥ InputStarvationSec + max(WalkStopSec, RunStopSec) + 0.25 s` and `MaxMoveDeltaTime ≤ MAXCLIENTUPDATEINTERVAL = InputStarvationSec`; the engine values are pinned in `DefaultGame.ini` and a boot test asserts they equal the SCL keys (§6).
  4. **Shared Character & Locomotion R11.5**: "always-tick, identical setting for all pawns" is scoped to the observable result (every pawn's pose, montages, root motion and notifies advance on the server whether rendered or not); the engine's role-driven tick timing for remotely controlled pawns is not changed (E4).
  5. **Shared Character & Locomotion R5.7**: the path-following override becomes a fault-and-no-op backstop; Crowd's follower (ADR-0016) issues `Move`.
- **ADR amendment request (not applied; ADR-0002 belongs to another author)**: ADR-0002 §5 should list the hand-over pass (§4.3) and the `PawnLeavingGame` possession plumbing (§5) as named apply-only exceptions next to the round-end step.
- Design docs: `design/gdd/shared-character-locomotion.md`, `participant-state-roster.md`, `round-match-framework.md`, `input-interaction-framework.md`, `npc-kill-penalty-informants.md`; `docs/architecture/architecture.md`.

## Validation gates
- **TD-ADR (technical-director), 2026-10-24: CONCERNS.** (1) Hand-over pass vs ADR-0002's single named exception → §4.3 states it is apply-only; amendment request to ADR-0002 under Related; commands between commit and possession void by epoch. (2) `PawnLeavingGame` open points → §5: no epoch bump, teardown guard, R17.2 reduces to `DriveBoundPawn`, cached last input (§4.2). (3) `TakeOverCrowdPawn` owning the bind endorsed → BoundLive-but-AI window stated (§3.2); GDD change request 1 kept. (4) Run-time CDO write rejected → config pin in `DefaultGame.ini` (pending user item), tuning keys mirrored and boot-tested, `MaxMoveDeltaTime` invariant, "forced position update" terminology. (5) Scope gaps → Unresponsive path and participant-bound AI cross-reference (§4.3), `Reset`/`OutsideWorldBounds` (§5, §9), upgrade checklist extended. Resolution: Revise flagged items (autonomous; logged).
- **Engine specialist (unreal-specialist lens), 2026-10-24: BLOCKING → revised.** Blocking: E7 was partly wrong (the PC tick sets a zero `ServerTimeStamp` to now) and calling `ForcePositionUpdate` for the AI → player hold advances `CurrentClientTimeStamp` (owner's first moves rejected) and asserts on the host's pawn → §4.3 now simulates with `PerformMovement` under a factory flag without touching timestamps (spike H1). Notes applied: E1/E2/E3/E6 confirmed and extended; forcing `bOnlyAllowAutonomousTickPose` false is unsafe (non-virtual updater, double-advanced root motion) → dropped, SCL R11.5 scoped to observable results (change request); `DetachFromControllerPendingDestroy` runs on the pawn's own destroy → override dropped, `Reset`/`OutsideWorldBounds` overridden instead; `MoveAutonomous` skips `ScaleInputAcceleration` → server snaps pace; replay-only saved-move fields need no custom container; starvation steps at ~4 Hz → spike ST1. Unverified: spawning a controller during connection cleanup (S8); `GetOwnerAdapter` not re-read.
- **GDD sync check:** checked 4 referenced GDDs (shared-character-locomotion, participant-state-roster, round-match-framework, input-interaction-framework). Naming/ownership inconsistencies found and recorded as change requests under Related (not applied in this pass): Roster R9 / Round R18 name Penalty as the takeover `BindPawn` caller (this ADR: `TakeOverCrowdPawn` binds); SCL R2a.2 "one server frame" (this ADR: slot commit + next-frame possession); SCL R5.7 path-following override (this ADR: fault-and-no-op backstop); SCL R11.5 always-tick wording.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §2, guidelines: non-replicated `UGameplayTasksComponent` default subobject on every pawn — requested by ADR-0016.
- §3.1, Key Interfaces: `ApplyAppearance` returns `bool`; factory destroys the deferred actor on `false` — requested by ADR-0012 (sync item 7).
- §3.2b, Key Interfaces: `ReleaseBody(AStealthPawn*)` executed apply-only by the hand-over pass; §2: `UStealthMotionWarpingComponent` (no replicated `WarpTargets` on any pawn) with traversal coordination — requested by ADR-0018 (items 2, 6).
- §3.2a, Key Interfaces: `CheckTakeOverEligibility` — requested by ADR-0019 (item 1).
- §4.3: later request replaces a pending one; invalid `Player` target dropped; body releases before possessions; root-motion action poses defer a hand-over like a traversal — requested by ADR-0019 (item 6).
- View target on factory-initiated unpossess: mechanism is ADR-0015's `SetViewTarget` override plus the `OnUnPossess` skip (§3.2b) — requested by ADR-0018 (item 6) / ADR-0019 (item 7).

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- Engine Compatibility, Knowledge Risk: MotionWarping is Beta in 5.8.4; the traversal warp and the protected `WarpTargets` disable are on the engine-upgrade checklist — architecture-review fix 2026-10-25 (Engine finding 3).
- Key Interfaces: header folders follow ADR-0007 rule (a): the factory sits in `Public/Server/Core/`, and the pawn and movement headers in `Public/View/Core/`. `ISharedPawnStateView` (ADR-0011, now `Public/Server/Core/`) is implemented by a server-only adapter in `Private/`, so no View header includes a Server header — architecture-review fix 2026-10-25 (Conflict 4, same rule applied here).
- Requests answered (2026-10-24): the ADR-0002 §5 amendment request under Related (hand-over pass and `PawnLeavingGame` plumbing as named apply-only exceptions) is applied in ADR-0002 §5 — architecture-review fix 2026-10-25 (Conflict 9).
