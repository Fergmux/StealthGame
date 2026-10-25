# Crowd & NPC Behaviour

> **Status**: Reviewed - NEEDS REVISION 2026-10-24 (revisions applied; re-review pending); reconciled 2026-10-24; ADR-synced 2026-10-24
> **Author**: design-system pipeline (autonomous) + ai-programmer, systems-designer, game-designer; creative-director (CD-GDD-ALIGN gate)
> **Last Updated**: 2026-10-24
> **Last Verified**: 2026-10-24
> **Implements Pillar**: P1 Hide in Plain Sight (primary); P2 Every Advantage Is a Tell (primary); P3 One Strike, Lasting Consequence; P4 A Race, Not a Hunt
> **Creative Director Review (CD-GDD-ALIGN)**: REVISED 2026-10-24 (CONCERNS, 5 items, all applied: OQ1 trade restated and bush entries context-free (R7); evidence-compounding guard `CrowdTellChainGapSec` + AC-CNB-31; panic runs count against T1; Player Fantasy body-language fix; alley absorption named as the one NPC-only exception (R4))
> **Design Review**: NEEDS REVISION 2026-10-24 (full mode: systems-designer with game-designer and qa-lead lenses, ai-programmer with unreal-specialist and performance lenses, creative-director senior verdict). Revisions applied in place: conflict C7 (Concealment version of bush use, k = 1.5), Objectives CR6, chain-gap rate compensation, stall-cover metric, interim pace rule, perception and handover fixes, AC rewrites. Log: `design/gdd/reviews/crowd-npc-behaviour-review-log.md`; a full re-review is required.

## Summary

Crowd & NPC Behaviour runs the ~40 citizens who share the city, the pawn class and the appearance deck with the ten players: their density, routes, pauses, glances, conversations, reactions to a strike, replacement after a death and the hand-over of pawns to and from participants. It makes the crowd a stable, learnable baseline that NPCs drive through exactly the player's command interface, and it owns the Baseline and Tell Matrix that calibrates how often NPCs perform each player advantage, so a deviation is evidence, never proof (Pillars 1 and 2).

> **Quick reference** — Layer: `Core` · Priority: `MVP` · Key deps: `Character Pool & Appearance, Shared Character & Locomotion, Input & Interaction Framework, Level Socket Contract, Information Visibility Policy, Tuning Data & Config, Round & Match Framework`

## Overview

