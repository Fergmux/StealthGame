# ADR-0003: Replication, relevancy and the concealment cull hybrid

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. Cannot be Accepted before ADR-0001, ADR-0002, ADR-0007 and ADR-0008 are Accepted and spikes S9, S10, S12 and S13 have reported.

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims re-read against the UE 5.8.4 install; design against IVP R5-R8, R11 and the reconciled GDDs of 2026-10-24)

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
| E10 | `Close` removes the channel from the connection's lookup immediately (`FindActorChannelRef` then returns null); with no channel, relevancy is re-tested at every update check and a channel re-opens only if the actor is relevant or force-relevant (set only by the two calls §2 bans). So a closed channel stays closed while the override returns false. A cull followed by an un-cull before the client acknowledges the close (two channels for one NetGUID in flight) is unverified | [V] `DataChannel.cpp:2408-2412, 2470`; `NetDriver.cpp:5752, 5774`; `NetworkObjectList.cpp:597` / [U] in-flight race: S10 |
| E11 | Re-open creates a new client actor instance; whether the NetGUID is reused, and how the AnimBP initialises, is unknown | [V new instance per IVP R7.7] / [U] S10 |
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
                 ADR-0002 staging ─► death write (non-concealment cull ⇒ →Visible; KilledConcealed stays)
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
- `AStealthPawn::IsNetRelevantFor` must never call `Super`; its only inputs are the Policy's cull record and the viewer's relation.
- Nothing may call `ForceNetRelevant()` or `ForceActorRelevantNextUpdate()` on a pawn; `ForceNetUpdate()` on a pawn must forward only inside `FStealthForcedNetUpdate` with an allowed reason, capped at one per pawn per frame.
- `PreReplication` must write `RemoteViewPitch16` from `HeadLook` for every pawn after `Super`.
- Every simulated proxy must use exponential smoothing with the same tuning values, including the listen-server simulated smoothing times.
- Outcome-bearing replicated structs must serialise to a fixed bit width with no value-dependent size.
- The concealment cull must land on the global boundary grid, close the channel explicitly per denied connection in the cull tick, and never wait for a client acknowledgement; un-cull must happen in the same server step as the state change.
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
