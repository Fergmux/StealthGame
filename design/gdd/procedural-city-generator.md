# Procedural City Generator

> **Status**: Reviewed - NEEDS REVISION 2026-10-24 (revisions applied; re-review pending); reconciled 2026-10-24; ADR-synced 2026-10-24
> **Design Review**: NEEDS REVISION 2026-10-24, lean depth (10 blocking items fixed in place; log: design/gdd/reviews/procedural-city-generator-review-log.md)
> **Creative Director Review (CD-GDD-ALIGN)**: CONCERNS 2026-10-24, then REVISED 2026-10-24 (6 revisions applied: corner-zone read made a measured pillar question with a decision rule, mirror-intercept audit and LSC request, ≥ 2 final-eligible courts outside the temple ring plus court-class and 30% prior audits, golden seeds excluded from practice and live draws, G0 data non-representative, half-predictability walkthrough test)
> **Provisional**: pending spikes R1/R4 (runtime ISM / Lumen / Chaos costs, server navmesh build) and the Manifest wire-size spike; all † values
> **Author**: design pipeline (autonomous) + level-designer, systems-designer, technical-artist, creative-director
> **Last Updated**: 2026-10-24
> **Last Verified**: 2026-10-24
> **Implements Pillar**: P1 Hide in Plain Sight (the city has no seams); P4 A Race, Not a Hunt (a fresh, fair city each round)

## Summary
The Procedural City Generator builds a fresh, compact city (about 128 m square) for every round from the public layout seed, by assembling the 2.0 m modular kit on an integer grid. It is the second implementation of the Level Socket Contract: it places streets, canals, plazas, courts, alleys, bushes and traversal edges so that the city passes every contract check, then the server validates it on its own navmesh and clients simply rebuild the same geometry from the replicated manifest. It exists so that no player can win by memorising a map while the city stays fair, readable and free of give-away places; the hand-built graybox is its first stand-in and its fallback.

> **Quick reference** — Layer: `Feature` · Priority: `MVP` (built last) · Key deps: `Level Socket Contract; Tuning Data & Config; Session & Travel (seed, soft); Round & Match Framework (ordering, soft); Shared Character & Locomotion (values, soft)`

