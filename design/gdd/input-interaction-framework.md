# Input & Interaction Framework

> **Status**: Reviewed - NEEDS REVISION 2026-10-24 (re-review pending); reconciled 2026-10-24; ADR-synced 2026-10-24; user decisions applied 2026-10-25 (`FActionState.StartServerTime` adopted; `LatencyWarnRttSec` retired in favour of HUD's `HudLatencyWarnRttSec`)
> **Author**: design-system pipeline (autonomous) + game-designer, ux-designer, gameplay-programmer, ue-replication-specialist, systems-designer, art-director, qa-lead, creative-director
> **Last Updated**: 2026-10-24
> **Last Verified**: 2026-10-24
> **Implements Pillar**: P1 Hide in Plain Sight; P2 Every Advantage Is a Tell
> **Creative Director Review (CD-GDD-ALIGN)**: REVISED 2026-10-24. CONCERNS (7 items) were all applied: camera-toggle exception in Player Fantasy; Open Question 1 narrowed; no Prepare/Interact inside a bush by default; only commanded displacement cancels a hold; brace sampled at Attack execution; gaze catalogue cited; status set.
> **Design Review**: NEEDS REVISION 2026-10-24 (24 revisions applied). Full review by systems-designer, qa-lead, ue-replication-specialist and creative-director; log: `design/gdd/reviews/input-interaction-framework-review-log.md`.

## Summary

The Input & Interaction Framework is the one set of server-validated commands that every character in the city is driven through: Move, Look, Traverse, Interact, Prepare, Attack and Hide. Human players reach these commands through their keyboard, mouse or gamepad, and NPC AI calls the same ones, so nothing downstream can tell a player from an NPC by how it acts. The framework validates every command on the server, owns the timing of held actions (tending an objective, bracing a defence), and announces actions to the systems that give them meaning. It exists so that the controls keep a player looking like a citizen until they choose otherwise (Pillar 1), and so that every advantage costs a visible deviation (Pillar 2).

> **Quick reference** — Layer: `Core` · Priority: `MVP` · Key deps: `Tuning Data & Config; Participant State & Roster (provisional); Round & Match Framework (provisional queue); Shared Character & Locomotion (runtime state)`

## Overview

The Input & Interaction Framework (IIF) is the **pawn command interface**: the single set of server-validated entry points through which *anything* drives a Shared Pawn. The commands are `Move`, `Look(Rotation)`, `Interact(Begin|End)`, `Prepare(Begin|End)`, `Attack(TargetSpot|TargetPawn)` and `Hide(Enter|Exit)`, plus the locomotion pass-through `Traverse`. A human player's keyboard, mouse or gamepad reaches them through Enhanced Input (UE 5.8) on the PlayerController. An NPC's AIController calls the very same entry points. Below that boundary no system can tell who issued a command; no `IsAI` parameter exists anywhere. That is the parity boundary that ADR-001 and Pillar 1 require. The IIF owns four further things. First, **validation**: every discrete command is checked on the server for rate, shape and legality (Participant state, locomotion state, the pawn's single action slot, the owning feature's predicate); the client is never trusted. Second, **hold actions**: the server-authoritative timer, progress and cancellation for held commands (objective interaction about 3 s, defensive preparation up to about 4 s), with only the owner's UI ring predicted on the client. Third, **generic action events**: `OnActionStarted`, `OnActionCancelled` and `OnActionCompleted(Pawn, ActionType)`, which feature systems subscribe to instead of reading input. Concealment, for example, listens to Attack. Fourth, **input contexts and defaults**: the Active vs Informant mapping contexts, default keys, the first/third-person toggle input, hold vs toggle alternatives, and the input latency budget. It does not decide what an action *means*. Assassination decides what a strike or a brace does, Objectives decides what a completed interaction advances, Concealment decides what entering a bush does, and Shared Character & Locomotion decides how Move, Look and Traverse animate. Players never see this system. They feel it as controls that keep them a citizen until they choose otherwise.

*Framing choices (autonomous, logged)*: the overview describes both the data layer and its player effect, because the system is Core but its feel is player-facing. No ADR exists yet, so ADR-001's direction ("AI drives it through the same movement/input path") is cited as the constraint and implementation choices are marked "-> ADR". The fantasy is indirect.

## Player Fantasy

*Framing: indirect / both. The creative-director was consulted and offered framings A "every action is a commitment", B "the hold is a visible choice" and C "you move like them until you choose not to". C was adopted, with B's sentence as the rule for the two hold actions (recommended option).*

**The controls keep you a citizen.** Let go of the stick and your body eases to a stop over half a second, exactly like the woman beside you. You did nothing to blend; the controls did it for you. Walking and looking cost nothing and say nothing. Every input beyond them is something you say out loud: hold Brace and the crowd sees you brace; kneel to tend the shrine and for three seconds you are the only body kneeling in the sun; press Run and you are the only one leaning. The one input that says nothing is the camera toggle. It is tell-free only because the third-person camera may see nothing your body's line of sight cannot (Camera System). If a camera mode ever gave more information, the toggle would become a Pillar 2 violation. The controls never make you slow, clumsy or late. The cost of an action is never input lag. It is what other people see.

Anchor moment: a stranger drifts toward you a little too directly. You press Brace before they arrive, because you read them, not because you reacted. For up to four seconds anyone watching can see your stance. If they keep coming, you were right.

*Pillars served*: P1 ("until someone commits, an enemy player cannot be told apart from an NPC by model, animation, audio, UI, or network presentation — only their choices give them away"). The default output of every input is the NPC baseline, and any deviation needs a deliberate input. P2 ("anything that helps a player ... must be a visible deviation from that baseline"). The cost of every non-default input is its tell and nothing else. Anti-pillar "NOT a reflex game": Brace is a decision made before the strike, never a parry window.

*Player-facing language rule*: use the verbs **Walk, Run, Brace, Strike, Tend** (objective) and **Slip in / Slip out** (bush). Never use parry, block, counter, dodge, react, "perfect timing", "window", QTE, exposure, suspicion, detected, spotted, stealth meter, disguise, target, lock on, enemy, player, NPC, AI, bot, health, shield, combo or hit. Prompts and reticles read the same for every valid target.

## Detailed Design

*Specialists consulted (full review mode)*: game-designer (rules), ux-designer (feel, mapping, prompts), gameplay-programmer (UE 5.8 feasibility against the prototype in `Source/StealthGame`), ue-replication-specialist (hold actions, validation, leak vectors). Where they disagreed, the recommended option was taken and logged: Prepare key (RMB vs Q, chose Q), interact release grace (0.15 s vs 0.10 s, chose 0.10 s), and the rejection-code policy. API details are **unverified for UE 5.8** and marked "(verify)".

### Core Rules

**R1. One boundary, one entry per command.** Every Shared Pawn carries one command component (provisional name `UPawnCommandComponent`, non-replicated, -> ADR). It exposes exactly these server-side entry points:

| Command | Payload | Kind | Execution owner |
|---|---|---|---|
| `Move` | 2D direction (unit-clamped) + `Gait` ∈ {Walk, Run} | continuous | Shared Character & Locomotion (CMC) |
| `Look` | `Rotation` (control rotation) | continuous | Shared Character & Locomotion (aim offset, head/body follow, smoothing) |
| `Traverse` | optional `TraversalPointHint` (the coping record the owner's local query found; Locomotion picks the traversal from context and re-validates with its own server query, never trusting the hint; SCL R7.2) | discrete | Shared Character & Locomotion |
| `Interact` | `Begin(TargetId)` / `End` | held | target's owner (Objectives in MVP) |
| `Prepare` | `Begin` / `End` | held | Assassination & Defence Reversal |
| `Attack` | always `WorldPoint` (view-ray point from Camera's `GetAimRay()`, CS-R8, including the `HideVolume` entry point when the ray enters a bush); plus optional `PawnRef` when the ray hit a rendered pawn (`TargetPawn`), else none (`TargetSpot`). `PawnRef` travels as the pawn's network id, never an object-pointer field (Round change request 6), and an id the server cannot resolve is treated as absent | instant | Assassination & Defence Reversal |
| `Hide` | `Enter(BushId)` / `Exit(Side)` (`Side` = 2-bit exit-side index chosen by the client from its look yaw; AI chooses through the same path; the server validates it, Concealment CON-R8) | instant (transition anim owned by Concealment) | Concealment (Bushes) |

`Gait` and `Traverse` extend the canonical vocabulary (change request in Cross-References). Informant and brace legality must gate running and parkour at the same boundary for every pawn, so they belong here.

**R2. No caller identity, anywhere.** No entry point, event, payload or listener parameter carries `IsAI`, `IsPlayer`, a controller pointer, a PlayerState pointer or a team. The command component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsNpc`, `GetTeam` or the Roster binding. The human path is a thin PlayerController Server RPC that resolves *its own possessed pawn* on the server and calls the component; the client never names the pawn it commands. The AI path calls the component directly on the server. **A listen-server host's local input takes the human path**: the host's PlayerController calls the same `IngestCommand(Source = HostLocal)` function that the Server RPC body calls. It gets a synthesized `ClientCommandSeq` and the same ingress checks (R5.1), and it uses the host's own Round mailbox. Its replies are queued and flushed with everyone else's (R6), never delivered inline. *(Design review 2026-10-24: the host no longer bypasses ingress. That would have been an `NM_ListenServer` special case and a fairness hole.)* Downstream, a human command and an AI command with the same content are byte-identical. The only exception is ingress (R5.1). Ingress sits before the boundary and other clients cannot see it. The only thing it attaches is an opaque `ReplySink`: the owning connection for human sources, null for AI. The command component never inspects the sink, it only hands it to the reply step (R6). This means no `IsPlayerControlled` check is ever needed to decide whether a reply is sent.

**R3. Move and Look are not routed through RPCs of their own.** Human Move and Look ride the CharacterMovementComponent's saved-move path (`ServerMove`, unreliable, batched). The control rotation carried there is Look. AI Move and Look are applied on the server through the same component functions. Observers see only the pawn's replicated movement and view pitch/yaw, which are identical for every pawn (Locomotion owns the net-parity settings). There is no Look action event, because a per-turn event would be a gaze tell source. That looking "says nothing" also depends on Crowd & NPC Behaviour's gaze catalogue (NPCs scan, glance at passers-by and look at stalls), so a turning head is not a player tell. Releasing all move input yields Locomotion's baseline deceleration (no sudden stop). Sharp stops and turns, if Locomotion offers them at all, need a deliberate input. *Parity requirement on Locomotion*: digital WASD release must never by itself produce the art bible's "sudden stop" or "abrupt reversal" tells (art bible 5.5).

**R3a. Look parity contract** (the IIF states the requirement; Shared Character & Locomotion owns the mechanism, -> ADR):
- **One control rotation for every pawn.** `Look` sets the pawn's control rotation for every pawn, human or AI.
- **One body-yaw rule.** Body yaw follows a single shared rule (turn-in-place and orientation) for every pawn. No pawn uses a controller-yaw-driven body while another uses orient-to-movement. Provisional mechanism: `bUseControllerRotationYaw = false` for all pawns.
- **One replicated pitch path.** Simulated-proxy view pitch is replicated through the same path for every pawn. The likely path is `RemoteViewPitch`, set from the control rotation for AI and human alike (verify in 5.8).
- **Same cadence and smoothing.** Human moves arrive as client-timestamped `ServerMove` bursts and NPC moves are stepped per tick. Observers must still see the same cadence and smoothing for both (Locomotion net-parity settings).

On an opponent client, a stationary human turning the camera must be indistinguishable from an NPC performing a gaze-catalogue turn of the same angle (AC 54).

**R4. Discrete commands are edges, not states.** Clients send `Begin`/`End` (or the instant command) on input *edges* only: Enhanced Input `Started` → Begin, `Completed`/`Canceled` → End. The Enhanced Input **Hold trigger is never used** for server-meaningful holds, because the server owns every hold timer. No command ever auto-repeats while a key is held. After any cancellation, a held key needs a fresh press. When the client loses the ability to observe a release, it sends `End` for every outstanding `Begin` immediately. That covers a mapping-context swap, a menu opening, window focus loss and possession change; an input flush may not fire `Completed`.

**R5. Two-stage server validation.** Every discrete command passes:

1. **Ingress check.** Runs on RPC arrival, on the human path only, including the host's `HostLocal` source (R2). It is cheap, and it checks:
   - The payload is well-formed: enums in range, vectors finite.
   - `WorldPoint` is **clamped** (not dropped) to `AssassinationRangeCm + CommandRangeSlackCm` of the pawn.
   - A `PawnRef` naming the commanding pawn itself is stripped from the command and counted. The Attack proceeds as a `TargetSpot`.
   - `ClientCommandSeq = (PossessionEpoch, Seq)` is strictly increasing. Stale or duplicate values are ignored, and a value from an earlier possession epoch never reaches the newly possessed pawn. `PossessionEpoch` is **owned by the server**: the controller counts its own possessions and delivers the value to its owner by the owner-only reliable RPC `ClientSetPossessionEpoch(Epoch)`, sent before the engine's client restart on every possession (ADR-0011 §2; Owner audience, a fact the owner already has: its own possession).
   - **Interact `TargetId`**: an id that is not a public Interact target (unknown, or not a court fixture or other public interact geometry) is dropped. **Every court fixture id is accepted**, whatever its role and whether the sender's team knows it (Objectives OC-6; Visibility Policy R7.4a, ruling D). The hidden-site oracle guard is kept by resolution, not by ingress: every non-live court, the hidden final included, resolves identically to `AllowNoEffect` (Objectives OC-7), so acceptance, reply, hold duration and timing never depend on a court's role (AC-IVP-41). The team's live-site set drives presentation only (UI 1, UI 3). *(Reconciliation 2026-10-24: replaces the earlier "drop ids outside the owner team's Policy-visible site set", so humans can feint-tend any court exactly as NPCs decoy-tend, Crowd CNB-R4.)*
   - The per-connection transport bucket (F3) may drop `Begin` and instant commands. It **never drops an `End` that matches an outstanding Begin**. Unmatched Ends above `UnmatchedEndPerSec` are dropped.

   Ingress drops are silent (no reply) and counted for the server-only abuse log. An RPC `WithValidation` failure, which disconnects, is reserved for structurally impossible payloads only. A command that passes ingress is placed in its source's **Round mailbox** together with its opaque `ReplySink`, via `RoundQueue.Submit(Command, SourceMailbox)`. The mailbox is the ingress source and is never in the payload, so R2 holds. AI commands go to the server-local mailbox with a null sink.
2. **Command check.** Runs inside the component's queue resolver, at the command's queue slot, for every pawn, human or AI. This is the only point where stage 2 runs, so every source is checked against the same live state at the same point in the frame:
   a. **legality class** of the pawn (R7), queried from Roster as `GetLegalityClass(Pawn)` ∈ {Active, Informant, RoundEnded} (provisional Roster API). Unbound pawns resolve to `Active`. The component never learns whether the pawn is bound;
   b. locomotion restriction from Locomotion's server state: `IsRunning`, `IsInTraversal` (server state, not key state);
   c. action-slot arbitration (R9);
   d. the **owning feature's predicate**, `CanBegin(Pawn, Command)` → {`Allow`, `AllowNoEffect`, `Reject(Code)`}, registered per ActionType by the owning system. Examples: Assassination checks the defence cooldown and attack lockout; Objectives checks site validity; Concealment checks bush proximity. `AllowNoEffect` runs the action exactly like `Allow` (same pose, timer, events) but its completion changes nothing. It exists so that a decoy action (for example an NPC tending a site it has no side for) is indistinguishable from a real one. Objectives decides when it applies. The choice between `AllowNoEffect` and `Reject` may depend only on the site and the pawn's legality class, never on who controls the pawn. Otherwise NPCs could tend sites that humans are rejected from, which would be a tell;
   e. per-pawn, per-type token bucket (F3), **consumed only when the command passes a–d and executes**, so mashing a key during a cooldown never starves the first legal press. `End` is never rate-limited, and an `End` with no matching active action is a no-op.

   **Ordering.** The IIF never stamps anything itself. It uses Round's queue API by its Round names: `RoundQueue.Submit`, `GetKey`, `Compare`, `CurrentKey`; follow-ons (Objectives' completion effect, Assassination's outcome) run inline in the item's own slot (Round change request 6). Round assigns each command its total-order key (`ServerTickIndex`, then `Seq`, stamped at drain start by Round R12's mailbox rotation). This `Seq` is distinct from the client's `ClientCommandSeq`. Inside one mailbox the order is FIFO, so an `End` always resolves after its `Begin`, even when both arrive in the same frame. A tap therefore produces a 0.10 s brace (F2), never a full-cap one. Because stage 2 runs at the slot, any earlier event in the same drain (a kill, a state transition, a phase change, a hold timer) is already reflected in the state being checked. A command that passes executes and emits `OnActionStarted` inside its own slot. A command that fails is `Voided` and gets its reply per R6. *(Design review 2026-10-24: stage 2 moved from pre-submit to resolution. This replaces "submit, then re-validate", and removes the frame-position dependence between net-receive and host-local commands.)*

**R6. Rejections reveal nothing about anyone else.** A rejected command produces no event, no replicated change and no cooldown change. Replies follow **two tiers**:

- **Ingress tier (R5.1): silent by design.** No reply is sent for a malformed payload, a rate-limited command, a stale seq, or an unknown or Policy-invisible `TargetId`. An honest client never sends an invisible id, so no honest player loses feedback.
- **Resolution tier (R5.2a-e): exactly one reply.** A command voided at resolution, *for any reason, including "pawn removed earlier in this drain"*, sends exactly one owner-only `ClientCommandRejected(ClientCommandSeq, Code)` to its `ReplySink`. On the wire this is an **entry kind of the batched owner-only reliable RPC `ClientCommandReplies`**, flushed once per connection at the end of the pass that produced it (drain end or staging pass), in submission order; Assassination's owner results and hold status ride the same batch (ADR-0011 §8). `Code` ∈ {`NotNow`, `Cooldown`, `Busy`, `OutOfReach`}. A null sink (AI) sends nothing, and that is the only difference, decided by the sink rather than any caller check.

Code mapping:

| Failure | Code |
|---|---|
| 2a, 2b, and a 2e bucket refusal | `NotNow` |
| 2c | `Busy` |
| 2d | the predicate's code (`Cooldown` for the defence cooldown and for Concealment's `HideEnter` re-entry cooldown, `BushReenterCooldownSec`; `NotNow` for Objectives' full-fixture refusal, OC-7) |
| Item voided because Round's latch closed the queue (`Rejected_Latched`, Round R9/R15) | `NotNow`, through the same drain-end reply step |
| Interact distance **or facing** failure at Begin | `OutOfReach` |

Every code is computed from facts the owner already knows: their own state, their cooldown, their slot, and their distance and facing to a site their team may already know. **No code ever depends on a target pawn's identity, team, concealment or brace state.** After attacker-side legality passes, an `Attack` is never rejected. It always executes as a strike, and the target outcome is Assassination's hit or whiff, identical in content and timing for NPC, player, informant, concealed pawn or empty spot. **Every reply** (a rejection or an `FHoldStatus` update) is **enqueued only at the end of the queue-resolution step** of the drain in which the command or hold timer resolved. It goes out on the same reliable owner channel at the next net flush, whatever stage the command reached and whatever it contained (timing side-channel guard). No deferred item may reply later. The host's local sink obeys the same queue and is never called inline. A reliable resend delays the reply only for the owner, who is the only party that can observe it. Assassination's owner-only `FStrikeResult` and `FDefenceResult` ride the same drain-end reply step (ASN-R19). **Presentation gating** (ASN-R23): a reply to a command of a pawn that was struck in the same or an earlier drain is *presented* by the owner's client no earlier than that pawn's own contact time (`ContactServerTime`), never on receipt; enqueue timing is unchanged.

**R7. Legality by class and locomotion state.** ✓ = legal (subject to slot and feature predicate), ✗ = rejected with `NotNow`.

| Command | Active (incl. every unbound pawn) | Active while running | Active while in traversal | Informant | RoundEnded |
|---|---|---|---|---|---|
| Move (Walk) | ✓ | ✓ | (Locomotion-owned) | ✓ | ✗ (Round may freeze) |
| Move (Gait=Run) | ✓ | ✓ | ✓ | clamped to Walk | ✗ |
| Look | ✓ | ✓ | ✓ | ✓ | ✓ |
| Traverse | ✓ | ✓ | ✗ (already traversing) | ✗ | ✗ |
| Interact Begin | ✓ | ✗ | ✗ | ✗ | ✗ |
| Prepare Begin | ✓ | ✗ | ✗ | ✗ | ✗ |
| Attack | ✓ | ✗ | ✗ | ✗ | ✗ |
| Hide Enter/Exit | ✓ | ✗ (Enter) / ✓ (Exit) | ✗ | ✗ | ✗ |
| Camera toggle | local, never a command (R14) | | | ✓ | ✓ |

While a `Prepare` is active, `Gait=Run` is clamped to Walk and `Traverse` is rejected ("no run/parkour while prepared"). **A braced pawn neither walks nor turns** (planted brace: SCL R5.2, confirmed by Assassination ASN-R3/OQ9): Move input is accepted and ignored for translation and yaw while `Bracing`, it never cancels the brace, and only head-look moves (answers OQ3). During a Hide transition, Move is zeroed (Concealment CON-R4a); Look stays free within the head clamp. "Informants only get Move/Look/camera" is enforced here for every informant pawn, whatever its controller.

**R8. Hold actions (Interact, Prepare).**

- **Server timer.** On execution the server records `ActionStartServerTime`, the server world time at queue resolution, held in double precision (F1). It never accepts a client timestamp and never back-dates; there is no forgiveness knob.
- **Hold timers are Round queue items.** Every time-driven end of a hold resolves inside Round's queue, never on the IIF's own tick. The item type is `HoldTimer(Complete | CapEnd | DebounceExpiry | ConditionCancel)`. Time-driven ends are scheduled when the hold begins as **due-time items** in Round's queue (`ScheduleAt(DueTime, …)`, ADR-0002 §4; equal due times order by the hold's Begin key, F1, F2); the IIF keeps no tick or timer of its own. Cancels caused by a condition (range, displacement, state) are detected by a read-only condition sweep that schedules a `HoldTimer(ConditionCancel)` at the current server time, so the cancel itself resolves in a queue slot (ADR-0011 §5). The IIF resolves all four. It carries the hold's `ActionEpoch`, so a timer for a hold that has already ended is voided.
  - **Same-frame order.** Timers due in the same frame are stamped in order of `(DueTime, the Key of the Begin that started the hold)`. This needs no participant identity (change request to Round, whose R12 sorts by `ParticipantId`).
  - **Resolution.** The resolver re-checks that the hold is intact (legality, range, target validity), then acts in its own slot. It emits `OnActionCompleted` for `Complete` and `CapEnd`, or executes the pending End for `DebounceExpiry`.
  - **Interact effect.** For Interact, Objectives applies its effect as a follow-on inside that same slot. Objectives' `ObjectiveCompletion` therefore inherits the `HoldTimer(Complete)` key (change request to Round R13 and Objectives).
  - **Order against commands.** Any command with a lower key than the timer is seen first. An End that resolves before `HoldTimer(Complete)` is pending; it cancels the hold if its debounce expires first. An Attack that resolves before `HoldTimer(CapEnd)` meets a braced defender. There is no general "completion wins" rule: **key order decides everything** (P3: one strike, one deterministic outcome).
- **Interact.**
  - **Target.** `TargetId` is a hint. The server accepts any public Interact target id, every court fixture included (R5.1); anything else never reaches stage 2. Objectives' predicate rejects `NotNow` when all of the fixture's `FixtureInteractSideCount` stand points are physically occupied (OC-7, OC-8), identically for every pawn and every court.
  - **Begin check.** At Begin only, the horizontal distance from capsule centre to the site's interact point must be ≤ `InteractRangeCm + CommandRangeSlackCm`, and the control-rotation yaw must be within `InteractFacingHalfAngleDeg` of the point (F6). Failing either check replies `OutOfReach`.
  - **After Begin.** Only distance is re-checked, against the cancel range `InteractRangeCm + CommandRangeSlackCm + InteractCancelHysteresisCm` (240 cm by default). Look therefore never cancels.
  - **Repeat Begin.** `Interact(Begin)` while already interacting is an idempotent no-op.
  - **Duration.** `D` comes from the target owner (Objectives: `ObjectiveInteractSec`, 3.0).

  **Cancels**, each with its server-only `CancelReason`:

  | Cause | `CancelReason` |
  |---|---|
  | End (after the debounce) | `Released` |
  | *Commanded* displacement > `InteractMoveCancelDistCm` (F6) | `Moved` |
  | Gait=Run | `Moved` |
  | Traverse | `Moved` |
  | Leaving the cancel range | `OutOfRange` |
  | An Attack, Prepare or Hide by the same pawn (R9) | `Superseded` |
  | Legality-class change or phase change | `Illegal` |
  | The target invalidated by its owner (never by a completion at the same court: other holds there run to their own end with no effect, Objectives OC-8) | `TargetInvalid` |
  | The pawn removed | `PawnRemoved` |

  **Commanded displacement** is the horizontal distance from the pawn's position at Begin. It accumulates only on ticks where the pawn's current Move intent is non-zero, and not during the first `InteractMoveGraceSec` (0.25 s) after execution. Move intent means human Move input or a Move issued by Crowd's AI follower; both enter through `Move` (R17). External pushes such as crowd avoidance or collisions never count. They cancel only through the range check. **Does not cancel**: Look, being approached, being targeted (undetectable by design), camera toggle. **Early release resets progress to 0.** Partial progress is never retained by the framework, so the tell stays one contiguous, readable interval (P2, P4). When `HoldTimer(Complete)` resolves with the hold intact, the server emits `OnActionCompleted(Pawn, Interact)` and the target owner applies the effect in the same slot. The release debounce has a stated consequence here. A release less than `ReleaseDebounceSec` before `D` still completes, because the End is still pending when `Complete` resolves. That is the same 0.10 s grace for every pawn, and it is not partial credit. When `HoldTimer(Complete)` and `HoldTimer(DebounceExpiry)` of the same hold share a `DueTime` exactly, `Complete` resolves first (Objectives change request 1b).
- **Prepare.** The defence is a **protected window that starts at execution**, not a charge-up. It ends at `End` or when the window reaches `DefenceDurationSec` (default 4.0, range 3-5), whichever comes first. A cap end emits `OnActionCompleted(Pawn, Prepare)`; an earlier end emits `OnActionCancelled`. The cooldown (`DefenceCooldownSec`, 60) is stamped by Assassination **at execution**, so an unused or early-released brace still burns it. `Prepare(Begin)` while a brace is active is an idempotent no-op with no reply. While the cooldown runs it is rejected `Cooldown` by Assassination's predicate (R6). In both cases there is no event, no restart, no extension and no cooldown change. **Mashing Prepare can neither reset nor extend anything.** Cancels: End (after the debounce), an *externally forced* run or traversal state reported by Locomotion (requested Run is clamped by R7 and cannot cancel), Attack by the same pawn (R9), a reversal that consumes the brace (`Reversed`, Assassination ASN-R7/R14: the slot becomes `Countering`), legality class change, pawn removed. On a reversal the consumed brace's `FHoldStatus` end carries `ServerEndTime = ContactServerTime`, and the owner's brace indicator ends at that server time, not on receipt (ASN-R23). The cap end is a `HoldTimer(CapEnd)` queue item, due at t_start + `DefenceDurationSec` (F2). Until an End is processed, `FHoldStatus.ServerEndTime` = start + cap; it is updated when the End executes. The **client does not send `Prepare(Begin)` while the owner-known cooldown is running**. That is an owner-side traffic saving only, with no effect anyone else can see. The server still validates every Begin it receives. This interpretation is **confirmed** by Assassination (ASN-R3, which owns the meaning; resolves OQ2); art bible 7.3's "charge then armed" ring is superseded (Assassination change request 10).
- **Release debounce, server-side, for every pawn.** Clients and AI send `End` immediately. On the server, an `End` becomes *pending* for `ReleaseDebounceSec` (0.10 s). A same-type `Begin` from the same pawn inside the pending window cancels the pending End. The action simply continues: no token, no new event, no cap restart, no cooldown change. When the window expires, a `HoldTimer(DebounceExpiry)` item resolves in the queue and the End executes. So the effective end is End arrival + 0.10 s for every pawn, and the shortest possible brace is 0.10 s for humans and AI alike (F2). This absorbs key flicker and toggle double-taps without a human-only code path; a client-side debounce was rejected because it gave humans a longer minimum brace than AI (a P1 tell). It is not a setting in MVP.
- **No observer-side data.** Hold progress, start time, duration and cooldown are never replicated to anyone but the owner (R12).

**R9. One action slot per pawn, fixed priority.** A pawn performs at most one of {Interact, Prepare, Attack, Hide transition} at a time. Priority is **Attack > Prepare > Interact > Hide**. A higher-priority command preempts (cancels, reason `Superseded`) an active lower one. A lower-priority command is rejected `Busy` against an active higher one. **Hide transitions are atomic**: they are rejected while any action is active, and while a transition runs every other discrete command is rejected `Busy`, Attack included. Concealment owns the transition length; the slot is never left in a half-entered state. **Prepare and Interact inside a bush are rejected `NotNow`** (confirmed by Concealment, change request 1; resolves the bush half of OQ3). A concealed brace would stack two advantages behind one tell and counter blind attacks (P2, anti-pillar "not hide-and-wait"). Concealed tending would defeat "the objective as a lens" (P4). Attack from a bush stays legal and breaks concealment. **Same-type commands** follow fixed rules:
- `Interact(Begin)` while interacting is a no-op (R8).
- `Prepare(Begin)` while bracing is a no-op (R8).
- `Attack` while `Striking`, including during recovery or lockout, is rejected `Busy`.
- `Hide` during a Hide transition is rejected `Busy`.

Nothing auto-resumes after a preemptor ends. Attack recovery and lockout (`AttackAttemptLockoutSec`, Assassination-owned) occupy the slot for their duration. A defender whose brace reverses a strike enters **`Countering`** for `CounterDurationSec` (Assassination ASN-R14): pose-locked, not braced, and every discrete command is rejected `Busy`.

**R10. Attack targeting has no lock-on and no target-dependent presentation.** The client builds the command from the camera-centre view ray, obtained from Camera's `GetAimRay()` (the IIF never builds its own ray; Camera CS-R8). Every Attack carries the ray's `WorldPoint` (ground point under the ray, clamped to range; Camera's `WorldPoint` includes the `HideVolume` intersection when the ray enters a bush, Concealment CON-R12). If the ray hits a non-ally pawn the client currently renders, within `AssassinationRangeCm + CommandRangeSlackCm`, the command also carries its `PawnRef` (`TargetPawn`); otherwise it carries none (`TargetSpot`). **"Rendered" means Camera's eye-mask flag α > 0** for that pawn on this client (CS-R5), never the engine's was-recently-rendered test; a masked pawn is never the `TargetPawn`. The client does not issue Attack at a pawn bearing its own team's teammate marker, which means `IsMarkedAlly(Pawn)` is true (Teammate Markers TM-R10, CR-4; the owner already knows its allies). Whether a server-side teammate strike is possible is Assassination's rule. The server treats `PawnRef` and `WorldPoint` as hints. **`PawnRef` is honoured only if** (a) `CanViewerKnow(Attacker, Pawn, Position)` is true, so the attacker's client may legitimately be rendering that pawn, and (b) the pawn lies within `PawnRefAgreementCm` (75) of the `WorldPoint`. Otherwise it is ignored. An invalid, stale, forged, concealed or unreplicated `PawnRef` therefore always falls back to the `WorldPoint`; in particular an `Attack(TargetPawn)` naming a pawn that is culled for the attacker resolves as `Attack(TargetSpot)` with no distinct reply (Visibility Policy R7.4). The result is identical to a `TargetSpot`, and no reply or timing differs. Assassination resolves the victim using the **same authoritative visibility check for both forms**, so naming a concealed pawn's id gains nothing a `TargetSpot` would not (x-ray oracle guard). **The defender's brace state is sampled at Attack execution (queue resolution), never during the strike animation.** The outcome is decided before any swing is visible, so no parry window can exist (anti-pillar "not a reflex game"). There is no reticle. The owner's contextual Strike hint is identical for any rendered non-ally pawn in range, and nothing appears for `TargetSpot` (UI Requirements 4; this follows art bible 7.1's ally exclusion, so 5.2 needs reconciling). Gamepad uses the same ray, and there is **never aim assist, snapping or magnetism** (anti-pillar; accessibility guard).

**R11. Generic action events (server only).** The IIF exposes three server-only multicast events, aggregated in one world-level hub so a listener subscribes once rather than once per pawn (-> ADR):

- `OnActionStarted(Pawn, ActionType)`: emitted on execution after queue resolution, in Round's total order.
- `OnActionCancelled(Pawn, ActionType)`: a held action ended before its natural end. A server-only `CancelReason` ∈ {Released, Moved, Superseded, Illegal, OutOfRange, TargetInvalid, PawnRemoved, Reversed} (`Reversed`: a brace consumed by a reversal, Assassination ASN-R14). For Prepare: End → Released, an externally forced run or traversal → Moved, Attack by the same pawn → Superseded is readable by listeners and logged; it never reaches any client.
- `OnActionCompleted(Pawn, ActionType)`: Interact reached `D`, or Prepare reached its cap. Attack and Hide emit only `OnActionStarted`. (Completed is an extension of the canonical vocabulary; change request.)

`ActionType` = {`Interact`, `Prepare`, `Attack`, `HideEnter`, `HideExit`}. Move, Look and Traverse are not ActionTypes. Locomotion publishes its own run and traversal state. Listeners must not infer identity from event timing, and no listener may relay an event to a client except through its own Visibility Policy audience.

**R12. What replicates.**

- *To everyone, for every pawn*: one replicated action state, the struct **`FActionState {Pose, Seq, Variant, StartServerTime, ContactServerTime, WarpPoint}`** with a single RepNotify (Assassination ASN-R21; reconciliation 2026-10-24: replaces the bare `ActionPose` enum, which is now its `Pose` field; "`ActionPose`" elsewhere in this GDD means `FActionState.Pose`). `Pose` ∈ {None, Brace, InteractPose, Strike, Counter, BushEnter, BushExit} (`BushTransition` split per Concealment change request 1; `Counter` per ASN-R14). `Seq` starts at 1 and increments on every write; `Variant` is the clip variant chosen by the owning feature (strike bearing, ASN-R16; never by outcome); `ContactServerTime` and `WarpPoint` are filled by Assassination for `Strike` and `Counter`, and hold fixed sentinel values for every other pose. The struct has the same shape and size for every pose and outcome (custom fixed-width serialiser, -> ADR; Assassination owns the field semantics). It is written only by the server in the component (Assassination's death-write `Counter` included, through the component), for NPC and player alike, with the class-default condition and the same cadence rules (the same net update settings for every pawn, and a forced net update on every write, so the replication delay U is identical). Observers drive animation from it and nothing else. It carries no progress, duration, cooldown, target reference or team. `StartServerTime` (the server time the pose began, 64-bit; Concealment CR-10b, adopted by user decision 2026-10-25) lets a re-created or late-relevant actor seek any clip, including hide clips, from server time; it is written in every `FActionState` write, identically for every pawn kind. **Relevancy parity bans** (owned and specified by Locomotion SCL R13; restated here; verify ForceNetUpdate behaviour under Iris in 5.8):
- No Shared Pawn uses net dormancy. Idle NPCs are tempting candidates for it, which is why the ban is explicit.
- No Shared Pawn uses `bOnlyRelevantToOwner`.
- No pawn gets per-class priority or relevancy overrides.
- No teammate-only always-relevance (SCL R13). (Locomotion/Animation own the pose set; the IIF owns the write.)
- **One exception** (Visibility Policy R6/R7): a pawn's relevancy to a connection may depend on its concealment state and the viewer's relation, and on nothing else (never binding, controller kind or participant state). The concealment cull is the Policy's, implemented through SCL R13's hook.
- *To the owner only*, on the PlayerController, never on the pawn: `FHoldStatus {ClientCommandSeq, ActionType, Outcome ∈ {Active, Completed, Cancelled}, ServerStartTime, ServerEndTime}` for the ring. It is sent on execution and again on every server-side end, carries no cancel reason, and a status whose `ClientCommandSeq` is older than the owner's current hold is ignored. The owner-only data also includes rejection codes, and the owner's cooldown end time (provided by Assassination). Because these live on the controller, no per-pawn property differs between an NPC pawn and a player pawn.

**R13. Input contexts.** Mapping contexts are UX only; the server validates regardless.

- `IMC_Common` (always): Move, Look, Camera toggle, Shoulder swap (local, effective in third person only; Camera CS-R10), pause/menu.
- `IMC_Active`: Run, Traverse, Interact, Prepare, Attack, Hide.
- `IMC_Informant`: no gameplay actions. Presses of the former Active keys are caught *locally* and show an owner-only plate, "Informants can only observe" (2 s, at most once per 3 s, no sound). Nothing is sent to the server.

The context swaps on the owner's **presented own role** (HUD-R6a) and on possession: `IMC_Informant` is selected only once the owner's own state (Owner-audience replication from Roster) is `Informant` **and** the HUD presents it (or no HUD presenter is bound, or the HUD's removal-hold cap has elapsed); until then the client keeps `IMC_Active` and the server rejects with `NotNow`, so the victim gets no pre-contact warning (ADR-0021 §8, ADR-0011 §9). A return to `Active` swaps immediately. On a swap, all held inputs are flushed and require a release. The 5.8.4 flush CVar is `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (digital keys only; verified in source, ADR-0011 E2); its exactly-one-`Completed` behaviour on flush is spike V1.

**R14. Camera toggle is local.** First/third-person toggle is a press, takes effect immediately, has a 300 ms local lockout, works mid-hold and mid-move, and is never sent through the command interface. The body presentation and permissions are identical in both modes (Camera owns this). If Camera's ADR needs a server-side view check, Camera owns that message, not the IIF.

**R15. Default bindings** (rebindable; gamepad support is *partial* in MVP: every gameplay command is mapped, default sensitivity only, no gamepad remap UI until Settings & Accessibility, no aim assist ever).

| Command | Keyboard / mouse | Gamepad | Input type |
|---|---|---|---|
| Move | W A S D | Left stick | axis |
| Look | Mouse | Right stick | axis |
| Run (Gait) | Left Shift | L3 (hold) | hold |
| Traverse | Space | A / Cross | press |
| Interact (Tend) | E | X / Square | hold |
| Prepare (Brace) | Q | LT / L2 | hold |
| Attack (Strike) | Left mouse | RT / R2 | press |
| Hide (Slip in / out) | F | B / Circle | press (toggles Enter/Exit by state) |
| Camera toggle | V | Y / Triangle | press |
| Shoulder swap (3P only; local, never a command) | X (middle mouse alternate) | D-pad Right | press |

Prepare is deliberately *not* right mouse. Right-mouse-hold is the strongest block/parry mental model in games, and that pulls the defence toward a reflex reading. It also keeps Strike and Brace on different hands. Hide has its own key so that an objective next to a bush is never ambiguous.

**R16. Hold vs toggle alternative (accessibility, provisional).** A client-local setting may translate press/press into Begin/End for Prepare, Interact and Run. The second press sends End, so early release stays available and stance durations are equivalent. The server stream is identical to hold mode, the setting is never replicated, and caps and cooldowns are unchanged. MVP ships hold only; the decision is an open question for Settings & Accessibility. The guard applies: no accessibility feature may give directional or player-only information or alter durations.

**R17. AI uses the same entry points under the same rules.** AIControllers call R1's functions and are subject to R5–R9 with no exemptions, including token buckets. AI motion is issued by Crowd's follower as `Move` commands through the same entry point (ADR-0016 §4, input emulation); the engine path-following component no longer feeds `Move`: its direct-move and path-move requests are a fault-and-no-op backstop (ADR-0010 §6, SCL R5.7). That way Move intent, and therefore commanded displacement (F6), means the same thing for AI and humans. AI-issued Prepare, Interact or Hide (red herrings) are Crowd & NPC Behaviour's decision (Baseline and Tell Matrix; CNB-R7, F2). NPCs never issue `Attack` (Crowd T6: the strike is the intended confession). AI `Interact` at a court takes exactly the human path: no ingress exemption is needed, because ingress accepts every court id (R5.1). The IIF only guarantees that they look and replicate identically.

### States and Transitions

Per-pawn **action slot** state (server-authoritative; the owner holds a predicted copy for the ring only):

| State | Entry condition | Exit condition | Behaviour |
|---|---|---|---|
| `Idle` | default; any action ends | a discrete command executes | `ActionPose=None`; Move/Look free |
| `Interacting(Target, Start)` | `Interact Begin` executes | `HoldTimer(Complete)` resolves with the hold intact → Idle (Completed); any R8 cancel → Idle (Cancelled) | `ActionPose=InteractPose`; progress per F1 |
| `Bracing(Start)` | `Prepare Begin` executes | `End` or cap → Idle; Attack → `Striking`; reversal → `Countering`; cancel → Idle | `ActionPose=Brace`; planted (no translation, no yaw; SCL R5.2); Traverse rejected |
| `Striking` | `Attack` executes (preempts any state) | Assassination's strike/recovery/lockout time elapses → Idle | `ActionPose=Strike`; slot busy |
| `Countering` | a braced defender reverses a strike (Assassination ASN-R14; the brace cancels `Reversed`) | `CounterDurationSec` elapses → Idle | `ActionPose=Counter` (written at the death write, ASN-R21); pose-locked; every discrete command `Busy` |
| `HideTransition(Enter|Exit)` | `Hide` executes from Idle | Concealment's transition time elapses → Idle | `ActionPose=BushEnter` / `BushExit`; Move zeroed |

Transitions out of any state on a legality-class change (e.g. Active → Informant) happen in the same queue resolution as the transition and emit `OnActionCancelled` with reason `Illegal`. Pawn removal (kill, despawn, unpossess) cancels with `PawnRemoved`.

Owner-side **ring** state (client, presentation only): `Hidden → Predicting` (local press, legal by owner-known data) `→ Confirmed` (FHoldStatus arrives) `→ Hidden` (completion, cancel or rejection). If no confirmation arrives within F4's timeout, `Predicting → Hidden`.

### Interactions with Other Systems

| System | Flows into IIF | Flows out of IIF | Interface owner |
|---|---|---|---|
| Tuning Data & Config | IIF knobs (Tuning Knobs table); `ObjectiveInteractSec`, `DefenceDurationSec`, `AssassinationRangeCm` read via `GetServerTuning()` / `GetClientTuning()` | none | TDC |
| Participant State & Roster | `GetLegalityClass(Pawn)` (provisional; Active for unbound pawns); Owner-audience own-state change (context swap) | none (the IIF never calls `RequestTransition`) | Roster |
| Round & Match Framework | Round key per item (mailbox rotation, R12); resolver callback; phase (`Active` vs other); `WrongPhase` drops (no reply) and `Rejected_Latched` items (IIF replies `NotNow`, R6); queue API `Submit`/`GetKey`/`Compare`/`CurrentKey` | `PawnCommand` items via `RoundQueue.Submit(Command, SourceMailbox)`; `HoldTimer(Complete/CapEnd/DebounceExpiry)` server items | Round (queue), IIF (items, resolver) |
| Shared Character & Locomotion | `IsRunning`, `IsInTraversal`; executes Move/Look/Traverse; owns `ActionPose` animation | Move, Look, Gait, Traverse; `ActionPose` writes | Locomotion (execution), IIF (gate) |
| Camera System | `GetAimRay()` / `WorldPoint` (CS-R8) and the eye-mask "rendered" flag (α > 0) for R10 and the Strike hint | toggle and shoulder-swap inputs (local) | Camera (aim ray, mask), IIF (bindings) |
| Assassination & Defence Reversal | `CanBegin` for Prepare/Attack (cooldown, lockout); cooldown end time (owner) | Prepare window start/end, Attack command + target hint; events | Assassination (meaning) |
| Concealment (Bushes) | `CanBegin` for Hide (re-entry `Cooldown`); transition time | Hide commands (`Enter(BushId)`, `Exit(Side)`); `OnActionStarted(Pawn, Attack)` (breaks concealment) | Concealment |
| Objectives Chain | `CanBegin` for Interact (legality and full-fixture `NotNow` only; every court otherwise `Allow`, OC-7), duration `D`, `TendCueDelaySec`; Team-audience live-site set (owner ring colour after `TendCueDelaySec` only; never ingress, IVP R7.4a) | `OnActionCompleted(Pawn, Interact)` inside the `HoldTimer(Complete)` slot (Objectives' effect is a follow-on there); cancels | Objectives |
| Crowd & NPC Behaviour | AI calls the R1 entry points | identical rejections and events | IIF |
| Information Visibility Policy | audiences (`Owner`, `All`) for R12 | owner-only status, `ActionPose` | Policy |
| HUD | none | owner-only `FHoldStatus`, rejection codes, prompt legality (local); UI 1-8 data, laid out and layered by HUD | HUD (display, anchors and layering), IIF (data) |
| Settings & Accessibility (VS) | client-local remaps, hold/toggle | none | Settings |

## Formulas

*systems-designer consulted. Corrections it made that were adopted:*
- Completion uses an epsilon test rather than tick-ceil, because listen-server frame time varies.
- The debounce moved server-side for parity.
- Tokens are consumed only on execution.
- The cancel hysteresis was widened.
- The move-cancel uses displacement instead of a stick deadzone.

*Design review 2026-10-24 corrections:*
- Hold timers resolve as Round queue items.
- U is one server frame under forced net update.
- The F3 load figure and example were fixed.
- T_ack covers one reliable resend.
- The F5 ranges were recomputed.
- The F6 cancel range is derived from a hysteresis key.

**Shared variables:**
- `t_now` and `t_start` are server world time in seconds, held in **double precision**. A float32 time after about an hour of uptime has a step of about 2.4e-4 s, which is larger than ε. `t_start` is the queue-resolution time of the executing command.
- `dt` is the server frame time: 0.0083-0.05 s, and **not fixed** on a listen server.
- `RTT_est` is the owner's smoothed ping: 0-0.3 s.

### F1. Hold progress and completion timer (server)

The hold_progress formula is defined as:

`P = clamp((t_now − t_start) / D, 0, 1)`. At each queue drain the IIF checks every active Interact. On the **first drain where `t_now − t_start ≥ D − ε`**, it submits `HoldTimer(Complete)` with `DueTime = t_start + D`. This is a server-generated item, stamped after that frame's intake items (Round R12). Completion happens when that item resolves with the hold intact (R8).

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_now | double (s) | ≥ 0 | calculated | server world time at this drain |
| t_start | double (s) | ≥ 0 | calculated | queue-resolution time of the Begin |
| D | float (s) | 2.0-6.0 | data file (`ObjectiveInteractSec` 3.0 for Interact; Objectives OC-14 hard range 2.0-6.0) | hold duration |
| ε | double (s) | 1e-4 | constant | guard against float error (3.0 / (1/60) is not exactly 180) |

**Output Range:** P ∈ [0, 1]. Completion overshoots D by [0, dt), because it is quantised to server drains. An End or Attack that arrived in the same frame is ordered before the timer.
**Example:** D = 3.0 s. A 60 Hz host completes at 3.000-3.017 s after t_start; a 30 Hz host at 3.000-3.033 s.

### F2. Effective brace window and debounce

The brace_window formula is defined as:

`W = min(t_endEff, t_start + D_cap) − t_start`, with `t_endEff = t_endRes + ReleaseDebounceSec` for **every** pawn. Both ends are `HoldTimer` items (`DebounceExpiry`, `CapEnd`), so the realised window is quantised: `W_real ∈ [W, W + dt)`.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_start | double (s) | ≥ 0 | calculated | queue resolution of `Prepare(Begin)`, which is also when the cooldown is stamped |
| t_endRes | double (s) | ≥ t_start | calculated | queue resolution of `Prepare(End)`. Human: release + one-way latency L_up + up to one frame. AI: the drain after it decides. |
| ReleaseDebounceSec | float (s) | 0.05-0.2 (hard) | data file (0.10) | pending-End window (R8) |
| D_cap | float (s) | 3.0-5.0 | data file (`DefenceDurationSec` 4.0) | maximum protected window |
| U | float (s) | 0-dt | calculated | replication delay of the `ActionPose` write. It is at most one server frame because every write forces a net update (R12; verify under Iris in 5.8). It is identical for all pawns. |
| t_interp | float (s) | 0-0.1 | calculated (Locomotion) | simulated-proxy interpolation, identical for all pawns |

**Output Range:** W_real ∈ [ReleaseDebounceSec, D_cap + dt), which is [0.10, 4.05) s by default.
- A tap still burns the full 60 s cooldown (Assassination).
- Observers see the brace from `t_start + L_down,o + U` to `t_start + W_real + L_down,o + U`.
- An attacker can be reversed by a brace they have not yet seen for up to `RTT_a/2 + L_down,a + U + t_interp`. That is about 0.25 s at 150 ms RTT on a 30 Hz host with t_interp 0.05. This is intended, because nothing is back-dated.
- The hard minimum of 0.05 s guarantees that a Brace write and its end land in different frames, so observers always see the pose.

**Example:** a human holds Q for 1.20 s at 100 ms RTT, so W = 1.20 + 0.10 = 1.30 s and W_real ∈ [1.30, 1.30 + dt). An AI that holds for 1.20 s gets the same. A human with a 30 ms key flicker at 0.6 s gets one continuous window, because the re-Begin resolved inside the pending End.

### F3. Token bucket (rate limit)

The token_bucket formula is defined as:

`tokens(t) = min(B, tokens(t_last) + R · (t − t_last))`. A command is admitted if `tokens ≥ 1` (the boundary value 1.0 is admitted), and then `tokens −= 1`. Initial tokens = B.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| B | int | 1-10 per pawn; 10-60 transport | data file (`Bucket*Size`, `TransportBucketSize`) | burst size per pawn per bucket, or per connection for transport |
| R | float (/s) | 0.5-10 per pawn; 5-40 transport | data file (`Bucket*PerSec`, `TransportBucketPerSec`) | refill rate |
| t, t_last | double (s) | ≥ 0 | calculated | server time now / at the last refill |

**Defaults:**
- **Per pawn** (consumed only on execution): Interact B3 R2; Prepare B2 R1; Attack B2 R1; HideEnter/HideExit B4 R2, as one shared bucket. Four buckets in total, with Σ B = 11 and Σ R = 6/s.
- **Per connection** (ingress, Begin and instant commands only, never a matched End): B30 R20. That is ≥ 2 × the per-pawn sums, so ingress never drops a press the per-pawn buckets would admit.
- **Abuse log**: written at ≥ `AbuseLogDropCount` (20) ingress drops within `AbuseLogWindowSec` (10 s). It is server-only, with no kick in MVP. An honest client cannot reach it, because it never sends Prepare during a known cooldown (R8) and its press rate stays below 20/s.

**Output Range:** tokens ∈ [0, B]; the sustained admit rate is ≤ R per second. Worst-case server load is 50 pawns × Σ R 6/s = 300 admitted commands/s sustained, plus a burst of 11 per pawn. That is bounded.
**Example:** a stub predicate returns `Allow`. Admissible Interact Begins arrive every 0.125 s for 2 s (16 Begins, each preceded by an End that has already executed). They are admitted at t = 0, 0.125, 0.25, 0.5, 1.0 and 1.5 s: 6 in total. At t = 0.5 the bucket holds exactly 1.0, which is admitted. A Q pressed during a cooldown is rejected at 2d with no token consumed, so the first press after the cooldown ends is always admitted.

### F4. Owner ring prediction and confirmation timeout (client, presentation only)

The ring_prediction formula is defined as:

`ring_local = clamp((t_local − t_press) / D, 0, 0.98)`.
- Before confirmation: `ring = ring_local`.
- After confirmation: `ring = max(ring_local, clamp((ServerNowEst − ServerStartTime) / D, 0, 0.98))`. The ring keeps advancing from the local press, and the server estimate can only correct it forward.
- `ring = 1.0` only on `FHoldStatus.Outcome = Completed`.
- The ring hides if still unconfirmed after `T_ack = 2·RTT_est + dt_max + AckSlackSec`, which covers one reliable resend.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_local, t_press | double (s) | ≥ 0 | calculated | client time now / at the press |
| ServerNowEst | double (s) | ≥ 0 | calculated | client's estimate of server time (re-synced if drift > 0.25 s) |
| ServerStartTime | double (s) | ≥ 0 | calculated | from `FHoldStatus` |
| dt_max | float (s) | 0.0083-0.05 | calculated | longest recent server frame |
| AckSlackSec | float (s) | 0.05-0.5 | data file (0.15) | slack before hiding an unconfirmed ring |

**Output Range:** ring ∈ [0, 0.98] until Completed. It is monotonic, so it never moves backward and never stalls at confirmation. The ring sits at 0.98 for about 0.02·D + RTT + up to 2·dt (0.21-0.28 s at D = 3 s, RTT 150 ms, 30 Hz). A late `FHoldStatus` carrying the latest `ClientCommandSeq` while the key is still held moves the ring `Hidden → Confirmed`. Prepare's brace indicator does not use the predicted branch (UI Requirements 2).
**Example:** RTT 150 ms on a 30 Hz host gives T_ack = 0.30 + 0.033 + 0.15 = 0.483 s. An Interact pressed at t = 0 shows 0.16 at 0.483 s. If confirmation has not arrived by then, the ring hides.

### F5. Input latency budget

The latency_budget formulas are defined as:

`L_look = t_frame + t_display` · `L_confirm = RTT + dt_server + dt_client` · `L_observer = RTT_a/2 + RTT_o/2 + 2·dt_server + t_interp + U`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_frame, dt_client | float (s) | 0.0167 at 60 fps | calculated | client frame |
| t_display | float (s) | ~0.016 | constant (hardware) | display latency |
| RTT, RTT_a, RTT_o | float (s) | 0-0.15 supported | calculated | owner / actor / observer round trip |
| dt_server | float (s) | 0.0167-0.033 | calculated | server frame |
| t_interp | float (s) | 0-0.1 | calculated (Locomotion smoothing) | simulated-proxy interpolation, identical for all pawns |
| U | float (s) | 0-dt_server | calculated (F2) | replication delay under forced net update |

**Output Range:**
- L_look ≤ ~33 ms.
- L_confirm is 93-110 ms at 60 ms RTT, and 183-200 ms at 150 ms RTT (60 Hz to 30 Hz host).
- L_observer is 0.18-0.35 s at 150 ms RTT.
- Above HUD's `HudLatencyWarnRttSec` (0.20 s RTT; IIF's duplicate `LatencyWarnRttSec` retired by user decision 2026-10-25, TDC CR-15.3) an owner-only warning is shown.

**Example:** RTT_a = RTT_o = 150 ms, 30 Hz host, t_interp 0.05, U 0.033. L_observer = 0.075 + 0.075 + 0.067 + 0.05 + 0.033 = 0.30 s. L_confirm = 0.15 + 0.033 + 0.017 = 0.20 s.

### F6. Interact reach and cancel hysteresis

The interact_reach formula is defined as:

- **Accept `Begin`** iff `d_xy ≤ InteractRangeCm + CommandRangeSlackCm` **and** `|yaw_ctrl − yaw_to_point| ≤ InteractFacingHalfAngleDeg`. A failure of either replies `OutOfReach`.
- **Cancel** with `OutOfRange` iff `d_xy > InteractRangeCm + CommandRangeSlackCm + InteractCancelHysteresisCm`.
- **Cancel** with `Moved` iff `|pos_xy − pos_xy,Begin|` exceeds `InteractMoveCancelDistCm`, counted only over ticks with non-zero Move intent after `t_start + InteractMoveGraceSec`.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| d_xy | float (cm) | ≥ 0 | calculated | horizontal distance from capsule centre to the interact point |
| InteractRangeCm | float (cm) | 80-250 | data file (150) | nominal reach |
| CommandRangeSlackCm | float (cm) | 20-80 | data file (40) | position-skew tolerance (ServerMove vs reliable RPC ordering), identical for all target kinds |
| InteractCancelHysteresisCm | float (cm) | 30-150 | data file (50) | extra distance before a range cancel; the cancel range is always > the accept range |
| InteractFacingHalfAngleDeg | float (deg) | 30-90 | data file (60) | facing tolerance, checked at Begin only |
| pos_xy − pos_xy,Begin | float (cm) | ≥ 0 | calculated | net horizontal displacement since Begin, accumulated only while Move intent ≠ 0 (human or AI, R17) and after the grace period; external pushes on zero-intent ticks never count |
| InteractMoveCancelDistCm | float (cm) | 5-40 | data file (15) | commanded displacement that counts as walking away |
| InteractMoveGraceSec | float (s) | 0-0.5 | data file (0.25) | settling time after Begin during which displacement is not accumulated |

**Output Range:** with the defaults, Begins are accepted at ≤ 190 cm and holds cancel beyond 240 cm. The 50 cm hysteresis absorbs crowd capsule pushes. Commanded displacement > 15 cm cancels, which is about 0.1 s of walking at about 150 cm/s. The rule is identical for humans and AI. Because the cancel range is derived, it is above the accept range at every value in the hard ranges.
**Example:**
1. A pawn at 185 cm, 40° off facing, begins.
2. A passer-by pushes it 25 cm, to 200 cm. The hold continues, because there was no Move intent on those ticks.
3. 0.5 s after Begin, the owner walks 20 cm. The hold cancels with reason `Moved` and progress resets to 0.
4. A Begin at 195 cm, or at 70° off facing, is rejected `OutOfReach`.

*No other formula is owned here.* The defence cooldown test (`now − LastActivation ≥ DefenceCooldownSec`), `AssassinationRangeCm` and `AttackAttemptLockoutSec` belong to Assassination; `ObjectiveInteractSec` belongs to Objectives.

## Edge Cases

*systems-designer consulted. Design review 2026-10-24: reply behaviour is aligned with R6's two tiers, and ordering is aligned with hold timers as queue items (R8).*

**Ordering and simultaneity**
- **If a defender's `Prepare(Begin)` and an `Attack` on that defender resolve in the same drain**: the lower Round key executes first.
  - If Prepare is first, Assassination sees a prepared target, and the strike is reversed.
  - If Attack is first, the target is struck before bracing. The Prepare is then voided at resolution, because its pawn was removed. The defender's connection gets exactly one `NotNow` (R6 resolution tier).
- **If A strikes B and B strikes A in the same drain**: the first in key order resolves. The second is voided at resolution, because the attacker was removed, and its sink gets one `NotNow`. It never becomes a strike, a strike count or a whiff.
- **If `Interact(Begin)` arrives before an earlier unreliable `ServerMove`**: they travel on different channels, so their order is not guaranteed. Begin is evaluated at its queue slot against the server's current position, with `CommandRangeSlackCm` as tolerance. Displacement-based cancel (F6) then catches real walking.
- **If `Prepare(Begin)` and `Prepare(End)` arrive in the same frame** (a tap): both sit in the same mailbox in FIFO order. The Begin executes first, the End becomes pending, and `HoldTimer(DebounceExpiry)` ends the brace at about 0.10 s (F2). A tap is never a full-cap brace.
- **If an earlier event in the drain invalidates a later command** (state transition, kill, phase change, hold timer): stage 2 runs at the command's own slot. It is therefore rejected with the R6 code to its `ReplySink`, if the sink is non-null. An `Attack` whose `PawnRef` died earlier in the drain resolves as a strike at its `WorldPoint`, never as a rejection.
- **If two pawns interact with the same target**: each runs an independent timer. Whether progress stacks, blocks or races is Objectives' rule; the IIF imposes none (Objectives OC-8: no stacking, up to `FixtureInteractSideCount` (4) tenders per fixture, a further Begin is `Reject(NotNow)`).
- **If both teams' last Interacts on the shared final site come due in the same frame**: both `HoldTimer(Complete)` items are stamped in `(DueTime, Begin key)` order. The first resolves, emits `OnActionCompleted`, and Objectives' completion follow-on calls Round's win in that slot. The latch then ends the phase. The second timer resolves after the latch and is voided (Round's `Rejected_Latched`). Its hold is frozen at the latch (it can no longer complete) and ends through the normal release blend, with reason `Illegal`, at Round's **round-end step** (Round R9: the latch step itself here, an objective win; when a strike decides the round, the deciding strike's death write, so no ring is cancelled before the blow lands; cross-review S-B1 2026-10-24). No draw is possible.
- **If an End resolves before `HoldTimer(Complete)` for the same hold**: the End is pending. If its debounce expiry has a lower key than Complete, the hold cancels (`Released`) and progress resets. Otherwise Complete resolves first, with the End still pending, and the hold completes (the 0.10 s grace in R8). Key order alone decides.

**Latency, loss and time**
- **If a reliable Begin or End is lost to 1 % packet loss**: it is resent, never dropped, and arrives about one RTT late. A late Prepare End extends the brace by at most that delay, and never past `DefenceDurationSec` (the CapEnd timer). A late Interact End that resolves after `HoldTimer(Complete)` finds no active action and is a no-op.
- **If the server completes a hold within ±RTT/2 of the owner's release**: the server's key order decides (above). The ring reconciles to the `FHoldStatus.Outcome` the server sends. A completed Interact is never undone.
- **If the client's server-time estimate drifts by more than 0.25 s**: the ring re-syncs, and the ring stays monotonic (F4). Only presentation is affected; the server timer is authoritative.
- **If RTT exceeds `HudLatencyWarnRttSec` (0.20 s; HUD-owned)**: everything works, but an owner-only latency warning shows. No gameplay compensation is applied and nothing is back-dated.

**Connection and possession**
- **If the owner disconnects mid-hold**: the server treats it as an End resolved as `Released` (after the debounce). `ActionPose` → None. The pawn's further fate (it keeps standing like an NPC, or is removed) is Round's and Session's rule. The IIF only guarantees that the release blend is the same as for any other release.
- **If the listen-server host disconnects**: UE has no host migration, so the session ends for everyone. Clients flush input and hide all rings and plates, and nothing from the IIF persists. Round and Session own the outcome; this is a known prototype limitation.
- **If the pawn is unpossessed or re-possessed mid-hold** (assassinated → informant respawn): the Active → Informant transition cancels the action with `Illegal` in the same resolution. A later End finds no action and is a no-op. `ClientCommandSeq` carries `PossessionEpoch`, so a late reliable Begin from the old pawn is ignored at ingress and never lands on the new pawn. The client also sends End for every outstanding Begin on possession change (R4).
- **If the Round phase leaves `Active` mid-hold**: every active action cancels with `Illegal` at the phase-change event's queue position; for a round end that position is Round's **round-end step** (R9), and between the latch and that step every hold is frozen (no completion, no cancel, the owner ring keeps running). Items still pending when Round's latch closes the queue, and items submitted before the round-end step, return `Rejected_Latched` (Round R9/R15) and the issuing owner gets one ordinary `NotNow` reply, held by Round and sent at the round-end step (R6; Round change request 6; cross-review S-B1 2026-10-24). `Illegal` is a server event reason only: `FHoldStatus` carries no reason (R12). Commands that arrive while the phase is not `Active` are dropped by Round (`WrongPhase`) before they reach the IIF resolver and get no IIF reply; the owner learns the phase from Round's public state. Both apply to every source alike.
- **If a teammate completes the objective while the owner is mid-hold on it**: the hold is **not** cancelled. It runs to its own natural end and completes with no effect (Objectives OC-8: cancelling every tender at the court on the same frame would be a correlation tell). `FHoldStatus.Outcome = Completed` is sent at its own end as for any hold. *(Reconciliation 2026-10-24: replaces the earlier `TargetInvalid` cancel.)*

**Input-layer**
- **If a player releases and re-presses Prepare within 0.10 s**: one continuous brace, with no new cooldown and no cap restart (R8 debounce). **If they re-press after 0.10 s**: the End has executed and the cooldown is running. The client does not send the Begin (R8), no ring appears, and the brace indicator shows Cooling. If a modified client sends it anyway, the server answers `Cooldown` with no other effect.
- **If toggle mode is on and the player double-taps Prepare within 0.10 s**: the server debounce merges the taps into one continuous brace. Two taps never burn the cooldown and then immediately end the brace.
- **If a key is held when the mapping context swaps, a menu opens or the window loses focus**: the client sends End for every outstanding Begin immediately. The held key then needs a release and a fresh press (`EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`, ADR-0011 E2; spike V1). A Common UI switch to menu input sends every End synchronously (ADR-0011).
- **If the player presses Tend while still releasing W**: commanded displacement is not counted for `InteractMoveGraceSec` (0.25 s), so the deceleration does not cancel the hold (F6).
- **If an informant presses a former Active key**: nothing is sent. The owner-only plate shows (rate-limited), and nothing changes on the pawn.
- **If a legitimate player hits a per-pawn bucket**: the command is rejected `NotNow` at resolution. Tokens are consumed only on execution, so this needs more than R executed commands per second, which no intended play pattern reaches.
- **If a player mashes keys**: the per-connection transport bucket (B30 R20) is at least twice the per-pawn sums. Ingress therefore never drops a press the per-pawn buckets would admit, a later Attack is never silently lost, and the abuse log is not tripped.
- **If an AI bug floods commands**: the same buckets apply. Excess is rejected server-side and logged per pawn, and the NPC simply idles. AI receives no exemption.

**Attack and target payloads (anti-oracle)**
- **If `PawnRef` names a concealed pawn**: it fails the `CanViewerKnow` gate (R10), so it is ignored. The outcome equals what the `WorldPoint` alone would produce.
- **If `PawnRef` names the attacker itself**: it is stripped at ingress and counted for the abuse log. The Attack proceeds as a `TargetSpot`.
- **If `PawnRef` names a nonexistent, stale or unreplicated pawn, or one farther than `PawnRefAgreementCm` from the `WorldPoint`**: it is ignored, and the Attack falls back to the `WorldPoint`.
- **If `PawnRef` names an informant**: it is a legal target, because informants are huntable. Assassination resolves it as `VictimKind=Informant`.
- **If `PawnRef` names a teammate**: the honest client never sends it, because a marked ally is excluded locally. A modified client's teammate strike is resolved by Assassination's teammate rule (open question). The IIF does not reject it, so a rejection code can never confirm membership.
- **If a modified client sends `Interact(Begin)` with the id of a site hidden from its team** (for example the final site before its preliminaries are done), or an id that does not exist: both are dropped silently at ingress through the same path, with the same timing. Probing can never reveal whether a hidden site exists or where it is.

**Pillar-1 leak cases**
- **If rejection timing differed by failure point**: it does not. Resolution-tier replies are enqueued only at the end of the drain's queue resolution (R6), and ingress drops get no reply at all.
- **If an `ActionPose` write cadence differed between NPC and player pawns**: it must not. Writes happen only at server transitions, with the same net settings for every pawn and a forced update on every write. Dormancy and owner-only relevancy are banned (R12). The parity tests cover this (AC 46, 48, 56).
- **If owner-only data were placed on the pawn**: it would differ between NPC and player pawns. It is forbidden, so all owner-only data lives on the PlayerController (R12).
- **If an AI's brace durations were systematically exact (e.g. always 4.00 s)**: that is a *behaviour* tell, not a channel leak, and Crowd's Baseline and Tell Matrix owns it. The IIF guarantees only that the channel and the minimum (0.10 s) are identical.
- **If a human's camera turn looked different from an NPC's head turn**: that is a channel leak, which R3a forbids. AC 54 verifies it.
- **If the listen-server host's process holds ServerOnly data** (bindings, concealed positions): it does so by construction. This is accepted for trusted prototype playtests only. The host's input still goes through ingress (R2). Its local presentation must still go through the same Visibility Policy path, and the IIF allows no `NM_ListenServer` special-casing.

## Dependencies

Source: `design/gdd/systems-index.md`, Enumeration row 3 and Dependency Map. Every interface named in a GDD not yet written (or still a skeleton) is **provisional**.

### Upstream (the IIF depends on)

| System (index #) | Data interface | Hard/Soft | Status |
|---|---|---|---|
| Tuning Data & Config (1) | IIF-owned keys (Tuning Knobs) registered per TDC's Knob Registry Convention; consumed keys `ObjectiveInteractSec`, `DefenceDurationSec`, `AssassinationRangeCm`; read via `GetServerTuning()` (server) and `GetClientTuning()` (owner prediction, client-visible keys only) | Hard (index) | GDD in progress; TDC already lists the IIF as a hard dependent |
| Participant State & Roster (2) | `GetLegalityClass(Pawn)` → {Active, Informant, RoundEnded}, Active for unbound pawns; owner-audience own-state change for the context swap | Hard (listed in index row 3, verified 2026-10-24, cross-review C-W11) | Designed (`GetLegalityClass` defined, Roster R7) |
| Round & Match Framework (6) | `RoundQueue.Submit(Command, SourceMailbox)`, Round key (tick, `Seq` by mailbox rotation), resolver callback; `HoldTimer` server items; round phase and latch drops | Hard for discrete commands (listed in index row 3, verified 2026-10-24, cross-review C-W11). The index orders Round after the IIF, so the IIF defines the queue-item contract it needs, and Round must accept it | Designed (Round R11-R12) |
| Shared Character & Locomotion (9) | server `IsRunning`, `IsInTraversal`; execution of Move/Look/Traverse (planted brace, R7); `FActionState` animation; the R3a Look-parity mechanism and the R12 relevancy bans (SCL R8, R13) | Hard at runtime, but it **depends on the IIF** in the index; the IIF reads only two booleans and does not import Locomotion's design (contained soft cycle, same pattern as Round/Level Socket) | provisional |
| Information Visibility Policy (7) | audiences `Owner`/`All` for R12 | Soft (vocabulary) | provisional |

### Downstream (these depend on the IIF)

| Dependent (index #) | What it takes | Hard/Soft |
|---|---|---|
| Shared Character & Locomotion (9) | Move(Direction, Gait), Look(Rotation), Traverse entry points; legality gate for run/traversal; `ActionPose` write | Hard |
| Camera System (10) | toggle and shoulder-swap input bindings and context membership (R13, R14); Camera supplies `GetAimRay()` and the mask flag back to R10 (soft peer) | Hard |
| Crowd & NPC Behaviour (11) | the R1 entry points AI controllers call; identical rejections; red-herring Prepare/Interact/Hide possible | Hard |
| Concealment (Bushes) (12) | Hide(Enter/Exit) commands; `OnActionStarted(Pawn, Attack)`; `CanBegin` registration for Hide | Hard |
| Assassination & Defence Reversal (13) | Prepare window (start, end, cap), Attack command with target hint, events, `CanBegin` registration for Prepare/Attack, slot arbitration | Hard |
| Objectives Chain (15) | Interact hold (Begin/End, timer, cancel rules), `OnActionCompleted(Pawn, Interact)`, `CanBegin` registration | Hard |
| HUD (18) | owner-only `FHoldStatus`, rejection codes, local prompt legality, informant plate; presenter of UI 1-8 (anchors and layering per HUD) | Hard (display) |
| Teammate Markers (17) | none taken; supplies `IsMarkedAlly(Pawn)` for R10 and the Strike hint's ally exclusion (TM-R10) | Hard (honest-client rule; IIF consumes) |
| Settings & Accessibility (24, VS) | remap surface, hold/toggle translation (R16), debounce constants | Hard (index) |
| NPC-Kill Penalty & Informants (14) | informant legality row (R7) and `IMC_Informant` | Soft (listed in index row 14, verified 2026-10-24, cross-review C-W11; behaviour stated here) |
| Onboarding & Practice Mode (25, Alpha) | key defaults and verbs | Soft |
| Anti-Cheat & Info-Leak Security (27, Alpha) | ingress validation, buckets, abuse counters, R6 rejection policy | Soft |
| Telemetry & Analytics (26, Alpha) | server-only `CancelReason`, rejection counts | Soft |

**Bidirectionality**: Locomotion, Camera, Crowd, Concealment, Assassination, Objectives and Settings already list the IIF in the index. The change requests are: add Roster and Round as IIF upstream dependencies in the index; NPC-Kill Penalty, HUD, Anti-Cheat and Telemetry should note their soft dependence on the IIF.

## Tuning Knobs

Registered per the TDC Knob Registry Convention (CR-15, with the columns it requires). The owner is the IIF, every key is server-authoritative, and every key is part of `TuningHash`.

- **Audience** says whether a key is replicated to clients in the per-round snapshot (`All`, needed for owner prediction and prompts) or never leaves the server (`ServerOnly`).
- **Latch** follows TDC CR-11. `Action` keys are captured when the action executes, so a mid-action edit never retimes it. `Use` keys are read at the point of use.
- **Identity-tell review (CR-15.8)**: every key below applies identically to every pawn, whatever its controller. None times a crowd-versus-player event, so none is an identity tell.

Keys consumed here but owned elsewhere are listed after the table and not duplicated.

*Design review 2026-10-24 changes:*
- The table was rebuilt to the CR-15 columns.
- `InteractCancelRangeCm` was replaced by `InteractCancelHysteresisCm`, so the cancel range can never fall inside the accept range.
- The bucket, abuse-log and latency keys were renamed to CR-15 suffixes.
- `PrepareLatencyForgivenessSec` was retired, because no formula used it and R8 already forbids back-dating.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|---|---|---|
| `ReleaseDebounceSec` | float | IIF | 0.10 | 0.05-0.2 | 0.05-0.12 | ServerOnly | Action | s | Sets the pending-End window and the minimum brace (F2). Too low: key flicker cancels holds, and a toggle double-tap ends a brace. Too high: the minimum brace becomes a visible, timed tell and adds protection after release. The hard minimum is 0.05 so that a Brace pose always replicates before its end (F2). |
| `InteractRangeCm` | float | IIF | 150 | 80-250 | 120-180 | All (prompts, ring prediction) | Use | cm | Nominal Interact reach (F6) for every Interact target. This is the only name for this quantity: TDC's migration row `ObjectiveInteractRange` should map here (change request). Too low: players must hug fixtures, and the crowd gets in the way. Too high: players can tend from a distance, and the pose is less readable. Interacts with Level Socket site spacing. |
| `InteractCancelHysteresisCm` | float | IIF | 50 | 30-150 | 30-80 | ServerOnly | Use | cm | How far past the accept distance a hold survives. Cancel range = `InteractRangeCm + CommandRangeSlackCm + InteractCancelHysteresisCm` (240 by default, F6). Too low: capsule pushes cancel holds. Too high: a player can walk away and keep tending. |
| `InteractFacingHalfAngleDeg` | float | IIF | 60 | 30-90 | 45-75 | All (prompts) | Use | deg | Facing tolerance, checked at Begin only. Too low: alignment is fiddly. Too high: players can tend with their back to the site. |
| `InteractMoveCancelDistCm` | float | IIF | 15 | 5-40 | 10-25 | ServerOnly | Use | cm | Commanded displacement (F6) that cancels a hold. Too low: a nudge of the stick cancels. Too high: players can slide while tending, which weakens the tell. External pushes never count (R8). |
| `InteractMoveGraceSec` | float | IIF | 0.25 | 0-0.5 | 0.15-0.35 | ServerOnly | Action | s | For this long after an Interact executes, commanded displacement is not accumulated. A Move still in progress when the press lands (E pressed just as W is released) therefore does not cancel. Too low: pressing Tend on arrival cancels spuriously. Too high: a short slide is allowed after Begin. The same for every pawn. |
| `CommandRangeSlackCm` | float | IIF | 40 | 20-80 | 30-50 | ServerOnly | Use | cm | Position-skew tolerance for Interact reach and the `WorldPoint` clamp. Too low: high-ping players are wrongly rejected. Too high: effective reach grows, including for strike-range hints. Interacts with `AssassinationRangeCm`. |
| `PawnRefAgreementCm` | float | IIF | 75 | 25-150 | 50-100 | ServerOnly | Use | cm | Maximum distance between an Attack's `PawnRef` pawn and its `WorldPoint` for the hint to be honoured (R10). Too low: honest hints at moving pawns are discarded and fall back to the spot, which is harmless. Too high: a forged `PawnRef` can reach a pawn the ray did not hit. |
| `AckSlackSec` | float | IIF | 0.15 | 0.05-0.5 | 0.1-0.25 | All | Use | s | Slack before an unconfirmed ring hides (F4). Too low: rings flicker on jitter. Too high: a rejected ring lingers. Presentation only. |
| `BucketInteractSize` | int | IIF | 3 | 1-10 | 2-4 | ServerOnly | Use | count | Per-pawn Interact burst (F3). Too low: legitimate retries are rejected. Too high: spam reaches feature code. |
| `BucketInteractPerSec` | float | IIF | 2.0 | 0.5-10 | 1-3 | ServerOnly | Use | 1/s | Per-pawn Interact refill rate (F3). |
| `BucketPrepareSize` | int | IIF | 2 | 1-10 | 1-3 | ServerOnly | Use | count | Per-pawn Prepare burst. The cooldown is the real limit; this is a backstop. |
| `BucketPreparePerSec` | float | IIF | 1.0 | 0.5-10 | 0.5-2 | ServerOnly | Use | 1/s | Per-pawn Prepare refill rate. |
| `BucketAttackSize` | int | IIF | 2 | 1-10 | 1-3 | ServerOnly | Use | count | Per-pawn Attack burst. The lockout (`AttackAttemptLockoutSec`) is the real limit; this is a backstop against probe spam. |
| `BucketAttackPerSec` | float | IIF | 1.0 | 0.5-10 | 0.5-2 | ServerOnly | Use | 1/s | Per-pawn Attack refill rate. |
| `BucketHideSize` | int | IIF | 4 | 1-10 | 2-6 | ServerOnly | Use | count | Per-pawn burst for HideEnter and HideExit, which share one bucket. Too low: going in and out of bushes feels sticky. Too high: bush flicker spam. Interacts with Concealment's transition time. |
| `BucketHidePerSec` | float | IIF | 2.0 | 0.5-10 | 1-3 | ServerOnly | Use | 1/s | Per-pawn Hide refill rate. |
| `TransportBucketSize` | int | IIF | 30 | 10-60 | 24-40 | ServerOnly | Use | count | Per-connection ingress burst (Begin and instant commands only). Must stay ≥ 2 × the sum of the per-pawn bucket sizes (22 by default), so ingress never drops a press the per-pawn buckets would admit. This cross-field rule is a change request to TDC. Too low: legitimate presses are dropped, including an Attack after cooldown mashing. Too high: risk of flooding the reliable buffer. |
| `TransportBucketPerSec` | float | IIF | 20 | 5-40 | 12-30 | ServerOnly | Use | 1/s | Per-connection ingress refill rate. Must stay ≥ 2 × the sum of the per-pawn refill rates (12 by default). |
| `UnmatchedEndPerSec` | float | IIF | 30 | 10-60 | 20-40 | ServerOnly | Use | 1/s | End-flood guard: Ends with no outstanding Begin on that connection are dropped at ingress above this rate. An End that matches an outstanding Begin is never dropped. |
| `AbuseLogDropCount` | int | IIF | 20 | 5-100 | 10-40 | ServerOnly | Use | count | Number of ingress drops within `AbuseLogWindowSec` that writes a server-only abuse-log entry. Legitimate client rates can never reach it (F3). Too low: log noise. Too high: abuse goes unseen. Anti-Cheat (Alpha) adds a kick policy. |
| `AbuseLogWindowSec` | float | IIF | 10 | 5-60 | 10-20 | ServerOnly | Use | s | Window for `AbuseLogDropCount`. |
| ~~`LatencyWarnRttSec`~~ | — | — | — | — | — | — | — | — | **RETIRED 2026-10-25 (user decision, cross-review C-W14, TDC CR-15.3):** one quantity, one key; F5 and UI-7 read HUD's `HudLatencyWarnRttSec` (default 0.20 s, HUD Tuning Knobs). |
| `CameraToggleLockoutSec` | float | IIF | 0.30 | 0-1 | 0.2-0.5 | All | Use | s | Local camera-toggle lockout (R14). Too low: the camera flickers. Too high: the toggle feels unresponsive. Interacts with Camera's blend time. |
| `InformantPlateCooldownSec` | float | IIF | 3.0 | 1-10 | 2-5 | All | Use | s | Rate limit for the informant plate (UI 6). Too low: it nags. Too high: a press after a long gap may go unexplained. |
| `RejectPlateCooldownSec` | float | IIF | 3.0 | 1-10 | 2-5 | All | Use | s | Rate limit for the rejection plate, per code (UI 5). |

**Consumed, owned elsewhere (do not duplicate):** `ObjectiveInteractSec` (Objectives, 3.0); `DefenceDurationSec` (Assassination, 4.0, range 3-5); `DefenceCooldownSec` (Assassination, 60); `AssassinationRangeCm` (Assassination, 200); `AttackAttemptLockoutSec` (Assassination, 1.75); `CounterDurationSec` (Assassination); `BushReenterCooldownSec` (Concealment, 6); `TendCueDelaySec` (Objectives, 1.0, client-visible, owner ring prediction); `FixtureInteractSideCount` (Objectives/LSC, 4); `PawnNetUpdateHz` (Locomotion, 15, range 12-17, identical for all pawns; SCL F9).

**Client-local, not tuning** (TDC CR-14): key bindings, the hold/toggle preference, Look sensitivity.

## Visual/Audio Requirements

*art-director consulted (art bible 1.3, 4.4, 4.6, 5.2, 5.5, 7.1-7.8).* The IIF has no visuals of its own. It defines **which presentation channels its commands may drive**, and the rule is strict:

**Observers (every other client) see only `ActionPose`-driven animation** (art bible 5.5): Brace (about 60 % stature, feet planted wide, forearms raised); InteractPose (two-handed or kneeling, facing the fixture, held over 1.5 s); Strike (one clip identical up to the contact frame, `TargetSpot` whiffs included); BushEnter / BushExit (step and rustle, body swap timed to occlusion, art bible 5.2.1; Concealment owns the clips); Counter (Assassination owns the clip).

- Every exit from a pose is **one blend for every reason** (completion, release, cancel, supersede, illegal), so `CancelReason` never shows on the body.
- A rejected or rate-limited command, or a mashed key, produces **no pose change, flinch, sound or VFX**.
- NPC and player pawns share the same AnimBP, notifies (and so foley), blend times and significance LOD. Nothing in the presentation path branches on controller type or `IsLocallyControlled`. Any difference found is fixed by parity, never mitigated (art bible Principle 3).
- No VFX, ring, outline or world-space sound accompanies any command for observers.

**Owner only**: the interact ring, brace indicator, prompts, Strike hint and plates (UI Requirements) use **Bone, Bone Dim, Obsidian Ink and Obsidian Raised**, with **Marigold** for the interaction ring fill at objective sites only. **Turquoise is never used** here, because it means allies only (art bible 4.4). **Cochineal is never used**, because a rejection or cancel is not finality (4.6). UI motion stays slow, level and quiet with no pulses (7.6). The interaction ring's real-time linear sweep needs a one-line exception in 7.6 (change request).

**Audio**: owner-only cues (rejection tone, end-of-protection cue with a text twin) play as 2D sounds on the UI bus. They are never attached to the pawn, never spatialised and never replicated. The audio-director designs them in Diegetic Audio / Event Feedback & Cues (VS). The MVP may ship them silent.

**Art bible change requests** (from the art-director): 7.3 row 4 (charge-then-armed → protected window from acceptance, four stepped segments, square = "protected now"); 7.6 Prepare ring row and "only linear motion" line; 7.5 (i); 7.x C7 and 7.8 audio handoff ("armed" means acceptance; add a protection-ended cue); 5.5 Brace row ("held for the protected window, up to `DefenceDurationSec`"); reconcile 7.1 vs 5.2 on whether allies trigger the Strike hint (this GDD follows 7.1: allies excluded, because the client never strikes a marked ally).

> 📌 **Asset Spec**: Visual/Audio requirements are defined. After the art bible is approved, run `/asset-spec system:input-interaction-framework` (UI plates and indicator only; poses belong to Locomotion and Assassination).

## Game Feel

*ux-designer and gameplay-programmer consulted. Numbers are targets to profile, provisional pending the net-emulation spike (100 ms RTT, 1 % loss).*

| Feel target | Target | Why |
|---|---|---|
| Look: input → camera | applied locally on the same frame; ≤ 1 frame + display (~33 ms at 60 fps) | aiming the eyes must never feel delayed; Look is not server-gated |
| Move: input → first visible body response | ≤ 100 ms at 0 ms RTT, locally predicted (Locomotion budgets motion-matching start latency) | controls must never be the cost (Player Fantasy) |
| Move release → stop | Locomotion's NPC-baseline deceleration (~0.5 s), never a snap | release must not create a sudden-stop tell (R3) |
| Hold feedback start | next frame after press (≤ 17 ms): the interact ring is predicted; the brace indicator shows a "pending" emphasis, and Protected appears only on server confirmation | the owner must know their press registered, without false safety |
| Owner's own brace/interact pose | starts on server confirmation (no predicted body pose) | the body observers see is server-driven; a predicted owner pose that snaps back would be a lie |
| Owner's own Hide clip (`BushEnter`/`BushExit`) | predicted locally, ≤ 3 frames (50 ms) at 60 fps, reconciled with the server (Concealment change request 1, CON-R4a; mechanism -> IIF ADR, spike S-CON-1) | the only predicted owner pose: the step into the leaves must feel immediate; observers still see only the server-driven pose |
| Server confirmation visible to owner | RTT + ≤ 1 server frame + ≤ 1 client frame (~93-110 ms at 60 ms RTT) | F5 |
| Rejection → ring hidden | instantly on `ClientCommandRejected` (0 ms fade) | no ambiguous half-state |
| Hold completion as seen by the owner | server overshoot < dt (F1); the owner sees Completed about RTT + dt after D (~6 % of a 3 s hold at 150 ms RTT); the ring waits at 0.98 meanwhile | acceptable: not a reflex game |
| Supported RTT | ≤ 150 ms playable; owner-only latency warning above 200 ms | F5 |

**Weight and snap**: Strike is one press with no wind-up input. Its weight comes from Assassination's animation and the commitment (lockout), never from input buffering. Brace and Tend are held. Their weight is the visible stance, and the button feel is plain and immediate. There is no input buffering: a press rejected because the pawn is still running is simply rejected, and the player presses again. A buffer was rejected because it would be a human-only code path.

**What it must never feel like**: a parry (no closing ring, no timing reward, no flash on "perfect" brace), a QTE, or sluggish controls used as balance.

## UI Requirements

Every element below is **owner-only**, screen-space, and derived from data the owner already has. None is replicated to or visible on any other client. The HUD GDD owns layout; UX specs go in `design/ux/hud.md`.

1. **Interact ring**: 48 px ring at the target (art bible 7.3 row 7), fill-only, linear sweep. It is **predicted** from the local press (F4) and re-based on `FHoldStatus`. It never shows completion before the server's `OnActionCompleted` reaches the owner, and it clamps at 0.98. **Colour**: the fill is neutral Bone at every court for the first `TendCueDelaySec` (1.0 s) after Begin; only then does it turn Marigold at an own live site (from Objectives' Team-audience live-site set) and stay Bone at any other court (Objectives OC-6; presentation only). Clamped to the screen edge at constant size if the target leaves view. Empties instantly in one step on any cancel or rejection.
2. **Brace indicator** (art bible 7.3 row 4, 56 px, bottom-left, Bone, no signal hue): **Ready** = empty outline. On press it shows only a local "pending" outline emphasis. **Protected** starts *only on server confirmation*, because predicting protection the server never granted would give false safety. In Protected, the inner stepped square appears at once and the outer ring fills in 4 stepped segments of `DefenceDurationSec`/4 each. It never shrinks, closes or flashes, so there is no parry-window reading. **End** (release, cap or cancel, identical; on a reversal it lands at `FHoldStatus.ServerEndTime = ContactServerTime`, never on receipt, ASN-R23) = square removed with a 250 ms stepped exit, then **Cooling** = linear Bone Dim sweep plus seconds numeral ("Brace in 0:42") counting the 60 s from acceptance. There is no "Ready!" sound; a ready cue is private and must stay unobservable. (This replaces art bible 7.3's charge-then-armed reading; change request.)
3. **Context prompts**: glyph of the *currently bound* key plus a verb (Tend, Slip in, Slip out). Computed locally from public Level Socket Contract / Manifest geometry only. **"Tend" shows identically at every court fixture in reach** for an Active owner, live or not, own, rival or hidden final alike, so it confirms nothing (Objectives OC-6; reconciliation 2026-10-24, replacing "only at the owner's live sites"); the live-site cue exists only in the ring colour after `TendCueDelaySec` (UI 1). Bush prompts use Manifest geometry. Prompts are never a per-press server query and never shown to informants. **A prompt is a hint, never a gate**: the command is valid from the moment of being in reach, whether or not the prompt's fade has finished.
4. **Strike hint, no reticle** (art bible 7.1 keeps screen centre empty; 7.5 bans crosshairs): a screen-fixed lower-centre hint, "[bound glyph] Strike" (Bone on an opaque plate). It appears whenever Camera's aim ray (`GetAimRay()`) hits *any* rendered (mask α > 0, R10) non-ally pawn in reach, NPC or player alike, and nothing appears for a spot. Allies are excluded, because the owner already knows them from their markers. Concealed pawns are not rendered, so the hint cannot act as x-ray. No highlight, outline, colour or sound differs by target class. Fallback if playtests show aiming fails: a static 4 px Bone stepped square at centre that never changes state (art bible 7.1 amendment, UX decision).
5. **Rejection feedback**: on `ClientCommandRejected`, the ring empties. An opaque Obsidian plate with 20 px Bone text, "Not now", "Busy" or "Too far", shows beside the brace indicator or in place of the prompt verb (exact anchors per HUD): 400 ms in, 1.5 s hold, 600 ms out, at most once per 3 s per code. A Prepare `Cooldown` gets no plate, because the Cooling numeral already says it (a `HideEnter` `Cooldown` is presented per HUD). Plates for a pawn struck in the same drain are presented no earlier than its contact time (ASN-R23). A quiet, level 2D owner-only tone plays on the UI bus. It is never played on the pawn and never spatialised (cue design: audio-director).
6. **Informant plate**: "Informants can only observe", 20 px Bone on an Obsidian plate at lower-centre (70 % height; exact anchor per HUD), 400 ms in, 2 s hold, 600 ms out, at most once per 3 s, no icon and no sound. It is suppressed until the removal card has collapsed.
7. **Latency warning**: a small owner-only icon above 200 ms RTT.
8. **Rebinding screen**: Settings & Accessibility (VS). MVP uses defaults (R15), with glyphs that follow any rebinding.

> **📌 UX Flag — Input & Interaction Framework**: This system has UI requirements. In Pre-Production, run `/ux-design` for the HUD (ring, brace indicator, prompts, reticle, plates) **before** writing epics. Stories should cite `design/ux/hud.md`, not this GDD.

## Cross-References

| This GDD references | Target doc | Element | Status |
|---|---|---|---|
| Knob registry, `GetServerTuning`/`GetClientTuning`, audiences | design/gdd/tuning-data-config.md | Dependencies, Core Rules | exists (in design) |
| `GetLegalityClass(Pawn)`, owner-audience state change | design/gdd/participant-state-roster.md | API | Designed (Roster R7, `GetLegalityClass`; verified 2026-10-24, cross-review C-W11) |
| `RoundQueue.Submit`, total-order key, phase | design/gdd/round-match-framework.md | queue contract | Designed (Round R11-R12; verified 2026-10-24, cross-review C-W11) |
| `IsRunning`, `IsInTraversal`, Move/Look/Traverse execution, planted brace, `FActionState` playback, `PawnNetUpdateHz`, R3a mechanism and relevancy bans | design/gdd/shared-character-locomotion.md | R5.2, R7.2, R8, R13, F9 | designed (in review); reconciled |
| Toggle presentation, shoulder swap, `GetAimRay()`, eye mask ("rendered") | design/gdd/camera-system.md | CS-R5, CS-R8, CS-R10 | designed (in review); reconciled |
| Prepare meaning, cooldown, `AttackAttemptLockoutSec`, victim resolution, `FActionState`, `Countering`, `Reversed`, presentation gating | design/gdd/assassination-defence-reversal.md | ASN-R3, R9, R14, R18, R19, R21, R23 | designed (in review); reconciled |
| Hide transition length, `Exit(Side)`, `BushEnter`/`BushExit`, re-entry `Cooldown`, concealment from Attack | design/gdd/concealment-bushes.md | CON-R4a, R8, R12 | designed (in review); reconciled |
| Interact duration, every-court ingress, full-fixture refusal, `TendCueDelaySec`, `AllowNoEffect` decoys, no stacking | design/gdd/objectives-chain.md | OC-6, OC-7, OC-8, OC-14 | designed (in review); reconciled |
| `IsMarkedAlly(Pawn)`; `ShowTeamNames` (pending) | design/gdd/teammate-markers.md | TM-R10, CR-4, OQ11 | designed (in review) |
| UI 1-8 layout, anchors, layering | design/gdd/hud.md | HUD-R6, HUD-R12, CR-HUD-3 | designed (in review) |
| Baseline and Tell Matrix rows (brace, tend, bush, strike, run, traversal, informant) | design/gdd/crowd-npc-behaviour.md | | designed |
| Audiences `Owner`/`All` | design/gdd/information-visibility-policy.md | | designed |
| Sites and emergence sockets for prompts | design/gdd/level-socket-contract.md | `ObjectivePrelimSite`, `ObjectiveFinalSite`, `BushSite` | designed |
| Ring, indicator, prompts, palette, tells | design/art/art-bible.md | 4.4, 4.6, 5.2, 5.5, 7.1, 7.3, 7.5, 7.6, 7.8 | exists; change requests below |

**Change requests to other documents** (not applied here; parallel-mode rule):
1. **Canonical vocabulary**: add `Move(Direction, Gait)`, `Traverse` and `OnActionCompleted(Pawn, ActionType)`. Fix `ActionType` = {Interact, Prepare, Attack, HideEnter, HideExit}. Add `ClientCommandRejected(ClientCommandSeq, Code)` with Code ∈ {NotNow, Cooldown, Busy, OutOfReach}.
2. **Systems index**: add Participant State & Roster and Round & Match Framework to the IIF's Depends On (row 3 and Dependency Map item 3). Note the soft dependence of NPC-Kill Penalty, HUD, Telemetry and Anti-Cheat on the IIF.
3. **Participant State & Roster**: add `GetLegalityClass(Pawn)` (server-only; returns Active for unbound pawns; never exposes binding).
4. **Round & Match Framework**:
   - Accept IIF discrete commands as queue items. Stage 2 runs in the IIF resolver at the slot.
   - Change `Submit` to `Submit(Command, SourceMailbox)`, with the host-local source using the host mailbox.
   - Reconcile R11 ("assigned at the moment accepted") with R12 (stamped at drain start by mailbox rotation).
   - Add item type `HoldTimer(Complete | CapEnd | DebounceExpiry)`, submitted and resolved by the IIF and carrying `ActionEpoch`. Sort same-frame timers by `(DueTime, Begin key)` instead of `ParticipantId`, which the IIF may not use.
   - Make Objectives' `ObjectiveCompletion` a follow-on inside the `HoldTimer(Complete)` slot rather than an independently timed item (R13 table).
   - State that phase changes cancel active holds at their queue position, and that `WrongPhase` drops send no IIF reply (post-latch items: `Rejected_Latched`, IIF replies `NotNow`; accepted from Round change request 6 at reconciliation 2026-10-24).
5. **Assassination & Defence Reversal**: confirm Prepare = protected window from acceptance until End or cap; cooldown stamped at acceptance. Keep the defence cooldown and lockout in a per-pawn component (not PlayerState), so NPC pawns have the same state. Define the teammate-strike rule and whether a strike at a bracing NPC is reversed. State that the defender's brace state is sampled at Attack execution, never on a contact frame (no parry window). Decide whether Prepare inside a bush stays forbidden (IIF default: forbidden).
6. **Shared Character & Locomotion**: key release must give the baseline deceleration (no sudden-stop tell). The body faces travel direction in both camera modes. Publish `IsRunning`/`IsInTraversal`, and clamp Run while braced. Treat a tending or bracing pawn (any pawn) as a static obstacle for crowd avoidance, so avoidance pushes do not move it; and expose commanded vs external displacement for R8.
7. **Objectives Chain**: adopt "early release resets progress"; define stacking and `AllowNoEffect` decoys. Prompts appear only at the owner's live sites, never at a hidden final site.
   - Expose the owner team's **Team-audience live-site set** to the client (for prompts) and to the server (for the IIF's ingress `TargetId` filter, the hidden-site oracle guard).
   - Apply the completion effect as a follow-on inside the `HoldTimer(Complete)` slot.
   - The choice between `AllowNoEffect` and `Reject` depends only on (site, legality class).
8. **Art bible** (art-director list in Visual/Audio): 7.3 row 4, 7.5 (i), 7.6, 7.x C7, 7.8, 5.5 Brace row; reconcile 7.1 vs 5.2 on allies and the Strike hint.
9. **Crowd & NPC Behaviour**: the Baseline and Tell Matrix must cover NPC brace, tend and bush use (red herrings yes/no) and AI brace-duration distributions. The gaze catalogue is what makes Look tell-free (R3). NPC avoidance must route around tending pawns.
10. **Tuning Data & Config** (design review 2026-10-24):
    - Map migration row `ObjectiveInteractRange` → `InteractRangeCm` (IIF-owned), not `ObjectiveInteractRangeCm`, so one quantity has one name.
    - Add the cross-field invariants `TransportBucketSize ≥ 2 × Σ Bucket*Size` and `TransportBucketPerSec ≥ 2 × Σ Bucket*PerSec`.
    - Note that `PrepareLatencyForgivenessSec` is retired and `InteractCancelRangeCm` is replaced by `InteractCancelHysteresisCm`.
11. **Shared Character & Locomotion** (design review): own the R3a Look-parity mechanism:
    - one control-rotation path;
    - one body-yaw rule (provisional `bUseControllerRotationYaw=false` for all pawns);
    - one simulated-proxy pitch path;
    - the same cadence for ServerMove-stepped and tick-stepped pawns.

    Also own the R12 relevancy bans: no dormancy, no `bOnlyRelevantToOwner`, no per-class priority. Route AI path-following through `Move`.
12. **Information Visibility Policy** (design review): support `CanViewerKnow(Attacker, Pawn, Position)` as the server gate for `PawnRef` (R10), and the owner team's site-visibility query used by the ingress `TargetId` filter.
13. **Level Socket Contract** (design review): its UI-3 change request is applied. Objective prompts now use Objectives' Team-audience site data; bush prompts may use Manifest geometry.
14. **Prototype** (`Source/StealthGame`, for the implementation story, not a GDD): legacy key binds on the pawn, RPCs on the pawn without authority checks, and cooldown on PlayerState all contradict R2/R12/R13. To be replaced when the IIF ADR is accepted.

**Change requests received (reconciliation 2026-10-24)**

| From | Request | Disposition |
|---|---|---|
| Assassination CR 1 | Replicated action state becomes `FActionState {Pose, Seq, Variant, StartServerTime, ContactServerTime, WarpPoint}`; slot state `Countering`; cancel reason `Reversed`; Prepare reading confirmed; OQ2-4 answered; IIF CR 5 accepted | APPLIED (R12, R9, R11, R8, States, OQ2-4). Struct replaces the bare enum with the fields Assassination designed; no field beyond them added |
| Assassination CR 16 | Reversal `FHoldStatus.ServerEndTime = ContactServerTime`; replies to a struck target presented no earlier than its contact time | APPLIED (R6, R8 Prepare, UI 2, UI 5) |
| Concealment CR 1 | Split `BushTransition` into `BushEnter`/`BushExit`; `Hide(Exit(Side))` 2-bit side; owner-side Hide clip prediction ≤ 50 ms; `WorldPoint` includes `HideVolume`; bush `NotNow` confirmed; `Cooldown` for `HideEnter` | APPLIED (R1, R6, R9, R10, R12, States, Visual, Game Feel) |
| Concealment CR 10b | `FActionState.StartServerTime` for hide-clip seeking | APPLIED 2026-10-25 (user decision; R12) |
| Concealment CR 15 | `Move` zeroed during Hide transitions (IIF part; warp, collision and gating are SCL's) | APPLIED (R7 note, States) |
| Camera CR 1 | Shoulder swap binding in `IMC_Common` (X / middle mouse, D-pad Right; 3P only); R10 and Strike hint use `GetAimRay()`; "rendered" = mask α > 0 | APPLIED (R10, R13, R15, UI 4, Interactions, Dependencies) |
| Objectives CR 1a | A completion no longer cancels other holds at the court with `TargetInvalid` | APPLIED (R8 cancel table, Edge Cases) |
| Objectives CR 1b | F1 `D` range 2.0-6.0; Complete before DebounceExpiry at equal `DueTime` | APPLIED (F1, R8) |
| Objectives CR 1c / IVP CR 8 | Ingress accepts any court fixture id; identical "Tend" prompt at every court; live-site set colours the ring only | APPLIED (R5.1, R8, R17, UI 1, UI 3, AC 66) |
| Objectives CR 1d | `TendCueDelaySec` (Objectives-owned, client-visible) before the ring turns Marigold | APPLIED (UI 1, consumed keys, AC 66) |
| Objectives CR 1e | Full-fixture `Interact(Begin)` is `Reject(NotNow)` for every pawn | APPLIED (R6 code table, R8, Edge Cases) |
| Objectives CR 1f | `Dropped(PostLatch)` reads Round's `Rejected_Latched` | APPLIED (Edge Cases) |
| Round CR 6 | Queue API names; inline follow-ons; post-latch `Rejected_Latched` with IIF `NotNow` reply; command RPCs carry ids, not object pointers | APPLIED (R1 `PawnRef` as network id, R5 ordering, R6, Interactions, Edge Cases, AC 64). Supersedes this GDD's earlier "post-latch drops send no IIF reply" (change request 4, last bullet); `WrongPhase` drops stay silent |
| SCL CR 1 | `TraversalPointHint`; planted brace (OQ3); `PawnNetUpdateHz` 15 (12-17); R3a mechanism and R12 bans owned by SCL R8/R13; concealment-cull exception | APPLIED (R1, R7, R12, consumed keys, Dependencies, OQ3) |
| IVP CR 2 | R12 relevancy bans amended (concealment and relation only); R7.4 culled `PawnRef` resolves as `TargetSpot` with no distinct reply | APPLIED (R12, R10) |
| HUD CR-HUD-3 | Downstream HUD row "presenter of UI 1-8 (anchors and layering per HUD)"; UI 5/6 cite HUD anchors; plate cooldown keys stay IIF-owned | APPLIED (Dependencies, Interactions, UI 5, UI 6) |
| Teammate Markers CR-4 | `IsMarkedAlly(Pawn)` meaning in R10; list Teammate Markers in Dependencies | APPLIED (R10, Dependencies) |
| Teammate Markers CR-4 | New local `ShowTeamNames` hold action | PENDING-USER (scope; already listed in active.md as teammate-markers OQ11, CD recommends moving it to VS) — not added |
| Crowd CR 4 | AI court `Interact` bypasses only the human ingress site check; NPCs never `Attack` (T6); OQ5 answered by R7 + F2 | APPLIED for T6 and OQ5 (R17, OQ5); the bypass is ALREADY DONE in a stronger form (ingress accepts every court for every source, so no AI exemption exists) |
| LSC CR 11 | UI 3 objective prompts from Objectives' Team-audience data | DECLINED as superseded by Objectives OC-6 / IVP R7.4a ruling D (prompt identical at every court; team data colours the ring only) |
| TDC CR 9 / migration | `ObjectiveInteractRange` maps to `InteractRangeCm` | ALREADY DONE (Tuning Knobs) |
| Penalty (Dependencies rows) | informant legality row (R7); voiding commands of a pawnless participant | ALREADY DONE (R7, R6, Downstream row) — no change request was addressed to the IIF |
| Assassination CR 8 / Crowd CR 6 | `OnActionStarted(Attack)` fires for every outcome including whiffs | ALREADY DONE (R11: Attack emits `OnActionStarted` on execution) |
| Cross-review 2026-10-24 S-B1 (Round R9 staging) | Align the round-end hold release with Round's round-end step | APPLIED (cross-review fixes 2026-10-24): Edge Cases (shared final site; phase leaves `Active` mid-hold): holds frozen at the latch, cancelled at the round-end step; post-latch `NotNow` replies sent at that step |
| Cross-review 2026-10-24 C-W11 | Stale "not in the index" and "provisional" claims | APPLIED (cross-review fixes 2026-10-24): Dependencies rows (Roster, Round, Penalty) and Cross-References statuses |
| ADR-0011 sync 2026-10-24 | R6 (and HUD Dependencies row): `ClientCommandRejected` is an entry kind of the batched `ClientCommandReplies` RPC | APPLIED (R6 resolution tier) |
| ADR-0011 sync 2026-10-24 | R5.1: `PossessionEpoch` server-owned, delivered by `ClientSetPossessionEpoch` (Owner); IVP leak-catalogue row requested | APPLIED (R5.1); IVP row applied in information-visibility-policy.md |
| ADR-0011 sync 2026-10-24 | R8: hold timers are `ScheduleAt` due-time items; condition cancels are `HoldTimer(ConditionCancel)` | APPLIED (R8) |
| ADR-0011 / ADR-0021 sync 2026-10-24 | R13 / OQ7: CVar name `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush`; context swaps on the owner's presented own role (HUD-R6a) | APPLIED (R13, flush edge case, OQ7 mostly answered) |
| ADR-0011 / ADR-0016 sync 2026-10-24 | R17: AI path-following no longer feeds `Move`; Crowd's follower issues `Move` (input emulation) | APPLIED (R17, R8 commanded-displacement wording) |
| ADR-0021 sync 2026-10-24 | Add the local `ShowTeamNames` action (TM CR-4) | PENDING-USER (unchanged: already listed in active.md as teammate-markers OQ11; scope, CD recommends VS) — not added |
| ADR-0007/0008 sync 2026-10-24 | Test root follows the module layout | APPLIED (AC tags) |

## Acceptance Criteria

*qa-lead consulted. Its rewordings of untestable criteria are applied. Design review 2026-10-24 changes:*
- Criteria 1-53 were renumbered: section E's three unnumbered criteria became 38-40.
- Pass conditions are now deterministic. The KS test was removed.
- Criteria 54-75 were added to cover the uncovered rules, formulas and UI requirements.

**Test forms:**
- **U**: automation test `StealthGame.Input.<Scenario>`. Pure logic with injected time and a stub Round queue, in the style of the existing `StealthGame.Rules.*` prototype tests; production tests live in `Source/StealthGameplay/Private/Tests/Input/`, multi-client tests in `Source/StealthTestHarness/Private/` (ADR-0007/0008).
- **P**: 2-client PIE (listen server + 2 clients).
- **N**: P with net emulation. The default is PktLag 50 ms per direction with 1 % loss; a criterion may state otherwise.

**Types:** Logic, Integration or UI. Each UI criterion names its retained screenshot under `production/qa/evidence/`. Numeric limits are read from the tuning data at test time, not hard-coded (defaults shown for readability).

**A. Boundary and validation (R1, R2, R4-R6, R17)**
1. *R1, Logic, U `EntryPointSet`*: **GIVEN** the command component **WHEN** its server entry points are listed by reflection **THEN** they are exactly Move, Look, Traverse, Interact(Begin/End), Prepare(Begin/End), Attack and Hide(Enter/Exit).
2. *R2, Logic, U `NoCallerIdentityParameter`*: **GIVEN** every entry point, hub event and payload struct **WHEN** parameters and fields are reflected **THEN** none has a type or name matching Controller, PlayerState, Team, IsAI or IsPlayer; **AND** a CI grep of the component source for `IsPlayerControlled|IsLocallyControlled|IsNpc|GetTeam|NM_ListenServer` returns 0 hits.
3. *R2/R17, Logic, U `BoundUnboundSameEvents`*: **GIVEN** three pawns in the same state — one driven by the remote-client path (stub PC RPC), one by the host-local path (`IngestCommand(Source=HostLocal)`), and one unbound pawn driven by direct AI calls **WHEN** each receives the same scripted command sequence **THEN** the recorded hub events (event, ActionType, tick offset) are identical for all three; **AND** the reply codes for the two human paths are identical; **AND** the AI pawn's null sink records zero replies.
4. *R4, Logic, U `EdgesNotRepeat`*: **GIVEN** a key held 2 s **WHEN** input is sampled **THEN** exactly one Begin is sent and nothing repeats.
5. *R4, Integration, P `FlushSendsEnd`*: **GIVEN** Prepare held **WHEN** (a) the window loses focus, (b) a menu opens, (c) the context swaps, or (d) the pawn is unpossessed **THEN** End is sent within one frame; **AND** the action restarts only on a fresh press.
6. *R5.1, Logic, U `IngressClampAndDrop`*: **GIVEN** four commands — an Attack with a `WorldPoint` 900 cm away, an Attack with a self `PawnRef`, a command with a duplicate `ClientCommandSeq`, and an Interact whose `TargetId` names a site hidden from the owner's team **WHEN** ingress runs **THEN**:
   - the point is clamped to `AssassinationRangeCm + CommandRangeSlackCm` (240 cm);
   - the self ref is stripped and counted, and that Attack proceeds as a `TargetSpot`;
   - the duplicate is ignored;
   - the hidden-site Interact is dropped;
   - the dropped commands get no reply.
7. *R5.1/F3, Logic, U `IngressNeverDropsMatchedEnd`*: **GIVEN** an empty transport bucket and one outstanding Begin **WHEN** its End arrives **THEN** it is processed; **AND WHEN** 40 unmatched Ends arrive in 1 s **THEN** at most `UnmatchedEndPerSec` (30) reach the mailbox.
8. *R5, Logic, U `StageTwoAtResolution`*: **GIVEN** an Interact in a mailbox and an earlier-keyed transition to Informant in the same drain **WHEN** the queue resolves **THEN** the Interact is rejected `NotNow` and `OnActionStarted` does not fire.
9. *R5d, Logic, U `AllowNoEffectParity`*: **GIVEN** predicates returning `Allow` and `AllowNoEffect` **WHEN** an Interact runs to D **THEN** ActionPose writes, timers and events are identical; **AND** only `Allow` changes objective state.
10. *R6, Logic, U `RejectionCodeSet`*: **GIVEN** a failure at each stage 2a, 2b, 2c, 2d (cooldown), 2e, Interact distance and Interact facing **WHEN** the command resolves **THEN** exactly one reply is enqueued per failure, with codes `NotNow`, `NotNow`, `Busy`, `Cooldown`, `NotNow`, `OutOfReach` and `OutOfReach` respectively; **AND** every reply is enqueued in the end-of-resolution step of that drain; **AND** no replicated property changes.
11. *R6, Logic, U `AttackNeverRejectedByTarget`*: **GIVEN** a legal attacker **WHEN** Attack is aimed at an NPC, a player, an informant, a concealed pawn and an empty spot **THEN** each executes as a strike, with no reply code, at the same tick offset.
12. *R17, Logic, U `AiNoExemption`*: **GIVEN** the Prepare predicate stubbed to `Allow`, a bucket of B2 R1, and Begins sent at t = 0, 0.2, 0.4, 0.6 and 0.8 s (each preceded by an executed End) **WHEN** an AI pawn and a human pawn each send this sequence **THEN** each path admits exactly 2 (t = 0 and t = 0.2).

**B. Legality, holds, slot, targeting (R7-R10)**
13. *R7, Logic, U `LegalityMatrix`*: **GIVEN** each class × locomotion state of the R7 table **WHEN** each command is issued **THEN** the outcome matches the table cell, including Run clamped to Walk for an Informant.
14. *R7, Logic, U `BraceClampsRunTraverse`*: **GIVEN** Bracing **WHEN** Gait=Run then Traverse are issued **THEN** Run is clamped to Walk; **AND** Traverse is rejected `NotNow`.
15. *R8, Logic, U `InteractEarlyReleaseResets`*: **GIVEN** an Interact at 2.0 s of 3.0 s **WHEN** End executes and Begin is re-pressed **THEN** progress restarts at 0.
16. *R8, Logic, U `InteractCancelMatrix`*: **GIVEN** an active Interact **WHEN** each R8 cause is applied in turn **THEN** `OnActionCancelled` fires with the reason given in R8's cancel table:

    | Cause | Reason |
    |---|---|
    | End | `Released` |
    | Commanded displacement | `Moved` |
    | Run | `Moved` |
    | Traverse | `Moved` |
    | Beyond the cancel range | `OutOfRange` |
    | Attack, Prepare or Hide | `Superseded` |
    | Class change | `Illegal` |
    | Target invalidated | `TargetInvalid` |
    | Pawn removed | `PawnRemoved` |

    **AND** Look (a 180° turn), camera toggle, an approaching pawn and being targeted by an Attack whiff do not cancel.
17. *R8, Logic, U `PrepareCooldownStampedAtExecution`*: **GIVEN** a 0.1 s Prepare tap **WHEN** the cooldown is read **THEN** it ends at t_start + `DefenceCooldownSec` (60 s).
18. *R8, Logic, U `PrepareMashNoReset`*: **GIVEN** (a) an active brace and (b) a cooling brace **WHEN** Begin is sent 10 times in each state **THEN** no event fires, the cap does not restart and the cooldown end is unchanged in both; **AND** in (a) no reply is sent; **AND** in (b) each Begin gets exactly one `Cooldown` reply.
19. *R8, Logic, U `DebounceMerge`*: **GIVEN** End resolved at t and Begin at t+0.05 **THEN** the action is continuous, with no new token or event; **GIVEN** Begin at t+0.15 instead **THEN** the End executed in the first drain at or after t+0.10 (within one dt).
20. *R9, Logic, U `SlotPriority`*: **GIVEN** each of the 16 ordered pairs from {Attack, Prepare, Interact, Hide} **WHEN** the second arrives during the first **THEN** the outcomes are:
    - a higher-priority command preempts (`Superseded`);
    - a lower-priority command is rejected `Busy`;
    - same-type Interact or Prepare is a no-op;
    - same-type Attack (during Striking) or Hide (during a transition) is rejected `Busy`;
    - any command during a Hide transition, Attack included, is rejected `Busy`;
    - nothing auto-resumes.
21. *R10, Logic, U `PawnRefSameAsSpot`*: **GIVEN** a concealed pawn at X **WHEN** Attack is sent (a) with its PawnRef and (b) as TargetSpot at X **THEN** Assassination receives an identical victim resolution.
22. *R10, Logic, U `StalePawnRefFallsBack`*: **GIVEN** a PawnRef killed earlier in the drain **WHEN** the Attack resolves **THEN** it strikes at the WorldPoint.

**C. Events, replication, contexts, camera, bindings (R3, R11-R16)**
23. *R3, Logic, U `NoLookEvent`*: **GIVEN** 100 Look changes **WHEN** the hub is observed **THEN** zero events fire.
24. *R3, Integration, P `ReleaseBaselineStop`*: **GIVEN** walking at full walk speed **WHEN** all movement keys are released **THEN** time to zero speed equals Locomotion's NPC baseline stop time (±1 frame; numeric baseline from the Locomotion GDD, about 0.5 s provisional); **AND** peak deceleration does not exceed the NPC baseline.
25. *R11, Logic, U `EventHubOrder`*: **GIVEN** 3 pawns acting in one drain **WHEN** the queue resolves **THEN** hub events fire in key order; **AND** reflection finds `CancelReason` in no client RPC or replicated property.
26. *R11, Integration, U `NoEventRelay`*: **GIVEN** each hub subscriber, with an RPC-send hook installed **WHEN** it handles an event **THEN** zero client RPCs are sent within the handler's call stack, except through a Visibility Policy audience.
27. *R12, Logic, U `NoOwnerDataOnPawn`*: **GIVEN** the Shared Pawn class and its components **WHEN** replicated properties are listed **THEN** the only IIF-written one is `FActionState` (fixed serialised size for every `Pose`); **AND** `FHoldStatus`, cooldown and rejection data exist only on the PlayerController.
28. *R13, UI, P `InformantPlate`*: **GIVEN** an Informant **WHEN** Q, E, LMB or F is pressed 3 times in 2 s **THEN** no server RPC is sent, the plate shows once and no sound plays. Screenshot: `production/qa/evidence/iif-informant-plate.png`.
29. *R14, Logic, U `CameraToggleLocal`*: **GIVEN** an active hold **WHEN** V is pressed **THEN** the hold continues and zero RPCs are sent; **AND** a second press within `CameraToggleLockoutSec` (0.30 s) is ignored.
30. *R15, Integration, P `DefaultBindings`*: **GIVEN** fresh settings **WHEN** each binding in the R15 table is pressed on keyboard/mouse and on gamepad **THEN** the mapped command reaches the server command log; **AND** RMB is not bound to Prepare.
31. *R16, Logic*:
    - MVP: **GIVEN** the MVP build **THEN** no toggle setting exists.
    - VS, deferred gate `ToggleStreamParity`: **GIVEN** scripted press/press timings in toggle mode, and the equivalent press/release timings in hold mode **WHEN** the server command log (type, payload, `ClientCommandSeq`, arrival tick) is captured for each **THEN** the two logs are identical.

**D. Formulas**
32. *F1, Logic, U `HoldCompletionEpsilon`*: **GIVEN** D=3.0 and injected dt of 1/60, 1/30, and a fixed varying table (0.0083, 0.05, 0.02, repeated) **WHEN** ticking **THEN** `HoldTimer(Complete)` is submitted on the first drain with elapsed ≥ 3.0−1e-4 (180 ticks at 60 Hz); **AND** P stays in [0,1]; **AND** the overshoot is < dt.
33. *F2, Logic, U `BraceWindowBounds`*: **GIVEN** holds of 0, 1.2 and 9 s at injected dt **WHEN** W_real is measured **THEN** it lies in [0.10, 0.10+dt), [1.30, 1.30+dt) and [4.0, 4.0+dt) respectively, identically on the AI and human paths.
34. *F3, Logic, U `TokenBucketDefaults`*: **GIVEN** a stub `Allow` predicate and an Interact bucket of B3 R2 **WHEN** 16 admissible Begins arrive at t = k·0.125 s **THEN** exactly 6 are admitted, at t = 0, 0.125, 0.25, 0.5, 1.0 and 1.5 s; **AND** Begins rejected at stage 2d consume no token.
35. *F4, Logic, U `RingMonotonicClamp`*: **GIVEN** RTT_est 150 ms on a 30 Hz host **WHEN** no confirmation arrives **THEN** the ring hides at T_ack = 0.483 s; **AND** it never exceeds 0.98 before `Outcome=Completed`; **AND** it never decreases, including at the moment of confirmation.
36. *F5, Integration, N `LatencyBudget`*: **GIVEN** PktLag 75 ms per direction (RTT 150 ms), a host fixed at 30 Hz and t_interp 0.05 **WHEN** Prepare is pressed 100 times **THEN**, measured on the PIE process clock, the owner's `FHoldStatus` arrives in ≤ 220 ms at p95; **AND** the observer's ActionPose OnRep occurs in ≤ 400 ms at p95.
37. *F6, Logic, U `InteractHysteresis`*: **GIVEN** a site with default keys **WHEN** Begins are issued at 190.0 cm and at 190.1 cm **THEN** the first is accepted and the second rejected `OutOfReach`; **AND WHEN** issued at 60° and at 61° off facing **THEN** the first is accepted and the second rejected `OutOfReach`; **AND GIVEN** an accepted hold at 185 cm **WHEN** pushed externally to 200 cm, then turned 180° by Look **THEN** it continues; **AND WHEN** held at 240.0 cm **THEN** it continues; **AND WHEN** at 240.1 cm **THEN** it cancels `OutOfRange`; **AND WHEN** 20 cm of commanded displacement occurs after the grace **THEN** it cancels `Moved`.

**E. Cross-system**
38. *R8/F6, Integration, P `CrowdBrushDoesNotCancelTend`*: **GIVEN** a pawn tending a site, and a recorded set of 20 fixed NPC paths passing within touching distance **WHEN** the paths are replayed **THEN** the hold is never cancelled by contact. Only End, commanded movement, or leaving the 240 cm cancel range cancels it.
39. *R9, Logic, U `NoBraceOrTendInBush`*: **GIVEN** a concealed pawn **WHEN** it issues Prepare(Begin) or Interact(Begin) **THEN** both are rejected `NotNow`; **AND** an Attack from the bush executes and breaks concealment.
40. *R10, Integration, U `BraceSampledAtExecution`*: **GIVEN** an Attack executed against an unbraced defender **WHEN** the defender's Prepare resolves in a later drain, during the strike animation **THEN** the outcome is the one decided at execution (struck); **AND** the late Prepare is voided with one `NotNow` to the defender's sink.
41. *Integration, U `ConcealmentHearsAttack`*: **GIVEN** a concealed pawn **WHEN** it Attacks **THEN** `OnActionStarted(Attack)` fires; **AND** Concealment breaks concealment in the same resolution.
42. *Integration, U `ObjectivesCompletion`*: **GIVEN** an Interact on a site whose predicate returns `Allow` **WHEN** its `HoldTimer(Complete)` resolves **THEN** `OnActionCompleted` fires once, Objectives advances exactly once inside the same slot, and a later End is a no-op.
43. *Integration, U `AssassinationCooldownNoReset`*: **GIVEN** a cooling defender **WHEN** Prepare is re-pressed, Interact is used or the camera is toggled **THEN** the cooldown end time is unchanged.
44. *Integration, U `RosterLegalityClass`*: **GIVEN** an unbound pawn **THEN** it resolves to Active; **GIVEN** `RequestTransition(Informant)` mid-hold **THEN** the hold cancels `Illegal` in the same resolution.
45. *Integration, U `RoundQueueOrder`*: **GIVEN** a defender Prepare and an attacker Attack in one drain **WHEN** the Prepare has the lower key **THEN** a reversal is reported; **WHEN** the Attack has the lower key **THEN** the defender is struck, no `OnActionStarted(Prepare)` fires, and the defender gets exactly one `NotNow`, enqueued at the end of that drain's resolution.

**F. Pillar-1 parity (observed on an opponent client)**
46. *Integration, N `ActionPoseParityObserved`*: **GIVEN** an NPC and a player each bracing 1.2 s **WHEN** an opponent client logs the Brace **THEN** the onset-to-offset durations differ by ≤ U + 1 frame; **AND** net update cadence and property size are identical.
47. *Integration, U `RejectionTimingIdentityBlind`*: **GIVEN** rejected Attack and Interact commands that differ only in the nearby pawn (NPC, player, concealed, teammate), 50 samples each **WHEN** the server-side reply-send log is inspected **THEN** in 100 % of samples the code is identical and the reply is enqueued in the end-of-resolution step of the same drain as the command; **AND** the same holds for `FHoldStatus` confirmations of accepted commands.
48. *Integration, N `PawnReplicatedValueParity`*: **GIVEN** a bound pawn and an NPC pawn **WHEN** an opponent client dumps every replicated property's value and per-property bytes (Network Insights) **THEN** the property sets match; **AND** `PlayerState` and `Owner` are null on both; **AND** the bytes per update match for the same state.
49. *UI, P `StrikeHintParity`*: **GIVEN** an NPC, an enemy player and an enemy informant with the same `AppearanceId`, each in reach **WHEN** the ray hits each **THEN** the hint widget, rendered alone to a render target, is pixel-identical in all three; **AND** no hint appears for an empty spot or a marked ally. Screenshots: `production/qa/evidence/strike-hint-parity-{npc,player,informant,spot,ally}.png`.
50. *Review, deferred gate `TellMatrixRows`*: **GIVEN** Crowd & NPC Behaviour's Baseline and Tell Matrix **WHEN** this GDD is reviewed against it **THEN** rows exist for brace, objective interaction, bush enter/leave and assassination, each stating the visible deviation and whether NPCs use it as a red herring. This cannot be checked until the Crowd GDD exists.

**G. Performance** (proposed budgets, provisional pending the 50-character spike; technical-director to confirm). All are measured on the RTX 3060-class reference PC, Development build, 1080p, with Insights scopes `IIF.Validate`, `IIF.HoldTimers` and `IIF.Hub`.
51. *Integration, N `CommandCostPer50Pawns`*: **GIVEN** 50 pawns (40 AI issuing at bucket maximum, 10 humans) on a 30 Hz listen server **WHEN** profiled for 60 s **THEN** the summed IIF scopes cost ≤ 0.15 ms per server tick at p95.
52. *Integration, N `ActionPoseBandwidth`*: **GIVEN** 50 pawns cycling poses at their maximum rate **WHEN** Network Insights measures one client for 60 s **THEN** ActionPose traffic averages ≤ 1.0 KB/s.
53. *Integration, N `OwnerStatusBandwidth`*: **GIVEN** a player mashing every action key at the bucket limit **WHEN** measured for 60 s **THEN** owner status plus rejection replies average ≤ 0.5 KB/s. This keeps the IIF ≤ 3 % of the 50 KB/s per-client budget.

**H. Coverage added by the design review (2026-10-24)**
54. *R3a, Integration, N `LookParityObserved`*: **GIVEN** a stationary human turning the camera 90° over 1 s, and an NPC performing a gaze-catalogue turn of 90° over 1 s **WHEN** an opponent client records both pawns' replicated rotation, view pitch and head/body yaw **THEN** update cadence, property set and smoothing curve match within 1 frame and 2°.
55. *R13, Integration, P `ContextSwapOnStateAndPossession`*: **GIVEN** an Active player **WHEN** Roster's Owner-audience state changes to Informant, and separately **WHEN** the pawn is re-possessed **THEN** `IMC_Informant` (resp. the class-appropriate context) is active within one frame; **AND** all held inputs are flushed.
56. *R12, Logic, U `ForceNetUpdateOnWrite`*: **GIVEN** a spy on the net-update request **WHEN** `ActionPose` is written on a bound pawn and on an NPC pawn **THEN** exactly one forced net update is requested per write on each; **AND** neither pawn uses dormancy or `bOnlyRelevantToOwner`.
57. *F4, Logic, U `RingResyncAndLateConfirm`*: **GIVEN** a server-time drift of 0.3 s **THEN** the estimate re-syncs and the ring does not move backward; **GIVEN** a hidden ring and a late `FHoldStatus` with the latest `ClientCommandSeq` while the key is held **THEN** the ring shows Confirmed; **GIVEN** Prepare **THEN** no predicted ring branch is used.
58. *F5, Integration, P `LookLatency`*: **GIVEN** 60 fps **WHEN** mouse input is injected **THEN** the camera transform changes in the same frame (≤ 1 frame, measured by frame counter).
59. *F3, Logic, U `TransportBucketAndAbuseLog`*: **GIVEN** the defaults **WHEN** a connection sends 35 Begins in 0.1 s **THEN** 30 pass ingress and 5 are dropped; **AND** HideEnter and HideExit draw on one shared per-pawn bucket; **AND** 20 ingress drops within 10 s write exactly one abuse-log entry, while 19 write none.
60. *R5.1, Integration, P `ImpossiblePayloadDisconnects`*: **GIVEN** a payload with an out-of-range enum value **WHEN** it arrives **THEN** `WithValidation` fails and the connection is closed; **AND GIVEN** a merely clamped or stale payload **THEN** it is not disconnected.
61. *R10, Integration, P `NoAllyAttackNoAimAssist`*: **GIVEN** the ray on a marked teammate **WHEN** Attack is pressed **THEN** no Attack with that `PawnRef` is sent; **AND GIVEN** a gamepad with a pawn 5° off the ray **WHEN** identical stick input is replayed with and without that pawn present **THEN** the resulting control rotations are identical.
62. *R9, Logic, U `LockoutOccupiesSlot`*: **GIVEN** an executed Attack with `AttackAttemptLockoutSec` stubbed to 1.5 **WHEN** Interact, Prepare and Hide are issued at +1.0 s **THEN** each is rejected `Busy`; **AND** at +1.6 s each is accepted.
63. *R8, Logic, U `ForcedRunCancelsBrace`*: **GIVEN** an active brace **WHEN** Locomotion reports an externally forced run **THEN** the brace cancels `Moved`; **AND GIVEN** an End executed at 1.3 s **THEN** `FHoldStatus.ServerEndTime` changes from start + 4.0 to start + 1.3 (± dt).
64. *Round phase, Logic, U `WrongPhaseCommands`*: **GIVEN** phase `Preparing` or `Ended` **WHEN** any discrete command is sent **THEN** Round drops it (`WrongPhase`); **AND** no `OnActionStarted` fires and no IIF reply is sent; **AND GIVEN** a command still pending in the queue when the latch closes it **THEN** it returns `Rejected_Latched`, no `OnActionStarted` fires, and exactly one `NotNow` reply is enqueued at drain end.
65. *UI 2, UI, N `BraceProtectedOnlyOnConfirm`*: **GIVEN** PktLag 75 ms per direction **WHEN** Q is pressed **THEN** the brace indicator shows only the pending outline until `FHoldStatus` arrives, and Protected (square plus first segment) only after. Screenshots: `production/qa/evidence/iif-brace-{pending,protected,cooling}.png`.
66. *UI 3, UI, P `PromptLegality`*: **GIVEN** the owner in reach of (a) an own live site, (b) an enemy site, (c) a hidden final site, (d) a non-site court, and (e) a bush, and separately as an informant **WHEN** prompts are evaluated **THEN** an identical "Tend" prompt shows in (a), (b), (c) and (d) (Objectives OC-6) and "Slip in" only in (e); **AND** nothing shows to the informant; **AND** after Begin the ring stays Bone for `TendCueDelaySec` at every court and then turns Marigold only in (a). Screenshots: `production/qa/evidence/iif-prompt-{live,enemy,hidden,court,bush,informant}.png`.
67. *UI 5, UI, P `RejectionPlate`*: **GIVEN** a `NotNow` rejection repeated 3 times within 3 s, then a `Cooldown` rejection **WHEN** plates are observed **THEN** the "Not now" plate shows once; **AND** no plate shows for `Cooldown`. Screenshot: `production/qa/evidence/iif-reject-plate.png`.
68. *UI 7, UI, N `LatencyIcon`*: **GIVEN** RTT 180 ms, then 220 ms **WHEN** the HUD is observed **THEN** the icon is hidden at 180 ms and shown at 220 ms. Screenshot: `production/qa/evidence/iif-latency-icon.png`.
69. *R8, Logic, U `HoldTimerKeyOrder`*: **GIVEN** an Interact whose `HoldTimer(Complete)` falls due in the same frame as an Attack that kills the tending pawn **WHEN** the drain resolves **THEN** the outcome follows key order: Attack first means the hold cancels `PawnRemoved` and there is no completion, while Complete first means one completion and then the kill; **AND** exactly one of the two outcomes occurs.
70. *R2, Logic, U `HostIngressParity`*: **GIVEN** the host-local source **WHEN** it sends a 900 cm `WorldPoint`, a duplicate seq and a mash at 35 presses per 0.1 s **THEN** clamping, ignoring and bucket drops are identical to the remote-client path; **AND** its replies are enqueued, never delivered inline.
71. *R5.1, Integration, N `HiddenSiteProbe`*: **GIVEN** a modified client sending `Interact(Begin)` from 100 cm with (a) a hidden final site's id, (b) a non-site court's id and (c) a rival live site's id, 50 times each, plus (d) a nonexistent id **WHEN** server-side handling is logged **THEN** (a), (b) and (c) are accepted at ingress, run the same hold, emit the same events and replies with the same timing class, and complete as `AllowNoEffect`; **AND** (d) is dropped silently at ingress; **AND** no client-observable message differs among (a)-(c) (reconciliation 2026-10-24: every court is accepted at ingress, IVP R7.4a, AC-IVP-41).
72. *R10, Logic, U `PawnRefGate`*: **GIVEN** an Attack whose `PawnRef` (a) fails `CanViewerKnow` or (b) lies 100 cm from the `WorldPoint` **WHEN** it resolves **THEN** it resolves exactly as a `TargetSpot` at the `WorldPoint`.
73. *R5/F2, Logic, U `SameFrameTapIsShortBrace`*: **GIVEN** `Prepare(Begin)` and `Prepare(End)` in the same mailbox in one frame **WHEN** the drain resolves **THEN** the brace executes and ends via `DebounceExpiry` within [0.10, 0.10 + dt) s, never at the cap.
74. *F6, Logic, U `InteractGraceOnArrival`*: **GIVEN** a pawn decelerating from walk speed when Interact executes **WHEN** it travels 12 cm within the first 0.25 s and then stops **THEN** the hold continues; **AND GIVEN** 20 cm of commanded travel after 0.25 s **THEN** it cancels `Moved`.
75. *Tuning, Logic, U `KnobRegistryConformance`*: **GIVEN** the IIF tuning data asset **WHEN** TDC's schema test (AC-3a) and doc lint (AC-3b) run **THEN** every key in the Tuning Knobs table is present with its default, hard and safe ranges, Audience and Latch; **AND** every key ends with a CR-15 suffix.

## Open Questions

| # | Question | Owner | Target |
|---|---|---|---|
| 1 | **Hold vs toggle for Prepare (and Interact, Run)**: ship the client-side toggle (R16) in Settings & Accessibility? (An extended release tolerance is **not** allowed, because it would alter defence duration, which breaks the accessibility guard. Settled here.) | Settings & Accessibility (accessibility-specialist, ux-designer) | Vertical Slice design |
| 2 | ~~Prepare semantics (protected window vs charge-then-armed): confirm the provisional R8 reading~~ **Answered** (reconciliation 2026-10-24): protected window confirmed by Assassination ASN-R3; R8 updated. | Assassination & Defence Reversal GDD | closed |
| 3 | ~~May a braced pawn walk, and at what speed? Should Prepare/Interact inside a bush stay forbidden?~~ **Answered** (reconciliation 2026-10-24): the brace is planted, no translation and no yaw (SCL R5.2, ASN-R3/OQ9); Prepare/Interact inside a bush stay `NotNow` (Concealment change request 1). R7 and R9 updated. | Locomotion / Concealment / Assassination | closed |
| 4 | ~~Teammate strike: impossible, a whiff, or legal?~~ **Answered** by Assassination ASN-R9 (allies are excluded from victim candidates; confirmed by Concealment OQ2). The client still never sends it (R10, `IsMarkedAlly`). | Assassination | closed |
| 5 | ~~Do NPCs use Prepare, Interact (`AllowNoEffect`) and bushes as red herrings, and with what duration distribution?~~ **Answered** by Crowd & NPC Behaviour R7 and F2 (Baseline and Tell Matrix T3-T6); NPCs never issue `Attack` (T6). | Crowd & NPC Behaviour | closed |
| 6 | Is LMB Strike too easy to misclick (one press costs a strike or your life)? No confirm step is allowed; playtest only | game-designer, ux-designer | first 2-client playtest |
| 7 | **Mostly answered by ADR-0011 (2026-10-24 ADR sync)**: remapping uses `UEnhancedInputUserSettings` (E4); flush CVar is `EnhancedInput.IgnoreHeldDigitalActionKeysOnFlush` (E2); presentation time is `EstimatedServerNow()` (ADR-0002 §9), never `GetServerWorldTimeSeconds`; legacy driver (ADR-0001). Still open: exactly-one-`Completed` on flush (spike V1). Original question: UE 5.8 verification: remapping API (`UEnhancedInputUserSettings` vs `PlayerMappableInputConfig`), Enhanced Input event behaviour on context removal and focus loss, `GetServerWorldTimeSeconds` accuracy, Iris default and owner-only conditions, server-side call of Server RPC functions, PlayerState/Owner/Controller replication conditioning on pawns | gameplay-programmer, ue-replication-specialist | IIF ADR (-> ADR) |
| 8 | Listen-server host advantage (~RTT/2 in Prepare-vs-Attack races) and host-held ServerOnly data: accepted for trusted prototype playtests; revisit with the dedicated server (Alpha) | technical-director | Alpha |
| 9 | Strike hint vs a static centre marker if aiming fails in playtests (art bible 7.1 amendment) | ux-designer, art-director | first playtest |
| 10 | Gamepad: full remap UI, sensitivity curves, and whether gamepad players need any help aiming the strike ray (never aim assist) | Settings & Accessibility | Vertical Slice |
| 11 | Design review follow-ups. Do Round & Match Framework and Objectives Chain accept `HoldTimer` items, `Submit(Command, SourceMailbox)` and the completion follow-on (change requests 4 and 7)? Verify ForceNetUpdate (U ≤ one frame) under Iris, `RemoteViewPitch` parity, and server-side NetGUID resolution for `PawnRef` in 5.8. Tune `PawnRefAgreementCm` and `InteractMoveGraceSec` in the first 2-client playtest | Round, Objectives, ue-replication-specialist, ux-designer | before re-review / IIF ADR |
