# ADR-0003: Replication, relevancy and the concealment cull hybrid

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one. **Exception, 2026-10-25:** S10's failures sat in mechanisms this ADR had handed to S10 (E10, E11) and were resolved by the dated, gated amendment "S10 resolution 2026-10-25" (the S10 gate is now the S10 re-run, story ivp-056); a failure that reverses any §5 decision still requires a superseding ADR.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. Cannot be Accepted before ADR-0001, ADR-0002, ADR-0007 and ADR-0008 are Accepted and spikes S9, S10, S12 and S13 have reported.

## Date
2026-10-24

## Last Verified
2026-10-25 (S10 resolution: E10, E11, E18 re-read against the UE 5.8.4 source); 2026-10-24 (engine claims re-read against the UE 5.8.4 install; design against IVP R5-R8, R11 and the reconciled GDDs of 2026-10-24)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (global grid stated as replacing R7.2's per-pawn boundary, with an IVP wording change request; dropped engine `ForceNetUpdate` callers listed, with an R5 wording change request; fallback-feed removal moved to after open-ack plus one update (TM-R4a); component ban aligned with IVP R11; relation change while culled handled through binding events). Engine validation: BLOCKING (one config row) → fixed in the text by the §1 `NetClientTicksPerSecond` row (listen-host client throttle; the config edit itself is a pending user item); E10 moved to [V]; minor notes applied (`FFixedWarpOffset` UPROPERTYs/`WithIdentical`; listen-host frame rate).

## Summary
ADR-0001 fixed that every pawn is one class with one replication configuration and that the legacy NetDriver is used; this ADR fixes the mechanisms that make that true on the wire and the one permitted exception: the concealment cull. It specifies the pawn net parity configuration, the forced-update policy, `RemoteViewPitch16` from `Look`, smoothing parity, fixed-width serialisers, the replicated-property allowlist and its audit, the team fallback position feed, and the legacy-only concealment cull (quantised delay, a relevancy override plus an explicit per-connection channel close, re-open behaviour, un-cull in the same server step).

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4) |
| **Domain** | Networking |
| **Layer** | Foundation |
| **Knowledge Risk** | HIGH — networking 5.4-5.8; several actor net-config members changed access in 5.5 |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/networking.md`, `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Foundation layer, Data Flow §1, Performance budget, AQ-05/AQ-06/AQ-11); ADR-0001, ADR-0002; engine source read this session: `Engine/Private/Pawn.cpp:1274-1299`, `Engine/Private/NetDriver.cpp:523-526, 4863-4887, 5462, 5746-5800, 5878-5887, 5929-5931`, `Engine/Private/NetworkObjectList.cpp:586-597`, `Engine/Private/Actor.cpp:3749-3761`, `Engine/Classes/Engine/NetConnection.h:736-741`, `Engine/Classes/Engine/ActorChannel.h:178`, `Engine/Classes/Engine/NetDriver.h:875-880, 923-928`, `Engine/Classes/GameFramework/Actor.h:488-490, 898-914, 3089, 3723, 4624, 4636`, `Engine/Classes/GameFramework/Pawn.h:52, 129-134, 227-234, 293`, `Engine/Classes/GameFramework/CharacterMovementComponent.h:245, 815-821` |
| **Post-Cutoff APIs Used** | `AActor::SetNetUpdateFrequency` / `SetMinNetUpdateFrequency` / `SetNetCullDistanceSquared` (direct member access deprecated in 5.5) [V]; `UNetDriver::GetNetServerMaxTickRate`/`SetNetServerMaxTickRate` (public member deprecated 5.3) [V]; `APawn::RemoteViewPitch16`, `SetRemoteViewPitch` (5.6) [V] |
| **Verification Required** | **S9** the explicit close lands at the deadline and the channel stays closed for that connection while culled (no re-creation through the relevancy timeout or a forced-relevant frame). **S10** re-open: new client actor, NetGUID reuse or not, AnimBP initialisation from `FActionState`, death fast-forward, client registries tolerate the destroy. **S12** cull delay vs measured L99 under `LeakNetProfile`. **S13** CMC cadence and correction parity for PC- vs AI-possessed pawns seen by a third client, adaptive frequency off, role `SimulatedProxy` for both. Fixed-width `NetSerialize` byte identity for kill, whiff and reversal. 10-connection bandwidth capture (S6). Harness startup assertion that `NetClientTicksPerSecond` ≥ `MaxConnections × MaxFps` (E15, `MaxFps` defined in §1) and a harness frame-time capture showing the listen host's frame time ≤ 1/60 s at p99 under the profiled load (E17; replaces any `NetServerMaxTickRate` check) |

**Engine facts this ADR relies on** ([V] read in 5.8.4 source this session; [U] unverified, owned by the named spike):

| # | Fact | Tag |
|---|---|---|
| E1 | `APawn::IsNetRelevantFor` returns true for `bAlwaysRelevant`, `RealViewer == Controller`, `IsOwnedBy(ViewTarget/RealViewer)`, `this == ViewTarget`, `ViewTarget == Instigator`, and `IsBasedOnActor` in both directions; it forwards to a movement base; otherwise distance relevancy if enabled | [V] `Pawn.cpp:1274-1299` |
| E2 | Legacy `ServerReplicateActors`: an actor with an open channel is re-tested for relevancy only every `min(RelevantTimeout, 1 s)`; its channel stays open while `ElapsedTime − RelevantTime < RelevantTimeout` or `ForceRelevantFrame ≥ LastProcessedFrame`; when neither holds, a non-startup actor's channel is closed (`EChannelCloseReason::Relevancy`), which destroys it on the client | [V] `NetDriver.cpp:5746-5773, 5878-5887`; `NetDriver.h:923-928` |
| E3 | With no channel, relevancy is evaluated whenever the actor is considered, and a channel is created only if the actor is relevant (or force-relevant) | [V] `NetDriver.cpp:5746-5800` |
| E4 | Legacy `UNetDriver::ForceNetUpdate` only moves the actor's `NextUpdateTime`; it does not change relevancy. `ForceRelevantFrame` is per actor (not per connection) and is set by `UNetDriver::ForceActorRelevantNextUpdate`; `AActor::ForceNetRelevant` makes an actor replicated and always relevant | [V] `NetDriver.cpp:4863-4887, 5929-5931`; `NetworkObjectList.cpp:586-597`; `Actor.cpp:3749-3761` |
| E5 | A server can find a connection's channel for an actor with `UNetConnection::FindActorChannelRef` and close it with `UActorChannel::Close(EChannelCloseReason)` | [V] `NetConnection.h:736-741`; `ActorChannel.h:178` |
| E6 | `net.UseAdaptiveNetUpdateFrequency` defaults to 0 | [V] `NetDriver.cpp:523-526` |
| E7 | `NetUpdateFrequency`, `MinNetUpdateFrequency`, `NetCullDistanceSquared` public access is deprecated (5.5): use the setters; `NetServerMaxTickRate` via getter/setter (5.3) | [V] `Actor.h:898-909, 4624, 4636`; `NetDriver.h:875-880` |
| E8 | `AActor::ForceNetUpdate`, `AActor::PreReplication` and `AActor::IsNetRelevantFor` are virtual; `APawn::SetRemoteViewPitch(double)` writes `RemoteViewPitch16` | [V] `Actor.h:990, 3089, 3723`; `Pawn.h:52, 227-234, 293` |
| E9 | CMC exposes `NetworkSmoothingMode` and listen-server simulated smoothing times (`ListenServerNetworkSimulatedSmoothLocationTime`/`RotationTime`) | [V] `CharacterMovementComponent.h:245, 815-821` |
| E10 | `Close` removes the channel from the connection's lookup immediately (`FindActorChannelRef` then returns null); with no channel, relevancy is re-tested at every update check and a channel re-opens only if the actor is relevant or force-relevant (set only by the two calls §2 bans). So a closed channel stays closed while the override returns false. A cull followed by an un-cull before the client acknowledges the close (two channels for one NetGUID in flight) is unverified — **revised: unsafe; see amendment "S10 resolution 2026-10-25", S10-R1** | [V] `DataChannel.cpp:2408-2412, 2470`; `NetDriver.cpp:5752, 5774`; `NetworkObjectList.cpp:597` / [V runtime] in-flight race: S10 (FAIL) |
| E11 | Re-open creates a new client actor instance; whether the NetGUID is reused, and how the AnimBP initialises, is unknown — **revised: NetGUID reused, first frame un-posed; see amendment "S10 resolution 2026-10-25", S10-R1** | [V new instance per IVP R7.7] / [V runtime] S10 |
| E12 | A `USTRUCT` with a custom `NetSerialize` (`WithNetSerializer`) is sent as a whole when any field changes, with exactly the bits it writes, on the legacy driver | [U] spike (byte capture) |
| E13 | CMC simulated-proxy update cadence and corrections are identical for PC- and AI-possessed pawns under the fixed frequency | [U] S13 |
| E14 | Each actor's `NextUpdateTime` gets a random delay of up to one server tick (`net.DisableRandomNetUpdateDelay` default 0), so per-pawn update boundaries are not a common grid (identical treatment for every pawn) | [V] `NetDriver.cpp:565-572, 5416-5425` |
| E15 | **Listen-host client throttle:** the server updates only `NetClientTicksPerSecond × dt` client connections per frame, in rotation (≈ 3 of 9 clients per frame at 60 fps with the default 200); a skipped client is re-queued only for actors that already have a channel, so a pawn that just became relevant waits for its next regular update (≈ 67-83 ms, different per client). `bTickingThrottleEnabled` is private with no setter; `NetClientTicksPerSecond` is an `[/Script/Engine.Engine]` config value | [V] `NetDriver.cpp:5204-5214, 6411-6425, 6483-6484`; `NetDriver.h:2471`; `Engine.h:1866`; `BaseEngine.ini:337` |
| E17 | On a listen server `UGameEngine::GetMaxTickRate` clamps the engine tick to `NetServerMaxTickRate` only when `bClampListenServerTickRate` (a `UPROPERTY(Config)` on the net driver) is true; otherwise the listen host ticks at its render rate (the default config) | [V] `GameEngine.cpp:1742-1745`; `NetDriver.h:868-876` |
| E16 | Every engine caller of `ForceNetUpdate` goes through the virtual `AActor::ForceNetUpdate`; `UNetDriver::ForceNetUpdate` is called directly only from it. Engine callers that reach a pawn: possession paths, `APawn::BecomeViewTarget`, the channel-saturation retry in `ServerReplicateActors`, the PlayerController pause loop. Plugin callers (e.g. GAS) unchecked | [V] `Actor.cpp:3013-3026`; `Pawn.cpp:434`; `NetDriver.cpp:5865`; `PlayerController.cpp:1125` / [U] plugins |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (identity invariants, carriers, legacy NetDriver; this ADR implements its pawn-side mechanisms and is scoped to the legacy cull path because ADR-0001 chose legacy); ADR-0002 (`ServerNow`, the drain and staging pass order, the death-write step at which a non-concealment cull re-opens); ADR-0007 (module boundaries: Visibility Policy in `StealthFoundation`, pawn in `StealthGameplay`); ADR-0008 (raw-bunch capture, allowlist audit, net-emulation profiles) |
| **Enables** | ADR-0010 (Shared Pawn implements the net config, `ForceNetUpdate` and `PreReplication` overrides), ADR-0017 (Concealment pushes cull state; clip scheduling), ADR-0018 (encounter data uses the fixed-width serialisers and the forced-update policy), ADR-0021 (Markers read the fallback feed), ADR-0024 (Iris migration re-expresses the cull as exclusion group filters) |
| **Blocks** | Shared Character & Locomotion net work, Concealment, Assassination encounter replication, Teammate Markers feed, the leak test suites |
| **Ordering Note** | Third in table order. The Iris filter path is **not** specified here (ADR-0001 picked legacy; TD concern 6): it moves to ADR-0024. The two schema items once slotted here, `FActionState.StartServerTime` (Concealment CR 10(b)) and the per-entry sequence of the fallback feed (Markers OQ14), were approved by the user 2026-10-25 and are specified in §4 and §7 (architecture AQ-11 closed) |