## Overview
The Procedural City Generator ("the Generator"; not to be confused with Unreal's PCG framework, which it does not depend on) is the second implementation of the Level Socket Contract (LSC, `design/gdd/level-socket-contract.md`). It registers with the LSC layout service as the provider of kind `Generated` and, from the public geometry seed `PublicSubSeed(Layout, LayoutAttempt)` (Session & Travel F5), assembles a compact city of about 124-132 m square (default 128 m; the LSC V-AREA band at 50 pawns sets the size) from the 2.0 m modular kit (art bible 6.2, M01-M12). It hands the service the socket set and the integer Layout Manifest.

The contract owns *what* a valid city is: socket vocabulary, predicates V-*, Selection, the Manifest and `LayoutHash`, reseed and fallback, D1-D9. **This GDD does not restate those rules; it references them by id.** The Generator owns *how* a city that passes them is made: the macro method (frame, street/canal/causeway/plaza/court skeleton on the integer grid); module selection, rotation and anti-repetition (art bible 6.7); socket placement built to pass the predicates; a grid pre-validator that discards most bad cities before any geometry exists; the build split (server builds and validates, clients instantiate from the Manifest, decoration is client-side and deterministic); time, memory and instance budgets; failure modes; determinism tests and golden seeds; and the kit content the MVP Generator needs.

It is **built last** (systems index build order): the hand-built graybox implements the contract first and remains the fallback map.

> **Provisional pending spikes.** Everything in this GDD that depends on runtime navmesh build time (art bible spike R4), runtime ISM / Lumen / Chaos costs (R1, R4) or Manifest wire size (LSC Open Question 2) is provisional until those spikes report. Sections marked † carry numbers from those spikes.

*Framing (autonomous, logged): a technical provider whose player effect is indirect. No ADR exists yet; implementation choices are marked "-> ADR".*

## Player Fantasy
*Framing: indirect. creative-director viewpoint applied inline under the run's specialist budget (the LSC anchor "The city has no seams" is the parent fantasy; this GDD inherits it rather than inventing a second one).*

**A city you have never walked, that still reads like home.** Every round the streets are new: you cannot win by memorising a route, and nobody else can either. But the city never feels random. Walls run in the same bays and the same quiet rhythm, the canal edge is always a kerb, coping always means you can climb, and a court looks like every other court. You learn the *language* of the city once and then read each new sentence of it.

**Nothing is out of place.** No alley is the "spawn alley", no wall is the odd one out, no plaza is lit like a stage. Repetition is the baseline, so a person who breaks the rhythm stands out; the architecture itself never does.

**Anchor moment.** The briefing fades and you are on a street you have never seen. Within ten seconds you know where the canal runs, which way the temple is, and that the doorway rhythm on your left is the same as everywhere. You stop looking at the city and start looking at people.

*Pillars served:* P1 (placement and architecture are part of presentation, so they must carry no tell); P4 (a fresh seed means geography is learned during the race, never before it, and the contract's fairness checks keep it from deciding the race). *Anti-pillar guard:* not large-map (the city is a fixed compact size), not an identification tool (no generated landmark points at a role).

## Detailed Design
### Core Rules
*Specialists consulted: level-designer (macro method, courts, zones, alleys, sightlines, graybox use), systems-designer (RNG, candidate draws, budgets, failure policy), technical-artist (ISM, collision, navmesh, client build, kit content). Every number is a data key (Tuning Knobs) unless stated; † = provisional pending spike R1/R4 or LSC Open Question 2. LSC ids (R*, V-*, D*) refer to `design/gdd/level-socket-contract.md` as revised 2026-10-24.*

#### R-G1. Role and boundary
1. The Generator is the LSC provider of kind `Generated` (LSC R1, R4.1). The layout service calls `Build(AttemptSeed, Params)`; the Generator returns {socket set, Manifest entries, decoration rule-set id} or a failure code (R-G10). It never calls Round, Session, Selection, Crowd or Objectives, and it never sees `FSiteAssignment`.
2. The LSC owns validity (R7), Selection (R9), reseed and fallback (R8), the Manifest format and `LayoutHash` (D7). The Generator **aims** at every predicate and pre-checks most of them on the grid (R-G5); only the LSC Validate on the server navmesh decides.
3. The MVP default `LayoutProviderMode` stays `Graybox`. The Generator runs in `GeneratedWithFallback` from the Generator milestone on (build order: last).

#### R-G2. Inputs, purity and secrecy
1. Inputs are exactly: `AttemptSeed = PublicSubSeed(Layout, LayoutAttempt)` (Session F5), `GeneratorKitVersion`, and `LayoutParams` (the frozen `CityGen*` keys plus the LSC keys), hashed into `LayoutParamsHash`. Candidate count J, G-PRE thresholds, guard band and collision strategy are part of `LayoutParams` or `GeneratorKitVersion`, so purity (LSC R2.4) holds.
2. Never an input: player count, teams, connection order, time, frame, world state, `RoundIndex`, `TargetRoundMinutes` (TDC OQ-7: not a sizing hint), any `ServerSubSeed`. `GetServerSubSeed` called from the Generator module fires a server assert.
3. Output is geometry only (LSC R2.5). A modified client that runs the Generator learns the city and nothing else (Session rule 14).

#### R-G3. Deterministic RNG
1. One integer stream type: PCG32 (`uint32` outputs, integer-only; it has 64-bit integer state; ADR-0006 §6), seeded per stage from the public layout sub-seed. Generator code never includes the server-secret random headers (ADR-0007 include-lint rule). Ranged draws use multiply-shift with rejection, never `mod` and never floats. Weighted picks use integer cumulative weights; shuffles are Fisher-Yates over canonically sorted arrays.
2. **Stage streams:** `StageSeed(s, j) = Mix32(AttemptSeed, s, j)` with Session's reference mixer used as a local pure function (stage id s, candidate index j). Changing a later stage never changes an earlier stage's output. j is **never** folded into the Attempt field (that would collide with real attempts).
3. Iteration is canonical everywhere: cells row-major from the origin; blocks, corridors and sockets by stable integer id. No unordered container feeds a draw.
4. Floats appear only in *measurement* (sun raycasts, nav path lengths), quantised to cm before comparison. G-PRE itself is integer-only (DDA sightlines, integer grid paths), so goldens are stable across CPUs and compilers.

#### R-G4. The pipeline: one candidate city in twelve stages
Cells are 2.0 m ("c"). Area types are LSC R1's. Every stage is a pure function of its stage stream and earlier stages.

| Stage | Design rule | Key numbers |
|---|---|---|
| **S1 Frame** | A **square** district of `N × N` c (square because the skeleton symmetry is a transpose, S2). A 3 c perimeter row of sealed 6.4 m buildings faces an outer non-walkable water skirt. The perimeter lane inside it has a **sawtooth** (a 2 c notch every 12 c), or it becomes a 120 m sightline. N is drawn per candidate. | `N` 62-66 (64 = 128 m); walkable target F1 |
| **S2 Skeleton** | A **regular-pitch lattice**: 5 blocks per axis of 7-10 c (mean 8), 20-25 blocks. Corridor widths alley 2 c, street 3 c, canal 3 c of water + quays, causeway 6 c (art 3.3). Every straight run is at most 12 c of corridor plus the junction (≤ 14 c = 28 m wall to wall); runs are broken by a **jog** offset by at least the corridor's clear width (2 c alleys, 3 c streets and canals). Every junction is a staggered **T of 3 × 3 c**; no 4-way crossing. The skeleton is generated for the half x ≥ y and **reflected across x = y, the sun axis** (art 2.1, azimuth 45° to grid X), which preserves sun and shade exactly; cut positions are then jittered ±1 c per half with **identical topology**. The seed may rotate the result 180° (the only rotation that keeps the axis on the sun line). A node-level chain pre-check (≤ 1.06) rejects unfair skeletons before any fill. | mix by length: streets ~50%, alleys ~33%, canals ≤ `CityGenCanalShareMaxPct` (12%; one per half, crossing ≥ every 12 c; quays carved 1 c into the flanking blocks), causeway ≤ 5% (≤ 12 c). Canals were ~20% before review 2026-10-24; at that share the water cells push N_walk below the F1 band (F1 example) |
| **S3 Area types** | One **Temple** precinct on the axis near the centre: pyramid base ≤ 8 × 8 c, platform of 1-2 courses; walkable only at courses 0-2 (≤ 3.2 m); tiers above are sealed and uncoped. Two **Market** plazas, a mirror pair (a rectangle on the axis would not be transpose-symmetric, and a square ≥ 24 m fails V-SIGHT on its diagonal), each 12 × 8 c (24 × 16 m, diagonal 28.8 m); no ingress collinear with a plaza's long axis unless it jogs within 4 c of the edge. **Causeway** blocks line the canals; **Residential** is the rest. An alley or court takes its block's area type. | Residential 45-55% of walkable |
| **S4 Courts** | **Stratified**, 14-16 courts as **widened junctions** first (a T junction gives 3 ingresses for free); a block courtyard (2 openings) only in the zone-corner quadrants. Strata (review 2026-10-24, corrected to the revised LSC V-SEP): **axis-corner quadrants** (the two quadrants the x = y sun axis passes through): 3 courts each at 30-40 m path from the grid corner, on or symmetric about the axis (an off-axis final adds a fairness skew of about 2·\|x − y\| m); these form the final-eligible pool, target `CityGenFinalEligibleTargetCount` (6). **Zone-corner quadrants**: 3 courts each at 30-60 m path from the grid corner (prelim pool). **Temple ring**: 2-4 courts ≥ 15 m from the pyramid base (prelim pool). 3 × 3 to 4 × 4 c (4 × 4 in Market): the junction widened by one cell on its sun-facing sides, and the sun-side host bays drop to one storey when the pool needs it (S11.3). ≥ 24 m apart, ≥ 6 m from any ExitPoint, fixture and host recipe per LSC R3 / art 6.8. **No single-pawn chokepoint** (Penalty CR-13, Objectives CR-8): every ingress ≥ `NavMinClearWidthCm` (2.8 m, more than two capsule diameters) clear, the fixture at the court centre with ≥ 1.2 m clear floor on all four faces (4 interact sides), and every court that can be final-eligible has ≥ 3 ingresses. "Final-eligible" is **not placed** as a role: it falls out of the LSC distance predicates. **Geometry consequence (review 2026-10-24):** with corner zones every temple-ring junction lies ≤ about 58 m path from one anchor, below V-SEP's 60 m from every start cell, so **no temple-ring court is ever final-eligible** and finals always fall in the two axis corners. The CD-GDD-ALIGN requirement (≥ 2 final-eligible courts outside the temple ring, so the final is never "the temple") is met structurally; the new structural read ("the final is in one of the two axis corners") belongs to Open Question 5. A role-count pre-check (≥ `CityGenFinalEligibleTargetCount` final-eligible, never below LSC `FinalCandidatesMinCount`; ≥ 3 per prelim stage; prefix ambiguity ≥ `FinalAmbiguityMinCount`) runs in G-PRE. | courts 14-16 = 6 + 6 + 2-4 (LSC D5 N_C ≥ 14) |
| **S5 Zones** | Each candidate anchor is a **plain street cell (never a junction) on the second lattice corridor from the grid edge on both axes** (the first interior street ring, about 15-16 c from the grid corner, ±33 m from the centre at N = 64; review 2026-10-24: "about 12 c in" fell inside a block, and the perimeter lane would clip the density disc to a few metres). ≥ `TeamSize + 1` start cells, each a CrowdRoute node (S12), ≥ 4 c apart within 9 c (18 m) of the anchor; ≥ 2 area types within 35 m; ≥ 15 m from any tier above 6.4 m. At this inset the 35 m density disc is about 3,050 m² gross, about 1,950 m² walkable, a 33% margin over V-ZONEFILL's floor of about 1,467 m². **Default: 2 zones at the anti-diagonal corners** (mirror images across the axis, so V-ZONE parity is exact). 4-zone mode adds the axis corners, used by Selection only when their pair passes V-SEP and V-ZONE (and then the axis-corner courts are no longer final-eligible for that pair). G-PRE checks for every candidate zone: V-SEP (start sets no line of sight, ≥ 70 m path, `T_contact` ≥ 15 s), V-ZONEFILL (A_z ≥ 1,467 m² × (1 + guard)), V-RESPAWN (D6b E_R ≥ 4 on the grid, using `RespawnRegion` = walkable cells ≤ 45 m grid path of the anchor) and V-ZONE. | `CityGenZoneCandidatesCount` 2 |
| **S6 Facades** | Blocks are sealed masses (no interiors, art 6.2). Each building-cell edge facing a walkable cell gets one **bay**, sequenced along its facade run by the bay sequencer (R-G6). Upper storey by recipe (Residential ≤ 2 storeys, Market 1, Causeway low). Free wall ends are M05 or a pilaster; joints between unlike bays fall on a quoin line. | — |
| **S7 Emergence alleys** | **Z-alleys** through blocks of depth 7-8 c: leg l1 perpendicular to street A, a middle leg parallel, leg l3 to street B, all 2 c wide, with l1 and l3 of 2-3 c clear (LSC legs ≤ 6 m) and depth l1 + 2 + l3 ∈ {7, 8} c; the middle leg is 1-3 c clear and the Z keeps ≥ 1 c of building between it and the block's side streets. Blocks of 9-10 c take an **L-alley** with one mouth (legs 2-3 c clear) whose origin is the closed end; it is a registered emergence alley, exempt from the S11.2 spur prune. Origin mid-way along the middle leg (Z) or at the closed end (L); **ExitPoint** = the navmesh point 5.0 m of walk from O inside the leg toward the lower-SocketId street (never at the mouth: the mouth is about 9-10 m from O, outside LSC's 2-6 m), ≥ 6 m from any bush, court or traversal point. Z-alleys are not cut into quay-flanked blocks. (Review 2026-10-24: 3 + 2 + 3 c legs did not fit 7 c blocks or any 9-10 c block, and ExitPoint was unplaced.) The Generator ray-casts W(O) and demands ≤ `CityGenWitnessCellsTargetCount` (30), a margin under LSC's 37. Quotas for the 12-13 sockets: Temple fringe 2, Market 2, canal quay 3, Residential ≤ 55%. Sockets ≥ 6 c apart. | LSC D5 N_E |
| **S8 Traversal** † | Coping-tagged variants only: M01 vault (0.8 m) at quays and market edges, M01 climb (1.6 m) and platform edges at the temple, M10 quay walls. A shortcut saves path but is never needed for reachability. Every traversal variant occurs ≥ 3 times; temple stairs on 4 faces. | LSC D5 N_T |
| **S9 Bushes** | Role-blind Poisson-disc over eligible planter cells (LSC V-BUSH-1). The stage **reads no zone, court, sun pool or socket** other than the LSC exclusion distances, so the LSC batch audit finds no correlation. | LSC D5 N_B |
| **S10 Props** | Waist and eye-band props (collision or sight-blocking) per art 6.4 densities, every family ≥ 3 times or not at all; they are Manifest entries. Ground-class props are decoration (R-G8). | art 6.4 |
| **S11 Repairs** | Deterministic, bounded, in this order: (1) sightline breaker = a jog or a full-height wall segment, **never** a stall or prop (awnings have a clear underside); (2) prune: unreachable cells, spurs and nooks become building (registered S7 L-alleys excepted) (no recess ≥ 0.6 × 0.6 m, no dead-end court < 6 × 6 m, no alcove behind a stair); (3) sun-pool host regeneration round a failing court (art 6.8), up to `CityGenSunPoolRepairMaxCount`; (4) occurrence fix: a variant used 1-2 times becomes the most common same-slot variant (lowest id on ties). A repair that would change connectivity in one half only is refused. | caps per pass |
| **S12 Sockets and emission** | Derive the LSC R3 records, assign SocketIds canonically, cover floors with slabs and emit Manifest entries (R-G6, R-G7). **CrowdRoute** (review 2026-10-24): edges on corridor centrelines and through every court and plaza; nodes at every junction, court centre, plaza and emergence ExitPoint, **and every ≤ `CityGenRouteNodeSpacingCount` (3 c) along each edge**, so the LSC R6.1 NPC pool (cells within 3 m of a node, ≥ 3 m apart) holds several hundred cells, not dozens; plazas and courts wider than 6 c get a second parallel edge so that **every walkable cell lies within 10 m of an edge** (Crowd CR-17, stricter than V-ROUTE's 95% within 12 m). Start cells (S5) are route nodes. TraversalPoint from coping; NavArea per cell. If the LSC adopts touchable-prop records (Crowd CR-8), they are emitted here from props and walls already in the Manifest, role-blind. | LSC R3, R6.1, D5, D7 |

#### R-G5. Candidate draws and the grid pre-validator (G-PRE)
1. Within LSC attempt k the Generator draws up to `CityGenCandidateDrawsCount` (J = 8) candidates j = 0..J-1 (each with its own N and area mix, so candidates are independent). Each runs S1-S12 as data only and then **G-PRE**: the LSC predicates evaluated on the cell grid (BFS connectivity, integer grid path lengths, DDA sightlines, W(O) ray casts, sun-pool raycast at the fixed sun, socket counts, the zone and fairness checks, Selection enumeration on grid paths).
2. G-PRE uses the **same predicate code** as LSC Validate behind a grid backend, plus a guard band `CityGenGPreGuardBandPct` (4 points) on ratio predicates (for example fairness 1.10 is pre-checked at 1.06), because grid and navmesh path lengths differ by 5-8%. **Absolute path floors and band edges** get their own guard `CityGenGPrePathGuardPct` (8%; review 2026-10-24: the corner-zone geometry makes them the tight predicates): floors are pre-checked raised by 8% (StartToFinal 60 → 65 m, ZoneSeparation 70 → 76 m, legs 30/40/40 → 33/43/43 m, enemy zone to prelim 50 → 54 m, A_z × 1.04 on area) and ceilings lowered by 8% (chain band 120-240 → 130-222 m). The first 50-seed batch measures the sign of the grid-vs-navmesh difference; a side that never binds may then be set to 0.
3. **G-PRE predicate list (named):** V-NAV-1/2 and V-AREA on the grid; V-SOCK and D5 counts; V-EMERGE (W(O) ≤ `CityGenWitnessCellsTargetCount`); V-SIGHT (DDA, every direction); V-COURT and V-POOL (fixed-sun raycast); V-BUSH-1/2; V-ROUTE and the S12 coverage target; V-TRAV; V-ZONE; and per candidate zone V-SEP, V-ZONEFILL and V-RESPAWN (S5); then V-FAIR on grid paths with the S4 role-count pre-check. Predicates with no grid form (V-BUSH-3 backdrop, V-POOL-40) are left to LSC Validate.
4. The first candidate that passes is spawned (R-G7), the server navmesh is built and the LSC Validate and Select run. If none passes, the attempt fails `GPreExhausted` with nothing spawned, in about 1 s.
5. The loop ends by J or by a deterministic work-unit cap, **never by wall-clock**: a slow host must not publish a different city. Wall-clock bounds only the whole attempt (LSC `LayoutAttemptTimeoutSec`).
6. `CandidateIndex`, `j_used`, stage timings and G-PRE failure ids go into `FLayoutAttemptRecord.MetricsSnapshot` (server-only).

#### R-G6. Module selection, rotation and anti-repetition
1. **Placement:** every module on the 2.0 m grid and 1.6 m course, pivot bottom-left, yaw from the facade normal (exact 4 rotations), scale exactly 1.0, **never mirrored** (determinant > 0 asserted). Projections (cornice, talud) overhang the neighbour's air only (art 6.2).
2. **Bay sequencer:** per facade run, a constrained random walk over the recipe's bay types. It enforces F3 (periodicity excess ≤ 1, covering identical runs ≤ 6 bays and no repeated short pattern), door gaps of 2-5 bays with no gap value three times in a row, door bays never adjacent (art 3.3), and no unique variant within 6 m of a socket (LSC V-ZONE). After the run, the occurrence rule (every variant ≥ 3 times in the layout or not at all) is enforced by S11.4.
3. **Bay stacks** (instance budget, F2): a ground bay plus its optional upper storey and cornice is one derived mesh, baked by script from the same module sources (not a new authored variant; producer to confirm against the mass-production gate). Kept separate: vault and climb walls (traversal exactness), M05, M07, M08, M12.
4. **Floor slabs:** walkable floor is covered greedily in canonical order by M06 slabs of 2 × 2, 2 × 4, 2 × 8, 4 × 4 and 8 × 8 m on 2 m multiples (joints stay hidden by the world-aligned material); scored and drain-channel cells stay 2 × 2. The cover is computed on the server and emitted in the Manifest, so clients derive nothing.

#### R-G7. What is built where
| Work | Server (listen host or dedicated) | Each client (host included for visuals) |
|---|---|---|
| S1-S12, G-PRE | Yes, off the game thread: the pure-data stages run on one worker task (ADR-0014 §9.1) | **Never** (clients run no generator and no RNG for geometry) |
| Kit instances (ISM per mesh asset, Nanite, Static after assembly) | Yes, from the Manifest it emitted | Yes, from the replicated Manifest only; time-sliced over about 48 components (12 meshes × 4 spatial quarters), ≤ 10 ms per slice † |
| Collision | Per-module simple collision (≤ 4 primitives; stacks ≤ 2; slabs 1 box) | Same, built from the Manifest (needed for movement prediction). Fallback if R4 fails: merged run boxes derived from the Manifest on both sides; the strategy is part of `GeneratorKitVersion` |
| Navmesh | **Server only**: runtime generation over one bounds volume, auto-update off during assembly, one full build after (-> ADR) †. **Frozen after the build** (Crowd CR-18): from `HandedOff(k)` until `TearingDown(k)` no tile is rebuilt, no dirty area is processed and nothing the Generator spawns carries a dynamic nav modifier, so the navmesh Validate measured is the navmesh Crowd walks for the whole round (the prototype's dynamic rebuild is not carried over) | None: no nav data, no nav export (`CanEverAffectNavigation` off on client instances) |
| Water | No nav exported from the water component; a pawn-only blocking volume over the water (visibility and camera channels ignore it); LSC NavArea = Blocked | Same collision from the Manifest |
| LSC Validate, Select | Yes (LSC) | No |
| Decoration (R-G8) | Host renders it like any client | Yes, deterministic from public inputs |
| `LayoutHash` | Computed at emission | Recomputed after building; sent in Session's single ack (LSC R4.7) |

#### R-G8. Decoration (client-side, deterministic)
1. Inputs `PublicSubSeed(Layout, LayoutAttempt)` and `LayoutHash`; the same integer RNG (R-G3) over cells in canonical order. Content: decals, ground-class props (≤ 0.5 m), foliage variant picks; macro value variation is in the material (world-aligned, two incommensurate noise scales; no per-instance albedo offset, art 6.7).
2. Decoration has no collision, no nav effect and a bounds top ≤ 0.5 m above its floor (LSC R2.3). An element that violates this is dropped with an `ensure` in non-Shipping builds.
3. Its instance count is at most `CityGenDecorReserveCount` and the sum with kit instances stays within the 5,000 runtime cap (F2). Non-Shipping builds compare a `DecorHash` between clients in tests.

#### R-G9. Budgets (all † until R1/R4 report)
| Budget | Bound | Source |
|---|---|---|
| Attempt time | ≤ LSC `LayoutAttemptTimeoutSec` (7 s); all attempts ≤ 45 s | F5, LSC R4.5 |
| Game-thread frame (host and clients) | ≤ 33 ms; slices of ≤ LSC `LayoutSliceWorkUnitsMax` | LSC R4.5 |
| Kit + decoration instances | ≤ 5,000 (target ≤ 4,000) | F2, art 8.x.2 |
| Manifest entries | ≤ `CityGenManifestEntriesMaxCount` (3,500 = 31.5 KB raw) | F2 |
| Collision shapes | ≤ 20,000 on server and each client | art 8.x.10 |
| Server spawn + nav build | ≤ 3.0 s; steady AI + nav ≤ 1.5 ms | art R4 |
| Client build | ≤ 3.0 s p95 on the reference PC | technical-artist |
| Kit GPU (Lumen + Nanite) | ≤ 5.0 ms | art R1, 8.x.16 |
| Non-Nanite draws (bush, foliage, ground props) | ≈ 300-500 of the 1,500 budget | art 8.x.2 |

#### R-G10. Failure modes
| Failure | Detected | Result |
|---|---|---|
| No candidate passes G-PRE | after J candidates | attempt fails `GPreExhausted`; nothing spawned; LSC reseeds (R8.2) |
| Candidate over `CityGenManifestEntriesMaxCount` or the instance cap | at emission | that candidate fails G-PRE (`ManifestOverflow`); the next candidate is tried |
| LSC Validate or Select fails after G-PRE passed | LSC | attempt fails with LSC ids; geometry torn down (instances, collision, nav) before the next attempt |
| Attempt exceeds 7 s | service watchdog | async work cancelled, state discarded, `Timeout`; the next attempt starts only if ≥ `CityGenAttemptStartMinSec` (4 s) of the 45 s remain, else fallback |
| Two consecutive timeouts | Generator | straight to LSC R8.3 with `HostTooSlow`; parameters are never adapted to the host |
| Deterministic configuration error (kit asset missing, `GeneratorKitVersion` table absent, invalid params) | once, at `Build` entry | `GeneratorConfigInvalid`; straight to R8.3, **no reseeds burned** |
| Client `GeneratorKitVersion` differs | client hash mismatch | LSC R4.7 / Session: resend once, then `ParamsMismatch`; the server never regenerates |

#### R-G11. Determinism, golden seeds and regression
1. **Same manifest everywhere:** every client builds from the same Manifest and must report the same `LayoutHash` (LSC D7); this is the cross-client determinism test. A per-stage hash of the grid state after each stage is logged in non-Shipping builds, so a drift is localised to one stage.
2. **Golden seeds:** `CityGenGoldenSeedCount` (20) seeds are committed with their expected `LayoutHash`, accepted candidate index, attempt outcome and predicate metrics. A golden is eligible only if every ratio predicate passes with ≥ `CityGenGoldenMarginMinPct` (5%) margin, so navmesh tiling noise cannot flip it.
3. **Bump rule:** any change to stage code, kit content, slab or stack tables or collision strategy changes `GeneratorKitVersion` and regenerates the goldens in the same change. A hash change without a bump fails CI as "unbumped drift".
4. **Batch audits** on 50 seeds per Generator change: LSC acceptance (≥ 34/50), LSC batch audits, F3 and F4, art 6.10 tests 4, 5 and 7, a role-prior audit (the top-ranked court by any single public metric is the final in ≤ 30% of seeds; neither axis corner holds more than 60% of drawn finals; review 2026-10-24 replaced the old "temple ring vs rest ≤ 50%" class audit, which corner zones make unsatisfiable because no ring court is final-eligible, S4), a **mirror-intercept audit** (at most `CityGenMirrorAssignMaxPct` (15%) of drawn assignments give the two teams prelim courts that are exact or ±1 c transposes of each other; CD-GDD-ALIGN), and a G-PRE false-pass rate (≤ 20% over 1,000 spawned candidates).
5. Golden seeds are **test data, not content**: they may appear only in scripted tutorials (provisional, Onboarding GDD); practice defaults to fresh seeds, and golden seeds are excluded from live seed draws (Session redraws, Cross-References), because practising on fixed cities would train memorisation (P2).

#### R-G12. The hand-built graybox
1. **First implementation of the contract.** The graybox (LSC R11) is built first from the same kit; it defines the validators' real behaviour before the Generator exists.
2. **Fallback map** after attempts are exhausted (LSC R8.3).
3. **Calibration target:** measure its walkable area, junction counts, W(O) histogram, combo histogram and chain lengths, and compare each 50-seed Generator batch against those distributions. The Generator never copies its layout.
4. **Manifest fixture provider** (non-Shipping): a provider that publishes a committed golden Manifest, so the client instantiation, hash and ack path is tested before the Generator is finished.

### States and Transitions
Per round, inside the LSC service's `Building(k)` state (LSC States table):

| State | Entered when | Exits to | Exit condition |
|---|---|---|---|
| `Idle` | Registered, or after teardown | `Checking` | Service calls `Build(AttemptSeed, Params)` |
| `Checking` | `Build` entry | `Drafting(k, 0)` / `ConfigInvalid` | Kit and params valid / not |
| `Drafting(k, j)` | Check passed, or candidate j-1 failed | `PreValidated(k, j)` / `Drafting(k, j+1)` / `Exhausted(k)` | G-PRE pass / fail with j+1 < J / fail at j = J-1 |
| `PreValidated(k, j)` | G-PRE pass | `Spawning(k)` | Manifest and sockets emitted |
| `Spawning(k)` | — | `NavBuilding(k)` | All instances and collision registered, first trace valid |
| `NavBuilding(k)` | — | `HandedOff(k)` | Server navmesh build complete |
| `HandedOff(k)` | Sockets and Manifest returned to the service | `Idle` (kept) / `TearingDown(k)` | LSC publishes / LSC rejects |
| `TearingDown(k)` | LSC reject, timeout or exhaustion | `Idle` | Instances, collision and nav removed |
| `Exhausted(k)`, `ConfigInvalid` | as above | `Idle` | Failure returned to the service |

Any state may be cancelled by the service watchdog (`Timeout`), which goes to `TearingDown(k)`.

### Interactions with Other Systems
The Generator talks only to the LSC layout service: in, `Build(AttemptSeed, Params)` and cancel; out, the socket set (LSC R3), Manifest entries, decoration rule-set id, failure code and metrics snapshot. Every other system reads the contract's records, never the Generator (data interfaces per system in Dependencies).

## Formulas
*systems-designer consulted (F1-F6), level-designer (F7), technical-artist (F2 counts). The LSC formulas D1-D9 (fairness, reachability, detour, chain band, socket counts, zone fill, layout hash, selection and acceptance) are not restated; the Generator is measured against them. "c" = 2.0 m cell.*

### F1. Grid size and walkable target
The grid_walkable formula is defined as: `N_int = N − 2 · P`; `T = clamp(round(f_walk · N_int²), A_min, A_max)`, with `A_min = LayoutWalkableCellsMinCount` and `A_max = min(LayoutWalkableCellsMaxCount, WalkableCellsPerPawnMaxCount · (NpcCount + 2 · TeamSize))` (LSC V-AREA). A candidate is accepted when `max(A_min, ⌈(1 − tol) · T⌉) ≤ N_walk ≤ min(A_max, ⌊(1 + tol) · T⌋)`.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| N | int | 62-66 (hard 56-76) | data file (`CityGenGridSideMinCount` / `MaxCount`), drawn per candidate | Grid side in cells |
| P | int | 3 | data file (`CityGenPerimeterRowCount`) | Sealed perimeter rows |
| f_walk | float | 0.55-0.70 (default 0.64) | data file (`CityGenWalkableTargetPct`) | Target walkable share of the interior |
| tol | float | 0.03-0.10 (default 0.05) | data file (`CityGenWalkableTolPct`) | Tolerance round the target |
| A_min, A_max | int | 2,000 / 2,250 at defaults | LSC keys, TDC `NpcCount` (40), `TeamSize` (5) | LSC V-AREA band |
| N_walk | int | 2,000-2,250 | calculated | Walkable cells of the candidate |

**Output Range:** at the defaults T is 2,000-2,250 cells (8,000-9,000 m²) for every N in 62-66; at N = 62, T = round(0.64 · 3,136) = 2,007; at N = 66, T = round(0.64 · 3,600) = 2,304 → clamped to 2,250. A larger `NpcCount` raises A_max and lets larger cities through.
**Example:** N = 64: N_int = 58, T = round(0.64 · 3,364) = 2,153, band = [max(2,000, 2,046), min(2,250, 2,260)] = [2,046, 2,250]. The 5 × 5 lattice of 8 c blocks with 3 c corridors (5 · 8 + 6 · 3 = 58) leaves 1,764 corridor cells. Canals turn corridor into water: `N_walk ≈ 1,764 − 3 · L_c + 2 · L_c + 400`, where L_c is the canal length in cells (3 c of water each, quays carved 1 c into both flanking blocks) and 400 is Z-alleys (+~250) plus widened courts and plazas (+~150). At the 12% canal cap, L_c ≈ 84 c gives about 2,080, which passes; at the earlier ~20% share (L_c ≈ 140 c) it gives about 2,024, below the 2,046 floor (and about 1,744 without carved quays). In G0 canals are disabled (L_c = 0, about 2,160). (Review 2026-10-24: the earlier example ignored water; Open Question 7 can still move the canal width.)

### F2. Instance count and Manifest size †
The instance_budget formula is defined as: `N_entries = N_slab + N_bay · k_bay + N_corner + N_roof + N_prop + N_water + N_temple + N_trav + N_bush + N_fixture`; `N_bay ≈ 2 · N_walk / w̄ − E_open`; `Bytes = 8 + 9 · N_entries` (LSC D7); `N_inst = N_entries + N_decor ≤ InstanceCap`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| N_walk | int | 2,000-2,250 | F1 | Walkable cells |
| w̄ | float | 3.0-3.5 | calculated | Mean corridor width in cells |
| E_open | int | 150-250 | calculated | Facade edges lost to junction and court openings |
| N_bay | int | 1,000-1,300 | calculated | Facade bays |
| k_bay | int | 1 with bay stacks; about 2.5 without | constant (R-G6.3) | Instances per bay |
| N_slab | int | 550-800 | calculated | Floor slabs after the greedy cover (2 m alleys fit only 2 × n strips) |
| N_corner, N_roof, N_prop, N_water, N_temple, N_trav, N_bush, N_fixture | int | ~200, ~100, ~130, ~120, ~200, ~40, 8-9, 14-16 | calculated | Other entry classes |
| InstanceCap | int | 5,000 (ASSUMED, art 8.x.2) | data file (`CityGenInstanceCapCount`) | Runtime kit + decoration cap; R1 measures it |
| N_decor | int | 0-`CityGenDecorReserveCount` (1,500) | calculated (client) | Decoration instances |

**Output Range:** with bay stacks about 2,400-2,900 entries, 21.6-26.1 KB raw (0.43-0.52 s of a 50 KB/s link, sent once); without stacks about 4,100-4,800 entries, which exceeds `CityGenManifestEntriesMaxCount` (3,500), so stacks are required unless R1 and the Manifest spike both clear the larger figure.
**Example:** N_walk 2,160, w̄ 3.2, E_open 200 → N_bay = 1,150. Entries = 650 + 1,150 + 200 + 100 + 130 + 120 + 200 + 40 + 9 + 15 = 2,614; Bytes = 8 + 23,526 = 23,534 (23.0 KB raw); decoration 1,500 → N_inst = 4,114 ≤ 5,000. (An earlier ~5 KB estimate is below this; LSC R4.5 says ~13.5 KB for 1,500 entries. See Cross-References request 1a.)

### F3. Facade periodicity excess
The periodicity_excess formula is defined as: `PE(run) = max over τ = 1..τ_max of C_τ / cap_τ`, where `C_τ` = the longest stretch of bays with `s_i = s_{i+τ}`, plus τ; `cap_1 = RunCap`, `cap_τ = PairCap · τ` for τ ≥ 2. A run passes when `PE ≤ 1`. The same score is applied to the door-gap sequence with `cap_1 = 2`.
Batch audit: pooled conditional entropy `H(s_{i+1} | s_i) ≥ EntropyFloor · log2(K)` over at least 5,000 transitions.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| s_i | int | 0..K-1 | calculated | Visible bay type at position i (stack or module + variant) |
| K | int | 2-12 | calculated | Bay types the recipe allows on this run |
| τ_max | int | 4 | constant | Longest period tested |
| RunCap | int | 4-6 (default 6) | data file (`CityGenFacadeIdenticalRunMaxCount`) | Art 6.7: identical runs ≤ 6 bays (12 m) |
| PairCap | int | 2-3 (default 3) | data file (`CityGenFacadePeriodRepeatMaxCount`) | Art 6.7: fewer than 4 repeats of a pattern |
| EntropyFloor | float | 0.45-0.65 (default 0.55) | data file (`CityGenEntropyRatioMinPct`) | Batch-only: entropy is too noisy per run |
| PE | float | 0 and up | calculated | Gate score |

**Output Range:** PE ≥ 0; the gate rejects PE > 1 (the sequencer re-draws the offending bay). The score generalises "no 4 repeats of an A-B pair" to any period up to 4 without penalising irregular rhythm.
**Example:** `ABABAB` → C_2 = 4 + 2 = 6, PE = 6/6 = 1.0, passes; `ABABABAB` → C_2 = 8, PE = 1.33, fails; `AABAABAAB` → C_3 = 9, PE = 9/9 = 1.0, passes; `AAAAAAA` → C_1 = 7, PE = 7/6, fails.

### F4. Layout diversity (batch)
The layout_diversity formula is defined as: `J*(a, b) = max over g ∈ G of |M_a ∩ g(M_b)| / |M_a ∪ g(M_b)|` on walkable masks centred and cropped to the smaller grid; the batch passes when the 95th percentile of J* over all pairs is at most `JaccardMax`.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| M_a, M_b | cell set | 2,000-2,250 cells | calculated | Walkable masks of two accepted layouts |
| G | set | 8 symmetries (square grids) | constant | Rotations and reflections |
| pairs | int | 1,225 for 50 seeds | calculated | All unordered pairs |
| JaccardMax | float | 0.50-0.75 (default 0.60, provisional) | data file (`CityGenJaccardP95MaxPct`) | Set at baseline + 3σ from a 200-seed simulation |

**Output Range:** 0-1. Two independent random 53% masks give about 0.36; a shared lattice pitch raises the baseline, which is why the threshold is calibrated, not fixed.
**Example:** a batch whose p95 J* is 0.52 passes; a generator change that collapses the jitter so every city shares one skeleton shows p95 ≈ 0.9 and fails.

### F5. Attempt time budget †
The attempt_time formula is defined as: `T_attempt = j_used · t_cand + t_spawn + t_nav + t_val + t_sel ≤ LayoutAttemptTimeoutSec`; `T_total = Σ_k T_attempt(k) + t_svc ≤ LayoutTotalBudgetSec`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| t_cand | float (s) | ≤ 0.12 | measured | S1-S12 + G-PRE for one candidate, off the game thread |
| j_used | int | 1-8 | calculated | Candidates drawn in the attempt |
| t_spawn | float (s) | ≤ 1.0 | measured † (R4) | Instances and collision on the server |
| t_nav | float (s) | ≤ 3.0 − t_spawn | ASSUMED (art 8.x.10, R4: spawn + nav build ≤ 3 s combined) † | Server navmesh build; `t_spawn + t_nav ≤ 3.0` is the single budget (R-G9, AC-G33) |
| t_val, t_sel | float (s) | ≤ 1.5 / ≤ 0.3 | measured (LSC) | LSC Validate and Select |
| t_svc | float (s) | ≤ 3.0 | measured | Freeze, hash, overhead |
| LayoutAttemptTimeoutSec / LayoutTotalBudgetSec | float (s) | 7 / 45 | LSC keys | Contract bounds (6 × 7 = 42 ≤ 45) |

**Output Range:** worst case 8 · 0.12 + 3.0 (spawn + nav, R4) + 1.5 + 0.3 = 5.76 s ≤ 7 s (1.24 s slack). If spike R4 reports only t_nav ≤ 3.0 with spawn on top (art 8.x.10's looser reading), the worst case is 6.76 s (0.24 s slack) and the caps J and t_cand become hard limits; the R4 report decides which reading holds. Expected about 4.1 s (j_used ≈ 1/p_g ≈ 3.3). (Review 2026-10-24: the earlier text summed a separate 1.0 s spawn and 3.0 s nav, which contradicted R-G9 and AC-G33.)
**Example:** p_g = 0.30 → j_used ≈ 3.3 → 0.40 + 1.0 + 2.0 (measured nav) + 0.6 + 0.1 = 4.1 s.

### F6. Acceptance and reseed rate
The acceptance formula is defined as: `p = (1 − (1 − p_g)^J) · p_v`; `P_fallback = (1 − p)^(R + 1)`; `E_attempts = (1 − (1 − p)^(R + 1)) / p`; `E[T_layout] ≈ E_attempts · E[T_attempt]`

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| p_g | float | 0-1 (design ≥ 0.30) | measured per candidate | G-PRE pass rate |
| J | int | 1-16 (default 8) | data file (`CityGenCandidateDrawsCount`) | Candidates per attempt |
| p_v | float | 0-1 (design ≥ 0.85) | measured | LSC pass rate given a G-PRE pass; the guard band raises it |
| R | int | 5 | LSC `LayoutMaxReseedAttemptsCount` | Reseeds |
| p | float | 0-1 | calculated | Per-attempt acceptance; LSC D8 requires ≥ 34/50 in the batch |

**Output Range:** the LSC floor p ≈ 0.565 gives P_fallback 0.68%. **Design target p ≥ 0.78**, because a 50-seed gate at true p = 0.71 passes only about 72% of the time, while at p = 0.80 it passes about 99%.
**Example:**

| p_g | J | p_v | p | P_fallback | E_attempts |
|---|---|---|---|---|---|
| 0.20 | 8 | 0.85 | 0.707 | 0.063% | 1.41 |
| 0.30 | 8 | 0.85 | **0.801** | **0.006%** | 1.25 |
| 0.35 | 8 | 0.85 | 0.823 | 0.003% | 1.21 |

At p = 0.801 and E[T_attempt] = 4.1 s, E[T_layout] ≈ 5.1 s.

### F7. Skeleton fairness skew
The skeleton_skew formula is defined as: `R_skel = max(C̃_0, C̃_1) / min(C̃_0, C̃_1) ≤ SkelMax`, where `C̃_T` is the D1 chain length computed on the skeleton's node graph (junction-to-junction lengths) for the best-balanced court triple; worst-case jitter skew `ΔC ≤ n_cut · δ · 2 m`.

**Variables:**
| Variable | Type | Range | Source | Description |
|----------|------|-------|--------|-------------|
| n_cut | int | 5-10 | calculated | Corridor cuts a chain crosses (LSC D4 band 120-240 m, target 170 m) |
| δ | int (cells) | 0-1 | data file (`CityGenSkeletonJitterCount`) | Per-half cut jitter |
| SkelMax | float | 1.03-1.08 (default 1.06) | data file (`CityGenSkeletonFairnessMaxMult`) | Pre-check, leaving room under LSC's 1.10 for fill differences |

**Output Range:** ΔC ≤ 16 m at n_cut 8, δ 1 in the worst case: 9.4% of the 170 m D4 target and 13% of a 120 m chain, so the jitter alone can approach or exceed LSC's 1.10 on short chains. Typical skew is far lower because jitters cancel, and SkelMax (1.06) is what bounds it: skeletons above SkelMax are rejected before any fill. The 50-seed batch reports the R_skel histogram per chain-length tercile; if short chains dominate rejections, lower δ to 0 on cuts crossed by chains under 150 m before touching SkelMax. (Review 2026-10-24: earlier text measured against a 220 m chain, superseded by D4.)
**Example:** C̃_0 = 214 m, C̃_1 = 224 m → R_skel = 1.047 ≤ 1.06, kept; 205 / 222 → 1.083, rejected (cheap: no facades or sockets were built).

## Edge Cases
*systems-designer (rule and formula space) and level-designer (spatial cases) consulted. Cases the LSC already resolves (hash mismatch, host leave, late join, packet loss on the Manifest, fallback exhaustion) are not repeated; see LSC Edge Cases.*

**Budget and failure**
- **If the 7 s attempt timeout fires mid-attempt:** async work is cancelled, partial instances, collision and nav are torn down, the attempt fails `Timeout`. The next attempt starts only if at least `CityGenAttemptStartMinSec` (4 s) of the 45 s remain; otherwise the service goes straight to fallback (LSC R8.3).
- **If two attempts in a row time out:** the host is too slow; straight to fallback with `HostTooSlow`. Parameters (J, N, budgets) are never adapted to the host, because that would make the city depend on hardware.
- **If all J candidates fail G-PRE:** the attempt fails `GPreExhausted` in about 1 s with nothing spawned; the least-bad candidate is never published.
- **If a configuration error is deterministic** (kit asset missing, version table absent, a `CityGen*` key outside its hard range that TDC failed to catch): `GeneratorConfigInvalid` at `Build` entry; fallback immediately, no reseeds burned (they would all fail the same way).
- **If a candidate exceeds `CityGenManifestEntriesMaxCount` or the instance cap:** that candidate fails G-PRE (`ManifestOverflow`). If more than 5% of a 50-seed batch's candidates overflow, the batch audit fails and the kit or slab tables must change.
- **If the server navmesh build reports failure or leaves an unbuilt tile:** the attempt fails `NavBuildFailed`; LSC V-NAV-1 would reject it anyway.
- **If G-PRE passes but LSC rejects more than 15% of spawned candidates in a batch** (p_v < 0.85): the guard band is too thin; raise `CityGenGPreGuardBandPct` or fix the predicate backend drift (R-G5.2). Both are batch-time actions, never runtime ones.

**Determinism and content**
- **If a golden seed's hash changes without a `GeneratorKitVersion` bump:** CI fails as unbumped drift; the per-stage hashes name the first stage that changed.
- **If two clients' decoration differs** (an iteration-order bug): gameplay is unaffected (decoration has no collision and is ≤ 0.5 m); non-Shipping tests compare `DecorHash` and fail; Shipping does nothing at runtime.
- **If a decoration element is taller than 0.5 m or has collision:** it is dropped with an `ensure`; it never becomes a hidden sight blocker.
- **If the kit has only one variant per module** (mass-production gate, art 8.x.14): the sequencer still works; variety comes from door rhythm, storey presence and layout only. F3 is still enforced; the occurrence rule is trivially met.
- **If a canal would isolate cells** (an island between canal and perimeter): S11.2 fills the island as building, or a crossing is forced every 12 c; a canal never meets the outer skirt without a sealed end.
- **If a court lands within 30 m path of a zone anchor:** it stays a court, but the LSC distance predicates make it ineligible as that zone's P1; no extra forbid rule.
- **If the temple tier lies within 15 m of a candidate zone anchor:** the anchor and its mirror slide one block along the perimeter lane together (symmetry is kept); if no slide passes, the candidate fails.
- **If the mirror pair of a zone would differ in area type** (asymmetric fill): area types are assigned on the skeleton before fill, so zone parity (V-ZONE) holds by construction; fill may change only bays, props, bushes and alleys within a half.

**Network, host and players**
- **If the listen-server host is also rendering the briefing screen:** generation runs off the game thread; spawning is sliced by `LayoutSliceWorkUnitsMax`; the host's frame stays ≤ 33 ms like every client's. (Host leave and late join during generation: LSC Edge Cases; a joining client only ever receives a published Manifest.)
- **If a modified client runs the Generator on the public seed:** it previews the geometry (the seed is public by design) and learns no role, zone mapping or spawn order (R-G2.3).
- **If players learn generator tendencies** (for example "the anti-diagonal corners are the zones" or "the final is in an axis corner"): corner zones are public and are **not** harmless: with the sun fixed on the mirror axis, the enemy always starts on the far side of that line, so "a figure arriving from the far side early" is a structural read; and with corner zones the final is always in one of the two axis corners (S4; no temple-ring court can be final-eligible). MVP accepts both under V-SEP (no contact for ≥ 15 s), splits the final pool between two far-apart corners (≥ 6 candidates, more than one team can camp) and measures both (Open Question 5, with decision rules to make 4-zone mode the default). Roles are bounded by LSC `RoleShareMaxPct`, prefix ambiguity and the R-G11.4 audits (single-metric final prior ≤ 30%, axis-corner share ≤ 60%, mirror assignments ≤ 15%).
- **Pillar 1 leak by geometry:** no module, prop, decal or light is chosen because of a socket's role; S9 bushes and S10 props read no role data; the LSC VA-1 human tests run on generated layouts too (AC-G19).

## Dependencies
The index row 16 lists five upstream dependencies (Level Socket Contract, Round & Match Framework, Tuning Data & Config, Session & Travel (soft), Shared Character & Locomotion (soft); verified 2026-10-24, cross-review C-W11) and no dependents: the Generator is a leaf, reached only through the LSC layout service. Every consumer of its output (Crowd, Concealment, Objectives, Shared Character, Round) reads the *contract's* records, never the Generator.

### Upstream (the Generator depends on)
| System | Data interface | Hard/Soft |
|---|---|---|
| Level Socket Contract (#8) | Provider interface (register as `Generated`; `Build(PublicSubSeed, Params) → {Sockets, Manifest, DecorationRules}`); every R3 socket schema, every R7 predicate and threshold; D5 counts; D7 hash serialisation; R8 attempt counter; `GeneratorKitVersion` and `LayoutParamsHash` definitions (R1). The Generator calls no Round, Session or Selection API directly. | Hard |
| Tuning Data & Config (#1) | The frozen server snapshot at `RequestLayout`: every `CityGen*` key (Tuning Knobs) plus the LSC keys it builds toward, `TeamSize`, `NpcCount`. `TuningHash` enters `LayoutParamsHash`. | Hard |
| Session & Travel (#5) | `PublicSubSeed(Layout, Attempt)` (F5) is the **only** random input. The Generator never calls `GetServerSubSeed` (server assert). Soft because the service passes the value in; the Generator does not call Session. | Soft (value passed by the service) |
| Round & Match Framework (#6) | Indirect only: Round calls the service; the Generator never references Round, `RoundIndex` or `FRoundParams` (Round GDD Dependencies row 16). The index row is kept as an ordering dependency (Preparing must exist). | Soft (ordering) |
| Shared Character & Locomotion (#9, **provisional pending traversal spike**) | Capsule radius and height, max step height and walkable slope (for nav agent settings and stair/kerb checks); walk 140 / run 450 cm/s (LSC D4/D6); traversal capability heights (= LSC `TraversalVaultHeightCm` / `TraversalClimbHeightCm`). | Soft (values only) |
| Art bible kit (`design/art/art-bible.md` 6.2-6.8, 8.x) | Not a system: the content dependency. Module dimensions, variants, district recipes, repetition rules, budgets. Kit content requests are in Visual/Audio. | Hard (content) |

### Downstream (these depend on the Generator)
| Dependent | What it takes | Hard/Soft |
|---|---|---|
| LSC layout service (#8) | A `Generated` provider; must receive a layout that passes the predicates at the D8 acceptance target (≥ 34/50 seeds). | Hard (in mode `Generated` / `GeneratedWithFallback`; none in the MVP default `Graybox`) |
| Telemetry & Analytics (#26, provisional) | Candidate index, stage timings, G-PRE failure ids, instance and entry counts per attempt (inside `FLayoutAttemptRecord.MetricsSnapshot`). | Soft |
| Anti-Cheat & Info-Leak Security (#27, provisional) | The guarantee that the Generator reads only public inputs and emits only geometry (R-G2), so a client running it learns the city and nothing else (Session rule 14). | Soft |
| Onboarding & Practice Mode (#25, provisional) | Scripted tutorials only may use golden seeds (R-G11.5); practice uses fresh seeds and never the golden set. | Soft |

**Bidirectionality note:** LSC already lists the Generator as a Hard dependent (its Dependencies row 16); TDC lists it as Hard with "size limits, retry budget, own keys (provisional)" and must register the `CityGen*` keys; Session lists it as Soft; Round lists it as indirect. Telemetry, Anti-Cheat and Onboarding are not yet written and must list the Generator as a soft upstream (Cross-References).

## Tuning Knobs
Registered under Tuning Data & Config CR-15; owner Procedural City Generator for every row. **Audience** `ServerOnly` for every key (clients receive geometry, never thresholds). **Latch** `Round` (frozen at `Preparing`, applied to the next layout). Every key enters `LayoutParamsHash`, so changing one changes `LayoutId` and requires regenerated goldens. Defaults are provisional until the first 50-seed batch; † pending R1/R4. Keys the Generator only *reads* (all `Layout*`, `Zone*`, `Bush*`, `Emergence*`, `Sun*` contract keys, `NpcCount`, `TeamSize`) are owned by the LSC and TDC and not duplicated here.

| Key | Type | Default | Hard range | Safe range | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|
| `CityGenGridSideMinCount` / `MaxCount` | int | 62 / 66 | 56-76 | 60-68 | Count | High: T clamps at A_max, building share grows, nav time. Low: courts cannot keep 24 m spacing |
| `CityGenPerimeterRowCount` | int | 3 | 2-4 | 3 | Count | Low: perimeter facades too thin to seal. High: wastes area |
| `CityGenWalkableTargetPct` | float | 64 | 55-70 | 60-66 | Pct | Of the interior. High: few facades, wide streets, thin crowd. Low: V-AREA fails |
| `CityGenWalkableTolPct` | float | 5 | 3-10 | 4-6 | Pct | High: density variance between rounds. Low: p_g falls |
| `CityGenBlockSizeMinCount` / `MaxCount` | int | 7 / 10 | 6-12 | 7-10 | Count | High: walkable band missed, long facades. Low: maze, detour p95 rises |
| `CityGenSkeletonJitterCount` | int | 1 | 0-2 | 1 | Count | High: fairness skew (F7). 0: halves become exact mirrors (more obvious) |
| `CityGenSkeletonFairnessMaxMult` | float | 1.06 | 1.03-1.08 | 1.05-1.07 | Mult | High: p_v falls (LSC 1.10 fails later). Low: p_g falls |
| `CityGenRouteNodeSpacingCount` | int | 3 | 2-5 | 2-3 | Count | Max cells between CrowdRoute nodes along an edge (S12). High: R6.1 NPC pool and start-cell choice shrink. Low: larger route graph, more Crowd path queries |
| `CityGenRunMaxCount` | int | 12 | 8-14 | 10-12 | Count | Straight corridor run before a jog. High: V-SIGHT fails. Low: maze |
| `CityGenCanalShareMaxPct` | float | 12 | 0-20 | 8-12 | Pct | Canal share of corridor length (S2, F1). High: water pushes N_walk below the V-AREA band. Low: fewer quays, vault and bridge variety |
| `CityGenFinalEligibleTargetCount` | int | 6 | 3-8 | 4-6 | Count | Final-eligible courts G-PRE requires (S4; never below LSC `FinalCandidatesMinCount`). 6 > `TeamSize` so a full team cannot camp every candidate (Objectives SP-4, OQ 9). High: p_g falls. Low: blanket camping possible |
| `CityGenGPrePathGuardPct` | float | 8 | 0-15 | 6-10 | Pct | Guard on absolute path floors and band edges in G-PRE (R-G5.2). High: p_g falls. Low: p_v falls (LSC rejects after spawn) |
| `CityGenZoneCandidatesCount` | int | 2 | 2 or 4 | 2 | Count | 4 adds the main-diagonal pair (more uncertainty, lower p) |
| `CityGenCourtTargetCount` | int | 15 | 14-18 | 14-16 | Count | High: courts crowd (24 m spacing). Low: LSC N_C and role counts fail |
| `CityGenWitnessCellsTargetCount` | int | 30 | 20-37 | 26-32 | Count | Generator margin under LSC's 37. High: LSC rejects. Low: alleys unbuildable |
| `CityGenSunPoolRepairMaxCount` | int | 20 | 0-40 | 10-20 | Count | Art 6.8 host regeneration tries. High: t_cand. Low: p_g |
| `CityGenCandidateDrawsCount` | int | 8 | 1-16 | 6-12 | Count | High: t_cand over budget (F5). Low: more reseeds (F6) |
| `CityGenGPreGuardBandPct` | float | 4 | 0-10 | 2-6 | Pct | High: p_g falls, survivor bias. Low: p_v falls |
| `CityGenAttemptStartMinSec` | float | 4 | 3-7 | 4-5 | Sec | High: budget unused. Low: doomed attempts |
| `CityGenTimeoutAbortCount` | int | 2 | 1-6 | 2-3 | Count | Consecutive timeouts before `HostTooSlow`. Low: transient stalls cause fallback |
| `CityGenManifestEntriesMaxCount` † | int | 3,500 | 2,000-4,000 | 3,300-3,700 | Count | High: bandwidth and decoration reserve. Low: overflow rate |
| `CityGenInstanceCapCount` † | int | 5,000 | 3,000-7,500 | 4,000-5,000 | Count | R1 sets it. High: Lumen / Chaos cost. Low: overflow, bare city |
| `CityGenDecorReserveCount` | int | 1,500 | 500-2,000 | 1,000-1,500 | Count | High: eats the instance cap. Low: bare ground |
| `CityGenFacadeIdenticalRunMaxCount` | int | 6 | 4-6 | 6 | Count | Art 6.7 cap; above 6 is forbidden. Low: p_g falls |
| `CityGenFacadePeriodRepeatMaxCount` | int | 3 | 2-3 | 3 | Count | Art 6.7 cap. Low: p_g falls |
| `CityGenEntropyRatioMinPct` | float | 55 | 45-65 | 50-60 | Pct | Batch audit. High: noisy facades. Low: monotony passes |
| `CityGenJaccardP95MaxPct` | float | 60 (provisional) | 50-75 | 55-65 | Pct | Batch audit. High: near-copy cities pass. Low: false batch failures |
| `CityGenMirrorAssignMaxPct` | float | 15 | 5-30 | 10-20 | Pct | Batch audit (R-G11.4). High: "go to my mirror point" becomes a hunting strategy. Low: false batch failures. Since reconciliation 2026-10-24 LSC R9.3 also rejects mirror-intercept combos per layout, using the skeleton symmetry transform the Generator hands to the layout service (server-only, never in the Manifest); this audit stays as the batch check |
| `CityGenGoldenSeedCount` / `CityGenGoldenMarginMinPct` | int / float | 20 / 5 | 10-50 / 3-10 | — | Count / Pct | Regression set size and margin |

**Interactions:** `CityGenCandidateDrawsCount`, `CityGenGPreGuardBandPct` and `CityGenSkeletonFairnessMaxMult` jointly set p (F6) and t_cand (F5): raising J buys acceptance with time. `CityGenWalkableTargetPct` and the grid side interact with LSC V-AREA through `NpcCount`: raising `NpcCount` raises A_max (F1) and lets the Generator build a larger city. `CityGenManifestEntriesMaxCount`, `CityGenInstanceCapCount` and `CityGenDecorReserveCount` share one budget (F2).

## Visual/Audio Requirements
*technical-artist consulted; art-director viewpoint applied inline (budget). The art bible is the authority; LSC VA-1 to VA-7 (no marker, kit per socket, sun pool, bush silhouette, emergence, audio) apply unchanged to generated layouts. This section adds what is specific to assembly.*

- **VG-1 Seams (art 6.7).** World-aligned kit materials (axis from the world normal; never object-aligned, because yaw would break continuity); course and quoin lines align across every adjacency, including the half-block at a running-bond bay end; modules abut with zero gap, closed end caps, walls start at Z = 0 below the floor top (no Lumen light leaks); no negative scale or mirroring (determinant > 0). **Corner cornice collision:** M04 cornices meet at M05 corners, T-junctions and wall ends and z-fight; M05 needs its own mitred cornice (art-bible change request).
- **VG-2 Slabs and stacks are invisible.** A seed rendered with slabs and stacks and again with per-cell modules differs by at most 1 L* mean luminance with no structure at joints (AC-G17). Slabs keep 0.2 m thickness (Lumen minimum); an 8 × 8 m slab's distance field resolution is checked in R1.
- **VG-3 No repetition tell.** F3 per run; the art 6.10 pattern audit (test 4) and sightline audit (test 5) on 50 seeds; an autocorrelation of a facade strip and a top-down floor capture shows no secondary peak at lags of 4-20 m (threshold 0.5, provisional).
- **VG-4 Lighting.** No static lighting; everything exists before the first lit frame (VSM page cache, Lumen surface cache settle behind the briefing screen, AC-G16); the sun is static for the round.
- **VG-5 Audio.** The Generator places **no** sound emitter. Area ambience follows area type (LSC VA-7); no emitter is keyed to a socket, court or zone.

**Kit content required (MVP)**

| Tier | When | Content | What the Generator can build |
|---|---|---|---|
| **G0 (gate)** | Until spikes R1-R3 lift the mass-production gate (art 8.x.14) | The 8 graybox modules at one variant each: M01 (blank; vault and climb variants are traversal pieces and need producer approval), M02, M03, M04, M05, M06, M09 civic, M10 (edge + water). The 4 derived bay stacks (M01+M04, M02+M04, M01+M03+M04, M02+M03+M04) and the slab sizes, baked by script. **Primitive placeholders** (producer to allow): planter + bush silhouette (real outline per LSC VA-5), court fixture, 2-3 ground props | Residential and stall-less Market only; canals disabled (no M11 bridges, `Causeway` blocks become Residential); the temple precinct is a stand-in platform of M01 climb + M09 civic + a sealed mass; variety from rhythm only. Enough for every spike, determinism test and batch audit. **G0 data is non-representative for pillar validation**: P4 objective-ending rate, AC-G21 and read tests count only on G1 layouts |
| **G1 (MVP Generator)** | After the gate | G0 plus M07, M08, M09 temple, M11 deck and bridge, M12 awning + counter, M01 frieze variant and a second M03 window variant (up to 6 stacks), the real planter + bush family (art 3.6), the court fixture family (≥ 3 decor copies per site), prop families ≥ 3 per area type (art 6.4), ground-class decoration set | The full pipeline R-G4 |

> **Asset Spec** — run `/asset-spec system:procedural-city-generator` after the art bible pass for the stacks, slab sizes and the mitred M05 cornice.

## Game Feel
The Generator has no input or moment-to-moment feel of its own (required for `Gameplay` systems, so stated explicitly). Its feel targets are what the player must **not** feel; each is measurable:

| Target | Measure | Bound |
|---|---|---|
| **No wait you notice** | Server time from `RequestLayout` to `OnLayoutReady` (one accepted attempt) | median ≤ 6 s, p95 ≤ 10 s † (F5); never above `LayoutTotalBudgetSec` (45 s), after which the contract falls back |
| **No hitch** | Longest game-thread frame on the listen-server host and on clients during `Preparing` | ≤ 33 ms (LSC R4.5); generation stages run off the game thread on one worker task (ADR-0014 §9.1) |
| **No first-frame pop** | Geometry, decoration and lighting complete before the briefing screen lifts | 0 missing or late-streamed kit instances in the first `Active` frame (AC-G16) |
| **No déjà vu** | Two rounds of one match never share a layout; batch layouts are not near-copies | `UsedSeeds` (Session F5) plus F4 |
| **No seams** | Module joins, texture continuity, rhythm | Art bible 6.7 seams rules and the human tests in Visual/Audio |
| **No tilted map** | Inherited from the LSC fairness predicates | the LSC fairness playtest criterion (ObjectiveWin per zone), re-run on generated layouts (AC-G21) |

## UI Requirements
The Generator has **no player-facing UI**. Generation runs behind Round's single neutral briefing screen (Round & Match Visual/Audio; art bible 2.3). That screen shows no progress bar, attempt count or "generating city" text, because a host-only or per-attempt difference would be a timing tell (Session rule 11.8), and a visible reseed count would advertise generator weakness.

Developer UI only, compiled out of Shipping (extends the LSC developer UI): `stealth.citygen.run <Seed> [Attempt]` builds a layout in a test world and prints the candidate index, stage timings, G-PRE metrics and `LayoutHash`; `stealth.citygen.batch <N> <FirstSeed>` runs the batch audits (F3, F4, F6, art 6.10 tests 4, 5 and 7) and writes a CSV to `Saved/CityGen/`; the overlay `stealth.citygen.debug` (default off) colours cells by area type, block, corridor class, repair pass and G-PRE failure, reusing the LSC socket overlay. No UX spec is needed. (No UX flag.)

## Cross-References
| This GDD references | Owner document | What is used | Status |
|---|---|---|---|
| Locked rules: server seed, identical layout and objective placement on every client, invalid layouts rejected before the round | design/gdd/game-concept.md; production/session-state/active.md | R-G2, R-G10 | Locked |
| Socket vocabulary, R2 Manifest, R4 handshake and budgets, R7 predicates, R8 reseed/fallback, R9 Selection, R11 graybox, D1-D9, VA-1-VA-7 | design/gdd/level-socket-contract.md (MAJOR REVISION NEEDED 2026-10-24, 26 revisions applied, re-review pending) | Everything the Generator aims at; re-aligned 2026-10-24 to the revised R3 (StartCell 18 m, RespawnRegion), R5b, R6.1 pool, V-SEP, V-ZONEFILL, V-RESPAWN, D4 120-240 m, D6, D6b, D8 34/50 | Reviewed (MAJOR REVISION NEEDED; revised): re-check on its re-review |
| Change requests addressed to the Generator | crowd-npc-behaviour.md CR-17/18 (via LSC: route coverage; no navmesh rebuild after `OnLayoutReady`), npc-kill-penalty-informants.md CR-13 (no single-pawn chokepoint at fixtures or objective paths), objectives-chain.md CR-8 (4 fixture sides, `ClueTag`, final-eligible ≥ 3 ingresses / no bush within 12 m, `FinalCandidatesMinCount` > `TeamSize`) | S4, S9, S12, R-G7 (see "Requests received" below) | Applied in this GDD 2026-10-24 (LSC-owned parts remain LSC change requests) |
| `PublicSubSeed`, `Mix32`, rule 14, F7 Preparing bound | design/gdd/session-travel.md | R-G2, R-G3 | Written |
| Preparing, `OnLayoutReady`, briefing screen | design/gdd/round-match-framework.md | UI Requirements, R-G1 | Written |
| CR-15 key convention, OQ-7 (`TargetRoundMinutes`) | design/gdd/tuning-data-config.md | Tuning Knobs, R-G2.2 | Written |
| Capsule, step, slope, speeds, traversal heights | design/gdd/shared-character-locomotion.md | R-G7, S8 | **Provisional pending traversal spike** |
| Kit 6.2, seams and repetition 6.7, sites 6.8, budgets 8.x.2, assembly 8.x.10, spikes 8.x.14 | design/art/art-bible.md | R-G4, R-G6, R-G9, Visual/Audio | Written |
| Runtime nav generation, 5.8 per-mesh nav areas | docs/engine-reference/unreal/modules/navigation.md | R-G7 | Reference (5.8 block verified 2026-10-17); exact API UNVERIFIED |

**Cross-GDD change requests** (for the orchestrator; not applied here):

1. **Level Socket Contract:** (a) state D7's `VariantId` and `ModuleId` ranges so they cover derived bay stacks and slab sizes (as distinct `ModuleId`s), and raise D7's "about 3,000" entry range to `CityGenManifestEntriesMaxCount` (3,500); R4.5's "about 1,500 entries / 13.5 KB" becomes "1,500-3,500 entries / 13.5-31.5 KB raw" for generated layouts; (b) confirm V-SIGHT measures sightlines in **every** direction (diagonals across plazas and junctions), which the Generator assumes; (c) confirm that a third and fourth candidate zone may fail V-SEP with each other (adjacent corners) as long as each is in a usable pair (V-SEP text already says "per zone pair Selection may use"); (d) the 4-zone mode needs Selection to draw the zone pair uniformly among usable pairs (R9.2 "the 2 used zones from the usable zone pairs" implies it; make it explicit); (e) **mirror intercept (CD-GDD-ALIGN):** consider a Selection pass condition that rejects combos whose two teams' prelim courts are exact or ±1 c transposes of each other, so the Generator's symmetric skeleton never yields "go to the reflection of my position" (the Generator's batch audit caps it at 15% meanwhile).
2. **Session & Travel F5:** the `Attempt` range names `GeneratorMaxAttempts`; the key is LSC `LayoutMaxReseedAttemptsCount`. Rename for consistency. Also redraw `Seed` when it is in the Generator's golden set (as for `UsedSeeds`), so live rounds never use a test city.
3. **Tuning Data & Config:** register the `CityGen*` keys (Tuning Knobs) with owner Generator, audience `ServerOnly`, latch `Round`; add cross-field invariants `CityGenGridSideMinCount ≤ MaxCount`, `CityGenWitnessCellsTargetCount ≤ EmergenceWitnessCellsMaxCount`, `CityGenSkeletonFairnessMaxMult < ChainFairnessMaxMult`, `CityGenFacadeIdenticalRunMaxCount ≤ 6`; answer OQ-7: the Generator does not use `TargetRoundMinutes`.
4. **Shared Character & Locomotion:** supply capsule radius, max step height (the kit needs ≥ 0.23 m risers to pass and the 0.4 m kerb to block) and walkable slope (temple stair 40°, only 4° under the default); confirm that a pawn cannot vault the 0.4 m canal kerb (it carries no coping). *Answered by SCL at reconciliation 2026-10-24:* the step height is 45 cm, so a 0.4 m kerb is stepped, not blocked (a blocking kerb would have to be 46-69 cm); the Generator keeps the 0.4 m kerb and relies on its R-G7 pawn-only water blocking volume (AC-G15 corrected); walkable floor angle stays the engine default 44.765°, so the 40° temple stair passes.
5. **Art bible:** M01 lists 4 shape variants against the 3-variant budget (6.2); add an M05 mitred-cornice piece (VG-1); record bay stacks and slab sizes as derived assets, not new variants; reconcile 6.8's shadow-trace check (0.4 m raster, ≥ 36 m² pool, 5:1 ratio, 20 regenerations) with LSC V-POOL (0.5 m grid, ≥ 30 m², ± 20% parity): the Generator runs the LSC predicate as the gate and the art 6.8 check as its S11.3 repair target.
6. **Producer (mass-production gate):** confirm that baked bay stacks and slab sizes are derived assets allowed under the gate, and allow primitive placeholders for the bush, court fixture and 2-3 props in tier G0.
7. **Telemetry & Analytics, Anti-Cheat & Info-Leak Security, Onboarding & Practice Mode (not yet written):** list the Generator as a soft upstream (Dependencies).
8. **Systems index row 16:** Depends On may add Session & Travel (soft) and Shared Character & Locomotion (soft, values); Round stays as an ordering dependency.
9. **Round & Match Framework** (review 2026-10-24): its Dependencies row for #16 says "the Generator may read `RoundIndex` and `Seed` via `FRoundParams`" and "Not yet written". Correct it to: the Generator reads neither; the layout service passes `PublicSubSeed(Layout, LayoutAttempt)` (R-G2.2 forbids `RoundIndex`, AC-G2), status Designed.
10. **Session & Travel** (review 2026-10-24): close Open Question 6: clients run no generator; they build from the replicated `FLayoutManifest` with no RNG and verify `LayoutHash` (R-G7, LSC R2.2). Session's rule 14 and the single ack are unchanged. The `?Seed=` URL option is parsed only by Session; the Generator never reads URL options.
11. **Level Socket Contract, from received requests** (review 2026-10-24): (a) if Objectives CR-8's 12 m final-eligible bush exclusion is adopted, make it a single **role-blind** `BushCourtExclusionCm` for every court (S9 never reads a court's eligibility; a per-role exclusion would make bush-free courts a final tell); (b) if `FinalCandidatesMinCount` is raised above `TeamSize` (Objectives OQ 9), the Generator already pre-checks `CityGenFinalEligibleTargetCount` (S4) and only its default moves; (c) `ClueTag` multiplicity (≤ 3 courts per clue) should be a V-* predicate so G-PRE can pre-check it; (d) state Penalty CR-13 as a V-NAV-2 clause (every court ingress and every bridge or stair on a route ≥ `NavMinClearWidthCm`, every fixture with ≥ 1.2 m clear floor on all four faces).

**Requests received and how they are met here** (review 2026-10-24):

| From | Request | Met by |
|---|---|---|
| Crowd CR-17 (to LSC) | `CrowdRoute` covers every legal player position within 10 m | S12 route edges and node spacing; AC-G37 |
| Crowd CR-18 | No runtime navmesh rebuild after `OnLayoutReady` | R-G7 navmesh frozen after hand-off; AC-G36 |
| Crowd CR-8 (to LSC) | Touchable-prop records | S12 emits them from Manifest props and walls if LSC adopts the record (LSC reconciliation 2026-10-24: PENDING-USER, schema change) |
| Penalty CR-13 (to Objectives and LSC) | No fixture or objective path closable by one standing pawn | S4 ingress and fixture clearance rule; corridors ≥ 2.8 m clear (V-NAV-2) everywhere; AC-G38 |
| Objectives CR-8 (to LSC) | 4 fixture interact sides; final-eligible courts ≥ 3 ingresses, no bush within 12 m; `ClueTag` ≤ 3 courts; `FinalCandidatesMinCount` > `TeamSize` | S4 (junction courts have 3 ingresses; fixture clear on 4 faces; final-eligible target 6); S9 via request 11a; `ClueTag` via request 11c |
| Concealment | No request addressed to the Generator; it consumes `BushSite` from the Manifest (planter 2.0 × 2.0 m, 0.35 m curb, 1-3 entry sides) | S9 emits LSC `BushSite` exactly; V-BUSH-1's ≥ 2 open sides is the stricter rule and governs |
| Round | `OnLayoutReady` within Preparing; Round's `PreparingWatchdogEff` already includes `LayoutTotalBudgetSec` (45 s) | R-G9, R-G10; Round row correction is request 9 |
| Session | `Seed` as URL option; Generator uses only `PublicSubSeed` | R-G2; request 10 |

**Change requests received (reconciliation 2026-10-24)**

| From | Request | Disposition |
|---|---|---|
| Shared Character & Locomotion (answer to change request 4) | The 0.4 m canal kerb is stepped (step height 45 cm), not blocked; a blocking kerb must be 46-69 cm or carry a blocking volume; walkable floor angle stays 44.765° | APPLIED (AC-G15 corrected; the existing R-G7 pawn-only water blocking volume is the blocker, kerb height unchanged; change request 4 marked answered) |
| Level Socket Contract (reconciliation of change requests 1 and 11) | D7 ranges, V-SIGHT every direction, V-SEP for a third/fourth zone, uniform zone-pair draw, mirror-intercept Selection condition, V-NAV-2 chokepoint clause, V-CLUE, `FinalCandidatesMinCount` 6 | ALREADY DONE here (S4, `CityGenFinalEligibleTargetCount` 6 matches); mirror condition needs the skeleton symmetry transform handed to the layout service (server-only): APPLIED (`CityGenMirrorAssignMaxPct` row). 12 m role-blind bush exclusion stays open (LSC OQ 14) |
| Crowd CR 18 | No runtime navmesh rebuild after `OnLayoutReady` | ALREADY DONE (R-G7, AC-G36) |
| Crowd CR 17 (via LSC) | Every legal player position within 10 m of `CrowdRoute` | ALREADY DONE (S12, AC-G37; now LSC `RouteMaxGapCm`) |
| Crowd CR 8 (via LSC) | Touchable-prop records | PENDING-USER at LSC (schema_changes); S12 emits them only if adopted |
| Session & Travel CR 7 | `PublicSubSeed` only; write `LayoutAttempt`/`LayoutHash`; integer-only generation | ALREADY DONE (R-G2, R-G3, LSC R4.4) |
| Level Socket Contract CR 1 | The Generator never references Round | ALREADY DONE (R-G1; the index row is the orchestrator's) |
| NPC-Kill Penalty CR-13 | No one-pawn chokepoint at fixtures or objective paths | ALREADY DONE (S4, AC-G38) |
| Cross-review 2026-10-24 C-W11 | Stale index claim in Dependencies | APPLIED (cross-review fixes 2026-10-24): Dependencies intro lists index row 16's five upstream links |
| ADR-0008 sync 2026-10-24 | AC preamble test path → `Source/StealthGameplay/Private/Tests/CityGen/` | APPLIED (AC preamble; golden data folder named) |
| ADR-0006 sync 2026-10-24 | R-G3.1 "`uint32` only" → "`uint32` outputs, integer-only (PCG32 has 64-bit integer state)"; ADR-0007 include-lint rule (i) keeps the Generator out of secret-RNG headers | APPLIED (R-G3.1) |
| ADR-0014 sync 2026-10-24 | Pure-data stages run on one worker task (§9.1); OQ3 (ISM/nav APIs) and OQ4 (threading, RNG) answered | APPLIED (stage table, No-hitch row, OQ3 and OQ4 marked answered) |

## Acceptance Criteria
*qa-lead viewpoint applied inline (budget); technical-artist supplied the performance and assembly criteria. Automation tests live in `Source/StealthGameplay/Private/Tests/CityGen/` (ADR-0007/0008; goldens in `Source/StealthGameplay/Private/Tests/Data/`) and are named `StealthGame.CityGen.<Area>.<Scenario>`. Batch tests use 50 seeds unless stated; "golden" means the R-G11 set. Gates follow `testing.strict` defaults; † criteria are provisional pending spikes and their thresholds are re-set from the spike report.*

**Core rules**
- **AC-G1 (R-G2.1)** [Logic/BLOCKING] GIVEN fixed (AttemptSeed, GeneratorKitVersion, LayoutParamsHash), WHEN the Generator runs in two fresh processes and once more with a different player count and join order, THEN socket sets and Manifest bytes are identical. `Determinism.PureFunction`
- **AC-G2 (R-G2.2)** [Logic/BLOCKING] GIVEN an instrumented build, WHEN the Generator runs, THEN no call reaches `GetServerSubSeed`, `RoundIndex` or `FSiteAssignment` (an injected call fires the assert). `Leak.NoSecretInputs`
- **AC-G3 (R-G3)** [Logic/BLOCKING] GIVEN the integer stream, THEN committed golden vectors reproduce bit-exactly, `RangeInt(7)` over 70,000 draws passes chi-square at p ≥ 0.01, and a float-draw counter on the build path is 0. `Rng.IntegerOnlyGolden`
- **AC-G4 (R-G4 S2, S5)** [Logic/BLOCKING] GIVEN 50 seeds, THEN every skeleton's topology is invariant under the transpose, and the two anti-diagonal zones' sun/shade fractions differ by ≤ 1 point before fill. `Skeleton.TransposeSymmetry`
- **AC-G5 (R-G4 S2)** [Logic/BLOCKING] GIVEN 50 seeds, THEN no straight corridor run exceeds 14 c wall to wall and every junction is a 3 × 3 c T (no 4-way crossing). `Skeleton.RunAndJunctionRules`
- **AC-G6 (R-G4 S7)** [Logic/BLOCKING] GIVEN 50 seeds, THEN every emergence socket has W(O) ≤ 30 cells, sockets are ≥ 6 c apart and Residential holds ≤ 55% of them; every Z-alley sits in a 7-8 c block with legs of 2-3 c clear, every L-alley in a 9-10 c block survives S11.2, and every ExitPoint is 2-6 m of navmesh walk from its origin (5.0 m ± one cell) and ≥ 6 m from any bush, court or traversal point. `Emergence.AlleyWitnessAndQuota`
- **AC-G7 (R-G4 S9, S10)** [Logic/BLOCKING] GIVEN one seed with its zone and court records perturbed between two runs, THEN bush and prop placements are byte-identical, and an injected role-field read in either stage fires the assert. `Leak.BushPropStagesRoleBlind`
- **AC-G8 (R-G4 S11)** [Logic/BLOCKING] GIVEN 50 seeds, THEN no sightline repair placed a prop or stall, no recess ≥ 0.6 × 0.6 m or dead-end court < 6 × 6 m exists, and no repair changed connectivity in one half only. `Repair.RulesHold`
- **AC-G9 (R-G5.5)** [Logic/BLOCKING] GIVEN a golden seed, WHEN the run is repeated with an injected 3× slowdown of every stage, THEN the accepted candidate index and `LayoutHash` are unchanged. `Determinism.CandidateLoopNotClockBound`
- **AC-G10 (R-G5.2)** [Logic/BLOCKING] GIVEN 1,000 spawned candidates that passed G-PRE, THEN LSC Validate rejects ≤ 20% of them (`CityGenGPreGuardBandPct` at default). `GPre.FalsePassRate`
- **AC-G11 (R-G6.1)** [Logic/BLOCKING] GIVEN 50 seeds, THEN every instance lies on the 2.0 m grid and 1.6 m course, yaw ∈ {0, 90, 180, 270}, scale exactly 1 and transform determinant > 0. `Placement.GridYawNoMirror`
- **AC-G12 (R-G6.2, art 6.7)** [Logic/BLOCKING] GIVEN 1,000 seeds, THEN every facade run has PE ≤ 1, door gaps are 2-5 with no value three times in a row, every variant occurs 0 or ≥ 3 times, and no unique variant lies within 6 m of a socket. `Facade.PeriodicityAndOccurrence`
- **AC-G13 (R-G7)** [Integration/BLOCKING] GIVEN a 2-client PIE on a golden seed, THEN clients hold no navigation data, the client build path's RNG counter is 0, and the server holds exactly one built navmesh. `Nav.ClientNone`
- **AC-G14 (R-G7 water)** [Integration/BLOCKING] GIVEN 20 seeds, THEN water footprints carry zero nav polygons and a scripted pawn walking at the kerb cannot enter the water. `Nav.WaterNotWalkable`
- **AC-G15 (R-G7, S3)** [Integration/BLOCKING] GIVEN 20 seeds, THEN every stair (civic and temple) connects the navmesh across it, and no pawn crosses a quay kerb into the water: the 0.4 m kerb alone is **stepped**, not blocked (SCL `TraversalMinHeightCm` = CMC step height 45 cm), so the R-G7 pawn-only water blocking volume is the blocker. `Nav.StairsAndKerb`
- **AC-G16 (R-G7, VG-4)** [Integration/BLOCKING †] GIVEN 20 golden seeds on the reference PC, THEN the client build takes ≤ 3.0 s p95, no frame exceeds 33 ms, and the first `Active` frame has 0 missing instances and a settled surface cache. `Assembly.ClientBuildNoHitch`
- **AC-G17 (VG-2)** [Visual/BLOCKING] GIVEN one seed rendered with slabs and stacks and with per-cell modules, THEN mean luminance differs by ≤ 1 L* and a reviewer sees no joint structure; captures retained in `production/qa/evidence/citygen/`. `Visual.StackSlabEquivalence`
- **AC-G18 (R-G8)** [Integration/BLOCKING] GIVEN a 2-client PIE on 5 seeds, THEN `DecorHash` is equal on both clients and every decoration element has no collision, no nav effect and a top ≤ 0.5 m. `Decor.ConstraintsAndParity`
- **AC-G19 (LSC VA-1 to VA-4 on generated layouts)** [Visual/BLOCKING at tier G1] GIVEN 3 generated seeds, THEN the LSC bush 25 m test, static-figure test and paired socket-capture test pass at their LSC thresholds; evidence retained.
- **AC-G20 (R-G9, F2)** [Logic/BLOCKING †] GIVEN 1,000 seeds, THEN kit + decoration instances ≤ 5,000 (≤ 4,000 in 95%), Manifest entries ≤ 3,500 and collision shapes ≤ 20,000 on the server and each client. `Budget.InstancesEntriesShapes`
- **AC-G21 (fairness, LSC playtest criterion on generated layouts)** [Integration/ADVISORY] GIVEN ≥ 30 playtest rounds on tier-G1 generated layouts (G0 rounds do not count), THEN ObjectiveWin per zone falls within 10-20 of 30.
- **AC-G22 (R-G10)** [Logic/BLOCKING] Each injected failure ends as stated: all candidates fail G-PRE → `GPreExhausted` with 0 instances spawned; overflow → next candidate; missing kit asset → `GeneratorConfigInvalid` and fallback with 0 reseeds; two injected timeouts → `HostTooSlow` and fallback; LSC reject → full teardown (0 instances, 0 nav tiles). `Failure.PathsAsSpecified`
- **AC-G23 (R-G11.2-3)** [Logic/BLOCKING] GIVEN the 20 golden seeds, THEN each reproduces its hash, candidate index and outcome; a stage change without a `GeneratorKitVersion` bump fails as unbumped drift and names the first differing stage. `Golden.SeedsReproduce`
- **AC-G24 (R-G11.1)** [Integration/BLOCKING] GIVEN a 3-client PIE on 3 golden seeds (one under 10% loss, 200 ms emulation), THEN every client reports the server's `LayoutHash` and an identical collision hash. `Determinism.CrossClientManifest`
- **AC-G25 (R-G12.4)** [Integration/BLOCKING] GIVEN the Manifest fixture provider with a committed golden Manifest, THEN a 2-client PIE builds, hashes, acks and reaches `Active`. `Fixture.ManifestPath`

**Formulas**
- **AC-G26 (F1)** [Logic/BLOCKING] GIVEN N = 62, 64, 66 at defaults, THEN T = 2,007, 2,153 and 2,250, and every accepted batch layout has N_walk inside its band. `Formula.WalkableBand`
- **AC-G27 (F2)** [Logic/ADVISORY] GIVEN 50 seeds, THEN the F2 estimate is within ± 15% of the measured entry count, and Bytes = 8 + 9 · entries exactly. `Formula.InstanceModel`
- **AC-G28 (F3)** [Logic/BLOCKING] Vectors: `ABABAB` → 1.0 pass, `ABABABAB` → 1.33 fail, `AABAABAAB` → 1.0 pass, `AAAAAAA` → 1.17 fail. `Formula.PeriodicityExcessVectors`
- **AC-G29 (F4)** [Logic/BLOCKING once calibrated; ADVISORY before] GIVEN 50 accepted layouts, THEN p95 J* ≤ `CityGenJaccardP95MaxPct`. `Formula.LayoutDiversity`
- **AC-G30 (F5)** [Integration/BLOCKING †] GIVEN 50 seeds on the reference listen-server host, THEN no attempt exceeds 7 s, and server time from `RequestLayout` to `OnLayoutReady` is ≤ 6 s median and ≤ 10 s p95. `Formula.AttemptTimeBudget`
- **AC-G31 (F6)** [Logic/BLOCKING] GIVEN a 50-seed batch, THEN ≥ 34 seeds are accepted on attempt 0 (LSC D8); the design target ≥ 39/50 is ADVISORY. `Formula.AcceptanceRate`
- **AC-G32 (F7)** [Logic/BLOCKING] Vectors: (214, 224) → 1.047 kept; (205, 222) → 1.083 rejected before fill (0 facades built). `Formula.SkeletonSkew`

**Performance (provisional pending spikes R1, R4)**
- **AC-G33** [Integration/BLOCKING †] GIVEN a golden seed at its measured instance count on a 3060-class PC at 1080p, THEN Lumen + Nanite for the kit costs ≤ 5.0 ms; GIVEN a listen-server host, THEN server spawn + nav build ≤ 3.0 s and steady AI + nav ≤ 1.5 ms. `Perf.KitGpuAndServerBuild`

**Batch and cross-system**
- **AC-G34 (R-G11.4, LSC batch audits)** [Logic/BLOCKING once the Generator exists] GIVEN a 50-seed batch, THEN the LSC batch audits pass (bush correlation p ≥ 0.05, emergence uniformity p ≥ 0.01), and for every single public court metric (size, sun-pool area, distance to centre, ingress count) the top-ranked court is the drawn final in ≤ 30% of seeds, neither axis corner holds > 60% of drawn finals, every accepted layout has ≥ `CityGenFinalEligibleTargetCount` final-eligible courts (none of them in the temple ring), and ≤ 15% of drawn assignments are mirror (±1 c transpose) prelim pairs. `Batch.LscAuditsAndRolePrior`
- **AC-G35 (R-G1, Round and Session)** [Integration/BLOCKING] GIVEN `GeneratedWithFallback` in a 2-client PIE with a missing kit asset injected, THEN the service reaches fallback travel with 0 reseeds, Round re-enters `Preparing` on the graybox, and no Generator symbol is referenced by Round (include-graph check). `Cross.FallbackShortCircuit`
- **AC-G36 (R-G7 navmesh frozen, Crowd CR-18)** [Integration/BLOCKING] GIVEN a 2-client PIE on 3 golden seeds, WHEN 60 s of `Active` play pass with pawns killed and NPCs emerging, THEN the server navmesh's tile build counter and dirty-area count do not change after `OnLayoutReady`, and the navmesh data hash at `Ended` equals the hash at `HandedOff`. `Nav.FrozenAfterHandOff`
- **AC-G37 (S12 CrowdRoute, LSC R6.1, Crowd CR-17)** [Logic/BLOCKING] GIVEN 50 seeds, THEN no two consecutive nodes on a route edge are more than `CityGenRouteNodeSpacingCount` cells apart, every walkable cell is within 10 m of a route edge, every start cell is a route node, and the R6.1 NPC pool (cells within 3 m of a node, greedy ≥ 3 m spacing in canonical order) holds ≥ 4 × (`NpcCount` + 2 · `TeamSize`) cells. `Route.NodeSpacingAndCoverage`
- **AC-G38 (S4 chokepoints, Penalty CR-13, Objectives CR-8)** [Logic/BLOCKING] GIVEN 50 seeds, THEN every court ingress, bridge and route stair has ≥ 2.8 m clear width, every fixture has ≥ 1.2 m clear floor on all four faces, every final-eligible court has ≥ 3 ingresses, and removing any single 0.8 m-diameter disc from the grid navmesh never disconnects a fixture from either zone anchor. `Courts.NoSinglePawnChokepoint`
- **AC-G39 (S5, R-G5.2-3)** [Logic/BLOCKING] GIVEN 50 seeds, THEN every anchor is a plain street cell on the second lattice corridor from both edges, every candidate zone passes V-SEP, V-ZONEFILL and V-RESPAWN in LSC Validate, and an injected candidate whose grid StartToFinal path is 62 m (inside the 8% path guard) is rejected by G-PRE. `GPre.ZonePredicatesAndPathGuard`

## Open Questions
| # | Question | Owner | Target |
|---|---|---|---|
| 1 | Spike R1: does the 5,000-instance cap hold, and does a naive per-module city (~7,500 instances) pass? If it does, bay stacks become optional (the Manifest cap still argues for them). | technical-artist + performance-analyst | Spike R1 |
| 2 | Spike R4 (add a client and a listen-server run): server spawn + nav ≤ 3 s; client collision creation time; "first valid trace" after assembly. Nav cell/tile settings (cell 16-19 cm, tile 8-10 m) to compare. | technical-artist + ai-programmer | Spike R4 |
| 3 | **Answered by ADR-0014 §3.2/§4 (2026-10-24 ADR sync)** (frozen nav built after assembly; ISM nav settings and client-side navigation off per ADR-0014; remaining engine checks are ADR-0014 spikes). Original question: runtime ISM, nav-source and per-mesh nav-area APIs in UE 5.8.4 (UNVERIFIED names): `CanEverAffectNavigation` on ISM, client-side navigation off, whether nav bounds need an explicit update. | unreal-specialist (-> ADR) | Before the Generator epic |
| 4 | **Answered by ADR-0014 §9.1 and ADR-0006 (2026-10-24 ADR sync)**: pure-data stages on one worker task; PCG32 integer stream. Original question: off-game-thread generation and the integer RNG choice (FRandomStream integer path vs a named xorshift/PCG32): one ADR. | lead-programmer (-> ADR) | Before the Generator epic |
| 5 | **Pillar question (CD-GDD-ALIGN):** do fixed anti-diagonal start corners make early reads structural? Decision rule: in G1 playtests, if correct enemy reads in the first 60 s exceed 1.5× the later-round rate, make 4-zone mode the default and accept the lower p (F6). **Added by review 2026-10-24:** with 2 corner zones the final is always in one of the two axis corners (S4 geometry consequence), so both teams know the final region in advance (equally, and split between two far-apart corners). Same decision rule plus: if telemetry shows teams pre-positioning at an axis corner before completing a preliminary in > 25% of rounds, make 4-zone mode the default (the final region then depends on the secret zone pair). Creative-director to confirm the 2-corner reading is acceptable for MVP. | creative-director + level-designer + systems-designer | First G1 playtests |
| 6 | Does the regular lattice feel gridded, and does the transpose make the unseen half predictable? G1 walkthrough test: after walking only their own half, can a tester sketch the other half? If yes, raising `CityGenSkeletonJitterCount` to 2 (and re-checking F7) becomes the default fix. | level-designer + art-director | First G1 walkthrough |
| 7 | Canal width: does the art 3.3 "6 m canal" include the quays? If yes, about 300 walkable cells move to water and F1's target shifts. | art-director | Art bible revision |
| 8 | Is "provisional pending spikes" (navmesh, ISM, Manifest size) acceptable for the Generator milestone plan, or should the Generator GDD be re-reviewed after R1/R4? (Recorded with the PR-SCOPE adjustment 9 pending decision.) | producer | Spike reports |
