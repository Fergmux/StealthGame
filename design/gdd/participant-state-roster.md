# Participant State & Roster

> **Status**: Reviewed - NEEDS REVISION 2026-10-24; reconciled 2026-10-24; ADR-synced 2026-10-24; user decisions applied 2026-10-25 (C-TeamView owner tally/`T_due` fields, teammate `RetireAtServerTime`; `bPawnBound` declined; scope: teammate `bReconnecting` (Team) for the MVP reconnect indicator, the Informant Pings C-TeamView part)
> **Design Review**: NEEDS REVISION 2026-10-24 (17 revisions applied)
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS -> REVISED 2026-10-24 (three additions applied to R6, Edge Cases and Acceptance Criteria: an opponent-count rule, a host-presentation rule and a no-tell takeover rule; table and binding model unchanged)
> **Author**: design-system pipeline (autonomous) + systems-designer, ue-replication-specialist, gameplay-programmer, creative-director, qa-lead
> **Last Updated**: 2026-10-24
> **Last Verified**: 2026-10-24
> **Implements Pillar**: P1 Hide in Plain Sight; P3 One Strike, Lasting Consequence

## Summary

The Roster is the server's single record of every human player: a match-stable id, their team, and whether they are `Active`, an `Informant` or `RoundEnded`. It is the only system allowed to change that state, through `RequestTransition` and a fixed legal-transition table. It also privately knows which pawn each player drives, so it alone answers "player, informant or NPC?". It exists so that elimination is decided once and stays decided, and so that no data it replicates lets an opponent's client tell a player from an NPC (Pillar 1).

> **Quick reference** — Layer: `Foundation` · Priority: `MVP` · Key deps: `None upstream (TeamSize injected from Tuning Data & Config); dependents: Session & Travel, Round & Match Framework, Information Visibility Policy, Shared Character & Locomotion, Assassination, NPC-Kill Penalty & Informants, Concealment, Objectives, Teammate Markers, HUD`

## Overview

Participant State & Roster is the server-authoritative record of every human player in a match: who they are (a match-stable `ParticipantId` and an identity key used for rejoin), which team they are on, and where they are in the round lifecycle (`Active`, `Informant`, `RoundEnded`). It is the only system allowed to change lifecycle state, through one API, `RequestTransition(Participant, NewState, Reason)`, which checks a fixed legal-transition table and emits one ordered state-change event per accepted change. It also holds the **server-only binding** between a participant and the Shared Pawn they currently drive, and answers the question every other system must never answer for itself: *is this pawn a player, an informant, or an NPC?* (`ResolveVictimKind`, `IsPawnBound`). The Roster owns **data only**: strike counts, defence cooldowns, concealment and objective progress live in the components of the features that own them. Players never see the Roster; they experience what it guarantees. Elimination is immediate and permanent for the round, a reconnect gives you back exactly what you were, and nothing the Roster replicates lets an opponent's client link a pawn to a player (Pillar 1). Without it, every feature would keep its own copy of "who is alive and on which team". Those copies drift apart under simultaneous events, seamless travel and reconnects. Each one is also a separate chance to replicate the player/NPC distinction to the wrong client.

*Framing choices (autonomous, logged)*: both the data layer and the player effect (Core category); no ADR exists yet, so ADR-001's crowd-identity direction is cited as the constraint and implementation choices are marked "-> ADR"; the fantasy is indirect.

## Player Fantasy

**Known only to your own.** Somewhere a server knows your name, your team and whether you still carry a blade. Nobody in the crowd does: not the scoreboard, not the lobby, not the body you walk in. The player list an opponent can open shows names and nothing else. It shows no status and no count of who is still in the race, and it never changes while the round runs. It does not show teams. An opponent can still work out which names are not on their own team, but the list never groups or marks them, and a name never tells them which body in the crowd is yours. Only your teammates know you, and they keep knowing you after you are cut down: the moment the blade lands you are an informant, still theirs, still walking the city. Drop out and come back, and you return as whatever you are at that moment. Come back inside the grace period and you are still competing. Come back later and you have been removed for the round, so you are an informant, and you are never turned away for having been gone. A reconnect does not wash informant status away, and it does not hand your place to anyone else.

What you are is decided once, by the server, and it stays decided until the next round. When two blades reach the same body at the same instant, only one of them counts, and the second never turns into a strike against its owner. When the round ends, it ends for everyone at the same moment.

Anchor moments, each mapped to an acceptance criterion below: (1) an opponent opens the player list mid-round and learns nothing about teams or status; (2) assassinated means informant in the same server step, and teammates see the change while opponents see nothing; (3) disconnect and rejoin restores the same team and the participant's current state, whether that is `Active` within grace or `Informant` after it, and a returning participant is never refused; (4) a double assassination produces exactly one state change and no wrongful strike.

*Pillars served*: P1 Hide in Plain Sight ("until someone commits, an enemy player cannot be told apart from an NPC by ... network presentation"); P3 One Strike, Lasting Consequence ("elimination is permanent for the round ... but elimination never removes you from the match"). *Language rule for any player-facing text derived from this system*: never "dead", "lives", "revive", "spectator", "ghost", "tracked" or "registered"; "eliminated" only as "eliminated for the round"; internal names (`RoundEnded`, `ParticipantId`, `Retired`) never reach the player. *(creative-director consulted: framing A "Known Only to Your Own" + one finality line from framing B, recommended option.)*

## Detailed Design

### Core Rules

*Specialists consulted: systems-designer (rules, table, invariants), ue-replication-specialist (audit against installed UE 5.8.4 source; tags [V] verified in source, [S] spike, [NS] not sourceable), gameplay-programmer (feasibility, prototype migration).*

**R1. One record per human, one owner.** The Roster holds one `ParticipantRecord` per human player in the match. No other system stores a copy of team or lifecycle state. Other systems hold a `ParticipantId` and ask. The Roster stores no feature state. It holds no strike count, no defence state, no concealment flag and no objective progress.

**R2. Authority.** Every mutating call is server-only and runs on the game thread. Clients never call the Roster. A client-originated request, such as "I attack", reaches the owning feature as an RPC. That feature derives the participant from the sending connection's controller. **No RPC parameter ever carries a `ParticipantId` or an identity key.** A mutating or binding query called on a client returns `Rejected_NotAuthority` / `NotTargetable(Stale)` and reports to the injected fault sink, which raises an `ensure` in development builds. It never returns the permissive answer `Npc`.

**R3. The record and its fields.** "Match" fields survive round changes, seamless travel and disconnect or rejoin, and are discarded only by `BeginMatch`/`EndMatch`. "Round" fields are rewritten by the per-round batch calls (R7).