## Context

### Problem Statement

With ~50 pawns always relevant to ~10 connections, every per-kind difference in replication — cadence, priority, forced updates, field presence, struct size, relevancy timing — is a packet-level detector of players (IVP L1-L7, F1, F2). At the same time the game needs exactly one kind-blind exception: a player (or NPC) hidden in a bush must not be sent to non-allies at all, because client-side hiding alone gives a modified client x-ray vision of every bush (IVP R7, L29). Unreal's legacy relevancy closes a channel only after `RelevantTimeout` and re-tests open channels at most once per `min(RelevantTimeout, 1 s)` (E2), so a relevancy override alone would cull on the network's schedule with jitter — itself a timing channel. The mechanisms must be fixed before the Shared Pawn, Concealment and Assassination replicate anything.

### Constraints

- ADR-0001: one pawn class, one replication configuration, owner-only possession pointers, legacy NetDriver, no ReplicationGraph, no team relevancy.
- Relevancy may depend on concealment state and viewer relation only (IVP R6).
- No dormancy, no distance cull, no per-kind priority, no multicast RPC on pawns (SCL R13, IIF R12).
- Budgets: per-client ≤ 50 KB/s, pawns ≤ 36 KB/s line, 31.1 KB/s worst case at NpcCount 50 (SCL F9; line raised from 30 KB/s, 2026-10-25); replication ≤ 1.5 ms game thread on the listen host (architecture CPU table).
- The listen host does not receive RepNotify for its own writes (ASN-R21, Round R20).
- Always-ask items (schema changes) cannot be decided here.

### Requirements

- Identical net config on class defaults and live instances of an NPC pawn, a remote-client pawn, a host pawn and a just-taken-over pawn (IVP R11, AC-IVP-45).
- Forced updates only for `FActionState`/`TraversalState` writes and death writes, same cap for every kind (IVP R5, ASN-R21).
- `RemoteViewPitch16` written from the `Look`/`HeadLook` source for every pawn (IVP R5, SCL R8.4).
- Smoothing parity incl. listen-host remote-client pawns (SCL R14).
- Concealment cull: quantised delay `ConcealCullDelayMs` (F3); cull at the first net-update boundary ≥ `t0 + D`, all culls of one frame at one boundary; never ack-dependent; un-cull in the same server step; killed-concealed re-open at the death write with `bDiedConcealed`; based-on clause dropped (IVP R7, L27).
- Fixed-width serialisation of outcome-bearing structs (ASN-R21).
- One checked-in allowlist with an audit that fails the build (IVP R11).
- Team fallback position feed with the TM-R4a invariant (IVP R8).

## Decision

### 1. Pawn net parity configuration

Set once in `AStealthPawn`'s constructor from the frozen tuning snapshot's SCL keys (values in config/tuning, never literals), and asserted by the audit on class defaults and live instances (§8):

| Setting | Value | API |
|---|---|---|
| Update frequency | `NetUpdateFrequency = MinNetUpdateFrequency = PawnNetUpdateHz` (15) | `SetNetUpdateFrequency`, `SetMinNetUpdateFrequency` (E7) |
| Adaptive frequency | off: `net.UseAdaptiveNetUpdateFrequency = 0` (engine default, E6) asserted at startup; CMC throttling cancellation disabled for pawns | cvar check in the harness boot test |
| Priority | `NetPriority = 3.0` | member |
| Dormancy | `DORM_Never`; no `SetNetDormancy`/`FlushNetDormancy` call on a pawn anywhere | grep ban |
| Cull distance | none: the relevancy override (§5) never consults distance; `SetNetCullDistanceSquared` left at default and unused; `bUseDistanceBasedRelevancy` irrelevant because Super is never called | — |
| Relevancy flags | `bAlwaysRelevant = false`, `bOnlyRelevantToOwner = false`, `bNetUseOwnerRelevancy = false` | members (E1) |
| Server tick | On the listen host the server tick **is** the host's render frame: `NetServerMaxTickRate` applies to a listen server only when `bClampListenServerTickRate` is set, which also caps the host's render rate [V] (E17). The rule is therefore stated on the **host frame time** `T_frame` (SCL F9 / I-SCL-10 as restated 2026-10-25): the host sustains a frame rate ≥ 60 (`T_frame` ≤ 1/60 s) at the profiled load, so each pawn's update boundary falls at most one host frame after its 1/`PawnNetUpdateHz` interval, `U_eff ≤ 1/PawnNetUpdateHz + T_frame` = 0.083 s at 15 Hz. The quantisation is identical for every pawn kind. There is no integer-multiple requirement and no `NetServerMaxTickRate` startup check; the harness frame-time capture (p99) checks it. The clamp flag is a recorded alternative (pending user item, Config edit) | host frame-rate requirement; harness frame-time capture |
| **Client throttle** | `NetClientTicksPerSecond` ≥ `MaxConnections × MaxFps` (e.g. ≥ 10 × 240 = 2400), where **`MaxFps`** is the listen host's frame-rate ceiling: the engine frame-rate limit (`t.MaxFPS`) when one is set, otherwise the design ceiling of 240 fps. The harness logs any host frame above `MaxFps`, because the rotation of E15 returns above it (definition added 2026-10-25), so the listen host updates **every** client every frame and the rotation of E15 is effectively off; asserted at startup by the harness and covered by S9. A `Config/DefaultEngine.ini` edit, outside this pipeline's write scope: **pending user item** | `[/Script/Engine.Engine] NetClientTicksPerSecond` (E15) |
| Role | `SimulatedProxy` on every non-owner for both kinds | verified by S13 |
| Acceleration replication | `p.EnableCharacterAccelerationReplication=1` (`Config/DefaultEngine.ini [ConsoleVariables]`, inherited from the sample): `ACharacter::GatherCurrentMovement` writes `FRepMovement.Acceleration = CMC->GetCurrentAcceleration()` and `bRepAcceleration = true` for every Character; stays on for both kinds, never overridden per class or instance (`ShouldReplicateAcceleration` is not overridden); see amendment 2026-10-25b | boot test asserts CVar = 1; pawn audit asserts `bRepAcceleration` on a replicated NPC, remote-client and host pawn |

No per-class or per-instance override of any row exists. Every pawn is relevant to every connection except through the concealment cull (§5).

### 2. Forced-update policy

- `AStealthPawn` overrides `ForceNetUpdate()` (E8, E16). It forwards to `Super` **only** inside a scoped reason token `FStealthForcedNetUpdate(EForcedUpdateReason)` with reasons `{ActionStateWrite, TraversalStateWrite, DeathWrite}`; every other call is **dropped** and counted in development builds: the fixed 15 Hz cadence sends the change at the next boundary. Dropped engine callers, accepted as dropped (the effect is identical for every pawn kind): the possession paths (ADR-0001 §4.2), `APawn::BecomeViewTarget`, the channel-saturation retry in `ServerReplicateActors`, and the PlayerController pause loop. Any plugin caller is dropped the same way. **IVP change request (wording):** R5's "deferred to the next fixed-cadence boundary" reads "dropped; the next fixed-cadence update carries the change".
- Cap: at most one forwarded forced update per pawn per server frame, for every pawn kind; extra scoped requests in the same frame coalesce.
- Assassination's forced updates (on the attacker at the strike slot; on every pawn written at the death write) use the same scoped reasons, identically for every outcome and victim kind (ASN-R21).
- `AActor::ForceNetRelevant()` and `UNetDriver::ForceActorRelevantNextUpdate()` must never be called for a pawn (E4: they would make a culled pawn relevant to every connection). A legacy forced update does not change relevancy (E4), so a death write's forced update on a culled victim does not re-open it.
- The GameState is not a pawn: Round forces a GameState net update on every Round mutation (Round R20), and the round-end step forces updates on the GameState and the Roster carriers (ADR-0002 §8.4).

### 3. One pitch path, smoothing parity

- `AStealthPawn::PreReplication` calls `Super` (which writes pitch from the control rotation only when a controller exists) and then always calls `SetRemoteViewPitch` from the clamped `HeadLook` pitch (E8), for every pawn, with or without a controller (IVP R5, SCL R8.4). `HeadLook` yaw replicates as one byte (SCL R8.4).
- Every simulated proxy uses `NetworkSmoothingMode = Exponential` with `PawnSmoothLocationSec`, the rotation smoothing time and `PawnSmoothSnapThresholdCm`; on the listen host the CMC's listen-server simulated smoothing times are set to the same values (E9), so remote-client pawns moved by `ServerMove` smooth like NPCs (SCL R14, AC-SCL-22). Client-authoritative position is never enabled; the transform timestamp is never forced.

### 4. Fixed-width serialisers

- `FActionState {Pose, Seq, Variant, StartServerTime, ContactServerTime, WarpPoint}` (Foundation `Public/View`; **declared once, in ADR-0018's Key Interfaces**, which owns its semantics, its `EActionPose` type and its `WithNetSerializer` + `WithIdentical` traits; this section fixes only the wire rule; `FFixedWarpOffset` is a `USTRUCT` whose members are all `UPROPERTY`s, or it provides `WithIdentical`, because legacy change detection compares UPROPERTYs) implements `NetSerialize` with `WithNetSerializer` and writes **the same number of bits for every value**: `Pose` 8 bits, `Seq` 16, `Variant` 8, `ContactServerTime` 64 (double), `WarpPoint` as three fixed-width signed offsets from the attacker (bit width and resolution chosen in ADR-0018 from F1's translation bound). No magnitude-packed vector, no optional field, no conditional bit. Kill, whiff and reversal produce byte-identical sizes (E12 spike).
- `FDeathState {DeathServerTime, bDiedConcealed}` (double, sentinel −1), `TraversalState` and `HeadLook` yaw are fixed-width likewise.
- `FActionState.StartServerTime` (Concealment CR 10(b); approved by the user 2026-10-25) is another fixed 64-bit field written in every `FActionState` write for every pawn kind, so the struct stays fixed-width; `ContactServerTime` no longer doubles as the hide-pose start time. `FDeathState.bDiedConcealed` (Concealment CR 10(a); approved the same day) is a fixed 1-bit field written only in the death write.
- Iris serialisers for these structs are ADR-0024's.

### 5. The concealment cull (legacy NetDriver only)

**5.1 Ownership.** Concealment (Feature) decides concealment; the Visibility Policy (Foundation) owns the audience and the cull; the Shared Pawn provides the relevancy hook. Concealment pushes state through the consumer-owned `IConcealmentCullSource` (architecture: `SetCullState(Pawn, bConcealed, CommitServerTime)`); the Policy never names a feature type.

**5.2 Per-pawn cull record** (server-only, in the Policy): `{ECullState State ∈ {Visible, CullPending, Culled}; double CommitServerTime; double CullDeadline; ECullReason Reason}`. `Reason` is `Concealment` (the reconciliation-era `KilledConcealed` reason is retired with the interim rule, user decision 2026-10-25).

