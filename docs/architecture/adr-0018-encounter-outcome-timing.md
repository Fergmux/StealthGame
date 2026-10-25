# ADR-0018: Encounter data and outcome presentation timing

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It cannot be Accepted before every ADR in "Depends On" below is Accepted (ADR-0001, 0002, 0003, 0004, 0008, 0010, 0011, 0012, 0013, 0017; and, through them, spikes S1, S2, S10, S14), and before the amendment requests listed under Related are answered by their owners.

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims re-read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; design against assassination-defence-reversal.md, hud.md and npc-kill-penalty-informants.md in their "reconciled 2026-10-24" state)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full); technical-director (TD-ADR) and unreal-specialist validation (gate record below); acceptance reserved for the user.

**Gate record (2026-10-24):** see "Validation gates" at the end of this document.

## Summary
A strike's outcome is decided in one server queue slot, but anything a client can observe about that outcome (a fall, a counter, a plate, a camera fade, a role change) must appear at the moment of contact, identically for every pawn kind and without arrival order or packet size encoding the outcome. This ADR fixes the encounter wire data (`FActionState`, `FDeathState`, the owner results `FStrikeResult`/`FDefenceResult`), the two-layer timing (one outcome-blind write at the slot, outcome writes as typed staged writes at `ContactServerTime − StrikeNetLeadSec`), the idempotent state-derived client presentation and its gating, the positional result-to-strike matching rule (with a reserved slot for the pending `ContactServerTime` field), and the server body sequence (fall, release, removal) for every victim.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4) |
| **Domain** | Networking (primary); Animation (clip scheduling, motion warping) |
| **Layer** | Feature (Assassination & Defence Reversal; wire structs live in Foundation `Public/View`, LP concern 2) |
| **Knowledge Risk** | HIGH — networking and animation behaviour 5.4-5.8 is post-cutoff for the model; motion warping replication verified below. **MotionWarping: Beta in 5.8.4** ([V] `MotionWarping.uplugin` `IsBetaVersion: true`, `VersionName 0.1`): its API and the protected `WarpTargets` member that E5's `DISABLE_REPLICATED_PROPERTY_FAST` names may change between engine versions; the protected-member disable and the `COND_Never` audit row are on the engine-upgrade checklist |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/networking.md`, `modules/animation.md` (index only), `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Module Ownership: Assassination, Penalty; Data Flow §1-§3; API Boundaries: Feature; Required ADRs row ADR-018; AQ-11); ADR-0001, ADR-0002, ADR-0003, ADR-0010, ADR-0011, ADR-0012, ADR-0013, ADR-0015, ADR-0016, ADR-0017; engine source read this session (E1-E6 below) |
| **Post-Cutoff APIs Used** | None new to this ADR. Relies on `TStructOpsTypeTraits::WithNetSerializer`/`WithIdentical` [V], `FArchive::SerializeBits`/`SerializeInt` [V], double `UWorld::TimeSeconds` [V], `UMotionWarpingComponent::AddOrUpdateWarpTargetFromLocation` [V]; everything else is consumed through ADR-0002/0003/0010/0011 interfaces |
| **Verification Required** | (1) byte capture: the `FActionState` bunch of a kill, a whiff and a reversal, and the `FDeathState` bunch of every victim kind, are identical in size (ADR-0003 E12 spike extended to this ADR's layout); (2) S10 re-open: an actor first opened after `DeathServerTime` presents the fall at the right frame (or not at all after the linger) through `RefreshPresentation`; (3) under `LeakNetProfile` the `FStrikeResult` arrival relative to the matching `FActionState` is measured in both orders and the plate shows at `t_show` (HUD F2) in both; (4) the motion-warping `WarpTargets` property carries no strike target on the wire (E5); (5) host parity: the listen host's own plates, fall and dispatch occur at the same server times as a remote client's |

**Engine facts this ADR relies on** ([V] read in the 5.8.4 install this session; [D] engine-reference docs; [U] unverified, owned by the named check):

| # | Fact | Tag |
|---|---|---|
| E1 | A `USTRUCT` may supply its own `NetSerialize` (`WithNetSerializer`) and `Identical` (`WithIdentical`) through `TStructOpsTypeTraits`. `NetSerialize`'s **return value means "mapped"** and must be `true` (`bOutSuccess` is pre-set true; false only logs). Legacy change detection compares through `Property->Identical`, which calls the native `Identical`, so it must compare **every** field incl. `Seq`. Push model is not required; shared serialisation is off unless `WithNetSharedSerialization` is set (safe to opt in: no object references) | [V] `CoreUObject/Public/UObject/Class.h:1221-1230, 1342-1345, 1404-1407`; `CoreUObject/Private/UObject/PropertyStruct.cpp:184-200`; `Engine/Private/RepLayout.cpp:746, 5555`; `CoreUObject/Private/UObject/Class.cpp:3672-3679` |
| E2 | `FArchive::SerializeBits(void*, int64)` and `SerializeInt(uint32&, uint32 Max)` exist; only the net bit-writer override caps `SerializeInt` to the width of `Max` (the base archive writes 32 bits), so the fixed-width serialiser uses `SerializeBits` and the width test runs on `FNetBitWriter` | [V] `Core/Public/Serialization/Archive.h:1716, 1726` |
| E3 | `UWorld::TimeSeconds` is a `double` | [V] `Engine/Classes/Engine/World.h:2010` |
| E4 | `UMotionWarpingComponent::AddOrUpdateWarpTargetFromLocation(FName, FVector)` sets a target locally | [V] `MotionWarping/Public/MotionWarpingComponent.h:211` |
| E5 | `UMotionWarpingComponent` is replicated by default (`SetIsReplicatedByDefault(true)`) and replicates `TArray<FMotionWarpingTarget> WarpTargets` push-based with `COND_SimulatedOnly`; `FMotionWarpingTarget` carries a full `FVector Location`, an `FRotator` and a `TWeakObjectPtr<const USceneComponent> Component`. A server-side `AddOrUpdateWarpTarget` therefore sends a second, variable-content copy of the strike target to every simulated proxy (and would overwrite a locally set target there) and, if built from a component, a reference to the victim. `WarpTargets` is `protected`, so a subclass disables it with `DISABLE_REPLICATED_PROPERTY_FAST` (engine precedent), which leaves a `COND_Never` lifetime row rather than removing it | [V] `MotionWarping/Private/MotionWarpingComponent.cpp:289, 292-299, 704, 715, 733, 845`; `Public/RootMotionModifier.h:210-235`; `Public/MotionWarpingComponent.h:255, 275-276`; `Engine/Public/Net/UnrealNetwork.h:407-419`; `Engine/Private/Net/UnrealNetwork.cpp:103-107`; precedent `ClusterUnionComponent.cpp:1406` |
| E8 | `ACharacter::RepRootMotion` (`COND_SimulatedOnly`) carries the root-motion montage, its position, location, velocity and base for simulated proxies; it is outcome-blind only if the server's lunge is outcome-blind | [V] `Engine/Private/Character.cpp:2028-2074, 2145` |
| E9 | 5.8.4 has no `APlayerController::PawnPendingDestroy` override: destroying a possessed pawn runs `AController::PawnPendingDestroy` (`UnPossess`, `ChangeState(NAME_Inactive)`, and `Destroy()` when `PlayerState` is null), so crowd controllers are destroyed and PlayerControllers go `Inactive` — behaviour by controller kind. A plain `UnPossess` runs `APlayerController::OnUnPossess`, which calls `SetViewTarget(this)` | [V] `Engine/Private/Controller.cpp:421-438`; `Pawn.cpp:550-561, 1112-1124`; `PlayerController.cpp:1282, 1353-1368` |
| E6 | The legacy NetDriver gives no ordering guarantee between a reliable client RPC on one actor (the PlayerController) and property replication of another actor (the pawn); each actor channel is ordered only within itself | [U] (behaviour stated by HUD F2 and the ue-replication-specialist at HUD review; this ADR does not depend on either order, Verification 3 measures it) |
| E7 | The listen host does not receive RepNotify for its own server writes; native code must call the handler directly | [D] `modules/networking.md` (RepNotify runs on clients); architecture.md Data Flow §2 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (one pawn class; carrier model: `All` facts on the pawn, Owner data on the PlayerController; no pawn RPC), ADR-0002 (queue slot, inline follow-ons, typed staged writes `DeathWrite`/`CounterPose`, staging pass, `RoundEndStep` binding by `CauseKey`, due-time heap, `EstimatedServerNow`), ADR-0003 (fixed-width serialisers, `FStealthForcedNetUpdate` reasons `ActionStateWrite`/`DeathWrite`, uniform net config, re-open at the death write, killed-concealed interim), ADR-0010 (`Disabled` through the movement freeze flag, hand-over pass, possession only through the factory), ADR-0011 (`UPawnCommandComponent::WriteActionState` as the only `FActionState` writer, the per-connection reply batch `ClientCommandReplies`, `FClientCommandSeq`), ADR-0012 (`VacateCard`), ADR-0013 (AnimBP reads `FActionState`/`FDeathState`; one fall montage), ADR-0017 (spot occupants, bush footprint, `HidePoseStartServerTime` stopgap, `MarkKilledConcealed`), ADR-0004 (Assassination and HUD tuning keys), ADR-0008 (byte-capture and net-emulation harness). All must be Accepted before this ADR is implemented. |
| **Enables** | ADR-0019 (informant lifecycle reads the scheduled `DeathServerTime` for the `T_due` floor and the body release), ADR-0021 (HUD result plates, own-removal card and brace indicator schedule from this ADR's data and dispatch), ADR-0016 (Crowd consumes `OnPawnKilled` with the victim-driving rule of §3.4), ADR-0023 (Event Log rows for strikes and staged writes) |
| **Blocks** | Assassination & Defence Reversal epic (all stories touching the strike, the death write, bodies or owner results); the HUD result-plate, removal-card and brace-indicator stories; the NPC-Kill Penalty tally/exile stories that fill `FStrikeResult` |
| **Ordering Note** | Should-have before Assassination is built (architecture.md). The three fields once carried as reserved slots — `ContactServerTime` in `FStrikeResult` (HUD OQ15), `FActionState.StartServerTime` (Concealment CR-10 b; ADR-0017 §8 A), `FDeathState.bDiedConcealed` (Concealment CR-10 a; ADR-0017 §8 B) — were approved by the user 2026-10-25 and are specified in §1. Amendment requests to ADR-0002, ADR-0010, ADR-0012 and ADR-0016 (Related) gate acceptance of this ADR, not its authoring; all were answered on 2026-10-24 (see Amendments). **Acceptance pair with ADR-0017** (architecture-review fix 2026-10-25): the two ADRs depend on each other's contracts (this ADR reads Concealment's spot queries and kill hook; Concealment reads this ADR's `FActionState`/`FDeathState` semantics and implements `IBodyRemovalObserver`), so they are reviewed and accepted together in one step. The edge is declared here only (ADR-0018 → ADR-0017); ADR-0017 records the pair in its Ordering Note instead of listing this ADR under Depends On, so the declared graph stays acyclic. |

## Context

### Problem Statement
Assassination resolves a strike completely inside the Attack's queue slot (ASN-R10): victim, outcome, Roster transitions, strike accounting and the elimination check all happen there, about `StrikeContactSec` (0.40 s; 0.75 s from a bush) **before** the blade visibly lands. If anything outcome-dependent left the server at the slot as an `All`-audience change, a modified bystander client would know kill versus whiff versus reversal up to `T_v` early, and arrival order or packet size could encode it even for an honest client (P1, ASN-R21). If it left late, falls would start late and be snapped. Owner data (the attacker's result, the victim's role change, the defender's reversal) must reach the owner early enough to be shown at contact, yet never be **shown** before it (ASN-R6, R23). Unreal Engine offers no ordering between an owner RPC and a pawn's property replication (E6), so the HUD cannot assume the strike's timing data arrives before its result. Finally, the motion-warping component of the Game Animation Sample's pawn replicates its warp targets by default (E5), a second, unaudited copy of the strike target on the wire.

This decision is needed now because Assassination, the HUD and Penalty are written against these structs, the death-write staging contract in ADR-0002 delegates its payloads to this ADR, ADR-0003 delegates the `WarpPoint` bit layout to it, and ADR-0012/ADR-0013 delegate card vacating, possession through the fall and body removal to it.

### Constraints
- **Pillar 1 / IVP:** a pawn carries only `All` facts, identical in type, size and timing for every pawn kind; no pawn RPC, no multicast, no effect actor (ADR-0001, ADR-0003, IVP L15).
- **One authoritative order:** every gameplay decision is made in a queue slot; outcome-revealing presentation state is applied only by typed staged writes, which decide nothing (ADR-0002 §5-§6).
- **Server clock only:** client presentation schedules from `EstimatedServerNow()` (ADR-0002 §9), never from the arrival time of a replicated value.
- **Schema decisions taken by the user 2026-10-25:** `ContactServerTime` in `FStrikeResult`, `FActionState.StartServerTime`, `FDeathState.bDiedConcealed` (all specified in §1) and `RetireAtServerTime` on the C-TeamView retire (Roster R6; this ADR supplies the kill's contact time, §3 step 3).
- **Listen server:** the host process holds server state; its presentation must pass through the same gate (ASN-R23).
- **No new tuning keys:** every timing value already exists in the ASN/HUD/Penalty knob tables.

### Requirements
- Fixed-width `FActionState {Pose, Seq, Variant, StartServerTime, ContactServerTime, WarpPoint}` and `FDeathState {DeathServerTime, bDiedConcealed}`, the same size for every outcome and victim kind (ASN-R21).
- One outcome-blind cue at the slot (the attacker's strike pose); every outcome-revealing write at `ContactServerTime − StrikeNetLeadSec` in one server step, written even after the round latch (ASN-R21).
- `WarpPoint` never reads a pawn the attacker cannot know (ASN-R21, revised at design review).
- Victim untouched until the death write (ASN-R16); one body sequence for every victim (ASN-R17); possession stays through the fall (ASN-R22).
- Owner results: exactly one `FStrikeResult` per executed Attack, one `FDefenceResult` per reversal by a bound defender, same shape for every result, presented at the local contact frame (ASN-R19, R23; HUD-R6, F2).
- Positional result-to-strike matching with no command sequence on an `All` struct (ASN-R23, HUD F2, CR-HUD-14).
- One client-local own-role dispatch, `OnRoleChangePresented`, at the pawn's own `DeathServerTime` (ASN-R23, HUD-R6a, Camera CS-R12).
- Tally moves with the plate (HUD-R7, PEN-R3, PEN-R14): `TallyAfter`/`bExiled` ride inside `FStrikeResult` (PEN-R15).
- Budget: no measurable addition to the 0.5 ms queue/resolver line or the 36 KB/s pawn bandwidth line (SCL F9, 31.1 KB/s worst case) (architecture Performance budget).

## Decision

### 1. Wire structs (StealthFoundation `Public/View`)

**1.1 `FActionState`** — one `ReplicatedUsing=OnRep_ActionState` property on `AStealthPawn`, `All` audience, written only by `UPawnCommandComponent::WriteActionState` inside `FStealthForcedNetUpdate(ActionStateWrite)` (ADR-0011 §7, ADR-0003 §2). Custom `NetSerialize` (`WithNetSerializer`) and `Identical` (`WithIdentical`) (E1), **208 bits for every value** (E2; was 144 before `StartServerTime`, user decision 2026-10-25):

| Field | Bits | Meaning (this ADR owns encounter semantics; ADR-0011 owns the pose list) |
|---|---|---|
| `Pose` (`EActionPose`, uint8) | 8 | `None`, `Strike`, `Brace`, `Counter`, `Interact`, `BushEnter`, `BushExit` (IIF R12 list; append-only). |
| `Seq` (uint16) | 16 | Per-pawn write counter **assigned by `WriteActionState` itself** (a caller's `Seq` is ignored): +1 on **every** write of that pawn, any pose, any pawn kind; default 1 (sentinel); wraps 65535 → 1. Compared only for equality ("differs from the last consumed write"). Never carries or encodes a `ClientCommandSeq` (ASN-R23: a command sequence on an `All` struct exposes a client's command cadence, which NPCs cannot match). |
| `Variant` (uint8) | 8 | For `Strike`: bits 0-1 entry variant (0 `Strike_F`, 1 `Strike_L60`, 2 `Strike_R60`; chosen from the bearing θ by ASN-R16), bit 2 `BushPreface`; other bits 0. For every other pose 0. |
| `StartServerTime` (double) | 64 | Server time the pose began, for every write and pose (`Strike` and its bush preface: the slot time; `Counter`: the death-write time; `BushEnter`/`BushExit`: the transition start, ADR-0017 §8 A; `None`: the write time). Added by user decision 2026-10-25 (Concealment CR-10 b). |
| `ContactServerTime` (double) | 64 | For `Strike`: slot server time + `StrikeContactSec` (+ `BushStrikeEmergeSec` when `BushPreface`). For `Counter`: the reversed strike's `ContactServerTime`. Otherwise −1 (no longer overloaded with the hide-pose start, 2026-10-25). |
| `WarpPoint` (`FFixedWarpOffset`) | 48 | Three `int16` in **0.1 cm** units, **world axes**, offset of the strike's warp target (the victim-capsule point the lunge aims at) from the attacker's server capsule location at the slot; `(0,0,0)` for every non-`Strike` pose. Range ±3,276.7 cm, far above the semantic clamp of §2.2, so the clamp, not the encoding, bounds it. `FFixedWarpOffset`'s members are `UPROPERTY`s (legacy change detection, ADR-0003 §4). |

No optional field, no conditional bit, no magnitude-packed vector. `NetSerialize` writes with `SerializeBits` and always returns `true`; `Identical` compares all six fields (E1); `WithNetSharedSerialization` may be set. The attacker's pawn changes no other replicated property at the slot except replicated movement and `RepRootMotion` (the strike clip is a root-motion montage; its asset differs only by `Variant`, which is already public).

**1.2 `FDeathState`** — one `ReplicatedUsing=OnRep_DeathState` property on `AStealthPawn`, `All`, `double DeathServerTime = −1.0` (64 bits; sentinel −1). Written once per pawn, only by the death write (§3.2), equal to the deciding strike's `ContactServerTime`. Never reset (bodies are never re-possessed, ASN-R17). `bool bDiedConcealed` (1 bit, so `FDeathState` is 65 bits; user decision 2026-10-25, Concealment CR-10 a): true only when the victim was `Concealed`/`CullPending` at the death write, written in the same write, identical for every victim kind (ADR-0017 §8 B).

**1.3 `FStrikeResult`** (Owner) — an entry kind of ADR-0011's per-connection reply batch `ClientCommandReplies` (owner-only reliable RPC on `AStealthPlayerController`), never a property:
`{ FClientCommandSeq ClientCommandSeq; EStrikeResult Result; uint8 TallyAfter; bool bExiled; }` with `EStrikeResult { RivalEliminated, WrongTarget, NoContact, Reversed }`. Same shape and size for every `Result` (ASN-R19). `TallyAfter` (saturating at 255) and `bExiled` are supplied by Penalty in the slot through `IStrikeTallySource` (§4.2); the tally was confirmed by the user 2026-10-25 (Penalty OQ-NKP-1). `double ContactServerTime` (user decision 2026-10-25, HUD OQ15): the strike's contact server time (equal to the attacker's strike-write `ContactServerTime`; for `NoContact` the whiff's nominal contact time), so every `Result` keeps one shape; HUD F2 reads it first (§5.3).

**1.4 `FDefenceResult`** (Owner) — another entry kind of the same batch: `{ EDefenceResult Result /* Reversed */ }`, sent only to a bound defender's connection on a reversal (null sink for an NPC defender). Carries no time and no attacker reference (ASN-R19).

**1.5 `UDefenceOwnerComponent`** (Owner) — a replicated `UActorComponent` on `AStealthPlayerController` (ADR-0001 §5 owner data component; IVP fact row `DefenceState`, Owner, PlayerController already exists; allowlist row required): `double CooldownEndServerTime = −1` written at `Prepare(Begin)` execution (`LastPrepareActivation + DefenceCooldownSec`) and on re-pawn copy (ASN-R18). The brace window itself is the IIF's `FHoldStatus`; on a reversal its end carries `ServerEndTime = ContactServerTime` (ASN-R23, ADR-0011 §7).

### 2. The slot: one outcome-blind write and the decided outcome

**2.1 Order inside the Attack's slot** (the IIF executes the `Attack` and calls Assassination's follow-on inline; all of it before slot end, ADR-0002 §5):
1. Victim resolution and outcome (ASN-R8, R10, F1, F4) from live state. For a bush spot, Assassination filters Concealment's occupants by F1 reach and draws the victim from the `BushSpotPick` secret stream (`ESeedDomain` 14, tag `CON_SPOT`, ADR-0017 §7, ADR-0006), never `H(...) mod n` (amendment 2026-10-24, confirmation requested by ADR-0017).
2. `WriteActionState(Attacker, {Strike, Variant, ContactServerTime, WarpPoint})` (Seq assigned by the writer) — the only replicated change at the slot (forced update `ActionStateWrite`), identical in shape for a kill, a whiff and a reversal.
3. Roster transition if any (`RequestTransition(V, Informant, Assassinated)` or `(A, Informant, ReversedAgainst)`), before `OnPawnKilled` (ASN-R10); the binding retirement passes `RetireAtServerTime` = this strike's `ContactServerTime` (Roster R6, Teammate Markers TM-R6; user decision 2026-10-25).
4. `UAppearanceAllocator::VacateCard(Dead pawn)` for the killed pawn (kill: V; reversal: A), with no kind parameter (ADR-0012 guideline 7; CP-12b). It runs **in the slot**, because the card ledger is server-only gameplay state and must not be mutated in the staging pass (ADR-0002 §5 allows no new mutation there); see the ADR-0012 sweep amendment under Related.
5. Concealment: for a victim killed while concealed, the occupancy exit in the slot; the un-cull is staged with the death write, which sets `bDiedConcealed = true` (ADR-0017 §8 B, ADR-0003 §5.7; user decision 2026-10-25).
6. `OnPawnKilled(Victim, Killer, VictimKind, Cause)` for kills and reversals (subscribers inline: Penalty strike/tally/exile, Crowd vacancy, Round log). Not emitted for a whiff.
7. Stage the outcome writes (§3.1) with `CauseKey` = this slot's `RootCauseKey`.
8. Build `FStrikeResult` (Penalty fills `TallyAfter`/`bExiled` through `IStrikeTallySource`, §4.2) and, on a reversal by a bound defender, `FDefenceResult`; append both to their sinks' per-pass reply buffers (ADR-0011 §8: within one slot the IIF's own reply precedes the feature result).
9. Event Log rows `KillResolved{VictimKind, Cause}` / `Reversal{Attacker, Defender}` with the slot key (Round R19).

**2.2 `WarpPoint` derivation (never reads a pawn the attacker cannot know).** Let `P_A` be the attacker's server capsule location at the slot.
- **Honoured `PawnRef`** (rule 1, which already required `CanViewerKnow(Attacker, V, Position)`), hit or reach-failed whiff: target = F1's extrapolated victim capsule centre `P_V(T_v)` (velocity held for `t_d = T_v − λ`, baseline braking after).
- **Open spot** (rule 3), hit or whiff: target = the submitted `WorldPoint`.
- **Bush spot** (rule 2), hit or whiff: target = the `WorldPoint` clamped to the bush footprint edge on the attacker's side (footprint from ADR-0017's Manifest-derived `FBushRuntime`, identical on server and clients).
- **Clamp (all cases):** horizontal `|target − P_A|_xy ≤ 2·r_c + StrikeContactGapCm + StrikeWarpLimitCm` and `|Δz| ≤ StrikeVerticalReachCm`; the bearing of the target is not clamped here (the warp yaw is capped at `StrikeWarpYawLimitDeg` by the animation, ASN-R16).
- Quantise `target − P_A` to 0.1 cm per axis (round half away from zero). No culled or concealed pawn's position ever enters the value (ASN-R21).
- **The server's own lunge warps to the same `WarpPoint`** for every outcome; the server never re-tracks the victim's capsule during the lunge (E8: `RepRootMotion` and replicated movement would carry a hit-only re-track to every simulated proxy). Contact residuals are reconciled by ordinary movement smoothing. (ASN-R16 "only the server may re-track" is a permission this ADR does not use; GDD sync note.)

