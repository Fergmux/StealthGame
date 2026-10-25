# ADR-0002: Server resolution queue, server clock and round-end staging contract

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. Cannot be Accepted before ADR-0006, ADR-0007 and ADR-0008 are Accepted (see ADR Dependencies).

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims re-read against the UE 5.8.4 install; design against round-match-framework.md R9-R16 as fixed by the cross-review re-check of 2026-10-24)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (parked host commands; post-latch timer lane; slot end defined after the Roster flush; `RootCauseKey` through cascade overflow with a shipping-build fallback; one held-replies mechanism; Round's latch record authoritative, void calls `Latch`; `ConnectionTransition` type and a Round R13 change request; named staging-pass exception; teardown-pending handles; clock RPC rate limit and an IVP change request). Engine validation: BLOCKING (text-only) → fixed (`TWeakInterfacePtr` needs `UINTERFACE`); minor notes applied (E4: newly spawned actors tick this frame; four-timestamp clock on a precise server clock; authority-only tick registration).

## Summary
Every interacting state change in a round (strikes, braces, holds, timers, transitions, wins) must resolve in one server-side total order, and everything observable about an outcome must be presented by server time so that arrival order and jitter never carry meaning. This ADR fixes the Foundation contract: one resolution queue per round keyed `(ServerTickIndex, Seq)` with permuted per-source mailboxes and a due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes for outcome presentation, the terminal latch storage with Round's rule injected, the Round R9 two-step round end (server-only latch step; round-end step bound to the deciding strike's death write), and one RTT-compensated client server-time estimator with an accuracy budget.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4) |
| **Domain** | Core (game-thread scheduling, ticking) and Networking (server clock on clients) |
| **Layer** | Foundation |
| **Knowledge Risk** | MEDIUM — tick groups and `FTickFunction` are pre-cutoff and re-verified; the client clock and spawn/destroy inside a tick group carry [U] items |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/networking.md`, `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Module Ownership: Foundation layer, consumer-owned interfaces, lifetime rules; Data Flow §1-§3; API Boundaries: core value types and queue; Performance budget); engine source read this session: `Engine/Private/LevelTick.cpp:1574-1575, 1750, 1778, 1929`, `Engine/Classes/Engine/EngineBaseTypes.h:86-101, 196, 341, 524`, `Engine/Classes/GameFramework/GameStateBase.h:142-166`, `Engine/Private/GameStateBase.cpp:164-193` |
| **Post-Cutoff APIs Used** | None (`TG_NewlySpawned` handling and `TWeakInterfacePtr` re-verified in 5.8.4 source). `UE_LOGF` (5.8) is preferred for any diagnostic text, but the Event Log never passes through logging (Round R19) |
| **Verification Required** | (1) Spawning a Shared Pawn (CMC) inside `TG_PostPhysics` from a resolver behaves (spike, alongside ADR-0010); destruction is always deferred (Decision §7), so destroy-in-drain needs no spike. (2) Net-emulation acceptance test of the client estimator (`LeakNetProfile`: PktLag 100 ms, PktLoss 1-2 %; plus a 200 ms and an asymmetric profile) against the accuracy budget in Decision §9. (3) The drain tick function runs exactly once per server frame after CMC and AI ticks, including on the listen host, whose server frame **is** its render frame: `NetServerMaxTickRate` has no effect on a listen server unless `bClampListenServerTickRate` is set ([V] `GameEngine.cpp:1742`, `NetDriver.h:869`), so the condition is a host frame-time condition (frame rate ≥ 60, i.e. frame time ≤ 1/60 s, at the profiled load; SCL I-SCL-10 as restated 2026-10-25), measured by the harness frame-time capture, not a `NetServerMaxTickRate` setting |

**Engine facts this ADR relies on** ([V] read in 5.8.4 source this session; [U] unverified):

| # | Fact | Tag |
|---|---|---|
| E1 | `UWorld::Tick` runs network dispatch (`BroadcastTickDispatch`, where server RPC bodies execute) before the tick groups, `TG_PrePhysics` … `TG_PostPhysics` next, and the replication flush (`BroadcastTickFlush`) at the end of the frame | [V] `LevelTick.cpp:1574-1575, 1750, 1778, 1929` |
| E2 | A custom `FTickFunction` subclass implements `ExecuteTick`, sets `TickGroup`, and is registered per level with `RegisterTickFunction(ULevel*)` | [V] `EngineBaseTypes.h:196, 341, 524` |
| E3 | The engine's client server-time (`AGameStateBase::OnRep_ReplicatedWorldTimeSecondsDouble`) computes `Replicated − local` with **no RTT compensation**, averages up to 250 samples, and stores the delta as a **`float`** `ServerWorldTimeSecondsDelta`; the replicated value is stamped by a timer (`ServerWorldTimeSecondsUpdateFrequency`), not at send time | [V] `GameStateBase.cpp:164-193`; `GameStateBase.h:142-166` |
| E4 | Tick functions of an actor spawned mid-frame are queued in the same frame (`QueueNewlySpawned` → `TG_NewlySpawned`), so a CMC pawn spawned in `TG_PostPhysics` begins ticking **this frame**; whether its first CMC tick and `DeltaTime` behave is unverified | [V] `TickTaskManager.cpp:1519-1545, 1687-1689, 2145-2150` / [U] spike with ADR-0010 |
| E6 | `FTickFunction::bRunOnAnyThread` defaults off; `UWorldSubsystem::OnWorldBeginPlay` exists; server `UWorld::TimeSeconds` is a double that pauses but does not dilate in this game | [V] `EngineBaseTypes.h:230, 349`; `WorldSubsystem.h:43`; `World.h:2010, 2847` |
| E7 | `TWeakInterfacePtr<T>` constructors `Cast` to and from `T`, which requires a UHT `UINTERFACE`; a plain C++ interface cannot be held by it | [V] `WeakInterfacePtr.h:45-64` |
| E8 | Server RPC bodies run in dispatch before `TimeSeconds` advances for the frame, and replies leave at the end-of-frame flush, so a world-time stamp in an RPC body is biased by up to one server frame | [V] `LevelTick.cpp:1574, 1610, 1929` |
| E5 | Unreliable owner-only client/server RPCs on a `APlayerController` are delivered in the next dispatch with no ordering guarantee relative to replicated properties | [D] `modules/networking.md`; ordering [U] (not relied on: the estimator uses only its own timestamps) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0006 (seeds and server-secret streams: `GetServerSubSeed(SeqOrder, ServerTickIndex)` and `GetServerSubSeed(TieRank, ServerTickIndex)` drive the intake and tie permutations); ADR-0007 (module layout and composition root: the queue lives in `StealthFoundation`, resolvers register from `AStealthGameMode::InitGame`/`OnWorldBeginPlay`, `Public/View` vs `Public/Server` split for the client estimator); ADR-0008 (harness: net-emulation profiles for the estimator acceptance test, headless queue specs) |
| **Enables** | ADR-0011 (IIF hold timers and replies as queue items), ADR-0016 (Crowd: AI discrete commands through the server-local mailbox, `ICrowdControl::Halt`), ADR-0018 (encounter data: death writes and counter poses as typed staged writes), ADR-0019 (informant respawn as a due-time item), ADR-0020 (objectives completion gate inside the hold-timer slot), ADR-0021 (HUD/markers present from `EstimatedServerNow`), ADR-0023 (Event Log keys) |
| **Blocks** | Round & Match Rules, Input & Interaction Framework, Assassination & Defence Reversal, NPC-Kill Penalty & Informants, Objectives Chain epics (every resolver), and every client presentation that schedules from server time (HUD, Teammate Markers, Concealment clips) |
| **Ordering Note** | Second in table order; Foundation. ADR-0005 (session, travel, stores) is a peer, not a dependency in either direction: it hosts the per-world queue after each travel and its start sequence publishes `ActiveAtServerTime`, which clients read through this ADR's estimator. Its value types (§2) are needed by ADR-0001's carriers only by name; neither ADR depends on the other's acceptance. The Round *rules* (phases, win policy, disconnect policy) stay with Round (Core) and are not decided here |

