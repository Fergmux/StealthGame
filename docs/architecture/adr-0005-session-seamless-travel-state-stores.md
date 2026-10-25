# ADR-0005: Session, seamless travel and match-scoped state stores

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed (awaiting user acceptance)

## Date
2026-10-24

## Last Verified
2026-10-24 (against UE 5.8.4 source at `C:/Program Files/Epic Games/UE_5.8/Engine/Source` and the 2026-10-24 reconciled Session & Travel, Participant State & Roster and Round & Match Framework GDDs)

## Decision Makers
Design pipeline (autonomous run, `/architecture-decision --review full`); technical-director (TD-ADR) and Unreal engine-specialist validation folded into one review; user acceptance pending.

**Review record (2026-10-24):** TD-ADR **CONCERNS** (8: `FRoundParams` written before the GameState exists; E10 wrong — `APlayerState` broadcasts entered/left/renamed messages; rejoin should adopt the retained PlayerState in `InitPlayerState`; `PreLogin` cannot strip options and `Super::InitNewPlayer` reads/rewrites them; `ApproveLogin` backstop never runs; host has no `UNetConnection`; arrival order vs `BeginPlay`; carried PlayerStates of disconnected participants). Engine validation **BLOCKING** on the first two (both small amendments) plus notes (`Logout` sees a null connection; pending-handshake reaping; replication-system `ensure` on travel). **All revised in place** (autonomous run, recommended option): §1 hook table rewritten, E10 corrected and E15-E22 added, §2 peer key and pending-slot reaping, §4 arrival order, §5 adoption-based rejoin and carried-PlayerState policy, guidelines 4/4a/4b, risks R-1/R-1b. Gates not re-run on the revised text; `/architecture-review` in a fresh session is the next independent check.

