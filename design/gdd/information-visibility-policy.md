# Information Visibility Policy

> **Status**: Reviewed - NEEDS REVISION 2026-10-25 (second-round lean re-review: the seven 2026-10-24 blockers confirmed resolved; two new blockers fixed in place, R10 stale OQ8 wording and AC-IVP-03 sample size vs F2, plus stale-number clean-ups; independent re-review pending), first review NEEDS REVISION 2026-10-24 (revisions applied); reconciled 2026-10-24; ADR-synced 2026-10-24; user decisions applied 2026-10-25 (tally confirmed; new fact rows `T_due`, `bDiedConcealed`, `StartServerTime`, `ContactServerTime`, `RetireAtServerTime`, feed `Seq`; `LatencyWarnRttSec` retired; OQ7/OQ8 closed); user scope decisions 2026-10-25 applied (new fact rows `TeamPing`, `TeammateReconnecting`, `RoundTeamSummary`; L11 updated; Informant Pings is MVP)
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS -> REVISED 2026-10-24 (six fixes applied: MVP-true fantasy wording and reveal anchor, F5 limited to the unconditional entry rate so contextual hides stay a P2 tell, probe cost from P3 with bush-scale spot tolerance, P4 constraint on any opponent-progress signal, `ConcealCullDelayMs` name, "trusted playtest" defined)
> **Author**: design-system pipeline (autonomous) + ue-replication-specialist, security-engineer, qa-lead, creative-director
> **Last Updated**: 2026-10-24
> **Last Verified**: 2026-10-24
> **Implements Pillar**: P1 Hide in Plain Sight; P2 Every Advantage Is a Tell; P4 A Race, Not a Hunt

## Summary

The Information Visibility Policy is the server's single rulebook for who may know what: one query, `CanViewerKnow(Viewer, Subject, Fact)`, a matrix of seven viewer classes against every fact, and one replication audience and carrier per field. It makes sure an opponent's client never receives anything that separates a player from an NPC, hides concealed players by not sending them at all, and gives teammates their markers. It exists because Pillar 1 can be broken by a single engine default, and it owns the leak catalogue and the automated tests that prove it is not.

> **Quick reference** — Layer: `Core` · Priority: `MVP` · Key deps: `Participant State & Roster, Round & Match Framework (upstream); peers SCL, Session & Travel, IIF, CPA, LSC; dependents Concealment, Crowd, Assassination, NPC-Kill Penalty & Informants, Objectives, Teammate Markers, HUD`

## Overview

The Information Visibility Policy (IVP) is the one server-side rule set that decides **who may know what** in a round of Smoking Mirror. It exposes one query, `CanViewerKnow(Viewer, Subject, Fact)`, a fixed matrix of seven viewer classes against the game's facts (is this pawn a player, which team, where, which look, concealed, informant, strikes, defence, objective progress, site assignment, connection, Event Log), and one **replication audience** per field (`ServerOnly`, `Owner`, `Team`, `All`) with the named carrier that implements it (the Shared Pawn, the PlayerState allowlist, the PlayerController team view, the GameState public structs). It lists every Unreal Engine default that would broadcast a player/NPC difference (`APawn::PlayerState`, the player array, `CompressedPing`, `bIsInactive`, `Owner`/`Controller`, relevancy) and says how each is overridden. It decides how bush concealment is enforced on the network, keeps the Pillar-1 leak catalogue, and owns the automated `StealthGame.Visibility.*` leak tests. Players never see the Policy; they experience its guarantee: a crowd in which nothing on screen, in the sound, or in the packets says "that one is a person" unless that person chose to show it. Without it, each of the 20-odd systems that touch player data would make its own call on what to replicate, and one wrong default is enough to make every enemy findable from a packet dump. ADR-001 (crowd identity rules, not yet written) is the implementation home; this GDD is its design home, and implementation choices are marked "-> ADR".

*Framing choices (autonomous, logged)*: both data layer and player effect (Core category); no ADR file exists, so ADR-001's direction from the systems index is cited as the constraint; the fantasy is indirect.

## Player Fantasy

**The crowd keeps your secret.** Out there, among fifty walking bodies, nothing gives you away except what you do. Not a name over your head, not a glow, not a shadow inside the bush, nothing an enemy's game is told. The city does not know which bodies are people, so neither can your rivals. Your team knows, and only your team. They see the small turquoise mark over you, and they keep seeing it after you have been cut down. Step into the leaves and you are gone for the enemy: no body, no shadow, nothing to find but the bush itself, and a blade swung at a guess. When the round ends, and only then, the round's result is told to everyone at once; the full "who was who" belongs to the Vertical Slice reveal (OQ7 pending).

The fantasy is a promise the player tests constantly without noticing: when an enemy finds you, it is because you ran, climbed, lingered at the wrong door or struck. It was never because the game told them. Every read an opponent makes is earned from behaviour (P2), and every escape you make is earned the same way (P1).

Anchor moments, each mapped to an acceptance criterion: (1) an opponent with a modified client dumps every replicated byte mid-round and still cannot tell your pawn from an NPC (AC-IVP-01, 02, 03); (2) you step into a bush and the enemy who watched you go in sees only leaves, with no body, shadow or data, but a blind strike at the bush can still kill you (AC-IVP-10 to 13); (3) your teammate's marker stays on you from across the city and through the leaves (AC-IVP-15); (4) the round ends for everyone in the same instant and only the result is released in MVP; who-was-who is the VS Reveal's (AC-IVP-20, OQ7).

*Pillars served*: P1 ("until someone commits, an enemy player cannot be told apart from an NPC by model, animation, audio, UI or network presentation"); P2 (the only permitted differences are chosen behaviours, readable by line of sight); P4 (no opponent count or progress signal turns the race into a hunt). *Language rule*: player-facing text derived from this system never says "hidden player", "detected", "tracked", "revealed" before `Ended`. *(creative-director viewpoint taken in-document under the pipeline budget rule; the CD-GDD-ALIGN gate reviews it.)*

## Detailed Design

### Core Rules

*Specialists consulted: ue-replication-specialist (audit against installed UE 5.8.4 source; tags [V] verified in source, [S] spike, [NS] not sourceable), security-engineer (leak catalogue, leak budget, concealment verdict). Spike IDs S1-S8 are Roster OQ0's; S9-S15 are added here (Open Questions).*

