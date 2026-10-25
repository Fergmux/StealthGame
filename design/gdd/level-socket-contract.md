# Level Socket Contract

> **Status**: Approved — 2026-10-25 (main-session decision D35 under user authorisation; see production/design-closeout/2026-10-25_rereview-followups.md and design/gdd/gdd-cross-review-2026-10-25b.md). Prior status: Reviewed - NEEDS REVISION 2026-10-25 (second round, lean; 17 revisions applied, re-review pending; first round was MAJOR REVISION NEEDED 2026-10-24); reconciled 2026-10-24; ADR-synced 2026-10-24; user decisions applied 2026-10-25 (`NpcCount` 50; `LayoutExactRefineMaxCount` key; pre-baked route-edge paths stored with `CrowdRoute`; touchable-prop records declined; C1 confirmed; CR-10 renames)
> **Design Review**: NEEDS REVISION 2026-10-25 (lean re-review; 17 revisions applied) after MAJOR REVISION NEEDED 2026-10-24 (26 revisions applied); log: design/gdd/reviews/level-socket-contract-review-log.md
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS 2026-10-24, then REVISED 2026-10-24 (6 revisions applied: zone density cap and contact time, emergence-rate invariant, 4 final candidates, the round-start reading of the amendment recorded, V-DENSITY, verdict header)
> **Author**: design pipeline (autonomous) + level-designer, systems-designer, ue-replication-specialist, art-director, creative-director, qa-lead
> **Last Updated**: 2026-10-25
> **Last Verified**: 2026-10-25
> **Implements Pillar**: P1 Hide in Plain Sight (placement is never a tell); P4 A Race, Not a Hunt (fair, symmetric race)

## Summary

The Level Socket Contract is the rulebook every playable city must pass before a round starts, whether it is the hand-built graybox map or a generated one. It names the places the game needs (start zones, shared alley emergence points, objective courts, bushes, crowd routes, traversal edges, the navmesh) and proves the city is reachable, fair to both teams and free of give-away places. It also decides in secret, on the server, which court is live for which team, and it picks the unseen crowd pawn an informant takes over, so players and NPCs start, enter the crowd and return exactly alike.

> **Quick reference** — Layer: `Core` · Priority: `MVP` · Key deps: `Tuning Data & Config; Session & Travel (seeds, ack), Round & Match Framework (Preparing), Participant State & Roster (soft)`

## Overview

The Level Socket Contract (LSC) defines what any playable level (the MVP graybox, later the Procedural City Generator) must provide and prove before a round starts. It has five parts:

1. **Socket vocabulary**: emergence sockets, start cells and candidate zones (`TeamSpawn`), objective courts that resolve into `ObjectivePrelimSite(Team, Stage)` / `ObjectiveFinalSite`, `BushSite`, `CrowdRoute`, `TraversalPoint`, `NavArea`.
2. **Layout service**: Round's `Preparing` talks to it through `RequestLayout` and `OnLayoutReady(LayoutId, Seed)`, never to a provider.
3. **Validation predicates**: reachability, separation, fairness, bushes, sightlines, sealed regions, emergence occlusion, respawn supply; every threshold is data.
4. **Server-secret Selection**: which zones are used, which zone is whose, and which court is live for which team and stage.
5. **Failure policy**: reject, reseed up to N times, then fall back to the hand-built map, logging every attempt.

For the player the effect is indirect but serves two pillars: under P1, where you start, where people come from and where the objectives are never mark anyone as a player; under P4, the city never decides the race. The system lets Crowd, Objectives and Concealment be built on the graybox first, keeps the Generator off the critical path, and makes "a valid level" one testable definition.

*Framing (autonomous, logged): data layer plus player effect. ADR-0014 (layout service, generation, navmesh) now covers the implementation; remaining choices are marked "-> ADR".*

## Player Fantasy

*Framing: indirect (infrastructure the player never touches). Shaped by creative-director: candidate A "The City Has No Seams" as the lead, B's fairness line second, C's emergence point folded into the anchor.*

**The city has no seams.** The city was there before you arrived, and it does not know you are a player. Every place where you could start, come back or need to go looks like anywhere else on the street. When you come back as an informant you are simply someone already in the crowd, somewhere near your side of the city. The court you are walking to does not announce itself as an objective until somebody starts working it. Whatever you read in the crowd, you read from what people chose to do, never from where they happened to appear.

**Same city, same odds.** Your route to the final site is about as long, as shaded and as crowded as the other team's. When you lose, it is because they read the crowd better than you did. It is never because they got the short side of the map. A new seed means you cannot win by memorising a layout, and the contract's fairness checks mean you cannot lose to one either.

**Anchor moment.** Twenty seconds into the round you are walking a street among strangers, and you could not point out the four people who arrived with you except by their markers. The enemy cannot point out theirs at all. A little later, after you cut down an informant, someone wearing your victim's face steps out of an alley. It is only a citizen, as people step out of alleys all round long. Your victim is already back, walking the far streets in a stranger's look, and nothing on the street showed the switch.

*Pillars served:* P1 Hide in Plain Sight ("until someone commits, an enemy player cannot be told apart from an NPC by model, animation, audio, UI, or network presentation"). Placement is part of presentation. P4 A Race, Not a Hunt: geography never decides the race.

*Language rule for any player-facing or designer-facing text derived from this system:* never "spawn room", "base", "home turf", "safe zone", "spawn protection", "landmark", "lane", "map control" or "materialise". Internal names (`TeamSpawn`, `EmergenceSocket`, `LayoutHash`) never reach the player.

## Detailed Design

### Core Rules

*Specialists consulted: level-designer (spatial feasibility, graybox authoring), ue-replication-specialist (authority, replication, determinism, handshake), art-director (visual rules, used here and in Visual/Audio). Every number below is a data key (see Tuning Knobs). Defaults are provisional until the graybox playtest.*

#### R1. Terms

- **Layout**: one level instance (geometry, sockets, validation result), identified by `LayoutId`: a hash of (Seed, GeneratorKitVersion, LayoutParamsHash) when generated, `Graybox:<MapName>:<MapContentHash>` for the graybox.
- **Provider**: exactly two kinds, `Graybox` (the hand-built map with a `LevelSocketProvider` data component) and `Generated` (the Procedural City Generator).
- **Socket**: a typed record with a stable integer `SocketId` (canonical order), a 2.0 m grid position (cell x/y, course index for height), a yaw index 0-3 and its R3 data.
- **Area type**: each walkable cell has one `AreaTypeId` (`Causeway`, `Market`, `Temple`, `Residential`; art 6.2), driving art density and recipes only. **No area type may exist for spawns, starts or objectives.**
- **Data classes**: *public* (identical everywhere: geometry, Manifest, `LayoutId`, `Seed`, `LayoutAttempt`, `LayoutHash`); *unreplicated* (socket records, routes, nav: derivable from public inputs by a modified client, so they **never encode a secret**; Session rule 14); *secret* (server-only until `Ended`: the role assignment (used zones, zone → team, live sites), the respawn takeover choice and every runtime random choice, drawn only from `ServerSubSeed(Domain, Attempt)`, R9).

#### R2. Layout Manifest and determinism

1. The server builds, validates and freezes the layout, then publishes a **Layout Manifest**: an integer list of every placement with collision or line-of-sight effect (module id, cell x/y, course, yaw index, variant id, planter/bush and fixture placements; cells × 200 cm, 4 exact rotations).
2. Clients build geometry from the Manifest with **no random number generation**, so they build exactly what the server validated.
3. **Cosmetic decoration** (decals, macro value variation, non-colliding ground props, foliage variant picks) is generated client-side from (`PublicSubSeed(Layout, LayoutAttempt)`, `LayoutHash`) with an integer RNG and stable-id iteration. It **has no collision and no nav modifier, and its bounds top is at most 0.5 m above its floor**; anything with collision or blocking sight in the 0.5-2.2 m eye band belongs in the Manifest.
4. **Same seed, same socket set**: the socket set and Manifest are a pure function of (Seed, GeneratorKitVersion, LayoutParamsHash), never of player count, connection order, time, frame or world state.
5. The Manifest carries **no role data**: a court in the Manifest is a court.
6. Bush hide volumes and entry axes, court fixtures and traversal coping are built locally from the Manifest as non-replicated components; each bush's `BushId` is its SocketId. **The contract spawns zero replicated actors** (ue-replication-specialist); any future per-court or per-bush replicated object must exist on **every** court or bush with identical class and net settings, spawned in SocketId order.
7. For the graybox the geometry is the map package: its `FLayoutManifest` has an empty entry list and its `LayoutHash` is the cook-time hash of collision and sight-relevant geometry, so a different map build is detected.

#### R3. Socket types and required data

| Type | Required data (all distances are data keys) | Who needs it | Replication |
|---|---|---|---|
| **EmergenceSocket** (shared portal point; art bible 5.2.1) | `Kind` = `AlleyMouth` (MVP; `Doorway` reserved, not valid in MVP because M02 doors are sealed). `OriginCell` (hidden, at least 1.2 × 1.2 m). `ExitPoint` (on the navmesh, 2-6 m walk from the origin). Witness set `W(O)` (R5). `AreaTypeId` | Crowd (NPC replacement, ambient entry/exit) | Not replicated |
| **StartCell** | A plain walkable cell on or within 3 m of a `CrowdRoute` node, a **player slot** only. `ZoneId` (0-3; never a team). Within `StartCellMaxRadiusCm` (16 m, D50) of its zone anchor | Round (player placement) | Not replicated |
| **TeamSpawn(Team)** (resolved by Selection) | A candidate zone: the start cells with one `ZoneId`, at least `TeamSize + 1` of them (default 6, hard 6-8), at least `StartCellSpacingCm` (8 m) apart. `Anchor` = cell centroid snapped to the navmesh. The **density disc** is radius `TeamZoneRadiusCm` (35 m) round the anchor (D6). The **respawn region** `RespawnRegion(zone, partner)` is every walkable cell within `InformantRespawnPathCm` (45 m path) of the anchor that is strictly nearer by path to this anchor than to the anchor of the partner zone of the used pair (R5b), so the two regions of a pair are disjoint | Round, NPC-Kill Penalty & Informants | Zone geometry not replicated; which zones are used and zone → team are **secret** |
| **ObjectiveCourt** (resolved by Selection into `ObjectivePrelimSite(Team, Stage)` or `ObjectiveFinalSite`) | A 6 × 6 to 8 × 8 m court on the grid (8 × 8 in Market areas). One fixture (a 0.8-1.0 m stone block from a family that appears at least 3 times as decor per site; art 6.8) with `FixtureInteractSideCount` (4, Objectives-owned) stand points, one per face, each on at least 1.2 m of clear floor and all inside the sun pool, built locally from the fixture transform and identical on every court (Objectives OC-8, Penalty CR-13). A `SunPool` rectangle. At least 2 ingress openings. `AreaTypeId`. A role-blind `ClueTag` (area type + nearest distinctive public feature; a pure function of the published geometry, computed by the same rule on every court; its rule version is part of `GeneratorKitVersion`, so D7 covers it). No role hint: eligibility for each role is derived only from the distance predicates (R9) | Objectives Chain | Courts are public geometry; roles are **secret** |
| **BushSite** | A 2.0 × 2.0 m planter cell flush against architecture (wall, platform edge, kerb or court corner). Dome 1.8 × 1.9 m (art 3.6). A `HideVolume` (the planter footprint × 2.25 m), an `EntryAxis` set (the open sides), a `PrincipalApproachAxis` (the longest unobstructed approach within 25 m) and a stable `BushId` | Concealment | Geometry public (built from the Manifest); occupancy is Concealment's |
| **CrowdRoute** | A graph of nodes (cell, `AreaTypeId`, capacity) and edges (width, kind, **pre-baked path**) on the navmesh. Each edge stores its path polyline (navmesh corridor points, both directions share one polyline), baked once in Validate after the navmesh freeze, so Crowd follows an edge with no path query (Crowd CR 17; added by user decision 2026-10-25). Stored server-side with the route graph, never in the replicated Manifest, so the Manifest wire size is unchanged | Crowd | Not replicated |
| **TraversalPoint** | `Type` (`Vault` at `TraversalVaultHeightCm` 0.8 m, `Climb` at `TraversalClimbHeightCm` 1.6 m, both owned by Shared Character & Locomotion; *provisional pending traversal spike*, and a 1.2 m hurdle may be added). Coping edge segment, both landing cells (at least `TraversalLandingClearCm` 1.0 m clear, on the navmesh). Derived from coping-tagged kit variants (M01 vault and climb, platform edges, M10 quay walls), never hand-placed separately | Shared Character & Locomotion | Geometry public (coping is visible by design, art 6.7) |
| **NavArea** | Per-cell class: `Walkable`, `Blocked` (M10 water, voids, sealed volumes). Optional cost classes (UE 5.8 per-mesh nav areas; -> ADR) | Crowd, validation | Not replicated (the navmesh is server-only) |

**Counts per layout** come only from D5 (emergence sockets, bushes, traversal points, courts at least `CourtSpacingCm` 24 m apart). Zones: `ZoneCandidatesMinCount` (2) to 4 candidate zones; Selection uses exactly 2 per round (R9).

#### R4. The layout service and the Preparing handshake

1. A server-side **layout service** (world subsystem; -> ADR) with which providers register. Round talks **only** to the service; Round and the Generator never reference each other (AC-3). `LayoutProviderMode` = `Graybox` (MVP default) | `Generated` | `GeneratedWithFallback`.
2. In `Preparing`, with tuning frozen (TDC), Round calls `RequestLayout(Seed, TuningHash)`. The public `Seed` comes from Session & Travel (F5, URL option, `FRoundParams`); generated geometry uses only `PublicSubSeed(Layout, LayoutAttempt)`.
3. The provider builds (graybox: instant collection of its markers; Generator: time-sliced); the service waits for the server navmesh, then runs **Validate** (R7) and **Select** (R9).
   - Each attempt must finish within `LayoutAttemptTimeoutSec` (7 s) and all attempts within `LayoutTotalBudgetSec` (45 s), after which the service goes straight to R8.3. TDC invariant: `(1 + LayoutMaxReseedAttemptsCount) × LayoutAttemptTimeoutSec ≤ LayoutTotalBudgetSec` (6 × 7 = 42).
   - Each time slice processes at most `LayoutSliceWorkUnitsLimit` cells or rays, so no game-thread frame exceeds 33 ms on the reference machine (AC-36).
4. On a pass the service freezes sockets and Selection and replicates **`FLayoutManifest`** = {`LayoutId`, `ProviderKind`, `GeneratorKitVersion`, `LayoutAttempt`, `LayoutHash` (D7), `Entries`} on the game state to all clients, with an idempotent handler keyed on `LayoutId` (**not** initial-only, so connected clients receive it). It never contains roles, socket records, sub-seeds or a hash of the Selection. 1,500-3,500 entries × 9 bytes = 13.5-31.5 KB raw for generated layouts (up to `CityGenManifestEntriesMaxCount`, 3,500), about 0.3-0.7 s of a 50 KB/s budget once per round (spike measures it). The service writes `LayoutAttempt` and `LayoutHash` into Session's `FRoundReadiness`, then fires server-side **`OnLayoutReady(LayoutId, Seed)`**.
5. **Acknowledgement is Session's single readiness ack** (rule 11.6). The contract adds one condition: a client acks only after building its geometry from the Manifest and recomputing `LayoutHash`; the ack echoes (Seed, LayoutAttempt, LocalLayoutHash, ClientHash, PoolHash) (Session rule 11.6; reconciliation 2026-10-24, Session CR 11). On a mismatch Session resends once, then disconnects with `ParamsMismatch`, and the service raises `OnClientLayoutFailed(Participant, LayoutMismatch)`. Clients that never ack are bounded by Session's travel deadline, F7's `max(t_travel + TravelLoadTimeoutSec, t_ready + TravelAckGraceSec)`; the contract has no ack timer.
6. **Spawn gate (requirement on Round):** no pawn is spawned for, possessed by or made relevant to a client until Session has moved it to `Connected` for this round (geometry built and verified); without collision a client's corrections spiral.
7. Round reads the frozen `FSiteAssignment` and TeamSpawn records only through the service's server-only query API.

#### R5. Emergence: one shared way into and out of the crowd for NPCs

1. **One procedure for all kinds.** Every mid-round appearance of a living **NPC**, and every absorption of one, goes through `RequestEmergence(Kind)` with the same selection rules (removing a dead body is Crowd's corpse cleanup, not an emergence). `Kind` ∈ {`Replacement` (after `OnPawnKilled`, including the replacement for a killed informant's body), `AmbientEntry`, `AmbientExit` (an NPC walks into an origin cell and is absorbed while out of every witness's sight), `PawnRecovery`}. `Kind` never affects the eligible sockets, timing, pose, animation, walk-in path or presentation. **No player pawn emerges in normal play**: informant respawn and a re-admitted informant use the takeover in R5b, a rejoining Active participant gets its own pawn back from crowd AI (Roster), and `RequestEmergence` refuses and logs any other kind. The single exception is `PawnRecovery` (Round R17.9, creative-director ruling; reconciliation 2026-10-24, Round CR 17): an `Active` participant whose pawn was lost without a resolved kill (KillZ, a bug) gets one own-card pawn through any socket, under the **same** clear check and selection as every other kind (the identity-blind R5.3 check is stricter than "out of non-team sight", so it satisfies Round's condition without reading teams).
2. **Static occlusion predicate.** The witness set `W(O)` of an origin cell O is every walkable cell within `EmergenceWitnessRangeCm` (32 m; never below `SightlineMaxCm`) with line of sight to O.
   - Eye samples at 1.6 m standing height, and at 3.2 m on any climb top in range; targets at O's head (1.75 m) and torso (1.0 m). The same samples are used by R5b.
   - `|W(O)|` ≤ `EmergenceWitnessCellsMaxCount` (37 cells, about 150 m²; hard ceiling 55). `OriginCell` at least 1.2 × 1.2 m.
   - Preferred form: Z- or L-shaped alley with legs of 6 m or less. A straight alley must be at least 10 m long with O at least 6 m deep. One socket per alley; sockets at least `EmergenceSpacingCm` (12 m) apart; an ExitPoint at least 6 m from any bush, court or traversal point.