**5.3 Deadline on one global grid.** `CullDeadline` = the first point of a **global boundary grid** (multiples of `1 / PawnNetUpdateHz` from the round's `Active` entry, shared by every pawn) at or after `CommitServerTime + ConcealCullDelayMs` (IVP F3: default 250 ms at `LeakNetProfile`). Every cull committed in one server frame therefore lands at the same boundary, its timing carries no fine information, and it never waits on a client acknowledgement (IVP R7.2). The grid **replaces** R7.2's "first pawn net-update boundary": per-pawn boundaries carry a random offset (E14) and are not a common grid, so they could not satisfy "all culls of one frame at one boundary". **IVP change request (wording):** R7.2 reads "the first point of the global pawn-update grid".

**5.4 Relevancy override.** `AStealthPawn::IsNetRelevantFor` **replaces** `Super` entirely (E1: no controller, view-target, instigator, owner or based-on clause; no distance):
- record `Visible` or `CullPending` → `true` for every connection (the `CullPending` pawn is stationary with its body already hidden locally at the server-time swap, ADR-0017 §5 — no anim notify gates the hide);
- record `Culled` → `true` only if the viewer is the pawn's owner or an ally of the pawn's participant (`CanViewerKnow(V, S, Position)`, from the Policy's per-connection viewer cache), else `false`; for a concealed NPC, `false` for every connection.
The viewer is resolved from `RealViewer` (the connection's PlayerController) through a server-side `PlayerController → FViewerId` cache maintained at login, rejoin and controller swap (never from client input); an unknown viewer is denied for a culled subject (default deny). Every actor owned by or attached to a pawn either uses no replication (ADR-0001 forbids replicated pawn-owned actors) or joins the same cull.

**5.5 Explicit close at the deadline.** A `FConcealmentCullTickFunction` (game thread, `TG_PostUpdateWork`, i.e. after ADR-0002's drain and staging pass and before the replication flush) moves records whose `CullDeadline ≤ ServerNow` to `Culled` and, for every connection the override now denies, closes that connection's channel for the pawn (`FindActorChannelRef` → `Close(EChannelCloseReason::Relevancy)`, E5) in the same frame. Without it the legacy driver would keep the channel open up to `min(RelevantTimeout, 1 s)` + `RelevantTimeout` (E2). From then on the override keeps returning false and no channel is re-created for that connection (E3, E10 [V]).
**Relation changes while culled.** Teams are locked in-round and Active → Informant never changes a relation (IVP R2 class invariants), but a culled **player** pawn can stop being bound: `ReleaseToCrowd` at grace expiry or an informant's disconnect turns it into a concealed NPC, which must become culled for its former allies too (IVP edge case "disconnects while concealed"). The Roster's binding events (`ReleaseToCrowd`, `ClearBinding`) therefore notify the Policy, which re-evaluates that pawn's denied connections and closes the newly denied ones at the next grid boundary through the same cull tick.

**5.6 Un-cull in the same server step.** On `HideExit` execution, on `OnConcealmentBroken` (attacking from the bush), or on a cull cancelled during `CullPending`, Concealment pushes `bConcealed = false` inside the queue slot; the record returns to `Visible` immediately, so the override returns true in that frame's replication flush. Each of these events is an `FActionState` write that already forces an update under §2 (`BushExit`, `Strike`), so the pawn is **considered in the same frame**; with the §1 client-throttle row, every client is updated that frame and gets its channel opened then (E15: without that row, skipped clients would wait for the pawn's next regular update, with per-client differences that would break F4's `T_uncull` assumption). Clients show the body only at the outward leaf-plane instant (`T_exit_occluded ≥ T_uncull`, IVP F4; Concealment owns the clip timing).

**5.7 Kills.** Nothing re-opens in the strike's outcome slot (ASN-R21). A victim culled for a reason other than concealment re-opens at Assassination's **death write** (ADR-0002 staging pass), already falling, carrying `DeathServerTime` so clients fast-forward, identically for every victim kind. A victim **killed while concealed** returns to `Visible` at the death write too (Concealment pushes `bConcealed = false` in the staging pass, same server step), carrying `FDeathState.bDiedConcealed = true` so every non-owner client keeps the body presentation-hidden (IVP R7.5, ASN-R10, CON-R9; `bDiedConcealed` approved by the user 2026-10-25, which retires the earlier interim rule of keeping it culled through fall and removal).

**5.8 Re-open behaviour.** A re-open creates a new client actor (E11). Client-side registries (animation significance, audio, decals, pawn lists, Markers) must tolerate a relevancy destroy and re-create; the new actor initialises its presentation from the replicated state (`FActionState`, `FDeathState`, `HeadLook`, `AppearanceId`) through the same idempotent `RefreshPresentation(EstimatedServerNow)` path, never from arrival; nothing gameplay-visible distinguishes a cull from a real destroy. NetGUID reuse and AnimBP initialisation are S10's.

**5.9 Parity and switches.** The cull applies identically to every pawn kind; a concealed NPC is culled for every connection (IVP R7.1). `bConcealServerCull = false` (client-side hide only) exists only for internal development builds (L29 CRITICAL) and never ships by default; there is no automatic fallback if Crowd misses F5 (IVP R7.6).

**5.10 Iris.** Not specified here. If ADR-0024 ever adopts Iris, the cull becomes per-connection exclusion group filter status with the F3 delay applied at the filter flip, and S1, S2, S7, S9, S10 and F1/F2 are rerun first.

### 6. Owner and Team data on the wire

- Owner and Team data travel only in ADR-0001's carriers on the receiving player's own `AStealthPlayerController` (owner-only relevancy). No team-filtered actor, no custom team relevancy.
- `FActionState` (All) is on the pawn; hold status, cooldowns and replies are owner-only on the controller (IIF R12).
- The Layout contract spawns **zero** replicated actors; any future per-court or per-bush replicated object must exist on every court or bush with identical class and net settings, spawned in SocketId order (LSC R2.6).
- No camera, boom or input component exists on the Shared Pawn class; the camera rig is created only on the locally controlled client and never replicated (Camera CS-R1, IVP L17).
- `FRoundPublicState` on the GameState is the only All-audience round struct; one RepNotify, diffed on the client; the host calls the same handler (Round R20).

### 7. Team fallback position feed

- A Visibility Policy-owned C-TeamView part, `UTeamPositionFeedComponent` (`UTeamViewComponentBase`, ADR-0001 §5), on each player's controller carries entries `{PublicHandle, Pos}` with `Pos` the teammate pawn's **feet** (skeletal mesh world location) quantised to a 10 cm grid as `int16 × 3` (IVP R8, F6).
- **Trigger, per connection:** during `Active`, a teammate's pawn exists on the server and that connection has **no open actor channel** for it, or has one whose open is **not yet acknowledged** (`FindActorChannelRef == null`, or the channel's open not acked [U `UChannel::OpenAcked`]: travel re-push, a takeover in flight, initial channel latency, any future cull). The entry is added in the **same server frame** in which the channel goes missing (or the `PawnRef` is re-pushed unresolved) and is removed only **one feed update after the channel's open is acknowledged**, because the client's `PawnRef` resolves only after the open bunch arrives; so a client never sees "null `PawnRef`, no feed" for an existing pawn for longer than one update (TM-R4a). With the §1 client-throttle row, "same server frame" holds for every client.
- Rate `TeamPositionFeedHz` (safe 1.5-4); bandwidth `B_feed = N_u · 13 B · h` ≤ 104 B/s (F6). Only teammate connections ever receive it.
- **Per-entry sequence** (Markers CR-11c, OQ14; approved by the user 2026-10-25): each entry carries `uint8 Seq`, advanced (wrapping) by the builder on every feed tick, so a stationary ally's entry still changes under change-only replication and Markers' staleness rule F5 is active.

### 8. The replicated-property allowlist and audit

- **One checked-in allowlist data file** (JSON) with sections for the Shared Pawn (and its components and sub-objects), `AStealthPlayerState`, `AStealthPlayerController` and each `UTeamViewComponentBase` part, the GameState, and every other replicated actor class a client receives. Each row: property, owning class, condition, audience, carrier, IVP fact row. **The Visibility Policy owns the file, the audience column and the parity tests; Shared Character authors the pawn rows; a row changes only with both owners' sign-off** (IVP R11). ADR-0008 fixes its path (`Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json`, row schema `{class, property, condition, audience, carrier, owners[]}`) and its staging for cooked Development runs.
- **Audit (build gate):** enumerates `GetLifetimeReplicatedProps` through each class's rep layout on the class default object (including Blueprint replication lists) and on live instances spawned in a test world (components, sub-objects, registered sub-object lists); fails on any property missing from the allowlist or with a different condition. It also audits net config (§1) on class defaults **and** on live instances of an NPC pawn, a remote-client pawn, a host pawn and a just-taken-over pawn (AC-IVP-45), and the absence of multicast RPCs on the pawn.
- Minimum pawn rows: the `AActor` and `ACharacter` sets of IVP R5 (constant values), replicated movement, `HeadLook` yaw, `RemoteViewPitch16`, `FActionState`, `TraversalState`, `AppearanceId`, `FDeathState`, and `PlayerState`/`Controller`/`Owner` as `COND_OwnerOnly` (ADR-0001). Forbidden on the pawn: replicated attachments, an `AbilitySystemComponent` or any other replicated gameplay-state component **beyond the one shared `ActionPose`/`FActionState` component** (identical class, creation order and flags on every pawn; IVP R11), and any concealment, team, strike, cooldown or binding field.
- **Render-flag split (ADR-0022 §2.1, ADR-0017 §6):** this audit asserts the pinned hidden-primitive flag *values* (`bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden`, `bRayTracingFarField` = false) on pawn primitive class defaults; cross-pawn equality of the whole render-flag set (CDO vs spawned NPC vs spawned player pawn) is owned by `StealthGame.Pawn.RenderFlagParity` (ADR-0022). Neither test duplicates the other.
- Raw-bunch parity (F1, S1) and the classifier gate (F2) run in the ADR-0008 harness under `LeakNetProfile`.

### Architecture Diagram

```
 server frame ─────────────────────────────────────────────────────────────────────────────
 TG_PostPhysics  ADR-0002 drain ─► Concealment resolver ─► IConcealmentCullSource::SetCullState
                                    (commit t0: record Visible→CullPending, deadline on grid)
                                    (HideExit / Broken: record →Visible, same slot)
                 ADR-0002 staging ─► death write (any cull ⇒ →Visible; killed-concealed carries bDiedConcealed)
 TG_PostUpdateWork  FConcealmentCullTickFunction: deadline ≤ now ⇒ Culled; for each denied
                    connection: FindActorChannelRef(Pawn)->Close(Relevancy)
                    UTeamPositionFeedComponent: entries for teammates with no channel on that connection
 TickFlush (legacy ServerReplicateActors)
    for each pawn considered (15 Hz cadence, or scoped forced update):
       AStealthPawn::IsNetRelevantFor(conn) = State≠Culled ∨ owner(conn) ∨ ally(conn)   [Super never called]
       PreReplication: SetRemoteViewPitch(HeadLook pitch)   (every pawn)
       fixed-width FActionState / FDeathState / TraversalState
 client ───────────────────────────────────────────────────────────────────────────────────
    hide body at the BushEnter swap (local latch, CON-R6b) ─► actor destroyed at the cull (non-allies)
    re-open ⇒ new actor ⇒ RefreshPresentation(EstimatedServerNow) from replicated state
```

### Key Interfaces

```cpp
// --- StealthFoundation Public/Server: Visibility Policy cull --------------------------------
class IConcealmentCullSource            // push API implemented BY the Policy, called by Concealment
{ public: virtual void SetCullState(APawn* Pawn, bool bConcealed, double CommitServerTime) = 0;
          /* MarkKilledConcealed retired 2026-10-25: killed-concealed re-opens at the death write (§5.7) */ };
enum class ECullState : uint8 { Visible, CullPending, Culled };
UCLASS() class UVisibilityPolicy : public UWorldSubsystem /* , public IConcealmentCullSource */
{
public:
    bool ShouldCullForConnection(const APawn* Pawn, const UNetConnection* Connection) const;
    bool IsRelevantForViewer(const APawn* Pawn, const AActor* RealViewer) const;  // §5.4
    // TR-ivp-001 (amendment 2026-10-25b): the one server-side visibility query. Default deny.
    bool CanViewerKnow(FViewerId Viewer, FSubjectRef Subject, EFact Fact) const;
    EViewerClass GetViewerClass(FViewerId Viewer, FSubjectRef Subject) const;
    // owns FConcealmentCullTickFunction (TG_PostUpdateWork) and the PC→FViewerId cache
};

// --- StealthGameplay Core: Shared Pawn hooks ------------------------------------------------
class AStealthPawn : public ACharacter
{
    virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
                                  const FVector& SrcLocation) const override;  // Policy only; no Super
    virtual void ForceNetUpdate() override;                // forwards only inside a scoped reason
    virtual void PreReplication(IRepChangedPropertyTracker& Tracker) override; // Super, then pitch
};
enum class EForcedUpdateReason : uint8 { ActionStateWrite, TraversalStateWrite, DeathWrite };
struct FStealthForcedNetUpdate { explicit FStealthForcedNetUpdate(AStealthPawn&, EForcedUpdateReason); };

// --- StealthFoundation Public/View: wire structs ---------------------------------------------
// FActionState, FFixedWarpOffset, FDeathState: declared in ADR-0018 Key Interfaces (single declaration;
// EActionPose Pose, WithNetSerializer + WithIdentical, 144 / 64 bits). This ADR owns only the rules of §4:
// fixed bit width for every value, no optional/conditional field (StartServerTime included, 2026-10-25).

// --- C-TeamView part (ADR-0001 family) ------------------------------------------------------
USTRUCT() struct FTeamPositionFeedEntry { GENERATED_BODY()
    UPROPERTY() FPublicHandle Handle; UPROPERTY() int16 X = 0; UPROPERTY() int16 Y = 0;
    UPROPERTY() int16 Z = 0;  /* 10 cm grid, feet */
    UPROPERTY() uint8 Seq = 0; /* advanced every feed tick, wraps (user decision 2026-10-25) */ };
UCLASS() class UTeamPositionFeedComponent : public UTeamViewComponentBase
{ GENERATED_BODY() UPROPERTY(Replicated) TArray<FTeamPositionFeedEntry> Entries; };  // <= 4
```

### Implementation Guidelines

- Pawn net config must be set only through the §1 values from tuning, with the 5.5+ setters, identically for every pawn; it must never be changed per instance, per kind or on possession.
- Pawns must never use dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy.
- `AStealthPawn::IsNetRelevantFor` must never call `Super`; its only inputs are the spawn gate, the Policy's cull record (including its pending-close entries, §5.6a, amendment "S10 resolution 2026-10-25") and the viewer's relation.
- Nothing may call `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn; `ForceNetUpdate()` on a pawn must forward only inside `FStealthForcedNetUpdate` with an allowed reason, capped at one per pawn per frame.
- `PreReplication` must write `RemoteViewPitch16` from `HeadLook` for every pawn after `Super`.
- Every simulated proxy must use exponential smoothing with the same tuning values, including the listen-server simulated smoothing times.
- Outcome-bearing replicated structs must serialise to a fixed bit width with no value-dependent size.
- The concealment cull must land on the global boundary grid, close the channel explicitly per denied connection in the cull tick, and never wait for a client acknowledgement; un-cull must return the record to `Visible` in the same server step as the state change, and a connection's channel re-opens per §5.6a/§5.6b (amendment "S10 resolution 2026-10-25").
- Nothing may re-open a culled pawn in a strike's outcome slot; a killed-concealed victim must re-open only at the death write, in the same step as `FDeathState {DeathServerTime, bDiedConcealed = true}`.
- Client presentation must tolerate a relevancy destroy and initialise a re-created actor from replicated state only.
- A new replicated property, condition or replicated class must not ship without an allowlist row signed off by its owners; the audit must fail the build otherwise.
- The fallback feed must be added in the same frame a teammate's channel goes missing and only to teammate connections.
- `FActionState.StartServerTime` must be written in every `FActionState` write for every pawn kind; the feed's `Seq` must advance on every feed tick for every entry.

## Alternatives Considered

### Alternative 1: Relevancy override alone (no explicit close)
- **Description**: Return false from `IsNetRelevantFor` and let the driver close the channel.
- **Pros**: Least code.
- **Cons**: Open channels are re-tested only every `min(RelevantTimeout, 1 s)` and closed after `RelevantTimeout` (E2): the cull lands 1-6 s late, with jitter that depends on when the last check ran — a timing channel and a long x-ray window.
- **Rejection Reason**: Fails IVP R7.2 (quantised, fixed delay).

### Alternative 2: Client-side hide only (replicate a concealed flag or keep the pawn)
- **Description**: Every client holds every pawn; clients hide concealed bodies locally.
- **Pros**: No network event at concealment; simplest.
- **Cons**: A modified client sees every bush occupant (L29 CRITICAL); a replicated flag is a permanent detector if NPCs lack it.
- **Rejection Reason**: Kept only as an internal development switch (§5.9).

### Alternative 3: Dormancy or distance culling for concealment
- **Description**: Put concealed pawns dormant, or use a cull distance.
- **Pros**: Engine-native.
- **Cons**: Dormancy is banned for pawns (kind-visible transitions); dormancy does not remove the actor from clients; distance does not express "ally vs non-ally".
- **Rejection Reason**: Does not hide position from non-allies and violates SCL R13.

### Alternative 4: Iris exclusion group filters now
- **Description**: Per-connection filter status destroys the object remotely without timeout lag.
- **Pros**: Cleaner per-connection control.
- **Cons**: ADR-0001 chose legacy; Iris is off by default and Beta-flagged; pointer trimming and serialisers unverified under Iris.
- **Rejection Reason**: Deferred to ADR-0024 (TD concern 6: one cull path only).

### Alternative 5: ReplicationGraph with a concealment node
- **Description**: Route culled pawns through a per-connection graph node.
- **Pros**: Per-connection actor lists with no timeout lag.
- **Cons**: Replaces `IsNetRelevantFor` and the whole legacy consider path; adds a system for a 50-pawn always-relevant scene; ADR-0001 rejected it.
- **Rejection Reason**: Disproportionate and conflicts with ADR-0001.

### Alternative 6: Defer possession `ForceNetUpdate` with a flag and replay it at the next boundary
- **Description**: Remember the unscoped call (possession, view-target, saturation retry, pause loop) and issue it at the next cadence boundary.
- **Pros**: Literal reading of "deferred".
- **Cons**: Extra per-pawn state; the fixed 15 Hz cadence already sends the change at that boundary.
- **Rejection Reason**: Dropping the unscoped call is observably identical and simpler.

## Consequences

### Positive
- Packet-level parity is a set of audited, data-driven settings plus three small overrides on one class.
- Concealment hides position from non-allies with a fixed, quantised, kind-blind delay and no x-ray window.
- Forced updates can never encode kind or re-open a culled pawn.
- Fixed-width structs remove size side channels by construction.
- One allowlist gives default-deny replication with a build gate.

### Negative
- The relevancy override runs per pawn per considered connection (~50 × 10 at 15 Hz); it must stay allocation-free and O(1).
- The explicit close relies on S9 (E10) for "stays closed".
- Every client must handle pawn destroy/re-create robustly (significance, audio, markers) — real work for presentation code.
- ~~Two schema slots remain open~~ — closed 2026-10-25 (user decision): `StartServerTime` and the feed `Seq` are specified; the feed costs ≤ 104 B/s while entries exist.
- L14 (every client holds every visible pawn's position) remains accepted until Anti-Cheat (Alpha).

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| A closed channel is re-created while culled (E10 verified; in-flight cull/un-cull race open) | Low | High (x-ray window) | S9/S10 under `LeakNetProfile`; ban `ForceNetRelevant`/`ForceActorRelevantNextUpdate`; harness assertion that no denied connection holds a channel for a `Culled` pawn |
| Listen-host client throttle left at default (E15): un-cull and feed timing vary per client | High if the config row is not applied | High (F4 `T_uncull` and TM-R4a broken; per-client timing) | §1 client-throttle row (pending user config edit); harness startup assertion; S9 |
| Re-open behaviour leaks kind (NetGUID reuse pattern, AnimBP init pop) (E11) | Medium | High | S10; NetGUID allocation gaps recorded (L32); one re-open path for every kind |
| Relevancy override cost exceeds the 1.5 ms replication line | Low | Medium | O(1) record lookup and per-connection viewer cache; profile at 10 connections |
| Measured L99 exceeds the F3 assumption (S12) | Medium | Medium | Recompute `ConcealCullDelayMs` and `T_uncull` together (IVP F4 cross-GDD invariant) |
| CMC cadence or correction differs by controller kind (E13) | Medium | High | S13; fixed frequency; same CMC config; F2 classifier gate |
| `NetSerialize` struct still sends variable sizes on legacy (E12) | Low | High | Byte-capture spike; fixed-bit writer with no conditionals |
| A feature adds a replicated field without a row | Medium | High | Audit build gate; canary per commit |

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| information-visibility-policy.md | R5 UE default overrides (relevancy Super clauses, possession `ForceNetUpdate`, adaptive frequency, net config, `RemoteViewPitch16`, smoothing timestamp, `ACharacter` fields) | §1, §2, §3, §5.4, §8 |
| information-visibility-policy.md | R6 relevancy varies by concealment and relation only; R7 hybrid cull (enforcement, timing, un-cull, kills, engine path) | §5 |
| information-visibility-policy.md | R8 team fallback feed incl. TM-R4a invariant | §7 |
| information-visibility-policy.md | R11 replicated-property audit, net config audit | §8 |
| shared-character-locomotion.md | R13 network parity configuration (values in config; bans; forced updates; allowlist); R14 smoothing parity | §1, §2, §3, §8 |
| participant-state-roster.md | R6 pawn replication parity | §1, §8 |
| concealment-bushes.md | CON-R6a one clock from replicated server start time (`StartServerTime`, approved 2026-10-25); CON-R10 per-viewer result, no same-bush clause | §4 slot, §5.4 |
| assassination-defence-reversal.md | ASN-R21 fixed-width `FActionState`, uniform forced updates, nothing re-opens in the outcome slot, re-open at the death write | §2, §4, §5.7 |
| level-socket-contract.md | R2.6 zero replicated actors from the contract | §6 |
| round-match-framework.md | R20 `FRoundPublicState`, forced GameState updates, host calls the handler | §2, §6 |
| camera-system.md | CS-R1 no camera components on the pawn, local only | §6 |
| information-visibility-policy.md | R1-R3 `CanViewerKnow`, `GetViewerClass`, seven viewer classes, fact matrix, default deny on every carrier (TR-ivp-001) | Amendment 2026-10-25b (query API); §8 audit |
| level-socket-contract.md | R4.6 spawn gate: no pawn relevant to a client before `Connected` (TR-lsc-007, relevancy half; spawn half in ADR-0014) | Amendment 2026-10-25b (per-connection gate in `IsNetRelevantFor`, before §5.4) |

## Performance Implications
- **CPU**: Relevancy override O(1) per considered pawn-connection pair (~7,500 calls/s at 15 Hz × 50 × 10); cull tick touches only records at their deadline; within the 1.5 ms replication line (to be measured on target-class hardware, AQ-18).
- **Memory**: One cull record per pawn; one viewer-cache entry per connection; allowlist loaded only by tests.
- **Load Time**: None.
- **Network**: Fixed 15 Hz pawns within the 36 KB/s pawn line (31.1 KB/s worst case at NpcCount 50, SCL F9); culled pawns cost nothing to non-allies; fallback feed ≤ 104 B/s per client; fixed-width structs cost a few bytes more than packed ones for a kill/whiff (accepted).

## Migration Plan
The prototype replicates `bConcealed`/`bRevealedUntilExit` to every client and hides bushes client-side (`AStealthCharacter::UpdateConcealmentVisibility`), uses separate `bDying`/`bPreparingDefence` bools, default net config and no relevancy override (first-playable report, known issue 2). None of it is promoted; the cull, the overrides and the serialisers are written new under this ADR once Accepted, behind the allowlist audit from the first commit.

## Validation Criteria
- AC-IVP-45 runtime differential audit: §1 values equal on the four pawn variants; adaptive frequency off; no dormancy.
- F1 structural leak count `S = 0` over ≥ 3 clients and ≥ 100 pawn lifetimes including cull/re-open, death, hand-over and rejoin; F2 classifier gate passes (`AUC_hat ≤ 0.55`, upper bound ≤ 0.58).
- S9: at `LeakNetProfile`, every non-ally connection loses the pawn at the grid boundary ≥ `t0 + ConcealCullDelayMs` (± one server frame), all same-frame culls at one boundary, and no channel re-appears while `Culled`.
- S10/S12/S13 report within the stated budgets; F4 `T_exit_occluded ≥ T_uncull` holds.
- Byte capture: `FActionState` bunches identical in size for kill, whiff and reversal; `FDeathState` likewise.
- Forced-update log: only the three scoped reasons appear for pawns, ≤ 1 per pawn per frame, equal distributions for NPC and player pawns (F7 timing parity).
- Fallback feed: never "null `PawnRef` and no feed" for more than one update for an existing teammate pawn; never present on an opponent's controller.
- Allowlist audit fails the build when a test-only replicated property is added without a row.

## Related
- ADR-0001 (identity rules and carriers; chose legacy), ADR-0002 (queue, staging, server clock), ADR-0007, ADR-0008 (dependencies).
- ADR-0010 (Shared Pawn), ADR-0017 (Concealment component and presentation), ADR-0018 (encounter data), ADR-0021 (Markers), ADR-0024 (Iris migration and the Iris cull path).
- Design: `design/gdd/information-visibility-policy.md` (R5-R8, R11, F3, F4, F6, L13, L27, L29), `design/gdd/shared-character-locomotion.md` (R13, R14), `design/gdd/concealment-bushes.md` (CON-R6a, CON-R10), `design/gdd/assassination-defence-reversal.md` (ASN-R21), `design/gdd/level-socket-contract.md` (R2.6), `design/gdd/round-match-framework.md` (R20), `design/gdd/camera-system.md` (CS-R1), `design/gdd/teammate-markers.md` (TM-R4a, OQ14).
- Architecture: `docs/architecture/architecture.md` (AQ-05, AQ-06, AQ-11).
- **GDD change requests raised by this ADR (not applied here):** IVP R7.2 wording (global pawn-update grid replaces the per-pawn boundary); IVP R5 wording (unscoped possession `ForceNetUpdate` dropped, the next cadence update carries the change); IVP R8 / Teammate Markers TM-R4a (feed entry kept until one update after the channel open is acknowledged).

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §5.4: "swap notify" read as the server-time swap (no notify gates the hide) — requested by ADR-0017.
- §8: cross-reference to ADR-0022's `RenderFlagParity` audit (values here, cross-pawn equality there) — requested by ADR-0022 (sync item 8).

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §4, Key Interfaces: the duplicate `FActionState` declaration is removed; ADR-0018's Key Interfaces block is the single declaration (`EActionPose`, `WithNetSerializer` + `WithIdentical`); this ADR keeps the fixed-width rule — architecture-review fix 2026-10-25 (Conflict 2).
- §1 server-tick row, E17 (new, [V]), Verification Required: the listen-host server tick is stated on the host frame time (`NetServerMaxTickRate` has no effect without `bClampListenServerTickRate`); `U_eff ≤ 1/PawnNetUpdateHz + T_frame`; no integer-multiple requirement or `NetServerMaxTickRate` startup check; harness frame-time capture instead. The clamp-flag alternative is a pending user item (Config edit) — architecture-review fix 2026-10-25 (Engine finding 2; logged as `design`).
- §1 client-throttle row: `MaxFps` defined (host frame-rate limit `t.MaxFPS` when set, else the 240 fps design ceiling) — architecture-review fix 2026-10-25 (Engine finding 2).
- §2 (Conflict 3): no change; ADR-0001 §4.2 now uses this ADR's "dropped; the next cadence update carries the change" — architecture-review fix 2026-10-25.
- Config applied 2026-10-25 (phase 2, user-authorised): `[/Script/Engine.Engine] NetClientTicksPerSecond=2400` (= 10 connections x 240 fps design ceiling) in `Config/DefaultEngine.ini`. `bClampListenServerTickRate` deliberately **not** set (option (b), host frame-time basis). The E15 startup assertion remains a harness item.

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §4, Key Interfaces, guidelines — user decision 2026-10-25: `FActionState.StartServerTime` (fixed 64-bit) and `FDeathState.bDiedConcealed` (fixed 1-bit) are adopted (Concealment CR 10 (a)-(b)); the "slot pending user" text is replaced.
- §5.2, §5.7, Key Interfaces — user decision 2026-10-25: a killed-concealed victim re-opens at the death write with `bDiedConcealed = true`; the interim `KilledConcealed` cull reason and `MarkKilledConcealed` are retired.
- §7, Key Interfaces — user decision 2026-10-25: the fallback feed entry carries a wrapping `uint8 Seq` advanced every feed tick (Markers CR-11c / OQ14); `B_feed = N_u · 13 B · h` ≤ 104 B/s (IVP F6).
- Key renames — user decision 2026-10-25 (TDC CR-10): `ConcealServerCullEnabled` is `bConcealServerCull`.

User scope decisions (2026-10-25: "accept all recommendations"); decision unchanged, Status unchanged (Accepted).
- Budget note — user decision 2026-10-25: two more owner-only C-TeamView payloads ride the PlayerController (no relevancy, cull or pawn change): the Informant Pings part (≤ 50 B/s per teammate connection at defaults, bursts ≤ 200 B; PNG F4) and the teammate `bReconnecting` bit (change-only). `FRoundResult` grows by 6 bytes, sent once at `Ended`. Opponent connections receive none of the team data. Inside the 50 KB/s per-client budget; no pawn-line change.

Architecture review 2026-10-25b (`docs/architecture/architecture-review-2026-10-25b.md`); text corrections only, decision unchanged, Status unchanged (Accepted).
- Diagram: the retired `KilledConcealed` interim replaced by the §5.7 rule (killed-concealed victims re-open at the death write with `FDeathState.bDiedConcealed`).

Technical Setup 2026-10-25b (architecture-review closure, `docs/architecture/architecture-review-2026-10-25b.md`); decision unchanged, Status unchanged (Accepted).
- §5, §8, Key Interfaces (TR-ivp-001; information-visibility-policy.md R1, R2, R3, AC-IVP-05): `UVisibilityPolicy` owns the visibility query API: `bool CanViewerKnow(FViewerId Viewer, FSubjectRef Subject, EFact Fact) const` and `EViewerClass GetViewerClass(FViewerId Viewer, FSubjectRef Subject) const`, both server-only (`Public/Server/`, declared in the Key Interfaces above; the architecture.md API Boundaries copy is a mirror, this ADR is the owner). `Viewer` MUST be derived from the server connection through the PC → `FViewerId` cache of §5.4, never from client input (a client-sent team, participant id or viewer class is ignored). `Subject` is a pawn, a participant, a court fixture (fact `SiteKnownToTeam`) or "the round". `Fact` is one row of the IVP R3 matrix.
- §5, Key Interfaces (TR-ivp-001; IVP R2): `EViewerClass` has exactly seven values, `SelfActive`, `SelfInformant`, `Teammate`, `TeammateInformant`, `Opponent`, `OpponentInformant`, `PostRound`. `GetViewerClass` MUST compute it in IVP R2's order from Roster (`GetTeam`, `GetState` of the viewer's participant, `GetParticipant` of the subject) and Round's phase: (1) phase `Ended` → `PostRound` for every viewer, set in Round's round-end step before any `Ended` data is sent; (2) subject is an unbound pawn (an NPC) → the NPC row, which is exactly the `All` facts and computes no class, so no code path makes an NPC answer depend on the viewer; (3) same participant → `SelfActive` / `SelfInformant`; (4) same team → `Teammate` / `TeammateInformant`; (5) otherwise `Opponent` / `OpponentInformant`. Invariant (art bible 5.2): for any fact about another subject, `TeammateInformant` ≡ `Teammate` and `OpponentInformant` ≡ `Opponent`.
- §8 (TR-ivp-001; IVP R3): the fact matrix in `design/gdd/information-visibility-policy.md` section "Core Rules", rule R3 (columns SA, SI, TM, OP, NPC row, PR; cells Y, N, E, —) is **normative**: the Policy's compiled matrix MUST equal that table row for row, and a new fact or audience is a change request to IVP, never a local decision. `E` (`PostRoundEligible`) answers `true` only in the `PostRound` class. `N` and `—` cells return `false` with no fault report and no log line.
- §8 (TR-ivp-001; IVP R1, R11, AC-IVP-05): default deny. `CanViewerKnow` MUST return `false`, and report to the injected fault sink (an `ensure` in development builds, a counted `LogStealthVisibility` error; tests inject a counting stub), for: an unlisted fact, an unknown viewer, a call on a client, and a call outside the game thread. Default deny covers every carrier, not only properties: (a) replicated properties, enforced by the §8 allowlist build gate (a property missing from the allowlist, or with a different condition, fails the build); (b) RPCs, where every client-facing RPC and its payload fields MUST map to an IVP R3 row and the audit enumerates RPCs with the same reflection pass (a pawn has no multicast RPC); (c) logs, where the Event Log releases nothing to a client before `Ended` and then only `E` rows through the Post-Round Reveal (no other egress). A fact/viewer pair with no matrix row is denied on all three carriers. The query is not called per frame by gameplay; the answers are compiled into the carriers, which are the enforcement.
- §1, config table (LOW; architecture-review 2026-10-25b Engine finding 6; `Config/DefaultEngine.ini:106`): `p.EnableCharacterAccelerationReplication=1` is inherited from the sample and is listed in the §1 table. It STAYS ON (verified 5.8.4 `Character.cpp:1734-1752`, `Character.h:553`): `GatherCurrentMovement` copies `CMC->GetCurrentAcceleration()` into `FRepMovement.Acceleration` and sets `bRepAcceleration`, which proxy animation needs (ADR-0013 reads acceleration; motion matching trajectory). It is Pillar-1 safe because it applies to every Character identically: `ShouldReplicateAcceleration` is not overridden, and the value comes from the one CMC input path (SCL R5.1/R5.7, ADR-0010 §6) for human and AI alike. The field is behaviour data, so it belongs to the ADR-0008 leak-suite inventory (boot test asserts the CVar = 1; the F1 raw-bunch parity and the IVP F2 classifier, whose features already include acceleration variance, cover it). ADR-0008 owns that inventory; adding the row there is its follow-up.
- §5.4, §1 (TR-lsc-007; level-socket-contract.md R4.6, round-match-framework.md P7 and AC 41, relevancy half; spawn half in ADR-0014 §7.1): `AStealthPawn::IsNetRelevantFor` MUST check a per-connection **spawn gate** first, before every §5.4 clause. The gate is a bool in the Policy's PC → `FViewerId` viewer cache, closed when the entry is created (login, rejoin, controller swap, new round epoch) and opened only when Session moves that participant's connection to `Connected` for the current round (geometry built and verified, Session's round acknowledgement complete; Session `GetConnectionState` transition, delivered through the Roster's connection event). While it is closed, `IsNetRelevantFor` returns `false` for EVERY pawn (NPCs, other players' pawns and the connection's own bound pawn), so no channel is opened and the client never receives a pawn before its geometry exists (a collision-less client's corrections would spiral). An unknown viewer is denied (default deny, as §5.4). A participant that is not `Connected` at round start still gets its pawn spawned AI-driven and unpossessed (ADR-0010 §3.1, Round R17.7), and the pawn becomes relevant to that connection only when the gate opens.
- §5.4 (TR-lsc-007; reconciliation): §5.4's "`true` for every connection" for `Visible` and `CullPending` pawns reads "for every connection whose spawn gate is open", and §1's "every pawn is relevant to every connection except through the concealment cull" gains the gate as the second, kind-independent exception (it applies to every pawn identically and keys on the connection, never on the pawn). The gate is a latch: `Unresponsive`, `Traveling` within the same epoch or a lost link do not close it (a gate flip would destroy and re-create client actors, §5.8); it is cleared only when the connection closes, at the start of a round travel (new Session acknowledgement epoch) and at `Reset`. A rejoin is a new connection with a closed gate until Session reports `Connected`. Opening the gate calls no `ForceNetUpdate` (forbidden outside §2's scoped reasons): each pawn is next considered at its regular update boundary (≤ `1/PawnNetUpdateHz + T_frame`, §1 server-tick row), which is harmless because Round P7 waits for every `Connected` client's `ServerWorldReady` (`WorldReadyTimeoutSec`) before `ActiveAtServerTime`. The gate and §5.5's cull compose by AND (a culled pawn stays denied after the gate opens). Test: LSC AC-33 `Cross.Round.SpawnGate` (client B's load delayed 5 s: no pawn spawned for, possessed by or net-relevant to B before `Connected`) and Round AC 41 `StealthGame.Round.SpawnOneFrameAndGate`.
- §6 (TR-round-009 cross-reference, orchestrator): field-level `FRoundPublicState` rules (`Phase` 0 = `Unknown`, `PhaseChangeCount`, times zeroed in `Active`, re-seed of the recreated GameState in `InitGameState`, bounded late-loader reveal) are owned by ADR-0002 §8.10; this ADR keeps the carrier and its unconditional, identical-bytes replication.

Spike S9 reported 2026-10-25 (`production/qa/spikes/2026-10-25_S9.md`; story ivp-004); decision unchanged, Status unchanged (Accepted). S9 is **PASS** under StealthLeak1 and StealthLeak2 with 3 client processes.
- §5.3: the grid deadline and same-frame landing are confirmed at run time: 4 subjects committed in one frame closed in one server frame, ≤ 2.5 ms after the deadline (inside one frame); clients lost the pawn one-way lag + one client frame later.
- §5.5 / E10 [V runtime]: with the override returning false the explicitly closed channel stayed closed for 62 s (no relevancy-timeout or forced-relevant re-creation, 0 `ForceRelevantFrame` changes), with denied viewers parked next to, on and under culled pawns. E2 [V runtime]: the override alone left the channel open 4.6-5.2 s (negative control) — the explicit close is required.
- L27: stock characters are not walkable bases for characters (capsule `CanCharacterStepUpOn = ECB_No`; also slid off with `ECB_Yes`), so the spike forced the base relation both ways; the replacing override never consults `IsBasedOnActor` and no channel re-opened. Whether the production pawn can ever be a base is ADR-0010's collision setup.
- E15: `NetClientTicksPerSecond` 2400 ≥ 10 × 240 holds with the design's 10 connections; it fails against the engine's default `AGameSession::MaxPlayers` (16). The production assertion must use the design constant or pin `MaxPlayers=10` (Config) — decided by the implementing story (023/035).

## Amendments (S10 resolution 2026-10-25)

Spike S10 reported **FAIL** on 2026-10-25 (`production/qa/spikes/2026-10-25_S10.md`; story `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`; evidence `production/qa/evidence/s10-reopen-evidence.md`). This amendment decides the fix. **Status unchanged (Accepted, conditional)**; the ADR's S10 gate is replaced by the **S10 re-run** gate (`production/epics/information-visibility-policy/story-056-s10-fix-implementation-and-rerun.md`), which must PASS S10-R7 before any story that depends on §5.6-§5.8 is implemented. Decided under the user's unattended authorisation (2026-10-25); decisions logged under `architecture-decision`.

**Why an amendment and not a superseding ADR.** The status text says a failed spike is handled by a superseding ADR. That rule exists so an accepted decision is never silently rewritten; it applies when the decision itself fails. Here it does not: the hybrid cull (legacy driver, `IsNetRelevantFor` override, explicit per-connection close on the global grid, the record returning to `Visible` in the same server step, a re-open being a new client actor initialised from replicated state through one path for every pawn kind) is unchanged, and S10 confirmed its load-bearing facts (new actor every re-open, NetGUID reused with no allocation, root exact, `T_reopen` within F4). The three failures sit inside mechanisms this ADR had marked unverified and handed to S10 (E10's in-flight race, E11's AnimBP initialisation) and in a consumer of ADR-0002 §9 that the spike's stub bypassed (it read `GetServerWorldTimeSeconds`, already forbidden for presentation). Each fix composes with the decision without reversing any part of it, so it is recorded as this dated, gated amendment with its own review record, and the control manifest is regenerated. The Status line records the exception; a failure that reverses any §5 decision still requires a superseding ADR.

### S10-R1. Engine facts updated (E10, E11) and added (E18)

| # | Fact | Tag |
|---|---|---|
| E10 (rev.) | The in-flight race is **unsafe**: when a cull is followed by an un-cull before the client has processed the close, the new channel's open can be processed first; `UPackageMapClient::SerializeNewActor` re-binds the actor still mapped to the NetGUID (it spawns only if nothing is found) and the late close then destroys it, leaving the new channel without an actor (~1 % of race re-opens under StealthLeak1, ~2 % under StealthLeak2). On the server a close is complete only at its **in-sequence acknowledgement**: `UChannel::ReceivedAcks` releases out-records only while the head record is acked (a partial open must be fully acked first) and reports cleanup only once the close record is released; `UChannel::CleanUp` then removes the channel from `OpenChannels` and clears `Connection->Channels[ChIndex]`; this runs in `UNetConnection::ReceivedPacket` during TickDispatch, so a `TG_PostUpdateWork` read sees it the same frame. The acknowledgement of the close bunch's packet alone is not enough: the client queues an out-of-order reliable bunch in `InRec`, so a received close can still wait behind an earlier reliable bunch. In-sequence acknowledgement and leaving `OpenChannels` are the **same event**, so S10 Q5g's hold tail (p90 1.09 s, max 8.0 s) is not explained by the signal it used; the re-run decomposes it (S10-R7). An open bunch is never queued in `InRec`, so a close cannot overtake its own channel's open (no reverse race on one channel). A client channel still holding queued bunches frees its slot and acknowledges, but destroys its actor later through `KeepProcessingActorChannelBunchesMap`; a new open for the same NetGUID queues behind it, so this path delays a re-open but cannot lose the pawn (the re-run's decomposition counts it). `Closing` is set only when the close bunch is actually sent; `UChannel::Close` sends nothing for an already-closing channel or a connection that is not open. Pooled channels reset `Closing` when added to the pool (pooling on by default) | [V runtime, S10]; [V] `DataChannel.cpp:453-538, 403-411, 640-672, 1607-1610, 1700, 2644-2680, 3228-3234`, `NetConnection.cpp:2792-2823, 3546-3553`, `PackageMapClient.cpp:510-512, 539, 748`, `NetDriver.cpp:536-539` |
| E11 (rev.) | Every re-open creates a new client actor (3,572 of 3,572) under the **same NetGUID** (re-opens allocate none; L32 clean). `PostNetInit` runs only for a newly spawned actor, not for a re-bound live one. The prototype's first rendered frame of a re-created pawn showed an un-evaluated (collapsed) pose. `AlwaysTickPose` (ADR-0013's client setting) ticks but refreshes bone transforms only when rendered; `AlwaysTickPoseAndRefreshBones` does both. Bone-transform finalisation is observable through `USkeletalMeshComponent::RegisterOnBoneTransformsFinalizedDelegate` (multicast; the dynamic `FOnBoneTransformsFinalized` is deprecated since 5.5). Whether that callback fires for a primitive-hidden mesh under ADR-0013's external tick-rate control is **unverified** (owned by the S10 re-run; the `ReopenWarmupMaxFrames` failsafe bounds the cost if it does not) | [V runtime, S10]; [V] `DataChannel.cpp:3528-3537`, `SkinnedMeshComponent.h:98-101`, `SkeletalMeshComponent.h:68-69, 2883-2885`; [U] callback under hidden + external control: S10 re-run |
| E18 (new) | `AActor::OnActorChannelOpen(FInBunch&, UNetConnection*)` is called on the client after every successful `SerializeNewActor`, **including a re-bind of a live actor**; `InBunch.ChIndex` is the opening channel. On "failed to find/create ACTOR" the engine only marks the channel `Broken` (later bunches ignored) and sends nothing, so the server cannot see a lost pawn without a client report. A client may send `NMT_ActorChannelFailure(ChIndex)` (`FNetControlMessage::Send` dereferences the connection without a null check); the server validates the index (Warning on an invalid one), logs each report, closes the connection only when the channel is the PlayerController's own, forwards only when the channel still has an `Actor` (a channel the server has closed has released it), and calls the virtual `APlayerController::NotifyActorChannelFailure(UActorChannel*)`, which runs inside `UControlChannel::ReceivedBunch` and is documented as the game's opportunity to correct a failed actor channel. Engine-initiated closes (e.g. `RelevantTimeout`) are outside the guard below; §5.5's explicit close makes them not occur for culled pawns (S9) | [V] `DataChannel.cpp:1851-1900, 2408-2416, 2904-2921, 3275-3278, 3309-3371, 3442-3449`; `Public/Net/DataChannel.h:119-131, 185`; `PlayerController.h:1918-1924`; `Actor.h:2857` |

### S10-R2. §5.6a Per-connection re-open guard (fixes Q5)

- Every relevancy close the Policy performs (the §5.5 cull close, a §5.5 relation-change close, an S10-R4 repair close) adds a **pending-close entry** to the pawn's cull record for that connection: `{TWeakObjectPtr<UNetConnection> Connection; const UActorChannel* ClosedChannel (compared, never dereferenced unless still in the slot); int32 ChIndex; double CloseSentServerTime; bool bStale}`. The entry is added only if `ClosedChannel->Closing` is true right after `Close()`; otherwise the close is counted as `CloseNotSent` and no entry is made (nothing was sent that could arrive late).
- The entry clears at the closed channel's **in-sequence close acknowledgement** (E10): the cull tick (`TG_PostUpdateWork`, every server frame, never sampled at a coarser cadence) treats the close as acknowledged when `Connection->Channels[ChIndex] != ClosedChannel`, or the slot holds the same object no longer `Closing` (pooled reuse). A connection that closes drops its entries.
- `AStealthPawn::IsNetRelevantFor` gains one clause, after the spawn gate and before §5.4: **if the pawn has a non-stale pending-close entry for this connection, return `false`** ("re-open held"). The clause keys on the connection and the channel state only, never on the pawn's kind, controller or relation, and applies to every re-open path (un-cull, death-write re-open, relation change back, repair). The record still returns to `Visible` in the same server step (§5.6 unchanged for the state machine, Concealment and every ally connection); only the **channel** of a connection whose previous close is unacknowledged waits.
- A held re-open opens at the pawn's next regular consider after the entry clears (≤ `U_eff` = 1/`PawnNetUpdateHz` + `T_frame` = 83 ms, §1). No forced update is added: §2's three scoped reasons are unchanged, and a release-time `ForceNetUpdate` would send an extra update to every connection at a time correlated with a cull.
- A held re-open is **late, never early**: the pawn appears on that client after its un-cull, so the hold can only delay a reveal, never expose a concealed pawn (no x-ray window; L29 unaffected). The hold is observable only by the held connection and never feeds the R7.8 `Position` answer or any gameplay gate.

### S10-R3. §5.6b Bounded hold and the forced fresh channel

- The hold is bounded by **`ReopenAckTimeoutMs`** (new IVP key, default **350 ms**, hard range 200-1000, ServerOnly, frozen in the tuning snapshot): `ReopenAckTimeoutMs = 50 · ceil((2 · L99 + 1/f_net) / 50)` = 50 · ceil((266.8 + 66.7) / 50) = **350 ms** at S12's measured L99 of 133.4 ms (one round trip at L99, plus one update interval for the client's ack packet and the server frame that reads it, on F3's 50 ms quantum). If L99 is re-measured, F3, F4 and this value are recomputed together.
- When an entry reaches `CloseSentServerTime + ReopenAckTimeoutMs` unacknowledged, the cull tick marks it **stale** (it no longer holds; counted `ReopenAckTimeout` per connection in development builds and in the harness): the re-open proceeds on a **fresh channel** at the pawn's next consider, and the (pawn, connection) pair is flagged **unverified**. The stale entry is still watched until its in-sequence acknowledgement; the unverified flag clears `2 × ReopenAckTimeoutMs` after that acknowledgement (time for a client report to arrive, one resend included) or at the pawn's next close to that connection. The stale close may overtake the fresh open on that client; that residual case is detected and repaired by S10-R4. With the guard, an open can overtake a close **only** after a timeout, so only unverified pairs can need repair.

### S10-R4. §5.6c Lost-pawn repair (client report, server fresh channel)

- **Client detection (every pawn, every client).** `AStealthPawn` on a non-authority process overrides `OnActorChannelOpen` (calling `Super`) and keeps `{uint8 ChannelOpenCount; int32 LastOpenChIndex; FNetworkGUID OpenNetGUID}` (E18). A second open on a live instance means an open overtook an older close on that client (the race precondition). If that instance is then destroyed by the network (`EndPlay` with `EEndPlayReason::Destroyed` on a client; clients never destroy replicated pawns themselves) while `ChannelOpenCount ≥ 2`, `EndPlay` only **queues** a probe `{LastOpenChIndex, OpenNetGUID}` on the local `AStealthPlayerController` (client-local, never sent from inside channel cleanup). On the next client tick the probe reports only if the channel at `LastOpenChIndex` is still an open, not-`Closing` actor channel for `OpenNetGUID` (the stale close destroyed the actor while the newer channel stayed open); a legitimate close of that channel has cleaned its slot by then and is not reported [U: S10 re-run verifies the condition]. The report is `FNetControlMessage<NMT_ActorChannelFailure>::Send(ServerConnection, LastOpenChIndex)`, sent only if `ServerConnection` is non-null, `USOCK_Open`, and its control channel is non-null and not `Closing`; otherwise the probe is dropped and counted. No game RPC is added; the message carries only a channel index the server assigned.
- **Server repair.** `AStealthPlayerController::NotifyActorChannelFailure(UActorChannel* Chan)` (E18) only **queues** `{TWeakObjectPtr<APawn> Pawn, int32 ChIndex, const UActorChannel* Chan}` when `Chan->Actor` is an `AStealthPawn`. The cull tick (the single site of every Policy relevancy close) re-checks `Connection->Channels[ChIndex] == Chan && Chan->Actor == Pawn && !Chan->Closing`, that the Policy currently finds the pawn relevant to this connection, and that the (pawn, connection) pair is **unverified** (S10-R3); if so it calls `RequestFreshChannel`: close that channel (`Relevancy`) and add a pending-close entry (S10-R2), so the pawn re-opens on a fresh channel after the in-sequence acknowledgement or `ReopenAckTimeoutMs`. A queued report that fails the re-check (a slot reused for another actor, a pair that is not unverified, an engine-reported `SerializeNewActor` failure) is dropped and counted as `UnexpectedChannelFailure`; the S10 re-run requires that count to be 0, so it doubles as a diagnostic. Reports for a channel the server has already closed are discarded by the engine before `NotifyActorChannelFailure` (E18).
- **Rate limit without loss.** Valid reports above **`ChannelRepairMaxPerSec`** (new IVP key, default 4 per connection, hard 1-10, ServerOnly) are **deferred** in a per-connection FIFO holding at most one entry per relevant pawn, never dropped; only invalid reports are dropped. The engine logs each report (`LogNet` Log line, Warning for an invalid index); the re-run's warning accounting allows these lines.
- **Leak and abuse review.** The report tells the server nothing it cannot already know (a channel it opened to that client); the reaction (one close and re-open) is visible only to the reporting client and is identical for every pawn kind. A modified client can at most make the server re-send pawns already relevant to it, bounded by the rate limit and the unverified-pair condition: no new information, bounded bandwidth. The control message is not an RPC, so it adds no allowlist row; the RPC audit (Technical Setup 2026-10-25b, default deny (b)) is unchanged. IVP's leak catalogue gains a row for it (GDD change request, S10-R9).

### S10-R5. §5.8a Hidden-until-first-pose warm-up (fixes Q3b)

- **Every client-side pawn actor instance** (initial spawn, re-open, repair re-open, rejoin and late join; NPC, player and informant pawns; non-owner and the owner's own pawn alike) is created **warm-up hidden**: a client-local `bPoseWarm = false`, combined with ADR-0017's presentation latch as **hidden = latch OR NOT `bPoseWarm`** (hidden-primitive flag set, so no shadow, reflection or Lumen trace of the un-posed body). Applying it to every creation path keeps a re-open indistinguishable from a spawn by presentation. (ADR-0017's fresh-actor rule covers non-owner pawns; the warm-up adds the owner's pawn, amendment recorded in ADR-0017.)
- In `PostNetInit` (which runs for every newly spawned client actor, E11), after `RefreshPresentation(EstimatedServerNow())` has applied the replicated state (§5.8, unchanged), the mesh is placed in **warm-up mode** by ADR-0013's subsystem: `VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones`, external update enabled at rate 1, until warm (amendment recorded in ADR-0013). `bPoseWarm` becomes true in the first `OnBoneTransformsFinalized` callback (E11) that follows the presentation apply; the subsystem then returns the pawn to its tier settings (ADR-0013 §3, client `AlwaysTickPose`). A pawn whose `AppearanceId` has not resolved has no mesh (ADR-0012 §4) and stays hidden until it does.
- **Clock gate.** While `IsServerClockSynced()` is false the warm-up does not complete: staying hidden is pawn presentation's documented unsynced fallback (ADR-0002 §9 amendment), because the pose cannot be seeked correctly without the estimator and the engine clock is forbidden for presentation. On the listen host `IsServerClockSynced()` is always true. Normal flow keeps re-opens out of this window (sync takes ≈ 2.5 s after a controller begins play, during `Preparing` or behind the rejoin loading screen); the harness counts any re-open that waits on it.
- **Failsafe.** **`ReopenWarmupMaxFrames`** (new IVP key, default 3 client frames, hard 1-10, audience All, uniform): a synced pawn not warm by then is shown anyway and counted (`WarmupTimeout`, ensure in development builds). The S10 re-run requires zero.
- Cost: the first visible frame comes at most one client frame later than the first rendered frame (≤ 16.7 ms at 60 fps), identically for every pawn; it is inside `T_reopen` (S10-R7).

### S10-R6. Clock reconciliation with ADR-0002 §9 (fixes Q3c)

- The cause of Q3c was the stub reading `GetServerWorldTimeSeconds` (−150 ms median under PktLag 100); presentation already must use `EstimatedServerNow()` only (registry pattern `presentation_on_arrival_or_engine_server_time`). The re-run measures the real estimator.
- **One budget, owned by ADR-0002 §9 and unchanged**: under `LeakNetProfile`, after sync, `|EstimatedServerNow − ServerNow|` ≤ **15 ms at p95** and ≤ **30 ms at p99**. **S10's single 16.7 ms bound is retired**: the pose-time and death-elapsed error of a re-opened pawn must meet that same budget (p95 ≤ 15 ms, p99 ≤ 30 ms), where **error = the time used by the pose and death evaluation − true server time at that evaluation instant** (one QPC clock across processes, as S12); display quantisation after the evaluation is not the estimator's and is not counted.
- Why the S10 bound moves to the estimator budget rather than the reverse: (1) the error is per client and identical for every pawn on that client, so Pillar 1 is unaffected; (2) 30 ms is two 60 Hz frames of seek offset on a body that has just appeared, with no earlier frame on that client to compare against, inside ASN-R21's 0.1 s cross-fade bound; (3) ADR-0002 already sized p95 to one 60 Hz frame for clip seeks, so the two numbers agree at p95 and only S10's p99 was stricter; (4) tightening the estimator to 16.7 ms at p99 would gate all presentation on an unmeasured improvement. ADR-0002 records this consumer binding in its own amendment (no number changes).

### S10-R7. Bounds and the S10 re-run acceptance (story-056)

`G` = un-cull server time → the pawn's first visible, posed frame on a given non-ally client. `L_gap` = network destroy of a pawn that is relevant to that client → its repaired first visible frame. Measured on one QPC clock across processes (as S12), StealthLeak1 and StealthLeak2, 3 clients. "First-send delivery" = the open bunch delivered on its first send (packet timing identifies it).

| Case | Bound | Derivation |
|---|---|---|
| Free re-open (no pending close at un-cull), first-send delivery | `G` p99 ≤ 250 ms | F4 `T_uncull` (unchanged) |
| Free re-open, all deliveries | `G` p99 ≤ 520 ms; resend fraction reported | `T_uncull` + 2 · L99; S12 measured StealthLeak2 per-client p99 375-396 ms; the resend-tail decision stays with story 029 |
| Held re-open, first-send delivery | hold ≤ `ReopenAckTimeoutMs` + 1 server frame (by construction); `G` p99 ≤ 650 ms | 350 + 17 + 83 + 133 + 49 = 632, up to the 50 ms quantum |
| Every re-open, cycles with at most one resend per hop | `G` max ≤ 1.0 s | 367 + 83 + 3 · L99 + 49 ≈ 0.9 s; burst-loss cycles counted and reported separately |
| Repair | `L_gap` max ≤ 1.25 s (rate-limit deferral reported separately, expected 0) | report ≤ 2 · L99 + one client tick + hold ≤ 367 + consider 83 + delivery ≤ 3 · L99 + 49 ≈ 1.18 s |
| `T_reopen` (open-bunch receipt → first visible posed frame, warm-up included) | p99 ≤ 49 ms | F4 at 250 ms with the measured L99: 250 − 133.4 − 66.7 = 49.9; replaces F4's provisional 33 ms (GDD change request) |
| Clock (S10-R6) | pose-time and death-elapsed error at the evaluation instant: p95 ≤ 15 ms, p99 ≤ 30 ms after sync | ADR-0002 §9 budget; estimator samples recorded in the same run |

**PASS requires all of:**
1. **0 lost pawns**: every (pawn, connection) pair is observed until its unverified flag clears (no fixed window); no client ends an observation lacking a pawn that is relevant to it with an open server channel; 0 crashes, 0 ensures, `UnexpectedChannelFailure` = 0; `failed to find/create ACTOR` warnings and the engine's report log lines are allowed only when the same pawn is repaired within `L_gap`, and are counted.
2. The table's bounds, over ≥ 600 cycles per profile.
3. **0 un-posed first visible frames**, by a probe **independent of the warm-up signal**: head-bone height from the component-space transforms submitted for that frame, and silhouette height on the render client, both ≥ 80 % of the standing reference; `WarmupTimeout` = 0; a retained, lead-signed frame strip.
4. NetGUID reuse holds for every re-open including repairs (0 allocations).
5. **Parity**: player-possessed and AI-possessed subjects cycle through the same rule, **≥ 300 cycles per kind per profile, equal sample sizes**; hold time, timeout, repair, `G` and warm-up frames enter the IVP F2 classifier as features and the gate holds (`AUC_hat ≤ 0.55`, upper bound ≤ 0.58); per-feature two-sample KS results are reported as descriptive only (a non-significant KS is not evidence of parity).
6. **Controls that prove the probes**: guard off must reproduce S10's lost pawns; warm-up off must make the probe in (3) detect the collapsed frame; a **fault-injection run** (`ReopenAckTimeoutMs` = 200 plus an injected close delay) must drive ≥ 100 timeout-and-repair cycles and still meet (1) and the repair bound.
7. The hold is decomposed into close-sent → in-sequence acknowledgement, acknowledgement → open, and any `KeepProcessingActorChannelBunchesMap` delay (E10), so S10 Q5g's tail is explained; a timeout rate above 5 % of held re-opens is a CONCERN with its root cause, not a FAIL, when 1-6 pass.

Registries other than the client's pawn set remain the production re-proof of stories 024, 027 and 044 (S10 limit unchanged).

### S10-R8. Parity (Pillar 1)

The guard's inputs are a connection and a channel's acknowledgement state; the timeout, deferral and rate limit are per connection; the repair is triggered by a channel index and re-checked against channel state; the warm-up applies to every client pawn actor; the clock error is per client. None reads pawn kind, controller, possession, ownership or relation, and every path is one function for every pawn. Timing differences between two re-opens can come only from the receiving connection's network, never from what the pawn is; S10-R7 (5) measures it with the F2 classifier.

### S10-R9. Interfaces, guidelines, keys, sync items and change requests

Key Interfaces additions:
```cpp
// StealthFoundation Public/Server: Visibility Policy (S10 resolution)
struct FPendingChannelClose { TWeakObjectPtr<UNetConnection> Connection; const UActorChannel* ClosedChannel = nullptr;
                              int32 ChIndex = INDEX_NONE; double CloseSentServerTime = 0.0; bool bStale = false; }; // per-pawn cull record
bool UVisibilityPolicy::IsReopenHeld(const APawn* Pawn, const UNetConnection* Connection) const;   // §5.6a clause
void UVisibilityPolicy::QueueChannelFailure(APawn* Pawn, UNetConnection* Connection, int32 ChIndex, const UActorChannel* Chan);
void UVisibilityPolicy::RequestFreshChannel(APawn* Pawn, UNetConnection* Connection);             // cull tick only, S10-R4
// StealthGameplay Core: Shared Pawn / PlayerController
virtual void AStealthPawn::OnActorChannelOpen(FInBunch& InBunch, UNetConnection* Connection) override;   // client: count, ChIndex, NetGUID
virtual void AStealthPlayerController::NotifyActorChannelFailure(UActorChannel* ActorChan) override;    // server: queue only
```

Implementation guidelines (added):
- A pawn **must never** be re-opened to a connection while that connection's previous close for the pawn is unacknowledged in sequence, except after `ReopenAckTimeoutMs`; the hold must key on the connection and channel only.
- Close acknowledgement **must** be read every server frame from the closed channel's slot (E10), never from a sampled `OpenChannels` scan and never from the close bunch's packet ack alone; an entry is made only when the close was actually sent.
- A held re-open **must not** add a forced update; it opens at the pawn's next regular consider.
- Client pawns **must** queue (never send from `EndPlay`) a lost-pawn probe for a network-destroyed instance that received two channel opens, and report it with `NMT_ActorChannelFailure` only if the newer channel is still open for that NetGUID and the connection is open; the server **must** only queue the report and act in the cull tick, for unverified pairs, deferring (never dropping) valid reports over the rate.
- Every client pawn actor **must** start warm-up hidden (hidden = latch OR NOT `bPoseWarm`) and become visible only after a bone-transform finalisation that follows the presentation apply with the estimator synced (failsafe `ReopenWarmupMaxFrames`); warm-up mode **must** use `AlwaysTickPoseAndRefreshBones` at rate 1.

Edits to the main text made by this amendment: Status (exception sentence); Implementation Guidelines on `IsNetRelevantFor` inputs (adds pending-close entries) and on un-cull (record in the same step, channel per §5.6a/b).

New tuning keys (IVP-owned, registered through ADR-0004's schema by the implementing story): `ReopenAckTimeoutMs` (350, 200-1000, ServerOnly), `ChannelRepairMaxPerSec` (4, 1-10, ServerOnly), `ReopenWarmupMaxFrames` (3, 1-10, All).

Cross-ADR sync (amendments recorded in those ADRs the same day): **ADR-0002 §9** (consumer binding of the budget; "stay hidden" as pawn presentation's unsynced fallback; host always synced); **ADR-0013** (the subsystem applies warm-up mode); **ADR-0017** (warm-up joins the presentation latch for every pawn including the owner's; §6 pose freshness at a bush-exit reveal uses `AlwaysTickPoseAndRefreshBones` or the S10-R5 finalisation gate, because under `AlwaysTickPose` a hidden mesh does not refresh bones).

GDD change requests raised (not applied here): **IVP R7.5** "becomes relevant to all again in the same server step" gains "a connection whose previous close for the pawn is not yet acknowledged receives the re-open at that acknowledgement or after `ReopenAckTimeoutMs`, whichever is first; a held re-open arrives late, never early, is observable only by the held connection and never feeds the R7.8 `Position` answer or any gameplay gate"; **IVP F4** `T_reopen` is redefined as open-bunch receipt → first visible posed frame (warm-up included), the re-run's measured value replacing the provisional 33 ms (bound 49 ms at the measured L99); F4's example adds that held re-opens appear after the leaf-plane instant; **IVP Tuning Knobs** gain the three keys; **IVP leak catalogue** gains a row for the lost-pawn report (client → server channel index, Owner, no new fact); **Concealment** (CON-R6a/R6b) notes that a held re-open's body appears when it arrives, seeked from state, behind the warm-up.

### S10-R10. Alternatives considered for this resolution

| Alternative | Rejection reason |
|---|---|
| Unbounded hold until close acknowledgement (S10 Q5g candidate) | Correct but S10 measured holds to 8.0 s: an unbounded presentation gap |
| Re-open immediately and accept the race | ~1-2 % of race re-opens lose the pawn until the next cycle (S10 Q5): a correctness fault |
| Client `UIpNetDriver` subclass that skips a stale relevancy close (`ShouldClientDestroyActor` + `NotifyActorChannelCleanedUp`) | Engine-internal: `UActorChannel::SetClosingFlag` → `RemoveActorChannel(Actor)` still removes the actor's map entry for the live new channel (verified); replaces the game net driver for one edge case. Kept as the escalation if the re-run's repair path fails |
| Minimum cull dwell (refuse an un-cull inside the acknowledgement window) | Changes the gameplay timing of `HideExit` and is observable by the hider; the problem is a channel, not the state machine |
| Iris exclusion-group filters | ADR-0001 chose legacy; ADR-0024's scope (§5.10) |
| Evaluate the pose synchronously at begin-play without hiding | No guarantee the first render follows the evaluation (S10 Q3b); used inside the warm-up, not instead of it |
| Tighten the estimator to 16.7 ms at p99 | Unmeasured; would gate all presentation on it (S10-R6) |

### S10-R11. Consequences and risks (added)

- Positive: no lost pawns by construction for acknowledged closes, a bounded gap and a repair for the rest, no un-posed frame on any client pawn, one clock budget instead of two.
- Negative: a held re-open shows the pawn up to ~0.65 s (p99) after its un-cull on that client, later than F4's leaf-plane instant (late, never early); one more per-connection state on the cull record; the repair gives an engine control message game-defined semantics; every client pawn starts up to one frame later.

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| In-sequence close acknowledgement is often slower than `ReopenAckTimeoutMs` (S10 Q5g tail), so many held re-opens time out and rely on repair | Medium | Medium (longer gaps, more repairs) | Re-run decomposes the hold (S10-R7 (7)); 0-lost, `G` max and `L_gap` still gate; escalation: client net-driver alternative |
| The client probe condition misclassifies (reports a legitimate close, or misses a lost pawn) | Medium | Medium | [U] verified by the re-run: `UnexpectedChannelFailure` = 0 and the guard-off and fault-injection controls |
| `OnBoneTransformsFinalized` does not fire for a hidden mesh under external tick control | Low | High (pawn shown late by the failsafe, or posed incorrectly) | E11 [U] owned by the re-run; `ReopenWarmupMaxFrames` failsafe and `WarmupTimeout` = 0 gate |
| `NMT_ActorChannelFailure` semantics change in a future engine version | Low | Medium | E18 pinned to 5.8.4 source; re-verify on engine upgrade; repair path covered by the fault-injection run |
| A modified client floods failure reports | Low | Low | Unverified-pair condition, per-connection deferral FIFO (one entry per pawn), rate limit |

### S10-R12. Gate record (2026-10-25)

- **Engine validation (unreal-specialist): MINOR NOTES → applied.** E10 citations widened; entry made only when the close was actually sent (`CloseNotSent`); `KeepProcessingActorChannelBunchesMap` path added to E10 (delays, cannot lose); E18 server filtering and engine log lines stated; client send guarded on connection state; server close deferred to the cull tick with a slot re-check; engine-initiated closes stated as outside the guard. E10 and E18 confirmed against 5.8.4 source. The reviewer did not check E11; the author verified the `SkinnedMeshComponent.h:98-101` tick-option comments and `SkeletalMeshComponent.h:68-69, 2883-2885` (delegate registration, 5.5 deprecation) directly; the callback's behaviour on a hidden mesh under external control stays [U] for the re-run.
- **TD-ADR (technical-director): CONCERNS → applied.** Status exception sentence and two guideline edits (and the registry stance); the ADR-0024-collision argument removed; free-re-open bound split by first-send delivery (S12 resend tail); held bound corrected to 650 ms; `G` max derived (1.0 s) and `L_gap` added (1.25 s) with per-pair observation instead of a fixed window; valid over-rate reports deferred, not dropped; client probe condition tightened and moved off `EndPlay`; negative controls, a fault-injection run, an independent first-frame probe and F2-classifier parity with ≥ 300 cycles per kind added; clock error measured at the evaluation instant; ADR-0013/0017/0002 sync items recorded; R7.5 change request extended (held connection only, never feeds R7.8). Pillar 1 confirmed: no mechanism reads pawn kind.
- GDD sync: checked the referenced GDDs (`information-visibility-policy.md`, `concealment-bushes.md`): no renamed names; the new keys and the F4 `T_reopen` definition are **GDD SYNC REQUIRED** items, raised as change requests above (not applied by this run).
