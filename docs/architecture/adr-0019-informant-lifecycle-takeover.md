# ADR-0019: Informant lifecycle and crowd-pawn takeover

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It cannot be Accepted before ADR-0001 (including the user's decision on the `T_due` carrier, §6 of that ADR), ADR-0002, ADR-0010, ADR-0012, ADR-0014, ADR-0016 and ADR-0018 are Accepted, and before the amendment requests listed under Related are answered by their owners.

## Date
2026-10-24

## Last Verified
2026-10-24 (design against npc-kill-penalty-informants.md, level-socket-contract.md R5b, crowd-npc-behaviour.md CNB-R11, character-pool-appearance.md CP-12b/CP-13 and round-match-framework.md R17/R18 in their "reconciled 2026-10-24" state; no new engine API is introduced, engine facts are inherited from ADR-0002/0010/0014)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full); technical-director (TD-ADR) and unreal-specialist validation (gate record at the end); acceptance reserved for the user. Orchestrator decision C1 (informant respawn = takeover of an unseen crowd pawn near the team spawn) is the design input; it is itself listed in active.md for user confirmation.

## Summary
An eliminated player stays in the match as an informant who must periodically receive a body, without any client being able to tell, from the wire or the screen, that a citizen has just become a person. This ADR fixes the server-side informant lifecycle owned by NPC-Kill Penalty & Informants: the per-participant store and its states (Active, Awaiting, Picking, Embodied, Absent, RoundEnded), the respawn as a due-time-heap item at `T_due`, the one-call-stack commit `PickRespawnPawn → TryReleaseNpc → TakeOverCrowdPawn → TakeNpcCard`, retries and widening, the exile hand-off of the surviving body to the crowd, the strike/tally store that feeds `FStrikeResult`, and the informant restriction enforcement map; it consumes ADR-0001's open `T_due` carrier slot without choosing a schema.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4) |
| **Domain** | Core (gameplay state and server scheduling); Networking only through inherited carriers |
| **Layer** | Feature (NPC-Kill Penalty & Informants) |
| **Knowledge Risk** | MEDIUM — no engine API is used directly; the takeover inherits ADR-0010's HIGH-risk possession parity (spikes S1, S14, H1) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/networking.md`, `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Module Ownership: Penalty & Informants; Data Flow §1; API Boundaries: Core/Feature; Required ADRs row ADR-019; AQ-02); ADR-0001 §5-§6, ADR-0002 §4-§8, ADR-0010 §3.2/§4.3-§4.5, ADR-0012 §2-§3, ADR-0014 §7.3, ADR-0016 §7-§8, ADR-0017 (`IsInBushOrTransition`), ADR-0018 |
| **Post-Cutoff APIs Used** | None. All engine-facing work goes through `UResolutionQueue` (ADR-0002), `UStealthPawnFactory` (ADR-0010), `ULayoutService` (ADR-0014) and `UAppearanceAllocator` (ADR-0012) |
| **Verification Required** | (1) takeover invisibility: a non-team client's raw-bunch capture over a takeover shows no change on the taken pawn beyond its ordinary cadence (no new actor, NetGUID, `AppearanceId`, net-config, velocity or `HeadLook` change; inherits S1/S14 and ADR-0010 H1); (2) the owner's first `ServerMove` after an AI → player hand-over corrects below `PawnSmoothSnapThresholdCm` (ADR-0010 H1); (3) the full lifecycle under `LeakNetProfile` with disconnect/reclaim at every state; (4) `PickRespawnPawn` cost under the 1-pick-per-frame guard with 10 connections and 40 NPCs (ADR-0014 budget) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (carrier model; **`T_due` carrier slot, pending user decision**), ADR-0002 (`ScheduleAt`, `ETimerKind::InformantRespawn`, resolvers, epoch tokens, latch, Roster flush barrier), ADR-0004 (Penalty/LSC/Round keys), ADR-0006 (secret streams consumed through ADR-0014), ADR-0010 (`TakeOverCrowdPawn`, `RequestHandOver`, hand-over pass, `ReleaseBody` requested by ADR-0018), ADR-0011 (informant legality, reply sink), ADR-0012 (`TakeNpcCard`, `ReleaseCard`, ledger pairing), ADR-0014 (`PickRespawnPawn`, widening, `RespawnPick` stream), ADR-0016 (`ITakeoverEligibilitySource::TryReleaseNpc`, `ICrowdControl::AdoptReleasedPawn`), ADR-0017 (`IsInBushOrTransition` inside Crowd eligibility), ADR-0018 (`OnPawnKilled`, `GetScheduledDeathServerTime`, `IStrikeTallySource`, body release before takeover). All must be Accepted before implementation. |
| **Enables** | ADR-0021 (HUD tally, exile card and returning countdown read Penalty's owner data), ADR-0023 (Penalty Event Log rows), Informant Pings (VS, system 20: embodied/awaiting states) |
| **Blocks** | NPC-Kill Penalty & Informants epic (strike count, tally, exile, informant respawn, returning informant); the HUD returning-countdown story (until the `T_due` carrier is decided, see §6) |
| **Ordering Note** | Should-have before Penalty is built (architecture.md). The `T_due` carrier is consumed as a slot: Penalty computes `T_due` regardless; the carrier adapter (§6) is implemented only after the user decides ADR-0001 §6 and ADR-0001 is amended. Penalty OQ-NKP-1 (tally vs live strike count) and CR-19 (C-TeamView tally re-push) are pending user decisions; this ADR implements the tally as written and reserves the re-push. |

## Context

### Problem Statement
Three locked rules meet here: an eliminated player becomes an informant who stays in the match; a killed informant "respawns at their team's spawn"; and an enemy must never be able to tell a player from an NPC. A fresh pawn appearing at a team spawn mid-round would be both a network tell (new actor, new NetGUID) and a location tell, so decided rule C1 reads the respawn as **taking over an existing, unseen crowd pawn inside the team's respawn region**. Doing that safely spans seven systems (Roster binding, Round's queue and re-admission, LSC's pick, Crowd's release, Character Pool's card ledger, the pawn factory's possession hand-over, and Penalty's own timers), each already given a mechanism by its ADR. What is missing is the owner of the **sequence**: which calls run in which order inside which queue slot, what happens when any step fails, how retries and widening are scheduled, how disconnects and reclaims move the participant between states, and how the countdown end `T_due` reaches only its owner without the user's pending carrier decision being pre-empted.

### Constraints
- **Pillar 1:** the takeover must change nothing any non-owner can observe (IVP R9c, R14; SCL R2a.2; CP-10); no pawn is spawned mid-round for a participant (Roster R9).
- **One order:** every step runs inside queue slots; timers are due-time-heap items (ADR-0002); possession changes are committed in a slot and executed by the hand-over pass (ADR-0010).
- **One random draw:** `PickRespawnPawn` is the only draw; `TakeNpcCard` is a card move (Penalty CR-6, ADR-0012 guideline 5).
- **Ordering constraints from accepted-track ADRs:** `TakeOverCrowdPawn` owns the eligibility check and `BindPawn`, and no card move may be committed unless it returned `Applied` (ADR-0010 §3.2); `TakeNpcCard` runs in the same call stack as its Roster counterpart (ADR-0012).
- **Pending user decisions (always-ask):** the `T_due` carrier (ADR-0001 §6, AQ-02); the tally re-push fields (Penalty CR-19); the tally itself vs a live count (OQ-NKP-1); `NpcKillStrikeLimit` hard minimum (OQ-NKP-11). None is decided here.
- **Layering:** Penalty (Feature) may call Core and Foundation services; Round (Core) reaches Penalty only through a consumer-owned interface it declares (architecture TD concern 1).

### Requirements
- Strike count (ServerOnly) and WrongTarget tally (Owner) per participant, round-scoped, keyed by `ParticipantId`; `StrikeCount ≤ WrongTargetTally` (PEN-R1-R3, R6, F1).
- Exile on the `L`-th ungraced NPC kill: transition, surviving body released to the crowd in the same call stack, `bExiled` in the killer's result, respawn scheduled unless latched (PEN-R4, R5).
- One respawn path for every cause; `T_due = max(t_lost + D, t_floor, t_now)` (PEN-R7, R8, F2); disconnected victim → Absent, keeps `t_lost`, no timer (PEN-R8, cross-review S-B3).
- The commit sequence in one server call stack with re-validation, retries every `EmergenceRetrySec`, LSC widening, never relaxing the unseen test (PEN-R9, R10; LSC R5b).
- Disconnect, reclaim and lost-body rules (PEN-R11; Round R17.5, R17.9, R18).
- Owner-only data: tally and `bExiled` via `FStrikeResult` (ADR-0018), `T_due` via the pending carrier (PEN-R15).
- The informant restriction contract enforced by its named owners (PEN-R12, R13).
- Event Log rows (PEN-R17).

## Decision

### 1. Ownership and placement
`UPenaltyService` (`StealthGameplay/Features/Penalty`, `Public/Server/`, world subsystem, server only, `NotBlueprintType`) owns a plain-C++ core `FPenaltyCore` (headless-testable, ADR-0007) holding one `FPenaltyRecord` per `ParticipantId`:

| Field | Audience | Meaning |
|---|---|---|
| `StrikeCount` (uint8) | ServerOnly | PEN-R2, F1 `k` |
| `WrongTargetTally` (uint8, saturating 255) | Owner (only through `FStrikeResult.TallyAfter`) | PEN-R3, F1 `t` |
| `LifeState` | ServerOnly | `Active`, `Awaiting`, `Picking`, `Embodied`, `Absent`, `RoundEnded` (§3) |
| `LostServerTime` (`t_lost`), `FloorServerTime` (`t_floor`) | ServerOnly | F2 inputs |
| `DueServerTime` (`T_due`) | Owner (carrier pending, §6) | F2 output |
| `RespawnRequestSeq` (uint32) | ServerOnly | one per respawn request; all retries reuse it (§4.3) |
| `PickStartServerTime`, `LastWideningLevel` | ServerOnly | retry bookkeeping and logging (§4.3) |
| `TimerHandle` | ServerOnly | the pending `InformantRespawn` item |
| `ExcludePawn` (`FPawnHandle`, ADR-0002 §2a) | ServerOnly | the participant's own released exile body (LSC R5b.1, PEN-R5) |

plus a round-scoped `ReleasedBodies` map `FPawnHandle → ReleaseServerTime` for the stale-read grace (PEN-R5). The store is cleared on `OnRosterReset` (new `RoundEpoch`) and frozen on `OnRosterRoundEnded`. Nothing in it is a replicated property.

Seams (all registered weakly from the composition root, ADR-0007):
- Penalty **implements** ADR-0018's `IStrikeTallySource` (tally and `bExiled` into `FStrikeResult`).
- Penalty **subscribes** to `UEncounterService::OnPawnKilled` (ADR-0018) and to the Roster's connection events, `OnRosterRoundEnded` and `OnRosterReset`. It does **not** subscribe to `OnParticipantStateChanged` for scheduling (delegate order inside the flush is unspecified, TD-ADR concern 4).
- Penalty **registers** the `TimerFired(InformantRespawn)` resolver with the queue (ADR-0002 §5).
- Round (Core) **declares** `IInformantBodyProvider` (Core `Public/Server/`) and Penalty implements it: `OnInformantAwaitingBody(P, Cause, t_lost)` — called by Round's own handler of an `Active → Informant` transition (`Assassinated`, `ReversedAgainst`, `StrikeLimit`) **after** its elimination check and only if the round did not latch, so no respawn is ever scheduled (and no countdown ever published) inside a deciding slot (ADR-0002 §8.2); `OnInformantReadmitted(P)` (Round R18, called only after the returner's Session acknowledgement, when its PlayerController is ready); `OnInformantPawnLost(P)` (Round R17.9 informant case, Penalty CR-8). Round never names a Penalty type and reports to the fault sink if no provider is registered when it needs one.

