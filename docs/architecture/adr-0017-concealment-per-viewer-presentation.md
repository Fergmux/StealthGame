# ADR-0017: Concealment component and per-viewer presentation

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It cannot be Accepted before ADR-0001, ADR-0002 (with the `ConcealmentTransition` timer amendment), ADR-0003, ADR-0010, ADR-0011 and ADR-0018 are Accepted and spikes **S-CON-1** (owner-side warp), **R5** (hidden-primitive leak matrix), **S9/S10** (cull close and re-open, owned by ADR-0003) have reported. Two user decisions (schema_changes) stay open as slots: `FActionState.StartServerTime` and `FDeathState.bDiedConcealed` (§8); the ADR is complete without them.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (transition items become a named post-latch exemption limited to Concealment state, cull and flags, and the guard-failure exit after the latch is an internal transition; fresh ally actors hold hidden while a teammate `PawnRef` is unresolved; killed-concealed flags clear at the server body removal or on the participant's rebind, with an ADR-0004 cross-field warning; owner-warp option (b) rewritten without client-authoritative position; emerge `Cross` scheduled from the same formula as clients). Engine validation (unreal-specialist lens): MINOR NOTES → applied (hidden primitives have no render proxy unless three gating flags keep one — fixed on class defaults incl. `bRayTracingFarField`; correction flags corrected; collision derived per saved move; pose ticked through the hide window; hidden state set before any RepNotify).

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims re-read against the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; design against `design/gdd/concealment-bushes.md` and `design/gdd/information-visibility-policy.md` R7 as revised and reconciled 2026-10-24)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full); technical-director (TD-ADR) and unreal-specialist validation; acceptance reserved for the user.

## Summary
A pawn in a bush must vanish for every non-ally (no body, shadow, light, sound or packet) while allies see a hatched marker, the owner keeps its body, NPCs use the same path, and every observer sees the entry and exit at the leaf plane exactly, independent of network arrival. Concealment is one non-replicated component on every pawn plus a server-only service (state machine, occupancy records, bush runtime built from the Manifest, transition timers as queue items, spot-zone queries, concealment log) that pushes cull state to the Visibility Policy (ADR-0003), and a client-side, fail-closed, state-derived body-hidden latch scheduled from the replicated server start time `FActionState.StartServerTime`, with `FDeathState.bDiedConcealed` for killed-concealed bodies (both approved by the user 2026-10-25).

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Networking + Rendering (with Physics/collision and Animation constraints) |
| **Layer** | Feature |
| **Knowledge Risk** | HIGH — architecture.md lists Concealment under Networking (cull) and Rendering (hidden shadow/Lumen/RT leaks), both HIGH for 5.4-5.8 |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/networking.md`, `modules/rendering.md`, `modules/physics.md`, `breaking-changes.md`, `deprecated-apis.md`; `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Feature layer, Data Flow §1-§2, API Boundaries, Performance budget, AQ-05, AQ-07, AQ-11); ADR-0001, ADR-0002, ADR-0003, ADR-0006, ADR-0016 (read); engine source listed in the facts table; prototype `AStealthBush` / `AStealthCharacter::SetInBush` and `production/first-playable-report.md` (evidence only) |
| **Post-Cutoff APIs Used** | None introduced by this ADR beyond those ADR-0003 owns (`SetRemoteViewPitch`, net-config setters). Primitive visibility flags and root-motion sources predate the cutoff and are re-verified below |
| **Verification Required** | **S-CON-1**: owner-side warp mechanism (§4) meets AC-CON-31 (≤ 50 ms press-to-motion, no owner correction > 5 cm) under `LeakNetProfile`. **R5**: hidden-primitive flag set (§6) leaves no shadow, VSM, Lumen (software and hardware RT), reflection or ray-traced trace of a hidden pawn (AC-CON-23). **S9/S10** (ADR-0003): close at the deadline, re-open creates a fresh actor that initialises through §5's latch. **OQ9**: seeking clips from `EstimatedServerNow` stays within one frame of the server key under `LeakNetProfile` (ADR-0002 accuracy budget). Collision-state re-derivation survives CMC replay after a correction (AC-CON-28) |

**Engine facts this ADR relies on** ([V] read in the 5.8.4 source this session; [D] engine-reference; [U] unverified, owned by the named spike):

| # | Fact | Tag |
|---|---|---|
| E1 | `UPrimitiveComponent` has `bCastHiddenShadow`, `bAffectDynamicIndirectLighting` + `bAffectIndirectLightingWhileHidden` (used only when the former is true), `bVisibleInRayTracing`, `bVisibleInReflectionCaptures`, `bVisibleInRealTimeSkyCaptures`, `bAffectDistanceFieldLighting`, `bCastDynamicShadow`; `USceneComponent` has `bHiddenInGame` and `SetVisibility(bool, bPropagateToChildren)` (`bVisible`), neither replicated on a non-replicated component | [V] `Engine/Classes/Components/PrimitiveComponent.h:465-473, 556-568, 622`; `SceneComponent.h:226, 931` |
| E2 | A primitive hidden with `SetVisibility(false)` is **not added to the scene at all** (no render proxy, hence no shadow, Lumen, ray-tracing, reflection or sky-capture contribution) unless `(CastShadow && bCastHiddenShadow)`, `(bAffectDynamicIndirectLighting && bAffectIndirectLightingWhileHidden)` or `bRayTracingFarField`; `OnVisibilityChanged` calls `MarkRenderStateDirty`. The non-Shipping world flag `bCreateRenderStateForHiddenComponentsWithCollsion` keeps a proxy for hidden components with collision. Still unverified: VSM shadow-cache page removal, groom (hair) components and Niagara attached to the pawn | [V] `PrimitiveComponent.cpp:2106-2122`; `SceneComponent.cpp:3574-3576`; `World.h:1293-1294` (engine gate) / [U] spike R5 |
| E3 | `AActor::bHidden` replicates and `APawn::IsNetRelevantFor` reads it (so it must never be used for concealment) | [V] ADR-0001 E3; `Pawn.cpp:1274-1299` per ADR-0003 E1 |
| E4 | `FRootMotionSource_MoveToForce` exists. The CMC's `bIgnoreClientMovementErrorChecksAndCorrection` **only stops the server generating corrections**; `bClientIgnoreMovementCorrections` stops the client applying them; taking the client's position needs `bServerAcceptClientAuthoritativePosition` (banned by ADR-0003 §3). Root-motion-source positions are corrected through `ClientAdjustRootMotionSourcePosition`; an owner-started source that does not match the server's (same `InstanceName`) would be duplicated, then corrected | [V] `RootMotionSource.h:545`; `CharacterMovementComponent.h:977-1000, 2683` (engine gate) / [U] duplicate-source behaviour: S-CON-1 |
| E5 | A non-replicated actor (`bReplicates = false`) spawned independently on server and clients is a valid way to build identical local geometry from replicated data; nothing of it reaches the wire | [V] general engine behaviour, prototype city generator builds client geometry this way (first-playable report §7) |
| E6 | Visibility-based anim ticking (`VisibilityBasedAnimTickOption`) can skip notifies on a hidden mesh, so anim notifies cannot drive gameplay-relevant or presentation-gating events for a hidden pawn; under `OnlyTickPoseWhenRendered` a hidden mesh also does not evaluate its seeked pose, so its first visible frame would show a stale pose | [V per CON-R16 / architecture.md (`VisibilityBasedAnimTickOption` [V]); stale-pose note (engine gate)] |
| E8 | `AActor::PostNetInit` exists and runs after the initial replicated properties are applied; RepNotifies from the first bunch may fire before it; cross-actor references (e.g. C-TeamView `PawnRef` on the PlayerController) are not resolved there | [V] `Actor.h:2958` / [U] RepNotify ordering (engine gate) |
| E7 | A Material Parameter Collection can hold 8 pulse slots × 2 `float4` (16 vectors) and is updated client-side without replication | [U] (no limit constant found in 5.8.4 `Public/Materials/MaterialParameterCollection.h`; 16 vectors is trivially small) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (one pawn class; carrier model; C-TeamView teammate entry `bIsConcealed`; owner data on the PlayerController). ADR-0002 (queue, slots, `EstimatedServerNow`, staging pass; `ETimerKind::ConcealmentTransition`, §3 — amendment applied). ADR-0003 (`IConcealmentCullSource`, cull grid, explicit close, un-cull in the same step, killed-concealed re-open with `FDeathState.bDiedConcealed` (`MarkKilledConcealed` retired), fixed-width `FActionState` incl. `StartServerTime`). ADR-0006 (secret streams; `BushSpotPick` seed domain 14, §7 — amendment applied). ADR-0010 (Shared Pawn: root-motion sources, capsule, collision profiles, `bPresentationHidden` presentation gate for footsteps/foley/Niagara/decals). ADR-0011 (`Hide(Enter/Exit)` entry points, `CanBegin` registration, slot arbitration, `BushEnter`/`BushExit` poses, owner prediction hook). ADR-0014 (`BushSite` in the Manifest; zero replicated actors). ADR-0016 (Crowd consumes `ICrowdPerceptionFilter`). ADR-0007/ADR-0008 (module placement, harness) |
| **Enables** | ADR-0018 (spot-zone and candidate queries), ADR-0019 (`IsInBushOrTransition` for takeover eligibility), ADR-0021 (Teammate Markers read `IsAllyBodyHidden`), ADR-0015 (concealed camera framing reads the owner `IsConcealed`), Informant Pings (system 20: server `IsInBushOrTransition`, owner `IsOwnPawnConcealedForPing()`) |
| **Blocks** | Concealment (Bushes) epic; Assassination blind-strike stories; Teammate Markers hatch rule; informant takeover eligibility |
| **Ordering Note** | Implements IVP R7's hybrid, which ADR-0003 mechanises on the wire; this ADR owns behaviour and presentation. The two fields once slotted in §8 were approved by the user 2026-10-25 and are specified there (ADR-0003, ADR-0018 amended the same day). S-CON-1 must report before the transition stories start. **Acceptance pair with ADR-0018** (architecture-review fix 2026-10-25): this ADR consumes ADR-0018's `FActionState`/`FDeathState` semantics, outcome slot vs death write, `BushStrike` variant and spot-pick consumer, and implements ADR-0018's consumer-owned `IBodyRemovalObserver`; ADR-0018 consumes this ADR's spot queries and kill hook. The two are reviewed and accepted together in one step. The edge is declared once, in ADR-0018's Depends On (ADR-0018 → ADR-0017), and not here, so the declared dependency graph stays acyclic. The Status line's "ADR-0018 … Accepted" prerequisite is read as "accepted together with ADR-0018" (the Status line is not edited; Status changes are reserved for the user) |