3. **Runtime clear check (viewer-, kind- and identity-blind).** A socket may be used only when no pawn **of any kind** is inside W(O) dilated by `EmergenceWitnessDilationCm` (2 m), no pawn is within `EmergenceClearRadiusCm` (3 m) of the ExitPoint, and at least `EmergenceSocketCooldownSec` (20 s) has passed since its last use. If no socket is clear the request retries every `EmergenceRetrySec` (1 s) in server sequence order; NPC requests wait, so a temporary NPC deficit is allowed. **The clear check is never relaxed**: standing in an alley can deny one socket, never force an emergence in view. Two same-tick requests for one socket resolve in sequence order; the second re-selects in the same tick.
4. **Selection among clear sockets** is uniform random from the `EmergencePick` stream (R9.1), drawn counter-based by request sequence id. (Least-recently-used was rejected: it makes the next socket predictable from the last 15 pops.) Selection **never reads `IsPlayer`, team or participant state**.
5. **Presentation:** the pawn (an NPC, or the rare `PawnRecovery` pawn) is created at O, already occluded from every viewer, and walks to ExitPoint on the normal locomotion path: no teleport, VFX, fade or sound (art bible 5.2.1). If the occlusion predicate cannot be met, **the layout fails**; an effect is never added to hide the problem.
6. **Ambient baseline (binding on Crowd).** Crowd runs ambient entries and exits through the same sockets at a rate of at least `AmbientEmergencePerSocketMinPerMin` (0.5: one entry and one exit per socket every 2 minutes; D9). An emergence must never mean "a kill just happened" (creative-director guard 1).
7. **Kill decoupling and rate bound.** A replacement is never served at the instant of the kill. It takes the next scheduled ambient entry (Crowd's `AmbientSchedule` stream), so entry *times* follow the schedule, never the kill. To conserve the population, one upcoming exit is cancelled, chosen uniformly among the upcoming exits at **all** sockets. Cancelled exits may not exceed `EmergenceExitCancelMaxPct` (25%) of the scheduled exits in any 60 s window; beyond that the replacement is deferred and the population runs short by at most `EmergenceDeficitMaxCount` (3) NPCs until the window allows. Verified by AC-13.

#### R5b. Informant respawn: takeover of an unseen crowd pawn

Decided cross-GDD rule (C1; confirmed by the user 2026-10-25): a killed informant's participant **takes over an existing unseen crowd NPC pawn** near its team's TeamSpawn. No pawn, actor or NetGUID is created.

1. **Request.** NPC-Kill Penalty & Informants calls `PickRespawnPawn(Team, RequestSeq, ExcludePawn)` after its own respawn delay, for **every** informant-body cause (Penalty PEN-R7: killed informant, exile by strike removal, reversal, informant pawn loss). `ExcludePawn` (optional, server-only) names a pawn that may not be returned: the exile body released to the crowd at the strike removal (Penalty CR-5, CR-6). A re-admitted informant awaiting a pawn (Round R18, Roster) uses the same call. The contract returns a pawn or `None`; Penalty then performs `RetireBinding` / `BindPawn` (Roster) and Crowd releases the pawn.
2. **Eligible pawn** for team T: an Unbound NPC pawn that is live, not in an interaction or tending pose, not in an emergence walk-in or pending exit, not claimed this tick, not the request's `ExcludePawn`, inside `RespawnRegion(zone_T, zone_U)`, U being the other used zone (the team's own anchor disc only: path to anchor_T ≤ `InformantRespawnPathCm` **and** strictly shorter than the path to anchor_U, no traversal; never a union of both teams' discs, so the two regions of a pair are disjoint), and **unseen**: no pawn of another team (active or informant) has line of sight to it from any point within `EmergenceWitnessDilationCm` of that pawn, using the R5.2 eye and target samples within `EmergenceWitnessRangeCm` and no view cone. The only team reads are T's own zone, the other used zone and the set of non-team pawns.
3. **Draw.** Uniform over the eligible pawns sorted by server-only stable pawn id, rejection-sampled (no `%`) from `GetServerSubSeed(RespawnPick, RespawnRequestSeq)` (Session F5's counter-as-Attempt shape, ADR-0006/ADR-0014: counter-based and independent per request; the same `RespawnRequestSeq` is reused on every retry of one request, so its stream continues). The unseen test is re-run at the commit tick; a pawn that fails it is dropped and the draw repeats.
4. **Result.** The participant possesses the pawn where it stands; the look is that pawn's existing card (Character Pool). The informant's killed body is removed like any NPC corpse, and Crowd emerges a replacement NPC whose card is `DrawReplacement()` (the victim's when it is the only vacancy, Character Pool CP-12a) through `RequestEmergence(Replacement)` (R5.7) (reconciliation 2026-10-24, Crowd CR 8). The total pawn count is constant.
5. **Starvation.** With no eligible pawn the request retries every `EmergenceRetrySec`. After `RespawnStarvationSec` (10 s) the region widens to 2 × `InformantRespawnPathCm` (still cut by the nearer-anchor rule); after another `RespawnStarvationSec` to any walkable cell not within `InformantRespawnPathCm` of an enemy-used anchor. The unseen test is never relaxed, a bound pawn is never taken and no pawn is ever spawned. Past both widenings the request keeps waiting (the participant stays "awaiting pawn", Roster) and logs `RespawnStarved`.
6. **Supply is validated** by V-RESPAWN (D6b). The same takeover is used for nothing else in MVP.

#### R6. Round-start placement

0. **How the CD-SYSTEMS amendment is read (CD-GDD-ALIGN, logged).** "Used identically by round-start placement" is met by one shared placement function for players and NPCs with the first world frame drawn only after `Active`. Players do not walk out of sockets at t0, because about 60 simultaneous emergences from about 15 sockets would themselves be a tell.
1. Round calls `RequestStartPlacement(PlayersByTeam, NpcCount)` once. The service places **players and NPCs with the same placement function and facing rule**:
   - **Player slots**: each player draws one distinct start cell of its team's zone, uniform from the `PlayerPlacement` stream.
   - **NPC pool**: every walkable cell within 3 m of a `CrowdRoute` node, at least 3 m from any other chosen cell (start cells included). NPCs are placed from it by Crowd's density function, which **may not read `FSiteAssignment`, zone roles or player positions** (server assert).
   - Round owns the spawn order (one shuffled list from its `SpawnOrder` stream, Round F5) and spawns all pawns in one frame.
2. **Zone fill (D6).** Within each used zone's density disc the number of NPCs is drawn uniformly from the window `[NpcZoneMin, NpcZoneMax]` (`PlayerPlacement` stream), and Crowd's density function treats the disc as full. `NpcZoneMin` enforces the density floor `ZoneNpcDensityMinMult` (0.8) and the player share cap `ZonePlayerShareMaxPct` (40%); `NpcZoneMax` enforces the pawn density cap `ZoneDensityMaxMult` (1.5 × the map mean). Zone NPCs across both used zones are at most `ZoneNpcShareMaxPct` (60%) of `NpcCount`. V-ZONEFILL proves the window is at least `ZoneNpcWindowMinCount` (3) wide, so the zone count is never a constant. No enemy can see a zone at t0 (V-SEP); the cap protects against a cluster persisting (AC-20).
3. Placement happens while clients are in the briefing state (art bible 2.3); no world view is rendered until `Active`, so the first frame shows everyone in place. A pawn on a cell next to an alley mouth faces out of the alley, for NPCs and players alike.
4. **TeamSpawn stays the logical anchor** (canonical vocabulary): TeamSpawn = the zone (anchor, start cells, density disc, respawn region); emergence sockets = the NPCs' physical way in and out; informant respawn = takeover inside the respawn region (R5b).

#### R7. Validation predicates

Each predicate has an id, a severity (`Reject` fails the attempt; `Warn` is logged only) and a data threshold; a layout is valid when every `Reject` passes. **All path lengths are navmesh paths without traversal unless a predicate says "with traversal"**, because informants and NPCs cannot use parkour.

| Id | Predicate | Severity |
|---|---|---|
| **V-NAV-1** | One connected walkable navmesh component; no disconnected island of `NavIslandRejectCellsCount` (1) cell or more; smaller islands filled or blocked in both collision and nav (no sealed region). | Reject |
| **V-NAV-2** | Corridor clear width ≥ `NavMinClearWidthCm` (2.8 m, art 6.4); no dead-end court under 6 × 6 m; no recess at least 0.6 m wide and 0.6 m deep (art 6.7). **No one-pawn chokepoint on an objective** (Penalty CR-13, Generator CR 11d): every court ingress and every bridge or stair on a `CrowdRoute` path between courts and zones is ≥ `NavMinClearWidthCm` wide, and every fixture has ≥ 1.2 m clear floor on all four faces. | Reject |
| **V-NAV-3** | Detour (D3): for pairs ≥ 40 m apart, p95 of path/Euclid ≤ `DetourP95MaxMult` (1.6) and maximum ≤ `DetourMaxMult` (2.0). | Reject |
| **V-AREA / V-DENSITY** | `LayoutWalkableCellsMinCount` (2,000 cells, 8,000 m²) ≤ A ≤ min(`LayoutWalkableCellsMaxCount` (3,250), `WalkableCellsPerPawnMaxCount` (45 cells, 180 m²) × (`NpcCount` + 2 · `TeamSize`)): crowd density is validated, not a side effect of map size. At the defaults the band is 8,000-10,800 m² (2,000-2,700 cells; typical 8,800; widened from 8,000-9,000 by `NpcCount` 50, user decision 2026-10-25); the 3,250-cell Max default binds only above `NpcCount` 50 (knob row, cross-review C-W15). | Reject |
| **V-SOCK** | Counts meet D5; every socket's R3 data is present and on the grid. | Reject |
| **V-EMERGE** | Every emergence socket passes R5.2. | Reject |
| **V-RESPAWN** | For every usable zone pair, each zone's cut `RespawnRegion(zone, partner)` has expected unseen respawn pool `E_R` (D6b) ≥ `RespawnPoolMinExpectedCount` (4). A failing pair is unusable (as in V-SEP); no usable pair fails the layout. | Reject |
| **V-REACH** | From every start cell of every candidate zone to every court Selection could use: a complete (non-partial) path within the D2 budget. | Reject |
| **V-SEP** | Per zone pair: no line of sight between the two start-cell sets (dilated 3 m); nearest start cells ≥ `ZoneSeparationPathCm` (70 m) by path; `T_contact` (D6) ≥ `ZoneContactMinSec` (15 s) for both zones, which governs. Every start cell ≥ `StartToFinalMinPathCm` (44 m; lowered from 60 m by decision D4 2026-10-25 so the generator's S4 axis-corner stratum can hold the 6 final-eligible courts `FinalCandidatesMinCount` requires, arithmetic in Generator S4 and D5 below) by path from every final-eligible court. A failing pair is unusable; no usable pair fails the layout. A third or fourth candidate zone may fail V-SEP with another candidate (for example adjacent corners) as long as each zone belongs to at least one usable pair; such a pair is simply never drawn (Generator CR 1c). | Reject |
| **V-SIGHT** | Longest straight walkable sightline ≤ `SightlineMaxCm` (32 m), measured in **every** direction (axis-aligned and diagonal, across plazas and junctions; Generator CR 1b); a one-module jog is not a break (the offset must be at least the corridor's clear width). | Reject |
| **V-COURT** | Courts meet R3, are ≥ 24 m apart centre to centre, have no bush inside or within `BushCourtExclusionCm` (6 m) of the edge, a `CrowdRoute` edge through them, and host-area prop density ± 20% (art 6.4). | Reject |
| **V-POOL** | Every court has a contiguous sun-lit rectangle ≥ 6 × 6 m containing its centre (geometric sun raycast on a 0.5 m grid at the fixed sun direction, not Lumen), of at least `SunPoolSamplesMinCount` (144 samples, 36 m², the 6 × 6 m rectangle; target 36-44 m²), within `SunPoolParityPct` (± 20%) of the median court (D9). | Reject |
| **V-POOL-40** | No sun pool visible along a ground-level line longer than `SunPoolBeaconRangeCm` (40 m) (art 2.5). | Warn in MVP (`bSunPoolBeaconIsReject` = false) |
| **V-BUSH-1** | Bush count (D5), spacing ≥ `BushSpacingCm` (12 m), ≤ `BushPerSectorMaxCount` (2) per 40 m sector, rows of ≤ 3, ≥ 1.0 m open floor on ≥ 2 sides, no recess, within 4 m of a route node, ≥ 6 m from any ExitPoint. | Reject |
| **V-BUSH-2** | Dome Ban: no non-bush mesh is a closed convex form with width:height 0.8-1.4 and height 1.3-2.3 m; every 2.0 × 2.0 planter holds a bush; no decorative planter is 2.0 m square (art 3.6, 6.5). | Reject |
| **V-BUSH-3** | Backdrop: along the principal approach axis ≥ `BushBackdropTargetPct` (50%, floor 40%) of dome silhouette rays hit light surface classes (Limewash, Paving, Sky). | Warn (the 25 m greyscale test decides) |
| **V-ROUTE** | `CrowdRoute` connected; ≥ `RouteCoverageMinPct` (95%) of walkable cells within `RouteCoverageRadiusCm` (12 m) of an edge, and **every** walkable cell a player can legally stand on within `RouteMaxGapCm` (10 m) of an edge, so no off-route position exists that only players occupy (Crowd CR 17); ≥ `RouteCyclesMinCount` (8) independent cycles, ≥ 2 per quadrant; ≥ 95% of nodes of degree ≥ 2; every socket within 3 m of a node. | Reject |
| **V-TRAV** | Traversal points meet R3 (both landings on the navmesh and clear, each landing and every climb top a capsule-diameter footprint at least `TraversalLandingClearCm` deep); count per D5. No obstacle meant as a walk-over is taller than `TraversalMinHeightCm` (45 cm, the CMC step height, SCL-owned): obstacles of 46 cm up to the vault band block and are not traversable (SCL CR 3). | Reject |
| **V-ZONE** | Per usable zone pair: area-type shares and sun/shade fractions differ by ≤ `ZoneParityPct` (15 points); each zone mixes ≥ 2 area types; each anchor ≥ 15 m from any tier above 6.4 m; anchors' distances to the nearest temple differ by ≤ 20%. Layout-wide: no unique module variant within 6 m of a socket and every variant occurs ≥ 3 times (art 6.7, "no fingerprint"). | Reject |
| **V-ZONEFILL** | Every candidate zone at P_z = `TeamSize` has a D6 window ≥ `ZoneNpcWindowMinCount` (3), and Σ `NpcZoneMin` over a used pair ≤ floor(`ZoneNpcShareMaxPct` · `NpcCount`). | Reject |
| **V-FAIR** | Selection (R9) finds ≥ `MinFairCombosCount` (12) passing combos, role share ≤ `RoleShareMaxPct` (50%), and prefix ambiguity holds (D8). | Reject |
| **V-CLUE** | Every court's `ClueTag` (R3) is shared by at most `ClueMatchMaxCount` (3; Objectives-owned, read only) courts, so a stage-card clue never names a single court (Objectives OC-15; Generator CR 11c lets G-PRE pre-check it). | Reject |

**Batch audits** (AC-40) run on a 50-seed Generator batch and on each graybox revision rather than per layout (a per-layout check would push the Generator into a visible pattern): bush placement uncorrelated with sun pools, sites, zones or team (permutation test, p ≥ 0.05, with an effect-size bound); per-socket emergence use near-uniform against area share (chi-square, p ≥ 0.01); zones, sites, bushes and sockets spread across area types.

#### R8. Validation failure, reseed and fallback

1. If any `Reject` predicate fails, or a build or nav timeout occurs, the attempt fails. The service records `FLayoutAttemptRecord` = {Attempt k, PublicSubSeed(Layout, k), Result, FailedPredicateIds[], MetricsSnapshot}. Nothing about failed attempts is replicated; clients only ever see a published layout.
2. If `ProviderKind = Generated`, the service increments `LayoutAttempt`. The Generator rebuilds from `PublicSubSeed(Layout, LayoutAttempt)` (Session & Travel F5; the contract defines no seed mixer of its own), up to `LayoutMaxReseedAttemptsCount` (5) attempts after the first. Attempts happen server-side, before publishing. The round `Seed` itself never changes.
3. If every attempt fails:
   - In mode `GeneratedWithFallback`, the server performs a **second seamless travel to the hand-built map** through Session's `RequestFallbackTravel` (rule 11.9; Session CR 11) (-> ADR; level streaming was considered and rejected). Match-scoped fields are preserved (Session & Travel), and Round re-enters `Preparing` with `ProviderKind = Graybox`.
   - In mode `Generated`, the round aborts to Lobby.
4. If the graybox itself fails validation, the round **aborts to Lobby**. The graybox is also validated in the editor and in CI, so this should never ship.
5. **End-reason logging.** Round writes the attempt records and the final disposition into the server-only Round Event Log header: `LayoutId`, the published `Seed`, `LayoutAttempt`, `LayoutFallbackLevel` and the Selection sub-seed value (R9). The disposition is `Accepted` / `FellBack` / `Aborted`. `LayoutFallbackLevel` is 0 when the first attempt was accepted, 1 when a reseed was accepted, 2 for a hand-built fallback and 3 for an abort to Lobby. It is a different field from Tuning Data & Config's `FallbackLevel`.

#### R9. Selection: the server-secret assignment of roles

1. **Secret streams.** Every runtime random choice comes from Session & Travel's server sub-seeds `ServerSubSeed(Domain, LayoutAttempt)` (F5): derived from the 128-bit `ServerSecret`, which never leaves the server, and independent of `Seed`. Each consumer has its **own** domain, and every draw is counter-based (keyed by the request's server sequence id), so interleaving and kills can never shift another consumer's draws:

   | Domain | Consumer | Use |
   |---|---|---|
   | `ObjectiveSites` | LSC | the Selection draw (R9.4) |
   | `PlayerPlacement` | LSC | player start cells and the zone NPC count (R6) |
   | `EmergencePick` (new) | LSC | socket choice among clear sockets (R5.4) |
   | `RespawnPick` (new) | LSC | takeover pawn (R5b) |
   | `AmbientSchedule` (new) | Crowd | ambient entry and exit times (R5.6-5.7) |
   | `SpawnOrder` | Round | round-start spawn shuffle (Round F5); not used by LSC |

   The sub-seed *values* are logged in the server-only Round Event Log header; `ServerSecret` never is. A test override exists only in non-Shipping builds. The three new domains are a change request to Session F5.
2. A **combo** assigns, together: the 2 used zones from the usable zone pairs (V-SEP, V-ZONE) and zone → team (2 options per pair); one final court from the final-eligible courts; and for each team `P1(T)` and `P2(T)` from distinct prelim-eligible courts. No court holds two roles.
3. A combo **passes** when all of these hold:
   - Per-leg path floors: zone → P1 at least `LegZoneToP1MinCm` (30 m), P1 → P2 at least `LegP1ToP2MinCm` (40 m), P2 → Final at least `LegP2ToFinalMinCm` (40 m).
   - Each team's prelim courts are at least `EnemyZoneToPrelimMinCm` (50 m) by path from the *other* zone's anchor.
   - Each team's chain length is within `ChainLengthMinCm`..`ChainLengthMaxCm` (D4).
   - Fairness ratios (D1) are within `ChainFairnessMaxMult` (1.10), `FinalLegFairnessMaxMult` (1.15) and, with traversal, `TraversalFairnessMaxMult` (1.20).
   - Bush balance (D9): within `BushChainBufferCm` (20 m) of each team's chain polyline the bush counts differ by at most `BushChainDiffMaxCount` (2).
   - **No mirror intercept** (Generator CR 1e, CD-GDD-ALIGN; Generated provider only): for each stage, the two teams' prelim courts are not the same court and not within one cell (200 cm) of each other's image under the layout's skeleton symmetry (a server-only transform the Generator hands to the service; the graybox supplies none, so the condition is inactive there). A symmetric skeleton therefore never yields "go to the reflection of my position".
4. The Selection is a **uniform random draw**, never the "best" combo, because the best could be derived from public geometry. It is drawn in two stages from the `ObjectiveSites` stream: first a used zone pair **uniformly among the usable zone pairs** that have at least one passing combo, then a combo uniformly among that pair's passing combos (Generator CR 1d), so a pair with more passing combos is not favoured. V-FAIR requires (D8):
   - at least `MinFairCombosCount` (12) passing combos;
   - candidate sets after the distance predicates: at least `PrelimCandidatesPerStageMinCount` (3) per `ObjectivePrelimSite(Team, Stage)` and at least `FinalCandidatesMinCount` (6, hard minimum 3) final-eligible courts; the default is `TeamSize + 1`, so a full team cannot post one camper per final candidate (Objectives SP-4, OQ 9; TDC cross-field warning `FinalCandidatesMinCount > TeamSize`). Sets may overlap; a combo never uses a court twice;
   - **role share**: no court holds one role (final, or a given team's P1 or P2) in more than `RoleShareMaxPct` (50%) of passing combos;
   - **prefix ambiguity**: for each team and each passing combo, the passing combos that share that team's prefix (used zones, zone → team, P1_T, P2_T) contain at least `FinalAmbiguityMinCount` (3) distinct finals, none in more than `RoleShareMaxPct` of them. A team that knows its own chain still cannot deduce the final from the constraints.
5. The result `FSiteAssignment` = {UsedZones[2], ZoneTeam[2], Final, P1[2], P2[2]} is **ServerOnly**. Objectives Chain owns every narrower audience (stall pressure, the hidden final site). The Visibility Policy releases the assignment and the Selection sub-seed value to `PostRound` in `Ended`, so the draw can be audited.
6. **Exactly one `ObjectiveFinalSite` is active** (locked: one shared final objective). Every court looks identical, because the fixture and sun pool are on all of them (art 6.8).

#### R10. Pillar 1 placement constraints

What makes a place "ordinary" is enforced by the rules above, not restated here: no role-specific mesh, decal, light, prop, sound or area type (VA-1, V-ZONE); equivalent courts (R3, V-COURT, V-POOL); zones that are ordinary, mixed, unseen streets (V-SEP, V-ZONE, R6.2); one emergence path for every NPC (R5) and a takeover that shows nothing (R5b). Two rules exist only here:

1. Bush placement is neither attracted to nor repelled from zones, sites or teams (batch audit).
2. Packaged builds contain no socket markers, labels, actor names, nav area names or collision profile names that reveal a role (R11.2, AC-38).

#### R11. The graybox implementation (MVP)

1. The hand-built map carries a **`LevelSocketProvider` data component** on one map actor. At server begin-play it collects editor-placed markers: emergence (OriginCell, ExitPoint, baked W(O) debug view), start cells (`ZoneId` 0-3 only), courts (box only, never a team, stage or role), bushes (planter cell, EntryAxis), route splines snapped to the grid, and nav blockers. Traversal points are derived from coping-tagged kit pieces, never hand-placed.
2. Marker actors are **server-only authoring data**, absent from the client cook. How UE 5.8 excludes them (editor-only actors, a server-only data layer or a cooked data asset) is **NOT SOURCEABLE** from the engine reference: -> ADR plus a packaging test. Until verified, markers carry no team, stage or role field, so the worst-case leak is "these cells are zone 2".
3. The graybox runs the **same Validate and Select** with the secret streams, so its used zones, zone → team and live sites vary per round although its geometry is fixed.
4. **Editor tooling:** a Validate button and a validation commandlet call the identical predicate function. CI runs `StealthGame.Layout.Graybox.Validates`.
5. **Authoring checklist** (level-designer; every count from D5):
   - 8,000-8,800 m² walkable (V-AREA at 50 pawns), 6 m streets and 4 m alleys (2.8 m clear), staggered T-junctions every 32 m or less.
   - Author in this order, because about 50 features each carry 6 m exclusions: courts (D5, at least 24 m apart, 8 × 8 in Market areas), then zones, then alleys (12-13, one socket each), then bushes (8-9), then traversal.
   - Below `NpcCount` 45 every candidate zone needs at least 2,000 m² of walkable area in its 35 m disc (D6: about 1,800 m² is required at `NpcCount` 35-40).
   - **4 candidate zones** at the map's sides, each with 6 start cells within 16 m of its anchor; every usable pair has no line of sight between start sets, at least 70 m of path and `T_contact` at least 15 s. Each respawn region passes V-RESPAWN.
   - Final-eligible courts are those at least `StartToFinalMinPathCm` (44 m, decision D4) path from every start cell with path parity between zone pairs; there is no "centre band" rule. Author the graybox so that at least `FinalCandidatesMinCount` (6) courts pass that floor with at least 3 ingresses each; do not author to the superseded 60 m rule, which admits only about 2 per axis corner.
   - Mirrored halves pass V-FAIR easily but create a visible symmetry: follow with an asymmetric edit pass.
   - Before locking the chain band, enumerate all combos on the graybox plan and set `ChainLengthMinCm`/`MaxCm` from the histogram (D4).

### States and Transitions

The layout service has one server-side state per round:

| State | Entered when | Exits to | Exit condition |
|---|---|---|---|
| `Idle` | World start, or after `Reset` | `Building` | Round calls `RequestLayout(Seed, TuningHash)` |
| `Building(k)` | The request arrives, or a reseed | `Validating(k)` / `AttemptFailed(k)` | Provider done and nav built / build or nav timeout |
| `Validating(k)` | Build complete | `Selecting(k)` / `AttemptFailed(k)` | All `Reject` predicates pass / any fails |
| `Selecting(k)` | Validation passed | `Published` / `AttemptFailed(k)` | V-FAIR passes and one combo is drawn / it fails |
| `AttemptFailed(k)` | Any failure | `Building(k+1)` / `FallbackTravel` / `Aborted` | Generated and k < `LayoutMaxReseedAttemptsCount` and budget left / attempts or budget exhausted in `GeneratedWithFallback` / otherwise, or the graybox failed |
| `Published` | Manifest replicated, `OnLayoutReady` fired | `Live` | Round leaves `Preparing` for `Active` (Round's condition, using Session's readiness acks) |
| `Live` | `Active` | `Released` | Round enters `Ended` |
| `Released` | `Ended` | `Idle` | Round `Reset` (next round travels to a new world; -> ADR, per-round travel) |
| `FallbackTravel` | Attempts exhausted | (new world) `Idle` | Seamless travel to the graybox completes |
| `Aborted` | **From any state**: graybox failed, Generated without fallback exhausted, `PlacementInfeasible`, `NpcCountTooLow`, or listen-server host lost | Round → `Lobby` | Immediate; `LayoutFallbackLevel = 3` logged |

Start placement is served only in `Published` and emergence and takeover only in `Live`; a request in any other state is rejected and logged (including a kill resolved in the same tick as round end, which Round's queue has already ordered after the end event).

### Interactions with Other Systems

Every interface, in both directions and with hard/soft marking, is listed once in **Dependencies** below. Two behavioural rules sit here: Crowd's density function and every LSC selection are blind to `FSiteAssignment`, `IsPlayer` and team (the one team read is R5b's own-zone and non-team-viewer set); and the contract publishes nothing to Teammate Markers (#17) or the HUD (#18).

## Formulas

*systems-designer consulted (authoring and review). "path" is the navmesh path length without traversal unless stated. A is walkable area in m² (V-AREA). Keys are stored in cm, cell counts or percent (TDC CR-15) and shown here in m and fractions; safe and hard ranges live only in Tuning Knobs.*

### D1. Fairness ratios

The fairness formula is defined as:

`C_T = (1/|Z_T|) · Σ_{c ∈ Z_T} [path(c, P1_T) + path(P1_T, P2_T) + path(P2_T, F)]`

`R_chain = max(C_0, C_1) / min(C_0, C_1) ≤ ChainFairnessMaxMult`

`R_final = max(path(P2_0, F), path(P2_1, F)) / min(path(P2_0, F), path(P2_1, F)) ≤ FinalLegFairnessMaxMult`

`R_trav = R_chain` recomputed with traversal-enabled paths, `≤ TraversalFairnessMaxMult`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| Z_T | set of cells | 6-8 cells | zone start cells | Team T's start cells |
| P1_T, P2_T, F | court | from Selection | calculated | Team T's prelim courts and the shared final of one combo |
| C_T | float (m) | ≥ 110 (leg floors); band-checked by D4 | calculated | Mean chain length for team T |
| R_* | float | ≥ 1.0 | calculated | Fairness ratio, compared with its Tuning key |

**Output Range:** ≥ 1.0, unbounded above; a combo passes only when all three ratios are within tolerance. No division by zero: the leg floors (R9.3) force C_T ≥ 110 m and every final leg ≥ 40 m. Until the traversal spike reports, traversal paths equal plain paths and R_trav = R_chain.
**Example:** C_0 = 230, C_1 = 215 → R_chain 1.070 (pass); final legs 70 and 62 → 1.129 (pass); traversal chains 198 and 170 → 1.165 (pass); chains 250 and 220 → 1.136 (fail).

### D2. Reachability budget

The reachability_budget formula is defined as:

`B(a, b) = min(ReachBudgetMaxCm, ReachDetourMult × max(Euclid(a, b), ReachBudgetFloorCm))`; a pair passes when the path is complete (not partial) and `path(a, b) ≤ B(a, b)`.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| Euclid(a, b) | float (m) | 0-200 | calculated | Horizontal straight-line distance |
| ReachDetourMult | float | = `DetourMaxMult` (2.0) | derived (read-only alias, knob row) | Equal to `DetourMaxMult` by construction (I-LSC-4 retired, cross-review C-W19) |
| ReachBudgetFloorCm | float (m) | default 20 | data file | Keeps the budget sane for close pairs |
| ReachBudgetMaxCm | float (m) | default 200 | data file | Caps any single start-to-court path |

**Output Range:** `[ReachDetourMult · ReachBudgetFloorCm, ReachBudgetMaxCm]`, 40-200 m at defaults. V-REACH equals V-NAV-3's maximum ratio up to Euclid = `ReachBudgetMaxCm / ReachDetourMult` (100 m); beyond that the cap is intentionally stricter.
**Example:** Euclid 90 → B = 180, so a 152 m path passes; Euclid 140 → 280 capped to 200; Euclid 10 → 40.

### D3. Detour statistic

The detour formula is defined as:

`ρ(a, b) = path(a, b) / Euclid_xy(a, b)` over unordered pairs of sample cells with `Euclid_xy ≥ 40 m`. `p95` is the nearest-rank value (sort ascending, rank `ceil(0.95 · n)`); `max` is the largest ρ.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| sample cells | set | about A/16 (500-675) | calculated | Walkable cells on a 4 m stride (every second cell) from layout origin (0, 0), in canonical order. The stride hits every 2-cell alley, and the fixed lattice keeps the result a pure function of the inputs (R2.4) |
| n | int | about 100,000-180,000 | calculated | Qualifying unordered pairs |
| DetourP95MaxMult / DetourMaxMult | float | defaults 1.6 / 2.0 | data file | Thresholds |

**Output Range:** ρ ≥ 1.0 (triangle inequality). V-NAV-1 runs first, so no pair has an infinite path.
**Example:** n = 100,000 → p95 is the ρ at rank 95,000. 1.42 passes (≤ 1.6); a maximum of 1.88 passes (≤ 2.0).

### D4. Chain length band and walk share

The walk_share formula is defined as:

`S = L / (v · T · 60)` (L in m, v in m/s, T in minutes); the predicate is `ChainLengthMinCm ≤ C_T ≤ ChainLengthMaxCm` for both teams.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| L (= C_T) | float (m) | 120-240 (target 170; **provisional**, set from the graybox enumeration, R11.5) | calculated | Team chain length (D1) |
| v | float (m/s) | 1.4 (**provisional**) | Shared Character & Locomotion | Reference crowd walk speed |
| T | float (min) | about 9 (median 7.1-10.9) | Objectives Chain F1 `T_round` (chain 6.7 min + about 2 min contest), per decision D29; `TargetRoundMinutes` (12) is non-binding and is **not** the basis here (cross-review C-W23) | Modelled round length |
| S | float | 0.159-0.317 at T = 9 (0.119-0.238 against the non-binding 12) | calculated | Share of the modelled round spent walking the chain |

**Output Range:** 0.16-0.32 inside the band at T = 9 (L 120: 0.159; L 170: 0.225; L 240: 0.317).
**Example:** 170 / (1.4 · 9 · 60) = 170 / 756 = 0.225, about 121 s of walking (30 % of the 402 s chain time of Objectives F1).
**Rationale:** travel adds spatial tension and puts the teams in the same crowd (P4) while about 80% of the round stays reading people, approaching and waiting. Above about a quarter of the round the race becomes a walking contest (reached at L about 190 m with T = 9: the upper part of the band is walk-heavy, so a chain target above 170 m, including Objectives' proposed lever toward 220 m, which gives S = 0.29, must be weighed against this); below about an eighth it is decided at the courts. (Restated 2026-10-25 against Objectives F1's about 9 minute median, C-W23 and D29; the earlier text used the 12-minute target, which neither document treats as binding.) 150-300 m (target 220) was not reachable in a compact 8,000-9,000 m² city with the leg and enemy-distance floors (level-designer review).

### D5. Socket counts per area

Rounding is half-up: `round(x) = floor(x + 0.5)`. The socket_count formulas are defined as:

`N_E = clamp(ceil(A / EmergenceAreaPerSocket), EmergenceSocketsMinCount, EmergenceSocketsMaxCount)`

`N_B = clamp(round(A / BushAreaPerSite), BushSitesMinCount, BushSitesMaxCount)`

`N_T = clamp(round(A / TraversalAreaPerPoint), TraversalPointsMinCount, TraversalPointsMaxCount)`

`N_C ≥ FinalCandidatesMinCount + 4 · PrelimCandidatesPerStageMinCount − Overlap`, with `Overlap ≤ 2` (16 at the defaults 6 + 12 − 2)

**Final-eligible feasibility (D4, 2026-10-25):** the count of courts V-SEP leaves final-eligible depends on `StartToFinalMinPathCm` F. On the generator's corner-zone geometry a court at (x, y) from an axis corner is about `112 − x − y` m from the nearest start cell, so with the 8 % path guard it is final-eligible iff `x + y ≤ 112 − 1.08 · F`; 6 courts at ≥ 24 m spacing need F ≤ 46.3 m at radius 16 m (44.4 m at 18 m) (Generator S4), hence the 44 m default. `FinalCandidatesMinCount` is never lowered below 6 to make a layout pass. **Resolved (D50, 2026-10-25; cross-review C-W24):** at the default F = 44 m and start-cell radius 16 m the margin at the mean geometry is 2.5 m (court (31, 31): x + y = 62 against the bound 64.48). The grid-only enumeration (`production/qa/spikes/2026-10-25_citygen-oq9-final-court-enumeration.md`) gives p_g 0.615 at `CityGenSkeletonJitterCount` 0; the default is 0 (D50).

Sector feasibility: `N_B ≤ BushPerSectorMaxCount · N_sec`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| A | float (m²) | 8,000-10,800 at defaults | calculated | Walkable area (cells × 4 m²) |
| EmergenceAreaPerSocket | float (m²) | default 700 (`EmergenceCellsPerSocketCount` 175) | data file | Area served by one socket |
| BushAreaPerSite | float (m²) | default 1,000 (`BushCellsPerSiteCount` 250) | data file | Area per bush |
| TraversalAreaPerPoint | float (m²) | default 800 (`TraversalCellsPerPointCount` 200) | data file | Area per traversal point |
| Overlap | int | 0-2 | calculated | Courts shared between candidate sets (R9.4) |
| N_sec | int | about 10-12 | calculated | 40 m sectors on a layout-origin grid holding at least one walkable cell |

**Output Range:** at the default band N_E = 12-16, N_B = 8-11, N_T = 10-14, N_C ≥ 16 (14 before reconciliation 2026-10-24 raised `FinalCandidatesMinCount` to 6). (A 13,000 m² layout (3,250 cells) is valid only with about 63 or more NPCs at `TeamSize` 5, i.e. 72 or more pawns in all (3,250 / 45), and gives 19/13/16.)
**Example:** A = 8,800 → N_E = ceil(12.57) = 13, N_B = round(8.8) = 9, N_T = round(11.0) = 11; 6 final + 3 × 4 prelim candidates with overlap 2 → N_C ≥ 16; bush sectors 2 × 11 = 22 ≥ 9. A = 8,500 → N_B = round(8.5) = 9 (half-up).

### D6. Zone NPC fill and contact time

The zone_fill formulas are defined as (ρ = NpcCount / A; ρ_p = (NpcCount + 2 · TeamSize) / A):

`NpcZoneMin_z = max(ceil(r · ρ · A_z), ceil(P_z · (1 − s_max) / s_max))`

`NpcZoneMax_z = min(floor(d_max · ρ_p · A_z) − P_z, floor(q · NpcCount / 2))`

`W_z = NpcZoneMax_z − NpcZoneMin_z + 1 ≥ ZoneNpcWindowMinCount` (V-ZONEFILL); the zone's NPC count is drawn uniformly from `[NpcZoneMin_z, NpcZoneMax_z]`

Contact: `T_contact_z = min over start cells e of the other used zone, and cells c with line of sight into z's start-cell set dilated by 3 m, of path(e, c) / v_run` ≥ `ZoneContactMinSec` (V-SEP)

Implied bounds: `A_z ≥ (P_z + NpcZoneMin_z + ZoneNpcWindowMinCount − 1) / (d_max · ρ_p)`; `NpcCount ≥ max(ceil(2 · (ceil(T(1 − s)/s) + W_min − 1) / q), ceil(LayoutWalkableCellsMinCount / WalkableCellsPerPawnMaxCount) − 2T)` (TDC cross-field invariant)

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| r | float | default 0.8 | `ZoneNpcDensityMinMult` | Zone NPC density floor relative to the map mean |
| A_z | float (m²) | about 1,500-2,000 | calculated | Walkable area of the 35 m density disc |
| P_z | int | 0-`TeamSize` | calculated | Players placed in zone z (V-ZONEFILL uses `TeamSize`) |
| s_max | float | default 0.40 | `ZonePlayerShareMaxPct` | Maximum player share of the zone's pawns (P1 cluster guard) |
| d_max | float | default 1.5 | `ZoneDensityMaxMult` | Maximum zone pawn density relative to the map mean |
| q | float | default 0.60 | `ZoneNpcShareMaxPct` | Maximum share of `NpcCount` pinned to the two used zones |
| v_run | float (m/s) | 4.5 (**provisional**) | Shared Character & Locomotion | Maximum run speed |
| T_contact_z | float (s) | 10-40 | calculated | Earliest time an enemy could see into zone z |

**Output Range:** `NpcZoneMin` ≥ 8 at `TeamSize` 5 and s_max 0.4; W_z ≥ 3 or the layout fails. With `NpcZoneMin` at the share term's 8 (A_z up to about 1,760 m²) the window needs `A_z ≥ 10 · A / (NpcCount + 2 · TeamSize)`; once the floor term reaches 9 it needs 16 pawns at 1.5 · ρ_p. At `NpcCount` 50 and A = 8,800 that is A_z ≥ 1,467 m² (1,564 m² if `NpcZoneMin` is 9; over the Generator band A 8,028-9,216 m²: 1,338-1,536 m²). **At `NpcCount` 35 the requirement is about 1,800 m², not 1,467 m²** (cross-review C-W23: the earlier figure reused `NpcCount` 50's ρ_p): the V-AREA window pins A at 8,000-8,100 m² (A_max = 45 × (35 + 10) = 2,025 cells), so ρ_p = 45 / 8,100 = 0.005556 and `NpcZoneMax` = min(floor(1.5 · 0.005556 · A_z) − 5, floor(0.6 · 35 / 2) = 10) reaches `NpcZoneMin` 8 + W 3 − 1 = 10 only for A_z ≥ 15 / (1.5 · 0.005556) = 1,800 m² (1,778 m² at A = 8,000; 1,760 m² at `NpcCount` 40, A = 8,800). The requirement never exceeds 1,800 m² for any `NpcCount` (A cannot exceed the density cap `180 m² × (NpcCount + 2 · TeamSize)`), and it is close to that value whenever the V-AREA floor of 8,000 m² holds A near the cap: 1,778-1,800 m² at `NpcCount` 35, 1,600-1,800 m² at 40. **Consequence:** against an unconstrained A_z of about 1,500-2,000 m² roughly half of the layouts fail V-ZONEFILL at `NpcCount` 35-40, and the Generator's S5 disc (about 1,950 m² walkable) clears it by only about 8-11 % (4-6 % after the 1.04 area guard), so the Generator's p_g is not independent of `NpcCount` there. **Fix adopted:** (1) the `NpcCount` floor is stated as two terms: the V-AREA term (35, the hard-valid floor: below it the preset is invalid and placement fails with `NpcCountTooLow`; the share term gives 34) and the density-cap term above (V-ZONEFILL is the authority: graybox discs and Generator anchors must clear `10 · A / (NpcCount + 2 · TeamSize)`); (2) graybox authoring below `NpcCount` 45 must give every candidate zone at least 2,000 m² of walkable area in its 35 m disc (R11.5); (3) recommended to the owner of `NpcCount` (TDC): safe range 35-50 narrowed to 45-50 (at 45 the Generator band gives a requirement of 1,460-1,676 m², a 16-33 % margin under the 1,950 m² disc). This GDD does not change `NpcCount`; **adopted by TDC at cross-review 2026-10-25-b** (safe 45-50; the I-LSC-1 reject floor stays 35).
**Example:** NpcCount 50, A = 8,800, A_z = 1,800, P_z = 5: floor term ceil(0.8 · 0.005682 · 1,800) = ceil(8.18) = 9; share term ceil(7.5) = 8 → NpcZoneMin 9. Cap term floor(1.5 · 0.006818 · 1,800) − 5 = 18 − 5 = 13; share-of-NpcCount term floor(0.6 · 50 / 2) = 15 → NpcZoneMax 13. W = 5 (pass); the zone holds 9-13 NPCs, at most 18 / 1,800 = 0.0100 = 1.47 × the map mean (≤ 1.5). Nearest enemy start cell 80 m of path from the first cell that sees z's start set at 12 m → path 68 m, T_contact = 15.1 s (pass); at 60 m it is 13.3 s (fail).

### D6b. Expected unseen respawn pool

The respawn_pool formula is defined as:

`E_R = ρ · A_R · (1 − φ_R)^k` ≥ `RespawnPoolMinExpectedCount` (V-RESPAWN)

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| ρ | float (NPC/m²) | about 0.0046-0.0063 | calculated | Mean NPC density, NpcCount / A |
| A_R | float (m²) | about 1,500-3,000 | calculated | Walkable area of the cut `RespawnRegion(zone, partner)` |
| φ_R | float | 0-1 | calculated | p95, over walkable cells v within `EmergenceWitnessRangeCm` of R, of the fraction of R's cells visible from v (R5.2 samples) |
| k | int | default 2 (`RespawnViewerCount`) | data file | Non-team viewers assumed near the region |
| E_R | float | ≥ 0 | calculated | Expected eligible pawns; P(empty) ≈ e^(−E_R) |

**Output Range:** ≥ 0; the layout fails below `RespawnPoolMinExpectedCount` (4, P(empty) ≈ 1.8%). Starvation beyond that is bounded by R5b.5.
**Example:** ρ = 50 / 8,800 = 0.005682, A_R = 2,400 → 13.6; φ_R = 0.2, k = 2 → E_R = 13.6 × 0.64 = 8.73 (pass). φ_R = 0.5 → 3.4 (fail: the region is too open).

### D7. Layout hash (desync detector)

The layout_hash formula is defined as:

`LayoutHash = FNV1a64(Bytes)` from offset basis `0xCBF29CE484222325`; per byte `h = (h XOR byte) · 0x100000001B3 (mod 2^64)`. `Bytes = u32(GeneratorKitVersion) ‖ u32(EntryCount) ‖ entries`, entries sorted ascending by (Course, CellY, CellX, ModuleId, VariantId, Yaw), each serialised little-endian as `i16 CellX, i16 CellY, u8 Course, u16 ModuleId, u8 VariantId, u8 Yaw` (9 bytes). For the graybox, `Bytes` is `u32(MapExtractFormatVersion) ‖ u32(RecordCount) ‖ records`, one record per collision or sight-relevant static mesh placement in the cook-time extract, sorted ascending by (X, Y, Z, MeshIdHash), each little-endian `u32 MeshIdHash, i32 X, i32 Y, i32 Z` (cm), `i32 Yaw, i32 Pitch, i32 Roll` (0.01 degree) and `i32 ScaleX, ScaleY, ScaleZ` (parts per 1,000): 40 bytes. Graybox placements are free-form and do not fit the 9-byte grid record, and a 1 cm move must change the hash (AC-3); ADR-0014 §6 owns the exact extract.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| GeneratorKitVersion | uint32 | ≥ 1 | constant (per build) | Kit and generator version |
| EntryCount | uint32 | 0 to `CityGenManifestEntriesMaxCount` (3,500) | calculated | Manifest entries (0 for the graybox manifest; the graybox uses its extract) |
| CellX, CellY | int16 | ±512 | calculated | 2.0 m grid cell |
| Course | uint8 | 0-15 | calculated | 1.6 m course index |
| ModuleId, VariantId, Yaw | uint16, uint8, uint8 | M01-M12, planter/fixture families and the Generator's derived bay stacks and slab sizes, each a distinct `ModuleId` (field width unchanged); 0-2; 0-3 | calculated | Placement |

**Output Range:** any 64-bit value. A desync detector, not a security check (clients report it themselves, Session rule 11.6). Seeds and sub-seeds are Session & Travel F5, not defined here.
**Example:** golden vectors: the empty byte string → `0xCBF29CE484222325`; the single byte `0x61` → `0xAF63DC4C8601EC8C` (standard FNV-1a 64 test values).

### D8. Selection space, ambiguity and acceptance

The selection formulas are defined as:

`N_raw = Σ_{usable zone pairs} 2 · Σ_{f ∈ F_elig} [ordered picks of 4 distinct prelim-eligible courts ≠ f]`

`N_pass = |{combos passing R9.3}| ≥ MinFairCombosCount`; `RoleShare(court, role) = |{passing combos with that court in that role}| / N_pass ≤ RoleShareMaxPct`

Prefix ambiguity, for each team T and passing combo c with prefix π = (used zones, zone → team, P1_T, P2_T): `|{F(c') : c' passing, prefix_T(c') = π}| ≥ FinalAmbiguityMinCount` and `max_f share_π(f) ≤ RoleShareMaxPct`

`P_fallback = (1 − p)^(N + 1)`; `E_attempts = (1 − (1 − p)^(N + 1)) / p`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| F_elig | set | ≥ `FinalCandidatesMinCount` (6; hard minimum 3) courts | calculated | Final-eligible courts |
| N_raw | int | about 10^5-10^6 | calculated | All combos, enumerated against a precomputed court-and-anchor path matrix |
| MinFairCombosCount | int | default 12 | data file | Minimum passing combos (log2 12 = 3.58 bits of prior uncertainty) |
| FinalAmbiguityMinCount | int | default 3 | data file | Distinct finals left once a team knows its own chain |
| p | float | 0-1 | measured (50-seed batch) | Generator per-attempt acceptance probability |
| N | int | default 5 | `LayoutMaxReseedAttemptsCount` | Reseeds after the first attempt |

**Output Range:** both draw indices (R9.4: zone pair among usable pairs with a passing combo, then combo within the pair) are rejection-sampled uniformly in canonical order (zone pair, zone map, F, P1_0, P2_0, P1_1, P2_1 by SocketId) from the `ObjectiveSites` stream; never `mod`.
**Example:** one usable pair, 16 courts, 6 final-eligible: N_raw = 2 · 6 · (15 · 14 · 13 · 12) = 393,120; four usable pairs give 1,572,480, still cheap against the precomputed path matrix. A court that is the final in 7 of 12 passing combos fails (58% > 50%); 6 of 12 passes. p = 0.5, N = 5 → P_fallback = 1.5625%, E_attempts = 1.97.

**Generator acceptance target (requirement on #16):** at least 34 of 50 batch seeds accepted (Wilson one-sided 95% lower bound p ≈ 0.565, so P_fallback = 0.435^6 ≈ 0.68% ≤ 0.8%).

### D9. Small checks

The small_checks formulas are defined as:

`PoolParity_i = |A_i − median(A)| / median(A) ≤ SunPoolParityPct`

`BushBalance = |n_0 − n_1| ≤ BushChainDiffMaxCount` (n_T = bushes within `BushChainBufferCm` of team T's chain polyline)

`λ_entry, λ_exit ≥ AmbientEmergencePerSocketMinPerMin · N_E / 60 s⁻¹` each; cancelled exits per 60 s ≤ `EmergenceExitCancelMaxPct` · scheduled exits per 60 s (R5.7)

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| A_i | float (m²) | 36-64 | calculated | Sun-pool area of court i (0.5 m samples × 0.25 m²) |
| n_T | int | 0-9 | calculated | Bushes near team T's chain |
| λ_entry, λ_exit | float (1/s) | ≥ 0.1 at defaults | Crowd (data) | City-wide ambient entry and exit rates |

**Output Range:** pass/fail checks.
**Example:** pool median 40 m², a court at 47 → 0.175 (pass at 0.20), 49 → 0.225 (fail); bush counts 4 and 3 → 1 (pass), 5 and 2 → 3 (fail); N_E = 13 at 0.5 per socket per minute → λ_entry ≥ 0.108/s (6.5 per minute), so up to 1.6 cancelled exits per minute before replacements are deferred.

## Edge Cases

*systems-designer (formula and rule space); ue-replication-specialist (network). Cases already resolved inside a rule are not repeated: same-tick socket requests, the clear check and NPC waits (R5.3); out-of-state requests (States); reseed, fallback and abort paths (R8).*

**Values and counts**

- **If `TeamSize` is below 5:** placement uses the actual P_z and D6 is recomputed; zones keep `TeamSize + 1` start cells with a minimum of 6, so zone geometry does not change.
- **If a start cell becomes unusable between `Published` and placement** (a provider bug): the player draws another free cell in its zone; if fewer free cells than P_z remain, `RequestStartPlacement` fails with `PlacementInfeasible` and Round aborts to Lobby.
- **If D6 or V-AREA is infeasible at the preset's `NpcCount`** (below 35 at the defaults): TDC rejects the preset (cross-field invariant, D6). If it slips through, the layout fails V-AREA or V-ZONEFILL, or placement fails with `NpcCountTooLow`, and Round returns to Lobby.
- **If `MinFairCombosCount` is set below 6:** rejected by its hard range in Shipping; non-Shipping test builds may go lower, and the round is flagged in the log.
- **If enough combos pass but role share or prefix ambiguity fails:** V-FAIR fails and the attempt is reseeded; on the graybox the round aborts (and CI has already failed).

**Emergence, respawn and placement**

- **If every socket stays blocked:** NPC requests wait (R5.3); the population runs at most `EmergenceDeficitMaxCount` (3) short. If the deficit is at its cap and a replacement is due, the replacement takes the next entry slot even if that exceeds the exit-cancel bound, and `EmergenceRateBoundExceeded` is logged: population constancy wins over the rate bound.
- **If no unseen eligible pawn exists for a respawn:** R5b.5 widens after 10 s and 20 s and then waits; the participant stays "awaiting pawn" (Roster) and `RespawnStarved` is logged. A pawn is never spawned and the unseen test is never relaxed.
- **If the chosen takeover pawn becomes visible to a non-team pawn between the draw and the commit tick:** the commit check fails, the pawn is dropped and the draw repeats in the same tick (R5b.3).
- **If two respawns of the same team pick in one tick:** they resolve in sequence order; a pawn claimed by the first is ineligible for the second. The two regions of a used pair are disjoint by construction (R3: a cell belongs to the nearer anchor; anchors can be as close as about 34 m of path, so without the cut 45 m regions would overlap), so a pawn is never eligible for both teams; the third widening step can reach the other team's side, and there the claim rule still applies.
- **If an NPC kill and an informant respawn resolve in the same tick:** Round's total order fixes the sequence; the killed NPC is not eligible for the takeover (it is not live), and Character Pool's card rules apply (CP-12).
- **If a kill happens while an ambient exit is in progress at a socket:** that socket is not clear (a pawn is within 3 m of the ExitPoint), so another is used.

**Disconnect, latency, rejoin and host**

- **If a player disconnects during `Preparing`, before placement:** Session handles the participant; placement runs with the players present and D6 uses the actual P_z.
- **If a player disconnects during `Active`:** the contract does nothing. The pawn is handed to crowd AI in the same server frame and is never destroyed for a connection event (Roster, Session rule 22).
- **If a participant rejoins during `Active`:** an Active participant gets its own pawn back from crowd AI (Roster); a re-admitted informant awaiting a pawn uses R5b. No body is ever created.
- **If a client arrives after `Published`, acks with a stale `LayoutId`, or recomputes a different `LayoutHash`:** it receives the same `FLayoutManifest` (not initial-only); a mismatch follows Session's resend-once-then-`ParamsMismatch` rule and `OnClientLayoutFailed(LayoutMismatch)` logs both hashes. The layout stays valid for everyone else, and no pawn is spawned for the client before `Connected` (R4.6).
- **If packet loss delays `FLayoutManifest`:** it is reliable state, retried until delivered; the ack waits and Session's travel deadline bounds the wait.
- **If the listen-server host leaves at any point:** no host migration is relied on (verify; ue-replication-specialist). The service is `Aborted`, `LayoutFallbackLevel` 3, and clients return to the menu (Session & Travel F6). No layout state is restored.
- **If the host is the cheater (listen server):** the host process holds every secret. Accepted for the prototype and stated; the dedicated server (Alpha) closes it.

**Degenerate strategies**

- **Camping an alley to spot respawns:** respawns never use alleys; an alley shows only ambient NPC traffic and replacements (R5.6-5.7).
- **Watching the enemy's respawn region:** a watcher who sees part of the region makes those pawns ineligible, so the takeover happens out of the watcher's sight; widening bounds the denial. The watcher learns at most "someone unseen may now be an informant", which is true of the whole crowd.
- **Memorising the graybox:** used zones, zone → team, the final and the stage courts are drawn per round from the secret stream; role share and prefix ambiguity (D8) keep any court from being an obvious role.
- **Reading round-start density** ("the crowded street is a spawn"): no enemy has line of sight to a used zone at t0 (V-SEP), the zone count varies within the D6 window, and the density cap holds through `T_contact`.
- **Using a traversal shortcut to beat the fairness ratio:** R_trav caps the advantage at 1.20, and traversal is a visible tell (P2).
- **Camping a bush beside a court:** bushes are at least 6 m from every court edge (V-COURT) and spread by sector caps; courts have at least 2 ingress paths and a route through them. Stall pressure belongs to Objectives Chain.

**Pillar 1 leak cases**

- **Packaging:** marker actors absent from the client cook (resolved by ADR-0014 §6: editor-only markers baked into an opaque layout-data asset; AC-38); until proven, markers carry `ZoneId` only.
- **Manifest:** geometry only (R2.5), scanned by AC-37.
- **NetGUID or spawn order:** round-start pawns are spawned in Round's single shuffled order through one factory (Round F5); the contract spawns no replicated actors and a respawn creates no actor (R5b).
- **Emergence timing correlated with kills:** prevented by R5.7; entry times follow the ambient schedule.
- **Crowd density near the live court:** Crowd's density function must not read `FSiteAssignment` (server assert, AC-39).
- **A unique-looking court or alley:** V-ZONE and VA-1; the human static-figure and bush tests cover what scripts cannot.

## Dependencies

The index row 8 lists Tuning Data & Config and, as soft, Participant State & Roster, Session & Travel and the Information Visibility Policy (verified 2026-10-24, cross-review C-W11): the contract compiles and validates the graybox with stubs, so Roster and Session are soft. The Information Visibility Policy is **Hard** here since cross-review C-W4 (2026-10-25), matching IVP's peer table; the index row 8 label follows (main session). Every GDD named below is now designed.

### Upstream (the contract depends on)

| System | Data interface | Hard/Soft |
|---|---|---|
| Tuning Data & Config (#1) | Frozen server snapshot at `RequestLayout`: every key in Tuning Knobs (owned here) plus `TeamSize`, `NpcCount`, `TargetRoundMinutes` (read only); `TuningHash` feeds `LayoutParamsHash`. | Hard |
| Session & Travel (#5) | `FRoundParams.Seed`; `PublicSubSeed(Layout, Attempt)` and `ServerSubSeed(Domain, Attempt)` (F5); the single readiness ack (rule 11.6); writes `LayoutAttempt`/`LayoutHash` into `FRoundReadiness`; the fallback seamless travel to the graybox. | Soft (graybox runs with a fixed seed and test sub-seed) |
| Participant State & Roster (#2) | Server-only `GetTeam`, for zone mapping and the respawn takeover's own-team and non-team sets. | Soft |
| Information Visibility Policy (#7, In Review) | Audience vocabulary; `PostRound` release of `FSiteAssignment` and the Selection sub-seed value. | **Hard** (peer contract, label aligned with IVP's peer table, cross-review C-W4 2026-10-25: the Selection secret and `FSiteAssignment`'s `ServerOnly` audience are only leak-safe under the Policy's rows; the graybox may still validate with a stub audience) |
| Shared Character & Locomotion (#9) | Reference walk speed (140 cm/s, D4) and run speed (450 cm/s, D6); traversal heights, *provisional pending traversal spike*. | Soft (values only) |

### Downstream (these depend on the contract)

| Dependent | What it takes | Hard/Soft |
|---|---|---|
| Round & Match Framework (#6) | `RequestLayout(Seed, TuningHash)`, `OnLayoutReady(LayoutId, Seed)`, `OnLayoutAborted`, `OnClientLayoutFailed`, `RequestStartPlacement`, `FSiteAssignment` and TeamSpawn queries, attempt records and `LayoutFallbackLevel` for the Event Log header. Owes the spawn gate (R4.6), the spawn-order shuffle (F5) and the abort-to-Lobby path. | Hard |
| Crowd & NPC Behaviour (#11, In Review) | `CrowdRoute`, `AreaTypeId`, `RequestEmergence(Replacement / AmbientEntry / AmbientExit)`, NPC start pool. Owes the ambient baseline (R5.6) from its `AmbientSchedule` stream, the exit-cancel rule (R5.7), the takeover release and a density function blind to the assignment. | Hard |
| NPC-Kill Penalty & Informants (#14, In Review) | `PickRespawnPawn(Team, RequestSeq, ExcludePawn)` (R5b) for every informant-body cause; owns the respawn delay and the Roster bind. | Hard |
| Concealment (Bushes) (#12, In Review) | `BushSite` (`BushId`, `HideVolume`, `EntryAxis`), built identically from the Manifest on server and clients. | Hard |
| Objectives Chain (#15, In Review) | `FSiteAssignment` (ServerOnly) and court geometry; owns the interaction actors and every narrower audience. | Hard |
| Procedural City Generator (#16, In Review) | Implements the provider interface from `PublicSubSeed(Layout, Attempt)` only, passes every predicate and the D8 acceptance target; never references Round. | Hard (on the contract) |
| Character Pool & Appearance (#4) | Emergence sockets for replacement materialisation; the takeover keeps the taken pawn's card (CP-12b intent). | Soft |
| Shared Character & Locomotion (#9) | `TraversalPoint` records and the coping rule ("coping means traversable, and only coping does"). | Soft |
| Input & Interaction Framework (#3) | Bush geometry from the Manifest for local prompts; objective prompts must come from Objectives' team-audience data, because roles are secret. | Soft |
| Telemetry & Analytics (#26), Anti-Cheat & Info-Leak Security (#27; Alpha tier, GDDs not yet written) | Attempt records, `LayoutFallbackLevel`, emergence and respawn counts; leak tests on manifest content and packaging. | Soft |

## Tuning Knobs

Every key is registered under Tuning Data & Config CR-15 with the Level Socket Contract as owner, **Audience** `ServerOnly` (clients receive results as Manifest geometry, never thresholds) and **Latch** `Round` (frozen at `Preparing`, applied to the next layout). Defaults are provisional until the graybox playtest; † marks *provisional pending traversal spike*. Areas are counts of 2.0 m cells (4 m²) or 0.5 m sun samples (0.25 m²). Read-only inputs owned elsewhere: `TeamSize`, `NpcCount`, `TargetRoundMinutes` (TDC), the walk/run speeds `WalkSpeedCmPerSec` (140) and `RunSpeedCmPerSec` (450), `TraversalMinHeightCm` and the traversal heights (Shared Character & Locomotion), and `FixtureInteractSideCount` and `ClueMatchMaxCount` (Objectives Chain).

| Key | Type | Default | Hard range | Safe range | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|
| `LayoutProviderMode` | enum | Graybox | Graybox, Generated, GeneratedWithFallback | — | — | Which provider builds. `Generated` without fallback aborts on exhaustion; Generator tests only |
| `LayoutMaxReseedAttemptsCount` | int | 5 | 0-10 | 3-5 (narrowed from 3-6, cross-review C-W17: 6 gives (1 + 6) × 7 = 49 s, above the 45 s default budget, I-LSC-2) | Count | High: long Preparing, hidden Generator weakness. Low: more fallbacks (D8) |
| `LayoutAttemptTimeoutSec` | float | 7 | 2-30 | 5-7.5 | Sec | Must satisfy `(1 + reseeds) × timeout ≤ LayoutTotalBudgetSec` (TDC I-LSC-2, reject); the largest safe corner is (1 + 5) × 7.5 = 45 s, which the narrowed budget safe range admits. Low: valid layouts time out on slow hosts |
| `LayoutTotalBudgetSec` | float | 45 | 10-90 | 45-60 (narrowed from 30-60, cross-review C-W17: below 45 a safe reseed/timeout corner fails I-LSC-2, for example 30-41 against the default 7 × 6 = 42) | Sec | Bounded by Session's F7 travel deadline (`max(t_travel + TravelLoadTimeoutSec, t_ready + TravelAckGraceSec)`); it no longer has to stay below `TravelLoadTimeoutSec` (Session CR 11). High: idle players. Low: premature fallback |
| `LayoutSliceWorkUnitsLimit` | int | 4,096 (*provisional pending spike*) | 256-65,536 | — | Count | Cells or rays per time slice. High: hitches. Low: slow builds |
| `LayoutExactRefineMaxCount` | int | 2,000 | 200-20,000 | 1,000-4,000 | Count | Cap on exact path refinements in Validate's detour p95 check (ADR-0014 §5.2); exceeding it fails the attempt with `RefineBudgetExceeded` (deterministic, never wall-clock). Added by user decision 2026-10-25. High: long Validate on a degenerate layout. Low: valid layouts fail and reseed |
| `LayoutWalkableCellsMinCount` | int | 2,000 | 1,500-4,000 | 2,000-2,025 (cross-review C-W15: above 2,025 cells the V-AREA floor exceeds the density cap `45 × (35 + 10)` at the I-LSC-1 floor 35 (the `NpcCount` hard-valid minimum; the safe minimum is 45 since cross-review 2026-10-25-b), so the preset is invalid there; each +45 cells lifts the `NpcCount` floor by 1) | Count | Lower bound of V-AREA; must be ≤ `LayoutWalkableCellsMaxCount`. Small: zones and courts cannot separate |
| `LayoutWalkableCellsMaxCount` | int | 3,250 (13,000 m²; unchanged) | 1,500-4,000 | 2,700-3,600 (was one shared 2,000-2,700 row with the Min key, cross-review C-W15: the default 3,250 lay outside it) | Count | Absolute ceiling on layout area, independent of pawn count. At the defaults it does not bind: the V-DENSITY term `WalkableCellsPerPawnMaxCount × (NpcCount + 2 · TeamSize)` = 2,700 cells at `NpcCount` 50, so the effective band is 2,000-2,700; the ceiling binds only above `NpcCount` 50 (a 3,250-cell layout needs 72 pawns). Must be ≥ Min. Large: thin crowd (anti-pillar). Small (below 2,700): caps the map below the density term at `NpcCount` 50 and starves the Generator's walkable target (PCG F1 band 2,046-2,260 cells) |
| `WalkableCellsPerPawnMaxCount` | int | 45 (180 m²) | 30-75 | 40-50 | Count | V-AREA/V-DENSITY. High: a thin crowd in which a lone figure reads as a player |
| `EmergenceSocketsMinCount` / `MaxCount` | int | 12 / 24 | 8-32 | 12-20 | Count | Low: starvation, predictable alleys. High: alleys everywhere, harder occlusion |
| `EmergenceCellsPerSocketCount` | int | 175 | 100-300 | 150-220 | Count | Area per socket (D5) |
| `EmergenceWitnessRangeCm` | float | 3,200 | 2,500-4,000 | = `SightlineMaxCm` | Cm | Must be ≥ `SightlineMaxCm` (TDC invariant), or pops become visible beyond the range |
| `EmergenceWitnessCellsMaxCount` | int | 37 | 10-55 | 30-40 (bounded below by Generator I-PCG-2, `≥ CityGenWitnessCellsTargetCount` 30; cross-review C-W18) | Count | High: sockets often blocked. Low: unbuildable alleys |
| `EmergenceWitnessDilationCm` | float | 200 | 0-400 | 150-300 | Cm | Covers interpolation (about 300 ms at run speed); also R5b. Low: edge pops. High: starvation |
| `EmergenceClearRadiusCm` | float | 300 | 150-500 | 250-400 | Cm | Low: walk-ins bump pawns. High: starvation |
| `EmergenceSpacingCm` | float | 1,200 | 800-2,000 | 1,000-1,600 | Cm | Low: sockets see each other. High: too few fit |
| `EmergenceSocketCooldownSec` | float | 20 | 5-60 | 15-30 | Sec | Low: visible pairs from one alley. High: starvation |
| `EmergenceRetrySec` | float | 1 | 0.2-5 | 0.5-2 | Sec | Retry cadence (emergence and takeover) |
| `AmbientEmergencePerSocketMinPerMin` | float | 0.5 | 0.2-2 | 0.4-0.55 (bounded above by Crowd I-CNB-3, (60/80)·(1−0.25) = 0.5625 at Crowd's defaults; cross-review C-W18) | Rate | Crowd floor for entries and for exits (D9). Low: emergence becomes rare and so meaningful |
| `EmergenceExitCancelMaxPct` | float | 25 | 10-50 | 20-30 | Pct | Exit-rate dip allowed per 60 s (R5.7). High: kills visible as fewer exits. Low: frequent deferrals |
| `EmergenceDeficitMaxCount` | int | 3 | 0-6 | 2-4 | Count | NPCs the population may run short. High: thin crowd after kill bursts |
| `RespawnStarvationSec` | float | 10 | 5-30 | 5-20 | Sec | Takeover widening steps (R5b.5). Low: respawns drift from the team's side. High: dead time (P4) |
| `RespawnPoolMinExpectedCount` | float | 4 | 2-8 | 3-6 | Count | V-RESPAWN (D6b). Low: frequent starvation. High: zones must be enclosed |
| `RespawnViewerCount` | int | 2 | 1-5 | 2-3 | Count | k in D6b |
| `InformantRespawnPathCm` | float | 4,500 | 3,000-7,000 | 4,000-5,500 | Cm | "At their team's spawn" (locked); too high stops it meaning that |
| `StartCellSpacingCm` | float | 800 | 300-1,200 | 600-1,000 | Cm | Low: teammates bunch. High: cells do not fit |
| `StartCellMaxRadiusCm` | float | 1,600 | 1,200-2,500 | 1,500-2,000 | Cm | High: T_contact infeasible. Low: teammates bunch. D50: was 1,800 |
| `TeamZoneRadiusCm` | float | 3,500 | 2,000-4,500 | 3,000-4,000 | Cm | Density disc (D6). High: discs crowd the map. Low: D6 window infeasible |
| `ZoneCandidatesMinCount` | int | 2 | 2-4 | 2 (3-4 only with `CityGenZoneCandidatesCount` 4; cross-review C-W19) | Count | Candidate zones per layout; the graybox authors 4. **Invariant I-PCG-7 (reject, proposed for TDC): `CityGenZoneCandidatesCount` (Generator, 2 or 4, default 2) ≥ `ZoneCandidatesMinCount`**: both keys name "candidate zones per layout" and the Generator is the supplier, so a Min of 3-4 rejects a 2-zone Generator (a Generator value of 3 does not exist) |
| `ZoneDensityMaxMult` | float | 1.5 (reverted from the 1.8 workaround once `NpcCount` became 50, 2026-10-25) | 1.2-2.5 | 1.5-2.0 | Mult | Zone pawn density cap. High: "the crowded street is a spawn". Low: window infeasible |
| `ZoneNpcWindowMinCount` | int | 3 | 2-6 | 3-5 | Count | V-ZONEFILL. Low: the zone NPC count becomes a constant |
| `ZoneContactMinSec` | float | 15 | 8-40 | 15-25 | Sec | Low: enemies see a zone before it disperses. High: zones cannot fit |
| `ZoneNpcDensityMinMult` | float | 0.8 | 0.5-1.5 | 0.7-1.0 | Mult | Zone density floor (D6) |
| `ZonePlayerShareMaxPct` | float | 40 | 25-50 | 33-40 | Pct | P1 cluster guard. High: player-heavy clusters. Low: NPC supply runs out |
| `ZoneNpcShareMaxPct` | float | 60 | 30-70 | 50-60 | Pct | High: the rest of the city empties at start. Low: D6 infeasible |
| `ZoneSeparationPathCm` | float | 7,000 | 6,000-12,000 | 7,000-9,000 | Cm | Nearest start cells of two used zones. Low: early contact (a hunt). High: cannot fit |
| `ZoneParityPct` | float | 15 | 5-25 | 10-15 | Pct | V-ZONE. High: one zone favoured |
| `StartToFinalMinPathCm` | float | 4,400 (60 → 44 m, decision D4 2026-10-25) | 4,000-10,000 | 4,000-4,400 on the generator (hand-built maps may use up to 8,000 if they still pass `FinalCandidatesMinCount`) | Cm | V-SEP: every start cell of every candidate zone at least this far by path from every final-eligible court. Audience `ServerOnly`, latch `Round` (table preamble). Owner: Level Socket Contract; read by the Generator (S4, G-PRE guard 47.5 m) and Objectives (final-eligible pool). Low: the final sits close to a spawn (below 40 m a team reaches it in under 30 s of walking). High: too few final candidates; above 46.3 m the generator's S4 geometry cannot reach 6 final-eligible courts (G-PRE fails every candidate). **Invariants:** (1) the floor must admit at least `FinalCandidatesMinCount` (6, never lowered to make a layout pass) final-eligible courts at `CourtSpacingCm` spacing: on the generator `x + y ≤ 112 − 1.08 · F` gives F ≤ 46.3 m at radius 16 m (D5 feasibility note, D50), hence the safe maximum 4,400; (2) `CityGenFinalEligibleTargetCount` ≥ `FinalCandidatesMinCount` (TDC I-PCG-5); (3) the lower bound 4,000 (about 29 s of walking at 1.4 m/s) keeps V-SEP meaningful, and `ZoneSeparationPathCm` (70 m) is unchanged. **Resolved (D50, 2026-10-25):** at the mean S4 geometry and radius 16 m the margin is 2.5 m (court (31, 31): x + y = 62 against 64.48), and the grid-only enumeration gives p_g 0.615 at `CityGenSkeletonJitterCount` 0 (`production/qa/spikes/2026-10-25_citygen-oq9-final-court-enumeration.md`). |
| `EnemyZoneToPrelimMinCm` | float | 5,000 | 3,000-8,000 | 4,000-6,000 | Cm | Low: prelims next to the enemy start |
| `LegZoneToP1MinCm` / `LegP1ToP2MinCm` / `LegP2ToFinalMinCm` | float | 3,000 / 4,000 / 4,000 | 1,500-8,000 | 3,000-5,000 | Cm | Leg floors; also prevent division by zero in D1 |
| `ChainLengthMinCm` / `MaxCm` | float | 12,000 / 24,000 (**provisional**) | 10,000-40,000 | from the graybox histogram | Cm | D4 (target 17,000). High: a walking contest. Low: decided at the courts |
| `ChainFairnessMaxMult` | float | 1.10 | 1.05-1.20 | 1.08-1.12 | Mult | Low: few combos pass. High: tilted maps |
| `FinalLegFairnessMaxMult` | float | 1.15 | 1.05-1.30 | 1.10-1.18 | Mult | Final race tolerance |
| `TraversalFairnessMaxMult` † | float | 1.20 | 1.10-1.40 | 1.15-1.25 | Mult | Parkour shortcut cap |
| `ReachDetourMult` | float | **derived**: = `DetourMaxMult` (2.0) | follows `DetourMaxMult` (1.8-2.4) | follows `DetourMaxMult` | Mult | D2. **Not an independent knob since cross-review C-W19 (2026-10-25):** the reach budget uses the V-NAV-3 maximum detour ratio, so the one tunable is `DetourMaxMult`; this name stays in the schema as a read-only alias so the D2 formula and existing references hold, and TDC I-LSC-4 (`ReachDetourMult = DetourMaxMult`) is true by construction (retire it as an invariant, keep it as the alias note) |
| `ReachBudgetFloorCm` / `MaxCm` | float | 2,000 / 20,000 | 1,000-3,000 / 15,000-30,000 | — | Cm | D2 |
| `DetourP95MaxMult` / `DetourMaxMult` | float | 1.6 / 2.0 | 1.4-1.8 / 1.8-2.4 | — | Mult | High: mazes. Low: grid-only cities |
| `NavMinClearWidthCm` | float | 280 | 240-400 | 280-320 | Cm | Low: crowd jams, bodies block. High: alley kit invalid |
| `NavIslandRejectCellsCount` | int | 1 | 1-4 | 1 | Count | Above 1, sealed pockets may survive |
| `SightlineMaxCm` | float | 3,200 | 2,500-4,000 | 3,000-3,200 | Cm | The art 6.7 reading range. High: sniping across the city |
| `CourtSpacingCm` | float | 2,400 | 1,600-4,000 | 2,400-3,200 | Cm | Low: courts cluster, live sites guessable |
| `PrelimCandidatesPerStageMinCount` / `FinalCandidatesMinCount` | int | 3 / 6 | 2-6 / 3-8 | 3 / 6 (narrowed from 3-5 / `TeamSize + 1`-8, cross-review C-W17 / 2B-W9d: TDC I-PCG-6 rejects `CityGenCourtTargetCount` (default 16, safe 16-18) < `FinalCandidatesMinCount + 4 · PrelimCandidatesPerStageMinCount − 2`, so Prelim 4 needs 20 and Final 7-8 needs 17-18 courts; raise the Generator target first, then these) | Count | Low: sites predictable; a final count ≤ `TeamSize` lets a full team camp every final candidate (Objectives SP-4). High: more courts than fit (N_C, D5) |
| `MinFairCombosCount` | int | 12 | 6-50 | 12-24 | Count | Low: assignment guessable. High: frequent reseeds |
| `RoleShareMaxPct` | float | 50 | 30-70 | 40-50 | Pct | High: an "obvious" court. Low: frequent reseeds |
| `FinalAmbiguityMinCount` | int | 3 | 2-5 | 3-4 | Count | D8 prefix ambiguity. Low: a team can deduce the final from its own chain |
| `SunPoolSamplesMinCount` | int | 144 (36 m²) | 144-192 | 144-176 | Count | Low: no exposure moment (art 2.5) |
| `SunPoolParityPct` | float | 20 | 10-30 | 15-25 | Pct | High: one court stands out |
| `SunPoolBeaconRangeCm` / `bSunPoolBeaconIsReject` | float / bool | 4,000 / false | 3,000-6,000 | — | Cm | Art 2.5 flag; true once the playtest confirms the threshold |
| `BushSitesMinCount` / `MaxCount` | int | 8 / 14 | 4-20 | 8-14 | Count | Concealment availability (Concealment GDD may refine); max also bounded by sectors (D5) |
| `BushCellsPerSiteCount` | int | 250 | 150-400 | 200-300 | Count | Area per bush (D5) |
| `BushSpacingCm` | float | 1,200 | 600-2,000 | 1,000-1,600 | Cm | Low: bush clusters (hide-and-wait). High: cannot fit |
| `BushPerSectorMaxCount` | int | 2 | 1-4 | 2 | Count | Clustering cap per 40 m sector |
| `BushCourtExclusionCm` | float | 600 | 300-1,200 | 600-800 | Cm | Low: bush camping at courts. High: few planter spots |
| `BushChainBufferCm` / `BushChainDiffMaxCount` | float / int | 2,000 / 2 | 1,000-3,000 / 0-4 | — | Cm / Count | Concealment fairness between chains |
| `BushBackdropTargetPct` / `FloorPct` | float | 50 / 40 | 20-80 | 40-60 | Pct | Warn-only V-BUSH-3 |
| `RouteCoverageRadiusCm` / `RouteCoverageMinPct` | float | 1,200 / 95 | 600-2,000 / 85-100 | — | Cm / Pct | Low coverage: empty streets, a lonely figure is a player |
| `RouteMaxGapCm` | float | 1,000 | 600-1,500 | 800-1,000 | Cm | V-ROUTE: largest distance from any legal player position to a route edge (Crowd CR 17). High: off-route spots only players use (a P1 tell). Low: route graph too dense to author |
| `RouteCyclesMinCount` | int | 8 | 5-20 | 8-12 | Count | Low: crowd shuttles on spokes (a baseline tell) |
| `TraversalPointsMinCount` / `MaxCount` / `TraversalCellsPerPointCount` † | int | 6 / 20 / 200 | 4-30 / 4-30 / 100-400 | 6-20 / 6-20 / 150-250 | Count | Parkour availability |
| `TraversalVaultHeightCm` / `TraversalClimbHeightCm` † | float | 80 / 160 | 60-120 / 120-200 | — | Cm | **Owned by Shared Character & Locomotion** (one quantity, one name; SCL CR 3); read only here, listed for reference, not registered by this GDD |
| `TraversalLandingClearCm` † | float | 100 | 60-150 | — | Cm | Owned here and registered (read by Shared Character & Locomotion). Depth of each landing and climb-top footprint (R3, V-TRAV). Low: the capsule clips the coping. High: few legal traversals |

**Interactions:** the fairness tolerances, `MinFairCombosCount`, `RoleShareMaxPct`, `FinalAmbiguityMinCount`, court counts and leg floors jointly set the Selection pass rate p (D8): loosen one when tightening another. `ZonePlayerShareMaxPct`, `ZoneNpcShareMaxPct`, `ZoneDensityMaxMult`, `ZoneNpcWindowMinCount`, `WalkableCellsPerPawnMaxCount`, `NpcCount` and `TeamSize` are coupled by the D6 `NpcCount` invariant (TDC cross-field). The chain band relates to the modelled round length (Objectives F1 `T_round`, about 9 min) only through the D4 rationale; `TargetRoundMinutes` is not read by any predicate.

## Visual/Audio Requirements

*art-director consulted. The art bible is the authority; this section states which of its rules the contract enforces as predicates (named) and which stay human tests (Acceptance Criteria, visual).*

**VA-1 No visual marker (art 6.7, 6.8).** No emergence alley, start cell or zone, court (candidate, live or unused) or bush site has any mesh, decal, trim, light, prop, sound or colour that exists because of its role; the only differences are ordinary geometry that also occurs elsewhere. Enforced by V-ZONE ("no fingerprint") and checked by the paired-capture test.

**VA-2 Kit usage per socket (art 6.2, M01-M12).**

| Socket | Built from | Must never use |
|---|---|---|
| Emergence alley | M01 walls, M05 Esquina quoins at wall ends, M06 paving; the 4 m alley has a Z or L jog with bends on M05 | A niche or recess for the origin. An origin-only floor variant. M02 as a passable door (sealed in MVP, so `Doorway` is invalid) |
| Court | The host area's own recipe modules; a pool cut by M04 parapet and cornice; plain 4 or 6 m openings with M05 quoins; the R3 fixture | Hue, glow, particles, banners, team signs (6.8) |
| BushSite | The unique 2.0 × 2.0 × 0.35 m planter and dome family (3.6), flush against M01, M05, the M07/M08 edges or M10 kerbs | A recess, a plaza centre, a court or its pool |
| TraversalPoint | M01 vault (0.8 m) and climb (1.6 m) variants, platform edges, M10 quay walls; limewash coping 0.1 m thick with a 0.05 m overhang on a top at least 0.4 m deep | Coping on any non-traversable surface. A ledge at the 1.6 m dado line |

**VA-3 Sun pool (art 2.4, 2.5).** Level lighting cut only by parapet and cornice geometry, identical on **every** court; pool ratio 5:1 (5,200 K sun over 7,500 K fill, 10-15 cm penumbra); never the only lit patch in its precinct. Area and parity are V-POOL; the 40 m beacon check is V-POOL-40, whose first lever is the pool ratio toward 4:1 (`SunPoolRatioMult`, a lighting value, Open Question 4).

**VA-4 Bush silhouette (art 3.6, 6.5).** V-BUSH-1/2 enforce dimensions, 90-degree rotation only, at most 2 variants within 5% of the master outline, one planter one bush, seating against architecture and the Dome Ban; V-BUSH-3 is Warn. The human 25 m greyscale and colour-swatch tests decide, on the graybox before the vertical slice and on generated layouts later (art 6.10 test 3).

**VA-5 Graybox stand-ins.** Real 2.0 m grid, real module outlines and the real bush silhouette, with one neutral material per surface class (dado L* 35, wall L* 80, paving L* 72, dome core L* 30 with tips at L* 42-48; a sphere or cube bush is not acceptable). Door bays are sealed leaves at L* 42-48. Every graybox court has its fixture and pool from day one. Debug draw (socket colours, labels, W(O), respawn regions, nav rays) exists only in non-Shipping builds behind a toggle that defaults off; evidence is captured with it off.

**VA-6 Emergence and takeover presentation (art 5.2.1).** An NPC is created at the hidden origin and walks out on normal locomotion; a takeover happens to an unseen pawn where it stands. Neither has VFX, fade, dissolve, flash or teleport; shadow, reflection and decal rules apply as everywhere else.

**VA-7 Audio** (audio-director to detail in Diegetic Audio, VS): emergence, absorption and takeover play **no sound of their own** (no stinger, whoosh, door creak or special footstep); footsteps use the shared surface path; no ambient emitter is placed at or keyed to a socket (ambience is per area type); courts carry no sonic marker; the ambient mix follows art 2.4 (low-medium, steady, dry-warm).

**Art bible amendments requested** (art-director revision pass; not applied here): 6.5 rule 6 records the 6 m court exclusion; 6.4/6.7 set 2.8 m as the alley clear-width minimum; 6.7 states that a one-module jog does not break a sightline (offset ≥ corridor clear width); 6.2 gives Market and Temple areas "service alleys"; 6.10 lists editor-only markers as the only debug art; 2.5 names the pool ratio as data and the 40 m check as a contract predicate.

> **Asset Spec** — after the art bible is approved, run `/asset-spec system:level-socket-contract` for the planter, fixture family and alley-mouth kit runs.

## Game Feel

The contract has no input or moment-to-moment feel of its own. Its targets are what the player must **not** feel:

- **No pop.** No player ever sees a pawn appear, or a body change hands. Zero visible spawns (AC-11).
- **No hitch.** Build, nav, validation and Selection are time-sliced (R4.3); no game-thread frame over 33 ms on the reference machine.
- **No wait you notice.** On the graybox `Preparing` adds at most 1 s of layout time (target under 0.1 s); a generated layout plus reseeds is bounded by `LayoutTotalBudgetSec` (45 s), covered by the briefing screen (art 2.3). An informant's respawn adds at most 2 × `RespawnStarvationSec` beyond Penalty's delay in all but starved cases.
- **No déjà vu.** On the graybox the geometry repeats, but the used zones, which zone is yours and which courts are live are drawn per round.
- **No feeling of a tilted map.** Playtest question: "Did either team's route feel shorter or safer?" Measured per zone, because zone → team is random (AC-55).

## UI Requirements

The contract has **no player-facing UI** and publishes nothing to the HUD or Teammate Markers; no UI may show socket, zone or site data except as Objectives Chain decides under its own audiences. Developer UI only, compiled out of Shipping: a provider **Validate** button with a per-predicate panel (pass/fail, measured value, threshold); the debug overlay toggle `stealth.layout.debug` (default off); and the server command `stealth.layout.report` (attempt records of the current round). No UX spec is needed.

## Cross-References

| This GDD references | Owner document | What is used | Status |
|---|---|---|---|
| Locked rules (shared final, informant respawn at the team's spawn, server seed, identical layout, invalid layouts rejected) and pillars P1/P4, anti-pillars large-map and hide-and-wait | design/gdd/game-concept.md; production/session-state/active.md | R3, R5b, R8, R9, R10, D4 | Locked |
| Informant respawn takeover (decided rule C1) | orchestrator pipeline rules; confirmed by the user 2026-10-25 | R5b | Decided 2026-10-24, confirmed 2026-10-25 |
| `TeamSize`, `NpcCount`, `TargetRoundMinutes`, CR-15 key convention, freeze at `Preparing`, `TuningHash` | design/gdd/tuning-data-config.md | Tuning Knobs, D4, D6 | In Review |
| `Seed`, `FRoundParams`, `FRoundReadiness`, rule 11.6 ack, rule 14 secret sub-seeds, F5 domains, F7 `T_layout`, `TravelLoadTimeoutSec` | design/gdd/session-travel.md | R2, R4, R8, R9 | In Review |
| `GetTeam`, crowd adoption, `BindPawn`, "awaiting pawn", disconnect hand-over | design/gdd/participant-state-roster.md | R5b, R6, Edge Cases | In Review |
| Phases, P3-P6 preconditions, F5 spawn shuffle and zone share, R18 re-admission, Event Log header | design/gdd/round-match-framework.md | R4, R6, R8, R5b | In Review |
| Card rules for replacement and takeover (CP-12, CP-12b) | design/gdd/character-pool-appearance.md | R5b.4 | In Review |
| Walk/run speeds, spawn factory, traversal heights | design/gdd/shared-character-locomotion.md | D4, D6, R3 | In Review (*traversal: provisional pending spike*) |
| Context prompts from site data (UI-3) | design/gdd/input-interaction-framework.md | Dependencies | In Review |
| Audiences, `PostRound`; ambient schedule, density function, Tell Matrix; stall pressure, site audiences; hide volumes; respawn delay; Generator | information-visibility-policy, crowd-npc-behaviour, objectives-chain, concealment-bushes, npc-kill-penalty-informants, procedural-city-generator (design/gdd/) | R5-R9 | In Review (cross-reviewed 2026-10-24; second-round reviews done 2026-10-25, provisional until each re-review lands) |
| Visual rules 2.3-2.5, 3.3, 3.6, 5.2.1, 6.2-6.8, 6.10 | design/art/art-bible.md | R3, R5, R7, VA-1 to VA-7 | Written |
| UE 5.8 navigation (runtime generation, per-mesh nav areas) | docs/engine-reference/unreal/modules/navigation.md | R4.3, NavArea | Reference (5.8 block verified 2026-10-17) |

**Cross-GDD change requests** (historic, as sent 2026-10-24; dispositions are in each receiving GDD, and items marked "to write" have since been written):

1. **systems-index.md:** row 8 category `Core` (the Dependency Map and design order already say Core); add Session & Travel, Participant State & Roster and Information Visibility Policy as soft upstream links; add Round and NPC-Kill Penalty & Informants as dependents; row 16 (Generator) must not depend on Round & Match Framework (the Generator never references Round, R4.1); the Overview bullet should read "candidate start zones, shared NPC emergence sockets and the informant respawn takeover region", replacing "spawn points" and "used identically for … informant respawn".
2. **Round & Match Framework:** R18 and the Cross-References line "respawns them at `TeamSpawn` through an emergence socket" become "through the LSC takeover (R5b)"; rename `TeamZoneRadiusM` to `TeamZoneRadiusCm` in F5; F5's `ZoneShare_z ≤ 0.5` assert stays (LSC guarantees ≤ 0.4); handle `PlacementInfeasible` and `NpcCountTooLow` by returning to Lobby; Round's spawn shuffle uses `SpawnOrder`, which LSC no longer uses.
3. **Session & Travel F5:** add server-only domains `EmergencePick`, `RespawnPick` and `AmbientSchedule` (counter-based draws keyed by sequence id); the T_layout range change is already applied.
4. **Tuning Data & Config:** register the keys above, including the new ones (`LayoutSliceWorkUnitsLimit`, `StartCellMaxRadiusCm`, `ZoneCandidatesMinCount`, `ZoneNpcWindowMinCount`, `ZoneParityPct`, `FinalAmbiguityMinCount`, `RespawnStarvationSec`, `RespawnPoolMinExpectedCount`, `RespawnViewerCount`, `AmbientEmergencePerSocketMinPerMin`, `EmergenceExitCancelMaxPct`, `EmergenceDeficitMaxCount`) and the retirements (`InformantRespawnSocketsMinCount`, `EmergenceStarvationSec`, `ZoneSeparationEuclidCm`). Cross-field invariants: the D6 `NpcCount` bound (35 at defaults, replacing "≥ 32"); `(1 + LayoutMaxReseedAttemptsCount) × LayoutAttemptTimeoutSec ≤ LayoutTotalBudgetSec`; `EmergenceWitnessRangeCm ≥ SightlineMaxCm`; `ReachDetourMult = DetourMaxMult`; start cells ≥ `TeamSize + 1`.
5. **Participant State & Roster:** R9 crowd adoption: "picks an existing Unbound crowd pawn at or emerging from those sockets" becomes "the pawn returned by LSC `PickRespawnPawn` (R5b): an unseen Unbound NPC inside the team's respawn region"; Edge Cases line on informant respawn: same wording.
6. **Character Pool & Appearance:** CP-12b "the informant pawn is new with a freshly taken card" becomes "the informant takes over an existing NPC pawn and keeps its card; the replacement NPC's card is drawn by `DrawReplacement()`, CP-12(a): uniform over the vacant cards, the victim's own card only when it is the sole vacancy" (decided rule C1); Cross-GDD item 13 drops informant respawn from the emergence-socket list.
7. **Shared Character & Locomotion:** R2 drops "informant respawn, if it creates a pawn rather than adopting one" (respawn never creates a pawn); traversal heights must equal `TraversalVaultHeightCm`/`TraversalClimbHeightCm` after the spike; supply the walk and run speed keys.
8. **NPC-Kill Penalty & Informants (to write):** call `PickRespawnPawn(Team, RequestSeq)` (superseded by Penalty CR-5: `PickRespawnPawn(Team, RequestSeq, ExcludePawn)`, R5b) after the respawn delay; own the delay key and the Roster bind; treat `None` as "keep waiting"; a killed informant's body is removed like an NPC corpse and triggers `RequestEmergence(Replacement)` whose card is `DrawReplacement()` (Character Pool CP-12(a): uniform over the vacant cards; the victim's own card only when it is the sole vacancy).
9. **Crowd & NPC Behaviour (to write):** ambient entries and exits at ≥ `AmbientEmergencePerSocketMinPerMin` per socket each from the `AmbientSchedule` stream; replacements take the next entry slot and cancel a uniform upcoming exit within the R5.7 bound; release a taken-over pawn without a visible behaviour snap; density function blind to `FSiteAssignment` and treating used zone discs as full; Baseline and Tell Matrix row "emerging from or leaving by an alley" (deviation none; NPCs do it: yes; owners LSC + Crowd) and "an NPC near a team's side starts acting as a player" (takeover; owner Crowd + Shared Character).
10. **Objectives Chain (to write):** consume `FSiteAssignment` (ServerOnly) and own every narrower audience; interaction actors exist only on the server or on every court with identical net settings (R2.6); never add a per-court cue; assume the final-eligible courts are a small set the public can guess, with D8 prefix ambiguity as the only guarantee.
11. **Concealment, Information Visibility Policy, Input & Interaction:** Concealment uses `BushId`/`HideVolume`/`EntryAxis` from the Manifest-built `BushSite`; the Policy adds `FSiteAssignment`, used zones and zone → team as ServerOnly facts released to `PostRound` in `Ended`; Input UI-3 takes objective prompts from Objectives' team-audience data.
12. **Art bible:** the amendments under Visual/Audio.
13. **Canonical vocabulary (pipeline):** add `EmergenceSocket`, `StartCell`, `ObjectiveCourt`, `RespawnRegion`, `PickRespawnPawn` and `FLayoutManifest`; `TeamSpawn(Team)`, `ObjectivePrelimSite(Team, Stage)` and `ObjectiveFinalSite` are roles resolved by Selection; "TeamSpawn (also informant respawn)" now means the respawn region, not a socket.

**Change requests received (reconciliation 2026-10-24)**

| From | Request | Disposition |
|---|---|---|
| NPC-Kill Penalty CR-5 | `PickRespawnPawn(Team, RequestSeq, ExcludePawn)`; every informant-body cause (PEN-R7) uses the call | APPLIED (R5b.1-2, Dependencies row; server-only parameter, not a schema change) |
| NPC-Kill Penalty CR-13 / Generator CR 11d | No objective fixture or path closable by one standing pawn: court ingresses, bridges and stairs ≥ `NavMinClearWidthCm`; ≥ 1.2 m clear floor on all four fixture faces | APPLIED (V-NAV-2 clause; R3 court row) |
| Crowd CR 8 | R5b.4 replacement card is `DrawReplacement()` (CP-12a) | APPLIED (R5b.4) |
| Crowd CR 8 | Publish touchable-prop records (stall rails, walls, shrines) in the Manifest for the idle overlay | DECLINED for MVP (user decision 2026-10-25): no touchable-prop records; Crowd's idle-touch overlay touch half stays off |
| Crowd CR 17 | `CrowdRoute` covers every legal player position within 10 m | APPLIED (V-ROUTE clause, new key `RouteMaxGapCm` 10 m) |
| Crowd CR 17 | Pre-baked route-edge paths in the Manifest | APPLIED 2026-10-25 (user decision) as a server-side path per `CrowdRoute` edge (Socket vocabulary row), not in the replicated Manifest |
| Objectives CR 8 (a) | Each fixture exposes 4 interact sides, built locally, identical on every court | APPLIED (R3 court row; `FixtureInteractSideCount` stays Objectives-owned) |
| Objectives CR 8 (b) | Selection keeps 1 court per (team, stage) | ALREADY DONE (R9.2) |
| Objectives CR 8 (c) / Generator CR 11c | Role-blind `ClueTag` per court, covered by the layout hash, each clue matching ≤ 3 courts as a V-* predicate | APPLIED (R3 court row as a pure function of published geometry, rule version inside `GeneratorKitVersion` so D7's byte format is unchanged; V-CLUE, reading Objectives' `ClueMatchMaxCount`; the duplicate LSC key was dropped 2026-10-25) |
| Objectives CR 8 (d) / Generator CR 11a | Final-eligible courts: ≥ 3 ingresses and no bush within 12 m, if the graybox affords it | DECLINED as a final-only rule (court geometry correlated with eligibility is a final tell; Generator CR 11a); `BushCourtExclusionCm` is already one role-blind key; raising it and the ingress count for every court is Open Question 14, pending the graybox enumeration |
| Objectives CR 8 (e) / Generator CR 11b | `FinalCandidatesMinCount` > `TeamSize` (at least 6) | APPLIED (default 6, hard 3-8; R9.4, D5, D8, OQ 11 feasibility note) |
| Generator CR 1 (a) | D7 id ranges cover derived bay stacks and slab sizes as distinct `ModuleId`s; entries up to 3,500; R4.4 size 13.5-31.5 KB | APPLIED (D7 table, R4.4; field widths unchanged) |
| Generator CR 1 (b) | V-SIGHT measured in every direction | APPLIED (V-SIGHT) |
| Generator CR 1 (c) | A third or fourth zone may fail V-SEP with another if each is in a usable pair | APPLIED (V-SEP) |
| Generator CR 1 (d) | Draw the zone pair uniformly among usable pairs | APPLIED (R9.4 two-stage draw, D8 output range) |
| Generator CR 1 (e) | Selection rejects mirror-transposed prelim courts (CD-GDD-ALIGN) | APPLIED (R9.3 pass condition, Generated provider only) |
| Round CR 17 | Emergence `Kind = PawnRecovery`; informant respawn as C1 takeover; respawn-set eligibility | APPLIED (R5.1, R5.5: same identity-blind clear check, no team read); the takeover and eligibility parts ALREADY DONE (R5b) |
| Session & Travel CR 7 | `PublicSubSeed` only for geometry; write `LayoutAttempt`/`LayoutHash` to `FRoundReadiness`; integer-only generation | ALREADY DONE (R2, R4.2, R4.4) |
| Session & Travel CR 11 | Ack echo `(Seed, LayoutAttempt, LocalLayoutHash, ClientHash, PoolHash)`; fallback through `RequestFallbackTravel`; F7 travel deadline, budget no longer below `TravelLoadTimeoutSec` | APPLIED (R4.5, R8.3, `LayoutTotalBudgetSec` row) |
| Shared Character & Locomotion CR 3 | Traversal heights owned by SCL; walk 140 / run 450; 46-69 cm obstacles block; landing and climb-top footprint `TraversalLandingClearCm` | APPLIED (R3, V-TRAV, Tuning Knobs read-only line and row) |
| Character Pool CR 13 | Emergence serves replacement identically for all victim kinds; informants never emerge; no team-specific emergence | ALREADY DONE (R5.1, R5.4) |
| Input & Interaction CR 13 | Objective prompts from Objectives' team data, bush prompts from Manifest geometry | ALREADY DONE (superseded by Objectives OC-6: prompts identical at every court) |
| Concealment (Dependencies) | `BushSite` from the Manifest | ALREADY DONE (R2.6, R3) |
| Cross-review 2026-10-24 C-W8, C-W18 | Stale `PickRespawnPawn` signatures; safe ranges rejected by I-PCG-2 and I-CNB-3 | APPLIED (cross-review fixes 2026-10-24): outgoing request 8 marked superseded, AC-53 uses the three-argument form; `EmergenceWitnessCellsMaxCount` safe 30-40, `AmbientEmergencePerSocketMinPerMin` safe 0.4-0.55 |
| Cross-review 2026-10-24 C-W11 | Stale index and status claims | APPLIED (cross-review fixes 2026-10-24): Dependencies intro and downstream statuses (outgoing request list kept as written) |
| ADR-0014 sync 2026-10-24 | AC-38: "no marker class or asset" → no marker class or marker actor; the baked layout-data asset only as an opaque blob without role tokens (listen-server builds; strict exclusion with the Dedicated Server) | APPLIED (AC-38; Packaging note) |
| ADR-0014 / ADR-0019 sync 2026-10-24 | R5b.3: `ServerSubSeed(RespawnPick, LayoutAttempt)` keyed by `RequestSeq` → `GetServerSubSeed(RespawnPick, RespawnRequestSeq)` | APPLIED (R5b.3; same intent: counter-based, independent per request) |
| ADR-0014 sync 2026-10-24 | AC preamble test root/prefix → `Source/StealthGameplay/Private/Tests/Layout/`, `StealthGame.Layout.*` | APPLIED (AC preamble, R7.4 CI test name) |
| ADR-0014 sync 2026-10-24 | OQ1, OQ2, OQ3 answered by §6, §8.2, §8.5 | APPLIED (OQ table) |
| ADR-0014 sync 2026-10-24 | New LSC tuning key `LayoutExactRefineMaxCount` (int, default 2,000, ServerOnly, Round latch) | APPLIED 2026-10-25 (user decision; Tuning Knobs) |

## Acceptance Criteria

*qa-lead consulted (authoring and review).*

- **Automation tests** live in `Source/StealthGameplay/Private/Tests/Layout/`, named `StealthGame.Layout.<Area>.<Scenario>` (the short names below omit the prefix; ADR-0007/0008/0014 vocabulary; multi-client tests in `Source/StealthTestHarness/Private/`).
- **Determinism:** every Logic and Integration test runs on an injected clock with fixed-step ticks (no test sleeps; "± 1 frame" means ± 1 tick), with pinned seed and sub-seed lists checked in as fixtures, and statistical tests state their critical value.
- **Stubs** until the real system exists: Session & Travel (AC-5, 6, 31-35, 42), Round (AC-5, 33), Crowd (AC-12, 13, 19, 20, 39, 50), Penalty & Informants and Roster (AC-14-18, 53), Visibility Policy (AC-37), Concealment (AC-52), Objectives (AC-51). An AC meaningful only against the real system says so and is ADVISORY until then.
- **Human-test evidence** goes in `production/qa/evidence/level-socket/`: retained captures with debug draw off, the results sheet and art-director sign-off. Gates use the `testing.strict` defaults; each AC gives its story type and gate.

### Core rules

- **AC-1 (R2.4)** [Logic/BLOCKING] GIVEN fixed (Seed, GeneratorKitVersion, LayoutParamsHash), WHEN the layout is built twice in fresh worlds and once more with a different player count and join order, THEN the socket sets (SocketId, type, cell, yaw, R3 data) and Manifest bytes are identical. `Determinism.SameSeedSameSockets`
- **AC-2 (R2.2-2.3)** [Logic/BLOCKING] GIVEN a published Manifest, WHEN a client builds its geometry, THEN the RNG call counter on the build path is 0, cosmetic decoration is byte-identical on two clients, and every cosmetic component has no collision, no nav modifier and a bounds top ≤ 50 cm above its floor. `Determinism.ClientBuildNoRng`, `Determinism.CosmeticInert`
- **AC-3 (R2.5-2.7, R4.1)** [Logic/BLOCKING] GIVEN a published layout, THEN `FLayoutManifest` holds only the R4.4 fields; 0 replicated actors are spawned; the graybox Manifest has 0 entries and its `LayoutHash` changes when one collision mesh moves 1 cm; an include-graph check finds no Round↔Generator reference. `Manifest.GeometryOnly`, `Manifest.ZeroReplicatedActors`, `Manifest.GrayboxCookHash`, `Module.RoundGeneratorIsolated`
- **AC-4 (R3)** [Logic/BLOCKING] GIVEN one fixture per R3 row with a required field missing or a socket off the 2.0 m grid, WHEN validated, THEN V-SOCK rejects, names the field, and does not snap the socket onto the grid. `Validate.SocketRequiredData.<Type>`
- **AC-5 (R4.2-4.5)** [Integration/BLOCKING] GIVEN a 2-client PIE on the graybox, WHEN Round enters `Preparing`, THEN the order is `RequestLayout` → `Published` → Manifest on both clients → each client's ack sent only after recomputing `LayoutHash`, echoing (Seed, LayoutAttempt, LayoutHash, TuningHash) → `OnLayoutReady` exactly once. GIVEN a service stub that never publishes, THEN Round stays in `Preparing`. `Handshake.PreparingOrder`, `Handshake.RoundWaitsForLayout`
- **AC-6 (R4.3)** [Logic/BLOCKING] GIVEN a provider stub that takes 8 s, THEN the attempt fails with `Timeout` at 7 s ± 1 tick; GIVEN the defaults and a stub that always times out, THEN R8.3 is reached at 6 × 7 = 42 s ± 1 tick; GIVEN a test fixture that bypasses the invariant (7 s timeout, 10 reseeds), THEN R8.3 is reached at 45 s ± 1 tick and the attempt in progress is cancelled; GIVEN a preset violating `(1 + reseeds) × timeout ≤ budget`, THEN TDC rejects it. `Handshake.AttemptTimeout`, `Handshake.TotalBudget`
- **AC-7 (R5.1)** [Logic/BLOCKING] GIVEN one `RequestEmergence` of each kind (Replacement, AmbientEntry, AmbientExit, PawnRecovery), THEN pose, animation, walk-in path, delay rule and eligible sockets are identical (the spawn class is the shared pawn class; only the card source differs). GIVEN a request of kind `InformantRespawn` or `Rejoin`, THEN it is refused and logged and no pawn is created. `Emergence.KindBlind`, `Emergence.RespawnKindsRejected`
- **AC-8 (R5.2)** [Logic/BLOCKING] GIVEN analytic fixture alleys with hand-computed witness sets, THEN W(O) equals the expected cell set; 37 cells passes, 38 fails; a 7 m Z leg, a 9 m straight alley, O 5 m deep, a 1.0 m origin, two sockets in one alley, sockets 11 m apart, and an ExitPoint 1.5 m or 7 m from O or 5 m from a bush each fail with the named rule. `Validate.EmergenceOcclusion`, `Validate.AlleyShape`
- **AC-9 (R5.3)** [Logic/BLOCKING] GIVEN a socket, WHEN an NPC, a player or an informant stands in W(O) dilated by 2 m, or a pawn is within 3 m of the ExitPoint, or the socket was used under 20 s ago, THEN it is not selected at 0 s, 30 s and 90 s of waiting (never relaxed). GIVEN two same-tick requests for one socket, THEN the lower sequence id takes it and the other re-selects in that tick. `Emergence.ClearCheck.<Npc|Player|Informant>`, `Emergence.SameTickOrder`
- **AC-10 (R5.4)** [Logic/BLOCKING] GIVEN 5 clear sockets and 5,000 requests with pinned sub-seeds, THEN picks are uniform (chi-square < 13.277, df 4); changing the requester's IsPlayer, team or participant state does not change any pick; inserting extra draws in another domain does not change any pick (counter-based). `Emergence.UniformAmongClear`, `Emergence.BlindToIdentity`
- **AC-11 (R5.5, VA-6, VA-7)** [Integration/BLOCKING + Visual/BLOCKING] GIVEN a 2-client PIE, 50 NPCs and scripted fixed-step cameras, WHEN 200 emergences and 50 takeovers occur, THEN no frame shows a pawn (in frustum, passing a sight trace) before it leaves O, no VFX/audio/fade component is attached, and no pawn moves more than 1.2 × v_max × dt per tick; a retained capture of 3 emergences and 1 takeover has art-director sign-off. `Emergence.NoVisiblePop`
- **AC-12 (R5.7)** [Logic/BLOCKING] GIVEN a pinned ambient schedule, WHEN an NPC kill resolves at time t, THEN the replacement emerges at the next scheduled entry after t (never at t), one upcoming exit is cancelled, and the NPC count is constant after the slot. `Emergence.KillDecoupled`
- **AC-13 (R5.7 bound)** [Integration/BLOCKING] GIVEN 20 pinned 600 s fixed-step simulations, each with ≥ 20 kills (NPC, player and informant), THEN the entry schedule with kills equals the no-kill schedule exactly; exit schedules differ only by cancelled exits; cancelled exits never exceed 25% of scheduled exits in any 60 s window (excess replacements are deferred) and the deficit never exceeds 3. GIVEN ≥ 2,000 cancellations over the runs, THEN the cancelled exit's socket is uniform (chi-square < the df = N_E − 1 critical value at p = 0.01). `Leak.EmergenceRateBound`
- **AC-14 (R5b.1-2, R5b.4)** [Integration/BLOCKING] GIVEN 12 NPCs (some inside team A's respawn region, some outside, one already bound) and A's informant killed, WHEN the respawn resolves, THEN the participant possesses a previously Unbound NPC whose path to A's anchor is ≤ `InformantRespawnPathCm` and shorter than its path to the other used anchor, its card is unchanged, the old body is removed as a corpse, and Crowd receives exactly one `Replacement` request, whose card is `DrawReplacement()` (Character Pool CP-12(a): drawn uniformly over the vacant cards, so it is the victim's own card only when that is the sole vacancy; with a backlog of two or more vacancies the test asserts membership of the vacant set, never equality to the victim's card). GIVEN two used anchors 40 m of path apart, THEN no cell is in both teams' regions. `Respawn.TakesOverUnboundNpcInTeamRegion`, `Respawn.RegionsDisjoint`
- **AC-15 (R5b.2-3)** [Logic/BLOCKING] GIVEN pawns visible to a non-team pawn (active or informant) from 1.6 m eyes or a 3.2 m climb top, or within 2 m of such a line, WHEN 1,000 draws run, THEN none is selected; GIVEN a pawn that becomes visible between draw and commit, THEN it is dropped and the draw repeats in the same tick. `Respawn.ChosenPawnUnseen`
- **AC-16 (R5b.3)** [Logic/BLOCKING] GIVEN exactly 8 eligible and 4 ineligible pawns (seen, outside the region, or bound), WHEN 8,000 pinned-sub-seed draws run, THEN ineligible pawns are chosen 0 times and chi-square against uniform is < 18.475 (df 7); shuffling actor order leaves every pick unchanged (canonical stable-id order); the draw uses rejection sampling and no `%`. `Respawn.UniformOverEligible`
- **AC-17 (R5b, Roster)** [Integration/BLOCKING] GIVEN 100 informant respawns and 20 rejoins after `Active`, THEN spawned-actor, replicated-actor and NetGUID deltas from them are 0, the player-pawn spawn-path counter is 0, the pawn count returns to its t0 value at each replacement slot, and a rejoining Active participant possesses its own previous pawn. `Respawn.CreatesNoActors`
- **AC-18 (R5b.5)** [Logic/BLOCKING] GIVEN no unseen eligible pawn, THEN the region widens to 2 × `InformantRespawnPathCm` (cut by the nearer-anchor rule) at 10 s and to the city (minus enemy-zone regions) at 20 s on the injected clock; seen pawns are never chosen; after both widenings the request waits and logs `RespawnStarved`; no pawn is created at any point. `Respawn.StarvationWidens`

### Placement, validation and selection

- **AC-19 (R6.1-6.2)** [Logic/BLOCKING] GIVEN `RequestStartPlacement(5v5, 50)` on the graybox, THEN one placement function and one facing rule serve players and NPCs; each player gets a distinct start cell of its zone; NPC cells are ≥ 3 m apart; each used zone's NPC count lies in its D6 window, is uniform over the window across 600 pinned rounds (chi-square at the window's df, p = 0.01), and the two zones total ≤ 60% of `NpcCount`. `Placement.SameFunction`, `Placement.ZoneFillWindow`
- **AC-20 (R6.2)** [Logic/BLOCKING at t0; Integration/ADVISORY later] GIVEN 20 pinned graybox round starts, THEN at t0 the pawn density within `TeamZoneRadiusCm` of each used anchor is ≤ 1.5 × the map mean. With the real Crowd (ADVISORY until it exists), the same holds at `T_contact` and at 30 s after `Active`. `Placement.ZoneDensityCap`
- **AC-21 (R6.3)** [Integration/BLOCKING] GIVEN a 2-client PIE (works under `-nullrhi`), THEN each client's `WorldViewEnabled` event is ordered after its `Active` transition. `Placement.NoViewBeforeActive`
- **AC-22 (R7)** [Logic/BLOCKING] GIVEN one fixture per `Reject` predicate (including V-RESPAWN, V-ZONEFILL, V-SIGHT, V-ROUTE and V-POOL, whose sun raycast is byte-identical over 2 runs), each breaking only that predicate, THEN the attempt fails with exactly that id; V-POOL-40 and V-BUSH-3 failures log a Warn and do not fail; the all-pass fixture passes. `Validate.PredicateMatrix.<Id>`
- **AC-23 (R7 paths)** [Logic/BLOCKING] GIVEN a court reachable only by a vault, THEN V-REACH fails while R_trav uses the traversal path. `Validate.ReachIgnoresTraversal`
- **AC-24 (R9.1, R11.3)** [Logic/BLOCKING] GIVEN the graybox and a pinned `ServerSecret`, WHEN `Seed` varies, THEN `FSiteAssignment` is unchanged; GIVEN 200 pinned secrets, THEN at least 12 distinct assignments appear, and two pinned secrets known to differ give different used zones or live sites; draws in one domain are unchanged when another domain's call count changes; the secret override does not exist in Shipping (compile check). `Selection.SecretStreamsOnly`
- **AC-25 (R9.2-9.4)** [Logic/BLOCKING] GIVEN 1,000 pinned secrets, THEN every drawn combo uses distinct courts and meets the leg floors, enemy-zone distance, D4 band, D1 ratios and bush balance. GIVEN a fixture where one court is the final in 7 of 12 passing combos, or where one team prefix leaves only 2 distinct finals, THEN V-FAIR fails; at 6 of 12 and at 3 finals it passes. `Selection.ComboPasses`, `Selection.RoleShareAndAmbiguity`
- **AC-26 (R9.4 draw)** [Logic/BLOCKING] GIVEN a fixture with exactly 12 passing combos, WHEN 12,000 draws are made with pinned sub-seeds, THEN every combo appears and chi-square < 24.725 (df 11); GIVEN a scripted RNG sweeping its whole range, THEN bucket counts are exactly equal and values ≥ floor(2^n / 12) · 12 are rejected. `Selection.UniformDraw`
- **AC-27 (R9.6)** [Logic/BLOCKING] GIVEN any published layout, THEN exactly one `ObjectiveFinalSite` is active and every court has the same fixture class and sun-pool presence. `Selection.SingleFinal`
- **AC-28 (R11)** [Logic/BLOCKING; CI gate Config/Data] A reflection scan finds `ZoneId` and no team, stage or role field on marker classes; the graybox passes every Reject predicate; the editor button and the commandlet call one predicate function with byte-identical reports; a smoke check confirms CI blocks merges on `Graybox.Validates`. `Graybox.MarkersZoneOnly`, `Graybox.Validates`, `Graybox.EditorCommandletParity`
- **AC-29 (States)** [Logic/BLOCKING] GIVEN each state in the States table, THEN only the listed transitions occur, including `Aborted` from any state on host loss, `PlacementInfeasible` and `NpcCountTooLow`; placement outside `Published` and emergence or takeover outside `Live` are rejected and logged, including a kill resolved in the same tick as round end. `State.TransitionTable`, `State.RejectOutOfState`

### Formulas

- **AC-30 (D1-D9)** [Logic/BLOCKING] Each formula passes its test vectors (worked examples plus boundaries); comparisons are integer-scaled so exact thresholds pass. `Formula.<Dn>`

| Formula | Vectors |
|---|---|
| D1 | 230/215 → 1.070 pass; 220/200 → 1.100 pass (exact); 250/220 → 1.136 fail; final legs 70/62 → 1.129 pass |
| D2 | Euclid 90 → B 180, a 152 m path passes; Euclid 140 → 200 (cap); Euclid 10 → 40; a partial path fails at any length |
| D3 | n = 100,000 → p95 at rank 95,000; pairs under 40 m excluded; 1.60 and 2.00 pass, 1.61 and 2.01 fail; the 4 m lattice samples a 2-cell alley fixture |
| D4 | 170 m → S = 0.169; 119 and 241 m rejected; 120 and 240 m accepted |
| D5 | A 8,800 → 13/9/11; 8,000 → 12/8/10; 13,000 → 19/13/16; 8,500 → N_B 9 (half-up); N_B > 2 · N_sec fails; N_C 16 with overlap 2 passes, 15 fails (6 final + 12 prelim − 2) |
| D6 | N 50, A 8,800, A_z 1,800, P 5 → min 9, max 13, W 5, peak 1.47 × (pass); A_z 1,400 → W 2 (fail); contact path 68 m → 15.1 s pass, 60 m → 13.3 s fail; `NpcCount` 35, A 8,000, A_z 1,800 → min 8, max 10, W 3 (pass); `NpcCount` 35, A 8,100, A_z 1,750 → min 8, max 9, W 2 (fail); 34 → V-AREA band empty, `NpcCountTooLow` |
| D6b | ρA_R 13.636, φ 0.2, k 2 → 8.73 (pass); φ 0.5 → 3.41 (fail) |
| D7 | empty → `0xCBF29CE484222325`; `0x61` → `0xAF63DC4C8601EC8C`; shuffled entries give the same hash; any one-field change differs; 9 bytes per generated entry (40 per graybox record), little-endian |
| D8 | 16 courts, 6 final → N_raw 393,120; four usable pairs → 1,572,480 (formula-only boundary: 14 courts, 4 final → 137,280); p 0.5, N 5 → P_fallback 1.5625%, E_attempts 1.97 |
| D9 | pool 47 vs median 40 → 0.175 pass, 49 → 0.225 fail; bushes 4/3 pass, 5/2 fail; N_E 13 → λ floor 0.108/s, cancel bound 1.625 per minute |

### Network and determinism

- **AC-31** [Integration/BLOCKING] GIVEN a 2-client PIE with client B on a different `GeneratorKitVersion`, THEN Session resends once and disconnects B with `ParamsMismatch`, `OnClientLayoutFailed(LayoutMismatch)` logs both hashes, and client A continues to `Active`. `Network.HashMismatchDisconnect`
- **AC-32** [Integration/BLOCKING] GIVEN a client that joins after `Published`, THEN it receives the same Manifest (not initial-only) and its ack succeeds. `Network.LateJoinManifest`
- **AC-33 (R4.6)** [Integration/BLOCKING] GIVEN a 2-client PIE with client B's load delayed by 5 s on the injected clock, THEN no pawn is spawned for, possessed by or net-relevant to B before B is `Connected`. `Cross.Round.SpawnGate`
- **AC-34** [Integration/BLOCKING] GIVEN a scripted loss pattern (the first 3 Manifest packets dropped) and 20 pinned network-emulation seeds at 10% loss and 200 ms, THEN the Manifest arrives, hashes match and the ack succeeds before `TravelLoadTimeoutSec` in every run. `Network.ManifestUnderLoss`
- **AC-35** [Integration/BLOCKING] GIVEN the listen-server host quits in `Preparing` or `Active`, THEN the service is `Aborted`, `LayoutFallbackLevel` is 3 and clients return to the menu. `Network.HostLeaveAborts`
- **AC-36 (R4.3)** [Logic/BLOCKING; perf ADVISORY] GIVEN a graybox `Preparing`, THEN no slice processes more than `LayoutSliceWorkUnitsLimit` units. ADVISORY on the named reference machine (median of 3 runs): no game-thread frame over 33 ms, layout time ≤ 1 s (target < 0.1 s), Manifest wire size ≤ 15 KB for about 1,500 entries (*provisional pending spike*). `Network.PreparingNoHitch`

### Leak and Pillar 1

- **AC-37** [Integration/BLOCKING] GIVEN a 2-client PIE through `Preparing` and `Active`, THEN no replicated property or RPC payload on any client holds `FSiteAssignment`, used zones, zone → team, roles, respawn regions or sub-seeds; in `Ended` the assignment and Selection sub-seed are released to `PostRound`. `Leak.NoAssignmentOnClient`
- **AC-38** [Integration/BLOCKING at milestone gate; *blocked on the R11.2 ADR*] GIVEN a packaged client build, THEN it contains no marker class or marker actor (the baked layout-data asset is present only as an opaque blob with no role tokens in names or tagged properties; listen-server builds, with strict exclusion arriving with the Dedicated Server; ADR-0014 §6), and no asset, actor, nav-area or collision-profile name matches the role word list (Spawn, Team, Zone, Final, Prelim, Objective, Emergence, Site, Respawn) as a whole, case-sensitive token, outside a checked-in allowlist; binary symbols are excluded. `Leak.NoMarkersInClientCook`, `Leak.NoRoleStringsInCook`
- **AC-39** [Logic/BLOCKING; identical-output half ADVISORY until the real Crowd exists] GIVEN one seed and two different `FSiteAssignment`s, THEN an instrumented assignment query from Crowd fires the server assert, and Crowd's NPC placement and density output are identical. `Leak.DensityBlindToAssignment`
- **AC-40 (batch audits)** [Logic/BLOCKING on each graybox revision; ADVISORY for the Generator until it exists] GIVEN a 50-seed batch (graybox: 50 pinned secrets), THEN the mean bush distance to sites, zones and sun pools is within ± 3 m of the random-walkable-cell baseline (95% CI) and a permutation test gives p ≥ 0.05; per-socket emergence use against area share gives chi-square below the p = 0.01 critical value. `Leak.BatchAudits`

### Failure and fallback

- **AC-41 (R8.1-8.2)** [Logic/BLOCKING] GIVEN an always-failing Generated stub, THEN `LayoutAttempt` runs 0 to 5 on `PublicSubSeed(Layout, k)` with `Seed` unchanged, 6 `FLayoutAttemptRecord`s carry their failed ids, and clients receive 0 Manifests for failed attempts. GIVEN a stub that fails twice then passes, THEN `LayoutFallbackLevel` is 1; first-attempt acceptance gives 0. `Fallback.ReseedSequence`
- **AC-42 (R8.3)** [Integration/BLOCKING] GIVEN `GeneratedWithFallback` with the always-failing stub in a 2-client PIE, THEN one seamless travel to the graybox preserves match-scoped fields, Round re-enters `Preparing` with `ProviderKind = Graybox`, and the log header shows level 2 and the attempt records. `Fallback.TravelToGraybox`
- **AC-43 (R8.3-8.4)** [Logic/BLOCKING] `Generated` exhausted → Lobby, level 3; graybox fails validation → Lobby, level 3; `Generated` with no provider → immediate `NoProvider`; a start cell lost after `Published` with too few free cells → `PlacementInfeasible`, Lobby. `Fallback.AbortPaths`
- **AC-44 (R8.5)** [Logic/BLOCKING] The Event Log header holds `LayoutId`, `Seed`, `LayoutAttempt`, `LayoutFallbackLevel`, every LSC sub-seed value and the disposition; a byte scan finds no `ServerSecret`. `Fallback.EventLogHeader`

### Visual human tests (graybox, VA-5 stand-ins, debug draw off)

Pass thresholds are set so the one-sided 95% bound on the true rate meets the target; testers are counted as clusters and option positions are randomised.

- **AC-45 (VA-4)** [Visual/BLOCKING] 25 m greyscale bush test, ≥ 10 testers, ≥ 20 in-situ captures: at least 92 of 100 bush judgments correct, and at most 5 of 100 decorative-plant judgments called bushes. Evidence: `bush-25m-*.png` plus the results sheet.
- **AC-46 (VA-4)** [Visual/BLOCKING] Colour-swatch test, 4-choice, ≥ 10 testers, ≥ 200 trials: the bush swatch is picked in at most 58 of 200 (rate bound ≤ 35%; chance 25%).
- **AC-47 (art 3.3)** [Visual/BLOCKING] Static-figure test, greyscale 30 m capture of every kit piece in context: each piece gets ≥ 10 trials across ≥ 5 testers and fails at 3 or more "person" calls; overall at most 5% over ≥ 200 trials.
- **AC-48 (VA-3, V-POOL-40)** [Visual; ADVISORY in MVP, BLOCKING once `bSunPoolBeaconIsReject` = true] From 40-60 m, 4-choice among lit patches (one court pool, three non-court), ≥ 200 trials: the court is picked in at most 58 of 200.
- **AC-49 (VA-1, VA-5)** [Visual/BLOCKING] Paired captures (socket vs non-socket equivalent) per category (alley, start zone, court, respawn region): ≥ 30 pairs and ≥ 5 reviewers (≥ 150 judgments) each, pass at ≤ 80 of 150 correct (rate bound ≤ 60%).

### Cross-system

- **AC-50 (Crowd)** [Integration/BLOCKING] GIVEN a 10-minute graybox simulation, THEN 100% of living-NPC spawns and absorptions after t0 (corpse removals excluded) go through `RequestEmergence` (spawn-path counter), and the scheduler's configured entry and exit rates are each ≥ `AmbientEmergencePerSocketMinPerMin` × N_E. `Cross.Crowd.EmergenceApiAndFloor`
- **AC-51 (Objectives)** [Integration/BLOCKING; actor parity ADVISORY until Objectives exists] A static dependency test shows Objectives reads `FSiteAssignment` only through the server query API; if Objectives actors exist, every court has one with identical class and net settings, spawned in SocketId order. `Cross.Objectives.AssignmentServerOnly`
- **AC-52 (Concealment)** [Integration/BLOCKING] GIVEN a 2-client PIE, THEN the hash of every `BushSite` (`BushId`, `HideVolume`, `EntryAxis`) is identical on the server and both clients. `Cross.Concealment.BushSiteParity`
- **AC-53 (Informants)** [Integration/BLOCKING] GIVEN 100 `PickRespawnPawn(Team, RequestSeq, ExcludePawn = null)` calls without starvation, THEN 100% of returned pawns lie in the team's respawn region and Penalty binds each through Roster `BindPawn`. `Cross.Informants.RespawnRegion`
- **AC-54 (Baseline and Tell Matrix)** [document review at the cross-GDD review; ADVISORY until the Crowd GDD exists] Crowd's matrix has the rows "emerging from or leaving by an alley" (deviation none; NPCs do it: yes; owners LSC + Crowd) and "takeover of a crowd pawn" (deviation none; owner Crowd + Shared Character).
- **AC-55 (Fairness playtest)** [Integration/ADVISORY] GIVEN ≥ 180 graybox playtest rounds (each zone is used in about half of them, so ≥ 45 appearances per zone; at 30 appearances the Wilson half-width is already 0.17 and even a fair map would fail), THEN the 95% Wilson interval of ObjectiveWin per *zone* lies within 0.35-0.65 (per-team results are not used, because zone → team is random).

## Open Questions

| # | Question | Owner | Target |
|---|---|---|---|
| 1 | **Answered by ADR-0014 §6 (2026-10-24 ADR sync)**. Original question: how does UE 5.8 keep server-only authoring data (marker actors) out of the client cook: editor-only actors, a server-only data layer, or a cooked server data asset? NOT SOURCEABLE from docs/engine-reference. | unreal-specialist + technical-director (-> ADR) | Before graybox build |
| 2 | **Answered by ADR-0014 §8.2 (2026-10-24 ADR sync)** (size still measured in the graybox spike). Original question: manifest replication size and serialisation under Iris vs legacy replication; is `TArray<uint8>` fine? | ue-replication-specialist (spike, Network Insights) | Graybox spike |
| 3 | **Answered by ADR-0014 §8.5 (2026-10-24 ADR sync)**. Original question: fallback by second seamless ServerTravel (chosen) vs level streaming: confirm Session can carry the round-scoped log header across it. | network-programmer (-> ADR) | Before Generator |
| 4 | Fixed sun azimuth for the V-POOL raycast: the art bible gives only the elevation (65 degrees). Who owns `SunAzimuthDeg`, and is `SunPoolRatioMult` a lighting value or tuning? | art-director + technical-artist | Before graybox lighting pass |
| 5 | Traversal heights (0.8 / 1.6 m, possibly a 1.2 m hurdle) and whether traversal paths enter D1 at all. | gameplay-programmer (traversal spike) | Spike report |
| 6 | **Answered (user decision 2026-10-25): `NpcCount` 50.** At 60 pawns V-AREA is 8,000-10,800 m² (133-180 m² per pawn); the D6 window at A_z 1,800 is W = 5 with `ZoneDensityMaxMult` back at 1.5. Original question: is 160-180 m² per pawn (V-AREA at 50 pawns) dense enough for Pillar 1? | game-designer + Crowd GDD (user decision) | Before graybox build |
| 7 | Whether Market and Temple areas get "service alleys" (art 6.2 amendment), so emergence is not concentrated in Residential areas. | art-director | Art bible revision |
| 8 | Post-MVP `Doorway` emergence kind: needs a permanently-ajar door bay variant (never animated). | art-director | Vertical Slice |
| 9 | Could any future layout feature let players see the other zone at t0, for example via a future rooftop route (art 6.7 excludes rooftops)? If so, revisit the D6 share cap. | level-designer | When rooftops are proposed |
| 10 | The listen-server host knows every secret. Accepted for the prototype; must the playtest protocol note it? | producer | MVP playtest plan |
| 11 | Spatial feasibility at the revised defaults (4 candidate zones with 16 m start sets, 70 m start-set separation, T_contact ≥ 15 s, ≥ 16 courts at 24 m (≥ 6 final-eligible since reconciliation 2026-10-24), 12-13 alleys, V-RESPAWN, V-CLUE, `RouteMaxGapCm` 10 m) inside 8,000-10,800 m²: confirm on the graybox plan, and set the chain band from the combo histogram (D4 is provisional at 120-240 m). If 16 courts do not fit, the first lever is `CourtSpacingCm`, then `FinalCandidatesMinCount` back toward `TeamSize` with a creative-director check of SP-4; the `NpcCount` 50 decision (2026-10-25, Open Question 6) widened the area band to 10,800 m². `T_contact` ≥ 15 s needs about 68 m of path from an enemy start cell to the first cell that sees the zone, so the start-set separation is 70 m plus that sight reach (up to `SightlineMaxCm`): the tightest constraint on the zone layout, to confirm first. | level-designer | Graybox plan |
| 14 | Objectives SP-4 (reconciliation 2026-10-24): may every court (role-blind; a final-only rule would make court geometry a final tell, Generator CR 11a) get at least 3 ingresses and a `BushCourtExclusionCm` raised from 6 m toward 12 m? Apply only if the graybox enumeration (OQ 11) still fits; until then 2 ingresses and 6 m stand. | level-designer + Objectives owner | Graybox plan |
| 12 | Takeover hand-off: how a crowd pawn's behaviour passes to a player without a visible snap (blend, dwell), and whether an informant should see a short orientation cue on its own client only. | Crowd + Shared Character + ux-designer | Crowd GDD |
| 15 | **RESOLVED (D50, 2026-10-25):** the grid-only 200-skeleton enumeration has run (`production/qa/spikes/2026-10-25_citygen-oq9-final-court-enumeration.md`): p_g = 0.615 (123/200) at `StartToFinalMinPathCm` 44 m, start-cell radius 16 m and `CityGenSkeletonJitterCount` 0, against 0.075 at the old baseline (18 m, jitter 1). A lower `StartToFinalMinPathCm` remains the fallback lever (safe minimum 4,000), never a lower `FinalCandidatesMinCount`. | level-designer + systems-designer | Before the Generator epic |
| 13 | Does the per-domain sub-seed list (R9.1) need Session to version its F5 domain enum, and are counter-based draws acceptable for Crowd's schedule? | Session & Travel owner (network-programmer) | Session revision |