**2.3 What does not change at the slot.** The victim's `FActionState`, movement, input, pose, collision and relevancy; any defender's pose (it stays in `Brace_Hold`); every `All` fact on any pawn other than the attacker. Holds of the victim are cancelled in the slot as server state (IIF cancel reason `PawnRemoved`, owner `FHoldStatus` sent but held for presentation, §5.2), but the IIF must **not** write the victim's `FActionState` for that cancel: the death write supersedes it (§3.3). Round's latch, if this slot decides the round, publishes nothing (ADR-0002 §8.2).

### 3. The death write: outcome-revealing state at `ContactServerTime − StrikeNetLeadSec`

**3.1 Staged writes** (ADR-0002 §6 closed set; this ADR owns the payloads' meaning, times and appliers, and ADR-0002 §2a declares the structs in Foundation `Public/Server`), all with `ServerTime = T_dw = ContactServerTime − StrikeNetLeadSec` (≥ slot time by I-ASN-4) and the slot's `FEpochToken`:
- Kill: `DeathWrite{Victim = V, DeathServerTime = ContactServerTime}`. Whether the victim re-opens is the Policy's decision from its own cull record (§3.2 step 3), not a payload field.
- Reversal: `DeathWrite{Victim = A, …}` and `CounterPose{Defender = V, ContactServerTime}` (TypeRank: DeathWrite before CounterPose, same server step).
- Whiff: none.
`FDeathWritePayload { FPawnHandle Victim; double DeathServerTime; }` and `FCounterPayload { FPawnHandle Defender; double ContactServerTime; }` are the Foundation structs of ADR-0002 §2a. `FPawnHandle` is ADR-0002's server-only weak `APawn` handle, whose generation is the weak pointer's serial number; the applier resolves it with `ResolvePawnHandle` and casts to `AStealthPawn` in this module.

**3.2 Applying a `DeathWrite`** (staging pass, one server step, identical for every victim kind; even after the latch; dropped on `Reset` by epoch):
1. `FDeathState.DeathServerTime = DeathServerTime` inside `FStealthForcedNetUpdate(DeathWrite)`.
2. `Disabled`: set the movement freeze flag through ADR-0010 §4.5 (the victim brakes by its own gait's baseline; traversal stops at its server position).
   **The death write never interrupts the lunge (closeout 2026-10-25):** if the pawn receiving the freeze flag is the attacker of a `Strike` whose lunge (commanded displacement toward `WarpPoint`, SCL) is in progress, which is the case for the attacker of a reversal (its `DeathWrite` is staged at `ContactServerTime - StrikeNetLeadSec`, before contact), the lunge **runs to `ContactServerTime`**: the movement freeze flag is recorded at `T_dw` but takes effect at `ContactServerTime` (the lunge's own end, after which the pawn brakes by its gait baseline), so the server position at contact and the owner's/other clients' local warp agree and no early stop is observable. Replicated state (`FDeathState`, visibility) still changes at `T_dw`; only the locomotion freeze is deferred. SCL confirms the lunge has no death-write cancel path.
3. Visibility: notify the Policy's death-write hook; a pawn culled for a reason other than concealment re-opens now, already carrying `DeathServerTime`; a killed-concealed pawn stays closed (ADR-0003 §5.7).
4. Nothing else: no Roster, card, score or legality change (all decided in the slot).

**3.3 Applying a `CounterPose`:** `WriteActionState(Defender, {Counter, Variant 0, ContactServerTime, WarpPoint 0})` (forced update `ActionStateWrite`). The defender never becomes `Disabled` (ASN-R16). `WriteActionState` remains the only writer of `FActionState`; the staging pass calls it as an apply-only consumer, which ADR-0011 §7 already names.

**3.4 Victim-driving rule (Crowd and humans).** From the slot until the death write a victim keeps its own motion: a human's `ServerMove`s keep driving it; an NPC victim's Crowd driver must keep issuing exactly what it would issue had the strike whiffed (outcome-blind stimuli only; the victim is not removed from driving, stimuli or the follower until the death write's freeze flag) — conformance request to ADR-0016 under Related. Its discrete commands resolve under its new legality (an `Informant` or `Dead` strike role) with replies held (§5.2).
**Every `OnPawnKilled` subscriber** is bound by the same rule: no subscriber may cause any change observable by a client before the death write (TD-ADR concern 2). In particular Crowd's kill handling (vacancy, ambient-exit cancellation, ADR-0016 §6) may only change exits scheduled at or after the kill's `KillServerTime`, and crowd stimuli about a strike come only from the outcome-blind `OnActionStarted(Attack)`, never from `OnPawnKilled`. `KillServerTime` is defined as `GetScheduledDeathServerTime(Victim)` = the strike's `ContactServerTime` (ADR-0016's handler parameter).

**3.5 Round end.** When the slot decided the round, ADR-0002 §8.3 binds the `RoundEndStep` to the latest `DeathWrite` staged in that slot by `CauseKey`; this ADR guarantees every kill and reversal stages its `DeathWrite` before slot end, including the NPC victim of a strike-limit exile.

### 4. Server-side ownership and seams

**4.1 `UEncounterService`** (`StealthGameplay/Features/Assassination`, `Public/Server/`, world subsystem, server only): implements the Attack follow-on (§2.1), owns the strike-role table (`Ready/Bracing/Countering/Striking/Cooling/Dead`, ASN States) beside the non-replicated `UDefenceStateComponent` (ASN-R18), implements ADR-0002's `IStagedWriteApplier` and is registered by the composition root with `RegisterStagedWriteApplier(DeathWrite)` and `RegisterStagedWriteApplier(CounterPose)` (§3.2, §3.3 run inside `ApplyStagedWrite`), registers the body timers (§6) as `TimerFired` resolvers, and exposes:
- `TOptional<double> GetScheduledDeathServerTime(const AStealthPawn*) const` — the `DeathServerTime` of a pawn whose kill is decided, recorded at §2.1 step 1 (so it is valid inside the `OnPawnKilled` fan-out and the Roster flush, before the write lands); Penalty's `T_due` floor reads it (ADR-0019).
- `bool IsDead(const AStealthPawn*) const` — strike role `Dead` (victim candidates exclude it, F4; ADR-0010's takeover eligibility and the ledger sweep read it).
- `FOnPawnKilled OnPawnKilled` (server-only native multicast, inline in the slot).

**4.2 `IStrikeTallySource`** — consumer-owned interface declared by Assassination, implemented by Penalty, registered weakly by the composition root (ADR-0007): `FStrikeTally GetTallyForResult(FParticipantId Attacker, EStrikeResult Result)` returns `{TallyAfter, bExiled}` after the `OnPawnKilled` fan-out has run in the same slot (so the strike, the tally and an exile are already applied). A missing source is a fault and yields `{0, false}` with a fault-sink report. Every `Result` asks it, so `NoContact`/`Reversed` carry the current tally, unchanged. Two conditions make the read sound (TD-ADR concern 4): (i) Penalty's strike, tally and exile follow-ons run **inline** in the `OnPawnKilled` fan-out (depth ≤ 3 from `Resolve`, well under `QueueCascadeDepthMax` 8); if a `CascadeOverflow` ever moved them out of the slot, Assassination defers building `FStrikeResult` into the same overflow item (carrying the slot's `RootCauseKey`), so the result never reads a stale tally, and the case is logged as a defect; (ii) `bExiled` is Penalty's own in-slot exile decision, never read from Roster state (Roster events dispatch only in `FlushPending`, after `Resolve`).

### 5. Client presentation

**5.1 One idempotent handler.** `OnRep_ActionState`, `OnRep_DeathState`, a relevancy re-open (`PostNetInit`), C-TeamView changes and the per-frame tick of a pawn with a scheduled clip all call **one** state-derived `FEncounterPresentation::Refresh(AStealthPawn&, double Now = EstimatedServerNow())`; no edge-triggered logic. Precedence: a valid `FDeathState` (≠ −1) wins over any `FActionState` pose (except Concealment's hidden-body latch, ADR-0017, which may hide it).
- **Strike:** set the local warp target `"StrikeContact"` from `P_attacker_local + WarpPoint` with `AddOrUpdateWarpTargetFromLocation` (E4) on every machine, never from the server's replicated warp-target array (§7.2); play the `Variant` clip so its contact frame lands at `ContactServerTime`; if late, seek and cross-fade ≤ 0.1 s; never snap.
- **Counter:** starts at `ContactServerTime` (never on receipt, even when the write arrives up to `StrikeNetLeadSec` early); the defender holds `Brace_Hold` until then.
- **Fall:** the one `Death_Fall` montage from `DeathServerTime` (ADR-0013), seeked when late; collision off pawn channels client-locally at `DeathServerTime + DeathFallSec` (matches §6.1 on the server); body hidden by VA-ASN-2's occlusion rule from `DeathServerTime + DeathFallSec + BodyLingerSec`; an actor first received after the linger is never shown.
- **Unsynced clock** (`IsServerClockSynced()` false, first ~5 s after travel): present on receipt (never earlier than receipt), logged.
- **Listen host:** no RepNotify for its own writes (E7); `WriteActionState` and the death-write apply call the same `Refresh` directly; on the host `EstimatedServerNow == ServerNow`.

**5.2 Own-role gating: `OnRoleChangePresented`.** A client-local native multicast `FOnRoleChangePresented(ERoleChangeReason {Assassinated, ReversedAgainst, InformantKilled}, double LocalTime)` on the owning client's `UEncounterViewSubsystem` (`Public/View`, `LocalPlayer`-scoped), fired by `Refresh` for the **locally possessed** pawn exactly once per `FDeathState` at `DeathServerTime` (or on arrival when the state arrives late). The reason comes from the owner's C-TeamView own `State` (Informant → `Assassinated`/`ReversedAgainst` per the transition the owner part records; already Informant → `InformantKilled`). HUD (`PresentedOwnState`, HUD-R6a), Camera (`Dying`/`ViewRelease`, ADR-0015), input-context swaps (ADR-0011 `IMC_Informant`) and owner audio subscribe to it; none may subscribe to the arrival of the Roster state. Outcome-caused owner data that arrives earlier (own `State`, `FHoldStatus` cancels, `NotNow` replies, `UDefenceOwnerComponent` changes) is buffered by its consumer and released at this dispatch. The exile (`StrikeLimit`) has no own death; its presentation clock is HUD's card-entrance event (PEN-R16, HUD-R9; ADR-0021), not this dispatch.

**5.3 Owner results: the positional matcher (MVP rule, no `ContactServerTime` in the result).** `FOwnResultMatcher` (`Public/View`, client-local, one per local player):
- On sending an `Attack` (and on a `Prepare(Begin)` for the defence side), capture the **pawn handle** being commanded (not "the currently possessed pawn": an exile releases it) and the last consumed `(Seq, ContactServerTime)` for that handle.
- A result `r` (`FStrikeResult`, or `FDefenceResult` for the defender) is `Pending` until the **first** `FActionState` observation on the captured handle with `Pose = Strike` (defender: `Counter`), `Seq ≠ last consumed Seq` and `ContactServerTime > last consumed contact time`; that write is consumed and gives `t_c`. Order of arrival of `r` and the write does not matter (E6).
- Show time (HUD F2): `t_show = min(max(t_c,local, t_arrival), t_arrival + HudResultPlateMaxHoldBackSec)`; if no matching write by the cap, show at the cap (lost-data fallback, still at or after contact by I-HUD-1). Late processing never drops an unshown, non-duplicate result in live play; duplicates are dropped by `(PossessionEpoch, Seq)` order and a last-8 ring.
- **Uniqueness:** at most one strike write per pawn is in flight, because `AttackAttemptLockoutSec` (hard min 1.2 s) ≥ the longest execution-to-contact delay (≤ 1.2 s by I-ASN-3); a strict-inequality warning is requested of TDC (Related).
- **Adopted 2026-10-25 (user decision, OQ15):** for the attacker `t_c` comes from `FStrikeResult.ContactServerTime` itself; the positional matcher above remains for the defender's `FDefenceResult` and as the attacker-side fallback.

**5.4 Tally and exile.** HUD applies `TallyAfter` at the plate's `t_show` (HUD-R7), and raises the exile card at `t_show + ExileBeatSec` when `bExiled` (HUD-R9, PEN-R16). No other path presents either.

### 6. The body sequence on the server (one for every victim)

**6.1 Body release at `DeathServerTime + DeathFallSec`** (possession stays put through the fall, ASN-R22): a due-time-heap item `TimerFired(BodyRelease, PawnHandle, Epoch)` scheduled in the kill slot. Its resolver (a) turns the body's collision off pawn channels (the body blocks no pawn after the fall, ASN-R17) and (b) commits `UStealthPawnFactory::ReleaseBody(Pawn)` (ADR-0010 amendment, Related): the hand-over pass detaches the body from its controller kind-blindly — a crowd controller is destroyed, a PlayerController is left pawnless in its normal playing state (never `Spectating`, ADR-0001), so the informant's later takeover (ADR-0019) never unpossesses a body and the engine's possessed-pawn destroy path (E9: `NAME_Inactive` for PlayerControllers, destroy for crowd controllers) never runs. Because `APlayerController::OnUnPossess` calls `SetViewTarget(this)` (E9), `AStealthPlayerController::OnUnPossess` must keep the current view target when the unpossess comes from `ReleaseBody`, leaving Camera's `Dying` → `NoPawn` fade (ADR-0015, CS-R12) in control of the view; coordination row under Related. Controller, `Owner` and `PlayerState` are `COND_OwnerOnly`, so no non-owner sees the change.
**6.2 Body removal at `DeathServerTime + DeathFallSec + BodyLingerSec + BodyRemovalGraceSec`:** `TimerFired(BodyRemoval, PawnHandle, Epoch)` → (a) `IBodyRemovalObserver::OnBodyRemovalCommitted(Body)` on the registered observer, then (b) `RequestTeardown(Pawn)` (ADR-0002 §7), both in the item's slot. `IBodyRemovalObserver` is a consumer-owned interface declared here (`Features/Assassination`, `Public/Server`), implemented by Concealment (`UConcealmentService`, which clears the killed-concealed Owner/Team `bIsConcealed` flags, ADR-0017 §3) and registered weakly by the composition root (ADR-0007 §4). It is called for **every** body, whatever the victim kind or concealment state, so the call itself carries nothing; a missing observer is a fault-sink report, never a crash. Clients have already hidden the body by VA-ASN-2; Roster `ClearBinding` runs at `EndPlay`.
**6.3 After the latch** both items return `Rejected_Latched` (no post-latch exemption is requested): bodies lie through `Ended` and are destroyed by `Reset`/travel; presentation is unaffected because clients derive collision and hiding from `FDeathState` (§5.1). The server keeps such a body's pawn collision on while clients have turned it off; this divergence is **accepted**, because the round-end step freezes every pawn's movement (ADR-0010 §4.5), so no human moves into a body during `Ended` and no correction results. `ETimerKind` gains `BodyRelease` and `BodyRemoval` (ADR-0002 amendment, Related; a single `BodySequence` kind with a stage field is an acceptable alternative for ADR-0002's owner).
**6.4 Same-look replacement beside the body.** The card is vacated in the slot (§2.1 step 4), so Crowd could emerge a same-look replacement while the body still lies. It is bounded by Assassination's I-ASN-5w (warning: `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < CrowdReplacementMinDelaySec`, 9.2 < 10 s at defaults) and by the replacement emerging out of every witness's sight (LSC R5); a preset that violates the warning accepts a possible same-look pair (request to ADR-0012 to state it, Related).

### 7. Uniform network treatment

**7.1** Forced updates: the attacker at the slot (`ActionStateWrite`), every pawn written at the death write (`DeathWrite`, `ActionStateWrite` for the counter), identically for every outcome and victim kind, under ADR-0003's single cap. No dormancy, priority or frequency change; no multicast; no replicated effect actor.
**7.2 Motion warping (E5).** No strike warp target may be set through a replicated path. Every `AStealthPawn`'s motion-warping component must not replicate `WarpTargets`: `UStealthMotionWarpingComponent` (subclass on the one pawn class) calls `Super::GetLifetimeReplicatedProps` and then `DISABLE_REPLICATED_PROPERTY_FAST(UMotionWarpingComponent, WarpTargets)` (the member is `protected`, so the non-FAST macro's name check does not compile from a subclass), identically for every pawn; warp targets are set locally on every machine from replicated state (§5.1). The project has no C++ use of motion warping today; whether the Game Animation Sample's Blueprint traversal needs server-set replicated targets is [U] and belongs to ADR-0010's traversal spike — if it does, traversal moves to the same local-from-`TraversalState` pattern (coordination request, Related). The reflection audit (ADR-0003 allowlist) must show the `WarpTargets` row as `COND_Never`.

### Architecture Diagram

```
 SERVER (listen host)                                   WIRE                          CLIENTS
 Attack slot k (TG_PostPhysics drain)
  1 resolve victim/outcome (ASN-R8,R10)
  2 WriteActionState(A,{Strike,Seq,Var,Ts,Tc,Warp}) ──► FActionState(A) [All, 208 b] ─► Refresh: strike clip,
  3 Roster transition   4 VacateCard (slot)              forced update                 contact at Tc, local warp
  5 Concealment kill hook                                                                (outcome-blind, every kind)
  6 OnPawnKilled ─► Penalty (strike, tally, exile), Crowd, Round
  7 StageWrite(DeathWrite[, CounterPose]) @ Tc − λ
  8 FStrikeResult / FDefenceResult ─► reply batch ─────► ClientCommandReplies [Owner] ─► FOwnResultMatcher:
  9 Event Log; ScheduleAt(BodyRelease, BodyRemoval)       (reliable, end of drain)       Pending until matching
                                                                                          FActionState → t_show
 Staging pass @ Tc − λ (one server step)
  DeathWrite: FDeathState{Tc}, Disabled, re-open ──────► FDeathState(V) [All, 64 b] ──► Refresh: fall from Tc;
  CounterPose: WriteActionState(D,{Counter,Tc}) ───────► FActionState(D) ─────────────► counter at Tc
  [RoundEndStep bound by CauseKey] ────────────────────► FRoundPublicState (ADR-0002)    own pawn: OnRoleChange-
                                                                                          Presented at Tc → HUD,
 Due-time heap                                                                            Camera, input, audio
  Tc + DeathFallSec: BodyRelease (collision, ReleaseBody)                                 (held data released)
  … + BodyLingerSec + BodyRemovalGraceSec: BodyRemoval → RequestTeardown
```

### Key Interfaces

```cpp
// ---- StealthFoundation Public/View: wire and view types ----------------------------------
UENUM() enum class EActionPose : uint8 { None, Strike, Brace, Counter, Interact, BushEnter, BushExit }; // append-only (ADR-0011)
UENUM() enum class EStrikeResult : uint8 { RivalEliminated, WrongTarget, NoContact, Reversed };
UENUM() enum class EDefenceResult : uint8 { Reversed };
namespace StrikeVariant { constexpr uint8 EntryMask = 0x3; constexpr uint8 BushPreface = 0x4; } // 0 F, 1 L60, 2 R60

USTRUCT() struct FFixedWarpOffset { GENERATED_BODY()
    UPROPERTY() int16 X = 0; UPROPERTY() int16 Y = 0; UPROPERTY() int16 Z = 0; };   // 0.1 cm, world axes

USTRUCT() struct FActionState { GENERATED_BODY()
    UPROPERTY() EActionPose Pose = EActionPose::None; UPROPERTY() uint16 Seq = 1;
    UPROPERTY() uint8 Variant = 0; UPROPERTY() double StartServerTime = -1.0;
    UPROPERTY() double ContactServerTime = -1.0;
    UPROPERTY() FFixedWarpOffset WarpPoint;
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);  // always 208 bits
    bool Identical(const FActionState* Other, uint32 PortFlags) const; };
template<> struct TStructOpsTypeTraits<FActionState> : TStructOpsTypeTraitsBase2<FActionState>
{ enum { WithNetSerializer = true, WithIdentical = true }; };

USTRUCT() struct FDeathState { GENERATED_BODY()
    UPROPERTY() double DeathServerTime = -1.0;
    UPROPERTY() bool bDiedConcealed = false; };   // always 65 bits (user decision 2026-10-25)

USTRUCT() struct FStrikeResult { GENERATED_BODY()          // entry of ClientCommandReplies (ADR-0011)
    UPROPERTY() FClientCommandSeq ClientCommandSeq; UPROPERTY() EStrikeResult Result = EStrikeResult::NoContact;
    UPROPERTY() uint8 TallyAfter = 0; UPROPERTY() bool bExiled = false;
    UPROPERTY() double ContactServerTime = -1.0; };   // user decision 2026-10-25 (HUD OQ15)
USTRUCT() struct FDefenceResult { GENERATED_BODY() UPROPERTY() EDefenceResult Result = EDefenceResult::Reversed; };

UCLASS() class UDefenceOwnerComponent : public UActorComponent   // on AStealthPlayerController, owner-only
{ GENERATED_BODY() UPROPERTY(Replicated) double CooldownEndServerTime = -1.0; };

// Client-local presentation (Public/View)
struct FEncounterPresentation { static void Refresh(AStealthPawn& Pawn, double Now); };   // idempotent
UENUM() enum class ERoleChangeReason : uint8 { Assassinated, ReversedAgainst, InformantKilled };
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnRoleChangePresented, ERoleChangeReason, double /*LocalTime*/);
class FOwnResultMatcher {                       // one per local player
public:
    void OnCommandSent(ECommandKind Kind, FClientCommandSeq Seq, const AStealthPawn* CommandedPawn);
    void OnStrikeResult(const FStrikeResult& R, double ArrivalLocal);
    void OnDefenceResult(const FDefenceResult& R, double ArrivalLocal);
    void OnActionStateObserved(const AStealthPawn* Pawn, const FActionState& S);
    bool PopDue(double LocalNow, FResolvedOwnResult& Out);   // HUD pulls plates at t_show
};

// ---- StealthGameplay Features/Assassination Public/Server ---------------------------------
DECLARE_MULTICAST_DELEGATE_FourParams(FOnPawnKilled, AStealthPawn* /*Victim*/, AStealthPawn* /*Killer*/,
                                      EVictimKind, EKillCause);                       // inline in the slot
struct FStrikeTally { uint8 TallyAfter = 0; bool bExiled = false; };
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UStrikeTallySource : public UInterface { GENERATED_BODY() };
class IStrikeTallySource { GENERATED_BODY() public:            // implemented by Penalty (ADR-0019)
    virtual FStrikeTally GetTallyForResult(FParticipantId Attacker, EStrikeResult Result) = 0; };
// FDeathWritePayload / FCounterPayload / FPawnHandle: declared by ADR-0002 §2a in StealthFoundation
// Public/Server/StealthQueuePayloads.h (this ADR owns their meaning, times and the applier).
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UBodyRemovalObserver : public UInterface { GENERATED_BODY() };
class IBodyRemovalObserver { GENERATED_BODY() public:          // implemented by Concealment (ADR-0017)
    virtual void OnBodyRemovalCommitted(AStealthPawn* Body) = 0; };  // every body, in the BodyRemoval slot (§6.2)
UCLASS(NotBlueprintType, NotBlueprintable) class UEncounterService : public UWorldSubsystem,
    public IStagedWriteApplier /* ADR-0002: DeathWrite, CounterPose */ {
public:
    FOnPawnKilled OnPawnKilled;
    TOptional<double> GetScheduledDeathServerTime(const AStealthPawn* Pawn) const;  // valid from the kill slot
    bool IsDead(const AStealthPawn* Pawn) const;
    void SetStrikeTallySource(TWeakInterfacePtr<IStrikeTallySource>);              // composition root
    void SetBodyRemovalObserver(TWeakInterfacePtr<IBodyRemovalObserver>);          // composition root
    virtual void ApplyStagedWrite(const FStagedWrite& Write) override;             // §3.2 / §3.3, apply-only
};
// ETimerKind BodyRelease, BodyRemoval: in ADR-0002 (owner Assassination; FTimerPayload Subject = pawn).
// UStealthPawnFactory::ReleaseBody(AStealthPawn*): in ADR-0010 (hand-over pass, apply-only).
```

### Implementation Guidelines

- `FActionState` must serialise to exactly 208 bits and `FDeathState` to exactly 65 bits for every value; there must never be an optional, conditional or magnitude-packed field in either.
- `FActionState.Seq` must advance by one on every write of that pawn and must never carry, derive from or correlate with a `ClientCommandSeq`.
- At the Attack's slot only the attacker's `FActionState` (and its movement/root-motion state) may change on the wire; nothing on the victim or a defender may change before the death write.
- `WarpPoint` must be derived only from an honoured `PawnRef`'s extrapolation or from the submitted `WorldPoint` (clamped); it must never read a culled or concealed pawn's position.
- Every outcome-revealing write (`FDeathState`, `Disabled`, re-open, `Counter`) must be a typed staged write at `ContactServerTime − StrikeNetLeadSec`; it must decide nothing new and must run after the latch.
- `VacateCard`, Roster transitions, strike accounting and holds' server cancels must happen in the slot, never in the staging pass.
- An NPC victim must be driven exactly as if the strike had whiffed until its death write.
- `FStrikeResult` must be sent for every executed Attack and `FDefenceResult` for every reversal by a bound defender, as entries of the ADR-0011 reply batch, with the same shape for every result; no other outcome field may reach any connection.
- Client presentation must go through one idempotent `Refresh(EstimatedServerNow())` called from every RepNotify, re-open and the host's direct path; it must never present an outcome on receipt, never snap, and never use arrival time except as the matcher's floor and lost-data cap.
- Own-role presentation (HUD, Camera, input contexts, owner audio) must subscribe only to `OnRoleChangePresented`; it must never react to the arrival of the Roster's owner state.
- Result plates must be matched to their strike positionally on the pawn handle captured at the press; for the attacker the result's own `ContactServerTime` is used first; `FActionState.StartServerTime` and `FDeathState.bDiedConcealed` must be filled in every write as §1 specifies (user decisions 2026-10-25).
- No code may set a strike or counter warp target through `UMotionWarpingComponent`'s replicated `WarpTargets`; the pawn's motion-warping component must not replicate it.
- Bodies must be released from their controller at `DeathServerTime + DeathFallSec` and torn down after the linger and grace through the queue, never by an engine timer, and identically for every victim kind.

## Alternatives Considered

### Alternative 1: Replicate the outcome at the slot and delay presentation on the client
- **Description**: Write `FDeathState`, `Disabled` and the counter in the slot; clients hold the presentation until `ContactServerTime`.
- **Pros**: One write, no staged-write queue, simpler server.
- **Cons**: Every bystander receives kill/whiff/reversal `T_v` (0.40-0.75 s) before contact; a modified client reads it, and the victim's frozen movement is visible at once on honest clients because `Disabled` is a movement state (ASN-R16 fix). The bystander residual would grow from `StrikeNetLeadSec` (0.15 s) to `T_v`.
- **Rejection Reason**: Violates ASN-R21's bound and the IVP L10 residual; the lead-write keeps the leak at λ.

### Alternative 2: A multicast "strike event" RPC on the pawn (or a replicated effect actor)
- **Description**: The server multicasts `{outcome, times}` to relevant clients at the death write.
- **Pros**: Familiar UE pattern; one message carries everything.
- **Cons**: Multicast RPCs are banned on pawns (SCL R13, ADR-0001); unreliable multicast can drop, reliable multicast queues behind other traffic; relevancy and late joiners miss it (no state to rebuild from); an effect actor is a new replicated actor (IVP L15) whose spawn itself signals a kill.
- **Rejection Reason**: Event-shaped data cannot be re-derived on re-open (S10) and breaks the "state, not events" rule.

### Alternative 3: Carry `ClientCommandSeq` in `FActionState` so the HUD can match results by key
- **Description**: Put the owner's command sequence into the strike write.
- **Pros**: Removes the positional inference.
- **Cons**: `FActionState` is `All`; a per-client command counter exposes command cadence that NPC pawns cannot reproduce (P1 tell, ASN-R23).
- **Rejection Reason**: A Pillar-1 leak to fix a presentation convenience.

### Alternative 4: Add `ContactServerTime` to `FStrikeResult` now
- **Description**: The attacker's result carries its own contact time (no new information to the owner).
- **Pros**: Removes the matcher for the attacker and the exile-path fallback.
- **Cons**: A schema change awaiting the user (HUD OQ15, always-ask category).
- **Rejection Reason**: Not this pipeline's decision; reserved as a slot (§1.3, §5.3) so adoption is a one-field amendment.

### Alternative 5: Write the outcome exactly at contact (λ = 0)
- **Description**: Stage the death write at `ContactServerTime`.
- **Pros**: Zero bystander lead.
- **Cons**: Every client receives the write one-way-latency after contact and must seek the fall (and the counter) on every strike, a visible hitch at normal latency.
- **Rejection Reason**: `StrikeNetLeadSec` (0.15 s, ServerOnly, bounded by I-ASN-4) trades a small, bounded bystander residual for on-time presentation.

### Alternative 6: Release and remove bodies with engine timers or by destroying the possessed pawn
- **Description**: `FTimerManager` timers for body removal; let pawn destruction unpossess the controller.
- **Pros**: No queue items.
- **Cons**: Engine timers are banned for gameplay timing (ADR-0002); destroying a possessed pawn runs the engine's `PawnPendingDestroy` path on the PlayerController (state changes outside the parity wrapper of ADR-0010) and differs by controller class.
- **Rejection Reason**: Kind-blind, queue-ordered release and teardown keep one body sequence and one possession path.

## Consequences

### Positive
- One outcome-blind cue at the slot and one server step of outcome writes make kill, whiff and reversal indistinguishable on the wire until λ before contact, with byte-identical sizes.
- Presentation is reproducible from state on any client at any time (late open, re-open after cull, rejoin), because nothing is event-shaped.
- The HUD's plate timing no longer depends on UE delivering an RPC and a property in any particular order.
- Round end, Penalty respawn floors and Crowd replacement all key off the same `ContactServerTime`, so "the end is never shown before the blow" holds by construction.
- The motion-warping replication finding closes an unaudited side channel before any code is written.

### Negative
- Two layers of state (slot and staged) and a client matcher are more machinery than "replicate the outcome".
- The positional matcher is an inference with a lost-data fallback that can show a plate up to `HudResultPlateMaxHoldBackSec` after arrival when the strike write is lost; the `ContactServerTime` field adopted 2026-10-25 removes it for the attacker; it remains for the defender.
- ~~`ContactServerTime` hide-pose overload~~ — removed 2026-10-25 (`StartServerTime` adopted).
- Four amendments to accepted-track ADRs (ADR-0002 timer kinds, ADR-0010 `ReleaseBody` and warp-target replication, ADR-0012 sweep wording, ADR-0016 victim driving) must land before this ADR can be accepted.

## Risks
- **RPC/property ordering under loss (E6)** — Medium likelihood, Low impact: handled by `Pending` + cap; Verification 3 measures both orders under `LeakNetProfile`.
- **Staging lateness** — the death write lands up to one server frame after `T_dw`; at a 30 fps host that eats 33 ms of the 150 ms lead. Mitigation: payloads carry their own server times; clients seek. If the host drops below 30 fps, the lead shrinks but nothing is shown early.
- **Clock unsynced at round start** — first ~5 s after travel present on receipt (never early). Strikes in the first seconds of `Active` are rare (start placement separates teams, LSC V-SEP).
- **Motion-warping traversal dependency (E5)** — if the sample's traversal needs replicated warp targets, disabling them breaks traversal for observers. Mitigation: ADR-0010 coordination before implementation; the traversal spike already gates ADR-0010.
- **Matcher ambiguity at the tuning boundary** (`AttackAttemptLockoutSec` = 1.2 = longest `T_v`) with a lost strike write: a plate could match the next strike's write (shows later, capped). Mitigation: TDC strict-inequality warning requested.
- **Re-open behaviour (S10)** — a re-created actor must initialise from `FActionState`/`FDeathState` in `PostNetInit`; covered by ADR-0003 S10 and Verification 2.
- **Listen-host bypass** — the host reads server state directly; mitigated by routing its HUD/Camera through the same `Refresh` and dispatch, tested by AC-ASN-35's host leg.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| assassination-defence-reversal.md | ASN-R16 strike/warp/contact contract; victim untouched until the death write | §1.1 Variant/Contact/Warp, §2.2 warp derivation, §2.3 nothing on the victim at the slot, §3.4 victim-driving rule |
| assassination-defence-reversal.md | ASN-R17 one body sequence; collision off after the fall; never re-possessed | §5.1 one fall from `FDeathState`; §6 body release and removal for every victim |
| assassination-defence-reversal.md | ASN-R18 owner cooldown end on the PlayerController | §1.5 `UDefenceOwnerComponent` |
| assassination-defence-reversal.md | ASN-R19 one `FStrikeResult` per Attack, same shape; `FDefenceResult` on a reversal | §1.3, §1.4, §2.1 step 8, §4.2 |
| assassination-defence-reversal.md | ASN-R20 no observer UI | §7.1 no multicast, no effect actor; only `FActionState`/`FDeathState` reach bystanders |
| assassination-defence-reversal.md | ASN-R21 outcome-blind cue, death write at `ContactServerTime − StrikeNetLeadSec`, uniform forced updates, idempotent presentation, fixed width | §1, §2, §3, §5.1, §7 |
| assassination-defence-reversal.md | ASN-R22 possession stays put through the fall | §6.1 body release at `DeathServerTime + DeathFallSec` |
| assassination-defence-reversal.md | ASN-R23 presentation gating; positional matching; host routed through the gate | §5.2 `OnRoleChangePresented`, §5.3 matcher, §5.1 host path |
| assassination-defence-reversal.md | F1 reach and warp (warp bound, extrapolation) | §2.2 derivation and clamp |
| hud.md | HUD-R6 result plates at the local contact frame; defender plate from its own Counter write | §5.3 matcher and `t_show`; §3.3 Counter write |
| hud.md | HUD-R6a one presented own state | §5.2 dispatch is the only own-role clock |
| hud.md | HUD-R7 tally moves with the plate | §1.3 `TallyAfter` in the result; §5.4 |
| hud.md | F2 result show time and matching; OQ15 slot | §5.3; reserved field §1.3 |
| npc-kill-penalty-informants.md | PEN-R14 informant-kill invariant (byte-identical plate, tally, cues) | §1.3 same shape for `WrongTarget` whatever the victim kind; §2.1 same writes for every kind; §6 same body sequence |
| npc-kill-penalty-informants.md | PEN-R15 owner data: tally and `bExiled` in `FStrikeResult` | §1.3, §4.2 `IStrikeTallySource` (`T_due` is ADR-0019's) |
| npc-kill-penalty-informants.md | F2 `T_due` floor needs `DeathServerTime` at the slot | §4.1 `GetScheduledDeathServerTime` |

## Performance Implications
- **CPU**: per Attack, one victim resolution (≤ a handful of capsule tests, ASN F1), one staged write or two, two body timers: microseconds inside the 0.5 ms queue/resolver line. Client `Refresh` runs only on RepNotify and while a clip is scheduled.
- **Memory**: per pawn 18 + 8 bytes of replicated state; per round a few dozen staged writes and timers.
- **Load Time**: none.
- **Network**: per strike one forced update of the attacker (≈ 18 bytes of `FActionState` plus header and movement) and one forced update per death-write pawn (8 bytes); owner results ride the existing reply batch (≤ 4 bytes each plus `FClientCommandSeq`). At the maximum strike rate (one per 1.75 s per attacker) this is far below the 36 KB/s pawn line; removing `WarpTargets` replication saves bytes on every warped action.

## Migration Plan
The prototype (`Source/StealthGame`) uses separate pawn bools (`bDying`, `bPreparingDefence`) and `Server*` RPCs on the pawn; ADR-0001's migration list already marks them for replacement by `FActionState` and the command component. Nothing is ported: Assassination is written fresh against this ADR once it and its dependencies are Accepted. The sample pawn's `UMotionWarpingComponent` is replaced by `UStealthMotionWarpingComponent` when the Blueprint is reparented (ADR-0010 migration).

## Validation Criteria
- `StealthGame.Assassination.WireWidth` (unit, on an `FNetBitWriter`): `FActionState` always 208 bits, `FDeathState` 65 bits, across randomised values incl. extremes; `NetSerialize` returns true; `Identical` matches field equality on all six fields.
- `StealthGame.Assassination.LungeOutcomeBlind` (integration): the attacker's `RepRootMotion` and replicated movement up to contact are identical for a hit, a whiff and a reversal with the same `WarpPoint` (no server re-track).
- `StealthGame.Assassination.ByteIdentity` (integration, harness raw-bunch capture): kill, whiff and reversal produce identical `FActionState` bunch sizes on a bystander; `FDeathState` bunches identical for NPC, player and informant victims (AC-ASN-35 support).
- `StealthGame.Assassination.NothingAtSlot` (integration): between the slot and `T_dw`, a bystander receives no change on the victim or defender pawn; the victim's movement continues (human and NPC).
- `StealthGame.Assassination.DeathWriteTiming` (net-emulation): the fall's first frame on every client is within one frame of `DeathServerTime` (seek allowed), never before; the counter likewise.
- `StealthGame.HUD.ResultMatchOrder` (net-emulation, both arrival orders forced): plate shown at `t_show`; with the strike write dropped, at the cap.
- `StealthGame.Assassination.RoleChangeGate` (integration): own HUD, Camera and input context change no earlier than the own `DeathServerTime` on a remote client and on the listen host.
- `StealthGame.Assassination.WarpTargetsNotReplicated` (reflection audit): the `WarpTargets` lifetime row of every `AStealthPawn`'s motion-warping component is `COND_Never`; a strike produces no motion-warping bunch.
- `StealthGame.Assassination.NoKillOnlyCrowdChange` (integration): between the slot and `T_dw`, no NPC (victim or bystander) changes behaviour differently for a kill than for a whiff at the same spot.
- `StealthGame.Assassination.ReopenFastForward` (S10 harness): an actor opened mid-fall joins at the right frame; opened after the linger, never shown.
- `StealthGame.Assassination.BodySequenceParity`: body release and removal times identical (± 1 server frame) for NPC, player and informant victims; no `Spectating` state entered.

## Related
- **Depends on**: ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0008, ADR-0010, ADR-0011, ADR-0012, ADR-0013, ADR-0017.
- **Enables**: ADR-0016 (victim-driving rule), ADR-0019 (death-time floor, body release before takeover, `IStrikeTallySource` implementation), ADR-0021 (HUD timing), ADR-0023 (Event Log).
- **Amendment requests (not applied; owned by other ADRs)**:
  1. **ADR-0002 §2/§4:** add `ETimerKind::BodyRelease` and `ETimerKind::BodyRemoval` (owner Assassination; no post-latch exemption).
  2. **ADR-0010 §3/§4.3:** add `UStealthPawnFactory::ReleaseBody(AStealthPawn*)` (commit in a slot, executed apply-only by the hand-over pass: crowd controller destroyed, PlayerController left pawnless, never `Spectating`); and coordinate traversal with §7.2 (no replicated `WarpTargets` on any pawn; `UStealthMotionWarpingComponent`).
  3. **ADR-0012 §3 ledger sweep:** "every live pawn holds exactly one card" reads "every pawn whose strike role is not `Dead`", because `VacateCard` runs in the kill slot while the body is still present; and state that a preset violating I-ASN-5w accepts a possible same-look replacement beside a body (§6.4). The same "live = strike role not `Dead`" definition applies to ADR-0002 §8.4(f)'s "every live pawn".
  4. **ADR-0016 §5/§6/§8:** an NPC victim keeps its driver, follower and stimuli handling unchanged until the death write's freeze flag; no `OnPawnKilled` side effect (exit cancellation, vacancy, reactions) may be observable before the death write; strike stimuli come only from `OnActionStarted(Attack)`; the handler's `KillServerTime` = `GetScheduledDeathServerTime(Victim)` (§3.4).
  5. **ADR-0004 / TDC (warning, not a new key):** cross-field warning `AttackAttemptLockoutSec > StrikeContactSec + BushStrikeEmergeSec` (strict), protecting the matcher's uniqueness.
  6. **ADR-0015 / ADR-0010 coordination:** `AStealthPlayerController::OnUnPossess` keeps the view target when the unpossess comes from `ReleaseBody` (E9), so Camera's own fade governs the view.
- **GDD sync notes (not applied; GDDs are owned by their authors)**: ASN-R21 "Clients schedule from the GameState server clock" is read as `EstimatedServerNow()` (ADR-0002 §9); ASN-R16 "only the server may re-track the victim's capsule" is not used: the server lunge warps to the fixed `WarpPoint` (§2.2, E8); ASN-R19 `FStrikeResult` travels as an entry of ADR-0011's `ClientCommandReplies` batch rather than a stand-alone RPC; IIF R12's pose list gains no value from this ADR; HUD F2 `ServerNowEst` = `EstimatedServerNow()`.
- **Formerly pending user decisions (all approved 2026-10-25)**: `ContactServerTime` in `FStrikeResult` (HUD OQ15); `FActionState.StartServerTime` and `FDeathState.bDiedConcealed` (Concealment CR-10); `RetireAtServerTime` (Markers OQ3; supplied in §3 step 3).

## Validation gates
- **TD-ADR (technical-director), 2026-10-24: CONCERNS → REVISED.** (1) Card vacated in the slot confirmed correct; same-look replacement beside a body bounded by I-ASN-5w (§6.4, ADR-0012 request). (2) Kill-only crowd reactions before the death write → §3.4 extended to every `OnPawnKilled` subscriber, `KillServerTime` defined, ADR-0016 request widened. (3) Body release coherent; `OnUnPossess` view-target jump handled (§6.1, ADR-0015 coordination); post-latch collision divergence accepted under the round-end freeze (§6.3). (4) `IStrikeTallySource` read conditions (inline fan-out, overflow deferral, `bExiled` from Penalty's own decision) (§4.2). (5) `Seq` owned by `WriteActionState`; "live pawn" defined for ADR-0002 §8.4(f). (6) Pending decisions confirmed reserved only. (7) Status prerequisites aligned with Depends On. (8) `WarpTargets` validation reads `COND_Never`.
- **Engine validation (unreal-specialist lens), 2026-10-24: MINOR NOTES → applied.** E1 return-value and `Identical` semantics; E2 width capped only on the net bit-writer; E5 confirmed incl. dirty-mark sites, `protected` member → `DISABLE_REPLICATED_PROPERTY_FAST`; E8 `RepRootMotion` → server lunge warps to the fixed `WarpPoint` (no outcome-dependent re-track); E9 `PawnPendingDestroy`/`OnUnPossess` behaviour confirms `ReleaseBody`. Traversal's dependence on replicated warp targets stays [U] for ADR-0010's spike.
- **GDD sync check:** checked 3 referenced GDDs (assassination-defence-reversal, hud, npc-kill-penalty-informants). No interface renames; reading notes recorded under Related (GameState clock → `EstimatedServerNow`; `FStrikeResult` as a batch entry; ASN-R16 server re-track not used).

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §2.1 step 1: this ADR confirmed as the consumer of the `BushSpotPick` stream for the bush-spot victim draw — requested by ADR-0017.
- This ADR's own requests (ADR-0002 timer kinds, ADR-0010 `ReleaseBody`/warp targets, ADR-0012 sweep wording, ADR-0016 victim driving, ADR-0015/ADR-0010 view target) are applied in those ADRs; the ADR-0004 cross-field warning is queued in ADR-0004 for the TDC GDD.

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §3.1, §4.1, Key Interfaces: `FDeathWritePayload`, `FCounterPayload` and `FPawnHandle` are no longer declared here; they are ADR-0002 §2a's Foundation `Public/Server` structs (this ADR keeps their meaning, times and the applier). `UEncounterService` implements `IStagedWriteApplier` and is registered for `DeathWrite`/`CounterPose` by the composition root; the unused `bReopen` mention is removed (re-open is the Policy's decision) — architecture-review fix 2026-10-25 (Conflict 1, blocking).
- §6.2, Key Interfaces: the `BodyRemoval` resolver calls the consumer-owned `IBodyRemovalObserver::OnBodyRemovalCommitted` (implemented by Concealment) before `RequestTeardown`, for every body — architecture-review fix 2026-10-25 (Conflict 6).
- Ordering Note: acceptance pair with ADR-0017; this ADR keeps the ADR-0018 → ADR-0017 edge, ADR-0017 drops the reverse edge from its Depends On — architecture-review fix 2026-10-25 (dependency cycle).
- Engine Compatibility, Knowledge Risk: MotionWarping is Beta in 5.8.4; the protected `WarpTargets` disable is on the engine-upgrade checklist — architecture-review fix 2026-10-25 (Engine finding 3).
- `FActionState`: this ADR's Key Interfaces block is the single declaration; ADR-0003 §4 and its Key Interfaces now point here — architecture-review fix 2026-10-25 (Conflict 2).
- Requests answered (2026-10-24): every amendment request under Related (items 1-6) has been applied by its owner (ADR-0002, ADR-0010, ADR-0012, ADR-0016, ADR-0015; item 5 queued in ADR-0004 for the TDC GDD). The Status line's "before the amendment requests listed under Related are answered" condition is therefore met; the Status line itself is not edited (Status changes are reserved for the user) — architecture-review fix 2026-10-25 (Conflict 9).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §1.1, Key Interfaces, guidelines, validation — user decision 2026-10-25: `FActionState.StartServerTime` added (fixed 64-bit; the struct is now 208 bits); `ContactServerTime` loses its hide-pose overload.
- §1.2, §3 step 5, Key Interfaces — user decision 2026-10-25: `FDeathState.bDiedConcealed` added (1 bit; 65 bits); a killed-concealed victim re-opens at the death write (ADR-0003 §5.7, ADR-0017 §8 B).
- §1.3, §5.3, Key Interfaces — user decision 2026-10-25: `FStrikeResult.ContactServerTime` added (Owner-only); HUD F2 reads it first; the positional matcher remains for the defender and as fallback (HUD OQ15 closed).
- §1.3 — user decision 2026-10-25: `TallyAfter`/`bExiled` no longer provisional (Penalty OQ-NKP-1 confirmed).
- §3 step 3 — user decision 2026-10-25: the binding retirement passes the strike's `ContactServerTime` as the Team-audience `RetireAtServerTime` (Roster R6, Markers CR-13).
- §5.2 — user decision 2026-10-25: the `InformantKilled` role-change reason is confirmed (Penalty CR-18 / OQ-NKP-12).

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §3.2 step 2: the death write never interrupts the lunge; the movement freeze flag for a pawn in an in-progress `Strike` lunge (the reversal's attacker) takes effect at `ContactServerTime`, while `FDeathState` and visibility change at `T_dw` (ASN re-review xdoc: SCL + ADR-0018 confirm; the replication specialist re-verifies AC-ASN-05, 20, 21, 44, 45 against this).
- §5.3 (closeout D8): `HudResultPlateMaxHoldBackSec` and `HudOwnRemovalHoldCapSec` have a **hard minimum of 1.25 s** in the TDC (ADR-0004 validates it), so the lost-data fallback in the show-time formula can never fire below the strike lead; `result_show_time` is `ContactServerTime`-based (HUD re-review xdoc).