## Summary
Ten players must stay together across an evening of rounds, move into each round's seeded city in one seamless travel, survive disconnects and rejoins, and never let a join, leave or reconnect identify a human in the crowd (Session & Travel, Roster, Round). This ADR fixes the mechanism: `AGameModeBase` with no match-state machine and no inactive-PlayerState reuse; four game-instance stores (Session, Roster, Round, Tuning preset) that are the only authoritative match state; engine seamless `ServerTravel` with `?Seed=`/`?Preset=`/`?Round=` server options, a re-bind of the replaced PlayerController and PlayerState on arrival, and Session's single server-requested round acknowledgement; admission gates in `PreLogin`; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; the engine's default login pawn spawn disabled; and no host migration.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4) |
| **Domain** | Networking (game framework: login, travel, connections); Core (subsystem lifetime) |
| **Layer** | Foundation |
| **Knowledge Risk** | HIGH for networking in 5.8 per VERSION.md; this ADR uses only long-standing `AGameModeBase` / `APlayerController` / `UNetDriver` hooks, each re-read in the 5.8.4 source below, so the residual risk is behavioural (spikes S3, S4, S5), not API names |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/networking.md` (no seamless-travel or PlayerState material), `breaking-changes.md`, `deprecated-apis.md`; 5.8.4 source: `Engine/Classes/GameFramework/GameModeBase.h`, `Engine/Private/GameModeBase.cpp`, `Engine/Private/PlayerController.cpp`, `Engine/Private/PlayerState.cpp`, `Engine/Private/Controller.cpp`, `Engine/Private/LevelActor.cpp`, `Engine/Private/World.cpp`, `Engine/Classes/Engine/NetDriver.h`, `Engine/Classes/Engine/NetConnection.h`, `Core/Public/Misc/NetworkVersion.h`, `Engine/Public/Subsystems/GameInstanceSubsystem.h` |
| **Post-Cutoff APIs Used** | None new. Note the 5.6 change: `DispatchPostLogin` is deprecated and its work moved into the protected virtual `OnPostLogin`, run at the end of `PostLogin` [V `GameModeBase.h:327-334`]; overrides use `PostLogin`/`OnPostLogin`, never `DispatchPostLogin`. `UE_LOGF` for logging (ADR-0007). |
| **Verification Required** | **S3** seamless and hard travel in multi-process standalone (PlayerController/PlayerState replacement, re-bind, C-TeamView re-push, same-map travel through the transition map, game-instance stores intact). **S4** reconnect: retained PlayerState re-attached to a new controller with no player-array change on any client; two clients on one machine with `-IdentityKey=`; supersede. **S5** listen-host loss: quit notice and crash detection bounds (F6). Engine timeout backstop values effective in packaged Development builds. |

**Engine facts used:**

| # | Fact | Tag | Evidence |
|---|---|---|---|
| E1 | `AGameModeBase` exposes `PreLogin`, `PreLoginAsync` (default calls `PreLogin`), `Login`, `PostLogin`, `Logout`, `HandleSeamlessTravelPlayer`, `PostSeamlessTravel`, `GetSeamlessTravelActorList`, `SwapPlayerControllers`, `InitSeamlessTravelPlayer`, `ProcessServerTravel`, `bUseSeamlessTravel` | [V] | `GameModeBase.h:223-579`; `GameModeBase.cpp:700-705` |
| E2 | Remote clients reach `PreLoginAsync` from the `NMT_Login` handler; the listen host's local player is created by `SpawnPlayActor` → `Login` → `PostLogin` and **never passes `PreLogin`** | [V] | `World.cpp:7556`; `LevelActor.cpp:~1087-1111` |
| E3 | `Login` calls `GameSession->ApproveLogin(Options)` (capacity / spectator checks) before spawning the controller | [V] | `GameModeBase.cpp:707-718` |
| E4 | `HandleStartingNewPlayer` (BlueprintNativeEvent) calls `RestartPlayer` unless spectating; it runs after `PostLogin` **and** at the end of `HandleSeamlessTravelPlayer` | [V] | `GameModeBase.cpp:1068-1076`, `:640` |
| E5 | Default seamless travel **replaces** each PlayerController (`SpawnPlayerControllerCommon` → `SeamlessTravelTo/From` → `SwapPlayerControllers`) and its PlayerState (old PS `SeamlessTravelTo(new)` → `DispatchCopyProperties`, then destroyed; engine comment "@fixme ... incorrect 'player left the game'/'player entered the game' messages") | [V] | `GameModeBase.cpp:607-640`; `PlayerController.cpp:3654-3662`; `PlayerState.cpp:413-421` |
| E6 | `GetSeamlessTravelActorList` keeps every PlayerState (and, to the transition map, the GameMode, GameState and GameSession) | [V] | `GameModeBase.cpp:539-559` |
| E7 | Seamless travel is **disabled in single-process PIE** unless `net.AllowPIESeamlessTravel=1` (or `-MultiprocessOSS`); PIE otherwise falls back to hard travel with a warning | [V] | `GameModeBase.cpp:46-51, 504-510` |
| E8 | The server sends the travel URL to every client through `ClientTravel` (only `?listen` is stripped), so `?Seed=`, `?Preset=`, `?Round=` reach clients | [V] | `GameModeBase.cpp:393-423` |
| E9 | On controller destruction `APlayerController::CleanupPlayerState` calls `PlayerState->OnDeactivated()`, whose default is `Destroy()`; `OnReactivated` exists | [V] | `PlayerController.cpp:1392-1400`; `PlayerState.cpp:128-134`; `PlayerState.h:259-262` |
| E10 | `AGameModeBase` itself has no `BroadcastLocalized` (that lives in `AGameMode`), **but `APlayerState` sends "entered" / "left" / "renamed" engine messages to every PlayerController regardless of game mode** (on `Destroyed`, on a name change) unless `ShouldBroadCastWelcomeMessage` returns false; `AddInactivePlayer`/`FindInactivePlayer` (address-keyed dedupe) exist only in `AGameMode` | [V] | `GameMode.cpp:590`; `PlayerState.h:225`, `PlayerState.cpp:199-221, 311-350`; Roster R4 |
| E11 | `UNetDriver::InitialConnectTimeout`, `ConnectionTimeout`, `TimeoutMultiplierForUnoptimizedBuilds`, `bNoTimeouts` | [V] | `NetDriver.h:936-959, 1254` |
| E12 | `UNetConnection::LastReceiveTime` and `LastReceiveRealtime` (double) are updated on packet receipt | [V] fields / [U] keepalive cadence | `NetConnection.h:481-482` |
| E13 | `FNetworkVersion::GetLocalNetworkVersionOverride` and `IsNetworkCompatibleOverride` delegates; `SetProjectVersion` | [V] | `NetworkVersion.h:55-60, 106` |
| E14 | `UGameInstanceSubsystem` lives as long as the `UGameInstance` (survives seamless and hard travel) | [V] class / [D] lifetime | `GameInstanceSubsystem.h:16` |
| E15 | `APlayerController::PostInitializeComponents` calls `InitPlayerState` (virtual), which spawns the PlayerState owned by the controller; the PlayerState then adds itself to the GameState's array | [V] | `PlayerController.cpp:1069-1076`; `Controller.cpp:652-690`; `PlayerState.cpp:139-150` |
| E16 | `InitGame` runs from `SetGameMode` **before** the GameState exists; the GameState is spawned in `PreInitializeComponents` and `InitGameState` is the first point it can be written | [V] | `World.cpp:6034`; `GameModeBase.cpp:116-143` |
| E17 | `PreLogin`'s `Options` is const; `Login` rebuilds options from the join URL; `Super::InitNewPlayer` reads `SpectatorOnly`, `Name` (truncated) and registers `PlayerId`/`UniqueId`, calling `ChangeName` | [V] | `GameModeBase.h:293`; `LevelActor.cpp:1076-1082`; `GameModeBase.cpp` `InitNewPlayer` |
| E18 | `ApproveLogin` (called by default `PreLogin` and by `Login`) checks `AtCapacity`, which counts live controllers (including a not-yet-closed superseded one) and honours `net.MaxPlayersOverride` / `?MaxPlayers=` | [V] | `GameModeBase.cpp:176-187, 682-693, 712-716`; `GameSession.cpp:21, 156-159, 322-340` |
| E19 | On seamless travel the net driver moves to the new world and `SwapPlayerControllers` copies the `NetConnection`; `PostSeamlessTravel` handles only controllers already loaded (the host); remote controllers arrive later through `ServerNotifyLoadedWorld` → `HandleSeamlessTravelPlayer`, after world `BeginPlay` | [V] | `World.cpp:8842, 9094-9097`; `GameModeBase.cpp:568-576`; `PlayerController.cpp:697-727` |
| E20 | By the time `Logout` runs, the controller's `Player`/`NetConnection` are already null (`OnNetCleanup`); `UNetConnection::GNetConnectionBeingCleanedUp` identifies the closing connection | [V] | `PlayerController.cpp:1467-1486` |
| E21 | `GetSeamlessTravelActorList` keeps the whole player array, so PlayerStates of disconnected participants (no controller) are carried and re-added by the new GameState without passing `HandleSeamlessTravelPlayer` | [V] | `GameModeBase.cpp:539-559`; `GameStateBase.cpp:71-74` |
| E22 | Seamless travel `ensure`s if the destination game mode wants a different replication system than the source | [V] | `World.cpp:8883-8889` |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (owner-only `PlayerState`/`Controller`/`Owner`, frozen player array §4.4, C-TeamView carrier, legacy NetDriver); ADR-0004 (`ClientHash`, `TuningSchemaVersion`, game-instance preset store) |
| **Enables** | ADR-0002 (queue and clock live in the new world each round; mailboxes keyed by `ParticipantId` survive controller replacement); ADR-0006 (`ServerSecret`/`MatchNonce` held in the Session store); ADR-0010 (named spawn site, `PawnLeavingGame`); ADR-0019 (rejoining informant adoption); ADR-0024 (dedicated server reuses every admission/travel/liveness rule) |
| **Blocks** | Foundation epics "Session & Travel" and "Participant State & Roster" (store container), the round-loop story, every multi-process test (ADR-0008) |
| **Ordering Note** | ADR-0001 states *what* must not churn (player array, pawn pointers); this ADR states *how* the login, travel and disconnect paths avoid churn. Accept after ADR-0001. Spikes S3/S4/S5 must report before the round-loop story closes. |

## Context

### Problem Statement
Unreal's defaults for a multiplayer session are hostile to Smoking Mirror's first pillar and to a multi-round evening: `AGameMode`'s match-state machine fights Round's five phases; its inactive-PlayerState mechanism keys reclaim on address (two clients on one machine collide) and flips a replicated inactive flag (a time-stamped disconnect tell); the default login spawns a pawn for every player (a pawn appearing without a preceding death identifies a human); seamless travel replaces controllers and PlayerStates; single-process PIE silently falls back to hard travel; and the engine's connection timeout is the only liveness signal. Every Foundation system above Session (Roster, Round, Tuning, Visibility Policy) needs a decided container that survives travel and a decided arrival path. Architecture.md lists this as a Must-have ADR with spikes S3, S4 and S5.

### Constraints
- **Listen server, direct join, no OSS** (MVP; OnlineSubsystem Null). Capacity `2 × TeamSize` (≤ 10).
- **Pillar 1:** no join/leave/reconnect is observable mid-round by a non-teammate (Session rules 22, 24, 25); the player array is frozen in-round (ADR-0001 §4.4).
- **Single writers:** Session owns `ConnectionState`; the Roster owns lifecycle state (via `RequestTransition`); Round owns phase and every consequence (Roster R11, Round R17).
- **Bounded waits:** every wait has a server-measured bound (Session F7; Round R6 watchdog; Round R7 `RevealPawnWaitMaxSec`, cross-review S-B4).
- **No engine patches** (no source-built engine in MVP).
- **Prototype:** `AStealthGameMode : AGameModeBase` with `bUseSeamlessTravel = false` (Roster OQ4) is evidence only.

### Requirements
- R1 One authoritative container per match-scoped store, surviving seamless and hard travel, cleared by `BeginMatch`/`EndMatch` on every session exit path (Roster R3, OQ3; Round R4; TDC CR-4).
- R2 Admission gates in Session rule 7 order at the start of the handshake, host seeded directly (rules 2, 7-10).
- R3 Round travel: rule 11 ordering, `?Seed=` server option, `FRoundParams` as the only client source, single server-requested acknowledgement with one absolute deadline (rules 11.1-11.9, F7).
- R4 Persistence: records in stores; PlayerStates/controllers/pawns are views; re-bind on both arrival paths (rules 16-17; Roster R4).
- R5 Liveness: Session's clamped accumulator, Soft/Hard thresholds, engine timeouts as a backstop (rules 18-19, F2, F3).
- R6 Rejoin, supersede, window, auto-rejoin, `RejoinToken` (rules 6, 15, 21, 23; Roster R11).
- R7 No pawn or PlayerState created, freed, hidden or deactivated by a connection event; default pawn spawn disabled; one named player-pawn spawn site (rule 22; Roster R9).
- R8 No host migration; session-ending notice; client host-loss detection (rule 4, F6).
- R9 Host commands on the game instance with a net-mode check (Round R22).

## Decision

### 1. Game mode: `AGameModeBase`, seamless travel on, engine defaults overridden

`AStealthGameMode : AGameModeBase` (never `AGameMode`), `bUseSeamlessTravel = true`, `bStartPlayersAsSpectators = false`. Overrides:

| Hook | Override |
|---|---|
| `PreLogin` (remote only, E2) | Session admission gates in rule 7 order (§3); fills `ErrorMessage` with the reason code. It **cannot** strip options (`Options` is const, E17) and does not call `Super` (whose `ApproveLogin` capacity check would refuse a supersede at full capacity, E18) |
| `Login` | Before `Super`, look up the key: on a **reclaim** stash the retained PlayerState as a pending reclaim for the controller about to spawn (§5) |
| `InitNewPlayer` | **Overridden without calling `Super`** (E17): parse only the allowlisted keys `IdentityKey`, `Name`, `RejoinToken`; ignore `SpectatorOnly`, `SplitscreenCount` and every other option; set the sanitised display name without `ChangeName` broadcasting (rule 10); never re-register `PlayerId`/`UniqueId` for a reclaimed PlayerState |
| `GameSession` | `AStealthGameSession::ApproveLogin` always returns success (E18): capacity, spectators and admission are decided once, in `PreLogin`. `MaxPlayers` is not used as a backstop (it would count the superseded controller and refuse the supersede) |
| `PostLogin` | `Session.BindConnection` → Roster `RegisterParticipant` (fresh login) or `RebindView` (reclaim). The host's local player arrives here without `PreLogin` (E2) and has **no `UNetConnection`**: its key is the reserved `FSessionPeerKey::Host` (§2), its `IdentityKey` comes in-process from `UStealthIdentitySubsystem` (never from URL options), and its record was seeded by `OpenSession`, which runs **before** the host's `?listen` map load (rule 2) |
| `HandleStartingNewPlayer_Implementation` | **No-op**: never `RestartPlayer` (E4). Pawns come only from Round's mixed spawn through `UStealthPawnFactory::SpawnSharedPawn` (ADR-0010, the one named spawn site, Roster R9) |
| `PlayerCanRestart_Implementation` | Returns **false** always (amendment 2026-10-24, ADR-0019 §4.4): a pawnless PlayerController (body released, informant awaiting a body) can never reach `RestartPlayer` through `ServerRestartPlayer`/`StartFire` |
| `HandleSeamlessTravelPlayer` | Call `Super` (new controller and PlayerState, E5), then `Session.BindConnection` → Roster `RebindView(Id, NewPlayerState)`, which writes the view fields from the record and re-pushes C-TeamView (Roster R4, R6). Remote controllers arrive here one by one after world `BeginPlay` (E19) |
| `PostSeamlessTravel` | Mark the server's own map load (`T_srvload`, rule 20) so Session lifts its forced `Closed` override; rebind the **host** controller (the only one already loaded, E19); apply the carried-PlayerState policy (§5) |
| `GetSeamlessTravelActorList` | On **lobby** travel, drop the PlayerStates of participants that are not connected (they become `Dormant`, rule 12); on round and fallback travel keep the engine default (E21) |
| `Logout` | `Session.NotifyConnectionClosed(Reason)` resolving the participant from `UNetConnection::GNetConnectionBeingCleanedUp` or a weak controller→participant map, because the controller's connection is already null here (E20); no Roster call from the game mode |
| `InitGame` | Parse and validate `?Seed=`, `?Preset=`, `?Round=` (server only) into members; on any malformed value raise `OnTravelFailed(BadParams)`; never substitute a default (rule 11.4). **No GameState write here** (E16) |
| `InitGameState` | Write `FRoundParams` to the new GameState (first point it exists, E16); Round P1 fires from here |

`AStealthPlayerState` overrides:
- `ShouldBroadCastWelcomeMessage` returns **false always** (E10): no "entered", "left" or "renamed" engine message ever reaches a client, in any phase (Session rule 25c). The lobby's join/leave lines (rule 24) come from the Lobby UI reading Session events, not from engine messages.
- `OnDeactivated` (E9) is **phase-conditional**: while admission is `RejoinOnly` (Preparing, Active, Ended) it does not destroy, so the PlayerState stays active, in the player array and with its replicated fields unchanged until the next travel (rule 22, ADR-0001 §4.4); in `Open`/`Closed` (Lobby, Reset) it destroys as normal (the record goes `Dormant`, rule 13).
- `CopyProperties` carries nothing authoritative (Roster R4).

`AStealthPlayerController::PawnLeavingGame` keeps the pawn for the AI hand-over (ADR-0010, spike S8). Every game mode used by the project stays on the legacy replication system, because seamless travel `ensure`s when source and destination differ (E22; ADR-0001 §7).

### 2. Match-scoped state: four game-instance stores, plain-C++ cores

| Store (`UGameInstanceSubsystem` facade) | Core (plain C++) | Owns | ADR for its contents |
|---|---|---|---|
| `USessionService` | `FSessionCore` | connection table (`ConnectionState`, generations, silence clocks, ack records keyed by `ParticipantId`), connection→participant map, kick list, admission mode, `ServerSecret` + `MatchNonce` + `UsedSeeds`, travel deadline | this ADR; seeds ADR-0006 |
| `URosterStore` | `FRosterCore` | `ParticipantRecord`s, bindings, tombstones, `MatchId`, `RoundEpoch`, `EventSeq` | ADR-0001 (audiences), Roster GDD |
| `URoundMatchStore` | `FRoundPhaseCore` | five-phase machine, match score, `RoundsPlayedCount`, Event Log archives and the round-scoped header across a fallback travel (rule 11.9) | ADR-0002 (queue/latch), Round GDD |
| `UStealthTuningStore` | `FTuningPipelineCore` | preset store, session-latched keys, pipeline state | ADR-0004 |

Rules:
- **These four stores are the only authoritative match state.** GameMode, GameState, PlayerController, PlayerState and pawns hold views or round-local objects only; nothing authoritative is stored on a controller or PlayerState, because seamless travel replaces both (E5).
- **Lifetime:** `BeginMatch` runs when the host opens the session (`Session.OpenSession`: draws `ServerSecret`/`MatchNonce` (ADR-0006), validates session-latched tuning (ADR-0004), `Roster.BeginMatch(TeamSize)`, checks engine timeouts (§6)). `EndMatch` runs on **every** session exit path: host quit, host return to main menu, `OnSessionEnding`, `UGameInstance::Shutdown`, and a `FWorldDelegates::OnWorldCleanup` of the last networked world; each store clears itself, so a former host keeps nothing (Roster R3 note on listen-host memory).
- **Server-only:** every mutating API asserts authority and the game thread; on a client process the stores exist but stay empty (a client may later host).
- **World bindings are weak and re-bound per world:** the composition root (`AStealthGameMode::InitGame` / `OnWorldBeginPlay`, ADR-0007) re-registers world-scoped providers and binds delegates with weak bindings after every travel; stores hold `TWeakObjectPtr` to world objects and never strong pointers across travel.
- **Connection identity across seamless travel** is `FSessionPeerKey`: the `UNetConnection` for remote peers (it moves with the net driver into the new world and is copied to the new controller, E19; the controller itself does not survive, E5), and the reserved value `Host` for the listen host's local player, which has no connection. `FSessionCore` maps `FSessionPeerKey → ParticipantId`; mailboxes, ack records and feature state are keyed by `ParticipantId`, never by controller or PlayerState (Session rule 11.6, Round R12).
- **`RebindView` is independent of world wiring:** it writes only the PlayerState view fields and C-TeamView from the Roster store and does not depend on providers registered in `OnWorldBeginPlay`; the host's rebind in `PostSeamlessTravel` therefore runs safely before world `BeginPlay` (E19), and any consumer that needs world wiring subscribes to Roster events instead. **The rebind event** (amendment 2026-10-24, ADR-0020 §6): `URosterStore::OnParticipantViewRebound(FParticipantId)` (the Roster GDD's `OnViewRebound(ParticipantId)`; same event, this is its C++ name) is raised synchronously on the server at the end of every `RebindView` (reclaim at rejoin, seamless arrival), after the Roster part is re-pushed; feature team-view parts (e.g. `UObjectiveTeamViewComponent`) subscribe weakly from the composition root and rewrite that participant's part inline in the handler. A rebind that runs before world wiring (the host's in `PostSeamlessTravel`) reaches no feature subscriber, which is correct because feature parts are round-scoped and start empty.
- **Strike-limit release inside the transition** (architecture-review fix 2026-10-25; requested by ADR-0019 item 8, Roster R8/R9, Penalty CR-3): `FRosterCore::RequestTransition(P, Informant, StrikeLimit, ExpectedEpoch)` **releases, not retires**, the participant's live binding. In the same call the Roster runs its own `ReleaseToCrowd(Pawn)` (binding erased, pawn `Unbound`; the `Rejected_NotDisconnected` rule does not apply on this internal path), so no caller invokes `ReleaseToCrowd` separately for an exile. The caller (Penalty, ADR-0019 §2.3) pairs it with `UAppearanceAllocator::ReleaseCard` in the same call stack (ADR-0012 pairing rule; card stays `NpcLive`), and the hand-over of the pawn to Crowd is ADR-0010's. Every other `Active → Informant` reason still retires the binding.
- **Pending handshakes are reaped by Session:** a client that passes `PreLogin` but never sends Join produces no game-mode callback, so Session's liveness tick scans its pending slots against the net driver's `ClientConnections` and releases any older than `PendingLoginTimeoutSec` (rule 9) [U exact field access; S4].

### 3. Admission (Session rules 5-10, F1)

- `PreLogin` evaluates, in order: (1) network version — the project protocol version is folded into the engine handshake through `FNetworkVersion::GetLocalNetworkVersionOverride` / `IsNetworkCompatibleOverride` (E13), so a mismatch fails before `PreLogin` → `VersionMismatch`; (2) `TuningSchemaVersion` equality (ADR-0004 `UStealthTuningStore::GetSchemaVersion`) → `VersionMismatch`; (3) kick list → `Refused`; (4) admission mode × known key (rule 8 table; `ShouldAdmitExpired` callback into Round) → `InProgress` / `Busy`; (5) capacity F1 → `Full`. A `RejoinToken` mismatch on a reclaim or supersede → `Refused` (rule 6, provisional; hard gate before the first external playtest).
- A handshake accepted by `PreLogin` holds a pending slot until `PostLogin` or `PendingLoginTimeoutSec`.
- Rejection text is shown only to the rejected client; nothing is broadcast (rule 24).
- **IdentityKey:** a client-side `UStealthIdentitySubsystem` (game instance) loads or creates the 128-bit key in a local file under `Saved/` on first launch, honours `-IdentityKey=`, and derives a per-instance key in PIE from the PIE instance index [U API name; spike S4]; it is sent only as a login option, never logged beyond a 4-byte prefix, never replicated.
- Display names are sanitised and de-duplicated in Session, fixed for the match, never used for identity (rule 10).

### 4. Round travel and the single acknowledgement (rule 11, F7)

```
Lobby world                              │ seamless ServerTravel                      │ Match world (server order, E16/E19)
Round: lock → ResetAll → Preparing       │                                            │ InitGame: parse + validate ?Seed ?Preset ?Round
       → RejoinOnly → RequestRoundTravel │ Session: draw Seed (ADR-0006), Closed,     │ InitGameState: FRoundParams on GameState (P1)
                                         │  Traveling, t_travel, InvalidateAllBindings │ PostSeamlessTravel: T_srvload, lift Closed,
                                         │  OnRoundTravelStarted                      │   host rebind (only loaded controller)
                                         │  ServerTravel(Match?Seed=&Preset=&Round=)  │ world BeginPlay / OnWorldBeginPlay wiring
                                         │  (URL also reaches clients, E8)            │ remote ServerNotifyLoadedWorld → HandleSeamlessTravelPlayer
                                         │                                            │   (per player): Super (new PC + PS) → BindConnection → RebindView
                                         │                                            │ TDC BeginResolve → snapshot; LSC → FRoundReadiness (t_ready)
                                         │                                            │ Session → ClientRequestRoundAck(FAckEpoch) per bound PC
                                         │                                            │ client → ServerRoundAck(FAckEpoch, Seed, LayoutAttempt,
                                         │                                            │          LocalLayoutHash, ClientHash, PoolHash)
                                         │                                            │ match → Connected, OnParticipantTravelComplete
                                         │                                            │ Deadline = max(t_travel+TravelLoad, t_ready+AckGrace)
