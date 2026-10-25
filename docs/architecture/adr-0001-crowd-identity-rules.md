# ADR-0001: Crowd identity rules

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. By the architecture's bootstrap order it cannot be Accepted before ADR-0009 (version control, a user decision), ADR-0007 and ADR-0008 are Accepted and spikes S1, S2 and S14 have reported (S7 and S11 too, only if Iris is ever considered).

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims re-read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; design against the 18 MVP GDDs in their "In Review, reconciled 2026-10-24" state)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full) on the user's mandated content for ADR-001; technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (PlayerState `Owner` owner-only for rejoin `SetOwner`; `PawnRef` typed `APawn` in Foundation; E12/E13 verified for legacy, §8 triggers reworded; owner-local stale-pointer cleanup; `T_due` option (c) constraint; UPROPERTY/GENERATED_BODY in the sketch; `PlayerId`/`UniqueId` owner-only instead of disabled). Engine validation (unreal-specialist lens): MINOR NOTES, nothing blocking (by-name reset works on private members; `ClientRestart` is the only owner-side `SetController`, so the §8 fallback adds `OnRep_Pawn`; §4.3 table matches the 5.8.4 set).

## Summary
Pillar 1 ("an enemy cannot tell a player from an NPC") can be broken by a single Unreal Engine default, because by default every player pawn replicates its `PlayerState`, `Controller` and `Owner` to every client. This ADR fixes the identity invariants and the carrier model: one pawn class driven through one command path, significance-only animation LOD, the pawn-to-participant link sent to its owner only, team data delivered only through the owner-only C-TeamView carrier on each PlayerController, the legacy NetDriver with Iris pinned off, and a written fallback if spikes S1 or S14 fail.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Networking (with Core gameplay framework and Animation constraints) |
| **Layer** | Foundation |
| **Knowledge Risk** | HIGH — networking is a HIGH-risk domain for 5.4-5.8 (`docs/engine-reference/unreal/VERSION.md`); Iris status changed in 5.8 |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/networking.md`, `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Module Ownership, Data Flow, API Boundaries, Required ADRs, Open Questions); engine source read this session: `Engine/Private/Pawn.cpp:1303-1312`, `Engine/Private/ActorReplication.cpp:555-569`, `Engine/Classes/GameFramework/Pawn.h:175-211`, `Engine/Classes/GameFramework/Actor.h:836-839`, `Engine/Public/Net/UnrealNetwork.h:405-512`, `Engine/Private/PlayerController.cpp:816-850`, `Engine/Private/Controller.cpp:67, 551-568, 832-834` |
| **Post-Cutoff APIs Used** | `APawn::RemoteViewPitch16` (5.6; 8-bit `RemoteViewPitch` deprecated, `COND_ReplayOnly`) [V `Pawn.cpp:1310-1312`]; the `net.Iris.UseIrisReplication` cvar default 0 [V `IrisConfig.cpp:15-16`, cited by architecture.md]. Everything else used here predates the cutoff but is re-verified in 5.8.4 source (below) |
| **Verification Required** | **S1** raw-bunch parity: `PlayerState`, `Controller`, `Owner` absent from every non-owner changelist for player and NPC pawns alike, under `LeakNetProfile` (PktLag 100, PktLoss 1-2 %). **S2** PlayerState trimming: only `PublicHandle` and `PlayerNamePrivate` reach non-owners; no side effect breaks login, OSS registration or `GameState` lookups. **S14** owner-only conditions do not break possession, `OnRep_Controller`, CMC `ServerMove` routing, camera management or `ClientRestart` across takeover, rejoin and AI hand-over (re-delivery to a new owner is verified in legacy source, E12; S14 confirms it at run time and checks the owner-local cleanup). Harness startup assertion that the game NetDriver runs the legacy replication system. S7/S11 only if Iris is reconsidered (ADR-0024) |

**Engine facts this ADR relies on** ([V] read in the 5.8.4 source this session; [U] unverified, owned by the named spike):

