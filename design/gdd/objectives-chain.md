# Objectives Chain

> **Status**: Reviewed - NEEDS REVISION 2026-10-25 (second review, lean; one blocking item fixed in place, independent re-review pending); first review NEEDS REVISION 2026-10-24 (all 8 blockers verified resolved); reconciled 2026-10-24; ADR-synced 2026-10-24; user decisions applied 2026-10-25 (`InteractSecOverride` removed from `FObjectiveDef`; `RivalPrelimCount` ON by default confirmed; `NpcCount` 50)
> **Author**: design pipeline (autonomous) + game-designer, level-designer, systems-designer
> **Last Updated**: 2026-10-25
> **Last Verified**: 2026-10-24
> **Implements Pillar**: P4 A Race, Not a Hunt (primary); P2 Every Advantage Is a Tell; P1 Hide in Plain Sight
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS -> REVISED 2026-10-24 (OC-12 rival signal APPROVED with two changes: IVP "no team" reworded to mean no member identity, delay hard floor 30 s; four fixes applied: live-site cue only after Begin (OC-6, OC-15, UI 4, AC-OC-08), clustering advice moved to onboarding, rival tick on the team-only 2D UI bus, counted vs feint tends logged separately (F3, AC-OC-21). Conditions before production: art-director review of Visual/Audio, qa-lead review of Acceptance Criteria, cultural read of card text before VS)
> **Design Review**: NEEDS REVISION 2026-10-24 (full mode: combined panel game-designer, systems-designer, level-designer, qa-lead; creative-director senior verdict). Revisions applied in place: OC-12 rewritten to IVP ruling B (`RivalPrelimCount`, fresh per-completion delay, global release quantum; CD approval for IVP R3/OQ4), F3 re-based on F1 and full-hold tends (Crowd CR6 reopened), F4 `W_eff`, `TendCueDelaySec`, full-fixture rule, SP-4 camping text, Round `Rejected_Latched`, AC rewrites. Log: `design/gdd/reviews/objectives-chain-review-log.md`; a full re-review is required. **Second review 2026-10-25 (lean, single session)**: prior items all resolved; new blocking item (OC-14 still allowed a per-definition hold override after the user removed `InteractSecOverride`) and 8 recommended items fixed in place; see the log.

## Summary

Objectives Chain is the race that drives every round: each team works through two secret preliminary stages of its own, then both race to one shared final court whose location a team learns only when its own preliminaries are done; the first valid completion wins. Every stage is a 3-second tend at an ordinary stone in an ordinary sunlit court, the loudest honest tell in the game, blurred by citizens' decoy tends and players' feints. It owns the objective and chain data, the team-only site view, the tend's predicate and completion gate, a delayed stage-level rival signal, and the stall-pressure rule: never a timer, never a reveal.

> **Quick reference** — Layer: `Feature` · Priority: `MVP` · Key deps: `Level Socket Contract, Round & Match Framework, Input & Interaction Framework, Participant State & Roster, Information Visibility Policy, Tuning Data & Config (upstream); Crowd & NPC Behaviour, Session & Travel (soft); dependents: HUD, Round, IIF, Post-Round Reveal, Telemetry`

## Overview

Objectives Chain is the server-side system that turns the city's identical stone courts into a race. It owns the **objective definition** (a reusable data record: stage, team eligibility, interaction requirements, completion state), the **mission chain** that strings definitions together (per team, two team-specific preliminary stages, then one shared final), the **site audiences** (which team may know which court is live, and when), the **Interact predicate** that decides what a 3-second tend at a court does, and the **stall-pressure rule** that keeps waiting from ever winning. It consumes the Level Socket Contract's server-secret `FSiteAssignment` (which court plays which role), the Input & Interaction Framework's hold timer, and the Round's resolution queue, and it calls `Round.DeclareObjectiveWin` when the final completes.

For the player the system is the reason to move: you know only your own next court, you must stand in its sun pool for three seconds with your hands on the stone while anyone may be watching, and you learn where the shared final lies only when your team has paid for its two preliminaries. Without it the round has no clock but the other team's knives, and Pillar 4 (A Race, Not a Hunt) has no engine. Implementation choices (team-view carrier layout, data-asset classes) are marked "-> ADR"; this GDD states behaviour only.

## Player Fantasy

**Three seconds in the light.** The court looks like every other court: a block of carved stone, a square of sun, people crossing. Only your team knows that this stone is yours. To move forward you must walk into the light, lay both hands on the stone and stay there, back to the street, for three breaths. Anyone could be watching. Most of the people who stop at stones are only citizens doing what citizens do; one of them may be you, and one of the watchers may be them.

The fantasy is the **chosen exposure**: progress is never free, never fast and never hidden, but it is also never forced on you at a time you did not pick. You decide when the court is quiet enough, who stands guard at the ingress, and whether to walk on and come back. Then the reward: the second stone done, the city tells your team, and only your team, where the ruler's cup is prepared. Now you know something the rivals do not, and the race narrows to one court that both of you must reach. And when you dither, the race reminds you: a small tick, late and placeless, says the rival house has done another of its tasks; it never says where, or when.

*Pillars served*: P4 (the race drives the round; waiting earns nothing), P2 (the tend is the loudest honest tell in the game: art bible 5.5), P1 (every court, fixture and tend looks the same for players and citizens), P3 (a strike during the tend ends the attempt and the life). *Specialist note*: under the pipeline budget rule the creative-director was not consulted section-by-section; this section is validated by the CD-GDD-ALIGN gate.

## Detailed Design

### Core Rules

*Specialists consulted (full review mode, budget rule: 3 + gate): game-designer (rules, stall pressure, public signal, chain framing), level-designer (discovery, exposure geometry, camping, pacing), systems-designer (formulas, edge cases, knobs). Disagreements and the chosen side are noted inline.*

**A. What Objectives owns**

**OC-1. Scope.** Objectives owns: the objective definition and chain data; per-team progress; the binding of definitions to courts from `FSiteAssignment`; the Team-audience site view; the `CanBegin` predicate and the completion effect for `Interact` at courts; the rival race signal (OC-12) and its release quantum; the stall-pressure rules (SP-1 to SP-8). It does **not** own: court placement or role selection (LSC), the hold timer, cancels and prompts (IIF), ordering and the win latch (Round), lifecycle state (Roster), audiences in general (Policy), NPC decoy behaviour (Crowd). All Objectives state is **server-side, round-scoped**, and reset on `OnRosterReset`.

**OC-2. The objective definition** (`FObjectiveDef`, a row in a chain data asset; -> ADR for the asset class). Reusable across chains:

| Field | Type | MVP value | Meaning |
|---|---|---|---|
| `ObjectiveId` | name | unique per chain | Stable id; logged |
| `StageKind` | enum `Prelim1 \| Prelim2 \| Final` | — | Position in the chain |
| `TeamEligibility` | enum `Team0 \| Team1 \| Both` | prelims one team, final `Both` | Which team's completion counts. A prelim of one team can never advance the other |
| `SiteRole` | enum `P1 \| P2 \| Final` | matches `StageKind` | Which `FSiteAssignment` slot binds it to a court at round start; never a fixed court |
| `CompletionsRequiredCount` | int | 1 | Completed tends needed at the site (one tend in MVP). Values above 1 are rejected at `Preparing` (`PreparingFailed(ObjectiveChainInvalid)`, as for `ObjectivesPerStageCount`) until a multi-tend rule (what a cancel or a second completer does to the count) is designed; OC-10 counts exactly one completion |
| `CardTitle`, `CardLine` | text | OC-16 | Team-only stage card text (localisable) |
| `ClueForm` | enum `AreaAndFeature` | — | How the court's public `ClueTag` (LSC request) becomes the card's clue line |

Runtime **site state**, per (team, definition): `Locked` → `Live` → `Completed` (States and Transitions). Nothing is stored on the court or the fixture.

**OC-3. The chain.** A chain asset holds, per team, `ObjectivesPerStageCount` (MVP 1) definitions for `Prelim1` and for `Prelim2`, and exactly one shared `Final`. Per team the two prelim sets are **similar in structure, not identical** (different `ObjectiveId`, card text and court, same hold and completion rules). At `OnLayoutReady` the server binds `Prelim1(T)` → `FSiteAssignment.P1[T]`, `Prelim2(T)` → `P2[T]`, `Final` → `Final`, through the LSC server query API only (LSC AC-51). MVP ships one chain (OC-16) and selects it for every round; values of `ObjectivesPerStageCount` above 1 are rejected at `Preparing` (`PreparingFailed(ObjectiveChainInvalid)`) until LSC Selection draws that many courts per stage (change request; Open Question 2).

**B. Who knows which court**

**OC-4. Progress is per team and one-way.** Each team has `StageIndex` ∈ {`Stage1`, `Stage2`, `Final`}. At `Active` entry both teams are `Stage1`: their `Prelim1` sites are `Live`, all else `Locked`. Completing every `Prelim1` definition of team T makes T's `Prelim2` sites `Live` in the same slot; completing every `Prelim2` makes the `Final` `Live` **for T only**. Nothing else changes a site's state: not elapsed time, not kills, not the other team's progress, not informant count. Progress never regresses (a completer's later death, disconnect or removal keeps the stage complete: it belongs to the team).