```

- `RequestRoundTravel(MapId, PresetId, RoundIndex)` and `CanRequestRoundTravel()` implement rule 11.2's checks and refusal codes with no side effect on refusal.
- **The travel URL is a server-side carrier, but it is not private** (E8): clients receive it. Design rule: **nothing secret ever goes into a travel URL** (only `Seed`, `PresetId`, `RoundIndex`, all public), and client code never parses travel options; `FRoundParams` (replicated, audience All, one struct) is the only client source (rule 11.4; resolves AQ-17: the seed reaches clients by both paths, and only the struct is read).
- **Transition map:** a dedicated empty `TransitionMap` is configured in `GameMapsSettings` (project config, applied by the implementing story); same-map lobby↔match travel goes through it [U, S3, Session OQ1]; if S3 fails, `LobbyMap` and `MatchMap` are distinct maps from day one.
- **Acknowledgement:** owner-only reliable RPCs on the (new) PlayerController: `ClientRequestRoundAck(FAckEpoch)` and `ServerRoundAck(...)` (rule 11.6). The server derives the participant from the receiving connection, never from a parameter. Ack records live in `FSessionCore`, keyed by `ParticipantId`, cleared at every epoch (travel start, TDC `Revision` change), never replicated. Explicit resend once with `AckSeq + 1`, then `ParamsMismatch`. The host is acknowledged through the same code path locally, with no RPC.
- **Deadline:** one server-measured `Deadline` per travel (F7); no client message, rejoin or supersede moves it.
- **Simultaneous reveal:** clients hold the loading screen until Round's reveal (rule 11.8, Round R7), bounded client-side by `RevealPawnWaitMaxSec` after `ActiveAtServerTime` (cross-review S-B4) and dropped at once on `Ended`; a rejoiner drops it when its acknowledgement completes.
- **World-ready stage (Round CR-13(d)) is adopted (user decision 2026-10-25)** as a separate owner-only reliable server RPC on the client's own PlayerController, `ServerWorldReady(FAckEpoch)`, sent once per epoch after the participant is `Connected` and while the phase is `Preparing`, when the client holds `ExpectedPawnCount` pawns with resolved `AppearanceId`s (Session rule 11). Session validates it like the first stage (bound connection, current `FAckEpoch`, `Connected`), records it in the `ParticipantId`-keyed, never-replicated bookkeeping and raises server-only `OnParticipantWorldReady(ParticipantId)` for Round P7. It never fails and is never a disconnect reason; the first stage is unchanged. The S-B4 bound stays as the client-side backstop.
- **Fallback travel** (rule 11.9) and **lobby travel** (rule 12, travel cancel) reuse the same path with their own `t_travel` and epochs.
- **PIE:** single-process PIE cannot exercise seamless travel by default (E7). Multi-client PIE tests set `net.AllowPIESeamlessTravel=1` only for smoke checks and label the result non-representative; every travel acceptance test and spike S3 runs **multi-process standalone** (ADR-0008).

### 5. Persistence, disconnect, rejoin and supersede

- **Seamless arrival:** E5 replaces controller and PlayerState; `HandleSeamlessTravelPlayer` override re-binds (`RebindView`), and the Roster rewrites the view fields from the record. The PlayerState replacement happens at the travel boundary, behind every client's loading screen, for every participant identically; it is never caused by a connection event (rule 17 note). The old PlayerState's destruction would send a "left the game" engine message (E10, the engine's own "@fixme"); `ShouldBroadCastWelcomeMessage = false` (§1) suppresses it and every other engine join/leave/rename message.
- **Carried PlayerStates of disconnected participants (E21):** on **round and fallback travel** they are kept (the player array stays frozen across the travel; ADR-0001 §4.4) and Session rebinds them to their records in `PostSeamlessTravel` (no controller, so they never pass `HandleSeamlessTravelPlayer`); on **lobby travel** they are dropped from the travel list, matching their records becoming `Dormant` (rule 12).
- **Disconnect:** `Logout` (socket close, quit) or a Session-initiated close (Hard timeout, kick) → `ConnectionState` change → Roster `NotifyConnectionChanged` → events to Round. The participant's `ConnectionGeneration` bumps only on entry of the projection into `Disconnected` (a supersede of a live connection is such an entry; `Rejoining`, `Expired` and a failed rejoin, which stay `Disconnected`, bump nothing; decision D45, Roster R11), and Round's grace expiry carries that generation (`ExpectedConnectionGeneration`). A kick that closes an `Expired` connection is a legal Disconnect: the Roster accepts `Disconnect` for a participant whose Session connection is `Expired` (Round CR-21, kick path; closeout 2026-10-25 D9). Round also receives `OnLobbyTravelComplete` / `OnRoundTravelStarted` from Session and handles `OnParticipantDisconnected` raised during travel. The PlayerController is destroyed by the engine; its PlayerState survives (`OnDeactivated` override, E9); its pawn survives via `PawnLeavingGame` (ADR-0010) and is handed to Crowd by Round (R17), never by Session.
- **Rejoin (fresh login, known key):** `PreLogin` admits per rule 8. `Login` stashes the retained PlayerState as a **pending reclaim** before `Super` spawns the controller. `AStealthPlayerController::InitPlayerState` (E15) checks for a pending reclaim and, if present, **adopts** the retained PlayerState (`SetPlayerState`, `SetOwner(this)`, `OnReactivated`) instead of spawning a fresh one, so no new PlayerState ever exists and the player array never gains or loses an entry on any client. `InitNewPlayer` (no `Super`, §1) does not re-register ids or rename. The participant is `Rejoining` until its acknowledgement (rule 21). For a returning informant, Round's R18 readmission calls Penalty's `IInformantBodyProvider::OnInformantReadmitted` (declared by Round, registered by the composition root; ADR-0019) only **after** that acknowledgement. Spike S4 verifies that the ownership change of the retained PlayerState is not visible to non-owners beyond what ADR-0001 allows (owner-only `Owner`) [U]. Fallback if adoption fails in S4: spawn the fresh PlayerState and destroy it in the same frame before any replication tick — a never-replicated dynamic actor sends nothing when destroyed on the legacy driver [V `NetDriver.cpp:4491-4520`, engine review], and the welcome-message suppression covers its `Destroyed`.
- **Supersede (rule 15):** drop the old connection→participant mapping first, close the old connection with `Superseded`, then bind the new one; in-round, `OnParticipantDisconnected(Superseded)` at that instant and `OnParticipantRejoined(Superseded)` at rejoin completion (decision D24, amendment below); in Lobby only the latter; latest connection wins.
- **Rejoin window and auto-rejoin:** Session's server clocks (rule 21); client auto-rejoin from `UStealthIdentitySubsystem` with the cached address and key (rule 23).
- **Lobby:** a Lobby disconnect makes the record `Dormant` at once (rule 13); its PlayerState is destroyed through the normal path (`OnDeactivated` is phase-conditional, §1); the lobby's join/leave lines come from Session events in the Lobby UI (rule 24), never from engine messages.

### 6. Liveness and engine timeouts (rules 18-19, F2, F3, F6)

- **Silence clock:** `FSessionCore` keeps one accumulator per remote connection. A server tick function in `TG_PrePhysics` (after the NetDriver's receive dispatch at the top of the frame) adds `min(DeltaTime, LivenessMaxCountedFrameSec)` and zeroes it whenever `UNetConnection::LastReceiveRealtime` advanced since the previous tick (E12); evaluation every `LivenessCheckIntervalSec`; Soft → `Unresponsive`, Hard → Session closes the connection. Suspended while `Traveling` (the deadline applies).
- **No application heartbeat** in the MVP (rule 18); `SessionHeartbeatIntervalSec` stays 0 unless the net spike shows idle-lobby packet rates below F3's floor.
- **Engine timeouts as backstop** (E11), set in the implementing story's `DefaultEngine.ini` for the game net driver: `ConnectionTimeout ≥ max(SessionHardTimeoutSec, T_prep_max) + 30` (**≥ 111 s at the defaults with `T_srvload` at its range maximum; 120 s to be configured** (not yet set: the engine base is `ConnectionTimeout=60`, `InitialConnectTimeout=60`, `BaseEngine.ini:1855-1856`; set it under the game net driver's section, today `/Script/OnlineSubsystemUtils.IpNetDriver`); the earlier 105 s came from the `T_srvload` 10 example and sat below the Session GDD's own worst case, `EngineConnectionTimeoutSec`, F7: late case `t_ready` 66 s, Deadline 81 s, 81 + 30 = 111 s), `InitialConnectTimeout ≥ PendingLoginTimeoutSec + 10`, `TimeoutMultiplierForUnoptimizedBuilds` accounted for. `Session.OpenSession` reads the live values and logs an error on violation; tests set them explicitly because editor/PIE may run with `bNoTimeouts`.
- **Client host-loss (F6):** the client's `UStealthIdentitySubsystem` runs the same clamped accumulator on its server connection, paused from travel start until `FRoundParams` (or the lobby ack request) arrives.

### 7. Session end, no host migration (rule 4, F6)

- Deliberate host quit: Session sends `ClientSessionEnding(Reason)` (owner-only reliable RPC on every controller), waits up to `ShutdownFlushSec` or until every client has acknowledged, then `EndMatch` and shutdown. Every client shows "The session has ended." and returns to the main menu. No client ever becomes host; the engine's host-migration paths are not used.
- Host crash: each client's host-loss clock fires (F6).

### 8. Host commands (Round R22)

Host commands (`stealth.round.*`, `stealth.tuning.preset`) are registered with `IConsoleManager` by `URoundMatchStore` / `UStealthTuningStore` on the **game instance**, each checking net mode (listen server or standalone), a local controller and `ParticipantId == HostId`. They are never placed on the PlayerController, pawn, HUD or cheat manager (development `ServerExecRPC` routes a client's exec through those [V per Round R22]). Debug-only commands are compiled out of Shipping (ADR-0004 §9 sentinel rule).

### Architecture Diagram

```
                       ┌──────────────────────── UGameInstance (process lifetime) ───────────────────────┐
                       │ USessionService   URosterStore   URoundMatchStore   UStealthTuningStore           │
                       │ (FSessionCore)    (FRosterCore)  (FRoundPhaseCore)  (FTuningPipelineCore)         │
                       │  conn table,      records,       phase machine,     preset store,                │
                       │  ack records,     bindings,      score, Event Log   session keys                 │
                       │  ServerSecret     epochs         archives                                        │
                       └───────▲──────────────▲──────────────▲───────────────────▲──────────────────────────┘
          weak re-bind each world │              │              │                   │
 ┌─────────────── UWorld (recreated by every travel) ────────────────────────────────────────────────────┐
 │ AStealthGameMode (AGameModeBase)                                                                       │
 │  PreLogin gates ─► Login/InitNewPlayer (reclaim: re-attach retained PS) ─► PostLogin BindConnection    │
 │  HandleStartingNewPlayer = no-op   HandleSeamlessTravelPlayer: Super → RebindView   Logout → Session   │
 │  InitGame: ?Seed ?Preset ?Round → FRoundParams                                                         │
 │ AStealthGameState: FRoundParams, FRoundReadiness, FRoundPublicState, TuningSnapshot (views)            │
 │ AStealthPlayerController (replaced at travel): ClientRequestRoundAck / ServerRoundAck, C-TeamView      │
 │ AStealthPlayerState (replaced at travel; OnDeactivated keeps it in-round)                              │
 │ Pawns: only UStealthPawnFactory::SpawnSharedPawn (ADR-0010)                                            │
 └────────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

