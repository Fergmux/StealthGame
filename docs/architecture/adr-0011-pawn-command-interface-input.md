# ADR-0011: Pawn command interface and input pipeline

## Status
Accepted — 2026-10-25, by the user (blanket acceptance of ADR-0001..0023, recorded by the orchestrator). **Conditional:** every spike or verification item named in the earlier status text below must still report before implementation depends on it; a failed spike is handled by a superseding ADR, not by editing this one.

*Earlier status text (kept for its gating conditions):* Proposed — awaiting user acceptance. It cannot be Accepted before ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0007, ADR-0008 and ADR-0010 are Accepted, the verification items under Engine Compatibility have reported, and `input-interaction-framework.md` has passed its pending re-review (it is still marked NEEDS REVISION).

## Date
2026-10-24

## Last Verified
2026-10-24 (engine claims read in the UE 5.8.4 install at `C:/Program Files/Epic Games/UE_5.8`; design against `input-interaction-framework.md` in its "NEEDS REVISION, re-review pending; reconciled 2026-10-24" state)

## Decision Makers
Autonomous design pipeline (`/architecture-decision`, rigor full, review mode full); technical-director (TD-ADR) and unreal-specialist validation (gate record below); acceptance reserved for the user.

**Gate record (2026-10-24):** TD-ADR CONCERNS → REVISED (server-owned `PossessionEpoch` via `ClientSetPossessionEpoch`; ingress stamps the controller's recorded control epoch; condition sweep schedules `HoldTimer(ConditionCancel)` with an ADR-0002 amendment request; second reply flush at the staging pass and batch ordering; R14/R15 covered; weak predicate registration and `UENUM`s; acceptance gated on the IIF re-review). Engine validation (unreal-specialist lens): MINOR NOTES → applied (server GUID resolution ignores culling; host passes a weak handle; Boolean actions; idempotent Ends; Common UI next-tick flush handled synchronously; E6 resend paths). Details under Validation gates.

## Summary
Pillar 1 needs every body in the city to be driven through one set of server-validated commands, so that nothing downstream of the input boundary can tell a human from an NPC. This ADR fixes that boundary: a non-replicated `UPawnCommandComponent` on every Shared Pawn with seven entry points; one owner-to-server transport (CMC saved moves for Move/Look, one reliable PlayerController RPC for discrete commands); two-stage validation (ingress on arrival, the command check at the command's queue slot); one action slot per pawn; hold timers as queue items; a server-only action hub; one drain-end reply step; and an Enhanced Input client builder that maps edges to Begin/End and never gives a Hold trigger server meaning, under the 5.8 Enhanced Input / Common UI unification.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.8 (installed 5.8.4, CL 58902052) |
| **Domain** | Input (Enhanced Input, Common UI routing) with Networking (RPC transport, CMC saved moves) and Core (component, delegates) |
| **Layer** | Core |
| **Knowledge Risk** | HIGH for the input layer (architecture.md: "UI and input" is a HIGH-risk domain because of the 5.8 Enhanced Input / Common UI unification); MEDIUM for the command path itself (pre-cutoff RPC and CMC mechanisms, re-verified) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `modules/input.md` (5.8 block: EI/Common UI unified, flush CVars, Raw Input deprecated), `modules/ui.md`, `breaking-changes.md` (5.8 Enhanced Input fixes behind CVars), `deprecated-apis.md`, `PLUGINS.md` (CommonUI); `docs/architecture/architecture.md` (Engine Knowledge Gap Summary, Core layer, Data Flow §1/§2/§3, API Boundaries → Core, Required ADRs, AQ-15); ADR-0001, ADR-0002, ADR-0003, ADR-0007, ADR-0008, ADR-0010; engine and plugin source read this session (E1-E10 below) |
| **Post-Cutoff APIs Used** | Enhanced Input 5.8 flush behaviour (`EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`, default true, marked for removal) [V]; `FModifyContextOptions::bIgnoreAllPressedKeysUntilRelease` [V]; `UEnhancedInputUserSettings` (replaces `UPlayerMappableInputConfig`, deprecated 5.3) [V]; Common Input `bEnableEnhancedInputSupport` [V]. No Raw Input plugin use |
| **Verification Required** | **V1** Enhanced Input edge behaviour on this project's contexts: `Started` on press and exactly one `Completed` or `Canceled` per release, on context add/remove, focus loss and menu open, with the client's outstanding-Begin flush (R4) producing exactly one End per outstanding Begin and no re-fire of `Started` after a flush (E2, E3). **V2** possession epoch: `ClientSetPossessionEpoch` always arrives before the `ClientRestart` of the same possession on the owner (same reliable controller channel), across takeover, rejoin, same-pawn re-possession after an `Unresponsive` hand-over, and the engine's `ClientRetryClientRestart`/null-pawn resend paths (E6). **V3** server-side resolution of an Attack `PawnRef` NetGUID through the NetDriver's GUID cache returns the live pawn for any GUID the server assigned, **including a pawn culled for that connection** (E7), so the `CanViewerKnow` and `PawnRefAgreementCm` gates alone decide visibility; an unassigned or stale GUID returns null. **V4** latency: AC 36 (`LatencyBudget`) and AC 58 (`LookLatency`) under the ADR-0008 net-emulation profiles. **V5** Common UI under the 5.8 unification: switching to `Menu` input mode while a gameplay hold key is down produces the builder's End synchronously, before CommonUI's next-tick `FlushPressedKeys` (E11; ADR-0021 owns the menu side). **V6** host parity: the listen host's local commands and a remote client's identical commands produce identical stage-1 output (`FCommandPayload` minus the opaque sink) |

**Engine facts this ADR relies on** ([V] read in 5.8.4 source this session; [D] engine-reference docs; [U] unverified, owned by the named verification item):

| # | Fact | Tag |
|---|---|---|
| E1 | `ETriggerEvent` has `Started` (None → Ongoing/Triggered), `Triggered`, `Ongoing`, `Canceled` (Ongoing → None) and `Completed` (Triggered → None); `Completed` does not fire if any trigger reports Ongoing in the same frame | [V] `Plugins/EnhancedInput/Source/EnhancedInput/Public/InputTriggers.h:34-60` |
| E2 | Held digital keys are ignored after a flush until released and re-pressed: CVar **`EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`** (default true; "will be removed in a future release"). The engine-reference docs name it `EnhancedInput.IgnoreHeldKeysOnFlush`, which is not the registered name | [V] `Private/EnhancedPlayerInput.cpp:47-51`; [D] `breaking-changes.md`, `modules/input.md` (name mismatch) |
| E3 | `FModifyContextOptions` (mapping-context add/remove) has `bIgnoreAllPressedKeysUntilRelease` (default true), `bForceImmediately` (default false), `bNotifyUserSettings` (default false) | [V] `Public/EnhancedInputSubsystemInterface.h:52-99` |
| E4 | `UPlayerMappableInputConfig` is deprecated since 5.3 in favour of `UEnhancedInputUserSettings` | [V] `Public/PlayerMappableInputConfig.h:23` |
| E5 | Enhanced Input and Common Input/UI are unified in 5.8 ("removes the need for duplicate data assets"); Common Input exposes `bEnableEnhancedInputSupport` (`UCommonInputSettings::IsEnhancedInputSupportEnabled`) and Common UI menus set an `ECommonInputMode` (`Game`/`Menu`/`All`) through `FUIInputConfig` | [D] `modules/input.md`, `modules/ui.md`; [V] `Plugins/Runtime/CommonUI/Source/CommonInput/Public/CommonInputSettings.h:63-75`, `CommonInputModeTypes.h:11`, `CommonUI/Public/Input/UIActionBindingHandle.h:103-105`; how the unified routing composes with gameplay contexts [U] V5 |
| E6 | `APlayerController::OnPossess` calls `ClientRestart(Pawn)` (reliable client RPC) once per possession; the server may re-send it through the throttled `ClientRetryClientRestart` (which calls `ClientRestart` locally on the owner), and a `ClientRestart` that arrives with a null pawn makes the owner call `ServerCheckClientPossessionReliable`, so the owner can see more than one `ClientRestart` per possession | [V] `PlayerController.cpp:778-837` (`OnPossess`, retry and null-pawn paths); `PlayerController.h:1142, 1507-1514` (engine gate) |
| E7 | `UPackageMapClient::GetObjectFromNetGUID(const FNetworkGUID&, bool bIgnoreMustBeMapped)` defers to the GUID cache, which on the server belongs to the NetDriver, not the connection: a GUID resolves to the live object whether or not that connection ever had (or still has) a channel for it. GUIDs are assigned lazily, so a pawn never sent to any client may have none | [V] `PackageMapClient.h:235, 526`; `PackageMapClient.cpp:2760-2762` (engine gate) |
| E8 | Human Move/Look reach the server through the CMC's `ServerMove` (unreliable, batched), with the control rotation; custom gait data rides `FSavedMove_Character` compressed flags (`FLAG_Custom_0`) | [V] `CharacterMovementComponent.h:2459-2462, 2980-3136` (ADR-0010 E8) |
| E9 | `APlayerController::SetupInputComponent` and `AController::OnRep_Pawn` are virtual | [V] `PlayerController.h:2011`; `Controller.h:182` |
| E11 | Common UI's action router calls `FlushPressedKeys` on the **next tick** after the input mode becomes `Menu` (`CommonUI.AutoFlushPressedKeys`, default true); in `Menu` mode the game viewport may never see the key release, so Enhanced Input is not guaranteed to deliver `Completed` | [V] `CommonUIActionRouterBase.cpp:39-42, 1901-1905`; delivery of `Completed` [U] V5; the router's input-mode-change delegate name [U] |
| E12 | `bIgnoreAllPressedKeysUntilRelease` acts through the deferred mappings rebuild and covers **Boolean** input actions only; the flush CVar (E2) applies only to digital keys on mappings with an action, and a release already queued that tick is not ignored | [V] `EnhancedInputSubsystemInterface.cpp:433, 1176`; `EnhancedPlayerInput.cpp:1021-1063` (engine gate) |
| E10 | Server RPC bodies run during the NetDriver's receive at the top of the world tick, before the tick groups, so a command received this frame is in its mailbox before the `TG_PostPhysics` drain | [V] tick order cited by ADR-0002 E1 / Round R11 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (one command path, no caller identity, carriers: owner-only data on the PlayerController); ADR-0002 (queue `Submit`, mailboxes, `ScheduleAt` due-time heap with `HoldTimer` ordering by Begin key, slot re-validation, latch with held `NotNow` replies, drain-end ordering); ADR-0003 (`FActionState` fixed-width serialiser and the `ActionStateWrite` forced-update scope; owner data never on the pawn); ADR-0004 (IIF tuning keys and client face); ADR-0007 (module placement, consumer-owned interfaces registered by the composition root, View/Server header split); ADR-0008 (test harness, net-emulation profiles); ADR-0010 (the pawn, `UStealthMovementComponent` input path and gait flag, control epoch, published state `ISharedPawnStateView`, traversal start). All must be Accepted before this ADR is implemented |
| **Enables** | ADR-0015 (Camera supplies `GetAimRay()` and the eye-mask flag consumed here); ADR-0016 (Crowd AI issues commands through these entry points); ADR-0017 (Concealment `Hide` predicate and transition); ADR-0018 (Assassination `Attack`/`Prepare` predicates, `FActionState` semantics, owner results on the reply step); ADR-0020 (Objectives `Interact` predicate and completion follow-on); ADR-0021 (HUD reads replies and `FHoldStatus`; CommonUI menu routing) |
| **Blocks** | Input & Interaction Framework epic; every feature story that registers a predicate or listens to the action hub (Assassination, Concealment, Objectives, Crowd red herrings); HUD ring and rejection plates |
| **Ordering Note** | Eleventh in table order (Core), after ADR-0010 because the command component executes on the Shared Pawn's movement component and stamps its control epoch. Whether CommonUI also owns the in-round HUD input layer (AQ-15) stays with ADR-0021; this ADR fixes only the rules any such choice must obey (Decision §9) |

## Context

### Problem Statement

Twenty-odd systems need to make a pawn do something: humans through keyboard, mouse or gamepad, ~40 NPCs through Crowd AI, the server through hand-overs and freezes. If each path validates, times or announces actions its own way, the differences become Pillar-1 tells: a human-only debounce gives humans a longer minimum brace than NPCs, a human-only RPC on the pawn shows a different replication profile, a reply sent at a different point in the frame reveals why a command failed, and an AI exemption from rate limits lets NPCs do what humans cannot. Timing also matters for fairness: Prepare-vs-Attack races must be decided by one total order, never by RPC arrival or tick order.

The prototype shows the failure modes concretely: gameplay input bound on the pawn with legacy binds, Server RPCs on the pawn without authority checks, the defence cooldown on the PlayerState (IIF change request 14), and NPCs moved by path following instead of commands. The decision is needed before any feature registers a predicate or listens for actions, and before the HUD can show a ring.

The engine side moved under the model's knowledge: in 5.8 Enhanced Input and Common UI are unified, and the flush fixes ship behind CVars marked for removal (E2, E5), so edge handling must not depend on engine flush semantics alone.

### Constraints

- ADR-0001: no caller identity anywhere below the boundary; owner and team data only on the PlayerController; no replicated pawn-owned actor; AI through the same command path.
- ADR-0002: every interacting state change resolves in a queue slot; timers are due-time-heap items; replies after the latch are held and sent at the round-end step; resolvers re-validate against live state.
- ADR-0003: `FActionState` is fixed-width with one RepNotify, forced updates only inside the `ActionStateWrite` scope, no multicast on the pawn.
- ADR-0010: the movement component consumes one input vector and one gait field for every pawn; possession changes bump a per-pawn control epoch.
- Listen-server host: its local input must take the human path (no `NM_ListenServer` special case, IIF R2).
- Engine: Enhanced Input (5.8) and optionally Common UI; the Hold trigger must not carry server meaning; no engine patches.
- Team: 1-2 programmers; the validation and slot logic must be plain C++ and testable headless.

### Requirements

- One non-replicated command component per pawn with the seven entry points; no caller identity in any parameter, event or listener (IIF R1, R2; TR-iif-001).
- Human Move/Look through CMC saved moves; AI through the same entry points; one control rotation (IIF R3, R3a; TR-iif-002 command half).
- Discrete commands as Enhanced Input edges; the Hold trigger never carries server meaning; release on loss of observability (IIF R4; TR-iif-003 command half).
- Two-stage validation: ingress on arrival (human and host only), the command check at the slot for every source (IIF R5; TR-iif-004).
- Exactly one owner reply per resolution-voided command, enqueued at drain end; ingress silent (IIF R6; mechanism in ADR-0002).
- Legality by class and locomotion state; one action slot with priority Attack > Prepare > Interact > Hide (IIF R7, R9).
- Hold timers as queue items with a server-side 0.10 s release debounce for every pawn (IIF R8; ordering in ADR-0002).
- Attack payload from the camera aim ray; no lock-on; `PawnRef` a hint gated by visibility and agreement (IIF R10; TR-iif-010).
- Server-only action hub `OnActionStarted/Cancelled/Completed` in one world-level hub (IIF R11; TR-iif-008).
- `FActionState` written only through the component; owner-only status on the PlayerController (IIF R12).
- Input contexts swap on the owner's own state (UX only); AI subject to the same per-pawn token buckets (IIF R13, R17; TR-iif-011).
- Latency budget F5 for human commands (TR-iif-012).

## Decision

### 1. The boundary: `UPawnCommandComponent`

Every `AStealthPawn` carries exactly one `UPawnCommandComponent` (`StealthGameplay/Core`), a **non-replicated** `UActorComponent` created in the pawn's constructor (never added per kind). It is the only code that writes `FActionState`, owns the pawn's action slot and hold timers, and submits a pawn's discrete commands to the queue. Its logic lives in a plain-C++ `FPawnCommandCore` (slot state machine, legality table, token buckets, debounce, reply decision) with no `UWorld` dependency, so ADR-0008's headless specs cover it.

**Entry points** (one per command, same functions for every source):

| Entry point | Kind | Server path |
|---|---|---|
| `Move(FVector2D Direction, EGait Gait)` | continuous | Human: called on the owning client every input frame; rides `ServerMove` (input vector + `FLAG_Custom_0`, ADR-0010). AI: called on the server when the decision changes; the component holds the last `Move` as an intent and re-applies it every movement tick, exactly as a held key is re-read every frame (Crowd CNB-R3.2, ADR-0016), so the CMC sees the same per-tick input stream from both sources. Never a queue item (ADR-0002 §3) |
| `Look(FRotator Rotation)` | continuous | Human: the owner's control rotation, set each frame by Camera's view envelope in `AStealthPlayerController::UpdateRotation` (ADR-0015) from `IA_Look`, carried by `ServerMove`. AI: the crowd controller writes the control rotation (pitch included) through this function. Never a queue item |
| `Traverse(FTraversalHint Hint)` | discrete | queue item → resolver → ADR-0010 §7 `TryStartTraversal` |
| `InteractBegin(FCourtFixtureId)` / `InteractEnd()` | held | queue items; hold timer (§5) |
| `PrepareBegin()` / `PrepareEnd()` | held | queue items; hold timer (§5) |
| `Attack(const FAttackPayload&)` | instant | queue item |
| `Hide(const FHideCommand&)` | instant (`Enter(BushId)` / `Exit(SideIndex)`) | queue item; transition owned by Concealment |

No parameter, event, payload or listener carries `IsAI`, `IsPlayer`, a controller, a PlayerState or a team; the component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query (IIF R2; ADR-0001 §2). Discrete entry points do not execute anything: they build an `FCommandPayload` and submit it (§3).

### 2. Transport (owner → server)

- **Move and Look** travel only in the CMC's `ServerMove` (E8): no RPC of their own, no Look event (a per-turn event would be a gaze tell source).
- **Discrete commands** travel in **one** reliable Server RPC on `AStealthPlayerController`: `ServerPawnCommand(const FNetPawnCommand& Cmd)`. The client never names the pawn it commands; the server resolves the controller's own possessed pawn. `FNetPawnCommand { ECommandKind Kind; FClientCommandSeq Seq; FNetCommandArgs Args; }` where `Args` carries only ids and quantised values: `FCourtFixtureId`, `BushId`, `SideIndex` (2 bits), `WorldPoint` (`FVector_NetQuantize10`), `PawnRefGuid` (`FNetworkGUID` value, never an object-pointer field; Round change request 6), the `TraversalPointHint` id, and for `Traverse` the owner's CMC client timestamp (ADR-0010 §7). Reliable, because a lost `End` would extend a hold; the transport bucket (§3.1) bounds the reliable buffer.
- `FClientCommandSeq = (PossessionEpoch, Seq)`: `Seq` is strictly increasing per controller; `PossessionEpoch` counts this controller's possessions. The server owns it: `AStealthPlayerController::OnPossess` increments a server-side `ServerPossessionEpoch` and, **before** the engine's `ClientRestart`, sends the owner-only reliable client RPC `ClientSetPossessionEpoch(uint16)`; reliable RPCs on one controller channel arrive in order, so the owner's `OwnerPossessionEpoch` is current before it acts on the new pawn (V2). The two fields are separate, so the listen host (where both run on one object) never double-counts; no dedupe by pawn is needed, and same-pawn re-possession (Round R17.1 Unresponsive → AI → same player) still advances the epoch. It is an RPC, not a replicated property, adds nothing to the pawn and carries only the owner's own counter (Owner audience; an IVP leak-catalogue row is requested under Related, as ADR-0002 did for its clock RPCs).
- **Control epoch at ingress.** At `OnPossess` the controller also records `PossessedControlEpoch = UStealthPawnFactory::GetControlEpoch(Pawn)` (the epoch the factory's hand-over already bumped, ADR-0010 §4.3-§4.4). Ingress stamps **the controller's recorded epoch**, never the pawn's current one, so a command from the old owner that arrives between a hand-over's commit and its possession step (one frame, or a whole traversal wait) carries the old epoch and is voided at its slot. AI commands carry the epoch their crowd sink was created with (ADR-0016).
- **Listen-server host:** the host's controller calls the same `IngestCommand(FCommandOrigin::HostLocal, Cmd)` that the RPC body calls, with a synthesised `ClientCommandSeq`, the same ingress checks and its own mailbox; its replies go through the same drain-end step and are never delivered inline (IIF R2). The host has no `NetConnection` or package map and a pawn may not have a GUID yet (E7), so the host's local entry carries the aim ray's weak pawn handle directly instead of a GUID; both paths converge to the same weak handle by the end of stage 1 (V6).
- **Server RPC validation** (`WithValidation`) rejects only structurally impossible payloads (enum out of range, non-finite vector); everything else is ingress's silent drop.

### 3. Two-stage validation

**3.1 Stage 1 — ingress** (RPC arrival and host-local only; never for AI). In `IngestCommand`, in this order, each failure a **silent drop** counted for the server-only abuse log (`AbuseLogDropCount` within `AbuseLogWindowSec`):
1. well-formed payload;
2. `PossessionEpoch` equals the controller's `ServerPossessionEpoch` and `Seq` is newer than the last accepted (stale or duplicate dropped; an old-epoch command never reaches the new pawn);
3. per-connection transport bucket (`TransportBucketSize`/`PerSec`) for `Begin` and instant commands; an `End` matching an outstanding Begin is **never** dropped; unmatched Ends above `UnmatchedEndPerSec` are dropped;
4. `Attack`: `WorldPoint` clamped (not dropped) to `AssassinationRangeCm + CommandRangeSlackCm` of the pawn; a `PawnRefGuid` naming the commanding pawn is stripped and counted; the `PawnRefGuid` is resolved through the NetDriver's GUID cache (E7) to a weak pawn handle (unresolvable → absent). Resolution succeeds even for a pawn culled for that connection, so it grants nothing: visibility is decided only by the `CanViewerKnow` and `PawnRefAgreementCm` gates (§9);
5. `InteractBegin`: the id must be a public Interact target; **every court fixture id is accepted**, whatever its role or the sender's knowledge (IIF R5.1, Objectives OC-6, IVP R7.4a ruling D).
A command that passes is submitted: `UResolutionQueue::Submit(Mailbox, FQueueItem{PawnCommand, FCommandPayload})` where the payload carries the pawn handle, the controller's recorded **control epoch** (§2; ADR-0010 §4.4), the `ClientCommandSeq` and an opaque `FReplySinkId` (an index into the server's reply-sink table; null for AI). AI commands skip stage 1 and submit to the server-local mailbox with a null sink. Commands submitted while the round queue is not open (phase not `Active`) are dropped by the queue (`WrongPhase`, no reply, every source alike).

**3.2 Stage 2 — the command check** (the component's `IQueueResolver` for `PawnCommand`, at the command's slot, for every source). In order, against live state:
0. pawn still valid, not `IsPendingTeardown` (ADR-0002 §7), and the payload's control epoch equals the pawn's current one (else `Voided(StaleControl)`; ADR-0010 §4.4);
1. **legality class** `Roster.GetLegalityClass(Pawn)` ∈ {Active, Informant, RoundEnded} (Unbound → Active, so the component never learns the binding), against the IIF R7 table;
2. **locomotion** from `ISharedPawnStateView` (`IsRunning`, `IsInTraversal`; ADR-0010 §8);
3. **action-slot arbitration** (§4);
4. the **owning feature's predicate** `ICommandPredicate::CanBegin(Pawn, Command)` → `Allow | AllowNoEffect | Reject(Code)` (§6);
5. the **per-pawn, per-type token bucket** (`Bucket*Size`/`PerSec`), consumed **only when 0-4 pass and the command executes**; `End` is never rate-limited, and an `End` with no matching active action is a no-op.
A command that passes executes inside its slot (writes `FActionState` through §7, starts hold timers, emits `OnActionStarted`, runs the owning feature's follow-ons inline). A command that fails is `Voided` and gets its reply (§8). AI is subject to every step including the buckets (IIF R17).

**3.3 Side-effect-free dry run.** `UPawnCommandComponent::QueryCanBegin(const FCommandPayload&, FDryRunMargins)` (server-only) evaluates steps 1-4 against live state with optional distance/angle margins (Crowd uses 10 cm / 10°, ADR-0016) and returns the verdict and code; it consumes no token, changes no slot, emits nothing and replies to nobody. It exists so AI can choose a behaviour that will pass; the authoritative check is still stage 2 at the slot (same-drain contention can still reject). Therefore **every `ICommandPredicate::CanBegin` must be side-effect-free** (`const`, no state change, no logging that differs from the slot path). Humans get the equivalent from owner-known data for prompts (§9); the dry run is a decision aid, never a channel either kind can observe.

### 4. One action slot per pawn

`FPawnCommandCore` holds one slot state per pawn: `Idle | Interacting(Target, Start) | Bracing(Start) | Striking | Countering | HideTransition(Enter|Exit)` (IIF States). Priority **Attack > Prepare > Interact > Hide**: a higher command preempts a lower active one (cancel reason `Superseded`); a lower one is rejected `Busy`. Hide transitions are atomic (rejected while any action is active; every discrete command `Busy` during one, Attack included). Prepare and Interact inside a bush are rejected `NotNow`. Same-type rules: `InteractBegin` while interacting and `PrepareBegin` while bracing are no-ops with no reply; `Attack` while `Striking` (incl. recovery/lockout) and `Hide` during a transition are `Busy`. Nothing auto-resumes. `Countering` (ASN-R14) is pose-locked and rejects every discrete command `Busy`. A legality-class change or phase change cancels the active action with `Illegal` in the same slot; pawn removal cancels with `PawnRemoved`.

### 5. Hold actions and timers

- On execution the server records `ActionStartServerTime = ServerNow()` (double); no client timestamp is accepted and nothing is back-dated.
- **Every time-driven end is a queue item** scheduled with `UResolutionQueue::ScheduleAt(DueTime, HoldTimer{Complete | CapEnd | DebounceExpiry, ActionEpoch})` (ADR-0002 §4: equal `DueTime`s order by the key of the hold's Begin; `Complete` before `DebounceExpiry` at an exact tie, Objectives change request 1b). The IIF has no tick of its own for holds. A timer whose `ActionEpoch` no longer matches is `Voided` silently (it belongs to an ended hold).
- **Interact**: `Complete` at `t_start + D` (`D` from the target owner, `ObjectiveInteractSec`). Begin check: distance ≤ `InteractRangeCm + CommandRangeSlackCm` and control-rotation yaw within `InteractFacingHalfAngleDeg` (else `OutOfReach`); after Begin only the cancel range `InteractRangeCm + CommandRangeSlackCm + InteractCancelHysteresisCm` is re-checked (Look never cancels). Commanded displacement (`ISharedPawnStateView::CommandedDisplacementCm`, ADR-0010 §6) beyond `InteractMoveCancelDistCm` after `InteractMoveGraceSec` cancels `Moved`; so does `Gait = Run` and `Traverse`. The distance and displacement re-checks run in the IIF's **condition sweep**: a tick function in `TG_PostPhysics` ordered immediately before ADR-0002's drain, which only **reads** state and, for a hold whose condition has failed, schedules `HoldTimer(ConditionCancel, ActionEpoch, Reason)` with `DueTime = ServerNow()` so it is stamped in the drain's pre-intake group with Begin-key ordering (ADR-0002 §4). Its slot re-checks the `ActionEpoch` and the condition and cancels (`Moved` or `OutOfRange`); it has no reply sink; after the latch it returns `Rejected_Latched` with no `NotNow` (holds are frozen). `ConditionCancel` is a new `HoldTimer` sub-kind and therefore an **amendment request to ADR-0002's closed item set** (Related). Early release resets progress to 0.
- **Prepare**: a protected window that starts at execution and ends at `End` or `CapEnd` at `t_start + DefenceDurationSec`; the cooldown is Assassination's, stamped at execution. While `Bracing`, requested Run is clamped (never cancels) and `Traverse` is rejected; an externally forced run or traversal cancels `Moved`.
- **Release debounce, server-side, for every pawn**: an `End` becomes *pending* for `ReleaseDebounceSec`; a same-type `Begin` from the same pawn inside the window cancels the pending End (no token, event, cap restart or cooldown change); otherwise `HoldTimer(DebounceExpiry)` executes the End. A tap is a 0.10 s brace for humans and AI alike.
- **Latch** (ADR-0002 §8.1): every hold is frozen (its completion item returns `Rejected_Latched`); holds cancel through the normal release blend at the round-end step.

### 6. Feature predicates and the action hub (server-only)

- `ICommandPredicate` is a **consumer-owned interface in Core** (architecture.md TD concern 1): Assassination (Prepare, Attack), Objectives (Interact), Concealment (Hide) implement it and the composition root registers them per `EActionType` with `RegisterPredicate(EActionType, TWeakInterfacePtr<ICommandPredicate>)` (weak, as ADR-0002's resolvers; ADR-0007). A missing predicate is a fault and resolves `Reject(NotNow)`. `AllowNoEffect` runs exactly like `Allow` (same pose, timer, events) and only its completion changes nothing; the choice between `AllowNoEffect` and `Reject` may depend only on the site and the legality class.
- **`UActionHub`** (world subsystem, `Public/Server/`, `NotBlueprintType`): native multicast delegates `OnActionStarted(AStealthPawn*, EActionType)`, `OnActionCancelled(AStealthPawn*, EActionType, ECancelReason)`, `OnActionCompleted(AStealthPawn*, EActionType)`, fired only inside queue slots, in key order. `EActionType = {Interact, Prepare, Attack, HideEnter, HideExit}`; `ECancelReason = {Released, Moved, Superseded, Illegal, OutOfRange, TargetInvalid, PawnRemoved, Reversed}`. No dynamic/Blueprint-assignable delegate, no client copy, and no listener may relay an event to a client except through its own Visibility Policy audience.
- **Interact target owner** (amendment 2026-10-24, ADR-0020 §5.1): `IInteractTargetOwner` (consumer-owned `UINTERFACE` in Core, `CannotImplementInterfaceInBlueprint`) with `GetInteractHoldSec()` (no court parameter), `OnInteractExecuted(Pawn, FCourtFixtureId, FQueueKey)` and `OnInteractEnded(Pawn, FCourtFixtureId, EInteractEnd {Completed, Cancelled}, FQueueKey)`, registered weakly from the composition root with `UActionHub::RegisterInteractTargetOwner`. This is the "target owner" that supplies `D` for `Interact` (§5). The IIF calls `OnInteractExecuted` inline in the Begin's slot after executing the command, and `OnInteractEnded` inline in the slot that ends the hold, **before** `OnActionCompleted`/`OnActionCancelled` fan out, with `CurrentKey()` as `Key`, for every pawn kind with no caller identity. Lifecycle rules: (a) outside a slot (the round-end step's cancel of frozen holds in the staging pass, pawn teardown) the call carries an invalid `FQueueKey` and `Cancelled` and is a claim release only; (b) `D` is read once, at execution, to schedule `HoldTimer(Complete)`; (c) a pending debounced End keeps the claim until its `DebounceExpiry` slot; (d) exactly one owner is registered for `Interact` (a second interact-target kind must route per kind by amendment); (e) non-Shipping drain-end consistency check: each fixture's claim count equals the IIF `Interacting` slots targeting it, mismatch → fault sink. A missing owner is a fault and `InteractBegin` resolves `Reject(NotNow)`.

### 7. What replicates

- **All, for every pawn**: `FActionState {Pose, Seq, Variant, StartServerTime, ContactServerTime, WarpPoint}` on `AStealthPawn` (one `ReplicatedUsing` property, fixed-width serialiser per ADR-0003 §4, semantics per ADR-0018). Only `UPawnCommandComponent::WriteActionState` writes it, on the server, inside `FStealthForcedNetUpdate(ActionStateWrite)` (ADR-0003 §2), identically for NPC and player; Assassination's death-write `Counter` goes through it too. Observers drive animation from it and nothing else. `StartServerTime` (added by user decision 2026-10-25, Concealment CR 10b) is written by `WriteActionState` in every write: the pose's start server time (for a discrete command, `ActionStartServerTime`).
- **Owner only, on `AStealthPlayerController`, never on the pawn**: `FHoldStatus {ClientCommandSeq, ActionType, Outcome ∈ {Active, Completed, Cancelled}, ServerStartTime, ServerEndTime}` (no cancel reason), `ClientCommandRejected` codes, and owner results from Assassination (`FStrikeResult`, `FDefenceResult`, ADR-0018), all carried by the reply step (§8). The owner's cooldown end time is Assassination's owner data (ADR-0018). Reply structs live in `StealthFoundation/Public/View` so Presentation reads them without including the root PlayerController.
- **Nothing else.** No hold progress, start time, duration, cooldown, target or team ever replicates to anyone but the owner (IIF R12); no RPC exists on the pawn.

### 8. One reply step

- **Ingress tier: silent.** No reply for anything dropped at stage 1.
- **Resolution tier: exactly one reply** per command voided at stage 2 for any reason (including "pawn removed earlier in this drain"), with `Code ∈ {NotNow, Cooldown, Busy, OutOfReach}` per the IIF R6 mapping; a command voided `Rejected_Latched` replies `NotNow` through ADR-0002's held-reply buffer, released at the round-end step. Codes are computed only from facts the owner already knows; none depends on a target pawn's identity, team, concealment or brace state. After attacker-side legality passes, an `Attack` is never rejected.
- **Timing**: every reply (rejection or `FHoldStatus`, and Assassination's owner results) is appended to its sink's per-pass buffer and flushed **once at the end of the pass that produced it**, as one reliable owner-only client RPC per connection, `ClientCommandReplies(const TArray<FCommandReply>&)` (the IIF GDD's `ClientCommandRejected(Seq, Code)` becomes an entry kind of this batch; GDD change request under Related), so arrival time never encodes the failure point. The normal flush point is the end of the drain's resolution step. The second flush point is the end of ADR-0002's **staging pass** when it runs a round-end step bound to a death write (§8.3-§8.4), which releases the held `NotNow` replies there. Inside a batch, entries are ordered by the queue key of the slot that produced them (held replies keep their original keys); within one slot, the IIF's reply precedes the feature's owner result (`FStrikeResult`/`FDefenceResult`). A null sink (AI) discards; that is the only difference, decided by the sink, never by a caller check. The host's sink obeys the same flush. Presentation of a reply to a pawn struck in the same or an earlier drain waits for that pawn's `ContactServerTime` (ASN-R23; ADR-0018).

### 9. The client input pipeline (Enhanced Input, 5.8)

- **Gameplay input lives on `AStealthPlayerController`**, not the pawn: `SetupInputComponent` (E9) binds an `UEnhancedInputComponent` to the IIF actions (`IA_Move`, `IA_Look`, `IA_Run`, `IA_Traverse`, `IA_Interact`, `IA_Prepare`, `IA_Attack`, `IA_Hide`, `IA_CameraToggle`, `IA_ShoulderSwap`, and since the user scope decision of 2026-10-25 `IA_WalkSlower` and `IA_Ping`, both in `IMC_Common`; see Amendments). The pawn's `SetupPlayerInputComponent` binds nothing gameplay-relevant; the Game Animation Sample's character input bindings are removed when its Blueprint is reparented (ADR-0010 migration). Possession changes therefore never rebind gameplay input.
- **Edges only (IIF R4).** Held commands bind `ETriggerEvent::Started` → `Begin` and **both** `Completed` and `Canceled` → `End` (E1); instant commands bind `Started`. Actions with server meaning are **Boolean** input actions (E12) with only the default (Down/Pressed) trigger: the **Hold, HoldAndRelease, Tap, Pulse and Chorded-action triggers are banned** on them (an editor data lint in ADR-0008 checks both and fails the build). `IA_Move` and `IA_Look` are axis actions and carry no edges. No command auto-repeats.
- **The builder owns release safety.** `UPawnCommandClient` (a client-only component on the controller) tracks every outstanding `Begin`. On a mapping-context swap, menu open, window focus loss, possession change or `ClientRestart`, it sends `End` for every outstanding `Begin` **before** the change is applied, then requires a fresh press. Ends are **idempotent**: once the builder has sent an End for a Begin, any later engine `Completed` or `Canceled` for that press is swallowed, so a flush never yields two Ends. Context changes use `FModifyContextOptions{bIgnoreAllPressedKeysUntilRelease = true}` (E3). The engine's held-key flush behaviour (E2) is a second line, not the contract, because its CVar is marked for removal.
- **Contexts (UX only; the server validates regardless):** `IMC_Common` (Move, Look, Camera toggle, Shoulder swap, menu) always; `IMC_Active` (Run, Traverse, Interact, Prepare, Attack, Hide); `IMC_Informant` (no gameplay actions; former Active keys show the owner-only "Informants can only observe" plate, rate-limited by `InformantPlateCooldownSec`; nothing is sent). The swap follows the owner's **own** participant `State` from its C-TeamView owner part (ADR-0001 §5) and possession, **gated by the presented own role** (amendment 2026-10-24, ADR-0021 §8): `IMC_Informant` is selected only when the pure Core function `UseInformantContext(RawOwnState, bPresenterBound, PresentedRole, LocalNow, TStateArrival, CapSec)` is true — raw state `Informant` and (no presenter bound, or the HUD's `PresentedOwnState` is `Informant`, or the `HudOwnRemovalHoldCapSec + HudRemovalCardDelaySec` cap has elapsed). The HUD pushes the presented role through `IOwnerCommandClientView::SetPresentedOwnRole`. Until the gate flips the client keeps `IMC_Active` (the server rejects `NotNow`, replies held to `OnRoleChangePresented`, ASN-R23), so the victim gets no pre-contact warning; a return to `Active` is immediate.
- **Attack payload (IIF R10):** built only from Camera's `GetAimRay()` (ADR-0015): always `WorldPoint`; `PawnRefGuid` only if the ray hit a pawn this client renders (Camera eye-mask α > 0), that is not `IsMarkedAlly` (Teammate Markers), within `AssassinationRangeCm + CommandRangeSlackCm`. No lock-on, snapping, magnetism or aim assist on any device. The server honours `PawnRef` only if `CanViewerKnow(Attacker, Pawn, Position)` and the pawn lies within `PawnRefAgreementCm` of `WorldPoint`; otherwise the Attack resolves as `TargetSpot` with no distinct reply or timing.
- **Owner prediction hook.** `UPawnCommandClient` raises a client-local `OnOwnerCommandPredicted(ECommandKind, FClientCommandSeq)` when it sends a command that owner-known data says is legal; Concealment (own `BushEnter`/`BushExit` clip ≤ 50 ms, ADR-0017) and Locomotion (traversal anticipation, ADR-0010 §7) start their owner-only presentation from it and reconcile from `FActionState` or cancel on the rejection reply. Observers never see a predicted start.
- **Owner prediction is presentation only**: the ring (F4) and prompt legality from owner-known data (own state, own cooldown, own slot, `IsRunning`/`IsInTraversal` computed locally); the client does not send `PrepareBegin` while its known cooldown runs (traffic saving only).
- **Rebinding** uses `UEnhancedInputUserSettings` (E4); `UPlayerMappableInputConfig` must not be used. Bindings, the hold/toggle preference and sensitivity are client-local, never tuning (TDC CR-14). A hold/toggle translation (IIF R16, not in MVP) would produce the identical Begin/End stream.
- **Common UI (5.8 unification, E5)**: whether Common UI's Enhanced Input support is enabled and whether it owns more than menus is ADR-0021's (AQ-15). Whatever it chooses: no gameplay command may be bound as a Common UI action or routed through a Common UI action domain; `UPawnCommandClient` subscribes to the Common UI action router's input-mode change (`UCommonUIActionRouterBase::OnActiveInputModeChanged` [V], confirmed by ADR-0021 E7) and sends every outstanding End **synchronously** on a change to `Menu`, never waiting for Enhanced Input, because Common UI flushes pressed keys on the next tick and the release may never reach the game viewport (E11, V5); gameplay `IMC_*` contexts are added and removed only by `UPawnCommandClient`.
- **Camera toggle and shoulder swap (IIF R14)** are bound in `IMC_Common` and handled client-locally by Camera (ADR-0015) with the `CameraToggleLockoutSec` lockout; they are never commands, never reach `ServerPawnCommand`, and work mid-hold and mid-move.
- **Default bindings (IIF R15)** are the data of `IMC_Common`/`IMC_Active` exactly as the IIF table lists them (WASD/left stick, mouse/right stick, Left Shift/L3 Run, Space/A Traverse, E/X Interact, Q/LT Prepare, LMB/RT Attack, F/B Hide, V/Y toggle, X-or-middle-mouse/D-pad-right shoulder swap); gamepad is partial in MVP (every command mapped, no remap UI, never aim assist). The data lint asserts the table.
- **Client view handle** (amendment 2026-10-24, ADR-0021 §5/§8): `UPawnCommandClient` implements the Core View interface `IOwnerCommandClientView` — `SetAllyMarkerQuery(TWeakInterfacePtr<IAllyMarkerQuery>)` (the markers presenter's `IsMarkedAlly`, used by the Attack payload and strike hint; answers false when none is bound), `SetPresentedOwnRole(EPresentedOwnRole)`, `EndAllOutstanding(EEndReason)` (synchronous idempotent Ends; called by the menu stack before every menu push) and `OnLocalUiAction()` (client-local `ShowTeamNames` from `IMC_Common`, never sent to the server). It reaches Presentation through `FStealthClientViewHandles` (ADR-0007).
- **Raw Input** is not used (deprecated in 5.8; `GameInputWindows` if a device layer is ever needed).

### 10. AI uses the same path (IIF R17)

The crowd controller (ADR-0016) calls the same entry points on the server: `Move`/`Look` every decision step, discrete red-herring commands (`InteractBegin`/`End`, `PrepareBegin`/`End`, `Hide`) through the server-local mailbox with a null sink. NPCs never issue `Attack` (Crowd T6), a participant-bound AI drive issues only `Move` and `Look` (Round R17), and neither rule is enforced by a kind check here: they are Crowd's behaviour constraints, verified by Crowd's tests.

### 11. Latency budget (IIF F5)

A human discrete command costs: one-way uplink + at most one server frame (the RPC lands in its mailbox before the same frame's drain, E10) + the drain-end reply flush at the next net update. `L_confirm = RTT + dt_server + dt_client` (93-110 ms at 60 ms RTT; 183-200 ms at 150 ms RTT). Look is local (`L_look ≤ 33 ms`, camera transform changes in the input frame). The budget is met by construction because nothing waits for a timer, a second RPC or a client acknowledgement; AC 36 and AC 58 measure it (V4). Above HUD's `HudLatencyWarnRttSec` (IIF's duplicate `LatencyWarnRttSec` retired by user decision 2026-10-25) the owner sees the latency icon; no gameplay compensation is applied.

### Architecture Diagram

```
 OWNER CLIENT                                   SERVER (listen host: same path via IngestCommand(HostLocal))
 Enhanced Input (IMC_Common/Active/Informant)
   IA_Move/IA_Look ─► UPawnCommandComponent::Move/Look (predict) ─► CMC ServerMove ──► UStealthMovementComponent
   IA_* Started ─► Begin, Completed|Canceled ─► End                                     (one input path; AI writes same fields)
   UPawnCommandClient (outstanding-Begin tracker; flush on context/menu/focus/possession)
     └─► AStealthPlayerController::ServerPawnCommand(FNetPawnCommand) [reliable] ─► IngestCommand
                                                                                    stage 1: payload · epoch/seq · transport bucket
                                                                                             · WorldPoint clamp · PawnRef resolve · court id
                                                                                    └─► UResolutionQueue::Submit(mailbox[ParticipantId])
 Crowd AI (server) ─► UPawnCommandComponent::InteractBegin/... ─────────────────────► Submit(server-local mailbox, null sink)
                                                         TG_PostPhysics drain, key order:
                                                           PawnCommand resolver (stage 2): control epoch · legality (Roster)
                                                             · IsRunning/IsInTraversal · slot · ICommandPredicate · per-pawn bucket
                                                             ├─ execute: WriteActionState (ActionStateWrite scope) · ScheduleAt(HoldTimer)
                                                             │           · UActionHub.OnActionStarted ─► features (inline follow-ons)
                                                             └─ void: reply buffer[sink]
                                                           HoldTimer resolver (Complete | CapEnd | DebounceExpiry)
                                                         drain end: ClientCommandReplies(sink) once per connection
 ◄────────────────────────── owner-only reliable RPC: rejections, FHoldStatus, FStrikeResult/FDefenceResult
 ◄────────────────────────── all clients: FActionState on the pawn (one RepNotify)
```

### Key Interfaces

```cpp
// --- StealthFoundation/Public/View: owner-visible reply data -----------------------------------
UENUM() enum class ECommandRejectCode : uint8 { NotNow, Cooldown, Busy, OutOfReach };
UENUM() enum class EHoldOutcome : uint8 { Active, Completed, Cancelled };
UENUM() enum class EActionType : uint8 { Interact, Prepare, Attack, HideEnter, HideExit };
USTRUCT() struct FClientCommandSeq { GENERATED_BODY()
    UPROPERTY() uint16 PossessionEpoch = 0; UPROPERTY() uint32 Seq = 0; };
USTRUCT() struct FHoldStatus { GENERATED_BODY()
    UPROPERTY() FClientCommandSeq Seq; UPROPERTY() EActionType ActionType;
    UPROPERTY() EHoldOutcome Outcome; UPROPERTY() double ServerStartTime = 0; UPROPERTY() double ServerEndTime = 0; };
USTRUCT() struct FCommandReply { GENERATED_BODY()   // tagged: Rejected | HoldStatus | StrikeResult | DefenceResult
    UPROPERTY() uint8 Kind = 0; UPROPERTY() FClientCommandSeq Seq; UPROPERTY() ECommandRejectCode Code;
    UPROPERTY() FHoldStatus Hold; /* FStrikeResult / FDefenceResult: ADR-0018 */ };

// --- StealthGameplay/Public/Server/Core/: the boundary (StealthPawnCommandComponent.h; the pawn header ----
//     forward-declares UPawnCommandComponent and never includes this header; ADR-0007 rule (a))
UCLASS(NotBlueprintable)
class UPawnCommandComponent : public UActorComponent      // non-replicated; one per AStealthPawn
{
    GENERATED_BODY()
public:
    void Move(FVector2D Direction, EGait Gait);           // continuous (owner predicts; AI on server)
    void Look(FRotator Rotation);                         // continuous
    void Traverse(const FTraversalHint& Hint);            // discrete ─► queue
    void InteractBegin(FCourtFixtureId Target); void InteractEnd();
    void PrepareBegin(); void PrepareEnd();
    void Attack(const FAttackPayload& Payload);           // WorldPoint + optional weak PawnRef
    void Hide(const FHideCommand& Cmd);                   // Enter(BushId) | Exit(SideIndex)
    // Server: the one write path for FActionState (ActionStateWrite forced-update scope).
    void WriteActionState(const FActionState& NewState);
    // Server: side-effect-free legality dry run (stage 2 steps 1-4, no token, no reply) — AI decision aid.
    FPredicateResult QueryCanBegin(const FCommandPayload& Cmd, FDryRunMargins Margins = {}) const;
private:
    FPawnCommandCore Core;                                // plain C++: slot, legality, buckets, debounce
};
// No parameter anywhere carries IsAI/IsPlayer, a controller, a PlayerState or a team.

// --- StealthGameplay/Public/Server/Core/: server-only pieces -----------------------------------
struct FCommandOrigin { FMailboxId Mailbox; FReplySinkId Sink; /* opaque; null for AI */ };
void IngestCommand(APlayerController& Owner, const FCommandOrigin& Origin, const FNetPawnCommand& Cmd); // stage 1
struct FPredicateResult { EPredicateVerdict Verdict; /* Allow | AllowNoEffect | Reject */ ECommandRejectCode Code; };
UINTERFACE(meta=(CannotImplementInterfaceInBlueprint)) class STEALTHGAMEPLAY_API UCommandPredicate : public UInterface { GENERATED_BODY() };
class STEALTHGAMEPLAY_API ICommandPredicate { GENERATED_BODY()
public: virtual FPredicateResult CanBegin(const AStealthPawn& Pawn, const FCommandPayload& Cmd) const = 0; }; // must be side-effect-free

UCLASS(NotBlueprintType, NotBlueprintable)
class UActionHub : public UWorldSubsystem                  // server-only, one per world
{
    GENERATED_BODY()
public:
    void RegisterPredicate(EActionType Type, TWeakInterfacePtr<ICommandPredicate> Predicate); // composition root
    void RegisterInteractTargetOwner(TWeakInterfacePtr<IInteractTargetOwner> Owner);           // composition root (ADR-0020)
    FOnPawnAction          OnActionStarted;     // DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPawnAction, AStealthPawn*, EActionType)
    FOnPawnActionCancelled OnActionCancelled;   // DECLARE_MULTICAST_DELEGATE_ThreeParams(..., AStealthPawn*, EActionType, ECancelReason)
    FOnPawnAction          OnActionCompleted;
};

// --- StealthGameplay/Public/Server/Core/: read-only pawn state the resolver uses (ADR-0010 §8) --------
//     implemented by a server-only adapter in StealthGameplay/Private/Core/ over UStealthMovementComponent's queries
class ISharedPawnStateView { public:
    virtual bool IsRunning() const = 0; virtual bool IsInTraversal() const = 0;
    virtual float CommandedDisplacementCm(double SinceServerTime) const = 0; };

// --- StealthGame (root): AStealthPlayerController transport ------------------------------------
//   UFUNCTION(Server, Reliable, WithValidation) void ServerPawnCommand(const FNetPawnCommand& Cmd);
//   UFUNCTION(Client, Reliable) void ClientCommandReplies(const TArray<FCommandReply>& Replies);
//   virtual void SetupInputComponent() override;           // Enhanced Input; UPawnCommandClient builder
//   UFUNCTION(Client, Reliable) void ClientSetPossessionEpoch(uint16 Epoch);  // sent in OnPossess before ClientRestart
//   uint16 ServerPossessionEpoch (server) / OwnerPossessionEpoch (owner); uint32 PossessedControlEpoch (server, from the factory)
```

### Implementation Guidelines

- Every pawn must carry exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source must enter through its entry points and the queue; nothing may write `FActionState` except `UPawnCommandComponent::WriteActionState`.
- No entry point, payload, event or listener may carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team); the component must never call `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query.
- Human Move/Look must travel only in `ServerMove`; there must be no Move/Look RPC and no Look event.
- Discrete commands must travel only in `AStealthPlayerController::ServerPawnCommand`; there must be no gameplay RPC on the pawn; the client must never name the pawn it commands; `PawnRef` must travel as a NetGUID value, never an object-pointer field.
- The listen host's input must go through `IngestCommand(HostLocal)` with the same ingress and the same reply flush; there must be no `NM_ListenServer` special case.
- `PossessionEpoch` must be owned by the server and delivered only by `ClientSetPossessionEpoch` before `ClientRestart`; ingress must stamp the controller's recorded `PossessedControlEpoch`, never the pawn's current control epoch.
- A resolved `PawnRef` must never be treated as evidence of visibility; only `CanViewerKnow` and `PawnRefAgreementCm` may admit it.
- Ingress drops must be silent; stage 2 must run only at the command's queue slot and in the stated order; the per-pawn token bucket must be consumed only on execution; `End` must never be rate-limited per pawn or dropped at the transport bucket when it matches an outstanding Begin.
- A resolution-voided command must produce exactly one reply, flushed only at the end of the drain's resolution step in one owner-only RPC per connection; `Rejected_Latched` must reply `NotNow` through the queue's held-reply buffer; no reply code may depend on a target pawn's identity, team, concealment or brace state.
- Hold ends must be `ScheduleAt` queue items; the IIF must never time a hold on its own tick, `FTimerManager` or a client timestamp; the release debounce must be server-side and identical for every pawn.
- Feature predicates must be registered weakly from the composition root per `EActionType`; `AllowNoEffect` vs `Reject` must depend only on the site and legality class.
- Action-hub events must fire only inside queue slots, must be native (not Blueprint-assignable) and must never be relayed to a client except through the Visibility Policy.
- Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) must live on the PlayerController carrier family, never on the pawn.
- Gameplay input must be bound on `AStealthPlayerController`; held commands must map `Started` → Begin and both `Completed` and `Canceled` → End; actions with server meaning must be Boolean and must never use Hold/HoldAndRelease/Tap/Pulse/Chorded triggers.
- Ends must be idempotent per Begin; a Common UI switch to `Menu` must produce the Ends synchronously.
- The condition sweep must only read state and schedule `HoldTimer(ConditionCancel)` items; it must never cancel a hold itself.
- `UPawnCommandClient` must send End for every outstanding Begin before any context swap, menu open, focus loss, possession change or `ClientRestart` takes effect; context changes must set `bIgnoreAllPressedKeysUntilRelease`.
- The Attack payload must come only from Camera's `GetAimRay()`; there must never be lock-on, snapping, magnetism or aim assist; `PawnRef` must be honoured only through `CanViewerKnow` and `PawnRefAgreementCm`.
- Rebinding must use `UEnhancedInputUserSettings`; `UPlayerMappableInputConfig` and the Raw Input plugin must not be used; no gameplay command may be a Common UI action.
- Prototype input code (legacy binds and Server RPCs on `AStealthCharacter`, cooldown on the PlayerState, NPC path-following movement) must be replaced, not ported, after this ADR is Accepted.

## Alternatives Considered

### Alternative 1: RPCs on the pawn (the prototype)
- **Description**: Keep `ServerAssassinate`, `ServerSetDefence`, `ServerSetInteracting` as Server RPCs on the pawn; the AI calls the implementation functions directly.
- **Pros**: Fewer hops; the pawn is the natural owner of its actions.
- **Cons**: A Server RPC on the pawn is callable only by its owning connection, so the AI path necessarily differs; the pawn's RPC table becomes part of the kind-visible class surface; validation and replies end up in different places per command; the prototype's versions had no authority checks.
- **Rejection Reason**: Violates the one-boundary rule (IIF R2) and ADR-0001's "no gameplay RPC on the pawn". One controller RPC that resolves its own pawn keeps the client from naming targets and keeps the pawn identical.

### Alternative 2: Validate everything at ingress (single stage)
- **Description**: Run legality, slot and predicate checks when the RPC arrives, then submit only accepted commands.
- **Pros**: Earlier rejection, simpler resolver.
- **Cons**: AI commands are submitted from the physics tick groups and human ones at dispatch, so the same command would be checked against different frame states depending on the source; an earlier slot in the same drain (a kill, a phase change) would not be seen; replies would leave at different points in the frame (timing side channel).
- **Rejection Reason**: Breaks source parity and the total order. Stage 2 at the slot is the only point every source shares (IIF design review 2026-10-24).

### Alternative 3: Enhanced Input Hold triggers drive server holds
- **Description**: Use the Hold/HoldAndRelease trigger and send one "Prepare triggered" message when the hold threshold passes.
- **Pros**: Built-in hold UX; fewer messages.
- **Cons**: The client would decide when a hold begins and how long it lasts, adding client-side timing the AI does not have (a parity tell) and trusting client time; Hold triggers report `Ongoing` and suppress `Completed` in the same frame (E1), which complicates release safety.
- **Rejection Reason**: The server owns every hold timer (IIF R4, R8); edges only.

### Alternative 4: Client-side release debounce
- **Description**: Debounce key flicker on the client before sending End.
- **Pros**: No server state for pending Ends.
- **Cons**: Humans would get a longer minimum brace than AI (a Pillar-1 tell) and a modified client could remove it.
- **Rejection Reason**: Server-side debounce for every pawn (IIF R8).

### Alternative 5: Replies as replicated owner-only properties
- **Description**: Put the last rejection and the hold status in owner-only replicated properties on the controller instead of a reply RPC.
- **Pros**: State-based, survives packet loss without reliability.
- **Cons**: Two rejections in one frame collapse into one; property replication timing is per actor and frequency-dependent, so reply timing would vary with unrelated owner data; ordering against `FStrikeResult` (also owner-only) is lost.
- **Rejection Reason**: One reliable, drain-end-batched RPC gives exactly one reply per voided command at a fixed point in the frame.

### Alternative 6: Gameplay input through Common UI actions
- **Description**: Route all input, gameplay included, through Common UI's action router now that Enhanced Input and Common UI are unified in 5.8.
- **Pros**: One input layer for menus and gameplay; platform glyphs for free.
- **Cons**: The unified routing's interaction with gameplay mapping contexts is undocumented in the reference and unverified (E5); Common UI's input modes can swallow or delay releases; the Pillar-1 and edge-safety rules would depend on UI framework behaviour.
- **Rejection Reason**: Gameplay commands stay on plain Enhanced Input contexts owned by `UPawnCommandClient`; Common UI scope is ADR-0021's under the constraints of §9.

## Consequences

### Positive
- One boundary makes Pillar 1 testable: a human and an AI command with the same content are byte-identical downstream of ingress, with the same validation, slot rules, buckets, debounce, events and replication.
- Every outcome is decided by queue key order at one point in the frame; Prepare-vs-Attack races are deterministic and fair to the host.
- Reply timing carries no information; ingress drops carry none at all.
- Release safety does not depend on engine flush CVars that 5.8 marks for removal.
- `FPawnCommandCore` is plain C++, so slot, legality, bucket and debounce rules are unit-tested headless.

### Negative
- A discrete command costs a queue item and a drain-end reply even when it is obviously illegal; acceptable within the 0.5 ms queue budget (§Performance).
- Reliable RPCs for every Begin/End add reliable-buffer pressure; bounded by the transport bucket.
- The possession epoch needs one extra owner-only reliable RPC per possession (`ClientSetPossessionEpoch`) and an IVP leak-catalogue row; its ordering against `ClientRestart` must be verified (V2).
- Feature systems cannot react to input directly; every reaction goes through predicates or hub events (intended, but more indirection).
- Removing the sample's pawn input bindings is a binary Blueprint edit (snapshot workflow until ADR-0009).

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Enhanced Input fails to deliver a release after a context or Common UI mode change, leaving a hold running (V1, V5) | Medium | High (stuck brace or tend) | `UPawnCommandClient` sends End for every outstanding Begin before any such change; AC on context swap mid-hold; engine flush CVar as a second line |
| Possession epoch desync drops every command after a takeover (V2) | Low | High | Server-owned epoch delivered by `ClientSetPossessionEpoch` before `ClientRestart` on one reliable channel; the harness asserts epoch equality after every possession in the takeover, rejoin and Unresponsive suites; ingress drops on epoch mismatch are counted and alarmed in development builds |
| Server-side NetGUID resolution resolves a pawn the attacker should not know (V3) | Medium (it always resolves, E7) | High (oracle) | Resolution is treated as granting nothing: `CanViewerKnow` and `PawnRefAgreementCm` gate every `PawnRef`; a culled `PawnRef` resolves as `TargetSpot` with identical reply and timing; leak test for culled `PawnRef` (AC-IVP) |
| Reply batching per connection delays a reply past the HUD's tolerance | Low | Low | One frame at most; HUD presentation already waits on `ContactServerTime` where it matters |
| A feature predicate reads caller identity | Low | High | Predicates receive only the pawn and payload; AC-SCL-01-style static scan extended to predicate implementations |
| Reliable buffer overflow under abuse | Low | Medium | Transport bucket before submission; abuse log; Anti-Cheat (Alpha) adds a kick |

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| input-interaction-framework.md | R1 one component, seven entry points; R2 no caller identity, host takes the human path (TR-iif-001) | §1, §2 |
| input-interaction-framework.md | R3/R3a Move and Look through saved moves, AI through the same functions, one control rotation (TR-iif-002, command half) | §1, §2; execution in ADR-0010 §6 |
| input-interaction-framework.md | R4 edges, no Hold trigger, release on loss of observability (TR-iif-003, command half) | §9; HUD side in ADR-0021 |
| input-interaction-framework.md | R5 two-stage validation; ingress accepts every court id (TR-iif-004) | §3 |
| input-interaction-framework.md | R6 two reply tiers, one reply per voided command, drain-end timing, `Rejected_Latched` → `NotNow` | §8; queue mechanism ADR-0002 |
| input-interaction-framework.md | R7 legality table; R9 one action slot with fixed priority | §3.2, §4 |
| input-interaction-framework.md | R8 hold timers as queue items, server-side debounce, Interact reach and cancels, Prepare window | §5; ordering ADR-0002 §4 |
| input-interaction-framework.md | R10 Attack from the aim ray, no lock-on, `PawnRef` hint gate (TR-iif-010) | §2, §3.1, §9 |
| input-interaction-framework.md | R11 server-only action hub in one world-level hub (TR-iif-008) | §6 |
| input-interaction-framework.md | R12 `FActionState` written only here, owner-only status on the controller | §7 (serialiser ADR-0003, semantics ADR-0018) |
| input-interaction-framework.md | R13 input contexts swap on owner state, UX only; R14 camera toggle local; R15 defaults; R16 hold/toggle identical stream (TR-iif-011, contexts half) | §9 |
| input-interaction-framework.md | R17 AI through the same entry points and buckets (TR-iif-011, AI half) | §3, §10 |
| input-interaction-framework.md | F1-F4, F6 hold, debounce, buckets, ring, reach formulas | §3, §5, §9 (values from tuning, ADR-0004) |
| input-interaction-framework.md | F5 input latency budget (TR-iif-012) | §11 |

## Performance Implications
- **CPU**: Ingress is O(1) per RPC. Stage 2 is O(1) per item plus one predicate call; at the IIF F3 worst case (300 admitted commands/s across 50 pawns ≈ 5 per frame at 60 Hz) it is a few microseconds per frame inside the 0.5 ms queue budget (ADR-0002). The condition sweep touches only pawns with an active hold. AI `Move`/`Look` are not items.
- **Memory**: Per pawn: slot state, four buckets, one pending End, hold timer handles (tens of bytes). Per connection: reply buffer, transport bucket, last `ClientCommandSeq`.
- **Load Time**: Input mapping contexts and actions are small data assets loaded with the controller class.
- **Network**: Uplink: one reliable RPC per discrete edge (≤ 20/s per connection by the transport bucket; honest play ≈ 1-2/s). Downlink: at most one reliable reply RPC per connection per drain that produced a reply. `FActionState` is unchanged from ADR-0003's accounting. Nothing new reaches non-owners.

## Migration Plan
Nothing in the prototype is promoted (ADR-0007 rule). When this ADR is Accepted: create `UPawnCommandComponent`/`FPawnCommandCore`, `UActionHub`, `ICommandPredicate`, `ISharedPawnStateView` in `StealthGameplay` (headers in `Public/Server/Core/`, cores in `Private/Core/`); the reply structs in `StealthFoundation/Public/View`; the transport, `UPawnCommandClient` and input setup on `AStealthPlayerController` in the root module; author `IMC_Common`, `IMC_Active`, `IMC_Informant` and the `IA_*` actions under `Content/StealthGame/Input` (the sample's own input assets stay untouched except the reparented character's bindings, removed under ADR-0010's migration). Replace, not port: `AStealthCharacter::SetupPlayerInputComponent` legacy binds, `ServerAssassinate`/`ServerSetDefence`/`ServerSetInteracting`/`ServerPing` on the pawn, cooldown fields on the PlayerState, `FindAssassinationTarget` lock-on-style targeting.

## Validation Criteria
- IIF acceptance criteria pass, in particular AC 24 (release baseline stop, with SCL), AC 36 (`LatencyBudget`), AC 46/48/56 (parity of `ActionPose` writes and cadence), AC 54 (look parity), AC 58 (`LookLatency`), AC 64 (post-latch `NotNow`), AC 66 (court prompts), and the ingress/reply/bucket/debounce ACs (tests `StealthGame.Input.*`, ADR-0008 layout).
- Byte-identity test: a human command and an AI command with the same content produce identical `FCommandPayload` bytes after ingress (excluding the opaque sink), identical slot transitions, events and `FActionState` writes.
- Reply-timing test: over 1,000 voided commands of every code and stage, every reply leaves in the drain-end flush (0 replies sent inside a slot).
- V1-V6 report PASS.
- Hand-over window test: a command sent by the old owner between a hand-over's commit and its possession step (including a traversal wait) is voided `StaleControl` and replies exactly one `NotNow`; a command sent after `ClientSetPossessionEpoch` reaches the new pawn.
- Common UI test: opening a menu mid-brace sends exactly one End before the next tick, and no second End follows when the engine later reports a release.
- Static scan: no gameplay `UFUNCTION(Server|Client|NetMulticast)` on `AStealthPawn` or its components; no Hold/Tap/Pulse/HoldAndRelease/Chorded trigger on an `IA_*` action with server meaning; no `UPlayerMappableInputConfig` use.

## Related
- **Depends on**: ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0007, ADR-0008, ADR-0010.
- **Enables**: ADR-0015 (aim ray, eye mask), ADR-0016 (Crowd uses the entry points), ADR-0017 (Hide), ADR-0018 (Attack/Prepare, owner results), ADR-0020 (Interact completion), ADR-0021 (HUD replies; Common UI scope, AQ-15).
- **Engine-reference and architecture correction (not applied; `docs/engine-reference/` and `architecture.md` are outside this pass's write scope)**: `modules/input.md`, `breaking-changes.md` and `architecture.md` (Engine Knowledge Gap Summary and the ADR-011 row) name the flush CVar `EnhancedInput.IgnoreHeldKeysOnFlush`; the 5.8.4 source registers `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (E2).
- **ADR amendment request (not applied; ADR-0002 belongs to another author)**: add `HoldTimer(ConditionCancel)` to ADR-0002's closed item set (§5 condition sweep).
- **Ownership of the non-command halves**: TR-iif-002's execution half is ADR-0010 §6 (movement) and ADR-0015 (view envelope writing the control rotation); TR-iif-003's HUD/Common UI half is ADR-0021.
- **GDD change requests (not applied; recorded for the GDD owners)**: (1) IIF R6 and HUD's Dependencies row: `ClientCommandRejected(ClientCommandSeq, Code)` is an entry kind of the batched `ClientCommandReplies` RPC; (2) IIF R5.1: `PossessionEpoch` is delivered by `ClientSetPossessionEpoch` (Owner audience) — Visibility Policy leak-catalogue row requested; (3) IIF R8: hold timers are `ScheduleAt` due-time items (ADR-0002 §4) rather than submitted "on the first frame the due time has passed", and condition cancels are `HoldTimer(ConditionCancel)` items; (4) IIF R13 OQ7: the CVar name above; (5) IIF R17: AI path-following no longer feeds `Move` (ADR-0010 §6 and ADR-0016 replace path following with a follower that calls `Move`).
- Design docs: `design/gdd/input-interaction-framework.md`, `shared-character-locomotion.md`, `round-match-framework.md`, `assassination-defence-reversal.md`, `concealment-bushes.md`, `objectives-chain.md`, `crowd-npc-behaviour.md`; `docs/architecture/architecture.md`.

## Validation gates
- **TD-ADR (technical-director), 2026-10-24: CONCERNS.** (1) Two-sided `PossessionEpoch` unsound (same-pawn re-possession locked the player out; host double count) → server-owned epoch sent by `ClientSetPossessionEpoch` before `ClientRestart`, separate server/owner fields. (2) Control-epoch gap between hand-over commit and possession → ingress stamps the controller's `PossessedControlEpoch`; test added. (3) Hold sweep vs ADR-0002 → read-only condition sweep scheduling `HoldTimer(ConditionCancel)` at `ServerNow()`; ADR-0002 amendment request. (4) Batched replies endorsed → GDD change request for `ClientCommandRejected`, staging-pass flush point, in-batch ordering. (5) Scope gaps → R14/R15 bullets, non-command halves assigned (ADR-0010/0015/0021), architecture.md CVar name in Related, IIF re-review as acceptance precondition. (6) Key Interfaces → `TWeakInterfacePtr`, `UENUM`s, declaration order, API macros. Resolution: Revise flagged items (autonomous; logged).
- **Engine specialist (unreal-specialist lens), 2026-10-24: MINOR NOTES.** CVar name confirmed (digital keys only); `bIgnoreAllPressedKeysUntilRelease` covers Boolean actions only → Boolean requirement in the lint; exactly-one-`Completed` on flush unconfirmed → idempotent Ends (V1 open); Common UI flushes on the next tick after `Menu` → synchronous Ends from the router's mode change (delegate name [U]); E6 resend paths corrected; server GUID resolution ignores culling → E7/V3 rewritten, gates alone decide; host has no package map → weak handle, V6; delegate aliases adopted. Unverified: E1's same-frame Ongoing suppression (irrelevant with default triggers), router delegate name.
- **GDD sync check:** checked 2 referenced GDDs (`input-interaction-framework.md`, `hud.md`) for the interface names this ADR fixes. Inconsistencies recorded as change requests under Related (not applied in this pass): `ClientCommandRejected` → entry of `ClientCommandReplies`; `PossessionEpoch` delivery; hold timers as `ScheduleAt` items and `ConditionCancel`; CVar name; AI path-following wording in R17.

## Amendments (2026-10-24 sync)
Inter-ADR amendment pass; decision unchanged, Status unchanged (Proposed).
- §6, Key Interfaces: `IInteractTargetOwner` + `UActionHub::RegisterInteractTargetOwner` with the lifecycle rules — requested by ADR-0020 (§5.1).
- §9: Informant context gated by the presented own role (`UseInformantContext`, `SetPresentedOwnRole`) — requested by ADR-0021 (§8).
- §9: `IOwnerCommandClientView` (`SetAllyMarkerQuery`, `EndAllOutstanding`, `OnLocalUiAction(ShowTeamNames)`) — requested by ADR-0021.
- §9: router delegate name `UCommonUIActionRouterBase::OnActiveInputModeChanged` [V] replaces "[U]" — requested by ADR-0021 (resolves E11's open name).
- architecture.md CVar name (`EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`) — already corrected in architecture.md; no edit here.

Architecture-review fix pass (`docs/architecture/architecture-review-2026-10-25.md`); decision unchanged, Status unchanged (Proposed).
- Key Interfaces, Migration Plan: `UPawnCommandComponent`, `ISharedPawnStateView` and the other server pieces sit in `StealthGameplay/Public/Server/Core/`. The pawn header forward-declares the component, and `ISharedPawnStateView` is implemented by a server-only `Private/` adapter. ADR-0007 lint rule (a) now passes — architecture-review fix 2026-10-25 (Conflict 4).
- `FCommandPayload` / `FHoldTimerPayload`: declared by ADR-0002 §2a in Foundation `Public/Server`. This ADR owns the values of `Kind`, `Phase`, `Arg` and `ActionType` and maps its typed commands onto them at ingress — architecture-review fix 2026-10-25 (Conflict 1).
- Requests answered (2026-10-24): the ADR-0002 `HoldTimer(ConditionCancel)` request under Related is applied in ADR-0002 §1 — architecture-review fix 2026-10-25 (Conflict 9).

User decisions (2026-10-25); decision unchanged in substance, Status unchanged (Accepted).
- §7 — user decision 2026-10-25: `FActionState.StartServerTime` adopted; `WriteActionState` fills it on every write (ADR-0018 §1.1 owns the width, now 208 bits).
- §9 — user decision 2026-10-25: the latency icon threshold is HUD's `HudLatencyWarnRttSec`; IIF's `LatencyWarnRttSec` is retired (cross-review C-W14).
- §8 — user decision 2026-10-25: `FStrikeResult` entries of `ClientCommandReplies` gain `ContactServerTime` (ADR-0018 §1.3).

User scope decisions (2026-10-25: "accept all recommendations"); decision unchanged in substance, Status unchanged (Accepted).
- §9 — user decision 2026-10-25: `IA_WalkSlower` (Axis1D, `IMC_Common`; Left Ctrl → 0.8, Left Alt → 0.9) scales the **keyboard** Move vector on the owning client before `ServerMove`. It is never a command, adds no RPC or replicated field, and the server sees only an ordinary Move magnitude that SCL quantises to the shared pace set (IIF R15a, SCL R5.1). Gamepad input is untouched.
- §2, §9 — user decision 2026-10-25: `IA_Ping` (Boolean press, `IMC_Common`; G / D-pad Up) sends `ServerTeamPing(FNetTeamPing {FVector_NetQuantize10 Point})` (the point only; revised at the ping GDD's design review), a reliable Server RPC on `AStealthPlayerController` that is **not** a pawn command: it never enters `ServerPawnCommand`, the action slot, the resolution queue or `FActionState`, names no pawn, and changes nothing on the pawn. The Informant Pings handler validates it (phase, role, its own token bucket, range) and writes the C-TeamView ping part (ADR-0001 amendment). The static scan rule (no gameplay `Server|Client` UFUNCTION on `AStealthPawn`) is unchanged and covers it.
- §9 — user decision 2026-10-25: the client-local `ShowTeamNames` action is deferred to the Vertical Slice with the Teammate Markers hold (TM OQ11); `IOwnerCommandClientView::OnLocalUiAction` stays in the interface but is not bound in MVP.

Closeout 2026-10-25 (re-review follow-ups, `production/design-closeout/2026-10-25_rereview-followups.md`); decision unchanged, Status unchanged (Accepted).
- §2, §9 (`FNetTeamPing.Point`): the quantisation is `FVector_NetQuantize10`, whose resolution is **0.1 cm** (one decimal), not a 10 cm grid; any reading of the ping point as "10 cm quantised" is withdrawn. The 10 cm grid applies only to the IVP fallback position feed (ADR-0003, team position feed). The ping GDD's `S_entry` estimate and the IVP `TeamPing` row use the 0.1 cm figure (IVP/Pings re-review xdoc).
- §9 (closeout D8): `HudOwnRemovalHoldCapSec` (the bound in the Informant-context gate) has a TDC hard minimum of 1.25 s; the gate formula is unchanged.
