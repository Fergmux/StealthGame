# Session & Travel

> **Status**: Reviewed - NEEDS REVISION 2026-10-24 (re-review pending); reconciled 2026-10-24; ADR-synced 2026-10-24
> **Author**: design-system pipeline (autonomous) + network-programmer, ue-replication-specialist, systems-designer, creative-director, qa-lead
> **Last Updated**: 2026-10-24
> **Last Verified**: 2026-10-24
> **Creative Director Review (CD-GDD-ALIGN)**: REVISED 2026-10-24 (CONCERNS, 8 items; items 1-7 applied in rules 3, 8, 11.8, 25f-g, Visual/Audio, Game Feel, AC 23/46-48; item 8 recorded as Open Question 7 + pending user decision, scope)
> **Design Review**: NEEDS REVISION 2026-10-24 (40 revisions applied)
> **Implements Pillar**: P1 Hide in Plain Sight (travel/join leak surfaces); P2 Every Advantage Is a Tell (fresh city per round, simultaneous reveal); P3 One Strike, Lasting Consequence (sealed rounds, reduced role over removal); P4 A Race, Not a Hunt (a round starts and survives disconnects without stalling); MDA Fellowship (the same group across an evening)

## Summary

Session & Travel gets ten players into one listen-server session by direct address. It moves them all, together, into each round's seeded city with one seamless travel (`?Seed=`). It also detects disconnects and matches a returning player back to their own record. It reports these facts to Round & Match, which decides their consequences. It exists so the group stays together across an evening of rounds, every client builds the same city from the same seed and plays by the same tuning (each client recomputes and reports its own tuning hash), and no join, leave, lag or reconnect ever shows an opponent which body in the crowd belongs to a person (Pillar 1).

> **Quick reference** — Layer: `Core` · Priority: `MVP` · Key deps: `Participant State & Roster (upstream); Tuning Data & Config (soft); Round & Match Framework, Lobby UI, Dedicated Server, Matchmaking (downstream)`

## Overview

Session & Travel is the server-side system that gets ten people into one listen-server session and keeps them together across a 3-5 round evening. It owns six things, and only these. (1) **Direct join**: a host opens a session that is also their own game client, and others join by typing an address (`ip[:port]`). The MVP has no matchmaking, no session browser and no NAT relay. (2) **Join gates**: build and tuning compatibility, capacity (`2 x TeamSize`, so 10 by default and 4 for 2v2 tests), and the join-in-progress policy (a new person can join only in `Lobby`; a known person can rejoin in any phase). (3) **Round travel**: when Round asks it to, Session sends everyone to the round's city in one seamless `ServerTravel` and carries the layout seed as a URL option (`?Seed=`). Every client therefore builds the same city and confirms the same seed, layout and tuning (by a hash it recomputes itself) in one acknowledgement before the round starts. (4) **Persistence across travel**: which participant fields carry over for the whole match and which reset every round. (5) **Liveness**: per-connection silence timers, which detect disconnects with tunable thresholds, and reconnect identity matching, which hands a returning person their own record. (6) **Session end**: the host leaving ends the session for everyone, because the MVP has no host migration.

Session decides nothing about gameplay. It reports facts, as `OnParticipantDisconnected`, `OnParticipantRejoined`, `OnRejoinWindowExpired` and `OnParticipantTravelComplete`, and Round & Match decides what those facts mean. Session also never spawns, frees or hides a pawn. Players feel the system only through what it guarantees. The group stays together between rounds without going back to the main menu, every round is played in a city nobody could have learned in advance, and dropping out and coming back is silent to everyone outside your team (Pillar 1: "an enemy player cannot be told apart from an NPC by ... network presentation"). Without this system, each of those guarantees is left to engine defaults. Those defaults broadcast joins, recreate PlayerStates on every travel and keep player lists live, and each of them can show an opponent which body in the crowd belongs to a person.

*Framing choices (autonomous, logged)*: the system is described both as a data layer and through its player effect (Core category, Foundation-style infrastructure). No ADR exists yet, so implementation choices are marked "-> ADR". The fantasy is indirect.

## Player Fantasy

*Framing: indirect. Players feel what the system enables. Shaped by creative-director: framing A "The Same Ten, the Same City" plus one closing line from framing B, the recommended combination.*

**The same ten, the same city.** You came with nine others, and the same nine are there when the next city rises. Nobody is sent back to the main menu between rounds. The streets rebuild themselves and everyone wakes in the same unfamiliar city at the same moment. No one could learn it in advance, so the only advantage left is how well you read people. Once a round begins its company is sealed: no stranger walks in partway through, and every knife in the crowd was there from the first step. Drop out and come back, and the crowd does not flinch.

The anchor moment is round two starting: the plaza you memorised is gone, so you read the crowd instead of the map. A quieter moment matters just as much. Mid-round an opponent's connection drops and comes back, and you see nothing, because there was nothing to see.

Anchor moments, each mapped to an acceptance criterion: (1) **a new city, everyone together**: every client moves to the next round without a menu, builds an identical city (matching seed and layout), and keeps its roster; (2) **a silent drop and return**: a mid-round disconnect and rejoin produces no notice, list change or UI change on any non-teammate client; (3) **a sealed round and an honest ending**: a new connection attempt mid-round is refused, and only the person trying sees the refusal; if the host leaves, every client gets the same plain "session ended" message within a bounded time and is never left frozen or silently handed to a new host.

*Pillars served*: P1 Hide in Plain Sight (in-round silence of joins, leaves and reconnects). P2 Every Advantage Is a Tell (a fresh seeded city per round means map knowledge is not an advantage, so reading behaviour is the advantage). P3 One Strike, Lasting Consequence (a sealed round: no late joiner and no backfill dilutes the consequences already paid; a returning participant is given a reduced role rather than removed, which is Round's policy, rule 8). P4 A Race, Not a Hunt (no disconnect can stall a round: every wait is bounded). *Aesthetic served*: Fellowship. The same group shares one city at a time through an evening of rounds. *Language rule for player-facing text from this system*: never "dead", "lives", "revive", "spectator", "ghost", "tracked", "registered" (Roster rule), and never "respawn", "drop-in", "join-in-progress", "backfill", "host advantage", "host migration" or "new host". In-round text never says "X joined", "X left", "X reconnected", "player count" or "waiting for players". Player-facing text says "city", not "map", "level", "seed", "server" or "travel".

## Detailed Design

*Specialists consulted (full review mode)*: network-programmer, ue-replication-specialist, systems-designer. Where they corrected the first draft, the correction is adopted and noted inline. UE 5.8 API behaviour cited here comes from the specialists' knowledge of UE 5.x and the 5.7-era `docs/engine-reference/unreal/modules/networking.md`. Every engine claim is marked **[verify]** and is to be confirmed against the installed 5.8.4 source or the net-emulation spike. Where a rule depends on the spike, it is **provisional pending spike**.

### Core Rules

**A. Session lifetime and hosting**

1. A **session** begins when a host chooses *Host*. The host's game opens the Lobby city as a listen server on `HostPort` (default 7777). The session ends when the host leaves or the host process stops. A session holds one **match**: consecutive rounds played by the same group. Match-scoped data lives exactly as long as the session.
2. The host is a participant like any other. Its record is created when the session opens. The host's local player never passes through the join handshake (verified in 5.8.4: the listen host's local player goes `Login` → `PostLogin` and never reaches `PreLogin`; ADR-0005 E2), so Session seeds the host record directly with the host's IdentityKey. The host's connection is never `Unresponsive` or `Disconnected`, because it has no network link to lose.
3. **Host parity rule**: the host's local view, meaning its HUD, player list and markers, reads through the same replicated-view and Information Visibility Policy path as a remote client's view. It never reads server-side state directly. This is a design rule and is testable (see Acceptance Criteria). It is not a security property: a host process can read server memory, and only the Dedicated Server tier (Alpha) closes that gap. Known caveat, accepted for the prototype: the host has 0 ms latency, so in a race between near-simultaneous actions the host's action reaches Round's resolution queue up to one client one-way delay earlier, which is about 50 ms at 100 ms RTT. The prototype playtest protocol must record who hosted each round so this can be measured, and it must **rotate the host every session** until Dedicated Server, so the edge is spread across players (anti-pillar: not a reflex game). Round is asked to state whether near-simultaneous resolution should ignore arrival-time differences smaller than one client one-way delay (change request 2).
4. **No host migration (MVP)**. If the host leaves on purpose, the server first sends every client a reliable "session ending" notice, then shuts down. If the host crashes, each client detects the loss itself (F6). Either way every client shows the same message, "The session has ended.", and returns to the main menu. No client is ever made the new host.

**B. Direct join and admission**

5. A joining player enters an address (`ip` or `ip:port`, IPv4 or a hostname). The game connects to it directly. The MVP has no session browser, matchmaking, invite or NAT traversal. Internet play requires the host to forward `HostPort`, which is a documented limitation. Later tiers (Dedicated Server, Matchmaking) replace the address field and keep every admission rule below.
6. **Identity**: every game install has a persistent 128-bit random **IdentityKey**, generated once and stored locally. A launch option `-IdentityKey=` overrides it, and in PIE each instance's key is derived from its instance index. The override exists because several clients on one dev machine would otherwise share a key and supersede each other. The client sends its key and a requested display name as join options. The server never replicates the IdentityKey to any client, including its owner's opponents and teammates. It is also never derived from or exposed as the engine's `PlayerId`, which is sequential and recreated on reconnect. When an online subsystem exists (VS and later), its unique net id replaces the IdentityKey as the match key (-> ADR). The key is a 128-bit secret shared only between a client and the host, and it is never logged in full (logs carry at most a 4-byte prefix). **Rejoin secret (provisional; a hard gate before the first playtest outside the team, per Roster R3)**: on a participant's first admission in the session the server draws a 128-bit `RejoinToken` from server-only OS entropy and sends it to that client only, by a reliable owner-only RPC; the client stores it with the cached address. Any later rejoin or supersede for that key must present the matching token, otherwise it is rejected `Refused` and the live connection is untouched, so a stolen key alone can neither reclaim a record nor knock its owner off. Full anti-cheat hardening (an authenticated platform id) stays with Anti-Cheat (Alpha).
7. **Admission gates** are evaluated in this order on the server at the start of the handshake (PreLogin-equivalent). The first failure rejects the connection with the listed reason code:
   1. engine network-version handshake, with the project `ProtocolVersion` folded into the engine's network version so mismatches fail at the earliest point **[verify]** → `VersionMismatch`;
   2. `TuningSchemaVersion` equality, which is TDC's `SchemaVersion` (TDC F-2 table) → `VersionMismatch`;
   3. a key the host kicked earlier in this session → `Refused` (checked before the admission mode, so a kicked key gets the non-retryable answer even during travel and stops retrying; the kick list is held in memory by IdentityKey in Session's session-scoped server object);
   4. admission mode, which Round pushes as described in rule 20, together with whether the key is known → `InProgress` or `Busy` (the decision table follows);
   5. capacity (F1) → `Full`.
   These checks are for compatibility, not security: a modified client can lie about its versions. Any client-supplied option other than IdentityKey, display name and `RejoinToken` (rule 6) is never read, including team, spectator flags and seeds: the engine cannot strip join options before login, so the server's new-player initialisation reads only those three allowlisted keys and skips the engine's default option parsing (which would read `SpectatorOnly` and the name; ADR-0005 §1, E17). The engine spectator allowance is 0, and spectators are deferred.
8. Admission decision by **admission mode** and whether the key matches a Roster record:

   | Admission mode (set by Round) | Unknown key | Known key, record Dormant | Known key, ConnectionState Disconnected (in window) | Known key, ConnectionState Expired | Known key, live connection |
   |---|---|---|---|---|---|
   | `Open` (Lobby) | admit if capacity | admit if capacity (returns as themselves) | n/a (Lobby has no windows: rule 13) | n/a | supersede (rule 15) |
   | `RejoinOnly` (Preparing, Active, Ended) | reject `InProgress` | reject `InProgress` | admit as **rejoin** (slot already held) | ask Round's policy hook `ShouldAdmitExpired(Participant)`: if true, admit as **rejoin** (the role on return, e.g. informant, is Round's decision); if false, reject `InProgress` | supersede |
   | `Closed` (Reset, or any travel in progress) | reject `Busy` (retryable) | reject `Busy` | reject `Busy` (retryable; the client retries) | reject `Busy` | supersede is deferred until travel completes (reject `Busy`) |

9. **Capacity** is `2 x TeamSize` participants (F1). `PendingLoginTimeoutSec` covers the handshake only, from the first packet to login completion; the map load and acknowledgement that follow are bounded by the travel deadline (F7), the rejoin window (rule 21) or, for a Lobby join, by the silence clock (rules 18-19) once login has completed. That is 10 at the default TeamSize of 5 and 4 for 2v2 tests. A handshake that is accepted but has not yet completed holds a slot. That slot is released if the handshake fails or exceeds `PendingLoginTimeoutSec`. Held rejoin slots count against capacity.
10. **Display names**: a name is trimmed, stripped of control characters and truncated to `DisplayNameCharLimit` characters. A duplicate gets a numeric suffix (" 2", " 3"). The name is fixed for the match. Display names are never used to match identity. This differs from the engine's default inactive-player match by name **[verify]**, under which two players called "Alex" could swap records.

**C. Round travel and the seed**