### Key Interfaces

```cpp
// --- Public/Server (StealthFoundation) ------------------------------------------
enum class EConnectionState : uint8 { Connecting, Connected, Unresponsive, Traveling, Disconnected,
                                      Rejoining, Expired, Dormant, Rejected, SessionEnded };
enum class EAdmissionMode   : uint8 { Open, RejoinOnly, Closed };
enum class ERejectReason    : uint8 { VersionMismatch, Refused, InProgress, Busy, Full };
enum class ETravelRefusal   : uint8 { Ok, TravelInProgress, AdmissionClosed, ParticipantUnresponsive,
                                      ParticipantConnecting, RoundScopeNotReset };
struct FAckEpoch { uint32 RoundIndex; int32 TuningRevision; uint16 AckSeq; };   // also used by lobby travel
USTRUCT() struct FRoundParams { FName MapId; int32 Seed; FName PresetId; uint32 RoundIndex; }; // GameState, All
USTRUCT() struct FRoundReadiness { uint32 RoundIndex; int32 LayoutAttempt; uint64 LayoutHash; }; // GameState, All

class USessionService : public UGameInstanceSubsystem {
public:
    bool OpenSession(const FSessionOpenParams&);                 // host; BeginMatch on all stores
    void EndSession(ESessionEndReason);                          // every exit path; EndMatch on all stores
    ETravelRefusal CanRequestRoundTravel() const;                // no side effect
    ETravelRefusal RequestRoundTravel(FName MapId, FName PresetId, uint32 RoundIndex);
    ETravelRefusal RequestLobbyTravel();
    ETravelRefusal RequestFallbackTravel(FName GrayboxMapId);
    void SetAdmissionMode(EAdmissionMode);                       // Round is the only caller
    EConnectionState GetConnectionState(FParticipantId) const;
    void KickParticipant(FParticipantId);                        // host debug; key refused for the session
    // Round's policy hook (consumer-owned interface, injected at world start)
    void SetExpiredAdmissionPolicy(TWeakInterfacePtr<IExpiredAdmissionPolicy>);  // ShouldAdmitExpired(P)
    // events (server delegates, fired in ascending ParticipantId order within a tick)
    FOnParticipantJoined OnParticipantJoined;  FOnParticipantUnresponsive OnParticipantUnresponsive;
    FOnParticipantResponsive OnParticipantResponsive;  FOnParticipantDisconnected OnParticipantDisconnected;
    FOnParticipantRejoined OnParticipantRejoined;  FOnParticipantTravelComplete OnParticipantTravelComplete;
    FOnRejoinWindowExpired OnRejoinWindowExpired;  FOnRoundTravelStarted OnRoundTravelStarted;
    FOnLobbyTravelComplete OnLobbyTravelComplete;  FOnTravelFailed OnTravelFailed;  FOnSessionEnding OnSessionEnding;
    // seeds: PublicSubSeed / GetServerSubSeed — ADR-0006
};
struct FSessionPeerKey { TWeakObjectPtr<UNetConnection> Connection; bool bIsHost; };  // Host = listen host's local player
// Game-mode adapter calls (StealthGame composition root → Session), never called by features:
//   EvaluateAdmission(Options, Address) -> TOptional<ERejectReason>   (PreLogin)
//   FindPendingReclaim(IdentityKey) -> APlayerState*                  (Login → stashed for InitPlayerState)
//   BindConnection(FSessionPeerKey, APlayerState*)                    (PostLogin / HandleSeamlessTravelPlayer / PostSeamlessTravel)
//   NotifyConnectionClosed(FSessionPeerKey, EDisconnectReason)        (Logout via GNetConnectionBeingCleanedUp / Session close)
//   OnServerRoundAck(FSessionPeerKey, const FRoundAckPayload&)        (ServerRoundAck RPC body)

// --- Owner-only RPCs on AStealthPlayerController --------------------------------
UFUNCTION(Client, Reliable) void ClientRequestRoundAck(FAckEpoch Epoch);
UFUNCTION(Server, Reliable) void ServerRoundAck(FAckEpoch Epoch, int32 Seed, int32 LayoutAttempt,
                                               uint64 LocalLayoutHash, uint64 ClientHash, uint64 PoolHash);
UFUNCTION(Client, Reliable) void ClientSessionEnding(ESessionEndReason Reason);
UFUNCTION(Client, Reliable) void ClientReceiveRejoinToken(FRejoinToken Token);   // first admission only
```