## Context

### Problem Statement

The locked rules say a hidden player is fully invisible to enemies yet can be blind-attacked. IVP R7 chose a hybrid: the server culls the pawn for non-allies (so no packet carries a concealed position) and every client hides the body locally at the leaf-plane instant of the enter clip (so the hide happens at occlusion, not on the network's schedule). Making that work needs decisions the GDDs mark "-> ADR": where per-pawn state lives without adding a replicated field to the pawn; how bushes exist without replicated actors; how clips and the hide are scheduled from server time (since 2026-10-25 via `StartServerTime`); how the client hide is fail-closed through re-creation, death and rejoin (since 2026-10-25 with `bDiedConcealed`); which primitive flags actually remove a hidden body from shadows, Lumen and ray tracing; how the owner's warp avoids a correction hitch; and how NPCs use the same path. The prototype proves per-viewer concealment is feasible (PIE test E PASS) but leaks: it replicates `bConcealed` to everyone, keys concealment on overlap, and hides with a client `SetVisibility` driven by that flag (first-playable report, known issue 2).

### Constraints

- ADR-0001: one pawn class; Owner data only on the owner's PlayerController; Team data only through C-TeamView; no team relevancy.
- ADR-0003: no concealment, team, cooldown or binding field on the pawn; at most the one shared `FActionState` component replicates gameplay state; the cull is the Policy's, pushed through `IConcealmentCullSource`; `ForceNetRelevant` banned on pawns.
- ADR-0002: every interacting state change resolves in the queue; timers are due-time-heap items; presentation schedules from `EstimatedServerNow()`.
- IVP R7.6: NPC parity mandatory; no automatic client-hide fallback in any build that leaves the team.
- Concealment has **no timer of any kind on dwell** and no capacity or occupancy check (CON-R3, CON-R7).
- `FActionState.StartServerTime` and `FDeathState.bDiedConcealed` were approved by the user 2026-10-25 (§8).
- Listen host: the host holds every server fact; its presentation must still run the same latch (CON-R16).

### Requirements

- Per-viewer result `IsConcealedFrom(Viewer, Pawn)` per IVP R7.8, server-only (CON-R10).
- Enter/exit/emerge timing chain F3 (a)-(f); commit at the swap; un-cull at exit execution; body shown only at the outward crossing (CON-R5, R8, R9).
- Fail-closed client latch `IsAllyBodyHidden` with fresh-actor evaluation and the CON-R6b precedence rules.
- Hide removes body, attachments, shadow, Lumen, reflections, ray tracing, decals, foley, Niagara and audio (VA-4).
- Collision: hide volumes block outside pawns; transitioning/concealed pawns ignore volumes and pawns; state re-derived, never stored by a notify (CON-R4).
- Owner warp with no visible snap (CON-R4a, S-CON-1).
- Blind-strike spot zone and candidates (CON-R12, F4); terminal bush rule.
- Rustle parity (CON-R13, VA-2); concealment log (CON-R15); host parity (CON-R16); NPC use of the same path (CON-R14).

## Decision

### 1. Placement: one non-replicated component, one server service, one local bush runtime

| Piece | Kind | Runs on | Owns |
|---|---|---|---|
| `UConcealmentComponent` | `UActorComponent` on **every** `AStealthPawn` (same class, creation order and flags on every pawn), `SetIsReplicatedByDefault(false)` | server and clients | server: the pawn's state `EConcealState {Outside, Entering, Concealed, Exiting, Emerging}`, `BushId`, `TransitionGeneration`, `TransitionStartServerTime`, `CommitSeq`, last-exit completion time (cooldown); client: the body-hidden latch (§5) and `bPresentationHidden` application (§6). The server-side fields never replicate (allowlist: zero rows for this class) |
| `UConcealmentService` | `UWorldSubsystem` facade over a plain C++ `FConcealmentCore` (headless-testable) in `StealthGameplay/Features/Concealment/` (`Public/Server/` headers guarded) | server | occupancy records per `BushId` (set of pawns in `Concealed`, or `Exiting`/`Emerging` before their crossing, with commit sequence ids); the `CanBegin(Hide)` predicate; resolvers for `Hide` and the transition timers; `IConcealmentCullSource` calls; spot queries; `ICrowdPerceptionFilter`; concealment log rows |
| `UBushRuntimeSubsystem` | `UWorldSubsystem` | server **and** clients | `FBushRuntime {BushId, Footprint (2.0 × 2.0 m), HideVolume (footprint × 2.25 m), EntryAxis set, Anchor}` built from `FLayoutManifest` `BushSite` entries at `OnLayoutReady` (server) / Manifest build (client) (CON-R1); one locally spawned, **non-replicated** `ABushVolumeSet` actor holding one box component per bush on the `BushVolume` object channel (E5); the rustle MPC writer (§9) |
| `UConcealmentOwnerViewComponent` | replicated `UActorComponent` on `AStealthPlayerController`, registered sub-object (ADR-0001 owner-data family; the PlayerController is relevant only to its owner) | server → owner | `bIsConcealed` for the owner (IVP R3 `IsConcealed` row, audience Owner). Allies get the same fact from the Roster C-TeamView teammate entry `bIsConcealed` (ADR-0001), which Concealment sets only through the Roster server face `SetTeammateConcealed(FParticipantId, bool)` (ADR-0001 §5): Concealment is its only caller, and the Roster stays the only writer of the replicated entry (its push step writes it). Needs an IVP allowlist row (process, owner sign-off) |

The bush mesh itself (VA-1: one mesh family, one MI, no per-instance data) is placed by the Layout Service / Generator from the same Manifest and is not Concealment's. Concealment spawns **zero replicated actors** (LSC R2.6).

### 2. Commands and legality (one path for every pawn)

- `Hide(Enter(BushId))` / `Hide(Exit(Side))` enter through `UPawnCommandComponent` (ADR-0011) for humans and Crowd alike; Concealment registers its `CanBegin` predicate for `HideEnter`/`HideExit` from the composition root. No input to legality, timing or reply is controller kind, team, occupancy or any other pawn's state (CON-R3, CON-R14).
- **Enter legality** (CON-R3): IIF class/locomotion legality (not running, not traversing, not `RoundEnded`) → `NotNow`; slot → `Busy`; horizontal capsule-centre distance ≤ `BushEntryReachCm` to the planter's outer edge on an open side → else `OutOfReach`; own re-entry cooldown `BushReenterCooldownSec` elapsed since the last exit/emergence completion → else `Cooldown`. **Occupancy is never read.** Entry side = open side nearest the pawn at execution.
- **Exit** (CON-R8): side index validated (closed side → nearest open); blocked-side search over **blocking pawns** only (`Outside`, or past their crossing); never refused; depenetration capped at `BushExitDepenetrationCapCm`.
- **Inside** (CON-R7): `Move` produces no displacement; body yaw locked at the end of `BushEnter`; `Look` free within the head clamp; Prepare/Interact `NotNow` (IIF R9). There is **no dwell, capacity, reveal or timeout key or timer** anywhere (AC-CON-12 whitelist check).

### 3. Transitions as queue items (server)

- `HideEnter` executes in the command's slot: state `Entering`, `TransitionGeneration++`, `TransitionStartServerTime = ServerNow()`, `FActionState {Pose = BushEnter, StartServerTime, ...}` written with the start time (§8 A), and the root-motion warp to the anchor started (§4). Two timer items are scheduled with `ScheduleAt` (ADR-0002 §4): `Commit` at `Start + BushEnterSwapSec`, `End` at `Start + BushEnterDurSec`. `HideExit` likewise schedules `Cross` at `Start + BushExitCrossSec` and `End` at `Start + BushExitDurSec`. The bush-emerge strike (Attack while `Concealed`, executed by Assassination's slot) schedules `Cross` at `ContactServerTime − StrikeContactSec` (the same formula clients use, §8 slot A, so server and clients cannot round differently).
- These are `TimerFired(ConcealmentTransition)` items carrying `{PawnHandle, Phase ∈ {Commit, Cross, End}, TransitionGeneration, FEpochToken}` in ADR-0002 §2a's generic Foundation `FTimerPayload` (`Subject` = the pawn's `FPawnHandle`, `Generation` = `TransitionGeneration`, `Phase` = `EConcealPhase`, `Seq` = 0; the epoch rides on the item); **this needs an ADR-0002 amendment** adding `ConcealmentTransition` to the closed `ETimerKind` set (an internal enum; no replicated schema changes) **and naming it as a third post-latch exemption**: these items keep resolving after the latch, may write only Concealment state, the cull record and the Owner/Team concealment flags (never an outcome, a Roster transition or a win), and are dropped by generation/epoch at `Reset`. Without it a `Commit` rejected after the latch would leave an `Entering` pawn unculled, its position on the wire in `Ended`, breaking "concealed pawns stay culled". An item whose generation or epoch is stale, or whose pawn is dead or pending teardown, resolves `Voided` with no effect (States table rule; ADR-0002 §7).
- **Commit** (in the item's slot): guard — the capsule lies wholly inside the `HideVolume`; on failure no commit, `ReportFault(ConcealGuard)`, and an ordinary exit on the entry side follows (AC-CON-29): during `Active` through the normal Hide path; after the latch as an **internal transition** run by the same resolver (state `Exiting`, `BushExit` pose, `Cross`/`End` items), never a `PawnCommand`, which would be `Rejected_Latched`. On success: state `Concealed`, occupancy record insert with `CommitSeq`, `IConcealmentCullSource::SetCullState(Pawn, true, ServerNow())` (ADR-0003 §5: `CullPending`, deadline on the global grid), Roster `SetTeammateConcealed(Participant, true)` for the pawn's participant if bound, owner `bIsConcealed = true`, log row `Commit`, host latch event (§5).
- **HideExit execution / `OnConcealmentBroken`**: `SetCullState(Pawn, false, ...)` **in the same slot** (un-cull in the same server step, ADR-0003 §5.6); state `Exiting`/`Emerging`. **Cross**: leave the occupancy record; Roster `SetTeammateConcealed(Participant, false)` and owner `bIsConcealed = false`; log row; host latch event. **End**: state `Outside`; cooldown starts.
- **Kill of a concealed pawn** (Assassination's outcome slot, ADR-0018): leave the occupancy record in the slot; at the death write push `bConcealed = false` in the staging pass so the victim re-opens carrying `FDeathState.bDiedConcealed = true` (ADR-0003 §5.7; user decision 2026-10-25); **do not** clear Owner/Team `bIsConcealed` until the body's removal (cross-review S-W2; CON-R9). The removal instant is a **server** event: Concealment clears both flags at the server's body removal of that pawn: ADR-0018's `BodyRemoval` resolver calls the consumer-owned `IBodyRemovalObserver::OnBodyRemovalCommitted(Body)` (declared by ADR-0018, implemented here by `UConcealmentService::OnConcealedBodyRemoved`, registered by the composition root) in the same slot, just before ADR-0002 `RequestTeardown`; **or** when the participant is bound to a new pawn (`BindPawn`/`TakeOverCrowdPawn`, ADR-0010/ADR-0019), whichever comes first — the flags are per participant, so they must never carry concealed framing or the ally latch onto the new body. A body that already exists on an ally client stays hidden after the clear by CON-R6b rule 4 (death after the commit never clears the latch). ADR-0004 is asked to add the cross-field warning `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < InformantRespawnDelaySec` so the normal order is removal first. Nothing re-opens in the outcome slot; the re-open happens only at the death write (§8 B).
- **Same-tick ordering** is the queue's: commands, kills and these items order by key (Concealment Edge Cases); two commits in one frame share one cull boundary (ADR-0003 §5.3).
- **Ownership changes** (CON-R11): a bound-AI hold never issues Hide; transitions in flight complete; `ReleaseToCrowd` leaves the pawn `Concealed` (the Policy re-evaluates former allies on the Roster's binding event, ADR-0003 §5.5); Crowd later exits it with an ordinary `Hide(Exit)` (ADR-0016 §7).
- **Phases**: in `Ended` Hide commands are rejected `NotNow`/`RoundEnded`, in-flight transitions complete through the post-latch exemption above, concealed pawns stay culled; `Reset` destroys every pawn, drops every pending item by epoch and clears every record on `OnRosterReset`.

### 4. Owner-side warp and collision (spike S-CON-1 picks the mechanism)

- The warp is `FRootMotionSource_MoveToForce` from the execution position to the fixed bush anchor over `BushEnterSwapSec`-bounded calm gait (F3 d), applied on the server for every pawn; NPC and player use the same server path (E4).
- **Owner smoothness**: S-CON-1 chooses between (a) the owner starting the **identical** source (same `InstanceName`, parameters and start server time) when its `FActionState` arrives through replication — **no new message**; the residual correction arrives through `ClientAdjustRootMotionSourcePosition` and is blended — or, if that is too late, the GDD's owner-only start message; and (b) suppressing correction hitches for the transition window only: server-side `bIgnoreClientMovementErrorChecksAndCorrection` (stops the server **generating** corrections) plus owner-side `bClientIgnoreMovementCorrections` (stops the client **applying** them) for the transition length plus one RTT, then a normal correction to the server's authoritative end position, which is the fixed anchor or exit point. The server never takes the client's position: `bServerAcceptClientAuthoritativePosition` stays banned (ADR-0003 §3, E4). Order of trial: (a) via replicated `FActionState` first (adds no wire element), then (b); an owner-only start message is tried last because it is a new owner wire element and needs an IVP allowlist row. AC-CON-31 is the acceptance test. The choice is recorded by amending this section.
- **Collision is a pure function** `ComputeConcealCollision(State, ElapsedInTransition)` evaluated on the server and on the owner (autonomous proxy) for each simulated move, never set by an anim notify (E6). During a correction replay the elapsed time is taken from **each saved move's own timestamp mapped to server time** (or the state is carried in a custom `FSavedMove_Character` field, ADR-0010's saved-move extension), never from the current `ServerNow`, which would give replayed moves the wrong state. Collision responses are written only when the computed state actually changes (each change refreshes the body's physics state, which is too expensive per tick). States: `Outside` → capsule blocks `BushVolume` and `Pawn`; `Entering`/`Concealed`/`Exiting`/`Emerging` → capsule ignores `BushVolume` and `Pawn`. `Move` input is zeroed from transition start on both sides. Nothing may use a concealed pawn as `Instigator`. The `BushVolume` object channel and the presentation aim channel (§6) are collision-profile entries in `Config/DefaultEngine.ini`, added by the implementation story (this pipeline cannot edit Config).

### 5. The client latch: one state-derived, fail-closed function

```
bool EvaluateBodyHidden(const FActionState& A, const FDeathState& D, bool bAllyIsConcealed /*ally clients*/,
                        bool bIsOwner, double Now /* EstimatedServerNow() */);
```
- **Owner**: always false (the owner keeps its body; CON-R6).
- **Set** while: a `BushEnter` pose with `Now ≥ Start + BushEnterSwapSec`; `bAllyIsConcealed` true (ally clients, from C-TeamView); a `BushExit` pose with `Now < Start + BushExitCrossSec`; a bush-emerge strike with `Now < Start + BushStrikeEmergeSec`; `D.bDiedConcealed` (§8 B).
- **Clear** only when no set rule holds **and** the last transition evidence is an exit/emerge crossing that has passed, or there is no concealment evidence at all (pose `None`, ally flag false). A pose change to `None`, a death pose or a strike pose never clears it; death after the swap never clears it (CON-R6b rules 2-4).
- **Precedence** (CON-R6b rule 4): `BushEnter` before its swap with the ally flag false waits for the swap; the ally flag true before the pose sets at once; death before the commit cancels the pending swap (ordinary visible kill).
- **Fresh actor** (re-open, rejoin, late arrival): every non-owner pawn actor is created **presentation-hidden** at construction / `PostInitializeComponents` (before any RepNotify can run, E8); no RepNotify before `PostNetInit` may clear it. The latch is first evaluated in `PostNetInit` from the full replicated state and clears only if no set rule applies (AC-CON-08). **Unresolved ally evidence:** on an ally client a `Concealed` teammate's only evidence after its pose returns to `None` is the C-TeamView flag, reached through a `PawnRef` that cannot resolve before the new actor's `PostNetInit` (E8). So while any living teammate entry on this client has an unresolved `PawnRef`, a fresh actor does **not** clear its hidden state; it re-evaluates when that `PawnRef` resolves (or the entry changes), bounded by the TM-R4a one-update rule (the fallback feed covers the marker meanwhile). New test: ally rejoin and re-open while a teammate is `Concealed` with pose `None` show no body frame (extends AC-CON-08, AC-CON-32).
- **When it runs**: on every client, from the pawn's single idempotent `RefreshPresentation(EstimatedServerNow())` path (architecture Data Flow §2) on RepNotify of `FActionState`/`FDeathState`, on C-TeamView change, and each frame while a hide pose is within its key window — never from arrival order and never from an anim notify (E6). Clips are **seeked** to `Now − Start`, never restarted or snapped (CON-R6a).
- **Listen host** (CON-R16): the host has no RepNotify for its own writes; the server's `Commit` and `Cross` items call the same `RefreshPresentation` on the host's local copy in the same step, and the host also runs the per-frame evaluation, so the host latch is the identical function with `Now == ServerNow()` (ADR-0002 host parity).
- **Public client read**: `bool IsAllyBodyHidden(const AStealthPawn*)` (Foundation-safe `Public/View` facade) returns the latch; Teammate Markers TM-R5 consumes it (ADR-0021). Opponents run the same latch but lose the actor when the cull lands.

### 6. `bPresentationHidden`: what hiding a body means

When the latch is set on a client (or the host), `UConcealmentComponent` applies, client-locally and never replicated:
- `SetVisibility(false, /*propagate*/ true)` on the mesh and every attached primitive (body, hair, garments, props). Because a hidden primitive gets **no render proxy** unless one of three gating flags keeps it (E2), those three are fixed on the **class defaults** of every pawn primitive and asserted by the ADR-0003 audit (flag values; cross-pawn equality of the whole render-flag set is ADR-0022's `StealthGame.Pawn.RenderFlagParity`), never toggled at runtime: `bCastHiddenShadow = false`, `bAffectIndirectLightingWhileHidden = false`, `bRayTracingFarField = false`. With no proxy there is no shadow, Lumen, ray-tracing, reflection or sky-capture contribution, so runtime toggles of `bVisibleInRayTracing`/`bVisibleInReflectionCaptures`/`bVisibleInRealTimeSkyCaptures` are unnecessary. Spike R5 runs with `bCreateRenderStateForHiddenComponentsWithCollsion` off and covers VSM cache pages, groom components and attached Niagara (E2 [U]).
- **Pose freshness** (E6): while a hide window is active (from the swap until the crossing) the mesh keeps `AlwaysTickPose`, or its pose is refreshed before it is shown, so the first visible frame is the seeked pose, not a stale one.
- The Shared Pawn's presentation gate `IsPresentationHidden()` (ADR-0010) suppresses footstep decals, foley, Niagara, spawned sounds and the audio emitter.
- The pawn's response on the **presentation aim channel** (a client-local trace channel used by aim, Strike-hint and prompt traces on every client, the host included) switches to ignore, so a hidden non-ally can never be found by a trace on any client; shared physics collision is unchanged and server hit tests use capsule math (CON-R16).
- **Never** `SetActorHiddenInGame`, `bHidden` or anything replicated (E3).
- The eye-mask dither of the Camera (ADR-0015) composes as an override: a hidden body is hidden whatever the mask says.

### 7. Server queries: per-viewer result, spot zones, eligibility, perception filter

- `bool IsConcealedFrom(FViewerId, const AStealthPawn*) const` = `State ∈ {Concealed} ∪ {Exiting, Emerging before Cross}` (or killed while concealed, until the death write) ∧ `¬CanViewerKnow(Viewer, Pawn, Position)` per IVP R7.8's answer over time; server-only; the truth table is AC-CON-05's checked-in table.
- `TOptional<FBushId> TargetsBush(FVector WorldPoint) const`: horizontal distance from `WorldPoint` to a footprint ≤ `BushSpotToleranceCm` (F4; zones never overlap at `BushSpacingCm`).
- `void GetSpotOccupants(FBushId, const AStealthPawn* Attacker, TArray<AStealthPawn*>& Out) const`: the occupancy record minus the attacker and its allies (Roster `GetRelation`), including pawns in `Exiting`/`Emerging` before their crossing, ordered by `CommitSeq`. Assassination (ADR-0018) filters by its F1 reach at each candidate's server position at the slot, then picks one. **The pick** uses `FSecretStream(GetServerSubSeed(BushSpotPick, LayoutAttempt), Tag("CON_SPOT"))` and ADR-0006's unbiased `UniformBelow` over a draw keyed by `(ServerTickIndex, AttackerCommandSeq, BushId)`, replacing F4's `H(...) mod n` (unbiased, and the salt comes from the secret service, never `RoundSeed`). **This needs an ADR-0006 amendment** appending a `BushSpotPick` domain (value assigned at amendment; not chosen here to avoid clashing with concurrent amendments).
- `bool IsInBushOrTransition(const AStealthPawn*) const`: true for every state except `Outside` (AC-CON-30); consumed by Crowd's eligibility and Penalty (ADR-0016 §8, ADR-0019).
- `ICrowdPerceptionFilter` (ADR-0016 §5): `IsPerceivable = State != Concealed`; `IsSteeringObstacle = State == Outside`; identical for every pawn kind, no viewer input. Concealment exposes **no occupancy query** to Crowd.

### 8. The two fields (approved by the user 2026-10-25; formerly slots with stopgaps)

**A — `FActionState.StartServerTime`** (Concealment CR 10 (b); approved 2026-10-25). One more fixed 64-bit member (ADR-0003 §4 keeps the struct fixed-width), written in every `FActionState` write: for `BushEnter` and `BushExit` the transition start (`TransitionStartServerTime`); for the bush-emerge strike (pose `Strike`, variant `BushStrike`) the emerge start, so the crossing is `StartServerTime + BushStrikeEmergeSec`. `ContactServerTime` keeps contact-only meaning. `EvaluateBodyHidden` and the clip seek read `StartServerTime`; the earlier stopgap (`ContactServerTime` carrying the start for hide poses; `HidePoseStartServerTime`) is retired.

**B — `FDeathState.bDiedConcealed`** (Concealment CR 10 (a); approved 2026-10-25; IVP R7.5, CON-R9, ASN-R10). A pawn killed while `Concealed`/`CullPending` re-opens at the death write (Concealment un-culls in the staging pass, same step as the death write) carrying `bDiedConcealed = true`; the latch's B set rule hides the body on every non-owner client (allies and opponents) through fall, linger and removal; the owner sees its own fall. The C-TeamView `bIsConcealed` hold until body removal stays (it keeps the victim's own concealed framing and is harmless for allies). An NPC killed while concealed follows the same path. The reconciliation-era interim (`KilledConcealed` cull reason, never re-opening the victim to non-allies) is retired.

### 9. Rustle and audio parity (presentation, client-local)

- A stateless `MPC_BushRustle` with **8 ring-buffered pulse slots** `{WorldPos.xyz, StartTime; SideDir.xy, Amplitude, unused}` (E7); every bush material runs the same WPO function and responds only to a pulse within ~1.5 m of its face (VA-2). Each client writes a slot when `RefreshPresentation` sees a `BushEnter`, `BushExit` or bush-emerge pose for any pawn, timed from the pose's start server time (onset 0.1 s before the swap or crossing), at the bush face on the transition's side; slots are reused round-robin client-locally. Identical for every pawn kind, both camera modes, every LOD, never scaled by occupancy, absent while a bush is merely occupied (CON-R13).
- One rustle SFX pool, spatialised at the bush face, fired by the same handler; nothing audible while occupied (VA-6). The rustle is tagged eye-masked for the third-person mask (Camera CS-R5b, ADR-0015).

### 10. Concealment log

Server-only rows `{Tick, BushId, PawnKind, Event ∈ {Commit, ExitCross, Emerge, Killed, BushStrike}}` (with candidate-set sizes before/after the reach filter on `BushStrike`, kill flag on `Emerge`) appended to the Round Event Log slice (ADR-0002/ADR-0007 Event Log store), never through `UE_LOG`/`UE_LOGF`, never released to clients in MVP (CON-R15). `PawnKind` is read only here, for metrics.

### 11. Switch and parity

`bConcealServerCull = false` (client-side hide only) exists only in internal development builds (IVP R7.6, L29 CRITICAL); Concealment's own rules do not change in that mode. No automatic fallback exists if Crowd misses F5. Every rule above is identical for NPC and player pawns.

### Architecture Diagram

```
 SERVER (listen host)                                               CLIENT (non-owner)                      OWNER CLIENT
 ┌─ UPawnCommandComponent::Hide (human RPC / Crowd sink) ─► mailbox
 ├─ TG_PostPhysics drain ───────────────────────────────────────────┐
 │  Hide resolver (UConcealmentService): CanBegin re-check          │
 │    Enter: state Entering, gen++, FActionState{BushEnter,         │──► FActionState (All, fixed width) ─► RefreshPresentation(EstNow)
 │           StartServerTime=Start}, warp MoveToForce    │      seek clip to Now−Start             (same, own body kept;
 │           ScheduleAt Commit(+Swap), End(+Dur)                    │      latch: hide at Start+Swap          warp per S-CON-1)
 │  TimerFired(ConcealmentTransition):                              │
 │    Commit: guard ─► Concealed, occupancy+CommitSeq,              │
 │      IConcealmentCullSource::SetCullState(true) ─► Policy        │──(ADR-0003 cull tick, grid boundary)──► channel closed (non-allies)
 │      Roster team view bIsConcealed / owner component ────────────┼──► allies: C-TeamView flag ─► latch set, hatched marker
 │    Exit exec / Broken: SetCullState(false) same slot             │──► re-open; body hidden until Start+Cross
 │    Cross: leave occupancy, flags false; End: Outside, cooldown   │
 │  Attack slot (ADR-0018): GetSpotOccupants ─► reach filter ─►     │
 │    BushSpotPick secret draw ─► victim; re-open at death write   │──► all: falling body, kept hidden (bDiedConcealed)
 │  host: Commit/Cross items call RefreshPresentation on host copy  │
 └──────────────────────────────────────────────────────────────────┘
 Every machine: UBushRuntimeSubsystem builds FBushRuntime + local non-replicated ABushVolumeSet from FLayoutManifest
 Every client: MPC_BushRustle pulse slots written from poses; presentation aim channel ignores hidden pawns
```

### Key Interfaces

```cpp
// --- StealthGameplay/Features/Concealment (Public/Server) ------------------------------------
enum class EConcealState : uint8 { Outside, Entering, Concealed, Exiting, Emerging };

UCLASS() class UConcealmentComponent : public UActorComponent      // on every AStealthPawn; never replicated
{   GENERATED_BODY()
public:
    bool IsPresentationHidden() const;                                // client/host latch result
    void RefreshPresentation(double EstimatedNow);                    // idempotent, state-derived (§5)
private:
    // server-only: EConcealState State; FBushId BushId; uint32 TransitionGeneration;
    //              double TransitionStartServerTime; uint32 CommitSeq; double LastExitCompleteServerTime;
};

UCLASS() class UConcealmentService : public UWorldSubsystem, public ICrowdPerceptionFilter,
      public IBodyRemovalObserver /* ADR-0018 §6.2 */
{   GENERATED_BODY()
public:   // server only; facade over FConcealmentCore (plain C++)
    ECommandVerdict CanBeginHide(const AStealthPawn*, const FHideCommand&) const;  // IIF predicate
    bool  IsConcealedFrom(FViewerId Viewer, const AStealthPawn* Pawn) const;       // IVP R7.8
    bool  IsInBushOrTransition(const AStealthPawn* Pawn) const;
    TOptional<FBushId> TargetsBush(const FVector& WorldPoint) const;               // F4 zone
    void  GetSpotOccupants(FBushId Bush, const AStealthPawn* Attacker,
                           TArray<AStealthPawn*>& OutByCommitSeq) const;           // minus attacker + allies
    void  OnConcealedPawnKilled(AStealthPawn* Victim);       // outcome slot: leave record (the un-cull is staged with the death write, bDiedConcealed; MarkKilledConcealed retired)
    void  OnConcealedBodyRemoved(AStealthPawn* Victim);      // clears Owner/Team bIsConcealed (S-W2)
    virtual void OnBodyRemovalCommitted(AStealthPawn* Body) override; // every body; forwards to OnConcealedBodyRemoved if killed concealed
    // ICrowdPerceptionFilter (ADR-0016)
    virtual bool IsPerceivable(const APawn*) const override;       // State != Concealed
    virtual bool IsSteeringObstacle(const APawn*) const override;  // State == Outside
};
// Resolvers registered from the composition root: PawnCommand(Hide) via IIF; TimerFired(ConcealmentTransition).
// No payload struct here: transition items use ADR-0002 §2a's Foundation FTimerPayload
//   {Kind = ConcealmentTransition, Subject = FPawnHandle, Generation = TransitionGeneration,
//    Seq = 0, Phase = (uint8)EConcealPhase {Commit, Cross, End}}; epoch on FQueueItem::Expected.
enum class EConcealPhase : uint8 { Commit, Cross, End };

// --- Local bush runtime (server + clients) ------------------------------------------------------
struct FBushRuntime { FBushId Id; FBox2D Footprint; FBox HideVolume; uint8 EntryAxisMask; FVector Anchor; };
UCLASS() class UBushRuntimeSubsystem : public UWorldSubsystem
{   GENERATED_BODY()
public:
    void BuildFromManifest(const FLayoutManifest&);          // spawns one local ABushVolumeSet (bReplicates=false)
    const FBushRuntime* Find(FBushId) const;
    void PushRustlePulse(FBushId, uint8 Side, double StartServerTime);   // client-local MPC slot
};

// --- Owner carrier (root module PC; Owner audience; allowlist row required) --------------------
UCLASS() class UConcealmentOwnerViewComponent : public UActorComponent
{   GENERATED_BODY()
    UPROPERTY(Replicated) bool bIsConcealed = false;          // owner only (PC relevancy)
};

// --- Presentation-safe read (Public/View) --------------------------------------------------------
bool IsAllyBodyHidden(const AStealthPawn* Pawn);              // client-local latch; Teammate Markers TM-R5

// --- Pure functions (headless-testable) ----------------------------------------------------------
bool EvaluateBodyHidden(const FActionState&, const FDeathState&, bool bAllyIsConcealed,
                        bool bIsOwner, double Now, const FStealthClientTuning&);
// HidePoseStartServerTime retired 2026-10-25: read FActionState.StartServerTime (§8 A)
// Tuning: the ADR-0004 client face; reads FConcealmentTuning_Public and FAssassinationTuning_Public
// (BushStrikeEmergeSec, StrikeContactSec) through it — no Concealment-specific client tuning type.
FConcealCollision ComputeConcealCollision(EConcealState, double ElapsedInTransition);
```

### Implementation Guidelines

- `UConcealmentComponent` must exist on every pawn with identical class and flags and must never replicate; no concealment field may ever be added to the pawn's replicated set.
- Bushes must be built locally from the Manifest on every machine; Concealment must spawn no replicated actor.
- Every Hide must enter through `UPawnCommandComponent`; legality, replies and timing must never read occupancy, controller kind or team; no dwell, capacity, reveal or timeout key may exist.
- Commit, crossing and end must be `TimerFired(ConcealmentTransition)` queue items carrying the transition generation; they must never be anim notifies, engine timers or feature ticks.
- Cull state must be pushed only through `IConcealmentCullSource` inside the slot; un-cull must happen in the same slot as exit execution or break; a concealed kill must re-open the pawn only at the death write, together with `bDiedConcealed = true`.
- Owner/Team `bIsConcealed` must be set at commit and cleared at the crossing, or at body removal for a killed-concealed pawn.
- Client presentation must hide and show bodies only through `EvaluateBodyHidden` from `EstimatedServerNow()`; every non-owner pawn actor must be created presentation-hidden and evaluated in `PostNetInit`; clips must be seeked, never restarted; anim notifies must never gate the hide.
- Hiding must apply the full §6 flag set and the presentation aim channel response; `SetActorHiddenInGame`/`bHidden` must never be used for concealment.
- Collision state must be computed per simulated move from that move's own server-mapped timestamp (or a saved-move field) on server and owner, written only on change, and never stored by a notify.
- `bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden` and `bRayTracingFarField` must be false on the class defaults of every pawn primitive (audited); concealment must never toggle them at runtime.
- After the latch, transition items must complete only through the ADR-0002 post-latch exemption and a guard-failure exit must be an internal transition, never a `PawnCommand`.
- Owner/Team `bIsConcealed` of a killed-concealed pawn must clear at the server body removal or on the participant's rebind, whichever comes first.
- A fresh non-owner actor must start hidden before any RepNotify and must not clear while a living teammate entry's `PawnRef` is unresolved on that client.
- The owner warp must never use `bServerAcceptClientAuthoritativePosition`.
- The spot pick must use the secret `BushSpotPick` stream with an unbiased ranged draw; it must never use `RoundSeed`, a public seed or `mod` on the result.
- `StartServerTime` must be written in every `FActionState` write and read for every hide clip; `bDiedConcealed` must be set only in the death write and only for a victim in `Concealed`/`CullPending` (§8).
- Concealment log rows must go to the Event Log store only, never through `UE_LOG`/`UE_LOGF`.
- `bConcealServerCull = false` must be compiled or configured only into internal development builds.

## Alternatives Considered

### Alternative 1: Replicated `bConcealed` on the pawn with client-side hide (the prototype)
- **Description**: The server sets a replicated flag; every client hides the body when it arrives.
- **Pros**: Simple; works in PIE (prototype test E).
- **Cons**: Every client holds every occupant's position (x-ray for a modified client, IVP L29) and the flag is a permanent detector; the hide happens at network arrival, not at the leaf plane.
- **Rejection Reason**: Violates IVP R7 and ADR-0003 §8 (no concealment field on the pawn).

### Alternative 2: Overlap-driven concealment (enter by walking into the volume)
- **Description**: Concealment starts when the capsule overlaps a bush volume (prototype `SetInBush`).
- **Pros**: No command; no warp.
- **Cons**: No legality, no timing chain, no NPC parity through the command path, physics-order-dependent commits, and a walk-in is not a chosen, visible tell.
- **Rejection Reason**: Violates CON-R3/R5/R14 and the queue ordering of ADR-0002.

### Alternative 3: Hide with `SetActorHiddenInGame` / `bHidden`
- **Description**: Use the actor's hidden flag.
- **Pros**: One call hides everything, including some shadow paths.
- **Cons**: `bHidden` replicates and feeds relevancy (E3): it would leak to every client and interfere with the cull.
- **Rejection Reason**: Wire leak.

### Alternative 4: Separate client timers / anim notifies for the swap and crossing
- **Description**: Start a client timer or rely on clip notifies when the pose arrives.
- **Pros**: Matches common practice.
- **Cons**: Arrival differs per client and restarts on re-creation; notifies can be skipped on a hidden mesh (E6); a fresh actor would pop.
- **Rejection Reason**: Breaks the one-clock rule (CON-R6a) and fail-closed re-creation (CON-R6b).

### Alternative 5: Implement `StartServerTime` / `bDiedConcealed` now
- **Description**: Add the two fields the GDD change requests ask for.
- **Pros**: Cleaner semantics; killed-concealed bodies re-open like every other victim.
- **Cons**: Both are replicated-schema changes awaiting the user (always-ask, schema_changes).
- **Rejection Reason**: Not decidable here; the stopgaps (§8) are complete and the slots keep the upgrade local.

### Alternative 6: Per-bush replicated actors carrying occupancy for allies
- **Description**: A replicated bush actor per site with an ally-filtered occupancy list.
- **Pros**: Easy marker data.
- **Cons**: Team-filtered actors are banned (ADR-0001 §5); per-bush replicated objects must be identical on every bush (LSC R2.6) and would invite occupancy leaks.
- **Rejection Reason**: Ally data already rides C-TeamView `bIsConcealed`.

## Consequences

### Positive
- No concealment byte exists on the wire for any non-ally: position hiding is the Policy's cull, presentation is a client-local function of All-audience state plus the viewer's own carriers.
- One state-derived latch on every machine (host included) gives frame-exact swaps, fail-closed re-creation and no dependence on arrival order.
- NPCs and players share every line of code; Crowd sees only a perception filter, never occupancy.
- The two fields were isolated in slots, so their approval (2026-10-25) was a local amendment.

### Negative
- ~~`ContactServerTime` overload~~ removed 2026-10-25: `StartServerTime` carries the start.
- A killed-concealed body is re-opened at the death write but never drawn on non-owner clients (latch B); the per-viewer hide is what keeps it unseen.
- Two cross-ADR amendments (ADR-0002 timer kind, ADR-0006 seed domain) before implementation.
- The R5 leak matrix and S-CON-1 are prerequisites with real schedule cost.

## Risks

- **Hidden primitives still leak in some lighting path** (E2; AQ-07 R5). Mitigation: the full flag set is applied, not the minimum; R5 matrix gates the epic; magenta-proxy occlusion proof (AC-CON-23).
- **Owner warp hitches under latency** (S-CON-1). Mitigation: three ordered mechanisms (§4); AC-CON-31 gate.
- **Clip seek drifts from the server key on clients** (OQ9). Mitigation: `EstimatedServerNow` accuracy budget (ADR-0002: ≤ 15 ms p95), one-frame tolerance tested under `LeakNetProfile`.
- **Transition items after the latch.** ADR-0002 §8.1 rejects every non-post-latch item `Rejected_Latched`, but CON States require in-flight transitions to complete in `Ended` and concealed pawns to stay culled. Mitigation: the requested ADR-0002 amendment names `TimerFired(ConcealmentTransition)` as a third post-latch exemption limited to Concealment state, cull and flags (§3); until it is Accepted this ADR cannot be Accepted.
- **Participant rebinding before body removal** (informant respawn faster than fall + linger + removal). Mitigation: flags cleared on rebind (§3); ADR-0004 cross-field warning.
- **Fresh ally actor shown before C-TeamView resolves.** Mitigation: unresolved-`PawnRef` hold (§5) and its test.
- **Re-open race** (cull then un-cull before the client acks the close, ADR-0003 E10 [U]). Mitigation: F3 (a) guarantees every exit starts `Culled` at defaults; S10.
- **Spot-pick salt or domain clash** with concurrent ADR-0006 amendments. Mitigation: the number is assigned at amendment time.
- **Collision re-derivation under CMC replay** desyncs owner and server. Mitigation: pure function of replicated/predicted state; AC-CON-28 correction case.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| concealment-bushes.md | CON-R1 bushes from the Manifest, zero replicated actors | §1 `UBushRuntimeSubsystem`, local `ABushVolumeSet` |
| concealment-bushes.md | CON-R2 one component, server-only record; owner component placement (-> ADR) | §1 non-replicated component on every pawn; `UConcealmentOwnerViewComponent` on the PC; ally flag via C-TeamView |
| concealment-bushes.md | CON-R3 legality, no occupancy check; CON-R7 inside rules, no dwell timer; CON-R8 exit | §2 |
| concealment-bushes.md | CON-R4 collision as a pure function; avoidance exclusion | §4; §7 `ICrowdPerceptionFilter` |
| concealment-bushes.md | CON-R4a owner-side warp (-> ADR, S-CON-1) | §4 ordered mechanisms, AC-CON-31 |
| concealment-bushes.md | CON-R5 commit at the swap as a queue timer item; guard | §3 |
| concealment-bushes.md | CON-R6, R6a one clock from replicated server start time; R6b fail-closed latch | §5; §8 A |
| concealment-bushes.md | CON-R9 breaks; kill of a concealed pawn (rule in force since 2026-10-25) | §3; §8 B |
| concealment-bushes.md | CON-R10 per-viewer result | §7 `IsConcealedFrom` |
| concealment-bushes.md | CON-R11 ownership changes | §3 |
| concealment-bushes.md | CON-R12, F4 spot zone and pick | §7 (unbiased secret pick; ADR-0006 amendment) |
| concealment-bushes.md | CON-R13 rustle parity; VA-2 MPC pulse slots (-> ADR) | §9 |
| concealment-bushes.md | CON-R14 NPCs use the same path | §2; ADR-0016 |
| concealment-bushes.md | CON-R15 concealment log | §10 |
| concealment-bushes.md | CON-R16 listen-host latch, aim channel, no `bHidden` | §5 host path; §6 |
| concealment-bushes.md | VA-4 leak closures | §6 flag set, R5 |
| concealment-bushes.md | F3 timing chain (a)-(f) | §3 timers from tuning; validator invariants are ADR-0004's |
| information-visibility-policy.md | R7.1-R7.3 hybrid: server cull + client hide at the swap | §3 cull push (mechanism ADR-0003); §5-§6 |
| information-visibility-policy.md | R7.5 un-cull in the same step; kills (re-open at the death write with `bDiedConcealed`) | §3; §8 B |
| information-visibility-policy.md | R7.6 NPC parity, no automatic fallback | §11; ADR-0016 |
| information-visibility-policy.md | R7.7 registries tolerate relevancy destroy; fresh actor | §5 fresh-actor rule |
| information-visibility-policy.md | R7.8 `Position` answer over time | §7 `IsConcealedFrom` |
| teammate-markers.md | TM-R5 `IsAllyBodyHidden` | §5 public read |

## Performance Implications
- **CPU (server)**: per Hide a handful of queue items and set operations; occupancy sets are tiny (E[O] ≈ 0.34); negligible against the 1.0 ms "camera traces + concealment/IIF/objectives misc" line. The cull tick is ADR-0003's.
- **CPU (client)**: latch evaluation only on RepNotify and per frame while a hide pose is in its window (a few pawns at a time). Each hide or show rebuilds the skinned mesh's render proxy (`MarkRenderStateDirty`); whether that is coalesced to end of frame is [U] — profile it in R5 at the worst case of several simultaneous swaps.
- **GPU**: rustle WPO on bush leaf tiers (≤ 14 bushes, 8 pulse slots); budget owned by the art bible / ADR-0022 (OQ5).
- **Memory**: negligible.
- **Network**: one owner-only bool on the PlayerController; the C-TeamView flag already exists (ADR-0001); the cull **removes** bandwidth for culled pawns.

## Migration Plan
The prototype (`AStealthBush` overlap volumes, replicated `bConcealed`/`bRevealedUntilExit` on `AStealthCharacter`, client `SetVisibility`) proves per-viewer concealment and blind strikes are feasible (first-playable report tests E and F PASS). It is **replaced, not ported**, after this ADR is Accepted (ADR-0007 promotion rule): no replicated concealment field survives; bush actors become Manifest-built local runtime; overlap concealment becomes the command path.

## Validation Criteria
- AC-CON-01a/01b, 02, 03, 04, 05 (per-viewer visibility, cull timing, truth table).
- AC-CON-06-12 (legality, exit/uncull timing, fresh-actor latch, inside rules, timing chain, no dwell timer).
- AC-CON-13-15, 25, 26 (breaks, spot zone and pick, blind-strike parity, no immunity, killed-concealed with `bDiedConcealed`).
- AC-CON-16-23, 27-32 (lifecycle, rustle parity, NPC same path, host parity, F5 log, Manifest parity, R5 matrix, collision, guard, eligibility, owner warp, rejoin).
- Spikes S-CON-1, R5, S9, S10 and OQ9 reported.

## Related
- ADR-0001 (carriers; C-TeamView `bIsConcealed`), ADR-0002 (queue, `EstimatedServerNow`; `ETimerKind::ConcealmentTransition` and its handling after the latch, amendment applied), ADR-0003 (cull mechanism, `IConcealmentCullSource`, death-write re-open with `bDiedConcealed`, `FActionState` fixed width, slot A), ADR-0004 (Concealment keys and F3 cross-field invariants), ADR-0006 (`BushSpotPick` domain 14, amendment applied), ADR-0010 (root motion, capsule, presentation gate), ADR-0011 (Hide entry points, predicate, poses), ADR-0013 (clip seeking), ADR-0014 (Manifest `BushSite`), ADR-0015 (concealed boom, eye mask), ADR-0016 (`ICrowdPerceptionFilter`), ADR-0018 (outcome slot, death write, spot-pick consumer, slot B), ADR-0019 (eligibility), ADR-0021 (Markers).
- GDD sync items (not edited here; GDDs are owned by their authors): **Concealment** CON-R6a says "GameState server clock" — read as `EstimatedServerNow()` (ADR-0002); F4's `H(...) mod |Reachable|` becomes an unbiased secret-stream draw (`BushSpotPick`); CON-R2 "-> ADR (owner component placement)" resolved as `UConcealmentOwnerViewComponent`; CON-R4a mechanism order recorded (§4); VA-2 "-> ADR" resolved (§9). **Round** R13 item table — add `TimerFired(ConcealmentTransition)`. **IVP** allowlist — add the owner component's `bIsConcealed` row (Owner audience, existing R3 fact). **Assassination** — confirm bush-emerge start derivation `ContactServerTime − (BushStrikeEmergeSec + StrikeContactSec)` under slot A's stopgap, and that `StrikeContactSec` has `All` audience (clients need it); ADR-0018 is the consumer of the `BushSpotPick` stream. **ADR-0003 §5.4** says "swap notify": read as the server-time swap (§5; no notify gates the hide). **ADR-0002 amendment** (coordinate with ADR-0016's concurrent `CrowdSchedule` request): `ConcealmentTransition` timer kind as a third post-latch exemption. **ADR-0004**: cross-field warning `DeathFallSec + BodyLingerSec + BodyRemovalGraceSec < InformantRespawnDelaySec`.
- Design: `design/gdd/concealment-bushes.md`; `design/gdd/information-visibility-policy.md` R7, F3-F5; `design/gdd/teammate-markers.md` TM-R5; `design/gdd/assassination-defence-reversal.md` ASN-R8, R10, R21.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §6: cross-reference to ADR-0022's `RenderFlagParity` (ADR-0003 audit keeps the flag values; `RenderFlagParity` owns cross-pawn equality) — requested by ADR-0022 (sync item 8).

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- §3, Key Interfaces: `FConcealTransitionPayload` removed; transition items use ADR-0002 §2a's Foundation `FTimerPayload` (`Subject` = `FPawnHandle`, `Generation` = `TransitionGeneration`, `Phase` = `EConcealPhase`); `FPawnHandle` is ADR-0002's — architecture-review fix 2026-10-25 (Conflict 1, blocking).
- §1, §3: the teammate `bIsConcealed` is set only through the Roster server face `SetTeammateConcealed(FParticipantId, bool)` declared in ADR-0001 (Concealment the only caller, the Roster the only writer) — architecture-review fix 2026-10-25 (Conflict 5).
- §3, Key Interfaces: the killed-concealed flag clear is driven by ADR-0018's consumer-owned `IBodyRemovalObserver::OnBodyRemovalCommitted`, implemented by `UConcealmentService` — architecture-review fix 2026-10-25 (Conflict 6).
- Key Interfaces: `FConcealClientTuning` removed; `EvaluateBodyHidden` and `HidePoseStartServerTime` take ADR-0004's client face `FStealthClientTuning` and read `FConcealmentTuning_Public` and `FAssassinationTuning_Public` through it. This is the reviewer's naming fix, applied as the client face because the stopgap reads two owners' keys (`BushStrikeEmergeSec`, `StrikeContactSec`) — architecture-review fix 2026-10-25 (Conflict 7).
- ADR Dependencies: ADR-0018 moved from Depends On to the Ordering Note as an acceptance pair — architecture-review fix 2026-10-25 (dependency cycle).
- Requests answered (2026-10-24): the ADR-0002 `ConcealmentTransition` amendment and its post-latch exemption, the ADR-0006 `BushSpotPick = 14` domain, the ADR-0003 §5.4 reading and the ADR-0004 cross-field warning (queued for the TDC GDD) are all applied by their owners; the Status line's "with the `ConcealmentTransition` timer amendment" condition is met — architecture-review fix 2026-10-25 (Conflict 9).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §8 A, §3, Key Interfaces, guidelines — user decision 2026-10-25: `FActionState.StartServerTime` adopted; `ContactServerTime` no longer carries hide-pose start times; `HidePoseStartServerTime` stopgap retired.
- §8 B, §3, §5, diagram, guidelines — user decision 2026-10-25: `FDeathState.bDiedConcealed` adopted; a killed-concealed victim re-opens at the death write and is kept hidden by the client latch; the `KilledConcealed` interim and `MarkKilledConcealed` call are retired (ADR-0003 §5.7).
- Key renames — user decision 2026-10-25 (TDC CR-10): `bConcealServerCull`.

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §7 (closeout D1): CON-R12 / AC-CON-24(c) multi-occupant share cap is `max(40 %, m0 + 10 pp)` (replaces the absolute 40 % plus m0 + 10 pp double cap); it is a spot-zone/candidate rule inside the server query and changes no interface or replicated field. The blind-strike spot pick stays the secret `BushSpotPick` stream (ADR-0006 domain 14, tag `CON_SPOT`); ASN-R8.2 / AC-ASN-39's older wording "stateless hash `SpotSalt`" is superseded by that stream (Assassination re-review xdoc; nothing here changes).

Technical Setup 2026-10-25 (Technical Director gate findings; Pings traceability TR-png-*; GDD rulings D26 and D33 in `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §7 server queries: Informant Pings is a third consumer of `IsInBushOrTransition(const AStealthPawn*)` (server step 2 of `ServerTeamPing`, ADR-0011 amendment). The query is already true for every state except `Outside`, so `Emerging` counts as concealed until the record leaves at the end of the strike's recovery (D33); no new server state or query is added. Pings read it at RPC arrival; reading never changes concealment, and a ping is never an `OnConcealmentBroken` trigger.
- §5 client side (new client-local owner query, `Public/View/`, beside the body-hidden latch): `IsOwnPawnConcealedForPing()` on the owning client, true when the owner's `UConcealmentOwnerViewComponent::bIsConcealed` is set, **or** the own `FActionState` shows a hide entry/exit or a bush strike-out whose recovery has not ended (the owner-visible image of `Entering`/`Exiting`/`Emerging`), **or** a `Hide(Enter)` the client sent is still unanswered (cleared by the own `FActionState` hide pose, by the owner reply for that `ClientCommandSeq`, or after a 1.0 s client-local backstop for a silent ingress drop). It is fail-closed: in steady states it equals the server predicate, and inside a transition it may refuse a ping the server would accept but never sends one the server would drop as `Concealed` (AC-PNG-17). It reads only owner data, adds no replicated field and needs no allowlist row.

Architecture review 2026-10-25b (`docs/architecture/architecture-review-2026-10-25b.md`); text corrections only, decision unchanged, Status unchanged (Accepted).
- Depends On, Key Interfaces comment and Related: retired `MarkKilledConcealed` references replaced by the `bDiedConcealed` death-write re-open; answered amendment requests (ADR-0002 `ConcealmentTransition`, ADR-0006 `BushSpotPick`) marked applied; the Pings amendment's section reference corrected (§7 server queries); Pings added to Enables.