**OC-5. A court's location enters a team's view only when it is live for that team** (locked rule "known only to that team"). The team view (`FObjectiveTeamView`, carried by C-TeamView, IVP R4; audience `Team`, which includes the team's informants) holds exactly: own `StageIndex`; the live site ids of own current stage; own completed `ObjectiveId`s; the current stage card (title, line, clue); and `RivalPrelimCount` (OC-12). It never holds a locked site, an opponent site, opponent hold progress or the final before own preliminaries are done. Opponent progress stays `ServerOnly` except OC-12. `FSiteAssignment` stays `ServerOnly` until the Policy releases it to `PostRound` in `Ended`. **When T unlocks the final, T learns its location only** (card + clue; Marigold ring when tended), and the opponent learns nothing at that instant (OC-12 later releases the second count into its `RivalPrelimCount`). The team view carries **presentation only** (IVP R3 `SiteKnownToTeam`, ruling D): it colours the ring and fills the stage card; it never gates ingress or the predicate.

**C. The tend**

**OC-6. Every court fixture is an Interact target for every Active pawn.** The IIF ingress filter accepts any court fixture id (public geometry); it no longer needs the team's live-site set to block hidden sites (change request, Cross-References 1c), because OC-7 makes every non-live court, the hidden final included, resolve identically. The **prompt** ("Tend", IIF UI-3) appears identically at **every** court fixture in reach for an Active owner, so it confirms nothing. **The live-site cue exists only after Begin, and only after `TendCueDelaySec`** (default 1.0 s): until then the owner's ring fills in a neutral Bone for every court; from that moment it turns Marigold at an own live site and stays Bone at any other court (CD-GDD-ALIGN ruling; delay added at design review 2026-10-24, creative-director). The delay exists so that a probe's earliest informed release (about `TendCueDelaySec` + reaction) falls inside the range of Crowd's early-release decoys (U(0.8, 2.8) s) instead of a few tenths of a second after Begin, which no NPC does (P1). Invariants I-OC3: `TendCueDelaySec + 0.25 ≥ CrowdDecoyEarlyReleaseMinSec` (0.8; a Crowd key since reconciliation 2026-10-24, change request 6 (b)) and `TendCueDelaySec ≤ ObjectiveInteractSec − 1.0`. Probing a court therefore costs at least about 1.3 s of a two-handed tend and an early release; an early release is the **stronger** tell (F3), a chosen one (P2). Informants never see a prompt. *Fallback, only if UX playtests reject a cue-after-Begin: keep a reach prompt at live sites only and ask Crowd for NPC B8 touches at court stones at a rate.*

**OC-7. The predicate reads only (site, public stand-point occupancy, legality class).** `CanBegin(Pawn, Interact(Court))` = `Reject(NotNow)` if `GetLegalityClass(Pawn)` ∈ {`Informant`, `RoundEnded`}; `Reject(NotNow)` if the fixture has no free stand point (all `FixtureInteractSideCount` stand points claimed by active Interact holds; a body standing at a face without holding blocks it by collision and reach only (the IIF's `OutOfReach`) and holds no claim, so nobody can lock a fixture by parking at it. Active holds are what every observer can see, the same for every pawn and every court, so it is no oracle; OC-8; ADR-0020 §5.3, game-designer sign-off 2026-10-24); otherwise `Allow`, for **every** court whatever its role or state. It reads no team, binding or progress. Whether the tend *does* anything is decided only by the completion gate (OC-10); a completion that fails the gate is exactly the IIF's `AllowNoEffect` outcome (same pose, timer and events, no change). *(game-designer: this replaces the author's draft that chose `Allow`/`AllowNoEffect` from the bound team's state at Begin; a single gate at the slot is simpler and reads current state.)*

**OC-8. Several tenders at one fixture.** Each fixture exposes `FixtureInteractSideCount` (4) stand points, one per face (LSC request; at least 3 clear by 1.2 m, all inside the sun pool), so up to 4 pawns tend at once with independent IIF timers; a fifth Begin is `Reject(NotNow)` (OC-7), for humans and NPC decoys alike (a decoy then picks another court, Crowd CR6). **No stacking**: two teammates do not tend faster, and a second completion at a `Completed` site has no effect. **A completion never cancels other holds at the court**: they run to their own end with no effect. Cancelling them in the completion slot would make every tender at that court stop on the same frame, a correlation tell (change request to IIF edge case "teammate completes mid-hold", which assumed `TargetInvalid`). Visibility of a site to a team is read for the owner's ring colour only (presentation, IVP ruling D), never to gate a Begin or cancel a running hold. **No one-pawn chokepoint** (Penalty CR-13, reconciliation 2026-10-24): no fixture and no objective path may be closable by one standing pawn. Every fixture keeps ≥ 1.2 m clear floor on all four faces (so one body blocks at most one stand point), and every court ingress is clear by more than two capsule diameters or the court has ≥ 2 ingresses (LSC V-NAV-2 clause; the Generator already enforces it for generated layouts, S4 and AC-G38). A body standing in a doorway is a readable deviation for any pawn kind, never a lock.

**OC-9. Decoys and feints.** Crowd's NPCs tend courts chosen uniformly over **all** courts, never reading `FSiteAssignment` (Crowd T3, AC-CNB-14), at Crowd's rate, with 75% held to the full `D` and 25% released early (Crowd keys). Humans may **feint**: tend any court, with no effect, to draw or test watchers (OC-6). Feints exist because a behaviour only NPCs could perform would prove NPC-ness (Crowd CNB-R4). *(game-designer preferred no human feints so that a human tend always means "a live site"; overruled for CNB-R4 parity, and because a feint is a chosen, costly exposure: P2.)* Pillar-1 consequence accepted: a tend is evidence, never proof (F3).

**OC-10. The completion gate** runs as a follow-on inside the `HoldTimer(Complete)` slot (IIF R8; it inherits that key). The completion **counts** only if, at that slot: (a) phase `Active` and the latch not set; (b) the pawn is bound (server-only Roster lookup) to participant P with `GetState(P) = Active`; (c) P's **Session connection state** (Session & Travel state table) is `Connected` (not `Unresponsive` or `Disconnected`; Round's and the Roster's collapsed "Connected" also covers `Unresponsive` and is not the value read here; cross-review S-I4 2026-10-24) and the pawn is human-controlled (not hand-over or grace AI, Round R17), re-validated as "the pawn's control epoch is unchanged since the hold's Begin and no hand-over is pending" (Round R13; ADR-0020 §5.4 (c)); (d) the court is a `Live` site of a definition eligible for `GetTeam(P)`; (e) that definition is not yet `Completed`. Otherwise nothing changes and nothing is emitted beyond the IIF's normal `OnActionCompleted`. If it counts: **stage**: mark `Completed`, apply OC-4 unlocks, log `ObjectiveStage(team, stage, ObjectiveId, ParticipantId, Key)`, push the team view, schedule OC-12; **final**: log `FinalCompleted(team, Key)` and call `Round.DeclareObjectiveWin(team, Key)` in the same slot. Opponent clients receive nothing from either. The completer's pose ends through the IIF's normal completion blend, identical for counted and no-effect completions.

**OC-11. Locked-stage access control.** A `Locked` site is, to every pawn, an ordinary court: OC-7 allows the tend, OC-10 (d) gives it no effect, the prompt is the same as at any court, the ring is Bone, and no reply, timing or audio differs from any non-live court (hidden-site oracle guard). Unlocks happen only through OC-4, never by time, host command (except non-Shipping `DebugObjectiveCompletion`, Round R13) or opponent action.

**D. Signals, timing, presentation**

**OC-12. The rival race signal (MVP: on).** *(Rewritten at design review 2026-10-24 to IVP ruling B: the earlier `RivalStageStep`, keyed by team and step with monotonic delivery, is denied by IVP R3/L34/OQ4 and withdrawn. The creative-director approved this shape as the CD approval IVP R3 requires.)* Each team's view carries `RivalPrelimCount` ∈ {0, 1, 2}: the number of the **opponent's** counted preliminary completions that have been **released**. It never counts the final (which ends the round) and carries nothing else: no member or participant identity, site, position, order, completion time or sub-stage data.

1. **Fresh per-event delay.** Every counted preliminary completion e (either team) draws its own `Δ_e` uniformly from [`RivalSignalDelayMinSec`, `RivalSignalDelayMaxSec`] (30-165 s; F4), **in integer milliseconds** (the bounds are converted to whole ms once at tuning freeze, so no float enters the draw; ADR-0006 §4), on the server-secret sub-seed domain `StageSignal`, keyed by the **round-global completion ordinal** (1st, 2nd, … prelim completion of the round, both teams counted together), never by team, step, completer or court.
2. **One global release quantum.** A completion is released at the first tick of the global grid at or after `t_c,e + Δ_e`: `t_rel,e = t_0 + Q·⌈(t_c,e + Δ_e − t_0) / Q⌉`, where `t_0` is `Active` entry and `Q` = `RivalSignalReleaseQuantumSec` (30 s). The grid is the same for both teams; its position leaks nothing.
3. **Count, not order.** At each release the opponent's `RivalPrelimCount` becomes the number of its released completions. Releases are unordered, so no `max()` coupling exists between steps; the count only rises, and may rise by 2 in one tick.
4. **No clock.** A release is a Round `TimerFired(StageSignal)` item owned by Objectives, scheduled at `t_rel,e` **only while a publication is pending**; with nothing pending, no Objectives timer exists (SP-3, AC-OC-16). Pending releases are discarded at the latch.
5. **Audience.** Team carrier only (C-TeamView, the team's informants included); never in `FRoundPublicState` (AC-IVP-21). `bRivalSignalEnabled` = false removes it entirely.

Rationale: the signal is the only pressure a stalled team feels ("they are ahead") without any reveal; the fresh delay plus the quantum keep it from confirming a seen tend (F4), and it never says where or when. The IVP R3 Team row for `RivalPrelimCount` was added at reconciliation 2026-10-24 (Cross-References change request 3; IVP OQ4 closed, L34 mitigated). The signal being ON by default in MVP was confirmed by the user 2026-10-25.

**OC-13. Stall pressure** is specified as rules SP-1 to SP-8 in the Stall Pressure subsection below.

**OC-14. Interaction time.** The hold is `ObjectiveInteractSec` (default 3.0 s, latch `Action`), for every definition, court and team: **no per-definition hold time exists** (`InteractSecOverride` was removed from `FObjectiveDef` by user decision 2026-10-25, because a per-definition value would make `D` depend on the court's role, a timing oracle: IVP R7.4a, AC-OC-08; a chain-wide client-visible override would need a new decision). **The hard range becomes 2.0-6.0 s, safe 2.5-4.5 s** (answers TDC OQ-9; 6.0 matches the IIF F1 `D` range): Crowd's B8 ambient touch lasts 0.8-1.5 s and the art bible (5.5) reads an objective tend as a pose held longer than 1.5 s; at 1.0 s a tend would be indistinguishable from a touch, an advantage without a readable deviation (P2 design test). Fast automated tests use the non-Shipping `DebugObjectiveCompletion`, not a short hold. Invariant I-OC1: `ObjectiveInteractSec >= CrowdAmbientTouchMaxSec + 0.5` (change request to TDC).

**OC-15. Presentation and discovery.** No compass, waypoint, map marker or world plate. A team finds its live court from the **stage card** (team-only HUD: title, one line, and a clue "A court [ClueTag phrase]" built from the court's public `ClueTag`: area type plus nearest distinctive public feature, for example "in the market, by the water steps"), then confirms it by tending: the Marigold ring after Begin and `TendCueDelaySec` (OC-6), never by a prompt at reach. The clue must match at most `ClueMatchMaxCount` (3) courts of the layout; a layout where it cannot is a `ClueTag` defect for LSC to fix in data, not a reason to show more. *(level-designer offered an optional 12 m team-only plate; game-designer rejected any plate as contradicting art bible 6.8; chosen: no plate in MVP, revisit if median search time exceeds F1's budget.)* The fixture shows nothing stateful (no replicated state, LSC R2.6; IVP L24): identical relief on every court, no tended mark, no stage props unless the same props sit on every court.

**OC-16. The MVP end-to-end chain: "The Ruler's Cacao".** Two rival houses race to spoil the ruler's cup. Framing rules: intrigue, never spectacle; the ruler is unnamed and never shown; no sacrifice, deity, real glyph or Nahuatl text (art bible 6.9); "cacao" is an English loanword; card text never names a place, because any court can hold any role. Prompt verb is always **Tend**; card text never uses objective, target, enemy, player, NPC (IIF language rule).

| Stage | Team | `ObjectiveId` | Card title | Card line |
|---|---|---|---|---|
| Prelim1 | 0 | `Cacao.T0.PayRunner` | Pay the Runner | Leave the beans for the steward's runner. |
| Prelim2 | 0 | `Cacao.T0.GatherRoot` | Gather the Root | Collect the bitter root set aside for your house. |
| Prelim1 | 1 | `Cacao.T1.HearRunner` | Hear the Runner | Hear the palace runner's news at a quiet stone. |
| Prelim2 | 1 | `Cacao.T1.TakeGourd` | Take the Gourd | Take the gourd left for your house. |
| Final | Both | `Cacao.Final` | The Ruler's Cacao | Spoil the ruler's cup before the rival house does. |

Both houses see identical final card text and clue. The house names are "your house" and "the rival house" only.

**OC-17. Lifecycle.** Bind at `OnLayoutReady` (server only); team views are pushed at `Active` entry (not in `Preparing`, so no client knows a site before the round starts); resolution only in `Active`; in `Ended` the Policy may release the stage timeline and assignment to `PostRound` (VS reveal); everything clears on `OnRosterReset`. A rejoining participant receives its team view with the C-TeamView rebuild (Roster R6).

### Stall Pressure

The anti-camping mechanism, as testable rules. **Never a timer, never a reveal** (P4 design test): no system adds a round clock, a countdown, a dwell timer, a time-triggered unlock, a reveal of positions or identities, or an escalation keyed to elapsed time in response to waiting. Concealment and Round state they add none (Round R16).

- **SP-1 Hidden final.** The final court is in team T's view if and only if T has completed every preliminary definition. Before that, no fact distinguishes it from any other court for T: no prompt difference, no Marigold ring, no effect, no reply code, timing, audio, NPC density or decoy rate difference (OC-7, OC-11; Crowd and LSC never read the assignment).
- **SP-2 Nothing secret to camp.** A team's preliminary courts are never in the opponent's view (`ServerOnly`), and prelims are at least 50 m by path from the enemy zone anchor (LSC R9.3). The only courts an opponent can predict are the publicly guessable final-eligible set (LSC floor `FinalCandidatesMinCount` 6, raised from 4 at the 2026-10-24 reconciliation; the Generator targets 6, `CityGenFinalEligibleTargetCount`; at least 3 distinct finals given a team's own chain, no court above 50%: LSC D8). With the Generator's default 2 corner zones the final-eligible courts all lie in the two axis-corner quadrants (Generator S4 geometry consequence), so both teams know the final **region** in advance, equally; whether that turns into early pre-positioning is Generator Open Question 5's measured pillar question (4-zone default if pre-positioning before a preliminary exceeds 25 % of rounds).
- **SP-3 Waiting earns nothing.** Objective state, unlocks and team-view content change only on a counted completion (OC-4, OC-10) or on an OC-12 release, which is itself caused by a completion (with no completion pending, nothing is scheduled). No value grows with elapsed time.
- **SP-4 Camping the final cannot win the race.** A team that has not unlocked the final cannot complete it (OC-10 d). A single camper guesses 1 in 6 or more final-eligible courts (LSC `FinalCandidatesMinCount` 6 since the 2026-10-24 reconciliation, above `TeamSize`; Generator `CityGenFinalEligibleTargetCount` 6); a whole team could still post one camper on five of the six candidates, but never all of them. That is a hunt strategy which P4 makes unprofitable but does not forbid: its price is zero objective progress for the camping team, five static posts each standing longer than `CrowdMaxIdleSec` (20 s) without context (a tell, Crowd T14), and it wins only by eliminating every Active rival. No penalty, timer or reveal is added against it (that would break "never a timer, never a reveal"). Change request to LSC: consider `FinalCandidatesMinCount` > `TeamSize`; F5 and telemetry measure it (Open Question 9). Final-eligible courts should have at least 3 ingresses (LSC change request; the Generator's junction courts already give 3, S4). A bush-free radius, if adopted, is one **role-blind** `BushCourtExclusionCm` applied to every court, never a final-only rule: bush-free courts would otherwise mark the final candidates (Generator request 11a, reconciliation 2026-10-24).
- **SP-5 The race is felt, not shown.** OC-12 tells a stalled team that the rivals are ahead, at stage level and late (30-195 s); it never tells it where or when. A rival count of 2 means "they may be at the final": if playtests show trailing teams turning to blanket-camping the final candidates instead of racing, the first lever is disabling the signal (SP-7 (1), Open Question 1).
- **SP-6 Both teams stall.** If neither team completes anything, nothing intervenes in game: the round continues, the host may abort (Round `bHostMayAbortActive`), and F5 logs it. A `StallFlag` round (Round F7) or an F5 mutual stall is an **Objectives defect**, answered only by the SP-7 levers.
- **SP-7 Response protocol for a stall defect** (in order, all structural): (1) `RivalSignalDelay*`, `RivalSignalReleaseQuantumSec` and enablement (within the F4 bound); (2) `ObjectiveInteractSec` within its safe range; (3) LSC final-eligible ingress and bush rules; (4) `ClueMatchMaxCount`; (5) LSC chain band and candidate counts. Never a timer, a reveal, a ping or a marker.
- **SP-8 Hiding is never a winning line** (Concealment AC-CON-24). A concealed pawn cannot tend (Interact is rejected inside a bush by Concealment, and no bush lies within 6 m of a court), so a team whose Active players hide makes no progress, while the other team's chain is untouched by their hiding. A team can therefore win only by completing the final or by striking, and every strike from a bush breaks concealment. This holds when a team's last survivors are all concealed: the opponent wins by finishing its chain.

### States and Transitions

**Site state, per (team, definition)** (server-only):

| From | To | Trigger | Guard |
|---|---|---|---|
| — | `Locked` | bind at `OnLayoutReady` | definition eligible for the team |
| `Locked` | `Live` | `Active` entry (`Prelim1`); own team completes every `Prelim1` (`Prelim2`); own team completes every `Prelim2` (`Final`) | in the completing slot (OC-4) |
| `Live` | `Completed` | a counted completion (OC-10) | gate (a)-(e) |
| any | (cleared) | `OnRosterReset` | — |

No transition back; no time trigger; `Ended` freezes all states.

**Team progress** (`StageIndex`): `Stage1` → `Stage2` → `Final` → (round ends by `ObjectiveWin`, or by the other team's win). **Rival count** (`RivalPrelimCount`, per viewing team): 0 → 1 → 2 (possibly 0 → 2 in one grid tick), changing only at OC-12 release ticks; pending releases are discarded at the latch.

### Interactions with Other Systems

| System | Flows in | Flows out | Owner of the interface |
|---|---|---|---|
| Level Socket Contract | `FSiteAssignment` (server API), court list, fixture sides, `ClueTag` (requested) | nothing (Objectives never writes LSC data) | LSC |
| Input & Interaction Framework | `Interact` Begin/End at a court id; `HoldTimer(Complete)` slot; `OnActionCompleted/Cancelled` | `CanBegin` result (OC-7); `D`; `TendCueDelaySec`; Team live-site set for ring colour after the cue delay only (presentation, IVP ruling D) | IIF (hold), Objectives (predicate, effect) |
| Round & Match Framework | slot, key, phase, latch | follow-on effect in the slot; `DeclareObjectiveWin`; `TimerFired(StageSignal)` items; log entries | Round |
| Participant State & Roster | `GetLegalityClass`, binding lookup, `GetState`, `GetTeam`, `Connection` | none | Roster |
| Information Visibility Policy | audiences; C-TeamView carrier | `FObjectiveTeamView` contents (OC-5) | Policy (rules), Objectives (content) |
| Crowd & NPC Behaviour | NPC `Interact` commands (decoys) | `Allow` at every court; no-effect completions | Crowd (behaviour), Objectives (predicate) |
| Tuning Data & Config | OC keys, `ObjectiveInteractSec` | — | TDC |
| HUD | — | team view (stage card, own 3 steps, `RivalPrelimCount` 0-2) | HUD (layout) |

## Formulas

All five are estimates or reporting metrics; none is a runtime rule except F4's delay window and release quantum (OC-12). Provisional inputs (κ, φ, o, q, r, t_settle) are set from the first graybox playtests (Open Question 4).

### F1. Chain duration estimate

The chain_duration formula is defined as:

`T_chain = κ·L / (v·φ) + n·(D + o) + n·(q / (1 − q))·(D/2 + r)`; rush floor `T_rush = κ·L / v_run + n·D`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| L | float (m) | 120-240 | LSC `ChainLengthMinCm`/`MaxCm` (target 170) | Team chain length (LSC D1 `C_T`) |
| κ | float | 1.1-1.4 | provisional 1.2 | Real path over LSC path (crowd friction, detours) |
| v / v_run | float (m/s) | 1.4 / 4.5 | Shared Character & Locomotion (`WalkSpeedCmPerSec` 140; `RunSpeedCmPerSec` 450, safe 400-500; `v_run` was 3.5 here until cross-review C-W20, 2026-10-25) | Walk and run speed |
| φ | float | 0.3-0.7 | provisional 0.45 | Share of travel time actually moving (waiting out sightlines) |
| n | int | 3 | calculated (2 prelims + final, `ObjectivesPerStageCount` 1) | Holds in the chain |
| D | float (s) | 2.0-6.0 | `ObjectiveInteractSec` (3.0) | Hold time |
| o | float (s) | 10-60 | provisional 20 | Per-stage locate and observe (clue search, OC-15), including about 1 wrong-court probe per stage at `ClueMatchMaxCount` 3 (each a short tend of about 1.5 s plus a walk; recheck the 10 s floor against logged leg times, Open Question 4) |
| q | float | 0-0.4 | provisional 0.15 | Probability a hold is cancelled |
| r | float (s) | 5-30 | provisional 15 | Reposition after a cancel |

**Output Range:** 307 s (L 120) to 535 s (L 240) at the provisional inputs; `T_rush` 41 s (L 120), 54 s (L 170) and 73 s (L 240), the absolute floor (running is the loudest tell and must stop for every hold); recomputed at `v_run` 4.5 m/s, cross-review C-W20 (was 50-91 s at 3.5 m/s, which put the floor 18-19 % too high; the walk branch at 1.4 m/s was already right, so `T_chain` 307-535 s and `T_round` about 7-11 min are unchanged). A round ends at the faster chain, plus contest time at the final. **`T_round` ≈ `T_chain` + about 2 min of final contest (about 7-11 min, median about 9 min) is the round length F3 and F4 consume**; they never use the non-binding 12-minute `TargetRoundMinutes` (the concept's about 12 minute figure is a tuning target, not a model). **This F1 is the source of the about 9 minute median** that Round F7's pacing and stall thresholds and Pillar 4's validation cite (decision D29, cross-review D-W9).
**Example:** L 170: walk 1.2·170 / (1.4·0.45) = 324 s; holds 3·(3 + 20) = 69 s; restarts 3·0.176·16.5 = 8.7 s; total 402 s (6.7 min). Rush floor at L 170: 1.2·170 / 4.5 + 3·3 = 45.3 + 9 = 54.3 s. With about 2 min of final contest, about 9 min, below the non-binding 12. **Finding (systems-designer, game-designer):** travel cannot carry 12 minutes; raise the LSC chain target toward 220 m (8.3 min) if playtest medians of objective-ended rounds fall below 6 min (weigh it against LSC D4's walk share: at T = 9 min a 220 m chain is S = 0.29, above D4's "about a quarter" walking-contest threshold, cross-review C-W23), then `ObjectivesPerStageCount` 2, then a vaguer clue (SP-7 order does not apply: this is pacing, not a stall).

### F2. Exposure per stage

The exposure formula is defined as:

`E_court = t_in + t_settle + D + t_out`; `E_tell = t_settle + D`; `E_team = Σ_stages E_court + N_cancel·D/2 + N_probe·(t_in + t_settle + t_probe + t_out)`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_in, t_out | float (s) | 1.5-4.0 | calculated (3-4 m from ingress to a fixture side at 1.4 m/s; default 2.5) | Walk into and out of the court |
| t_settle | float (s) | 0.5-1.5 | provisional 1.0 | Stop and face the stone before Begin |
| D | float (s) | 2.0-6.0 | `ObjectiveInteractSec` | Hold |
| N_cancel | int | ≥ 0 | measured | Cancelled holds in the chain |
| N_probe | int | ≥ 0 | measured (about 1 per stage, 3 per chain) | Wrong-court probes released after the cue (OC-6) |
| t_probe | float (s) | 1.3-2.0 | `TendCueDelaySec` + reaction (default about 1.5) | Hold time of a probe before release |

**Output Range:** `E_court` 5.5-15.5 s per stage (9 s default), `E_tell` 2.5-7.5 s (4 s default); team chain 27 s in courts, 12 s in the tell pose, plus 1.5 s per cancel and about 7.5 s in court (1.5 s in the pose) per probe: about 50 s in courts per chain with 3 probes.
**Example:** 2.5 + 1.0 + 3.0 + 2.5 = 9 s in the court; only 4 s is the tell. Observers have 2-4 ingress lanes of up to 32 m each (level-designer).

### F3. Tend posterior (decoy and feint noise)

The tend_posterior formula is defined as (by duration class; revised at design review 2026-10-24):

`P_full = R_full / (R_full + N_npc·r_N·c)`; inverse `r_N = R_full·(1 − P_full) / (N_npc·c·P_full)`; `P_short = R_short / (R_short + N_npc·r_N·(1 − c))`; with `R_full = n_full / T_round`, `R_short = (n·q/(1 − q) + N_probe) / T_round`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| T_round | float (min) | 7-11 | calculated from F1 (`T_chain` + about 2 min of contest; default 9) | Round length the rate is spread over; never the non-binding 12-min target |
| n_full | float | 3-5 | calculated (3 counted holds + about 0.6 full-length feints; default 3.6) | Opponent team's full-length tends per chain |
| R_full | float (/min) | 0.30-0.55 | calculated (3.6 / 9 = 0.40) | Opponent team's full-length tend rate |
| R_short | float (/min) | 0.25-0.55 | calculated ((0.53 + 3) / 9 ≈ 0.39) | Opponent team's early-release tends (cancels plus probes, F2 `N_probe`) |
| N_npc | int | 35-50 (TDC safe range; hard 6-100) | `NpcCount` (50) | NPCs. `r_N` is per NPC and tuned for 50: at 35 NPCs and the same interval `P_full` = 0.77 (above the band), so any `NpcCount` change rescales `CrowdDecoyTendIntervalSec` in proportion (as the 40 → 50 change did) |
| c | float | 0.6-0.9 | Crowd decoy completion share (75 %) | Share of decoy tends held to the full `D` |
| r_N | float (/min/NPC) | ≥ 0 | Crowd (`CrowdDecoyTendIntervalSec` 13,125 s → 0.0046 at 50 NPCs, 2026-10-25; was 10,500 s → 0.0057 at 40 NPCs, and 18,750 s → 0.0032 before that) | Decoy tends per NPC per minute |
| P_full | float | 0.65-0.75 (design band, target 0.70) | calculated | Probability that a seen full-length tend is an opponent player |
| P_short | float | reported (alarm above 0.90) | calculated | Probability that a seen early-release tend is an opponent player |

**Output Range:** the band 0.65-0.75 applies to **full-length tends**, the tell that matters (P2) without making it proof (P1); it is the overlap of Crowd T3's band and the systems-designer's ceiling of 0.75 (F4's bound is computed at 0.70). An early release (a cancel or a probe after the cue) is a chosen, **stronger** tell: `P_short` is reported on its own line, not banded; if it measures above 0.90, the first lever is Crowd's early-release share (more decoys released early), never a lower decoy rate (Open Question 10).
**Example:** at Crowd's earlier 0.0032: `P_full` = 0.40 / (0.40 + 50·0.0032·0.75 = 0.12) = 0.77, above the band. The earlier F3 computed `R_P` = 0.30 from a 12-min round that F1 contradicts, so Objectives' change request 6 asked Crowd for the wrong rate. For `P_full` = 0.70: r_N = 0.40·0.30 / (50·0.75·0.70) = 0.0046, i.e. `CrowdDecoyTendIntervalSec` ≈ 13,125 s (reopened change request 6, rescaled to `NpcCount` 50). Then `P_short` = 0.39 / (0.39 + 50·0.0046·0.25 = 0.057) = 0.87 (reported). Boundaries: `T_round` 7 min gives `R_full` 0.51 and `P_full` 0.75 at r_N 0.0046; 11 min gives 0.33 and 0.66, both in band. Two tends at one court within 5 min are almost never both NPC (≈ 1e-3 at uniform court choice): an accepted P2 tell.

### F4. Rival-signal correlation bound

The signal_likelihood formula is defined as (revised at design review 2026-10-24 for the fresh per-event delay and the global release quantum, OC-12):

`LR_sig = 1 / (1 − e^(−λ_c·W_eff))`, `W_eff = (RivalSignalDelayMaxSec − RivalSignalDelayMinSec) + RivalSignalReleaseQuantumSec`; requirement `W_eff ≥ −ln(1 − 1/LR_max) / λ_c,min`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| Min, Max | float (s) | 30-120, 70-300 | data file (30, 165) | Per-completion delay bounds (OC-12 1) |
| Q | float (s) | 10-90 | data file (30) | Global release quantum (OC-12 2) |
| W_eff | float (s) | ≥ 100 (a preset below 100 is rejected, never clamped: TDC CR-6, I-OC2) | calculated (165) | Width of the interval a release tick leaves for the completion time: the uniform window plus the grid wait |
| λ_c | float (/s) | 0.0033-0.0055 | calculated (2 releases per about 6-10 min) | Background rate of rival releases; λ_c,min = 0.0033 is the slow round, the binding case |
| LR_max | float | 2.4 | design bound | Largest evidence a matched release may add |
| LR_sig | float | ≥ 1 | calculated | Evidence gained by matching a seen tend to a release |

**Output Range:** bound `LR_sig ≤ 2.4`, so that a full-length tend seen at F3's `P_full` = 0.70 (odds 2.33) combined with a matched release stays at or below a posterior of 0.85 (odds 5.6); requires `W_eff ≥ 0.539 / λ_c,min` = 163 s. Mean delay is Min + W_eff/2 ≈ 112 s; the latest release is Max + Q = 195 s. Below the bound the signal confirms sightings (P1 risk); far above, it arrives too late to press anyone. If `P_full` is measured above 0.70, the bound tightens (at 0.76, `LR_max` = 1.8). λ_c is a Poisson approximation of only 2 releases per team; the creative-director's per-tick check (likelihood ratio ≈ 1/(λ_c,min·(Max − Min)) = 2.24) agrees, and the coarse formula is the stricter.
**Example:** defaults W_eff = 165 − 30 + 30 = 165 s: at λ_c 0.0033, λ_c·W_eff = 0.545, 1 − e^(−0.545) = 0.420, `LR_sig` = 2.38 (pass); at 0.0055, 1.68. The withdrawn 40-140 s window with no quantum (W 100): 3.56 at λ_c 0.0033 (fail).

### F5. Stall metrics (reporting only, never a rule)

The stall_metrics formulas are defined as:

`G_mutual = max gap between consecutive counted completions of either team, with t = 0 (Active start) and t_end included`; `G_team(T)` = the same gap over team T's completions only (reported, not part of the defect test: one team stalling while the other progresses is a race, not a stall); `FinalDwell = t_end − min_T(t_P2,T)`; `StallDefect = StallFlag ∨ (G_mutual > ObjectiveStallGapDefectSec) ∨ (FinalDwell > ObjectiveFinalDwellDefectSec)`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_k,T | float (s) | ≥ 0 | Round Event Log `ObjectiveStage` / `FinalCompleted` | Team T's completion times since `Active` |
| t_end | float (s) | > 0 | Round Event Log `WinLatched` | Round end |
| StallFlag | bool | — | Round F7 | Round's long-round flag |
| ObjectiveStallGapDefectSec / ObjectiveFinalDwellDefectSec | float (s) | 300 / 240 | data file | Defect thresholds |

**Output Range:** all ≥ 0; a healthy round has at most 5 counted completions (4 preliminaries and the final) in about 7-9 min (typical gap about 90 s; 300 s is over 3×). The end reason (`ObjectiveWin` vs `Elimination`) stays Round F7's `ObjectiveEndShare` (target > 0.5); a final `Complete` rejected by the latch is logged as `TendCompleted(…, NoEffect)` and counts in tend statistics, not as a counted completion, so it is not an extra `FinalCompleted` row and does not move `t_end` or `FinalDwell` (cross-review C-W35).
**Example:** team 0 completes P1 at 95 s and P2 at 260 s; team 1 completes P1 at 180 s and P2 at 330 s; team 0 completes the final at 420 s (`t_end`): `G_mutual` = 95 s, `G_team(0)` = 165 s, `G_team(1)` = 180 s, `FinalDwell` = 420 − 260 = 160 s: no defect. A round with no completion before an elimination at 400 s: `G_mutual` = 400 s: defect, reviewed under SP-7.

## Edge Cases

*Ordering cases follow the Round key (`ServerTickIndex`, `Seq`); validity is judged at the slot, never retroactively.*

- **If a hold's `HoldTimer(Complete)` and a strike on the tender fall in the same tick**: the lower key acts first. Complete first: the completion is judged by OC-10 and counts (a stage stays banked; a final latches the win and the strike returns Round's `Rejected_Latched`, owner reply `NotNow`). Strike first: the hold cancels `PawnRemoved`/`Illegal`, no completion.
- **If the final completes and its completer is killed later in the same drain**: the win latched in the earlier slot stands; the kill item returns `Rejected_Latched` (Round R15, queue-item lifecycle). No reversal of the result.
- **If a stage completes and its completer then dies, disconnects or is struck out**: the stage stays `Completed` and the unlock stands (OC-4).
- **If both teams' final holds come due in the same frame**: stamped by `(DueTime, Begin key)`; the first counted completion latches `ObjectiveWin`; the second returns `Rejected_Latched` and its hold ends by the normal blend. No tie. **The rejected final `Complete` is logged as a tend** (cross-review C-W35): Round's server-only `TendCompleted(ParticipantId, court, NoEffect)` entry is written for it (Round adds the log rule; Objectives requests it, change request 4), so F3's tend counts and F5's `ObjectiveEndShare` read include the losing final instead of under-counting it.
- **If both teams complete preliminary stages in the same tick**: both count independently (different courts; LSC: no court holds two roles); log order follows keys.
- **If two holds at the same live site come due in the same slot sequence**: the first counts; the second finds the site `Completed` and has no effect, with no log entry (OC-8).
- **If a teammate completes the site while another teammate is mid-hold**: the other hold continues and completes with no effect; it is not cancelled (OC-8). The second tender's card changes at once, so they can release by choice.
- **If the tender becomes Unresponsive (3 s silence) mid-hold**: Round hands the pawn to AI by the normal blend and the IIF cancels the hold; progress resets to 0. If `Complete` falls in the slot where the participant is Unresponsive or Disconnected, OC-10 (c) gives no effect.
- **If the tender rejoins inside the grace**: control returns by blend; the hold does not resume and must be begun again; team progress is unchanged; the team view is re-sent with the C-TeamView rebuild: Objectives subscribes to the Roster's server-only `OnViewRebound(ParticipantId)` notification (Roster R12) and writes that participant's team view inline in the handler (ADR-0020 §6).
- **If control flips to hand-over AI exactly at the `Complete` slot**: "human-controlled" is evaluated at the slot: no effect.
- **If the tender's legality class changes mid-hold** (killed, struck out, round end): the IIF cancels `Illegal`; teammates' holds elsewhere continue; banked stages remain.
- **If every Active member of a team is gone after it unlocked the final**: Round's elimination rule ends the round for the other team; informants cannot complete (OC-7 Reject).
- **If an informant presses Interact at a court**: `Reject(NotNow)` and the IIF informant plate ("Informants can only observe"); no prompt was shown.
- **If a human tends a locked, completed, opponent or hidden-final court**: the tend runs fully (pose, ring in Bone, timer) and completes with no effect; the reply, timing and log treatment are identical for all four (OC-11, SP-1). No information about the court's role is returned.
- **If a release lands within `ReleaseDebounceSec` (0.10 s) before `D`**: the End is still pending when `Complete` resolves, so the hold completes (IIF R8); at exact equality of `DueTime`, Complete resolves first (change request to IIF).
- **If a rival-signal release tick falls after the latch**: the pending release is discarded with the round (no item is scheduled after the latch; a `TimerFired` already queued returns `Rejected_Latched`); nothing is published in `Ended` except through the Policy's `PostRound` release.
- **If a team's second preliminary completion draws a shorter delay than its first**: releases are unordered (OC-12 3): whichever releases first raises the opponent's `RivalPrelimCount` to 1, the other to 2; nothing couples the two draws.
- **If both of a team's releases fall on the same grid tick**: the opponent's `RivalPrelimCount` goes from 0 to 2 in one update with one UI tick (UI 3).
- **If a team completes its preliminaries while the opponent has already won**: impossible to apply; items after the latch return `Rejected_Latched`.
- **If every stand point of a fixture is occupied** (4 pawns tending, any mix of humans and NPCs): a fifth `Interact(Begin)` there is `Reject(NotNow)` at `CanBegin` (OC-7), identically at every court and for every pawn; occupancy is public, so the reply reveals nothing. An NPC decoy picks another court (Crowd CR6).
- **If a face's claim is free but a body is standing on it without holding**: the Begin is still allowed when the tender passes the IIF reach and facing check; the claim is bookkeeping only, and the pose plays where the tender stands, not at the face (OC-7, ADR-0020 §5.3).
- **If a pawn stands still in a court ingress or at a fixture face** (any kind: player, informant, NPC, a held grace pawn): it blocks at most one stand point and never the court, because every fixture has four faces with ≥ 1.2 m clear floor and every court has a second ingress or one wider than two capsule diameters (OC-8, Penalty CR-13); others walk round it. No blocking-specific rule or reply exists (it would be a tell).
- **If a human releases a probe before `TendCueDelaySec`**: the ring was Bone throughout and nothing was learned; the release is an ordinary early release (OC-6).
- **If the listen-server host leaves**: the session ends (`EndReason = SessionEnded`, Round R17.8); pending signals are discarded; no host migration in MVP. *Stated honestly*: the host process holds `FSiteAssignment` in memory, so a modified host client could read every site; this is a code discipline until the Dedicated Server (Alpha), as for the Round Event Log.
- **If a cheat client reads its own team view**: it learns only what its team may know (own live site ids). The exact id gives a slightly faster search than the clue; accepted (game-designer: hiding team-entitled data from honest players only buys fairness theatre).
- **If a 1v0 dev round runs (`bAllowEmptyTeamDev`)**: the chain runs for the one team; `bRivalSignalEnabled` is forced false (no `RivalPrelimCount` is published); the round is excluded from F5 metrics.
- **If the layout falls back to the graybox (LSC R8)**: identical rules; the assignment is still drawn per round from the secret stream.
- **If a clue matches more than `ClueMatchMaxCount` courts in a layout** (possible only for a layout that reached Objectives without LSC V-CLUE, which rejects any generated layout whose `ClueTag` is shared by more than `ClueMatchMaxCount` courts; for example a hand-built or debug map): the round proceeds with the clue (never a sharper hint); the layout is logged `ClueTooBroad` once per layout hash for LSC data fixing (AC-OC-28).
- **Pillar-1 leak: if anything at the court changes on a counted completion** (VFX, sound, NPC reaction, fixture state, pawn replication): that is a defect. Counted and no-effect completions take the same slot path with no extra replication to non-team clients (AC-OC-12).
- **Pillar-1 leak: if a team clusters at its live court** (five pawns converging): that is a chosen tell, not a system leak; "one tends, others watch the ingresses" is taught by onboarding or a loading tip (Onboarding & Practice Mode), never by card text.

## Dependencies

### Upstream (Objectives depends on)

*Status columns are snapshots from the 2026-10-24 reviews; `design/gdd/systems-index.md` is authoritative.* Peer, no dependency: Informant Pings & Team Communication (MVP since 2026-10-25) draws a team-only place ring and reads nothing from Objectives (OQ 8).

| System | Data interface Objectives consumes | Hard/Soft | Status |
|---|---|---|---|
| Level Socket Contract (#8) | `FSiteAssignment` {Final, P1[2], P2[2], ZoneTeam} through the **server query API only** (LSC AC-51); court geometry, fixture transform and interact sides from the Manifest-built court | Hard | In Review |
| Round & Match Framework (#6) | Resolution queue (key, slot, follow-ons inline); phase `Active`; `DeclareObjectiveWin(Team, Key)`; latch (items after it return `Rejected_Latched`; `Dropped` is only `WrongPhase`/`StaleEpoch`); Round Event Log entries `ObjectiveStage`, `FinalCompleted`; F7 `StallFlag`; `OnRosterReset` reset hook; server sub-seed domain for OC-12 (new domain `StageSignal`, change request via Session F5); re-validation of the tender at the slot (R13 `ObjectiveCompletion` row: Active, Connected, human-controlled, team, site live and not completed = OC-10 (a)-(e)) | Hard | In Review |
| Input & Interaction Framework (#3) | `Interact(Begin/End)` hold, `HoldTimer(Complete)` slot, `CanBegin` registration, ingress `TargetId` filter, `OnActionCompleted/Cancelled(Pawn, Interact)`, owner prompts and ring | Hard | In Review |
| Participant State & Roster (#2) | `GetLegalityClass(Pawn)`, `GetState`, `GetTeam`, `Connection` projection, server-only pawn binding lookup | Hard | In Review |
| Information Visibility Policy (#7) | Audiences: own `ObjectiveProgress` and live sites `Team` via C-TeamView; opponent progress `ServerOnly` except the OC-12 `RivalPrelimCount` in the ruling-B shape (R3 Team row added at reconciliation 2026-10-24; IVP OQ4 closed); `SiteKnownToTeam` presentation only, ingress accepts any court (R7.4a, ruling D); `PostRound` release in `Ended` | Hard | In Review |
| Tuning Data & Config (#1) | `ObjectiveInteractSec` and the OC-owned keys (Tuning Knobs), read via `GetServerTuning()` / `GetClientTuning()`; latch classes per CR-11 | Hard | In Review |
| Crowd & NPC Behaviour (#11) | NPC decoy tending through the same `Interact` path, assignment-blind (OC-9); Tell Matrix row T3 "objective interaction"; decoy rate r_N (F3; 0.0046/min at `NpcCount` 50), full-fixture refusal and early-release share (reopened CR6 applied there) | Soft (without it there are no decoys and every tend is a player: playtest-only) | In Review |
| Session & Travel (#5) | `ConnectionState` (via the Roster projection) and rejoin; `ServerSubSeed(StageSignal, …)` | Soft | In Review |
| Concealment (Bushes) (#12) | No tend while concealed (Concealment rejects Interact in a bush); Objectives' SP-8 is the only push out of hiding (AC-CON-24) | Hard peer for P4 | In Review |

### Downstream (these depend on Objectives)

| System | What it consumes | Hard/Soft | Status |
|---|---|---|---|
| Round & Match Framework (#6) | `ObjectiveCompletion` follow-on in the `HoldTimer(Complete)` slot; the final's call to `DeclareObjectiveWin`; Objectives owns stall pressure (bidirectional) | Hard | In Review |
| HUD (#18) | Team view: own stage index, stage card text (verb + clue), live-site ids (used only for the Marigold ring after `TendCueDelaySec`; no plate, OC-15), OC-12 `RivalPrelimCount`; never anything else about opponents (HUD-R11 matches UI 1-3: 3 own steps, 2 rival steps, rise by 2 in one tick, rival steps 0 in a 1v0 dev round) | Hard | In Review |
| Input & Interaction Framework (#3) | Team-audience live-site set (ring colour after `TendCueDelaySec`, presentation only), `CanBegin` predicate (incl. full-fixture reject), `D` (bidirectional) | Hard | In Review |
| Crowd & NPC Behaviour (#11) | `AllowNoEffect` acceptance at every court; fixture interact sides; full-fixture `Reject(NotNow)` | Soft | In Review |
| Information Visibility Policy (#7) | Objectives implements the `ObjectiveProgress` and `SiteAssignment` narrower audiences | Hard for the Policy rows | In Review |
| Procedural City Generator (#16) | Must produce courts that pass LSC V-FAIR; fixture clear on four faces and ≥ 3 ingresses on final-eligible courts (S4, AC-G38, meets OC-8's chokepoint rule); final-eligible target 6 (`CityGenFinalEligibleTargetCount`, SP-4; reachable since D4); no role-specific court rule (role-blind bush exclusion, request 11a) | Soft | In Review |
| NPC-Kill Penalty & Informants (#14) | Informant `Interact` refused (OC-7 via legality class, PEN-R12); no one-pawn chokepoint at fixtures or objective paths (OC-8, Penalty CR-13) | Soft | In Review |
| Post-Round Reveal & Results (#21, VS) | Released stage timeline and `FSiteAssignment` in `Ended` (`PostRound`) | Soft | Not written |
| Telemetry & Analytics (#26, Alpha) | F5 stall metrics from the Event Log | Soft | Not written |
| Onboarding & Practice Mode (#25, Alpha) | A practice chain definition (data only) | Soft | Not written |

Bidirectionality: the systems index already lists Input, Round, Level Socket, Roster, Policy and TDC as Objectives' upstream, and HUD as a dependent. The change requests to add Crowd (soft) and Session (soft) to Objectives' upstream are done (both are in the table above and in systems-index row 15, verified by the 2026-10-25 cross-review); Crowd's GDD lists Objectives as soft.

## Tuning Knobs

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|---|---|---|
| `ObjectiveInteractSec` (revised) | float | Objectives | 3.0 | 2.0-6.0 | 2.9-4.5 (was 2.5-4.5; cross-review fix 2026-10-25, C-W10: Crowd I-CNB-6 `CrowdDecoyEarlyReleaseMaxSec` (default 2.8) < `ObjectiveInteractSec` rejects 2.5-2.8) | All (owner ring prediction) | Action | Sec | Hold time (OC-14). High: exposure dominates, rounds slow, rushes impossible to stop. Low: tend reads like an ambient touch (P2 lost). Invariant I-OC1 |
| `ObjectivesPerStageCount` | int | Objectives | 1 | 1-3 | 1-1 | ServerOnly | Round | Count | Sites per preliminary stage (OC-3). >1 rejected until LSC Selection supports it. High: long chains, more exposure |
| `bRivalSignalEnabled` | bool | Objectives | true | — | — | ServerOnly | Round | — | OC-12 on/off; **ON by default confirmed by the user 2026-10-25**. Off: stalled teams feel no race (SP-5) |
| `RivalSignalDelayMinSec` | float | Objectives | 30 | 30-120 | 30-60 (restated, cross-review C-W18: warn-free at the other defaults only to 32; above that raise Max or Q so that I-OC2's `W_eff ≥ 163 s` still holds, for example Min 60 needs Max ≥ 193) | ServerOnly | Round | Sec | Earliest per-completion delay (OC-12 1, F4). Low: releases track tends; a release 10 s after a seen tend feels like confirmation whatever F4 says (CD ruling sets the 30 s floor). High: no pressure |
| `RivalSignalDelayMaxSec` | float | Objectives | 165 | 70-300 | 163-200 (narrowed from 150-200, cross-review C-W18: 150-162 warns under I-OC2 at the other defaults) | ServerOnly | Round | Sec | Latest per-completion delay. Invariant I-OC2 on `W_eff = Max − Min + Q`: warn below 163 s, reject below 100 s; and `Max ≥ Min + 30` (reject). TDC never repairs a value to satisfy an invariant (TDC CR-6; reconciliation ruling 2026-10-24). Low width: confirmation leak (F4). High: signal useless |
| `RivalSignalReleaseQuantumSec` | float | Objectives | 30 | 10-90 | 28-45 (narrowed from 20-45, cross-review C-W18: below 28 `W_eff` falls under 163 s at the other defaults) | ServerOnly | Round | Sec | Global release grid period, shared by both teams, anchored at `Active` entry (OC-12 2, IVP OQ4). Should exceed the longest court visit (F2 `E_court` 15.5 s) so one tick cannot single out a visit (the safe range 28-45 does; the hard minimum 10 does not, so a value below 16 is outside what F4 and this reasoning cover). Low: fine timing returns. High: adds up to Q of latency |
| `TendCueDelaySec` | float | Objectives | 1.0 | 0.5-2.0 | 0.8-1.5 | All (owner ring prediction) | Action | Sec | Time after Begin before the ring turns Marigold at an own live site (OC-6). Invariant I-OC3 (**rejects** the preset, TDC; cross-review C-B2 2026-10-24): `+ 0.25 ≥` Crowd's `CrowdDecoyEarlyReleaseMinSec` (0.8) and `≤ ObjectiveInteractSec − 1.0`. Low: probes release far earlier than any decoy (P1 tell). High: every probe costs nearly a full hold |
| `ClueMatchMaxCount` | int | Objectives | 3 | 1-6 | 2-3 | ServerOnly | Round | Count | Courts a stage clue may match (OC-15). High: long searches, more court visits (tells). 1: a waypoint in words |
| `FixtureInteractSideCount` | int | Objectives | 4 | 1-4 | 3-4 | All | Round | Count | Tend stand points per fixture (OC-8). Low: crowding, fixtures blocked by NPC decoys |
| `ObjectiveStallGapDefectSec` | float | Objectives | 300 | 120-900 | 240-420 | ServerOnly | Use | Sec | F5 mutual-gap defect threshold (reporting only) |
| `ObjectiveFinalDwellDefectSec` | float | Objectives | 240 | 90-600 | 180-360 | ServerOnly | Use | Sec | F5 final-dwell defect threshold (reporting only) |

**Identity-tell review (CR-15.8):** `RivalSignalDelay*` and `RivalSignalReleaseQuantumSec` time a team-visible event against a player action; reviewed under F4 (bound `LR_sig ≤ 2.4` at λ_c,min) and pass at the defaults. `TendCueDelaySec` times the owner's earliest informed release against Crowd's decoy early releases (I-OC3). No other key times crowd-versus-player events.

**Interactions:** `ObjectiveInteractSec` with Crowd's decoy completion length (decoys hold exactly `D`; Crowd reads this key, never its own copy) and with I-OC1; the delay pair and the quantum with the rival-release rate λ_c (a longer or shorter chain changes λ_c, recheck F4); `TendCueDelaySec` with `ObjectiveInteractSec` and Crowd's early-release distribution (I-OC3); F3's `T_round` with F1 (any F1 change rechecks the r_N request) and `NpcCount` (TDC; `CrowdDecoyTendIntervalSec` scales with it, F3); `ClueMatchMaxCount` with F1's `o`; LSC `ChainLengthMinCm`/`MaxCm`, `FinalCandidatesMinCount` and `FinalAmbiguityMinCount` (owned by LSC) set F1 and SP-4.

**Consumed, owned elsewhere (not duplicated):** `InteractRangeCm`, `ReleaseDebounceSec` (IIF); `TargetRoundMinutes` (Round; non-binding and not read by Objectives, F1 supplies the modelled round length), `bHostMayAbortActive`, `DisconnectGraceSec` (Round); `CrowdDecoyTendIntervalSec`, `CrowdDecoyCompletePct`, `CrowdDecoyEarlyReleaseMinSec`/`MaxSec`, `CrowdMaxIdleSec` (Crowd); `CityGenFinalEligibleTargetCount` (Generator); `BushCourtExclusionCm` (LSC); `NpcCount`, `TeamSize` (TDC canonical); LSC Selection keys.

## Visual/Audio Requirements

*`art-director` not consulted (pipeline budget rule); derived from art bible 5.5, 6.8, 6.9, 7.1, 7.3 and the level-designer's review. Review before production.*

- **VA-OC1 Court and fixture**: exactly art bible 6.8 and LSC VA-1/VA-3: one waist-height stone block of a decor family, identical relief (stepped-fret, water-lines, maize, reeds) on every court; no hue, glow, particle, banner, team sign, tended mark or stage prop. Any stage dressing (beans, root, gourd, cup) must be on **every** court identically or not at all (LSC R2.6). Recommended: none in MVP.
- **VA-OC2 The tend pose**: the shared `InteractPose` (art bible 5.5): two-handed, body stopped and facing the stone, readable from 32 m. Same asset, same blend in, same loop, same blend out for counted, no-effect, decoy and feint tends. A counted completion has **no** distinct end animation.
- **VA-OC3 Audio**: no stinger, chime, stone scrape or crowd reaction on a completion or an unlock, for anyone in the world. The tend's own foley (if any) is the shared pose foley at the same level for every pawn (audio-director to detail in Diegetic Audio, VS). Owner-only and team-only UI sounds (the completion tone, the rival-step tick) play on the non-spatial 2D UI bus of the owning clients only, never in world space.
- **VA-OC4 Cultural review**: chain card text and any future fixture relief go through the art bible 6.9 sensitivity read before the vertical slice.

## Game Feel

| Moment | Target | Why |
|---|---|---|
| Press Interact in reach | Pose starts within 0.15 s of the server Begin (owner sees the ring predicted at once, IIF F4) | Commitment must feel immediate, not laggy |
| The hold | 3.0 s of stillness with a steady ring (Bone for the first `TendCueDelaySec`, then Marigold only at an own live site); no pulsing, no escalating sound | The tension is the watchers, not a minigame (not a reflex game) |
| Release early | Ring empties in one step; no partial credit; no penalty but lost time | Decisive and readable (P3) |
| Counted completion | Owner and team: card turns over, one owner-only tone, own step fills (500 ms, art bible 7.6); world: nothing | The reward is knowledge, never spectacle |
| Final unlock | Team: card shows "The Ruler's Cacao" and its clue; nothing else changes anywhere | Quiet advantage, P4 race narrows |
| Rival count rises | Small, late (30-195 s after the rival's completion, on the shared 30 s grid), one level tick on the team-only 2D UI bus, even when the count rises by 2 | Pressure, not alarm |

## UI Requirements

All elements are team- or owner-only, screen-space, from data the team view already holds (IVP R4); layout belongs to the HUD GDD and `design/ux/hud.md`.

1. **Stage card** (team, all members including informants): title, one line, clue line "A court [ClueTag phrase]". Top-centre plate under the progress steps; 22 px Primary title, Bone on Obsidian; changes with a 500 ms turn on a counted completion.
2. **Own progress steps** (team): 3 steps (Stage 1, Stage 2, Final), Marigold fill (art bible 7.3 element 5, revised from 5 steps).
3. **Rival steps** (team): 2 steps showing `RivalPrelimCount`, filled only by OC-12 releases, labelled "Rival house"; never a final step, never a sub-stage fill, never a time or order; both steps may fill in one update (one tick, one 500 ms fill); never shown in `FRoundPublicState`.
4. **Tend prompt and ring**: IIF UI-1/UI-3: the "Tend" prompt shows identically at every court fixture in reach (Active owners only); after Begin the ring fills Bone, and from `TendCueDelaySec` turns Marigold at an own live site and stays Bone elsewhere; nothing for informants.
5. **No world plate, waypoint, compass, arrow, distance read-out or map** (OC-15).
6. **End banner** wording is Round's ("completed the final objective" is internal; player text per Round's approved phrasings).

> **📌 UX Flag — Objectives Chain**: This system has UI requirements (stage card, own/rival steps, prompt and ring colour rule). Run `/ux-design` for the HUD before writing epics; stories cite `design/ux/hud.md`.

## Cross-References

| This GDD references | Document | Section | Status |
|---|---|---|---|
| Court geometry, Selection, `FSiteAssignment`, D1/D4/D8, R2.6 per-court object parity, AC-51 | design/gdd/level-socket-contract.md | R2, R3, R9, D1, D4, D8 | Reviewed, MAJOR REVISION NEEDED |
| Queue, slot, follow-ons, `DeclareObjectiveWin`, completion validity, latch, Event Log, F7 | design/gdd/round-match-framework.md | R11-R17, R19, F7, queue-item lifecycle (`Rejected_Latched`) | Reviewed, MAJOR REVISION NEEDED |
| Interact hold, ingress filter, `CanBegin`, `AllowNoEffect`, cancels, prompts | design/gdd/input-interaction-framework.md | R5, R6, R8, F1, F6, UI 1/3 | Reviewed, NEEDS REVISION |
| `GetLegalityClass`, `GetState`, `GetTeam`, connection projection | design/gdd/participant-state-roster.md | R4, R10, R11 | Reviewed, NEEDS REVISION |
| Audiences, C-TeamView carrier, opponent-progress row (ruling B), `SiteKnownToTeam` presentation only (ruling D) | design/gdd/information-visibility-policy.md | R3, R4, R7.4a, L33, L34, OQ4, AC-IVP-40/41 | Reviewed, NEEDS REVISION |
| `ObjectiveInteractSec`, CR-11 latch, CR-15 knob columns, OQ-9 | design/gdd/tuning-data-config.md | CR-11, CR-15, canonical table | Reviewed, MAJOR REVISION NEEDED |
| Decoy tending, Tell Matrix row T3, decoy rate and early-release share | design/gdd/crowd-npc-behaviour.md | Tell Matrix, decoy tending, Tuning Knobs, Dependencies | Reviewed, NEEDS REVISION |
| Site look, fixture, tell vocabulary, cultural rules, HUD element 5 | design/art/art-bible.md | 5.5, 6.8, 6.9, 7.1, 7.3 | Approved |

**Change requests raised by this GDD** (recorded here; the orchestrator routes them):
1. **Input & Interaction Framework**: (a) a teammate's completion no longer cancels other holds at the court with `TargetInvalid`; they run to their natural end with no effect (OC-8); (b) F1's `D` range 1.0-6.0 becomes 2.0-6.0 to match OC-14, and when `HoldTimer(Complete)` and `HoldTimer(DebounceExpiry)` of the same hold share a `DueTime`, Complete resolves first; (c) the ingress `TargetId` filter for Interact accepts **any court fixture id** (public geometry), not only the team's live sites, so humans can feint-tend any court exactly as NPCs decoy-tend (Crowd CNB-R4 player-reproducibility); the hidden-site oracle guard is kept because every non-live court, the hidden final included, resolves identically to `AllowNoEffect` (OC-6, OC-7). UI-3 changes: the "Tend" prompt shows identically at every court fixture in reach (never a live-site confirmation); the Team live-site set only colours the ring after `TendCueDelaySec` (OC-6); IIF AC-66 is updated accordingly. *(c) is already reflected in IVP R7.4a (ruling D).* (d) **New (design review 2026-10-24):** `TendCueDelaySec` (owner ring stays Bone until it elapses; client-visible key for ring prediction); (e) **new:** an `Interact(Begin)` at a fixture whose stand points are all physically occupied is `Reject(NotNow)` from Objectives' `CanBegin`, for every pawn (OC-7, OC-8); (f) **new:** IIF's post-latch wording `Dropped(PostLatch)` (edge case "both teams' last Interacts") should read Round's `Rejected_Latched`.
2. **Tuning Data & Config**: `ObjectiveInteractSec` hard range 1.0-10.0 becomes **2.0-6.0**, safe 2.5-4.5 (answers OQ-9; OC-14), plus invariant I-OC1 `ObjectiveInteractSec >= CrowdAmbientTouchMaxSec + 0.5`; register the OC-owned keys of the Tuning Knobs table.
3. **Information Visibility Policy** *(rewritten at design review 2026-10-24; the earlier `RivalStageStep` request is withdrawn per ruling B)*: add the R3 Team row "`RivalPrelimCount` (count of the opponent's released preliminary completions, 0-2, never the final; no identity, site, position, order or timing; fresh per-completion delay U(30, 165) s on `StageSignal` keyed by the round-global completion ordinal; released only on the global 30 s grid anchored at `Active`, shared by both teams; quantum key `RivalSignalReleaseQuantumSec` owned by Objectives): SelfActive Y, Teammate Y, informants of the team Y, Opponent N; Team carrier (C-TeamView)", approved by the creative-director at this review as the CD approval R3 requires; close OQ4 citing this ruling; L34 status becomes Mit by the R3 row and AC-IVP-40 (which this shape satisfies: count ≤ 2, never changes on the final, releases on the grid, delays independent of completer, court and team). *(Applied by IVP at reconciliation 2026-10-24.)*
4. **Round & Match Framework**: R13 table row `ObjectiveCompletion` is a follow-on inside `HoldTimer(Complete)` (already requested by the IIF); the `StageSignal` timer is a `TimerFired` item owned by Objectives; the Event Log gains `StageSignalReleased(viewingTeam, count)` (server-only), the per-team F5 fields, and server-only `TendCompleted(ParticipantId, court, Counted | NoEffect)` for bound pawns so counted tends and feints are measured separately (F3), **including a final `Complete` rejected by the latch (`NoEffect`, cross-review C-W35: Round to state the log rule)**; `Valid(i)` (Connected + human) applies to stage completions too *(applied in Round R13: re-validation of tender Active, Connected, human-controlled, team, site live and not completed)*; post-latch items are `Rejected_Latched` throughout (Objectives' edge cases now use that code).
5. **Session & Travel**: add sub-seed domain `StageSignal` to F5.
6. **Crowd & NPC Behaviour**: decoy tends use the full `InteractPose` for `D` through `Interact`, choose courts without reading `FSiteAssignment`, include courts currently being tended (no avoidance of occupied fixtures beyond physical space), and meet F3's band (T3 target 0.65-0.75; recomputed with the real team tend rate `R_P` 0.30/min the default 0.0042/min gives P = 0.64, so move `CrowdDecoyTendIntervalSec` to about 18,750 s, r_N 0.0032); Tell Matrix row "objective interaction: red herring yes". *(Applied by Crowd.)* **Reopened at design review 2026-10-24:** the 0.0032 request used `R_P` = 0.30 from a 12-min round that F1 contradicts; with F3 now banding full-length tends at `T_round` 9 min (`R_full` 0.40), set r_N ≈ **0.0057**/min (`CrowdDecoyTendIntervalSec` ≈ 10,500 s, which needs Crowd's safe range 15,000-23,000 widened) so `P_full` = 0.70; update T3's rate and AC-CNB-14's computed band. Also: (a) a decoy whose chosen fixture has no free stand point gets `Reject(NotNow)` like any pawn and chooses another court (OC-8); (b) expose the decoy early-release bounds (today U(0.8, 2.8) s and 25 %) as keys (`CrowdDecoyEarlyReleaseMinSec`, `…MaxSec`, `…Pct`) so Objectives' I-OC3 can check them; (c) if measured `P_short` exceeds 0.90 (F3), the first lever is a higher early-release share, not a lower decoy rate. *(All applied by Crowd at reconciliation 2026-10-24: `CrowdDecoyTendIntervalSec` 10,500, safe 8,400-13,500; `CrowdDecoyEarlyReleaseMinSec`/`MaxSec`; share = `100 − CrowdDecoyCompletePct`.)* **Rescaled again 2026-10-25** (user decision, `NpcCount` 40 → 50): `CrowdDecoyTendIntervalSec` 13,125 s (r_N ≈ 0.0046; Crowd safe range 10,500-16,875), `P_full` still 0.70 (F3).
7. **HUD / art bible 7.3 element 5**: the objective bar has **3 steps** (Stage 1, Stage 2, Final), not 5; the rival bar shows only `RivalPrelimCount` (0-2, delayed, may fill two steps at once) and never a final step; element 7's Marigold ring fill applies only after `TendCueDelaySec` at own live sites, and element 6's prompt is identical at every court.
8. **Level Socket Contract**: each court fixture exposes 4 interact sides (one per face, `FixtureInteractSideCount`), built locally and identical on every court; Selection stays court-count 1 per (team, stage) while `ObjectivesPerStageCount` = 1; publish a role-blind `ClueTag` per court (area type + nearest distinctive public feature, included in the D7 layout hash) such that each clue matches at most 3 courts (OC-15); for final-eligible courts, at least 3 ingresses if the graybox affords it (SP-4); any bush-free radius only as one role-blind `BushCourtExclusionCm` for every court (amended at reconciliation 2026-10-24 per Generator request 11a; the earlier "no bush within 12 m of final-eligible courts" would mark the candidates); **new (reconciliation 2026-10-24, Penalty CR-13):** a V-NAV-2 clause that no fixture or objective path is closable by one standing pawn (fixture ≥ 1.2 m clear on all four faces; every court ingress wider than two capsule diameters or ≥ 2 ingresses; OC-8); **new (design review 2026-10-24):** consider `FinalCandidatesMinCount` > `TeamSize` (at least 6) so a full team cannot post one camper per final candidate (SP-4, Open Question 9). Note: the pipeline brief's 150-300 m chain band is superseded by LSC D4's 120-240 m (target 170); F1 recommends moving the target toward 220 m if rounds run short.
9. **Systems index**: add Crowd (soft) and Session (soft) to Objectives' Depends On. *(Applied to row 15 at design review 2026-10-24.)*

**Change requests received (reconciliation 2026-10-24):**

| From | Request | Disposition |
|---|---|---|
| Penalty CR-13 | No objective fixture or objective path closable by one standing pawn (clear approach > two capsule diameters, or two approaches) | APPLIED: OC-8 rule, Edge Cases, our CR 8 (LSC V-NAV-2 clause), Dependencies row for Penalty |
| Generator (S4, OQ5; requests-received table) | Final-eligible target 6 > `TeamSize`; with 2 corner zones every final lies in the two axis corners | APPLIED: SP-2, SP-4, OQ9 (partly answered; LSC floor 6, kept at 6 by D4 2026-10-25), Dependencies row; pre-positioning stays Generator OQ5's measured question |
| Generator request 11a | Any bush-free radius must be one role-blind `BushCourtExclusionCm` for every court, never final-only | APPLIED: SP-4, our CR 8 amended, OQ6 |
| IVP OQ4 / Cross-References item 8 (ruling B) | Revise OC-12, F4, AC-OC-09/10 to a stage-level count with a fresh per-event delay and a global quantum | ALREADY DONE at Objectives' design review; IVP added the R3 row this pass (OC-12, OC-5 and Dependencies wording updated) |
| IVP ruling D (R7.4a) | `SiteKnownToTeam` presentation only; ingress accepts any court | ALREADY DONE (OC-5, OC-6, AC-OC-26) |
| Crowd CR 5 | `AllowNoEffect` accepts decoy tending at every non-live court identically for humans; AC pointing to T3 | ALREADY DONE (OC-7, OC-9, AC-OC-24, AC-OC-30) |
| HUD (HUD-R11, CR 7 cross-check) | 3 own steps, 2 rival steps from `RivalPrelimCount` (may rise by 2 in one tick, never a final step), ring after `TendCueDelaySec`, identical prompt; team view only | ALREADY DONE (UI 1-4 match HUD-R11, HUD-E03..E05); Dependencies status updated |
| Round CR 8 | `ObjectiveCompletion` items; `Valid(i)` includes Connected and human control; Objectives owns stall pressure; `StallFlag` is an Objectives defect | ALREADY DONE (OC-10, SP-1..SP-8, Round R13 row) |
| Concealment CR 8, CR 9 | List Concealment; hiding is never a winning line (AC-CON-24) | ALREADY DONE (Dependencies row, SP-8) |
| TDC CR 7 | Justify the 1.0 s `ObjectiveInteractSec` minimum or raise it | ALREADY DONE (hard range 2.0-6.0, OC-14, I-OC1) |
| Session CR 8, LSC CR 10 | Site selection and `StageSignal` from server sub-seeds; `FSiteAssignment` ServerOnly, no per-court cue | ALREADY DONE (OC-3, OC-12, OC-15, SP-2, AC-OC-25) |
| IIF CR 7 | Prompts only at the owner's live sites; expose the live-site set for the ingress filter | DECLINED (superseded): IVP ruling D (R7.4a) and IIF's own reconciled UI-3 put an identical prompt at every court and make ingress accept any court; the rest of CR 7 (follow-on completion, (site, legality) predicate) is ALREADY DONE |
| TDC CR 12 (TDC Cross-References item 12) | Enforce I-OC2 as reject/warn, never as a clamp of `RivalSignalDelayMaxSec` (CR-6) | APPLIED at /review-all-gdds 2026-10-24 (finding C-B1): Tuning Knobs row and AC-OC-22 restated as rejection |
| Cross-review 2026-10-24 C-B2 (TDC I-OC3 registered `reject`) | State I-OC3's failure mode; align AC-OC-32 | APPLIED (cross-review fixes 2026-10-24): `TendCueDelaySec` row says I-OC3 rejects; AC-OC-32 expects a rejected preset |
| Cross-review 2026-10-24 S-I4 | Name the connection state OC-10 (c) reads | APPLIED: OC-10 (c) reads Session's connection state `Connected`, not the Roster's collapsed value |
| ADR-0020 sync 2026-10-24 | OC-2: `InteractSecOverride` rejected by chain validation until a chain-wide, client-visible override exists (needs game-designer sign-off) | APPLIED (OC-2 row: reserved, must be unset; game-designer sign-off obtained 2026-10-24, logged); **field REMOVED 2026-10-25** (user decision): `FObjectiveDef` has no per-definition hold time; the hold is always the chain-wide `ObjectiveInteractSec` (a per-definition value would make `D` depend on the court's role, a timing oracle, IVP R7.4a, AC-OC-08); ADR-0020's reject rule is moot |
| ADR-0020 sync 2026-10-24 | OC-7/OC-8: "physically occupied" implemented as active-hold claims (needs game-designer sign-off) | APPLIED (OC-7 wording, new edge case; game-designer sign-off obtained 2026-10-24, logged) |
| ADR-0020 sync 2026-10-24 | Human-controlled re-validation = control epoch unchanged since Begin and no hand-over pending; team view re-written from the Roster rebind notification | APPLIED (OC-10 gate (c), rejoin edge case; Roster `OnViewRebound`) |
| ADR-0006 sync 2026-10-24 | OC-12 delay drawn in integer milliseconds (no float in the draw) | APPLIED (OC-12 step 1) |

*Engine feasibility*: no engine reference for the `Scripting` domain exists under `docs/engine-reference/unreal/modules/`, so the rule logic was not checked against the pinned engine; `networking.md` (UE 5.8, Iris production-ready) was consulted only for the Team carrier, whose mechanism belongs to ADR-001 (C-TeamView in each teammate's PlayerController; no team-filtered replicated actor, IVP R4).

## Acceptance Criteria

*`qa-lead` lens applied at design review 2026-10-24 (non-testable wording rewritten, ACs 28-32 added); a dedicated qa-lead pass is still due before sprint planning. Tags: [U] unit/automation with stub Round/IIF/LSC, [PIE-2] two-client PIE, [V] visual evidence in `production/qa/evidence/`. Test names `StealthGame.Objectives.<Scenario>`.*

**Core rules**
- **AC-OC-01 (OC-2, OC-3)** [U] GIVEN the MVP chain asset, WHEN validated at `Preparing`, THEN it has per team one `Prelim1` and one `Prelim2` with distinct ids and text, one `Final` with `TeamEligibility = Both`, and binds to `P1[T]`, `P2[T]`, `Final` of a fixture `FSiteAssignment`; GIVEN `ObjectivesPerStageCount` = 2, or any definition with `CompletionsRequiredCount` = 2, THEN `PreparingFailed(ObjectiveChainInvalid)`; the `FObjectiveDef` type has no hold-time field (static check; user decision 2026-10-25). `ChainBindAndValidate`
- **AC-OC-02 (OC-4)** [U] GIVEN team 0 at `Stage1`, WHEN its `Prelim1` completes, THEN `Prelim2(0)` is `Live` in the same slot and team 1's states are unchanged; WHEN the fake clock advances 60 min with no completion, THEN no state changes. `ProgressOnlyByCompletion`
- **AC-OC-03 (OC-5, SP-1)** [PIE-2] GIVEN a round at `Active` start, WHEN each client's replicated data is dumped, THEN team 0 clients hold only `P1[0]` as a live site, team 1 clients only `P1[1]`, no client holds `Final`, `P2[*]` or any opponent site, and `FRoundPublicState` holds no objective field; WHEN team 0 completes both prelims, THEN only team 0 clients gain `Final`. `TeamViewContents`
- **AC-OC-04 (OC-6, OC-7)** [U] GIVEN an Active-class pawn (human-bound, unbound NPC) and an Informant pawn, WHEN `CanBegin(Interact)` is called on a live, locked, completed, opponent and hidden-final court with a free stand point, THEN the Active pawns get `Allow` at all five and the Informant gets `Reject(NotNow)` at all five; WHEN all 4 stand points of the court are occupied, THEN every pawn gets `Reject(NotNow)` at every court kind; the predicate's inputs are only (court, its public stand-point occupancy, legality class) (static check: no Roster team, binding or `FSiteAssignment` call in the predicate). `PredicateSiteAndClassOnly`
- **AC-OC-05 (OC-8)** [U] GIVEN two teammates holding at their live site 0.5 s apart, WHEN the first completes, THEN the stage counts once, the second hold is not cancelled, completes at its own `D`, and changes nothing; no `TargetInvalid` is emitted. `NoStackNoCancel`
- **AC-OC-06 (OC-10)** [U, table-driven] GIVEN a final `HoldTimer(Complete)` slot and, one row each, a completer that is (1) Active+Connected+human on an eligible team, (2) AI-driven (hand-over), (3) Unresponsive, (4) Disconnected, (5) Informant at the slot, (6) Active on a team for which the final is not `Live`, THEN `DeclareObjectiveWin` is called exactly once for row 1 and 0 times for rows 2-6, and site state and team views are unchanged for rows 2-6; the same table for a stage completion yields exactly one `ObjectiveStage` log entry (row 1) and 0 for rows 2-6. `CompletionGate`
- **AC-OC-07 (OC-10, Round R11)** [U] GIVEN a stub queue, WHEN a counted final completes, THEN `DeclareObjectiveWin` is called with the `HoldTimer(Complete)` key inside that slot and no other item interleaves. `FollowOnInSlot`
- **AC-OC-08 (OC-6, OC-11, SP-1)** [PIE-2] GIVEN a human tending its hidden final, a locked P2, a completed P1, an opponent site and its own live site, WHEN each tend runs to `D` and a server/owner trace is recorded, THEN the owner's replies and `FHoldStatus` messages are byte-exact across all five cases, Begin-to-Complete timing agrees within ± 1 server tick, the four non-live cases change no state, and the ring is Bone for all five until `TendCueDelaySec` and then Marigold only at the own live site. Screenshots (ring only) `production/qa/evidence/oc-tend-{hidden,locked,completed,opponent,live}.png`. `HiddenSiteOracle`
- **AC-OC-09 (OC-12)** [U] GIVEN a pinned `StageSignal` sub-seed and team 1 completing `Prelim1` at t_c = 100 s (Active at t = 0), THEN team 0's `RivalPrelimCount` becomes 1 exactly at `t_rel` = 30·⌈(100 + Δ_1)/30⌉ s with Δ_1 ∈ [30, 165], never earlier, and only at a multiple of 30 s from `Active`; GIVEN team 1's second completion draws a delay that releases it first or on the same tick, THEN the count still rises 0 → 1 → 2 or 0 → 2 and never falls; the replicated field carries only the integer (no id, site, time or order); a release due after the latch is never published; with `bRivalSignalEnabled` false or in a 1v0 dev round, nothing is published and no `StageSignal` timer is scheduled. `RivalSignalReleaseGrid`
- **AC-OC-10 (OC-12)** [U] GIVEN 1,000 pinned sub-seeds and scripted rounds that vary which team, completer and court produce the k-th preliminary completion, THEN Δ is uniform on [30, 165] (Kolmogorov-Smirnov p > 0.01, per team) and Δ for ordinal k is identical whichever team, completer or court produced it (the draw is keyed by ordinal only); every release time of both teams lies on the one 30 s grid from `Active`. Satisfies AC-IVP-40. `RivalSignalDistribution`
- **AC-OC-11 (OC-14)** [U] GIVEN a preset with `ObjectiveInteractSec` 1.0, THEN the effective value is 2.0 with a warning; GIVEN `CrowdAmbientTouchMaxSec` 1.8 and `ObjectiveInteractSec` 2.0, THEN I-OC1 warns. `InteractSecFloor`
- **AC-OC-12 (P1 parity)** [PIE-2] GIVEN a counted completion and a no-effect completion at two courts, WHEN an opponent client's received replication (properties, RPCs, spawned actors and components) is traced over the 2 s around each, THEN the opponent receives zero Objectives-related properties or RPCs in both cases, and the remaining traffic (pawn pose and movement) is the same set of fields in both; no court, fixture or pawn property changes. `CompletionNoWorldSignature`
- **AC-OC-13 (OC-15)** [U] GIVEN the graybox layout and 1,000 pinned assignments, THEN every live site's clue matches 1 to `ClueMatchMaxCount` courts and the clue string is a function of public `ClueTag` data only (no role input). `ClueBoundAndRoleBlind`
- **AC-OC-14 (OC-16)** [Integration/BLOCKING, PIE-2] GIVEN the graybox, 1v1, WHEN each player completes its two preliminaries and team 0 completes the final, THEN `FRoundResult{ObjectiveWin, Winner 0}`, the Event Log holds 4 `ObjectiveStage` and 1 `FinalCompleted` entries in key order, and team 1's client never held the final id. Evidence `production/qa/evidence/oc-e2e-*.png`. `EndToEndCacaoChain`
- **AC-OC-15 (OC-17)** [U] GIVEN a round reset, THEN all site states, team views and pending signals are cleared on `OnRosterReset`; GIVEN a rejoin inside the grace, THEN the rejoiner's team view equals its teammates'. `ResetAndRejoin`

**Stall pressure**
- **AC-OC-16 (SP-3, SP-6)** [U] GIVEN a round where no pawn tends, WHEN the fake clock runs 10 × `TargetRoundMinutes`, THEN no Objectives state, team view or `RivalPrelimCount` changes, no Objectives timer exists (an OC-12 release timer exists only while a release is pending, and none is), and the round is still `Active`. `WaitingEarnsNothing`
- **AC-OC-17 (SP-4)** [U] GIVEN team 1 before its unlock, WHEN a team 1 pawn tends the true final to `D`, THEN no effect and no state change. `FinalNeedsOwnUnlock`
- **AC-OC-27 (SP-8)** [U] GIVEN team 1's every Active pawn concealed and team 0 tending normally, WHEN team 0 completes its chain, THEN `ObjectiveWin(0)`; GIVEN a concealed team 1 pawn issuing Interact at the nearest court, THEN it is rejected (Concealment) and no team 1 state changes. Linked: Concealment AC-CON-24. `HidingNeverWins`
- **AC-OC-18 (SP-7)** [Review] GIVEN this GDD and the Round, Concealment and Crowd GDDs, THEN no rule adds a round clock, dwell timer, time-based unlock or positional reveal in response to waiting. `NeverATimerNeverAReveal`

**Formulas**
- **AC-OC-19 (F1)** [Playtest/ADVISORY] GIVEN ≥ 20 objective-ended graybox rounds, THEN the median `ObjectiveWin` duration and per-team leg times are logged next to `T_chain` (L of the drawn combo); a median below 6 min triggers the F1 pacing levers.
- **AC-OC-20 (F2)** [Playtest/ADVISORY] GIVEN the server log of ≥ 10 graybox rounds, THEN for each counted completion the time from the tender's last stop (speed below the walk threshold) to `Complete` is reported, mean within `E_tell` ± 1 s (target 4 s); probes (early releases after `TendCueDelaySec` at non-live courts) are counted per stage as `N_probe`.
- **AC-OC-21 (F3)** [U + Playtest] GIVEN `R_full` 0.40 (F1 `T_round` 9 min), `NpcCount` 50, completion share 0.75 and Crowd's rate after reopened change request 6 (r_N 0.0046), THEN the computed `P_full` is in [0.65, 0.75] and stays there for `T_round` 7 and 11 min; GIVEN Crowd's registered `CrowdDecoyTendIntervalSec` (13,125 s since the `NpcCount` 50 change, 2026-10-25), THEN the computed `P_full` is in band (a regression to 18,750 s gives 0.77 and fails). GIVEN ≥ 60 internal graybox rounds or an accelerated simulation of the same tend volume, THEN the measured share of opponent-player tends is reported separately for full-length tends (banded 0.65-0.75), early releases (`P_short`, alarm above 0.90) and, among player tends, counted vs feint (`TendCompleted` kind); heavy feinting pushing `P_full` above 0.75 is a chosen tell and not a defect.
- **AC-OC-22 (F4)** [U] GIVEN the defaults (30, 165, Q 30), THEN `W_eff` = 165 s and `LR_sig` ≤ 2.4 at λ_c = 0.0033; GIVEN a preset with Min 40, Max 150, Q 30 (`W_eff` 140), THEN I-OC2 warns; GIVEN Min 60, Max 100, Q 20 (`W_eff` 60), THEN I-OC2 rejects the preset (`W_eff` < 100; TDC CR-6 fallback applies, no value is repaired); GIVEN Min 120, Max 70, THEN I-OC2 rejects the preset (`Max ≥ Min + 30` fails). `RivalSignalBound`
- **AC-OC-23 (F5)** [U] GIVEN fixture logs labelled by team (team 0: P1 95, P2 260, final 420; team 1: P1 180, P2 330), THEN `G_mutual` 95, `G_team(0)` 165, `G_team(1)` 180, `FinalDwell` 160, no defect; GIVEN no completion and an elimination at 400 s, THEN `StallDefect`. `StallMetrics`

**Cross-system**
- **AC-OC-24 (Crowd T3)** [Review, deferred gate] GIVEN the Crowd Baseline and Tell Matrix, THEN row T3 "objective interaction" states the 3 s pose, red herring yes (decoy and feint), the F3 band, and this GDD; AC-CNB-14 passes. `TellMatrixRowT3`
- **AC-OC-25 (LSC AC-51)** [U, static] Objectives reads `FSiteAssignment` only through the LSC server query API, from no client module, and spawns no per-court replicated actor. `AssignmentServerOnly`
- **AC-OC-26 (IIF ingress, IVP R7.4a)** [PIE-2] GIVEN a human `Interact(Begin)` with a court id that is not live for its team, THEN the command reaches the queue (not dropped at ingress) and completes with no effect, with acceptance, reply code, hold duration and timing class identical to an own live site (shares the AC-IVP-41 fixture); GIVEN an id that is not a court fixture, THEN it is dropped at ingress as an unknown id (IIF R5, no reply). `IngressAcceptsAnyCourt`
- **AC-OC-28 (OC-15)** [U] GIVEN a fixture layout in which a live site's clue matches 4 courts with `ClueMatchMaxCount` 3, THEN the round proceeds, the stage card shows the same clue string (never a sharper one), and exactly one `ClueTooBroad` entry is logged per layout hash. `ClueTooBroadLogged`
- **AC-OC-29 (OC-17)** [U] GIVEN a round in `Preparing` after `OnLayoutReady`, THEN no client's C-TeamView holds any site id or stage card; WHEN `Active` begins, THEN each team's view is pushed in that slot. `NoTeamViewBeforeActive`
- **AC-OC-30 (OC-9, Crowd CNB-R4)** [U] GIVEN an NPC decoy tend and a human feint at the same non-live court, THEN both pass the same `CanBegin`, run the same IIF path, emit identical `OnActionCompleted` events and identical replication to non-owner clients, and neither changes state. `FeintMatchesDecoy`
- **AC-OC-31 (OC-8, Crowd CR6)** [U] GIVEN a court with one stand point taken by a tending human, WHEN Crowd selects decoy courts over 2,000 trials, THEN the tended court is still chosen at the uniform rate (chi-square p > 0.01) and the decoy takes a free stand point; WHEN all 4 points are taken, THEN the decoy's Begin is `Reject(NotNow)` and it picks another court. `DecoyIncludesTendedCourts`
- **AC-OC-33 (OC-5, OC-12, OC-17)** [PIE-2] GIVEN a team with one Active member and one Informant (an informant of the same team), WHEN a release raises that team's `RivalPrelimCount`, THEN both clients receive the same count and the same stage card and live-site set in the same update, and a client of the other team receives none of them; GIVEN the informant rejoins inside the grace, THEN its team view equals the teammate's (Roster `OnViewRebound`). `InformantsShareTeamView`
- **AC-OC-32 (OC-6, I-OC3)** [U] GIVEN `TendCueDelaySec` 0.5 (its hard minimum) with Crowd's `CrowdDecoyEarlyReleaseMinSec` 0.8 (0.75 < 0.8), THEN I-OC3 rejects the preset (TDC I-OC3, `reject`; invariants never repair, CR-6; cross-review C-B2 2026-10-24); GIVEN `TendCueDelaySec` 2.5 with `ObjectiveInteractSec` 3.0, THEN the hard range clamps it to 2.0 with a warning and I-OC3 then passes (2.0 ≤ 3.0 − 1.0); GIVEN the defaults, THEN the owner ring's Marigold transition at an own live site occurs at Begin + 1.0 s (± 1 frame) and never at a non-live court. `TendCueDelay`

## Open Questions

| # | Question | Owner | Resolve by |
|---|---|---|---|
| 1 | Is the rival signal (OC-12, `RivalPrelimCount`, U(30, 165) s plus the 30 s grid) right for MVP? Default on (creative-director, design review 2026-10-24); A/B with `bRivalSignalEnabled` false in the first 10-player test, comparing F5 defects, `ObjectiveEndShare`, and how often a trailing team at rival count 2 switches to camping final candidates (telemetry: trailing-team positions near final-eligible courts after the count reaches 2). If it turns the race into a hunt, disable it (SP-7 (1)) | creative-director, game-designer | first 10-player playtest |
| 2 | `ObjectivesPerStageCount` > 1 needs LSC Selection to draw several courts per stage (and V-FAIR to cover them) | level-designer (LSC) | before Vertical Slice |
| 3 | Can LSC publish a role-blind `ClueTag` per court (area type + nearest distinctive public feature, in the D7 hash) with every clue matching ≤ 3 courts on the graybox? Fallback: area type only | level-designer | graybox plan |
| 4 | Calibrate F1's κ, φ, o (including probes; recheck the 10 s floor), q, r, F2's t_settle and `N_probe`, and F3's `n_full` and `T_round` from logged leg times (any change rechecks the Crowd r_N request); raise the LSC chain target to about 220 m if objective-ended rounds median under 6 min (check LSC D4's walk share first: S = 0.29 at 220 m against the quarter threshold) | systems-designer | after 20 graybox rounds |
| 5 | Level-designer proposal, not adopted: only a pawn that completed a preliminary may tend the final (caps candidate campers). Revisit if F5 shows final-candidate camping | game-designer | after playtests |
| 6 | Final-eligible courts with ≥ 3 ingresses: affordable on the graybox? (LSC change request; generated layouts already meet it, Generator S4.) A bush-free radius may only be a role-blind `BushCourtExclusionCm` for every court (Generator request 11a, reconciliation 2026-10-24), never final-only | level-designer | graybox plan |
| 7 | An optional 12 m team-only site plate if median search time (stage live → first Begin at the site) exceeds 60 s | ux-designer | after playtests |
| 8 | Informants' external voice may relay the clue to anyone listening (an out-of-game channel; nothing to do for Objectives). The MVP in-game channel is Informant Pings & Team Comms' momentary team-only place ring (user scope decision 2026-10-25, no in-game text chat): a teammate or informant who knows the live court may ping it, a team-entitled chosen waypoint that costs the pinger an action and shows nothing to the rival; confirm in that GDD that a ping carries no stage, site-role or card data, and that no ping rule is keyed to a court's role | Informant Pings & Team Comms | first playtest |
| 9 | Blanket camping of final candidates (SP-4): with the old LSC floor (`FinalCandidatesMinCount` 4) and `TeamSize` 5 a team could post one camper per candidate (floor now 6). **Partly answered (reconciliation 2026-10-24):** generated layouts target 6 final-eligible courts (Generator `CityGenFinalEligibleTargetCount`, S4 pre-check), above `TeamSize`; the LSC floor is now 6 (change request 8 applied by LSC at reconciliation, `FinalCandidatesMinCount` 6) and is **never lowered below 6** to make a layout pass: Generator OQ 9 was closed 2026-10-25 (decision D4) by lowering `StartToFinalMinPathCm` to 44 m, which makes 6 final-eligible courts reachable on the generated geometry at the mean S4 geometry (Generator S4 arithmetic, LSC V-SEP/D5; the pass rate under skeleton jitter is still OPEN, cross-review C-W24, Generator OQ 9 / LSC OQ 15). With 2 corner zones the finals sit in the two axis corners (SP-2), which Generator Open Question 5 measures. Does F5 or telemetry show camping in rounds lost by `Elimination`? If so, LSC raises `FinalCandidatesMinCount` above `TeamSize` or 4-zone mode becomes the default; never a penalty, timer or reveal | level-designer, game-designer | after 20 graybox rounds |
| 10 | Early-release tends (`P_short`, F3) are about 0.87 player at Crowd's r_N 0.0046 (50 NPCs, 2026-10-25): if measured above 0.90, Crowd raises its early-release share first (lower `CrowdDecoyCompletePct`; Crowd R7, change request 6 (c)); is `TendCueDelaySec` 1.0 s enough to put probe releases inside the decoy early-release distribution? | systems-designer, Crowd | after 10 internal playtests |