### Implementation Guidelines
1. The game mode **must** derive from `AGameModeBase`, never `AGameMode`, and **must never** use `AddInactivePlayer`/`FindInactivePlayer` or any address-keyed reclaim.
2. `HandleStartingNewPlayer` **must** be a no-op; no code path may call `RestartPlayer`, `SpawnDefaultPawnFor` or spawn a pawn outside `UStealthPawnFactory`.
3. Authoritative match state **must** live only in the four game-instance stores; nothing authoritative may be stored on a PlayerController, PlayerState, GameMode or GameState, and nothing may be keyed by controller or PlayerState pointer.
4. `AStealthPlayerState::OnDeactivated` **must not** destroy the PlayerState while admission is `RejoinOnly`, and no code may toggle `bIsInactive` or remove a PlayerState from the array between `Preparing` and the next travel (ADR-0001 §4.4). A rejoiner **must** adopt its retained PlayerState in `InitPlayerState`; no fresh PlayerState may be spawned for a reclaim.
4a. `AStealthPlayerState::ShouldBroadCastWelcomeMessage` **must** return false in every phase; no engine "entered/left/renamed" message may reach any client (Session rule 25c).
4b. `InitNewPlayer` **must not** call `Super`; only `IdentityKey`, `Name` and `RejoinToken` options may be read. `FRoundParams` **must** be written in `InitGameState`, never in `InitGame`.
5. Nothing secret **may** be placed in a travel URL or login option; client code **must never** parse travel URL options (read `FRoundParams`).
6. Every Session RPC **must** be owner-only (client RPC on the owning controller); no multicast; the participant is always derived from the receiving connection.
7. Every wait **must** be bounded by a server-measured clock; no client message may start, extend or restart a deadline.
8. Session **must never** spawn, free, hide or possess a pawn, and **must never** call `RequestTransition`; it reports facts, Round decides.
9. Ack records, kick list, silence clocks and connection maps **must** be keyed by `ParticipantId` (or `UNetConnection` for the live mapping) and never replicated.
10. Travel acceptance tests **must** run multi-process standalone; a PIE seamless-travel result **must** be labelled non-representative.
11. Host commands **must** be registered on the game instance with the net-mode/host check, never on PlayerController, pawn, HUD or cheat manager.
12. `EndMatch` **must** run on every session exit path; a store that survives into a new session is a defect.