11. **Round start ordering** (normative):
    1. **Lobby**: Round decides to start. In the order fixed by Round R5 it locks teams, calls the Roster's `ResetAll(RoundIndex)`, enters `Preparing` and pushes `RejoinOnly`, then calls `RequestRoundTravel(MapId, PresetId, RoundIndex)`. `PresetId` is the name held in TDC's server-side preset store (TDC CR-4), which Round reads and passes unchanged; no client option, login option or earlier URL is a source for it.
    2. **Session validates**. It rejects with a reason and changes nothing unless: the last lobby arrival is complete and admission mode is `Open` or `RejoinOnly` (Round pushes `RejoinOnly` for Preparing immediately before this call, Round R5; `Closed` is refused), no travel is in progress, no participant is `Unresponsive` or `Connecting` (retryable; bounds below), and the Roster's `IsRoundScopeReset(RoundIndex)` returns true (Roster R7). Refusal codes, checked in this order: `TravelInProgress`, `AdmissionClosed` (mode `Closed` or lobby arrival not complete), `ParticipantUnresponsive` (retryable), `ParticipantConnecting` (retryable), `RoundScopeNotReset`. On a refusal Round undoes its own steps (Round R5). Session also exposes **`CanRequestRoundTravel()`** (reconciliation 2026-10-24, Round change request 13b): it runs the same checks in the same order and returns the first refusal code or `Ok`, with no side effect, so Round can pre-check a start before it locks teams and resets the Roster. Because Round pushes `RejoinOnly` before the call, no new unknown-key handshake can start after it, so a `ParticipantConnecting` refusal clears within `PendingLoginTimeoutSec` and `ParticipantUnresponsive` within `SessionHardTimeoutSec`; a start can be delayed but never blocked indefinitely.
    3. **Session draws the round Seed** (F5), sets admission mode `Closed`, moves every connected participant to `Traveling`, records the single server time `t_travel` from which every participant's travel deadline is measured (F7; it is never restarted, see 11.7), calls the Roster's `InvalidateAllBindings()`, raises `OnRoundTravelStarted(Params)`, and issues one seamless `ServerTravel` to `MatchMap?Seed=<Seed>?Preset=<PresetId>?Round=<RoundIndex>`. `?Preset=` is a server-side carrier only: at Preparing TDC resolves the preset from its own store, and the store wins if the two disagree (TDC CR-4).
    4. **The new map's server game mode parses the URL options** at initialisation. If `Seed` is missing, duplicated, non-numeric, zero, negative or out of range, it never substitutes a default. It raises `OnTravelFailed(BadParams)`, and Round returns everyone to the Lobby. Otherwise it writes `FRoundParams {MapId, Seed, PresetId, RoundIndex}` into the new game state as **one replicated struct**, with audience `All` and identical bytes for every viewer, so clients never see a partial set. The URL is the server-side carrier across the map boundary. Clients never read the URL: the replicated struct is the only client source **[verify; specialists agree that clients do not consume the travel URL options]**.
    5. **Preparing work on the new map**. Round is already in Preparing (11.1) and calls TDC's `BeginResolve()`. TDC freezes the preset, computes `TuningHash` (all keys, server-only, never replicated) and `ClientHash` (public keys only), and raises `OnTuningReady` (TDC CR-7). The layout is built from `Seed` and reports `OnLayoutReady(LayoutId, Seed)` (Level Socket Contract). If the generator rejects and reseeds, the final `LayoutAttempt` and the `LayoutHash` are written to a second replicated struct, `FRoundReadiness {RoundIndex, LayoutAttempt, LayoutHash}`. **No hash of tuning is replicated**: `FRoundReadiness` carries no `TuningHash` and no `ClientHash`, because each client recomputes `ClientHash` itself from TDC's public snapshot (TDC CR-8, CR-9). Clients cannot replay the server's validation loop, so they never reproduce reseed attempts themselves.
    6. **Round acknowledgement (the single acknowledgement; TDC, the Level Socket Contract and Character Pool & Appearance add no other, and none has its own timeout)**. The acknowledgement is **server-requested** and travels only between the server and the owning client:
       - **Request**. Once the participant's connection is bound in the current epoch (`RebindView` on a seamless arrival, `RegisterParticipant` on a fresh login), the server sends a reliable client RPC on that participant's own PlayerController, `ClientRequestRoundAck(FAckEpoch {RoundIndex, TuningRevision, AckSeq})`. `AckSeq` starts at 0 for each epoch. The request goes to the owner only, so no other client learns that anyone was asked twice (a counter bumped on a replicated struct would tell everyone that someone failed).
       - **Client conditions**. The client replies once all of these hold, in any arrival order: the map is loaded; it holds `FRoundParams` and `FRoundReadiness`; it holds TDC's public snapshot with `bValid` set and the `RoundIndex`/`Revision` named in the request; it has built its geometry from `FLayoutManifest` and recomputed `LayoutHash` locally (Level Socket Contract R4.7); every Character Pool entry is resident and the client has computed its `PoolHash` (Character Pool CP-18; provisional until Character Pool adopts this rule); and its PlayerController exists (after seamless travel it already does). Its "already acknowledged" marker is keyed by `FAckEpoch`, so a PlayerController carried through seamless travel never suppresses the next round's reply.
       - **Reply**. The client recomputes `ClientHash` from the snapshot fields it received (TDC F-2) and sends one reliable server RPC on its own PlayerController, `ServerRoundAck(FAckEpoch, Seed, LayoutAttempt, LocalLayoutHash, ClientHash, PoolHash)`. The server derives the participant from the sending connection (Roster R2), never from an RPC parameter.
       - **Acceptance**. An acknowledgement is judged only if (a) it arrives on the participant's currently bound connection, (b) its `FAckEpoch` equals the outstanding request, and (c) the participant is `Traveling` or `Rejoining`. Anything else (a stale epoch, an old `AckSeq`, a duplicate, an old connection after a supersede) is dropped and logged, at most one log line per second per participant, and never satisfies or fails the current wait. If `Seed`, `LayoutAttempt` and `LocalLayoutHash` equal the server's values, `ClientHash` equals the server's `ClientHash` for that epoch, and `PoolHash` equals the server's, Session moves the participant to `Connected`, calls the Roster's `NotifyConnectionChanged`, and raises `OnParticipantTravelComplete(Participant)` (for a rejoin, `OnParticipantRejoined`). That event is also TDC's tuning-`Ready` (TDC CR-9) and Character Pool's residency confirmation, so Round's readiness barrier waits on this one event (Round R6 P5).
       - **Explicit resend**. A replicated property that has not changed is never sent again, so a resend cannot be "replicate the struct again". On the first mismatch of any field, a `ClientHash` or `PoolHash` mismatch included, Session sends `ClientRequestRoundAck` again with `AckSeq + 1`; the client re-reads its current structs and snapshot, recomputes, and replies once more. If that reply also mismatches, the participant is disconnected with reason **`ParamsMismatch`** (Roster projection `Disconnected`; Round applies its normal disconnect policy) and client auto-rejoin does not run. Session never aborts the round because of client reports: any number of mismatching clients only disconnects themselves (TDC CR-9).
       - **Bookkeeping**. The server's acknowledgement record `{FAckEpoch outstanding, MismatchCount}` is keyed by **`ParticipantId`**, never by connection, PlayerController or PlayerState (the controller and PlayerState are replaced at every seamless travel, rule 16, and the controller is replaced on rejoin). It lives in Session's game-instance-scoped connection table, is cleared at every epoch start (each travel start and each TDC `Revision` change, which re-requests from every participant not yet `Connected`), and is never replicated: a replicated "acknowledged" flag would publish which entries are human (TDC AC-11).
       - **Host**. The listen-server host's participant is acknowledged through the same code path, called locally with no RPC, once its local view meets the same conditions (TDC CR-9).
       - **Order**. Acknowledgements are judged one at a time in the receive phase, so same-tick acknowledgements resolve in `ParticipantId` order; Round's "nobody `Traveling`" check runs after the receive phase.
       The echo is self-reported and catches accidents, not cheaters.
    7. **Absolute deadline**. Every participant in `Traveling` reaches a terminal travel outcome by the F7 `Deadline` = max(`t_travel + TravelLoadTimeoutSec`, `t_ready + TravelAckGraceSec`): travel complete, or `Disconnected(TravelTimeout)`, which opens the normal rejoin window. The deadline is measured on the server from two server events only (travel start and the publication of the readiness data); no client message starts, extends or restarts it, and a disconnect and rejoin during Preparing does not reset it. A participant that leaves `Traveling` for `Disconnected` leaves Round's readiness wait set and becomes a normal disconnect (Round R6 P5), so a `Rejoining` participant can never extend Preparing. A `Rejoining` participant, in any phase, is bounded by its handshake deadline `PendingLoginTimeoutSec` and by the absolute end of the window that opened at its original disconnect (`t_disconnect + RejoinWindowSec`, rule 21); a rejoin or supersede never resets either clock. Round can therefore start Active once layout and tuning are ready and no participant is still `Traveling`, without a timer of its own (F7).
    8. **Simultaneous reveal**: every client, the host included, stays on the neutral loading screen from travel start until Round enters Active. The new city is revealed to all participants in the same server step: Round's entry into Active replicates one reveal flag in its public state, and each client's loading screen drops on that flag, never on its own load completing. A fast loader, or the host, never gets Preparing time to study the city (Pillar 2). A rejoiner sees the loading screen until its acknowledgement completes.
    9. **Fallback travel inside Preparing (provisional; Level Socket Contract mode `GeneratedWithFallback`, its reseed-and-fallback rules)**. If every generated layout attempt fails, Round calls `RequestFallbackTravel(GrayboxMapId)`. Session accepts it only in Preparing with a round travel already complete on the server. It keeps `Seed`, `PresetId` and `RoundIndex`, starts a new acknowledgement epoch (`FAckEpoch` with a new `AckSeq` base), records a new `t_travel`, and issues a second seamless `ServerTravel` to the graybox map with the same URL options. Every rule of 11.3-11.8 applies again, and the round-scoped Round Event Log header is carried by Round's game-instance-scoped store. The F7 deadline of the fallback travel is measured from its own `t_travel`.
12. **Lobby travel**. After a round, Round calls `RequestLobbyTravel()`. This is allowed in Ended and Reset, and in Preparing as a **travel cancel** when the host aborts before the server's round map load completes (Round change request 13g): Session then lets the in-progress round travel finish its server-side load, discards that round's acknowledgement epoch (outstanding requests are dropped, no participant is disconnected for it), and runs the lobby travel below from there; `Seed` stays in `UsedSeeds`. Session forces admission `Closed`, closes any connection that is `Connecting` or `Rejoining` with reason `Busy` (its client retries), moves connected participants to `Traveling`, records `t_travel`, calls `InvalidateAllBindings()`, and travels seamlessly to `LobbyMap`, which carries no seed. **Lobby arrival** uses the same server-requested acknowledgement as rule 11.6 with `FAckEpoch {LobbyEpoch, 0, AckSeq}` and no seed, layout or tuning fields: a reply on the bound connection moves the participant to `Connected`. A participant that misses `t_travel + TravelLoadTimeoutSec` (lobby travel has no readiness data, so the grace term does not apply), or whose connection closes during lobby travel, becomes `Dormant` directly (no window, rule 13). Participants still `Disconnected` or `Expired` become `Dormant`, and their slots are released (rule 13). When every participant is `Connected` or `Dormant`, Session raises `OnLobbyTravelComplete`; it does not open admission itself. Round pushes `Open` when it enters `Lobby` (rule 20), and Session's own `Closed` override is lifted at that event, so the one writer of the mode stays Round. For the bare prototype `LobbyMap` may equal `MatchMap`, in which case travel reloads the same map. Same-map seamless travel through the transition map is **provisional pending spike**. Any non-seamless fallback URL must keep the listen option.
13. **Lobby has no rejoin windows**. A Lobby disconnect immediately releases the slot and leaves the record `Dormant`. The record keeps its match-scoped fields, so the person can return as themselves through normal admission (rule 8, `Open` column). This fulfils the Roster promise that you "return as exactly what you were" across the whole match, while in-round slots are reserved only for `RejoinWindowSec`.
14. **Seed publicity and secret sub-seeds**. `Seed` is public: it is replicated to everyone and appears in URLs and host logs. It may drive only content that every client is allowed to know, which is the layout geometry and the socket set. Every hidden decision is drawn from server-only sub-seeds derived from a 128-bit `ServerSecret` that is generated at session open and never leaves the server (F5). Hidden decisions are: which sockets host the objectives (the final site is hidden until a team's preliminaries are done), appearance deck deals, player placement and emergence order, NPC route and spawn order, and pawn spawn interleaving. Session owns the secret and the derivation API `GetServerSubSeed(Domain, Attempt)`. A modified client that runs the generator locally learns the city and nothing else.

**D. Persistence across travel**

15. **Supersede**: if a connection arrives with the key of a participant whose old connection is still live (`Connected` or `Unresponsive`), the usual cause being a client that crashed and restarted inside the timeout, the new connection takes over and the old one is closed with reason `Superseded`. Order: Session first drops the old connection's connection-to-participant mapping, so an in-flight RPC from the old controller can never resolve to the record (Roster R11); then it closes the old connection; then it binds the new one. The new client holds no snapshot and has never acknowledged, so in-round (`RejoinOnly`) the participant moves to `Rejoining` and must pass the rule 11.6 acknowledgement; in `Open` (Lobby) it passes the lobby acknowledgement of rule 12. A supersede never takes a second slot and skips F1. Session raises only `OnParticipantRejoined(Participant, Superseded = true)`, when that acknowledgement completes. It raises no `OnParticipantDisconnected`, so Round never hands the pawn away and back within one tick (systems-designer correction); Round's hand-over at `OnParticipantUnresponsive`, if it already happened, continues until the rejoin event. A supersede does not restart the rejoin window or any deadline (rule 11.7). A third connection that arrives during the handshake supersedes again; the latest connection wins.
16. **Where records live**: the Roster's participant records and Session's connection table live in a server object scoped to the game instance, which survives map change. PlayerStates, PlayerControllers and pawns hold views only. Nothing authoritative is stored on a PlayerController or a PlayerState, because in UE 5.8.4 the default seamless travel **replaces both** (a new controller and a new PlayerState are spawned in the new world; the old PlayerState copies its properties to the new one and is destroyed; ADR-0005 E5), so any state kept on them would be lost or carried silently. A connection is therefore identified across travel by a **peer key**, not by its controller or PlayerState: the network connection for a remote peer (it moves with the net driver into the new world) and a reserved `Host` key for the listen host's local player, which has no connection; the connection table maps peer key → `ParticipantId`, and mailboxes, acknowledgement records and feature state are keyed by `ParticipantId` (ADR-0005 §2). Binding a connection to its record happens on every arrival path through one Session operation, `BindConnection(PeerKey, PlayerState)`, which calls the Roster's canonical API (Roster R4 mapping): `RegisterParticipant(IdentityKey, DisplayName)` on a fresh login (it returns `Registered` or `Reclaimed`; a reclaim re-associates the retained PlayerState) and `RebindView(Id, NewPlayerState)` on a seamless-travel arrival, which **rebuilds** the new PlayerState's view fields and C-TeamView from the record (no login re-run; the key comes from the peer key, never from the replaced view). Remote players re-bind one by one as they finish loading; the host's local player re-binds when the new world is set up; carried PlayerStates of disconnected participants (no controller) are re-bound to their records on round and fallback travel and dropped on lobby travel (rule 12). The PlayerState replacement happens at the travel boundary for every participant identically, behind the loading screen, and is never caused by a connection event (rule 22); the engine's "entered"/"left" messages it would trigger are suppressed by turning off the PlayerState welcome broadcast (ADR-0005 §1, E10). On a fresh-login rejoin the new controller **adopts** the participant's existing PlayerState instead of spawning one, so the player array never gains or loses an entry on any client (ADR-0005 §4). Session's other names map to the Roster as: round-scope reset `ResetAll(RoundIndex)`, readiness query `IsRoundScopeReset(RoundIndex)`, travel start `InvalidateAllBindings()`, connection change `NotifyConnectionChanged`. *(Rewritten 2026-10-24 from ADR-0005's engine finding: the earlier text said PlayerControllers persist through seamless travel and keyed binding by `IdentityKey`.)*
17. **Field scope table** (the Roster owns the fields and their audiences; this table is the travel contract):

    | Field | Owner | Scope | Across travel | On in-round rejoin |
    |---|---|---|---|---|
    | `ParticipantId` (Roster-assigned, never the engine PlayerId) | Roster | Match | carried | same record |
    | `IdentityKey` (ServerOnly) | Roster | Match | carried | used to match |
    | `DisplayName` | Roster | Match | carried | unchanged |
    | `Team` (audience Team; Round may reassign only in Lobby) | Roster | Match | carried | unchanged |
    | Lifecycle state (`Active`/`Informant`/`RoundEnded`) | Roster | Round | **explicitly reset** by `ResetRoundScope(RoundIndex)`, never assumed reset by travel | unchanged (an informant stays an informant) |
    | Pawn binding (ServerOnly) | Roster | Round | invalidated at every travel start (`OnTravelStarting`), because pawns are destroyed | unchanged: the rejoiner reattaches to the existing pawn |
    | Feature state (strikes, defence cooldown, concealment, objective progress) | owning feature components | Round | reset by the owning feature at round start | unchanged |
    | `ConnectionState`, silence clock, rejoin-window clock, last address (ServerOnly) | Session | Session | carried | updated |
    | Match score and round index | Round | Match | carried by Round | unchanged |

    PlayerStates are recreated at every travel (verified in 5.8.4, ADR-0005 E5). The engine's copy-on-travel step, `CopyProperties`, may carry display fields into the new view. The table, not the copy, is the source of truth: after travel every view is rebuilt from the record. The PlayerState is replaced only at travel boundaries, never because of a disconnect (rule 22).

**E. Liveness, disconnect and rejoin detection**