## Context

### Problem Statement

Smoking Mirror has no ties and no draws by design (locked rule: "the server resolves events in order"). Two players may press Attack and Prepare in the same network frame; a hold may complete in the same frame a strike lands; a disconnect grace may expire as the final objective completes. The outcome must be one deterministic, server-secret, reproducible order, and nothing about the outcome may be visible before the blow lands on screen. Round R11-R16 defines that order and R9 defines a two-step round end; Assassination (ASN-R21) defines a pending-write queue for outcome-revealing writes at contact time; every client-side presenter (HUD, Teammate Markers, Concealment, Camera) needs one server-time estimate to schedule from.

These rules span six systems (Round, IIF, Assassination, Penalty, Objectives, Roster) and four presentation consumers. If each system kept its own clock, timer or ordering, the "no ties" guarantee would depend on tick-function order, which Unreal does not define across actors. The contract must exist, as a header and a testable core, before any resolver is written (systems-index: "the interface owner is built first").

### Constraints

- All gameplay mutation happens on the game thread (Roster I7).
- Server RPC bodies run in network dispatch at the top of the frame, before any tick group (E1); human movement arrives there, AI and host movement in the physics tick groups.
- The engine's client server time is not RTT-compensated and is a float delta (E3): unusable for contact-timed presentation at 100 ms RTT.
- No client-supplied timestamp may influence order (Round R12); no lag compensation (Round R12).
- Hidden order must come from server-secret sub-seeds (ADR-0006), never from the public seed or join order.
- Testability: the queue must be a plain C++ core runnable headless without a `UWorld` (architecture LP concern 6).
- CPU: queue drain, resolvers, Roster and staged writes share 0.5 ms of the listen host's game-thread budget (architecture Performance budget).

### Requirements

- One total order per round with a reproducible, client-unpredictable tie-break; one resolver per item type; inline follow-ons; Roster events flushed between items (Round R11-R14, Roster R12).
- Timed items ordered with intake by a fixed rule (Round R12, IIF R8).
- A single-assignment terminal latch whose terminate rule is Round's (Round R15, TD concern 5).
- The R9 two-step round end with the round-end step in the same server step as the deciding strike's death write (Round R9; cross-review S-B1, S-B2).
- Typed, epoch-stamped, logged staged writes for outcome presentation, dropped on `Reset` (ASN-R21).
- A client server-time estimate that is RTT-compensated, smoothed and monotonic, with a stated accuracy budget and a net-emulation acceptance test (TD concern 3; TR-asn-010).

## Decision

### 1. One queue per round, one key