Crowd & NPC Behaviour (CNB) is the server-side system that owns the forty-odd citizens who are not players: how many there are (`NpcCount`, default 40) and where they stand (the density model), what they do minute to minute (the **baseline behaviour catalogue**, including the **gaze catalogue**), which "player-like" things they also do on purpose (the **red herrings**), how they react to a strike nearby, how the city refills after a death (replacement through the Level Socket Contract's emergence sockets), and how a pawn passes between crowd control and a participant (disconnect takeover, `ReleaseToCrowd`, informant takeover). It drives every NPC through the **same pawn command interface** a human uses (`Move`, `Look`, `Traverse`, `Interact`, `Prepare`, `Hide`), on the one Shared Pawn class, so an NPC can do nothing a player could not and a player has no motion an NPC never shows.

For the player the system is the world itself. Pillar 1 is only as strong as the crowd is believable, and Pillar 2 only works if the crowd is a **stable, measurable baseline** with chosen deviations on top. CNB therefore also owns the **Baseline and Tell Matrix**: one row per advantage (run, traversal, objective interaction, brace, bush, strike, informant restrictions, alley emergence, connection stall, AI handoff and the SCL motion tells), stating the visible deviation, whether and how often NPCs produce it, and the owning system. Each feature GDD adds an acceptance criterion pointing at its row. Without this system the game is a deathmatch among mannequins: every runner a confession, every still body a disconnect, every emergence a kill notice.

Implementation choices (StateTree versus Behavior Tree, steering, nav query budget) are marked **-> ADR**; ADR-001 (crowd identity rules: one pawn class, AI through the player's movement path, no AI-keyed LOD or budget) binds the whole document.

## Player Fantasy

*Framings from the game-designer consult; reviewed by the creative-director in the CD-GDD-ALIGN gate (CONCERNS, applied 2026-10-24; see header).*

**"The city has a rhythm, and you can learn it."** The crowd is the instrument both teams play. As the hidden player you feel like *a local*: you catch a stranger's glance holding half a second too long, slow to a stop at a stall, and watch them look away and walk on; for one breath you were as ordinary as everyone else. As the hunter you feel like *a reader of people*: a woman in a feathered shawl is hurrying for no reason, you track her, and then you realise she is hurrying toward a shrine, not away from you. You were reading a person, not a marker.

The anchor moment is **the doubt after a deviation**: someone ran, vaulted, braced or stepped into a planter, and the crowd has done each of those often enough that it is evidence, not proof. Pillar 1 ("Hide in Plain Sight") is served when nothing about the crowd separates a player from a citizen unless the player chose it; Pillar 2 ("Every Advantage Is a Tell") is served when every advantage still stands out against a baseline stable enough to learn. The crowd must never feel like a slot machine (anti-pillar: reading must not be a coin flip) or like a crowd of mannequins (art bible 5.4: "composed, not stiff").

## Detailed Design

### Core Rules

*Specialists consulted: ai-programmer (R3, R10-R12, R15, States), systems-designer (R7, R9, R13, Formulas, Edge Cases), game-designer (R4, R6-R8, R16 Tell Matrix, Player Fantasy). qa-lead and art-director were not spawned (orchestrator budget of three); their lenses are applied explicitly in Acceptance Criteria and Visual/Audio.*

**CNB-R1. Population.** At round start Round spawns `NpcCount` (default 40) NPC pawns in its single shuffled spawn list (Round P6), placed by CNB's density function (R13) inside LSC `RequestStartPlacement`. Mid-round, CNB creates NPC pawns only through `RequestEmergence(Replacement | AmbientEntry)` and the one factory `SpawnSharedPawn(..., Ai, ...)` (SCL R2), calling `DrawReplacement()` in the spawn tick; it removes them only through `RequestEmergence(AmbientExit)` (CPA: card becomes `NpcVacant`). It never spawns, hides or destroys a pawn by any other path. The live NPC count is `NpcCount − V − B + Rel`, where V = vacancies (in-flight ambient cycles and unserved kills, F3), B = NPC pawns taken over by informants, Rel = participant pawns released to the crowd; none of these depends on who died.

**CNB-R2. Identity blindness.** No CNB code path reads `IsPlayer`, Roster binding, team, participant state, `VictimKind`, `FSiteAssignment`, zone roles, strike count or objective progress to choose a behaviour, a target, a rate, a socket, a destination or a timing. Perception reads only public state every client could see: position, velocity, `ActionPose`, `TraversalState`, body facing, and (server side) `IsConcealedFrom` treated identically for every concealed pawn. **NPC perception never sees a concealed pawn:** every perception query (glance target, conversation partner, avoidance obstacle, reaction source) excludes every pawn in `Concealed`, of any kind, exactly as a non-ally viewer would, and requires server line of sight from the NPC's eye; RVO, crowd avoidance and every steering query also exclude pawns in `Entering`, `Exiting` and `Emerging` (they ignore pawn collision, Concealment CON-R4), so no NPC bends its path around an invisible or transitioning occupant; the bush's `HideVolume` is the steering obstacle, never its occupants (conflict C7: bush use is occupancy-blind). **No Roster read at all (ADR-0016 §2):** the restricted driver (R10) is selected by the **call** Round or Penalty makes, not by reading the binding: `DriveBoundPawn(Pawn, …)` (a participant-bound pawn: restricted subset) versus `AdoptReleasedPawn(Pawn, …)` (an unbound NPC: full behaviour). Enforcement: CNB modules include no Roster or Objectives query header (build check expects zero Roster call sites, AC-CNB-01).

**CNB-R3. Same commands, same rules.** Every NPC is possessed by the Shared Pawn's AIController (SCL R2) and acts only through the IIF R1 entry points under R5-R9 and R17: same legality, slot arbitration, token buckets and 0.10 s release debounce. Path following never writes to the movement component: a CNB follower converts the path into `Move(Direction, Gait)` (R3.2 below). Before any discrete command CNB evaluates the same legality the IIF will (dry run, with reach and facing margins of 10 cm and 10°); a soak test expects zero AI rejections other than **same-drain contention** (a human claims the same fixture stand point or bush side in the same drain). On any rejection the StateTree abandons that behaviour and starts a new leg; it never retries the same target within 10 s. AI `Look` goes through the SCL R8 filter like any control rotation, so its head speed, clamp and ease are a human's; because the engine AIController discards control-rotation pitch unless its focus is a pawn [verified 5.8.4, `AAIController::UpdateControlRotation`], the crowd's AIController overrides that update and writes the `Look` yaw and pitch into the control rotation directly (R15). **Same commands make NPC behaviour legal, not human-looking**: R3.4 adds the motion-parity rule.
1. **Pace.** Walk magnitude is one of the shared set {0.8, 0.9, 1.0}, chosen only at the start of a route leg or after a stop, with time share at 1.0 = `CrowdFullPacePct` (SCL R5.1 requires ≥ 50 %). Group drift picks the pace nearest a neighbour's; yielding never changes pace. **Interim pace rule (OQ3, creative-director ruling 2026-10-24):** a keyboard player cannot produce 0.8 or 0.9, so slower NPC paces would be an NPC-only behaviour (R4). Until a keyboard "walk slower" modifier exists in SCL/IIF (pending user decision: it adds a player control), `CrowdFullPacePct` is 100 (NPCs walk only at 1.0) and C4 pace matching is inactive; when the modifier ships, the default returns to 55 and C4 is re-enabled.
2. **Path.** Async nav query → funnel (string-pulled) corners → per-pawn lane offset (±0.5 m, per-pawn stream) → pure-pursuit carrot at max(150 cm, v·1 s) → yield bias → `Move`. The follower begins turning early enough to fit the SCL F3 arc (134 cm at 1.0), so NPCs do not stop at corners; it never commands a heading change > `ReversalAngleDeg` while moving except as the deliberate reversal behaviour (C8). Arrival issues `Move(0)` at the SCL F2 stop distance (42 cm at 140 cm/s) before the stop point. The follower recomputes its carrot at `CrowdSteerPerSec`, but its last `Move` is a **held intent re-applied every movement tick**, as a held key is, never a one-shot input. Paths come from pre-baked `CrowdRoute` edge paths where an edge exists (no query), and from async queries otherwise.
3. **Avoidance (B6) as Move.** On a predicted closest approach < 100 cm the carrot is offset laterally up to 50 cm (side from relative position, keep-right tie-break, per-pawn coin only on an exact head-on); applied with probability `CrowdYieldPct` (70 %, so non-yield is 30 %, the top of SCL OQ10's 10-30 % start range) and a 0.2-0.6 s jitter. The decision is taken when the predicted approach is first detected, typically 3-5 m out. Planted and tending pawns are static 50 cm obstacles plus margin (SCL R16); concealed and bush-transitioning pawns are not perceived (R2, CON-R4) and the bush `HideVolume` is static geometry. Blocked > 0.5 s widens the offset to 100 cm; blocked 3 s repaths. **Progress monitor:** less than 50 cm of net progress in 5 s counts as blocked; after two failed repaths, or a failed or partial path query, the NPC abandons the leg and draws a new destination. No pawn is ever pushed. No runtime nav carving.
4. **Motion parity (human-input profile).** NPC heading changes, turn quantisation and the relation between walking heading and glance direction must match logged human patterns, weighted by the measured input-device mix (keyboard and mouse, gamepad). The follower drives `Move`/`Look` through a virtual input layer: a virtual camera yaw plus quantised 8-way or analog input from a per-pawn device profile drawn from the pawn's stream (R14), so a walking glance bends the path as a human's does. **Same view envelope (Camera change request 5):** the virtual camera yaw and pitch pass through Camera's exported pure function `EvaluateViewEnvelope` (Camera F1, CS-R3), so an NPC's edge pushes, latches and relaxes follow the same function as a human's view; push durations and frequencies are drawn from logged human telemetry, because matching the code alone does not give motion parity. Until human logs exist, provisional profiles from internal play are used; this shares the telemetry dependency of R7 (OQ12). Gated by AC-CNB-27 and AC-CNB-28 (BLOCKING). Input emulation versus an SCL turn-smoothing filter applied to every pawn is decided in the Crowd ADR (**-> ADR**, OQ11).

**CNB-R4. Player-reproducibility rule.** The NPC catalogue may contain **only** behaviours a human can produce through the same commands and the shared AnimBP. Anything only NPCs do would prove NPC-ness, shrink the candidate pool and raise blind-guess success (P1). Consequences: (a) **B8 brief touch** is a shared **contextual idle overlay** in the AnimBP (SCL change request) that any idle pawn within 100 cm of a touchable prop plays with the same per-pawn random timer, player or NPC; (b) **sitting is cut** from MVP; (c) conversation gestures are the same shared overlay for any idle pawn facing another within 150 cm; (d) a public `AmbientSpot` Interact verb (sit, browse) is **deferred** to a Vertical Slice decision (scope change: pending user decision); if adopted it must be usable by every Active pawn and gets its own Tell Matrix row. **The one permitted NPC-only event is alley absorption and creation (T8)**, allowed only because no pawn can observe it: the LSC clear check forbids it in any pawn's line of sight and a watched exit gives up after `CrowdExitGiveUpSec` and walks back out (AC-CNB-09).

**CNB-R5. Baseline behaviour catalogue** (the "normal" Pillar 2 measures against; art bible B1-B8; parameters are knobs).

| ID | Behaviour | Rule (defaults) | Art ref |
|---|---|---|---|
| C1 | Route walk | Legs between `CrowdRoute` nodes chosen by R13 weights; leg length 10-30 m; pace per R3.1 | B1, B5 |
| C2 | Pause | After each leg: 55 % short U(1, 3) s, 45 % long U(4, 16) s (F4); long pauses face a stall, mural or view when one is within 3 m (head pitched down 10-20° allowed) | B2, B4 |
| C3 | Converse (reciprocal) | An NPC that comes to rest within 150 cm of another resting, perceivable pawn turns to face it with probability `CrowdConverseStartPct` (50 %); an NPC that is faced this way turns back with probability `CrowdConverseReplyPct` (60 %). A conversation exists only while **both** face each other within 60°, and lasts 6-20 s; the initiator turns away after U(2, 5) s if not faced back, whoever the partner is. Both probabilities come from logged player face-back rates and never read who the neighbour is; a player who faces an NPC is treated exactly as an NPC who does. NPCs never walk toward a stationary pawn to start one | B2 + overlay |
| C4 | Group drift | A walker within 2 m heading within 30° may match the neighbour's pace for at most 5 s per leg (no formation, no equal spacing); never formed at t0; **inactive while the interim pace rule holds** (R3.1) | B7 |
| C5 | Yield / side-step | R3.3 | B6 |
| C6 | Turn-in-place | Idle gaze beyond 70° for 0.5 s re-centres the body (SCL R5.5) | B2 |
| C7 | Reversal ("forgot something") | Stop, pivot, start back along the last leg; mean interval `CrowdReversalIntervalSec` (F2) | B4 + pivot |
| C8 | Alley exit / entry | Ambient cycle (R9), drawn like any leg end; walk-in and walk-out on the normal path | B1 |
| C9 | Touch overlay | R4(a); touch length U(`CrowdAmbientTouchMinSec` 0.8, `CrowdAmbientTouchMaxSec` 1.5) s for every pawn (Objectives I-OC1 keeps a tend ≥ 0.5 s longer) | B8 |

**Stationary-stretch watchdog.** Any chain of stationary behaviours (pause, then conversation, then a reaction pause) is budgeted from the start of the visible stationary stretch: when a stretch reaches `CrowdMaxIdleSec` the NPC starts a leg, so no chain exceeds the single-entry maximum (AC-CNB-13).

**CNB-R6. Gaze catalogue** (all through `Look`; the SCL head filter makes every NPC glance look like a human's).

| ID | Gaze | Rule (defaults) |
|---|---|---|
| G1 | Scan | While paused: sweeps of 30-60° yaw every 3-7 s |
| G2 | Glance at a passer-by | Walking or paused: nearest moving pawn within 6 m with line of sight (any kind, never a concealed pawn, R2), hold 0.4-2.5 s, every 4-10 s; while walking, the glance direction obeys the R3.4 motion-parity profile |
| G3 | Look at goods | Facing a stall or prop within 3 m: pitch −10 to −20° (through the crowd AIController's direct control-rotation write, R3), 1-4 s; a contextual hold, not T13 |
| G4 | Salient motion | A run, traversal landing or strike within 15 m with line of sight: glance with probability 0.5 (run, traversal) or per R8 (strike), after a reaction delay drawn from logged player reaction times to the same stimulus (provisional U(0.3, 1.2) s), timed from the stimulus, not from the decision tick |
| G5 | Partner look | Conversing: look at the partner's head 60-80 % of the time; a contextual hold, not T13 |
| G6 | Long hold (red herring) | An independent clock with mean `CrowdHeadHoldIntervalSec` (480 s, 0.125/min) turns the NPC's next G2 glance into a 2.5-6 s hold on that passer-by (T13, F2) |
| G7 | Re-centre | C6 |

**CNB-R7. Red herrings.** For each advantage the NPC rate is set from a target posterior band (F2), never zero for an SCL, IIF or Concealment tell, and never adapted at runtime from observed player behaviour (rates are re-derived offline per playtest build from logged player rates, steps capped at 25 %). Every rate in R16 and F2 is an **accepted** (observed) rate; the sampler draws at F2's compensated rate so the chain-gap guard below does not lower it. Contexts and duration rules:
- **Run**: 2-6 s bouts starting and ending at leg boundaries (a late porter); destinations drawn by R13 weights, never toward courts preferentially; the run ends by ordinary SCL braking.
- **Traversal**: on legs that cross public coping (nav link at a `TraversalPoint`), vault or climb with the F2 probability; climb only where coping is ≥ 1.6 m (temple steps, canal walls).
- **Brace** (real `Prepare`, same 60 s cooldown, same planted stance): the T4 clock decides **when** a brace is due; a due brace is performed at the next flinch trigger within 30 s (a runner passing within 2 m, a traversal landing within 3 m), else as an ambient startle. A trigger alone never causes a brace, so sprinting past a suspect draws a flinch only from the few NPCs that happen to be due (about 1 in 100 at defaults), which makes the "sprint past and watch for a flinch" test powerless. The R8 startle is counted in the same budget (F2). Duration: 55 % to the cap, 45 % released at U(1.0, 3.8) s. Attacking a braced NPC follows Assassination's outcome rule with no controller term (requirement on Assassination).
- **Decoy tending** (`Interact` with `AllowNoEffect`): courts chosen uniformly over **all** courts (live, decoy, used or not, and courts currently being tended), never weighted by the assignment; the NPC takes a free stand point (Objectives `FixtureInteractSideCount`) and picks another court only when every stand point is physically occupied (Objectives change request 6: no avoidance beyond physical space); if its `Interact(Begin)` is refused `Reject(NotNow)` because the fixture filled in the meantime (a fifth Begin, Objectives OC-7/OC-8), it abandons that court and chooses another, exactly as after any rejection (R3). Rate: r_N ≈ 0.0057/min (`CrowdDecoyTendIntervalSec` 10,500 s; Objectives F3 at `T_round` 9 min, reconciliation 2026-10-24, superseding 0.0032). `CrowdDecoyCompletePct` (75 %) hold to completion (`ObjectiveInteractSec`, 3.0 s, exactly like a human completion); the rest release early at U(`CrowdDecoyEarlyReleaseMinSec` 0.8, `CrowdDecoyEarlyReleaseMaxSec` 2.8) s (keys exposed for Objectives' I-OC3). If Objectives' measured `P_short` exceeds 0.90, the first lever is a larger early-release share (lower `CrowdDecoyCompletePct`), never a lower decoy rate (Objectives F3, OQ10).
- **Bush** (`Hide(Enter/Exit)` through Concealment's one path, CON-R14): when a bush entry is due, the NPC enters the nearest bush with an open side within 10 m of its current leg ("stepping into the shade"). The choice never reads occupancy, occupants or player positions: Concealment has no capacity and no occupancy check (CON-R3), any number of pawns may share a bush, and a second entrant into an occupied bush tells an observer nothing (conflict C7, Concealment version). NPCs obey the same `BushReenterCooldownSec` (6 s) and exit-side rule as humans. The NPC's stay is drawn at entry from an **uncapped** log-normal (`CrowdBushDwellMedianSec` 15 s, `CrowdBushDwellSigma` 0.8, mean ≈ 20.7 s): it is the NPC's own choice of when to leave, exactly as a player chooses. There is no dwell cap, no dwell timer and nothing ever times a hider out (locked rule; Concealment AC-CON-12). Target: `P_hide ≤ 1/6` (IVP F5 at k = 1.5), met by r_N = 0.19/min per NPC (F2). NPC bush entries are **context-free**: never timed after strikes, never weighted toward courts or objective routes, never timed to being watched, because context is where the honest Pillar 2 read of a bush entry lives (IVP F5 note).
- **Assassination strike**: NPC rate **0**. The strike is the one confession the locked rules intend ("the assassination animation itself exposes it").
- **Evidence compounds (coin-flip guard)**: an NPC never produces two different Matrix tells (T1-T5, T12, T13) within `CrowdTellChainGapSec` (20 s) of each other. A red-herring draw that falls inside the NPC's gap is discarded and that tell's exponential clock restarts at the gap's end, so no NPC tells pile up just after the gap; F2 compensates the draw rate so accepted rates equal the targets. An R8 reaction that would be a tell (panic run, startle brace) inside the responder's gap becomes a glance. So a single sighting is evidence (F2 bands), while two different tells by one pawn within `CrowdTellChainGapSec` are something only a player produces (AC-CNB-31). The same window value is used everywhere.

**CNB-R8. Reaction to a strike nearby.** Stimulus: `OnActionStarted(Pawn, Attack)` (kill, whiff or reversal alike; CNB never reads the outcome or `VictimKind`). Every unbound NPC with server line of sight from its eye to the strike point within `CrowdStrikeReactRadiusCm` (15 m) draws one reaction after a delay drawn from logged player reaction times (provisional U(0.3, 1.2) s, timed from the stimulus): 40 % none, 30 % glance at the victim or strike point (never tracking the attacker afterwards) 1-3 s, 12 % pause 2-5 s, 7 % reroute away at walk, 3 % short panic run ≤ 3 s, 8 % startle brace (the last two only within 6 m; beyond 6 m, or inside the responder's chain gap (R7), they become glances). Startle braces count against the T4 brace budget and panic runs against the T1 run budget: the ambient T1 and T4 rates are reduced offline by the expected R8 contribution (F2), so the run and brace posteriors do not drift near kill sites. Refractory 8 s per NPC; at most `CrowdStrikeResponderLimit` (8) responders per strike. No gathering, no flight from the district, no screams. Bodies are static obstacles and G2 glance attractors for passers-by. Body lifetime and removal belong to Assassination and are identical for every victim kind.

**CNB-R9. Ambient emergence and replacement** (LSC R5; CPA CP-12; formulas F3).
1. **Ambient cycles.** CNB's `AmbientSchedule` stream generates exit-first cycles at one per `CrowdAmbientCycleIntervalSec` (80 s) per socket, i.e. 13 × 60 / 80 = 9.75 per minute city-wide, 1.5 × the LSC floor of 0.5 per socket per minute: an NPC chosen uniformly among eligible unbound NPCs within 40 m of a socket path takes "leave by the alley" as its next leg, walks in, and is absorbed (`AmbientExit`) once the LSC clear check passes; the matching `AmbientEntry` is scheduled G ~ U(6, 20) s after the exit. If the exiting NPC is watched for `CrowdExitGiveUpSec` (8 s) inside the alley it turns and walks back out (an ordinary alley dawdle; NPCs also dawdle in alleys with mean interval `CrowdAlleyDawdleIntervalSec` per NPC). **Released exile body (Penalty CR-4, PEN-R5):** when a struck-out participant's pawn is released to the crowd, it finishes its strike lockout under the restricted driver (R10) and its next leg is an `AmbientExit` that **takes the next scheduled city-wide exit slot** of this cadence (that slot's originally chosen NPC simply does not leave), so the city's exit rate stays at baseline (Penalty AC-NKP-45); the normal give-up rule applies while it is watched.
2. **Kill.** `OnPawnKilled` (any `VictimKind`) adds one vacancy at t_k (t_k = the victim's scheduled death-write server time, `GetScheduledDeathServerTime`; no effect of a kill, whether vacancy, exit cancellation or reaction, is observable before the death write, and an NPC victim keeps its driver and follower unchanged until then; ADR-0018 §3.4) and cancels one upcoming exit chosen uniformly among the exits scheduled in the next 60 s at all sockets (LSC R5.7 bounds apply). The **replacement** is the first scheduled entry at or after t_k + `CrowdReplacementMinDelaySec` (10 s); if none has served it by `CrowdReplacementMaxDelaySec` (60 s), the next clear socket serves it at the 1 s retry. The look is `DrawReplacement()`, uniform over vacant cards (the victim's card when only one is vacant).
3. **Same for every victim.** The ledger input is the kill time only. A killed informant's body, a killed player's body and an NPC body produce the same vacancy, the same delay distribution and the same socket pool (CPA (b)).

**CNB-R10. Participant-bound pawns driven by AI** (Round R17, Roster R11, Session 25f).
1. **Takeover** on Round's request (Unresponsive or Disconnected): the SCL R4 stop-and-blend handover; the AI holds `Move(current heading, Walk)` at the pace nearest the current speed for `HandoverHoldSec` (0.8 s) (if the pawn is slower than the SCL pivot speed, or the hold heading leaves the navmesh within the hold distance, it holds `Move(0)` instead), never decelerating faster than `CrowdStopDecel` (a derived constant, not a key: the SCL run braking `RunSpeedCmPerSec / RunStopSec` = 600 cm/s²), then runs the **restricted driver**: C1 route walk, C2 pauses, C5 yield, C6 re-centre and G1-G3, G5, G7. It issues **only `Move` and `Look`** (a sink type that cannot carry other commands). Outstanding holds end through the IIF release path. A concealed pawn stays concealed (Round R17); while concealed it is invisible, so `CrowdMaxIdleSec` applies only to visible stretches.
2. **Idle bound.** No visible stationary stretch of a restricted-driver pawn exceeds `CrowdMaxIdleSec` (20 s), the longest stationary stretch any catalogue entry produces (C3 conversation), so a stall is never longer than an ordinary citizen's.
3. **Hand-back** (rejoin or responsive): the driver issues `Move(0)` and detaches at the start of a server frame (never during a traversal); the human's first command shapes from the current velocity (SCL R4).
4. **`ReleaseToCrowd`** (grace expiry, informant disconnect, strike removal with a surviving pawn): the pawn joins the full catalogue at its next leg boundary; nothing visible changes at the release instant. **Strike removal** (Penalty PEN-R4.2, PEN-R5): the pawn is released in the exile's own Roster call; it finishes its strike lockout (`AttackAttemptLockoutSec`) under the restricted driver, then takes the R9.1 exile exit (the next scheduled exit slot); the participant adopts a different pawn at `T_due` (PEN-R9).
5. **Handover order (one rule for R10 and R11).** Round's drain commits the change and bumps the pawn's **control epoch**; from that instant the pawn's command sink rejects every command carrying the old epoch (pending async path callbacks, debounced Ends, StateTree tasks and follower ticks of the previous driver). The possession change itself happens at the start of the next server frame, never during a traversal. Until the new driver's first command arrives (a human's first `ServerMove`) or `HandoverHoldSec` passes, SCL holds the last `Move`, so the pawn neither brakes nor jumps at the handover (SCL change request; AC-CNB-23, 24).

**CNB-R11. Informant takeover supply** (decided rule C1, LSC R5b, CPA CP-12b). CNB publishes, per unbound NPC pawn, a server-only eligibility reason ∈ {`Eligible`, `Tending`, `Bracing`, `Hiding`, `Emerging`, `Exiting`, `Reacting`, `Conversing`, `Bound`, `AlleyExit`}; only `Eligible` pawns are candidates. `AlleyExit` covers any pawn whose current leg is an `AmbientExit`, including a released exile body in pending exit, so a participant can never re-adopt its own released body (Penalty PEN-R5, LSC R5b.2). LSC `PickRespawnPawn` applies the region and unseen tests and the uniform draw; CNB's `TryReleaseNpc(Pawn)` re-checks atomically in the commit tick, bumps the control epoch, aborts the pawn's StateTree, removes it from the crowd ledger and leaves the pawn standing or walking as it was for the SCL AI → player handover in the order of R10.5. Eligibility never reads appearance; a pawn in a bush or a bush transition is never eligible (`Hiding`; Concealment `IsInBushOrTransition`).

**CNB-R12. Phase gating.** Before `OnRoundActive` every NPC is `Dormant` (no commands; Round R6). At `OnRoundActive` every NPC and AI-driven pawn starts in the same server step, each from a random per-pawn phase (first pause or leg drawn from its stream) and a per-pawn decision-tick phase offset (so the 4 Hz decisions spread evenly across frames), so the crowd does not start in unison. At `Ended`, in Round's **round-end step** (Round R9: the latch step itself for an objective win, or the deciding strike's death write when a strike decides the round; between the latch and that step the crowd keeps running unchanged, only its discrete commands and items return `Rejected_Latched`, silently to the AI's null sink; cross-review S-B1 2026-10-24), the Director raises **Halt**: it latches every AI command sink, clears `Move` and stops each StateTree, so no StateTree or follower tick in the same frame can re-issue motion. Crowd no longer ends outstanding holds itself: every pawn's holds are cancelled by Round's round-end step (ADR-0002 §8.4 (g)), and every pawn, AI- or human-driven, brakes by the factory's kind-blind stop-and-blend path at SCL baseline (≤ `RunStopSec`; ADR-0010 §4.5, ADR-0016 §7); ambient cycles, replacements and reactions stop; open cycles are abandoned without emergence. At `Reset` CNB releases nothing (Round destroys the world).

**CNB-R13. Density model** (F1). Destination and start-cell weights depend only on area type and geometry (`Causeway`, `Market`, `Temple`, `Residential`; a court inherits its host area's type) and on observed NPC density from public positions, never on the site assignment, zone roles or player positions. Route node capacity counts pawns of any kind (a person does not walk into an occupied spot).

**CNB-R14. Random streams.** Every behavioural draw is **counter-based**: `H(seed, ordinal, decisionType, decisionIndex)`, with the seed from `GetServerSubSeed(CrowdBehaviour, LayoutAttempt)` and the pawn's server-only spawn ordinal, so a pawn's k-th draw of a given decision type is fixed no matter how many draws of other types the world caused (yield, conversation and F1 weights are conditional). Schedule draws are counter-based too, in Session F5's counter-as-Attempt shape: each schedule event draws from its own sub-seed `GetServerSubSeed(AmbientSchedule, counter)` with a short secret stream for the few draws inside that event (ADR-0006 §4). Streams are never `FMath::Rand`, are independent of kills (a kill shifts no other pawn's draws), and live in the Crowd Director keyed by ordinal (controllers are destroyed on possession), so they travel with a pawn through takeover and release; the SCL factory assigns an ordinal to every pawn, participants included.

**CNB-R15. Architecture and cost** (decided by ADR-0016: StateTree per NPC plus a follower that emits `Move`/`Look` by **input emulation**, §4; provisional pending the 50-character spike). A server-only **Crowd Director** subsystem (population ledger, ambient schedule, density, stimulus fan-out, Halt) plus one **StateTree** per NPC on the AIController, throttled to ~4 Hz decisions with event interrupts (strike, blocked path, Halt), plus a per-pawn **follower** at the steering rate that emits `Move`/`Look`. Navigation is used for path queries only (async, ≤ 2 starts per frame); neighbour queries use the Director's 2 m spatial hash. Mass and `UPathFollowingComponent` driving are rejected (Mass duplicates 50 `ACharacter` pawns; path following would bypass the IIF): the AIController's path-following component does not tick and no StateTree `MoveTo` or `AITask_MoveTo` is used. The crowd AIController overrides `UpdateControlRotation` to write the `Look` yaw and pitch directly (R3). The navmesh is frozen at `OnLayoutReady` for the round (no runtime rebuild; the prototype rebuilds at runtime: change request to the Generator), and a path the follower holds is re-queried if the navigation system reports it invalid. Budget: server AI + nav ≤ 1.5 ms per frame on the listen-server host (F5). Any cut is one global value for every AI-driven pawn and never by distance, visibility or viewer, and no Crowd LOD or cut ever changes a pawn's replication settings (net frequency, priority, relevancy; Concealment CON-R14 wire parity); reaction latencies are sampled timers, so a cut never changes behaviour statistics. **No cut lowers `NpcCount`** without a creative-director review (fewer NPCs shrink the Pillar 1 candidate pool). [verified 5.8.4 by ai-programmer: `UStateTreeComponent::ScheduleTickFrame`, `FStateTreeScheduledTick::MakeSleep`, `UStateTreeAIComponent`; `FindPathAsync` runs on the task graph, so most path cost is worker-thread time.]

**CNB-R16. Baseline and Tell Matrix** (the authoritative list; rates are F2 defaults; LR = likelihood ratio player : NPC per head).

| # | Advantage | Visible deviation | NPCs do it? (red herring, default rate per NPC) | Target P(player \| seen) | Owner |
|---|---|---|---|---|---|
| T1 | Run | 450 cm/s, lean, wide gait | Yes: late porter, 0.185/min | 0.5-0.65 | SCL + CNB |
| T2 | Traversal (vault, climb) | hands to coping, arc off the ground | Yes: 0.052 vault, 0.022 climb /min | 0.5-0.65 | SCL + CNB |
| T3 | Objective interaction | 3 s two-handed pose at a court fixture | Yes: decoy tending, 0.0057/min (all courts uniformly, occupied courts included; a full fixture is refused and another court chosen) | 0.65-0.75 for full-length tends (the strongest honest read, P4; Objectives F3 `P_full`); early releases reported separately (`P_short`, alarm > 0.90) | Objectives + IIF + CNB (AC-OC-21, AC-OC-24) |
| T4 | Brace / prepare | planted, compressed stance up to 4 s | Yes: flinch and startle, 0.025/min; a braced NPC reverses an attacker exactly as a braced player does | 0.5-0.7 | Assassination + IIF + CNB (AC-ASN-26) |
| T5 | Bush enter / leave | step into the planter, leaves view | Yes (red herring): shade-seeking, 0.19/min, occupancy-blind; NPC stay uncapped log-normal (median 15 s, σ 0.8), exits by the same side rule | ≤ 1/6 (IVP F5 at k = 1.5; 0.165 at r_P 0.3) | Concealment + IVP + CNB (AC-CON-15, AC-IVP-34) |
| T6 | Assassination strike | lunge and contact | **No** (0): the intended confession | 1.0 | Assassination (AC-ASN-27) |
| T7 | Informant restrictions | walk only, no traversal or actions: no deviation from the walk baseline; never seen running or vaulting | n/a (informants look like walkers) | n/a | Penalty & Informants |
| T8 | Alley emergence / exit | walks out of or into an alley | Yes, NPC-only absorption, ≥ 0.5 per socket per min each way; players may walk in and out of alleys freely | ≈ 0 (no player emerges) | LSC + CNB |
| T9 | Connection stall | 4 s+ stationary body | Yes: ≥ 10 % of NPCs in a stationary stretch already ≥ 4 s old at any moment (F4) | ≈ base rate | Session + Round + CNB |
| T10 | AI handoff (takeover / hand-back) | heading held 0.8 s at walk, then a crowd behaviour | Yes: straight legs ≥ 0.8 s are the norm | ≈ base rate | SCL + Round + CNB |
| T11 | Sudden stop | impossible for every pawn (SCL R5.3) | n/a | n/a | SCL |
| T12 | Abrupt reversal | stop-pivot-start | Yes: "forgot something", 0.19/min | ≈ 0.5 | SCL + CNB |
| T13 | Head hold > 2.5 s on a passer-by | gaze lingers on a pawn (holds on goods or a conversation partner are context, not T13) | Yes: G6, 0.125/min | ≈ 0.5 | SCL + CNB |
| T14 | Loiter without context | long stand in the open | Yes: long pauses with a context within 3 m; ≤ `CrowdMaxIdleSec`. A visible stand longer than `CrowdMaxIdleSec` is a player's choice: an intended P2 tell with no red herring | ≈ base rate up to 20 s; 1.0 beyond | CNB |
| T15 | Pawn look, replacement look, informant look | none (deck, CP-12) | n/a: never a tell | base rate | CPA |
| T16 | Slow walk (pace 0.8 / 0.9) | walking below full pace | Interim: **No** (R3.1: NPCs walk at 1.0 until a keyboard walk-slower modifier exists); a slow walker is a pad player who chose it | 1.0 (interim) | SCL + IIF + CNB |
| T17 | Camera mode / camera toggle (1P or 3P, shoulder swap) | none: the camera is client-local and changes nothing on the pawn (Camera CS-R2) | n/a: not an advantage and not visible | base rate | Camera |

### States and Transitions

One top-level StateTree state per unbound NPC (server only, never replicated; observers see only the pawn's commands' results). "Bound" is not a state but a driver flag (R10) selecting the restricted subset {Routing, Pausing, Halted}.

| State | Entered when | Exits to | Commands issued | Takeover-eligible |
|---|---|---|---|---|
| `Dormant` | spawned before `OnRoundActive` | `Routing`/`Pausing` at `OnRoundActive` (random phase) | none | no |
| `Emerging` | `AmbientEntry`/`Replacement` spawn at an origin cell | `Routing` once past the socket's witness set | Move, Look | no |
| `Routing` | a leg starts | `Pausing`, `Conversing`, `Tending`, `Hiding`, `Exiting`, `Bracing` (flinch), `Reacting` | Move (Walk or Run), Look, Traverse | yes |
| `Pausing` | leg end, reversal pivot, reaction pause | `Routing`, `Conversing`, `Reacting` | Move(0), Look | yes |
| `Conversing` | C3 partner condition | `Routing`/`Pausing` after 6-20 s or when the partner leaves | Look | no |
| `Tending` | decoy court chosen and reached | `Routing` on completion or early release | Interact(Begin/End), Look | no |
| `Bracing` | flinch or startle | previous state on release or cap | Prepare(Begin/End), Move (turn only), Look | no |
| `Hiding` | bush chosen (occupancy-blind) and reached | `Routing` when its drawn, uncapped stay ends (R7) | Hide(Enter/Exit) | no (in a bush or transition) |
| `Exiting` | ambient cycle exit assigned | absorbed (pawn removed) or `Routing` after `CrowdExitGiveUpSec` | Move, Look | no |
| `Reacting` | strike stimulus (R8) | `Routing`, `Pausing` or `Bracing` | Move, Look, Prepare | no |
| `Halted` | Halt at `Ended` | none (terminal) | sink latched, `Move` cleared, tree stopped (holds are cancelled by Round's round-end step; braking is the factory stop path, R12) | no |

Interrupt priority: `Halted` > `Reacting` > the rest. A handover or release (R10, R11) is committed in Round's drain (epoch bump) and takes effect at the start of the next server frame, never during a traversal (R10.5, SCL R4). A visible stationary stretch across `Pausing`, `Conversing` and `Reacting` is capped by the stationary-stretch watchdog (R5).

### Interactions with Other Systems

| System | Flows in | Flows out | Interface owner |
|---|---|---|---|
| Round & Match | `OnRoundActive`, Halt at `Ended`, hand-over / hand-back requests, `ReleaseToCrowd` | AI started; pawns halted; restricted driver attached/detached | Round (when), CNB (how) |
| Character Pool | `NpcAppearances`, `DrawReplacement`, `VacateCard(Pawn)`, `TakeNpcCard` (via Penalty) | spawn-tick draws; vacancies through LSC | CPA |
| Shared Character & Locomotion | factory, movement execution, handover R4, `IsRunning`, `IsInTraversal` | `Move`/`Look`/`Traverse` via the IIF | SCL |
| Input & Interaction | R1 entry points, `OnActionStarted/Cancelled/Completed` | AI commands (null reply sink) | IIF |
| Level Socket Contract | `CrowdRoute`, area types, start pool, sockets, clear check, `PickRespawnPawn` | `RequestEmergence(...)`, ambient schedule, `TryReleaseNpc` | LSC (geometry, selection), CNB (schedule) |
| Visibility Policy | `IsConcealedFrom`; rule "never read `IsPlayer`"; F5 at k = 1.5 | NPC bush rate (F5 input); NPC stay and exit distributions recorded in T5 (AC-IVP-34) | IVP |
| Assassination | `OnPawnKilled`, strike `OnActionStarted(Attack)`, brace outcome rule | reactions; vacancy | Assassination |
| Concealment | Hide path and its legality replies (no occupancy data reaches CNB), `BushReenterCooldownSec`, exit-side rule, `IsInBushOrTransition` | NPC Hide commands | Concealment |
| Objectives | `AllowNoEffect` decoy predicate | NPC Interact at every court | Objectives |
| Penalty & Informants | `PickRespawnPawn` call (through LSC) | eligibility flags; released pawn | Penalty |
| Session & Travel | `GetServerSubSeed` | — | Session |
| Tuning Data & Config | `NpcCount`, `Crowd*` keys after `OnTuningReady` | — | TDC |

## Formulas

*systems-designer consulted (F1-F4, F6); ai-programmer (F5). Every rate is a placeholder re-derived from playtest logs; none has a controller-kind term. A = walkable area per LSC V-AREA (default graybox 8,800 m², N_E = 13 sockets).*

### F1. Density model

The crowd_density formula is defined as:

`ρ_t = N · w_t / Σ_t' (w_t' · A_t')` (start placement and target density per area type t); destination weight of node n: `W_n = w_t(n) · a_n · clamp((ρ_t / max(ρ_obs,t, 0.25 · ρ_t))^0.5, 0.5, 2)` (the floor stops an empty district dividing by zero; the clamp binds first anyway); node capacity `cap_n = clamp(floor(L_n / 100 cm), 1, 4)`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| N | int | 35-60 | data file (`NpcCount`, 40) at start placement; the live NPC count (R1) for destination weights | NPC count |
| w_t | float | 0.5-1.6 | data file (`CrowdAreaWeightCausewayMult` 1.4, `...MarketMult` 1.6, `...TempleMult` 0.8, `...ResidentialMult` 0.6) | attractor weight; a court inherits its host's weight |
| A_t | float (m²) | calculated | layout | walkable area of type t |
| a_n | float (m²) | calculated | layout | node catchment area |
| ρ_obs,t | float (/m²) | calculated | CNB ledger | current density of **CNB-driven** pawns in type t (its own ledger; no Roster read) |
| L_n | float (cm) | 100-600 | layout | node frontage |

**Output Range:** density ratio to the map mean 0.55-1.47 at defaults, under the D6 target (1.5 ×) and the LSC zone cap (1.8 ×). The ratio depends on all four weights and the area shares, so the weight ranges alone do not guarantee it (Market 1.6 with the others at 0.5 gives 2.06 ×): the layout load computes `max_t ρ_t / ρ̄` and rejects a preset above 1.5 (cross-field check, TDC change request). The relaxation term returns a disturbed district to target in about 60-90 s.
**Example:** area shares 0.25 / 0.25 / 0.20 / 0.30 of 8,800 m² give Σ w·s = 1.09; NPCs per type 12.8 / 14.7 / 5.9 / 6.6 (sum 40); densities 0.0058 / 0.0067 / 0.0033 / 0.0025 per m² against a mean of 0.00455 (Market 1.47 ×). **Blindness proof:** the inputs are {N, type map, geometry, own ledger}; permuting `FSiteAssignment` over 50 fixed layouts must give bit-identical outputs (AC-CNB-05).

### F2. Red-herring rate per tell

The red_herring_rate formula is defined as:

`r_N = r_P · n_P · (1 − P*) / (P* · n_N)` ; `LR = r_P / r_N` ; posterior for one sighting `P = n_P · r_P / (n_P · r_P + n_N · r_N)` ; robustness: both band edges hold only if `r_P,hi / r_P,lo ≤ [P_hi (1 − P_lo)] / [P_lo (1 − P_hi)]` ; **chain-gap compensation** (R7): the sampler's draw rate for tell j is `a_j = r_N,j / (1 − (Λ − r_N,j) · T_gap)`, where `Λ = Σ r_N` over T1-T5, T12, T13 (accepted rates) and `T_gap` = `CrowdTellChainGapSec` / 60; valid only while `(Λ − r_N,j) · T_gap ≤ 0.5` (cross-field check)

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| r_P | float (/min per player) | ≥ 0 | measured in playtest (provisional defaults below) | how often a non-ally active player shows the tell |
| n_P | int | 1-5 | `TeamSize` | non-ally active players |
| n_N | int | 35-60 | `NpcCount` | NPCs |
| P* | float | 0.11-0.9 | data file (per-tell target, R16; the bush target is IVP F5's `k · b`) | target posterior that a pawn seen doing the tell is a player |
| r_N | float (/min per NPC) | > 0 (floor: 25 % of the table default for every SCL, IIF or Concealment tell, even if a measured r_P is 0) | calculated → `Crowd<Tell>IntervalSec` = 60 / r_N | NPC red-herring rate (accepted) |
| Λ, T_gap | float (/min), float (min) | 0.5-1.2, 0.17-2 | calculated; `CrowdTellChainGapSec` / 60 | total accepted tell rate per NPC; chain gap |
| a_j | float (/min per NPC) | ≥ r_N,j | calculated (never a key) | draw rate before the chain-gap guard |

**Output Range (defaults):**
| Tell | r_P | P* | r_N (/min/NPC) | City-wide NPC events /min | LR |
|---|---|---|---|---|---|
| Run | 2.0 | 0.575 | 0.185 | 7.4 | 10.8 |
| Vault / climb | 0.8 | 0.575 | 0.052 / 0.022 | 2.1 / 0.9 | 10.8 |
| Brace (flinch + startle) | 0.3 | 0.60 | 0.025 | 1.0 | 12 |
| Decoy tending (all 14 courts) | 0.08 full-length (team `R_full` 0.40, Objectives F3 at `T_round` 9 min) | 0.70 (`P_full`) | 0.0057 (computed by Objectives F3 over full-length tends, completion share c = 0.75) | 0.23 | 18.7 (full-length) |
| Bush | 0.3 | ≤ 1/6 (shipped 0.165) | 0.19 | 7.6 | 1.58 |
| Reversal | 1.5 | 0.50 | 0.19 | 7.5 | 8 |
| Head hold > 2.5 s | 1.0 | 0.50 | 0.125 | 5.0 | 8 |

**Example:** run: `r_N = 2.0 · 5 · 0.425 / (0.575 · 40) = 0.185` per NPC per minute (7.4 NPC runs a minute city-wide against 10 by the 5 opponents); one sighted run moves a pawn from the 1-in-9 prior to 57 %, evidence but not proof. Robustness for the 0.5-0.65 band: the measured r_P spread must stay within 1.86 ×, otherwise narrow the band or collect more data. With `TeamSize` < 5, scale each r_N by `TeamSize / 5`; with `NpcCount` ≠ 40, scale by `40 / NpcCount`, so the city-wide NPC event rate stays constant. **The bush row is exempt from both scalings**: its target `k · b` moves with b = n_P / (n_P + n_N), so r_N is recomputed from IVP F5 at the live n_P and n_N (r_N / r_P = 0.625 at 40 NPCs, 0.64 at 60; per-NPC rate roughly constant). **Bush:** the target is IVP F5 at k = 1.5 (`P_hide ≤ 1/6`; the Policy's ceiling, ruling A, conflict C7); Crowd's earlier request for k = 2.0 is denied, and the gap is closed by raising r_N from 0.15 to 0.19/min (Concealment F2's recommendation; r_N,req = 0.1875 at r_P 0.3). A blind sweeper striking a bush it saw someone enter hits a player with P ≈ 0.165 per strike (about 42 % within three strikes, about 5 NPC kills per player found), so a sweep is a P3 gamble, not a free check. **Decoy row (reconciliation 2026-10-24):** the earlier 0.0032 came from a 12-min round that Objectives F1 contradicts; Objectives F3 now bands full-length tends at `T_round` 9 min, `r_N = R_full·(1 − P_full) / (N_npc·c·P_full) = 0.40 · 0.30 / (40 · 0.75 · 0.70) = 0.0057`. **Chain-gap compensation example:** Λ = 0.795/min, T_gap = 1/3 min; bush `a = 0.19 / (1 − 0.605 / 3) = 0.238`/min, run `a = 0.185 / (1 − 0.610 / 3) = 0.232`/min; without it the accepted bush rate would fall to about 0.152 and P_hide to 0.198 (fails F5). **R8 share of the T1 and T4 budgets:** the ambient rate is `r_amb = max(0, r_N − s̄ · m̄ · p_R / n_N)`, with s̄ the logged strikes per minute, m̄ the mean responders within 6 m per strike and p_R the R8 share (3 % panic run, 8 % startle brace); computed offline per playtest build (never adapted at runtime). Example: one strike every 2 min with 3 responders within 6 m gives 0.5 · 3 · 0.08 / 40 = 0.003 startle braces per NPC per minute, so the ambient brace rate is 0.022.

### F3. Ambient cycles and replacement delay

The replacement_delay formula is defined as:

`λ = k_s · N_E / 60` (cycles per second, exit-first, entry after `G ~ U(G_lo, G_hi)`); replacement `D = d_min + W`, `W ~ Exp(λ)` for one vacancy (`Gamma(j, λ)` for the j-th queued vacancy), served by the next clear socket if `D > d_max`; mean absent NPCs `V̄ = λ · E[G]`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| k_s | float (/socket/min) | 0.67-1.5 | data file (60 / `CrowdAmbientCycleIntervalSec`, 80 s → 0.75; hard 40-90 s) | cycles per socket per minute. Floor: `k_s · (1 − EmergenceExitCancelMaxPct / 100) ≥ 0.5` (LSC floor still met after kill cancels: 0.75 · 0.75 = 0.56 at defaults; needs interval ≤ 90 s). Ceiling: 2 · k_s ≤ 3 events per socket-minute (20 s socket cooldown; interval ≥ 40 s) |
| N_E | int | 13-16 | layout (LSC D5) | emergence sockets |
| G_lo, G_hi | float (s) | 4-40 | data file (`CrowdCycleGapMinSec` 6, `CrowdCycleGapMaxSec` 20) | exit-to-entry gap |
| d_min | float (s) | 3-30 (safe 8-15) | data file (`CrowdReplacementMinDelaySec`, 10) | no replacement before this |
| d_max | float (s) | 30-180 (safe 45-90) | data file (`CrowdReplacementMaxDelaySec`, 60) | fallback: next clear socket |
| D | float (s) | d_min-d_max | calculated | kill to replacement entry |

**Output Range:** λ = 0.1625/s (9.75 entries and 9.75 exits a minute, 1.5 × the LSC floor); D mean 10 + 6.2 = 16.2 s, p95 = 10 + ln 20 / 0.1625 = 28.4 s (second queued vacancy p95 ≈ 39 s); V̄ = 0.1625 · 13 = 2.1 NPCs absent on average; exit-cancel capacity 0.25 · 9.75 = 2.4 kills/min before LSC's deficit rule binds; socket use 1.5 per socket-minute against a 3/min ceiling from the 20 s cooldown (50 % utilisation, check in graybox).
**Example:** an NPC is killed at t = 100 s; entries are scheduled at 104, 113 and 131 s; the first at or after 110 s is 113 s, so D = 13 s, the card is drawn uniformly from the vacant set, and the exit scheduled at 140 s at another socket is cancelled. A player killed at t = 100 s produces exactly the same D: the ledger reads only t_k.

### F4. Idle-pause share (connection-stall cover)

The pause_share formula is defined as:

`f = E[(P − P_th)⁺] / (W̄ + E[P])` (share of NPCs whose current stationary stretch is **already** ≥ P_th old, the cover a stalled player actually needs) ; regulator: while fewer than `ceil(f_min · N_live)` NPCs are in a stationary stretch already ≥ P_th old, the long-pause share is raised to `min(2 · p_long, 0.9)`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| P_th | float (s) | 4 | calculated (`SessionSoftTimeoutSec` + 1) | stall-cover threshold |
| W̄ | float (s) | 8-30 | calculated (C1 leg length / pace; ≈ 16) | mean walk segment |
| p_long | float | 0.1-0.6 | data file (`CrowdLongPausePct`, 45) | share of long pauses |
| short / long | float (s) | U(1, 3) / U(`CrowdIdleMinSec` 4, `CrowdIdleMaxSec` 16) | data file | pause lengths |
| f_min | float | 0.10 | data file (`CrowdStallCoverMinPct`, 10) | Session 25f floor |

**Output Range:** E[(P − 4)⁺] = 0.45 · 6 = 2.7 s per cycle, E[P] = 0.55 · 2 + 0.45 · 10 = 5.6 s, so f = 2.7 / (16 + 5.6) = 12.5 % at defaults before conversations (C3 stretches older than 4 s add to it); at the previous defaults (35 %, U(4, 14)) f was only 8.6 %, below the floor. The regulator reads NPC state only (no viewer or player position). The instantaneous count is about Binomial(38, 0.125), so without the regulator P(count < 4) ≈ 28 %; the regulator target is < 2 % of samples, verified by simulation in AC-CNB-12. Total standing share (any pause) is 5.6 / 21.6 ≈ 26 %.
**Example:** 40 NPCs, 2 absent in cycles: 38 live, about 4.75 in a stationary stretch already ≥ 4 s old; a player whose connection stalls for 4 s is one still body among about six.

### F5. Server AI cost

The ai_cost formula is defined as:

`T_frame = [N·f_d·c_d + N_m·f_s·(c_f + c_n) + Q·c_p + R·c_r + N·f_g·c_g + N·FPS·c_t + T_dir + T_disp] / FPS` ≤ 1.5 ms (Crowd's own game-thread cost; the server movement simulation of AI-moved pawns is SCL movement cost, budgeted by SCL (change request) and measured alongside in AC-CNB-26)

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| N | int | ≤ 70 | runtime | AI-driven pawns (`NpcCount` up to 60 + up to 10 bound) |
| c_t | µs | 2 | estimate | per-pawn per-frame tick overhead (AIController, sleeping StateTree, follower held intent) |
| f_d, c_d | Hz, µs | 2-6, 25 | `CrowdDecisionPerSec` (4); estimate | StateTree decisions |
| N_m, f_s, c_f, c_n | int, Hz, µs, µs | 40, 15-30, 8, 6 | `CrowdSteerPerSec` (30); estimates | moving pawns, steering rate, follow and neighbour cost |
| Q, c_p | /s, µs | 3, 150 | runtime; estimate; cap `CrowdPathStartsPerFrameLimit` (2) | async path queries |
| R, c_r | /s, µs | 10, 10 | runtime; cap 8 per frame | line-of-sight rays (stimulus, gaze) |
| f_g, c_g | Hz, µs | 30, 2 | estimate | gaze target update |
| T_dir, T_disp | ms/s | 3, 3 | estimate | Director, command dispatch (about 3,000 `Move`/`Look` calls per second at ~1 µs) |
| FPS | Hz | 30-60 | host frame rate | |

**Output Range:** ≈ 37 ms per second of server time at 50 pawns → 0.62 ms/frame average at 60 fps, ≈ 1.1 ms peak (a strike burst of 30 rays spread over 3 frames plus 2 path starts; decisions are phase-offset per pawn (R12), so 50 × 25 µs never lands in one frame); 1.24 ms average at a 30 fps host. At 70 pawns ≈ 51 ms/s at 60 fps (0.85 ms) and ≈ 47 ms/s at 30 fps (1.56 ms, tick overhead halves) (**over budget**: the cut order engages). The prototype's measured marginal game-thread cost of about 0.19 ms per character (all systems, RTX 5090-class host) makes the whole-game host budget the larger risk (OQ10, technical-director). **Cut order if over budget** (each one global value): steering 30 → 20 Hz; neighbour candidate cap; decisions 4 → 3 Hz; coarse waypoint A* on the route graph instead of navmesh queries; defer stimulus rays (reaction delays are sampled timers, so behaviour statistics do not change).
**Example:** 50 · 4 · 25 µs = 5.0 ms/s; 40 · 30 · 14 µs = 16.8; paths 0.5 (≈ 0 with pre-baked route-edge paths); rays 0.1; gaze 50 · 30 · 2 µs = 3.0; tick overhead 50 · 60 · 2 µs = 6.0; Director 3; dispatch 3 → 37.4 ms/s / 60 = 0.62 ms. **Estimates only: provisional pending the 50-character spike** (and a 70-pawn run at 30 fps, AC-CNB-26). The cut order never lowers `NpcCount` (R15).

### F6. Behaviour classifier gate and pipeline parity

The classifier_gate formula is defined as:

`BA_j = max_θ,dir (TPR + TNR) / 2` per feature j (θ fit on training matches, evaluated on held-out matches); `SE = 0.25 · sqrt(1/n_P + 1/n_N)` (the unclustered lower bound; the gate uses the larger of this and a cluster-bootstrap SE over participants); pass iff for every j: `BA_j ≤ BA_max` and `BA_j + 1.645 · SE ≤ BA_upper`. Pipeline parity per feature: pass iff `|d| ≤ 0.1`, `σ²_bot / σ²_ai ∈ [0.8, 1.25]`, `KS D ≤ 0.1`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| j | feature | 9 | test config | mean speed, speed variance, pause rate, pause length, heading-change rate, head-yaw rate, glance hold length, path straightness, full-pace share, each over a 30 s window of ordinary walking (no tell in the window) |
| n_P, n_N | int | ≥ 600 / ≥ 2400 | test config | human windows (≥ 30 participants, cluster bootstrap) / NPC windows |
| BA_max, BA_upper | float | 0.55 / 0.58 | test constant | matches the Policy's F2 pair |
| d, KS D | float | — | calculated | Cohen's d and Kolmogorov-Smirnov distance, n ≥ 1,236 per class (Policy F7 power) |

**Output Range:** pass/fail. **Example:** SE = 0.25 · sqrt(1/600 + 1/2400) = 0.0114; BA = 0.54 on pause length → upper bound 0.559 ≤ 0.58: pass; BA = 0.57 on full-pace share fails, and the C1 pace weights are retuned. The parity test feeds the same CNB command streams through (a) the AIController and (b) a bot on a test client's PlayerController using Enhanced Input (keys and mouse), isolating pipeline differences; the BA gate measures behaviour against recorded human play.

## Edge Cases

**Population and replacement**
- **If more than `EmergenceDeficitMaxCount` (3) replacements are pending**: the deficit is held at 3; the next replacement takes the next entry slot with its exit cancel waived, and `EmergenceRateBoundExceeded` is logged. Looks stay uniform over vacant cards.
- **If every socket is watched or blocked**: entries retry every `EmergenceRetrySec` (1 s) and the clear check is never relaxed; exiting NPCs re-route to another socket or give up after `CrowdExitGiveUpSec` and walk back out; after `CrowdReplacementMaxDelaySec` the first socket that clears serves the oldest vacancy.
- **If two pawns are killed in the same server tick**: Round's total order sequences them; each adds a vacancy and cancels a distinct upcoming exit; the replacements take the first and second eligible entries.
- **If a scheduled entry fires with no vacancy** (only possible through a ledger bug): `DrawReplacement` returns `Error_NoVacancy`, nothing spawns, `LedgerMismatch` is logged and the cycle is dropped.
- **If an exiting NPC is killed inside the alley**: it is an ordinary kill (vacancy + replacement); its pending exit is void and is not counted as a cancel.
- **If `NpcCount` is at a hard extreme**: values below the LSC/TDC minimum make the preset invalid before the round; at 60 the schedule and floor are unchanged and per-NPC red-herring rates scale by 40/`NpcCount` (F2).

**Reactions and red herrings**
- **If a strike is a whiff, a reversal or a kill of a concealed pawn**: the same stimulus and reaction table apply (R8); reactions never differ by outcome.
- **If an NPC in a bush is blind-attacked**: it dies as an ordinary NPC kill (attacker gains a strike); its planned stay is discarded; the replacement follows F3.
- **If a decoy court's fixture is being tended** (by anyone): the NPC still may choose that court and takes a free stand point; only when every stand point is physically occupied does it choose another court. It never waits within 2 m and never interrupts (Objectives change request 6). If a stand point fills between its choice and its `Interact(Begin)`, the Begin is refused `Reject(NotNow)` (a fifth tender, Objectives OC-8) exactly as for a human, and the NPC chooses another court (R7).
- **If a red-herring draw falls inside the NPC's chain gap**: it is discarded and that tell's clock restarts at the gap's end (R7); F2's compensated draw rate keeps the accepted rate on target.
- **If an NPC's flinch brace is attacked**: the outcome is Assassination's braced-defender rule with no controller term (the attacker is reversed exactly as against a braced player); this is intended (P3).
- **If a bush already holds pawns**: irrelevant to the NPC. Bush choice never reads occupancy, occupants or player positions (CON-R3 has no capacity and no occupancy check; CON-R14), any number of pawns may share a bush, and an observer who saw X enter learns nothing about the next entrant Y (conflict C7, Concealment version). Occupants are never steering obstacles or glance targets (R2).
- **If an NPC has been concealed for a long time**: nothing happens; there is no dwell cap and no timer on hiders. The NPC leaves when its drawn stay ends; the log-normal tail means some NPC stays exceed a minute, so a long stay is not proof of a player.
- **If many strikes occur within seconds in one area**: the per-NPC 8 s refractory and the 8-responder cap bound the reaction; no cascade, no crowd-wide panic.

**Participant-bound pawns and takeover**
- **If a participant disconnects while running, tending or braced**: SCL R4 blend; holds end by the IIF release path (a brace's cooldown is already spent); the restricted driver starts after `HandoverHoldSec`.
- **If a participant disconnects while concealed**: the pawn stays concealed (Round R17); the restricted driver issues no Hide(Exit); at `ReleaseToCrowd` it becomes a concealed NPC whose remaining stay is drawn from the NPC stay distribution conditioned on the time already concealed (uncapped), and it then exits by an ordinary `Hide(Exit)` (Concealment AC-CON-16).
- **If a participant disconnects mid-traversal**: the takeover waits for the landing (SCL R4).
- **If a rejoin arrives during the 0.8 s hold**: hand-back at the next server frame; the human's first input shapes from the current velocity.
- **If an async path query, a debounced End or a StateTree task completes after a handover was committed**: it carries the old control epoch and the sink drops it (R10.5); nothing reaches the pawn.
- **If the human's first `ServerMove` is late after possession**: SCL holds the last `Move` until it arrives or `HandoverHoldSec` passes, so the pawn does not brake at the handover (R10.5).
- **If an informant takeover candidate is killed, starts a behaviour or becomes seen in the commit tick**: `TryReleaseNpc` fails its atomic re-check; Penalty, its only caller, drops the candidate and re-picks through LSC `PickRespawnPawn` with that pawn excluded (one pick per slot, next-frame retry; ADR-0019 §4).
- **If a struck-out participant's pawn survives**: the pawn is released to the crowd in the exile's Roster call (Penalty PEN-R4.2); the participant adopts a different pawn at `T_due` (PEN-R9); the released pawn finishes its strike lockout under the restricted driver and then takes its exit in the next scheduled exit slot (R9.1, R10.4; Penalty CR-4, reconciliation 2026-10-24). It reports `AlleyExit` until absorbed, so it is never a takeover candidate (R11).
- **If a pawn is released during `Ended`**: it stays `Halted`.

**Phase, network and host**
- **If `Ended` latches while an NPC is mid-run, mid-traversal or mid-emergence**: braking by SCL baseline; a traversal lands first; an emerging NPC halts where it is (it is already out of the origin cell or still occluded); no cycle completes.
- **If the listen-server host hitches** (frame > 33 ms): the follower and timers use elapsed time, not frame counts; path starts stay capped per frame with round-robin deferral; no AI-driven pawn is deferred by type or by whether a viewer sees it.
- **If the host leaves**: the session ends (no migration in MVP); nothing in CNB survives.
- **If the navmesh is not ready at `OnRoundActive`**: Round's Preparing gate (`OnLayoutReady`) guarantees it is; CNB asserts and logs `NavNotReady`, and NPCs stay `Dormant` until it is (a development failure, never a shipping state).
- **If a path query fails or returns a partial path, or a pawn makes no progress**: the leg is abandoned and a new destination drawn (R3.3 progress monitor); after two failed repaths at the same chokepoint the NPC never retries that destination this leg. A pawn killed with a query in flight has its callback dropped (epoch, R10.5).
- **If the navigation system invalidates a held path** (it should not: the navmesh is frozen for the round, R15): the follower re-queries; until the new path arrives it holds its last `Move` for at most 0.5 s, then `Move(0)`.
- **If Halt latches while a command is pending**: the sink passes only `Move(0)` and Ends (R12); pending commands are dropped.
- **If a human claims the same stand point or bush side in the same drain**: the AI's command is rejected (same-drain contention, R3); the NPC starts a new leg, never retries that target within 10 s.

**Pillar 1 leak cases**
- **If a player stands still beside an NPC**: the NPC may turn to face it (C3), identically for any still pawn; a conversation forms only if the player faces back, exactly as an NPC partner does with probability `CrowdConverseReplyPct`; a partner that does not face back is weak evidence, not proof. An NPC never walks toward a stationary pawn to start one, so luring is impossible.
- **If a player tests "who yields" by walking at people**: the yield decision is taken 3-5 m out; NPCs do not yield 30 % of the time (SCL OQ10 range), and players never auto-yield but may step aside by hand, so one encounter is weak evidence. Repeatedly walking at pawns is itself a visible deviation and brings the tester into strike range; the non-yield rate is tuned in graybox (SCL OQ10).
- **If a hunter sprints past a suspect to provoke a flinch**: a trigger alone never causes an NPC brace (R7); only an NPC with a brace already due flinches (about 1 in 100), so the test carries almost no information.
- **If a pawn walks or stands far from every `CrowdRoute` node**: NPCs reach only places the route graph covers; LSC route data must cover every legal player position within 10 m (LSC change request), else an off-route pawn is a player.
- **If a player waits at a socket to watch replacements**: standing in the witness set denies that socket (the clear check); replacements emerge elsewhere; the watcher sees only exits that give up and walk back.
- **If observers count NPC runs per minute to calibrate**: rates are constant per build and per round; the band (F2) is designed for that knowledge.

## Dependencies

### Upstream (CNB depends on)

| System | Data interface CNB consumes | Hard / Soft | Status |
|---|---|---|---|
| Character Pool & Appearance (4) | `NpcAppearances` (dealt order), `DrawReplacement()` in the replacement's spawn tick, `VacateCard(Pawn)` (ADR-0012; formerly `RetireNpcCard`), `TakeNpcCard()`; card ledger rules CP-11..CP-13 | Hard | Written |
| Shared Character & Locomotion (9) | `SpawnSharedPawn(..., Ai, ...)` factory (R2); `Move`/`Look`/`Traverse` execution, pace set, shaping, head-look filter, handover R4, static-obstacle rule R16, `IsRunning`/`IsInTraversal` | Hard | Designed |
| Input & Interaction Framework (3) | The R1 entry points under R5-R9 and R17 (same buckets, debounce, legality), `OnActionStarted/Cancelled/Completed` hub (stimuli) | Hard | Written |
| Level Socket Contract (8) | `CrowdRoute`, `AreaTypeId`, NPC start pool in `RequestStartPlacement`, `RequestEmergence(Replacement / AmbientEntry / AmbientExit)`, `PickRespawnPawn` eligibility (R5b), `BushSite`, `TraversalPoint`, courts | Hard | Designed |
| Information Visibility Policy (7) | Rule "never read `IsPlayer`"; R7.6 NPC bush parity (F5 at k = 1.5, the ceiling); L4/L6/L7/L10/L13 leak rows | Hard | Reviewed (NEEDS REVISION, revisions applied; re-review pending) |
| Tuning Data & Config (1) | `NpcCount`, `TeamSize` and every `Crowd*` key via `GetServerTuning()` after `OnTuningReady` (CR-15) | Hard | Written |
| Round & Match Framework (6) | `OnRoundActive` (start AI), the Ended crowd hold (R9), hand-over/hand-back requests (R17), `ReleaseToCrowd` adoption, spawn list (P6) | Hard at runtime (listed in index row 11, verified 2026-10-24, cross-review C-W11) | Designed |
| Participant State & Roster (2) | `ReleaseToCrowd`, `GetLegalityClass` (via IIF), binding queries used only to pick the restricted tree for a bound AI pawn (R10) | Soft (listed in index row 11, verified 2026-10-24, cross-review C-W11) | Written |
| Session & Travel (5) | `GetServerSubSeed(CrowdBehaviour / AmbientSchedule, Attempt)` (rule 14); rule 25f stall-cover requirement | Soft | Written |
| Assassination & Defence Reversal (13) | `OnPawnKilled(Victim, Killer, VictimKind)`; body lifetime and removal; braced-outcome rule applies to NPC braces identically | Soft (event only; CNB never reads `VictimKind` for behaviour) | Reviewed, revisions applied |
| Concealment (Bushes) (12) | The one Hide path NPCs use (occupancy-blind, no cap, same re-entry cooldown and exit-side rule); `IsConcealedFrom`; `IsInBushOrTransition`; transitioning pawns excluded from avoidance (CON-R4) | Soft (bush red herrings disabled if absent; then the IVP cull must be off) | Reviewed, revisions applied (conflict C7 resolved for its version) |
| Objectives Chain (15) | `AllowNoEffect` predicate for decoy tending at every court, assignment-blind; `ObjectiveInteractSec`; `FixtureInteractSideCount`; full-fixture `Reject(NotNow)`; F3 tend band and I-OC3 | Soft (decoy tending disabled if absent) | Reviewed, revisions applied (its reopened change request 6 applied here 2026-10-24) |
| Camera System (10) | `EvaluateViewEnvelope` (Camera F1) for the virtual look layer (R3.4); T17 row | Soft | Reviewed, revisions applied |
| NPC-Kill Penalty & Informants (14) | Exile body release timing and `AmbientExit` request (PEN-R4.2, R5; CR-4) | Soft (downstream user too) | Reviewed, revisions applied |

### Downstream (these depend on CNB)

| System | What it needs from CNB | Hard / Soft |
|---|---|---|
| Round & Match Framework (6) | NPC population start, AI takeover/hand-back, Ended hold, `ReleaseToCrowd` adoption | Hard |
| Participant State & Roster (2) | Grace-period driving with idle ≤ `CrowdMaxIdleSec`; takeover decel ≤ `CrowdStopDecel` (derived from SCL keys) (AC-PSR-40b, 43) | Soft |
| Character Pool & Appearance (4) | 1:1 replacement through its API only; one delay distribution; appearance-independent candidates | Hard (contract) |
| Level Socket Contract (8) | Ambient floor, exit-cancel rule, density blind to the assignment, release of a taken-over pawn (AC-43, AC-57, AC-61) | Hard (contract) |
| NPC-Kill Penalty & Informants (14) | Eligibility flags for `PickRespawnPawn`; release of the chosen NPC; informant rows of the Tell Matrix | Hard |
| Assassination (13), Concealment (12), Objectives (15), SCL (9), IIF (3) | Their Tell Matrix rows (each adds an AC pointing to its row); NPC brace, bush and decoy-tending rates | Soft |
| Information Visibility Policy (7) | NPC bush rate meeting F5 at k = 1.5 (AC-IVP-25); NPC stay and exit distributions in T5 (AC-IVP-34) | Hard (contract) |
| Diegetic Audio (22, VS) | Crowd murmur density per area; footsteps come from SCL | Soft |
| Onboarding & Practice Mode (25, Alpha) | NPC-only crowd, catalogue and matrix for tutorials | Hard |
| Telemetry & Analytics (26, Alpha) | Crowd statistic streams for the parity and classifier gates (F6) | Soft |
| Anti-Cheat (27, Alpha) | Crowd-side leak surfaces (L4, L6, L7, L13) | Soft |

**Bidirectionality.** The index row 11 lists Crowd's upstream set, Round, Roster, Session, Assassination, Concealment and Objectives included (verified 2026-10-24, cross-review C-W11); only Camera is still not listed (Cross-References 14, 21). Camera and Penalty already list Crowd. Dependents that must list CNB: Round, Roster, CPA, LSC, Penalty & Informants, IVP, SCL, IIF, Diegetic Audio, Onboarding.

## Tuning Knobs

CR-15 table. Every key is ServerOnly (no client needs crowd tuning; NPC behaviour reaches clients only as pawn motion), one value for every AI-driven pawn, and has no player twin. Defaults are provisional until the graybox playtest; rates are re-derived offline from logged player rates (F2), never at runtime. Interval keys hold `60 / r_N` per NPC.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|---|---|---|
| `NpcCount` | int | CNB (TDC canonical row) | 40 | per TDC | per TDC | ServerOnly | Round | Count | Crowd size. Low: candidate pool shrinks, blind guesses pay (TDC I-4). High: AI and animation budgets (F5, SCL F8) |
| `CrowdAreaWeightCausewayMult` / `...MarketMult` / `...TempleMult` / `...ResidentialMult` | float | CNB | 1.4 / 1.6 / 0.8 / 0.6 | 0.2-2.0 | 0.5-1.6 | ServerOnly | Round | Mult | F1 attractors. Market > 1.6 breaks the LSC zone cap; all equal flattens the city's rhythm |
| `CrowdFullPacePct` | float | CNB | 100 (interim, R3.1); 55 once a keyboard walk-slower modifier exists | 50-100 | 50-65 or 100 | ServerOnly | Use | Pct | Walking-time share at pace 1.0 (SCL R5.1 floor 50). Below 100 without the modifier: slow walking is NPC-only (R4 breach, classifier gate fails); with the modifier, high values make a stick-walker stand out |
| `CrowdYieldPct` | float | CNB | 70 | 0-100 | 70-90 | ServerOnly | Use | Pct | B6 side-step probability; non-yield = 100 − this, SCL OQ10 asks 10-30 %. 100: "who does not yield" identifies players; low: shoulder collisions and a cheap yield test |
| `CrowdLongPausePct` | float | CNB | 45 | 10-60 | 35-50 | ServerOnly | Use | Pct | F4 (the regulator raises it to at most 90 %). Low: stalls stand out (Session 25f); high: city looks idle, loiter tell vanishes |
| `CrowdIdleMinSec` / `CrowdIdleMaxSec` | float | CNB | 4 / 16 | 2-30 | 4-16 | ServerOnly | Action | Sec | Long-pause length (TDC I-5: Min ≤ Max; Min ≥ `SessionSoftTimeoutSec` + 1; Max ≤ `CrowdMaxIdleSec`) |
| `CrowdStallCoverMinPct` | float | CNB | 10 | 0-30 | 10-20 | ServerOnly | Use | Pct | F4 regulator floor (Session 25f) |
| `CrowdMaxIdleSec` | float | CNB | 20 | 10-60 | 15-25 | ServerOnly | Use | Sec | Longest stationary stretch of any catalogue entry and of a restricted-driver pawn (Roster AC-PSR-40b, Session AC 46). Must be ≥ `CrowdIdleMaxSec` and `CrowdConversationMaxSec` |
| `CrowdConversationMinSec` / `CrowdConversationMaxSec` | float | CNB | 6 / 20 | 2-60 | 5-20 | ServerOnly | Action | Sec | C3 length |
| `CrowdConverseStartPct` / `CrowdConverseReplyPct` | float | CNB | 50 / 60 | 0-100 | 30-70 / 40-80 | ServerOnly | Use | Pct | C3 face-a-neighbour and face-back probabilities; provisional until matched to logged player face-back rates. Reply 100: a partner that does not face back is certainly a player; 0: conversations vanish |
| `CrowdGroupDriftSec` | float | CNB | 5 | 0-20 | 3-8 | ServerOnly | Action | Sec | C4 pace matching per leg (inactive under the interim pace rule). High: escorting a player becomes a lure |
| `CrowdAmbientTouchMinSec` / `CrowdAmbientTouchMaxSec` | float | CNB | 0.8 / 1.5 | 0.3-3.0 | 0.6-1.5 | ServerOnly | Action | Sec | C9 touch overlay length for every pawn (shared SCL overlay). Max too high: a touch reads as an objective tend (Objectives I-OC1: `ObjectiveInteractSec ≥ CrowdAmbientTouchMaxSec + 0.5`) |
| `CrowdRunIntervalSec` | float | CNB | 324 | 60-3600 | 240-480 | ServerOnly | Use | Sec | T1 (r_N 0.185/min). Low: running stops being evidence (coin flip); high: every runner is a confession |
| `CrowdVaultIntervalSec` / `CrowdClimbIntervalSec` | float | CNB | 1154 / 2727 | 120-7200 | ±40 % | ServerOnly | Use | Sec | T2; non-zero by SCL R7.3 |
| `CrowdBraceIntervalSec` | float | CNB | 2400 | 300-7200 | 1600-3600 | ServerOnly | Use | Sec | T4 (flinch + startle, total accepted rate; the ambient share is this minus the R8 contribution, F2). Low: braced pawns everywhere make strikes suicidal |
| `CrowdBraceCapSharePct` | float | CNB | 55 | 0-100 | 40-70 | ServerOnly | Action | Pct | Share of NPC braces held to the cap; matched to measured player braces (Policy F7 style test) |
| `CrowdDecoyTendIntervalSec` | float | CNB | 10500 | 1800-60000 | 8400-13500 | ServerOnly | Use | Sec | T3 (r_N 0.0057/min, `P_full` 0.70 in Objectives F3's 0.65-0.75 band at `T_round` 9 min; the safe range spans `P_full` 0.65-0.75; reconciliation 2026-10-24, was 18,750). Low: tending stops being the strongest read (P4); high: every tend is a player |
| `CrowdDecoyCompletePct` | float | CNB | 75 | 0-100 | 60-90 | ServerOnly | Action | Pct | Decoy tends held to completion (3.0 s); the early-release share is `100 −` this (one quantity, one name). First lever if Objectives' `P_short` > 0.90: lower it (more early releases), never raise the interval |
| `CrowdDecoyEarlyReleaseMinSec` / `CrowdDecoyEarlyReleaseMaxSec` | float | CNB | 0.8 / 2.8 | 0.3-3.0 / 1.0-5.0 | 0.6-1.0 / 2.2-3.0 | ServerOnly | Action | Sec | Uniform early-release time of a decoy tend (Objectives change request 6 (b)). Min must satisfy Objectives I-OC3 (`TendCueDelaySec + 0.25 ≥ Min`) and Max < `ObjectiveInteractSec`. Min high: informed probe releases fall before any decoy release (P1 tell); Max ≥ `D`: early releases become completions |
| `CrowdBushIntervalSec` | float | CNB | 316 (r_N 0.19/min) | 120-320 | 240-320 | ServerOnly | Use | Sec | T5 (`P_hide ≤ 1/6`, IVP F5 at k = 1.5). Constraint: `60 / CrowdBushIntervalSec ≥ r_N,req` (0.1875 at r_P 0.3; recomputed from the measured r_P). Low: bushes crowd with NPCs; above the constraint: bush entries confess and the cull leaks (IVP L13, cull must be off) |
| `CrowdBushDwellMedianSec` / `CrowdBushDwellSigma` | float | CNB | 15 / 0.8 | 3-120 / 0.3-1.5 | 8-25 / 0.6-1.0 | ServerOnly | Action | Sec / — | Median and log-sigma of the NPC's own **uncapped** stay in a bush (log-normal, mean ≈ 20.7 s). Not a cap and not a timer on hiders (locked "no dwell timer"; Concealment deliberately has no dwell key). Provisional until compared with the logged player dwell (AC-CNB-15, advisory). Short: NPC stays look like peeks; long: NPCs crowd the bushes (Concealment F1) |
| `CrowdReversalIntervalSec` / `CrowdHeadHoldIntervalSec` | float | CNB | 316 / 480 | 60-3600 / 120-3600 | ±40 % | ServerOnly | Use | Sec | T12, T13 (G6 clock, 0.125/min) |
| `CrowdAlleyDawdleIntervalSec` | float | CNB | 1200 | 300-7200 | 900-1800 | ServerOnly | Use | Sec | Walk into an alley and back out (cover for exits that give up) |
| `CrowdStrikeReactRadiusCm` | float | CNB | 1500 | 500-3200 | 1000-2000 | ServerOnly | Use | Cm | R8 stimulus radius |
| `CrowdStartleRadiusCm` | float | CNB | 600 | 200-1000 | 400-800 | ServerOnly | Use | Cm | Panic-run and startle-brace radius |
| `CrowdPanicRunPct` / `CrowdStartleBracePct` | float | CNB | 3 / 8 | 0-20 | 2-5 / 5-12 | ServerOnly | Use | Pct | R8. 0 panic runs: any runner near a kill is a player |
| `CrowdTellChainGapSec` | float | CNB | 20 | 10-35 | 20-30 | ServerOnly | Use | Sec | Minimum gap between two different tells by one NPC (coin-flip guard; F2 compensates the draw rates). Low: chained NPC tells make reading a coin flip; high: compensation exceeds its validity bound `(Λ − r_N,j) · T_gap ≤ 0.5` (about 38 s at defaults, set by the rarest tell) |
| `CrowdStrikeResponderLimit` | int | CNB | 8 | 1-20 | 6-10 | ServerOnly | Use | Limit | Responders per strike |
| `CrowdReactRefractorySec` | float | CNB | 8 | 0-30 | 5-12 | ServerOnly | Use | Sec | Per-NPC reaction cooldown |
| `CrowdAmbientCycleIntervalSec` | float | CNB | 80 | 40-90 | 60-90 | ServerOnly | Round | Sec | Per socket. > 90 breaks the LSC floor once kill cancels apply (AC-57, F3); < 40 exceeds the socket cooldown ceiling; < 60 saturates sockets |
| `CrowdCycleGapMinSec` / `CrowdCycleGapMaxSec` | float | CNB | 6 / 20 | 2-60 | 5-25 | ServerOnly | Use | Sec | Exit-to-entry gap; mean absent NPCs F3 |
| `CrowdReplacementMinDelaySec` / `CrowdReplacementMaxDelaySec` | float | CNB | 10 / 60 | 3-30 / 30-180 | 8-15 / 45-90 | ServerOnly | Use | Sec | F3. Min too low: an emergence follows a kill closely; max too low: the fallback dominates |
| `CrowdExitGiveUpSec` | float | CNB | 8 | 2-30 | 5-12 | ServerOnly | Use | Sec | Watched exit turns back |
| `CrowdDecisionPerSec` / `CrowdSteerPerSec` | float | CNB | 4 / 30 | 1-10 / 10-60 | 3-6 / 20-30 | ServerOnly | Use | PerSec | F5 cost; behaviour statistics must not change with them (AC-CNB-21) |
| `CrowdPathStartsPerFrameLimit` | int | CNB | 2 | 1-8 | 2-4 | ServerOnly | Use | Limit | F5 peak control |

**Identity-tell review (CR-15.8).** Replacement keys (`CrowdReplacement*`, `CrowdAmbientCycleIntervalSec`, `CrowdCycleGap*`) time a crowd-versus-player event; they read only the kill time and apply to every victim kind, and the replacement is the next ordinary scheduled entry, so they are not a tell (AC-CNB-08). `CrowdMaxIdleSec` bounds a connection stall to the NPC maximum (not a tell by construction). The red-herring intervals *are* the tell calibration and are reviewed through F2. **Consumed, owned elsewhere:** `HandoverHoldSec`, `WalkSpeedCmPerSec`, `RunStopSec`, `ReversalAngleDeg`, `LookMaxRateDegPerSec` (SCL); `ObjectiveInteractSec` (Objectives/TDC); `DefenceDurationSec`, `DefenceCooldownSec` (Assassination/TDC); `EmergenceRetrySec`, `EmergenceDeficitMaxCount`, `EmergenceExitCancelMaxPct`, `AmbientEmergencePerSocketMinPerMin` (LSC); `SessionSoftTimeoutSec` (Session); `DisconnectGraceSec` (Round). **Cross-field constraints (TDC change request):** `CrowdIdleMinSec ≥ SessionSoftTimeoutSec + 1`; `CrowdMaxIdleSec ≥ max(CrowdIdleMaxSec, CrowdConversationMaxSec)`; `(60 / CrowdAmbientCycleIntervalSec) · (1 − EmergenceExitCancelMaxPct / 100) ≥ AmbientEmergencePerSocketMinPerMin`; `2 · 60 / CrowdAmbientCycleIntervalSec ≤ 60 / EmergenceSocketCooldownSec`; `60 / CrowdBushIntervalSec ≥ r_N,req` (IVP F5 at `HidePosteriorFactor`, measured r_P); F2 compensation validity `(Λ − r_N,j) · CrowdTellChainGapSec / 60 ≤ 0.5` for every tell; F1 `max_t ρ_t / ρ̄ ≤ 1.5` at layout load; Objectives I-OC1 `ObjectiveInteractSec ≥ CrowdAmbientTouchMaxSec + 0.5`. **Consumed, owned elsewhere (added):** `BushReenterCooldownSec`, `HidePosteriorFactor`, `FixtureInteractSideCount`, `EmergenceSocketCooldownSec`.

## Visual/Audio Requirements

*art-director not spawned (orchestrator budget of three specialists); requirements follow art bible 5.4 (B1-B8, rules 1-5), 5.5 (tell vocabulary), 5.2.1 (emergence presentation) and 1.3 (uniformity spec).*

1. **No NPC-only asset.** Every animation an NPC plays comes from the one AnimBP and pose set (SCL R10) and is reachable by a player pawn through the same `ActionPose` or locomotion state. New assets this GDD needs are shared assets: the contextual idle overlay (talking hands, nod, B2 weight-shift variants; any idle pawn facing another within 1.5 m plays it, players included, SCL change request), `InteractPose` variants for `AmbientSpot` touch (0.8-1.5 s, one hand, B8) and seat (sit-down, seated idle, stand-up) if CR-1 is approved, and a `Startle` additive used by nothing but a gaze plus weight shift (no new pose state).
2. **Emergence and exit presentation** follow LSC R5.5: created and absorbed at the occluded origin cell, walking on the normal path; no fade, VFX or sound, ever.
3. **Reactions are motion only**: glance, pause, reroute. No scream VO, no gasp stinger, no crowd-panic sound in MVP; Diegetic Audio (VS) may add a murmur swell keyed on the strike stimulus (identity-blind) and never on `VictimKind`.
4. **Twins never move in lock-step**: each NPC's idle phase, weight-shift timer and gaze timer start from its own random offset (CPA (d)); players get their animation phase from the same per-pawn offset rule (SCL).
5. **Crowd murmur** (VS, Diegetic Audio): density-driven ambience per area type from CNB's area counts, never from a count that includes or excludes players differently (it counts pawns).
6. Reference scene for visual sign-off: the SCL reference scene (50 pawns, graybox market) recorded at 15 m and 30 m; the 20 m greyscale detection test of art bible 5.5 is run against NPC red herrings as well as players (a red herring must read as the same tell).

> 📌 **Asset Spec** — Visual/Audio requirements are defined. After the art bible is approved, run `/asset-spec system:crowd-npc-behaviour` (contextual idle overlay, AmbientSpot touch and seat poses).

## Game Feel

The crowd is felt as **rhythm**: the city should feel unhurried, legible and slightly indifferent, so that the player's own choices are the only loud thing in it.

| Feel target | Value | Why |
|---|---|---|
| Visible pace mix in any 20 m view | interim: every NPC walker at full pace (R3.1); ~55 % / ~45 % at 0.8-0.9 once a keyboard walk-slower modifier exists; ~25 % of pawns standing (F4) | a keyboard walker is ordinary; a still body is ordinary (Session 25f) |
| Something "player-like" happening in view | about one run, vault, brace or decoy tend per 40-60 s in a 32 m sightline (F2 defaults) | red herrings are frequent enough to doubt, rare enough to notice |
| Reaction to a strike in view | heads turn within 0.3-1.2 s (logged human reaction times), most bodies keep going within 3 s | "the city noticed, and moved on": no panic to hide in, no stampede to use |
| Emergence | a citizen walks out of an alley every ~6 s somewhere in the city (60 / 9.75, F3) | appearing is never news |
| Conversation and group drift | 2-4 small knots (2-3 pawns) visible in a market view | gives players something to join, and observers something to doubt |
| Takeover / hand-back | no frame shows deceleration above the baseline, a head move faster than 140 °/s or a pose pop | the one moment a script touches a player's body must be invisible |
| Ended hold | every pawn brakes to idle within `RunStopSec` (0.75 s) from Round's round-end step (for a strike win, the deciding strike's death write: the city stops as the blow lands, never before it) | the city holds its breath; nobody is left walking |

Mastery target: a skilled observer gets better at reading *combinations* (a run that ends at a court, a brace with nobody near) rather than single events; a single deviation should be evidence, never proof.

## UI Requirements

CNB has **no player-facing UI** in any viewer state, and that is a requirement, not an omission: no NPC indicator, no "crowd density" readout, no replacement or emergence notice, no reaction marker, no takeover notice (Pillar 1; anti-pillar "not an identification-tools game"). Development-only server debug views (StateTree state per NPC, schedule ledger, cost meter) are compiled out of shipping builds and are never drawn on a client in a networked session, including the listen-server host's own view (AC-CNB-25). The practice mode (Onboarding, Alpha) may show NPC-only tutorials; any such overlay is owned by that GDD and is unavailable in a match.

## Cross-References

| Document | Sections used | Used by (this GDD) |
|---|---|---|
| design/gdd/game-concept.md | Pillars, anti-pillars, Key Dynamics ("mimic NPC rhythms") | Player Fantasy, R4, R7 |
| design/gdd/shared-character-locomotion.md | R2, R4, R5, R7.3, R8, R13, R16; F1-F3, F6; AC-SCL-32/36/37; Cross-References 5 | R1, R3, R10, R16 |
| design/gdd/input-interaction-framework.md | R1-R9, R12, R17; Cross-References 9; AC 38, 50; OQ 5 | R3, R7, R16 |
| design/gdd/character-pool-appearance.md | CP-10..CP-13, F-6; Interactions row; Cross-References 8; OQ-10 | R1, R9, R11 |
| design/gdd/level-socket-contract.md | R5, R5b, R6, D6, D9; AC-43, 53, 57, 61; Cross-References 6 | R1, R9, R11, R13, F1, F3 |
| design/gdd/participant-state-roster.md | R9, R11; AC-PSR-40b, 43; OQ6 | R10 |
| design/gdd/round-match-framework.md | P6, R6, R7, R9, R17, R23; Cross-References 9 | R10, R12 |
| design/gdd/session-travel.md | rule 14, rule 25f, AC 46 | R14, F4 |
| design/gdd/information-visibility-policy.md | R7.6, F2, F5 (k = 1.5 ceiling, ruling A), F7; L4/L6/L7/L10/L13; OQ3; AC-IVP-25, 34 | R7, F2, F6, T5 |
| design/gdd/concealment-bushes.md | CON-R3, CON-R4, CON-R11, CON-R14; F1, F2; AC-CON-12, 15, 16, 19; edge cases on co-occupants and dwell | R2, R3.3, R7, T5, Edge Cases |
| design/gdd/objectives-chain.md | OC-6, OC-9, OC-14 (I-OC1), F3; Cross-References change request 6; AC-OC-24 | R7, T3, Tuning Knobs |
| design/gdd/tuning-data-config.md | CR-5, CR-11, CR-15, I-5; Cross-References 8 | Tuning Knobs |
| design/gdd/camera-system.md | F1 `EvaluateViewEnvelope`, CS-R2, change request 5 | R3.4, T17 |
| design/gdd/npc-kill-penalty-informants.md | PEN-R4.2, PEN-R5 (exile body exit), CR-4, AC-NKP-45 | R9.1, R10.4, R11, Edge Cases |
| design/art/art-bible.md | 5.4 (B1-B8, rules 1-5), 5.5 tell vocabulary, 5.2.1 | R5, R6, Visual/Audio |

**Change requests raised by this GDD** (the orchestrator routes them; this GDD edits no other file):
1. **Information Visibility Policy** — **RESOLVED 2026-10-24 (request denied):** the earlier request to raise F5's k to 2.0 was declined (IVP review ruling A; cross-GDD conflict C7 resolved for Concealment's version). k stays 1.5 (IVP range now 1.0-1.5); Crowd meets F5 by raising its own r_N to 0.19/min (T5, F2, AC-CNB-15), which also answers IVP OQ3 and AC-IVP-34. **New request:** reconcile IVP Cross-References item 4 ("meeting F5 and F7 on dwell time") with the F5 note (exit timing is a P2 context tell, "not a transport test"); Crowd treats dwell parity as advisory (AC-CNB-15).
2. **Art bible 5.5 / 5.4 (art-director owns)**: revise the bush row "NPCs never enter bushes" to "NPCs enter bushes as context-free red herrings (T5)"; this GDD is the deliberate choice the concept's design flag asked for; B8 touch is a shared contextual overlay; sitting is not in the MVP catalogue.
3. **Shared Character & Locomotion**: add the shared contextual idle overlay (touch within 100 cm of a touchable prop; conversation gestures facing a pawn within 150 cm) for every idle pawn with per-pawn random timers; confirm that pace is the `Move` magnitude and consider a keyboard "walk slower" modifier so 0.8/0.9 are reproducible on keys (OQ3).
4. **Input & Interaction Framework**: AI `Interact` at courts bypasses only the human ingress site-visibility check (stage 2 applies); record that NPCs never issue `Attack` (T6); answer to IIF OQ 5 is R7 + F2.
5. **Objectives Chain**: the `AllowNoEffect` predicate must accept decoy tending at every court from any Active pawn for which the site is not live, identically for humans (IIF R5.2d); add AC pointing to T3. Its change request 6 to Crowd is **applied** (occupied courts included, T3 band 0.65-0.75; rate re-applied at reconciliation 2026-10-24 as reopened: r_N 0.0057, `CrowdDecoyTendIntervalSec` 10,500, full-fixture refusal, early-release keys); `CrowdAmbientTouchMaxSec` (1.5 s) now exists for I-OC1. *(Already satisfied by Objectives: AC-OC-24, AC-OC-30.)*
6. **Assassination & Defence Reversal**: a braced NPC reverses an attacker exactly as a braced player does; `OnActionStarted(Attack)` fires for whiffs; body lifetime and removal identical for every victim kind; ACs pointing to T4, T6.
7. **Concealment (Bushes)**: NPCs use the one Hide path; its change request 3 to Crowd is **applied** (occupancy-blind choice, no dwell cap, r_N 0.19 against k = 1.5, same re-entry cooldown and exit-side rule; T5 cites AC-CON-15). Remaining ask: CON-R14 should cite T5 and AC-CNB-15 as the parity evidence for its OQ3.
8. **Level Socket Contract**: R5b.4 "a replacement NPC carrying the victim's card" → "a replacement whose card is `DrawReplacement()` (the victim's when it is the only vacancy, CPA CP-12a)"; publish touchable-prop records (stall rails, walls, shrines) in the Manifest for the overlay; AC-61 is satisfied by T8.
9. **Round & Match Framework**: R9 Halt and R17 restricted driver as specified in R10/R12; note that Round R17's "no `TakeNpcCard` card move is needed" differs from CPA CP-12b (`TakeNpcCard` is the card move): reconcile in Round.
10. **Participant State & Roster**: `CrowdStopDecel` is a derived constant (SCL run braking, 600 cm/s²), not a key; `CrowdMaxIdleSec` = 20 s.
11. **Session & Travel**: AC 46(a)'s "Crowd's maximum idle pause" is `CrowdMaxIdleSec`; 25f's 10 % target is AC-CNB-12.
12. **Tuning Data & Config**: register the Crowd keys above and the three cross-field constraints.
13. **NPC-Kill Penalty & Informants**: consume R11 eligibility and `TryReleaseNpc`; own T7 and an AC pointing to it. *(Satisfied: Penalty CR-4 and AC-NKP-17.)*
14. **Systems index**: add Round, Roster, Session, Assassination, Concealment and Objectives to Crowd's Depends On (soft except Round).
15. **Shared Character & Locomotion** (review 2026-10-24): (a) the replicated remote pitch is the 16-bit `RemoteViewPitch16` [verified 5.8.4], so the ≈ 1.41° quantisation figure and AC-SCL-18 need correcting; the crowd AIController writes `Look` pitch into the control rotation directly (R3); (b) hold the last `Move` after a possession change until the new controller's first command or `HandoverHoldSec` (R10.5); (c) budget the server movement simulation of AI-moved pawns (F5 excludes it); (d) the Crowd ADR will weigh an SCL turn-smoothing filter on every pawn against NPC input emulation (R3.4).
16. **Round & Match Framework** (review 2026-10-24): handover commit in the drain bumps a per-pawn control epoch; possession takes effect at the next frame start (R10.5); confirm AI `Move`/`Look` bypass the 32-item mailbox cap.
17. **Level Socket Contract** (review 2026-10-24): `CrowdRoute` must cover every legal player position within 10 m (off-route pawns would be players); pre-baked route-edge paths in the Manifest.
18. **Procedural City Generator** (review 2026-10-24): no runtime navmesh rebuild after `OnLayoutReady` (the prototype rebuilds at runtime).
19. **Tuning Data & Config** (review 2026-10-24): register the new and renamed keys (`CrowdBushDwellSigma`, `CrowdHeadHoldIntervalSec` replacing `CrowdHeadHoldPct`, `CrowdConverseStartPct`/`ReplyPct`, `CrowdAmbientTouchMinSec`/`MaxSec`) and the cross-field constraints in Tuning Knobs.
20. **Telemetry & Analytics** (review 2026-10-24): one shared pipeline for logged player tell rates (R7, F2), reaction and face-back rates (R6, C3) and motion profiles (R3.4, including view-envelope edge-push durations); track as one producer dependency (OQ12). *(ADR sync 2026-10-24: the producer side is ADR-0023's Event Log telemetry section; the consumer is the Alpha Telemetry GDD; the input-device mix stays a manual playtest-sheet item until a client-reported field is approved.)*
21. **Systems index** (reconciliation 2026-10-24): add Camera System (soft) to Crowd's Depends On (`EvaluateViewEnvelope`, R3.4).

**Change requests received (reconciliation 2026-10-24):**

| From | Request | Disposition |
|---|---|---|
| Objectives CR 6 (reopened at its design review) | r_N ≈ 0.0057/min (`CrowdDecoyTendIntervalSec` ≈ 10,500 s, widen the safe range), supersedes 0.0032; update T3 and AC-CNB-14 | APPLIED: R7 decoy rule, T3, F2 table and decoy note, Λ example, Tuning Knobs (default 10,500, safe 8,400-13,500), AC-CNB-14 |
| Objectives CR 6 (a) | A decoy refused at a full fixture (fifth Begin) chooses another court | APPLIED: R7, Edge Cases, AC-CNB-14 |
| Objectives CR 6 (b) | Expose early-release bounds and share as keys for I-OC3 | APPLIED: `CrowdDecoyEarlyReleaseMinSec`/`MaxSec` added; the share stays `100 − CrowdDecoyCompletePct` (one quantity, one name) rather than a third key |
| Objectives CR 6 (c) | If `P_short` > 0.90, raise the early-release share first, never lower the rate | APPLIED: R7, Tuning Knobs note |
| Penalty CR-4 | Exile body: after its lockout an `AmbientExit` that takes the next scheduled city-wide exit slot; rewrite R10.4 path and the struck-out edge case | APPLIED: R9.1, R10.4, R11 (`AlleyExit` eligibility reason, never re-adopted), Edge Cases; our CR 13 marked satisfied |
| Camera CR 5 | Tell Matrix row "camera mode / toggle: no visible deviation; not an advantage; owner Camera" | APPLIED: T17 |
| Camera CR 5 (new) | R3.4 virtual look runs through Camera's `EvaluateViewEnvelope` (F1); push durations from human telemetry | APPLIED: R3.4; our CR 20 extended; new CR 21 (index) |
| Concealment CR 13 | Concealed and transitioning pawns excluded from RVO, avoidance and steering; Crowd LOD never changes replication; dwell unchanged | APPLIED: R2, R3.3, R15 (dwell already as requested) |
| Concealment CR 3 | Occupancy-blind bush choice, no dwell cap, r_N 0.19 at k = 1.5, same cooldown and exit side | ALREADY DONE (R7 bush rule, T5, AC-CNB-15) |
| Assassination item 8 | NPC braces reverse exactly as player braces; `OnActionStarted(Attack)` for whiffs; identical body lifetime; ACs for T4, T6 are AC-ASN-26/27 | ALREADY DONE (R7 brace, R8, R9.3) plus T4/T6 AC references APPLIED |
| IVP OQ3 / Cross-References item 4 | T5 and our CR 1 restated against k = 1.5, raise r_N | ALREADY DONE (T5, CR 1 resolved) |
| Generator (requests-received table) | Our CR 17 (route coverage, via LSC) and CR 18 (frozen navmesh) | ALREADY DONE by the Generator (S12, R-G7, AC-G36, AC-G37); no Crowd change |
| Session CR 6, SCL CR 5, LSC CR 9, CPA CR 8, TDC CR 8, Round CR 9/18, Roster OQ6, IIF CR 9 | Authoring-time requests (stall-cover pauses, pace set, ambient cycles, card API, `Crowd*` names, takeover and hand-back, Tell Matrix coverage) | ALREADY DONE (R3, R5, R9-R12, R16, Tuning Knobs; cited in the Cross-References table) |
| Cross-review 2026-10-24 S-B1 (Round R9 staging) | Halt and stops at Round's round-end step, not at the latch | APPLIED (cross-review fixes 2026-10-24): CNB-R12, Game Feel "Ended hold" row, AC-CNB-22 |
| Cross-review 2026-10-24 C-W11 | Stale index claims | APPLIED (cross-review fixes 2026-10-24): Round and Roster rows, Bidirectionality (only Camera still unlisted in row 11) |
| ADR-0016 sync 2026-10-24 | CNB-R2 / AC-CNB-01: the "one permitted read" of the Roster binding becomes a call parameter (`DriveBoundPawn` vs `AdoptReleasedPawn`); include check expects zero Roster call sites | APPLIED (R2, AC-CNB-01) |
| ADR-0016 sync 2026-10-24 | CNB-R12 and States `Halted`: Halt latches the sink, clears `Move`, stops the tree; holds cancelled by Round's round-end step (g); braking is ADR-0010's stop-and-blend | APPLIED (R12, States table) |
| ADR-0016 sync 2026-10-24 | CNB-R15 / OQ11 resolved as input emulation; OQ5 possession via the hand-over pass; OQ8 StateTree verified | APPLIED (R15 header, OQ5, OQ8, OQ11) |
| ADR-0006 sync 2026-10-24 | CNB-R14 "schedule draws use `AmbientSchedule` the same way" → F5 counter-as-Attempt shape | APPLIED (R14) |
| ADR-0012 sync 2026-10-24 | `RetireNpcCard()` → `VacateCard(Pawn)` | APPLIED (Interactions and Dependencies rows) |
| ADR-0018 sync 2026-10-24 (amendment 4 to ADR-0016) | No `OnPawnKilled` side effect observable before the death write; t_k = scheduled death time; NPC victim keeps its driver until then | APPLIED (R9.2 kill vacancy; aligns with Assassination's victim-`Disabled`-at-death-write ruling) |
| ADR-0019 sync 2026-10-24 | Penalty is the only caller of `TryReleaseNpc`; a failed re-check is re-picked by Penalty (not "LSC redraws") | APPLIED (edge case) |
| ADR-0023 sync 2026-10-24 | CR 20: shared pipeline = ADR-0023 (producer) + Alpha Telemetry GDD (consumer); input-device mix stays manual | APPLIED (CR 20 note); client-reported device field already PENDING-USER in active.md |
| ADR-0007/0008 sync 2026-10-24 | Test root follows the module layout | APPLIED (AC preamble) |

## Acceptance Criteria

*qa-lead not spawned at authoring (orchestrator budget); its lens was applied at the 2026-10-24 design review, which rewrote AC-02, 03, 07, 10, 12-18, 23, 24, 26, 27, 29-31 and added AC-32 to 38. Each criterion is independently runnable, names its test (`StealthGame.Crowd.<Scenario>` in `Source/StealthGameplay/Private/Tests/Crowd/`; multi-client tests in `Source/StealthTestHarness/Private/`, ADR-0007/0008), and states its story type and gate. "Reference scene" = the SCL reference scene on the graybox (40 NPCs, 10 bot players, fixed seeds). Statistical tests use fixed, pre-registered seeds; a failing seed is a defect, never reseeded.*

**Identity blindness and command path**
- **AC-CNB-01** [Logic/BLOCKING] `Crowd.NoIdentityReads`: GIVEN the Crowd module, WHEN the build-time include check runs, THEN no Crowd source includes the Roster, Objectives assignment or Policy fact headers (zero Roster call sites; the driver is selected by `DriveBoundPawn` vs `AdoptReleasedPawn`, R2), and an instrumented run of the reference scene records 0 calls to `IsPlayer`, `GetTeam`, `VictimKind` or `FSiteAssignment` from Crowd code.
- **AC-CNB-02** [Integration/BLOCKING] `Crowd.CommandsOnlyThroughIIF`: GIVEN 10 min of the reference scene, WHEN movement-component writes are instrumented, THEN 100 % of NPC velocity changes come from `Move` entry-point calls (instrumented by a test hook in the movement component's input consumption), there are 0 direct CMC velocity or `RequestDirectMove` writes from Crowd, 0 StateTree `MoveTo`/`AITask_MoveTo` uses and 0 path-following component ticks, and 0 AI commands are rejected by the IIF except same-drain contention, each of which is logged with the contending human command (R3).
- **AC-CNB-03** [Logic/BLOCKING] `Crowd.PaceSetAndFullPaceShare` (with SCL AC-SCL-36): GIVEN 10 min of the reference scene, WHEN NPC walk magnitude is logged, THEN only {0.8, 0.9, 1.0} occur, the share of NPC walking time at 1.0 is within ±3 points of `CrowdFullPacePct` (exactly 100 % under the interim rule) and never below 50 %, and every pace change happens at a stop or a leg boundary.
- **AC-CNB-04** [Logic/BLOCKING] `Crowd.NoCornerStops`: GIVEN 200 recorded route legs with a 45-90° corner, WHEN replayed, THEN ≥ 95 % of corners are taken without speed dropping below 0.8 × leg pace, and no commanded heading change exceeds `ReversalAngleDeg` while moving except C7 reversals.

**Density and population**
- **AC-CNB-05** [Logic/BLOCKING] `Crowd.DensityBlindToAssignment` (LSC AC-43): GIVEN 50 fixed layouts each run with two different `FSiteAssignment`s, WHEN start placement and the first 120 s of destination weights are computed, THEN outputs are bit-identical, and an instrumented assignment query from Crowd fires the server assert.
- **AC-CNB-06** [Logic/ADVISORY] `Crowd.AreaDensityTargets`: GIVEN the reference scene after 120 s, WHEN density per area type is sampled every 10 s for 10 min, THEN each type's mean is within ±20 % of F1's target and no zone disc exceeds `ZoneDensityMaxMult` × the map mean for more than 30 s.
- **AC-CNB-07** [Logic/BLOCKING] `Crowd.SpawnPathAndLedger` (LSC AC-57, CPA AC-CP-10): GIVEN a 10-minute simulation with 20 scripted kills of mixed victim kinds (2 per minute, under the 2.4/min exit-cancel capacity of F3), WHEN spawns and removals are counted, THEN 100 % of mid-round NPC spawns and removals go through `RequestEmergence` and the SCL factory, `DrawReplacement` is called in each spawn tick, the measured entry and exit rates averaged over sockets are each ≥ 0.5 per socket per minute, and after every event the CPA ledger invariants hold.
- **AC-CNB-08** [Logic/BLOCKING] `Crowd.ReplacementDelayVictimBlind`: GIVEN ≥ 1,236 simulated kills of player pawns and ≥ 1,236 of NPC pawns (and of informants), WHEN the kill-to-replacement-entry delay D is measured, THEN for each pair of classes |d| ≤ 0.1, variance ratio ∈ [0.8, 1.25] and KS D ≤ 0.1; the pooled mean is 16 ± 2 s and p95 ≤ 32 s at defaults (F3); no replacement occurs earlier than `CrowdReplacementMinDelaySec`.
- **AC-CNB-09** [Integration/BLOCKING] `Crowd.EmergenceNeverWitnessed`: GIVEN 10 min with 10 bot viewers walking random routes, WHEN every emergence and absorption is checked against every pawn's eye samples (LSC R5.2), THEN 0 occur within any pawn's line of sight, and watched exits give up after `CrowdExitGiveUpSec` and walk back out.

**Catalogue, gaze and red herrings**
- **AC-CNB-10** [Logic/BLOCKING] `Crowd.TellBaselineNonZero` (SCL AC-SCL-37): GIVEN 600 simulated minutes of the reference scene (headless, accelerated, fixed pre-registered seeds; no strikes, so R8 adds nothing), WHEN accepted NPC runs, vaults, climbs, braces, decoy tends, bush entries, reversals and long head holds (after the chain-gap guard) are counted, THEN each count is > 0 and each city-wide rate lies within the two-sided 99 % Poisson interval of F2's accepted default (decoy tends: expected ≈ 77 events).
- **AC-CNB-31** [Logic/BLOCKING] `Crowd.EvidenceCompounds` (property test): GIVEN 600 simulated minutes of the reference scene with scripted strikes every 30 s, WHEN every NPC's accepted tells (T1-T5, T12, T13, R8 panic runs and startle braces included) are logged, THEN no NPC shows two different tells within `CrowdTellChainGapSec`, and the gap times between an NPC's consecutive different tells show no excess just after `CrowdTellChainGapSec` (the count in [gap, gap + 5 s] is within the 99 % Poisson interval of the count expected from the restarted exponential clocks). The resulting posterior for "two different tells within the gap" is reported (playtest metric, not gated: it depends on the measured r_P, OQ6).
- **AC-CNB-11** [Logic/BLOCKING] `Crowd.NoNpcStrike`: GIVEN 60 min of simulation, WHEN NPC commands are logged, THEN NPCs issue 0 `Attack` commands and participant-bound AI pawns issue 0 commands other than `Move` and `Look`.
- **AC-CNB-12** [Logic/BLOCKING] `Crowd.StallCoverShare` (Session 25f, AC 46): GIVEN 10 min of the reference scene sampled every 1 s, WHEN NPCs in a stationary stretch already ≥ 4 s long are counted (the F4 metric), THEN the city-wide share is ≥ 10 % (at least `ceil(0.1 · N_live)` NPCs) in ≥ 98 % of samples, and the regulator never sets the long-pause share above 90 %. The per-viewer share (10 bot viewers, samples with ≥ 10 NPCs visible) is **reported, not gated** (at f ≈ 12.5 % a 10-NPC view holds at least one such NPC only about 74 % of the time).
- **AC-CNB-13** [Logic/BLOCKING] `Crowd.MaxIdleBound`: GIVEN 60 min of simulation, WHEN every visible stationary stretch of every NPC and restricted-driver pawn is measured, including chains of pause, conversation and reaction pause, THEN none exceeds `CrowdMaxIdleSec` (20 s).
- **AC-CNB-14** [Logic/BLOCKING] `Crowd.DecoyTendAssignmentBlind`: GIVEN 2,000 decoy tends over 50 layouts (headless accelerated simulation, about 8,800 simulated minutes at r_N 0.0057), WHEN court choice is tabulated, THEN a chi-square goodness-of-fit test against the uniform distribution over all courts shows p > 0.01, and so does a test of live versus decoy versus unused versus currently-tended courts; 75 ± 5 % of decoy holds complete at exactly `ObjectiveInteractSec` and every early release lies in [`CrowdDecoyEarlyReleaseMinSec`, `CrowdDecoyEarlyReleaseMaxSec`]; every decoy Begin refused `NotNow` at a full fixture is followed by a different court choice; and with Objectives' `R_full` 0.40 (F3 at `T_round` 9 min) the full-length T3 posterior `P_full` computed from the measured NPC rate lies in 0.65-0.75 (AC-OC-21).
- **AC-CNB-15** [Integration/BLOCKING] `Crowd.NpcBushParity` (IVP F5 at k = 1.5, AC-IVP-25; Concealment AC-CON-19, 21): GIVEN an NPC-only headless run of ≥ 120 simulated minutes with fixed seeds, WHEN accepted NPC bush entries (after the chain-gap guard) are counted from the Concealment entry log, THEN the per-NPC rate is ≥ `r_N,req` (0.1875/min at the current r_P 0.3; recomputed when r_P is measured), so `P_hide ≤ 1/6`; every NPC entry uses the one Concealment call site; the bush chosen for a fixed seed is identical with the bush empty and occupied; NPC stays show no upper truncation (the empirical 99th percentile is within 15 % of the log-normal's); and no NPC re-enters within `BushReenterCooldownSec`. **Advisory** (not gated, see change request 1): NPC versus logged player dwell is reported with Policy F7's statistics.
- **AC-CNB-16** [Logic/BLOCKING] `Crowd.GazeWithinHeadLimits`: GIVEN 10 min of NPC `Look` streams, WHEN `HeadLook` is logged, THEN head rate never exceeds `LookMaxRateDegPerSec`, every G2 glance holds 0.4-2.5 s unless it is a G6 hold of 2.5-6 s, the G6 rate is within the 99 % Poisson interval of 0.125 per NPC-minute over a 600-minute run, no G2 target is concealed or out of line of sight, and G3 holds reach a pitch of −10 to −20°.
- **AC-CNB-17** [Integration/BLOCKING] `Crowd.ConversationPartnerBlind`: GIVEN a scripted player pawn standing still within 150 cm of an idle NPC 1,100 times and an NPC in the same spot 1,100 times, WHEN the NPC's face-toward rate is counted, THEN the two rates differ by ≤ 5 percentage points (TOST equivalence at α = 0.05); a conversation forms only while both pawns face each other within 60°; an unanswered initiator turns away after 2-5 s for either partner kind; and NPCs never path toward a stationary pawn to start one.

**Reactions**
- **AC-CNB-18** [Logic/BLOCKING] `Crowd.StrikeReactionOutcomeBlind`: GIVEN 500 scripted strikes each of kill-NPC, kill-player, whiff and reversal in a fixed crowd, WHEN the reactions of NPCs within 15 m with line of sight are logged, THEN the reaction distribution does not differ by outcome (chi-square p > 0.01); stratified by distance (≤ 6 m, > 6 m), each R8 share is within ±20 % relative of its target (panic runs and startle braces only ≤ 6 m, glances in their place beyond); no reaction starts earlier than the reaction-time distribution's minimum after the stimulus; ≤ 8 NPCs respond per strike; no NPC reacts twice within `CrowdReactRefractorySec`; and no responder's gaze tracks the attacker after its glance ends.
- **AC-CNB-19** [Visual/BLOCKING] `Crowd.StrikeReactionLook`: GIVEN the graybox market and a strike in view at 10 m, WHEN recorded from a bystander at 20 m, THEN the clip shows glances and pauses but no gathering, no flight from the district and no NPC run longer than 3 s; retained screenshots/clip in `production/qa/evidence/` with lead sign-off.

**Bound pawns, takeover, phases**
- **AC-CNB-20** [Integration/BLOCKING] `Crowd.TakeoverBlend` (Roster AC-PSR-43, Round 22): GIVEN an Active player running mid-interaction at a 1/60 s step, WHEN the connection goes Unresponsive, THEN max |dv/dt| ≤ `CrowdStopDecel`, the hold ends with the ordinary cancel pose, the head never exceeds `LookMaxRateDegPerSec`, the pawn is never destroyed, and from takeover to hand-back it issues only `Move` and `Look`.
- **AC-CNB-21** [Logic/BLOCKING] `Crowd.CostKnobsDoNotChangeBehaviour`: GIVEN the reference scene at `CrowdDecisionPerSec` 4 and at 3, and `CrowdSteerPerSec` 30 and 20, WHEN the nine F6 features are compared, THEN each passes the F6 parity thresholds between settings.
- **AC-CNB-22** [Integration/BLOCKING] `Crowd.EndedHold` (Round R9): GIVEN 50 pawns moving, running, tending and traversing, WHEN `Ended` latches, THEN in Round's round-end step (R9; for a strike win the deciding strike's death write, and before it no AI-driven pawn changes speed or ends a hold because of the latch) every AI-driven pawn ends its holds and issues `Move(0)`, every pawn reaches idle within `RunStopSec` + traversal remainder with no deceleration above the baseline, and no emergence, absorption or reaction occurs afterwards.
- **AC-CNB-23** [Integration/BLOCKING] `Crowd.InformantReleaseAtomic` (LSC AC-53): GIVEN 100 `PickRespawnPawn` calls, WHEN candidates are released, THEN every released pawn was `Eligible` at the commit tick, none was tending, bracing, in a bush or bush transition, emerging, exiting, reacting or conversing, the StateTree is stopped and the control epoch bumped in the same tick, 0 old-epoch commands reach the pawn afterwards (an async path callback is injected after the commit in 20 of the calls), the pawn shows no pose change at the release, and in a multi-process run with network emulation (100 ms RTT) its speed never drops below 90 % of its pre-release speed in the 1 s after possession unless the new driver commanded it.
- **AC-CNB-24** [Integration/BLOCKING] `Crowd.ReleaseToCrowdNoChange`: GIVEN a disconnected player's pawn at grace expiry, WHEN `ReleaseToCrowd` runs, THEN on an observer client in the release frame and the next 10 frames |Δv| per frame never exceeds the SCL walk acceleration × Δt, the heading rate never exceeds the SCL turn rate, `ActionPose` is unchanged, and the pawn's next behaviour starts at a leg boundary.

**Performance, UI, parity gates**
- **AC-CNB-25** [UI/BLOCKING] `Crowd.NoCrowdUi`: GIVEN a shipping-configuration listen server and client, WHEN each viewer class's screen is captured during a round with kills, emergences and a takeover, THEN no crowd debug, NPC indicator or notice is drawn (retained screenshots).
- **AC-CNB-26** [Performance/BLOCKING, provisional pending spike] `Crowd.ServerCost`: GIVEN the 50-pawn reference scene in a Test build on the project's reference host (CPU model and clock recorded with the evidence; the listen-server host process only, bot clients in separate processes, never in-process PIE), frame rate locked at 60 fps, a strike burst every 20 s, WHEN Insights captures 5 min with the named scopes `Crowd.Director`, `Crowd.StateTree`, `Crowd.Follower`, `Crowd.Perception`, `Crowd.NavQuery` and `Crowd.Dispatch`, THEN their summed game-thread time is ≤ 1.5 ms per frame at p99 and ≤ 0.8 ms on average; the server movement simulation of AI-moved pawns is reported alongside (SCL budget); and a 70-pawn run locked at 30 fps is reported with the cut order applied (advisory).
- **AC-CNB-27** [Logic/BLOCKING] `Crowd.PipelineParity` (F6): GIVEN the same CNB command streams fed through the AIController and through a bot client's Enhanced Input path for 1,236+ 30 s windows each (about 10.3 h per pipeline: a nightly multi-process run with ≥ 10 bot clients), WHEN the nine F6 features are computed on an observer client, THEN every feature passes |d| ≤ 0.1, variance ratio ∈ [0.8, 1.25] and KS D ≤ 0.1.
- **AC-CNB-28** [Playtest/BLOCKING for Alpha, ADVISORY for MVP] `Crowd.ClassifierGate` (F6): GIVEN ≥ 600 human 30 s windows of ordinary walking from ≥ 30 participants and ≥ 2,400 NPC windows, WHEN each feature's held-out balanced accuracy is computed, THEN every BA ≤ 0.55 with one-sided upper bound ≤ 0.58; results kept in `production/qa/evidence/`.
- **AC-CNB-29** [Logic/BLOCKING] `Crowd.StreamsKillIndependent`: GIVEN a fixed-timestep harness and two runs of one seed differing only by one extra NPC kill at t = 60 s, WHEN every other NPC's counter-based draws are compared by (ordinal, decision type, decision index), THEN for every non-involved NPC the k-th draw of each decision type is identical in both runs (the decisions may differ in timing; the draw values may not).

**Added at review 2026-10-24**
- **AC-CNB-32** [Logic/BLOCKING] `Crowd.YieldRate`: GIVEN 2,000 scripted predicted approaches < 100 cm between an NPC and a walking pawn (half player-driven, half NPC), WHEN side-steps are counted, THEN the NPC yield share is `CrowdYieldPct` ± 3 points for both partner kinds, the decision is taken at first detection, and no pawn is ever pushed.
- **AC-CNB-33** [Logic/BLOCKING] `Crowd.PhaseGating`: GIVEN a round start with 40 NPCs, WHEN commands are logged, THEN no NPC issues a command before `OnRoundActive`; at `OnRoundActive` every NPC starts in the same server step with first pause or leg lengths that pass a KS test against their distributions (no unison), and decision ticks are spread so that no frame holds more than ceil(N / (FPS / `CrowdDecisionPerSec`)) + 2 decisions.
- **AC-CNB-34** [Logic/BLOCKING] `Crowd.StreamDeterminism`: GIVEN two runs with the same seed and fixed timestep, WHEN all draws are logged, THEN they are bit-identical; a build check finds no `FMath::Rand` or `FRandomStream` without a CNB key in Crowd code; and a takeover then release of one pawn leaves its later draws unchanged (streams keyed by ordinal in the Director).
- **AC-CNB-35** [Logic/BLOCKING] `Crowd.ExitCancelOnKill`: GIVEN 200 kills at random times, WHEN the ambient schedule is logged, THEN each kill cancels exactly one exit scheduled within the next 60 s (uniform over sockets, chi-square p > 0.01) within LSC R5.7's bounds, and an exiting NPC killed inside an alley adds a vacancy without counting a cancel.
- **AC-CNB-36** [Logic/BLOCKING] `Crowd.PerceptionIgnoresConcealed`: GIVEN fixed-seed scenes with a bush holding 0, 1 and 3 concealed pawns (player and NPC), WHEN NPC steering, glance targets, conversation partners and bush choices are recorded, THEN they are identical across the three occupancies, and no glance targets a pawn without line of sight.
- **AC-CNB-37** [Logic/BLOCKING] `Crowd.RatesNotAdaptedAtRuntime`: GIVEN a round in which bot players run, hide and tend at 3 × their F2 r_P, WHEN Crowd's effective draw rates are logged every 60 s, THEN they never change during the round, and no Crowd code writes a `Crowd*` tuning value after `OnTuningReady`.
- **AC-CNB-38** [Integration/BLOCKING] `Crowd.HandoverEpochAndHalt`: GIVEN 100 takeovers and 100 hand-backs with injected late path callbacks and debounced Ends, and 50 Halts with commands pending, WHEN the sink is instrumented, THEN 0 old-epoch commands and 0 non-`Move(0)` motion commands after Halt reach any pawn, and every possession change happens at a frame start, never during a traversal.

**Cross-system (Tell Matrix rows)**
- **AC-CNB-30** [Review/ADVISORY, at the cross-GDD review] `TellMatrixRows`: GIVEN R16, WHEN SCL (AC-SCL-32), IIF (AC 50), LSC (AC-61), Concealment (AC-CON-15), Assassination, Objectives (AC-OC-24), IVP (AC-IVP-34) and Penalty & Informants are reviewed, THEN each named GDD contains an acceptance criterion that cites its row(s) (T1-T16) by number; the check is a list of (GDD, row, AC id) with no missing entry.

## Open Questions

| # | Question | Owner | Resolve by |
|---|---|---|---|
| OQ1 | **Resolved 2026-10-24** (IVP review ruling A; cross-GDD conflict C7, Concealment version): the bush target is `P_hide ≤ 1/6` (IVP F5 at k = 1.5, the ceiling); k = 2.0 and the game-designer's 0.30-0.40 are declined. Crowd meets it with r_N 0.19/min, occupancy-blind choice and an uncapped stay (T5). Remaining graybox check: Crowd meets F5 (AC-CNB-15, AC-IVP-25), camping wins stay rare (AC-CON-24), sweeps stay rare but not absent; if F5 is not met at the MVP-core checkpoint, escalate to the creative-director and the user (IVP R7.6; no automatic fallback). | creative-director + IVP author | first graybox playtest |
| OQ2 | A public `AmbientSpot` verb (sit, browse) for every Active pawn in Vertical Slice? Adds a player verb (scope change: **pending user decision**); MVP ships without it under R4. | user / game-designer | VS planning |
| OQ3 | Keyboard reproducibility of paces 0.8/0.9. **Interim rule applied (review 2026-10-24, creative-director):** NPCs walk only at 1.0 and C4 is inactive (R3.1, T16). Open: add a keyboard "walk slower" modifier (adds a player control: **pending user decision**), after which `CrowdFullPacePct` returns to 55 and the Fantasy's slower pace becomes available to every player. | user; SCL + IIF authors | before the classifier gate |
| OQ4 | May a participant-bound AI pawn issue `Hide(Exit)` (ai-programmer recommendation, a human could) instead of staying concealed until release (Round R17)? Current rule: no. | Round author | Round review |
| OQ5 | **Answered by ADR-0016 §7 / ADR-0010 §4.3 (2026-10-24 ADR sync)**: possession, executed by the factory's hand-over pass at the next frame start. Original question: bound takeover by controller possession (SCL R2/R4) or by attaching an AI command source to the pawn's mailbox without re-possession? Review 2026-10-24 (ai-programmer): prefer possession, because a disconnect destroys the PlayerController and the alternative covers only Unresponsive; R10.5's epoch rule applies either way. | technical-director -> ADR | ADR-001 follow-up |
| OQ6 | Human tell rates r_P are assumed (F2); measure them and re-derive every red-herring interval within the F2 robustness bound. | game-designer + Telemetry | each playtest build |
| OQ7 | Socket utilisation is ~50 % at defaults (F3); confirm in graybox that the clear check plus 20 s cooldown sustain 9.75 entries and exits per minute. | level-designer + CNB | graybox build |
| OQ8 | **Resolved by ADR-0016 (E5, E6, E12, E14, E15; 2026-10-24 ADR sync)**: StateTree scheduled-tick behaviour verified. Earlier partial resolution (review 2026-10-24): `UStateTreeComponent::ScheduleTickFrame`, `FStateTreeScheduledTick::MakeSleep`, `UStateTreeAIComponent` and task-graph `FindPathAsync` exist in the installed 5.8.4 source. Open: add a StateTree page to the engine reference; confirm `-nullrhi` runs keep pose ticking for root-motion traversal and brace clips (AC-CNB-20, 22-24). | ai-programmer | before the Crowd ADR |
| OQ9 | Diegetic justification and animation for NPC bush entry ("stepping into the shade", gardener) without a new pose. | art-director | art bible revision |
| OQ10 | Host CPU budget: the prototype's marginal game-thread cost of about 0.19 ms per character (all systems, RTX 5090-class host) puts 60 fps with 50-70 pawns on a 3060-class host in doubt. Which fallback (lower host tick, simulation LOD that is identical for every pawn, dedicated host) keeps `NpcCount`? Lowering `NpcCount` needs a creative-director review (P1). | technical-director + performance-analyst | 50-character spike |
| OQ11 | **Answered by ADR-0016 §4 (2026-10-24 ADR sync)**: input emulation; no shared turn filter; input-device mix stays a manual playtest-sheet item until a client-reported field is approved (pending user, ADR-0023). Original question: motion parity (R3.4): emulate human input (virtual camera and quantised 8-way or analog input from device profiles) or apply an SCL turn-smoothing filter to every pawn? Which input-device mix is targeted? | technical-director -> Crowd ADR; SCL author | before AC-CNB-27 |
| OQ12 | One telemetry pipeline must supply logged player tell rates (F2), reaction and face-back rates (R6, C3) and motion profiles (R3.4); until it exists every such value is provisional. | producer + Telemetry | first internal playtest |