18. **Silence clock**: for each remote connection the server measures time since any packet was last received (the engine updates its last-receive time on every packet, including acks and keepalives **[verify]**). The clock is evaluated every `LivenessCheckIntervalSec`. Because the engine's last-receive time is wall-clock, the silence clock is Session's own accumulator, not a subtraction of timestamps: every server tick it adds `min(dt, LivenessMaxCountedFrameSec)` and it is zeroed whenever the connection's last-receive time has advanced since the previous tick. A server frame longer than `LivenessMaxCountedFrameSec` therefore adds only that much, so one host hitch cannot mark nine clients unresponsive in the same tick (network-programmer correction). The client's host-loss clock (F6) uses the same accumulator and clamp, and is paused while the client is loading a map. Receives are processed before the liveness evaluation in each tick. **No application heartbeat in MVP**: during a round the server streams the crowd continuously and clients send movement and acks every frame. `SessionHeartbeatIntervalSec` is an ADR fallback, disabled by default (F8 sizes it).
19. **Thresholds**: silence ≥ `SessionSoftTimeoutSec` → `Unresponsive`, and `OnParticipantUnresponsive` is raised. Traffic resuming → `Connected`, and `OnParticipantResponsive` is raised. Silence ≥ `SessionHardTimeoutSec` → Session closes the connection itself and moves to `Disconnected(Timeout)`. A socket close, an explicit quit or a kick moves to `Disconnected` at once with that reason. While `Traveling` the silence clock is suspended and the travel deadline applies instead. The engine's own connection timeout is only a backstop. It must be configured to at least `max(SessionHardTimeoutSec, T_prep_max) + 30 s` (F7; 105 s at defaults), and the engine's initial-connect timeout to at least `PendingLoginTimeoutSec + 10 s`, so neither fires before Session's thresholds. The settings are the net driver's connection and initial-connect timeouts and the unoptimised-build timeout multiplier **[verify the 5.8 names]**; editor and PIE builds may disable timeouts by default, so tests set them explicitly. Session validates the live values against the live knob values at session open and logs an error on a violation (-> ADR and engine config; Session cannot assume the engine suspends its timeout during travel **[verify]**).
20. **Admission mode is pushed by Round**: Round calls `SetAdmissionMode(Open | RejoinOnly | Closed)` on every phase change (Lobby → `Open`; Preparing, Active and Ended → `RejoinOnly`; Reset → `Closed`). Session also forces `Closed` during any travel in progress, which runs from `t_travel` until the server's own map load completes (`T_srvload`); after that the mode Round pushed applies again, so rejoins proceed while clients are still loading. Session never reads Round's phase, which keeps the dependency one-way.
21. **Rejoin window**: an in-round disconnect holds the participant's slot for `RejoinWindowSec`. A matching key inside the window is admitted as a rejoin (rule 8). The rejoiner loads the current map and must pass the round acknowledgement (rule 11.6, with the current epoch and the client's own recomputed `ClientHash`) before it becomes `Connected`. That is the tuning check at rejoin. A rejoin does not restart any deadline: the `Rejoining` participant's acknowledgement must complete before the window that opened at the original disconnect lapses, or it becomes `Expired`. Session then raises `OnParticipantRejoined(Participant, DowntimeSec)`. When the window lapses, Session moves to `Expired` and raises `OnRejoinWindowExpired(Participant)` once. What that means for the round (forfeit, team counts, elimination checks) belongs to Round. Session never calls `RequestTransition`.
22. **Session never spawns, frees, hides or re-creates a pawn or a PlayerState because of a connection event**. Default pawn spawning at login is disabled (verified: the engine's default starting-new-player step spawns a pawn after login **and** after every seamless arrival, so it is overridden to do nothing; ADR-0005 E4). Pawns come only from Round's mixed spawn. A rejoiner reattaches to its existing pawn, so no new pawn appears. The engine's inactive-player mechanism is overridden: while `Disconnected` or `Expired`, the participant's PlayerState stays active and in the replicated player array until the next travel (the game mode derives from the base game mode, which has no inactive-player list, and the PlayerState's deactivation is overridden not to destroy it; ADR-0005 §1, E9-E10). Removing it, or flipping its replicated inactive flag, would be a client-visible, time-stamped disconnect signal (replication-specialist finding).
23. **Client auto-rejoin**: when a client detects that its connection is lost (F6) and the session has not announced its end, it shows "Connection lost. Reconnecting..." and retries the cached host address with the same IdentityKey every `ClientAutoRejoinIntervalSec`. It continues until it is admitted, until it is rejected with a non-retryable reason (`InProgress`, `VersionMismatch`, `Refused`, `Full`), until it was disconnected with `ParamsMismatch`, or until the player cancels. `ParamsMismatch` stops only the automatic retry: a manual Join with the same key inside the window is admitted as a normal rejoin, because the slot is still held. The cached address and key survive a crash because they are written to local storage when the client joins.

**F. Pillar-1 presentation rules in join and travel**

24. **What each phase shows**:

    | Surface | Lobby (`Open`) | Preparing / Active / Ended | Session end |
    |---|---|---|---|
    | Join/leave notices | "Name joined" / "Name left", to everyone (inherent: the lobby knows who is present) | **none, to anyone**. An optional teammate-only notice is a VS HUD item and uses audience Team | "The session has ended." to everyone, simultaneously |
    | Player list | names, `n / capacity`, ready flags; team shown only to teammates (Roster/Visibility) | a **frozen name list** captured when Preparing starts: no counts, no status, no team for non-teammates; entries are never added or removed mid-round | — |
    | Connection data of others (ping, address, connection state, join time) | not shown | not shown and **not replicated** to non-owners | — |
    | Rejection messages | shown only to the rejected client | shown only to the rejected client | — |

25. **Leak-surface checklist owned here and enforced through the Visibility Policy's replicated-property allowlist** (each item has an acceptance criterion):
    - **(a) The player array must not change mid-round.** Rule 22 covers this: no removal and no inactive flag.
    - **(b) Per-player connection fields must not reach other clients.** These are ping (including the compressed ping), sequential player id, start time, inactive, bot, spectator and only-spectator flags, the from-previous-level flag, score, the unique net id (check that a null-subsystem id does not embed the address) and any player-name change after Lobby **[verify the 5.8 field list]**. They are not replicated to non-owners, or are held constant. Every Session RPC (acknowledgement request and reply, resend, rejection, `RejoinToken` delivery, "session ending") goes to the owning connection only.
    - **(c) Engine join/leave broadcasts are suppressed at the server.** Client-side suppression alone is bypassable. In-engine voice and chat are disabled for the MVP, which uses external voice.
    - **(d) A connection event must not change a pawn's replicated shape.** Specifically, a pawn's replicated references to its PlayerState, controller and owner must not differ between player pawns and NPC pawns, and must not change on disconnect or rejoin. A pawn exporting a reference that never resolves on a non-owner client is a perfect player detector. By engine default these references do differ between player and NPC pawns, and an AI hand-over changes the pawn's owner **[verify]**, so the fix needs replication-condition overrides whose form depends on the legacy-versus-Iris ADR (Open Question 3). This item is owned with Shared Character & Locomotion and the Visibility Policy.
    - **(e) Spawn order and network object order are not grouped by player-ness.** Round interleaves pawn spawns in a server-secret shuffled order (rule 14), so player pawns are never the first, last or only late-created actors.
    - **(f) Binding outcome requirement: across any connection event (Unresponsive, Disconnected, Rejoined, Superseded, Expired), the pawn's observable behaviour stays within the Crowd baseline.** It never freezes in a way NPCs do not, vanishes, snaps or changes pose discontinuously. Round and Crowd choose the mechanism. The **recommended** mechanism: Session raises `OnParticipantUnresponsive` at Soft (3 s), Round hands the silent player's pawn to NPC control through the same pawn command interface, and a rejoin hands it back with no visible change. Supporting baseline requirement on Crowd: idle pauses of at least `SessionSoftTimeoutSec + 1 s` must be **common**, not merely possible. Provisional target: at any moment at least 10% of NPCs visible to a viewer are in such a pause, tuned by Crowd and verified by Crowd's AC-CNB-12. With that, a still body is never unusual. This requirement has a blocking acceptance criterion (#46). **Provisional pending spike** and the Round/Crowd GDDs.
    - **(g) Sub-threshold lag (bursts under Soft) must not produce an unnatural stop.** When the server receives no movement input for a player pawn, the pawn decelerates into an NPC-style idle and never stops dead mid-stride. When input resumes, the correction is smoothed with the same rules as NPC pawns, with no visible snap. This is owned jointly with Shared Character & Locomotion (change request 5). **Provisional pending spike** (net emulation, 100 ms / 1%, 1-3 s bursts).

**G. Implementation notes (-> ADR)**

- Use the base game mode class without the built-in match-state machine, so it does not fight Round's phases.
- Choose legacy replication or Iris in an ADR before implementing the leak fixes in rule 25. Iris filters differently, and the specialists recommend legacy for the MVP.
- Engine timeout configuration.
- Online-subsystem identity.

### States and Transitions

Per-participant **`ConnectionState`** is owned by Session and is ServerOnly. It is a separate concept from the Roster's lifecycle state, which only `RequestTransition` writes. Each concept has a single writer. On every change of `ConnectionState`, Session calls the Roster's `NotifyConnectionChanged(Id, Projection, ServerTimeMs)` with the projection of Roster R11 (`Connected`/`Unresponsive`/`Traveling` → `Connected`; `Disconnected`/`Rejoining`/`Expired` → `Disconnected`; `Dormant` → `Dormant`; `Rejected` and `Connecting` for a new key → no call). Same-tick ordering: receives, then liveness evaluation, then window expiry. Events raised in one tick fire in ascending `ParticipantId` order.

| State | Meaning | Holds a slot | Silence clock |
|---|---|---|---|
| `Connecting` | handshake accepted at the start; login not yet complete | yes (pending) | — (`PendingLoginTimeoutSec`) |
| `Connected` | live, playing | yes | running |
| `Unresponsive` | silent ≥ Soft, < Hard | yes | running |
| `Traveling` | loading a new map | yes | suspended (travel deadline) |
| `Disconnected` | in-round loss; rejoin window running | yes | — (window clock) |
| `Rejoining` | known key in handshake, map load and acknowledgement | yes | — (travel deadline) |
| `Expired` | in-round window lapsed (round-scoped; Roster projection `Disconnected`, never the Roster's `Retired` pawn tombstone: Roster R11) | no | — |
| `Dormant` | record kept, no connection (Lobby, or carried over from Expired at Lobby travel) | no | — |
| `Rejected` | terminal for that connection attempt; no record created or changed | no | — |
| `SessionEnded` | terminal for everyone | — | — |

| From | To | Trigger / guard | Event raised (server) |
|---|---|---|---|
| (none) | `Connecting` | handshake passes rule 7 gates 1-2 and admission is permitted (rule 8, F1) | — |
| (none) | `Rejected` | any gate fails | — (client sees reason) |
| `Connecting` | `Connected` | login completes, mode `Open`; record created or Dormant record reused | `OnParticipantJoined` |
| `Connecting` | `Rejected` | handshake fails or exceeds `PendingLoginTimeoutSec` | — (slot released) |
| `Connected` | `Unresponsive` | silence ≥ Soft | `OnParticipantUnresponsive` |
| `Unresponsive` | `Connected` | any packet received | `OnParticipantResponsive` |
| `Connected`, `Unresponsive` | `Traveling` | travel issued (Unresponsive cannot occur at round travel: rule 11.2; it can at lobby travel) | `OnRoundTravelStarted` (once, for all) |
| `Traveling` | `Connected` | acknowledgement accepted (rule 11.6; lobby acknowledgement for lobby travel, rule 12) | `OnParticipantTravelComplete` (round travel); `OnLobbyTravelComplete` once all are terminal (lobby travel) |
| `Traveling` | `Disconnected` | **round travel only**: socket close, travel deadline (F7), or second acknowledgement mismatch (`ParamsMismatch`) | `OnParticipantDisconnected(reason)` |
| `Traveling` | `Dormant` | **lobby travel only**: socket close or travel deadline (no window in Lobby, rule 13) | `OnParticipantDisconnected(reason, InLobby)` |
| `Connected`, `Unresponsive` | `Disconnected` | silence ≥ Hard, socket close, quit, kick; **in-round only** | `OnParticipantDisconnected(reason)` |
| `Connected`, `Unresponsive` | `Dormant` | same triggers **in Lobby** | `OnParticipantDisconnected(reason, InLobby)` |
| `Disconnected` | `Rejoining` | matching key admitted in mode `RejoinOnly` | — |
| `Rejoining` | `Connected` | acknowledgement matches | `OnParticipantRejoined(DowntimeSec)` |
| `Rejoining` | `Disconnected` | drop, handshake timeout or second mismatch during a rejoin that started from `Disconnected`; the window keeps running from the original disconnect | — (no duplicate disconnect event) |
| `Rejoining` | `Expired` | the same failures during a rejoin that started from `Expired` | — (no second expiry event) |
| `Connected`, `Unresponsive`, `Rejoining` | `Rejoining` (in-round) or `Traveling`-equivalent lobby acknowledgement (Lobby), new connection | supersede (rule 15): old mapping dropped, old connection closed, new connection bound | `OnParticipantRejoined(Superseded)` when the acknowledgement completes |
| `Disconnected`, `Rejoining` | `Expired` | window lapses (an arrival in the same tick wins) | `OnRejoinWindowExpired` |
| `Expired` | `Rejoining` | matching key in mode `RejoinOnly` **and** Round's `ShouldAdmitExpired` returns true | — (`OnParticipantRejoined` on completion) |
| `Disconnected`, `Expired` | `Dormant` | lobby travel arrives | — |
| `Dormant` | `Connecting` | key admitted in mode `Open` (passes F1) | — |
| any | `SessionEnded` | host leaves or crashes | `OnSessionEnding(reason)` |

Illegal (asserted in tests): any transition into `Connected` while admission mode is `Closed`; `Expired` → `Rejoining` without Round's policy approval; `Dormant` → `Rejoining`; an Unresponsive or Disconnected host; two live connections for one key.

### Interactions with Other Systems

| System | Direction | Interface (Roster, Round, TDC, Level Socket Contract and Character Pool are written; the rest are provisional) |
|---|---|---|
| Participant State & Roster | Session → Roster (hard) | Roster's canonical calls (Roster R4 mapping): `BeginMatch(TeamSize)` at session open, `RegisterParticipant(IdentityKey, DisplayName)` → `Registered` / `Reclaimed` / rejection on a fresh login, `RebindView(Id, PlayerState)` on a seamless arrival (Session's `BindConnection` wraps both), `NotifyConnectionChanged(Id, Projection, ServerTimeMs)` on every `ConnectionState` change (projection table in Roster R11), `InvalidateAllBindings()` at every travel start, `IsRoundScopeReset(RoundIndex)` for rule 11.2, `RemoveParticipant` only via Round in Lobby; the field scope table (rule 17). Session's `Expired` maps to Roster `Disconnected`, never to `Retired`. Roster changes lifecycle only via `RequestTransition`, which Round calls. |
| Round & Match Framework | Round → Session (hard) | Calls `RequestRoundTravel`, `RequestLobbyTravel`, `SetAdmissionMode`, `GetConnectionState(Participant)`, and `KickParticipant`. `KickParticipant` is host debug only, and the kicked key is refused for the rest of the session. Round implements the policy hook `ShouldAdmitExpired(Participant)` (rule 8; Round R18) and the reveal at Active (rule 11.8), and orders its start as rule 11.1 states (lock, `ResetAll`, Preparing, `RejoinOnly`, then `RequestRoundTravel` with TDC's stored `PresetId`). Round's readiness barrier (Round R6 P5) waits on `OnParticipantTravelComplete` alone, which covers tuning and pool residency. Subscribes to `OnParticipantJoined/Unresponsive/Responsive/Disconnected/Rejoined/TravelComplete`, `OnRejoinWindowExpired`, `OnRoundTravelStarted`, `OnLobbyTravelComplete`, `OnTravelFailed` and `OnSessionEnding`. Round owns every consequence: pawn hand-over, forfeits, Preparing → Active gating, `RequestTransition(..., Disconnect)`. |
| Round & Match Framework / Input & Interaction | Session → Round mailbox (server) | Every command item a client RPC delivers to Round's per-source mailbox is stamped with the receiving connection's current `ConnectionGeneration` (Roster R3), so Round and the IIF can void a command from a superseded or pre-disconnect connection at resolution (Round change request 13e). Server-only; never replicated. | Session (stamp), Round (use) |
| Tuning Data & Config | Session → TDC (soft) | Reads TDC's `SchemaVersion` (as `TuningSchemaVersion`) for the join gate; compares each client's echoed `ClientHash` with the server's `ClientHash` for the epoch (`RoundIndex`, `Revision`) in the single acknowledgement (rule 11.6); never reads, replicates or echoes `TuningHash`; carries the `PresetId` from TDC's preset store as the server-side `?Preset=` option (TDC CR-4); reads its own session-scoped keys (Tuning Knobs) when the session opens. `OnParticipantTravelComplete` is TDC's tuning-`Ready`. TDC adds no acknowledgement, timer or kick of its own (TDC CR-9). |
| Level Socket Contract / Procedural City Generator | Generator → Session (soft) | Reads `FRoundParams.Seed`, writes `LayoutAttempt` and `LayoutHash` into `FRoundReadiness`, and raises `OnLayoutReady(LayoutId, Seed)`. Uses public sub-seeds only for geometry (F5). A client acknowledges only after building its geometry and recomputing `LayoutHash` (LSC R4.7). The graybox fallback travel inside Preparing uses `RequestFallbackTravel` (rule 11.9, provisional). |
| Character Pool & Appearance | Pool → Session (soft) | `PoolHash` is a field of the single acknowledgement (rule 11.6); residency is confirmed by `OnParticipantTravelComplete`; Character Pool draws deals from `GetServerSubSeed(AppearanceDeal, Attempt)`. |
| Objectives Chain, Crowd & NPC Behaviour, Round placement and spawn order | consumers of `GetServerSubSeed` (soft) | Every hidden draw uses a server-only sub-seed (rule 14). Crowd also receives the hand-over recommendation (rule 25f). |
| Information Visibility Policy | peer contract (soft) | Session supplies the travel and disconnect leak checklist (rule 25), and the Policy's allowlist and leak tests enforce it. Session uses the Policy's audience vocabulary. |
| Shared Character & Locomotion | peer contract (soft) | Shares rule 25d: pawn replicated shape is identical for players and NPCs and is unchanged by connection events. |
| HUD (MVP) / Lobby UI (VS) | UI → Session (soft) | Read the lobby view (names, `n / capacity`), the frozen in-round name list, rejection reason codes, and the local client's own connection status. |
| Dedicated Server (Alpha) / Matchmaking (FV) | → Session (hard for them) | Reuse all admission, travel and liveness rules with the host-is-player flag off. Under a dedicated server, "host leaves" means "server shuts down". |
| Telemetry & Analytics (Alpha) / Anti-Cheat (Alpha) | consumers (soft) | Disconnect reasons, downtime, travel durations and rejection counts. IdentityKey hardening and the fact that hashes are client-asserted. |

## Formulas

*systems-designer consulted. Defaults and safe ranges follow from these formulas. Network rates marked "spike" are working values until the net-emulation spike (100 ms RTT, 1% loss) measures them.*

### F1. Admission capacity

The admission_capacity formula is defined as:

`Capacity = 2 * TeamSize`; `Occupied = count(records whose ConnectionState is in {Connected, Unresponsive, Traveling, Disconnected, Rejoining}) + count(pending handshakes that have no record yet)`; `AdmitNew = (AdmissionMode == Open) AND (Occupied < Capacity)`. A supersede handshake and a rejoin handshake for a known key are never counted a second time: they reuse the record's slot.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| TeamSize | int | 1-5 | data file (`TeamSize`, TDC; owned by Round) | Players per team |
| Occupied | int | 0-10 | calculated (Session) | Slots held, including pending handshakes and in-window rejoin slots |
| AdmissionMode | enum | Open, RejoinOnly, Closed | calculated (pushed by Round, rule 20) | Current admission mode |

**Output Range:** `Capacity` is 2-10. A rejoin by a key in `Disconnected` is admitted without this test because its slot is already held. `TeamSize` may be lowered in Lobby only to a value where `Occupied <= 2 * TeamSize`, which the TDC preset validation enforces (provisional).
**Example:** TeamSize 5, so Capacity is 10. Nine participants are Connected and A's handshake is pending, so Occupied is 10. B's handshake arrives next and is rejected as `Full`; B sees "This session is full." (`Full` is non-retryable, rule 23, so B's client does not retry by itself). If A's handshake then fails, Occupied drops to 9 and B's next manual Join is admitted.

### F2. Disconnect detection latency bound

The detection_latency formula is defined as:

`L_max(T) = T + dC + d_ow + H_excess`; `L_min(T) = T + d_min` (the accumulator cannot reach T before T seconds of silence have been counted), where `H_excess` is the sum, over server frames longer than `LivenessMaxCountedFrameSec`, of (frame time - `LivenessMaxCountedFrameSec`)

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| T | float | Soft 2-5 s; Hard 8-20 s | data file (`SessionSoftTimeoutSec` 3, `SessionHardTimeoutSec` 12) | Silence threshold |
| dC | float | 0.1-0.5 s | data file (`LivenessCheckIntervalSec` 0.25) | Evaluation interval |
| d_ow | float | 0.02-0.3 s | measured (spike) | One-way delay of the last packet (50 ms at 100 ms RTT) |
| d_min | float | 0-0.05 s | measured | Minimum one-way delay |
| H_excess | float | 0-several s | calculated | Server hitch time excluded from the silence clock |

**Output Range:** at the defaults and with no hitches, Soft fires 3.0-3.3 s and Hard 12.0-12.3 s after the client actually goes silent; across the full safe ranges Soft fires up to T + 0.8 s. Sustained slow server frames (each longer than `LivenessMaxCountedFrameSec`) slow the clock, so the clamp must be at least the worst sustained server frame time the spike measures. Each host hitch delays detection by its excess duration, and no hitch can cause a false detection.
**Example:** T = 3, dC = 0.25, d_ow = 0.05, so L_max = 3.30 s. T = 12 gives 12.30 s. A 2.0 s host hitch with `LivenessMaxCountedFrameSec` 0.1 adds H_excess = 1.9 s, so Hard fires at 14.2 s instead of 12.3 s.

### F3. False-timeout probability

The false_timeout formula is defined as:

`k = floor(T * R_c)`; `P_iid = p^k`; `P_false(T) ~= P_iid + P_burst(B >= T)`; `lambda = N_clients * EventsPerHour(T) * P_false(T)`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| T | float | 2-20 s | data file | Soft or Hard threshold |
| R_c | float | 5-60 packets/s | measured (spike); floor 5/s from the engine keepalive of about 0.2 s **[verify]** | Client-to-server packet rate (idle Lobby is the minimum) |
| p | float | 0-0.2 | measured (spike; design point 0.01) | Independent packet-loss probability |
| P_burst(B >= T) | float | 0-1 | measured (spike, Wi-Fi playtest) | Probability that a loss burst or outage lasts at least T |
| N_clients | int | 1-9 | calculated | Remote clients (the host has no link) |
| EventsPerHour | float | — | measured | Burst opportunities per client-hour |

**Output Range:** P_iid is negligible at every safe setting. Real false timeouts come from bursts such as Wi-Fi roaming and OS stalls of 1-3 s. Rules that follow from this: `SessionSoftTimeoutSec >= 2 * RTT_p99 + 1 s` and `SessionSoftTimeoutSec >= 3 / R_c`. Hard should exceed the longest burst the spike measures, plus a 50% margin.
**Example:** In Lobby, R_c = 5 and T = 3 give k = 15, so P_iid = 0.01^15 = 1e-30. In a round, R_c = 30 gives k = 90, which is effectively 0. If the spike measures 3 s bursts on Wi-Fi at 0.5 per client-hour with P(B >= 3) = 0.2, then 9 clients see 9 * 0.5 * 0.2 = 0.9 false Unresponsive events per hour. Each one only triggers a reversible pawn hand-over. At Hard = 12, P(B >= 12) is assumed to be about 0.

### F4. Rejoin-window adequacy

The rejoin_window_adequacy formula is defined as:

`Adequate = RejoinWindowSec >= M * (t_restart + t_connect + t_load + A) + T_busy`; `ReservedFromLastPacket = SessionHardTimeoutSec + RejoinWindowSec`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| RejoinWindowSec | float | 60-600 s (hard; safe 60-300) | data file (default 120) | In-round slot reservation after Disconnected; the hard minimum 60 keeps Round's `G_eff = min(G, RejoinWindowSec - 30)` at least 30 s |
| M | float | 1.25-2.0 | constant (design margin 1.5) | Safety factor |
| t_restart | float | 10-60 s | playtest | Relaunching the game after a crash |
| t_connect | float | 0.5-5 s | measured | Handshake |
| t_load | float | 5-40 s | measured (spike) | Loading the match map |
| A | float | 1-5 s | data file (`ClientAutoRejoinIntervalSec` 3) | Worst-case wait for the next retry |
| T_busy | float | 0-20 s | measured (spike: `T_srvload`) | Longest time a rejoin is refused `Busy` because a travel is in progress (rule 20); 0 when no travel overlaps the window |

**Output Range:** true or false. At the default, a returning player has 132 s from their last packet, which is about 18% of a 12-minute target round.
**Example:** 1.5 * (30 + 2 + 20 + 3) + 0 = 82.5 s <= 120 s, so the window is adequate; with a travel overlapping the window (T_busy 20) it is 102.5 s, still adequate. If the spike measures a 40 s load, 1.5 * 75 + 20 = 132.5 s exceeds 120 s, so the window would have to rise to at least 135 s (or the overlap case be accepted as a known miss). A 60 s load gives 1.5 * 95 + 20 = 162.5 s, so at least 165 s.

### F5. Round seed and sub-seed derivation

The seed_derivation formula is defined as:

`Seed = (H64(ServerSecret || MatchNonce || RoundIndex || Draw) mod (2^31 - 1)) + 1`, redrawn with `Draw + 1` while `Seed` is in `UsedSeeds` or in the Procedural City Generator's golden regression set (so a live round never uses a test city; Generator change request 2)
`PublicSubSeed(Domain, Attempt) = Mix32(Seed, Domain, Attempt)`, used for layout geometry only
`ServerSubSeed(Domain, Attempt) = H64(ServerSecret || MatchNonce || RoundIndex || Domain || Attempt)`, server-only and independent of `Seed`
Encoding: every `H64` input is fixed-width little-endian (`ServerSecret` 16 bytes, `MatchNonce` 8, `RoundIndex` 4, `Draw`/`Domain`/`Attempt` 4 each) and starts with a one-byte purpose tag (0x01 for `Seed`, 0x02 for `ServerSubSeed`), so the public `Seed` input can never equal a secret sub-seed input. Reference `Mix32` (integer-only, platform-independent; every operand is `uint32` and every shift is a logical shift, since signed shifts break the golden vectors): `x = Seed XOR (Domain * 0x9E3779B1) XOR (Attempt * 0x85EBCA6B)` (mod 2^32); `x ^= x >> 16; x *= 0x7FEB352D; x ^= x >> 15; x *= 0x846CA68B; x ^= x >> 16`; result `x AND 0x7FFFFFFF`, with 0 mapped to 1. An ADR may replace the mixer only by keeping the contract and replacing the golden vectors. `H64` is SipHash-2-4 with `ServerSecret` as its 128-bit key and the remaining fields (purpose tag first) as the message (chosen in ADR-0006 §3).

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| ServerSecret | 128-bit | — | calculated at session open from OS entropy | Never replicated, logged or put in a URL |
| MatchNonce | 64-bit | — | calculated at session open | Separates matches |
| RoundIndex | uint32 | 1 to 2^32-1 (tests use 1-99) | Round | Round number in the match; no cap, a match may run past 99 rounds |
| Draw | int | 0-n | calculated | Redraw counter |
| UsedSeeds | set | <= rounds played | calculated | Seeds already used this match |
| Domain | uint32 | enum | constant (public: Layout = 1 only; server-only: ObjectiveSites = 2, AppearanceDeal = 3 (consumed with `Attempt = 0`; Character Pool derives its three streams internally), PlayerPlacement = 4, NpcRoutes = 5, SpawnOrder = 6, and, added at the 2026-10-24 reconciliation, EmergencePick = 7, RespawnPick = 8, AmbientSchedule = 9 (Level Socket Contract), StageSignal = 10 (Objectives), SeqOrder = 11 and TieRank = 12 (Round's per-tick intake permutation and tie rank), and, registered by ADR-0006, CrowdBehaviour = 13 (Crowd CNB-R14) and BushSpotPick = 14 (Concealment F4 blind-strike spot pick, ADR-0017)) | Purpose tag. The public API `PublicSubSeed` refuses every domain except Layout (returns an error and raises an `ensure`); `GetServerSubSeed` is server-only and asserts authority |
| Attempt | uint32 | Layout: 0-`LayoutMaxReseedAttemptsCount` (LSC key); counter-based server domains (7-12): 0 to 2^32-1; CrowdBehaviour (13) and BushSpotPick (14): the round's final `LayoutAttempt` (draws inside are keyed by a counter tuple) | Level Socket Contract (layout attempts); the consuming system (counters) | Reject-and-reseed counter for Layout; for EmergencePick, RespawnPick, AmbientSchedule, StageSignal, SeqOrder and TieRank, the draw counter (request sequence id, completion ordinal or server tick) so each draw is independent and reproducible |

**Output Range:** `Seed` is in [1, 2^31 - 1], so it is never 0 and never repeats within a match. Sub-seeds are near-uniform 31-bit values (public; 0 is mapped to 1, so 1 has double weight) or uniform 64-bit values (secret).
**Example:** Seed = 123456789. `Mix32(Seed, 1, 0)` = 2107646860 (= `PublicSubSeed(Layout, 0)`); `Mix32(Seed, 1, 1)` = 523860287; `Mix32(Seed, 2, 0)` = 313478722, a vector for the mixer function itself (the public API refuses Domain 2). These are the golden vectors (re-verified by the 2026-10-24 review) for `StealthGame.Session.SeedDerivationGolden`. Over a 10-round match the chance of any raw collision before redraw is C(10,2) / (2^31 - 1) = 45 / 2147483647, about 2.1e-8 (99 rounds: about 2.3e-6). Every collision is redrawn anyway.

### F6. Client host-loss detection

The host_loss_detection formula is defined as:

`L_host_crash = HostLossTimeoutSec + dC + d_ow`; `L_host_quit <= d_ow + ShutdownFlushSec`; constraints `HostLossTimeoutSec > T_host_stall_max` and `ShutdownFlushSec >= 2 * RTT_p99 + 0.1 s`. The client's clock is the same clamped accumulator as rule 18 and is paused from travel start until `FRoundParams` (or the lobby acknowledgement request) arrives, so neither the client's own map load nor the host's map load or layout build counts as silence.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| HostLossTimeoutSec | float | 8-30 s | data file (default 12) | Client-side silence from the server before declaring the session ended |
| dC | float | 0.1-0.5 s | data file (`LivenessCheckIntervalSec`) | Client evaluation interval |
| d_ow | float | 0.02-0.3 s | measured | One-way delay |
| ShutdownFlushSec | float | 0.3-1.0 s | data file (default 0.5) | Longest time the host waits after the reliable "session ending" notice; it closes earlier once every client has acknowledged the notice |
| T_host_stall_max | float | 0-30 s | measured (spike) | Longest host game-thread stall outside travel (for example a synchronous generator step); the generator must not block the host's game thread during Active |

**Output Range:** after a deliberate quit every client shows "The session has ended." within about 0.55 s; a client whose notice and its retransmission are both lost within the flush falls back to the crash bound. After a crash it shows within about 12.3 s. No client ever stays frozen, and no client declares the session ended because of a map load.
**Example:** At 100 ms RTT: L_host_quit = 0.05 + 0.5 = 0.55 s; L_host_crash = 12 + 0.25 + 0.05 = 12.30 s.

### F7. Travel deadline and Preparing bound

The travel_deadline formula is defined as:

`t_ready` = server time at which `FRoundReadiness` and TDC's public snapshot are both published; `Deadline = max(t_travel + TravelLoadTimeoutSec, t_ready + TravelAckGraceSec)`; `T_prep_max = Deadline - t_travel`, where `t_ready - t_travel <= T_srvload + LayoutTotalBudgetSec + T_tuning`; constraint `EngineConnectionTimeoutSec >= max(SessionHardTimeoutSec, T_prep_max at the layout budget) + 30`. Both terms of `Deadline` are server-measured events; the deadline is fixed once `t_ready` is known, is common to every participant in the travel, and is never moved by a client message, a rejoin or a supersede (TDC CR-9).

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_travel | float | — | calculated | Server time the travel was issued |
| TravelLoadTimeoutSec | float | 20-180 s | data file (default 60) | Load-and-acknowledge deadline counted from travel start |
| TravelAckGraceSec | float | 5-60 s | data file (default 15) | Minimum time every client has to acknowledge after the server publishes the readiness data, so a slow server load or layout build never times out every client |
| T_srvload | float | 1-20 s | measured (spike) | Server's own map load after `ServerTravel` |
| T_layout | float | 0.1-45 s | measured (Generator); bounded by the Level Socket Contract's `LayoutTotalBudgetSec` (45) | Time to `OnLayoutReady` |
| T_tuning | float | < 1 s | measured (TDC) | Time to `OnTuningReady` |
| EngineConnectionTimeoutSec | float | >= 105 s at defaults | engine config (-> ADR) | Engine backstop timeout |

**Output Range:** Preparing never waits longer than `T_prep_max` on Session's account: `TravelLoadTimeoutSec` when the server is ready early, at most `T_srvload + LayoutTotalBudgetSec + T_tuning + TravelAckGraceSec` when it is late. Each late participant becomes `Disconnected(TravelTimeout)` and can rejoin.
**Example:** TravelLoadTimeoutSec 60, TravelAckGraceSec 15, T_srvload 10, T_layout 8, T_tuning 0.2: t_ready = 18.2 s, Deadline = max(60, 33.2) = 60 s. With all clients loading in 15 s they acknowledge just after 18.2 s and Preparing ends then. Worst case at the layout budget (T_layout 45): t_ready = 55.2 s, Deadline = max(60, 70.2) = 70.2 s, so the engine backstop must be at least max(12, 70.2) + 30 = 100.2 s, rounded to 105 s.

### F8. Heartbeat bandwidth (ADR fallback only; disabled in MVP)

The heartbeat_bandwidth formula is defined as:

`B_hb = (L_pay + L_hdr) / SessionHeartbeatIntervalSec`; `f = B_hb / B_budget`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| L_pay | bytes | 8-16 | constant (design, 12) | Sequence number and timestamp |
| L_hdr | bytes | 30-50 | estimate (36) | UDP/IP plus engine framing |
| SessionHeartbeatIntervalSec | float | 0 (disabled) or 0.25 to `SessionSoftTimeoutSec / 3` | data file | Heartbeat period, if enabled; values in (0, 0.25) and above Soft/3 are rejected by validation |
| B_budget | KB/s | 50 | constant (performance budget, game concept) | Per-client bandwidth budget |

**Output Range:** 24-192 B/s, which is 0.05-0.4% of the budget. It is sent on the player's own controller channel and never on a pawn channel, so it cannot become a pawn-level tell.
**Example:** (12 + 36) / 1 = 48 B/s = 0.048 KB/s, so f = 0.1% per client. Server ingress for 9 clients is 0.43 KB/s.

## Edge Cases

*systems-designer consulted. The edge-case list was reviewed against Core Rules and F1-F8. Network and replication specialists added the leak cases.*

**Admission and capacity**
- **If two handshakes compete for the last slot**: admission is evaluated serially on the server's game thread. The first handshake holds a pending slot (F1). The second is rejected as `Full` and its client does not retry automatically (rule 23); a later manual Join is admitted only if the first handshake has failed by then.
- **If a pending handshake never completes**: after `PendingLoginTimeoutSec` the connection is closed as `Rejected`, its slot is released, and no Roster record is created.
- **If a new key connects in Preparing, Active or Ended**: it is rejected as `InProgress`, with the message shown only to that client. No in-round client receives any event.
- **If a known key whose record is Dormant connects mid-round** (for example someone who left in Lobby and returns after the round started): it is rejected as `InProgress`. Dormant records return only in `Open` mode.
- **If an Expired key connects in the same round**: Session asks Round's `ShouldAdmitExpired(Participant)`. If it returns true, the key is admitted as a rejoin, and Round decides the role it returns to (Pillar 3: a reduced role over removal). The expiry consequence Round already applied stands. If it returns false, or Round does not implement the hook, the key is rejected as `InProgress` (provisional default). Either way, at the next lobby travel the record becomes Dormant and the person can return as themselves through `Open` admission.
- **If any connection arrives while admission mode is `Closed`** (Reset or travel in progress): it is rejected as `Busy` (retryable), and the client retries every `ClientAutoRejoinIntervalSec`.
- **If the protocol version or `TuningSchemaVersion` differs**: the connection is rejected as `VersionMismatch` (non-retryable). Nothing is created or changed.
- **If a client sends team, spectator, seed or other unknown options**: they are never read; only the key, the requested name and the `RejoinToken` are parsed (rule 7; the engine cannot strip join options, ADR-0005 E17).
- **If two installs share an IdentityKey** (a copied config or two clients on one dev machine without the override): the later connection supersedes the earlier one (rule 15), and the server logs the address change. This is resolved for development by `-IdentityKey=` and PIE-indexed keys.
- **If a duplicate display name joins**: it gets a numeric suffix. Identity matching ignores names entirely.

**Travel and seed**
- **If `RequestRoundTravel` is called while a travel is in progress**: it is rejected as `TravelInProgress`, and nothing changes.
- **If `RequestRoundTravel` is called while a participant is Unresponsive or Connecting**: it is rejected as retryable (`ParticipantUnresponsive` or `ParticipantConnecting`). An Unresponsive participant recovers or becomes Dormant within `SessionHardTimeoutSec`; a Connecting handshake completes or is closed within `PendingLoginTimeoutSec`; no new unknown-key handshake starts once Round has pushed `RejoinOnly`. The start cannot be blocked indefinitely.
- **If the drawn Seed equals one already used this match**: Session redraws with `Draw + 1` before anything is sent. Clients never see the collision.
- **If the `Seed` option is missing, duplicated, non-numeric, zero, negative or overflows on arrival**: no default is substituted. `OnTravelFailed(BadParams)` is raised, and Round returns everyone to the Lobby.
- **If the generator rejects and reseeds**: the final `LayoutAttempt` is written to `FRoundReadiness`, and clients echo it. Clients never re-run the rejection loop.
- **If a client receives `FRoundReadiness` before its map has finished loading, or the reverse**: it acknowledges only once both conditions are true, in either order, and sends exactly one acknowledgement.
- **If a client's echoed Seed, LayoutAttempt, LocalLayoutHash, ClientHash or PoolHash differs**: Session re-requests the acknowledgement once with `AckSeq + 1` (rule 11.6; the replicated structs are not re-sent). A second mismatch gives `Disconnected(ParamsMismatch)` with no auto-rejoin. The window still opens, so a manual retry after the player fixes their build is possible. Any number of mismatching clients only disconnects themselves; the round is never aborted on client reports.
- **If an acknowledgement arrives with a stale epoch, an old `AckSeq`, as a duplicate, or on a superseded connection**: it is dropped and logged (at most once per second per participant) and neither satisfies nor fails the wait.
- **If client-side state from the previous round survives seamless travel** (the controller and PlayerState are replaced on arrival, rule 16, but a client-local subsystem may still hold an "already acknowledged" marker): the marker is keyed by `FAckEpoch`, so the new round's request is answered normally. The acknowledgement request is sent only after `BindConnection` on the new controller (ADR-0005 R-6).
- **If TDC publishes a new `Revision` (PIE hot-reload only, TDC CR-12) while acknowledgements are outstanding**: a new epoch starts; every participant not yet `Connected` is re-requested and stale replies are dropped. The F7 deadline does not move.
- **If a client drops during travel**: socket close gives `Disconnected` immediately. Otherwise `Disconnected(TravelTimeout)` occurs at `Deadline_i` (F7). The rejoin window starts then, and the other participants' travel continues unaffected.
- **If a rejoin attempt arrives while a travel is in progress**: it is rejected as `Busy` and retried. A travel is "in progress" from `t_travel` until the server's own map load completes (`T_srvload`, at most 20 s); admission is `RejoinOnly` again from then on, so a rejoin can proceed while other clients are still loading. The window keeps running; F4's term `T_busy` accounts for this wait.
- **If the host quits or crashes during travel**: every participant moves to `SessionEnded`, and clients show "The session has ended." (F6). No client is left in the transition map.
- **If seamless travel fails for a client** (for example a missing transition map or a failed load): the participant times out to `Disconnected(TravelTimeout)`. Its client auto-rejoins with its cached address and key and loads the current map directly. The rejoin is a full reconnect, and the round acknowledgement still applies.
- **If same-map travel in the prototype misbehaves**: this is **provisional pending spike**. The fallback is a distinct `LobbyMap`, and no rule in this GDD changes.

**Liveness, disconnect and rejoin**
- **If a packet arrives in the same tick as the Hard threshold**: receives are processed first, the silence clock resets, and the participant stays.
- **If a rejoin completes in the same tick as the window lapses**: the arrival is processed first and the rejoin wins. `OnRejoinWindowExpired` is not raised.
- **If two participants disconnect in the same tick**: two `OnParticipantDisconnected` events fire in ascending `ParticipantId` order. That order is cosmetic, because Round resolves consequences through its own queue.
- **If the host hitches for 2 s**: at most `LivenessMaxCountedFrameSec` of that hitch counts towards any silence clock (F2). Nobody is marked Unresponsive by the host's own stall.
- **If a client crashes and restarts within the Hard timeout**: the new connection supersedes the old one. Only `OnParticipantRejoined(Superseded)` is raised, and the pawn is never handed away and back.
- **If a rejoining client drops again during its rejoin**: it returns to `Disconnected`, with no second disconnect event. The window continues from the original disconnect time and is not restarted, so repeated reconnects cannot extend it.
- **If the window expires during Preparing**: `Expired` and `OnRejoinWindowExpired` are raised once. Round decides whether the round starts without the participant. Later in-round attempts follow rule 8's `Expired` column: Session asks Round's `ShouldAdmitExpired`, and admits as a rejoin or rejects `InProgress` on its answer.
- **If a rejoin that started from `Expired` fails** (drop, handshake timeout or second mismatch): the participant returns to `Expired`, not `Disconnected`, and no second `OnRejoinWindowExpired` is raised.
- **If lobby travel starts while a participant is `Rejoining` or `Connecting`**: that connection is closed with reason `Busy` and its client retries; the record (if any) becomes `Dormant` on lobby arrival (rule 12).
- **If a participant disconnects in Lobby**: the slot is released at once and the record becomes Dormant. "Name left" is shown in the Lobby. The participant can return as themselves if capacity allows.
- **If a participant is kicked by the host (debug)**: the disconnect reason is `Kicked` (this reason on `OnParticipantDisconnected` is the `Disconnected(Kicked)` flag Round R15 (c) reads). In-round the participant goes straight to `Expired` (no window, because the key can never be readmitted) and `OnParticipantDisconnected(Kicked)` then `OnRejoinWindowExpired` are raised in that order in the same tick; in Lobby the record becomes `Dormant` and Round may remove it (`RemoveParticipant`). Every later connection with that key is rejected as `Refused` (non-retryable, gate 3, before the admission mode) for the rest of the session. Consequences are Round's, as for any disconnect.
- **If the host's own player is evaluated for liveness**: it is skipped, because the host has no link. The host is never Unresponsive or Disconnected, and a host quit always means `SessionEnded`.
- **If an engine-level timeout fires before Session's thresholds** (misconfiguration): the resulting socket close is handled as `Disconnected(ConnectionClosed)`. The configuration assertion in the acceptance criteria exists to catch this before playtests.

**Pillar 1 leak cases**
- **If an opponent's connection drops mid-round**: no client outside that participant's team receives any message, list change, count change, ping change or PlayerState change. The PlayerState stays in the player array (rule 22). Whatever the pawn visibly does is Round's or Crowd's behaviour, and rule 25f constrains it.
- **If a rejoiner comes back mid-round**: no pawn is spawned. The rejoiner repossesses its existing pawn, and the pawn's replicated references (rule 25d) do not change shape.
- **If a modified client runs the generator locally from the public Seed**: it obtains the city geometry, which everyone may know. Objective selection, appearance deals, placement and spawn order come from server-only sub-seeds (rule 14), so the hidden final site and who is where cannot be derived.
- **If a modified client reads the replicated player array mid-round**: it sees exactly the frozen set of PlayerStates with allowlisted fields only (rule 25b). Nothing in the array changes when someone disconnects.
- **If player pawns are spawned in a separate block after the NPCs**: this is forbidden. Round interleaves spawns in a server-secret shuffled order, and a test asserts that spawn order is uncorrelated with player-ness (rule 25e).
- **If the host player wants an information edge**: no design path gives one, because the host's view uses the same replicated path (rule 3). Reading memory is cheating, out of scope until Dedicated Server. The host's latency edge (rule 3) is recorded in playtests.
- **If a rejection reason could leak information** (for example a rejoin refused as "player eliminated"): no such reason exists. The reasons are only `Full`, `InProgress`, `Busy`, `VersionMismatch`, `Refused` (a kicked key) and `ParamsMismatch`, and none of them reveals anyone's team or state.

## Dependencies

Interfaces that belong to a GDD not yet written are **provisional**. As of the 2026-10-24 review, Roster, TDC, Round & Match, Level Socket Contract, Character Pool & Appearance and Input & Interaction are written, and this GDD uses their names; Visibility Policy, Generator, Crowd, Shared Character, Objectives and HUD have been written since (verified 2026-10-24, cross-review C-W11).

### Upstream (Session depends on)

| System | Data interface | Hard/Soft |
|---|---|---|
| Participant State & Roster (#2) | `BeginMatch`, `RegisterParticipant`, `RebindView`, `NotifyConnectionChanged`, `InvalidateAllBindings`, `IsRoundScopeReset` (Roster R4, R7, R9, R11), the field scope table (rule 17). Session cannot admit, rejoin or travel without it. | Hard |
| Tuning Data & Config (#1) | `SchemaVersion` as `TuningSchemaVersion` (join gate), the server's `ClientHash` per epoch (compared with each client's recomputed echo; `TuningHash` is never used by Session), `PresetId` from TDC's preset store (carried as `?Preset=`), session-scoped knobs read at session open. TDC's own table lists Session as a soft consumer. | Soft; listed in index row 5 (verified 2026-10-24, cross-review C-W11) |
| Character Pool & Appearance (#4) | `PoolHash` for the acknowledgement (rule 11.6) | Soft (provisional until Character Pool adopts the single acknowledgement) |
| Engine networking (UE 5.8 listen server, seamless travel, net driver) | Handshake hooks, seamless-travel arrival hooks, last-receive time, transition map, connection timeout config | Hard (engine; **[verify]** items) |

### Downstream (these depend on Session)

| System | What it takes from Session | Hard/Soft |
|---|---|---|
| Round & Match Framework (#6) | `RequestRoundTravel` / `RequestLobbyTravel` / `RequestFallbackTravel` / `SetAdmissionMode` / `GetConnectionState` / `KickParticipant` / `GetServerSubSeed`; all connection and travel events; the F7 Preparing bound. Round owns every disconnect consequence. | Hard |
| Information Visibility Policy (#7) | The travel and disconnect leak checklist (rule 25) for its allowlist and leak tests; the frozen in-round name list rule | Soft (peer contract) |
| Level Socket Contract (#8) / Procedural City Generator (#16) | `FRoundParams.Seed`, the `FRoundReadiness` fields they write, `PublicSubSeed` and `ServerSubSeed` (F5), the single acknowledgement, the fallback travel (rule 11.9) | Soft (the graybox map ignores the seed) |
| Character Pool & Appearance (#4), Objectives Chain (#15), Crowd & NPC Behaviour (#11) | `GetServerSubSeed(Domain, Attempt)` for hidden draws; for Character Pool, residency confirmation through the single acknowledgement; for Crowd, the disconnect hand-over recommendation and the idle-pause baseline (rule 25f) | Soft |
| Shared Character & Locomotion (#9) | Rule 25d: pawn replicated shape is player/NPC-identical and unchanged by connection events | Soft (peer contract) |
| HUD (#18) | Local connection status ("Reconnecting...", "The session has ended."), the frozen name list | Soft |
| Lobby UI (#19, VS) | Lobby view (names, `n / capacity`, ready), join flow, rejection codes | Hard |
| Dedicated Server (#28, Alpha) | All admission, travel and liveness rules with host-is-player off | Hard |
| Matchmaking (#29, FV) | Admission gates and identity; replaces address entry | Hard |
| Teammate Markers (#17) | seamless travel and rejoin timing (markers cleared and rebuilt from the re-pushed view); added at the 2026-10-24 cross-review (C-W13) | Soft |
| Telemetry & Analytics (#26), Anti-Cheat (#27) | Connection event stream; identity and hash hardening points | Soft |

**Bidirectionality note**: Roster must list Session & Travel as a dependent and adopt the field scope table. Round must list Session as an upstream dependency (the index already does) and state that it pushes the admission mode. Lobby UI, Dedicated Server and Matchmaking already list Session in the index. The soft dependents (Visibility Policy, Level Socket Contract, Generator, Character Pool, Objectives, Crowd, Shared Character, HUD, Telemetry, Anti-Cheat) should cite this GDD where they use the sub-seed API, the leak checklist or the connection status. Change requests are listed under Cross-References.

## Tuning Knobs

Every knob registers under TDC's Knob Registry Convention (TDC CR-15), sub-struct `Session`, with the CR-15 columns below. **Scope: session.** These values are read when the session opens and are not refrozen per round, because admission and liveness run before and between rounds. A change takes effect at the next session open (a TDC session-scope convention; provisional, change request to TDC). Audience is ServerOnly unless stated otherwise. The two client-side values are sent to clients on join in a small identical `SessionClientParams` struct (audience All), and cooked defaults are used before that struct arrives.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|---|---|---|
| `SessionSoftTimeoutSec` | float | Session | 3.0 | 1-10 | 2-5 | ServerOnly | Session | Sec | Silence before Unresponsive, which triggers Round's hand-over. High: a silent pawn waits longer before the crowd AI takes it (tell risk). Low: false events from Wi-Fi bursts (F3). Must be ≥ 3 / R_c and ≥ 2·RTT_p99 + 1; Crowd's idle-pause baseline must cover Soft + 1 s. Identity-tell review (CR-15.8): it times a crowd-versus-player event; safe only while Round's hand-over is on and Crowd's pauses are common (rule 25f). |
| `SessionHardTimeoutSec` | float | Session | 12.0 | 5-60 | 8-20 | ServerOnly | Session | Sec | Silence before Disconnected. High: slow detection, and the window opens late. Low: brief outages cost a full disconnect. Validation rejects `Hard < Soft + 2` (2 s covers dC + d_ow + one burst at the soft bound); engine backstop ≥ max(Hard, T_prep_max) + 30. Identity-tell review: no visible effect (Round's grace timer, not this, ends the active role). |
| `RejoinWindowSec` | float | Session | 120 | 60-600 | 60-300 | ServerOnly | Session | Sec | In-round slot reservation. High: a slot is held long for a lost player. Low: crash recovery fails (F4). Must satisfy F4 and Round's `DisconnectGraceSec <= RejoinWindowSec - 30`. |
| `TravelLoadTimeoutSec` | float | Session | 60 | 20-180 | 30-120 | ServerOnly | Session | Sec | Load-and-acknowledge deadline from travel start (F7). High: Preparing can stall. Low: slow machines miss rounds. |
| `TravelAckGraceSec` | float | Session | 15 | 5-60 | 10-30 | ServerOnly | Session | Sec | Minimum acknowledgement time after the readiness data is published (F7). Low: a late layout build times out every client. High: Preparing waits longer on a stuck client. |
| `HostLossTimeoutSec` | float | Session | 12.0 | 5-60 | 8-20 | All (client consumer: host-loss clock, F6) | Session | Sec | Client silence before "The session has ended." Must exceed `T_host_stall_max`. Low: false session ends. High: a crashed host freezes clients longer. |
| `ClientAutoRejoinIntervalSec` | float | Session | 3.0 | 1-10 | 2-5 | All (client consumer: auto-rejoin) | Session | Sec | Retry period. Low: handshake spam. High: wasted window. Term `A` in F4. |
| `LivenessCheckIntervalSec` | float | Session | 0.25 | 0.05-1.0 | 0.1-0.5 | ServerOnly | Session | Sec | Detection granularity (F2). |
| `LivenessMaxCountedFrameSec` | float | Session | 0.1 | 0.02-0.5 | 0.05-0.25 | ServerOnly | Session | Sec | Hitch clamp (F2 `H_excess`). High: host hitches cause false timeouts. Low: sustained slow frames slow detection; keep ≥ the worst sustained server frame. |
| `PendingLoginTimeoutSec` | float | Session | 20 | 5-60 | 10-30 | ServerOnly | Session | Sec | Handshake deadline (first packet to login completion; F1); frees slots held by stuck joins. Engine initial-connect timeout ≥ this + 10. |
| `ShutdownFlushSec` | float | Session | 0.5 | 0.1-2.0 | 0.3-1.0 | ServerOnly | Session | Sec | Longest wait after the "session ending" notice (F6). Must be ≥ 2·RTT_p99 + 0.1. Low: a lost notice sends clients down the crash path. |
| `SessionHeartbeatIntervalSec` | float | Session | 0 (disabled) | 0 or 0.25-5 | 0.5-1 when enabled | ServerOnly | Session | Sec | ADR fallback heartbeat (F8). Validation rejects values in (0, 0.25) and above Soft / 3. |
| `DisplayNameCharLimit` | int | Session | 20 | 4-32 | 12-24 | ServerOnly | Session | Limit | Name truncation length (rule 10); lobby layout fit. |

Latch `Session`: read at session open and not re-frozen per round, because admission and liveness run before and between rounds. TDC's CR-11 has no such class yet (change request 3). **Protocol constant, not a knob**: the 1-per-second log limit for dropped acknowledgements (rule 11.6). **Network configuration, not tuning** (outside TDC, never gameplay values): `HostPort` (7777, listen port, needs forwarding for internet play), `LobbyMap` and `MatchMap` (asset paths; same-map travel is provisional pending spike).

Not knobs: `Capacity`, which is derived from `TeamSize` (Round/TDC) and has no separate key; `ProtocolVersion`, a deliberate build constant; and `TuningSchemaVersion`, which TDC owns.

## Visual/Audio Requirements

This section is optional for a Core system, and no art-director consult was needed. Session has no in-world visual or audio events. Its requirements are negative:
- **No in-world feedback of any connection event.** No VFX, sound, nameplate, flicker or animation change may happen because someone joins, leaves, lags, reconnects or travels (Pillar 1). The pawn hand-over, if Round adopts it, must be visually continuous: no snap or pose reset, and the same blend rules as any Crowd behaviour change.
- **Travel presentation**: all clients show the same neutral loading screen during travel, with no participant names, team colours or counts. In the Held Breath art direction this is a plain field or a generic emblem (art bible §7 UI tone). It is never an image of the round's city. It persists for every client, the host included, until Round enters Active (rule 11.8). Lobby join and leave chimes, if any, are Lobby UI (VS). The MVP plays none.

## Game Feel

This section is optional for a Core system. Its feel targets are responsiveness and honesty, not physical feel:
- **Join responsiveness**: on LAN, under 3 s from pressing Join to appearing in the Lobby list. Under 100 ms RTT, under 5 s. Rejection messages appear within 1 RTT of the gate decision.
- **No limbo**: every waiting state shows exactly one plain status line ("Connecting...", "Reconnecting...", "Loading the city..."), and every one of them ends in a bounded time (F6, F7, `PendingLoginTimeoutSec`). Nothing waits silently forever.
- **Round transition**: from the host pressing Start to every client standing in the new city should take the slowest participant's load time plus at most 1 s, with the reveal simultaneous (rule 11.8). Nobody is shown the main menu between rounds (Player Fantasy anchor 1).

## UI Requirements

MVP UI is minimal and functional, because Lobby UI is Vertical Slice. It needs:
- **Main menu**: *Host* (opens the session) and *Join* (an address field plus a Join button). The field remembers the last address.
- **Minimal lobby panel**: participant names, `n / capacity`, and a host-only Start control (owned by Round's start flow). Join and leave lines appear in a small lobby log.
- **Status overlays** (local client only): "Connecting...", "Loading the city...", "Connection lost. Reconnecting..." with a Cancel option, and "The session has ended." with an OK button that returns to the menu.
- **Rejection messages**, one plain sentence per reason code: `Full` "This session is full."; `InProgress` "This round has already begun."; `Busy` "The session is changing cities. Trying again..."; `VersionMismatch` "Your game version does not match the host's."; `ParamsMismatch` "Your game could not match the host's city. Please restart the game."; `Refused` "You cannot rejoin this session."
- **No in-round element** may show joins, leaves, connection states, pings or counts of other participants (rule 24). Language rules are in the Player Fantasy section.

> **📌 UX Flag — Session & Travel**: This system has UI requirements. In Pre-Production, run `/ux-design` for the Host/Join menu, the lobby panel and the connection overlays before writing epics. Stories should cite `design/ux/[screen].md`. Lobby UI (VS) will supersede the lobby panel.

## Cross-References

| This GDD references | Where | What | Status |
|---|---|---|---|
| participant-state-roster.md | Rules 2, 6, 11, 13, 15-17, 22; States | Canonical API (R4 mapping), connection projection (R11), `Expired` is not `Retired`, supersede mapping drop, rejoin secret gate (R3), `ResetAll` order (R5, R7) | Read (written). Adopted in the 2026-10-24 review. |
| tuning-data-config.md | Rules 7.2, 11.3-11.7, 21; Interactions | `SchemaVersion`, preset store and `?Preset=` (CR-4), `TuningHash` never replicated (CR-7), public snapshot and epoch (CR-8), single acknowledgement with client-recomputed `ClientHash` (CR-9) | Read (revised 2026-10-24). Adopted: change requests 2(a)-(g). |
| systems-index.md | row 5, Overview bullet, High-Risk row | Scope, the "PlayerState recreated each round" risk | Consistent |
| game-concept.md | Technical Considerations | Listen server through VS, ServerTravel with seed, 50 KB/s budget | Consistent |
| round-match-framework.md | Rules 8, 11.1-11.2, 12, 20; Interactions | Start order (R5), Preparing barrier (R6 P5), disconnect policy (R17), `ShouldAdmitExpired` (R18), phase table | Read (written). Conflicts resolved here (rule 11.2); remaining ones are change requests below. |
| level-socket-contract.md, character-pool-appearance.md | Rules 11.5-11.6, 11.9, 14 | LSC R4.7 acknowledgement condition, fallback travel, `LayoutHash`; CPA CP-18 `PoolHash` | Read (written). Adopted (single acknowledgement). |
| information-visibility-policy.md, procedural-city-generator.md, crowd-npc-behaviour.md, shared-character-locomotion.md, objectives-chain.md, hud.md | Rules 14, 24, 25 | Allowlist, `OnLayoutReady`, sub-seeds, hand-over, pawn shape | Designed (verified 2026-10-24, cross-review C-W11) |

**Change requests to other GDDs** (the orchestrator applies them; this GDD does not edit them). Round also receives the `ShouldAdmitExpired` policy hook (rule 8), the rule-11.8 reveal step, and the host-rotation and arrival-time question (rule 3):
1. **Roster**: adopt the rule-17 field scope table (already largely adopted). Drop any remaining text mapping Session `Expired` to `Retired` (Session now follows R11). Confirm `NotifyConnectionChanged` is called on every Session state change with the R11 projection, and that a fresh-login rejoin re-associates the existing PlayerState. Record the provisional `RejoinToken` (rule 6) as the server-issued rejoin secret that R3 requires before the first external playtest.
2. **Round & Match**: push `SetAdmissionMode` on every phase change. Own every consequence of `OnParticipantUnresponsive`, `Disconnected`, `Rejoined` and `RejoinWindowExpired`. Adopt or reject the recommendation to hand a silent player's pawn to NPC control at Soft and give it back on rejoin. Interleave pawn spawns from `GetServerSubSeed(SpawnOrder)`. Gate Preparing → Active on "no participant Traveling" (F7) instead of a timer of its own. Record the host identity per round in the Round Event Log (rule 3). **2026-10-24 review additions**: (a) R6 P5 barrier waits on Session's `OnParticipantTravelComplete` only: remove `TuningAckTimeoutSec` (retired by TDC), `PoolResidentTimeoutSec` and the separate TDC and pool acknowledgements, and remove the "half or more of the connected clients mismatch" quorum abort (TDC CR-9: mismatches only disconnect the mismatching client). (b) R5 order is kept (lock, `ResetAll`, Preparing + `RejoinOnly`, `RequestRoundTravel`); Session rule 11.2 now accepts `RejoinOnly`. Pass the `PresetId` read from TDC's preset store. (c) Handle the Session refusal codes of rule 11.2. (d) Push `Open` on entering Lobby after `OnLobbyTravelComplete` (rule 12); Session no longer opens admission itself. (e) Use `RequestFallbackTravel` for the Level Socket Contract graybox fallback (rule 11.9). (f) Make the client loading screen drop on the replicated Active reveal flag (rule 11.8). (g) Reconcile R18 `ShouldAdmitExpired` (true only in Active/Ended for non-Active participants) with Roster R11 ("must return true for every `InRound` record"); Session asks and obeys either way.
3. **Tuning Data & Config**: add a latch class `Session` to CR-11 (read at session open, not refrozen per round) and register the `Session` sub-struct with the 13 keys of Tuning Knobs (including the new `TravelAckGraceSec` and the renamed `DisplayNameCharLimit`). Add the cross-field invariants `SessionHardTimeoutSec >= SessionSoftTimeoutSec + 2`, F4 adequacy at the measured inputs, `SessionHeartbeatIntervalSec` in {0} or [0.25, Soft / 3], and `ShutdownFlushSec >= 0.3`. Note in CR-9 that the acknowledgement also carries `PoolHash` and is server-requested (`ClientRequestRoundAck` / `ServerRoundAck`). Change requests 2(a)-(g) to Session are applied.
4. **Information Visibility Policy**: include rule 25 (a)-(e) in the replicated-property allowlist and leak tests. Add the frozen in-round name list rule.
5. **Shared Character & Locomotion**: own rule 25d jointly. The pawn's PlayerState, controller and owner references must not differ between player and NPC pawns for non-owners.
6. **Crowd & NPC Behaviour**: the baseline idle-pause catalogue must include **common** pauses of at least `SessionSoftTimeoutSec + 1 s` (provisional target: at least 10% of visible NPCs paused at any moment). Add a Baseline and Tell Matrix row: "connection stall / hand-over" (deviation: none permitted; NPCs do it: yes, via common pauses; owner: Round + Crowd). Execute any Round-requested hand-over through the pawn command interface with Crowd-identical blending.
7. **Level Socket Contract / Generator**: use only `PublicSubSeed` for geometry. Write `LayoutAttempt` and `LayoutHash` to `FRoundReadiness`. Use integer-only generation.
8. **Character Pool, Objectives Chain**: draw deals and objective-site selection from `GetServerSubSeed`, never from `Seed`.
9. **Systems index**: add Tuning Data & Config and Character Pool & Appearance as soft dependencies of Session & Travel.
10. **Character Pool & Appearance**: CP-18 residency is confirmed by Session's single acknowledgement, which carries `PoolHash` (rule 11.6); retire `PoolResidentTimeoutSec` and the separate pool acknowledgement (the F7 deadline bounds the wait; add any extra pool load time to `TravelLoadTimeoutSec`). A `PoolHash` mismatch gives `ParamsMismatch`, not Reason `Disconnect`. Change request 12 ("include `PoolHash` in the join-compatibility check") is met by the acknowledgement, not the join gate.
11. **Level Socket Contract**: the acknowledgement echo is `(Seed, LayoutAttempt, LocalLayoutHash, ClientHash, PoolHash)`, not `(..., TuningHash)` (LSC R4.7 and its AC); the fallback uses `RequestFallbackTravel` (rule 11.9); the travel deadline is F7's `max(t_travel + TravelLoadTimeoutSec, t_ready + TravelAckGraceSec)`, so `LayoutTotalBudgetSec` no longer has to stay below `TravelLoadTimeoutSec`.


**Change requests received (reconciliation 2026-10-24)**

| From | Request | Disposition |
|---|---|---|
| TDC change requests 2 (a)-(g) | `ClientHash` echo, explicit resend, `ParamsMismatch`, `PresetId` from the store, `TuningSchemaVersion`, rejoin-only admission and absolute deadline, `ParticipantId` keying | ALREADY DONE (2026-10-24 review; TDC also registered the 12 keys with latch `Session` and I-ST-1 to I-ST-3) |
| Character Pool CR-12 | `PoolHash` in the single acknowledgement; `AppearanceDeal` consumed with `Attempt = 0` | ALREADY DONE (rule 11.6); APPLIED (F5 Domain row) |
| Crowd CR-11 | AC 46 (a) names `CrowdMaxIdleSec`; rule 25f's 10 % target is AC-CNB-12 | APPLIED |
| Level Socket Contract CR-3 | server-only domains `EmergencePick`, `RespawnPick`, `AmbientSchedule` (counter-based) | APPLIED (F5 domains 7-9, Attempt row) |
| Objectives CR-5 | sub-seed domain `StageSignal` | APPLIED (F5 domain 10) |
| Round CR-4 / CR-14 (redirected from TDC) | `SeqOrder` and `TieRank` sub-seed domains | APPLIED (F5 domains 11-12) |
| Generator CR-2 | `Attempt` range names LSC's `LayoutMaxReseedAttemptsCount`; redraw `Seed` in the golden set | APPLIED (F5) |
| Generator CR-10 | close Open Question 6 | APPLIED (OQ 6 closed) |
| Round CR-3 (a)-(e) | no lag compensation; hand-over at Soft adopted; `ShouldAdmitExpired` per R18; round-scope reset precondition; ping, inactive flag and PlayerState presence covered by rule 25 | ALREADY DONE (rules 8, 22, 25 (a)-(b); OQ 5) |
| Round CR-13 (a) | `RejoinWindowSec` hard minimum 30 -> 60; F4 table and knob consistent | APPLIED |
| Round CR-13 (b) | `CanRequestRoundTravel()` with no side effects | APPLIED (rule 11.2) |
| Round CR-13 (c) | flag `Disconnected(Kicked)` on a host kick | ALREADY DONE (reason `Kicked` on `OnParticipantDisconnected`; wording tied to Round R15 (c) in Edge Cases) |
| Round CR-13 (d) | add a world-ready stage to the single round acknowledgement (Round P7) | PENDING-USER (schema_changes: a new stage and message in the acknowledgement protocol; active.md). Until decided, the acknowledgement keeps its current fields and Round's P7 relies on its own `WorldReadyTimeoutSec` wait |
| Round CR-13 (e) | stamp `ConnectionGeneration` on every command item the IIF submits | APPLIED (Interactions; server-only stamp, no replicated field) |
| Round CR-13 (f) | `ShouldAdmitExpired` true for every `InRound` record in Preparing, Active and Ended | ALREADY DONE (rule 8 asks Round's hook and obeys; Round R18 defines it) |
| Round CR-13 (g) | travel cancel for a host abort before P1 | APPLIED (rule 12: `RequestLobbyTravel` accepted in Preparing as a cancel) |
| Roster OQ13, OQ14 | server-issued rejoin secret before the first external playtest; Roster API names and projection | ALREADY DONE (rule 6 `RejoinToken`, OQ 9; rules 16, R11 projection) |
| Cross-review 2026-10-24 C-W11, C-W13 | Stale "not written" claims; Teammate Markers missing from Downstream | APPLIED (cross-review fixes 2026-10-24): Dependencies intro and Cross-References status; Teammate Markers downstream row |
| ADR-0005 sync 2026-10-24 | Rule 16: in 5.8.4 seamless travel replaces both PlayerController and PlayerState (E5); bind by peer key (`FSessionPeerKey`), rebuild views on arrival | APPLIED (rule rewritten to the ADR mechanism; `BindConnection(PeerKey, PlayerState)`; gameplay unchanged; logged as a design decision). Bookkeeping note (rule 11.6) and the travel-marker edge case aligned |
| ADR-0005 sync 2026-10-24 | Rule 7: client options are "never read" (new-player initialisation without the engine default), allowlist gains `RejoinToken` (rule 6) | APPLIED (rule 7 and the unknown-options edge case) |
| ADR-0005 sync 2026-10-24 | Rule 2 `[verify]` (host skips `PreLogin`) verified (E2); rule 15/22 verify tags (PlayerState recreation, login pawn spawn, inactive-player override); OQ 2 and OQ 11 | APPLIED (tags replaced by the verified facts; OQ 11 answered, OQ 2 mostly answered, keepalive cadence still open) |
| ADR-0006 sync 2026-10-24 | F5: register domain `CrowdBehaviour = 13` (Crowd CNB-R14, keyed by `LayoutAttempt`); `H64` = SipHash-2-4; ADR-0006's 2026-10-24 amendment appends `BushSpotPick = 14` (ADR-0017) | APPLIED (F5 Domain and Attempt rows; H64 sentence) |
## Acceptance Criteria

*qa-lead consulted. qa-lead independently re-computed the F5 golden vectors. The ambiguities it flagged are resolved in the rules: the `Refused` reason code for a kicked key, manual rejoin after `ParamsMismatch`, the spawn-order threshold, and the rule-3 test method.*

**Test tags**:
- **[U]** engine-free unit test with an injected clock and fakes.
- **[PIE-n]** n-client PIE.
- **[MP]** multi-process run, because a crash cannot be simulated inside single-process PIE.
- **[S]** static reflection or config check.
- **[SPIKE]** needs net emulation at 100 ms RTT and 1% loss.
- **[10P]** sign-off needs a full 10-player test.
- **[PLAYTEST]** a recorded human trial; evidence in `production/qa/evidence/`.

Any criterion without [10P] is verified only at its stated client count, and the handoff report must call full-capacity behaviour untested until the [10P] run. Every test name below is written as its `<Scenario>` and its full automation name is `StealthGame.Session.<Scenario>`. Tests live under `Source/<Module>/Private/Tests/`; **[U]** means a world-less automation test with injected clock and fakes (an Unreal automation test cannot be fully engine-free).

**A. Hosting**
1. GIVEN a player chooses Host, WHEN the session opens, THEN a host record exists with the host's IdentityKey and no join handshake ran for it. `HostRecordSeeded` [PIE-1] (pending the rule-2 [verify] item)
2. GIVEN a host and one client, WHEN liveness is evaluated over 60 s of fake time, THEN the host never leaves `Connected`. `HostNeverUnresponsive` [U]
3. GIVEN a host and one client, WHEN both render their HUD and player list, THEN (a) a binding audit finds no host widget reading server-side state, and (b) for a host and a client on the same team (equivalent viewers), a diff of the serialised HUD view-model shows identical lists and markers. `HostParityBindings` [S] + [PIE-2]

**B. Admission**
4. GIVEN each cell of the rule-8 table, WHEN a key in that state connects in that admission mode, THEN the outcome and reason code match the cell, and the two `Open`-mode "n/a" cells are asserted unreachable. `AdmissionMatrix` [U] (16 outcomes: the `Expired`/`RejoinOnly` cell is tested with the hook returning true and false)
5. GIVEN several gates fail at once, WHEN a handshake arrives, THEN the reason is that of the first failing gate in rule-7 order. `GateOrder` [U]
6. GIVEN join options `Team=`, `SpectatorOnly=` and `Seed=`, WHEN the client is admitted, THEN no system reads them, and the participant is neither a spectator nor on a client-chosen team. `StripsUnknownOptions` [U] + [PIE-2]
7. GIVEN joins named "Alex", "Alex", one with a control character and one over-length, WHEN they are admitted, THEN the names are sanitised, truncated and suffixed, and a later rejoin matches by key and never by name. `DisplayNameSanitize` [U]
8. GIVEN a client with a different `ProtocolVersion` or `TuningSchemaVersion`, WHEN it connects, THEN it is rejected `VersionMismatch` and no record is created or changed. `VersionGate` [PIE-2]
9. GIVEN a key the host kicked earlier this session, WHEN it connects in any mode, THEN it is rejected `Refused`. `KickedKeyRefused` [U]

**C. Travel and seed**
10. GIVEN each rule-11.2 precondition violated in turn, WHEN `RequestRoundTravel` is called, THEN it is rejected with exactly the matching code (`TravelInProgress`, `AdmissionClosed`, `ParticipantUnresponsive`, `ParticipantConnecting`, `RoundScopeNotReset`), no state changes, AND with admission `RejoinOnly` and every precondition met the call is accepted. `RoundTravelPreconditions` [U]
11. GIVEN URL `Seed` values that are missing, duplicated, `abc`, `0`, `-1` and `2147483648`, WHEN the match map initialises, THEN `OnTravelFailed(BadParams)` is raised and no default seed is used; AND GIVEN `1` and `2147483647`, THEN `FRoundParams.Seed` equals that value. `SeedParseRejects` [U]
12. GIVEN a round start with 2 clients, WHEN the travel completes, THEN both clients' replicated `FRoundParams` equal the server's in one update (never a partial struct), AND a static check finds no client code path that reads the travel URL options. `RoundParamsAtomic` [PIE-2] + [S]
13. GIVEN all 24 arrival orders of {map loaded, `FRoundParams` + `FRoundReadiness`, TDC snapshot valid for the requested epoch, `ClientRequestRoundAck` received} with the PlayerController present, WHEN the client becomes ready, THEN exactly one `ServerRoundAck` is sent per `AckSeq`, carrying the client's own recomputed `ClientHash`; AND GIVEN a PlayerController carried over from the previous round with an acknowledged marker for the old epoch, THEN the new request is still answered. `AckOrderIndependent` [U]
14. GIVEN a client whose (a) `LocalLayoutHash`, (b) `ClientHash` (one public tuning field corrupted through the test hook) or (c) `PoolHash` differs from the server's, WHEN it acknowledges, THEN the server sends exactly one further `ClientRequestRoundAck` with `AckSeq + 1` and no replicated struct is re-sent; a second mismatch gives `Disconnected(ParamsMismatch)` with auto-rejoin stopped, a manual rejoin inside the window is admitted, and the round is not aborted. `AckMismatchOnce` [U]
15. GIVEN a 3-client round start, WHEN (a) one client closes its connection cleanly during travel, THEN that participant becomes `Disconnected(ConnectionClosed)` in the same tick and the other two reach `Connected` `TravelDropIsolated` [PIE-3]; AND WHEN (b) one client's acknowledgement is suppressed by a fake, THEN it becomes `Disconnected(TravelTimeout)` at the F7 `Deadline` ± `LivenessCheckIntervalSec` and the others are unaffected `TravelTimeoutIsolated` [U]
16. GIVEN `LobbyMap == MatchMap`, WHEN lobby travel is issued, THEN seamless same-map travel completes and admission returns to `Open`. `SameMapLobbyTravel` [PIE-2] [SPIKE] (provisional)

**D. Persistence**
17. GIVEN a live connection, WHEN the same key connects again, THEN the old connection closes `Superseded`, `OnParticipantRejoined(Superseded)` fires once, and `OnParticipantDisconnected` does not fire. `SupersedeNoDisconnect` [U]
18. GIVEN 3 consecutive rounds with 2 clients, WHEN each travel completes, THEN every Match-scoped field in rule 17 is unchanged, lifecycle is reset only by an explicit `ResetRoundScope` call, and pawn bindings are invalid after `OnTravelStarting`. `FieldScopeAcrossTravel` [PIE-2]
19. GIVEN the PlayerController class, WHEN reflected, THEN it declares no authoritative Session or Roster state. `NoStateOnController` [S]
20. GIVEN an informant who disconnects and rejoins inside the window, WHEN the rejoin completes, THEN the participant is the same record, still an informant, and on the same team. This reuses Roster's anchor criterion 3. `RejoinSameRecord` [PIE-2]

**E. Liveness, disconnect and rejoin**
21. GIVEN a packet arriving in the Hard-threshold tick, and separately a rejoin arriving in the window-lapse tick, WHEN those ticks run, THEN the arrival wins: no disconnect in the first case, no expiry in the second. `SameTickOrdering` [U]
22. GIVEN a rejoiner who drops mid-rejoin, WHEN they reconnect, THEN no second disconnect event fires and the window still ends at its original time. `WindowNotExtended` [U]
23. GIVEN a window that lapses, WHEN expiry runs, THEN `OnRejoinWindowExpired` fires exactly once; a later in-round attempt is admitted as a rejoin if a fake Round's `ShouldAdmitExpired` returns true, and rejected `InProgress` if it returns false. `ExpiryOnce` [U]
24. GIVEN a disconnect in Lobby, WHEN it is processed, THEN the slot is released at once, the record is Dormant, and the same key is readmitted as itself while capacity allows. `LobbyNoWindow` [U]
25. GIVEN a client process killed mid-round, WHEN the player relaunches, THEN the client retries the cached address and key every `ClientAutoRejoinIntervalSec`, is admitted as a rejoin, and stops retrying on a non-retryable reason. `ClientAutoRejoin` [MP]
26. GIVEN each illegal transition listed under States and Transitions, WHEN it is forced, THEN an assertion fires and the state is unchanged. `IllegalTransitions` [U]
27. GIVEN the live engine timeout configuration at session open, WHEN Session validates it, THEN the connection timeout is ≥ `max(SessionHardTimeoutSec, T_prep_max) + 30` (105 s at defaults) and the initial-connect timeout is ≥ `PendingLoginTimeoutSec + 10`, and a violating config logs an error naming the setting. `EngineTimeoutConfig` [S] + [U]

**F. Pillar-1 presentation**
28. GIVEN an Active round with 3 clients, WHEN an opponent drops and rejoins, THEN a non-teammate client records zero changes across the whole window in its notice log, frozen name list, player-array membership, inactive flags and allowlisted PlayerState fields. This also covers Player Fantasy anchor 2. `SilentDropRejoin` [PIE-3]; sign-off [SPIKE] [10P]
29. GIVEN the PlayerState, pawn and game-state classes, WHEN reflected, THEN the per-player connection fields in rule 25b are owner-only or constant, and player and NPC pawns export the same replicated reference set. `LeakAllowlist` [S]
30. GIVEN a rejoin with 3 clients, WHEN a non-owner client inspects the rejoiner's pawn, THEN its owner, controller and PlayerState references are unchanged before and after and are indistinguishable from an NPC pawn's. `PawnShapeStable` [PIE-3]
31. GIVEN an in-round connection refused `InProgress`, WHEN the in-round clients are observed, THEN none receives an RPC, replicated change or message. This covers anchor 3, first part. `RejectionPrivate` [PIE-3]
32. GIVEN the 1,000 fixed seeds of the `SpawnOrderSeeds` fixture, each shuffling 10 players among 40 NPCs, WHEN the player ranks are bucketed into deciles, THEN the chi-square statistic is < 21.67 (df 9, p = 0.01) and neither the first nor the last spawned actor is a player in more than 25% of shuffles (expected 20%). Deterministic because the seeds are fixed. `SpawnOrderUncorrelated` [U]
33. GIVEN every Session player-facing string, WHEN it is scanned, THEN no banned term from the Player Fantasy language rule appears. `BannedTerms` [S]

**Formulas**
34. **F1** GIVEN TeamSize 5, 9 `Connected` and 1 `Connecting`, WHEN B connects, THEN B is rejected `Full`; WHEN the pending handshake then fails, B's retry is admitted. `CapacityF1` [U]; sign-off [10P]
35. **F2** GIVEN a fake clock and a client silent from t = 0, WHEN liveness is evaluated at the defaults, THEN Soft fires in [3.00, 3.30] s and Hard in [12.00, 12.30] s; with a single 2.0 s host frame, Hard fires by 14.2 s; with 20 consecutive 0.5 s frames, no other client is marked Unresponsive. `DetectionBoundF2` [U]
36. **F3** GIVEN the spike's measured RTT_p99 and R_c, WHEN the config is validated, THEN `SessionSoftTimeoutSec >= 2*RTT_p99 + 1` and `>= 3/R_c`; AND a 30-minute 3-client soak at 100 ms / 1% loss logs zero false Unresponsive events. `FalseTimeoutF3` [S] + [PIE-3] [SPIKE]; sign-off [10P]
37. **F4** GIVEN the spike's measured `t_load` and `T_srvload` and the playtest's `t_restart`, WHEN F4 is evaluated, THEN `RejoinWindowSec >= 1.5 * (t_restart + t_connect + t_load + A) + T_busy`. `RejoinWindowF4` [S] [SPIKE] [PLAYTEST]
38. **F5** GIVEN Seed 123456789, THEN `Mix32` returns 2107646860 (domain 1, attempt 0), 523860287 (attempt 1) and 313478722 (domain 2) using `uint32` operands and logical shifts. Over a fake 99-round match, Seed stays in [1, 2^31-1] and never repeats. `ServerSubSeed` is unchanged when only Seed changes. `PublicSubSeed` refuses every domain except Layout. `SeedDerivationGolden` [U]; AND in a [PIE-2] run the captured server log, client logs, travel URLs and a reflection of every replicated property contain no byte sequence of `ServerSecret` or `RejoinToken`. `SecretNeverExported` [PIE-2] + [S]
39. **F6** GIVEN 3 clients at 100 ms RTT on one machine (timestamps from the shared platform clock), WHEN the host quits, THEN every client shows "The session has ended." within 0.55 s plus one frame; WHEN the host process is killed, THEN within 12.3 s; AND GIVEN a client loading a map for 20 s with no server traffic, THEN it does not declare the session ended. No client becomes host or stays in the transition map. This covers anchor 3, second part. `HostLossF6` [MP] [SPIKE]; sign-off [10P]
40. **F7** GIVEN a fake clock, readiness data published at t = 18.2 s and one client that never acknowledges, WHEN time advances, THEN that client is `Disconnected(TravelTimeout)` at t ∈ [60, 60 + `LivenessCheckIntervalSec`]; AND GIVEN readiness published at t = 55.2 s, THEN the deadline is 70.2 s for every participant; AND a disconnect plus rejoin during Preparing does not move the deadline. `TravelDeadlineF7` [U]
41. **F8** GIVEN the shipped config, THEN `SessionHeartbeatIntervalSec == 0`, and no heartbeat RPC is declared on any pawn class. `HeartbeatDisabled` [S]

**Anchor and cross-system**
42. **Anchor 1, new city, together**: GIVEN participants in round 1, WHEN round 2 starts, THEN no client shows the main menu, every client's accepted acknowledgement carries identical `(Seed, LayoutAttempt, LocalLayoutHash)` and a `ClientHash` equal to the server's for round 2's epoch, no replicated property holds `TuningHash`, and all keep their team and name. `AnchorNewCity` [PIE-2]; sign-off [10P] [SPIKE]
43. **Roster rebind**: GIVEN a seamless-travel arrival and a fresh-login rejoin, WHEN each binds, THEN the first calls `RebindView` and the second `RegisterParticipant` (returning `Reclaimed`) exactly once, both reach the same record, and the player array has the same entries before and after. `RosterRebindBothPaths` [PIE-2]
44. **Round event contract**: GIVEN a fake Round subscriber over a scripted session, WHEN events fire, THEN same-tick events arrive in ascending ParticipantId order, and Session never calls `RequestTransition` or spawns, destroys or hides a pawn or PlayerState. `EventContract` [U]
45. **Visibility allowlist coverage**: GIVEN the Visibility Policy allowlist, WHEN it is diffed against the reflected replicated properties, THEN every field named in rule 25 is listed with its audience. `AllowlistCoverage` [S] (provisional on the Visibility Policy GDD)

46. **Rule 25f, binding and provisional; blocks this GDD's sign-off until the Round and Crowd GDDs define the mechanism**: (a) GIVEN an Active round with 3 clients under net emulation, WHEN a player's connection goes silent for 15 s and then rejoins, THEN in a non-owner client's recording that pawn's longest stationary stretch is ≤ Crowd's `CrowdMaxIdleSec` (20 s), no frame-to-frame position change exceeds the NPC smoothing threshold, and its visibility never toggles. `ConnectionEventBaseline` [PIE-3] [SPIKE]. (b) Playtest sign-off: 20 trials, each showing an observer 5 candidate pawns of which one had the connection event; pass if ≤ 7 correct picks (P(X ≥ 8 | p = 0.2) ≈ 0.03), recorded in `production/qa/evidence/`. [PLAYTEST]
47. **Rule 25g, provisional pending spike and the Shared Character GDD (which owns the smoothing threshold)**: GIVEN a 2 s loss burst on one client while its pawn walks, WHEN it is observed from a non-owner client, THEN the pawn decelerates to idle (no dead stop mid-stride), and on recovery the correction shows no positional snap larger than the NPC smoothing threshold. `SubThresholdLagSmooth` [PIE-2] [SPIKE]
48. **Rule 11.8**: GIVEN the host and 2 clients with injected load delays of 0, 5 and 10 s, WHEN round travel runs, THEN on every client, the host included, the loading widget is visible and world rendering is off until the client logs `OnReveal`, every client logs `OnReveal` with the same server frame number (the frame Round enters Active), and a rejoiner keeps the loading screen until its acknowledgement completes. `SimultaneousReveal` [PIE-3]

**Added by the 2026-10-24 review (rules without a criterion before)**
49. **Rule 11.6, stale and foreign acknowledgements**: GIVEN an outstanding request `(RoundIndex 2, Revision 0, AckSeq 0)`, WHEN acknowledgements arrive for round 1, for `AckSeq` 1 before it was issued, twice for `AckSeq` 0, and from the old connection after a supersede, THEN none changes the participant's state, each is logged at most once per second, and the valid one is still accepted. `AckStaleEpochDropped` [U]
50. **Rule 11.6, keying and privacy**: GIVEN 3 clients mid-acknowledgement, WHEN every replicated property of the GameState, PlayerStates and PlayerControllers is reflected, THEN none holds acknowledgement state, `ConnectionState` or `TuningHash`, AND the acknowledgement record is keyed by `ParticipantId` and cleared at the next travel start. `AckBookkeepingPrivate` [S] + [U]
51. **Rule 15, supersede re-acknowledges**: GIVEN an Active participant, WHEN the same key (with the right `RejoinToken`) connects again, THEN the participant becomes `Rejoining`, the new client must pass the acknowledgement, `OnParticipantRejoined(Superseded)` fires once on completion, no second slot is used, and the window and deadline are not restarted. `SupersedeRequiresAck` [U]
52. **Rule 12, lobby travel**: GIVEN `RequestLobbyTravel` in Ended (and again in Reset), WHEN it runs with one client that never answers the lobby acknowledgement and one record `Disconnected`, THEN the silent client becomes `Dormant` at `t_travel + TravelLoadTimeoutSec`, the `Disconnected` record becomes `Dormant`, their slots are released, `OnLobbyTravelComplete` fires once, and admission stays `Closed` until Round pushes `Open`; AND a call in Active is refused. `LobbyTravelDormancy` [U]
53. **Rule 14, sub-seeds are server-only**: GIVEN the built client and server modules, WHEN reflected and statically checked, THEN no client-callable function or RPC reaches `GetServerSubSeed`, and calling it without authority returns an error and raises an `ensure`. `ServerSubSeedServerOnly` [S] + [U]
54. **Rule 20, admission push**: GIVEN a fake Round, WHEN it pushes `Open` during a travel in progress, THEN the effective mode stays `Closed` until the server's map load completes and then becomes `Open`, AND a static check finds no Session reference to Round's phase. `AdmissionModePush` [U] + [S]
55. **Rule 22, no pawn at login**: GIVEN a round in Active with 2 clients, WHEN one disconnects and rejoins, THEN the server's pawn count and the player array are identical before, during and after, and no pawn is spawned by login. `NoDefaultPawnSpawn` [PIE-2]
56. **Rule 9, pending login**: GIVEN a handshake that never completes, WHEN `PendingLoginTimeoutSec` elapses, THEN the connection is closed, its slot is released and no Roster record exists. `PendingLoginTimeout` [U]
57. **Rule 19, responsive again**: GIVEN an Unresponsive participant, WHEN one packet arrives, THEN it is `Connected` in that tick and `OnParticipantResponsive` fires exactly once. `ResponsiveAgain` [U]
58. **Rule 6, identity**: GIVEN 3 PIE instances, THEN each has a distinct IdentityKey, AND no replicated property or log line contains more than a 4-byte prefix of any key; AND GIVEN a rejoin with the right key but a wrong `RejoinToken`, THEN it is rejected `Refused` and the live connection is untouched. `IdentityKeyPrivate` [PIE-2] + [S] + [U] (token part provisional)
59. **Kick**: GIVEN an Active participant, WHEN the host kicks them, THEN the disconnect reason is `Kicked`, the participant is `Expired` with no window in the same tick, `OnParticipantDisconnected(Kicked)` precedes `OnRejoinWindowExpired`, and a later attempt with that key is rejected `Refused` even while admission is `Closed`. `KickedSlotReleased` [U]
60. **Rule 10, names fixed**: GIVEN a rejoin whose requested display name differs, THEN the record keeps its original name. `DisplayNameFixed` [U]
61. **Rule 24, lobby notices**: GIVEN 3 clients in Lobby on two teams, WHEN one joins and one leaves, THEN every client logs "Name joined" / "Name left", and each client's lobby list shows team only for its own teammates. `LobbyNotices` [PIE-3]
62. **UI strings**: GIVEN each rejection reason code, WHEN the client receives it, THEN the overlay text equals the UI Requirements sentence exactly `RejectionText` [U]; AND a retained screenshot of each status overlay is stored in `production/qa/evidence/` (UI story evidence).
63. **Game feel**: GIVEN LAN and 100 ms emulation, WHEN a client presses Join, THEN it appears in the lobby list in under 3 s (LAN) and 5 s (100 ms); a rejection appears within 1 RTT plus one frame of the gate decision; and a round transition takes at most the slowest client's load time plus 1 s. `JoinLatency` [PIE-2] [SPIKE]
64. **Rule 11.9, fallback travel (provisional)**: GIVEN Preparing with a fake generator that fails every attempt, WHEN Round calls `RequestFallbackTravel`, THEN `Seed`, `PresetId` and `RoundIndex` are unchanged, a new epoch and `t_travel` start, and every client acknowledges again on the graybox map. `FallbackTravelReacks` [U]

## Open Questions

| # | Question | Owner | Resolve by |
|---|---|---|---|
| 1 | Does seamless travel to the same map through a transition map work reliably in 5.8.4, or do we need a distinct `LobbyMap` from day one? | network-programmer (spike) | Before the round-loop story |
| 2 | **Mostly answered by ADR-0005 (2026-10-24 ADR sync)**: login and seamless-arrival hooks, the listen host skipping `PreLogin`, option parsing, the inactive-player override and PlayerState replacement are verified (E1-E22); still [U]: keepalive cadence (E12, net spike) and the timeout defaults. Original question: verify the engine facts marked **[verify]**: login and seamless-arrival hook names, which client options the engine honours by default, last-receive time updated by keepalives, default and travel-time connection timeouts, the inactive-player override, the PlayerState replicated field list | ue-replication-specialist | Architecture (ADR) |
| 3 | Legacy replication or Iris for the MVP? Leak fixes (rule 25) depend on the choice. The specialists recommend legacy. | technical-director (ADR) | Before Visibility Policy implementation |
| 4 | Measured client packet rate, burst-outage distribution on Wi-Fi, and map load time: they confirm or revise the Soft, Hard, window and travel defaults (F3, F4, F7) | network-programmer (net-emulation spike) | Spike report |
| 5 | **Answered by Round R17.1 (2026-10-24)**: Round adopts the hand-over at Soft (`bHandOverOnUnresponsive`, default on) with a blended hand-back; provisional pending the net spike and the Crowd GDD, which must still meet rule 25f. | -- | closed (Crowd GDD confirms the baseline) |
| 6 | **Answered by the Procedural City Generator (R-G7, LSC R2.2; reconciliation 2026-10-24)**: clients run no generator; they build from the replicated `FLayoutManifest` with no RNG and verify `LayoutHash`. Rule 14 and the single acknowledgement are unchanged; the `?Seed=` URL option is parsed only by Session, never by the Generator. | -- | closed |
| 7 | Should the MVP show a teammate-only "teammate reconnecting" indicator? CD-GDD-ALIGN recommends considering it for Fellowship. It is Pillar-1 safe because it uses audience Team only. Moving it from VS into MVP is a scope change and is left as a pending user decision. Until the user decides, external team voice is a **stated MVP playtest requirement**, and the protocol records team-side confusion after disconnects. | user (scope) / ux-designer | Pending user decision |
| 8 | Host latency advantage: is it measurable in playtests? If it is, should hosting rotate per round or session for fairness until the Dedicated Server? | producer + game-designer | After the first 10-player playtest |
| 9 | The provisional `RejoinToken` (rule 6) must be implemented before the first playtest outside the team (Roster R3; creative-director 2026-10-24). Open: token lifetime across sessions, and the authenticated online-subsystem identity that replaces both key and token | security-engineer + network-programmer | Token: before the first external playtest; platform id: Alpha |
| 10 | Ten-player behaviour (capacity race, nine simultaneous travel acknowledgements, bandwidth) is unverified until a full-capacity test, as required by the build handoff | qa-lead | First 10-player test |
| 11 | **Answered by ADR-0005 §4 (2026-10-24 ADR sync)**: the controller's PlayerState initialisation is overridden to adopt the retained PlayerState, so no PlayerState is created and the player array is unchanged; harness-tested in spike S3. Original question: does the 5.8 fresh-login path create or restore a PlayerState for a rejoiner, and can the new controller be bound to the existing one without a player-array change (rule 16)? | ue-replication-specialist | Architecture (ADR) |
| 12 | Character Pool, Round and the Level Socket Contract must adopt the single acknowledgement (one deadline, `PoolHash` field, no `PoolResidentTimeoutSec`, no tuning quorum); until they do, rules 11.6-11.7 are provisional in those GDDs' terms | producer (change propagation) | Before the round-loop story |
| 13 | Security review not run in this design review (usage limit): handshake-slot exhaustion, supersede griefing, rejection-code probing as an oracle, display-name impersonation | security-engineer | Before the first external playtest |