### 2. Strikes, tally and exile (inside the Attack's slot)
**2.1 `OnPawnKilled(Victim, Killer, VictimKind, Cause)`** (inline in the slot, ADR-0018 §2.1 step 6):
- If `VictimKind = Informant`: Roster `RetireBinding(Victim)` inline (Roster R9 "not a transition"), so a second strike on the falling body resolves `AlreadyResolved`; then schedule the victim participant's respawn (§3.2, cause `InformantKilled`).
- Strike rule (F1): resolve the killer with `GetParticipant(Killer)`; `k' = k + [Cause = Strike ∧ VictimKind = Npc ∧ State(killer) = Active ∧ ¬Graced(Victim)]` where `Graced` = `ReleasedBodies[Victim]` exists and `slot time − ReleaseServerTime ≤ StrikeOutReleaseGraceSec` (inclusive). The tally is **not** changed here (it follows the result, §2.2). Log `StrikeCounted`.
- Exile when `k' ≥ L ∧ State = Active` (§2.3).
- A `Player` victim (Assassinated / ReversedAgainst) is handled by the Roster state event that the same slot already raised (§3.2).

**2.2 `GetTallyForResult(Attacker, Result)`** (called by Assassination after the fan-out, ADR-0018 §4.2): if `Result = WrongTarget`, `t := min(t + 1, 255)`; return `{TallyAfter = t, bExiled = (this slot exiled Attacker)}`. Every other result returns the current `t` and `bExiled = false`. The invariant `StrikeCount ≤ WrongTargetTally` is asserted after each update (development builds fault; Shipping logs).