## Alternatives Considered

### Alternative 1: `AGameMode` with its match states and inactive-PlayerState reuse
- **Description**: Use the engine's `AGameMode` (`WaitingToStart`/`InProgress`/`WaitingPostMatch`) and its built-in reconnect (`AddInactivePlayer`/`FindInactivePlayer`).
- **Pros**: Reconnect and match flow "for free"; familiar.
- **Cons**: The match-state machine fights Round's five phases; inactive reuse dedupes by address (a second same-machine client destroys the first's record, Roster R4 [V]), duplicates the PlayerState and flips the replicated inactive flag (a time-stamped disconnect tell, rule 22); `AGameMode` adds its own `BroadcastLocalized` messages on top of the PlayerState ones (E10).
- **Rejection Reason**: Breaks Pillar 1 and the Roster's key-based reclaim.

### Alternative 2: Hard travel every round (the prototype)
- **Description**: `bUseSeamlessTravel = false`; every client reloads and re-logs in each round.
- **Pros**: Simpler engine path; works in single-process PIE; no transition map.
- **Cons**: Every round is a mass disconnect/reconnect: re-running `PreLogin`/`Login` for ten clients per round, a menu-like reload, and the Roster treating each round as a mass reclaim; more failure windows (a client that fails to reconnect between rounds), longer loads (Session F4, F7).
- **Rejection Reason**: Session rule 11 and the Fellowship fantasy ("no menu between rounds") choose seamless; kept as the **fallback** if S3 shows seamless travel unreliable in 5.8.4 (the Roster already treats hard travel as a mass reclaim, Roster OQ4).

### Alternative 3: One persistent world with level streaming per round
- **Description**: Never travel; stream the round's city in and out of a persistent level.
- **Pros**: No PlayerController/PlayerState replacement; no transition map; stores could even live on the GameState.
- **Cons**: Every world-scoped system (navmesh build, queue, crowd, layout service) must reset cleanly without a world teardown; leaks between rounds become state bugs; runtime navmesh and ISM generation (ADR-0014) assume a fresh world.
- **Rejection Reason**: Higher reset risk for a 1-2 programmer team; travel gives a clean world per round for free.

### Alternative 4: Stores on the GameState / GameMode, copied across travel
- **Description**: Keep match state in world actors and carry it through `GetSeamlessTravelActorList` / `CopyProperties`.
- **Pros**: No subsystems; visible in the world outliner.
- **Cons**: GameMode and GameState are recreated by travel (they ride only to the transition map, E6); copying is error-prone and does not survive hard travel or the fallback travel; GameState is replicated, so server-only data on it is one property flag away from leaking.
- **Rejection Reason**: Violates R1 and default-deny.

### Alternative 5: Keep the same PlayerController across seamless travel
- **Description**: Override `HandleSeamlessTravelPlayer` to reuse the carried controller instead of spawning a new one.
- **Pros**: Fewer re-bindings; owner-only components on the controller persist.
- **Cons**: Diverges from the engine's tested path; the carried controller's components and timers from the old world persist silently, exactly the hidden-state risk rule 16 warns about.
- **Rejection Reason**: Rebuilding views from the store at a known point (`RebindView`) is simpler to test than auditing everything a carried controller keeps.

## Consequences

### Positive
- One container pattern for every match-scoped store; survives seamless, hard and fallback travel; testable cores.
- The default engine paths that leak (inactive flag, join/leave messages, login pawn, address reclaim) are removed by construction.
- Every wait is bounded by server clocks; no client can stall a round or extend a deadline.
- The arrival path is uniform: every view (PlayerState fields, C-TeamView) is rebuilt from the store at one point.

### Negative
- More engine overrides to keep correct across engine upgrades (PlayerState re-attach on rejoin is the riskiest, spike S4).
- Single-process PIE cannot test seamless travel representatively; the multi-process harness (ADR-0008) is on the critical path for every travel story.
- A transition map and engine timeout config are project-config changes outside this design run (applied by implementing stories).
- Owner-only C-TeamView components are recreated at every travel and must be re-pushed (Roster `RebindView`).

## Risks
- **R-1 Retained-PlayerState adoption has a wire-visible side effect** (owner change, `OnReactivated` side effects, an engine message missed by the suppression) [U]. *Mitigation:* spike S4 with raw-bunch capture (ADR-0008) and an acceptance check that no client receives `ClientReceiveLocalizedMessage` for any PlayerState in any phase; fallback: fresh PlayerState spawned and destroyed in the same frame before replication (sends nothing on the legacy driver [V]); last resort: a rejoin waits for the next travel (rejected unless S4 forces it, because it extends downtime).
- **R-1b Engine join/leave/rename messages** leak connection events [V E10]. *Mitigation:* `ShouldBroadCastWelcomeMessage = false`; leak test above.
- **R-2 Same-map seamless travel through the transition map is unreliable** [U, Session OQ1]. *Mitigation:* S3; distinct `LobbyMap` and `MatchMap` from day one if it fails.
- **R-3 Engine timeouts fire before Session's thresholds** in a build configuration (unoptimised multiplier, editor `bNoTimeouts`). *Mitigation:* startup validation in `OpenSession`; tests set values explicitly.
- **R-4 Keepalive cadence lower than assumed** in an idle lobby, inflating false Unresponsive events [U E12]. *Mitigation:* net spike measures `R_c`; enable `SessionHeartbeatIntervalSec` (F8) if F3's floor is not met.
- **R-5 A store survives into the next session** (missed exit path). *Mitigation:* `EndMatch` on every exit path listed in §2, and a test that opens two sessions in one process and asserts empty stores.
- **R-6 Controller replacement breaks an owner-only component or RPC ordering** at arrival (e.g. ack request sent before the new controller replicates). *Mitigation:* the ack request is sent only after `BindConnection` on the new controller; AC rejoin/travel tests in S3.
- **R-7 Listen host holds all ServerOnly facts** (IdentityKeys, bindings). Accepted for the MVP (IVP R10, AQ-14); dedicated server in ADR-0024.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| session-travel.md | Rules 1-4: session lifetime, host seeded directly, host parity path, no host migration | §1 (PostLogin host seeding, E2), §7; host parity mechanism itself is ADR-0001 §5 |
| session-travel.md | Rules 5-10, F1: direct join, IdentityKey, ordered admission gates, capacity, display names | §3, §1 `PreLogin`/`ApproveLogin` |
| session-travel.md | Rules 11.1-11.9, F7: round start ordering, seed URL option, `FRoundParams`, single acknowledgement, absolute deadline, simultaneous reveal, fallback travel | §4 |
| session-travel.md | Rule 12-13: lobby travel, travel cancel, no lobby windows | §4, §5 |
| session-travel.md | Rule 14: seed publicity; secret sub-seeds held by Session | §2 (`ServerSecret` in `FSessionCore`); derivation ADR-0006; URL never secret (§4) |
| session-travel.md | Rules 15-17: supersede, records in game-instance stores, field scope across travel | §2, §5 |
| session-travel.md | Rules 18-21, F2-F4, F6: silence clock, thresholds, engine backstop, admission mode, rejoin window | §6, §3 |
| session-travel.md | Rule 22: no pawn/PlayerState churn on connection events; default login pawn disabled | §1 (`HandleStartingNewPlayer` no-op, `OnDeactivated`), §5 |
| session-travel.md | Rule 23: client auto-rejoin | §5, §3 (`UStealthIdentitySubsystem`) |
| session-travel.md | Rules 24-25: presentation and leak checklist (a) frozen array, (b) RPCs owner-only, (c) no engine join/leave broadcasts | §1 (`ShouldBroadCastWelcomeMessage = false`, `AGameModeBase`, E10), §5 (adoption-based rejoin, carried PlayerStates), Guidelines 4, 4a, 6; field trimming in ADR-0001/ADR-0003 |
| participant-state-roster.md | R3 match vs round scope; R4 registration API, no inactive-PlayerState reuse; OQ3 store location; OQ4 seamless vs hard | §2, §1, §5; OQ3 and OQ4 answered (game-instance store; seamless with hard travel as fallback) |
| participant-state-roster.md | R9 one named spawn site; R11 disconnect projection, rejoin reclaim, duplicate connection | §1 (no default spawn; factory ADR-0010), §5 |
| participant-state-roster.md | TR-roster-002, -003, -005, -008: single writer and legal-transition table (R7, R8), game-thread events and `EventSeq` (R12, I6, I7), O(1) allocation-free queries (R10, AC-PSR-28), opponent counts never before `Ended` (R6 binding rule) | Amendments 2026-10-25b (§2, Roster rules) |
| round-match-framework.md | R4 phase machine in a game-instance store; admission mode pushed per phase | §2 `URoundMatchStore`; `SetAdmissionMode` |
| round-match-framework.md | R6 Preparing barrier on `OnParticipantTravelComplete`; P7 world-ready | §4 (single ack plus the `ServerWorldReady` stage, adopted 2026-10-25) |
| round-match-framework.md | R7 reveal bound `RevealPawnWaitMaxSec` (S-B4) | §4 simultaneous reveal |
| round-match-framework.md | R17 disconnect policy consumes Session facts; R18 `ShouldAdmitExpired` | §3, §5, `IExpiredAdmissionPolicy` |
| round-match-framework.md | R22 host commands on the game instance with a net-mode check | §8 |
| tuning-data-config.md | CR-4 game-instance preset store; CR-9 one acknowledgement owned by Session | §2, §4 (ADR-0004 supplies `ClientHash`, `TuningSchemaVersion`) |