**R1. One query, default deny.** The Policy exposes one server-side query, `CanViewerKnow(Viewer, Subject, Fact) -> bool`. `Viewer` is a **connection's participant** (keyed on the server connection, never on client input, so rejoin and `RebindView` keep it); `Subject` is a pawn, a participant, a court fixture (for `SiteKnownToTeam`) or "the round"; `Fact` is one row of R3. An `N` cell is a legitimate answer: it returns `false` with no fault report and no log line. An unlisted fact, an unknown viewer, a call on a client and a call outside the game thread all return `false` and report to the injected fault sink (the same sink as Roster R2: an `ensure` in development builds and a counted `LogStealthVisibility` error; tests inject a counting stub). The query is **not** called per frame by gameplay: the Policy's answers are compiled into the carriers of R4, and the carriers *are* the enforcement. The query exists so that (a) server systems that send per-viewer data (Concealment's cull, Assassination's kill confirmation, Objectives' audiences, the VS reveal) ask one place, and (b) the replicated-property audit (R11) can check every carrier against the matrix.

**R2. Viewer classes.** The class is computed from the Roster (`GetTeam`, `GetState` of the viewer's participant; `GetParticipant` of the subject) and Round's phase, in this order:
1. Round phase `Ended` → `PostRound` (for every viewer, set in Round's **round-end step**, before any `Ended` data is sent; Round R9: the latch step itself, or the deciding strike's death write when a strike decides the round, so nothing about the end is released before the blow; cross-review S-B1 2026-10-24).
2. Subject is an unbound pawn (an NPC) → the viewer gets the **NPC row**: exactly the `All` facts. No class is computed, so there is no code path in which an NPC answer depends on the viewer.
3. Viewer participant = subject participant → `SelfActive` or `SelfInformant` (by the viewer's state).
4. Same team → `Teammate` or `TeammateInformant` (by the viewer's state).
5. Otherwise → `Opponent` or `OpponentInformant`.

**Class invariants** (art bible 5.2: classes are decided by relation, not by alive/informant status): for every fact about *another* subject, `TeammateInformant ≡ Teammate` and `OpponentInformant ≡ Opponent`. `SelfInformant` differs from `SelfActive` only in how the owner's HUD presents its own data (wrong-target tally frozen, cooldown hidden: HUD's rule). An informant's elimination never grants knowledge, and never removes knowledge of allies. **Parity invariant (Pillar 1):** for every `Opponent`-class answer about a player pawn, the observable value is drawn from the same set and distribution as the NPC row for an NPC pawn. A fact that cannot meet that is `ServerOnly` for opponents.

**R3. The visibility matrix.** Y = may know (through the named carrier); N = must not know (not replicated to that connection); E = `PostRoundEligible` (may be released only in `Ended`, only by the VS Post-Round Reveal; in MVP only `PublicResult` is released, Round R19). — = not applicable (the pair cannot occur, e.g. an informant cannot be concealed); the query returns `false` with no fault report. Columns: SA SelfActive, SI SelfInformant, TM Teammate (≡ TeammateInformant), OP Opponent (≡ OpponentInformant), NPC row, PR PostRound.

| Fact | SA | SI | TM | OP | NPC row | PR | Audience | Carrier |
|---|---|---|---|---|---|---|---|---|
| `IsPlayer` (pawn is bound) | Y | Y | Y | **N** | N | E | Team | C-TeamView `PawnRef` |
| `TeamMembership` of a pawn | Y | Y | Y | **N** | N | E | Team | C-TeamView |
| Team of a public name | Y | Y | Y | N (derivable: "not in my team view", accepted, Roster R6) | — | E | Owner, Team | C-TeamView |
| Pawn↔participant link | own | own | Y (`PawnRef`) | **N** | N | E | Team | C-TeamView |
| `Position` (visible pawn) | Y | Y | Y | Y | Y | Y | All | Pawn replicated movement |
| `Position` (concealed pawn; from commit until the outward leaf-plane crossing, R7.8) | Y | — (informants cannot hide) | Y (marker only, no body) | **N** (culled, R7) | N (concealed NPC culled for all) | N in MVP | Owner, Team | Pawn relevancy |
| Possession: the pawn's `PlayerState`, `Controller`, `Owner` pointers, client-side `PlayerState->GetPawn()`, controller↔pawn link, possession events (R14) | own | own | **N** (allies get `PawnRef` through C-TeamView, never engine pointers) | **N** | N | N (`NeverReleased`; any VS reveal uses the Event Log) | Owner | Owner's connection only (`COND_OwnerOnly`, R14); the owner's `PossessionEpoch` by the owner-only reliable RPC `ClientSetPossessionEpoch` (IIF R5.1, ADR-0011) |
| `AppearanceId` | Y | Y | Y | Y | Y | Y | All | Pawn (CPA) |
| Look-owner link | own | own | Y | N | — | E | Team | C-TeamView via `PawnRef` |
| `ActionPose`, `HeadLook`, `RemoteViewPitch`, `TraversalState` | Y | Y | Y | Y | Y | Y | All | Pawn (IIF R12, SCL R13) |
| `IsConcealed` | Y | — | Y | **N** (only "it went in", by line of sight) | N | N | Owner, Team | C-TeamView entry flag (Team); owner's own component `UConcealmentOwnerViewComponent.bIsConcealed` on the PlayerController (Owner; ADR-0017) |
| `IsInformant` (State) | Y | Y | Y | **N** | N | E | Owner, Team | C-TeamView |
| `StrikeCount` (true NPC-kill strike count) | N | N | N | N | — | E | ServerOnly | Penalty store (reconciliation 2026-10-24, Penalty CR-1; mirrors Penalty PEN-R3 and Assassination CR-2). **Confirmed by the user 2026-10-25** (Penalty OQ-NKP-1 closed). Never Team (art bible 7.7) |
| `WrongTargetTally` (count of the owner's `WrongTarget` plates; ticks identically for NPC and informant kills, an upper bound on strikes) | Y | Y (frozen) | **N** | N | — | E | Owner | `FStrikeResult.TallyAfter` (Assassination, owner-only) plus the C-TeamView owner part `{WrongTargetTally, LastResultSeq}` pushed on reclaim, rejoin and reset only (Penalty CR-19, Roster R6). **Confirmed by the user 2026-10-25** |
| `T_due` (informant respawn-takeover due time; server time, 0 = none) | own (while awaiting) | own (while awaiting) | **N** | N | — | E | Owner | C-TeamView owner part (Roster R6; Penalty PEN-R15; ADR-0001 §6). Written at scheduling and rescheduling, cleared at the binding; drives HUD F7's live countdown. Added by user decision 2026-10-25 (cross-review C-W5 closed). Adds no byte to any non-owner changelist |
| `RetireAtServerTime` (when a teammate's binding retires: the kill's `ContactServerTime`, or the release slot time) | — | — | Y | **N** | — | E | Team | C-TeamView teammate entry, set only in the retiring update (Roster R6; Teammate Markers TM-R6). Added by user decision 2026-10-25. An ally already learns the kill from the same update; the time adds only the exact blow moment it will see anyway |
| `DefenceState` (window, cooldown end) | Y | — | N | N | — | E | Owner | PlayerController (IIF R12) |
| Brace stance (`ActionPose=Brace`) | Y | — | Y | Y | Y | Y | All | Pawn (the P2 tell) |
| Kill confirmation (`VictimKind` as Player / Npc) | attacker only | — | N | N | — | E | Owner | PlayerController; informant kills present as `Npc` (Roster R10). The `WrongTargetTally` value and `bExiled` ride in the same owner-only `FStrikeResult`, identical in shape for every result, so the plate stays byte-identical for NPC and informant kills (Penalty CR-1; confirmed 2026-10-25). The same reply carries `ContactServerTime` (owner-only, the strike's contact server time, Assassination ASN-R19; added by user decision 2026-10-25) so HUD gates the plate on contact without inference; it is the time the attacker's own clip already shows |
| `FActionState.StartServerTime` (server time the current action pose began) | Y | Y | Y | Y | Y | Y | All | Pawn (`FActionState`, IIF R12), identical for every pawn kind; lets a re-created or late-relevant actor seek a hide/exit clip from server time (Concealment CON-R6a/b). Added by user decision 2026-10-25 |
| `FDeathState.bDiedConcealed` (the victim was concealed at the death write) | Y | Y | Y | Y | Y | Y | All | Pawn (`FDeathState`, Assassination ASN-R10), identical for every victim kind (NPCs conceal too); every non-owner client keeps that body presentation-hidden (Concealment CON-R9). Added by user decision 2026-10-25 |
| `ObjectiveProgress` own team | Y | Y | Y | — | — | Y | Team | Objectives' team view (provisional) |
| `ObjectiveProgress` opponent team | N | N | N | N | — | E | ServerOnly | — (the only exception is the `RivalPrelimCount` row below, in the ruling-B shape. `RivalStageStep` as first specified in Objectives OC-12 stays **denied** (review 2026-10-24, ruling B; L34)) |
| `RivalPrelimCount` (Subject = the round: count of the **opponent's** released preliminary completions, 0-2, never the final; no member or participant identity, site, position, order or timing; fresh per-completion delay U(30, 165) s on the `StageSignal` sub-seed keyed by the round-global completion ordinal; released only on the global 30 s grid anchored at `Active`, shared by both teams; quantum key `RivalSignalReleaseQuantumSec` owned by Objectives) | Y | Y | Y | **N** (a team's view is never sent to the other team) | — | Y (own team's view only) | Team | C-TeamView (`FObjectiveTeamView`, Objectives OC-12). Added at reconciliation 2026-10-24 (Objectives change request 3; creative-director approval at the Objectives design review as R3 requires; OQ4 closed; AC-IVP-40) |
| `SiteAssignment` (`FSiteAssignment`, zone→team, Selection sub-seed) | per Objectives | per Objectives | per Objectives | N | — | E | ServerOnly (Objectives owns any Team grant) | LSC server API |
| `SiteKnownToTeam` (Subject = court fixture: the court is a live site of the viewer's own team, Objectives OC-5) | Y | Y | Y | **N** | — | E | Team | C-TeamView (`FObjectiveTeamView` live-site ids). Derived from `SiteAssignment`; drives presentation (prompt, stage card, progress credit) only, **never ingress acceptance** (R7.4a) |
| `ConnectionState`, ping, join time | own | own | **N** (Session 24; the only teammate exception is the `TeammateReconnecting` bit below) | N | — | N (`NeverReleased`) | Owner | Owner's connection; the clock-exchange pair `ServerClockPing(T1)` / `ClientClockPong(T1, T2, T3)` (unreliable, owner only, timestamps only; ADR-0002 §9) |
| `TeammateReconnecting` (one bit: the teammate is disconnected inside its grace during `Active`; no time, reason, ping or address) | — | — | Y | **N** | — | N | Team | C-TeamView teammate entry `bReconnecting` (Roster R6), derived by the Roster push step from Session's `Connection` projection. Added by user decision 2026-10-25 (Session OQ7 closed). Allies may already infer a disconnect from the AI-held pawn's behaviour; the bit only makes it explicit to them. Never in any `All` carrier |
| `TeamPing` (a teammate's momentary ping: sender `PublicHandle`, per-sender `PingSeq`, 10 cm quantised world point, server time; Informant Pings PNG-R6) | Y (own echo) | Y (own echo) | Y | **N** | — | N | Team | C-TeamView part `UTeamPingViewComponent` (`FTeamPingView`, one slot per team member) on each team member's PlayerController. Added by user decision 2026-10-25 (Informant Pings moved to MVP). The point is the sender's own claim (no LOS check); it names no pawn; nothing about it reaches the pawn, the PlayerState or the GameState (PNG-R9) |
| Opponent counts (Active, Informant, AwaitingPawn) | N | N | N | N | — | Y | ServerOnly until `Ended` | `FRoundResult` only |
| `RoundTeamSummary` (per team: stages completed 0-3, members eliminated for the round at the round's end, team total of `WrongTarget` results; Round R9 (a) `FRoundResult.TeamSummary`) | N | N | N | N | — | **Y** (released in MVP, at the round-end step only) | ServerOnly until `Ended`, then All | `FRoundResult` in `FRoundPublicState` (Round R9, R19 `PublicResult`). Added by user decision 2026-10-25 (text-only round summary in MVP, HUD-E21). Team aggregates only: no per-participant value, no time, no NPC/informant split (wrong targets count NPC and informant kills alike, as the tally does). Never present before `Ended` |
| Own-team counts | Y | Y | Y | — | — | Y | Team | derived locally from C-TeamView |
| Round Event Log | N | N | N | N | — | per class | ServerOnly | Round; `PublicResult` → `FRoundPublicState` |
| Event Log **telemetry section** (playtest probes and aggregates, Round R19, ADR-0023) | N | N | N | N | — | N (`NeverReleased`) | ServerOnly | none: exported to the server's disk after termination, never replicated |
| Phase, `RoundIndex`, `PhaseChangeCount`, `MatchScore`, `bCalledOff`, `FRoundResult`/`LastResult`; `ExpectedPawnCount` (`D`, published in `Preparing`, constant for the round, reveals nothing beyond the lobby's `n / capacity`) and `ActiveAtServerTime` (non-zero only in `Preparing`) | Y | Y | Y | Y | — | Y | All | `FRoundPublicState` (GameState; Round R20, change request 19 confirmed at reconciliation 2026-10-24). Never a per-team or per-participant value, never an opponent count (Round R20) |
| `PublicHandle`, `DisplayName` | Y | Y | Y | Y | — | Y | All | C-Public (PlayerState allowlist) |
| Public tuning keys | Y | Y | Y | Y | — | Y | All | TDC snapshot (GameState); must be global values only |
| `TuningHash`, `DealDigest`, seeds' sub-seed values | N | N | N | N | — | E (`TuningHash`, sub-seed values); `ServerSecret` never | ServerOnly | Round Event Log |

**R4. Carriers and their audience rule.** Each audience has exactly one kind of carrier, and a carrier holds only its audience:

| Audience | Carrier | Rule |
|---|---|---|
| `All` | Shared Pawn (identical for NPC and player), C-Public PlayerState allowlist `{PublicHandle, DisplayName}`, GameState public structs (`FRoundPublicState`, TDC snapshot) | A pawn carries **only** `All` facts, identical in type, path, condition and timing for every pawn kind (hard invariant). A public struct carries no per-participant and no per-team value. |
| `Team` | C-TeamView, copied by the server into each teammate's own PlayerController (Roster R6) | No team-filtered replicated actor and no custom team relevancy filter, other than the R6/R7 concealment cull (which is keyed on concealment and applies identically to every pawn kind). |
| `Owner` | The owner's PlayerController (and the owner's own pawn only for `Owner`-conditioned engine fields made identical in kind, R5) | Never on the pawn as a gameplay field (IIF R12). |
| `ServerOnly` | Game-instance stores (Roster, Round, LSC, Penalty, Objectives) | Never a replicated property. Presentation modules may not include their query headers (R10). |

Consumers call the Policy (or read its carriers) and **never re-derive a rule**: no system decides on its own that a fact may go to a viewer. A system that needs a new fact or a new audience raises a change request to this GDD.

**R5. UE defaults the Policy overrides.** Each row is a hard requirement on the named implementer; the mechanism is -> ADR-001.

| Default (UE 5.8.4) | Why it leaks | Required treatment | Implementer | Tag |
|---|---|---|---|---|
| `APawn::PlayerState` replicates `COND_None` | Non-null on player pawns only | `COND_OwnerOnly` (condition reset via `FName` overload); `PlayerState->GetPawn()` must stay null on non-owners | SCL | [V] macro; S1, S14 |
| `APawn::Controller`, `AActor::Owner` `COND_None` | A PlayerController exports an unmappable NetGUID; an AIController also does, so null-parity cannot be assumed | Owner-only, and S1 must show **identical raw-bunch class** (null / mapped / unmappable) for both kinds | SCL | [V]; S1, S14 |
| `AActor::Instigator` `COND_None`, defaults to self in `PreInitializeComponents` | Differs if any spawn path sets it | Assert `Instigator == Self` for every pawn spawn path; no replicated spawned actor (effect, projectile) carries `Owner`/`Instigator` | SCL, IIF, Assassination | [V]; AC-IVP-06 |
| `Role`/`RemoteRole` | Must be `SimulatedProxy` on non-owners for both kinds | Verify (legacy: `FScopedRoleDowngrade` [V]; Iris [S11]) | SCL | [S] S13 |
| `RemoteViewPitch16` (`COND_SkipOwner`; the 8-bit `RemoteViewPitch` is deprecated, replay-only in 5.8) written by `APawn::PreReplication` from the control rotation only if a controller exists | Absent/stale for controller-less NPCs, control rotation instead of `Look()` for AI | Shared Pawn overrides `PreReplication` and writes pitch after Super from the `Look()` source for every pawn; controller presence never changes the value | SCL, Crowd | [V] |
| Other `AActor` props `bHidden`, `bTearOff`, `bCanBeDamaged`, `bReplicateMovement` (`COND_None`), `AttachmentReplication` (`COND_Custom`) | Any per-kind value or write timing is a detector | `bHidden` always false (presentation hide is client-local, CON-R16); `bTearOff` never set for any pawn (death is the shared fall sequence); `bCanBeDamaged` and `bReplicateMovement` constant; no replicated attachment (no attached replicated prop or effect) | SCL, Assassination | [V] |
| `APawn::PossessedBy` / `UnPossessed` call `ForceNetUpdate()` | Off-cadence bunch to **every** connection at takeover, rejoin and re-pawn: a possession event visible to non-owners | Shared Pawn overrides `ForceNetUpdate`: every unscoped call (the possession paths, `BecomeViewTarget`, the channel-saturation retry, the controller pause loop, any plugin) is **dropped**, and the next fixed-cadence update carries the change (ADR-0003 §2); only `ActionPose`/`TraversalState` writes and Assassination's uniform forced updates at the strike slot and the death write (`FActionState`, `FDeathState`, ASN-R21; reconciliation 2026-10-24) may force an update, with the same cap for every pawn kind (R14) | SCL | [V]; S14 |
| `bNetUseOwnerRelevancy`, `bOnlyRelevantToOwner`, `bAlwaysRelevant`, `NetPriority`, `NetCullDistanceSquared` (net config, not replicated) | With owner relevancy, `GetNetPriority`/relevancy forward to the owner (a controller), so scheduling follows possession | `bNetUseOwnerRelevancy = false`, `bOnlyRelevantToOwner = false`, priority and cull distance identical on every pawn; audited on class defaults and live instances (R11) | SCL | [V] |
| `APawn::IsNetRelevantFor` Super clauses (`RealViewer == Controller`, `IsOwnedBy`, `ViewTarget == Instigator`, `IsBasedOnActor` in **both** directions, forwarding to the movement base) | Each can keep a concealed pawn relevant | The cull override replaces Super completely for concealed subjects (`owner ∨ ally ∨ ¬concealed` only); for unconcealed pawns every pawn is always relevant (SCL R13) | SCL, Concealment | [V]; S9 |
| Engine spectator state (`NAME_Spectating`, `NAME_Inactive`, spectator RPCs, `SpectatorOnly` URL option, `ASpectatorPawn`) | Spectator RPCs cycle `PlayerArray` (humans only) and set the view target to a human's pawn; `bIsSpectator` replicates to all | Banned outright (R13) | Session, Round, Roster, Camera | [V] |
| PlayerState `Score`, `StartTime`, `bIsSpectator`, `bOnlySpectator`, `bFromPreviousLevel` (`COND_None`); `PlayerId`, `UniqueId`, `bIsABot`, `bIsInactive` (initial-only); `CompressedPing` (`COND_SkipOwner`) | Join order, ping, inactive flag are per-player and time-stamped | Disabled or held constant for the round (`bIsSpectator`, `bOnlySpectator`, `bIsInactive` constant **false**, never written, R13); `bShouldUpdateReplicatedPing = false`; `PlayerNamePrivate` carries `DisplayName` only; no PlayerState byte changes on any Roster transition | Roster, Session | [V]; S2 |
| Player array built on clients from received PlayerStates | Count and join/leave timing | Frozen in-round list (Session 22, 25a); NPCs have no PlayerState (`bWantsPlayerState = false`), so the array reveals the human count, which is public by design (lobby `n / capacity`) | Session, Roster | [V] |
| `IsPlayerControlled()` / `IsBotControlled()` helpers | Read `PlayerState` | Banned in client presentation code (include/grep check) | all presentation | [V] |
| Adaptive net update frequency; CMC cancels throttling on movement | Cadence follows input cadence, which differs PC vs AI | Fixed frequency for every pawn (SCL F9, 15 Hz), adaptive disabled | SCL | [S] S13 |
| `ReplicatedServerLastTransformUpdateTimeStamp` | Computed differently for remote players and AI under linear smoothing | Keep exponential smoothing and do not force the timestamp | SCL | [V] |
| `ACharacter` fields: `RepRootMotion` (carries a montage reference, i.e. a NetGUID), `ReplicatedBasedMovement`, `ReplicatedServerLastTransformUpdateTimeStamp`, `ReplicatedMovementMode`, `bIsCrouched`, `bProxyIsCharacterMovementActive`, `bProxyIsJumpForceApplied`, `AnimRootMotionTranslationScale`, `ReplicatedGravityDirection`, `ReplayLastTransformUpdateTimeStamp` (replay only) | Same wire fields for both kinds; a root-motion montage used by only one kind is a path-visible tell | Each allowlisted by name (R11); networked root motion only from montages in the shared `ActionPose` set used by NPCs and players alike, else forbidden | SCL | [V] |
| Relevancy (`IsNetRelevantFor`, `bAlwaysRelevant`) | Any kind-dependent relevancy is a detector | Every pawn always relevant (SCL R13) **except** the concealment cull of R7, keyed on concealment only | SCL, Concealment | [V] |
| GameState `ReplicatedWorldTimeSecondsDouble`, `bReplicatedHasBegunPlay`, `GameModeClass`, `SpectatorClass` | Not identifying | Allowlisted; `SpectatorClass` replicated as null (R13) | Round | [V] |
| Demo / replay recording (`COND_ReplayOnly` props) | A replay holds everything | Disabled in MVP builds | Session | [V] |

**R6. Pawn parity is the default; relevancy may vary by concealment only.** The Roster R6 parity rule, IIF R12 relevancy bans and SCL R13 stay binding with one amendment (change request to SCL and IIF): **a pawn's relevancy to a connection may depend on its concealment state and the viewer's relation, and on nothing else** — never on binding, controller kind or state. Dormancy stays banned.

**R7. Concealment enforcement: hybrid (decision).** Options considered: (a) client-side local hide only, (b) server relevancy culling only, (c) hybrid. **Chosen: (c).** Client-side hide alone needs every client to hold the concealed pawn and its position, so a modified client sees through every bush; a replicated "concealed" flag would also be a permanent detector if NPCs lack it. Server culling is the only position-hiding mechanism available before Alpha. Culling alone pops the body on the network's schedule instead of at the leaf plane. Rules (Concealment implements; Policy owns the audience):
1. **Enforcement (server).** When Concealment commits a pawn as concealed, the server stops replicating that pawn to every connection for which `CanViewerKnow(V, S, Position)` is false: every non-ally connection for a player pawn, **every** connection for a concealed NPC pawn. Self and allies (active or informant) keep relevancy. The cull is keyed on the concealment state alone. Concealed pawns stay culled through `Ended`: MVP releases no concealed position at the round's end (Concealment change request 2, OQ7).
2. **Cull timing.** The cull lands at the **first point of the global pawn-update grid at or after** `t0 + ConcealCullDelayMs` (F3): the grid is the multiples of `1 / PawnNetUpdateHz` counted from the round's `Active` entry, shared by every pawn. It replaces a per-pawn boundary because the engine gives each actor's next update time a random delay, so per-pawn boundaries are not a common grid (ADR-0003 §5.3, E14). It is a fixed quantised delay that never waits for a client acknowledgement (an ack-dependent cull is a timing channel). All culls committed in one server frame land on the same grid point.
3. **Presentation (client).** Every client, including allies, hides the body locally at the swap-at-occlusion instant of the `BushEnter` animation (Concealment CON-R5 splits the former `BushTransition` into `BushEnter` / `BushExit`) (art bible 5.2.1), driven by the replicated `ActionPose`, never by a network arrival, on the rendered frame of the animation's swap notify (±1 frame at 60 Hz). Allies then show the hatched marker and no body. Shadow, reflection, Lumen and footstep-decal casters hide with the body.
4. **Blind attack (consequence).** A culled pawn does not exist on the attacker's client, so no prompt appears and the attacker can only strike a spot. `Attack(TargetSpot)` resolves on the server against server positions (server movement and AI tick regardless of relevancy [V]); hit tests use the capsule, not animated bones. **One resolver, the id is only a hint.** Every strike resolves through one server function from `WorldPoint` and capsule overlap. A client `PawnRef` is honoured only through the strike-target gate `CanViewerKnow(Attacker, Pawn, Position)` (IIF R10; answer over time in R7.8), and then only to break a tie among capsule candidates for which that gate is true. The gate selects a server-internal resolution path and never an accept/reject decision: an id that is visible, concealed or culled, stale, or random produces the same outcome class, reply code, rate-limit budget, ingress treatment and timing class as the equivalent `Attack(TargetSpot)` (F7 id classes). Assassination owns range and tolerance; the spot tolerance is set at the scale of the bush, so a blind strike is a choice to commit to that bush, not an aiming test.
4a. **Court ingress (same principle).** IIF ingress accepts any court fixture id (public geometry, Objectives' feint rule); `SiteKnownToTeam` decides presentation and progress credit only. Reply, hold duration and timing are identical for an own live site, an own non-site, a rival site and a decoy court, and no fixture carries a replicated busy/occupied/in-use state. Rate and abuse budgets are keyed on the sender, never on a fixture's site status.
5. **Un-cull.** On `HideExit` execution or on `OnConcealmentBroken` (attacking from the bush), the pawn becomes relevant to all again **in the same server step** as the state change. **Kills (revised at reconciliation 2026-10-24, Assassination change request 13 and Concealment change request 11; replaces "un-cull and death time in the kill step"):** nothing re-opens in the strike's outcome slot, so the wire stays outcome-blind until contact (ASN-R21, AC-ASN-20). A victim culled for a reason other than concealment re-opens at Assassination's **death write** (`ContactServerTime − StrikeNetLeadSec`), already falling, with the replicated death server time so clients fast-forward, identically for every victim kind. A victim **killed while concealed** re-opens at the death write like any other, carrying `FDeathState.bDiedConcealed` = true so every non-owner client keeps the body presentation-hidden (CON-R9; field approved by user decision 2026-10-25, which retires the earlier interim rule of keeping it relevancy-closed through fall and removal). For a concealed subject the cull predicate replaces the whole `IsNetRelevantFor` body (controller, view-target, instigator and both based-on clauses), and every actor owned by or attached to the pawn uses owner relevancy or joins the same cull (Concealment CON-R2; L27). The exit transition keeps the body behind the leaf plane for at least `T_uncull` (F4); clients show the body only at the outward leaf-plane instant.
6. **NPC parity is mandatory (requirement on Crowd).** NPCs use bushes through the same Concealment code path, with entry, dwell and exit distributions such that the cull-event posterior meets F5. **No automatic fallback (review 2026-10-24, creative-director ruling).** `HidePosteriorFactor` stays 1.5 (Crowd's request for 2.0 is rejected); Crowd meets F5 by raising NPC bush use, not by loosening the test. If Crowd cannot meet F5 at the MVP-core checkpoint (latest AC-IVP-25 run), the case is escalated to the creative-director and the user; client-side hide (`bConcealServerCull = false`) lets a modified client see every bush occupant (L29, CRITICAL) and may be switched on only for internal development builds, never by default. A cull that only players can trigger is never shipped.
7. **Engine path.** Legacy driver: an `IsNetRelevantFor` override alone lags up to `min(RelevantTimeout, 1 s)` plus up to 0.5 s jitter before the channel closes [V], so the cull needs an explicit per-connection channel close at the deadline [S9]. Iris: per-connection exclusion-group filter status, which destroys the object remotely [V]. Re-open creates a new client actor instance [V]; NetGUID reuse is [S10]. The explicit close must keep the relevancy predicate false for that connection on every later frame (`bIsRecentlyRelevant` and `ForceRelevantFrame` must not re-create the channel) [S9]. Client-side registries (animation significance, audio, decals, pawn lists) must tolerate a relevancy destroy, and nothing gameplay-visible may distinguish a cull from a real destroy [S10]. The Iris-vs-legacy choice (S11) is ADR-001's; under Iris the F3 delay is applied at the filter flip.
8. **`Position` answer over time** (the strike-target gate and `IsConcealedFrom` read this, never the relevancy state). For a non-ally viewer V and subject S, from the server's Concealment record and timestamps:

| Phase of S | `CanViewerKnow(V, S, Position)` | Why |
|---|---|---|
| `Outside`, or `BushEnter` playing before the server commit | true | Clients still render the pawn |
| From commit t0 (`CullPending`, `Culled`, `Concealed`) | false | Clients hid the body at the swap notify (R7.3); a `PawnRef` here names a pawn nobody renders |
| `HideExit` executed, before the outward leaf-plane crossing (`Exiting`) | false | Relevancy re-opens at `HideExit` (R7.5) but the body is not shown until `T_exit_occluded` |
| After the outward leaf-plane crossing (`Emerging`, `Outside`) | true | Body visible |
| Killed while concealed | true from the death write (re-opens falling with `bDiedConcealed`, body still presentation-hidden by CON-R9; R7.5, user decision 2026-10-25) | The body falls inside the dome and is never drawn for non-owners either way (art bible 5.2.1 rule 3); nothing re-opens in the outcome slot |

Self and allies: always true. A concealed NPC: false for every viewer through the same phases.

**R8. Teammate markers.** Ally pawns are never culled for allies, so the marker's position comes from the ally's pawn through C-TeamView `PawnRef`. **Fallback feed:** for any teammate entry whose `PawnRef` is unresolved on that client (travel re-push, a takeover in flight, a future ADR-introduced cull distance), the server adds a quantised position `{PublicHandle, Pos (10 cm grid, int16 x3)}` to that viewer's C-TeamView at `TeamPositionFeedHz` (F6); each entry also carries a one-byte `Seq` (wrapping) that the server advances on every feed tick, so a stationary ally's entry still changes and Teammate Markers' staleness rule F5 works (Markers CR-11c; added by user decision 2026-10-25). It is removed only **one feed update after that connection acknowledges the pawn's channel open** (the client's `PawnRef` resolves only after the open bunch arrives; ADR-0003 §7). **Definitions (Teammate Markers CR-2, CR-11 a-b; reconciliation 2026-10-24):** `Pos` is the pawn's **feet** (its visual root: the skeletal mesh component's world location), so the tracked-to-fed handoff does not jump by the capsule half-height. The feed's trigger is evaluated **per connection**: the teammate's pawn exists on the server and that connection has no open channel for it, or one whose open is not yet acknowledged. **Invariant (TM-R4a):** the feed entry is added in the same C-TeamView update in which the `PawnRef` becomes unresolvable for that connection, and only to teammate connections, so a client never sees "null `PawnRef`, no feed" for a pawn that still exists for longer than one update. Opponents' clients never receive it and never create the widget.

**R9. Informants.** (a) Informants cannot see concealed **non-allies** (no x-ray): they are viewers of class `OpponentInformant` ≡ `Opponent`. They keep their allies' markers, including the hatched marker of a concealed ally. (b) Enemies see informants exactly as NPCs: every `IsInformant` and `IsPlayer` answer for opponents is `N`, the informant's pawn carries only `All` facts, and the informant-kill invariant (Roster R10) makes the attacker's feedback identical to an NPC kill. (c) Informants never conceal (IIF R7: Move/Look only). The takeover (cross-GDD rule C1 and its extension: informant respawn and a returning informant whose pawn is gone) never picks a pawn that is `Entering`, `Concealed`, `Exiting` or `Emerging` (Concealment `IsInBushOrTransition`). The takeover is a possession change and follows R14; an Active player's `PawnRecovery` re-pawn is a new spawn through the crowd emergence path and follows L4 (one spawn path, identical to NPC replacement). (d) **Accepted informant-side residual** (Penalty PEN-R14.3, CR-20; creative director): an awaiting informant may see its own takeover take longer than its countdown (respawn starvation). It is Owner-only, reveals nothing about any other pawn, and adds no fact to R3.

**R10. Listen-server host caveat (accepted for prototype).** The host process holds every `ServerOnly` fact in memory: bindings, concealed positions, every PlayerController's owner data, the Event Log, guests' IdentityKeys. `ServerOnly` protects against clients, not against the host. Accepted for trusted playtests only; the Dedicated Server (Alpha) removes it. The host is a **trusted full-information client** (a documented, accepted leak), and on the host's screen remote-player pawns arrive by `ServerMove` while NPCs tick locally (L6, attacker X). **Trusted playtest** means named testers under a no-recording, no-build-sharing agreement, no stream, a development build with no open CRITICAL row (R12): no public demo, festival build or open test ships while L14 or L21 is still Accepted. **Evidence playtests** (any session whose data feeds F5, F8 or AC-IVP-25) seat a non-competing developer as host, or exclude the host team's data as contaminated; the headless-server option was deferred to Alpha by the user 2026-10-25 (OQ8 closed), so this rule stands for every evidence playtest until the Alpha dedicated server; AC-IVP-36 checks the session record. Still binding on the host: presentation reads only the host player's C-TeamView, owner data and `FRoundPublicState`; presentation modules may not include server query headers (build-time include check); no debug overlay that reads server data ships; the Event Log is written to disk only at `Ended`/`Reset`; demo recording is disabled. The host also has zero latency (Roster R6 accepted limit).

**R11. Replicated-property audit (default deny).** A checked-in allowlist names every replicated property, its condition and its audience for the Shared Pawn (and its components and subobjects), PlayerState, PlayerController, GameState and every replicated actor class that clients receive. An automated test enumerates them by reflection (`GetLifetimeReplicatedProps` on the class default object, mapped through the class rep layout, including Blueprint replication lists; real instances spawned in a test world for components and subobjects; under Iris, the replication-state descriptors [S11]) and **fails the build on any property not in the allowlist or with a different condition**. A new replicated field therefore cannot ship without a Policy row.
- **One allowlist file, two owners.** The allowlist is a single checked-in data file (`PawnReplicationAllowlist` plus sections for PlayerState, PlayerController, GameState) read by both the Policy's audit and Shared Character's tests. Shared Character authors the Shared Pawn rows (SCL R13); the Policy owns the file, the audience column and the parity tests (AC-IVP-01, 02, 42, 45); a row changes only with both owners' sign-off. Its minimum Shared Pawn content is the R5 `AActor` and `ACharacter` sets, replicated movement, `HeadLook` yaw, `RemoteViewPitch16`, `ActionPose`, `TraversalState`, `AppearanceId`, and `PlayerState`/`Controller`/`Owner` as `COND_OwnerOnly`. Forbidden on the pawn: replicated attachments, an `AbilitySystemComponent` or other replicated gameplay-state component beyond the one shared `ActionPose` component (identical class, creation order and flags on every pawn), and any replicated Concealment state (CON-R2 is server-only).
- **Net config is audited too**: `NetUpdateFrequency`, `MinNetUpdateFrequency`, `NetPriority`, `NetDormancy`, `NetCullDistanceSquared`, `bNetUseOwnerRelevancy`, `bOnlyRelevantToOwner`, `bAlwaysRelevant` and the adaptive-frequency settings must be equal on class defaults **and** on live instances of an NPC pawn, a remote-client pawn, a host pawn and a just-taken-over pawn (runtime differential audit, AC-IVP-45).

**R12. Pillar-1 leak catalogue.** Every known surface, its mitigation and owner. Attacker: H honest UI, M modified client, S packet capture, W stream/screenshot watcher, X listen host. Status: Mit mitigated by a rule here or in the owner GDD; Spk pending spike; Acc accepted until Alpha; Req requirement raised to a GDD not yet written.

| # | Surface | Att. | Mitigation | Owner | Status |
|---|---|---|---|---|---|
| L1 | **Possession** (the most serious Pillar-1 item): pawn `PlayerState`/`Controller`/`Owner` references and `PlayerState->GetPawn()` on non-owners (engine default `COND_None` makes every player pawn identifiable from its own data) | M,S | R14: `COND_OwnerOnly`, absent from every non-owner changelist for both kinds; canary AC-IVP-37 runs per commit | SCL, Policy (tests), Roster | Spk S1, S14 (known prototype leak `IsNpc()` until then) |
| L2 | PlayerState default fields | M,S | R5, AC-PSR-44 allowlist | Roster, Policy | Spk S2 |
| L3 | Player array or name-list change mid-round | M,S | Frozen list, `RebindView` | Session, Roster | Mit; S4 |
| L4 | NetGUID or creation order (spawn, NPC replacement, takeover, cull re-open) | M,S | Shuffled interleaved spawn; one spawn path for every kind; replacements and re-opens never kind-dependent | Round, Crowd, SCL | Mit (spawn); Spk S10 (re-open) |
| L5 | Update cadence, bytes/s, adaptive frequency, dormancy | S,M | Fixed 15 Hz, no dormancy, adaptive off | SCL | Spk S13; F2 |
| L6 | Movement texture (input jitter, heading quantisation, corrections) | M,S,X | Same CMC config and smoothing; AI fed through the same command path | SCL, Crowd | Spk (net emulation); F2 |
| L7 | `RemoteViewPitch` / head-look distribution | M,S | One `Look()` path, crowd gaze catalogue | SCL, Crowd | Spk; F2 |
| L8 | `ActionPose` timing | all | Server-written, forced update, same delay U | IIF | Mit (pose itself is the P2 tell) |
| L9 | Rejection replies: code, timing, silent vs replied; target-id oracle | M | IIF R6 drain-end replies; R7.4 culled-id → spot | IIF, Assassination | Mit; F7 |
| L10 | Kill confirmation; informant-kill feedback; death side effects (replacement spawn, destroy timing) | H,M,W | Owner-only confirmation; informant kill presents as NPC (tally ticks identically, Penalty PEN-R3); one fall/removal sequence; nothing re-opens in the outcome slot (R7.5). **Accepted residuals** (Assassination ASN-R21, change requests 3 and 13; reconciliation 2026-10-24): (1) a modified-client **bystander** (no Owner or Team audience on the strike) may learn kill vs reversal at most `StrikeNetLeadSec` before contact; (2) the attacker, the target and the target's allies may learn the outcome up to `T_v` early from Owner/Team data (Roster state, replies, `FStrikeResult`, `FDefenceResult`); honest clients present nothing before the local contact frame (ASN-R6, R23) | Assassination, Penalty, CPA, Crowd | Mit (ASN-R19, R21, R23; PEN-R3, R14); residuals Acc; AC-IVP-14 |
| L11 | Voice and chat | S,W | In-engine voice/chat disabled; external voice unmanaged; the MVP team ping is Team audience through C-TeamView only (`TeamPing` row; user decision 2026-10-25) | Session, Informant Pings | Acc |
| L12 | Footstep and action audio | H | One sound set per gait and surface for all pawns; culled pawns silent for culled viewers | Diegetic Audio (VS), SCL | Req |
| L13 | Concealment cull as a remote "someone hid" detector | S,M | NPC bush use meeting F5 (R7.6) | Concealment, Crowd | Req (CRITICAL) |
| L14 | Every client holds every visible pawn's position (wall-hack, live classifier) | M | Accepted while always-relevant; LOS-based pruning at Anti-Cheat | SCL, Anti-Cheat | Acc (not shippable beyond trusted playtests) |
| L15 | `Owner`/`Instigator` on spawned effects or projectiles | M,S | No replicated spawned actor carries them; presentation from pawn state | SCL, IIF, Assassination | Mit by R5; AC-IVP-06 |
| L16 | RPC routing (client RPCs only on owned actors; multicasts) | S | No gameplay RPCs on pawns; owner data on the controller; multicasts identical for NPC and player actions | IIF, SCL | Mit |
| L17 | Components and subobjects present only on player pawns (camera boom, input component) | M | One component set; local-only components created only on the locally controlled client and never replicated | SCL, Camera | Spk (S1 extension) |
| L18 | Animation significance LOD | H,M | Significance takes no player-ness input | SCL | Mit |
| L19 | Client logs, console, `stat net`, net dumps | H,M | Stripped in Shipping; no log line names pawn kind | DevOps, Anti-Cheat | Acc |
| L20 | Crash dumps, Insights traces | M,X | `ServerOnly` stores not allocated on clients; no auto-upload | DevOps | Acc |
| L21 | Listen host memory and files | X | R10 | Roster, Round | Acc until Dedicated Server |
| L22 | Reveal timing | all | One round-end step (Round R9; staged to the deciding strike's death write); crowd stops by the normal path | Round | Mit |
| L23 | Streams and screenshots (ally markers, informant intel, followed camera) | W | Streamer alias mask, "do not stream live" informant reminder, broadcast-delay advice (art bible 7.7) | HUD, Settings | Acc |
| L24 | Opponent counts, objective state, court fixtures | M | R3; fixtures carry no replicated state; selection from `ServerSecret` sub-seeds only. **Accepted witness-side residual** (Penalty PEN-R14.2, CR-20; creative director): an exiled striker's released body may reveal one enemy removal to a witness who strikes it (a `WrongTarget` within the stale-read grace) or watches its exit | Round, Objectives, LSC, Penalty | Mit; Spk (fixtures); residual Acc |
| L25 | Look-to-participant link carried into the next round | all | Looks re-dealt every round (CPA deal per round) | CPA | Mit |
| L26 | Public tuning snapshot with per-team or per-participant values | M | Snapshot holds global values only (R4) | TDC | Mit |
| L27 | Pawn based on (standing on) a concealed pawn keeps it relevant via `IsBasedOnActor` | M | Cull override drops the based-on clause for concealed subjects | SCL, Concealment | Spk S9 |
| L28 | Blind-attack outcome as a bush-occupancy probe | M,H | Accepted as the designed P2 risk/reward. The cost of a probe comes from P3: the strike is visible, a whiff is a failed attack, and a strike that kills a concealed NPC is an NPC-kill strike. No cooldown or timer gates it | Assassination | Spk S15 |
| L29 | Client-side-hide mode (`bConcealServerCull = false`): every client holds every bush occupant (x-ray of the hide mechanic) | M | Never the default; internal development builds only; escalation rule R7.6 | Policy, Concealment | Acc (dev builds only) |
| L30 | Engine spectator path: `NAME_Spectating`/`NAME_Inactive`, `ServerViewNextPlayer`-family RPCs cycling `PlayerArray` (humans only), `SpectatorOnly` option, `bIsSpectator` flags | M,S | R13 ban, AC-IVP-38 | Session, Round, Roster, Camera | Req |
| L31 | Possession events: `ForceNetUpdate` in `PossessedBy`/`UnPossessed`, owner-relevancy forwarding, role switch at takeover, rejoin, `PawnRecovery` | S,M | R5, R14: unscoped forced update dropped, the next fixed-cadence update carries the change; non-owner changelist empty at the possession frame | SCL | Spk S14 |
| L32 | NetGUID allocation gaps (controllers, PlayerStates and pawns share one counter; rejoin exports new GUIDs) | S | All PlayerControllers and PlayerStates allocated in `Preparing` before any pawn spawn; S10 records gaps against events | Session, Round, SCL | Spk S10 |
| L33 | Strike or court id oracle (accept/reject, reply, budget or timing keyed on the id's visibility or site status) | M | R7.4, R7.4a one resolver; F7 id classes | IIF, Assassination, Objectives | Mit; AC-IVP-39, 41 |
| L34 | Opponent progress signal timing (a delayed sub-stage count confirms a watched hold was real) | M,H | R3: opponent progress ServerOnly except the `RivalPrelimCount` row (preliminary-stage count only, never the final step, fresh per-completion delay, one global 30 s release grid; Objectives OC-12, F4 bound `LR_sig ≤ 2.4`); `RivalStageStep` stays denied | Objectives | Mit by the R3 row (reconciliation 2026-10-24); AC-IVP-40 |

**Severity and ship gate.** CRITICAL: L1, L2, L4, L13, L29, L30, L31. Every other row is HIGH unless it is `Acc` for a VS/Alpha system. **Any CRITICAL row with status `Spk` or `Req` blocks every build that leaves the development team, including invited and evidence playtests** (R10); this is checked by AC-IVP-33 and the canary AC-IVP-37, which does not wait for spikes.

**R13. The engine spectator state is never used.** Every PlayerController is in `NAME_Playing` from login to `Reset`, including with no pawn; `NAME_Spectating` and `NAME_Inactive` are unreachable. Informants between pawns, `AwaitingPawn` participants, `PawnRecovery` waits and rejoiners in `Ended` view through a **client-local, non-replicated camera actor** set through the owner-only view-target path; the view target is never another participant's pawn, PlayerState or controller. Required (-> ADR-001): the game's PlayerController forces `NAME_Playing` on login and accepts no other state (any other `ChangeState`, `BeginSpectatingState`, `BeginInactiveState`, `StartSpectatingOnly`, `SetSpectatorPawn` or the `PawnPendingDestroy` state change is a fault-sink report and a no-op); the spectator server RPCs (`ServerViewNextPlayer`, `ServerViewPrevPlayer`, `ServerViewSelf`, `ServerSetSpectatorLocation`, `ServerSetSpectatorWaiting`, `ServerRestartPlayer`) fault and return; the GameMode has `bStartPlayersAsSpectators = false`, `MustSpectate`/`CanSpectate`/`PlayerCanRestart` false and strips `SpectatorOnly` at login (Session rule 6); `SpectatorClass` is null; `bIsSpectator`, `bOnlySpectator`, `bIsInactive` are never written. A grep check bans `ASpectatorPawn`, `ViewAPlayer`, `SetIsSpectator`, `StartSpectatingOnly` and `ChangeState(NAME_Spectating|NAME_Inactive)` in game code (AC-IVP-38). Owners: Session (login, options), Round (pawnless and `Ended` states), Roster (transitions), Camera (local camera); the Policy owns the test.

**R14. Possession never reaches a non-owner client.** Possession means the pawn's `PlayerState`, `Controller` and `Owner` pointers, the client-side `PlayerState->GetPawn()`, the controller↔pawn link and every possession event (takeover per rule C1 and its extension, rejoin, `PawnRecovery`, hand-over to bound AI). Rules (Shared Character implements; the Policy owns this leak row L1/L31, the allowlist and the parity tests with Shared Character):
1. `APawn::PlayerState`, `APawn::Controller` and `AActor::Owner` are reset to `COND_OwnerOnly` in the Shared Pawn's replicated-property registration (condition reset helper; not the per-actor active-override toggle) [V]. Non-owners therefore never receive these properties for either kind; tests assert **absence** from every non-owner changelist, not equal null/unmappable classes.
2. A possession change produces no non-owner-visible bunch: `ForceNetUpdate` from `PossessedBy`/`UnPossessed` is dropped and the next fixed-cadence update carries the change (R5), owner relevancy is off, and role changes are visible to the new owner only.
3. Owner-side effects (`OnRep_Controller`, camera management, RPC routing through the owner's connection) stay functional; RPC routing uses the server-side connection and is unaffected [V]. S14 confirms no breakage.
4. Audits run over three controller classes: host PlayerController, remote PlayerController, AIController (R11, F1, F2).
5. The engine spectator state is never a substitute for possession (R13).

**R15. Future-tier guards.** Any cosmetic enters the NPC appearance deck at equal weight or is cut (CPA); no accessibility or settings feature may give directional or player-only information beyond the default presentation (Settings & Accessibility, VS).

### States and Transitions

The Policy is stateless per query. Two things change its answers:

**Viewer class over a round** (per viewer-subject pair): `Lobby`/`Preparing`: only `All` facts and the viewer's own and team data exist. `Active`: R2 classes; a Roster transition (Active → Informant) changes SA→SI or TM→TM (no knowledge change, invariant). `Ended` (Round round-end step (R9), same step for all): every pair → `PostRound`; `PostRoundEligible` content may be released by the VS Reveal; MVP releases `FRoundResult` only. `Reset`: everything round-scoped is discarded; nothing carries over.

**Per-pawn, per-connection relevancy (R7):**

| State | Entered by | Non-ally connection receives | Ally / self receives | Exit |
|---|---|---|---|---|
| `Visible` | default; un-cull | pawn (all `All` facts) | pawn + C-TeamView | Concealment commits → `CullPending` |
| `CullPending` | concealment commit at t0 | pawn (stationary, body already hidden locally at the swap instant) | pawn | t0 + `ConcealCullDelayMs` → `Culled`; exit/break/kill before then → `Visible` |
| `Culled` | deadline reached | nothing (actor destroyed on the client) | pawn (no body drawn), hatched marker | `HideExit` executes, `OnConcealmentBroken` → `Visible` in the same server step; kill → `Visible` at the death write with `bDiedConcealed` (body stays presentation-hidden, R7.5); never in the outcome slot |

For a concealed NPC every connection is "non-ally". `Reset` destroys every pawn; there is no carried relevancy state.

### Interactions with Other Systems

| System | Data in / out | Owner of the interface |
|---|---|---|
| Participant State & Roster | In: `GetRelation`, `IsPawnBound`, `GetState`, `GetTeam`, `GetParticipant`. Out: ratified audiences for C-Public, C-TeamView, C-Server; AC-PSR-44/45 ownership | Roster supplies facts; Policy owns audiences |
| Round & Match Framework | In: phase (`PostRound` from the round-end step, R9), Event Log release classes. Out: release rule (only in `Ended`, only through the Policy) | Round (phase, log); Policy (who may read) |
| Session & Travel | Rule 25a-g checklist; Policy's audit (R11) enforces it | Session (checklist), Policy (tests) |
| Shared Character & Locomotion | Pawn allowlist (SCL R13) is an R11 input; implements R5 overrides and the R7 cull hook | SCL |
| Input & Interaction Framework | `ActionPose` All; owner-only status on the controller; R7.4 culled-id handling of `Attack(TargetPawn)` | IIF |
| Concealment (Bushes) | Implements `IsConcealedFrom(Viewer)` = `not CanViewerKnow(V, S, Position)` for a concealed subject; R7 timing; `OnConcealmentBroken` triggers un-cull | Concealment (behaviour), Policy (audience) |
| Crowd & NPC Behaviour | NPC bush use meeting F5; never reads `IsPlayer`; takeover never picks a concealed pawn | Crowd |
| Assassination | Spot-attack resolution of culled pawns; kill confirmation Owner; nothing re-opens in the outcome slot, re-open at the death write, killed-concealed victim re-opens with `bDiedConcealed` (R7.5, user decision 2026-10-25) | Assassination |
| NPC-Kill Penalty & Informants | `StrikeCount` ServerOnly and `WrongTargetTally` Owner (confirmed 2026-10-25); informant classes; takeover rule R9c; residuals in L24 and R9d | Penalty |
| Camera System | Neutral view through a client-local camera actor, never the spectator state (R13); local-only camera components (L17); presentation include check (AC-IVP-17) | Camera |
| Objectives Chain | `ObjectiveProgress` and `SiteAssignment` audiences; fixtures carry no replicated state | Objectives (narrower audiences) |
| Teammate Markers, HUD | Read C-TeamView only (R8, R10) | Markers, HUD |
| CPA, LSC, TDC | Their audience rows in R3 | each owner |

## Formulas

The Policy defines no gameplay balance curve. Its numeric rules are leak budgets (pass/fail gates for tests), two timing bounds for the concealment cull, and one bandwidth bound. The base rate a non-ally viewer faces is `b = n_P / (n_P + n_N)` = 5 / (5 + 50) = 0.0909 (5 opponents among 55 non-ally pawns at defaults; `NpcCount` 50 since 2026-10-25).

### F1. Structural leak count

`S = | Schema(P) Δ Schema(N) |`, pass iff `S = 0`

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| Schema(P) | set | — | measured on a non-owning client | Raw-bunch schema of player pawns: property handles and order, bit-length class, condition, NetGUID-resolution class (null / mapped / unmappable), components and subobjects, RPC set |
| Schema(N) | set | — | measured | The same for NPC pawns |
| S | int | 0 to ∞ | calculated | Size of the symmetric difference |

**Output range:** 0 (pass) or ≥ 1 (fail). Measured over ≥ 3 clients and ≥ 100 pawn lifetimes that include spawn, cull and re-open, death, hand-over and rejoin. **Example:** the prototype replicates `APawn::PlayerState` with `COND_None`; player pawns carry a mapped reference, NPC pawns null, so `S ≥ 1` and the build fails (AC-IVP-02).

### F2. Classifier advantage (transport leak budget)

`SE = sqrt((n1 + n2 + 1) / (12 · n1 · n2))` (Hanley-McNeil at AUC = 0.5); pass iff `AUC_hat ≤ AUC_max` and `AUC_hat + 1.645 · SE_boot ≤ AUC_upper`

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| AUC_hat | float | 0-1 | calculated | Area under the ROC curve of a classifier (player vs NPC) trained on per-pawn received feature windows (inter-arrival time, bunch size, speed and acceleration variance, heading-change rate, pitch distribution, snap count), aggregated per pawn-round, train/test split by match |
| n1, n2 | int | ≥ 300 / ≥ 3000 | test config | Player and NPC pawn-round units (60 matches × 5 opponents, × 50 NPCs) |
| SE_boot | float | ~0.018 | calculated | Cluster bootstrap over matches (2,000 resamples); `SE` above is its sanity value |
| AUC_max | float | 0.50-0.60 | test constant | Point threshold, 0.55 (MVP), 0.52 (Alpha) |
| AUC_upper | float | 0.52-0.62 | test constant | One-sided 95 % upper-bound threshold, 0.58 (MVP), 0.55 (Alpha) |

**Method:** behaviour is held constant by replaying the same recorded human command streams (`Move`, `Look`, actions) through the pawn command interface, once from a PlayerController on a test net client and once from an AIController, so only transport differences remain (behavioural tells are P2 and tested elsewhere). The gate sits on the round aggregate because weak per-window leaks compound (a window AUC of 0.52 over 60 independent 10 s windows becomes about 0.65 per round). **Output:** pass/fail. **Example:** `SE = sqrt(3301 / (12 · 300 · 3000)) = 0.0175`. `AUC_hat = 0.53` → upper bound 0.53 + 0.029 = 0.559 ≤ 0.58, pass. `AUC_hat = 0.56` fails the point test. Meaning for a viewer's single best guess among 55 pawns: AUC 0.50 → 9 % (chance), 0.55 → about 13 %, 0.60 → about 18 %, 0.70 → about 29 %.

### F3. Concealment cull delay

`D_cull = q · ceil((L99 + 1 / f_net) / q)`

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| L99 | float (ms) | 50-300 | network profile (test: PktLag 100 ms, 2 % loss) | p99 one-way server-to-client latency, default 150 |
| f_net | float (Hz) | 10-30 | SCL F9 | Pawn net update frequency, 15 |
| q | float (ms) | 50 | constant | Quantum, so the delay carries no fine timing |
| D_cull | int (ms) | 100-1000 | → `ConcealCullDelayMs` | Time from concealment commit to the cull |

**Output range:** 100-1000 ms, clamped. **Example:** `50 · ceil((150 + 66.7) / 50) = 50 · 5 = 250 ms`. During `D_cull` a modified client still receives a stationary pawn whose entry the viewer could already see; after it, nothing.

### F4. Un-cull lead (requirement on Concealment's exit transition)

`T_uncull = L99 + 1 / f_net + T_reopen`; require `T_exit_occluded ≥ T_uncull`

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| T_reopen | float (ms) | 16-100 | provisional pending S10 | Channel open plus client actor creation, 33 |
| T_exit_occluded | float (ms) | — | Concealment GDD | Time from `HideExit` execution until the body crosses the leaf plane outward |
| T_uncull | float (ms) | — | calculated | Time the non-ally client needs to hold the pawn before it may be shown |

**Output:** a lower bound on Concealment's timing. **Example:** `150 + 66.7 + 33 = 250 ms`. An exit transition of 0.6 s whose leaf-plane crossing is at 0.35 s gives 350 ≥ 250: pass. A pawn re-opened at a death write (a victim culled for a non-concealment reason, or a killed-concealed victim carrying `bDiedConcealed`) arrives up to `T_uncull` late, and the fall is fast-forwarded by the replicated death time (R7.5). **Cross-GDD invariant (Concealment change request 2, reconciliation 2026-10-24):** `T_uncull` at the `LeakNetProfile` (250 ms) is the floor for Concealment's `BushExitCrossSec` and `BushStrikeEmergeSec` (Concealment F3 b, c; TDC cross-field check); if spike S12 measures a larger L99, `T_uncull` and both floors are recomputed together.

### F5. Hide-event posterior (NPC bush-use parity)

`P_hide = n_P · r_P / (n_P · r_P + n_N · r_N)`; pass iff `P_hide ≤ k · b`, equivalently `r_N / r_P ≥ n_P · (1 − k·b) / (k · b · n_N)`

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| n_P | int | 1-5 | `TeamSize` | Non-ally player pawns (opponents) |
| n_N | int | 20-60 | `NpcCount` | NPC pawns |
| r_P, r_N | float (/min) | ≥ 0 | measured in playtest / Crowd tuning | Bush entries per pawn per minute, players and NPCs |
| k | float | 1.0-1.5 | test constant | Allowed posterior inflation over base rate, 1.5 (the ceiling; review 2026-10-24 ruling A: never raised to meet Crowd, R7.6) |
| P_hide | float | 0-1 | calculated | Probability that a pawn seen (or detected) entering concealment is a player |

**Output range:** 0-1. **Example:** `k·b = 3/22` (0.136); required `r_N / r_P ≥ 5 · (19/22) / ((3/22) · 50) = 0.633`. If players hide 3× as often as NPCs, `P_hide = 15 / (15 + 50) = 0.23`: fail. `r_P` comes from the server-side Concealment entry log of internal playtests (≥ 10 matches); F5 bounds only the unconditional entry rate. The context of a player's hide (entering while watched, after an attack, near an objective, the timing of the exit) remains the P2 tell, and Crowd must not be tuned to match those contextual distributions; NPC dwell and exit behaviour is recorded in the Baseline and Tell Matrix (AC-IVP-34), not a transport test. **Why k stops at 1.5 (review 2026-10-24, creative-director ruling A):** the cull fires for every hide city-wide, so a modified client sees every entry event, not only the sighted ones; at k = 2.0 (`P_hide ≤ 0.18`) a packet watcher who logs each cull narrows the five opponents to about one entry in five or six across the whole map, every minute. Crowd's request for k = 2.0 (Crowd T5, change request 1, OQ1) is therefore declined; the gap is closed by raising `r_N` (Concealment recommends 0.19/min at k = 1.5), never by loosening k. A Crowd posterior target above 1/6 is not a valid design input to this test.

### F6. Team position fallback feed

`B_feed = N_u · s · h`

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| N_u | int | 0-4 | runtime | Teammate entries with an unresolved `PawnRef` on this client |
| s | int (bytes) | 13 | constant | 4 B handle + 6 B quantised position + 1 B `Seq` (user decision 2026-10-25) + 2 B overhead |
| h | float (Hz) | 0-10 | `TeamPositionFeedHz` | Feed rate |
| B_feed | float (B/s) | 0-520 | calculated | Added per-client bandwidth |

**Example:** worst case `4 · 13 · 2 = 104 B/s`, 0.21 % of the 50 KB/s per-client budget (the `Seq` advance makes every entry change every tick, so the worst case is also the steady case while entries exist).

### F7. Timing parity (side channels)

For each transport timer T in {rejection reply delay, `ActionPose` delay U, kill-confirmation delay}: pass iff `|d| ≤ 0.1`, `σ²_P / σ²_N ∈ [0.8, 1.25]`, `KS D ≤ 0.1`, with `n = 2 · (z_α + z_β)² / d²` samples per class

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| d | float | — | calculated | Cohen's d between player-target and NPC-target samples |
| z_α, z_β | float | 1.645, 0.84 | constants | One-sided 5 % significance, 80 % power |
| n | int | ≥ 1236 | calculated | Samples per class |

**Example:** U is uniform over 0-66.7 ms (σ = 19.3 ms), so the largest allowed mean shift is 1.9 ms; `n = 2 · 2.485² / 0.01 = 1235.0` → 1236 per class.

### F8. Human strip test (replaces art bible 7.7 test 1, change request)

`h_fail = ceil(Σ_i n_i · c_i + 1.645 · sqrt(Σ_i n_i · c_i · (1 − c_i)))` over scenes i

| Variable | Type | Range | Source | Description |
|---|---|---|---|---|
| n_i | int | 8 per scene, 96 total | test design | Trials of scene i (12 scenes × 8 testers) |
| c_i | float | per scene | scene design | Chance level of scene i = enemies shown ÷ figures shown (target 1/9) |
| h_fail | int | — | calculated | Hits at or above which the test fails |

**Example** (every scene at 1/9): `Σ n_i·c_i = 10.67`, `sqrt(96 · 0.111 · 0.889) = 3.08`, `h_fail = ceil(10.67 + 5.07) = 16`. The art bible's 25 %-over-36-trials version passes a true 25 % leak about 60 % of the time, so it is too weak to gate.

**Relevancy rule (no distance formula).** `Relevant(S, V) = (V is S's owner) ∨ Ally(V, S) ∨ ¬Concealed(S)`. Cull distance is unlimited for every pawn (SCL R13); no per-kind distance exists.

## Edge Cases

- **If a player disconnects while concealed:** the pawn stays bound, AI-held and concealed (Round R17: participant-bound AI never toggles concealment), culled for non-allies. On `ReleaseToCrowd` it becomes a concealed NPC: it stays culled for non-allies and becomes culled for its former allies at the same boundary. Their marker entry was already cleared with the transition. Crowd exits it later on the NPC dwell distribution.
- **If a player rejoins while concealed:** nothing changes for any non-ally connection; the rejoiner receives its pawn and C-TeamView only.
- **If two or more pawns (any kinds) commit concealment in the same server frame:** all culls land at the same net-update boundary `D_cull` later; their order carries no information.
- **If concealment is broken or the pawn exits during `CullPending`:** the cull is cancelled and the pawn never leaves `Visible`; clients re-show the body at the outward leaf-plane instant.
- **If a concealed pawn is killed:** nothing re-opens in the outcome slot; the pawn re-opens at Assassination's death write with the death state, the death server time and `FDeathState.bDiedConcealed` = true (R7.5), so every client receives it already falling and fast-forwards, and every non-owner client keeps the body presentation-hidden (CON-R9); the fall/removal sequence is identical for NPC, player and informant victims (art bible 5.2.1 rule 3).
- **If an attacker sends `Attack(TargetPawn)` with the id of a pawn culled for them:** resolved exactly as `Attack(TargetSpot)` at the command's `WorldPoint`; no distinct reply, timing or code.
- **If a viewer's client lags (RTT above HUD's `HudLatencyWarnRttSec`):** the local hide still happens at the animation's swap instant; the server cull never waits on that client.
- **If a pawn stands on (is based on) a concealed pawn:** the based-on relevancy clause is ignored for concealed subjects; the concealed pawn stays culled.
- **If a viewer becomes an informant while watching a concealed opponent:** no change (class invariant).
- **If an informant respawn takeover candidate is concealed or in `BushEnter`/`BushExit`:** it is ineligible (R9c).
- **If the round ends while pawns are culled:** they stay culled for non-allies through `Ended` (MVP; any reveal is the VS Reveal's). `Reset` destroys every pawn.
- **If bandwidth saturates:** every pawn degrades by the same priority; no per-kind priority exists.
- **If a client sends a team, participant id or viewer class:** ignored; viewer identity is the server connection (Roster R2).
- **If seamless travel re-creates PlayerControllers:** C-TeamView `PawnRef`s are cleared at `InvalidateAllBindings` and re-pushed after the controller swap; until then the fallback feed (R8) is not sent because no pawn exists; markers stay hidden.
- **If a teammate's `PawnRef` is unresolved during `Active`:** the fallback feed runs at `TeamPositionFeedHz` until it resolves.
- **If the listen host's player inspects memory or files:** they see everything (R10). Accepted for trusted prototype playtests; the host's screen still matches a client's (AC-PSR-42).
- **If the host quits:** no migration; the session ends for all with the same generic message (Session); nothing is released.
- **If Iris replaces the legacy driver:** S1, S2, S7, S9, S10 and F1/F2 are re-run under Iris before any build ships with it.
- **If Crowd cannot meet F5:** no automatic fallback and no change to k (R7.6, ruling A). The case is escalated to the creative-director and the user at the MVP-core checkpoint. Until it is resolved, only internal development builds may run client-side hide (`bConcealServerCull = false`, L29 CRITICAL; L13 accepted there); no build that leaves the team runs it, and a player-only cull is never shipped.
- **If a new replicated property is added without a Policy row:** the audit test fails the build (R11).
- **If a debug command or overlay that reads server data runs on a client or the host:** compiled out of Shipping; in development builds it requires a PIE-only flag and logs a warning.

## Dependencies

The index row 7 lists Roster and Round as hard upstream dependencies and SCL, Session, IIF, CPA and LSC as soft peers (verified 2026-10-24, cross-review C-W11). The Policy keeps that narrow: it **reads facts** from them and from nothing else at query time. The other links below are *peer contracts* (the other system implements an audience the Policy declares) and are soft for the Policy, hard for the implementer.

### Upstream (the Policy depends on)

| System | Interface the Policy uses | Hard / Soft | Status |
|---|---|---|---|
| Participant State & Roster (#2) | `GetRelation(PawnA, PawnB)` (viewer class), `IsPawnBound` (fact `IsPlayer`), `GetState` (`IsInformant`), `GetTeam` (`TeamMembership`), `GetParticipant`; R6 carriers C-Public, C-TeamView, C-Server | Hard | Designed (NEEDS REVISION; R6 consistent) |
| Round & Match Framework (#6) | Phase (`Ended` flips every viewer to `PostRound` in the round-end step, R9), Event Log release classes, `FRoundPublicState`, the no-opponent-count rule | Hard | Reviewed - MAJOR REVISION NEEDED 2026-10-24 (re-review pending) |
| Tuning Data & Config (#1) | Registers the Policy's knobs (CR-15); TDC uses the Policy's `ServerOnly | All` audience subset for tuning keys | Soft | Reviewed - MAJOR REVISION NEEDED 2026-10-24 (re-review pending) |

### Peer contracts (the other system implements a Policy audience)

| System | What it implements | Hard for whom | Status |
|---|---|---|---|
| Shared Character & Locomotion (#9) | Pawn replicated-property allowlist (R13), pawn-pointer suppression (S1), replication parity, the per-viewer concealment cull hook (R7 here) | Hard for SCL | Designed (in progress) |
| Session & Travel (#5) | Rule 25a-g leak checklist (frozen PlayerStates, suppressed join/leave broadcasts, connection data never to non-owners) | Hard for Session | Designed |
| Input & Interaction Framework (#3) | `ActionPose` audience `All`; owner-only `FHoldStatus`, rejection codes, cooldown end on the PlayerController; reply-timing guard (R6) | Hard for IIF | Designed |
| Character Pool & Appearance (#4) | `AppearanceId` `All`; look-owner link `ServerOnly` until `PostRound` | Hard for CPA | Designed |
| Level Socket Contract (#8) | `FSiteAssignment`, zone-to-team map and Selection sub-seed `ServerOnly`, released to `PostRound` | Hard for LSC | Reviewed - MAJOR REVISION NEEDED 2026-10-24 (re-review pending) |

### Downstream (these depend on the Policy)

| Dependent | Interface it uses | Hard / Soft | Status |
|---|---|---|---|
| Crowd & NPC Behaviour (#11) | Must never read `IsPlayer`; red-herring duty for any tell whose network surface reaches non-allies (bush use, R7.6); takeover blend parity | Hard | Reviewed, revisions applied. Conflict resolved: Crowd withdrew k = 2.0 and designs to F5 at k = 1.5 with r_N 0.19/min (T5, AC-CNB-15); measurement still due at the MVP-core checkpoint (OQ3) |
| Concealment (Bushes) (#12) | Implements `IsConcealedFrom(Viewer)` through `CanViewerKnow(V, S, Position)`; owns transition timing; uses R7 hybrid enforcement; killed-concealed target rule (CON-R9, `bDiedConcealed`) | Hard | Reviewed, revisions applied; consistent (k 1.0-1.5, `BushEnter`/`BushExit`, change requests 3 and 12 applied) |
| Assassination & Defence Reversal (#13) | Kill confirmation audience Owner (attacker only); `Attack(TargetSpot)` resolution against server positions of culled pawns (R7.4); informant-kill invariant; outcome slot vs death write (R7.5); L10 residuals | Hard | Reviewed, revisions applied |
| NPC-Kill Penalty & Informants (#14) | `StrikeCount` ServerOnly and `WrongTargetTally` Owner (confirmed 2026-10-25); informant viewer classes; takeover eligibility R9c (the line-of-sight rule C1 is Penalty's own); residuals R9d, L24 | Hard | Reviewed, revisions applied |
| Objectives Chain (#15) | `ObjectiveProgress` Team (own) / ServerOnly (opponent); `RivalPrelimCount` Team (R3 row, ruling-B shape); reads `FSiteAssignment` server-side only; `SiteKnownToTeam` presentation only, ingress accepts any court (R7.4a) | Hard | Reviewed, revisions applied. Conflict resolved: OC-12 rewritten to ruling B; R3 row added, OQ4 closed (reconciliation 2026-10-24) |
| Teammate Markers (#17) | C-TeamView `PawnRef` and the Team `Position` fallback feed (R8: feet position, per-connection trigger, same-update invariant) | Hard | Reviewed, revisions applied |
| HUD (#18) | Reads only C-TeamView, owner data and `FRoundPublicState` (R10 host parity); leak rows L10, L22, L23, L34; AC-IVP-17 | Hard | Reviewed, revisions applied |
| Camera System (#10) | Neutral view through a client-local, non-replicated camera actor, never the engine spectator state (R13, L30); camera components local-only (L17); presentation include check (AC-IVP-17) | Soft (Policy side) / Hard for Camera | Reviewed, revisions applied |
| Informant Pings (#20, MVP since 2026-10-25) | Ping audience Team, momentary, place-anchored (`TeamPing` row, C-TeamView part) | Hard | In Review (design review NEEDS REVISION 2026-10-25, revisions applied; re-review pending) |
| Post-Round Reveal (#21, VS) | `PostRound` class, `PostRoundEligible` Event Log content | Hard | Not written |
| Diegetic Audio (#22, VS) | Audio parity: no sound path keyed on player-ness; concealed-pawn audio culled with the body for non-allies | Hard | Not written |
| Event Feedback & Cues (#23, VS) | All cues route through Owner/Team audiences | Soft | Not written |
| Settings & Accessibility (#24, VS) | Parity review: no setting adds directional or player-only information (R13) | Soft | Not written |
| Telemetry (#26), Anti-Cheat (#27), Dedicated Server (#28) (Alpha) | Leak catalogue as audit input; host caveat removal; LOS-based culling upgrade | Soft | Not written |

**Bidirectionality.** Roster, Round, Session, IIF, CPA, LSC, SCL and TDC already name the Policy. Every downstream GDD must list "Information Visibility Policy" in its Dependencies. The index change request (SCL, Session, IIF, CPA, LSC as soft peers of #7) has been applied to row 7 (verified 2026-10-24, cross-review C-W11).

## Tuning Knobs

Shipped keys (TDC CR-15 columns; registered under owner `InformationVisibilityPolicy`):

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / what breaks |
|---|---|---|---|---|---|---|---|---|---|
| `ConcealCullDelayMs` | int32 | IVP | 250 | 100-1000 | 200-400 | ServerOnly | Round | ms | F3. Too low: under lag the pawn is culled before slow clients reach the swap instant and pops out of view early (visible to honest viewers); below `L99 + 1/f_net` it also leaks lag. Too high: a modified client keeps the concealed pawn's position longer. Must be a multiple of 50. Interacts with SCL `f_net` and the test network profile. |
| `TeamPositionFeedHz` | float | IVP | 2.0 | 0-10 | 1.5-4 (bounded below by Teammate Markers I-TM-1, `TeammateMarkerFeedStaleSec ≥ 3/Hz` at its default 2.0 s; cross-review C-W18) | ServerOnly | Round | Hz | F6. 0 disables the fallback (markers vanish while a `PawnRef` is unresolved). High values cost bandwidth for no gain; positions are only a fallback. |
| `TeamPositionFeedGridCm` | int32 | IVP | 10 | 5-50 | 10-25 | ServerOnly | Round | cm | Quantisation of the fallback position. Coarse values make the marker jitter; fine values cost bits only. |
| `bConcealServerCull` | bool | IVP | true | — | — | ServerOnly | Round | — | R7.6. false = client-side hide (L29 CRITICAL: every client holds every bush occupant). Must be false whenever NPC bush use is off or the latest F5 run failed (AC-IVP-30); such a build is internal development only and the F5 failure is escalated, never answered by raising k. |

Test constants (not shipped keys, held in the test harness config; changing them is a GDD change):

| Constant | Value (MVP / Alpha) | Formula | Effect |
|---|---|---|---|
| `LeakAucMax` / `LeakAucUpper` | 0.55 / 0.58 (Alpha 0.52 / 0.55) | F2 | Lower = stricter transport-leak gate, needs more samples |
| `LeakMinMatches` | 60 | F2 | Fewer matches widen SE; below 30 the gate cannot detect AUC 0.60 reliably |
| `HidePosteriorFactor` (k) | 1.5 (allowed range 1.0-1.5; 1.5 is the ceiling) | F5 | Lower = stricter (needs more NPC bush use). Higher lets the cull identify players more and is not permitted (ruling A, R7.6): Crowd meets F5 by raising `r_N`, never by raising k |
| `TimingMaxCohenD`, `TimingVarRatio`, `TimingMaxKsD` | 0.1, [0.8, 1.25], 0.1 | F7 | Looser bounds admit measurable timing channels |
| `StripTestTrials` / `StripTestFailHits` | 96 / 16 | F8 | Fewer trials lose power to detect real leaks |
| `LeakNetProfile` | PktLag 100 ms, PktLoss 2 %, L99 150 ms | F3, F4 | The profile all leak tests and cull timings are measured under. If S12 measures a different L99, the Policy owner recomputes F3 and updates `ConcealCullDelayMs` (GDD change). |

## Visual/Audio Requirements

The Policy owns no assets. It owns the **constraint** that the art bible 5.2 viewer-state matrix and 7.7 are network-true, not only render-true:

| Requirement | Art bible source | Policy rule |
|---|---|---|
| Opponent sees "bush only: no body, no shadow, no reflection" | 5.2, 5.2.1 rule 2 | R7: the concealed pawn is culled for non-allies, so shadow, Lumen, reflection, footstep decal and audio cannot leak; the local hide at the swap instant covers the gap before the cull lands |
| Swap at occlusion, mirrored on exit, no fade | 5.2.1 rule 1 | R7.3: the local hide and the re-show are timed by the `ActionPose = BushEnter` / `BushExit` animations, never by a network arrival |
| Ally marker exists only on allies' clients | 5.2, 7.7 rule 2 | R8: marker data travels only in C-TeamView; opponents' clients never create the widget |
| Kill presentation identical for NPC / player / informant | 5.2.1 rule 3 | R5 informant-kill invariant inherited from Roster R10; kill confirmation is Owner-audience UI only |
| Accent colour local, never replicated | 7.7 rule 3 | Owner-local setting, no audience |
| No per-pawn audio difference | 5.2 forbidden list | R4: audio follows the pawn's audience; a culled pawn emits nothing to that viewer (Diegetic Audio, VS) |

*art-director not consulted separately (budget rule); the rows restate approved art bible rules and add only their network enforcement.*

## Game Feel

Not a feel system; its feel contribution is **absence**: (1) a bush entry looks the same on every screen at the leaf-plane instant, with no pop when the network cull lands (the local hide already happened); (2) a blind strike at a bush feels identical whether it hits or whiffs until Assassination's result, because the attacker's client never held the target; (3) the `Ended` reveal is one instant for everyone (Round R9 round-end step). Latency target: the local hide is frame-exact to the animation (0 ms network dependence); the server cull lands `ConcealCullDelayMs` (F3) after the commit so a modified client cannot keep a concealed position fresh.

## UI Requirements

The Policy defines no screen. It binds every UI that shows information about another participant:
- Presentation code reads only the viewer's C-TeamView, the viewer's own owner-only data and `FRoundPublicState` (Roster R6 host-parity rule; AC-IVP-17 include check). It never calls `CanViewerKnow`, never reads a pawn pointer and never computes a fact.
- No widget, prompt, reticle or audio cue may differ by subject kind for non-allies (art bible 5.2 forbidden list, 7.7 rule 4). The assassination prompt is identical for every valid in-range target and is absent for a concealed target because the viewer holds no actor (R7.4).
- The in-round player list (if shown, Roster OQ11) shows the frozen name list only: no team grouping for opponents, no status, no count.
- Streamer mode (art bible 7.7, VS): aliases replace names in labels, lobby and the reveal; an informant "do not stream live" reminder. It adds no data and removes no marker by default.

## Cross-References

| This GDD references | Element | Where used | Status |
|---|---|---|---|
| design/gdd/game-concept.md | Pillars P1, P2, P4; locked bush, informant, marker rules | Overview, R7, R9 | Locked |
| design/gdd/participant-state-roster.md | R6 carriers, accepted limits, opponent-count and host-presentation rules; R10 queries and informant-kill invariant; OQ0 spikes S1-S8; AC-PSR-42/44/45 | R1-R5, R10, AC | Designed |
| design/gdd/round-match-framework.md | R9 latch and round-end steps, R17 bound-AI constraint, R19 Event Log classes, R20 `FRoundPublicState` | R2, R3, States | Designed |
| design/gdd/session-travel.md | Rules 22, 24, 25a-g | R5, R12 | Designed |
| design/gdd/shared-character-locomotion.md | R13 always relevant, pawn allowlist; F9 net frequency | R5, R6, F3 | Designed (in progress) |
| design/gdd/input-interaction-framework.md | R6 rejection timing, R7 informant legality, R12 audiences and relevancy bans | R3, R6, R7.4 | Designed |
| design/gdd/character-pool-appearance.md | `AppearanceId` All, look-owner link | R3 | Designed |
| design/gdd/level-socket-contract.md | R9 `FSiteAssignment` ServerOnly, `PostRound` release | R3 | Designed |
| design/gdd/tuning-data-config.md | CR-8 audiences, CR-15 knob table | R3, Tuning Knobs | Designed |
| design/art/art-bible.md | 5.2 matrix, 5.2.1 swap at occlusion, 7.7 per-viewer summary, streamer mask, tests | R3, R7, UI, F8 | Written |
| design/gdd/assassination-defence-reversal.md | ASN-R10, R21, R23 (outcome slot, death write, killed-concealed `bDiedConcealed` rule), I-ASN-4 residuals; change requests 3, 13 | R5, R7.5, R7.8, L10 | Reviewed, revisions applied |
| design/gdd/npc-kill-penalty-informants.md | PEN-R3 (tally), PEN-R14 residuals, OQ-NKP-1; CR-1, CR-20 | R2, R3, R9d, L24 | Reviewed, revisions applied |
| design/gdd/objectives-chain.md | OC-12 `RivalPrelimCount`, F4; change request 3 | R3, L34, OQ4 | Reviewed, revisions applied |
| design/gdd/teammate-markers.md | TM-R4, TM-R4a; CR-2, CR-11 | R8 | Reviewed, revisions applied |
| design/gdd/concealment-bushes.md | CON-R2, CON-R9 (target rule in force), F3; change requests 2, 11 | R7.1, R7.5, F4 | Reviewed, revisions applied |
| design/gdd/crowd-npc-behaviour.md | T5, AC-CNB-15; change request 1 | Cross-References 4, OQ3, AC-IVP-34 | Reviewed, revisions applied |
| design/gdd/camera-system.md | CS-R12.2 neutral view, CS-R2 | R13, L17, Downstream | Reviewed, revisions applied |
| design/gdd/hud.md | HUD-R1 three carriers, tally display | R2, Downstream | Reviewed, revisions applied |
| docs/engine-reference/unreal/modules/networking.md | Iris status; no coverage of relevancy internals, so R5/R7 claims come from installed 5.8.4 source | R5, R7 | Reference |

**Change requests raised by this GDD (for the orchestrator; not applied here):**
1. **Shared Character & Locomotion:** amend R13 "relevancy can never be a tell" to R6 here (relevancy may vary by concealment and relation only); add the cull hook (legacy: `IsNetRelevantFor` override plus explicit channel close; Iris: exclusion groups), the based-on-clause exception (L27), `Instigator == Self` assertion, fixed net frequency with adaptive throttling off, and the S9/S10/S13/S14 spikes.
2. **Input & Interaction Framework:** amend R12 relevancy bans the same way; add R7.4 (an `Attack(TargetPawn)` naming a pawn culled for the attacker resolves as `Attack(TargetSpot)` with no distinct reply).
3. **Concealment (not written):** implement R7; exit timing `T_exit_occluded ≥ T_uncull` (F4); `IsConcealedFrom(V)` defined through `CanViewerKnow`; state who sees entry and exit by line of sight; add no dwell timer.
4. **Crowd & NPC Behaviour (now designed; review 2026-10-24 adds: k stays 1.5, its k = 2.0 request is declined, raise `r_N` instead; see OQ3):** NPCs use bushes through the same Concealment path, meeting F5 (`r_N / r_P ≥ 0.633` at 50 NPCs; recompute at the live `TeamSize` and `NpcCount`) on the unconditional entry rate; NPC dwell and exit timing are recorded in the Baseline and Tell Matrix as advisory evidence (AC-IVP-34, Crowd AC-CNB-15), not a transport test (F7 covers transport timers only; reconciled 2026-10-24 per Crowd change request 1); Baseline and Tell Matrix row "bush enter/leave: red herring yes"; F5 bounds only the unconditional entry rate, and Crowd must not be tuned to match the contextual distributions of player hides (entering while watched, after an attack, near an objective, exit timing), which remain the P2 tell; never read `IsPlayer`.
5. **Assassination (now designed; "un-cull and death time in the kill step" superseded by Assassination change request 13, see R7.5):** spot-attack resolution against capsules of culled pawns; nothing re-opens in the outcome slot; kill confirmation Owner-only; the probe cost is P3 (a whiff is a failed attack; killing a concealed NPC is an NPC-kill strike), never a cooldown or timer; spot tolerance at bush scale, so a blind strike commits to a bush rather than testing aim (S15).
6. **NPC-Kill Penalty & Informants (now designed):** takeover never picks a concealed or transitioning pawn (R9c); `StrikeCount` never Team. *(Superseded by Penalty CR-1: `StrikeCount` ServerOnly, `WrongTargetTally` Owner; confirmed by the user 2026-10-25.)*
7. **Teammate Markers (not written):** consume `PawnRef` plus the R8 fallback feed (answers Roster OQ8).
8. **Objectives Chain (now designed):** own-team progress Team, opponent progress ServerOnly unless a signal meeting the R3 row and OQ4 conditions is approved by the creative director ("no team" means no member or participant identity); court fixtures carry no replicated state; IIF ingress accepts any court and `SiteKnownToTeam` drives presentation only (R7.4a). Review 2026-10-24: OC-12 `RivalStageStep` denied as specified; revise to a preliminary-only count with a fresh per-event delay and a global release quantum, never the final step (OQ4).
9. **Art bible 7.7:** replace strip test (1) with F8 (96 trials, fail at 16).
10. **Systems index row 7:** add SCL, Session & Travel, IIF, CPA, LSC as soft peers; add Event Feedback & Cues and Settings & Accessibility as soft dependents.
11. **Participant State & Roster:** none blocking; its R6 is ratified unchanged. Note in R6 that `AActor::Owner` of an AI pawn also exports an unmappable GUID (null parity is not automatic).
12. **Concealment (Bushes):** narrow its F5 variable table's k range from 1.0-2.0 to 1.0-1.5 to match F5 here (review 2026-10-24, ruling A). *(Applied by Concealment.)*

**Change requests received (reconciliation 2026-10-24):**

| From | Request | Disposition |
|---|---|---|
| Objectives CR 3 | Add the R3 Team row `RivalPrelimCount` (ruling-B shape, CD-approved); close OQ4; L34 Mit by the row and AC-IVP-40 | APPLIED: R3 rows (opponent progress, new `RivalPrelimCount`), L34, OQ4 closed, Downstream row |
| Penalty CR-1 | `StrikeCount` ServerOnly (N before `Ended`, E after); add `WrongTargetTally` (SA Y, SI Y frozen, TM N, OP N, PR E, Owner, `FStrikeResult` + C-TeamView owner push); kill confirmation notes the byte-identical plate | APPLIED (R3 rows, R2 invariant wording, Interactions, Downstream); confirmed by the user 2026-10-25 (OQ-NKP-1 closed) |
| Penalty CR-20 | Witness-side residual (PEN-R14.2) on L24; informant-side Owner-only residual (PEN-R14.3) on R9 | APPLIED: L24, R9 (d) |
| Assassination CR 3 | Bounded `StrikeNetLeadSec` bystander residual on L10 | APPLIED: L10 (status Req → Mit with accepted residuals) |
| Assassination CR 13 | R7.5 / our CR 5: nothing re-opens in the slot; non-concealment cull re-opens at the death write; killed-concealed re-opens at the death write with `bDiedConcealed` (approved 2026-10-25; was an interim closed rule); L10 both residuals (≤ `StrikeNetLeadSec`, Owner/Team ≤ `T_v`) | APPLIED: R7.5, R7.8 row, States table, Interactions, F4 note, AC-IVP-13, L10, CR item 5 marked superseded |
| Assassination CR 14-19 | Addressed to Round, Roster, IIF, SCL, TDC, Camera (death-write staging, `OnRoleChangePresented`, `Disabled` at the death write, uniform forced net updates) | No Policy change needed except consistency: R5 `ForceNetUpdate` row now allows the uniform slot and death-write updates (from item 17) — APPLIED |
| Concealment CR 11 | R7.5 re-open at the death write, R7.8 timed from it, cull predicate replaces the whole `IsNetRelevantFor` body, owned/attached actors follow owner relevancy, L27 | APPLIED (merged with Assassination CR 13: the death-write re-open is the target rule; the interim rule that kept a killed-concealed victim closed was superseded 2026-10-25 by `bDiedConcealed`) |
| Concealment CR 2 | R9c / AC-IVP-31 use `BushEnter`/`BushExit`/`Emerging`; concealed pawns stay culled through `Ended`; the 250 ms bound as a cross-GDD invariant | APPLIED: R7.1 sentence, F4 invariant note (`T_uncull` floors `BushExitCrossSec`, `BushStrikeEmergeSec`); R9c and AC-IVP-31 ALREADY DONE (state names) |
| Teammate Markers CR-2 | Fallback feed `Pos` is the pawn's feet (visual root) | APPLIED: R8 |
| Teammate Markers CR-11 (a), (b) | Per-connection feed trigger; same-update invariant (TM-R4a) | APPLIED: R8 |
| Teammate Markers CR-11 (c) | Per-entry feed sequence for staleness | APPLIED 2026-10-25 (user decision; R8 feed `Seq`) |
| Crowd CR 1 (new) | Reconcile Cross-References item 4 ("F5 and F7 on dwell time") with the F5 note (dwell parity advisory) | APPLIED: item 4, AC-IVP-34 note |
| Crowd (T5, AC-CNB-15) | k = 2.0 request withdrawn; r_N 0.19 at k = 1.5 | ALREADY DONE there; OQ3 design side closed, Downstream row updated |
| Camera (Dependencies, CS-R12.2) | Neutral view without spectator state; L17; AC-IVP-17 | ALREADY DONE (R13, L17, L30, AC-IVP-17, AC-IVP-38); Camera added to Interactions and Downstream (bidirectionality) — APPLIED |
| HUD (Dependencies row 7) | Reads C-TeamView, owner data, `FRoundPublicState`; L10, L22, L23, L34; tally replaces strike pips | APPLIED: R2 wording, Downstream row; the rest ALREADY DONE (R4, R10) |
| Round CR 19 | Pawnless state never uses spectator flags; `ActiveAtServerTime`, `ExpectedPawnCount` are `All` Preparing fields | APPLIED: R3 `FRoundPublicState` row; spectator part ALREADY DONE (R13) |
| Round CR 10 | `PostRound` class from `Ended`; Event Log release classes; host-parity check | ALREADY DONE (R2, R3, R10, AC-IVP-17) |
| Session CR 4, SCL CR 9, CPA CR 11, LSC CR 11, TDC CR 6, Roster OQ8/OQ15 | Allowlist and frozen name list; pawn allowlist; `AppearanceId`/look-owner rows; `FSiteAssignment` ServerOnly; `ServerOnly | All` tuning subset; PlayerState allowlist, team position feed | ALREADY DONE (R3, R5, R8, R11, L3, AC-IVP-42) |
| IIF CR 12 | `CanViewerKnow(Attacker, Pawn, Position)` as the `PawnRef` gate; owner team's site-visibility query for the ingress filter | First half ALREADY DONE (R7.4, R7.8); second half DECLINED: ruling D (R7.4a), ingress accepts any court and `SiteKnownToTeam` is presentation only |
| Cross-review 2026-10-24 S-B1 (Round R9), C-W18 | `PostRound` flips at Round's round-end step; `TeamPositionFeedHz` safe range rejected by I-TM-1 | APPLIED (cross-review fixes 2026-10-24): R2 rule 1, L22, AC-IVP-20; `TeamPositionFeedHz` safe 1.5-4 |
| Cross-review 2026-10-24 C-W11 | Stale index claims | APPLIED (cross-review fixes 2026-10-24): Dependencies intro and Bidirectionality |
| ADR-0003 sync 2026-10-24 | R7.2 wording: global pawn-update grid replaces the per-pawn boundary | APPLIED (R7.2) |
| ADR-0003 sync 2026-10-24 | R5 wording: unscoped possession `ForceNetUpdate` dropped, the next cadence update carries the change | APPLIED (R5 row, L31, R14 item 2) |
| ADR-0003 sync 2026-10-24 | R8 / TM-R4a: feed entry kept until one update after the channel open is acknowledged | APPLIED (R8 trigger and removal) |
| ADR-0002 sync 2026-10-24 | R11/R12: owner-only clock exchange RPC pair (timestamps only) under the owner connection/ping fact | APPLIED (R3 `ConnectionState`/ping carrier; existing Owner fact, no new fact) |
| ADR-0011 sync 2026-10-24 | Leak-catalogue row for `ClientSetPossessionEpoch` (Owner) | APPLIED (R3 possession row carrier; existing Owner fact) |
| ADR-0017 sync 2026-10-24 | Allowlist: owner component's `bIsConcealed` (Owner audience, existing R3 fact) | APPLIED (R3 `IsConcealed` carrier; the R11 allowlist file gains the row when written) |
| ADR-0023 sync 2026-10-24 | Release-class table: telemetry section under `NeverReleased` | APPLIED (new R3 row) |
| User decision 2026-10-25 (scope; Informant Pings to MVP) | Fact row and allowlist row for the team ping part | APPLIED (R3 `TeamPing` row; L11; Downstream row; AC-IVP-46) |
| User decision 2026-10-25 (scope; Session OQ7) | Team-audience teammate-reconnecting bit | APPLIED (R3 `TeammateReconnecting` row; `ConnectionState` row notes the one exception; AC-IVP-46) |
| User decision 2026-10-25 (scope; HUD OQ2) | Per-team round summary released at `Ended` | APPLIED (R3 `RoundTeamSummary` row, PR = Y in MVP; AC-IVP-46) |

## Acceptance Criteria

*qa-lead consulted. Tags: [Unit] deterministic automation test with checked-in fixtures (the R3 matrix, the R4 carrier table, the allowlist) · [Int] test world · [PIE-2]/[PIE-3] multi-client PIE · [Net] under `LeakNetProfile`, asserted as bounds (PktLoss is not seedable) · [Stat] nightly statistical harness with fixed recorded command streams and recorded bootstrap seeds · [Human] tester protocol · [Doc] document review · [Sxx] spike-dependent, **provisional** · [Dep:X] needs GDD X (not written). A negative control that passes means the harness is broken. Owned and extended Roster criteria, not repeated: AC-PSR-41 (opponent counts never replicated), AC-PSR-42 (host view = client view), AC-PSR-44 (PlayerState allowlist), AC-PSR-45 (pawn replication parity).*

**A non-team client cannot determine player-ness from replicated data**
- **AC-IVP-01** [Unit] `StealthGame.Visibility.ReplicatedPropertyAudit`: GIVEN the checked-in allowlist, WHEN the audit enumerates by reflection the replicated properties and conditions of the Shared Pawn (with components and subobjects from instances spawned in a test world), PlayerState, PlayerController, GameState and every client-received actor class, including Blueprint lists, THEN any property absent from the allowlist or with a different condition fails the build. Negative control: a test-only class with one unlisted replicated property fails. (R11; Iris descriptor path [S11])
- **AC-IVP-02** [PIE-3][Net][S1][S10] `StealthGame.Visibility.RawBunchSchemaParity`: GIVEN ≥ 3 clients and ≥ 100 pawn lifetimes including spawn, cull and re-open, death, hand-over and rejoin, WHEN a non-owning client records each pawn's property handles and order, bit-length class, condition, NetGUID class, components, subobjects and RPC set, THEN `S = 0` (F1). Negative control: `COND_None` `APawn::PlayerState` gives `S ≥ 1`.
- **AC-IVP-03** [Stat][Net][S13] `StealthGame.Visibility.TransportClassifierAuc`: GIVEN ≥ 60 matches (n1 ≥ 300, n2 ≥ 3000 pawn-rounds, F2) replaying the same recorded command streams through the player and AI paths, split by match, WHEN the F2 classifier is scored with a 2,000-resample cluster bootstrap, THEN `AUC_hat ≤ 0.55` and `AUC_hat + 1.645·SE_boot ≤ 0.58`. Negative control: an injected player-only cadence offset fails.

**Query, classes, matrix and carriers (R1-R5)**
- **AC-IVP-04** [Unit] `StealthGame.Visibility.ViewerClassInvariants`: GIVEN fixture rosters covering every state, relation and phase, WHEN classes are computed, THEN the R2 order holds (`Ended` → `PostRound` first, unbound → NPC row, then Self, Teammate, Opponent); `TeammateInformant ≡ Teammate` and `OpponentInformant ≡ Opponent` for every fact about another subject; the class follows the server connection across rejoin and `RebindView`; client-supplied team, id or class is ignored.
- **AC-IVP-05** [Unit] `StealthGame.Visibility.QueryDefaultDeny`: GIVEN an unlisted fact, an unknown viewer, a client-side call and a non-game-thread call, WHEN `CanViewerKnow` runs, THEN it returns `false` and the counting fault-sink stub records exactly one report per call; a "—" (not applicable) cell returns `false` with no report. (R1, R3)
- **AC-IVP-06** [Int] `StealthGame.Visibility.SpawnedActorInstigatorOwner`: GIVEN every pawn spawn path (round start, NPC replacement, takeover, cull re-open) and every replicated spawned effect or projectile class, WHEN spawned in a test world, THEN `Instigator == Self` on every pawn and no replicated spawned actor carries a non-null replicated `Owner` or `Instigator`. (R5, L15)
- **AC-IVP-07** [Unit] `StealthGame.Visibility.MatrixPerFact`: GIVEN the R3 matrix fixture, WHEN every fact × {SA, SI, TM, TMI, OP, OPI, PR} is evaluated for a bound subject, THEN Y cells return `true`, N cells `false`, E cells `false` before `Ended` and gated by release class in `Ended`; a fixture/code disagreement fails.
- **AC-IVP-08** [Unit] `StealthGame.Visibility.NpcRowViewerIndependent`: GIVEN an unbound subject, WHEN every fact is queried from every class, THEN all answers are identical and equal exactly the `All` facts; for every fact the `Opponent` answer about a bound pawn equals the NPC-row answer. (R2 parity invariant)
- **AC-IVP-09** [Int][S1][S13][S14] `StealthGame.Visibility.EngineDefaultOverrides`: GIVEN a player pawn and an NPC pawn, WHEN effective replication settings are read, THEN `PlayerState`, `Controller`, `Owner` are owner-only and `PlayerState->GetPawn()` is null on non-owners; net frequency is fixed (SCL F9) with adaptive off; `RemoteViewPitch` comes from the `Look()` path; non-owners see `SimulatedProxy`; demo recording is disabled. (R5; PlayerState rows are AC-PSR-44)
- **AC-IVP-19** [PIE-3] `StealthGame.Visibility.TeamViewNeverToOpponents`: GIVEN a 2v2 with an instrumented replication log through an elimination, a takeover, a disconnect and a rejoin, WHEN each connection's received data is inspected, THEN no opponent connection (Active or Informant) ever receives a C-TeamView entry, `PawnRef`, `IsConcealed` flag or fallback position of the other team. (R4, R8, R9a)
- **AC-IVP-21** [Unit] `StealthGame.Visibility.PublicStructsGlobalOnly`: GIVEN `FRoundPublicState` and the TDC public snapshot, WHEN fields are enumerated, THEN none is keyed by team or participant and none varies with opponent counts or objective progress. (R4, L24, L26; runtime check AC-PSR-41)
- **AC-IVP-22** [PIE-3] `StealthGame.Visibility.FrozenPlayerArray`: GIVEN an `Active` round, WHEN a player disconnects, rejoins, is eliminated and becomes an informant, THEN on every client the count, order and names of the player array are unchanged, no PlayerState is created or destroyed, and no NPC pawn has a PlayerState. (R5, L3)
- **AC-IVP-35** [Unit] `StealthGame.Visibility.CarrierAudienceConsistency`: GIVEN the allowlist's audience column and the R4 table, WHEN cross-checked, THEN the pawn carries only `All` rows, every `Team` fact is in C-TeamView, no `Owner` fact is a gameplay field on the pawn, and no `ServerOnly` fact is a replicated property. (R4)

**Relevancy and concealment (R6, R7, F3, F4)**
- **AC-IVP-10** [PIE-3][S9] `StealthGame.Visibility.ConcealmentCullAudience`: GIVEN a concealed player pawn and a concealed NPC pawn, WHEN `Culled` is reached, THEN the player pawn has no actor on any non-ally client (Active or Informant) and stays relevant to its owner and every ally including informant allies; the NPC pawn has no actor on any client. (R7.1, R9a)
- **AC-IVP-11** [PIE-3][Net][S9][S12] `StealthGame.Visibility.ConcealmentCullTiming`: GIVEN `ConcealCullDelayMs = 250`, WHEN pawns commit concealment, including several in one server frame and one viewer whose acks are withheld, THEN each cull lands at the first global pawn-update grid point at or after `t0 + 250 ms` (R7.2; the same grid point for all commits of one server frame, ±1 server tick), withheld acks shift no cull by more than one server tick, and exit, break or kill during `CullPending` cancels the cull. (R7.2, F3)
- **AC-IVP-12** [PIE-2][Net][Dep:Concealment] `StealthGame.Visibility.ConcealmentLocalHideAtSwap`: GIVEN an ally and an opponent client watching a `BushEnter`, one with RTT above `HudLatencyWarnRttSec` (the IIF duplicate `LatencyWarnRttSec` was retired by user decision 2026-10-25), WHEN the swap notify fires, THEN on both clients body, shadow, reflection, Lumen contribution and footstep decals hide on that rendered frame (±1 frame at 60 Hz), triggered by `ActionPose` not the cull; the ally then shows the hatched marker. Retained screenshot sequence in `production/qa/evidence/` with lead sign-off. (R7.3)
- **AC-IVP-13** [PIE-2][S9][S15][Dep:Assassination] `StealthGame.Visibility.BlindSpotAttackOnCulled`: GIVEN a pawn culled for the attacker, WHEN the attacker sends `Attack(TargetSpot)` at its capsule, and separately `Attack(TargetPawn)` with its id and the same `WorldPoint`, THEN both resolve on the server against capsules and kill it, with identical reply codes and timing class and no prompt beforehand; no non-ally connection receives any bunch for the victim in the outcome slot; a victim culled for a non-concealment reason re-opens already falling at the death write (`ContactServerTime − StrikeNetLeadSec`, ±1 tick) with the death time set in that step, and a victim killed while concealed re-opens at the same death write carrying `FDeathState.bDiedConcealed` = true and is never rendered on any non-owner client (user decision 2026-10-25). (R7.4, R7.5)
- **AC-IVP-23** [Unit][S9] `StealthGame.Visibility.RelevancyTruthTable`: GIVEN every combination of {bound, unbound} × {PC, AI} × {Active, Informant, AwaitingPawn} × {concealed, not} × {owner, ally, non-ally}, WHEN the cull predicate is evaluated, THEN it equals `owner ∨ ally ∨ ¬concealed`; binding, controller kind and state never change it; a pawn based on a concealed pawn does not keep it relevant (L27); no pawn becomes dormant. (R6)
- **AC-IVP-24** [Unit][Doc][S10][Dep:Concealment] `StealthGame.Visibility.UncullLeadBound`: GIVEN Concealment's exit-transition asset, WHEN `T_exit_occluded` is read from its leaf-plane notify, THEN it is ≥ `T_uncull` (250 ms); in a [PIE-3][Net] run the non-ally client creates the re-opened actor before the outward leaf-plane instant in ≥ 99 % of 200 exits. (F4)
- **AC-IVP-28** [Unit] `StealthGame.Visibility.CullDelayKnobValidation`: GIVEN `L99 = 150` and `f_net = 15`, WHEN F3 is computed and the key is loaded, THEN F3 gives 250 ms, and TDC validation rejects values outside 100-1000, not a multiple of 50, or below `L99 + 1/f_net`. (F3)
- **AC-IVP-30** [Int][Doc] `StealthGame.Visibility.PlayerOnlyCullNeverShips`: GIVEN a build configuration, WHEN `bConcealServerCull = true`, THEN NPC bush use is enabled and the latest AC-IVP-25 run passed (at `HidePosteriorFactor` ≤ 1.5); otherwise the build uses client-side hide, the cull is invoked for no pawn, and the build is flagged internal-development-only (L29): the release checklist rejects it. (R7.6)
- **AC-IVP-32** [PIE-3] `StealthGame.Visibility.ReleaseToCrowdWhileConcealed`: GIVEN a concealed player who disconnects, WHEN `ReleaseToCrowd` runs, THEN the pawn stays culled for non-allies, becomes culled for former allies at the same boundary with their marker entry already gone, and never un-culls before release. (Edge Cases)

**Cross-system**
- **AC-IVP-14** [PIE-3][Dep:Assassination][Dep:Penalty] `StealthGame.Visibility.KillConfirmationParity`: GIVEN kills of an NPC, an Active opponent and an opponent informant, WHEN each resolves, THEN only the attacker's PlayerController receives `VictimKind`, the informant kill presents as `Npc`, no other connection receives a kind-dependent field, and the fall, removal and replacement sequence is identical for all three. (R3, R9b, L10)
- **AC-IVP-15** [PIE-3][Dep:Teammate Markers] `StealthGame.Visibility.TeammateMarkerConcealedAlly`: GIVEN an ally across the map, the same ally `Culled`, and the same ally with `PawnRef` forced unresolved, WHEN the viewer (Active, then Informant) reads markers, THEN the marker tracks the pawn in the first two cases (hatched when concealed) and the fallback position within one grid cell in the third; opponent clients create no marker widget. (R8, R9a)
- **AC-IVP-16** [Unit][Int] `StealthGame.Visibility.TeamFallbackFeedBudget`: GIVEN 0-4 unresolved entries at 2 Hz, WHEN the feed runs, THEN each entry is 13 B, measured bytes/s ≤ `N_u·13·h` (≤ 104 B/s), `Seq` advances by one per feed tick per entry, the feed stops within one period of the `PawnRef` resolving, and nothing is sent during travel. (F6)
- **AC-IVP-17** [Unit] `StealthGame.Visibility.PresentationIncludeCheck`: GIVEN every UI, HUD, marker, camera and audio-cue module, WHEN the build-time include and grep check runs, THEN none includes server query headers (Policy, Roster, Round, LSC, Penalty, Objectives) and none calls `IsPlayerControlled`, `IsBotControlled`, pawn `GetPlayerState`/`GetController`/`GetOwner` or `CanViewerKnow`. (R5, R10; extends AC-PSR-42)
- **AC-IVP-18** [Int][Doc] `StealthGame.Visibility.ListenHostHardening`: GIVEN a Shipping listen-server build, WHEN symbols, console commands and file writes are inspected during a round, THEN no debug command or overlay reads server data, demo recording cannot start, and the Event Log is written to disk only at `Ended` or `Reset`. The host memory caveat (L21) is recorded as accepted, not a pass condition. (R10)
- **AC-IVP-20** [PIE-3] `StealthGame.Visibility.PostRoundLatchRelease`: GIVEN visible and culled pawns when the round ends, WHEN Round's round-end step runs (R9; for a strike win the deciding strike's death write), THEN every viewer becomes `PostRound` in that server step before any `Ended` data is sent; in MVP only `FRoundResult` is newly released, and culled pawns stay culled through `Ended`. (R2, Round R19)
- **AC-IVP-25** [Stat][Dep:Crowd] `StealthGame.Visibility.HidePosteriorParity`: GIVEN nightly crowd runs with fixed seeds and player bush-entry rates from the playtest Concealment entry log, WHEN rates are compared, THEN `r_N / r_P ≥ 0.633` (`P_hide ≤ 3/22`, k·b at 50 NPCs), and NPC entries use the same Concealment call site as player entries. (F5, R7.6)
- **AC-IVP-31** [Unit][Dep:Penalty] `StealthGame.Visibility.TakeoverSkipsConcealed`: GIVEN candidate sets with concealed, transitioning and visible pawns, WHEN the informant takeover selects with a fixed seed, THEN it never picks a concealed or transitioning pawn, and returns none if no other candidate exists. (R9c)
- **AC-IVP-34** [Doc][Dep:Crowd] `StealthGame.Visibility.TellMatrixBushRow`: GIVEN Crowd's Baseline and Tell Matrix, THEN it has the row "bush enter/leave: red herring yes" with NPC dwell and exit distributions referenced to F5 at k = 1.5 (Crowd T5 cites k = 1.5 and r_N 0.19/min since its 2026-10-24 review; dwell and exit distributions are recorded, advisory, not a transport test, F5 note).

**Timing, human, statistical and process gates**
- **AC-IVP-26** [Stat][Net] `StealthGame.Visibility.TimingParity`: GIVEN ≥ 1236 samples per class for rejection reply delay, `ActionPose` delay U and kill-confirmation delay, WHEN player-target and NPC-target samples are compared, THEN `|d| ≤ 0.1`, variance ratio in [0.8, 1.25], `KS D ≤ 0.1`. Negative control: a 3 ms player-only shift in U fails. (F7, L9)
- **AC-IVP-27** [Human] `StealthGame.Visibility.HumanStripTest`: GIVEN 12 scenes × 8 testers from the strip harness, WHEN testers pick enemy players, THEN total hits stay below `h_fail` computed per F8 from the scenes' chance levels (16 when every scene is 1/9); results retained in `production/qa/evidence/`. (F8)
- **AC-IVP-29** [Doc][S11] `StealthGame.Visibility.IrisRerunGate`: GIVEN any build with Iris enabled, WHEN the release gate runs, THEN recorded Iris passes exist for S1, S2, S7, S9, S10 and AC-IVP-01, 02, 03; missing passes block the build.
- **AC-IVP-33** [Doc] `StealthGame.Visibility.LeakCatalogueReview`: GIVEN rows L1-L34, WHEN reviewed at each milestone gate, THEN every row has an owner and status; Mit rows cite a rule or AC, Spk rows a spike ID, Req rows a change request; Acc rows (L11, L14, L19-L21, L23; L29 dev builds only) appear in the release checklist as "not shippable beyond trusted playtests" (R10 definition: no public demo, festival build or open test while L14 or L21 is Accepted). (R12)

**Added at review 2026-10-24 (criteria the rules already cited, plus rulings A-D)**
- **AC-IVP-36** [Doc] `StealthGame.Visibility.EvidencePlaytestHost`: GIVEN any playtest whose data feeds F5, F8 or AC-IVP-25, WHEN its session record is checked before the data enters a gate, THEN the record names a non-competing host, or marks the host team's rows excluded and they are absent from the computed rates; a record with neither is rejected. (R10, OQ8)
- **AC-IVP-37** [Int] `StealthGame.Visibility.PossessionCanary` (runs per commit, does not wait for spikes): GIVEN a 2-client test world with one player pawn and one NPC pawn, WHEN a takeover, a rejoin and a `PawnRecovery` possession each occur, THEN the non-owner client's changelist for both pawns contains no `PlayerState`, `Controller` or `Owner` property at any frame, no bunch for the possessed pawn is sent at the possession frame, and `PlayerState->GetPawn()` is null on the non-owner. Negative control: `APawn::PlayerState` at `COND_None` fails. (R14, L1, L31)
- **AC-IVP-38** [Unit][Int] `StealthGame.Visibility.SpectatorStateBanned`: GIVEN game code and a test world, WHEN the grep check runs and a test calls `ChangeState(NAME_Spectating)`, `ChangeState(NAME_Inactive)`, `StartSpectatingOnly` and each banned spectator server RPC, THEN the grep finds no banned symbol, every call is a no-op with exactly one fault-sink report, the controller stays `NAME_Playing`, and `SpectatorClass` replicates as null. (R13, L30)
- **AC-IVP-39** [Int][Net][Dep:Assassination] `StealthGame.Visibility.StrikeIdOracleParity`: GIVEN `Attack(TargetPawn)` commands whose id is visible, concealed-and-culled, stale or random, each paired with `Attack(TargetSpot)` at the same `WorldPoint`, WHEN each resolves, THEN each pair has the same outcome class, reply code, rate-limit budget consumption and timing class (F7 bounds over ≥ 1236 samples per id class). (R7.4, L33)
- **AC-IVP-40** [Unit][Dep:Objectives] `StealthGame.Visibility.RivalSignalShape`: GIVEN the C-TeamView schema, WHEN it is checked against R3, THEN no opponent-progress field exists unless the R3 row for a revised signal exists (`RivalStageStep` as specified in Objectives OC-12 is absent); for a revised signal, with 1,000 pinned sub-seeds: the value never exceeds the preliminary-stage count and never changes on the final, every publication time lies on the global release quantum grid shared by both teams, and per-event delays are independent of completer, court and team (KS p > 0.01 against the declared distribution for each team). (R3, L34, OQ4)
- **AC-IVP-41** [Int][Net][Dep:Objectives] `StealthGame.Visibility.CourtIngressParity`: GIVEN `Interact` at an own live site, an own non-site court, a rival site and a decoy court, WHEN IIF ingress handles each, THEN acceptance, reply code, hold duration and timing class are identical (F7 bounds), budgets are charged to the sender only, only presentation and progress credit differ, and no fixture carries a replicated busy, occupied or in-use field. (R7.4a, L24, L33)
- **AC-IVP-42** [Unit] `StealthGame.Visibility.AllowlistContent`: GIVEN the allowlist file, WHEN it is parsed, THEN the Shared Pawn section contains every R11 minimum entry with its condition (`PlayerState`, `Controller`, `Owner` as `COND_OwnerOnly`), contains no replicated attachment, no `AbilitySystemComponent`, no second gameplay-state component and no Concealment state, and every row records both owners' sign-off. (R11)
- **AC-IVP-43** [Unit] `StealthGame.Visibility.HidePosteriorFactorCeiling`: GIVEN the test-harness config, WHEN `HidePosteriorFactor` is loaded, THEN 1.0-1.5 is accepted and any value outside it (including 2.0) fails the harness before AC-IVP-25 runs. (F5, ruling A)
- **AC-IVP-44** [Doc] `StealthGame.Visibility.ExternalBuildCriticalGate`: GIVEN any build leaving the development team (invited, evidence or public), WHEN the release checklist runs, THEN no CRITICAL row (L1, L2, L4, L13, L29, L30, L31) has status `Spk` or `Req`, `bConcealServerCull` is true, and AC-IVP-37 passed on that commit. (R12 ship gate, R7.6)
- **AC-IVP-46** [Unit][PIE-2] `StealthGame.Visibility.ScopeAdditions2026_10_25`: GIVEN the C-TeamView schema, the `FRoundPublicState` schema and a 2v2 PIE round with pings, a teammate disconnect inside the grace and an objective win, WHEN the R11 audit and the per-connection replicated-property logs are checked, THEN `UTeamPingViewComponent` and `bReconnecting` appear only on PlayerControllers and only in the sender's or disconnected player's teammates' logs (never an opponent's), and `FRoundResult.TeamSummary` is absent or zero in every `FRoundPublicState` received before `Ended` and present only from the round-end step.
- **AC-IVP-45** [PIE-3][Int] `StealthGame.Visibility.NetConfigRuntimeDifferential`: GIVEN an NPC pawn, a remote-client pawn, a host pawn and a just-taken-over pawn, WHEN the R11 net-config fields are read on class defaults and on the live instances, THEN every field is equal across all four. Negative control: a test-only override of `NetPriority` on one pawn fails. (R11, L5)

## Open Questions

| # | Question | Owner | Target |
|---|---|---|---|
| OQ1 | **Spikes added** (to Roster OQ0's S1-S8): **S9** cull latency and prompt channel close, legacy vs Iris; **S10** re-open behaviour (NetGUID reuse, new actor ABP initialisation from `ActionPose`, death fast-forward); **S11** Iris vs legacy decision (ADR-001; config currently selects legacy [V]); **S12** cull delay vs lag under `LeakNetProfile`; **S13** CMC cadence and correction parity for PC- vs AI-possessed pawns seen by a third client, adaptive frequency off; **S14** owner-only resets of `PlayerState`/`Controller` do not break possession or RPC routing; **S15** blind-attack probing as an occupancy oracle. Every rule tagged with these is **provisional pending spike**. | ue-replication-specialist / technical-director | Before the MVP-core checkpoint |
| OQ2 | Legacy or Iris (S11). Iris gives immediate per-connection culling without channel-close work; legacy is the specialists' earlier MVP recommendation (Session G). | technical-director (ADR-001) | ADR-001 |
| OQ3 | Can Crowd meet F5 at k = 1.5 with a plausible NPC bush behaviour (occupancy-blind choice, no dwell cap, `r_N` about 0.19/min per Concealment; cross-GDD conflict C7 resolved for the Concealment version)? Crowd's request to raise k to 2.0 is declined (review ruling A). If not met at the MVP-core checkpoint, escalate to creative-director and user (R7.6); no automatic fallback. ~~Change request to Crowd: T5 target 0.20 and change request 1 (k = 2.0) and OQ1 restated against `P_hide ≤ 1/6`; raise `r_N` instead.~~ **Design side resolved (reconciliation 2026-10-24):** Crowd T5 targets `P_hide ≤ 1/6` with r_N 0.19/min and withdrew k = 2.0; only the measurement remains open. | Crowd GDD author / creative-director | MVP-core checkpoint |
| OQ4 | **CLOSED (reconciliation 2026-10-24):** Objectives revised OC-12 to `RivalPrelimCount` in the ruling-B shape (preliminary count 0-2, never the final, fresh per-completion delay U(30, 165) s keyed by the round-global completion ordinal, one global 30 s release grid from `Active`), approved by the creative-director at the Objectives design review as R3 requires; the R3 Team row is added and L34 is mitigated (AC-IVP-40). History: Opponent objective progress signal. **Partly resolved (review 2026-10-24, ruling B):** Objectives OC-12 proposes `RivalStageStep` (delayed opponent preliminary count, Team carrier). Denied as specified; the Policy will add a Team row only for a revised signal that is stage-level (preliminaries only, **never the final step**), carries no member identity, site, position or sub-stage timing, draws a **fresh delay per publication event** (independent of completer, court and team, from the `StageSignal` sub-seed), and releases every publication only on **one global release quantum** shared by both teams (Objectives owns the quantum key). **Change request to Objectives:** revise OC-12, F4, AC-OC-09/10 and its IVP change request 3 accordingly; the IVP row is added at that revision, not before. | Objectives GDD / creative-director | Objectives revision |
| OQ5 | The human count is derivable from the player array (NPCs have no PlayerState). Accepted because the lobby shows `n / capacity`; confirm with Session that nothing else depends on hiding it. | Session GDD author | Session revision |
| OQ6 | L14 (every client holds every visible position) is the largest accepted surface. Anti-Cheat (Alpha) should evaluate line-of-sight-based relevancy pruning, which would change SCL R13. | Anti-Cheat / technical-director | Alpha |
| OQ7 | **CLOSED — declined for MVP by the user 2026-10-25** (Round OQ4): no name labels at `Ended`; the look-to-participant link waits for the VS Post-Round Reveal. The Policy still supports it (`PostRoundEligible`). | user | closed 2026-10-25 |
| OQ8 | **CLOSED — deferred to Alpha by the user 2026-10-25:** no headless evidence server in MVP; the non-competing-host rule (R10, AC-IVP-36) stands for every evidence playtest until the Alpha dedicated server. Original question (review ruling C): should evidence playtests (any session whose data feeds F5, F8 or AC-IVP-25) run on a headless (dedicated-style) server so no competing player is the host? The locked listen-server fact stands until the user decides. Interim rule now (R10): evidence playtests seat a non-competing developer as the trusted host, or exclude the host team's data as contaminated. | user / technical-director | closed 2026-10-25 (Alpha) |