**2.3 Exile sequence** (one call stack, still in the slot, depth-first after the strike write, PEN-R4):
1. `Roster.RequestTransition(P, Informant, StrikeLimit, ExpectedEpoch)`; the Roster performs `ReleaseToCrowd(Pawn)` inside the transition (Roster R8/R9, Penalty CR-3), so the pawn is `Unbound` from this call on.
2. `UAppearanceAllocator::ReleaseCard(Pawn)` (card `BoundLive → NpcLive`, same `AppearanceId`; ADR-0012 pairing rule).
3. `ICrowdControl::AdoptReleasedPawn(Pawn, {RestrictedUntilServerTime = the pawn's strike lockout end, bExitAtNextScheduledSlot = true})` (ADR-0016; Crowd CR-4: the prompt exit takes the next scheduled city-wide exit slot).
4. `UStealthPawnFactory::RequestHandOver(Pawn, Ai, ExpectedEpoch)` (ADR-0010 §4.3). The commit and the control-epoch bump happen now; the possession itself must wait until the pawn's strike lockout (a root-motion pose) ends, as a traversal does (amendment request to ADR-0010: extend "never during a traversal" to root-motion action poses). The owner's Camera holds its view on the released body until the HUD card entrance (PEN-R16, ADR-0015); the PlayerController's unpossess must not cut the view (§4.4).
5. `ReleasedBodies[Pawn] = slot time`; `ExcludePawn = Pawn`; `bExiled` for this slot's result.
6. The respawn is **not** scheduled in this call stack: the Roster event of step 1 is dispatched in the slot's Roster flush after `Resolve` returns (ADR-0002 §5), where Round's elimination check runs; if the round did not latch, Round calls `IInformantBodyProvider::OnInformantAwaitingBody(P, StrikeLimit, slot time)` and Penalty schedules (§3.2, `t_floor = t_lost`: the body does not fall). After a latch Round makes no call, so no respawn is ever scheduled (PEN-R4 step 4).
7. Event Log `StrikeLimit` (Round's row, written with the transition); `RespawnScheduled` when §3.2 runs.

### 3. The lifecycle
**3.1 States.** `Active(k)` (competing); `Awaiting` (Informant, no pawn, `InformantRespawn` item pending); `Picking(W)` (the item fired; drawing or retrying, `W` = LSC widening level 0-2); `Embodied` (Informant with a live bound pawn); `Absent` (Informant, disconnected, no pawn, no item; `t_lost` kept); `RoundEnded` (frozen). Exiled is never a resting state: §2.3 runs inside one slot and leaves the participant `Awaiting`.

**3.2 Scheduling a respawn** (`ScheduleRespawn(P, Cause, t_lost)`, always inside a slot):
- `t_floor = GetScheduledDeathServerTime(DyingPawn) + DeathFallSec + NoPawnFadeSec` when the participant's body is dying (Assassinated, ReversedAgainst, InformantKilled; ADR-0018 §4.1 is valid from the kill slot), else `t_floor = t_lost`.
- `T_due = max(t_lost + D, t_floor, ServerNow)` with `D = InformantRespawnDelaySec`, or `D = 0` for a body lost without a kill (F2).
- **If the participant's Roster connection is `Disconnected` at this slot** (`Unresponsive` counts as connected, Roster R11): store `t_lost` and `t_floor`, schedule **nothing**, state `Absent` (PEN-R8, S-B3). Round's later grace expiry for that participant is its own logged no-op.
- Otherwise `TimerHandle = ScheduleAt(T_due, TimerFired(InformantRespawn, {P, RoundEpoch, RespawnRequestSeq}))` (ADR-0002 §2a generic Foundation `FTimerPayload`: `Subject` = participant `P`, `Seq` = `RespawnRequestSeq`, `Generation` = `Phase` = 0; `RoundEpoch` is the item's `FEpochToken`), state `Awaiting`, log `RespawnScheduled{P, Cause, t_lost, T_due}`, and notify the `T_due` carrier adapter (§6).

**3.3 Transitions** (the GDD's States table, mechanised):

| From | Event (always inside a slot or its Roster flush) | To | Action |
|---|---|---|---|
| Active | `OnInformantAwaitingBody(P, Assassinated/ReversedAgainst, slot time)` from Round after its elimination check (never if latched) | Awaiting (or Absent if Disconnected) | §3.2 with `t_lost` = slot time |
| Active | `OnInformantAwaitingBody(P, StrikeLimit, slot time)` (§2.3 step 6; never if latched) | Awaiting | §3.2, `t_floor = t_lost` |
| Active | Round grace expiry → `Informant(Disconnect)` + `ReleaseToCrowd` (Round R17.4) | Absent | `t_lost` = expiry slot time; no item |
| Awaiting | `InformantRespawn` item resolves | Picking(0) | §4 |
| Awaiting / Picking | Roster connection → `Disconnected` | Absent | cancel the item (`Cancel(TimerHandle)` or epoch-stale), keep `t_lost`; log `RespawnCancelled{Disconnect}` |
| Picking | takeover `Applied` | Embodied | log `InformantRespawn{PickLatencySec, WideningLevel}`; clear `ExcludePawn`; adapter cleared (§6) |
| Picking | no eligible pawn / deferred | Picking(W') | §4.3 retry |
| Embodied | own pawn killed (`VictimKind = Informant`) | Awaiting | `RetireBinding` inline; §3.2 cause `InformantKilled` |
| Embodied | Roster connection → `Disconnected` (Round R17.5 releases the pawn to the crowd in that slot) | Absent | `t_lost` = that slot time, `t_floor = t_lost`; no item |
| Embodied | `OnInformantPawnLost(P)` (Round R17.9 routes the informant case here) | Picking(0) | `T_due = now`, `D = 0`; log `PawnLost`; run §4 in the same slot |
| Absent | `OnInformantReadmitted(P)` (Round R18; phase `Active`) | Awaiting if `now < T_due` else Picking(0) | `T_due = max(t_lost + D, t_floor, now)` with the **kept** `t_lost`, `t_floor`; a returner after `T_due` runs §4 in the same slot |
| any | `OnRosterRoundEnded` (latch step or the deciding strike's round-end step) | RoundEnded | cancel every item; freeze; between the latch and that step Penalty items resolve `Rejected_Latched` (ADR-0002) |
| any | `OnRosterReset` | Active(0), t = 0 | clear the store for the new epoch |

A stale item from an earlier round resolves `Rejected_StaleEpoch` (ADR-0002 §4); a stale `BindPawn` is rejected by the Roster's epoch guard.

### 4. The commit: one call stack inside the `InformantRespawn` slot
**4.1 Order** (PEN-R9 mechanised under ADR-0010/0012's constraints):
1. **Re-validate**: phase `Active`, epoch matches, Roster state `Informant`, no live pawn, connection not `Disconnected`. Otherwise void (log `RespawnCancelled{reason}`) and move to `Absent` (disconnect) or stop (round over).
2. **Pick**: `FRespawnPick R = ULayoutService::PickRespawnPawn(Team, RespawnRequestSeq, Exclude)` (ADR-0014 §7.3: region by widening level, unseen test by `ViewBlock` traces, uniform rejection-sampled draw from `RespawnPick`, re-run of the unseen test at commit). `Exclude` = the participant's own released exile body plus this request's short-lived exclusion set (below; signature confirmation requested of ADR-0014). This is the **only** random draw. ADR-0014 guards the pick at one per server frame, so **at most one pick per slot**:
   - `Deferred` (the per-frame guard) → §4.3 next-frame retry, no widening time consumed.
   - `None` → §4.3 retry.
3. **Pre-check (amendment request to ADR-0010)**: `UStealthPawnFactory::CheckTakeOverEligibility(R.Pawn)` — the same predicate as `TakeOverCrowdPawn` step 1, side-effect free (ADR-0014 is asked to apply it, with Crowd's `GetTakeoverEligibility`, inside its own pre-filter, so this rarely fails). `NotEligible` → add `R.Pawn` to the request's exclusion set and take the §4.3 next-frame retry. The exclusion set lives in the request record, persists across next-frame retries and is cleared when a regular `EmergenceRetrySec` retry fires (a transiently ineligible pawn can return).
4. **Crowd release**: `ITakeoverEligibilitySource::TryReleaseNpc(R.Pawn, Expected)` (ADR-0016 §8: atomic `Eligible` re-check, sink latched, StateTree stopped, ledger B += 1, pawn left moving). Penalty is its **only** caller (confirmation requested of ADR-0016/ADR-0014, whose texts say "via `PickRespawnPawn`"). `false` → exclude and next-frame retry.
5. **Takeover**: `UStealthPawnFactory::TakeOverCrowdPawn(R.Pawn, P, ExpectedEpoch)` (ADR-0010 §3.2: eligibility, `Roster.BindPawn`, `ICrowdControl::ReleaseControl` — confirmation requested that it is idempotent on a pawn `TryReleaseNpc` already released — and `RequestHandOver(Player)` with the control-epoch bump). On `NotEligible` call `ICrowdControl::AdoptReleasedPawn(R.Pawn, {})` to return the pawn to the crowd (confirmation requested of ADR-0016 that this exactly reverses `TryReleaseNpc`: ledger B −1, sink unlatched, tree restarted), exclude it and take the next-frame retry. Once ADR-0010 provides the step-3 pre-check, steps 3-5 run with nothing in between that can change the pawn and `NotEligible` here is a **defect** (fault sink); until then it is an ordinary PEN-R9 outcome and is only logged. A `BindPawn` rejection (`Rejected_StaleEpoch`/`WrongState`) means the request is stale: return the pawn the same way and void.
6. **Card**: only after `Applied`, `UAppearanceAllocator::TakeNpcCard(P, R.Pawn)` (`NpcLive → BoundLive`, `AppearanceId` untouched); then Roster `RecordAppearance(P, AppearanceId)` (log-only history). If `TakeNpcCard` fails after `Applied`, the takeover stands: report `LedgerMismatch` to the fault sink, no rollback.
7. **Commit bookkeeping**: state `Embodied`, log `InformantRespawn{P, PickLatencySec = now − T_due, WideningLevel}`, clear `ExcludePawn`, the exclusion set and the request; the carrier adapter clears the countdown (§6).

No actor, component or NetGUID is created; the pawn's `AppearanceId`, velocity, `HeadLook` and net config do not change (ADR-0010 §4.1; the engine's `ForceNetUpdate` in `PossessedBy`/`UnPossessed` is suppressed by ADR-0003 §2 in both directions, incl. the exile). The hand-over executes at the next frame start (ADR-0010 §4.3, AI → player with the held input until the owner's first `ServerMove`; the owner's `ServerMove`s are refused until it acknowledges the pawn, so the held-input window can last about one extra round trip — added to spike H1). Teammates see the outline marker move to the new body through C-TeamView's `PawnRef` (ADR-0001 §5, Teammate Markers TM-R7). The participant's PlayerController is normally already pawnless: an exile body was handed to AI at the exile (§2.3), and a dying body was released at `DeathServerTime + DeathFallSec` (ADR-0018 §6.1), which precedes `T_due` by at least `NoPawnFadeSec` (F2 floor). Where it is not (a `PawnLost` re-pawn with `D = 0`, or a due-time tie if `NoPawnFadeSec` were 0), the engine's `OnPossess` unpossesses the old pawn first [V `PlayerController.cpp:879-882`], and ADR-0010 is asked to state that the hand-over pass executes body releases before possessions in the same frame.

**4.4 The owner's view across unpossess and pawnless time.** `APlayerController::OnUnPossess` calls `SetViewTarget(this)` when the view target is the pawn [V `PlayerController.cpp:1353-1368`]; on the owning client a replicated null pawn reaches `AutoManageActiveCameraTarget` [V `Controller.cpp:551-569`; `PlayerController.cpp:603-633, 4976-4983`]; and a local PlayerController with no pawn moves itself to `Inactive`, whose `BeginInactiveState` sets the pawn null again [V `PlayerController.cpp:2333-2337, 5988-6005`]. `Inactive` never sets the spectator flag [V `PlayerController.cpp:5960-5968`], so nothing replicates and the "never Spectating" rule holds; taking over a pawn restores `Playing` on both ends [V `PlayerController.cpp:~860, ~919`]. Two engine paths must still be closed (requests to ADR-0015/ADR-0010/ADR-0005): `AStealthPlayerController` sets `bAutoManageActiveCameraTarget = false` and overrides `OnUnPossess` to keep the view target for every factory-initiated unpossess (exile hand-over, `ReleaseBody`), so Camera (ADR-0015) alone sets view targets; and `AStealthGameMode::PlayerCanRestart_Implementation` returns false, so `Inactive`'s re-enabled `StartFire → ServerRestartPlayer → RestartPlayer` path can never spawn a pawn [V `PlayerController.cpp:3263-3266, 3302-3330`; `GameModeBase.cpp:1203, 1264-1276`]. A remote client's PlayerController stays `Playing` on the server while pawnless (`PlayerTick` runs only with `PlayerInput`) [V `PlayerController.cpp:5638-5647`].

**4.2 Several informants due in one drain** resolve in key order (TieRank for equal due times, ADR-0002 §4); because of the one-pick-per-frame guard the second gets `Deferred` and retries next frame, where the pawn taken by the first is `Bound` and ineligible (Crowd `Bound` reason; LSC "claimed this tick").

**4.3 Retries and widening** (PEN-R10, LSC R5b.5): `RespawnRequestSeq` is allocated once per respawn request and reused on every retry, so ADR-0014's per-request stream continues instead of repeating, and its widening is keyed by the request: level 0 until `RespawnStarvationSec` after `PickStartServerTime`, level 1 (2 × the region) until twice that, then level 2 (any walkable cell not within `InformantRespawnPathCm` of an enemy-used anchor). The service returns the level it used (`FRespawnPick.WideningLevel`, interface confirmation requested of ADR-0014). A retry is `ScheduleAt(now + EmergenceRetrySec, TimerFired(InformantRespawn, same payload))`; a `Deferred` pick or an excluded candidate re-schedules for the next frame with a due time **strictly greater** than this drain's start (`ServerNow + 1 ms`), because an inline item due at or before now would run in the same drain (ADR-0002 §4). The unseen test is never relaxed, a bound pawn is never taken, no pawn is ever spawned; past level 2 the participant keeps waiting and `RespawnStarved` is logged once per request. Round's `LateJoinTakeoverTimeoutSec` view-only fallback never applies to informants (Penalty CR-7).

### 5. Informant restriction enforcement map (PEN-R12, R13)
Penalty adds no restriction of its own (an informant-only rule would itself be a tell). Each row is enforced where the table says and has a named test:

| Restriction | Enforced by | Mechanism | Test |
|---|---|---|---|
| Walk only (Run clamped to Walk), Look, camera toggle | ADR-0010 (effective-gait clamp via `IGaitLegalitySource`), ADR-0015 | gait clamp; camera client-local | AC-NKP T7 / SCL ACs |
| No run, traverse, interact, brace, attack, hide | ADR-0011 | legality class `Informant` → `NotNow`; client `IMC_Informant` context | IIF legality ACs |
| No x-ray (cannot see concealed non-allies) | ADR-0001/0003 (IVP viewer class `OpponentInformant ≡ Opponent`), ADR-0017 | concealment cull and presentation identical to an Active opponent | AC-IVP / AC-CON |
| Keeps every ally's marker | ADR-0001 C-TeamView (Team audience) | unchanged teammate entries | AC-TMK |
| Looks like an NPC to every opponent | ADR-0001 (only `All` facts on the pawn), ADR-0010 (possession parity) | no field differs; takeover invisible | §Validation 1 |
| Does not count for elimination | Roster `CountActive` (Round R15) | state `Informant` | Roster/Round ACs |
| Can be killed; not a strike; NPC-kill feedback | ADR-0018 (byte-identical `WrongTarget`), this ADR §2.1 | `VictimKind = Informant` → no strike, tally +1 | AC-NKP / AC-PSR-52 |
| Respawns near its team spawn, never returns to Active this round | this ADR §3-§4; Roster (no revival transition) | takeover in the respawn region | §Validation |
| Tell Matrix T7 (never seen running, vaulting, climbing, interacting, bracing, hiding) | ADR-0011 + ADR-0010 clamps; Crowd T7 calibration (ADR-0016) | the above | AC-NKP-17 / AC-CNB-30 |

### 6. The `T_due` carrier: consumed slot (pending user decision)
Penalty always computes and stores `DueServerTime` (§3.2) and raises a server-side notification on every change: `OnDueTimeChanged(P, TOptional<double> T_due)` (set at scheduling and readmission; unset at embodiment, cancellation, round end and reset). The **carrier adapter** that turns this into owner-visible data is **not implemented** until the user decides ADR-0001 §6 (architecture AQ-02; category schema_changes) and ADR-0001 is amended. For each recorded option the adapter is:

| ADR-0001 §6 option | Adapter (only after the decision) | Reconnect / rejoin | Inputs the client would need, and whether it already holds them |
|---|---|---|---|
| (a) field in the C-TeamView owner part | Roster owner part gains `DueServerTime` (double, −1 when none); Penalty writes it on `OnDueTimeChanged` through the Roster's owner-part write API, never while `IsLatched()` | survives (replicated state) | `T_due` itself (new Owner field: needs IVP fact + allowlist rows; never on the pawn) |
| (b) field in an owner reply | an `FInformantDueReply {DueServerTime}` entry in ADR-0011's `ClientCommandReplies` batch on every change, resent on `OnInformantReadmitted` | resent by Penalty on readmission; lost on a client restart until the next change unless resent | `T_due` itself (new Owner reply entry: needs an IVP leak-catalogue row) |
| (c) client derivation | no carrier | derived again from the inputs on rebuild | `D` = `InformantRespawnDelaySec` (held: public tuning); own `DeathServerTime` (held: `FDeathState`, All); `T_v` of the killing strike (held only if the client can identify the killing strike's `FActionState`, which names no victim); `NoPawnFadeSec`, `DeathFallSec` (held: tuning); exile slot time (held by the exiled owner via its own strike write); **`t_lost` after a disconnect/reclaim and `t_now` of the reclaim (not held: server-only)** |

**Interim until decided:** no countdown value leaves the server. HUD-E11 has no input and can present only its `s ≤ 0` form ("Returning", static; UI-NKP-4), which reveals nothing and needs no new field (GDD sync note to HUD F7; ADR-0021 owns the presentation). Nothing else in this ADR changes when the carrier is decided.

### 7. Event Log and telemetry hooks
Penalty writes (server-only, `PostRoundEligible` unless stated; never `UE_LOG`/`UE_LOGF`, output log or trace channels; ADR-0002, Round R19): `StrikeCounted{Killer, StrikeAfter, TallyAfter, Graced}`, `RespawnScheduled{Participant, Cause, t_lost, T_due}`, `InformantRespawn{Participant, PickLatencySec, WideningLevel}`, `RespawnCancelled{Participant, Reason}`, `PawnLost{Participant, Cause}`; LSC writes `RespawnStarved`; Round writes `StrikeLimit` with the transition.

### Architecture Diagram

```
 Attack slot (ADR-0018)                         InformantRespawn slot (due-time heap, ADR-0002)
  OnPawnKilled ──► Penalty                        re-validate (Informant, no pawn, connected, Active, epoch)
   ├ VictimKind=Informant: RetireBinding           │
   │   └ ScheduleRespawn(InformantKilled)          ├─ LSC PickRespawnPawn(Team, ReqSeq, Exclude) ── None/Deferred ─► retry item
   ├ strike rule (F1, grace)                       ├─ Factory CheckTakeOverEligibility (requested)   ─ no ─► re-pick (≤4)
   └ k = L & Active ─► EXILE (one call stack)      ├─ Crowd TryReleaseNpc (atomic)                    ─ no ─► re-pick
       Roster RequestTransition(StrikeLimit)       ├─ Factory TakeOverCrowdPawn: BindPawn, ReleaseControl,
        └ ReleaseToCrowd                           │    RequestHandOver(Player)  ─ defect ─► AdoptReleasedPawn, re-pick
       CPA ReleaseCard                             ├─ CPA TakeNpcCard (only after Applied); Roster RecordAppearance
       Crowd AdoptReleasedPawn(lockout, exit)      └─ Embodied; log; T_due adapter cleared
       Factory RequestHandOver(Ai)
       (respawn via Round's OnInformantAwaitingBody)   next frame: hand-over pass possesses (ADR-0010)
  GetTallyForResult ─► FStrikeResult{TallyAfter, bExiled} (ADR-0018)
 Round (after its elimination check, not latched) ─► IInformantBodyProvider::OnInformantAwaitingBody ─► ScheduleRespawn (or Absent if Disconnected)
 Round R18 readmission ─► IInformantBodyProvider::OnInformantReadmitted ─► Awaiting / commit now
 Round R17.9 informant pawn lost ─► OnInformantPawnLost ─► commit now (D = 0)
 OnDueTimeChanged ─► [T_due carrier adapter: SLOT, pending user decision]
```

### Key Interfaces

```cpp
// ---- StealthGameplay Core Public/Server (declared by Round; implemented by Penalty) ------
enum class ERespawnCause : uint8 { Assassinated, ReversedAgainst, StrikeLimit, InformantKilled, PawnLost, Readmitted }; // Core, beside the interface
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class UInformantBodyProvider : public UInterface { GENERATED_BODY() };
class IInformantBodyProvider { GENERATED_BODY() public:
    virtual void OnInformantAwaitingBody(FParticipantId P, ERespawnCause Cause, double LostServerTime) = 0; // Round, after its elimination check, only if not latched
    virtual void OnInformantReadmitted(FParticipantId P) = 0;   // Round R18, inside Round's slot, after the returner's Session ack
    virtual void OnInformantPawnLost(FParticipantId P) = 0;     // Round R17.9 informant case, inside the PawnLost slot
};

// ---- StealthGameplay Features/Penalty Public/Server ---------------------------------------
enum class EInformantLifeState : uint8 { Active, Awaiting, Picking, Embodied, Absent, RoundEnded };
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDueTimeChanged, FParticipantId, TOptional<double> /*T_due*/); // server-only
UCLASS(NotBlueprintType) class UPenaltyService : public UWorldSubsystem,
    public IStrikeTallySource /*ADR-0018*/, public IInformantBodyProvider, public IQueueResolver /*InformantRespawn*/
{
public:
    FOnDueTimeChanged OnDueTimeChanged;                       // consumed only by the T_due carrier adapter (slot)
    EInformantLifeState GetLifeState(FParticipantId) const;   // server-only queries (tests, Round assertions)
    TOptional<double>   GetDueServerTime(FParticipantId) const;
    // IStrikeTallySource
    virtual FStrikeTally GetTallyForResult(FParticipantId Attacker, EStrikeResult Result) override;
    // IInformantBodyProvider
    virtual void OnInformantAwaitingBody(FParticipantId P, ERespawnCause Cause, double LostServerTime) override;
    virtual void OnInformantReadmitted(FParticipantId P) override;
    virtual void OnInformantPawnLost(FParticipantId P) override;
    // IQueueResolver (ETimerKind::InformantRespawn)
    virtual EResolveResult Resolve(const FQueueItem& Item, FQueueKey Key) override;
};
// No payload struct here: the InformantRespawn item uses ADR-0002 §2a's Foundation FTimerPayload
//   {Kind = InformantRespawn, Subject = FParticipantId P, Generation = 0, Seq = RespawnRequestSeq, Phase = 0};
//   RoundEpoch = FQueueItem::Expected. FPawnHandle (ExcludePawn, ReleasedBodies) is ADR-0002's.

// Consumed (owned elsewhere): ULayoutService::PickRespawnPawn -> FRespawnPick { AStealthPawn* Pawn;
//   ERespawnPickStatus Status {Picked, None, Deferred}; uint8 WideningLevel; }   (confirmation requested of ADR-0014)
// UStealthPawnFactory::CheckTakeOverEligibility(const AStealthPawn*) const -> ETakeoverResult (requested of ADR-0010)
```

### Implementation Guidelines

- Penalty state must live only in `FPenaltyCore` keyed by `ParticipantId`; it must never be a replicated property, and `StrikeCount` must never reach any client before `Ended`.
- Every Penalty mutation must run inside a queue slot (an `OnPawnKilled` follow-on, a Roster-event flush, an `InformantRespawn` item, or an `IInformantBodyProvider` call from Round's slot); Penalty must never use an engine timer, a feature tick or a latent action.
- The exile must run `RequestTransition(StrikeLimit)` (with the Roster's internal `ReleaseToCrowd`), `ReleaseCard`, `AdoptReleasedPawn` and `RequestHandOver(Ai)` in one call stack inside the killing Attack's slot; the surviving body must never be left `Retired`.
- A respawn must never spawn a pawn; it must only take over a pawn returned by `PickRespawnPawn`, and `PickRespawnPawn` must be the only random draw of the sequence.
- The commit order must be pick → eligibility pre-check → `TryReleaseNpc` → `TakeOverCrowdPawn` → (`Applied` only) `TakeNpcCard` → `RecordAppearance`, in one call stack; no card move may be committed for any other result, and a released pawn that is not taken over must be handed back with `AdoptReleasedPawn` in the same stack.
- Respawns for `Active → Informant` causes must be scheduled only from Round's `OnInformantAwaitingBody` call (after its elimination check, never when latched), never from a Penalty subscription to Roster state events.
- A respawn cause on a `Disconnected` participant must schedule no timer; reclaim must recompute `T_due` from the kept `t_lost` and `t_floor`, never restart the delay and never skip it.
- Retries must reuse the request's `RespawnRequestSeq`; the unseen test must never be relaxed and widening must come only from LSC.
- `T_due` must be computed and logged but must not reach any client until the user decides ADR-0001 §6; no countdown field, reply entry or client derivation may be implemented before that.
- Penalty must add no informant-only movement, collision, sight or region rule; restrictions are enforced only by the owners in §5.

## Alternatives Considered

### Alternative 1: Spawn a fresh pawn at the team spawn (the literal locked rule)
- **Description**: Spawn a new Shared Pawn for the informant at a start cell or emergence socket of its team.
- **Pros**: Simple; matches the rule's wording; no crowd coordination.
- **Cons**: A new actor and NetGUID mid-round, appearing near a team's own spawn, is both a network tell and a location tell (P1/P2); Roster R9 forbids mid-round player-pawn spawns; LSC R5 forbids player emergence.
- **Rejection Reason**: Breaks Pillar 1; orchestrator decision C1 replaced it (pending user confirmation in active.md).

### Alternative 2: Re-possess the dead body or keep the participant's old look
- **Description**: Bring the informant back in its own body or a same-look pawn.
- **Pros**: Continuity for the player.
- **Cons**: Bodies are never re-possessed (ASN-R17); a look that follows a participant across death gives the killer a lasting handle (CP-12b).
- **Rejection Reason**: Identification aid; contradicts two reviewed GDD rules.

### Alternative 3: Card move before the takeover, reverted on failure (the GDD's PEN-R9 order)
- **Description**: `TakeNpcCard` at step 4, `TakeOverCrowdPawn` at step 5, revert the card on `NotEligible`.
- **Pros**: Matches the GDD text literally.
- **Cons**: ADR-0010 and ADR-0012 forbid committing a card move unless the takeover returned `Applied`; a revert path is a second ledger mutation in the same slot with its own failure modes.
- **Rejection Reason**: Same observable result with no revert; the GDD order is a sync note.

### Alternative 4: Engine timers or a Penalty tick for respawns and retries
- **Description**: `FTimerManager` for `T_due` and the retry cadence.
- **Pros**: Less queue plumbing.
- **Cons**: Banned by ADR-0002 (no ordering against other items, no epoch voiding, no latch interaction).
- **Rejection Reason**: Every timer is a due-time-heap item.

### Alternative 5: Choose a `T_due` carrier here
- **Description**: Pick option (a) and implement it.
- **Pros**: The HUD countdown would work in MVP.
- **Cons**: A new replicated field is a schema change reserved for the user (AQ-02, C-W5).
- **Rejection Reason**: Not this pipeline's decision; consumed as a slot with a per-option adapter plan and a no-new-data interim.

### Alternative 6: Penalty calls `BindPawn` itself and possesses through Round
- **Description**: Penalty binds and asks Round to possess, as the Roster R9/Round R18 text reads.
- **Pros**: Matches the older GDD wording.
- **Cons**: ADR-0010 makes `TakeOverCrowdPawn` own eligibility and `BindPawn` atomically; two binders invite a bound-but-not-taken window.
- **Rejection Reason**: One owner for the bind (ADR-0010); recorded as a GDD sync note.

## Consequences

### Positive
- One sequence, one owner: every informant body, for every cause, comes from the same slot-ordered call stack, so the takeover is invisible by construction and testable headlessly in `FPenaltyCore`.
- Disconnects and reclaims never shorten or skip a respawn and never strand a participant (S-B3 closed).
- The `T_due` decision becomes a one-adapter change, with each option's consequences pre-analysed for the user.
- Penalty is reached by other systems only through two consumer-owned interfaces (`IStrikeTallySource`, declared by Assassination, a sibling Feature; `IInformantBodyProvider`, declared by Round in Core).

### Negative
- Until the user decides the `T_due` carrier, the MVP HUD shows a static "Returning" instead of a countdown (a presentation regression against UI-NKP-4's numeric form).
- The commit depends on four other services in one stack; a defect in any of them surfaces here (mitigated by the defect branches and logs).
- Two amendment requests (ADR-0010 pre-check, ADR-0014 `FRespawnPick` shape) must be answered before implementation.

## Risks
- **Starvation in a watched home region** — Medium likelihood, Medium impact (long black screen). Mitigation: LSC widening (10 s, 20 s), Penalty F3 budget `D + 2 × RespawnStarvationSec ≤ 40` (TDC warning), V-RESPAWN layout validation; telemetry of `PickLatencySec`.
- **Hand-over correction on the new owner's first move (ADR-0010 H1)** — Low/Medium: owner-only snap if it fails; fallback in ADR-0010 (keep crowd driving until acknowledged).
- **Re-pick churn** — at most one pick per slot (ADR-0014's per-frame guard); excluded candidates and `Deferred` picks retry next frame; the exclusion set clears at each regular retry so it cannot grow without bound.
- **Order between a Roster disconnect event and a due item in the same drain** — the key order decides; a disconnected participant's item re-validates and voids (step 1), so no bind lands on an unconnected participant. **Reverse race:** a takeover commits and a later disconnect slot in the same drain releases the pawn while the AI → player hand-over is still pending. ADR-0010 is asked to rule that a later `RequestHandOver` on the same pawn replaces the pending one, and that a `Player` target whose controller is invalid when the pass runs is dropped (the pawn stays crowd-possessed).
- **`Unresponsive` informant at commit** — PEN-R9 counts `Unresponsive` as connected, so the takeover commits; the pawn holds the last command for `HandoverHoldSec` and then brakes by the baseline, an undriven stop. Round R17.1's AI hand-over covers only `Active` participants; Round/ADR-0016 are asked to extend it to informant-bound pawns (Move/Look only). Until then the stop is a known risk.
- **Exile body and the owner's camera** — the PlayerController unpossesses the released body after its strike lockout; without §4.4's `OnUnPossess`/`bAutoManageActiveCameraTarget` changes the owner's view would cut to the controller. Covered by §4.4 and the Camera ACs.
- **Pending OQ-NKP-1** — if the user picks a live exact strike count, only `GetTallyForResult` changes (returns `StrikeCount`) and Roster R10 narrows; the lifecycle is unaffected.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| npc-kill-penalty-informants.md | PEN-R1 ownership; server-only store keyed by `ParticipantId` | §1 `UPenaltyService`/`FPenaltyCore` |
| npc-kill-penalty-informants.md | PEN-R2 strike conditions; PEN-R3 tally in `FStrikeResult`; PEN-R6 carry-over/reset; F1 | §2.1, §2.2, §3.3 reset row |
| npc-kill-penalty-informants.md | PEN-R4 exile with release in the same call; PEN-R5 grace and exit; `ExcludePawn` | §2.3; `ReleasedBodies`; §4.1 step 2 |
| npc-kill-penalty-informants.md | PEN-R7 one respawn path for every cause | §3.2, §3.3, §4 |
| npc-kill-penalty-informants.md | PEN-R8 Round timer at `T_due`; disconnected cause → Absent, no timer (S-B3); F2 | §3.2 |
| npc-kill-penalty-informants.md | PEN-R9 commit sequence in one call stack | §4.1 (card after `Applied`, sync note) |
| npc-kill-penalty-informants.md | PEN-R10 retries, starvation, cancellation | §4.3, §3.3 |
| npc-kill-penalty-informants.md | PEN-R11 disconnect, return, lost bodies | §3.3 rows Embodied/Absent/PawnLost |
| npc-kill-penalty-informants.md | PEN-R12/R13 informant contract and T7 | §5 enforcement map |
| npc-kill-penalty-informants.md | PEN-R14 informant-kill invariant | §2.1 no strike; ADR-0018 byte-identical result |
| npc-kill-penalty-informants.md | PEN-R15 owner data incl. `T_due` | §2.2 via `FStrikeResult`; §6 slot + interim |
| npc-kill-penalty-informants.md | PEN-R17 Event Log rows | §7 |
| level-socket-contract.md | R5b request, eligibility, draw, result, starvation | §4.1 step 2, §4.3 (mechanism ADR-0014) |
| crowd-npc-behaviour.md | CNB-R11 takeover supply, `TryReleaseNpc`, `AlleyExit` for the exile body | §4.1 step 4; §2.3 step 3 |
| character-pool-appearance.md | CP-12b crowd adoption only, one draw, `ReleaseToCrowd` at the exile | §2.3 step 2, §4.1 step 6 |
| character-pool-appearance.md | CP-13 informant re-pawn incl. returning informant; no late join | §3.3 Absent row; `IInformantBodyProvider` |
| round-match-framework.md | R17.5, R17.9 (informant case), R18 (Penalty owns retries) | §1 seams, §3.3 |

## Performance Implications
- **CPU**: per respawn one `PickRespawnPawn` (ADR-0014: trace-bound, ≤ 1 pick per server frame) plus O(1) calls; strike/tally updates are O(1) inside the Attack slot. Within the 0.5 ms queue/resolver line except the pick, which is ADR-0014's budget.
- **Memory**: one small record per participant (≤ 10) and a map of released bodies (≤ the exiles of one round).
- **Load Time**: none.
- **Network**: none of Penalty's own; the takeover's possession change is owner-only pointers; the `T_due` carrier, once decided, adds at most one double per change to one connection.

## Migration Plan
The prototype's informant respawn (spawn at team start, `PlayerStart`-based, `Source/StealthGame` `AStealthGameMode`) is evidence only and is not ported; it violates Roster R9. Penalty is written fresh against this ADR once it and its dependencies are Accepted.

## Validation Criteria
- `StealthGame.Penalty.StrikeTally` (unit, `FPenaltyCore`): the F1 example (NPC, informant, NPC, NPC → tally 1-4, strikes 1,1,2,3, exile on the fourth); grace boundary at exactly `StrikeOutReleaseGraceSec` (inclusive); saturation at 255; `StrikeCount ≤ Tally` always.
- `StealthGame.Penalty.DueTime` (unit): F2 examples (kill at 100.0 → 115.0; disconnect 103, reclaim 120 → 120.0); bush-strike floor; `D = 0` for a lost body.
- `StealthGame.Penalty.DisconnectedVictim` (integration): an AI-driven `Active` player in grace is assassinated → `Absent`, no item; reclaim inside and after grace → `Awaiting`/immediate commit (S-B3).
- `StealthGame.Penalty.CommitOrder` (integration with fakes): `TakeNpcCard` is never called unless `TakeOverCrowdPawn` returned `Applied`; a `TryReleaseNpc` failure re-picks; a forced `NotEligible` returns the pawn with `AdoptReleasedPawn` and logs a defect; ledger sweep clean after every drain.
- `StealthGame.Penalty.TakeoverInvisible` (multi-process, raw-bunch capture): a rival client observes no change on the taken pawn at the takeover frame or the hand-over frame beyond its ordinary 15 Hz movement updates (AC-IVP-45 parity).
- `StealthGame.Penalty.Starvation` (integration): all region pawns watched → level 1 after 10 s, level 2 after 20 s, then indefinite wait with one `RespawnStarved`; no spawn.
- `StealthGame.Penalty.ExileSequence` (integration): the exile body is `Unbound`, card `NpcLive`, crowd-driven with the lockout restriction, takes the next scheduled exit slot, is never the participant's own respawn pawn; the round latches at once when it was the team's last `Active` player and no respawn is scheduled.
- `StealthGame.Penalty.TDueNotOnWire` (reflection/bunch audit): until ADR-0001 §6 is amended, no replicated property, RPC parameter or reply entry is derived from `DueServerTime`.

## Related
- **Depends on**: ADR-0001, ADR-0002, ADR-0004, ADR-0006, ADR-0010, ADR-0011, ADR-0012, ADR-0014, ADR-0016, ADR-0017, ADR-0018.
- **Enables**: ADR-0021, ADR-0023; Informant Pings (VS).
- **Amendment / confirmation requests (not applied; owned by other ADRs)**:
  1. **ADR-0010 §3.2:** add `CheckTakeOverEligibility(const AStealthPawn*) const` (side-effect-free, same predicate as `TakeOverCrowdPawn` step 1) so the `TryReleaseNpc` → `NotEligible` path is unreachable in normal play.
  2. **ADR-0014 §7.3 / Key Interfaces:** confirm `FRespawnPick { Pawn; Status ∈ {Picked, None, Deferred}; WideningLevel }`, widening keyed by `RespawnRequestSeq` from the request's first call, and that a `Deferred` result is returned when the per-frame pick guard defers.
  3. **ADR-0005 / Round (Core):** Round declares `IInformantBodyProvider` and calls it from its R18 readmission and R17.9 informant-pawn-loss slots; the composition root registers Penalty.
  4. **ADR-0001 §6:** this ADR is the consumer of the `T_due` carrier decision; the per-option input table (§6) is offered as neutral input to the user's decision.
  5. **ADR-0016:** Penalty is the only caller of `TryReleaseNpc` (its §8 text says "via LSC `PickRespawnPawn`" and "LSC redraws"); `ReleaseControl` is idempotent on an already-released pawn; `AdoptReleasedPawn(Pawn, {})` exactly reverses `TryReleaseNpc` (ledger B −1, sink unlatched, tree restarted); extend Round R17.1's AI driving to `Unresponsive` informant-bound pawns.
  6. **ADR-0010:** a later `RequestHandOver` on a pawn replaces a pending one; a `Player` target with an invalid controller is dropped; the hand-over pass executes body releases before possessions; extend "never during a traversal" to root-motion action poses (the exile hand-over waits for the strike lockout).
  7. **ADR-0015 / ADR-0010 / ADR-0005:** `AStealthPlayerController` sets `bAutoManageActiveCameraTarget = false` and keeps the view target on factory-initiated unpossess (exile hand-over, `ReleaseBody`; extends ADR-0018 Related item 6); `AStealthGameMode::PlayerCanRestart_Implementation` returns false.
  8. **Participant State & Roster (GDD) / Roster ADR owner:** R8/R9 "release, not retire" for `StrikeLimit` inside `RequestTransition` (Penalty CR-3).
  9. **Round (Core):** call `IInformantBodyProvider::OnInformantAwaitingBody` from its own transition handler after the elimination check (never when latched), and `OnInformantReadmitted` only after the returner's Session acknowledgement.
- **GDD sync notes (not applied; GDDs owned by their authors)**: PEN-R9 steps 4/5 order (card after takeover); PEN-R9 step 3 "Crowd `TryReleaseNpc`" stays Penalty's call (ADR-0016 text says via LSC); Round R18/R17 gain the `OnInformantAwaitingBody` call after the elimination check; Roster R9 and Round R18 name Penalty as the `BindPawn` caller, under ADR-0010 `TakeOverCrowdPawn` binds on its behalf; HUD F7/HUD-E11 interim form while `T_due` has no carrier; LSC R5b.3 stream key (`RespawnPick` by `RespawnRequestSeq`, ADR-0014) instead of `LayoutAttempt`.
- **Pending user decisions consumed (already listed in active.md; nothing new)**: `T_due` carrier (C-W5 / AQ-02); Penalty OQ-NKP-1 (tally vs live count) and CR-19 (tally re-push); decision C1 confirmation; OQ-NKP-11 (strike limit hard minimum).

## Validation gates
- **TD-ADR (technical-director), 2026-10-24: CONCERNS → REVISED.** (1) `TryReleaseNpc` caller conflict with ADR-0016/0014 → confirmation requests (Related 5), plus `ReleaseControl` idempotence and `AdoptReleasedPawn` as exact reversal. (2) In-slot re-pick cap unreachable under ADR-0014's one-pick-per-frame guard → one pick per slot, exclusion set in the request record, next-frame retry strictly after the drain start (§4.1, §4.3). (3) `TakeNpcCard` failure after `Applied` → takeover stands, `LedgerMismatch`, no rollback. (4) Flush subscriber order → scheduling moved to Round's `OnInformantAwaitingBody` call after its elimination check, never when latched (§1, §2.3, §3.3). (5) Reverse disconnect race → ADR-0010 request + risk row. (6) `Unresponsive` informant at commit → risk row + Round/ADR-0016 request. (7) Controller not always pawnless → pass order request; engine tolerance [V]. (8) `NotEligible` not a defect until the pre-check exists. (9) §6 table made neutral (inputs held / not held); `TDueNotOnWire` reworded. (10) Missing-provider fault; layering wording. (11) Roster CR-3, exile view target, R18 after Session ack added to Related.
- **Engine validation (unreal-specialist lens), 2026-10-24: MINOR NOTES → applied.** Possession `ForceNetUpdate` suppressed both directions incl. the exile (ADR-0003 §2); `ClientRestart`/`ServerMove` acknowledgement window added to spike H1; exile camera: `OnUnPossess` `SetViewTarget(this)`, client `AutoManageActiveCameraTarget` and local `Inactive` transitions → `bAutoManageActiveCameraTarget = false` + `OnUnPossess` override (§4.4); exile hand-over waits for the strike lockout (root motion); pawnless 15+ s never reaches Spectating, `PlayerCanRestart` override closes the `StartFire` restart path (§4.4). All [V] with file:line in §4.1/§4.4.
- **GDD sync check:** checked 5 referenced GDDs (npc-kill-penalty-informants, level-socket-contract, crowd-npc-behaviour, character-pool-appearance, round-match-framework). No interface renames; ordering/caller notes recorded under Related (PEN-R9 card order, Roster R9/Round R18 `BindPawn` caller, LSC R5b.3 stream key, HUD F7 interim, Round calls `OnInformantAwaitingBody`).

## Amendments
Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §3.2, §1 store table, Key Interfaces: `FInformantRespawnPayload` removed; the `InformantRespawn` item uses ADR-0002 §2a's Foundation `FTimerPayload` (`Subject` = participant, `Seq` = `RespawnRequestSeq`), and `FPawnHandle` is ADR-0002's — architecture-review fix 2026-10-25 (Conflict 1, blocking).
- Related item 8 ("release, not retire" for `StrikeLimit` inside `RequestTransition`, with the Roster calling `ReleaseToCrowd` internally) is now applied in ADR-0005 and in ADR-0012's caller wording — architecture-review fix 2026-10-25 (Conflict 8).
- Requests answered (2026-10-24 and 2026-10-25): Related items 1 and 6 (ADR-0010), 2 (ADR-0014), 3, 7 and 9 (ADR-0005, ADR-0015, ADR-0010), 5 (ADR-0016) and 8 (ADR-0005/ADR-0012, this pass) are applied by their owners; item 4 is input to the user's pending `T_due` carrier decision (ADR-0001 §6). The Status line's "before the amendment requests listed under Related are answered" condition is met except for that pending user decision, which the Status line already names separately; the Status line itself is not edited — architecture-review fix 2026-10-25 (Conflict 9).