| # | Fact | Tag |
|---|---|---|
| E1 | `APawn::GetLifetimeReplicatedProps` registers `DOREPLIFETIME(APawn, PlayerState)` and `DOREPLIFETIME(APawn, Controller)` with no condition | [V] `Pawn.cpp:1307-1308` |
| E2 | `APawn::PlayerState` is a **private** `UPROPERTY` (`AllowPrivateAccess`); `APawn::Controller` is `replicatedUsing=OnRep_Controller` | [V] `Pawn.h:175-179, 209-211` |
| E3 | `AActor::Owner` (and `Instigator`, `bHidden`, `bTearOff`, `bCanBeDamaged`, `Role`, `RemoteRole`, `bReplicateMovement`) replicate **push-based** through `FDoRepLifetimeParams SharedParams` with no condition | [V] `ActorReplication.cpp:563-569` |
| E4 | `ResetReplicatedLifetimeProperty(ThisClass, PropertyClass, FName, ELifetimeCondition, OutProps)` and the `FDoRepLifetimeParams` overload exist and look the property up by name at run time, so a subclass can reset the condition of a private inherited property (a misspelled name is a fatal error at startup, not a compile error). `RESET_REPLIFETIME_CONDITION` uses `GET_MEMBER_NAME_CHECKED` and so works only for accessible members: `APawn::Controller` and `AActor::Owner` are public, `APawn::PlayerState` is private | [V] `UnrealNetwork.h:455, 488-510`; `UnrealNetwork.cpp:124-183`; `Pawn.h:176-179, 203-211`; `Actor.h:833-839` (TD/engine gate) |
| E5 | `DisableReplicatedLifetimeProperty(ThisClass, PropertyClass, FName, OutProps)` and `DISABLE_REPLICATED_PRIVATE_PROPERTY` exist | [V] `UnrealNetwork.h:407-412, 444` |
| E6 | `AController` sets `bOnlyRelevantToOwner = true`; `AController::PlayerState` and `AController::Pawn` replicate on the controller | [V] `Controller.cpp:67, 832-834` |
| E7 | On the owning client, only `APlayerController::ClientRestart_Implementation` sets `Pawn->Controller` (`GetPawn()->SetController(this)`); `AController::OnRep_Pawn` sets the controller side (`SetPawn`) and clears the old pawn's controller. If `ClientRestart` arrives before the pawn exists on the client, the engine asks the server to resend it | [V] `PlayerController.cpp:829-833, 843-845`; `Controller.cpp:551-568` |
| E8 | Possession paths call `ForceNetUpdate()` | [V] `Pawn.cpp:434, 678, 717` (architecture.md) |
| E9 | `APlayerState` is always relevant; `AGameStateBase::PlayerArray` is not replicated and is rebuilt on each client from received PlayerStates | [V] `PlayerState.cpp:26`, `GameStateBase.h:54-55` (architecture.md) |
| E10 | Iris is off unless `net.Iris.UseIrisReplication=1`; `BaseEngine.ini` marks the game driver `bCanUseIris=true` | [V] `IrisConfig.cpp:15-16`, `BaseEngine.ini:346` (architecture.md) |
| E11 | `AActor::bReplicateUsingRegisteredSubObjectList` and the registered sub-object list exist | [V] `Actor.h:627, 970` (architecture.md) |
| E12 | Legacy driver: each connection's owner status is recomputed every time the actor replicates; when it changes, the condition map is rebuilt and properties that changed while hidden from that connection are sent, so a **new owner receives the current values**; the old owner stops receiving updates but **keeps its last values** (the engine clears only the old pawn's `Controller` on that client) | [V legacy] `DataChannel.cpp:3809-3812`; `RepLayout.cpp:1998-2016, 2111-2115, 2163-2170, 7231-7260`; `Controller.cpp:553-563`. [U] under Iris |
| E13 | The condition-only `ResetReplicatedLifetimeProperty` overload changes only the condition and keeps the push-based flag; the `FDoRepLifetimeParams` overload overwrites the push and notify settings | [V] `UnrealNetwork.cpp:137-157, 185-204` |
| E14 | No engine or sample client path dereferences `APawn::PlayerState` on a simulated proxy in a way that breaks when it is absent (CMC simulated proxies, AnimBP, camera, Enhanced Input). `Controller` is already unresolvable on other clients today because PlayerControllers replicate only to their owner, so the residual risk is `PlayerState`; a text grep of the sample's `.uasset` files found no `GetPlayerState`/`IsPlayerControlled` (weak evidence) | [U] S14 |
| E15 | On the owning client the CMC runs the owner's move and sends `ServerMove` only when `IsLocallyControlled()` is true, which needs `Pawn->Controller` set and local; `APawn::GetNetConnection` checks `Controller` before `Owner` | [V] `CharacterMovementComponent.cpp:1751-1753`; `Pawn.cpp:272-275, 753-761` |
| E16 | `AActor::SetOwner` marks `Owner` dirty (push model), so re-owning a PlayerState to a rejoiner's new controller sends `Owner` to every client unless its condition is reset | [V] `Actor.cpp:2622-2623` |
| E17 | The 5.8.4 `APlayerState::GetLifetimeReplicatedProps` set is exactly `Score`, `PlayerId`, `CompressedPing` (`COND_SkipOwner`), `bIsSpectator`, `bOnlySpectator`, `bIsABot`, `bIsInactive`, `bFromPreviousLevel`, `StartTime`, `UniqueId`, `PlayerNamePrivate`; `Score`, `PlayerId`, `PlayerNamePrivate` and `bShouldUpdateReplicatedPing` are private (`SetShouldUpdateReplicatedPing` exists). On clients `OnRep_UniqueId` registers with the online session and `OnRep_bIsInactive` re-sorts the player lists | [V] `PlayerState.cpp` (`GetLifetimeReplicatedProps`); `PlayerState.h:45-69, 298` |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0009 (version control and binary asset workflow; user decision pending) → ADR-0007 (module layout, composition root, include boundaries: the `Public/View` vs `Public/Server` split this ADR's carrier rules use) → ADR-0008 (test and automation strategy: the multi-client/multi-process harness and raw-bunch capture that S1, S2, S14 run in). All three must be Accepted, and S1/S2/S14 must report, before this ADR can be Accepted |
| **Enables** | ADR-0003 (replication, relevancy and the concealment cull: implements the pawn parity and pointer mechanisms; scoped to the legacy cull path once this ADR picks legacy), ADR-0010 (Shared Pawn: class, factory, possession parity), ADR-0011 (pawn command interface), ADR-0013 (significance-only animation), ADR-0019 (informant lifecycle; consumes the `T_due` slot), ADR-0020 (objectives team-view sub-struct), ADR-0021 (HUD and markers read only carriers), ADR-0024 (Iris migration path) |
| **Blocks** | Every epic that replicates a pawn, PlayerState or PlayerController field: Shared Character & Locomotion, Information Visibility Policy, Participant State & Roster carriers, Teammate Markers, HUD, NPC-Kill Penalty & Informants (takeover), Session & Travel (rejoin/hand-over shape) |
| **Ordering Note** | Written first by mandate, accepted after ADR-0009 → ADR-0007/ADR-0008 → spikes (architecture.md "Bootstrap order"). Until then stories that touch the identity boundary stay Blocked by design. The `T_due` carrier slot (Decision §6) was filled by the user decision 2026-10-25 (option (a), the C-TeamView owner part; see Amendments) |

## Context

### Problem Statement

Smoking Mirror's first pillar is "Hide in Plain Sight": until someone commits, an enemy player cannot be told apart from an NPC by model, animation, audio, UI or **network presentation**. Unreal's defaults break that pillar silently. Every player pawn replicates a non-null `PlayerState` to every client, while an NPC pawn's is null (E1); the pawn's `Controller` and `Owner` export NetGUIDs whose resolvability differs by controller kind (E3, Roster R6); `PlayerState` actors are always relevant and carry join order, ping, inactive and bot flags (E9, IVP R5); possession forces an off-cadence update to every connection (E8). The prototype already ships the resulting leak: `AStealthCharacter::IsNpc()` is `PlayerState == null`, and it works on every client (first-playable report, known issue 2).

These are cross-cutting rules: 20-odd systems touch player data, and if each one decides for itself what to replicate, one wrong default makes every enemy findable from a packet dump. The decision must be made before any Foundation or Core code, because the pawn class, the PlayerState class, the PlayerController class and every replicated field are shaped by it.

### Constraints

- **Locked user rules** (active.md, GAME_BUILD_HANDOFF.md "Confirmed game rules"): players and NPCs share the character pool and look identical; teammate markers exist only on teammates' clients and enemies must not be able to obtain them; informants are seen by enemies exactly as NPCs; listen-server host, 2 teams of up to 5, ~40 NPCs.
- **Engine:** UE 5.8.4, C++ primary; the legacy NetDriver is the configured default (E10); no source-built engine in the MVP (no engine patches).
- **Networking HIGH risk:** owner-only resets of `APawn::PlayerState/Controller` and `AActor::Owner` without breaking possession or RPC routing are unproven (AQ-04, S1/S14).
- **Write scope of this run:** `Config/DefaultEngine.ini` cannot be edited by the pipeline; the Iris pin is a pending user item.
- **Team:** 1-2 programmers; the mechanism must be small, testable and not depend on a Beta plugin.
- **Listen host:** the host process holds all server data (IVP R10, accepted until the Dedicated Server tier, ADR-0024).

### Requirements

- One pawn class for every body; no code path outside the Roster's and the Policy's server-side queries branches on player vs NPC (architecture principle 1). Permitted server-only consumers of the Roster's kind query (`GetParticipant`) include **telemetry row selection** (ADR-0023 §2.3): the Event Log sink may use the result only to choose which server-only row it writes, never to change gameplay or anything replicated.
- AI drives that pawn through the same command path as a human.
- Animation LOD and budgets take no player-ness input.
- The pawn-to-participant link (`PlayerState`, `Controller`, `Owner`, `PlayerState->GetPawn()`, possession events) reaches only the owner's connection, with the **identical raw-bunch class** on non-owners for both kinds.
- Teammate identity reaches teammates only, through one carrier (C-TeamView) on each PlayerController; feature team data rides that carrier.
- Every replicated property has one audience and one carrier (default deny); the allowlist audit can enforce it.
- A replication system is chosen, pinned and asserted.
- A fallback exists if the spikes fail, so the pillar does not depend on an unproven mechanism.

## Decision

The ADR fixes **invariants and carriers**. Mechanisms belong to ADR-0003 (replication, relevancy, cull), ADR-0010 (pawn and possession), ADR-0011 (commands) and ADR-0013 (animation); each of those must satisfy the invariants below and may not relax them.

### 1. One pawn class

Every body in a round (NPC, active player, informant, concealed, dying) is an instance of **one** class, `AStealthPawn : ACharacter` (final name chosen here; SCL R1 left it "-> ADR"). The Game Animation Sample character Blueprint is reparented onto it, as the prototype's `AStealthCharacter` already shows is feasible.

- No subclass, per-instance component set, per-instance replication flag, per-instance net config or per-instance tuning distinguishes NPCs from players. The only per-instance data that may differ is `AppearanceId`, `FActionState`, movement state and the possessing controller (SCL R1).
- No property, function, component, AnimBP or audio branch on the pawn may test `IsPlayerControlled()`, `IsBotControlled()`, `IsLocallyControlled()` (for anything another client can observe), the controller class, `PlayerState` presence, team or Roster state. The only exceptions are owner-local presentation on the owning machine (SCL R9: own head hidden in first person, the camera) that change nothing replicated.
- NPCs are possessed by one crowd AIController class with `bWantsPlayerState = false` (no NPC PlayerState). Players are possessed by `AStealthPlayerController`. The controller class is server-side knowledge and never reaches a non-owner (§4).
- Pawn creation goes only through the pawn factory (`UStealthPawnFactory::SpawnSharedPawn` / `TakeOverCrowdPawn`, ADR-0010); every spawn path is identical for both kinds, asserts `Instigator == Self`, and spawns no replicated child actor carrying `Owner` or `Instigator` (IVP R5).

### 2. One command path

AI drives the pawn through the **same pawn command interface** as a human (`UPawnCommandComponent`, non-replicated, server-side; IIF R1, R2, R17; ADR-0011 owns the mechanism):

- Discrete commands (`Traverse`, `InteractBegin/End`, `PrepareBegin/End`, `Attack`, `Hide`) enter the same entry points under the same ingress, legality, slot arbitration and token buckets. No parameter, event, payload or listener carries `IsAI`, `IsPlayer`, a controller, a PlayerState or a team.
- Movement: human `Move`/`Look` ride the CMC saved-move path; AI `Move` is applied through the same component function into the same CMC input (no path-following component writing velocity), and AI `Look` writes the control rotation that the same `HeadLook` filter consumes (SCL R8; the crowd AIController overrides `UpdateControlRotation`, CNB-R3). On the pawn, one consumer exists for movement input (the CMC) and one for control rotation (the `HeadLook` filter), whatever the controller.
- `RemoteViewPitch16` is written by the pawn in `PreReplication` from the `HeadLook` source for every pawn, with or without a controller (IVP R5, SCL R8.4; mechanism ADR-0003).

### 3. No AI-keyed animation LOD or budget (significance only)

Animation fidelity on a client is a function of the pawn's figure screen height and Camera's eye-mask "rendered" flag only (SCL R11, F7, F8). No input to tiering, URO, search interval, `VisibilityBasedAnimTickOption`, mesh LOD, the AnimationBudgetAllocator or any audio concurrency rule may be controller kind, team, binding, participant state or any gameplay fact. The AnimationBudgetAllocator may be adopted only with this significance function as its sole input (ADR-0013 decides whether; it does not adopt it, and engine URO stays off). The server ticks pose for every pawn identically (always-tick), **scoped to the observable result** (ADR-0010 E4, ADR-0013): every pawn's pose, montages, root motion and notifies advance on the server whether rendered or not, with authority using `AlwaysTickPoseAndRefreshBones` at rate 1 for every pawn; the engine's role-driven tick timing for remotely controlled pawns (their server pose advances with `ServerMove`s) is not changed, and host-view parity is verified by ADR-0013's ANIM-V3. Performance fallbacks lower fidelity for every pawn equally (SCL R12). Tie-breaking keys are client-local random draws, never spawn index, NetGUID or actor name.

### 4. The pawn-to-participant link is owner-only

**4.1 Pawn pointers.** `AStealthPawn::GetLifetimeReplicatedProps` calls `Super` and then resets the conditions of the three inherited link properties to `COND_OwnerOnly` on the class itself (not the per-actor active-override toggle), with the condition-only overload, which keeps `Owner`'s push-based flag (E13):

- `APawn::PlayerState` → `COND_OwnerOnly` by name (`ResetReplicatedLifetimeProperty(..., FName("PlayerState"), COND_OwnerOnly, ...)`; the member is private, E2/E4). Because a misspelled name fails only at startup, the harness boot test asserts the reset took effect.
- `APawn::Controller` → `COND_OwnerOnly` with the compile-checked `RESET_REPLIFETIME_CONDITION` (public member, E4).
- `AActor::Owner` → `COND_OwnerOnly` with `RESET_REPLIFETIME_CONDITION` (public member; push flag kept, E13).

Non-owners therefore receive these properties for **neither** kind: the test is absence from every non-owner changelist (S1, AC-IVP-01/02/37), not "equal null". Teammates are non-owners too: allies get the pawn↔participant link only as a `PawnRef` through C-TeamView (§5), never through engine pointers. `PlayerState->GetPawn()` is therefore null on every client except the owner's, and no client code may rely on it. On the legacy driver a new owner receives the current values after a takeover (E12 [V]).

**Owner-local cleanup.** After an ownership change the previous owner's client keeps the last `Owner`/`PlayerState` values on its former body (E12), so on that one client `IsPlayerControlled()` and `GetNetConnection()` would answer wrongly for a body it no longer controls. This is not a leak to anyone else, but it is a stale-state hazard: `AStealthPlayerController` clears `Owner` and `PlayerState` on the previous pawn **client-locally** whenever its own pawn changes (owner-local, changes nothing on the wire), and no owner-side code may read those fields on a pawn it does not currently possess.

**4.2 Possession is invisible to non-owners.** Takeover, rejoin, `PawnRecovery` and hand-over to crowd AI produce no non-owner-visible bunch: the possession `ForceNetUpdate` (E8) is **dropped** and the next fixed-cadence update carries the change (ADR-0003 §2 owns the mechanism; dropping is observably identical to deferring and needs no per-pawn state), owner relevancy is off on every pawn (`bNetUseOwnerRelevancy = false`, `bOnlyRelevantToOwner = false`), and role changes are visible only to the new owner (IVP R14; the dropped call and parity config are ADR-0003/ADR-0010 mechanisms; S14 verifies).

**4.3 PlayerState is a closed public allowlist (C-Public).** `AStealthPlayerState` replicates to all clients exactly `PublicHandle` (new, constant for the match) and `PlayerNamePrivate` (carrying `DisplayName` only, frozen after Lobby), plus the base `AActor` properties held constant for the round (`bReplicateMovement`, `Role`/`RemoteRole`, `bHidden`, `bTearOff`, `bCanBeDamaged`, `Instigator`; each listed in the allowlist). **`AActor::Owner` on `AStealthPlayerState` is reset to `COND_OwnerOnly`**, because re-associating a rejoiner's retained PlayerState with its new controller calls `SetOwner`, which would otherwise broadcast a connection event to every client (E16; Session rule 22). Every other engine-default replicated PlayerState property (E17) is either **disabled** (`DISABLE_REPLICATED_PRIVATE_PROPERTY` for the private ones, `DISABLE_REPLICATED_PROPERTY` otherwise, E5) because its client-side default equals the value the GDDs require, or **reset to `COND_OwnerOnly`** where the owning client may need its own value:

| Property (engine default) | Treatment | Why |
|---|---|---|
| `Score`, `StartTime` (`COND_None`) | Disabled | Join time is a per-player, time-stamped value; score is unused |
| `PlayerId`, `UniqueId` (initial-only) | Reset to `COND_OwnerOnly` (`PlayerId` by name: private) | Join order; a null-subsystem id may embed an address. Owner-only keeps the owning client's own ids (lower S2 risk than disabling); other clients never receive them, so their `OnRep_UniqueId` session registration does not run for others (harmless without an online subsystem; voice is disabled in MVP) |
| `bIsABot`, `bIsInactive` (initial-only) | Disabled (client default `false`) | No NPC has a PlayerState; inactive is a time-stamped disconnect signal (Session rule 22) |
| `bIsSpectator`, `bOnlySpectator`, `bFromPreviousLevel` (`COND_None`) | Disabled (client default `false`) and never written on the server | Spectator state is banned (§4.5); travel provenance is per-player |
| `CompressedPing` (`COND_SkipOwner`) | Disabled, and `SetShouldUpdateReplicatedPing(false)` (the flag is private, E17) | Connection data is Owner-only (Session rule 24) |

No Roster transition, connection event or team change ever writes a PlayerState byte. Team, state, appearance, connection and pawn data never go on the PlayerState (Roster R6). The prototype's replicated `Team`, `LifeState` fields are the pattern this forbids.

**4.4 The player array is frozen in-round.** No PlayerState is created, destroyed, deactivated or renamed between `Preparing` and the next travel; a rejoiner is re-associated with its retained PlayerState (Session rules 22, 25a; Roster R6). The array reveals only the human count, which is public by design (lobby `n / capacity`). Mechanism: ADR-0005.

**4.5 Engine spectator state is banned.** Every PlayerController stays in `NAME_Playing`; spectator RPCs fault and return; `bStartPlayersAsSpectators = false`; `SpectatorClass` replicates null; pawnless viewers use a client-local, non-replicated camera actor (IVP R13; AC-IVP-38 grep check).

**4.6 Presentation never derives identity.** Client presentation code may not call `IsPlayerControlled()`/`IsBotControlled()`, read a pawn's `PlayerState`, `Controller` or `Owner`, iterate world pawns to find people, or include any `Public/Server/` header (compile guard, ADR-0007; AC-IVP-17).

### 5. The carrier model

Each audience has exactly one kind of carrier, and a carrier holds only its audience (IVP R4):

| Audience | Carrier | Rule |
|---|---|---|
| `All` | `AStealthPawn` (identical for every pawn), C-Public on `AStealthPlayerState` (§4.3), GameState public structs (`FRoundPublicState`, tuning client snapshot, `FLayoutManifest`) | A pawn carries only `All` facts, identical in type, path, condition and timing for every pawn kind. A public struct carries no per-participant and no per-team value |
| `Team` | **C-TeamView** on each `AStealthPlayerController` | The server copies team data into each teammate's own controller. No team-filtered replicated actor, no custom team relevancy (the concealment cull of ADR-0003 is keyed on concealment, not team) |
| `Owner` | The owner's `AStealthPlayerController` (C-TeamView owner part, owner data components, owner replies); the owner's own pawn only for the three `COND_OwnerOnly` engine pointers of §4.1 | Never a gameplay field on the pawn |
| `ServerOnly` | Game-instance and world stores (Roster, Session, Round, Penalty, Objectives, LSC) | Never a replicated property |

**C-TeamView** is a set of replicated `UActorComponent`s owned by the PlayerController, all derived from `UTeamViewComponentBase` (Foundation `Public/View/`), registered on the controller's registered sub-object list (E11). The PlayerController is relevant only to its owner (E6), so every part reaches exactly one connection:

- **Roster part** (`URosterTeamViewComponent`, Roster R6): owner `{PublicHandle, Team, State, RoundEpoch, WrongTargetTally, LastResultSeq, T_due}`; per teammate (at most four) `{PublicHandle, State, PawnRef, IsConcealed, RetireAtServerTime, bReconnecting}` (user decisions 2026-10-25). `bReconnecting` (one bit, **Team** audience; MVP teammate-reconnecting indicator, scope decision 2026-10-25) is derived by the Roster push step from Session's existing `Connection` projection (true while `Disconnected` inside `DisconnectGraceSec` during `Active`); no new writer or server input. The three added owner fields are **Owner** audience and Penalty-sourced: `WrongTargetTally`/`LastResultSeq` are written only on reclaim, rejoin and `OnRosterReset` (never per strike, Penalty CR-19), `T_due` when Penalty schedules or reschedules a respawn and cleared at the binding (§6). `RetireAtServerTime` is **Team** audience and is non-zero only in the update that retires the teammate's binding (the kill's `ContactServerTime`, or the release slot time; Teammate Markers TM-R6). Penalty supplies the owner values through the Roster server face (`SetOwnerTally`, `SetOwnerRespawnDue`); Assassination/Penalty pass the retire time through `RetireBinding(Pawn, RetireAtServerTime)`; the Roster's push step stays the only writer. Pushed on change only, at most once per server frame, all changes in a frame coalesced; `PawnRef`s cleared at `InvalidateAllBindings` and re-pushed after each controller swap and on reclaim. **`IsConcealed` input** (architecture-review fix 2026-10-25): the Roster server face exposes `SetTeammateConcealed(FParticipantId, bool)` (`URosterStore`, `Public/Server`), whose only caller is Concealment (ADR-0017 §3). It records the flag in the participant's record and marks the participant's teammates dirty. The Roster's push step stays the **only writer** of the replicated entry. The flag is per participant: Concealment clears it at the crossing, at the body removal or on the participant's rebind (ADR-0017 §3), and the Roster clears every record's flag at `OnRosterReset`.
- **Fallback position feed** (IVP R8, F6): `{PublicHandle, Pos, Seq}` entries (`Seq`: one wrapping byte advanced every feed tick so staleness works under change-only replication; user decision 2026-10-25, Markers OQ14) for teammates whose `PawnRef` does not resolve on that connection; built by the Visibility Policy, carried here (ADR-0003 owns the builder and timing).
- **Feature team sub-structs ride C-TeamView** (cross-review C-W6): each feature that needs a Team audience adds its own `UTeamViewComponentBase` subclass to the controller (MVP: `UObjectiveTeamViewComponent` carrying `FObjectiveTeamView`, ADR-0020; `UTeamPingViewComponent` carrying `FTeamPingView { Entries[TeamSize] {SenderHandle, PingSeq, Point (10 cm), ServerTime} }` (fixed-size slots; no epoch of its own), written by the Informant Pings server handler into the controller of every member of the sender's team, added by user scope decision 2026-10-25). The Roster owns none of their contents, and the Roster lists are its part, not a closed schema. Adding a part is a change request to the Visibility Policy (new fact row and allowlist row) — never a local decision.
- **Teammate markers only on teammates' clients.** Markers are built from the local controller's C-TeamView entries and feed only (TM-R1, TM-R2). An opponent's controller is never relevant to you, so an opponent's client holds no entry and no feed for your team and never creates a marker for it: the locked rule is enforced by the absence of data, not by a client-side filter.
- **Host parity.** The listen host's presentation reads its own local controller's C-TeamView through the same component path as a remote client (Session rule 3, HUD-R1); it never reads another controller's view or a server store.
- Presentation finds parts with `FindComponentByClass<>()` on its own controller, so `StealthPresentation` never includes the root module's `AStealthPlayerController` header.

### 6. Owner-only `T_due` carrier (decided by the user 2026-10-25: option (a))

The informant countdown end `T_due` (server time; PEN-R15, HUD) has audience **Owner** and must reach only the awaiting informant's connection. **Decision (user, 2026-10-25; cross-review C-W5, architecture AQ-02 closed): option (a), a field `T_due` (double, server time, 0 = none) in the C-TeamView owner part (§5).** It satisfies the constraints below: Owner audience only, on the PlayerController carrier family, no byte in any non-owner changelist (the controller is relevant only to its owner), with an IVP fact row and allowlist row (IVP R3 `T_due` row). The options that were recorded for the user were (a) a field in the C-TeamView owner part, (b) a field in an owner reply, (c) client derivation from public tuning and the owner's own fall time. Options (a) and (b) must satisfy: Owner audience only; on the PlayerController carrier family of §5 or an owner reply, never on the pawn; adds no byte to any non-owner changelist; gets an IVP fact row and an allowlist row. Option (c) has no carrier; it must instead show that every input it derives from is already Owner- or All-audience data the client legitimately holds, and that the derived value matches the server's `T_due` within the presentation tolerance. The decision is recorded in the Amendments below, and ADR-0019 consumes it (§6 of ADR-0019).

### 7. Replication system: legacy NetDriver, Iris pinned off

The MVP uses the **legacy NetDriver** (no ReplicationGraph). `net.Iris.UseIrisReplication=0` is to be pinned explicitly in `Config/DefaultEngine.ini` so an engine default change or a stray command-line switch cannot flip the game driver (E10); that config edit is outside this pipeline's write scope and is a **pending user item** (active.md "[create-architecture] Project-config changes"). The test harness asserts the active replication system at startup and fails any test session that is not on the legacy path (ADR-0008). The cull path ADR-0003 specifies is therefore the legacy one (`IsNetRelevantFor` replacement plus explicit channel close); the Iris filter path, Iris parity reruns (S7, S11) and the migration decision move to ADR-0024.

### 8. Written fallback if S1 or S14 fail

If S1 shows any non-owner receives a distinguishable bunch for the three pointers, or S14 shows the `COND_OwnerOnly` reset breaks possession, `OnRep_Controller`, input or owner-side behaviour (re-evaluation on ownership change is already verified for the legacy driver, E12, so it is not a trigger):

1. **Never replicate `APawn::PlayerState` or `APawn::Controller` to any client, on every pawn**, by disabling them in `AStealthPawn::GetLifetimeReplicatedProps` (`DISABLE_REPLICATED_PRIVATE_PROPERTY` / `DisableReplicatedLifetimeProperty` by name for `PlayerState`, `DISABLE_REPLICATED_PROPERTY` for `Controller`, E5). This is uniform for both kinds, so non-owner parity is guaranteed by construction.
2. **The owner gets its own link through its PlayerController.** The controller's `Pawn` and `PlayerState` reach the owner only (E6). On the owning client only `ClientRestart` sets `Pawn->Controller` (E7), and the CMC moves and sends `ServerMove` only when that link is set (E15), so `AStealthPlayerController::OnRep_Pawn` must **also** call `GetPawn()->SetController(this)` locally, so owner input never waits for a late `ClientRestart`; the harness checks owner input works before `ClientRestart` arrives. Owner-side code obtains its PlayerState from its own controller, never from the pawn. The ally mapping already comes only from C-TeamView `PawnRef`, so nothing else changes.
3. **`AActor::Owner`.** If S1 also shows a distinguishable `Owner` bunch, `Owner` is disabled for every pawn too. Movement routing does not need it, because `APawn::GetNetConnection` resolves through `Controller` first (E15); a client-local `Owner` would matter only for actors the pawn owns, and this ADR allows no replicated pawn-owned actor (IVP R5). If the fallback also fails S1 or S14, ADR-0001 is superseded by an amendment, Pillar-1 leak row L1 stays CRITICAL and blocks every build that leaves the development team (IVP R12), and the escalation goes to the technical-director and the user.

### Architecture Diagram

```
                     SERVER (listen host)                                CLIENT C (owner of pawn P)        CLIENT X (anyone else, ally or enemy)
 ┌──────────────────────────────────────────────────────┐
 │ Roster / Penalty / Objectives (ServerOnly stores)    │
 │        │ copy team & owner data per connection       │
 │        ▼                                             │
 │ AStealthPlayerController(C)  [bOnlyRelevantToOwner]──┼──► C-TeamView parts (Roster, feed,      (never relevant: X receives
 │   ├ URosterTeamViewComponent                         │      Objectives, owner T_due)            nothing from C's controller)
 │   ├ UObjectiveTeamViewComponent ...                  │
 │   └ owner data / replies                             │
 │                                                      │
 │ AStealthPawn P (one class for every body)            │
 │   PlayerState / Controller / Owner  COND_OwnerOnly ──┼──► P's own pointers                      (absent from X's changelist,
 │   movement, HeadLook, RemoteViewPitch16,             │                                           same for NPC pawns)
 │   FActionState, TraversalState, AppearanceId (All) ──┼──► same All facts ───────────────────────► same All facts
 │                                                      │
 │ AStealthPlayerState (always relevant)                │
 │   PublicHandle, PlayerNamePrivate (All) only ────────┼──► name list ────────────────────────────► name list (frozen in-round)
 │                                                      │
 │ Crowd AIController (no PlayerState) ─┐               │
 │ AStealthPlayerController ────────────┴─► UPawnCommandComponent ─► CMC + HeadLook filter (one path)
 └──────────────────────────────────────────────────────┘
 Ally Y learns "P is my teammate" only from Y's own C-TeamView PawnRef, never from P's data.
```

### Key Interfaces

```cpp
// --- StealthGameplay/Core: the one pawn class ------------------------------------------
UCLASS()
class AStealthPawn : public ACharacter
{
    GENERATED_BODY()
public:
    // Super, then (all on this class, never per actor):
    //   ResetReplicatedLifetimeProperty(StaticClass(), APawn::StaticClass(),
    //       FName(TEXT("PlayerState")), COND_OwnerOnly, OutLifetimeProps);   // private member, E2/E4
    //   RESET_REPLIFETIME_CONDITION(APawn, Controller, COND_OwnerOnly);       // public, compile-checked
    //   RESET_REPLIFETIME_CONDITION(AActor, Owner, COND_OwnerOnly);           // condition-only: push flag kept, E13
    // Fallback (Decision §8): disable PlayerState (by name) and Controller (and Owner if S1 requires).
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    // Invariant: no member, component or Blueprint branch reads controller kind, PlayerState
    // presence, team or Roster state for anything another client can observe.
};

// --- StealthGame (root): the public PlayerState --------------------------------------------
UCLASS()
class AStealthPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    // All clients: PublicHandle + PlayerNamePrivate (DisplayName) + constant base AActor props.
    // COND_OwnerOnly: AActor::Owner (rejoin SetOwner, E16), PlayerId (by name), UniqueId.
    // Disabled: Score, StartTime (DISABLE_REPLICATED_PRIVATE_PROPERTY for private ones), bIsABot,
    // bIsInactive, bIsSpectator, bOnlySpectator, bFromPreviousLevel, CompressedPing;
    // SetShouldUpdateReplicatedPing(false) in the constructor.
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated) FPublicHandle PublicHandle;   // constant for the match
};
// AStealthPlayerController: on its own pawn change, clears Owner/PlayerState on the previous pawn
// client-locally (§4.1 owner-local cleanup); in the §8 fallback, OnRep_Pawn also calls
// GetPawn()->SetController(this) locally.

// --- StealthFoundation Public/View: C-TeamView part base ------------------------------------
UCLASS(Abstract)
class UTeamViewComponentBase : public UActorComponent
{
    GENERATED_BODY()
    // Replicated by default; valid only when owned by an APlayerController (ensure + fault sink
    // otherwise); registered on the owner's sub-object list; push-model properties; never
    // attached to a pawn. Subclasses own their own replicated struct.
};

// Foundation may not depend on StealthGameplay, so PawnRef is typed APawn (never AStealthPawn).
USTRUCT() struct FTeamViewOwnerPart
{   GENERATED_BODY()
    UPROPERTY() FPublicHandle Handle;  UPROPERTY() ETeam Team;
    UPROPERTY() EParticipantState State;  UPROPERTY() uint32 RoundEpoch = 0;
    // Owner-only, Penalty-sourced (user decision 2026-10-25):
    UPROPERTY() uint8 WrongTargetTally = 0;        // written only on reclaim, rejoin, OnRosterReset
    UPROPERTY() FClientCommandSeq LastResultSeq;   // HUD re-push gate (HUD-R7)
    UPROPERTY() double T_due = 0.0; };             // informant respawn due (server time); 0 = none
USTRUCT() struct FTeamViewTeammateEntry
{   GENERATED_BODY()
    UPROPERTY() FPublicHandle Handle;  UPROPERTY() EParticipantState State;
    UPROPERTY() TObjectPtr<APawn> PawnRef;  UPROPERTY() bool bIsConcealed = false;
    UPROPERTY() double RetireAtServerTime = 0.0; };   // Team; set only in the retiring update (user decision 2026-10-25)
// Roster server face (URosterStore, Public/Server; ADR-0005 §2): the only input to the teammate IsConcealed field.
//   void SetTeammateConcealed(FParticipantId Participant, bool bConcealed);   // caller: Concealment only (ADR-0017)
UCLASS() class URosterTeamViewComponent : public UTeamViewComponentBase
{   // Roster part (Roster R6). Written only by the Roster's push step, coalesced once per frame.
    GENERATED_BODY()
    UPROPERTY(Replicated) FTeamViewOwnerPart OwnerPart;
    UPROPERTY(Replicated) TArray<FTeamViewTeammateEntry> Teammates;   // <= 4
};
// Fallback feed entries {PublicHandle, Pos int16x3 @ 10 cm, uint8 Seq}: IVP builder, ADR-0003.
// Feature parts (e.g. UObjectiveTeamViewComponent / FObjectiveTeamView): ADR-0020.
// T_due: carried in FTeamViewOwnerPart (Decision §6, user decision 2026-10-25).
```

### Implementation Guidelines

- Every body must be an `AStealthPawn`; there must never be an NPC subclass, an NPC-only component, or a per-instance replication or net-config difference.
- Code on the pawn, its components, AnimBP and audio must never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state for anything another client can observe.
- AI must issue every command through `UPawnCommandComponent` entry points and must move only through the same CMC input and control-rotation consumers a human uses; nothing may write a pawn's velocity or rotation directly for AI.
- Animation tiering, URO, budget allocation, mesh LOD and audio concurrency must take only screen height and the eye-mask rendered flag; they must never take a gameplay fact.
- `AStealthPawn` must reset `PlayerState` (by name), `Controller` and `Owner` (`RESET_REPLIFETIME_CONDITION`) to `COND_OwnerOnly` on the class with the condition-only overload, and must never use the per-actor active-override toggle for them; `AStealthPlayerState` must reset its own `Owner`, `PlayerId` and `UniqueId` to `COND_OwnerOnly`.
- Owner-side code must never read `Owner` or `PlayerState` on a pawn it does not currently possess; `AStealthPlayerController` must clear them client-locally on its previous pawn.
- Possession must never produce an off-cadence non-owner update (the unscoped possession `ForceNetUpdate` is dropped and the cadence carries the change, ADR-0003 §2/ADR-0010).
- `AStealthPlayerState` must replicate only `PublicHandle` and `PlayerNamePrivate`; team, state, connection, appearance and pawn data must never be on a PlayerState.
- The in-round player array must never change; the engine spectator state must never be entered.
- Team-audience data must travel only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; there must never be a team-filtered actor or team relevancy rule.
- Owner-audience data must travel only on the owner's PlayerController (components or owner replies), never as a gameplay field on the pawn.
- Presentation code must never read a pawn's `PlayerState`, `Controller` or `Owner`, must never iterate world pawns to find people, and must read only its own controller's carriers, the GameState public structs and client-local queries.
- A new replicated field, fact or audience must not ship without a Visibility Policy row and an allowlist row; the reflection audit must fail the build otherwise.
- The game NetDriver must run the legacy replication system; the harness must assert it at startup; ReplicationGraph must not be enabled.
- `T_due`, `WrongTargetTally` and `LastResultSeq` must travel only in the C-TeamView owner part; the tally fields must never be written per strike (only on reclaim, rejoin, `OnRosterReset`); `RetireAtServerTime` must be non-zero only in the retiring update.
- Prototype code (`Source/StealthGame`) must not be promoted into the production modules under this ADR until it is Accepted; the prototype patterns listed under Migration Plan must be replaced, not ported.

## Alternatives Considered

### Alternative 1: ReplicationGraph to filter player data per connection (the prototype header's suggestion)
- **Description**: Enable the ReplicationGraph and route pawns, PlayerStates and team data through custom nodes that decide which connections receive which actors.
- **Pros**: Mature (pre-cutoff) per-connection actor routing; scales actor lists well.
- **Cons**: It filters **actors**, not properties: a pawn relevant to everyone still sends its `PlayerState` pointer to everyone, so the core leak (E1) remains. It replaces `IsNetRelevantFor`, so ADR-0003's legacy concealment cull would have to be rebuilt as graph nodes. It adds a second relevancy system for a 50-pawn, always-relevant scene that gains nothing from spatial routing.
- **Rejection Reason**: Does not solve the property-level leak and conflicts with the cull path; property conditions on one pawn class do.

### Alternative 2: Adopt Iris now (group filters + Iris property conditions)
- **Description**: Switch the game driver to Iris (`net.Iris.UseIrisReplication=1`), use per-connection exclusion group filters for the cull and Iris conditions for the pointers.
- **Pros**: 5.8 release notes call Iris production-ready; per-connection filter status destroys culled objects promptly (no relevancy-timeout lag); it is Epic's long-term direction.
- **Cons**: Off by default in 5.8.4 (E10); the plugin still carries a Beta flag [D]; condition-reset and pointer trimming behaviour under Iris is unverified (S7); custom `NetSerialize` fixed-width structs need Iris serializers [U]; the team has no Iris experience and the specialists recommend legacy (Session G, AQ-01).
- **Rejection Reason**: Higher unknowns on the exact surfaces Pillar 1 depends on; deferred to ADR-0024 with the S7/S11 rerun gate.

### Alternative 3: Give every NPC a PlayerState ("parity by addition")
- **Description**: Make the crowd AIController create a PlayerState so `APawn::PlayerState` is non-null for every pawn.
- **Pros**: Removes the null/non-null difference without touching engine property conditions.
- **Cons**: Adds ~40 always-relevant PlayerStates (E9) whose fields (names, ids, join time, ping) would each need a fabricated, player-plausible distribution; the player array would then misstate the human count that the lobby makes public; `Controller` and `Owner` still export controller-kind-dependent references, so owner-only conditions are needed anyway; more bandwidth and a larger leak surface.
- **Rejection Reason**: Strictly more surface for the same remaining work.

### Alternative 4: A lightweight NPC pawn (separate class or Mass crowd)
- **Description**: Cheaper NPC representation for 40+ bodies.
- **Pros**: CPU and bandwidth savings.
- **Cons**: Every difference in class, components, movement or animation is a Pillar-1 detector; violates the locked "one shared character pool, identical look" rule and SCL R1.
- **Rejection Reason**: Contradicts the pillar and a locked rule.

### Alternative 5: A per-team replica actor for team data
- **Description**: One replicated actor per team, relevant only to that team's connections, carrying markers and team state.
- **Pros**: One write per team instead of one per teammate controller.
- **Cons**: Needs a custom team relevancy filter (forbidden by IVP R4/R6), whose behaviour under Iris is unverified [S]; a team-filtered actor is itself a team grouping visible in a packet capture.
- **Rejection Reason**: Roster R6 already replaced it with the copy-into-each-controller model.

### Alternative 6: AI-specific animation budget or LOD
- **Description**: Cheaper animation for NPCs (lower update rate, no motion matching) to save the 4 ms line.
- **Pros**: Large CPU saving.
- **Cons**: An animation-quality difference keyed on controller kind is a direct visual tell (IVP L18).
- **Rejection Reason**: Significance-only LOD gives savings that apply to every pawn equally.

## Consequences

### Positive
- The most serious Pillar-1 leak (L1, possession pointers) is closed by a uniform, class-level rule with an automated canary instead of by write discipline in many systems.
- One carrier family for Team and Owner data means one audit point: every team or owner fact is a component on the receiving controller, discoverable by reflection.
- Teammate markers are safe by construction: enemies never receive the data, so no client-side filter can be bypassed.
- Legacy driver keeps the cull path and the leak tests on a well-understood system; the Iris decision is deferred to a tier (Alpha) that also brings a dedicated server.
- The fallback (§8) means the pillar does not hinge on S14 alone.

### Negative
- Engine and plugin code that reads `Pawn->GetPlayerState()` or `Pawn->GetController()` on simulated proxies no longer works there; sample code and any plugin that does must be audited (E14).
- Disabling PlayerState defaults removes client-side `PlayerId`/`UniqueId`; any future client feature that needs them (voice, platform friends) needs a new decision.
- C-TeamView copies team data into up to five controllers per change (bounded, coalesced per frame).
- Acceptance is gated behind three other ADRs and three spikes, so every identity-touching story stays Blocked longer.
- The listen host still holds all server facts (IVP R10): this ADR protects against clients, not against the host.

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Owner-only pointers break an owner-side path not covered by E12/E15 (camera, input context, sample traversal) | Low-Medium | High (possession or input breaks) | S14 before acceptance; §8 fallback with `OnRep_Pawn` setting the local controller link |
| A misspelled by-name reset (`PlayerState`, `PlayerId`) fails only at startup (E4) | Low | Medium | Harness boot test asserts every reset/disable took effect; reflection allowlist audit |
| Stale `Owner`/`PlayerState` on the previous owner's former body after a takeover (E12) | High (by engine design) | Low (owner-local only) | §4.1 owner-local cleanup; owner code never reads those fields on a pawn it does not possess |
| Sample or plugin code reads `PlayerState` on simulated proxies (E14) | Medium | Medium | Grep audit of the sample's C++ and Blueprints during ADR-0010; S14 runs the sample's traversal and camera paths |
| Owner-only `PlayerId`/`UniqueId` breaks something client-side in login or travel | Low | Medium | S2 under seamless travel (ADR-0005, S3) |
| Iris behaviour of the condition resets differs from legacy (E12 is [U] under Iris) | n/a in MVP | High if Iris is adopted | Legacy pinned (§7); S7 rerun is a gate in ADR-0024 |
| A feature adds a replicated field outside the carrier model | Medium | High (silent leak) | Reflection allowlist audit fails the build (IVP R11, ADR-0008); canary per commit (AC-IVP-37) |
| Iris gets switched on by a config or command-line change | Low | High (cull and tests run on an unvalidated path) | Pin the cvar (pending user edit); harness startup assertion |
| ~~`T_due` decision stays open and blocks ADR-0019~~ | — | — | Closed 2026-10-25: the user chose option (a) (§6) |

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| participant-state-roster.md | R6 carriers C-Public / C-TeamView / C-Server; pawn-level link null-or-identical on non-owners; pawn replication parity; frozen name list; "ADR-001 owns the mechanism" | §4.1, §4.3, §4.4, §5; PlayerState field treatment table decided |
| participant-state-roster.md | R10 informant-kill invariant (attacker perceives nothing that separates an informant kill) | §4 and §5 keep `State` and `IsInformant` on Owner/Team carriers only; nothing on the pawn changes on a transition |
| information-visibility-policy.md | R1-R5 one query, viewer classes, matrix, one carrier per audience, UE default overrides | §4 (R5 rows for `PlayerState`, `Controller`, `Owner`, PlayerState defaults, player array, spectator, `IsPlayerControlled`), §5 (R4 carriers) |
| information-visibility-policy.md | R13 spectator state never used; R14 possession never reaches a non-owner | §4.5; §4.1-4.2 |
| shared-character-locomotion.md | R1 one class; R8 one look path; R11 significance-only LOD; R13 parity config (invariants only) | §1, §2, §3; §4.1-4.2 (mechanisms in ADR-0003/0010/0013) |
| input-interaction-framework.md | R2 no caller identity; R17 AI uses the same entry points | §2 |
| character-pool-appearance.md | CP-16 one look for everyone; no per-viewer appearance or extra component | §1 (no per-instance component set), §5 (markers are UI on the controller, not pawn components) |
| crowd-npc-behaviour.md | CNB-R2 identity blindness; CNB-R3 same commands, same rules | §2; §1 (no NPC PlayerState, one AIController) |
| npc-kill-penalty-informants.md | PEN-R14 informant-kill invariant; PEN-R15 owner-only data (incl. `T_due`) | §5 Owner carrier; §6 `T_due` in the C-TeamView owner part |
| teammate-markers.md | TM-R1 one source (own C-TeamView); TM-R2 no world iteration | §5 C-TeamView; §4.6 |
| hud.md | HUD-R1 three carriers, nothing else | §4.6, §5 |
| session-travel.md | Rule 3 host parity; rule 22 no pawn/PlayerState churn on connection events; rule 25 leak checklist (a)-(d) | §5 host parity; §4.3-4.4; §4.1-4.2 for 25(d); §7 legacy-vs-Iris choice (Session G) |

## Performance Implications
- **CPU**: Negligible. Condition evaluation for three properties per pawn per connection (legacy rep-layout conditions are evaluated per connection already). C-TeamView pushes are change-only and coalesced (≤ 1 per frame, ≤ 5 controllers per team).
- **Memory**: A few hundred bytes per PlayerController for C-TeamView parts; disabled PlayerState properties save their shadow state.
- **Load Time**: None.
- **Network**: Slight saving: three pointer properties no longer go to ~9 non-owner connections per player pawn; PlayerState defaults (ping updates in particular) stop. C-TeamView adds ≤ 4 small teammate entries per controller, change-only. Within the 50 KB/s per-client budget.

## Migration Plan

Nothing in `Source/StealthGame` is promoted until this ADR is Accepted (architecture "Prototype promotion rule"). The prototype is feasibility evidence and **violates** these rules in the following places, each of which is replaced, not migrated:

| Prototype site (read-only evidence) | Violation | Replacement |
|---|---|---|
| `AStealthCharacter` replicates `bConcealed`, `bRevealedUntilExit` to all (`StealthCharacter.cpp:63-64`) | Concealment state on the pawn; a modified client sees every bush occupant (report known issue 2) | Server-only concealment record; cull per ADR-0003/ADR-0017 |
| `AStealthCharacter` inherits unconditional `APawn::PlayerState`/`Controller` (E1); `IsNpc()` = `PlayerState == null` (`StealthCharacter.cpp:138`) | L1 possession leak; client-side kind detection | §4.1; `IsNpc` removed from every client path |
| `AStealthPlayerState::Team`, `LifeState` replicated `COND_None` (`StealthPlayerState.cpp:12-13`) | Team and informant state to every client | C-TeamView owner/teammate parts (§5) |
| `AStealthHUD::DrawTeammateMarkers` iterates `TActorIterator<AStealthCharacter>` and filters by `IsNpc()`/`GetTeam()` (`StealthHUD.cpp:171-174`) | World iteration and identity derivation in presentation (TM-R2) | Markers from own C-TeamView `PawnRef` only (ADR-0021) |
| `AStealthCharacter::IsTeammateOfLocalViewer()` reads the local PC's PlayerState team | Presentation derives team relation from replicated team fields | C-TeamView membership |
| `bDying`, `bPreparingDefence` as separate pawn bools, `Server*` RPCs on the pawn | Not the shared `FActionState`; commands not through one command component | `FActionState` (ADR-0018), `UPawnCommandComponent` via controller RPCs (ADR-0011) |
| `AStealthNpcController` sets `bWantsPlayerState = false` (`StealthNpcController.cpp:10`) | Consistent with this ADR | Kept as the pattern for the crowd AIController |
| The single `AStealthCharacter` class for players and NPCs | Consistent with §1 | Pattern carried into `AStealthPawn` |

## Validation Criteria
- **S1** (raw-bunch capture, multi-process, `LeakNetProfile`): for 2 player and 2 NPC pawns in view of a third client, `PlayerState`, `Controller` and `Owner` are absent from every non-owner changelist, for both kinds, across spawn, takeover, rejoin and hand-over; AC-IVP-01, AC-IVP-02, AC-PSR-45.
- **S2**: the reflection allowlist audit shows `AStealthPlayerState` replicating only `PublicHandle` and `PlayerNamePrivate`; a 3-client diff under net emulation shows no per-player field; login, seamless travel and rejoin work; AC-PSR-44.
- **S14**: possession, `OnRep_Controller`, `ClientRestart`, CMC `ServerMove` routing, camera and input work for the owner across takeover (informant), rejoin, `PawnRecovery` and AI hand-over; the new owner receives the three pointers after an ownership change.
- **Canary AC-IVP-37** per commit (absence of the three pointers on non-owners) and **AC-IVP-38** (spectator grep) pass.
- **AC-IVP-45** runtime differential audit: net config equal on an NPC pawn, a remote-client pawn, a host pawn and a just-taken-over pawn.
- **Carrier audit**: every replicated property of `AStealthPawn`, `AStealthPlayerState`, `AStealthPlayerController` and its `UTeamViewComponentBase` parts maps to exactly one IVP audience row; the build fails otherwise.
- **Markers**: an opponent's client holds no C-TeamView entry or feed entry for the other team at any time (TMK ACs, AC-IVP-15).
- Harness startup assertion reports the legacy replication system in every leak-suite run.

## Related
- Depends on ADR-0007, ADR-0008, ADR-0009 (acceptance order); enables ADR-0003, ADR-0010, ADR-0011, ADR-0013, ADR-0019, ADR-0020, ADR-0021, ADR-0024.
- ADR-0002 (resolution queue, server clock, round-end staging): the round-end step publishes phase `Ended` (IVP `PostRound` class) through `FRoundPublicState` and the C-TeamView carriers defined here.
- Design: `design/gdd/information-visibility-policy.md` (R1-R5, R13-R14, L1, L2, L30, L31), `design/gdd/participant-state-roster.md` (R6, R10, OQ0 spikes S1-S8), `design/gdd/shared-character-locomotion.md` (R1, R8, R11, R13), `design/gdd/teammate-markers.md`, `design/gdd/hud.md`, `design/gdd/session-travel.md` (rules 3, 22, 25), `design/gdd/gdd-cross-review-2026-10-24.md` (C-W5, C-W6).
- Architecture: `docs/architecture/architecture.md` (Required ADRs row ADR-001, AQ-01, AQ-02, AQ-04).
- Evidence: `production/first-playable-report.md` (known issue 2), `Source/StealthGame` (read-only).

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §3: "always-tick" scoped to the observable result (E4 scoping, as in ADR-0010); AnimationBudgetAllocator/URO not adopted — requested by ADR-0013 (sync item 7).
- Requirements: "telemetry row selection" named as a permitted server-only consumer of the Roster's kind query — requested by ADR-0023 (sync item 5).
- §6 (`T_due` slot): ADR-0019 confirmed as the consumer — already recorded (Ordering Note, §6); no edit.

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §4.2, guidelines: the possession `ForceNetUpdate` is "dropped; the next fixed-cadence update carries the change", matching ADR-0003 §2 (the replication owner decides; this still satisfies this ADR's invariant that possession produces no off-cadence non-owner update) and ADR-0010 §4.1 — architecture-review fix 2026-10-25 (Conflict 3).
- §5, Key Interfaces: Roster server face `SetTeammateConcealed(FParticipantId, bool)` added (Concealment the only caller; the Roster push step the only writer of the replicated teammate `IsConcealed`) — architecture-review fix 2026-10-25 (Conflict 5).
- Config applied 2026-10-25 (phase 2, user-authorised): `net.Iris.UseIrisReplication=0` pinned in `Config/DefaultEngine.ini [ConsoleVariables]` (legacy net driver, AQ-01 closed as recommended); startup log shows `Set CVar [[net.Iris.UseIrisReplication:0]]`. The harness assertion is still owed (ADR-0008).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §6, §5, Key Interfaces, guidelines, risks — user decision 2026-10-25: the `T_due` carrier is option (a), an Owner-only `double T_due` in `FTeamViewOwnerPart` (cross-review C-W5 / AQ-02 closed; ADR-0019 §6 consumes it).
- §5, Key Interfaces — user decision 2026-10-25: the owner part gains Owner-only `WrongTargetTally` and `LastResultSeq` (Penalty CR-19; written only on reclaim, rejoin and `OnRosterReset`); the tally replacing the strike counter is confirmed (Penalty OQ-NKP-1).
- §5, Key Interfaces — user decision 2026-10-25: the teammate entry gains Team-audience `RetireAtServerTime` (Teammate Markers CR-13 / OQ3), passed through `RetireBinding(Pawn, RetireAtServerTime)`.
- §5, Key Interfaces — user decision 2026-10-25: the IVP fallback feed entry gains a wrapping `uint8 Seq` (Markers CR-11c / OQ14; ADR-0003 builds it).
- §5 — user decision 2026-10-25: a team-only `bPawnBound` per teammate entry (Markers CR-12) is declined for MVP; the feed inference (TM-R4a) stands.

User scope decisions (2026-10-25: "accept all recommendations"); decision unchanged in substance, Status unchanged (Accepted).
- §5, Key Interfaces — user decision 2026-10-25: new feature part `UTeamPingViewComponent` (`FTeamPingView`, one slot per team member) for Informant Pings & Team Communication (moved to MVP; `design/gdd/informant-pings-team-comms.md` PNG-R6). Team audience, change-only, coalesced per server frame like every part; IVP R3 `TeamPing` fact row and allowlist row added. The send path is the controller RPC of ADR-0011's amendment, never the pawn.
- §5, Key Interfaces — user decision 2026-10-25: the Roster teammate entry gains Team-audience `bReconnecting` (one bit; Session OQ7 closed; IVP R3 `TeammateReconnecting` row; Session rule 24 amended), derived from the existing `NotifyConnectionChanged` projection; consumers HUD-R10a and Teammate Markers TM-R11.

Spike results (2026-10-25, production/qa/spikes/2026-10-25_S1-S2-S14.md): S14 PASS (no sample Blueprint reads other clients' `PlayerState`; prototype client helpers already scheduled for replacement), S2 PASS (owner-only links + PlayerState trims keep login, player list, owner HUD and possession working; remote markers break as expected until C-TeamView carriers exist), S1 identity links never sent to non-owner connections with trims on (0 vs 97 sends; legacy driver, property-level logging, one-process PIE, ~2 % loss — not a byte-exact or multi-machine test). Amendments from the spikes:
- §PlayerState trims — set the replicated-ping flag inside the PlayerState class itself (`SetShouldUpdateReplicatedPing` is protected in 5.8.4).
- §Parity — the sample AnimBP branches on `IsLocallyControlled`, so on the listen host NPCs and remote players animate differently; ADR-0013 must remove this branch for every pawn (host-side tell).
- §Parity — add the sample's `CharacterInputState` and `TraversalResult` replicated properties to the parity allowlist audit; `RemoteViewPitch16` differs between player and NPC pawns today (measured) and must be driven from `Look` for both (ADR-0003).
- §Possession cleanup — stale `PlayerState`/`Owner` on the previous owner's old body is confirmed; the cleanup step is required.
- Still open (condition carried): rejoin, seamless travel, PawnRecovery and a multi-process/multi-machine S1 run.