## Performance Implications
- **CPU**: silence clock per connection per tick (≤ 9 adds and compares) and ack bookkeeping: negligible (inside the 0.5 ms "queue, Roster, staged writes" line and "engine/world tick, other").
- **Memory**: four stores, each a few KB at 10 participants; Event Log archives per Round F8.
- **Load Time**: seamless travel avoids a full disconnect/reconnect; map load is bounded by F7 (`TravelLoadTimeoutSec` 60 s).
- **Network**: one reliable ack request and reply per participant per epoch (< 100 B each); `FRoundParams` and `FRoundReadiness` once per round; no heartbeat in MVP.

## Migration Plan
1. Prototype `AStealthGameMode` (`bUseSeamlessTravel = false`, spawn-at-start-spot logic, `UStealthSettings`) is evidence only and is replaced, not migrated.
2. Build order: (a) `FSessionCore`, `FRosterCore` (Roster first, architecture.md) with headless tests; (b) game-instance facades + `BeginMatch`/`EndMatch`; (c) game mode overrides and admission; (d) multi-process harness (ADR-0008) and spikes S3/S4/S5; (e) round travel + acknowledgement with stubs for TDC, LSC, CPA; (f) liveness and rejoin.
3. Project-config items for the implementing stories (outside this design run): `TransitionMap` in `GameMapsSettings`; net driver `ConnectionTimeout`/`InitialConnectTimeout`; `net.Iris.UseIrisReplication=0` pin (ADR-0001).

## Validation Criteria
- Session & Travel ACs (admission order, travel, single acknowledgement, deadline, silence clock, rejoin, supersede, session end) pass in the multi-process harness.
- Roster AC-PSR-40a (`TravelPersistence`) passes across one seamless and one hard travel.
- S3: after two seamless travels, every store's content equals its pre-travel value; every client's C-TeamView equals the server's before the first round tick.
- S4: a mid-round rejoin produces **no** PlayerState actor-channel open/close, no player-array change and no `ClientReceiveLocalizedMessage` on any non-owner client (raw-bunch capture), and no client ever receives an engine entered/left/renamed message in any phase; two clients on one machine with distinct `-IdentityKey=` never supersede each other.
- S5: deliberate quit → every client shows the end message within `ShutdownFlushSec + d_ow`; crash → within F6 `L_host_crash`.
- A two-sessions-in-one-process test shows empty stores at the second `OpenSession`.