| Field | Scope | Replication audience | Written by | Notes |
|---|---|---|---|---|
| `ParticipantId` | Match | **ServerOnly** | Roster at registration | Monotonic per match and never reused. Server-internal: its order would leak join order and team parity, so it never leaves the server. |
| `PublicHandle` | Match | All (opaque) | Roster at registration | A random 32-bit value, unique within the match and never reused, drawn from a server-only random source. It carries no order. It is the only participant reference that any client receives: C-Public and C-TeamView use it. |
| `IdentityKey` | Match | ServerOnly | Session & Travel (from the join request) | Used for rejoin. **In MVP it is client-asserted and unauthenticated.** It is Session rule 6's 128-bit install key, sent as a join option, or `-IdentityKey=` / the PIE instance index in development. The Roster never receives it from a gameplay RPC (R2). Possession of the key is the only proof of identity, so a client that obtains another player's key can reclaim that record. A server-issued rejoin secret is a **hard gate before the first playtest outside the team**; Session's provisional `RejoinToken` (Session rule 6) is that secret (Session change request 1); an authenticated platform `UniqueNetId` replaces the key when an online subsystem exists (-> ADR). |
| `DisplayName` | Match | All | Session & Travel | The engine player name. The only human-readable public field. Fixed for the match (Session rule 10). |
| `Team` | Match | Owner, Team | `AssignTeams` (Session request) | `0`, `1` or `None`. Never replicated to opponents (R6). |
| `Connection` | Match | ServerOnly (the owner's client knows its own) | **Session & Travel only**, via `NotifyConnectionChanged` | A *projection* of Session's `ConnectionState`, which is the single source of truth (mapping table in R11): `Connected`, `Disconnected` or `Dormant`. Also holds ServerOnly `DisconnectedAt` (server time in integer ms) and `ConnectionGeneration`, which increases by 1 on every change into `Disconnected`. The Roster never writes it itself. It is not replicated to teammates, which matches Session rule 24 ("not replicated to non-owners"). |
| `AppearanceHistory` | Match | ServerOnly | Round, via `RecordAppearance(Id, AppearanceId)` after `AllocateAppearances` | The last `AppearanceHistoryDepth` AppearanceIds dealt to this participant. **Log-only.** Character Pool & Appearance does not use it for allocation (CPA change request 6: a no-repeat rule would leak), so the default depth is 0. `RecordAppearance` drops the oldest entry when the depth is exceeded, and it does nothing at depth 0. |
| `InRound` | Round | ServerOnly | `ResetAll` | True for every record that was not `Dormant` at `ResetAll`. Only `InRound` records are counted by F1–F5. A `Dormant` record stays `RoundEnded` for the whole round. |
| `State` | Round | Owner, Team | `RequestTransition` / batch calls only | `Active`, `Informant` or `RoundEnded`. Opponents never receive it. The post-round reveal reads the Round Event Log, not the Roster. |
| `LastTransitionReason`, `LastEventSeq` | Round | ServerOnly | Roster | First-reason-wins record (R8). |
| `AssignedAppearanceId` | Round | ServerOnly | Round (allocator output) | Held from allocation until the participant's pawn is spawned. The pawn's own `AppearanceId` property is Character Pool's. |
| Pawn binding (live pawn + tombstones) | Round | ServerOnly. A teammate's pawn reference is Team. | `BindPawn` / `RetireBinding` / `ReleaseToCrowd` / `ClearBinding` | See R9 and R10. |
| `MatchId` (roster-wide) | Match | ServerOnly | `BeginMatch` (random 64-bit) | Combined with `RoundEpoch` into the epoch token `(MatchId, RoundEpoch)`. Because `RoundEpoch` restarts at 0 each match, a timer left over from a previous match would otherwise match the new match's epoch. |
| `RoundEpoch` (roster-wide counter) | Match | Owner | Roster (`ResetAll` increments it) | uint32. Lets Round and features discard stale timers and events. `RoundIndex` is Round's own counter, which Session puts in the travel URL. `ResetAll(RoundIndex)` records the pair, so `IsRoundScopeReset(RoundIndex)` can answer Session. The two counters advance together, once per round, but only `RoundEpoch` is used for staleness checks. |
| `EventSeq` (roster-wide counter) | Match | ServerOnly | Roster | uint64. Increases by exactly +1 for every emitted Roster event of **any** type: state, batch, registration, reclaim, connection and team events alike (R12, I6). No-ops and rejections consume nothing. |

**R4. Registration and identity (called by Session & Travel).**
- `BeginMatch(TeamSize)` clears the store, draws a new `MatchId`, and resets `ParticipantId`, `EventSeq` and `RoundEpoch` to 0. `TeamSize` is **injected** by the caller. Round & Match Framework owns the `TeamSize` key (Tuning Data & Config's canonical table, latch class `Round`). The Roster reads no config itself. `SetTeamSize(TeamSize)` may be called again while teams are unlocked, with a value ≥ the largest current team's non-Dormant record count; otherwise it returns `Rejected_TeamSizeBelowRoster`. While teams are locked the value is frozen. Values outside 1–5 return `Rejected_InvalidTeamSize`. Tests use 1 (1v0 and 1v1) through a test preset, even though Tuning Data & Config's soft range is 2–5.
- `RegisterParticipant(IdentityKey, DisplayName)` returns one of:
  - `Registered(Id)`: a new record with `State = RoundEnded`, `Team = None` and `Connection = Connected`.
  - `Reclaimed(Id)`: the key matches an existing record (R11). That includes a `Dormant` record returning in the Lobby, which returns as itself: same id, same handle, same match-scoped fields. Its `Team` is kept if that team has room under F5, and otherwise becomes `None`.
  - `Rejected_TeamsLocked`: teams are locked and the key is new, or matches a `Dormant` record. A Dormant record is not part of the running round, and Session rejects it as `InProgress` anyway.
  - `Rejected_RosterFull`: there are already `2 × TeamSize` **non-Dormant** records. Dormant records do not hold a slot, matching Session's `Occupied` (Session F1).
- **Session API mapping** (one operation, two names; the Roster names are canonical and Session adopts them):

  | Session & Travel name | Roster call |
  |---|---|
  | `FindByIdentityKey` + `CreateParticipant` | `RegisterParticipant` (lookup first, create only if the key is unknown) |
  | `BindConnection(PeerKey, PlayerState)` | `RegisterParticipant` on a fresh login, or `RebindView(Id, PlayerState)` on a seamless-travel arrival. Either one writes the view fields `PublicHandle` and `DisplayName` into the PlayerState from the record. Seamless travel **replaces** both the PlayerController and the PlayerState in UE 5.8.4 (ADR-0005 E5), so `RebindView` always writes a new PlayerState and re-pushes C-TeamView to the new controller; Session keys the connection by a peer key (the network connection, or a reserved `Host` key), never by controller or PlayerState (Session rule 16). `AStealthPlayerState::CopyProperties` carries nothing authoritative. |
  | `ResetRoundScope(RoundIndex)` | `ResetAll(RoundIndex)` (R7) |
  | round-scope readiness query (Session rule 11.2) | `IsRoundScopeReset(RoundIndex)` |
  | `OnTravelStarting` | `InvalidateAllBindings()` (R9) |
- `RemoveParticipant(Id)` is legal only while teams are unlocked. It is used only for an explicit Lobby removal (a host kick) and at `EndMatch`; otherwise it returns `Rejected_TeamsLocked`. The id is retired, not reused. **A Lobby disconnect does not remove the record**: Session sets `Connection = Dormant` (Session rule 13), and the record keeps its id and match-scoped fields.
- Check order for registration: `Rejected_NotAuthority`, then the reclaim match, then `Rejected_TeamsLocked`, then `Rejected_RosterFull`.
- **No engine inactive-PlayerState reuse.** The prototype `AStealthGameMode` derives from `AGameModeBase` [V], and `AddInactivePlayer` / `FindInactivePlayer` exist only in `AGameMode` [V]. The address-only duplicate check that would destroy a second same-machine client's inactive PlayerState is in `AGameMode::AddInactivePlayer` [V]. The game mode **must stay on `AGameModeBase`, or override both functions**. Reclaim is decided by the Roster key alone. A disconnected participant's PlayerState is kept active and in the player array until the next travel (Session rule 22, R6).

**R5. Team assignment.** Session & Travel requests teams. Round & Match Framework owns size rules, balance rules and the minimum to start. The Roster enforces only capacity and the lock.
- `AssignTeams(Batch[(Id, Team)])` is atomic. It is validated against the *final* state (Formula F5), so a full 5v5 swap succeeds where a stepwise swap would fail. It returns `Applied`, `Rejected_TeamsLocked`, `Rejected_UnknownParticipant`, `Rejected_InvalidBatch` (duplicate id, or a team not in {0, 1}) or `Rejected_TeamFull`, checked in that order. On rejection nothing changes.
- `SetTeamsLocked(bool)` is called by Round. `SetTeamsLocked(true)` returns `Rejected_UnassignedParticipants` while any **non-Dormant** record has `Team = None`, which enforces I2. Round must assign or remove those participants first. A Dormant record's `Team` may be `None`, because it never enters the round (`InRound = false`). **Fixed order, once per round:** `SetTeamsLocked(true)` (a no-op when the lock is already on; Round unlocks teams only on arrival in `Lobby`, so every round started from `Lobby` locks first), then `ResetAll(RoundIndex)`, then Session's `RequestRoundTravel`. `ResetAll` while unlocked returns `Rejected_TeamsUnlocked`, so a `Team = None` record can never become `Active`. The lock lasts until Round returns to `Lobby`. While locked, `RosterCount(T)` cannot change.
- **Fewer than 10 participants is legal.** Capacity is a maximum, so 2v2, 4v3 and 1v0 are all representable. Round decides whether such a round may start; the default is at least one per team except behind a dev flag. The Roster contains no "full team" assumption anywhere.

**R6. Replication carriers. ADR-001 owns the mechanism; the Information Visibility Policy ratifies the audiences.** UE defaults that drive this design [V]:
- Every `APlayerState` is always relevant. `PlayerArray` is not replicated: each client builds it from the PlayerStates it receives.
- `APlayerState` sends `PlayerNamePrivate`, `Score`, `StartTime`, `bIsSpectator`, `bOnlySpectator` and `bFromPreviousLevel` to all clients, and `PlayerId`, `UniqueId`, `bIsABot` and `bIsInactive` in the initial bunch.
- `APawn::PlayerState` and `APawn::Controller` replicate with no condition, and `AActor::Owner` is replicated.

The carriers are therefore:
- **C-Public (All), on `APlayerState`: a closed allowlist.** It carries exactly `PublicHandle` and `DisplayName`. Team, state, connection, appearance and pawn data never go here. Every engine-default replicated PlayerState property is either **disabled**, or **held constant for the whole round** and reset only at travel:
  - `PlayerId`;
  - `Score`, `StartTime`;
  - `bIsSpectator`, `bOnlySpectator`;
  - `bFromPreviousLevel`;
  - `bIsABot`;
  - `bIsInactive`;
  - `UniqueId`;
  - `CompressedPing` (`COND_SkipOwner` [V]; ping is connection data that Session rule 24 keeps from non-owners).

  Which treatment each property gets is decided in the ADR. Disabling `UniqueId` or `PlayerId` has side effects on voice, session registration and `GameState` lookups, so it is a trade-off, not a free trim. The Information Visibility Policy owns the allowlist and its reflection test (AC-PSR-44). The mechanism is a replicated-props override. Engine members such as `APawn::PlayerState` are private, so the `FName` overloads are needed [S2: the condition-reset approach is verified in headers; its behaviour under Iris is unverified].
- **The in-round name list is frozen (adopts Session rules 22 and 24).** A disconnected or expired participant's PlayerState stays active and in the player array until the next travel. Its allowlisted fields do not change. A rejoiner's new connection is re-associated with the **retained** PlayerState (`RebindView`), so no second entry appears. Mid-round, no client sees any entry added, removed or changed.
- **C-TeamView (Owner + Team), on each `APlayerController`:** the controller is relevant only to its owner on both the legacy driver and Iris [V]. It carries:
  - the owner's own `{PublicHandle, Team, State, RoundEpoch, WrongTargetTally, LastResultSeq, T_due}` (user decision 2026-10-25). The three added fields are **Owner-only** and Penalty-sourced (Penalty PEN-R3, PEN-R15; IVP R2 Owner rows): `WrongTargetTally` (uint8, saturating) and `LastResultSeq` (the `ClientCommandSeq` of the last `FStrikeResult` that changed it) are written **only** on reclaim, rejoin and `OnRosterReset`, never per strike (Penalty CR-19), so a C-TeamView update can never beat a result plate to the screen; `T_due` (server time the informant's respawn takeover becomes due; 0 = none) is written when Penalty schedules or reschedules a respawn and cleared at the binding, and drives HUD's live "Returning in {s}" countdown (HUD F7). Penalty supplies the values through the Roster server face (`SetOwnerTally`, `SetOwnerRespawnDue`); the Roster's push step stays the only writer of the replicated part;
  - one entry per teammate: `{PublicHandle, State, PawnRef, IsConcealed, RetireAtServerTime, bReconnecting}`. **`bReconnecting`** (bool, **Team** audience; added by user decision 2026-10-25 for the MVP teammate-reconnecting indicator, Session OQ7 closed) is true exactly while that teammate's `Connection` projection is `Disconnected`, the Round phase is `Active`, its `State` is `Active` or `Informant`, and server time is before `DisconnectedAt + DisconnectGraceSec` (Round F4); it is false otherwise, so it clears at reclaim, at grace expiry, at `EndRoundAll` and at `OnRosterReset`. The Roster's push step derives it from the `Connection` projection Session already writes (`NotifyConnectionChanged`); no new writer and no new server input exist. It carries one bit and no time, reason, ping or address (Session rule 24 as amended; IVP R3 `TeammateReconnecting` row). Consumers: HUD-R10a feed lines, Teammate Markers TM-R11 label suffix. `RetireAtServerTime` (user decision 2026-10-25, Teammate Markers CR-13; **Team** audience) is set only in the update that retires the teammate's binding or releases its pawn: the Attack's slot time for a release, the kill's `ContactServerTime` (Assassination ASN-R19) for a kill or reversal; 0 otherwise. It lets the teammate marker leave exactly as the blow lands (TM-R6). A `bPawnBound` flag per entry (Teammate Markers CR-12) was **declined for MVP** by the user 2026-10-25 (the feed inference of TM-R4a stands). `IsConcealed` is the carrier the Information Visibility Policy's fact table already assigns to that fact (audience Owner, Team; Concealment CON-R2 writes it); listed here at the 2026-10-24 reconciliation (Teammate Markers CR-1), not a new field of this pass.
  - Feature team-audience sub-structs ride the same controller view under IVP R4 (MVP: Objectives `FObjectiveTeamView`, OC-5; Informant Pings `FTeamPingView`, PNG-R6, added by user decision 2026-10-25); the Roster owns none of their contents, so the two lists above are the Roster's part of C-TeamView, not its closed schema (cross-review C-W6 2026-10-24).

  Teammate connection state is **not** included (Session rule 24), with one exception: the one-bit Team-audience `bReconnecting` above (user decision 2026-10-25). Update policy: C-TeamView is pushed on change only, at most once per server frame, with all changes in that frame coalesced into one update. Its net priority equals the controller's default. `PawnRef` entries are cleared at `InvalidateAllBindings` (travel start) and re-pushed after the controller swap. **Presentation timing of strike-caused role changes** (Assassination ASN-R23): an Owner or Team role change caused by a strike may arrive in C-TeamView before the strike's contact frame on that client; it is presented through the client-local `OnRoleChangePresented` dispatch at the struck pawn's `DeathServerTime`, never on replication arrival.

  "Team audience" is implemented as *the server copies team data into each teammate's owner-only view*. This needs no custom relevancy filter and no Iris dependency, and it replaces the earlier per-team replica-actor idea, which `IsNetRelevantFor` may not support under Iris [S]. The view is re-pushed after every controller swap (seamless travel) and on reclaim.
- **C-Server (ServerOnly):** the store itself (-> ADR: a GameInstance-level subsystem so it outlives GameMode, GameState and PlayerState on both seamless and hard travel; dormant on clients). It is never a replicated property.
- **Pawn-level link (requirement on Shared Character & Locomotion):** `APawn::PlayerState`, `APawn::Controller` and `AActor::Owner` must be **null or identical in kind on every non-owning connection, for player and NPC pawns alike**. The candidate is an owner-only condition on all three [S1, see OQ0]. S1 must check the raw received property bunch, not only the resolved pointers. A player pawn's Owner is an owner-only PlayerController, so a non-owner receives an unmappable NetGUID for it, where an NPC pawn's value is null. That difference is a tell even when both resolve to null. The reverse holds too (IVP change request 11): an AI pawn's `AActor::Owner` is its server-only AIController, which also exports an unmappable NetGUID, so null parity is not automatic for NPC pawns either; S1 checks both kinds. Until the spike passes, the prototype's known leak, `IsNpc() = (PlayerState == null)` evaluated on clients, remains, and **no client code may derive player-ness from these pointers.** `PlayerState->GetPawn()` is null on clients for non-owned players, and client code must not rely on it.
- **Pawn replication parity (requirement on Shared Character & Locomotion, binding).** Player and NPC pawns are one class with one replication configuration, and binding, transition or possession never changes it. This covers:
  - `NetUpdateFrequency`, adaptive update frequency, `NetPriority`, `NetCullDistanceSquared`;
  - dormancy, which no pawn class may use differently for NPCs;
  - replicated components and subobjects;
  - multicast RPCs (the same cue fires for a player action and the identical AI action);
  - `RemoteViewPitch`. For AI-driven pawns it must be fed from the same look path (`Look(Rotation)`) with a crowd-plausible distribution, so a pitch value or a pitch change is never player-only.

  The values live in config, not code. The AC is AC-PSR-45 [S1].
- **Accepted limits, stated honestly:**
  - Opponent team membership is derivable as "every public name not in my team view". It is therefore not secret, and the public list must not present it as a grouping.
  - On a listen server the host process holds every ServerOnly field in memory, including guests' IdentityKeys, until `EndMatch`. ServerOnly protects against clients, not against the host player, and this lasts until Dedicated Server (Alpha). The Player Fantasy therefore holds for the nine guests and not fully for the host. `EndMatch` runs on every session exit path, so a former host does not keep the store.
  - A disconnected player's pawn changes controller at the hand-over (R11). The pawn's visible behaviour at that moment is the remaining correlation surface. The takeover rule limits it, but it cannot be removed entirely. The name list itself no longer changes.
  - Same-tick ordering uses Round's total order (server tick, then arrival sequence id). Low-latency players, and the host at zero latency, therefore win exact ties. This is accepted for determinism and replay.
- **Opponent counts never leave the server before `Ended` (binding rule, Pillar 4 and Pillar 1).** This covers opponent `ActiveCount`, `InformantCount`, `AwaitingPawn` and any value derived from them, such as "1 enemy left", a remaining-rival icon, or an elimination-progress bar. None of these may be replicated to, computed on, or presented by any client before Round phase `Ended`. The requirement binds Round & Match Framework, HUD, Event Feedback & Cues and the Information Visibility Policy. A visible opponent count would turn the endgame into a hunt, and its changes would act as a remote elimination detector. Only the owner's *own-team* counts, derived from C-TeamView, may be shown.
- **Presentation reads only C-TeamView, also on the host (binding rule).** All presentation logic on every machine reads its viewer's C-TeamView and nothing else. That includes the listen-server host's local player view (HUD, teammate markers, audio, UI prompts, camera). Presentation never calls Roster server queries (`IsPawnBound`, `ResolveVictimKind`, `GetRelation`, `GetState` of an opponent) and never reads pawn pointers. On the host the authority guard of R2 does not protect anything, because the host is the authority. Only this rule keeps the host player's screen identical to a client's. It is enforced structurally: the server Roster query API lives in a header that no presentation module (UI, HUD, markers, camera, audio cues) may include, and a build-time include check fails the build if one does (AC-PSR-42).

**R7. Round batch calls (called by Round & Match Framework).**
- `ResetAll(RoundIndex)`, Reason `Reset` (Session calls the same operation `ResetRoundScope`):
  - every **non-Dormant** record becomes `Active`, whatever its prior state, and gets `InRound = true`;
  - every `Dormant` record stays (or becomes) `RoundEnded` with `InRound = false`;
  - all bindings and tombstones are cleared, along with `LastTransitionReason` and `AssignedAppearanceId`;
  - `RoundEpoch` increments, and `RoundIndex` is recorded so that `IsRoundScopeReset(RoundIndex)` returns true;
  - one `OnRosterReset(RoundEpoch, EventSeq)` is emitted.

  `Team` and `Connection` are kept. Teams must be locked, otherwise the result is `Rejected_TeamsUnlocked` (R5). It is called once per round, **before** Session's `RequestRoundTravel` (Session rule 11.2 requires the reset first), inside Round's `StartRound` while in `Lobby`. After an aborted start or a rollback to `Lobby`, Round may call it again with the **same** `RoundIndex`: the call is applied in full again (a new `RoundEpoch`, a new `OnRosterReset`), and `IsRoundScopeReset(RoundIndex)` stays true. That is therefore before any of the round's pawns exist. Feature components (strikes, defence, concealment) reset on `OnRosterReset`, not on per-participant events. Between `ResetAll` and Round phase `Active`, the `Active` state means only "will compete this round": I4 and F4 apply only in phase `Active`.
- `EndRoundAll()`, Reason `RoundEnd`: every `Active` or `Informant` record becomes `RoundEnded`, in one step, and one `OnRosterRoundEnded(RoundEpoch, EventSeq)` is emitted. Round calls it in its R9 **round-end step** (the latch step, or the deciding strike's death write), in the same server step as the replicated `Ended` publication and with a forced net update on every written record; clients treat a `RoundEnded` that arrives before `Phase = Ended` as no change (Teammate Markers TM-R5, HUD; cross-review S-B2 2026-10-24). A second call is a no-op with no event.
- `RequestTransition(Id, RoundEnded, RoundEnd)` and `RequestTransition(Id, Active, Reset)` on a single participant return `Rejected_BatchOnlyReason`. These two reasons exist only in their batch forms, so a round can never end or reset for some participants and not others.

**R8. Single-participant transitions.** `RequestTransition(Participant, NewState, Reason [, ExpectedEpoch] [, ExpectedConnectionGeneration])` applies the legal-transition table below. **R8's result precedence is authoritative**: the table illustrates it, and wherever the two seem to differ, the precedence decides. `ExpectedEpoch` is the token `(MatchId, RoundEpoch)`.
- On `Applied`, the state, `LastTransitionReason` and `LastEventSeq` are written immediately. In the same call, an `Active -> Informant` change retires the participant's live binding (R9), **except for Reason `StrikeLimit`**, which releases the live binding to the crowd instead (the pawn becomes `Unbound` and its card stays `NpcLive`; the `Rejected_NotDisconnected` rule of `ReleaseToCrowd` does not apply to this path). The exiled player's surviving body is from then on an NPC (Penalty PEN-R4.2, change request CR-3).
- **First reason wins:** a later request for the same target state returns `NoOp_AlreadyInState`. It neither overwrites the reason nor emits an event.
- If `ExpectedEpoch` is supplied and differs from `(MatchId, RoundEpoch)`, the result is `Rejected_StaleEpoch`. This guards timers, such as a disconnect grace timer, that fire after a reset or in a later match.
- A `Disconnect` request **must** carry `ExpectedConnectionGeneration`, the participant's `ConnectionGeneration` when the grace timer started. If it differs, the result is `Rejected_StaleConnection`. This stops an old grace timer from firing against a newer disconnect: drop at 100 s, rejoin at 129.9 s, drop again at 129.95 s, and the first timer at 130 s is stale.
- **Result precedence:** the first check that fails decides the result.
  1. `Rejected_NotAuthority`
  2. `Rejected_WrongThread`, when the call is off the game thread (I7; an `ensure` is also reported to the fault sink).
  3. `Rejected_UnknownParticipant`
  4. `Rejected_StaleEpoch`
  5. `Rejected_BatchOnlyReason`, when the Reason is `RoundEnd` or `Reset`.
  6. `Rejected_IllegalReason`, when the Reason is not an entering reason for `NewState`. The entering reasons are: Informant ← {Assassinated, ReversedAgainst, StrikeLimit, Disconnect}; RoundEnded ← {RoundEnd}; Active ← {Reset}. Examples: `(Active, Assassinated)`, `(RoundEnded, StrikeLimit)` and `(Active, StrikeLimit)`. That makes Active→Active with any non-batch reason `Rejected_IllegalReason`, not `IllegalTransition`.
  7. `NoOp_AlreadyInState`, when the current state equals `NewState`.
  8. `Rejected_IllegalTransition`, when the current state is not a legal source, e.g. `RoundEnded -> Informant`.
  9. `Rejected_NotDisconnected`, when the Reason is `Disconnect` and `Connection = Connected`.
  10. `Rejected_StaleConnection`, when the Reason is `Disconnect` and `ExpectedConnectionGeneration` is missing or differs.
  11. `Applied`

  For example, an Informant who receives a `Disconnect` request gets `NoOp_AlreadyInState`, whether connected or not. (F6's `AcceptDisconnect` being false does not mean a rejection in that case.)
- The Roster does not order requests. Round & Match Framework's resolution queue (key: server tick, then arrival sequence id) does. The Roster applies calls in the order received.

**R9. Pawn binding (server-only).** The binding status of any pawn is one of `Unbound` (no record: an NPC), `BoundLive(Id)` or `Retired(Id)` (a tombstone).
- `BindPawn(Id, Pawn)` is legal only when all of these hold:
  - the participant is `Active` or `Informant`;
  - the participant has no live pawn;
  - the pawn is `Unbound`.

  Otherwise it returns `Rejected_AlreadyBound`, `Rejected_PawnInUse` or `Rejected_WrongState`. It also takes an optional `ExpectedEpoch`: a respawn timer from an earlier round gets `Rejected_StaleEpoch`. After `ResetAll`, a formerly awaiting informant is `Active` with no pawn, and without the epoch guard a stale respawn would bind. **BindPawn precedence:** `Rejected_NotAuthority`, then `Rejected_WrongThread`, then `Rejected_UnknownParticipant`, then `Rejected_StaleEpoch`, then `Rejected_WrongState`, then `Rejected_AlreadyBound`, then `Rejected_PawnInUse` (a pawn that is null, destroyed or pending kill also gives `Rejected_PawnInUse`), then `Applied`.
- **A player pawn is bound before it can be seen or targeted.** There are exactly two paths, and both run in one server call stack:
  - **Fresh spawn** (round start only, from Round's interleaved mixed spawn). Deferred spawn, then `BindPawn`, then finish spawning, then possess. If the bind fails, the pawn is destroyed. This happens in **one named spawn site**, which overrides the engine's default-pawn path (`SpawnDefaultPawnAtTransform` / `RestartPlayer` use a plain spawn). No other code may spawn a player pawn. Replication starts on the next net tick, after `FinishSpawning`, so the bind is never network-visible.
  - **Crowd adoption** (informant respawn and a rejoining or returning informant; decided rule C1). The locked rule "a killed informant respawns at their team's spawn" is honoured as "near the team spawn, out of enemy sight": the pawn is the one returned by the Level Socket Contract's `PickRespawnPawn` (R5b), an unseen `Unbound` NPC inside the team's respawn region. NPC-Kill Penalty & Informants calls Shared Character & Locomotion's `TakeOverCrowdPawn` (R2a), which performs the eligibility check and calls `BindPawn` **on Penalty's behalf**, atomically, inside Penalty's queue slot; Penalty never calls `BindPawn` itself for a takeover, and no card move (`TakeNpcCard`) is committed unless the takeover returned `Applied` (a `NotEligible` result changes nothing). The possession is committed in the same slot and executed by the hand-over pass at the start of the next server frame; in between the pawn is `BoundLive` but still crowd-possessed, issuing nothing, which every Roster query treats as ordinary (ADR-0010 §3.2, §4.3). No new actor and no new NetGUID is created. A fresh pawn appearing mid-round would be a visual and a network tell (P1/P2), so a mid-round fresh spawn of a player pawn is forbidden. Penalty (with Crowd's eligibility rules) owns how the pawn is chosen; Character Pool's card effect is `TakeNpcCard` (`NpcLive -> BoundLive`; the pawn keeps its `AppearanceId`).

  These two paths rule out a "live but unbound player pawn" window in which an attack would read as an NPC kill.
- **Disconnect hand-over hook (engine override, spike S8).** By default `APlayerController::PawnLeavingGame` destroys the pawn when the controller is destroyed [V]. The game's PlayerController overrides it (implemented by Shared Character & Locomotion R2b; spike S8 stays the Roster's). The pawn is unpossessed and detached from the leaving controller, then possessed by a crowd AI controller in the same server frame, and the binding stays `BoundLive`. The pawn is never destroyed because of a connection event (Session rule 22).
- `InvalidateAllBindings()` is called by Session at travel start (`OnTravelStarting`), because travel destroys every pawn. It clears every live binding and tombstone, but no lifecycle state. `PawnRef` entries in C-TeamView are cleared too. It emits no event.
- `RetireBinding(Pawn, RetireAtServerTime)` turns `BoundLive` into `Retired` and clears the participant's live pawn; the caller passes the time the teammates' C-TeamView entry carries as `RetireAtServerTime` (R6; the kill's `ContactServerTime`). It is used when the participant is eliminated and when an informant is killed. The tombstone lasts until the pawn's `EndPlay` (`ClearBinding`) or `ResetAll`, and it is keyed by object identity plus a generation number so a pooled or recycled pawn cannot inherit it.
- `ReleaseToCrowd(Pawn)` erases a live or retired binding outright, so the pawn becomes `Unbound`, in the same call that Crowd & NPC Behaviour adopts it. It is used for an abandoned disconnected pawn (Edge Cases) and, inside `RequestTransition(…, StrikeLimit)`, for an exiled participant's surviving pawn (R8). From then on the pawn *is* an NPC, and killing it is an NPC kill. For a disconnect expiry, Round calls `RequestTransition(…, Disconnect)` and `ReleaseToCrowd` **in the same server call stack**, so a rejoin can never land in between and leave an untargetable retired body.
- `ClearBinding(Pawn, EndPlayReason)` is called at pawn `EndPlay`. If the binding was still live, the participant's live pawn becomes null, and for an `Active` participant the Roster raises **`OnBindingLost(Id, PawnDestroyed)`** (a server-side event, R12) so Round can run its own-card respawn (Round R17.9). The missing retire is reported (an `ensure` routed through the injected fault sink) **only** when `EndPlayReason == Destroyed` during Round phase `Active`. Teardown paths (travel, PIE end, level unload) clear the binding silently.
- **Results on the wrong input** (none of these changes anything or raises an `ensure`, except where stated):
  - `RetireBinding` on an Unbound pawn: `NoOp_NotBound`. On a Retired pawn: `NoOp_AlreadyRetired`.
  - `ClearBinding` on an Unbound pawn: `NoOp_NotBound`.
  - `ReleaseToCrowd` when the bound participant is `Connected` and not `RoundEnded`: `Rejected_NotDisconnected`. A connected player's pawn can never be handed to the crowd.
- **`ResetAll` precondition (Round's guarantee):** every player pawn of the previous round has been destroyed or released to the crowd before `ResetAll`. After `ResetAll`, any surviving former player pawn would resolve as `Npc`. That is correct only if it really is a crowd pawn.

**R10. Queries (server-only, O(1), allocation-free on the hot path).**
- `ResolveVictimKind(Pawn)` returns `Player` (BoundLive + Active), `Informant` (BoundLive + Informant) or `Npc` (a valid live pawn that is Unbound). Otherwise it returns `NotTargetable(AlreadyResolved | Stale | RoundOver)`:
  - `AlreadyResolved`: the pawn is Retired.
  - `Stale`: the pawn is destroyed or pending kill.
  - `RoundOver`: the bound participant is RoundEnded.

  Evaluation order: a null, destroyed or pending-kill pawn gives `Stale`. Then a Retired pawn gives `AlreadyResolved`, including one whose participant is now RoundEnded. Then a BoundLive pawn whose participant is RoundEnded gives `RoundOver`. Then the three live kinds. This is the "NPC or player?" answer Assassination uses to fill `OnPawnKilled.VictimKind`. **`NotTargetable` never maps to `Npc`**, so a second attack on an already-resolved body can never cost a strike. `ResolveVictimKind` is ServerOnly, and its result never reaches a client directly.
- **Informant-kill invariant (P1, binding on Assassination and NPC-Kill Penalty & Informants).** Everything the attacker can perceive after killing an `Informant`-kind victim must be **identical to an NPC kill**: animation, audio, kill confirmation, strike feedback and HUD. That includes whether a visible strike is shown. Otherwise the attacker's own feedback becomes a probe for "that pawn was an informant". Informant kills do not count as strikes. The true strike count is server-only and never replicated; the attacker's HUD shows only a tally of `WrongTarget` plates, which increments identically for both victim kinds. Three residuals are permitted (Penalty PEN-R14): (1) removal timing: an Active attacker whose tally has reached `NpcKillStrikeLimit` can infer that at least one past `WrongTarget` victim was an informant or a graced released body, never which one; (2) a witness who strikes a body it saw strike may receive `WrongTarget` and infer that its striker was exiled; (3) an awaiting informant may see its takeover take longer than its countdown. Any other perceivable difference requires a pillar-safe alternative approved by the creative director. (Approved by the creative director, Penalty CD-GDD-ALIGN and design review rulings R1 and R7; the visible tally was confirmed by the user 2026-10-25, Penalty OQ-NKP-1 closed.)
- `IsPawnBound(Pawn)` is true only for BoundLive. It is the Visibility Policy's `IsPlayer` fact.
- `GetParticipant(Pawn)` and `GetPawn(Id)` map between pawns and participants.
- `GetState(Id)`, `GetTeam(Id)` and `GetRoundEpoch()` read single fields.
- `GetRelation(PawnA, PawnB)` returns `Self`, `Teammate`, `Opponent` or `NotBothPlayers`. A Retired pawn still resolves through its tombstone's participant, so a dying body keeps its relation until `EndPlay`. Assassination uses it for friendly-target rejection and Concealment uses it for per-viewer hiding.
- `CountActive(T)`, `CountInformant(T)`, `CountRoster(T)` and `IsTeamEliminated(T)` are counts over `InRound` records (Formulas). On a client they return 0 (and `false`) and report to the fault sink. They never return real values there. `IsTeamEliminated` returns false outside Round phase `Active`, so after `EndRoundAll` it never reports both teams eliminated.
- `GetLegalityClass(Pawn)` returns `Active`, `Informant` or `RoundEnded`, the action-legality class the Input & Interaction Framework uses. A BoundLive or Retired pawn returns its participant's state. An **Unbound pawn returns `Active`**, so the answer never exposes the binding.
- `GetUnboundActiveParticipants()` is Round's pre-`Active` check (R9's invariant I4).
- **Callers must not cache answers across frames.** A held `ParticipantId` is stable for the match; a held answer about a pawn is not.

**R11. Disconnect and rejoin.**
- **Connection projection.** Session & Travel owns `ConnectionState`. The Roster's `Connection` is a projection that only Session writes, through `NotifyConnectionChanged(Id, Projection, ServerTimeMs)`:

  | Session `ConnectionState` | Roster `Connection` |
  |---|---|
  | `Connecting` (new key) | no record yet |
  | `Connected`, `Unresponsive`, `Traveling` | `Connected` |
  | `Disconnected`, `Rejoining`, `Expired` | `Disconnected` |
  | `Dormant` | `Dormant` |
  | `Rejected` | unchanged |

  Session's `Expired` is a connection state only. It does **not** map to the Roster's `Retired`, which is a pawn-binding tombstone (Session change request to drop that mapping).
- **Disconnect.** Session's notify changes only `Connection`, `DisconnectedAt` and `ConnectionGeneration`. The lifecycle state does not change. Whether and when a disconnected participant leaves active play is **Round's policy**.
  - **Hand-over.** The pawn is handed to a crowd AI controller through the R9 hook, so it neither freezes nor vanishes (Pillar 1). The takeover blends with no dead stop, and a participant-bound AI issues only `Move` and `Look` commands (Round R17). Provisionally this happens at Session's `Unresponsive` (soft timeout, about 3 s), as Session recommends to Round, and the pawn is handed back on `OnParticipantResponsive`. It always happens no later than `Disconnected`. During the hand-over the pawn stays `BoundLive`.
  - **Grace.** The grace clock starts at `Disconnected` (`DisconnectedAt`). Round's default is `DisconnectGraceSec = 30` (hard 3-120; below 10 for dev presets only), applied as **`G_eff = min(DisconnectGraceSec, RejoinWindowSec - 30)`** (Round F4; Session `RejoinWindowSec`, default 120). A participant is therefore always moved to `Informant` before their Session slot can expire.
  - **Expiry.** Round requests `Active -> Informant` with Reason `Disconnect`, the `ExpectedEpoch` and the `ExpectedConnectionGeneration` captured when the timer started. In the same call stack it calls `ReleaseToCrowd`. The Roster's rules: `Disconnect` is accepted only when `Connection == Disconnected` (otherwise `Rejected_NotDisconnected`) and only with the matching generation (otherwise `Rejected_StaleConnection`).
- **Rejoin.** `RegisterParticipant` with a matching key returns `Reclaimed(Id)`: the same `ParticipantId`, `PublicHandle`, `Team`, `State` and `AppearanceHistory`, `Connection = Connected`, and the retained PlayerState re-associated (`RebindView`) with C-TeamView re-pushed. A reclaimed `Active` participant within grace repossesses its still-bound pawn. A reclaimed `Informant` with no live pawn is "awaiting pawn" and gets an **adopted crowd pawn** (R9) from NPC-Kill Penalty & Informants. **Reconnecting never changes state**: an informant stays an informant. A participant returning after grace, including from Session `Expired`, is an `Informant` and **is never refused** for having been gone. Round's `ShouldAdmitExpired` must return true for every `InRound` record (Round change request).
- **Feature state across a reclaim.** Strikes, defence cooldown and concealment are not reset by a reclaim. Each owning feature keys its state by `ParticipantId`, not by connection or PlayerState, and resets only on `OnRosterReset` (NPC-Kill Penalty & Informants owns strike carry-over).
- **Duplicate connection for one key** (a half-open old socket): the newest connection wins. Session kicks the old one, and the record is reclaimed once. Before the old connection closes, Session drops its connection-to-participant mapping, so an in-flight RPC from the old controller can never resolve to the record (supersede race).
- **Host loss (listen server).** The store lives in the host process. The match ends for every client, and the Roster is not recovered: there is no host migration in MVP. Client behaviour belongs to Session & Travel.

**R12. Events (server-side delegates).** All events are queued and dispatched FIFO after the **outermost** mutating Roster call returns. State is mutated immediately, so a request made from inside a listener sees live state and gets an accurate result; its own events queue behind the current ones. The events:
- `OnParticipantStateChanged{Id, Team, Old, New, Reason, EventSeq, RoundEpoch, ActiveCountAfter[2], InformantCountAfter[2]}`
- `OnRosterRoundEnded{RoundEpoch, EventSeq}`
- `OnRosterReset{RoundEpoch, EventSeq}`
- `OnParticipantRegistered`, `OnParticipantReclaimed`, `OnParticipantRemoved`, `OnConnectionChanged` and `OnTeamsChanged`
- `OnBindingLost{Id, Cause = PawnDestroyed, EventSeq}` (R9 `ClearBinding` of a live binding of an `Active` participant without a resolved kill; Round change request 12c)

Every event above, of every type, consumes exactly one `EventSeq` (I6). `InvalidateAllBindings` and `RebindView` emit no event. **Rebind notification (ADR sync 2026-10-24):** after `RebindView` has written the views, the store raises the server-only wiring notification `OnViewRebound(ParticipantId)`; it is not a lifecycle event, consumes no `EventSeq` and never reaches a client. Feature parts that ride C-TeamView (Objectives, ADR-0020 §6) subscribe to it and re-write their owner or team view parts inline; it does not depend on world wiring (ADR-0005).

**Payload counts are authoritative for ordering; live reads may already be newer.** Listeners act on applied events only, never on a request's return value, and must be idempotent per `EventSeq`.

### States and Transitions

| State | Meaning | Counts for elimination | May hold a live pawn | Typical holders |
|---|---|---|---|---|
| `Active` | Competing this round | Yes | Yes (required during Round phase `Active`) | Every non-Dormant record after `ResetAll` |
| `Informant` | Eliminated or removed for the rest of the round; walks, observes, can be killed and respawns as an informant | No | Yes, or "awaiting pawn" (no live pawn) | Assassinated, reversed, struck-out or disconnect-expired participants |
| `RoundEnded` | Not in a round: round over, or newly registered and not yet reset in | No | No (`BindPawn` rejected) | Everyone after `EndRoundAll`; new registrations; `Dormant` records for the whole round |

**Legal-transition table.** The table illustrates R8, and R8's result precedence is authoritative. Checks 1–6 of R8 (authority, thread, id, epoch, batch-only reason, illegal reason) are applied **before** the table's "not listed" rule. So a reason that is not an entering reason for the target is `Rejected_IllegalReason`, and only the combinations that remain unlisted are `Rejected_IllegalTransition`.

| From | To | Reason | API | Result | Side effects |
|---|---|---|---|---|---|
| Active | Informant | `Assassinated` (victim of a successful assassination) | RequestTransition | Applied | Live binding retired; `OnParticipantStateChanged` |
| Active | Informant | `ReversedAgainst` (attacker of a reversed assassination) | RequestTransition | Applied | same |
| Active | Informant | `StrikeLimit` (strike limit reached; `NpcKillStrikeLimit` default 3) | RequestTransition | Applied | same |
| Active | Informant | `Disconnect` | RequestTransition | Applied only if `Connection == Disconnected` and `ExpectedConnectionGeneration` matches, else `Rejected_NotDisconnected` / `Rejected_StaleConnection` | same (Round calls `ReleaseToCrowd` in the same call stack) |
| Informant | Informant | any of the four above | RequestTransition | `NoOp_AlreadyInState` (first reason kept, no event) | none |
| Active, Informant | RoundEnded | `RoundEnd` | `EndRoundAll` only | Applied (batch) | `OnRosterRoundEnded` |
| RoundEnded | RoundEnded | `RoundEnd` | `EndRoundAll` | NoOp | none |
| RoundEnded | Informant | any entering reason | RequestTransition | `Rejected_IllegalTransition` (late queued events after the round ended) | none |
| Active, Informant, RoundEnded (non-Dormant) | Active | `Reset` | `ResetAll` only, teams locked | Applied (batch) | bindings and tombstones cleared; `RoundEpoch`+1; `OnRosterReset`. Dormant records go to or stay `RoundEnded` |
| any | Active or RoundEnded | `Reset` or `RoundEnd` via RequestTransition | — | `Rejected_BatchOnlyReason` | none |
| Active, Informant, RoundEnded | Active | any other reason | RequestTransition | `Rejected_IllegalReason` (no revival: the only way back to Active is `ResetAll`) | none |
| any | Informant | `Reset` / `RoundEnd` | RequestTransition | `Rejected_BatchOnlyReason` | none |
| any | any | unknown Id | — | `Rejected_UnknownParticipant` | none |

Where several rows could apply, R8's result precedence decides.

**Not a transition:** killing an informant (`VictimKind = Informant`). The state stays `Informant`; NPC-Kill Penalty & Informants calls `RetireBinding` and later binds an adopted crowd pawn (R9). No `EventSeq` is consumed. What the attacker perceives follows the informant-kill invariant (R10).

**Invariants** (asserted in debug builds and covered by tests):
- **I1.** Record accounting: the number of records equals registrations minus removals since `BeginMatch`. Every record has exactly one `State`. For each team, the `InRound` records satisfy `Active + Informant + RoundEnded = RosterCount`, and `RosterCount` does not change while teams are locked. A `Dormant` record is never `Active` or `Informant`.
- **I2.** While teams are locked, no non-Dormant participant has `Team = None`, and no `Team = None` record is ever `Active` or `Informant`.
- **I3.** A participant has at most one live pawn; a pawn is bound to at most one participant.
- **I4.** During Round phase `Active`, every `Active` participant who was connected at `ActivePhaseStart` has a live pawn. Exceptions:
  - a participant inside the R9 spawn or adoption window, which is bounded to one server call stack;
  - a participant whose live pawn was cleared by the `ClearBinding` safety path. Round must restore a live pawn for them within one server frame, by crowd adoption, or report a fault.

  A participant already disconnected at `ActivePhaseStart` has no pawn. Their grace runs from `ActivePhaseStart` (F6), and if they rejoin within grace they receive an adopted crowd pawn.
- **I5.** A tombstone never becomes live or unbound, except via `ClearBinding`, `ReleaseToCrowd`, `InvalidateAllBindings` or `ResetAll`.
- **I6.** `EventSeq` increases by exactly 1 per emitted Roster event of any type (R3, R12).
- **I7.** No mutation off the game thread.

### Interactions with Other Systems

| System | Direction | Data in / out | Owner of the interface |
|---|---|---|---|
| Session & Travel (5) | Session -> Roster | `BeginMatch`, `RegisterParticipant` (Session's `FindByIdentityKey`/`CreateParticipant`/`BindConnection`, mapped in R4), `RebindView`, `RemoveParticipant` (Lobby kick only), `NotifyConnectionChanged` (sole writer of the `Connection` projection, R11), `InvalidateAllBindings` at `OnTravelStarting`, `IsRoundScopeReset`, `AssignTeams` requests; Roster returns Registered/Reclaimed; Session re-pushes C-TeamView after controller swaps | Roster defines the calls; Session decides when |
| Round & Match Framework (6) | `SetTeamsLocked`, `ResetAll(RoundIndex)` (before `RequestRoundTravel`), `EndRoundAll`, `RequestTransition(…, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)`, `SetTeamSize`, `GetUnboundActiveParticipants`, `RecordAppearance`; Round consumes `OnParticipantStateChanged` payloads to evaluate the elimination trigger (F4) and ends the round | Roster owns state; Round owns phase, order, win, disconnect policy |
| Information Visibility Policy (7) | Policy reads Roster | `IsPawnBound`, `GetRelation`, `GetState`, `GetTeam`; ratifies R6 audiences and owns the leak tests | Policy owns audiences; Roster supplies facts |
| Shared Character & Locomotion (9) | Pawn calls Roster | Deferred spawn then `BindPawn`; `ClearBinding` at `EndPlay`; implements the pawn-level link suppression of R6 | Roster defines binding; pawn implements hooks |
| Crowd & NPC Behaviour (11) | Round/Roster -> Crowd | `ReleaseToCrowd` hand-off of an abandoned pawn; drives a disconnected player's pawn during grace (provisional) | Crowd owns adoption |
| Concealment (12) | Concealment reads | `GetRelation(viewer, subject)` for per-viewer hiding; reacts to `OnRosterReset` | Concealment |
| Assassination & Defence Reversal (13) | Assassination reads and requests | `ResolveVictimKind`, `GetRelation`, `GetState(attacker)` at resolution time; `RequestTransition(victim, Informant, Assassinated)` / `(attacker, Informant, ReversedAgainst)` after Round's queue resolves the event | Assassination owns attack validity; Roster owns the answer |
| NPC-Kill Penalty & Informants (14) | Penalty requests | `RequestTransition(…, Informant, StrikeLimit)` with release on exile (R8); for informant kills, `RetireBinding`, then crowd adoption of the `PickRespawnPawn` result through SCL `TakeOverCrowdPawn`, which binds (`BindPawn`) on Penalty's behalf (R9); reads "awaiting pawn" | Penalty owns strikes and respawn |
| Objectives Chain (15) | Objectives reads | `GetState` (only `Active` may interact or complete), `GetTeam` | Objectives |
| Teammate Markers (17) | Markers read C-TeamView on client | teammate `{PublicHandle, State, PawnRef}`; marker fill state from `State` (no teammate connection state, R6) | Markers |
| HUD (18) | HUD reads C-TeamView on client | own `State`, `Team`; teammate states | HUD |
| Character Pool & Appearance (4) | Pool and Round write | `AssignedAppearanceId` spawn hand-off copy; `RecordAppearance` (log-only history that the allocator does not read) | Pool owns the deal |
| Input & Interaction Framework (3) | IIF reads | `GetLegalityClass(Pawn)` (Unbound pawns return `Active`); owner-audience own-state change for the input context swap | Roster |
| Level Socket Contract (8) | LSC reads (server) | `GetTeam`, for start-placement zone mapping and the informant respawn set | LSC |
| Progression & Cosmetics (30, Full Vision) | future | `IdentityKey` as the account hook | — |

## Formulas

*systems-designer consulted. The Roster defines counting and validity rules, not balance curves; the only tuned value it consumes is `TeamSize`.*

**F1. ActiveCount.** The ActiveCount formula is defined as:

`ActiveCount(T) = | { p in Roster : p.InRound AND p.Team = T AND p.State = Active } |`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| T | enum | {0, 1} | constant (two teams, locked rule) | Team being counted |
| p.Team | enum | {0, 1, None} | calculated (Roster record) | Participant's team |
| p.State | enum | {Active, Informant, RoundEnded} | calculated (Roster record) | Lifecycle state |
| TeamSize | int | 1–5 (default 5) | data file (`TeamSize`, owned by Round & Match Framework in Tuning Data & Config, latch `Round`; injected) | Upper bound of the output |

**Output Range:** 0 to `TeamSize`. Connection state is ignored: a disconnected participant counts until Round transitions them. **Example:** team 0 = {A Active, B Active, C Informant, D Active (Disconnected, in grace), E Informant}: `ActiveCount(0) = 3`.

**F2. InformantCount and AwaitingPawn.** The InformantCount formula is defined as:

`InformantCount(T) = | { p : p.InRound AND p.Team = T AND p.State = Informant } |`; `AwaitingPawn(T) = | { p : p.InRound AND p.Team = T AND p.State = Informant AND LivePawn(p) = null } |`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| T | enum | {0, 1} | constant | Team |
| LivePawn(p) | pawn ref or null | — | calculated (binding table) | Participant's BoundLive pawn |

**Output Range:** 0 to `TeamSize`; `AwaitingPawn(T) <= InformantCount(T)`. **Example:** C was just killed as an informant (binding retired, respawn pending), E walks: `InformantCount(0) = 2`, `AwaitingPawn(0) = 1`.

**F3. RosterCount and conservation.** The RosterCount formula is defined as:

`RosterCount(T) = | { p : p.InRound AND p.Team = T } |`; invariant `ActiveCount(T) + InformantCount(T) + RoundEndedCount(T) = RosterCount(T)`. Outside a round (before the first `ResetAll`, `InRound` false for all), `RosterCount(T)` counts non-Dormant records with `Team = T` instead.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| RoundEndedCount(T) | int | 0–TeamSize | calculated | Records of team T in `RoundEnded` |
| RosterCount(T) | int | 0–TeamSize | calculated | All `InRound` records on team T, connected or not. Dormant records are excluded |

**Output Range:** 0 to `TeamSize`; constant while teams are locked (R5). **Example:** 2v2 test: `RosterCount(0) = RosterCount(1) = 2`; after one elimination on team 1: Active 1 + Informant 1 + RoundEnded 0 = 2.

**F4. Elimination predicate and trigger.** The elimination formulas are defined as:

`IsTeamEliminated(T) = (RosterCount(T) >= 1) AND (ActiveCount(T) = 0)`

`IsTeamEliminated(T)` is defined only in Round phase `Active`; in every other phase it returns false.

`EliminationTrigger(e) = e.Applied AND e.Old = Active AND e.New = Informant AND e.Reason in {Assassinated, ReversedAgainst, StrikeLimit} AND Phase = Active AND NOT EndedLatched AND e.ActiveCountAfter[e.Team] = 0 AND RosterCount(e.Team) >= 1 AND e.ActiveCountAfter[1 - e.Team] >= 1` → `EliminationWin(1 - e.Team)`

The trigger is **edge-triggered**: it is evaluated once per applied event, in `EventSeq` order, and never re-evaluated from live state.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| e | event | — | calculated (`OnParticipantStateChanged` payload) | One applied single-participant transition |
| e.ActiveCountAfter[T] | int | 0–TeamSize | calculated (payload snapshot) | F1 evaluated immediately after this transition |
| e.Reason | enum | the six Reasons | calculated (payload) | Only decision-caused reasons can trigger a win. `Disconnect` never does |
| Phase | enum | Round phases | Round & Match Framework | Must be `Active` |
| EndedLatched | bool | — | Round & Match Framework | True once any win has been declared |

**Output Range:** boolean.
- An empty team (`RosterCount = 0`) is never eliminated, and a team with no Active participant can never be awarded the win (`ActiveCountAfter[1 - e.Team] >= 1`). A 1v0 dev round therefore ends only by objective.
- **A disconnect expiry never yields `EliminationWin`, and neither does a host kick, which reaches the Roster as a disconnect.** A network drop is not a decision (P3), and a host must not be able to win by kicking the last opposing Active player.
- Every case in which a team reaches `ActiveCount = 0` without the trigger firing is a **Round adjudication**. That covers the last Active player's disconnect expiring, both teams reaching 0 in the same tick, and an all-disconnected round. Round resolves each one with its own outcome and reason code (forfeit or void, Round change request); the Roster only reports the `Reason`.

**Why no draw is possible:** each single transition changes exactly one participant on one team, so one event can drive at most one team to 0. If two independent transitions in the same tick each drive their own team to 0, the second event sees `ActiveCountAfter[1 - e.Team] = 0`, so it awards nothing, and the first event has already latched a valid win. Round evaluates the trigger per event in `EventSeq` order and latches the first win. Batch events (`OnRosterRoundEnded`, `OnRosterReset`) are not single transitions and are never evaluated, even though they drive both counts to 0. **Example:** team 0 has one Active (A); A's assassination of a prepared defender is reversed, producing `e{A, team 0, Active→Informant, ReversedAgainst, ActiveCountAfter = [0, 3]}`; the trigger is true and the result is `EliminationWin(1)`.

**F5. Team capacity and batch validity.** The AssignTeams validity formula is defined as:

`Free(T) = TeamSize - RosterCount(T)`; `Valid(B) = NOT Locked AND |B| >= 1 AND for all T: |{p non-Dormant : Team'(p) = T}| <= TeamSize AND for all (id, t) in B: id is known, t in {0, 1}, and id is unique in B`. An empty batch returns `Rejected_InvalidBatch`.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| B | list of (Id, Team) | 1–10 entries | Session & Travel request | Requested assignments |
| Team'(p) | enum | {0, 1, None} | calculated | Team after applying B to the current state |
| Locked | bool | — | Round (`SetTeamsLocked`) | Team lock |
| TeamSize | int | 1–5 | data file (`TeamSize`) | Capacity per team |

**Output Range:** boolean, all-or-nothing. `MaxParticipants = 2 × TeamSize` (10 by default). **Example:** teams are 5/5 and a batch swaps A (team 0) with F (team 1): the final state is 5/5, so `Valid`. The same two moves submitted one at a time would fail the first move (`Rejected_TeamFull`), which is why the batch exists.

**F6. Disconnect transition guard (Roster part) and grace reference (Round part).** The guard formula is defined as:

`AcceptDisconnect(p) = p.Connection = Disconnected AND p.State = Active AND ExpectedEpoch = (MatchId, RoundEpoch) AND ExpectedConnectionGeneration = p.ConnectionGeneration`; Round's provisional timing, in integer milliseconds: `GraceExpiryMs(p) = max(p.DisconnectedAtMs, ActivePhaseStartMs) + G_eff_Ms`, with `G_eff = min(DisconnectGraceSec, RejoinWindowSec - 30)` (Round F4); the timer fires when `NowMs >= GraceExpiryMs`.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| p.DisconnectedAtMs | int64 (ms, server time) | ≥ 0 | calculated (Session notify) | When the connection dropped (entry to Session `Disconnected`) |
| ActivePhaseStartMs | int64 (ms) | ≥ 0 | Round & Match Framework | Start of this round's `Active` phase |
| DisconnectGraceSec | float (s), applied as ms | 3–120 (default 30; below 10 for dev presets only, warned); applied as `G_eff` | data file (Round-owned key) | Grace before removal. 0 is not allowed because it would forfeit on the first frame of `Active` |
| RejoinWindowSec | float (s) | 60–300 (default 120) | data file (Session-owned) | Session slot reservation; upper bound of the grace |
| ExpectedEpoch | (uint64, uint32) | — | caller (Round) | `(MatchId, RoundEpoch)` captured when the timer was started |
| ExpectedConnectionGeneration | uint32 | ≥ 1 | caller (Round) | `ConnectionGeneration` captured when the timer was started |

**Output Range:** boolean (Roster accepts or rejects); expiry time in milliseconds.

**Example:** a participant drops at t = 100 000 ms during `Active` (generation 1), so the grace expires at 130 000 ms.
- A rejoin at 129 900 ms keeps them `Active`.
- If they never return, Round requests the transition at 130 000 ms with the epoch and generation it captured.
- Had a `ResetAll` happened in between, the request would return `Rejected_StaleEpoch`.
- Had they rejoined at 129 900 ms and dropped again at 129 950 ms (generation 2), the first timer's request would return `Rejected_StaleConnection`, and only the new timer, expiring at 159 950 ms, could apply.

A rejoin and an expiry in the same tick resolve rejoin-first, because Session processes receives before window expiry (Session rule ordering).

**F7. Carrier size (bandwidth sanity check).** The TeamView size formula is defined as:

`TeamViewBytes ≈ OwnBytes + (TeamSize - 1) × EntryBytes`, sent only on change

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| OwnBytes | int | ~22 | constant (estimate: handle 4 B, team + state 1 B, epoch 4 B, tally 1 B, `LastResultSeq` ~4 B, `T_due` 8 B; user decision 2026-10-25) | Owner block |
| EntryBytes | int | ~17 | constant (estimate: handle 4 B, state + concealed 1 B, pawn NetGUID ~4 B, `RetireAtServerTime` 8 B) | Per-teammate entry |

**Output Range:** about 90 bytes per full update at `TeamSize` 5 (22 + 4 × 17; was about 45 before the 2026-10-25 fields), and changes happen a handful of times per round. That is negligible against the 50 KB/s per-client budget. These are estimates only; to be measured with `stat net` (spike S6).

## Edge Cases

*systems-designer consulted (edge-case sweep over Sections C and D). Ordering of anything that happens in the same server tick is decided by Round & Match Framework's resolution queue (tick, then arrival sequence id); the Roster applies calls in that order.*

**Simultaneous events**
- **If a player's 3rd NPC kill and their own assassination resolve in the same tick**:
  - *NPC kill first:* the strike is counted, `RequestTransition(A, Informant, StrikeLimit)` is Applied and A's pawn is released to the crowd (R8). The assassin's attack then resolves `Npc`: a `WrongTarget` plate and tally +1, but no strike, because the released body is inside `StrikeOutReleaseGraceSec` (Penalty PEN-R5).
  - *Assassination first:* A becomes `Informant` (`Assassinated`), and A's queued NPC attack is voided at resolution because the attacker is no longer `Active` (Assassination checks attacker state at resolution, not submission). The NPC lives.
- **If two attackers assassinate the same player pawn in the same tick**: the first is Applied. For the second, `ResolveVictimKind` returns `NotTargetable(AlreadyResolved)` (tombstone): no transition, no strike, and no kill confirmation beyond what Assassination's own rules give the second attacker.
- **If the last Active players of both teams attack each other in the same tick**: the first in queue order resolves and its event's `ActiveCountAfter` drives one team to 0. Round latches that `EliminationWin`, and the second attack resolves after the latch with no effect. No draw. The winner is decided by arrival order, which favours lower latency (accepted limit, R6).
- **If two independent transitions in the same tick drive both teams to 0 Active** (for example, a strike-out on team 0 and a reversal on team 1, each against a different pawn): the first event in queue order wins, but only if its payload shows the other team still had an Active player. The second event's payload shows `ActiveCountAfter[1 - e.Team] = 0`, so it awards nothing. If neither event qualifies, Round adjudicates (F4).
- **If `RequestTransition(Informant, X)` arrives for a participant who is already `Informant`** (any of the four entering reasons): `NoOp_AlreadyInState`. `LastTransitionReason` keeps the first reason and no event is emitted.
- **If an assassination is reversed and the attacker was already `Informant`** (attack validated earlier, attacker struck out first): the reversal request is `NoOp_AlreadyInState` and the defender is untouched.
- **If a listener of `OnParticipantStateChanged` calls `RequestTransition` re-entrantly** (e.g. Round ending the round from inside the handler): the call applies immediately and its events queue behind the current one. Listeners therefore see events strictly in `EventSeq` order.

**Round boundaries**
- **If a queued event resolves after `EndRoundAll`**: `RequestTransition` from `RoundEnded` returns `Rejected_IllegalTransition`. Round also drops events whose `RoundEpoch` is stale or that arrive while the phase is not `Active`. Nothing changes.
- **If `EndRoundAll` is called twice**: the second call is a NoOp for every record and emits no event.
- **If a grace or respawn timer started in round N fires after `ResetAll`**: the request carries `ExpectedEpoch = N` and returns `Rejected_StaleEpoch`. `BindPawn` for a stale respawn is rejected because the timer's owner checks the epoch first (NPC-Kill Penalty & Informants contract).
- **If an informant is killed and `EndRoundAll` resolves before their respawn**: the binding stays retired. `BindPawn` is rejected for `RoundEnded` (`Rejected_WrongState`), so the respawn is cancelled.
- **If `BindPawn` is attempted before `ResetAll` in a new round** (participant still `RoundEnded`): `Rejected_WrongState`. The spawning system destroys the deferred pawn.

**Binding and pawn lifetime**
- **If a player pawn is destroyed between attack submission and resolution**: `NotTargetable(Stale)`. No strike, no transition. Never `Npc`.
- **If a pawn is recycled from a pool (NPC replacement)**: the tombstone is keyed by object identity plus generation, and recycling calls `ClearBinding`. A fresh NPC is `Unbound` and its kill is an NPC kill.
- **If a player pawn exists for one frame before `BindPawn`**: this cannot happen. Player pawns use deferred spawn: bind, then finish spawning, then possess. A failed bind destroys the pawn.
- **If a pawn reaches `EndPlay` while still `BoundLive`** (the retire step was missed): `ClearBinding` nulls the participant's live pawn. If `EndPlayReason == Destroyed` during phase `Active`, it reports a fault and, for an `Active` participant, raises `OnBindingLost(Id, PawnDestroyed)`. Lifecycle state is unchanged. Round's I4 recovery is its own-card respawn (Round R17.9; `Void(PawnLost)` past its cap), not a crowd adoption.
- **If travel starts while pawns are bound**: Session's `OnTravelStarting` calls `InvalidateAllBindings`. Every binding and tombstone is cleared without events, and the teardown `EndPlay` calls clear silently.

**Disconnect and rejoin**
- **If an `Active` participant disconnects during `Active`**: only `Connection`, `DisconnectedAt` and `ConnectionGeneration` change. The pawn stays `BoundLive`. Through the R9 `PawnLeavingGame` override it is driven by a crowd AI controller over the same command path, so it does not freeze or vanish (Pillar 1). A kill on it during grace is a `Player` kill: the participant becomes `Informant (Assassinated)` with no pawn; NPC-Kill Penalty holds it Absent with no respawn timer until it reclaims (PEN-R8), and on reclaim it is "awaiting pawn" like any returning informant (Round R18; cross-review S-B3 2026-10-24).
  - **The takeover itself must not be a tell** (requirement on Crowd & NPC Behaviour and Shared Character & Locomotion). The AI controller takes over by blend, with no dead stop: a running pawn decelerates through the baseline gait, an interaction is cancelled with its ordinary cancel animation, and the pawn then picks up a crowd behaviour. There is no single-frame snap from player movement to crowd baseline.
  - An AI-held pawn keeps its team's `ActiveCount` above 0 for up to `DisconnectGraceSec`. This is accepted, and flagged to Round as a Pillar 4 stall vector to be answered with objective pressure, never with a reveal (OQ5).
- **If the disconnect grace expires**: Round requests `Active -> Informant (Disconnect)` with the captured epoch and connection generation. It is Applied because `Connection == Disconnected` and the generation matches. Round calls `ReleaseToCrowd` in the same call stack. From that instant the pawn is an NPC, and killing it costs the attacker a strike like any NPC. Defence the player had active is cancelled by its owning feature on its next tick; concealment is not cancelled: it ends by the pawn's natural Crowd-driven exit from the bush (Concealment change request 4).
- **If the disconnecting participant was the last `Active` on their team and grace expires**: F4's trigger does **not** fire, because the Reason is `Disconnect`. The team has 0 Active, and Round adjudicates by its R15 partition: `EliminationWin` with `EndReason = ForfeitElimination` when the other team has a Connected Active member, otherwise `Void(Abandoned)`; a host kick of the last Active member gives `Void(KickedLastActive)`. The Roster only reports the event.
- **If every remaining `Active` participant on both teams is disconnected**: the expiries produce no elimination trigger; Round's R15 partition gives `Void(Abandoned)` (no Connected Active member remains on either side).
- **If a participant flaps (disconnects and reconnects repeatedly)**: each drop starts a new grace with a new generation, and the old timers are stale. Round may cap the total grace per participant per round (recommended `DisconnectGraceBudgetSec`, Round-owned, provisional). The Roster enforces only the generation check.
- **If a participant rejoins at the same instant grace expires**: Session processes receives before window expiry, so the reclaim lands first. The expiry request then fails with `Rejected_NotDisconnected`.
- **If an `Informant` disconnects**: `Connection` flips and the state stays `Informant`. Round calls `ReleaseToCrowd` on their pawn at once, because an informant's pawn has no grace value. `AwaitingPawn` rises by 1 and `GetPawn(Id)` is null. The released pawn resolves `Npc`. On reclaim they are "awaiting pawn" and receive an adopted crowd pawn (R9).
- **If a participant reconnects**: `Reclaimed(Id)` gives the same id, handle, team and state, and `Connection = Connected`. The retained PlayerState is re-associated and C-TeamView is re-pushed. An `Active` participant within grace repossesses the bound pawn. An `Informant`, including one who returns after grace, gets an adopted crowd pawn. Nobody is refused.
- **If two live connections present the same `IdentityKey`**: newest wins. Session kicks the old connection and the record is reclaimed exactly once.
- **If a participant disconnects while teams are unlocked (Lobby)**: Session sets `Connection = Dormant` (Session rule 13). The record keeps its id, handle and match-scoped fields, and stops holding a capacity slot. A return in the Lobby is `Reclaimed` as the same record. If teams lock while it is still Dormant, it sits out the round (`InRound = false`, `RoundEnded`) and can return at the next Lobby.
- **If a participant disconnects during `Ended`**: only `Connection` changes. A `RequestTransition(Disconnect)` from `RoundEnded` is `Rejected_IllegalTransition` (Round should not send one). `ResetAll` keeps `Connection = Disconnected`, and Round's start rule decides whether the next round starts without them.
- **If a participant is disconnected before the round's `Active` phase begins**: Round still spawns them a bound pawn, driven by the crowd AI from the start (Round R6; the earlier "no pawn is spawned, adopted crowd pawn on rejoin" rule is superseded). On rejoin within grace they repossess it. Round's grace clock starts at `ActivePhaseStart` (F6).
- **If a new `IdentityKey` tries to join while teams are locked**: `Rejected_TeamsLocked`. Session refuses the join or queues it for the next Lobby (Session's choice).
- **If the dev build has no online subsystem**: the key comes from `-IdentityKey=` or the PIE instance index (Session rule 6). Two PIE clients therefore never collapse into one record, and a PIE instance that restarts with the same index reclaims its record. The key is unauthenticated in all MVP builds (R3).
- **If a client presents another participant's `IdentityKey`** (a spoof, MVP): the Roster cannot tell the difference, so the record is reclaimed by the presenter and the owner is superseded. This is an accepted MVP risk, limited to trusted playtests, and closed by the server-issued rejoin secret before the first external playtest (R3, OQ13).

**Team assignment and counts**
- **If `AssignTeams` would put 6 on a team**: the whole batch is rejected (`Rejected_TeamFull`) and nothing changes.
- **If a team change is requested while locked**: `Rejected_TeamsLocked`, with no side effects.
- **If one team has no participants (1v0 dev test)**: `IsTeamEliminated` is false for the empty team, so an elimination win is impossible and only `ObjectiveWin` can end the round. Whether such a round may start is Round's dev flag.
- **If there are 2v2 or 4v3 participants**: all formulas work unchanged. `TeamSize` is a cap, not a requirement.
- **If `RegisterParticipant` arrives with 10 non-Dormant records already present**: `Rejected_RosterFull` (`2 × TeamSize`). Dormant records do not count.
- **If `TeamSize` changes between rounds**: Round calls `SetTeamSize` while unlocked. A value below the largest current team returns `Rejected_TeamSizeBelowRoster`, and Round must reassign first.

**Host and network**
- **If the listen-server host quits or crashes**: the store is lost with the host process, the match ends for every client, and no Roster state is recovered. There is no host migration in MVP. Session & Travel defines what clients see.
- **If a client receives the C-TeamView update late (latency)**: the UI shows the previous state until the update arrives. Roster state is never client-predicted, and nothing gameplay-relevant is decided on the client.
- **If replication order differs** (pawn arrives before the teammate entry or after it): the client's teammate `PawnRef` resolves when the pawn becomes relevant. A teammate outside net-cull distance has an unresolved ref, so marker positioning beyond relevancy needs a separate position carrier (Teammate Markers / Visibility Policy, see Open Questions).

**Pillar 1 leak cases**
- **If any client-side code tests `Pawn->GetPlayerState() != null` (prototype `IsNpc()`)**: this is forbidden. Until spike S1 passes, the pointer still leaks, and after it passes the pointer is null for all non-owned pawns. Client code must use C-TeamView for teammates and nothing for opponents.
- **If the public player list is grouped or coloured by team for opponents**: forbidden. Names only, in an order that encodes nothing (alphabetical), because team is derivable but must not be presented.
- **If any PlayerState field outside the allowlist would change on a player event** (`Score`, `bIsABot`, `UniqueId`, `PlayerId`, `bIsInactive`, ping and so on): it must be disabled or held constant (R6), because a field that changes when someone is eliminated or disconnects is a remote kill or disconnect detector.
- **If a disconnected player's pawn were destroyed**: it would vanish from the crowd and reveal itself as a player. That is why the `PawnLeavingGame` override, grace and `ReleaseToCrowd` exist. The pawn is never destroyed because of a disconnect.
- **If a disconnected player's name dropped from the list**: forbidden. The in-round list is frozen (R6, Session rule 24).
- **If an informant respawned as a freshly spawned pawn at `TeamSpawn`**: forbidden. A new actor and NetGUID appearing mid-round at a team spawn would identify a player. Informants still respawn at their team's spawn (locked rule), but by adopting an existing crowd pawn at the emergence sockets anchored on `TeamSpawn` (R9).

## Dependencies

**Upstream (this system depends on):** none (Foundation layer, per the systems index). `TeamSize` is **injected** into `BeginMatch` / `SetTeamSize` by its caller (Round, which owns the key in Tuning Data & Config), so the Roster has no direct config dependency (coding standard: dependency injection over singletons). Engine dependency: UE 5.8.4 replication (PlayerState, PlayerController relevancy, pawn pointer conditions). See R6 and the spikes.

**Downstream (systems that depend on this one).** Each row gives the interface, whether the dependency is hard or soft, and the status of the other GDD. Every row is **provisional** until the other GDD exists, unless marked otherwise.

| Dependent | Interface it uses | Hard / Soft | Other GDD status |
|---|---|---|---|
| Session & Travel | `BeginMatch`, `RegisterParticipant` (Registered/Reclaimed; Session's `FindByIdentityKey`/`CreateParticipant`/`BindConnection` mapped in R4), `RebindView`, `RemoveParticipant` (Lobby kick only), `NotifyConnectionChanged` (sole writer of the `Connection` projection), `InvalidateAllBindings` at `OnTravelStarting`, `IsRoundScopeReset`, `AssignTeams`; supplies the client-asserted `IdentityKey` (R3); keeps PlayerStates through disconnects (R6) | Hard | **Designed** (session-travel.md, 2026-10-24); API names to be reconciled (OQ14) |
| Round & Match Framework | `SetTeamsLocked`, `ResetAll(RoundIndex)`, `EndRoundAll`, `RequestTransition(…, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)`, `SetTeamSize`, `GetUnboundActiveParticipants`, `RecordAppearance`, F1–F4 counts; consumes `OnParticipantStateChanged` (elimination trigger) and `OnBindingLost`; owns `DisconnectGraceSec`, `TeamSize`, the R15 forfeit/void partition and the I4 own-card respawn (R17.9) | Hard | **Designed** (round-match-framework.md, in review) |
| Information Visibility Policy | `IsPawnBound` (fact `IsPlayer`), `GetRelation` (viewer class), `GetState` (fact `IsInformant`), `GetTeam` (fact `TeamMembership`); ratifies R6 audiences; owns leak tests | Hard | **Designed** (information-visibility-policy.md) |
| Shared Character & Locomotion | the single player-pawn spawn site (deferred spawn, then `BindPawn`); crowd adoption through `TakeOverCrowdPawn` (R2a); `PawnLeavingGame` override (SCL R2b); `ClearBinding(Pawn, EndPlayReason)` at `EndPlay`; pawn-pointer suppression and replication parity (R6) | Hard | **Designed** (shared-character-locomotion.md) |
| Assassination & Defence Reversal | `ResolveVictimKind`, `GetRelation`, `GetState`; `RequestTransition(Assassinated / ReversedAgainst)`, `RetireBinding` implied by the transition | Hard | **Designed** (assassination-defence-reversal.md) |
| NPC-Kill Penalty & Informants | `RequestTransition(StrikeLimit)` with release on exile (R8); `RetireBinding` + crowd adoption (`PickRespawnPawn`, then `TakeOverCrowdPawn`, which calls `BindPawn`) for informant respawn; `AwaitingPawn`; `OnRosterReset` to clear strikes; the informant-kill invariant (R10); strike carry-over across reclaim | Hard | **Designed** (npc-kill-penalty-informants.md) |
| Concealment (Bushes) | `GetRelation(viewer, subject)`; `OnRosterReset` | Hard | **Designed** (concealment-bushes.md) |
| Objectives Chain | `GetState` (only `Active` may interact), `GetTeam` | Hard | **Designed** (objectives-chain.md) |
| Teammate Markers | C-TeamView teammate entries `{PublicHandle, State, PawnRef, IsConcealed, RetireAtServerTime, bReconnecting}` (`State` drives the filled/outline marker of art bible 3.4; `IsConcealed` the hatch; `RetireAtServerTime` the exact exit, TM-R6; `bReconnecting` the label suffix, TM-R11) | Hard | **Designed** (teammate-markers.md) |
| Informant Pings & Team Communication | sender `State`, team, bound pawn, `PublicHandle`; C-TeamView as the carrier family for its `FTeamPingView` part; `OnRosterReset` (clears the part) | Hard | **Designed** (informant-pings-team-comms.md, MVP since 2026-10-25) |
| HUD | C-TeamView own and teammate state, teammate `bReconnecting` (reconnect lines, HUD-R10a); own `State` drives the informant frame, the removal card and brace-indicator hiding; owner `WrongTargetTally`/`LastResultSeq` (tally re-push gate, HUD-R7) and `T_due` (returning countdown, HUD F7) | Hard (HUD CR-HUD-1) | **Designed** (hud.md) |
| Crowd & NPC Behaviour | `ReleaseToCrowd` adoption; drives a disconnected player's pawn during grace (listed in index row 11 as soft, verified 2026-10-24, cross-review C-W11) | Soft | **Designed** (crowd-npc-behaviour.md) |
| Character Pool & Appearance | `AssignedAppearanceId` spawn hand-off; `RecordAppearance` (log-only); the card after crowd adoption | Soft | **Designed** (character-pool-appearance.md); OQ7 answered: history is log-only, depth 0 |
| Input & Interaction Framework | `GetLegalityClass(Pawn)` (Active for unbound pawns); owner-audience own-state change | Hard | **Designed** (input-interaction-framework.md) |
| Level Socket Contract | `GetTeam`, for start-placement zone mapping and the informant respawn set | Soft | **Designed** (level-socket-contract.md) |
| Progression & Cosmetics (Full Vision) | `IdentityKey` as the future account hook | Soft | Not yet written |
| Camera System | own binding and owner-audience state (pawn present, Informant) for CS-R12, Owner audience only (Camera Dependencies; added at the 2026-10-24 cross-review, C-W13) | Soft | **Designed** (camera-system.md) |

**Bidirectionality note.** The systems index lists the Roster as a dependency of every designed row above, Crowd & NPC Behaviour, Character Pool & Appearance, Input & Interaction Framework and Level Socket Contract included (rows 3, 4, 8, 11; verified 2026-10-24, cross-review C-W11). Tuning Data & Config (designed) is an indirect upstream: the Roster reads no key, but `TeamSize` arrives by injection. Each dependent GDD must list "Participant State & Roster" in its own Dependencies.

## Tuning Knobs

The Roster is deliberately almost knob-free: it implements rules, and the rules' numbers belong to their owners. Knobs it **owns**:

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|---|---|---|
| `AppearanceHistoryDepth` | int | Roster | **0** (log-only) | 0-16 | 0-8 | ServerOnly | Round | Depth | How many past AppearanceIds per participant are kept, for logs and the post-round record. Character Pool's allocator never reads it (CPA change request 6). High: memory only; it must never feed the deal, because a no-repeat rule would leak. 0 keeps no history (the default). |
| `RosterTransitionLogDepth` (dev only) | int | Roster | 64 | 16-1024 | 16-512 | ServerOnly | Round | Depth | Ring buffer of applied transitions for debugging and test assertions; never replicated; compiled out of Shipping. High: memory only (about 24 B per entry). Low: too short to diagnose a full round. |

Converted to TDC's CR-15 columns at the 2026-10-24 reconciliation (TDC change request 1); hard ranges added (the earlier table gave only safe ranges).

Knobs it **consumes**. These are owned elsewhere and must not be duplicated here:

| Key | Owner | Default | Roster use |
|---|---|---|---|
| `TeamSize` | Round & Match Framework (key in Tuning Data & Config, latch `Round`) | 5 | Capacity per team (F5), the output bound of F1–F3, `MaxParticipants = 2 × TeamSize`. Values below 5 are for tests (2v2, and 1 through a test preset). Raising it above 5 contradicts the locked 5v5 rule. It is frozen while teams are locked (R4). |
| `NpcKillStrikeLimit` | Tuning Data & Config / NPC-Kill Penalty | 3 | None directly. Penalty decides when to request `StrikeLimit`. |
| `RejoinWindowSec` | Session & Travel | 120 s | Bounds the grace through `G_eff` (F6). |
| `DisconnectGraceSec` | Round & Match Framework | 30 s (hard 3–120, below 10 dev-only; applied as `G_eff = min(G, RejoinWindowSec - 30)`) | F6 timing, applied by Round. **Too high:** an absent player's crowd-driven pawn keeps their team's elimination count alive for too long. **Too low:** a short network drop forfeits the round (Pillar 3 punishes the network, not a decision). |

**Constants, not knobs:** two teams; the three states; the six Reasons; the legal-transition table. These are locked rules.

## Visual/Audio Requirements

This system has no direct visual or audio output (Core category; optional section). It **constrains** presentation:
- Every state change that a viewer can perceive is presented by the owning feature under art bible 5.2 / 5.2.1:
  - Elimination plays the one authored fall-and-removal sequence, identical for NPC, enemy, ally and informant.
  - Informant respawn adopts an existing crowd pawn out of every opponent's line of sight (R9). No pawn ever appears.
  - A swap happens only at occlusion.
- No Roster event may trigger any per-pawn VFX, SFX, material change, LOD change or net-update-rate change visible to a non-team client.
- The teammate marker fill (filled = Active, outline-only = Informant, art bible 3.4 / 7 element 1) is driven by C-TeamView `State`.

`art-director` was not consulted for this section. It is optional for a Core system with no visual output, and the constraints above are quoted from the art bible.

## Game Feel

Not a player-operated system; optional for Core, and no feel targets are set. The only feel-relevant property is **latency of state reflection**: a teammate's marker changes fill within one C-TeamView replication update of the server transition (target ≤ 250 ms at 100 ms RTT, provisional). The client never predicts it.

## UI Requirements

The Roster has no screen of its own. It supplies data to:
- **HUD:** own state (Active/Informant), for the informant frame grade and the removal message, which NPC-Kill Penalty phrases ("Exiled").
- **Teammate Markers:** teammate state. Connection state is not supplied (R6, Session rule 24).
- **Public player list** (Lobby UI and any in-round list): **names only**, alphabetical (invariant-culture ordinal sort), never grouped, coloured or counted by team or status for opponents. In-round it is the **frozen** list captured at `Preparing` (Session rule 24), and its length never changes mid-round. The owner's own team view may group teammates.

Player-facing strings derived from Roster state follow the Player Fantasy language rule. UX ownership is HUD / Teammate Markers / Lobby UI, so no `/ux-design` spec is needed for this system itself.

## Cross-References

| This GDD references | Element | Where |
|---|---|---|
| design/gdd/game-concept.md | Pillars P1, P3; informant rules; 5v5 | Overview, Player Fantasy, R5 |
| design/gdd/systems-index.md | Row 2, Dependency Map, circular-dependency note (Participant State / Round / NPC-Kill Penalty) | Dependencies |
| design/gdd/tuning-data-config.md (designed) | `TeamSize` (owner Round, latch `Round`), `NpcKillStrikeLimit` canonical keys | Tuning Knobs, F1, F5 |
| design/gdd/character-pool-appearance.md (designed) | `AssignedAppearanceId` hand-off, `AppearanceHistory` log-only (change request 6) | R3, Tuning Knobs |
| design/gdd/input-interaction-framework.md (designed) | `GetLegalityClass(Pawn)` | R10 |
| design/gdd/level-socket-contract.md (designed) | Team read for zone mapping and the informant respawn set | Dependencies |
| design/art/art-bible.md | 5.2 viewer-state matrix, 5.2.1 swap-at-occlusion and kill presentation, 3.4 / 7 teammate marker fill states | Visual/Audio, UI Requirements |
| design/gdd/session-travel.md (designed) | Rule 6 identity key (client-asserted), rule 8 admission, rule 13 Dormant, rule 17 field scope table, rules 22/24/25 frozen PlayerStates and list, `ConnectionState`, `RejoinWindowSec` | R3, R4, R6, R11 |
| design/gdd/round-match-framework.md (designed) | Phases, resolution queue key, `DisconnectGraceSec`, elimination latch | R7, R8, F4, F6 |
| design/gdd/information-visibility-policy.md (designed) | Audiences ServerOnly/Owner/Team/All, `CanViewerKnow`, leak tests | R6 |
| design/gdd/shared-character-locomotion.md (designed) | Deferred spawn + bind; pawn pointer suppression | R6, R9 |
| design/gdd/assassination-defence-reversal.md (designed) | `ResolveVictimKind`, `OnPawnKilled`, attacker-state check at resolution | R10, Edge Cases |
| design/gdd/npc-kill-penalty-informants.md (designed) | `StrikeLimit`, informant respawn, `OnRosterReset` | R7, Edge Cases |
| design/gdd/crowd-npc-behaviour.md (designed) | `ReleaseToCrowd` adoption; grace-period driving | R9, R11 |
| design/gdd/teammate-markers.md (designed) | C-TeamView consumption; out-of-relevancy positions | R6, Edge Cases |
| docs/engine-reference/unreal/modules/networking.md | Iris production-ready vs plugin Beta flag; the file has no PlayerState/seamless travel/filter coverage, so those claims come from installed 5.8.4 source (ue-replication-specialist) | R6 |
| Source/StealthGame (prototype) | `AStealthPlayerState` replicates `Team`/`LifeState` to all; `IsNpc()` derived from `PlayerState`; to be migrated | R6, Open Questions |

**Change requests received (reconciliation 2026-10-24)**

| From | Request | Disposition |
|---|---|---|
| Assassination CR-5 | OQ9 answered (attacker state at resolution; `NotTargetable` gives `NoContact`) | APPLIED (OQ9 closed) |
| Assassination CR-15 | strike-caused Owner/Team role changes presented via `OnRoleChangePresented` at `DeathServerTime` | APPLIED (R6 update policy) |
| Character Pool CR-6 | `AppearanceHistoryDepth` 0 log-only; spawn-handoff copy; R9 card effect `TakeNpcCard`; CPA soft dependency | APPLIED (R9, knob table); depth 0 and the soft row ALREADY DONE |
| Concealment CR-4 | grace-expiry edge case: concealment ends by a natural Crowd exit | APPLIED (Edge Cases) |
| Crowd CR-10 | `CrowdStopDecel` derived constant; `CrowdMaxIdleSec` 20 s | APPLIED (OQ6 closed) |
| HUD CR-HUD-1 | HUD dependency becomes Hard | APPLIED (Downstream table) |
| IIF CR-3 | `GetLegalityClass(Pawn)` | ALREADY DONE (R10) |
| IVP CR-11 | R6 note: an AI pawn's `Owner` also exports an unmappable GUID | APPLIED (R6) |
| LSC CR-5 | R9 crowd adoption and the respawn edge case use `PickRespawnPawn` | APPLIED (R9; edge cases already point to R9) |
| Penalty CR-3 | `StrikeLimit` releases the binding to the crowd; edge case; AC-PSR-14; interactions row | APPLIED (R8, R9, Edge Cases, AC-PSR-14, Interactions) |
| Penalty CR-9 | R10 informant-kill invariant: last two sentences replaced (three residuals) | APPLIED (CD-approved; the visible tally confirmed by the user 2026-10-25) |
| Penalty CR-19 | C-TeamView owner part carries `{WrongTargetTally, LastResultSeq}` | APPLIED 2026-10-25 (user decision; R6 owner part, plus the Owner-only `T_due`) |
| SCL CR-13 | R9 names `TakeOverCrowdPawn` (R2a); `PawnLeavingGame` override implemented by SCL R2b, spike S8 stays here | APPLIED (R9) |
| Teammate Markers CR-1 | `IsConcealed` in the C-TeamView teammate entry; Markers rows Designed; OQ8 answered | APPLIED (R6 lists the carrier IVP's fact table already assigns; OQ8 closed) |
| Teammate Markers CR-12 | team-only `bPawnBound` per teammate entry | DECLINED for MVP (user decision 2026-10-25; Markers OQ13 closed) |
| Teammate Markers CR-13 | team-only `RetireAtServerTime` on the retire update | APPLIED 2026-10-25 (user decision; R6 teammate entry) |
| Round CR-2 (a), (d) | `ResetAll` in `StartRound` before travel; `ExpectedEpoch` on every timer | ALREADY DONE (R7, R8) |
| Round CR-2 (b), CR-12 (d) | a participant disconnected before `Active` still gets a bound AI-driven pawn | APPLIED (Edge Cases) |
| Round CR-2 (c) | takeover "blend, no dead stop"; participant-bound AI issues `Move`/`Look` only | APPLIED (R11, Edge Cases) |
| Round CR-12 (a) | R15 forfeit/void partition in F4 edge cases | APPLIED (Edge Cases) |
| Round CR-12 (b) | `DisconnectGraceSec` floor 3 (dev), `G_eff = min(G, RejoinWindowSec - 30)` | APPLIED (R11, F6, consumed-knob table) |
| Round CR-12 (c) | `OnBindingLost(P, PawnDestroyed)`; I4 recovery is Round's own-card respawn | APPLIED (R9, R12, Edge Cases) |
| Round CR-12 (e) | `ResetAll(RoundIndex)` repeatable with the same index after an aborted start | APPLIED (R7) |
| Round CR-12 (f) | lock wording aligned with Round's unlock on `Lobby` arrival | APPLIED (R5) |
| Session CR-1 | field scope table, no `Expired -> Retired` mapping, `NotifyConnectionChanged` on every change, fresh-login rejoin re-associates the PlayerState | ALREADY DONE (R3, R4, R11) |
| Session CR-1 | record the provisional `RejoinToken` as the server-issued rejoin secret R3 requires | APPLIED (R3 `IdentityKey` row) |
| TDC change request 1 | Tuning Knobs table in CR-15 columns | APPLIED |
| Cross-review 2026-10-24 C-W6, S-B2, S-B3 | C-TeamView content list reads as closed; `EndRoundAll` timing; killed-in-grace path | APPLIED (cross-review fixes 2026-10-24): R6 sub-struct note; `EndRoundAll` called at Round's round-end step; disconnect edge case names Penalty's Absent path |
| ADR-0010 sync 2026-10-24 (also ADR-0019) | R9 / Interactions: Penalty calls `TakeOverCrowdPawn`, which performs the eligibility check and `BindPawn` on Penalty's behalf; no card move unless `Applied` | APPLIED (R9 crowd adoption, Interactions and Dependencies rows; slot commit + next-frame possession wording) |
| ADR-0005 sync 2026-10-24 | R4 mapping: `BindConnection` keyed by peer key; seamless travel replaces PlayerController and PlayerState (E5); OQ3 / OQ4 answered | APPLIED (R4 row; OQ3, OQ4 marked answered) |
| ADR-0020 sync 2026-10-24 | Name the Roster event raised after `RebindView` that feature view parts subscribe to | APPLIED (`OnViewRebound(ParticipantId)`, server-only wiring notification, no `EventSeq`; stated under the event list) |
| ADR-0019 sync 2026-10-24 | R8/R9 "release, not retire" for `StrikeLimit` inside `RequestTransition` (Penalty CR-3) | ALREADY DONE (Penalty CR-3 row above) |
| ADR-0007/0008 sync 2026-10-24 | Test root follows the module layout | APPLIED (`Source/StealthFoundation/Private/Tests/Roster/`; harness tests in `StealthTestHarness`) |
| User decision 2026-10-25 (scope; Session OQ7, HUD OQ4, TM OQ1) | Team-audience teammate-reconnecting flag in C-TeamView | APPLIED (R6 `bReconnecting`, derived from the `Connection` projection; AC-PSR-53) |
| User decision 2026-10-25 (scope; Informant Pings to MVP) | C-TeamView ping part for Informant Pings | APPLIED (R6 feature-parts list; the part is owned by Pings, PNG-R6) |

## Acceptance Criteria

*qa-lead consulted at authoring and again at the independent design review on 2026-10-24. The review's AC defects (PIE determinism, the fault sink, the injected dependency list, split and rewritten AC-28/33/37–43) and coverage gaps (AC-44 to AC-52) are applied below. At authoring the qa-lead found ten gaps; all were resolved in Sections C and D before this was written: result precedence (R8), `Rejected_IllegalReason` cases, `EventSeq` scope, `BindPawn` epoch, wrong-input results, the `ResetAll` precondition, the I4 bound and the spike list OQ0.*

**Test root:** `Source/StealthFoundation/Private/Tests/Roster/` (the Roster store's module, ADR-0007); [PIE-n] and [MP-n] tests live in `Source/StealthTestHarness/Private/` (ADR-0008). Test names `StealthGame.Roster.*` are unchanged.

**Tags:**
- **[Core]** pure C++ automation test: no world, no actors. The Roster core takes these injected dependencies:
  - an opaque pawn handle `{ObjectKey, Generation}`;
  - a **pawn-liveness provider** (valid, destroyed, pending kill);
  - an authority provider;
  - a thread check;
  - a server clock in integer milliseconds;
  - a random source for `MatchId` and `PublicHandle`, seeded per test;
  - a **fault sink**.

  Every "fault is reported" assertion checks that the fault sink recorded exactly one fault with the expected code. Tests never rely on `ensure` firing, because an `ensure` fires once per call site per process.
- **[Int]** automation world or functional test.
- **[PIE-n]** PIE listen server with n clients. **Gating runs** use fixed net emulation: PktLag 100 ms, no lag variance, PktLoss 0 %. Each check waits on a condition (for example "update received") with a timeout counted in server frames or integer milliseconds. Hitting the timeout fails the test. A separate **non-gating [Soak]** run repeats the PIE tests at PktLoss 2 % and lag variance ±20 ms, and its results are advisory.
- **[MP-n]** standalone multi-process run (listen server plus n client processes). It is required wherever PIE seamless travel or one-GameInstance-per-instance is not representative.
- **[UI-ev]** retained screenshot in `production/qa/evidence/`. It is human-reviewed evidence, never the pass condition.
- **[S#]** blocked until spike S# (OQ0) passes.
- **[Dep:X]** needs GDD or system X to exist. Until then the test runs against a stub of X's interface.

**Authority, ownership, registration (R1–R5)**
- **AC-PSR-01** [Core] `StealthGame.Roster.RecordSchemaWhitelist`: GIVEN the participant record type, WHEN its reflected fields are enumerated, THEN they equal R3's field set exactly, with no strike, defence, concealment or objective field (R1, R3).
- **AC-PSR-02** [Core] `StealthGame.Roster.ClientCallsRejected`: GIVEN authority = false, WHEN every mutating call, `ResolveVictimKind`, `IsPawnBound`, `GetLegalityClass` and every count query (`CountActive`, `CountInformant`, `CountRoster`, `IsTeamEliminated`) is invoked, THEN:
  - the results are `Rejected_NotAuthority` / `NotTargetable(Stale)` / false / 0;
  - the fault sink records one fault per call;
  - the store is unchanged;
  - no call returns `Npc` or a real count (R2, R10).
- **AC-PSR-03** [Core] `StealthGame.Roster.OffGameThreadMutationAsserts`: GIVEN a worker thread, WHEN it calls `RequestTransition` or `BindPawn`, THEN the result is `Rejected_WrongThread`, the fault sink records one fault, and nothing changes (R2, R8, I7).
- **AC-PSR-04** [Int] `StealthGame.Roster.NoRpcCarriesParticipantId`: GIVEN every Server/Client/Multicast UFUNCTION in the game module, WHEN parameter types are scanned by reflection, THEN none is or contains, transitively through structs and containers, a `ParticipantId` or `IdentityKey` type. Both are distinct struct types, not raw integers or strings, which makes the scan meaningful (R2).
- **AC-PSR-05** [Core] `StealthGame.Roster.BeginMatchAndRegister`: GIVEN `BeginMatch(5)`, WHEN keys K1..K10 then K11 register, THEN:
  - ids are 0..9 in order;
  - each new record is `RoundEnded` / `None` / `Connected`;
  - K11 gets `Rejected_RosterFull`;
  - after `RemoveParticipant(3)` and a new key, the new id is 10;
  - a second `BeginMatch` resets `ParticipantId`, `EventSeq` and `RoundEpoch` to 0 and draws a different `MatchId`;
  - every `PublicHandle` is unique, and with the seeded random source the handles are not in id order (R3, R4).
- **AC-PSR-06** [Core] `StealthGame.Roster.LockedRosterIsFrozen`: GIVEN teams locked, WHEN a new key registers, or a Dormant record's key registers, or `RemoveParticipant`, `AssignTeams` or `SetTeamSize` is called, THEN each returns `Rejected_TeamsLocked`, `RosterCount(T)` is unchanged, and a known non-Dormant key still returns `Reclaimed` (R4, R5, F3).
- **AC-PSR-07** [MP-2][S4] `StealthGame.Roster.MP.SameMachineClientsDistinct`: GIVEN two client processes on one machine with distinct `-IdentityKey=` values and no online subsystem, WHEN both join and one rejoins with its key, THEN there are 2 records, the rejoin reclaims its own record, and the other client's PlayerState still exists (R4).
- **AC-PSR-08** [Core] `StealthGame.Roster.AssignTeamsAtomicSwap`: GIVEN teams 5/5, WHEN one batch swaps A (team 0) with F (team 1), THEN `Applied` and still 5/5. WHEN the same two moves are sent one at a time, THEN the first returns `Rejected_TeamFull` (F5).
- **AC-PSR-09** [Core] `StealthGame.Roster.AssignTeamsAllOrNothing`: GIVEN batches with 6 on one team, an unknown id, a duplicate id or team 2, WHEN each is applied, THEN it returns `Rejected_TeamFull` / `Rejected_UnknownParticipant` / `Rejected_InvalidBatch` respectively, the store is unchanged, and no `OnTeamsChanged` fires (R5, F5).
- **AC-PSR-10** [Core] `StealthGame.Roster.UndersizedRostersAndLock`: GIVEN 2v2, 4v3 and 1v0 rosters, WHEN F1–F4 are evaluated with everyone `Active`, THEN `(ActiveCount, RosterCount)` per team equals (2,2)/(2,2), (4,4)/(3,3) and (1,1)/(0,0), and `IsTeamEliminated(emptyTeam)` = false. GIVEN one non-Dormant record with `Team = None`, WHEN `SetTeamsLocked(true)` is called, THEN the result is `Rejected_UnassignedParticipants`. GIVEN teams unlocked, WHEN `ResetAll` is called, THEN `Rejected_TeamsUnlocked` and no record is `Active` (R5, R7, F4, I2).

**Batch calls and transitions (R7, R8, legal-transition table)**
- **AC-PSR-11** [Core] `StealthGame.Roster.ResetAllBatch`: GIVEN teams locked and mixed states, bindings, tombstones and one Dormant record, WHEN `ResetAll(RoundIndex = 2)` runs, THEN:
  - every non-Dormant record is `Active` with `InRound = true`, and the Dormant record is `RoundEnded` with `InRound = false`;
  - `IsRoundScopeReset(2)` = true;
  - bindings, tombstones, `LastTransitionReason` and `AssignedAppearanceId` are cleared;
  - `Team` and `Connection` are kept;
  - `RoundEpoch` is +1;
  - exactly one `OnRosterReset` fires and no per-participant event fires (R7).
- **AC-PSR-12** [Core] `StealthGame.Roster.EndRoundAllBatchIdempotent`: GIVEN Active and Informant records, WHEN `EndRoundAll` runs twice, THEN all are `RoundEnded` after the first call with exactly one `OnRosterRoundEnded`, and the second call emits nothing and does not advance `EventSeq` (R7).
- **AC-PSR-13** [Core] `StealthGame.Roster.BatchOnlyReasonsRejectedSingly`: GIVEN any state, WHEN `RequestTransition` is called with Reason `RoundEnd` or `Reset` (for any target), THEN the result is `Rejected_BatchOnlyReason` and nothing changes (R7, R8 precedence 5).
- **AC-PSR-14** [Core] `StealthGame.Roster.Transition.ActiveToInformant` (parameterised over Assassinated, ReversedAgainst, StrikeLimit, and Disconnect with Connection = Disconnected): GIVEN Active with a live pawn, WHEN the request is made, THEN it returns `Applied`. Before the call returns, the state is `Informant`, the reason and `LastEventSeq` are written, and the pawn is Retired, so `ResolveVictimKind` = `NotTargetable(AlreadyResolved)`; for `StrikeLimit` the pawn is instead `Unbound`, so `ResolveVictimKind` = `Npc`, and the participant has no live pawn (R8, Penalty CR-3). Exactly one `OnParticipantStateChanged` is then dispatched (R8).
- **AC-PSR-15** [Core] `StealthGame.Roster.Transition.DisconnectRequiresDisconnected`: GIVEN Active and Connected, WHEN `RequestTransition(Informant, Disconnect)` is called, THEN the result is `Rejected_NotDisconnected` and no event fires. GIVEN Informant and Connected, WHEN the same request is made, THEN `NoOp_AlreadyInState` (R8 precedence, F6).
- **AC-PSR-16** [Core] `StealthGame.Roster.Transition.FirstReasonWins` (each of the 4 reasons): GIVEN an Informant whose reason was `Assassinated`, WHEN any entering reason is requested, THEN `NoOp_AlreadyInState`, the reason is still `Assassinated`, and `EventSeq` is unchanged (R8).
- **AC-PSR-17** [Core] `StealthGame.Roster.Transition.NoRevivalNoLateEvents`: GIVEN an Informant or a RoundEnded participant, WHEN →Active is requested with any non-batch reason, THEN `Rejected_IllegalReason`. WHEN RoundEnded→Informant is requested with any entering reason, THEN `Rejected_IllegalTransition`. In both cases nothing changes and no event fires (table).
- **AC-PSR-18** [Core] `StealthGame.Roster.Transition.UnknownIdRejected`: GIVEN an id that was never registered, and an id that was retired, WHEN each Id-taking API is called, THEN `Rejected_UnknownParticipant` and nothing changes (table).
- **AC-PSR-19** [Core] `StealthGame.Roster.Transition.StaleEpochRejected`: GIVEN `ExpectedEpoch = N`, WHEN `ResetAll` runs and then `RequestTransition` or `BindPawn` is called with N, THEN `Rejected_StaleEpoch` and nothing changes. GIVEN `ExpectedEpoch = (M1, 1)` captured in one match, WHEN `BeginMatch` and one `ResetAll` run and the call is made, THEN `Rejected_StaleEpoch`, because the `MatchId` differs (R8, R9, F6).
- **AC-PSR-20** [Core] `StealthGame.Roster.Transition.ExhaustiveMatrix`: GIVEN every combination of from-state × to-state × reason × epoch match/mismatch × connection × generation match/mismatch, WHEN requested, THEN each result equals the expected value in a checked-in CSV fixture (`RosterTransitionMatrix.csv`, one row per combination, hand-derived from R8). The test also asserts the fixture's row count, and every cell outside the table's Applied/NoOp rows is a rejection (table, R8).
- **AC-PSR-21** [Core] `StealthGame.Roster.InformantKillIsNotATransition`: GIVEN an Informant with a live pawn, WHEN `RetireBinding(pawn)` is called, THEN the state stays `Informant`, no state event fires, and `AwaitingPawn(T)` is +1. Calling `RetireBinding` again returns `NoOp_AlreadyRetired` (table, F2, R9).

**Binding and queries (R9, R10)**
- **AC-PSR-22** [Core] `StealthGame.Roster.BindPawnGuards`: GIVEN, in turn, a participant that already has a live pawn, a pawn that is BoundLive or Retired, and a RoundEnded participant, WHEN `BindPawn` is called, THEN `Rejected_AlreadyBound` / `Rejected_PawnInUse` / `Rejected_WrongState` and the binding table is unchanged (R9, I3).
- **AC-PSR-23** [Int] `StealthGame.Roster.PlayerPawnBoundBeforeVisible`: GIVEN the Shared Character spawn path, WHEN 10 player pawns spawn and one bind is forced to fail, THEN at each player pawn's `BeginPlay` `IsPawnBound` = true, and the failed one is destroyed before `FinishSpawning` (R9).
- **AC-PSR-24** [Core] `StealthGame.Roster.TombstoneGenerationAndRelease`: GIVEN a Retired handle, WHEN the same object returns with a new generation, THEN `Npc`. WHEN `ReleaseToCrowd` is called on a Disconnected participant's BoundLive or Retired pawn, THEN `Npc` and `IsPawnBound` = false. WHEN `ReleaseToCrowd` is called on a Connected participant's pawn, THEN `Rejected_NotDisconnected` (R9, I5).
- **AC-PSR-25** [Core] `StealthGame.Roster.ClearBindingWhileLive`: GIVEN an Active participant's BoundLive pawn, WHEN `ClearBinding(pawn, Destroyed)` is called during phase `Active`, THEN the fault sink records one fault, `GetPawn(Id)` = null, the state is unchanged, and `GetUnboundActiveParticipants` contains Id. WHEN instead `ClearBinding(pawn, EndPlayInEditor / LevelTransition)` is called, or `InvalidateAllBindings` runs, THEN no fault is recorded and no event fires (R9, I4).
- **AC-PSR-26** [Core] `StealthGame.Roster.ResolveVictimKindMatrix`: GIVEN pawns that are BoundLive+Active, BoundLive+Informant, Unbound, Retired, destroyed, pending-kill, null, BoundLive with a RoundEnded participant, and Retired with a RoundEnded participant, WHEN `ResolveVictimKind` is called, THEN `Player` / `Informant` / `Npc` / `AlreadyResolved` / `Stale` / `Stale` / `Stale` / `RoundOver` / `AlreadyResolved`, and no `NotTargetable` input ever yields `Npc` (R10 evaluation order).
- **AC-PSR-27** [Core] `StealthGame.Roster.GetRelationMatrix`: GIVEN viewer pawn A (team 0) and the subjects (A, A), (A, teammate B), (A, opponent C), (A, Retired teammate D), (A, Retired opponent E), (A, informant teammate F), (A, NPC N), (A, null), and the reversed pairs, WHEN `GetRelation` is called, THEN `Self` / `Teammate` / `Opponent` / `Teammate` / `Opponent` / `Teammate` / `NotBothPlayers` / `NotBothPlayers`, and each reversed pair gives the same result (R10).
- **AC-PSR-28** [Core] `StealthGame.Roster.QueryHotPathBudget`: GIVEN 10 participants and 200 pawn handles, WHEN 10,000 calls each are made to `ResolveVictimKind`, `IsPawnBound` and `GetRelation`, THEN a counting allocator records 0 allocations, and an instrumented probe counter shows at most 2 hash-table probes per call at both 200 and 2,000 handles (R10, "O(1), allocation-free"). A wall-clock timing is recorded as advisory only.

**Formulas (F1–F7)**
- **AC-PSR-29** [Core] `StealthGame.Roster.CountsAndConservation`: GIVEN a fixed scripted 200-step sequence (register, assign, transition, bind, retire, disconnect, reset, end round), WHEN each step is applied, THEN:
  - F1–F3 equal a brute-force recount;
  - I1–I3 hold and `AwaitingPawn` ≤ `InformantCount`;
  - a Disconnected Active participant counts in F1;
  - the F1 and F2 worked examples reproduce exactly (3; 2 and 1).
- **AC-PSR-30** [Int][Dep:Round] `StealthGame.Roster.EliminationTriggerFromPayload` (with Round): GIVEN team 0 has one Active (A), WHEN A is reversed, THEN the payload's `ActiveCountAfter` = [0, 3] and Round declares `EliminationWin(1)` exactly once. GIVEN `EndRoundAll` or `ResetAll` drives both counts to 0, THEN no win is declared (F4).
- **AC-PSR-31** [Int][Dep:Round][Dep:Assassination] `StealthGame.Roster.NoDrawSameTick` (with Round): GIVEN the last Active on each team attacks the other in the same tick, WHEN the queue resolves, THEN exactly one `EliminationWin` is latched, for the first in queue order, and the second attack has no effect (F4).
- **AC-PSR-32** [Int][Dep:Round] `StealthGame.Roster.DisconnectGraceTiming` (Round, injected ms clock):
  - GIVEN a drop at 100 000 ms during Active, WHEN the player rejoins at 129 900 ms, THEN they are Active.
  - WHEN there is no rejoin, THEN at 130 000 ms the transition is `Applied`, `ReleaseToCrowd` runs in the same call stack, and the pawn resolves `Npc`.
  - WHEN the drop is before `ActivePhaseStart`, THEN expiry = `ActivePhaseStartMs + 30 000`.
  - WHEN the player rejoins at 129 900 ms and drops again at 129 950 ms, THEN the first timer's request returns `Rejected_StaleConnection` and the state stays Active until 159 950 ms (F6).
- **AC-PSR-33** [Core] `StealthGame.Roster.TeamViewSerializedSize`: GIVEN a C-TeamView with 4 teammates and every field at its maximum value, WHEN it is net-serialized through the struct's serializer, THEN it is ≤ 96 B (F7; raised from 64 B for the fields added by user decision 2026-10-25). The ≤ 1 % budget share is checked by a manual `stat net` profile in a [MP-9][S6] 5v5 session and recorded as advisory evidence.

**Edge cases and cross-system**
- **AC-PSR-34** [Core] `StealthGame.Roster.ReentrantListenerOrdering`: GIVEN a listener that calls `RequestTransition`, WHEN the first transition applies, THEN the nested call returns an accurate result against live state, and events dispatch FIFO after the outermost call with `EventSeq` strictly +1 across all event types. Each payload's counts are its own snapshot (R12, I6).
- **AC-PSR-35** [Int][Dep:Assassination][Dep:NPC-Kill Penalty] `StealthGame.Roster.SimultaneousResolution` (with Assassination and NPC-Kill Penalty), three cases:
  - (a) two assassins hit one pawn in the same tick: 1 transition, 0 strikes;
  - (b) the 3rd NPC kill is queued before the player's own assassination: `StrikeLimit`, then `AlreadyResolved`;
  - (c) the assassination is queued before the 3rd NPC kill: `Assassinated`, the NPC attack is voided, and the NPC lives (Edge Cases; Player Fantasy anchor 4).
- **AC-PSR-36** [Core] `StealthGame.Roster.RoundBoundaryLateEvents`:
  - GIVEN a killed informant with a pending respawn, WHEN `EndRoundAll` resolves first, THEN `BindPawn` returns `Rejected_WrongState`.
  - WHEN instead `ResetAll` happens and then the stale respawn fires with its old epoch, THEN `Rejected_StaleEpoch`.
  - GIVEN `EndRoundAll` has run, WHEN a queued `RequestTransition(victim, Informant, Assassinated)` arrives, THEN `Rejected_IllegalTransition` (Edge Cases).

**Network and Pillar-1 leak**
- **AC-PSR-37** [PIE-3][UI-ev] `StealthGame.Roster.PIE.PublicListRevealsNothing`: GIVEN 3 clients plus the host, WHEN a team-1 client dumps the player-list widget tree mid-round, before and after an elimination and before and after a team-0 disconnect, THEN:
  - all four dumps are identical;
  - they contain only name text widgets in invariant-culture ordinal order;
  - they have no team, state, colour or count property, and the entry count is unchanged.

  Screenshots are kept as evidence (Player Fantasy anchor 1; R6).
- **AC-PSR-38** [PIE-3][S2] `StealthGame.Roster.PIE.EliminationLeakDiff`: GIVEN an instrumented build that logs every replicated `(actor class, property)` update each client receives, WHEN a team-0 player is assassinated, THEN:
  - the server state is `Informant` in the same server frame;
  - the teammate's C-TeamView shows `Informant` within 250 ms at the fixed 100 ms lag (the Game Feel target);
  - on the opponent client, the `(class, property)` updates received in the 2 000 ms after the kill are a subset of a checked-in allowlist (pawn movement and animation properties only), with zero `PlayerState` and zero `PlayerController` updates;
  - the same holds when the victim is an NPC, and when the victim is an informant (control cases). The three received sets have identical `(class, property)` membership (Player Fantasy anchor 2; R6, R10 informant-kill invariant).
- **AC-PSR-39** [PIE-3][S1] `StealthGame.Roster.PIE.PawnPointerDump`: GIVEN a fixed-seed layout with all 4 player pawns and 20 NPC pawns placed inside every client's cull radius, and a modified client that dumps `PlayerState`, `Controller` and `Owner` for every pawn it receives (resolved value and raw received bunch), plus every PlayerController and teammate entry it receives, WHEN it dumps after settling and again after each scripted event (spawn, kill, informant adoption, disconnect hand-over, rejoin), THEN:
  - the received pawn count equals 24 at every dump;
  - all three pointers are null on every non-owned pawn, player and NPC alike, and the raw bunch contents are identical in kind (no unmappable NetGUID for player pawns only);
  - the client receives no PlayerController but its own;
  - its C-TeamView lists only its own team (R6; retires the prototype `IsNpc()` leak).
- **AC-PSR-40a** [MP-3][S3] `StealthGame.Roster.MP.TravelPersistence`: GIVEN a match across one seamless and one hard travel, WHEN the new map's first round tick runs, THEN `ParticipantId`, `PublicHandle`, `Team`, `IdentityKey`, `AppearanceHistory`, `MatchId` and `RoundEpoch` equal their pre-travel values, `InvalidateAllBindings` ran once at travel start, and every client's C-TeamView content equals the server's before that tick (R4, R6).
- **AC-PSR-40b** [MP-3][S4][S8] `StealthGame.Roster.MP.RejoinWithinGrace`: GIVEN an Active client that drops within grace, WHEN it rejoins, THEN:
  - it gets `Reclaimed` with the same id, handle, team and state, with exactly one `OnParticipantReclaimed`, also when two sockets present the same key;
  - it repossesses the same pawn;
  - during grace the pawn is never destroyed, and its longest idle interval is ≤ `CrowdMaxIdleSec` (Crowd key, provisional);
  - an Informant who rejoins is still an Informant and receives an adopted crowd pawn (Player Fantasy anchor 3).
- **AC-PSR-40c** [MP-3][S5] `StealthGame.Roster.MP.HostLoss`: GIVEN a running round, WHEN the host process is killed, THEN on every client `ClientRosterStore.Num() == 0` and no Roster query returns a value other than the not-authority result.
- **AC-PSR-40d** [Int] `StealthGame.Roster.TeamViewPawnRefOrdering`: GIVEN a test hook that forces a teammate pawn to arrive before, and then after, its C-TeamView entry, WHEN both orders run, THEN `PawnRef` resolves once the pawn is relevant in both and nothing crashes (R11, Edge Cases).

- **AC-PSR-41** [PIE-3][S2] `StealthGame.Roster.PIE.OpponentCountsNeverReplicated`: GIVEN 3 clients plus the host in a 2v2 under emulation, with an instrumented replication log, WHEN opponents are eliminated one by one until one Active opponent remains, THEN:
  - before `Ended`, no client receives any property or RPC whose value changes with the opponent `ActiveCount`, `InformantCount` or `AwaitingPawn`;
  - no HUD widget shows an opponent count, which is checked by a [UI-ev] screenshot at "one opponent left";
  - after `Ended`, the post-round reveal may show the counts (R6 opponent-count rule).
- **AC-PSR-42** [PIE-2][UI-ev] `StealthGame.Roster.PIE.HostViewEqualsClientView`: GIVEN the listen-server host and one client on the same team, with a fixed camera transform and TAA off, WHEN each serializes its presentation model (marker set with positions, prompt list, HUD widget tree) while looking at the same group of opponent and NPC pawns, THEN:
  - the two serialized models are equal apart from own-body entries (screenshots kept as evidence);
  - an instrumentation hook records **zero** calls from presentation code to Roster server queries or pawn `PlayerState`/`Controller`/`Owner` reads on the host;
  - a build-time include check (`StealthGame.Roster.PresentationIncludeCheck`) fails if any UI/HUD/marker/camera/audio-cue module includes the server Roster query header (R6 presentation rule).
- **AC-PSR-43** [Int][Dep:Crowd][Dep:Shared Character][S8][UI-ev] `StealthGame.Roster.DisconnectTakeoverBlend`: GIVEN an Active player sprinting mid-interaction, at a fixed step of 1/60 s, WHEN their connection drops, THEN:
  - the AI takeover never decelerates faster than `CrowdStopDecel` (`max|dv/dt| <= CrowdStopDecel`);
  - the active montage after the cancel is the ordinary interaction-cancel asset;
  - aim and head pitch change no faster than the crowd gaze catalogue's maximum turn rate;
  - the pawn is never destroyed.

  It needs a retained screenshot sequence and lead sign-off (R9 hand-over hook, R11 takeover rule).
- **AC-PSR-44** [Int] `StealthGame.Roster.PlayerStateReplicatedAllowlist`: GIVEN the game PlayerState class, WHEN its lifetime replicated properties are enumerated by reflection after the replicated-props override, THEN:
  - the actively replicated set is exactly {`PublicHandle`, `DisplayName`};
  - every engine-default property in R6's list is disabled or flagged held-constant;
  - a scripted disconnect, rejoin and elimination change no allowlisted or held-constant value (R6 closed allowlist, Session rules 22/24).
- **AC-PSR-45** [PIE-3][S1][Dep:Shared Character] `StealthGame.Roster.PIE.PawnReplicationParity`: GIVEN 2 player pawns and 2 NPC pawns in view of an opponent client for 60 s of scripted identical motion and look, WHEN the client logs each pawn's received properties, subobjects, multicast RPCs, updates per second, bytes per second, dormancy transitions and `RemoteViewPitch` samples, THEN:
  - the player and NPC pawns show identical property, subobject and RPC sets;
  - their update rates and byte rates are within 10 % of each other;
  - none of them goes dormant;
  - their pitch distributions are drawn from the same look path.

  Binding, transition or possession changes none of the replication settings (R6 parity rule).
- **AC-PSR-46** [Core] `StealthGame.Roster.ConnectionProjection`: GIVEN each Session `ConnectionState` in R11's mapping table, WHEN `NotifyConnectionChanged` delivers its projection, THEN:
  - only `Connection`, `DisconnectedAt` and `ConnectionGeneration` change;
  - `ConnectionGeneration` increases by 1 only on entry to `Disconnected`;
  - the lifecycle state is unchanged;
  - exactly one `OnConnectionChanged` fires, consuming one `EventSeq`;
  - a Lobby departure sets `Dormant`, the record keeps its id, and `Rejected_RosterFull` ignores it (R4, R11).
- **AC-PSR-47** [Core] `StealthGame.Roster.ReclaimCore`: GIVEN an Active participant (Disconnected, pawn BoundLive) and an Informant (Disconnected, awaiting pawn), WHEN each key registers again, THEN:
  - each gets `Reclaimed` with the same id, handle, team, state and `AppearanceHistory`, and `Connection = Connected`;
  - one `OnParticipantReclaimed` fires for each;
  - the Active participant's pawn is unchanged;
  - the Informant's `AwaitingPawn` is still counted until `BindPawn` adopts an Unbound pawn.

  GIVEN the same key registers twice in one frame, THEN exactly one `Reclaimed` event fires (R11).
- **AC-PSR-48** [Core] `StealthGame.Roster.InformantDisconnectReleaseReclaim`: GIVEN an Informant with a live pawn, WHEN Session notifies `Disconnected` and Round calls `ReleaseToCrowd`, THEN:
  - the state stays Informant;
  - `AwaitingPawn(T)` rises by 1 and `GetPawn(Id)` = null;
  - the released pawn resolves `Npc`.

  WHEN the participant then reclaims and `BindPawn` adopts a different Unbound pawn, THEN `AwaitingPawn(T)` falls by 1 and that pawn resolves `Informant` (R9, R11, F2).
- **AC-PSR-49** [Int] `StealthGame.Roster.TeamViewRepushOnControllerSwap`: GIVEN a client's PlayerController is replaced (a seamless-travel swap simulated in an automation world) and, separately, a reclaim occurs, WHEN the server processes it, THEN the new controller's C-TeamView equals the server's projection within 1 server frame, and it lists only the owner's team (R6, R11).
- **AC-PSR-50** [Core] `StealthGame.Roster.EliminationTriggerCore`: GIVEN payloads from the Roster core, WHEN F4's trigger is evaluated in phase `Active`, THEN:
  - (a) the last Active participant's `StrikeLimit` with the opponent's `ActiveCount` 3 gives a win for the opponent;
  - (b) the same with Reason `Disconnect` gives no win;
  - (c) a 1v0 strike-out gives no win, because the winner side is empty;
  - (d) two same-tick events driving both teams to 0 give a win for the first event only, because the second event's opposing `ActiveCountAfter` is 0;
  - (e) `IsTeamEliminated` is false for both teams after `EndRoundAll` (F4).
- **AC-PSR-51** [Core] `StealthGame.Roster.LegalityClassAndAdoption`: GIVEN Unbound, BoundLive-Active, BoundLive-Informant and Retired pawns, WHEN `GetLegalityClass` is called, THEN it returns `Active` / `Active` / `Informant` / the participant's state. GIVEN an awaiting informant and an Unbound pawn, WHEN `BindPawn` adopts it, THEN `Applied`, and no fresh-spawn path is involved (R9, R10).
- **AC-PSR-52** [PIE-3][Dep:NPC-Kill Penalty][Dep:Assassination][UI-ev] `StealthGame.Roster.PIE.InformantKillLooksLikeNpcKill`: GIVEN an attacker who kills an informant and, as a control, an NPC, WHEN the attacker client's received RPCs, HUD widget tree and played cue ids are logged, THEN the two logs are equal apart from the victim actor id (R10 informant-kill invariant).

- **AC-PSR-53** [PIE-3][Dep:Session] `StealthGame.Roster.PIE.TeammateReconnectingFlag` (R6, user decision 2026-10-25): GIVEN a 2v2 Active round, WHEN a team-0 player disconnects, rejoins inside `DisconnectGraceSec`, disconnects again and lets the grace expire, THEN its team-0 ally's C-TeamView entry shows `bReconnecting` true from the update after Session's `Disconnected` projection until the reclaim, false after the reclaim, true again on the second disconnect and false at grace expiry; the flag is false in `Preparing`, `Ended` and after `OnRosterReset`; team-1 clients receive no byte for it (their replicated-property logs equal a control run); the entry carries no time or reason field.

**Review-only rules** (code review, no runtime test): no other system stores a copy of team or state (R1); callers do not cache pawn answers across frames (R10); no client-side code derives player-ness from pawn pointers (R6); only the named spawn site spawns a player pawn (R9); every session exit path calls `EndMatch` (R6). **Owned by other GDDs** (named here so they are not lost):
- defence and concealment cancel after `ReleaseToCrowd` — Assassination, Concealment;
- objectives refuse non-`Active` players — Objectives Chain;
- concealment and strikes reset on `OnRosterReset` — Concealment, NPC-Kill Penalty & Informants;
- player-facing text follows the Player Fantasy language rule — HUD, Event Feedback & Cues.

## Open Questions

| # | Question / change request | Owner | Target |
|---|---|---|---|
| OQ0 | **Spike list** (referenced by the Acceptance Criteria):<br>**S1** pawn-pointer suppression with a modified-client dump.<br>**S2** PlayerState default-field trimming and the 3-client PIE leak diff under net emulation (PktLag 100, PktLoss 2 %), legacy driver and Iris.<br>**S3** seamless travel and hard travel, multi-process standalone (seamless travel in PIE may not be representative).<br>**S4** reconnect: null-subsystem keys, two clients on one machine, Roster reclaim versus the engine's inactive PlayerState.<br>**S5** listen-host loss.<br>**S6** `stat net` bandwidth.<br>**S7** Iris repeat of S1 and S2, if Iris is adopted.<br>**S8** disconnect hand-over: override `APlayerController::PawnLeavingGame` so that the pawn survives controller destruction and is possessed by a crowd AI controller in the same frame, with no visible pointer or pitch discontinuity on non-owners. S1 must inspect raw bunches, not only resolved pointers. | ue-replication-specialist | Before the MVP-core checkpoint |
| OQ1 | **Spike S1:** can `APawn::PlayerState`, `APawn::Controller` and `AActor::Owner` be made owner-only, with player and NPC pawns indistinguishable on non-owners and no unresolved-NetGUID ordering tell, without an engine change? Under Iris as well, if adopted. Until it passes, the P1 network claim is **provisional pending spike**. | ue-replication-specialist / technical-director (ADR-001) | Before the MVP-core checkpoint |
| OQ2 | Legacy replication driver or Iris? The C-TeamView design works on both; the PlayerState field trimming and pointer conditions are unverified under Iris ([NS] enablement path). | technical-director (ADR) | ADR-001 |
| OQ3 | **Answered by ADR-0005 (2026-10-24 ADR sync)**: game-instance store with explicit `BeginMatch`/`EndMatch`; `AGameModeBase` with no inactive-PlayerState reuse; the single spawn site is `UStealthPawnFactory` (ADR-0010 §3). Original question: store location: a GameInstance-level subsystem with an explicit `BeginMatch`/`EndMatch`, with `EndMatch` on every session exit path. Keep the game mode on `AGameModeBase` (no inactive-PlayerState reuse) or override `AGameMode::AddInactivePlayer`/`FindInactivePlayer`. Name the single player-pawn spawn site that replaces `SpawnDefaultPawnAtTransform`. | lead-programmer (ADR) | Architecture |
| OQ4 | **Answered by ADR-0005 (2026-10-24 ADR sync)**: seamless travel, with hard travel as the fallback. Original question: the prototype sets `bUseSeamlessTravel = false`. Session & Travel must decide; the Roster works for both, because hard travel is treated as a mass reclaim. | Session & Travel GDD | Session GDD |
| OQ5 | **Answered (reconciliation 2026-10-24)** by round-match-framework.md R5, R15, R17, R18 (grace `G_eff`, R15 forfeit/void partition, own-card I4 respawn, `ShouldAdmitExpired`); Roster text aligned (R5, R7, R11, F6, Edge Cases). Original request: **Change request, Round & Match Framework:** adopt `DisconnectGraceSec` (provisional 30 s, range 10–120, `<= RejoinWindowSec`) and own `TeamSize`. Disconnect policy: hand-over at Session `Unresponsive` (provisional), grace from `Disconnected`, then `Active -> Informant (Disconnect)` with `ReleaseToCrowd` in the same call stack. Capture `ExpectedEpoch (MatchId, RoundEpoch)` and `ExpectedConnectionGeneration` on timers. Do not evaluate F4 on batch events. A disconnect or kick never yields `EliminationWin`. Define forfeit/void outcomes with reason codes for a team at 0 Active without a trigger, for both teams at 0, and for an all-disconnected round. Make `ShouldAdmitExpired` return true for every `InRound` record. Optionally add a per-round `DisconnectGraceBudgetSec` against flapping. Own I4 recovery (adopt a crowd pawn within one frame). Call order: `SetTeamsLocked(true)`, then `ResetAll(RoundIndex)`, then `RequestRoundTravel`. Call `ResetAll` once per round, before `RequestRoundTravel`. Never replicate or present opponent counts before `Ended` (R6). Answer the grace-period Pillar 4 stall, where an AI-held pawn keeps a team "alive" for up to 30 s, with objective pressure, never a reveal. Guarantee that no previous-round player pawn survives `ResetAll`. | Round GDD author | Round GDD |
| OQ6 | **Answered (reconciliation 2026-10-24)** by crowd-npc-behaviour.md: `CrowdMaxIdleSec` = 20 s; `CrowdStopDecel` is a derived constant (SCL run braking, 600 cm/s²), not a key; Roster added to Crowd's index row (soft). Original request: **Change request, Crowd & NPC Behaviour:** support `ReleaseToCrowd` adoption of an abandoned player pawn, and AI driving of a disconnected player's still-bound pawn during grace, with the takeover through the normal stop-and-blend path (AC-PSR-43). Supply the reverse direction: an unseen Unbound crowd pawn handed over for informant adoption (R9), with crowd population kept constant. Define `CrowdMaxIdleSec` and `CrowdStopDecel`. Add Participant State & Roster as a soft dependency in the systems index. | Crowd GDD author / orchestrator | Crowd GDD |
| OQ7 | **Answered (reconciliation 2026-10-24)**: the adopted pawn keeps its own card; the card effect is `TakeNpcCard` (`NpcLive -> BoundLive`, CPA CP-12b, Penalty CR-6); Roster added to Character Pool's index row (soft). Original text: **Closed** (Character Pool & Appearance change request 6): `AppearanceHistory` is log-only and `AppearanceHistoryDepth` defaults to 0. Remaining request: Character Pool must define what an informant's card becomes when the informant **adopts a crowd pawn** (R9). The CPA state table currently assumes a same-card respawn. Add the Roster as a soft dependency in the index. | Character Pool GDD author / orchestrator | Pool GDD revision |
| OQ8 | **Answered (reconciliation 2026-10-24)** by IVP R8 (team-only fallback position feed) plus Teammate Markers TM-R4. Original question: **Change request, Teammate Markers / Visibility Policy:** a teammate beyond net-cull distance has an unresolved `PawnRef`. Do markers need a team-only position feed, and is that a Team-audience `Position` fact? | Teammate Markers GDD | Markers GDD |
| OQ9 | **Answered (reconciliation 2026-10-24)** by Assassination ASN-R2 (attacker state read at resolution) and ASN-R19/F4 (a `NotTargetable` body gives `NoContact`, identical for NPC and player bodies). Original request: **Change request, Assassination:** confirm that attacker state is checked at *resolution*, and that the response to a `NotTargetable` target gives the attacker no feedback that differs from an NPC kill in a way that reveals player-ness. | Assassination GDD author | Assassination GDD |
| OQ10 | **Change request, Shared Character & Locomotion:** deferred spawn, bind, finish spawn for all player pawns; NPC and player pawns must share identical net settings. | Shared Character GDD | Shared Character GDD |
| OQ11 | Is the public player list shown in-round at all? Names only, alphabetical, is the Roster's maximum. | ux-designer (Lobby UI / HUD) | VS |
| OQ13 | **Change request, Session & Travel:** a server-issued rejoin secret replaces the client-asserted `IdentityKey` as proof of identity. This is a hard gate before the first playtest outside the team (creative-director ruling; Session currently defers it to Alpha). Until then every MVP build documents the key as unauthenticated. | Session GDD author / security-engineer | Before the first external playtest |
| OQ14 | **Adopted by Session & Travel (2026-10-24 review; confirmed at reconciliation)**. Original request: **Change request, Session & Travel:** adopt the Roster API names (R4 mapping table). Drop the mapping of `Expired` to Roster `Retired`, which is a pawn tombstone (R11). Publish the `ConnectionState`-to-`Connection` projection (R11). Count capacity on non-Dormant records only, which matches F1 `Occupied`. Call `InvalidateAllBindings` at `OnTravelStarting` and `RebindView` on seamless arrival. Update its cross-reference to the Roster, which no longer has only its Overview and Player Fantasy. | Session GDD author / orchestrator | Session GDD revision |
| OQ15 | **Done (reconciliation 2026-10-24)**: TDC registers `DisconnectGraceSec` under Round (hard 3-120, superseding 10-120); the systems index lists the Roster for IIF and LSC. Original requests: **Change requests, other GDDs:**<br>**Tuning Data & Config:** `TeamSize` owner is Round (confirmed). Register `DisconnectGraceSec` under Round with range 10–120. Allow a test preset with `TeamSize` 1.<br>**Information Visibility Policy:** own the closed PlayerState allowlist (AC-PSR-44), the frozen name-list leak test and the pawn parity rule (AC-PSR-45).<br>**Assassination & NPC-Kill Penalty:** the informant-kill invariant (R10, AC-PSR-52). Strike carry-over across reclaim, keyed by `ParticipantId`.<br>**Shared Character & Locomotion:** the single spawn site, crowd adoption, the `PawnLeavingGame` override (S8), and replication parity including `RemoteViewPitch`.<br>**Teammate Markers:** C-TeamView no longer carries teammate connection state.<br>**Systems index:** add Participant State & Roster as a dependency of Input & Interaction Framework and Level Socket Contract (both GDDs already name it). | respective GDD authors / orchestrator | Next revision of each |
| OQ12 | Prototype migration: keep `AStealthGameMode` on `AGameModeBase`. Move `Team`/`LifeState` off `AStealthPlayerState`'s all-client replication. Add `RoundEnded` to `EStealthLifeState`. Bind at `PossessedBy` / deferred spawn. Retire in `EliminatePlayer`. Split client-side `IsNpc()`/`GetTeam()` into server roster queries and a client viewer model. | gameplay-programmer | First Roster story |