- Exactly one `FResolutionQueueCore` (plain C++) per round, owned by the `UResolutionQueue` world subsystem facade in `StealthFoundation`. It opens at Round's `Active`-entry step and closes at the latch (or at an abort/void).
- **Key** `FQueueKey { uint64 ServerTickIndex; uint64 Seq; }`. `ServerTickIndex` increments once per server frame at drain start. `Seq` is a 64-bit counter restarting at 1 each round, assigned on the game thread when an item is **stamped** (never on arrival). Keys compare lexicographically, are unique, and the queue drains in ascending key. `Compare(KeyA, KeyB)`, `GetKey(Item)` and `CurrentKey()` are the only ordering primitives any system may use; no feature keeps its own clock or timestamp to order events against another system's (Round R11, R14).
- **Item types are a closed set** (an addition is an amendment to this ADR): `PawnCommand`, `HoldTimer` (`Complete | CapEnd | DebounceExpiry | ConditionCancel`; `ConditionCancel` is scheduled at `ServerNow()` by the IIF's read-only condition sweep, ADR-0011), `TimerFired` (subkinds `GraceExpiry`, `InformantRespawn`, `StageSignal`, `DefenceWindowEnd`, `PawnLossRetry`, `CrowdSchedule` (ADR-0016 §6), `ConcealmentTransition` (ADR-0017 §3), `BodyRelease` and `BodyRemoval` (ADR-0018 §6)), `PawnLost` (Round R17.9), `ConnectionTransition` (Session-originated disconnect consequences that must be ordered against gameplay, including a host kick, Round R15(c)), `HostCommand` (`ResetRound`, `AbortRound`; parked while latched, §8.1), `DebugItem` (compiled out of Shipping). The post-latch timer kinds `EndedMinDwell` and `EndedAutoReset` (§4) belong to `TimerFired`. `ObjectiveCompletion` is **not** an item: it runs as a follow-on inside the `HoldTimer(Complete)` slot (Round R13, OC-10). **Deferred outcomes** (Round R14.3, a wind-up) are not used in MVP (no system defines one); adding one is an amendment that adds a `DeferredOutcome` type carrying its `CauseKey`.
- **GDD change request (Round, minor):** Round R13's item table lists neither `HostCommand`, `PawnLost` nor `ConnectionTransition`; this ADR adds them to the closed set and asks Round to list them (recorded under Related; not edited here).

### 2. Core value types (Foundation `Public/View`)

Defined here because every layer needs them (architecture LP concern 3): `FParticipantId` (server-only meaning, never replicated), `FEpochToken {MatchId, RoundEpoch}`, `FViewerId` (derived from the server connection), `FMailboxId` (one per participant + one server-local), `FQueueKey` (§1), `ETransitionReason {Assassinated, ReversedAgainst, StrikeLimit, Disconnect, RoundEnd, Reset}`. These are plain ids and enums with no server-only content, so any layer may include them.

### 2a. Queue payload types: owned by this ADR, in Foundation `Public/Server` (architecture-review fix 2026-10-25)

Every type that crosses the queue boundary is declared **here, in `StealthFoundation/Public/Server/StealthQueuePayloads.h`**, never in a feature module. ADR-0007 forbids Foundation from including anything in `StealthGameplay`, so the queue header can name only Foundation types. Feature ADRs own the **meaning** of the fields they fill (and the applier or resolver that reads them), not the struct.

- **`FPawnHandle { TWeakObjectPtr<APawn> Pawn; }`**: the one server-only pawn reference used in payloads. Its generation is the weak pointer's own object serial number: a destroyed pawn, or a reused object slot, never resolves ([V] `CoreUObject/Public/UObject/WeakObjectPtr.h:172, 203, 421-424`). It is typed `APawn` (engine), never `AStealthPawn` (StealthGameplay), the same rule as ADR-0001's C-TeamView `PawnRef`. `ResolvePawnHandle(const FPawnHandle&) → APawn*` (on `UResolutionQueue`) returns `nullptr` when the handle is stale **or** the pawn is pending teardown (§7). The caller casts the result in its own module. A handle is never replicated and never logged as text.
- **`FSubjectRef`**: a tagged union of `FPawnHandle` | `FParticipantId` | court fixture (`uint16 SocketId`, the raw LSC id; the typed `FCourtFixtureId` stays with ADR-0014 and converts at the boundary).
- **`FQueueItem` payloads (closed `TVariant`, one per item type):** `FCommandPayload` (`PawnCommand`), `FHoldTimerPayload` (`HoldTimer`), `FTimerPayload` (`TimerFired`), `FPawnLostPayload` (`PawnLost`), `FConnectionTransitionPayload` (`ConnectionTransition`), `FHostCommandPayload` (`HostCommand`), `FDebugPayload` (`DebugItem`, compiled out of Shipping). Every field is an id, an epoch, a time, a small enum or a raw `uint8`/`uint16`/`uint32` whose meaning the named owner defines. None holds a raw `UObject*` or a feature type.
  - `FCommandPayload { FPawnHandle Pawn; uint32 ControlEpoch; FClientCommandSeq ClientSeq; FReplySinkId Sink; uint8 Kind; uint8 Phase; FSubjectRef Target; FVector WorldPoint; uint32 Arg; }`. ADR-0011 owns `Kind`/`Phase`/`Arg` values and maps its typed commands (`FAttackPayload`, `FHideCommand`, `FTraversalHint`, interact target) onto them at ingress. `FClientCommandSeq` (ADR-0011) and `FReplySinkId` are Foundation `View`/`Server` ids.
  - `FHoldTimerPayload { FPawnHandle Pawn; EHoldTimerKind Kind; uint8 ActionType; uint32 ActionEpoch; FQueueKey BeginKey; }`. ADR-0011 owns `ActionType` (its `EActionType`, already a Foundation `View` enum).
  - **`FTimerPayload` (one generic fixed shape for every `ETimerKind`):** `{ ETimerKind Kind; FSubjectRef Subject; uint32 Generation; uint32 Seq; uint8 Phase; }`. Each timer owner documents its mapping, and no per-feature struct enters the variant:

    | `ETimerKind` | Owner (ADR) | `Subject` | `Generation` | `Seq` | `Phase` |
    |---|---|---|---|---|---|
    | `ConcealmentTransition` | Concealment (ADR-0017 §3) | pawn | `TransitionGeneration` | 0 | `EConcealPhase` {Commit, Cross, End} |
    | `InformantRespawn` | Penalty (ADR-0019 §3) | participant | 0 | `RespawnRequestSeq` | 0 |
    | `BodyRelease`, `BodyRemoval` | Assassination (ADR-0018 §6) | pawn | 0 | 0 | 0 |
    | `CrowdSchedule` | Crowd (ADR-0016 §6) | pawn or participant | schedule generation | event ordinal | event kind |
    | `GraceExpiry`, `PawnLossRetry` | Round | participant | `ConnectionGeneration` | retry ordinal | 0 |
    | `StageSignal` | Objectives (ADR-0020) | court fixture | 0 | completion ordinal `k` | team |
    | `DefenceWindowEnd` | Assassination (ADR-0018) | pawn | 0 | `ActionEpoch` | 0 |
    | `EndedMinDwell`, `EndedAutoReset` | Round (§4 post-latch lane) | none | 0 | 0 | 0 |

    The item's `FEpochToken Expected` (on `FQueueItem`) carries the round epoch for every kind. A feature may keep a typed adapter (`ToTimerPayload`/`FromTimerPayload`) in its own `Private/` folder. Adding a field to `FTimerPayload` is an amendment to this ADR.
  - `FPawnLostPayload { FParticipantId Participant; FPawnHandle Pawn; }`; `FConnectionTransitionPayload { FParticipantId Participant; uint32 ConnectionGeneration; uint8 Transition; }` (Session/Round own `Transition`); `FHostCommandPayload { uint8 Command; /* ResetRound | AbortRound */ }`.
- **Staged-write payloads (closed `TVariant` on `FStagedWrite`):** `FDeathWritePayload { FPawnHandle Victim; double DeathServerTime; }`, `FCounterPayload { FPawnHandle Defender; double ContactServerTime; }` (meaning, times and appliers: ADR-0018), `FRoundEndPayload` (Round, §8).

`FQueueItem` = `{ EQueueItemType Type; FEpochToken Expected; FMailboxId Source; TVariant<…payloads above…> Payload; }`, with the control/possession epoch and `ConnectionGeneration` inside the payloads that need them. `FQueueItem`, `FStagedWrite`, every payload, `FPawnHandle`, `FSubjectRef`, `EQueueItemType`, `ETimerKind`, `EHoldTimerKind`, `EStagedWriteType` and the resolver/applier interfaces are **server-only**, so they live in `Public/Server/` behind ADR-0007's guard. No presentation code can name a queue payload.

### 3. Intake: mailboxes and permutation

- Command RPC bodies (dispatch time, E1) and AI/host sources (physics tick groups) only **push** into a per-source FIFO mailbox keyed by `FParticipantId` (plus one server-local mailbox for AI and server sources). Mailboxes are keyed by participant, not connection or controller, because controllers are replaced at travel.
- At drain start the non-empty mailboxes are sorted into the canonical base order (ascending `FParticipantId`, server-local last) and permuted by a per-tick Fisher-Yates permutation drawn from `GetServerSubSeed(SeqOrder, ServerTickIndex)` (ADR-0006); each mailbox's items are stamped in FIFO order. At most `MailboxMaxItemsPerDrain` (32) discrete items per mailbox per drain; excess dropped and counted (`MailboxOverflow`). Items from a superseded connection generation are `Dropped(StaleConnection)`. AI `Move`/`Look` are continuous inputs, never items (Round R12).

### 4. Due-time heap

- Every timed item (hold ends, defence window end, grace expiry, informant respawn, stage-signal release, pawn-loss retry, crowd schedule events, concealment transition phases, body release and removal) is scheduled with `ScheduleAt(ServerTime, Item)` into the queue's **due-time heap**; no gameplay timer uses the engine timer manager or a feature tick.
- **AI decision timing is input generation, not a gameplay timer** (ADR-0016 §6). An AI controller's private clocks (pauses, gaze holds, reaction delays) may be evaluated against `ServerNow()` in the controller's own per-frame due-time list, because they mutate no gameplay state: their only outputs are `Move`/`Look` inputs and commands that enter the command path and are ordered by the queue like a human's key presses. Crowd schedule events that spawn, absorb or re-card pawns are gameplay timers and use `TimerFired(CrowdSchedule)`.
- At drain start, items with `DueTime ≤ DrainStartTime` are stamped **before** that frame's intake; items that become due during the drain's own processing (scheduled inline with `DueTime ≤ now`) are stamped **after** intake. Within each group the order is `(DueTime, TieRank)`, except that `HoldTimer` items with equal `DueTime` are ordered by the key of their `Begin` command (IIF R8); `TieRank` is a per-tick server-secret shuffle of participant ids from `GetServerSubSeed(TieRank, ServerTickIndex)` (Round R12).
- Every timed item carries an `FEpochToken` (and a hold's `ActionEpoch`); a stale token resolves `Rejected_StaleEpoch` and changes nothing.
- **Post-latch lane.** `TimerFired(EndedMinDwell | EndedAutoReset)` items are scheduled only by Round, only from the round-end step, are **exempt from `Rejected_Latched`**, and are cleared on `Reset`. With the Concealment exemption below they are the only items the drain resolves after the latch (parked `HostCommand`s are handed to Round at the dwell's end, Round R9). No other gameplay timer may use this lane.
- **Post-latch exemption: `TimerFired(ConcealmentTransition)`** (ADR-0017 §3). These items keep resolving after the latch so an in-flight hide transition completes and a concealed pawn stays culled in `Ended`. Their resolver may write only Concealment state, the cull record and the Owner/Team concealment flags (never an outcome, a Roster transition, a score or a win), may schedule only further `ConcealmentTransition` items, and is dropped by generation/epoch at `Reset`. `BodyRelease`/`BodyRemoval` have **no** exemption (ADR-0018 §6.3).

### 5. The drain

- One custom `FTickFunction` (`FResolutionDrainTickFunction`), game thread only (`bRunOnAnyThread = false`, E6), `TickGroup = TG_PostPhysics`, registered **only on the authority** against the persistent level in `OnWorldBeginPlay` (a `UWorldSubsystem` cannot choose its tick group) (E2), and unregistered (`UnRegisterTickFunction`) in `Deinitialize`. It runs once per frame after human (dispatch-time) and AI/host (physics-group) movement has settled, so resolvers validate against this frame's positions.
- For each stamped item in key order: `Resolver.Resolve(Item, Key)` → `Executed | Voided(reason)`. Resolvers re-validate against **live** state at the slot and run every follow-on **inline, depth-first, inside the slot** (Roster transitions, `OnPawnKilled` and subscribers, strike-limit transitions, the elimination check, `OnActionStarted` fan-out). Depth is bounded by `QueueCascadeDepthMax` (8, asserted); a breach logs `CascadeOverflow` and submits the remaining work as items **in the order it was raised**, each carrying `RootCauseKey` (the originating slot's key; a fresh item's own key); every staged write records the `RootCauseKey` of the slot that staged it as its `CauseKey`.
- **Roster event flush.** After each `Resolve` returns, the queue calls the injected `IEventFlushBarrier::FlushPending()` (the Roster implements it: FIFO dispatch of events queued during the outermost call, Roster R12) and asserts nothing remains queued before dequeuing the next item. Round evaluates elimination inside this flush (Round R11), so `TryTerminate` normally fires inside `FlushPending()`. **Slot end** means: after `Resolve` **and** `FlushPending()` have returned. The same barrier is called after an inline round-end step and after each staging-pass write that mutates the Roster (`EndRoundAll` raises batch events).
- **Resolvers** register per item type (and per `TimerFired` subkind) with `RegisterResolver(EQueueItemType, TWeakInterfacePtr<IQueueResolver>)` from the composition root; one resolver per type; registrations are weak and cleared in `Deinitialize`. Owners: `PawnCommand`/`HoldTimer` IIF; `TimerFired` each timer's owner (Round grace, Penalty respawn, Objectives stage signal, Assassination defence window end, body release and body removal, Round pawn-loss retry, Crowd schedule, Concealment transitions); `PawnLost`/`HostCommand`/`DebugItem` Round.
- Nothing outside the drain mutates gameplay state, with **named apply-only exceptions**: (1) the round-end step (§8.4), which, when bound to a death write, runs in the staging pass and mutates phase, Roster state, score and holds; (2) the Shared Pawn **hand-over pass** (ADR-0010 §4.3), which at the next frame start executes possession changes (and body releases, ADR-0018 §6.1) committed inside an earlier slot; (3) the **`PawnLeavingGame` possession plumbing** (ADR-0010 §5), which hands a disconnecting player's pawn to a crowd controller during controller destruction without touching the binding or issuing any command. Each applies a decision already made in a slot (or, for (3), changes no gameplay state) and makes none of its own.

### 6. Typed staged writes (the pending-write queue)

- Outcome-revealing presentation state is written by **typed staged writes**, not inline: `FStagedWrite { EStagedWriteType Type; double ServerTime; FEpochToken Expected; FQueueKey CauseKey; TVariant<FDeathWritePayload, FCounterPayload, FRoundEndPayload> Payload; }` with `EStagedWriteType { DeathWrite, CounterPose, RoundEndStep }` (closed set; an addition is an amendment). Held `NotNow` replies are **not** a staged write: the latch keeps them in a held-reply buffer that step (d) of the round-end step releases. Every staged write is logged by type to the Event Log.
- `StageWrite` is callable only from inside a slot (asserted). The **staging pass** runs in the same tick function immediately after the drain, every frame: it applies every staged write with `ServerTime ≤ ServerNow`, in order `(ServerTime, CauseKey, TypeRank)` with `TypeRank` DeathWrite < CounterPose < RoundEndStep. A write therefore lands at most one server frame after its time; clients never depend on that lateness, because the payloads carry their own server times (`DeathServerTime`, `ContactServerTime`).
- Staged writes **apply already-decided state** and never make a new gameplay decision; they run even after the latch (an earlier strike's death write still plays, ASN-R21) and are dropped on `Reset` by epoch mismatch.
- **Appliers (architecture-review fix 2026-10-25).** The staging pass applies each write through an applier that a gameplay module registered by write type: `RegisterStagedWriteApplier(EStagedWriteType, TWeakInterfacePtr<IStagedWriteApplier>)`, called only by the composition root (`AStealthGameMode::InitGame` / `OnWorldBeginPlay`, ADR-0007 §4). The rules match resolvers: one applier per type, weak, cleared in `Deinitialize`. `IStagedWriteApplier::ApplyStagedWrite(const FStagedWrite&)` is apply-only (it decides nothing, it never calls `StageWrite` or `ScheduleAt`, and it must treat a stale `FPawnHandle` as a no-op). `DeathWrite` and `CounterPose` are registered by Assassination (`UEncounterService`, ADR-0018 §4.1). `RoundEndStep` is not registrable: the queue routes it to `ITerminalDecision::RunRoundEndStep` (§8.4), and registering an applier for it asserts. A write whose type has no live applier is dropped, logged (`StagedWriteNoApplier`) and reported to the fault sink. The flush barrier runs after every applied write that mutates the Roster (§5).
- Assassination owns the meaning and times of the death-write and counter-pose payloads (`ContactServerTime − StrikeNetLeadSec`; ADR-0018) and their applier; Round owns the round-end step's payload (§8). The structs themselves are declared here (§2a).

### 7. Deferred actor teardown

Resolvers never destroy actors inline. `RequestTeardown(Actor)` queues the destruction; a **teardown pass** runs after the staging pass in the same frame. Because weak handles to an actor awaiting teardown still resolve, later slots in the same drain must treat it as gone: every resolver's re-validation (and the Roster's `Stale` evaluation) checks `IsPendingTeardown(Actor)`. Gameplay state has already changed in the slot; only the actor teardown moves, so the behaviour is observable-identical and the destroy-inside-`TG_PostPhysics` spike is not needed. Spawns requested by resolvers (Crowd replacement through the pawn factory) are allowed inside the slot pending the E4 spike; if it fails, spawns move to the same post-drain pass with their bind-and-possess sequence intact.

### 8. Terminal latch and the Round R9 two-step round end

**8.1 Latch storage, Round's rule.** **Round's latch record (`Terminated`, outcome, `TerminalKey`) is authoritative.** `UResolutionQueue::Latch(FQueueKey)` is single-assignment, is called solely by Round's `TryTerminate` in the same call that sets `Terminated`, and only **closes intake**; `IsLatched()` mirrors Round's record for resolvers that must not include Round headers. Round owns the terminate decision (win predicates, Reason partition, void codes; Round R15) and implements `ITerminalDecision` (no `OnLatched` callback: Round is the caller). A **void** also calls `Latch` and stages no `RoundEndStep` (it leaves `Active` straight to `Reset`, Round R8). After the latch:
- the queue is **closed**: every item still pending, and every command, AI item or timer item submitted before the round-end step, resolves `Rejected_Latched` (logged); the `NotNow` replies the IIF owes for them are **held** in the latch's held-reply buffer and sent at the round-end step, never before;
- every active hold of every pawn kind is **frozen**: its completion item can only return `Rejected_Latched`, it is not cancelled, and its owner's ring keeps running;
- locomotion is untouched (human input and crowd walking continue; only discrete commands are refused, for every pawn kind);
- `HostCommand` items (`ResetRound`, `AbortRound`) are the exception to `Rejected_Latched`: they are **parked unresolved** and handed to Round at the round-end step, which executes a held `ResetRound` at the end of `EndedMinDwellSec` (Round R9 (v));
- a later `TryTerminate` returns false and logs `LateWinSuppressed`.

**8.2 Latch step (server-only).** Inside the winning slot (normally inside its Roster flush): `Terminated` is set and `{Winner, EndReason, RoundIndex}` is held in Round's server latch record; the queue closes (§8.1). **Nothing is replicated or presented in the latch step.**

**8.3 Binding the round-end step.** In the latch step Round stages exactly one `RoundEndStep` write whose `CauseKey` is the deciding slot's `RootCauseKey`. At **slot end** (after `Resolve` and `FlushPending()`, §5) the queue binds it:
- if the deciding slot staged one or more `DeathWrite`s (a strike decided the round: a kill, a reversal or a strike-limit exile, all from one `Attack` slot), the `RoundEndStep`'s `ServerTime` is set to the **latest** of those death writes' `ServerTime`, and the staging pass runs it immediately after that death write in the **same server step** (TypeRank, §6). At defaults that is 0.25 s after the slot (0.60 s for a `BushStrike`);
- if no death write matches but the deciding transition has a strike reason (`Assassinated`, `ReversedAgainst`, `StrikeLimit`) — possible only after a `CascadeOverflow` moved work out of the slot — the binding falls back, **in every build** (not only an assert), to the latest pending `DeathWrite` whose victim is the deciding participant's pawn, and if none exists defers the step by `StrikeNetLeadSec` from the slot; the case is logged as a defect;
- otherwise (objective win, forfeit by grace expiry, debug win), the round-end step **is** the latch step: the staged `RoundEndStep` is **consumed inline** at slot end, inside the drain.
Binding by `CauseKey` rather than by time equality removes any dependence on the order in which a resolver stages its death write and raises the elimination inside the slot. **Accepted (Round R9):** if a later, unrelated item in the same drain decides the round (e.g. a grace-expiry forfeit) while an earlier strike's death write is still pending, `Ended` publishes before that earlier blow lands; R9 permits it (the earlier death write still plays).

**8.4 Round-end step (the observable end, one server step).** The queue calls `ITerminalDecision::RunRoundEndStep(const FRoundEndPayload&)`; Round's handler performs, in order (Round R9 a-h):
(a) phase `Ended` and `FRoundResult {Winner, EndReason (public form), RoundIndex, TeamSummary[2]}` published in `FRoundPublicState` with a forced net update of the GameState (`TeamSummary` per team `{StepsCompleted, EliminatedCount, WrongTargetCount}`, computed in the latch step and zero before this step; user scope decision 2026-10-25); the same step clears the Informant Pings team parts;
(b) `Roster.EndRoundAll()` (batch, never evaluated as an elimination) with a forced net update of every carrier it wrote (the C-TeamView parts on the affected PlayerControllers, ADR-0001), so records leave in the same server frame as the GameState;
(c) `MatchScore[Winner]` incremented in the **same** `FRoundPublicState` write as (a) (staged with the publication);
(d) the held `NotNow` replies are sent (the latch's held-reply buffer is released);
(e) `OnRoundEnded(Result)` fires; the Visibility Policy treats every viewer as `PostRound` from here (MVP releases only `FRoundResult`);
(f) **the whole city holds**: Round calls `ICrowdControl::Halt()` (Crowd stops issuing commands) and the Shared Pawn's kind-blind server stop-and-blend for **every** live pawn (live = strike role not `Dead`; a body awaiting its death write still counts, ADR-0018), AI-driven and human-driven alike (ADR-0010 owns the stop path) — chosen over an `OnRoundEnded` subscriber because R9 requires a fixed order inside the step and delegate subscriber order is unspecified;
(g) every frozen hold cancels through the normal release blend (no failure cue);
(h) `EndedMinDwellSec` and `EndedAutoResetSec` start counting from this step as post-latch-lane items (§4), so `Reset` can never run between the two steps; parked `HostCommand`s are handed to Round; Penalty's `OnRosterRoundEnded` handling runs with (b), followed by the flush barrier.
From the round-end step every command is rejected by class `RoundEnded` (Look allowed).

**8.5 Client arrival rule.** The GameState and the Roster carriers are different actors, so a client may receive `RoundEnded` records a net frame before `Phase = Ended`. Consumers (Teammate Markers TM-R5, HUD states) treat a `RoundEnded` received while their phase is not yet `Ended` as **no change**; they move only when the phase does.

### 9. Server clock and the client server-time estimator

- **Server clock.** `ServerNow()` is the server world's `TimeSeconds` in **double** precision, read on the game thread. No gameplay rule uses wall-clock time.
- **Client estimate.** `EstimatedServerNow(const UWorld*)` (Foundation `Public/View`; the only server-time source presentation may use; not `GetServerWorldTimeSeconds`, E3) is computed from an owner-only clock exchange on `AStealthPlayerController`:
  - the client sends `ServerClockPing(T1)` (unreliable) every 0.5 s for the first 5 s after a controller begins play or after travel, then every 2 s; the server answers `ClientClockPong(T1, T2, T3)` (unreliable, owner only), where `T2` is stamped in the RPC body and `T3` in the drain tick, both on a **precise server clock** `ServerNowPrecise = TimeSeconds + (FPlatformTime::Seconds() − FrameStartPlatformSeconds)`, so neither is quantised to the frame (E8). Client stamps `T1`, `T4` use `FPlatformTime::Seconds()` (not the hitch-clamped world time). The server rate-limits `ServerClockPing` (at most 4 per second per connection; excess dropped). The pair carries **timestamps only** (Owner audience under IVP's existing connection/ping fact; an IVP change request adds the RPC pair to the leak catalogue, recorded under Related);
  - for each pong, NTP four-timestamp form: `Delay = (T4 − T1) − (T3 − T2)`, `Offset = ((T2 − T1) + (T3 − T4)) / 2`; the estimator keeps the last 16 samples and uses the offset of the **minimum-delay** sample (robust to queuing delay and loss); `EstimatedServerNow = FPlatformTime::Seconds() + SlewedOffset`;
  - the estimate is **monotonic**: corrections are slewed at ≤ 50 ms per second; a step is allowed only on first sync or when the error exceeds 250 ms;
  - `IsServerClockSynced()` is false until 5 samples have arrived; consumers then use their documented unsynced fallback (Round R7: time from `Active` arrival);
  - on the listen host, `EstimatedServerNow == ServerNow` exactly (host parity: the host calls the same function).
- **Accuracy budget** (acceptance test, ADR-0008 harness): under `LeakNetProfile` (symmetric PktLag 100 ms, PktLoss 2 %), after sync, `|EstimatedServerNow − ServerNow|` ≤ 15 ms at p95 and ≤ 30 ms at p99 over a 10-minute session; the estimate never decreases; under a 200 ms symmetric profile ≤ 25 ms p95. Asymmetric paths bias any two-way method by half the asymmetry; the test records it and the budget does not cover it (stated limit). The budget is chosen to sit inside the presentation tolerances that consume it (one frame at 60 Hz for clip seeks, the 0.1 s cross-fade bound of ASN-R21).
- **Fallback if the exchange fails the budget:** GameState `ReplicatedWorldTimeSecondsDouble` corrected by half the connection's measured RTT, double-precision and slewed (Alternative 6) — recorded, not chosen.

### Architecture Diagram

```
 SERVER FRAME (listen host)
 ┌─ NetDriver dispatch (E1) ───────────────────────────────────────────────┐
 │  human command RPC ─► IIF ingress ─► mailbox[ParticipantId] (FIFO)      │
 │  CMC ServerMove (Move/Look, not items)                                  │
 ├─ TG_PrePhysics..EndPhysics ─────────────────────────────────────────────┤
 │  Crowd/AI ─► IIF ─► mailbox[ServerLocal]       CMC simulates all pawns  │
 ├─ TG_PostPhysics: FResolutionDrainTickFunction ──────────────────────────┤
 │  1 ServerTickIndex++                                                    │
 │  2 stamp: due-heap items (Due ≤ DrainStart) by (Due, TieRank|BeginKey)  │
 │  3 stamp: mailboxes, base order permuted by SubSeed(SeqOrder, Tick)     │
 │  4 for item in Key order: Resolve ─► inline follow-ons ─► Roster flush  │
 │       └─ TryTerminate (Round) ─► Latch(Key) ─► LATCH STEP (server only) │
 │            └─ StageWrite(RoundEndStep, CauseKey = TerminalKey)          │
 │       └─ slot end: bind RoundEndStep to the slot's latest DeathWrite    │
 │          (no death write ⇒ round-end step runs here, inline)            │
 │  5 due items scheduled inline (Due ≤ now) stamped after intake          │
 │  6 STAGING PASS: writes with ServerTime ≤ now, by (Time, CauseKey, Rank)│
 │       DeathWrite ─► CounterPose ─► RoundEndStep (releases held replies) │
 │  7 TEARDOWN PASS: deferred actor destruction                            │
 ├─ NetDriver replication flush (E1) ──────────────────────────────────────┤
 │  forced updates: GameState (phase Ended), Roster carriers, death writes │
 └─────────────────────────────────────────────────────────────────────────┘
 CLIENT: ServerClockPing/Pong ─► min-RTT offset ─► slewed, monotonic EstimatedServerNow()
         RepNotify ─► RefreshPresentation(EstimatedServerNow) (idempotent, state-derived)
```

### Key Interfaces

```cpp
// ---- StealthFoundation Public/View: value types (client-safe) ----------------------------
struct FQueueKey { uint64 ServerTickIndex = 0; uint64 Seq = 0; };              // total order
// FParticipantId, FEpochToken, FViewerId, FMailboxId, ETransitionReason (§2)
double EstimatedServerNow(const UWorld* World);    // client + host; monotonic
bool   IsServerClockSynced(const UWorld* World);

// ---- StealthFoundation Public/Server/StealthQueuePayloads.h (§2a; server-only) -------------
struct FPawnHandle { TWeakObjectPtr<APawn> Pawn; };   // generation = weak-pointer serial; APawn, never AStealthPawn
struct FSubjectRef { /* tagged: FPawnHandle | FParticipantId | uint16 CourtSocketId */ };
enum class EQueueItemType : uint8 { PawnCommand, HoldTimer, TimerFired, PawnLost, ConnectionTransition,
                                    HostCommand, DebugItem /* !UE_BUILD_SHIPPING only */ }; // closed set
enum class ETimerKind : uint8 { GraceExpiry, InformantRespawn, StageSignal, DefenceWindowEnd,
                                PawnLossRetry, CrowdSchedule, BodyRelease, BodyRemoval,
                                ConcealmentTransition /* post-latch exemption, §4 */,
                                EndedMinDwell, EndedAutoReset /* post-latch lane */ };
enum class EHoldTimerKind : uint8 { Complete, CapEnd, DebounceExpiry, ConditionCancel };
enum class EResolveResult : uint8 { Executed, Voided };
enum class EVoidReason : uint8 { Rejected_Latched, Rejected_StaleEpoch, Dropped_StaleConnection,
                                 Revalidation /* owner-specific code in the log entry */ };
struct FCommandPayload   { FPawnHandle Pawn; uint32 ControlEpoch; FClientCommandSeq ClientSeq; FReplySinkId Sink;
                           uint8 Kind; uint8 Phase; FSubjectRef Target; FVector WorldPoint; uint32 Arg; }; // values: ADR-0011
struct FHoldTimerPayload { FPawnHandle Pawn; EHoldTimerKind Kind; uint8 ActionType; uint32 ActionEpoch; FQueueKey BeginKey; };
struct FTimerPayload     { ETimerKind Kind; FSubjectRef Subject; uint32 Generation; uint32 Seq; uint8 Phase; }; // mapping table §2a
struct FPawnLostPayload  { FParticipantId Participant; FPawnHandle Pawn; };
struct FConnectionTransitionPayload { FParticipantId Participant; uint32 ConnectionGeneration; uint8 Transition; };
struct FHostCommandPayload { uint8 Command; /* ResetRound | AbortRound */ };
struct FQueueItem { EQueueItemType Type; FEpochToken Expected; FMailboxId Source;
                    TVariant<FCommandPayload, FHoldTimerPayload, FTimerPayload, FPawnLostPayload,
                             FConnectionTransitionPayload, FHostCommandPayload, FDebugPayload> Payload; }; // ID-only
enum class EStagedWriteType : uint8 { DeathWrite, CounterPose, RoundEndStep };
struct FDeathWritePayload { FPawnHandle Victim; double DeathServerTime; };      // meaning + applier: ADR-0018
struct FCounterPayload    { FPawnHandle Defender; double ContactServerTime; };  // meaning + applier: ADR-0018
struct FRoundEndPayload   { /* Round (§8) */ };
struct FStagedWrite { EStagedWriteType Type; double ServerTime; FEpochToken Expected;
                      FQueueKey CauseKey;   // the staging slot's RootCauseKey
                      TVariant<FDeathWritePayload, FCounterPayload, FRoundEndPayload> Payload; };

// ---- StealthFoundation Public/Server: contracts ------------------------------------------
// UHT interfaces: TWeakInterfacePtr requires UINTERFACE (E7)
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UQueueResolver : public UInterface { GENERATED_BODY() };
class IQueueResolver { GENERATED_BODY() public: virtual EResolveResult Resolve(const FQueueItem&, FQueueKey) = 0; };
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UStagedWriteApplier : public UInterface { GENERATED_BODY() };
class IStagedWriteApplier { GENERATED_BODY() public: virtual void ApplyStagedWrite(const FStagedWrite&) = 0; }; // apply-only
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UTerminalDecision : public UInterface { GENERATED_BODY() };
class ITerminalDecision { GENERATED_BODY() public: virtual void RunRoundEndStep(const FRoundEndPayload&) = 0; };
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UEventFlushBarrier : public UInterface { GENERATED_BODY() };
class IEventFlushBarrier { GENERATED_BODY() public: virtual void FlushPending() = 0; };   // Roster implements
// FResolutionQueueCore (plain C++) takes raw pointers registered/unregistered by the facade,
// so headless specs inject stubs without UObjects.

class FResolutionQueueCore { /* plain C++; no UWorld; headless specs */ };
UCLASS() class UResolutionQueue : public UWorldSubsystem
{   // facade; owns FResolutionDrainTickFunction (TG_PostPhysics, game thread, registered in
    // OnWorldBeginPlay against the persistent level)
public:
    void Submit(FMailboxId Source, FQueueItem Item);                 // stamped at drain start
    FTimerItemHandle ScheduleAt(double ServerTime, FQueueItem Item); // due-time heap
    void RegisterResolver(EQueueItemType, TWeakInterfacePtr<IQueueResolver>);
    void RegisterTimerResolver(ETimerKind, TWeakInterfacePtr<IQueueResolver>);
    void RegisterStagedWriteApplier(EStagedWriteType, TWeakInterfacePtr<IStagedWriteApplier>); // DeathWrite, CounterPose
    void SetTerminalDecision(TWeakInterfacePtr<ITerminalDecision>);                              // RoundEndStep route
    APawn* ResolvePawnHandle(const FPawnHandle& Handle) const;       // nullptr if stale or pending teardown
    void SetEventFlushBarrier(TWeakInterfacePtr<IEventFlushBarrier>);
    int32 Compare(FQueueKey A, FQueueKey B) const;
    FQueueKey CurrentKey() const;                                    // valid inside a slot
    void StageWrite(const FStagedWrite& Write);                      // inside a slot only
    void RequestTeardown(AActor* Actor);                             // after staging pass
    bool IsPendingTeardown(const AActor* Actor) const;               // treat as gone in later slots
    bool Latch(FQueueKey TerminalKey);   // Round's TryTerminate only; closes intake (Round's record is authoritative)
    bool IsLatched() const;              // mirror for resolvers
    double ServerNow() const;                                        // double, server only
};
// AStealthPlayerController (root module): owner-only clock exchange, timestamps only
//   UFUNCTION(Server, Unreliable) void ServerClockPing(double T1);           // rate-limited
//   UFUNCTION(Client, Unreliable) void ClientClockPong(double T1, double T2, double T3);
```

### Implementation Guidelines

- Every interacting state change must resolve as an item (or as an inline follow-on inside an item's slot) of the round's single queue; nothing outside the drain may mutate gameplay state.
- Systems must order events only with `Compare`/`GetKey`/`CurrentKey`; they must never keep a feature-local clock, timestamp or tick-order assumption to order events against another system.
- Command RPC bodies and AI sources must only push into mailboxes; they must never resolve inline.
- Gameplay timers must be `ScheduleAt` items in the due-time heap; they must never use `FTimerManager`, a feature tick or a latent action.
- Resolvers must re-validate against live state at the slot, must run follow-ons inline, and must never re-submit or re-stamp an outcome; a deferred outcome is a new item with its own key and a logged `CauseKey`.
- Mailbox and tie permutations must come from `GetServerSubSeed(SeqOrder|TieRank, ServerTickIndex)`; they must never use the public seed, join order or a rotating index.
- Outcome-revealing presentation writes must be typed staged writes; they must never decide anything new, and they must be dropped on `Reset`.
- Resolvers must never destroy actors inline; they must call `RequestTeardown`.
- Only Round's `TryTerminate` may call `Latch`; after the latch the queue must reject every item with `Rejected_Latched` except parked `HostCommand`s, the post-latch lane and the `ConcealmentTransition` exemption (§4), hold the IIF's `NotNow` replies, freeze (not cancel) holds and leave locomotion untouched.
- Interfaces held by `TWeakInterfacePtr` must be `UINTERFACE`s; the plain core must take explicitly registered raw pointers.
- Every queue item payload, staged-write payload, `FPawnHandle` and `FSubjectRef` must be declared in `StealthFoundation/Public/Server/` (§2a); a feature module must never declare a type that `FQueueItem` or `FStagedWrite` names, and a feature timer must map onto the generic `FTimerPayload`.
- Staged writes must be applied only through the applier registered for their type by the composition root (or, for `RoundEndStep`, through `ITerminalDecision`); a write with no live applier must be dropped, logged and fault-reported, never applied inline by the staging resolver.
- Resolvers must treat actors pending teardown as gone.
- The latch step must replicate and present nothing; the round-end step must run in one server step, bound to the deciding slot's latest death write by `CauseKey`, or inline when the slot staged none.
- The round-end step must call `ICrowdControl::Halt()` and the kind-blind stop-and-blend for every live pawn in the same step; it must never stop human pawns by input loss.
- Client presentation must schedule only from `EstimatedServerNow()`; it must never use `GetServerWorldTimeSeconds()`, arrival time of a replicated value, or a client wall clock.
- The clock exchange RPCs must carry only timestamps; they must never carry gameplay data.
- Event Log entries for queue activity must never pass through `UE_LOG`/`UE_LOGF`, the output log or trace channels (Round R19).

## Alternatives Considered

### Alternative 1: Order by tick-function order and the engine timer manager
- **Description**: Each system resolves its own commands in its own tick or RPC body; timers via `FTimerManager`; ties fall where the engine schedules them.
- **Pros**: No queue; idiomatic Unreal.
- **Cons**: Cross-actor tick order is unspecified, so "Attack vs Prepare in the same frame" depends on actor registration order (often join order: a player-identity bias); timers fire at frame granularity in unspecified order; no reproducible log; no single latch point.
- **Rejection Reason**: Cannot satisfy "no ties, resolved in order" or reproducibility.

### Alternative 2: Lag-compensated ordering (rewind by estimated one-way delay)
- **Description**: Order commands by estimated client send time.
- **Pros**: Fairer to high-latency players in reflex contests.
- **Cons**: Client-supplied or estimated timestamps are spoofable and become a timing channel; contradicts Round R12 ("no lag compensation") and the anti-pillar "not a reflex game".
- **Rejection Reason**: Contradicts the GDD and opens a cheat surface.

### Alternative 3: One-step round end at the latch
- **Description**: Publish `Ended`, score and `RoundEnded` records in the winning slot.
- **Pros**: Simpler; no staging.
- **Cons**: For a strike-decided round, the result reaches clients up to `StrikeNetLeadSec` (0.25-0.60 s) before the blow lands on screen: the end is revealed before contact (cross-review S-B1); frozen pawns among walking NPCs reveal who was who.
- **Rejection Reason**: Breaks ASN-R23 and Round R9.

### Alternative 4: Round-end step as an `OnRoundEnded` subscriber chain
- **Description**: Round fires `OnRoundEnded`; Crowd, IIF and HUD-side servers react.
- **Pros**: Looser coupling.
- **Cons**: Delegate subscriber order is unspecified; R9's (a)-(h) order and "same server step" become untestable.
- **Rejection Reason**: Ordering contract needs explicit calls; `ICrowdControl` already inverts the dependency.

### Alternative 5: Untyped "pending flag" writes
- **Description**: A generic deferred lambda queue for presentation writes.
- **Pros**: Flexible.
- **Cons**: Unloggable, unauditable, captures raw pointers, can smuggle decisions out of the slot (TD concern 5).
- **Rejection Reason**: Typed, epoch-stamped, logged writes are required.

### Alternative 6: Engine GameState server time (optionally corrected by half RTT)
- **Description**: Use `GetServerWorldTimeSeconds()` or `ReplicatedWorldTimeSecondsDouble + RTT/2`.
- **Pros**: No new RPCs.
- **Cons**: Uncorrected, it lags by the one-way delay and stores a float delta (E3); corrected, the replicated value is stamped by a timer, not at send, so it carries up to one GameState send interval of extra staleness and needs its own filtering; accuracy unproven.
- **Rejection Reason**: Kept as the recorded fallback (§9) if the exchange fails its budget.

## Consequences

### Positive
- "No ties, no draws" is a property of one data structure with unit-testable specs (property tests over permutations, keys, latch, staging).
- Fairness among same-frame sources is exactly 1/2 each way and server-secret; reproducible from the log.
- The round end is one observable server step aligned with the blow, for every terminal cause.
- One client clock for all presentation: HUD plates, concealment clips, contact-timed animation and markers agree on time.
- Deferred teardown removes a spike dependency.

### Negative
- Every gameplay system must be written as a resolver against this contract; direct engine timers and RPC-side effects are forbidden, which is less idiomatic for Unreal programmers.
- One server frame of quantisation for staged writes (≤ 16.7 ms at 60 Hz) — absorbed by payload-carried server times.
- Two new owner-only RPCs per PlayerController for the clock (tiny bandwidth, one allowlist row).
- The queue is a single point of failure: a bug there affects every system (mitigated by headless specs).

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Spawn inside `TG_PostPhysics` misbehaves for CMC pawns (E4) | Low | Medium | Spike with ADR-0010; fallback moves spawns to the post-drain pass |
| Drain or staging overruns the 0.5 ms GT budget in a busy frame (many items, cascades) | Medium | Medium | Per-mailbox cap 32; cascade bound 8; `stat` counters per pass; profile at 10 connections + 40 NPCs |
| Estimator misses budget on real networks (asymmetric routes, Wi-Fi jitter) | Medium | Medium | Min-RTT filter; record asymmetry; fallback (Alternative 6); presentation tolerances stated |
| The decisive transition and its death write end up under different keys (cascade overflow) | Low | High (round-end before blow) | `RootCauseKey` carried through overflow; shipping-build fallback binding to the deciding victim's pending death write, else `StrikeNetLeadSec` deferral; logged defect; AC Round 12a-12d |
| Clock samples biased by frame quantisation (E8) | Medium (if unmitigated) | Medium | Four-timestamp exchange on `ServerNowPrecise`; platform-time client stamps |
| Roster flush not complete before next item | Low | High (stale counts) | `IEventFlushBarrier` + assert after every `Resolve` |
| A system adds a new item or staged-write type ad hoc | Medium | Medium | Closed enums; amendment rule; code review |

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| round-match-framework.md | R9 one round-end ordering contract, two server steps (latch step server-only; round-end step at the deciding strike's death write; arrival rule) | §8.2-8.5 |
| round-match-framework.md | R11 one queue, key, inline follow-ons, Roster flush, drain position, deferred teardown | §1, §5, §7 |
| round-match-framework.md | R12 intake mailboxes, permutation, cap, stale generation, due-time ordering, no lag compensation | §3, §4 |
| round-match-framework.md | R13 resolvers and item types; R14 contract with Assassination (key compare, outcome in slot, deferred outcome as new item) | §1, §5 |
| round-match-framework.md | R15 single-assignment latch (Round's rule); R16 no ties, no draws, no clock | §8.1; §1 total order |
| input-interaction-framework.md | R5 two-stage validation (stage 2 at the slot); R8 hold timers as queue items with Begin-key tie order; R9 one action slot (resolved in key order) | §4, §5 |
| assassination-defence-reversal.md | ASN-R2 re-check at resolution; ASN-R5 defence sampled at Attack execution; ASN-R13 mutual/chained strikes by key | §5 (resolution at the slot, key compare) |
| assassination-defence-reversal.md | ASN-R21 outcome writes at contact via a pending-write queue, written even after a latch, dropped on Reset; ASN-R22/R23 presentation by server time | §6, §9 |
| npc-kill-penalty-informants.md | PEN-R8 informant respawn as a due-time-heap `TimerFired` at `T_due` | §4 |
| objectives-chain.md | OC-10 completion gate inside the `HoldTimer(Complete)` slot, `DeclareObjectiveWin` | §1, §5 |
| participant-state-roster.md | R12 events queued and flushed FIFO after the outermost call | §5 `IEventFlushBarrier` |
| teammate-markers.md / hud.md | TM-R5 and HUD state rule: `RoundEnded` before `Phase = Ended` is no change | §8.5 |

## Performance Implications
- **CPU**: Drain + staging + teardown + Roster flush within 0.5 ms of the listen host's game thread (architecture budget). Typical frame: tens of items; mailbox cap bounds a flood. Heap operations O(log n).
- **Memory**: Per-round queue, heap and staged writes: a few KB; Event Log is ADR-0023/Round's.
- **Load Time**: None.
- **Network**: Clock exchange: ~2 small unreliable RPCs every 2 s per client after warm-up (< 50 B/s). Forced net updates at the round-end step are one-off.

## Migration Plan
The prototype (`Source/StealthGame`) resolves commands inline in `Server*` RPC bodies on the pawn and ends rounds in `AStealthGameMode::EndRound` directly; it has no queue, no latch, no staging and uses `GameState` server time. None of it is promoted (architecture "Prototype promotion rule"); the queue is new code, written first as `FResolutionQueueCore` with headless specs, then the facade and the tick function, then resolvers per owning system as their ADRs are accepted. `StealthRules::EvaluateEliminationWinner` is evidence that the win predicates are pure functions and can be unit-tested; Round's `TryTerminate` replaces it.

## Validation Criteria
- Headless specs (`StealthGame.Queue.*`): key uniqueness and total order; per-tick permutation is uniform (χ² over 10⁵ ticks, each of M! orders); same-source FIFO preserved; cap and stale-generation drops; due-time stamping before/after intake; `HoldTimer` equal-due ordering by `Begin` key; cascade bound; latch single-assignment; `Rejected_Latched` after latch; held replies released only at the round-end step; staging order `(ServerTime, CauseKey, TypeRank)`; `Reset` drops staged writes by epoch.
- `StealthQueuePayloads.h` and the queue facade compile in `StealthFoundation` with no `StealthGameplay` include (ADR-0007 lint (e)); headless specs cover `ResolvePawnHandle` on a destroyed and a pending-teardown pawn, the applier route per `EStagedWriteType`, and the no-applier drop with its fault report.
- Round ACs 12a-12d (two-step round end, arrival rule) pass in multi-client PIE: for a strike-decided round, no client receives `Phase = Ended`, a `RoundEnded` record or a score change before the deciding death write's server step; for an objective win, they leave in the latch slot's frame.
- Assassination ACs for same-frame Attack/Prepare ordering by key (both orders observed ≈ 50 %).
- Estimator acceptance test (§9 budget) under `LeakNetProfile` and the 200 ms profile; monotonicity check.
- Profiling: drain + staging ≤ 0.5 ms p95 on the listen host with 10 connections and 40 NPCs (target-class hardware capture, AQ-18).

## Related
- ADR-0001 (crowd identity rules): the C-TeamView carriers the round-end step force-updates, and the `PostRound` viewer class.
- ADR-0003 (replication and relevancy): forced-update policy for death writes and the un-cull timing at the death write.
- ADR-0006 (seeds), ADR-0007 (modules, composition root), ADR-0008 (harness): dependencies.
- ADR-0010 (stop-and-blend path), ADR-0011 (IIF), ADR-0016 (Crowd Halt), ADR-0018 (death-write and counter-pose payload meaning, times and applier), ADR-0019 (informant respawn), ADR-0020 (objectives), ADR-0021 (HUD timing), ADR-0023 (Event Log).
- **GDD change requests raised by this ADR (not applied here; for the owning GDDs' next revision):** (1) Round R13 item table: add `HostCommand`, `PawnLost` (already in R17.9) and `ConnectionTransition`, and the post-latch timer lane for `EndedMinDwellSec`/`EndedAutoResetSec`. (2) Visibility Policy R11/R12: list the owner-only clock exchange RPC pair (timestamps only) under the Owner connection/ping fact. (3) GDDs that say "GameState server clock" for presentation (ASN-R21, CON-R6a, Round R20, HUD) are read as `EstimatedServerNow()`.
- Design: `design/gdd/round-match-framework.md` (R9-R16), `design/gdd/gdd-cross-review-2026-10-24.md` ("Re-check after fixes": S-B1, S-B2), `design/gdd/assassination-defence-reversal.md` (ASN-R2, R5, R13, R21-R23), `design/gdd/input-interaction-framework.md` (R5, R8, R9), `design/gdd/npc-kill-penalty-informants.md` (PEN-R8), `design/gdd/objectives-chain.md` (OC-10), `design/gdd/participant-state-roster.md` (R12), `design/gdd/teammate-markers.md` (TM-R5), `design/gdd/hud.md`.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §5: hand-over pass and `PawnLeavingGame` possession plumbing named as apply-only exceptions next to the round-end step — requested by ADR-0010.
- §1, Key Interfaces: `HoldTimer(ConditionCancel)` added to the closed item set — requested by ADR-0011.
- §1, §4, Key Interfaces: `TimerFired(CrowdSchedule)` added, and the "AI decision timing is input generation" definition recorded — requested by ADR-0016.
- §1, §4, §8.1 guideline, Key Interfaces: `TimerFired(ConcealmentTransition)` added as the post-latch exemption limited to Concealment state, cull and flags — requested by ADR-0017.
- §1, §4, Key Interfaces: `TimerFired(BodyRelease)` and `TimerFired(BodyRemoval)` added (owner Assassination; no post-latch exemption) — requested by ADR-0018.
- §8.4(f): "every live pawn" defined as strike role not `Dead` — requested by ADR-0018.

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §2, new §2a, §6, Key Interfaces, guidelines: Foundation owns every queue and staged-write payload type, `FPawnHandle` (weak `APawn` pointer whose serial number is its generation) and `FSubjectRef`, in `Public/Server/StealthQueuePayloads.h`; `FTimerPayload` is one generic fixed shape `{Kind, Subject, Generation, Seq, Phase}` with a per-timer mapping table; `FConnectionTransitionPayload` added to the variant (the item type existed without a payload); queue types moved out of `Public/View` — architecture-review fix 2026-10-25 (Conflict 1, blocking).
- §6, Key Interfaces: `RegisterStagedWriteApplier(EStagedWriteType, TWeakInterfacePtr<IStagedWriteApplier>)` from the composition root; `RoundEndStep` routes to `ITerminalDecision`; missing applier = dropped + logged + fault; `ResolvePawnHandle` — architecture-review fix 2026-10-25 (Conflict 1, blocking).
- Engine Compatibility, Verification (3): restated as a listen-host frame-time condition (`NetServerMaxTickRate` ignored on a listen server without `bClampListenServerTickRate`, [V]); the clamp-flag alternative stays a pending user item (Config edit) — architecture-review fix 2026-10-25 (Engine finding 2).

User scope decisions (2026-10-25: "accept all recommendations"); decision unchanged in substance, Status unchanged (Accepted).
- Round-end step (a) — user decision 2026-10-25: `FRoundResult` gains `TeamSummary[2]` (`FRoundTeamSummary {uint8 StepsCompleted, uint8 EliminatedCount, uint8 WrongTargetCount}`) for the MVP text-only round summary (HUD-E21). It is computed in the latch step from the latch record, Objectives' store and the Event Log, is zero in every earlier `FRoundPublicState`, and is published only in the round-end step (6 bytes; team aggregates only; IVP `RoundTeamSummary` row; Round R9 (a), AC 12d). The round-end step also clears every `UTeamPingViewComponent` (Informant Pings PNG-R10). No change to the queue, the latch or the staging order.

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §1, §2a (mapping table), Key Interfaces: `TimerFired(CrowdSchedule)` is confirmed as a member of the closed `ETimerKind` set (already added by the 2026-10-24 sync; the Crowd re-review follow-up that said the kind was still missing is closed). The `Phase` byte is a Crowd-owned `uint8` event kind (ambient exit, ambient entry, replacement, retry, exile-exit slot; ADR-0016 §6); adding an event kind is a Crowd/ADR-0016 change, not an amendment here, because the payload shape is unchanged. The "AI decision timing is input generation" definition requested by ADR-0016 is recorded in §4's guideline text and stands.