## Related
- ADR-0001 (identity invariants; frozen player array; legacy driver) — depends on.
- ADR-0002 (queue and server clock; mailboxes keyed by `ParticipantId`).
- ADR-0003 (PlayerState/GameState property allowlist, field trimming).
- ADR-0004 (tuning preset store, `ClientHash`, `TuningSchemaVersion`) — depends on.
- ADR-0006 (seed and sub-seed derivation; `ServerSecret` in `FSessionCore`).
- ADR-0007 (module placement: stores in `StealthFoundation/Public/Server`, game mode in `StealthGame`).
- ADR-0008 (multi-process harness, raw-bunch capture for S4).
- ADR-0010 (named spawn site, `PawnLeavingGame`, AI hand-over).
- ADR-0024 (dedicated server reuses admission, travel and liveness rules).
- **GDD sync (2026-10-24), not applied by this run:** `session-travel.md` rule 16 names `BindConnection(IdentityKey, PlayerState)` and says PlayerControllers persist through seamless travel; in 5.8.4 the default seamless path **replaces** both controller and PlayerState (E5), so this ADR keys binding by `FSessionPeerKey` and rebuilds views on arrival (the GDD's "nothing authoritative on a PlayerController" rule is unchanged and now load-bearing). Rule 7's "any client-supplied option ... is stripped" is implemented as "never read" (`InitNewPlayer` without `Super`), and the allowlist gains `RejoinToken` (rule 6). Rule 2's "[verify: the listen-server local player skips PreLogin]" is verified (E2). TDC CR-9's "after seamless travel the PlayerController already exists" holds only in the sense that a new controller exists before the acknowledgement request is sent.
- Design: `design/gdd/session-travel.md` (rules 1-25, F1-F8, OQ1, OQ2, OQ11 answered here in part), `participant-state-roster.md` (R3, R4, R9, R11, OQ3, OQ4), `round-match-framework.md` (R4, R6, R7, R17, R18, R22; CR-13(d) adopted 2026-10-25), `tuning-data-config.md` (CR-4, CR-9); `docs/architecture/architecture.md` (AQ-10, AQ-13, AQ-17).

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §2: Roster rebind event named `URosterStore::OnParticipantViewRebound(FParticipantId)`, raised after every `RebindView` — requested by ADR-0020 (§6).
- §1: `AStealthGameMode::PlayerCanRestart_Implementation` returns false — requested by ADR-0019 (item 7).
- §5: informant readmission calls `IInformantBodyProvider::OnInformantReadmitted` only after the Session acknowledgement; interface declared by Round, registered by the composition root — requested by ADR-0019 (items 3, 9; the awaiting-body call itself is Round/ADR-0019's, already specified in ADR-0019 §1).

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §2: `RequestTransition(StrikeLimit)` releases (not retires) the pawn, with the Roster calling `ReleaseToCrowd` internally; the caller pairs `ReleaseCard` — requested by ADR-0019 (item 8); architecture-review fix 2026-10-25 (Conflict 8).
- §2: the rebind event `URosterStore::OnParticipantViewRebound(FParticipantId)` is the Roster GDD's `OnViewRebound(ParticipantId)` (name mapping recorded) — architecture-review fix 2026-10-25 (other finding).
- Roster server face (ADR-0001 §5): `URosterStore::SetTeammateConcealed(FParticipantId, bool)` is part of this store's API (Concealment the only caller) — architecture-review fix 2026-10-25 (Conflict 5).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §4 — user decision 2026-10-25: Round CR-13(d) world-ready stage adopted as `ServerWorldReady(FAckEpoch)` → `OnParticipantWorldReady` (Session rule 11, Round P7).

User scope decisions (2026-10-25: "accept all recommendations"); decision unchanged, Status unchanged (Accepted).
- Disconnect path — user decision 2026-10-25: Session rule 24 now allows one Team-audience bit about a teammate, `bReconnecting`, which the Roster push step derives from the `Connection` projection Session already writes through `NotifyConnectionChanged`. Session gains no new output, RPC or replicated field, and nothing reaches non-teammates (welcome-message suppression and the frozen name list are unchanged).

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §6: engine `ConnectionTimeout` backstop is **at least 111 s at the defaults with `T_srvload` at its range maximum, 120 s to be configured** (was "105 s"), matching Session F7 and AC 27 (session-travel re-review xdoc).
- §5 Disconnect: `ConnectionGeneration` bumps on every `Connection` change including the Lobby `Rejoining` projection (Roster R11); the Roster accepts `Disconnect` for a participant whose connection is `Expired` (kick path; Round CR-21; closeout D9); Round consumes `OnLobbyTravelComplete`, `OnRoundTravelStarted` and `OnParticipantDisconnected` during travel (session-travel re-review xdoc).

Cross-review fix 2026-10-25 (`design/gdd/gdd-cross-review-2026-10-25.md` S-B2, ruling D24; decision unchanged, Status unchanged):
- Supersede-from-live: when Session accepts a superseding connection in a round it raises `OnParticipantDisconnected(Reason = Superseded)` for the old connection at that instant; grace and the rejoin window run from that instant; the old pawn goes through the standard disconnect hold (AI-driven, Round R17) until the rejoiner possesses; `OnParticipantRejoined(Superseded)` is raised at rejoin completion. A failed supersede-rejoin raises no second event and does not restart grace (Session rule 25f, AC 17; Round R17.1-R17.3; Roster R11).
- Kick: the Roster records a host kick with an optional `bKicked` flag on `NotifyConnectionChanged`; Session refuses the kicked key; the teammate `bReconnecting` flag clears silently (`ReconnectEndCause = Cleared`, ruling D23).

Technical Setup 2026-10-25b (architecture-review closure, `docs/architecture/architecture-review-2026-10-25b.md`); decision unchanged, Status unchanged (Accepted).
- §2 `FRosterCore` lifecycle writes (TR-roster-002; participant-state-roster.md R7, R8, States and Transitions table): `FRosterCore::RequestTransition` MUST be the only writer of a participant's lifecycle `State`, `LastTransitionReason` and `LastEventSeq` (the single-writer line in Constraints). The GDD "States and Transitions" legal-transition table is NORMATIVE for the transition set, and R8's result precedence is authoritative where the two seem to differ. Legal set: `Active -> Informant` for Reason `Assassinated`, `ReversedAgainst`, `StrikeLimit` or `Disconnect` (`Disconnect` applies only if `Connection == Disconnected` and `ExpectedConnectionGeneration` matches; the transition retires the live binding with `RetireAtServerTime`, except `StrikeLimit`, which releases it, see the strike-limit rule in §2); `Informant -> Informant` is `NoOp_AlreadyInState` (first reason kept, no event); `Active|Informant -> RoundEnded` only through `EndRoundAll`; non-Dormant `-> Active` only through `ResetAll` (teams locked; `Dormant` records go to or stay `RoundEnded`); `RoundEnded -> Informant` is `Rejected_IllegalTransition`; there is no revival (any other reason into `Active` is `Rejected_IllegalReason`); `InformantKilled` is never an entering reason (it is carried only by `RetireBinding`, no state change). R8 result precedence, the first failing check decides: 1 `Rejected_NotAuthority`; 2 `Rejected_WrongThread` (plus an `ensure` to the fault sink); 3 `Rejected_UnknownParticipant`; 4 `Rejected_StaleEpoch` (only when `ExpectedEpoch = (MatchId, RoundEpoch)` is supplied and differs); 5 `Rejected_BatchOnlyReason` (Reason `RoundEnd` or `Reset`); 6 `Rejected_IllegalReason` (Reason not an entering reason for `NewState`: Informant <- {Assassinated, ReversedAgainst, StrikeLimit, Disconnect}, RoundEnded <- {RoundEnd}, Active <- {Reset}); 7 `NoOp_AlreadyInState`; 8 `Rejected_IllegalTransition`; 9 `Rejected_NotDisconnected`; 10 `Rejected_StaleConnection` (`ExpectedConnectionGeneration` missing or different); 11 `Applied`. `RoundEnd` and `Reset` MUST exist only as the batch calls `EndRoundAll()` (one `OnRosterRoundEnded`; a second call is a no-op with no event) and `ResetAll(RoundIndex)` (needs locked teams, else `Rejected_TeamsUnlocked`; `RoundEpoch` + 1; clears bindings and tombstones; one `OnRosterReset`; a repeat with the same `RoundIndex` is applied in full), so a round can never end or reset for only some participants. The Roster does not order requests; it applies calls in arrival order, and ADR-0002's queue key (server tick, then arrival sequence id) orders them. Tests: one case per precedence step and per table row.
- §2 `FRosterCore` events (TR-roster-003; R2, R12, I6, I7): every mutating call MUST run on the game thread (checked on entry; off-thread gives `Rejected_WrongThread` with a fault-sink `ensure`, I7) and on the server (a client call gives `Rejected_NotAuthority`). State is written immediately; events are queued and dispatched FIFO only after the OUTERMOST mutating Roster call returns, so a listener's re-entrant request sees live state and its own events queue behind the current ones; listeners act on applied events only and MUST be idempotent per `EventSeq`. `EventSeq` MUST increase by exactly 1 for each emitted Roster event of ANY type (I6): `OnParticipantStateChanged`, `OnRosterRoundEnded`, `OnRosterReset`, `OnParticipantRegistered`, `OnParticipantReclaimed`, `OnParticipantRemoved`, `OnConnectionChanged`, `OnTeamsChanged`, `OnBindingLost`; the number is taken when the event is queued, in mutation order, so FIFO dispatch order equals `EventSeq` order. These calls and results emit NO event and consume NO `EventSeq`: every `NoOp_*` and `Rejected_*` result, `InvalidateAllBindings`, `RebindView` (and its wiring notification `OnParticipantViewRebound`, §2 above), `SetOwnerTally`, `SetOwnerRespawnDue`, and the pawn-binding calls `BindPawn`, `RetireBinding` (including the informant-kill retire), `ReleaseToCrowd` and `ClearBinding` (the last raises only `OnBindingLost` when its R9 condition holds).
- §2 `FRosterCore` query structure (TR-roster-005; R10, R2, AC-PSR-28 `StealthGame.Roster.QueryHotPathBudget`): server queries (`ResolveVictimKind`, `IsPawnBound`, `GetParticipant`, `GetPawn`, `GetState`, `GetTeam`, `GetRelation`, `GetLegalityClass`, `CountActive`, `CountInformant`, `CountRoster`, `IsTeamEliminated`) MUST be O(1) and allocation-free on the hot path. The pawn-to-participant lookup is a `PawnBindingTable`: a pre-sized open-addressing hash table keyed by `FPawnHandle` (ADR-0002 §2a) whose value is `{ParticipantDenseIndex, BoundLive | Retired}`. Records live in a fixed array indexed by that dense index, with `ParticipantId -> index` a second pre-sized table, and the per-team `Active`, `Informant` and `RosterCount` counters are updated in the same call as every state change. Both tables are reserved at `BeginMatch` for the maximum pawn count (`NpcCount` hard maximum plus participants, rounded up to a power of two) and never rehash or grow in a query or a bind. `AC-PSR-28` is the pass test: 10 participants, 200 and 2,000 pawn handles, 10,000 calls each to `ResolveVictimKind`, `IsPawnBound` and `GetRelation`; a counting allocator records 0 allocations and an instrumented probe counter shows at most 2 hash probes per call (wall-clock timing advisory only). On a client process the stores exist but are empty, and every query MUST return 0 or `false` (`CountActive`, `CountInformant`, `CountRoster`, `IsTeamEliminated`; binding queries answer `NotTargetable(Stale)`, mutators `Rejected_NotAuthority`) and MUST report to the injected fault sink (`ensure` in development builds); a client query MUST never return the permissive answer `Npc` and never a real value.
- §2 and §4 round-end step (TR-roster-008; participant-state-roster.md R6 binding rule "Opponent counts never leave the server before `Ended`", AC-PSR-42): opponent `ActiveCount`, `InformantCount`, `AwaitingPawn` and every value derived from them (for example "1 enemy left", a remaining-rival icon, an elimination-progress bar) MUST NOT be replicated to, computed on, or presented by any client before Round phase `Ended`. The Roster count queries are server-only (see the query rule above), and no store, event payload or log line that reaches a client may carry them. The only carrier allowed to hold opponent-derived values, and only from the round-end step (the same server step as the `Ended` publication, ADR-0002 §8 (a)), is `FRoundResult.TeamSummary[2]` `{StepsCompleted, EliminatedCount, WrongTargetCount}` inside `FRoundPublicState` (zero in every earlier publication, published once with a forced net update, 6 bytes of team aggregates, IVP `RoundTeamSummary`). C-TeamView (ADR-0001 §5) carries only own-team data (the owner part plus at most four teammate entries), from which a client derives own-team counts; it has no opponent field. A `RoundEnded` state that reaches a client before `Phase = Ended` is treated as no change. All presentation, including the listen host's, reads only its local C-TeamView and never a Roster server query (build-time include check, AC-PSR-42); the `[UI-ev]` screenshot at "one opponent left" shows no opponent count.
- §2 `URoundMatchStore` (TR-round-009 cross-reference, orchestrator): `FRoundPhaseCore` also holds `PhaseChangeCount` (monotonic across travel) and is the source of the `FRoundPublicState` re-seed in `InitGameState` (ADR-0002 §8.10). The Round Event Log archive kept across travel is the ring owned by ADR-0002 §8.9.

Story-phase rulings 2026-10-25 (`production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §5 Disconnect — decision D45: `ConnectionGeneration` bumps **only on entry to `Disconnected`** (the Roster GDD R3/R11 rule, the more specific source). This supersedes the closeout bullet above that read "bumps on every `Connection` change including the Lobby `Rejoining` projection". Implemented in `FRosterCore::NotifyConnectionChanged` (story psr-009).
- §2 — decision D43: the Round-phase input the Roster needs (R9 `ClearBinding` fault, F4 `IsTeamEliminated`) is an injected dependency of `FRosterCore` (`IRosterRoundPhase`), adapted by the `URosterStore` facade like the other injected dependencies; the facade and the `BeginMatch`/`EndMatch` wiring on every session exit path are story psr-061.
