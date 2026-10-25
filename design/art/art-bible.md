# Art Bible: Smoking Mirror (working title)

> **Status**: Approved with concerns (AD-ART-BIBLE CONCERNS accepted 2026-10-24 after two gate runs; open items OI-1..OI-13 in 8.x.17); reconciled 2026-10-24; ADR-synced 2026-10-24; user decisions applied 2026-10-25 (wrong-target tally confirmed; `NpcCount` 50, so about 60 bodies per match; derived kit assets and G0 placeholders allowed before the mass-production gate; balanced deck confirmed)
> **Owned By**: art-director
> **Last Updated**: 2026-10-24
> **Art Director Sign-Off (AD-ART-BIBLE)**: CONCERNS (accepted) 2026-10-24 -- gate run 2 flags 1-8 applied; open items OI-1..OI-13 tracked in 8.x.17

## 1. Visual Identity Statement

> **The visual rule:** *The city holds still and the crowd shares one body, so only behaviour can speak.*

### 1.1 The rule and how it resolves ambiguity

The concept document's line, "The city holds still so behaviour can speak,"
is the right instinct and is kept as the spine of the rule. It is sharpened in
one respect. As written it governs the *environment* only. In Smoking Mirror
the crowd is also part of the picture, and the crowd is where the game's most
dangerous visual failure lives: a single character model that looks slightly
different from the others is a tell the player never chose, which is a
Pillar 1 violation no matter how good the model looks. The added clause ("the
crowd shares one body") extends the same discipline to characters, so the rule
now covers every pixel that is not a player's deliberate action.

The rule resolves any visual ambiguity with two questions asked in order:

1. **Does this let a viewer tell one person from another by anything that
   person did not choose?** (model, height, outline, finish, colour, accessory,
   animation fidelity, LOD behaviour). If yes, remove it or bring it to parity.
   This question can veto any other visual goal, including beauty, cultural
   richness and fidelity.
2. **Does this make a *chosen* action easier to see against a calm
   background?** (a head turn, a sudden stop, a run, an objective
   interaction). If yes, keep it. If it competes with a chosen action for the
   eye (flicker, snapping motion, a high-contrast detail at crowd height),
   quiet it.

Question 1 outranks question 2. A gorgeous prop that makes one NPC unique fails;
a plain one that keeps the crowd anonymous passes.

*Alternatives considered for the one-line rule:*

- **Keep the concept line verbatim.** Rejected as the headline because it
  leaves the character pool unprotected. Without the "one body" clause an
  artist could reasonably argue that a richly varied crowd serves "a living
  city." The clause removes that argument. The original sentence is retained
  inside the new one.
- **"Everything is a baseline; only deviation is loud."** Strong on logic but
  abstract; an artist cannot hold it in mind while painting a wall. Kept as the
  logic behind Principle 1 instead.
- **A cultural-mood rule ("hot, ordered, sunlit civic calm").** Accurate to the
  Held Breath direction and used in Section 2, but it describes a mood, not a
  decision procedure. A visual identity statement must settle disputes, and a
  mood cannot.

### 1.2 Supporting principles

The three principles drafted in the concept are kept, in the same order, and
each is given an enforceable test, the pillar it serves and the failure it
prevents.

#### Principle 1 - Baseline before deviation

The world's ambient motion (water, banners, smoke, awnings, birds, foliage) is
slow, periodic and uniform, so that a deliberate human action is the only thing
that changes abruptly. The crowd's collective idle and walk behaviour is the
baseline; the environment must never compete with it.

- **Serves:** Pillar 2 (Every Advantage Is a Tell), secondarily Pillar 1.
  A tell is only a tell against a quiet baseline.
- **Design test:** *When an environmental animation is ambiguous, choose the
  slower, more periodic or more static option.* Concretely, any looping motion
  inside the crowd eye band (0.5 m to 2.2 m above any walkable surface) must
  have a loop period of 4 s or longer, an eased onset (no single-frame starts or
  stops), and no velocity spike that a viewer could confuse with a head snap or
  a sudden halt. If a tester at 20 m can mistake it for a person's action, slow
  it or lift it out of the eye band. The starting values are a hypothesis and
  are to be tuned in playtest, but the principle is not negotiable.
- **Failure it prevents:** a flapping banner or a sudden bird flush that makes
  players misread an NPC as a suspect, turning reading into guessing
  (Pillar 2's design test forbids exactly this).

#### Principle 2 - Silhouette and value over detail

At game distance a figure is a shape and a value, not a texture. Environment
sits at low-to-mid contrast; characters are simple, sturdy forms with legible
limbs and a clear facing direction.

- **Serves:** Pillar 1 (a clean shared silhouette is what makes the crowd
  indistinguishable) and Pillar 2 (heading and gait must be readable for
  tells to exist at all).
- **Design test:** *When detail and clarity compete, choose the clearer value
  mass.* A greyscale render at 20 m and at 30 m must let a tester count the
  figures and read each one's direction of travel. Pass thresholds: at least
  90 percent correct figure count and at least 85 percent correct heading
  across 6 scenes, 6 testers. Environmental surfaces behind the crowd must not
  contain high-contrast detail (strong pattern, hard value edges) at crowd
  height that would hide the limb read.
- **Failure it prevents:** a visually rich market where figures dissolve into
  busy backgrounds, or where a dark cloak makes one body read heavier than its
  neighbours.

#### Principle 3 - Uniformity is a production contract

Every model in the shared character pool is built to one measurable spec:
height, proportions, silhouette mass, palette band, material response, texture
density, rig and rest pose. Meshy output is raw material; it is normalised in
Blender to the spec and rejected if it cannot be.

- **Serves:** Pillar 1 (Hide in Plain Sight) directly. Its design test already
  names "an outlier Meshy model" as something to cut or bring to parity.
- **Design test:** *When a character's look is ambiguous, choose the option
  that makes it more like the rest of the pool.* Verified by the line-up and
  silhouette tests in 1.3.7; a model that "came out different" fails, however
  good it looks on its own.
- **Failure it prevents:** the classic AI-generation drift, where one
  model is a little taller, glossier or more saturated and the first player to
  notice can hunt every instance of it.

*Alternatives considered for the principle set:* a fourth principle,
"Cultural grounding over spectacle," was drafted and folded into the cultural
framing note below, because it is a constraint on content, not a way to resolve
visual ambiguity. "Warm light, cool shadow" was left to Section 2 (Mood), where
it belongs.

#### Trade-offs this statement creates (flagged, not decided here)

- **Uniformity versus Sensation (aesthetic #4) and cultural richness.** The
  Mexica visual world is full of ornament, feather work and status-coded dress.
  Principle 3 deliberately withholds most of it from the pool. The ornament
  lives in the *environment* (architecture, murals, market goods), not on the
  citizens. This is a pillar-driven reduction; if the creative director
  wants richer characters, the route is more pool variants inside the band,
  never role-unique pieces.
- **Uniformity versus Meshy's native variability.** Meshy will not hit these
  tolerances unaided. The cost is paid in Blender normalisation time; budget it
  per model (Section 8).
- **Silhouette clarity versus the UI.** Teammate markers sit above heads
  (confirmed rule). Headwear height is therefore capped (1.3.3) so markers
  never collide with a model's outline.

### 1.3 Character-Pool Uniformity Spec

This is the concrete production contract requested by the concept director. It
governs every model that can be worn by a player or spawned as an NPC, and
nothing else (no hero characters exist; informants use the same pool).

All figures are *contract values*: the number is the target, the tolerance is
the maximum permitted deviation, and a measurement outside tolerance is a fail,
not a judgement call. Where a figure depends on an asset that must be measured
(for example the Manny skeleton), the contract states the rule and records the
working value; the measured value replaces the working value once, at pool
setup, and then recorded as the contract value (open item OI-1: nothing is
called frozen until it has been measured).

#### 1.3.1 Pool composition and permitted variation

| Item | Contract |
| ---- | ---- |
| Base models | **8** is the target; **6** is the minimum acceptable for MVP. Fewer than 6 makes duplicates too obvious; more than 8 raises Blender normalisation cost and inspection burden with no read benefit. |
| Variants per model | Up to **3**: the default, one **garment palette swap**, and one **accessory combination**. Nothing else may vary. |
| Appearance cap | Distinct appearances (model x variant) never exceed **24** in the shipped pool (8 x 3). MVP floor: 6 x 2 = **12**. |
| Appearance deal (art-side invariants) | All bodies in a match (about 60: players and NPCs together, `NpcCount` 50 since the user decision 2026-10-25) are dealt from **ONE shuffled appearance deck**. (1) **Balanced counts:** the per-appearance copy counts in the deck differ by **at most 1** (60 bodies over 12 appearances = twelve appearances with 5 copies; the earlier 50 bodies gave ten with 4 and two with 5). (2) **Players are random members of the deck:** a player's appearance is drawn by the same shuffle, uniformly, with no weight, reserve or priority. (3) **NPC draws are never conditioned on player appearances:** no rule of the form "give each player appearance N NPC copies", because that would make the appearances with extra NPC copies exactly the players' appearances, and a viewer who counts look-alikes could then tell who is a player (a Pillar 1 leak). (4) **Team-blind subset per match:** each match uses a subset of **at most 12 appearances (model x variant) with at least 4 copies each**, chosen by the server from the seed before teams, roles or players are known, out of the 24-appearance cap. Copies minus one is at least 3, so every look has at least 3 other bodies by construction, without any rule that references players. For matches with fewer than 48 bodies the subset size is floor(bodies / 4), capped at 12. The 24 cap is justified by the pool limit (8 x 3) and this deck (subset K <= 12, at least 4 copies each), not by any duplicate guarantee. The art contract guarantees only the cap and that every appearance is individually valid; **the Character Pool & Appearance GDD (`design/gdd/character-pool-appearance.md`, OI-11) owns the allocator** and must pass the invariant tests in 1.3.7 (T6). |
| Team/role independence | No model, swatch or accessory may be assigned by team, role, objective or kill count. Appearance assignment must be team-blind; the art side guarantees every model x variant combination is individually valid so the assignment never has to be constrained. |
| Distribution parity | Players and NPCs are dealt from the same deck (previous row) with identical probability for every appearance. A model is never "NPC-only" or "player-only", and the deck contains no marker of which cards were handed to players. |
| Body base | Every model's under-garment body (the surface under the clothes) conforms to one canonical **Body_Base** mesh (see 1.3.2). Faces, hair, garments and accessories differ; the body does not. |

Variation sits in garment colour *within the band*, face, hair shape and
accessory presence, all of them low-weight on the silhouette. It is never in
mass, height, finish or hue family.

*Alternatives considered for pool structure:*

- **One shared body with fully modular heads, hair and garments (a kit).**
  Mathematically the strongest uniformity (identical rig and weights by
  construction) and the cheapest to test. Not chosen as the default because
  Meshy generates complete figures and the concept scopes a 6-8 model Meshy
  pool. It remains the **fallback**: if normalised Meshy bodies repeatedly
  fail the Body_Base conform test, switch to Meshy-generated heads and garments
  on the one canonical body.
- **Full per-model variety (different heights and builds) for a "natural"
  crowd.** Rejected: any height or build spread is an unchosen tell and
  contradicts Pillar 1. A uniform crowd looks plain; the plainness is the
  camouflage and the intended trade.
- **A single model with palette swaps only.** Rejected: faces and hair would
  repeat so visibly that the crowd reads as clones, which is a different
  immersion failure and invites the player to ignore the crowd entirely.

#### 1.3.2 Height, proportion and body tolerances

| Measure | Contract | Tolerance |
| ------- | -------- | --------- |
| Reference skeleton and mesh | **`SK_UEFN_Mannequin`** (`/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin`, mesh `SKM_UEFN_Mannequin`), the skeleton the playable character `CBP_SandboxCharacter` and the crowd pool use (measured 2026-10-25, OI-1 closed). `SK_Mannequin` (Manny, 180.5 cm, 161 bones) is **not** the pool reference: every "Manny" in 1.3, 5.x and 8.x that names the pool reference reads `SK_UEFN_Mannequin` (the text edits outside this section are change requests CR-OI1-4 to CR-OI1-6, 1.3.2 OI-1 block). | exact asset |
| Stature (sole of foot to skull crown, rest pose, headwear and hair excluded) | **165.6 cm**, measured (height of the `SKM_UEFN_Mannequin` LOD0 imported bounds, UE 5.8.4, 2026-10-25; Blender sole-to-crown on the reference FBX 165.57). *Superseded:* the 175 cm working value and Manny's 180.5 cm. | **+/- 2 cm**: **163.6 to 167.6 cm** |
| Head height (chin to crown) | **19.3 cm** (re-derived 2026-10-25, see the OI-1 block) | +/- 1 cm |
| Bone chain lengths (thigh, shin, foot, spine chain, clavicle-to-upper-arm, upper arm, forearm, hand, neck) | Equal to `SK_UEFN_Mannequin`'s measured chains (OI-1 block below; machine-readable copy `Source/StealthGameEditor/Private/Tests/CharacterPool/Golden/UefnReferenceChains.txt`). **Precedence: chain lengths win.** Every absolute dimension in this section (stature, head height, shoulder, chest, hip, foot length) is derived from the measured chains; if a chain-length check (+/- 3 percent) and an absolute-dimension check disagree on a model, the chain-length result decides and the absolute figure is re-derived, never the chain "corrected" to hit a round number | **+/- 3 percent per chain** (confirmed by the art-director 2026-10-25, the validator tolerance of story 008) |
| Shoulder width (outer skin, under-garment body, at the `upperarm_l` joint height in the reference A-pose) | **51.0 cm** (re-derived; was 40) | +/- 2 cm |
| Chest depth (under-garment body, torso section between `spine_03` and `upperarm_l`) | **27.6 cm** (re-derived; was 24) | +/- 2 cm |
| Hip width (under-garment body, between `thigh_twist_01_l` and pelvis + 2 cm, hands excluded) | **34.8 cm** (re-derived; was 34) | +/- 2 cm |
| Body_Base conformity (under-garment surface) | 95th percentile surface deviation from Body_Base | <= 1.5 cm; mean <= 1.0 cm |
| Foot sole length / footwear thickness | **26.3 cm** (re-derived; was 26) / <= 1.5 cm (the shared flat cactli sandal piece of 1.3.10 only; no bare feet, since a barefoot model in a sandalled pool is a tell) | +/- 1.5 cm / n.a. |
| Foot contact | Sole plane at Z = 0 in rest pose | +/- 0.5 cm |
| Scale | Imported object scale 1.0, no negative scale, no non-uniform scale | exact |

Rationale for holding stature to +/- 2 cm: the motion-matching databases
the playable character uses are played on the reference skeleton. Stride
length, foot placement and traversal heights all assume its leg length.
Scaling a character to differ visibly makes feet slide, and that sliding is a
*tell that differs per model*. A tight stature band removes both the visible
height difference and the slide. The contract is "the reference skeleton's
measured stature +/- 2 cm", so that no pool model ever needs a retarget scale.

**OI-1: CLOSED 2026-10-25.** *Measured:* UE 5.8.4 editor run of 2026-10-25
(`production/qa/spikes/2026-10-25_S1-S2-S14.md` section 4, component-space
reference-pose bone dump `production/qa/logs/2026-10-25_A_mannequin-bone-dump.txt`),
re-measured in Blender 5.2.3 on `assets/characters/reference/SKM_UEFN_Mannequin.fbx`
by `tools/asset-pipeline/body_measure.py` with identical chain results.
*Decision (user instruction, 2026-10-25):* the pool reference is the UEFN
mannequin at **165.6 cm**, not `SK_Mannequin`. *Owner:* technical-artist.

| Measure (reference pose, cm) | `SK_UEFN_Mannequin` (contract) | Superseded |
| --- | --- | --- |
| Bones | **88** (`root` + 87). **Zero extra bones, reference = 88.** The prototype Meshy citizens `SK_Citizen02` to `05` carry **89**: Blender exported the `SKM_UEFN_Mannequin` empty above the armature as an extra root bone. Every pool FBX is checked for 88 skeleton nodes (one `Root` + 87 `LimbNode`, no `Null` above `root`) | Manny 161 (Manny Simple / Quinn Simple 89) |
| Stature (LOD0 imported bounds) | **165.6** (band 163.6 to 167.6) | 175 working value; Manny 180.5 |
| Thigh (`thigh_l` to `calf_l`) | **39.05** | Manny 43.34 |
| Shin (`calf_l` to `foot_l`) | **42.56** | Manny 42.22 |
| Foot (`foot_l` to `ball_l`) | **15.15** | Manny 16.78 |
| Spine (`pelvis` through `spine_01..05` to `neck_01`) | **49.47** | Manny 57.56 |
| Clavicle to upper arm (`clavicle_l` to `upperarm_l`) | **15.51** | Manny 17.81 |
| Upper arm (`upperarm_l` to `lowerarm_l`) | **26.65** | Manny 27.77 |
| Forearm (`lowerarm_l` to `hand_l`) | **22.35** | Manny 27.25 |
| Hand (`hand_l` through the middle finger to `middle_03_l`) | **16.23** (straight line 15.44) | Manny 17.21 |
| Neck (`neck_01` through `neck_02` to `head`) | **9.95** | Manny 10.02 |

Per-chain validator tolerance: **+/- 3 percent** (proposed by story 001,
confirmed by the art-director at the re-sign below).

*Re-derived dependent values.* The stature (165.6) is outside the old 173 to
177 cm band, so every dependent value was re-derived. Method, per value: (1)
scale the old figure by **165.6 / 175 = 0.9463**; (2) sanity-check the result
against the chain-bound reference body (`SKM_UEFN_Mannequin` is skinned to
exactly these chains, measured by `body_measure.py`, definitions in its
docstring); (3) where the two disagree by more than the tolerance, the
chain-bound figure decides (chains win). The UEFN mannequin is not a scaled
Manny (its forearm is 18 percent shorter and its shoulders broader), so most
figures follow step 3:

| Value | Old | Scaled (x 0.9463) | Reference body | Contract now | Derivation |
| --- | --- | --- | --- | --- | --- |
| Head height | 23 +/- 1 | 21.8 | 19.3 | **19.3 +/- 1** | crown 165.57 minus chin 146.25 on the reference mesh; the scaled 21.8 is 2.5 cm off, so the chain-bound figure decides |
| Shoulder width | 40 +/- 2 | 37.9 | 51.0 | **51.0 +/- 2** | skin width at the `upperarm_l` joint height; the joints alone sit 2 x 17.78 = 35.6 cm apart (clavicle chain 15.51), so 37.9 cm would leave 1.2 cm of deltoid; chains decide |
| Chest depth | 24 +/- 2 | 22.7 | 27.6 | **27.6 +/- 2** | torso section front-to-back maximum; chains decide |
| Hip width | 34 +/- 2 | 32.2 | 34.8 | **34.8 +/- 2** | outer width between thigh twist and pelvis; scaled value 2.6 off; chains decide |
| Foot sole length | 26 +/- 1.5 | 24.6 | 26.3 | **26.3 +/- 1.5** | heel to toe; foot chain 15.15 plus heel and toes; chains decide |
| Hem height (1.3.3) | 55 +/- 8 | 52.0 +/- 7.6 | knee joint 51.8 | **52.0 +/- 7.6** (44.4 to 59.6) | scaled; check: the band still runs from just below the knee joint (`calf_l` at 51.76) to the upper calf, the original intent |
| Garment envelope, front (1.3.3) | 54 +/- 3 | 51.1 | body shoulders 51.0 | **64.2 +/- 3** | the scaled 51.1 is narrower than the bare body, so the envelope is re-derived as the body shoulder 51.0 plus the old garment allowance scaled (54 - 40 = 14, x 0.9463 = 13.2) |
| Garment envelope, side (1.3.3) | 34 max | 32.2 | body chest 27.6 | **37.1 max** | body chest depth 27.6 plus the old allowance scaled ((34 - 24) x 0.9463 = 9.5) |
| Leg gap (1.3.3) | 14 (8 % of stature) | 13.2 | n.a. | **13.2** | 8 percent of 165.6 |
| 8.6 import outcome | 175 cm | 165.6 | 165.6 | **imported at 165.6 cm (165.6 uu), import uniform scale 1.0** | the reference FBX is already at the reference stature, so no import scale differs from 1.0 |
| 8.7 step 3 target | 175 +/- 2 | 165.6 +/- 2 | 165.6 | **165.6 +/- 2 (163.6 to 167.6)** | the measured stature |

The 1.3.3 rows (envelope, side depth, hem height, leg gap) and hem width
(44 +/- 4, not re-derived here) still carry the old numbers in their own table:
this block supersedes them until the art-director edits 1.3.3 (CR-OI1-4).

*Downstream values that assume 175 cm (change requests, owners edit their own
documents; story 001 edits none of them):*

| CR | Value | Owning document | Owner |
| --- | --- | --- | --- |
| CR-OI1-1 | `TeammateMarkerAnchorCm` 195 (= 175 + 7 + 13) and OQ7 "re-derive when OI-1 closes": proposed 186 (= 165.6 + 7 headwear cap + 13 clearance, rounded) | `design/gdd/teammate-markers.md` (F2 variable table, tuning knob table, OQ7) and the tuning data file | game-designer with ux-designer |
| CR-OI1-2 | Reference-camera distance table (175 cm figure, "screen fraction about 1.5 / distance") in 8.5, `H_fig = 175` in SCL F-tier formula and ADR-0013 F7: re-derive with 165.6 (fraction about 1.43 / distance) | `design/art/art-bible.md` 8.5; `design/gdd/shared-character-locomotion.md` (H_fig row and example); `docs/architecture/adr-0013-animation-motion-matching-significance.md` | art-director (8.5); game-designer (SCL); technical-director (ADR-0013) |
| CR-OI1-3 | T2 line-up (15 m arc, 1.5 m spacing) and T3 (20 m, 30 m) distances, the 20 to 30 m reading range and G-TWIN 15 m captures: confirm or re-derive for a 165.6 cm figure (about 5 percent smaller on screen) | `design/art/art-bible.md` 1.3.7; `design/gdd/character-pool-appearance.md` CP-15, AC-CP-23, AC-CP-24b | art-director; game-designer (CPA) |
| CR-OI1-4 | 1.3.3 table: envelope 64.2 +/- 3, side depth 37.1, hem 52.0 +/- 7.6, leg gap 13.2; re-derive hem width 44 +/- 4 | `design/art/art-bible.md` 1.3.3 | art-director |
| CR-OI1-5 | "Manny" / `SK_Mannequin` as the pool reference in 1.3.7 checklist items 1, 3, 17, 18, 1.3.9, 5.x and 8.x.7 (skeleton path) | `design/art/art-bible.md` | art-director |
| CR-OI1-6 | GDD text naming Manny as the pool skeleton (CP-5 MVP tier "built on the Manny body") | `design/gdd/character-pool-appearance.md` | game-designer |

**Re-sign of 1.3 (OI-1 closure):** art-director, 2026-10-25 (unattended
build run authorised by the user; recorded in
`production/session-logs/decision-log.md`). Body_Base (1.3.9) is built at
165.6 cm on these chains (story 002); no working value remains in force.
**OI-1 was a hard prerequisite for the first Meshy credit, including the single test character of the mass-production gate (8.x.14);** that prerequisite is now met, and OI-10 (Body_Base accepted) still gates the credit.

*Alternatives considered:* a +/- 5 percent natural variation band (rejected,
because a 9 cm height difference is visible at 20 m and is chosen by nobody),
and historically scaled stature (Mexica adults are generally documented as
shorter than modern averages; rejected for gameplay because it would force the
whole animation set to be re-authored and would shrink the capsule).

#### 1.3.3 Silhouette and clothing mass

The silhouette rule is stated as an outcome: **no pool model may be
distinguishable from another by outline alone at 20 m or 30 m in greyscale,
standing or walking.** The measurable constraints that achieve it:

| Measure | Contract |
| ------- | -------- |
| Garment envelope, front view | Maximum outline width **54 cm +/- 3 cm** at the widest point between 100 and 140 cm height (shoulder/cloak line). No garment edge outside this envelope at any height. **This is a TARGET that every cut is drawn to, not an outcome that follows from the cut's name**: a cut that wants more width (a fuller drape, a deeper fold) moves the fabric inside the envelope (gathers, wraps, a tighter knot), and the envelope never grows to fit the cut. Verified per model by T1 item 6 and by the pairwise IoU in this table. |
| Garment envelope, side view | Maximum depth **34 cm** at any height; front or back panels never project further. |
| Hem height | **55 cm +/- 8 cm** above the ground on every model (knee to upper calf). Lower hems (ankle-length skirts) are not allowed, because they hide leg motion and the gait is the tell. |
| Garment topology | Garments are skinned to the shared skeleton. **No cloth simulation**, no physics bones, no dangling parts longer than 15 cm. Secondary motion would differ per model or between player and NPC, and cloth simulation is also outside the 4 ms animation budget. |
| Head plus hair | Maximum width **22 cm**; hair rises no more than **+4 cm** above the skull. |
| Hem width (outline at the hem) | **44 cm +/- 4 cm** (front view, at the hem line). Together with the 54 cm shoulder/cloak line this is the 3.2 shoulder-weighted trapezoid. |
| Heading mass | At least **60 percent** of the hair or head-covering volume lies **behind the ear plane** (3.2 heading rule), measured on the rest-pose mesh. |
| Leg gap | At least **14 cm** (8 percent of stature) between the legs at mid-stride in the walk loop, measured at the knee line in side view, on LOD0 and on LOD3. |
| Neckline | The garment's neckline sits at least **6 cm below the chin** on every model; no hood, high collar or cape merges head and shoulders. |
| Neck gap | The neck is visible as bare skin for at least 6 cm below the chin, front and side view, with no hair, bead strand, collar or knot crossing it; a gap of background is visible between the jaw and the garment line in side view. |
| Headwear | Total height above skull crown **no more than +7 cm**, so teammate markers never intersect a different outline. |
| Handheld or carried items | **None** on any pool model. Carried props are animation and gameplay content, not model content, and would be a role signal. |
| Face and skin marks | No face paint, tattoos, scars or body paint. These are role markers and are also a risk for the cultural framing below. |
| Outline similarity (computed) | Silhouette masks rendered in the rest pose from front, side and back at 20 m (1080p), aligned on the foot centroid; **pairwise intersection-over-union >= 0.90** for every pair of pool models. |
| Outline similarity (human) | Described in 1.3.7, test T3. |

Cultural vocabulary is carried by *cut lines and surface pattern inside the
envelope*. The knotted-shoulder cloak (tilmatli), the huipil and wrap skirt,
and the loincloth (maxtlatl) can all be drawn to the same envelope: a shoulder
cloak covers the width, a huipil is cut to the hem band. Body-type neutrality
is therefore a feature: different clothing vocabularies must not produce
different masses. **No model is a maxtlatl-only look.** A loincloth may appear
only beneath a cloak or wrap that carries the knee-to-calf hem, so that every
model keeps the 55 cm +/- 8 hem, the 44 cm +/- 4 hem width and the legs
visible below the hem. The eight cuts are listed in the roster (1.3.10).

#### 1.3.4 Palette, value and material response

**Palette and value band**

| Region | Contract |
| ------ | -------- |
| Skin | CIELAB albedo **L* 44 to 52 (mean about 48), C* 18 to 30, hue angle 45 to 65 degrees**. One narrow warm-brown band for the whole pool. No skin is outside the band, and variation inside the band is non-systematic. Narrowed from L* 38 to 55 on 2026-10-24 so that (a) every garment swatch (L* 60 to 62) is at least 8 L* lighter than the top of the band, which satisfies the 4.3 rule of at least 5 L*, and (b) the leg and forearm backdrop rule (4.1 Law 3b) is measured against a stable skin mean of 48. |
| Hair | **L* 10 to 22, C* <= 8.** All pool hair is dark. |
| Garments | **L* 40 to 78, C* <= 28.** No pure white, no black, no saturated colour. |
| Excluded hues | Turquoise/teal (hue angle **170 to 230 degrees at C* > 12**) is reserved for allied UI; saturated red (hue angle **15 to 40 degrees at C* > 28**) is reserved for danger/finality UI. Neither may appear on any world character. |
| Swatch table | Garments draw from a fixed table of **8 earth swatches** (undyed cotton, buff, ochre, adobe, muted indigo, maguey green-grey, umber, charcoal-brown), defined in Section 4. A swatch is a discrete table entry, not a free colour. |
| Swatch spread | Every swatch must appear on **at least 3 distinct models**, so no hue is unique to a model, team or role. |
| Value-neutral swaps | A palette swap must keep the model's area-weighted mean garment L* within **+/- 5 L*** of its default. Swaps change hue family, never the value mass. |
| Mean-value parity | In the sun-lit capture (1.3.7, T2), the mean rendered luminance of every model must be within **+/- 8 percent** of the pool median, in sun and in shade. |
| Colour belongs to the city | No colour on any world character may be unique to a team, role or objective. Team identity is shown only by the teammate marker (UI, turquoise) and only to teammates. |

**Material response**

| Property | Contract |
| -------- | -------- |
| Shading model | Default Lit for every surface of every pool model. No Subsurface Profile, clear coat, cloth shading, anisotropy or custom shading model. |
| Roughness | Cloth **0.70 to 0.90**; skin **0.50 to 0.65**; hair **0.60 to 0.75**; leather and sandals **0.60 to 0.80**. |
| Metallic | **0.0 on all surfaces** (hard zero; no outliers). Jewellery is shell, stone, bone or ceramic, all non-metal. Gold, obsidian mirrors and polished metal are reserved for the environment and for the title's symbolism. |
| Specular | Engine default (0.5). No overrides. |
| Emissive | **None**. Zero on every pool model. A glowing detail is a beacon. |
| Opacity | Opaque in appearance. No masked hair cards or translucent cloth; hair is sculpted geometry. (Cheaper, and avoids per-model shader cost differences.) The shared master's blend mode is Masked only for the camera's eye-mask dither, identical on every pawn (8.x.6, ADR-0022 §3). |
| Baked lighting | **None.** Meshy textures commonly contain baked shadow and highlight. Albedo must be de-lit and re-baked; under a neutral HDRI, no directional brightness gradient greater than 10 percent may remain across any 20 cm region. |
| Master material | One master material with parameter-driven swatch selection (implementation by technical-artist). Pool models are instances of it and do not own custom node graphs. |

Rationale: value and finish are the two things the eye compares first at
distance. Hue differences are masked by distance and haze, but a glossy or
too-light model reads as a different "kind" of figure at 15 to 30 m. The narrow
roughness bands and the hard-zero metallic and emissive channels remove the
possibility of a model that catches the sun differently.

*Alternatives considered:* full-range roughness variation for realism
(rejected, produces sun glints on single models); a stylised flat-colour pool
(rejected, because it fights the PBR world, would read as a UI element in the
crowd and conflicts with the "stylised-realistic" direction).

#### 1.3.5 Texture, geometry and rig budgets

Sized for ~60 on-screen, motion-matched characters at 60 fps (RTX 3060-class,
1080p; 10 players + `NpcCount` 50, user decision 2026-10-25) with at most **4 ms** of
animation for the whole crowd (about 67 microseconds per character on average;
it was 80 at 50 characters; working value since `NpcCount` 50 (user decision 2026-10-25), re-verify in spike R3).

| Item | Contract |
| ---- | -------- |
| Material slots | **1** per model. |
| UV | One UV channel (UV0), no overlapping UVs on unique surfaces, padding >= 8 px at 1024. |
| Texture set | BaseColor (sRGB, BC7), Normal (BC5), ORM packed Occlusion/Roughness/Metallic (linear), each **1024 x 1024**, plus a **required** fourth texture, the swatch/trim region mask (512 x 512, linear, BC1): **R = garment region, G = trim region (at most 25 percent of garment area, 4.3), B = accessory region**. The mask is what lets one material instance express "garment swatch + trim swatch" (8.x.6). |
| Texel density | **5.12 texels/cm (+/- 10 percent, 4.6 to 5.6)**, measured as the median across visible faces. This is the same density for face, hands and body; faces do not get an extra tile. Hidden faces (skin under garments) are deleted, not textured. |
| Texture memory | About 4 MB per model; **under 40 MB** for the full pool of 8. |
| LOD0 triangles | **12,000 +/- 2,000** (10,000 to 14,000). |
| LOD1 / LOD2 / LOD3 triangles | **<= 6,000 / <= 2,500 / <= 900**. |
| Worst-case crowd estimate (re-derived 2026-10-24 against the 8.5 screen-height bands and the 8.x.8 tier caps) | The 8.x.8 caps limit how many characters can sit in each band: at most **8** at LOD0 (figure >= 19 percent of screen height), at most **14** in the LOD1 band, and the remaining **38** at LOD2 (the pessimistic case puts everyone inside 24 m, so none at LOD3). 8 x 14k (LOD0 at its upper tolerance) + 14 x 6k + 38 x 2.5k = 112k + 84k + 95k = **about 291k triangles** for 60 characters (was 266k at 50), before shadows (about 275k at the nominal 12k LOD0; working value since `NpcCount` 50 (user decision 2026-10-25), re-verify in spike R3). A typical frame is far lower because the 20 to 32 m reading range and beyond is LOD3 (900). The earlier 6 / 14 / 20 / 10 split was a scenario that did not follow from the bands and is withdrawn. 291k is an **ASSUMED design-density case**, not a verified ceiling: the arithmetic is exact, but the 8 / 14 / 38 caps hold only while the camera and spawn-density rule of OI-13 (owner level-design with game-designer) keeps at most 8 figures in the LOD0 band and 14 in the LOD1 band. It is about 36 % of the **800k non-Nanite scene ceiling (8.x.2), which includes characters**; props, foliage and bushes share the remaining **about 509k**. |
| LOD behaviour | LOD switching is by **figure screen height alone** and is **identical for players and NPCs**; the screen-height table in **8.5 is authoritative** for every threshold and distance (5.6 defers to it). LOD silhouettes stay within 2 percent of LOD0 outline area, so a LOD pop is not readable as a change. **The 24 to 30 m part of the reading range is LOD3**, so the 900-triangle LOD3 mesh must keep the **back-weighted head mass** (the heading rule: at least 60 percent of hair volume behind the ear plane) and the **leg gap** (at least 14 cm at mid-stride); a LOD3 that loses either fails T1 item 16. |
| Skeleton | The **project's reference skeleton asset `SK_UEFN_Mannequin`** (1.3.2, OI-1 closed 2026-10-25), unmodified. Hierarchy and bone names identical to the asset. **Zero extra bones**: no cloth, hair, accessory or corrective bones on pool models. **Measured bone count: 88** (`root` + 87, UE 5.8.4, 2026-10-25); it is the same number for all models, and an FBX whose skeleton has 89 nodes (an exported Blender armature object or empty above `root`, the prototype citizens' defect) fails. *Superseded:* "the project's Manny skeleton asset" (161 bones). |
| Skinning | Maximum **4 influences per vertex**. Weights transferred from the `SKM_UEFN_Mannequin` body weights or from the accepted Body_Base (nearest-face-interpolated Data Transfer), then hand-corrected. LOD2 and below may strip fingers and twist bones; the same stripping applies to all models. |
| Deformation quality | Across the standard pose set (walk, run, idle, brace, assassination, vault, reversal counter), no visible mesh intersection greater than **1.0 cm** and no collapsed or candy-wrapped joint beyond 15 degrees of twist. |
| Physics asset | Identical capsule-based asset on all models; no simulated bodies. |
| Rest pose | The `SK_UEFN_Mannequin` **reference pose** (A-pose, upper arm about 42 degrees below horizontal). Per-bone rotation within **1 degree**, per-bone position within **0.5 cm** of the reference. Origin at ground level between the feet, forward axis matching the Unreal +X convention after export, and all transforms applied. |

*Alternatives considered:* a 2048 x 2048 texture set (rejected, density of
about 12 texels/cm is invisible at game distance and wastes memory and
bandwidth); Nanite skeletal meshes (not assumed, because they are not a
dependable foundation for the Game Animation Sample's skinning and would add a
differing code path between LODs; revisit when the technical-director confirms
support in UE 5.8).

#### 1.3.6 Accessory rules

- **Allowed:** hair knot or tied hair, a plain cotton headband, a plain cotton
  head-wrap, matte stone/shell neck beads (bead diameter <= 3 cm), small ear
  spools (diameter <= 3 cm), a shoulder knot, flat sandals (cactli).
- **Occurrence rule:** an accessory may exist on a pool model only if the same
  accessory exists on **at least 2 other pool models** in the same pool (at
  least 3 occurrences in all) in some variant. It must also be geometrically
  the same piece, not a lookalike.
- **No role-unique accessory:** feather headdresses, helmets, jaguar or eagle
  costume, face paint, weapons, tools, banners, baskets or bags are not allowed
  on the pool. Sumptuary rank markers and warrior regalia are excluded on
  purpose, since they are role signals in the source culture and would be
  role signals here.
- **Projection limit:** nothing projects more than 6 cm from the body surface,
  keeping accessories inside the garment envelope of 1.3.3.
- **Material rule:** accessories obey every rule in 1.3.4; no metal, no
  emissive.
- **Counting:** accessory triangles count against the LOD0 budget. Accessories
  are merged into the single skeletal mesh (one material slot).

#### 1.3.7 Acceptance tests and per-model checklist

A model enters the pool only when it passes the numeric audit (T1) and then
the visual tests (T2 to T4). Failure of any test sends it back to Blender
normalisation. **There are no waivers**: a model that cannot pass is cut,
because accepting one is accepting a Pillar 1 violation.

| Test | Procedure | Pass condition |
| ---- | --------- | -------------- |
| **T1 Numeric audit** | Script-assisted measurement of every item in the checklist below on the imported asset. | Every item in tolerance. |
| **T2 Line-up at 15 m** | All pool appearances (all models, all variants), order randomised, at 1.5 m spacing on an **arc of 15 m radius centred on the camera** (every figure at 15 m), **at most 12 figures per capture** (a 24-appearance pool is captured as 2 randomised halves; a straight 36 m row would not fit the frame at 15 m), idle pose, UE capture at midday: once in full sun, once in building shade. Capture at 1080p. 6 testers each give 3 shuffles and name the one that stands out by finish, brightness or colour. | No model is named by more than **chance + 12.5 points**, i.e. **1 / N_models + 0.125** of selections in sun or in shade (25 percent at 8 models, about 29 percent at 6), and the mean luminance parity in 1.3.4 holds. |
| **T3 Greyscale silhouette** | Desaturate the same scene. Render at 20 m and at 30 m, standing and in the walk loop. (a) Computed: pairwise silhouette IoU as in 1.3.3. (b) Human: tester sees one model's silhouette video for 5 s, then picks it from the set of all N_models models. | (a) IoU >= 0.90 for every pair. (b) Correct identification **<= 1 / N_models + 0.125** (25 percent at 8 models, about 29 percent at 6) across at least 64 trials. |
| **T4 Motion and skin** | Play the standard pose and locomotion set on every model under identical conditions. | Deformation rule in 1.3.5 holds; no foot sliding visible at the Manny reference stride; animation cost within 10 percent of the pool median per character. |
| **T5 Crowd integration** (periodic) | 60 characters at once in the graybox map (10 players + `NpcCount` 50), 60 fps target, animation <= 4 ms. | Budget holds with the real pool. Re-run when the pool changes. |
| **T6 Allocator invariants** (automated, owned by the Character Pool & Appearance GDD) | Simulate 10,000 matches of the appearance deal (1.3.1). | (a) max minus min copies <= 1 in every match; (b) every appearance has >= 4 copies; (c) the subset is <= 12 appearances; (d) the NPC copy count of a player's appearance has the same distribution as that of a non-player appearance (two-sample test, p > 0.05); (e) the subset is identical when the same seed is dealt with different player lists. |
| **T7 Minimum variety (G-VAR)** (owned by the Character Pool & Appearance GDD, AC-CP-24a) | Active models at 8 m in idle; paired discrimination trials, randomised pair order, 3 s exposure, 60 pre-registered trials per pair from 6 testers. | For every pair, point estimate of correct discrimination >= 75 percent and lower 95 percent bound >= 65 percent (provisional, CPA OQ-3). T1 to T5 prove no model stands out; T7 proves the models still differ, so a pool that passes only by being identical cannot ship. |

**MVP tier (Character Pool & Appearance CP-5).** The MVP pool may be the
shared-body kit of 1.3.1 invoked early: six models built on the Manny body as
recombinations of hair, headwear and garment-cut geometry, each baked into one
merged single-slot mesh. Quinn is not counted as a model difference (it differs
in build and would fail checklist items 1 to 5). Every test above, T7
included, applies unchanged; provisional pending the Meshy retarget spike and
the Manny measurement (OI-1).

**Per-model checklist** (each line pass/fail, recorded on the model's sign-off
sheet):

1. Stature within the measured `SK_Mannequin` stature +/- 2 cm (the 175 cm working value applies only to the provisional Body_Base; no model is generated or audited before OI-1 closes).
2. Head height 23 cm +/- 1 cm.
3. All bone chains within +/- 3 percent of Manny.
4. Shoulder, chest and hip dimensions within 1.3.2.
5. Body_Base conformity: 95th percentile deviation <= 1.5 cm.
6. Garment envelope front and side within 1.3.3.
7. Hem height 55 cm +/- 8 cm; legs visible below the hem.
8. Hair <= +4 cm, headwear <= +7 cm above the skull; head width <= 22 cm.
9. No carried items, face paint, tattoos or role costume.
10. Skin, hair and garment colours inside the CIELAB bands; no excluded hue.
11. Every garment swatch is from the 8-swatch table, used on >= 3 models.
12. Palette swap keeps mean garment L* within +/- 5.
13. Roughness values inside their bands; metallic = 0; emissive = 0; Default Lit only.
14. No baked lighting (neutral HDRI gradient test).
15. 1 material slot, 1 UV channel, 1024 x 1024 texture set (BaseColor, Normal, ORM) **plus the 512 x 512 BC1 linear swatch/trim region mask** `_M` (R garment, G trim, B unused: accessory variants are separate mesh assets, CPA CP-4), with the **G (trim) region <= 25 percent of the garment area**; texel density 4.6 to 5.6 per cm.
16. LOD0 10k to 14k triangles; LODs inside their ceilings; LOD silhouette within 2 percent; **LOD3 keeps the back-weighted head mass and the 14 cm leg gap**.
17. Manny skeleton unmodified, zero extra bones, <= 4 influences per vertex.
18. Rest pose within 1 degree / 0.5 cm of Manny's reference pose; scale 1.0; origin at the ground.
19. Every accessory present on >= 2 other models; none role-unique.
20. T2, T3 and T4 passed, with captures retained in `production/qa/evidence/`.
21. Hem width (outline at the hem) 44 cm +/- 4 cm.
22. At least 60 percent of the hair or head-covering volume behind the ear plane.
23. Leg gap at least 14 cm at mid-stride (walk loop, side view), on LOD0 and LOD3.
24. Neckline at least 6 cm below the chin.
25. Visible neck gap: at least 6 cm of bare neck below the chin, front and side, nothing crossing it.

#### 1.3.8 Dependencies and handoffs

- **technical-artist:** the pool master material and swatch parameters,
  LOD settings applied identically to players and NPCs, the T1 audit script.
- **gameplay/systems (Character Pool & Appearance GDD, `design/gdd/character-pool-appearance.md`; OI-11):**
  owns the **appearance-deal allocator** of 1.3.1: one shuffled deck for all
  bodies, per-appearance counts differing by at most 1, players as random
  members of the deck, NPC draws never conditioned on player appearances, and a
  team-blind per-match subset of at most 12 appearances with at least 4 copies
  each. The art side states the invariants and test T6; the GDD implements them.
- **Section 4 (Color System):** defines the 8 swatches and hex values; this
  section defines only their permitted band.
- **Section 8 (Asset Standards):** adopts the budgets of 1.3.5 as hard limits
  and defines the Meshy-to-Blender normalisation recipe and its time budget.
- **Open measurement (OI-1, owner technical-artist):** Manny's exact stature,
  bone count and chain lengths have not been read from the project's asset.
  Until they are, 175 cm is a working value only, and nothing in 1.3 is
  frozen. Rule when checks conflict: bone chain lengths win and absolute
  dimensions derive from them (1.3.2).
- **Body_Base (first Blender-only deliverable, 1.3.9)** and the **roster
  (1.3.10)** are prerequisites for generating any Meshy character.

#### 1.3.9 Body_Base: the first Blender-only deliverable

Every contract value in 1.3.2 is only checkable against a concrete mesh, so the
canonical **Body_Base** is built before any Meshy model is generated, and no
Meshy credit is spent until it is accepted **and** OI-1 is closed (1.3.2).

| Item | Contract |
| ---- | -------- |
| Owner / acceptance | **technical-artist** builds it in Blender only (no Meshy). It is accepted when it passes **T1 items 1 to 5 and 17 to 18** (1.3.7) on its own, and the art-director records the sign-off. |
| Depends on | **OI-1** (the measured `SK_Mannequin` stature, bone count and chain lengths). If OI-1 is not closed, Body_Base is built to the 175 cm working value and re-scaled once when it closes. This is the only deliverable allowed to precede OI-1: the re-scaled Body_Base is re-accepted (T1 items 1 to 5 and 17 to 18) before the first Meshy credit, so no generated model is ever built against the working value. |
| Geometry | One unisex adult body, bare under-garment surface only (no hair, no face detail beyond a neutral head form), rest pose = the Manny reference A-pose, soles at Z = 0, origin between the feet, scale 1.0, transforms applied. Not exported to Unreal; it lives in each model's `.blend` as `Body_Proxy` (8.7). |
| Dimensions | Stature, head height, shoulder 40 cm, chest depth 24 cm, hip 34 cm and foot length per 1.3.2, with bone chain lengths equal to Manny's (chains win, 1.3.2). |
| Skinning | Weights transferred from the Manny body mesh and hand-corrected; <= 4 influences; no extra bones. Pose set of 1.3.5 passes (no intersection > 1.0 cm, no joint twist beyond 15 degrees). |
| Use | Conform target for every model (95th percentile surface deviation <= 1.5 cm, mean <= 1.0 cm), shrinkwrap/sculpt reference in 8.7 step 3, and the weight donor for the pool when Manny's own mesh is not suitable. |
| Fallback | If 3 of the first 6 Meshy models fail conformity, switch to Meshy-generated heads and garments on Body_Base (1.3.1). |

#### 1.3.10 Character roster (N = 8)

The roster fixes, for each base model, the cut, hair form, accessory set and
swatches, so the 4.3 assignment rule, the occurrence rules and the 24-instance
cap are checkable on paper before generation. **Every cut is drawn to the
envelope TARGET of 54 cm +/- 3 cm at the cloak line and 44 cm +/- 4 cm at the
hem (1.3.3), keeps a knee-to-calf hem (55 cm +/- 8), a neckline at least 6 cm
below the chin, a visible neck gap and free forearms.** Hair is dark (L* 10 to
22), sculpted geometry, with at least 60 percent of its volume behind the ear
plane. All eight wear the **same flat cactli sandal piece** (sole <= 1.5 cm), so
footwear is uniform rather than an accessory. Swatches follow the 4.3 rule with
indices modulo 8: default S(k), palette swap S(k+1), trim S(k+3) in both
variants.

| k | Garment cut (inside the envelope) | Trim placement (swatch S(k+3), <= 25 percent) | Hair / knot form | Accessory set (accessory variant) | Default | Palette swap | Trim |
| - | --------------------------------- | --------------------------------------------- | ---------------- | --------------------------------- | ------- | ------------ | ---- |
| 1 | Tilmatli, knot on the right shoulder, straight hem | Hem band | Low back knot at the nape | Plain cotton headband | S1 | S2 | S4 |
| 2 | Tilmatli, knot on the left shoulder, straight hem | Shoulder border | Short tied tail at the nape | Neck beads + ear spools | S2 | S3 | S5 |
| 3 | Huipil over a wrap skirt, plain hem | Hem band | Coiled bun, low and back | Ear spools | S3 | S4 | S6 |
| 4 | Huipil over a wrap skirt, hip sash | Sash (belt) | Two-section tie at the nape | Plain cotton headband | S4 | S5 | S7 |
| 5 | Tilmatli, belted at the waist | Belt | Braid coiled at the back of the head | Neck beads | S5 | S6 | S8 |
| 6 | Short-sleeved huipil over a wrap skirt | Shoulder border | Straight cut at the nape, back-weighted | Ear spools | S6 | S7 | S1 |
| 7 | Maxtlatl beneath a knee-to-calf tilmatli (cloak carries the hem; loincloth shows only as the belt tail) | Hem band | Low tied tail | Plain cotton headband | S7 | S8 | S2 |
| 8 | Mantle fastened at the front over a wrap skirt | Shoulder border | Low knot, side-tied at the nape | Neck beads | S8 | S1 | S3 |

Roster checks (all pass on paper, to be re-verified by T1 on the real models):
each swatch appears on exactly 3 models (S1: default k=1, swap k=8, trim k=6;
and so on for S2 to S8); the headband appears on k = 1, 4, 7, neck beads on
k = 2, 5, 8, ear spools on k = 2, 3, 6 (each accessory piece on exactly 3
models, satisfying the occurrence rule of 1.3.6, every piece geometrically the
same); the three variants per model are default, palette swap, and accessory
variant (default swatch + trim + accessory set), so 8 x 3 = **24 appearances =
the 24-instance cap** and every appearance is one instance (8.3). With a 6-model
MVP the cut, trim placement and hair columns of rows 1 to 6 apply, the swatch columns are recomputed modulo 6 (MVP roster below), and the floor is 6 x 2 = 12 appearances (no
accessory variant). The sensitivity reader (1.4, 5.8) reviews the cut list
before generation.

**MVP roster (N = 6).** With six base models the 4.3 rule is applied with
indices modulo **6**, so swatches S7 and S8 are inactive and rows 4 to 6 do
**not** keep the swap and trim of the N = 8 table. Cut, trim placement and hair
form are those of rows 1 to 6 above; only the swatch columns are recomputed.

| k | Default S(k) | Palette swap S(k+1) | Trim S(k+3) | Change from the N = 8 table |
| - | ------------ | ------------------- | ----------- | --------------------------- |
| 1 | S1 | S2 | S4 | none |
| 2 | S2 | S3 | S5 | none |
| 3 | S3 | S4 | S6 | none |
| 4 | S4 | S5 | S1 | trim S7 -> S1 |
| 5 | S5 | S6 | S2 | trim S8 -> S2 |
| 6 | S6 | S1 | S3 | swap S7 -> S1, trim S1 -> S3 |

N = 6 checks (on paper, re-verified by T1 on the real models): **each active
swatch appears on exactly 3 distinct models**, once as default, once as swap
and once as trim (S1: default k=1, swap k=6, trim k=4; S2: k=2, 1, 5; S3: k=3,
2, 6; S4: k=4, 3, 1; S5: k=5, 4, 2; S6: k=6, 5, 3), so T1 item 11 holds; no
model's trim equals its own default or swap (k+3 differs from k and k+1
modulo 6); S7 and S8 appear on no model and no material instance references
them. The MVP ships two variants per model (default and palette swap), so
6 x 2 = **12 appearances**; the accessory column of rows 1 to 6 is inactive, no
accessory is present on any MVP model, and the 1.3.6 occurrence rule holds
vacuously. **The N = 8 accessory deal must not be reused at N = 6:** rows 1 to
6 alone carry the headband on 2 models (k = 1, 4) and the neck beads on 2
(k = 2, 5), which fails the occurrence rule. If an accessory variant is added
at N = 6, the accessory sets are re-dealt so each piece sits on exactly 3
models (proposed: headband on k = 1, 3, 5; ear spools on k = 2, 4, 6; neck beads
unused) and the art-director re-signs this block. When the pool grows to 7 or
8 models, the swatch columns are recomputed modulo the new N (the N = 8 table
above is that result at 8).

### 1.4 Cultural framing flag

The Mexica (Aztec) built an imperial, urban civilisation of canals,
causeways, markets, planned districts and a bureaucratic state. Smoking Mirror
is *inspired by* Tenochtitlan, not a documentary reconstruction, and it must
read as the city of a sophisticated state.

- Avoid "savage," "tribal" and jungle-ruin tropes, and avoid sacrifice
  spectacle: no gore, blood-soaked temples, skull-pile set dressing as
  atmosphere, or ritual violence as a visual hook. The game's violence is a
  quiet assassination in a crowd, and the visual identity must not borrow
  horror-ritual imagery to dress it up.
- Ground references in primary and scholarly sources: the Codex Mendoza,
  the Florentine Codex, archaeology (Templo Mayor excavations) and museum
  collections (Museo Nacional de Antropologia, the Templo Mayor Museum).
  Note that the surviving codices were produced under Spanish colonial
  supervision and carry that bias; treat them as sources to be read critically.
- Keep the citizens plain and the divine/royal visual vocabulary off the
  crowd. This is both a uniformity requirement (1.3.6) and a respect
  requirement: deities, rank regalia and ritual costume are not costumes.
  Tezcatlipoca (the title's reference) is used as a motif and name, not a
  character.
- A sensitivity read by a qualified reviewer is recommended before the vertical
  slice, as noted in the concept document.

## 2. Mood & Atmosphere

### 2.1 The mood, and the one law that protects it

"Held Breath" is the feeling of a hot, ordered city at the exact moment
nothing is happening and everyone is aware that something might. It is civic
calm with attention underneath it: sunlit, still, rhythmic, and quietly
watched. The mood is deliberately *not* dread, gloom or ritual menace
(Section 1.4 forbids borrowing horror-ritual imagery), because the game's
tension is supposed to come from the player's own suspicion of a stranger, not
from the set telling them to be afraid. A frightening world would also darken
and busy the picture, and darkness hides tells (Principle 2).

The mood is carried by four things the art can control: **warm light with cool
shadow**, **low-to-mid contrast**, **a single shared value mass for the
crowd** (Section 4), and **motion that is slow and periodic** (Principle 1).

#### The Lighting Law

This is the single rule that lets nine different moods coexist with a game
whose first pillar is that nobody may be told apart by anything they did not
choose.

1. **The world is lit once.** From round start until the instant the round
   ends, the sun direction, sun and sky colour temperature, sky, fog, haze,
   shadow settings and exposure are identical on every client, in every game
   state, and do not animate. The level's static light pools (the staged
   objective pools in 2.5) are part of that fixed lighting; they exist whether
   or not anyone stands in them.
2. **State mood is a per-viewer, screen-space grade.** Every state below that
   changes how a viewer's frame looks does so with a full-frame grade applied
   after the scene is rendered, identically to every pixel. There is **no
   per-actor, per-material, stencil or custom-depth treatment of any
   character**, ever. Outlines, tints and glows on a figure are identification
   aids (anti-pillar) and a Pillar 1 failure.
3. **The grade has a budget.** A *sustained* grade (held for more than 2 s)
   may move the frame by at most: **colour temperature +/- 1,000 K** from the
   baseline, **saturation -30 percent to +10 percent**, **contrast +/- 20
   percent**, **vignette 30 percent** at the corners. A *transient* grade (2 s
   or less, the assassination beat in 2.7) may reach **saturation -60 percent**,
   **+/- 900 K** and **contrast +/- 30 percent**. No grade may flash: luminance
   may dip and recover but never strobe, and nothing may exceed 3 luminance
   reversals per second.

   **Contrast is a post-process scalar, not the world ratio.** In this section
   "contrast" with a percentage means the slope of the tone curve about the
   18-percent-grey pivot, applied to the finished frame (0 percent = neutral,
   +20 percent = 1.2x slope). It is a property of the viewer's grade and is
   budgeted here. The **world sun-to-shade ratio** ("3 : 1", "5 : 1") is a
   property of the lit world: it is set by geometry, albedo and the single sun
   (Law 1), is identical for every viewer, and is **never** changed by a grade.
   The two are tracked in separate columns in 2.11 and are not interchangeable.
   **Exposure is not a Law 3 axis** (Law 4 governs it); the briefing ramp in 2.3
   is the single authored exposure ramp and is explicitly exempt from Law 3's
   budget on that axis, while its contrast and vignette stay inside the budget.
4. **Exposure is fixed.** No eye adaptation. Adaptation is a slow, uneven
   brightness drift that is different on every screen and in every viewing
   direction, which would read as the world breathing and mask or mimic a
   tell. Any exposure motion is authored (the briefing ramp in 2.3), never
   automatic. The implementation is the technical-artist's; the requirement is
   that adaptation range is held to +/- 0.3 EV or less.
5. **Two exceptions, both because the information no longer matters.** The
   lobby is a separate space with its own lighting (2.2). The round-end state
   (2.10) may change the global light and its grade may exceed the Law 3
   budget, because the match is over and nothing can leak (the Defeat reveal,
   +20 percent contrast scalar with a world ratio eased to 5 : 1, is the
   largest combination).

*Alternatives considered for how states get their mood:*

- **Change the world lighting per state (golden hour for victory-to-be, storm
  light for danger).** Rejected during play. A world-wide change is visible to
  everyone, so it announces events at a location and it alters the contrast the
  crowd is read against. It is acceptable only at round end.
- **Per-character effects (a glow on a hider, a tint on an informant).**
  Rejected outright; see rule 2.
- **A per-viewer grade with no budget.** Rejected: unbounded grading becomes
  the main look of the game and erodes the baseline the tells are read against.

#### Baseline lighting (the "exploration" values every other state is measured from)

| Property | Value | Rationale |
| -------- | ----- | --------- |
| Time of day | Solar noon plus about 30 minutes; sun elevation **65 degrees +/- 3** | Short, soft-edged shadows (length about 0.45 to 0.55 of object height) that give form without throwing long diagonals across the plaza. Long shadows read as moving lines of motion in a still crowd. |
| Sun | Directional, **5,600 K**, the **only** sun in the world. **Azimuth is a project constant: 45 degrees from the +X axis of the 2.0 m plan grid** (working value, open item OI-4 to confirm in the graybox), identical in every seed, level, round and client; the generator never rotates the sun or the grid | Neutral-warm. The warmth comes from the albedos (Section 4), not from tinting the light. A 45-degree azimuth gives every street orientation a cornice shade band and leaves no street running parallel to the sun. Because the grid is axis-aligned, a fixed azimuth makes sun direction a fixed function of layout (the generator's shadow-trace check in 6.8 relies on this). |
| Sky and ambient fill | **9,000 K** | Creates the cool-shadow half of "warm light, cool shadow" without making shade dark. |
| World sun to open-shade ratio | **3 : 1** (about 1.6 stops), a property of the lit world, not a grade (Law 3) | Low-to-mid contrast (Principle 2). Shade is bright enough that a figure standing in it keeps its limb read. |
| Shadow edge | Penumbra about **25 to 40 cm** at crowd height | Crisp enough to model form, soft enough that a shadow edge across a figure is not a hard value cut that looks like a pattern. |
| Atmosphere | Dry haze: **6 percent** aerial perspective at 30 m, **12 percent** at 60 m, tinted 9,000 K | Depth separation at the 20 to 30 m reading range without softening the crowd. |
| Heat shimmer / distortion | **None** | Any refractive wobble in the eye band is motion that could be mistaken for a human tell (Principle 1). |
| Exposure | Fixed (see Lighting Law 4) | |

### 2.2 Lobby and menus

- **Emotional target:** composed anticipation. The player is being seated, not
  hurried. The feeling is "I am about to be someone else for twelve minutes."
- **Lighting character:** a shaded calpulli courtyard in late morning. A single
  shaft of sun at about **4,800 K** enters through a roof opening; the room is
  filled by **7,800 K** bounce from limewashed walls. Sun-to-fill about
  **2.5 : 1**, deliberately the lowest sun-to-fill ratio of any state before
  the round ends (the Victory reveal, 2.10, ties it at 2.5 : 1 only after the
  round is decided) so that UI text and the player roster read cleanly over
  it. The lobby is its own space (Law 5) and carries no contrast scalar.
  Saturation about 5 percent below baseline.
- **Descriptors:** hushed, shaded, composed, unhurried, ceremonial-but-civic.
- **Energy level:** low. Nothing moves faster than a loop period of 10 s.
- **The element that carries the mood:** a polished **obsidian mirror disc**
  (about 1.2 m across) set into the courtyard floor or wall, holding a darkened
  reflection of the sky and a slow drifting wisp of smoke (loop period 10 s or
  longer, eased). It is the only place in the game where the title's symbolism
  is the focal image. It is a lobby-only motif; obsidian mirrors in the match
  are environment dressing and never a landmark (Section 1.3.4 reserves
  obsidian for the environment).
- **Why it is distinct:** the only state with a single directional shaft in an
  enclosed space, the warmest key light of any state before the round ends
  (4,800 K; the Victory reveal is warmer at 4,200 K, but only after the round
  is decided) and the lowest sun-to-fill ratio before round end.

### 2.3 Round start and briefing

- **Emotional target:** the inhale. Clear-headed readiness: "I know what I am
  doing; now I have to be unnoticeable."
- **Lighting character:** the baseline itself (midday, **5,600 K** sun, 3 : 1)
  reached by an authored four-second ramp rather than a cut. The grade starts
  at **-0.5 EV** (the one authored exposure ramp, explicitly exempt from the
  Law 3 budget on the exposure axis, as Law 4 allows), contrast **-20 percent**
  (the post-process scalar at the budget floor; the world stays 3 : 1),
  vignette **12 percent**, and eases to baseline over **4 s** with a
  smooth-step curve. The in-world primer card
  ("Walk like them. Watch for who doesn't.") sits on a Bone plate (Section 4.6).
- **Descriptors:** poised, level, ceremonial, expectant, clear.
- **Energy level:** low, rising to baseline.
- **The element that carries the mood:** the **exposure ease-in itself**, the
  visual equivalent of drawing breath. The crowd is already walking at its
  normal pace under the ramp (a frozen crowd at round start would make the
  first moving figure a tell).
- **Why it is distinct:** it is the only state defined by change over time,
  not by a stable look, and the only one that starts under baseline brightness.

### 2.4 Exploration and blending (the dominant state)

This is where players spend most of the match. Every other state is a
departure from it and returns to it.

- **Emotional target:** watchful patience: relaxed shoulders, focused eyes.
  The player should feel *camouflaged by calm* and see a world that offers no
  help.
- **Lighting character:** the baseline: **5,600 K** sun at 65 degrees, **9,000
  K** sky, **3 : 1**, no grade, no vignette. Contrast is low-to-mid and the
  scene's overall value sits at mid-to-light, with the crowd at the shared
  mid value (Section 4.3).
- **Descriptors:** sunlit, ordered, still, rhythmic, dry-warm.
- **Energy level:** low-medium and steady. The only sustained change in the
  frame is the crowd.
- **The element that carries the mood:** the **grid of sun and shade** laid
  across the plazas by parapets and awnings. It is static, regular and
  horizontal, so it reads as the city's pulse of stillness, and every figure
  that crosses it does so at the shared crowd pace. Against that grid a
  deviation (a sudden stop, a run) is the loudest thing in the frame.
- **Why it is distinct:** it is the control. Every other state is defined by
  what it adds to or subtracts from this one.

### 2.5 Objective interaction (exposure)

- **Emotional target:** deliberate exposure. The player has chosen to become
  readable and should feel it: "everyone could be looking at me and I cannot
  tell."
- **Lighting character:** the objective site is a *staged pool of direct sun*
  inside a shaded precinct. The pool is made **only by geometry cutting the
  project's single sun** (the court's parapet and cornice, 6.8); there is no
  separate light, light function or per-site lamp (implementation path, 4.5).
  Measured outcome, not an input: pool-to-surround illuminance ratio **5 : 1
  +/- 0.5**, the highest local contrast in the match, enforced per generated
  layout by the shadow-trace check in 6.8. The pool reads at the sun's own
  5,600 K; the surround, lit by sky and bounce, measures about 7,500 K (a
  measured outcome). The pool is hard-edged (penumbra 10 to
  15 cm). It is part of the level lighting (Lighting Law 1), so it is present
  and identical whether the site is in use or not, and for every team: the
  same pool serves a team's own stage or the shared final stage. The
  interacting player receives only a light grade: vignette **8 percent**,
  contrast **+5 percent**, eased over 0.4 s.
- **Descriptors:** exposed, pinned, bright, hushed, held.
- **Energy level:** medium. High tension, low motion.
- **The element that carries the mood:** **the sun pool**, a hard-edged
  rectangle or arc of direct light on stone. Whoever stands in it is rim-lit
  and silhouetted against shade for the watching players. This puts Pillar 2
  into the lighting: the site is where a behaviour is easiest to read.
- **Why it is distinct:** the only place where local contrast rises above the
  baseline and where light, not colour, marks a location. It also keeps
  objectives from needing a colour that belongs to a team or role.
- **Flag:** the pool should be discoverable by architecture first (Section 6)
  and by light second. A bright pool in a dark precinct is a long-range
  beacon for *finding* sites; its size and the surrounding shade depth are
  playtest-tuning values. If playtests show sites are found by light alone from
  more than 40 m, soften the ratio toward 4 : 1. **Contract form (reconciled
  2026-10-24, LSC amendment):** the pool ratio is **data**, a lighting value
  owned by art (`SunPoolRatioMult`, default 5 : 1, lever toward 4 : 1), not a
  hard-coded number; the 40 m check is the Level Socket Contract predicate
  **V-POOL-40** (Warn in MVP, AC-48), and its first lever is this ratio.

### 2.6 Pursuit and suspicion

- **Emotional target:** narrowing. "I am being looked at, or I think I am."
  Tight, dry, a held pulse.
- **Lighting character:** a per-viewer grade only; the world does not change.
  Effective temperature **6,000 K** (+400 K), saturation **-15 percent**,
  contrast **+15 percent**, vignette **18 percent**. Eased in over **1.5 s**,
  out over **3 s** so it does not pulse. No hue shift, and in particular **no
  red**: cochineal means finality (Section 4.4), and suspicion is not yet
  finality.
- **Descriptors:** narrowing, dry, taut, quiet, cool-edged.
- **Energy level:** medium-high.
- **The element that carries the mood:** the **closing peripheral vignette**,
  which removes the periphery of the frame (where the crowd is read) at the
  moment the player most needs it.
- **Flags:**
  1. The signal that triggers this state is a gameplay decision. Whatever it
     is, it must never indicate *which* person is the pursuer or the suspect
     (anti-pillar: not an identification-tools game; Pillar 1). If the game
     design provides no viewer-facing pursuit or suspicion signal, this state
     is not built and its grade budget is simply unused.
  2. The grade is on the viewer's frame only. No other client renders it.
- **Why it is distinct:** the only sustained state that reduces the frame's
  usable width and the only one that shifts cooler and more contrasty without
  a hue.

### 2.7 Assassination and reversal moment

- **Emotional target:** finality. The single decisive beat the whole round
  builds toward.
- **Lighting character:** a **transient** grade for the *two participants only*
  (attacker and victim, or defender and attacker in a reversal): effective
  **6,500 K** (+900 K), saturation **-60 percent**, contrast **+25 percent**,
  vignette **25 percent**. Timing: snap in over **0.12 s** (about 7 frames at
  60 fps), hold **0.20 s**, ease out over **0.80 s**; **1.1 s** total. It is
  a post-process beat on the participants' cameras. **It starts at the local
  contact frame, never before** (no participant's frame changes during the
  lunge), and **a whiff (no contact) gets no grade**: the grade means "a body
  met a body" (Assassination VA-ASN-4, reconciled 2026-10-24). It must not time-dilate or
  hold the simulation: a hit-stop that freezes the world would delay the
  round's state and, for a player, would differ from an NPC (Pillar 1).
  Bystanders see only the world animation, which is already
  "visibly dramatic" per the concept document.
  A reversal uses the identical grade, so a reversal cannot be told from a
  kill by the look of the frame alone; the outcome is shown by the elimination
  result (cochineal finality card, Section 4.4) for the player who lost.
- **Descriptors:** stark, drained, decisive, silent, held.
- **Energy level:** peak, but short: the highest contrast and lowest
  saturation in the game, for about one second.
- **The element that carries the mood:** the **drained, held frame**: colour
  leaves the picture, leaving the figures and the stone as pure value, which is
  exactly the shape-and-value language of Principle 2.
- **Why it is distinct:** the only state with a desaturation below -30 percent
  and the only one lasting about one second.

### 2.8 Concealment in a bush

- **Emotional target:** shelter and intimacy: a pause with the city still
  visible through leaves, not safety. Pillar 4 and the "not a hide-and-wait
  game" anti-pillar mean the look must feel temporary.
- **Lighting character:** a per-viewer grade for the hider only. Effective
  **5,000 K** (-600 K) with a slight green tint, saturation **-10 percent**,
  contrast **-10 percent**, a leaf-edged vignette of **30 percent** (the
  maximum allowed). Eased in over 0.6 s and out over 0.4 s. The bush itself
  renders the same for everyone (below).
- **Descriptors:** dappled, close, green-shaded, muted, brief.
- **Energy level:** low, with held attention.
- **The element that carries the mood:** the **leaf-edge frame**: the bush's
  own tiered leaf silhouette (3.6) rimming the viewer's frame, so the hider
  literally sees the city through the object they chose.
- **Per-viewer legibility rules:**
  1. The bush mesh, material and lighting are identical for all viewers
     whether it is occupied or empty. There is **no persistent colour, light,
     value or motion difference** between an occupied and an empty bush. Any
     rustle on entry or exit is a single short authored motion defined by the
     concealment GDD and counts as the visible tell of the action (Pillar 2);
     it is the action that is the tell, not the bush's state afterwards.
  2. Whatever the concealment GDD decides about what allied viewers may see,
     it is shown by the teammate marker (Section 3.4, Section 4.6) and never by
     tinting or revealing the character inside the foliage.
  3. An enemy or neutral viewer sees only a bush.
- **Why it is distinct:** the only state tinted green-warm (-600 K) instead of
  cool, and the only one whose vignette has a leaf shape.

### 2.9 Informant state

Players who are eliminated or removed continue as informants. This is a long
state, often several minutes, so the grade must be comfortable to look at.

- **Emotional target:** the patient observer, detached and useful. It should
  feel different from being alive, not like punishment (Pillar 3: a meaningful
  reduced role).
- **Lighting character:** per-viewer grade for the informant only. Effective
  **6,600 K** (+1,000 K, the sustained maximum), saturation **-30 percent** (the
  sustained maximum), contrast **-5 percent**, **no vignette** (the periphery
  is exactly what an informant needs to read). A 2 px Bone corner bracket
  frame (Section 4.6) states the mode in the HUD without colour. **The grade
  is on from the cut frame** of the informant's view (the first frame on the
  new body or on the neutral view), **never eased in** (Penalty VA-NKP-4,
  Camera CS-R12.2; reconciled 2026-10-24).
- **Descriptors:** cool, quiet, translucent, attentive, level.
- **Energy level:** low.
- **The element that carries the mood:** the **desaturation**: the same city
  with the warmth lifted out. It is a soft grade of the whole frame, and the
  informant's value read is unchanged, so crowd reading keeps its full quality.
- **Per-viewer legibility rules (informant view must not identify enemies):**
  1. The informant's frame is graded uniformly (Lighting Law 2). Characters are
     not tinted, outlined or revealed by team.
  2. Sightings are drawn as **place-based pings**, never person-based ones: an
     open broken ring (Section 3.4) in Marigold on the ground at the position
     where a sighting occurred. Parameters (7.3 row 9): world radius **>= 4 m**
     (cluster scale, never body scale), screen-space minimum **32 px**, static
     after placement, **no centre mark**, mandatory Obsidian keyline (3 px, 4 px
     beyond 20 m), **age shown by stepped stroke weight** (heavy, medium, light
     at thirds of its life), never by opacity, and a hard design-set expiry
     (start 8 s). A ping does not attach to a figure and does not follow it.
  3. Allies appear only through the standard teammate marker, each shown in
     **that ally's own state** (filled = active, hatched = concealed,
     outline-only = informant). The informant's own marker, as every ally sees
     it, is the outline-only form; the informant viewer does not see a
     different encoding. Enemies and NPCs appear as citizens.
  4. Section 2.5's interaction grade is not applied to the prepare hold
     (7.3 note e).
  5. The informant's own body is a normal pool model; informant status is not
     visible to enemy viewers by the body's look.
- **Why it is distinct:** the only sustained state with a large saturation drop
  and a cool temperature and no vignette. It differs from baseline chiefly in
  colour (temperature and saturation), so it is verified in the colour pass of
  the 2.11 test, not the greyscale pass.

### 2.10 Round end: victory and defeat reveal

Round end releases the held breath. Because the match is over, this is the
only in-match state allowed to change the global light (Lighting Law 5), and
the reveal (who was who) is allowed to name every player.

**Shared sequence (about 3 s):** the crowd keeps walking; the grade eases over
3 s; then the reveal overlay appears, showing every player's marker, name and
the round's key events. All characters keep their normal pool models. The
reveal is a UI layer, never a model change.

| | **Victory** | **Defeat** |
| - | ----------- | ---------- |
| **Emotional target** | Release; the satisfying exhale of a read that paid off or a race won | Acceptance with curiosity: "who was that?"; sting, not shame |
| **Lighting** | Sun eases from 65 to **45 degrees** over 3 s at **4,200 K**, saturation **+8 percent**, contrast **-10 percent**, sun-to-shade eases to **2.5 : 1**, no vignette | Sun stays at 65 degrees; effective **6,200 K**; saturation **-20 percent**; contrast **+20 percent** (ratio eases to **5 : 1** with cool fill); vignette **10 percent**. The city does not reward the loser with warm light |
| **Descriptors** | golden, long-shadowed, open, warm, released | flat, cool, hard, indifferent, clear |
| **Energy** | Medium, falling | Medium-low, steady |
| **Mood carrier** | The **lengthening, warming shadows** across the plaza | The **even cool light** and a single cochineal header bar (Section 4.4: finality) |
| **Colour rule** | No cochineal anywhere. Turquoise appears only on ally markers | Cochineal appears only on the one finality header and strike/elimination pips |

The reveal overlay uses the same marker shape for everyone (3.4); allies are
turquoise, opponents are Bone-outline on Obsidian (Section 4.6), so team
identity is stated by shape fill and name label as well as colour.

### 2.11 Distinctness matrix

Each state differs from baseline by at least two independent axes. Two axes
are **world properties** (the sun-to-shade ratio and the lit scene) and four
are **viewer-grade properties** (effective temperature, contrast scalar,
saturation, vignette); the world ratio and the contrast scalar are separate
columns because they are separate things (Lighting Law 3).

| State | Effective temp | World sun : shade | Contrast scalar (post-process) | Saturation | Vignette | Energy | Scope of look |
| ----- | -------------- | ----------------- | ------------------------------ | ---------- | -------- | ------ | ------------- |
| Lobby | 4,800 K shaft / 7,800 K fill | 2.5 : 1 (own space) | none | -5 % | none | Low | Own space |
| Briefing | 5,600 K | 3 : 1 | -20 % to 0 over 4 s (plus the exempt -0.5 EV ramp, 2.3) | 0 | 12 % to 0 | Low rising | Viewer grade |
| **Exploration** | **5,600 K** | **3 : 1** | **0** | **0** | **none** | **Low-medium** | **World (baseline)** |
| Objective site | 5,600 K sun in the pool, about 7,500 K measured surround | 5 : 1 +/- 0.5 pool to surround (measured, geometry-made) | +5 % (actor only) | 0 | 8 % (actor only) | Medium | World light pool plus light actor grade |
| Pursuit / suspicion | 6,000 K | 3 : 1 | +15 % | -15 % | 18 % | Medium-high | Viewer grade |
| Assassination / reversal | 6,500 K | 3 : 1 | +25 % (transient) | -60 % (transient) | 25 % | Peak (1.1 s) | Participants' grade |
| Concealment | 5,000 K plus green | 3 : 1 | -10 % | -10 % | 30 % (leaf) | Low, held | Hider's grade |
| Informant | 6,600 K | 3 : 1 | -5 % | -30 % | none | Low | Viewer grade |
| Victory | 4,200 K | eases to 2.5 : 1 | -10 % | +8 % | none | Medium, falling | World (post-round) |
| Defeat | 6,200 K | eases to 5 : 1 | +20 % | -20 % | 10 % | Medium-low | World (post-round) |

**The distinctness test runs in two passes, because a greyscale image cannot
carry temperature or saturation.**

1. **Colour pass (temperature and saturation axes).** A grid of 10 randomised
   colour captures at 320 px wide; a tester names the state of 9 of 10. All ten
   states are tested.
2. **Greyscale pass (luminance axes: contrast scalar, vignette, world ratio,
   exposure).** Greyscale captures at 320 px wide, shown as A/B pairs against
   the baseline; for each state that differs from baseline by at least 10
   percent on a luminance axis (Briefing, Objective site, Pursuit, Assassination,
   Concealment, Victory, Defeat) the tester picks the non-baseline frame in 9
   of 10 pairs. Lobby and Informant are excluded from this pass: they differ
   chiefly in colour (Informant's -5 percent contrast scalar is below the 10
   percent threshold) and are covered by the colour pass.

### 2.12 Rules and tests

- **Single-source check:** a capture of the same frame from two clients in
  different states, with the grade disabled, must be pixel-identical in the
  world layer. If it is not, the Lighting Law has been broken.
- **Distinctness test:** the two-pass test (colour, then greyscale) described
  in 2.11.
- **HUD leak tests (added from 7.7):** (1) strip test; (2) packet/memory audit;
  (3) parity grep for per-figure widgets; (4) target-parity test for the attack
  input; (5) dual-edge contrast test on the brightest and darkest ground. The
  procedures and pass lines are in 7.7.
- **Budget check:** every grade preset is stored as data (not hard-coded) and
  validated against the budget in Lighting Law 3 by a script; presets outside
  the budget fail the build.
- **Photosensitivity check:** no preset produces more than 3 luminance
  reversals per second; the assassination beat is reviewed on a capture at
  60 fps.

*Alternatives considered for the overall mood:*

- **"Obsidian Hour" (dusk, torchlight).** The strongest mood and the strongest
  cultural drama; rejected for the baseline because darkness lowers the
  readability of tells. It remains the concept document's possible later
  time-of-day variant, and the state grammar above would carry over unchanged
  if it is added.
- **Hard-contrast noon (sun-to-shade 8 : 1).** More dramatic photographically;
  rejected because it breaks Principle 2 and puts figures in the shade into a
  value hole where the limb read is lost.
- **A cooler, neutral baseline (6,500 K sun).** Rejected: the warmth is the
  "hot, ordered, sunlit civic calm" of the chosen direction and is also the
  principal way the setting reads as Mexica rather than generic ancient city.

*Trade-offs flagged:*

- **Mood versus Pillar 1.** Every per-viewer grade is a legitimate local
  presentation, but it is also a risk: a captured frame from a player in a
  state is information about that player. The rule that the grade is local,
  uniform and never replicated protects this, and the technical-director should
  confirm no grade parameter is networked.
- **Informant desaturation versus Sensation (aesthetic #4).** A long
  desaturated state is a real cost to the experience of the setting. The grade
  is limited to -30 percent for this reason and should be reviewed after
  playtests.

## 3. Shape Language

### 3.1 The governing idea: an upright minority in a horizontal majority

The shape language is one contrast, applied everywhere: **the city is broad,
horizontal and rectilinear; the people are the only compact verticals.**

In Gestalt terms this is figure-ground separation by *orientation contrast*
(a vertical element in a field of horizontals is perceived as the figure) and
grouping by *similarity* (identical figures read as one crowd, so the figure
that moves differently is the one that breaks the group). It serves the visual
rule directly: *the city holds still* (horizontals and repeated masses are the
visual language of stillness) *and the crowd shares one body* (one silhouette
recipe), *so only behaviour can speak*. It serves Pillar 2 (a tell is only a
tell against a quiet baseline) and Pillar 1 (no shape-based identity).

Every shape rule below is tested against two questions drawn from Section 1:
does it let a viewer tell people apart by anything they did not choose (it
must not), and does it make a chosen action easier to see against a calm
background (it should).

### 3.2 Character silhouette philosophy

#### One recipe, zero archetypes

The game has no character archetypes and no per-role silhouettes. Where a
typical shape-language section assigns one distinguishing trait per archetype,
this one assigns **none**: **the only things that may distinguish one figure
from another are behavioural**: gait cadence, head-turn timing, pausing, a
run, a crouch, an interaction. The silhouette is not a costume; it is the
instrument the behaviour is read on.

Every pool model is built to a single silhouette recipe (the numbers are
inherited from Section 1.3.2 and 1.3.3; this section gives the shape logic):

| Part | Shape | Rule |
| ---- | ----- | ---- |
| Head | A near-circle, **22 cm** wide, 23 cm high, clearly separated from the shoulders by a visible neck | The head is the primary tell instrument (head turns). It must read as its own circle. Garment necklines sit at least **6 cm** below the chin; no hoods, high collars or capes that merge head and shoulders. |
| Torso and garment | A shoulder-weighted trapezoid: **54 cm** at the cloak line (100 to 140 cm height) tapering to **44 cm +/- 4** at the hem | A single mass of one value (Section 4.3). The taper makes the legs read as separate from the torso and keeps the weight at the top, where heading is read. |
| Legs | Two parallel columns visible below the hem, **55 cm +/- 8** long | The gait is the second tell instrument. The negative space between the legs must open and close through the stride; hems never cover the knee-to-ankle line. |
| Arms | Two free columns hanging against the torso; forearms and hands always visible, never wrapped in garment | Arms are the third tell instrument (interaction, brace, run). A garment that covers a forearm hides a tell. |
| Hair and headwear | A back-weighted mass (below) rising no more than **4 cm** (hair) or **7 cm** (headwear) above the skull | Gives the head direction. |

**The heading rule.** Every model's head-plus-hair silhouette must be
**front-to-back asymmetric by the same ratio**: at least **60 percent of the
hair or head-covering volume sits behind the ear plane** (a back knot, tied
hair, or a head-wrap knotted at the nape). Why: at 30 m a bare circle has no
direction, so a head turn is invisible; with a consistent back mass the head
reads as a pointer, and *every model's* turn reads identically. Variation in
hair shape is permitted by Section 1.3.1, but only inside this ratio, and any
variant that breaks the silhouette IoU test (>= 0.90, 1.3.3) fails.

**The negative-space rule.** In the walk loop at its widest swing, the
silhouette must show: a gap between the legs of at least **8 percent of
stature (14 cm)** at mid-stride, and a visible gap between the swinging arm
and the garment line on at least one side. This is what makes cadence and arm
tells legible at 55 px tall.

#### Thumbnail readability

At the 60 degree vertical field of view at 1080p (confirmed by the Camera
System GDD, 2026-10-24):

| Distance | Figure height | What must still be readable |
| -------- | ------------- | --------------------------- |
| 10 m | about 165 px | Head turns, hand position, face-toward or away |
| 20 m | about 82 px | Head direction (via the heading rule), leg opening, arm swing, stop versus walk |
| 30 m | about 55 px | A figure's presence, heading, and walk-versus-stop-versus-run. The head is about 7 px wide, the garment about 17 px wide, and each leg about 4 px |

The Principle 2 test (>= 90 percent figure count, >= 85 percent heading, 20
and 30 m, greyscale) is the acceptance test for this section as well.

#### What each behaviour must look like in silhouette

| Behaviour (the tell) | Silhouette signature | What the shape must guarantee |
| -------------------- | -------------------- | ----------------------------- |
| Head turn | Head circle rotates against a still torso; the back mass swings to the side | Visible neck gap; back-weighted head (heading rule) |
| Walk at crowd pace | Alternating leg gap of about 14 cm at mid-stride, small arm swing | Hem 55 cm +/- 8 above ground; legs uncovered |
| Sudden stop | The silhouette freezes in mid-stride, legs apart | No cloth simulation or secondary motion (1.3.3), so a stop is a *clean* stop |
| Run | Forward lean of the torso (side depth up to **34 cm**), bent arms, wide leg gap | Side-view envelope in 1.3.3; free arms |
| Brace (no crouch exists; SCL R10 removes the sample's crouch) | The figure compresses to about 60 percent of stature; the head drops into the garment line | Head circle remains separable from the garment line after compression |
| Interaction (reach, kneel) | One arm leaves the torso line | Free arms |

#### What the silhouettes must never do

- Carry an object, a feather, a weapon or a regalia piece (Section 1.3.6).
- Show a different mass, height, hem or head shape from the rest of the pool
  (Section 1.3.2 to 1.3.3).
- Include pattern with high contrast that cuts the torso into bands at crowd
  height (it reads as a role badge, and it competes with the silhouette).
- Be recognisable as a role by outline. The T3 silhouette test (1.3.7) applies.

*Alternatives considered:*

- **Distinct silhouettes by social class or trade (the codices show this
  richly).** Rejected: every one is a role signal, and Pillar 1 forbids it.
  The ornament lives in the environment (Section 6).
- **A more naturalistic crowd with varied builds.** Rejected for the reason
  already recorded in 1.3.1: any build spread is an unchosen tell.
- **A stylised, flat, tiny-head figure for maximum thumbnail read.** Rejected:
  it would fight the stylised-realistic direction and read as a UI element in
  the crowd.

### 3.3 Environment geometry

Environment shapes are **broad, horizontal, rectilinear, rhythmic and tiered**.
This is the Held Breath rule applied to architecture: horizontal mass says
"still," and repeated modules say "baseline." The modular kit is a natural
fit, and the repetition is deliberate (concept document).

#### Proportion and angle vocabulary

| Rule | Value | Why |
| ---- | ----- | --- |
| Building mass proportion | Width to height **>= 1.5 : 1** for every building. Temple platforms may be **1.25 : 1** | Horizontal dominance: at least about 85 percent of skyline edge length is horizontal |
| Permitted structural angles | **0 degrees** (horizontal), **90 degrees** (vertical), the **talud batter of 14 degrees from vertical** (1 : 4), and **stairs** at the fixed ratios below. No other structural diagonals | A small angle vocabulary is itself a repetition. Sloped talud over vertical tablero is the characteristic section of monumental Mexica masonry, so the rule is cultural as well as geometric |
| Civic stair | **Nominal** rise **0.17 m**, run **0.30 m** (about 30 degrees). Built rise is solved as 1.6 m / n: 9 risers of 0.178 m (within 5 percent of nominal) | Walkable, readable as a gentle step |
| Temple stair | **Nominal** rise **0.22 m**, run **0.26 m** (about 40 degrees). Built rise: 7 risers of 0.229 m (within 5 percent of nominal; about 41 degrees) | Reads as steeper and more formal |
| Roofs | Flat with a **0.5 m** parapet. No gables, domes or pitched roofs | Keeps the skyline horizontal. Chimney stacks and pitched shapes are not part of the vocabulary |
| Openings | Rectangular, taller than wide (roughly 1 : 1.6), in a regular rhythm, with at least **1 m** clear spacing | Doorways are the only repeated verticals in a wall; their width (>= 0.9 m) keeps them from reading as a figure (static-figure rule below) |
| Curves | Only in the plan of canals and causeway edges, in water, in ceramics and in foliage. **No arched, domed or curvilinear architecture** | Round forms in the built world would compete with the one round form we care about, the head |

#### The vertical modules (all dimensions are the kit contract)

- **Plan grid:** **2.0 m**, with a 0.4 m sub-grid for trims.
- **Vertical course:** **1.6 m**. One storey is **3.2 m** (2 courses); a
  talud is 1 course; a temple tier is 1 to 2 courses.
- **Street-wall banding (the "heads against light, bodies against dark"
  rule):** on any wall that encloses a walkable space, the first course
  (**0 to 1.6 m**) is a dark stone dado in Tezontle Red-Brown (Section 4.2,
  **L* 30 to 33**), and everything above 1.6 m is Limewash Bone (L* about 80).
  The crowd's garments sit at a shared L* 61 (Section 4.3), so every body reads
  lighter than the dado and every head, with its dark hair, reads against the
  brightest surface in the frame. The dado is also at least 10 L* from the
  skin mean (L* 48), so bare legs and forearms read against it (Law 3b, 4.1).
  This puts the highest-contrast edge of the environment on the head turn
  (Pillar 2).
- **Parapet and cornice line:** a single horizontal at **3.2 m** or **6.4 m**,
  with at least **40 cm** of projection so it throws a constant shade band
  (the sun/shade grid of Section 2.4).
- **Projection convention (kit contract):** a module's **bounding box and
  pivot refer to the unprojected cell**. The 0.4 m talud batter and the 0.4 m
  cornice projection lie *outside* the cell's footprint and are excluded from
  the "multiple of 2.0 m" bounding-box rule; the pivot is the minimum corner of
  the unprojected cell. No projection exceeds 0.4 m (one sub-grid step). Coping,
  quoins and pilasters are **structural elements**, not trims, with fixed
  dimensions (6.2).

#### Tiering

Monumental structures are built from stacked platforms, each tier setting back
by at least **1.6 m**. The temple precinct is the only place where tiers rise
above 6.4 m (to a maximum of about 24 m); elsewhere the skyline stays flat. The
stepped silhouette of the temples is the only large diagonal-feeling shape in
the game and is therefore the long-distance landmark (3.5).

#### The static-figure rule

A motionless upright in the walkable world is read as a person standing still
(a tell: Pillar 2) or as an NPC loitering. So no fixed prop in the walkable
eye band (0.5 m to 2.2 m) may look like a person:

- Any upright with **height 1.5 to 2.2 m, width 0.35 to 0.6 m, and an aspect
  ratio between 1 : 2.5 and 1 : 5** is forbidden unless it is **>= 0.9 m wide**
  (architectural), **>= 2.6 m tall** (a post or column reads as architecture),
  or **placed in a regular rhythm** at a fixed spacing of at least 3 m with
  identical form (a colonnade is a pattern, not a person).
- No statues or stelae at human scale and human proportions in open walkable
  space. Statuary is allowed at monument scale (>= 2.6 m) or inside niches
  (recessed at least 0.4 m).

Test: a greyscale 30 m capture of every kit piece in context; a tester must
not mistake any prop for a person in more than 5 percent of trials.

#### Reading corridors

Sightlines of **20 to 30 m** are the game's reading range (Principle 2). The
kit's standard street widths are therefore chosen so that corridors exist
without long straights that favour snipers on a screen: alleys **4 m** (2
modules), streets **6 m**, canals **6 m**, causeways **12 m**, plazas at least
**24 m** clear on one axis. These are working values for level design; the
constraint is that all of them land on the 2.0 m grid.

#### What this means for the modular kit

- Every piece snaps to the 2.0 m grid with a bottom-left pivot, and joins on
  the 1.6 m course line. Pieces never need non-uniform scale or per-instance
  deformation to fit.
- **Variety is combination, not distortion**: the seeded layout recombines a
  small set of repeatable bays. No kit piece may have a unique silhouette that
  is visible from the reading range on its own, since a unique piece is a
  landmark and a landmark is a navigation aid the seeded city does not want.
- The vocabulary is small by design (the angle list above), so a new piece
  that needs a new angle is a rule exception and needs art-director sign-off.
- Section 6 sets the stylistic content of each piece; this section sets only
  its outline, grid and course placement.

*Alternatives considered:*

- **Organic, curved, vernacular streets.** More picturesque; rejected because
  they make crowd geometry irregular and invite sightline surprises that read
  as unfair.
- **Dramatic vertical towers.** Rejected: verticals compete with figures.
- **A looser modular system with free-form sculpting.** Rejected for the
  seeded layout (each piece must fit any neighbour) and for Principle 1's
  baseline logic.

### 3.4 UI shape grammar

The UI echoes the world: **rectilinear, stepped, flat**. It borrows the
*structure* of codex glyph work (flat fills, a single outline weight, one
internal detail per glyph, no gradients or bevels) rather than its literal
pictographs, so that every function-critical symbol is legible without
cultural knowledge. Deity and ritual glyphs are never used in functional UI
(Section 1.4).

| Rule | Specification |
| ---- | ------------- |
| Corners and frames | Panels are rectangles with **stepped (two-step) corners**, echoing a pyramid profile. Step size **4 px at 1080p**. No rounded corners, no drop shadows |
| Stroke | One outline weight, **2 px at 1080p** minimum (scale with UI resolution, **never thinner than 2 px**). Keylines: a **2 px Obsidian outer line** on every coloured glyph so it reads on any background; the **teammate marker and the sighting ping carry a 3 px keyline** (the ping 4 px beyond 20 m), verified by the dual-edge test (7.2) |
| Fills | Flat. No gradients, no glows (a glow is a beacon), no inner bevels. **Opacity is never a state encoding** (plates are 100 percent opaque; fades are transitions only). Stepped fret or stripe patterns are limited to **5 high-contrast repeats** and never fill a large area |
| Circle | Reserved for **radial progress and time** (interaction ring, cooldown ring) and for the sighting ring. A full circle in the UI means "duration or place." |
| Minimum size | Functional icons **32 px** square at 1080p; the teammate marker **28 x 32 px** default, **24 x 28 px** minimum, constant pixel size, always on top, edge-clamped when off-screen (7.3 row 1) |
| Motion | UI motion is eased and at least 4 s in any loop; no UI element pulses faster than 0.5 Hz (it must not compete with the baseline) |
| Pairing | Every colour-coded element has a shape (this section) and, where relevant, a label (Section 4.7) |

**Core glyph set** (shape is the meaning; colour reinforces it):

| Meaning | Shape | Colour | Notes |
| ------- | ----- | ------ | ----- |
| **Teammate marker** (ally) | **Downward-pointing pentagon** (a "house" pointing down), about 28 x 32 px default (24 x 28 px minimum), with a **two-step notch cut into the top edge, centred** | Turquoise (Section 4.6) with a 3 px Obsidian keyline | The notch is a stepped-fret nod. It is the only downward pentagon in the UI. Hangs above the character's head: **anchored 195 cm above the feet** (175 cm stature + 7 cm headwear cap, 1.3.3, + 13 cm clearance; Teammate Markers CR-7; re-derived if OI-1 moves the stature), drawn **always on top (no depth test) at constant pixel size**, and clamped to the screen edge as the same pentagon with a stepped base when off-screen. Never attached to anything that is not an ally |
| Teammate marker, concealed state | Same silhouette, **filled with a two-band stepped hatch** (2 px Obsidian stair bands across the lower half) | Turquoise | State is conveyed by fill pattern, never by opacity and never by hue; the hatch survives greyscale at 24 px |
| Teammate marker, informant state | Same silhouette, **outline only (no fill)**: a 3 px Turquoise stroke with the Obsidian keyline on **both sides** of the stroke (outside and inside), because a hollow interior over bright ground otherwise fails the dual-edge test (Teammate Markers CR-7) | Turquoise | Fill is the second axis: filled = active, hatched = concealed, outline = informant. These are states **of the marked player**, independent of the viewer: the outline is how every ally sees an informant ally |
| **Danger / finality** | **Upward-pointing triangle**, solid, with a stepped base (two steps) | Cochineal | The orientation is opposite to the ally marker: ally points *down* at a head, danger points *up* |
| Wrong-target tally square (replaces the strike counter pip; confirmed by the user 2026-10-25) | **Square**, `L` squares, filled up to min(n, L), outline-only beyond | Cochineal (filled) / Bone (outline) | Plus the line "Wrong targets: {n}" (7.3 row 3) |
| **Sighting ping** (team ping: any teammate, Active or informant, may place one; seen by allies only; Informant Pings PNG-R1, user decision 2026-10-25) | **Open ring with a gap** (a "broken circle", 60 degree gap), placed on the ground; world radius **>= 4 m**, screen minimum **32 px**, static, **no centre mark**; age shown by stepped stroke weight (6 / 4 / 3 px at thirds of its life, *proposed*), then expiry (start 8 s) | Marigold with a mandatory Obsidian keyline (3 px, 4 px beyond 20 m) | A place, never a person |
| Objective progress | **Stepped bar** (own bar 3 steps, rival bar 2 steps; 7.3 row 5) | Marigold | The steps echo a pyramid and are readable as quantity without colour |
| Neutral interact prompt | Square plate with stepped corners, Bone outline | Bone | |
| **Defence ring** (brace indicator; added from 7.5, states per 7.3 row 4) | Circle: empty when ready; on **Protected** (server-confirmed) an inner stepped square appears at once and the ring fills in four stepped segments over the protected window (never shrinks or closes); square removed at the end; Bone Dim linear sweep while cooling | Bone / Bone Dim, **no signal hue** | Self only. Duration, so a circle is permitted |
| **Result stamp** (added from 7.5) | Stepped square with a single horizontal bar | Bone | A successful action. Failure and penalty use the danger triangle |
| **Key glyph plate** (added from 7.5) | Stepped square, Bone outline, showing the bound key or button | Bone | Prompts show the currently bound input |

**Enemy marking.** There is no marker, ring, outline or label for enemies or
NPCs, ever (anti-pillar). In the round-end reveal only (2.10), the same
pentagon silhouette is used for everyone: ally markers are Turquoise-filled,
opponent markers are Bone-outlined on Obsidian. That is shape fill plus name
plus colour, and appears only after the match is decided.

*Alternatives considered:*

- **Literal codex glyphs for the HUD (day signs, deity glyphs).** Rich, but not
  legible at 24 px and not legible to anyone unfamiliar with them; also a
  sensitivity risk when a deity glyph is used as a button.
- **Round, soft modern HUD.** Cleanest to read; rejected because curves would
  clash with the rectilinear world and because circles are reserved for place
  and duration.
- **A single shape for all markers with colour as the only difference.**
  Rejected: that is exactly the colour-only encoding colourblind safety forbids.

### 3.5 Hero versus supporting shapes

There is no hero character. The hero shape is the **figure as a type** (head
circle over a trapezoid over two legs), and no individual figure is allowed to
become a hero.

| Tier | Shape | Where | Rule |
| ---- | ----- | ----- | ---- |
| 1 Hero | The human figure (small, upright, mid-value) | Everywhere there is walkable space | Wins every contest for the eye: orientation contrast, value separation from the backdrop (Section 4.3), clear head/leg/arm structure. Nothing else in the walkable eye band may have a similar silhouette (static-figure rule) |
| 1 Hero (distance) | Stepped temple massing | Skyline, seen from the plazas | The only large staircase-silhouette landmark. Reads at 100 m and orients without being a navigation aid in a seeded city (it is always in the temple precinct, whose position the seed may vary) |
| 2 Key affordance | The bush (3.6) | Scattered throughout | One consistent silhouette (below). Never competes with figures in value |
| 3 Supporting | Street walls, platforms, stairs, awnings, parapets | Everywhere | Horizontal, rectilinear, low-to-mid contrast |
| 4 Background | Decorative foliage, pottery, market goods, signage | Fills space | Low-contrast, mid-to-low chroma, and none may take a bush or figure silhouette |
| UI hero | Teammate marker, then the danger glyph | Screen-space | The only two UI shapes with colour reserved to them; they must pop against any world scene |

*Rationale:* the hierarchy is the visual priority order for the player's eye
(Section 1.2, Principle 2). The temple landmark is deliberately the one
exception to "no landmarks" because a stepped pyramid is the identity of the
setting, and the cultural brief requires the city to read as the capital of a
sophisticated state.

### 3.6 The bush silhouette specification

The concept document requires one distinct, consistent bush read that
decorative foliage never uses. The bush must be learnable in a few minutes and
recognisable at 25 m, but never a beacon: it must not carry a colour no other
foliage has (colour belongs to the city), and it must not glow, shimmer or move
differently from other foliage.

**Silhouette: a closed dome on a square stone planter.**

| Element | Spec |
| ------- | ---- |
| Planter | A **2.0 m x 2.0 m** square stone curb, **0.35 m** high, in Tezontle Red-Brown (P3, L* 30 to 33, 4.2) with a Limewash cap. It lands on the kit grid and gives a rectilinear footprint that the bush alone provides. No other planter in the game has this proportion (decorative planters are never 2.0 m square) |
| Dome | A closed, convex, nearly hemispherical mass, **1.8 m +/- 0.2** wide and **1.9 m +/- 0.1** tall, overall height including planter about 2.25 m. Height to width **1 : 1 to 1.2 : 1**. The top is gently flattened, never pointed |
| Leaf read | Large overlapping oval leaves arranged in **three horizontal tiers** (an echo of the tiered architecture), each tier slightly overhanging the one below. Leaf length **15 to 20 cm** |
| Value structure | **Two-value read**: a dark inner core (**L* 36 to 38**, lifted from about 30 on 2026-10-24) with lighter leaf tips (**L* 42 to 48**, tiers 2 and 3) catching the sun. **Dome-to-planter separation:** the core sits only 3 to 8 L* above the planter's tezontle (L* 30 to 33), so the boundary is carried by the **Limewash cap** (L* about 80) as a continuous light line between planter and dome; overhanging leaves never hide more than 25 percent of the cap's front length. This two-value dome is not used by any decorative plant. **Law 3b flag (OI-2):** leaf tips in the lowest 1.2 m (leg and forearm zones of a figure standing in front of the bush) sit near the skin mean (L* 48); the proposed resolution is tips of L* 38 or lower in the lowest tier, so tier 1 reads as one dark band at the core value (L* 36 to 38, itself Law 3b compliant) and the two-value read is carried by tiers 2 and 3, checked by the leg-and-forearm read test (4.1) before the bush family is signed off |
| Colour | Inside the Foliage Green family (Section 4.2). **Same hue family as decorative foliage; the bush is identified by shape and value structure, never by a unique colour** |
| Motion | Same slow, periodic leaf-sway as decorative foliage (period >= 4 s, eased). **Occupied and empty bushes are identical** (2.8) |
| Variants | One mesh family, up to 2 variants differing in leaf placement only, silhouette within **5 percent** of the master outline. Rotation in 90-degree steps only |

**The Dome Ban (what decorative foliage must not do):** no decorative plant
may be a closed convex dome with width to height between 0.8 : 1 and 1.4 : 1 and
a height between 1.3 m and 2.3 m. Decorative planting uses silhouettes from a
separate vocabulary:

- **Radial spiky rosettes** (maguey and agave): pointed, star-like, low
  (<= 1.4 m).
- **Tall slim stalks** (maize, reeds): vertical, thin (<= 0.4 m wide), and
  respecting the static-figure rule.
- **Broad flat canopies** (ahuehuete, willow-like): a trunk and a spreading
  crown, high (>= 4 m) so the canopy is above the eye band.
- **Low ground cover** (<= 0.6 m).

**Tests:** in a greyscale capture at 25 m, **>= 90 percent of testers
correctly classify** bush or not-bush, with **<= 5 percent of decorative plants
classified as bushes**. In colour, a tester shown only the bush's colour (a
swatch) cannot pick it from the foliage family at better than chance. Both
tests are run on the graybox before the vertical slice. The same 25 m greyscale capture also checks the **dome-to-planter boundary**: the Limewash cap line is visible on every bush in sun and in shade, and the measured median rendered L* of the dome's lowest tier is at least 3 above the planter face; a bush whose dome and planter merge into one dark block fails, even if testers classify it correctly.

*Alternatives considered:*

- **A distinctive bush colour (a brighter or flowering green).** Easiest to
  learn, rejected as a beacon that doubles as a colour-coded asset.
- **Make bushes a special prop type (a hedge wall or a stall).** Rejected:
  loses the "plain sight" fantasy of slipping into the foliage.
- **Several bush silhouettes.** Rejected: the concept requires one, because a
  second silhouette doubles the learning burden and weakens the recognition
  test.

### 3.7 Rules summary and trade-offs

- The city is horizontal, the people are upright; a person's silhouette is
  never a role.
- The heading rule, hem and arm rules are the shape-side half of the
  uniformity contract in Section 1.3.
- The UI is stepped, flat and rectilinear; shape is the primary carrier of
  meaning and colour reinforces it.
- **Trade-off flagged (heading rule versus hair variety):** requiring a
  back-weighted head on every model reduces how different hair can look.
  This is deliberate. If the creative director wants more hair variety, it
  must come from texture and cut line, not from a change in the head's
  silhouette.
- **Trade-off flagged (planter versus bush-as-organic-hide):** the stone planter
  makes bushes read very reliably, at some cost to the "growing in the city"
  naturalism. If playtests show bushes are too easy to spot from the
  planter alone, the first lever is the planter height (0.35 m to 0.2 m),
  not the dome.

## 4. Color System

### 4.1 Colour philosophy and the four colour laws

The colour concept comes straight from the visual rule: *the city holds still
and the crowd shares one body, so only behaviour can speak.* In colour terms:
**the world is warm, earthy and mid-value; colour is never information about a
player; and the only saturated colours in the frame are the ones the UI has
reserved to speak.**

1. **Colour belongs to the city.** The garment colours of the crowd are drawn
   from the same earth families as the walls and ground (4.3), and no hue on a
   world character is unique to a team, role, objective or kill count
   (Section 1.3.4). Colour in the world is never a tell.
2. **The crowd shares one value.** All eight garment swatches sit in a
   narrow value corridor of **L\* 60.0 to 62.0** (4.3). The crowd reads as one
   value mass in greyscale and differs by hue and chroma only. This is the
   colour-side half of Principle 3 and the reason the colour of a garment is
   not a clue.
3. **The backdrop avoids the crowd's value.** Any surface that forms the
   backdrop of a figure at crowd height (0 to 2.2 m) uses an albedo of **L\*
   53 or lower, or L\* 69 or higher**; it stays at least 8 L\* away from the
   crowd's L\* 61. This gives every figure value separation from its
   background (Principle 2).

   **Law 3b: the limbs get their own separation.** Skin sits at L\* 44 to 52
   (mean 48, Section 1.3.4), inside Law 3's dark allowance, so Law 3 alone
   would let a wall erase a bare leg or forearm in greyscale. Therefore the
   backdrop albedo in the **leg zone (0 to 0.55 m)** and the **forearm zone
   (0.7 to 1.2 m)** must also be at least **10 L\*** from the skin mean, which leaves at least **6 L\*** from the nearest edge of the skin band (44 or 52) in the worst case: **L\*
   38 or lower, or L\* 58 or higher**. Combined with Law 3, those zones take
   **L\* 38 or lower, or L\* 69 or higher**. Consequences: the street-wall dado
   (P3) is held at **L\* 30 to 33** (15 to 18 L\* below skin), sealed
   ground-floor door leaves are L\* 34 to 38 (6.2 M02), and paving (P2, L\* 72)
   already clears the rule. Two palette colours fall in the excluded band when
   they form a backdrop in those zones: **P4 Canal Green (L\* 51)** and **P5
   Foliage Green (L\* 43)**, with the bush's lighter leaf tips (3.6). They are
   not exempted; they are an explicit open item (OI-2, owner art-director with
   technical-artist): dark-value variants for water edges and the lowest 1.2 m
   of planting are the proposed resolution, decided by the test below. The gait
   and the arm tells (3.2) are read on bare limbs, which is why this zone rule
   exists.
4. **Saturation is rationed for meaning.** No world albedo exceeds **C\* 34**,
   so no world surface (garments included, at C\* 28 or below) can reach the
   chroma of a UI signal colour; any world albedo in the turquoise hue range
   (**hue 170 to 230 degrees**) stays at **C\* 12 or below**; **any world
   albedo in the marigold hue range (hue 60 to 100 degrees) stays at C\* 30 or
   below** (the UI Marigold is hue 80, C\* 67; the cap keeps ochre, paving,
   goods and flowers well clear of it); and every UI signal colour (4.6) has
   **C\* 42 or above**. A UI signal colour is therefore always the most
   saturated pixel group in the frame.

Laws 3, 3b and 4 are hypotheses to be verified in the graybox, using the
Principle 2 greyscale test, a chroma histogram of rendered frames, and the
leg-and-forearm read test:

**Leg-and-forearm read test (Law 3b).** Render the standard pose set (walk at
mid-stride, idle with arms hanging, one arm raised) for the pool at 20 m and
30 m, in sun and in shade, against every surface that can sit behind a figure
(dado, door leaf, upper wall, limewash, paving, water edge, planting, bush
planter). (a) *Measured:* the median rendered L\* difference between skin
pixels (legs, forearms) and the adjacent backdrop pixels is **at least 10** for the pool and **at least 6** for every individual model in
sun and in shade. (b) *Human, greyscale:* testers shown each frame for 2 s
identify stride versus stand from the legs in **at least 90 percent** of
trials, and arm raised versus hanging from the forearm in **at least 85
percent**, at 20 m and 30 m (the same lines as Principle 2). Repeated in
6.10.

All hex values are sRGB albedo values. L\*, C\* and hue angles are CIELAB
(D65, 2 degrees), **calculated by hand from the hex values and rounded**:
treat L\* as accurate to about +/- 0.5, and re-measure every swatch with the
T1 audit script before it is frozen (4.3).

### 4.2 Primary palette (seven colours)

| # | Name | Hex | L\* / C\* | Meaning in this world | Where it lives |
| - | ---- | --- | --------- | --------------------- | -------------- |
| P1 | **Limewash Bone** | `#CEC6B2` | 80 / 9 | Civic calm and order; the brightest surface that is not a light source. The baseline | Upper walls (above 1.6 m), stucco, awnings, temple caps, plaster |
| P2 | **Causeway Dust** | `#C1AE90` | 72 / 18 | The ground the crowd stands on; warm, dry, shared | Paving, plazas, packed earth, light-coloured stone |
| P3 | **Tezontle Red-Brown** | `#664335` | 32 / 20 (hue 48) | Weight, foundations and thresholds; the volcanic stone of the city, held muted so it never reads as a danger red. Darkened from `#704A3B` (L\* 35 / C\* 21) on 2026-10-24 so the dado clears Law 3b by 15 to 18 L\* | Street-wall dado (0 to 1.6 m, **L\* 30 to 33**, macro variation +/- 1.5), platform bases, plinths, door frames, bush planters |
| P4 | **Canal Green** | `#6F7F68` | 51 / 15 (hue 135) | Water, coolness, moisture; the city's "rest" colour | Canal water, wet stone, chinampa edges, algae. In a figure's leg or forearm backdrop zone it falls in the Law 3b excluded band: open item OI-2 |
| P5 | **Foliage Green** | `#5C6B4A` | 43 / 20 (hue 126) | Living things; the bush and decorative plants share it | All plants, crops, reeds. In the lowest 1.2 m behind a figure it falls in the Law 3b excluded band: open item OI-2 (proposed: dark-value variant at L\* 38 or lower) |
| P6 | **Midday Sky** | `#A9BAC6` | 75 / 9 (hue 246) | Open sky and cool ambient light; the only cool of any size | Sky dome, sky reflections, distance haze |
| P7 | **Obsidian Ink** | `#1C1917` | 9 / 2 | The deepest value: depth, interior, threshold, and the UI's ground. Also the title's symbol | Upper-storey window openings (M03), the lobby mirror, UI plates. **Not ground-floor doorways**: those are sealed leaves at L\* 34 to 38 (6.2 M02, 6.7), never a black void, because a head in front of Obsidian loses its dark-on-light read |

*Reading the palette:* P1 and P2 are the **light** values (L\* 69 or above), P3
to P5 the **dark** values (L\* 53 or below), P6 the sky, P7 the floor of
the scale. Nothing sits at the crowd's L\* 61 except the crowd. The warm
dominance (P1, P2, P3) with a cool minority (P4, P6) is the "warm light, cool
shadow" balance of Section 2 expressed in albedo.

*Alternatives considered:*

- **A saturated Mexica polychrome (reds, blues and yellows as in the
  codices).** Gorgeous but at odds with "saturation is rationed for meaning";
  every red or turquoise in the world would dilute the UI's reserved colours.
  Polychrome survives in the palette only as **muted bands** on temples
  (4.5).
- **A cooler, stone-grey base.** Rejected: loses the warmth that reads as
  Mexica and as hot midday.

### 4.3 Character garment swatches (the 8-swatch table)

This is the table promised in Section 1.3.4. A swatch is a discrete entry, not
a free colour. **All eight lie in the value corridor L\* 60.0 to 62.0**, so the
colour a figure wears carries no value information, and every swap is
value-neutral by construction (a swap moves mean garment L\* by at most **2.0**,
well inside the 5 L\* allowed by Section 1.3.4).

| ID | Name | Hex | L\* | C\* | Hue (deg) | Character |
| -- | ---- | --- | --- | --- | --------- | --------- |
| S1 | **Undyed Cotton** | `#9F9580` | 62.0 | 12 | 90 | Warm grey-cream; the plainest cloth |
| S2 | **Buff** | `#A59378` | 61.8 | 17 | 80 | Pale warm tan |
| S3 | **Ochre** | `#A99266` | 61.5 | 26 | 84 | Muted yellow earth; the most chromatic swatch (limit is C\* 28) |
| S4 | **Adobe** | `#BC8979` | 61.5 | 24 | 43 | Rose-clay; pink-leaning, kept clear of the saturated-red exclusion (hue 15 to 40 at C\* above 28) by a margin of 3 degrees and 4 chroma units |
| S5 | **Maguey Green-Grey** | `#8F9779` | 61.1 | 17 | 120 | Dusty sage; not in the turquoise hue range (170 to 230) |
| S6 | **Muted Indigo** | `#8691AF` | 60.2 | 18 | 280 | Dusty blue-violet (hue 280, outside the turquoise range); the only cool garment |
| S7 | **Umber** | `#A08E7E` | 60.2 | 12 | 69 | Soft brown-grey |
| S8 | **Charcoal-Brown** | `#96908A` | 60.0 | 4 | 71 | Near-neutral warm grey. The name is kept from Section 1.3.4 as the *ash end* of the table; at the shared value it is a mid-grey, not a dark colour |

**Why a narrow corridor.** Section 1.3.4 requires every model's mean rendered
luminance within **+/- 8 percent** of the pool median. A 1 L\* change at this
level is about 3.9 percent of garment luminance; garments are about 80 percent
of a figure's luminance, so a corridor of +/- 1 L\* consumes about **3 percent**
of the 8 percent budget and leaves the rest for skin, hair, accessories and
render tolerances. A corridor spanning the full Section 1 band (L\* 40 to 78)
would fail the parity rule by a factor of several, because a dark-garment
model and a light-garment model cannot have the same mean luminance. So the
Section 1 band is the outer envelope for validation and this table is
deliberately a much narrower subset of it.

**Skin and limb separation.** Skin is L\* 44 to 52, mean 48 (Section 1.3.4,
narrowed on 2026-10-24). The rule is that every garment swatch is at least **5
L\* lighter** than the top of the skin band; with the garment corridor at L\*
60.0 to 62.0 the actual margin is **8 L\*** (60.0 minus 52), so the rule holds
with 3 L\* to spare and neither number had to move. The two swatches nearest
the skin hue (S4 at hue 43 and S7 at hue 69) are kept at C\* 24 and 12, so a
bare forearm always reads against a garment by value (**12 to 14 L\*** against
the mean skin). The skin band and Law 3b (4.1) are designed together: the
backdrop is held 10 L\* away from the same mean of 48. Hair (L\* 10 to 22) is
darker than any garment by at least 38 L\*, so the head reads as a dark mass.

**Assignment rule.** With **N** base models in the pool (6 to 8), the first N
swatches S1 to SN are active (a 6-model MVP uses S1 to S6; S7 and S8 join at 7
and 8 models). Model k (k = 1 to N) is assigned, with indices taken modulo N:

- **Default garment:** swatch S(k).
- **Palette-swap variant:** swatch S(k+1).
- **Trim (hem band, shoulder border or belt), in both variants:** swatch
  S(k+3), covering no more than **25 percent** of the garment area.
- **Accessory variant:** garment **S(k)** (the default swatch, so the mean
  garment L\* is unchanged) and trim S(k+3); it introduces no swatch of its
  own. The accessories are **a separate merged mesh asset** with its own LODs
  (Character Pool & Appearance CP-4), not a material mask.
- **Material expression:** the master material carries two swatch indices,
  `GarmentSwatchIndex` (S(k) or S(k+1)) and `TrimSwatchIndex` (S(k+3)), and
  reads the garment and trim regions from the swatch/trim region mask (1.3.5,
  8.x.6). The per-model assignments are tabulated in the roster (1.3.10): the N = 8 table and the N = 6 MVP block, which differ in the swap and trim of models 4 to 6 because the indices wrap at N.

Each swatch then appears on exactly **3** distinct models (as default, as swap
and as trim), which meets the "at least 3 models per swatch" rule of Section
1.3.4 with no spare slots at any pool size from 6 to 8; if a model is added
or dropped, the table re-balances by changing N. Because every swatch is within
2 L\* of every other, the trim never makes a greyscale band; it is a hue/chroma
accent only.

**Excluded colours (reiterated for Sections 5 and 8):** no turquoise or teal
(hue 170 to 230 at C\* above 12), no saturated red (hue 15 to 40 at C\* above
28), no pure white, no black, and no colour above C\* 28 on any character.
Swatches are the only source of garment colour. The hand-computed values in
this table are targets; the value corridor (L\* 60.0 to 62.0) and the hue and
chroma limits are the contract, and a hex that re-measures outside them is
corrected, not the corridor.

*Alternatives considered:*

- **A full tonal range (cotton almost white, charcoal almost black).**
  Richer-looking crowd; rejected because it breaks luminance parity and
  makes dark or light models read as a different kind of figure at 15 to 30 m,
  which is what the T2 line-up test exists to prevent.
- **Eight hues at the same chroma.** Rejected: high chroma across the board
  would put the crowd into the same saturation range as the signal colours.
  Chroma is therefore graduated (C\* 4 to 26) and kept below the world's C\* 34.
- **Hand-picked colours per model.** Rejected: unique hues become unique
  identifiers.

### 4.4 Semantic colour usage

| Colour | Meaning | Allowed on | Never on |
| ------ | ------- | ---------- | -------- |
| **Turquoise** `#0FC2B8` | **Ally** (a teammate, the one who is on your team). Precious, and rare | The teammate marker and its states; ally-only chat/ping chrome; round-end ally markers | Anything in the world (props, water, characters, sky, FX); any enemy, NPC or neutral element; objective, loot or generic highlight; any button or decoration |
| **Cochineal** `#B01E3C` (deep) / `#E04A62` (bright) | **Danger and finality**: consequences that cannot be undone this round | The strike counter pips (used), the NPC-kill penalty warning, the elimination/removal card, the defeat header bar, the danger glyph | Victory, ally elements, general warnings or errors, button states, health-like bars, anything decorative; any world prop or garment (the closest world colour, Tezontle, is held at C\* 20 and a different hue) |
| **Marigold** `#E3A72F` | **Intel and progress**: neutral information everyone may use | Objective progress, informant sighting pings, neutral prompts that need emphasis | Danger or ally meaning; any team-specific meaning (objective colour is never team colour) |
| **Bone** `#EFE8D6` | Neutral text and line | All text, outlines, neutral glyphs | |
| **Obsidian** `#1C1917` | Ground and plates | UI plates, keylines | |
| World palette (4.2) | The city | The world | The UI signal roles |

The rule that makes this table work is that **a colour that has a meaning does
not appear anywhere else**: turquoise and cochineal each have a single job, and
the world is built so that neither colour can occur there by accident (law 4,
and the character exclusions in 4.3).

**Where a reader may see the rule bent, and why it is not:** the title's
"smoking mirror" symbolism (obsidian) and the Tezontle red-brown could look
like cochineal at a glance. They differ in chroma (20 versus 61 to 63) and
hue (48 degrees versus about 20), and the danger glyph's shape backs it up
(4.7).

### 4.5 Per-area colour temperature

All areas share **one sun** (a single directional light, 5,600 K, one azimuth,
one midday time: Section 2.1 and Lighting Law 1) and **one sky** (9,000 K).
**Nothing in the table below is a light input.** The columns are *measured
outcomes*: areas differ only because geometry, albedo and open sky change how
much sun, sky and bounce reach a figure. They are targets to be measured in the
graybox (open item OI-6, owner technical-artist), by reading a neutral 18
percent grey card held at 1.5 m at solar noon, with no grade applied. If an
area measures outside its tolerance, the fix is an albedo or geometry change
inside the 4.2 and 3.3 bands, **never a per-area light, sun colour or fill
override**. The earlier per-area sun colour temperatures (5,400 to 5,800 K) are
deleted: they contradicted a world that is lit once. **An area's profile must
be identical whichever team's objective it hosts**, so an area's look can never
become a team signal.

| Area | Shade ratio (measured, sun-lit : open-shade) | Shade tint and bounce (measured) | Palette emphasis | Notes |
| ---- | -------------------------------------------- | -------------------------------- | ---------------- | ----- |
| **Causeway and canal** | **2.5 : 1 +/- 0.3** (lowest of the open areas) | Grey card in shade about **9,000 to 9,500 K**: sky dominates, little bounce | P2 paving, P4 water, P6 sky | The coolest and most open area; longest sightlines. Water has **no sun-glitter**: specular is broad, roughness >= 0.25 and wave motion has a period of **6 s or more** (Principle 1). Reflections are muted to the sky tone |
| **Market plaza** | **3 : 1 +/- 0.3** in the open, **2.2 : 1 +/- 0.3** under awnings | Open shade about **9,000 K**. **Under awnings about 4,300 to 5,000 K**, produced by warm bounce off P1 awning undersides and P2 paving. **No translucent material is used** (the kit master is opaque, 8.x.10); the earlier "transmitted light" is withdrawn | P1, P2; goods allowed up to **C\* 34** in garment hue families, **but C\* 30 or below in hue 60 to 100** (Law 4) | The warmest area and the densest crowd. Goods never use turquoise hues, and cochineal-range reds are limited to C\* 24 or below. **Awnings are cotton in P1 only** (S2 removed: its L\* 61.8 is the crowd's own value and would break Law 3 as an overhead backdrop). Awning cloth is static or sways with a period of 4 s or more |
| **Temple precinct** | **4 : 1 +/- 0.4** (highest of the open areas) | Grey card in shade about **9,000 K +/- 500**, minimal bounce | P3, P1 | Formal and sober. The tiers throw deep, regular shadow bands. Painted bands (muted red-brown, ochre, indigo) stay at **C\* 30 or below**. No red-saturated or sacrificial dressing (Section 1.4) |
| **Residential alleys** | **2 : 1 +/- 0.3** | Grey card in shade between about **5,000 and 7,200 K**: sky fill pulled warm by bounce off the P1 upper walls and P2 paving | P1, P2, P3 dado | Narrow and shaded; sun arrives as slivers through the single sun's geometry. The mean scene luminance must stay at **>= 70 percent** of the plaza's so figures keep their limb read (the T2 shade capture, 1.3.7). Bounce warms shaded skin and prevents the cool shade from turning figures grey |
| **Objective sites** | **5 : 1 +/- 0.5** (pool to surround) | Pool reads at the sun's own 5,600 K; surround about **7,500 K +/- 500** | The area's own palette | The staged sun pool of Section 2.5. No unique hue: a site is marked by light and architecture. The same treatment applies to every team's stage and to the final site |

**Implementation path for the sun pool (one path, decided).** The pool is
produced by **geometry cutting the single directional sun**: the court's
parapet and cornice (M04, 6.8) shade the surround and leave the pool lit. There
is no spot, rect or point light at a site, no light function, no per-site
light channel and no Lumen-only trick. Sky and bounce come from Lumen. The
5 : 1 pool-to-surround ratio is therefore a result, enforced per generated
layout by the **generator shadow-trace check in 6.8** (lit area versus surround
at 5 : 1 +/- 0.5, regenerate on failure), which relies on the fixed sun
azimuth of 2.1. Calibration of the check against in-engine measurement is
open item OI-5 (technical-artist).

*Alternatives considered:* one uniform temperature for the whole city
(simplest, rejected because it makes areas indistinguishable and wastes the
setting's variety); colour-coded districts (rejected: a district colour is a
navigational aid and could become a team signal).

### 4.6 UI palette and where it diverges from the world

The UI uses a small, flat palette on dark plates so that every colour has the
same contrast ground anywhere in the world.

| Role | Name | Hex | L\* / C\* | Contrast against Obsidian plate |
| ---- | ---- | --- | --------- | ------------------------------- |
| Plate | **Obsidian Ink** | `#1C1917` | 9 / 2 | n/a (ground), used at **100 percent opacity** (changed from 85 percent: sunlit ground at L\* 69 or higher bled through and lowered real contrast) |
| Raised plate | **Obsidian Raised** | `#2B2622` | 16 / 4 | |
| Text, line | **Bone** | `#EFE8D6` | 92 / 8 | about **14 : 1** |
| Secondary text | **Bone Dim** | `#B9B09C` | 72 / 11 | about **8 : 1** |
| Ally | **Turquoise** | `#0FC2B8` | 71 / 42 (hue 188) | about **8 : 1** |
| Danger and finality (on dark) | **Cochineal Bright** | `#E04A62` | 54 / 63 (hue 18) | about **4.4 : 1** |
| Danger and finality (on Bone) | **Cochineal Deep** | `#B01E3C` | 39 / 61 (hue 20) | about **5.5 : 1** against Bone plates |
| Intel, progress | **Marigold** | `#E3A72F` | 72 / 67 (hue 80) | about **8 : 1** |

**Divergence from the world, by design.** The world is warm, matte and low in
chroma (C\* 34 or below). The UI adds **three** saturated hues (turquoise,
cochineal, marigold), all of C\* 42 or more, that appear nowhere in the world
(law 4). That is the entire point: the signal colours never have to compete
with the environment, and the player learns that a saturated colour is always a
message.

Plates are Obsidian at **100 percent opacity** with a stepped-corner outline
(Section 3.4); no plate is semi-transparent. Body text is Bone on Obsidian
(>= 4.5 : 1 required; the measured value is far above that); graphics and icons
must meet **>= 3 : 1** against their ground, measured on the plate over the
brightest ground. Cochineal Deep is used on Bone plates and Cochineal Bright on
Obsidian plates; the two stops are the same semantic colour. **Cochineal
Bright text rule:** Cochineal Bright measures about 4.4 : 1 on Obsidian, below
the 4.5 : 1 text floor, so it is **never used for text below 28 px at 1080p**,
and at 28 px and above only on an opaque plate (large-text 3 : 1 floor). Danger
text is Bone, set beside a Cochineal glyph or on a Cochineal Deep bar. Bone on
Cochineal Deep (5.5 : 1) and Cochineal Bright on the final plate are
**re-measured in Section 8** on the real plate (open item OI-3). Typography is
set in Section 7.

**The two reserved colours, restated.**

- **Turquoise is for allies only.** It marks the teammate marker and nothing
  else. A turquoise element on screen means "a member of my team." Using it
  for anything else (objectives, highlights, loot, buttons) spends the one
  colour that tells a player who is theirs, and it is a Pillar 1 problem if it
  ever indirectly identifies a non-ally.
- **Cochineal is for danger and finality only.** The 3rd NPC-kill penalty, the
  strike pips, the elimination card and the defeat header. It is deliberately
  the strongest, scarcest signal.

### 4.7 Colourblind safety

**Principle: no information is carried by hue alone.** Each semantic colour is
paired with at least one *shape or fill* cue that survives complete loss of
colour, and, where the information is urgent, with a *non-visual* cue. The
non-visual cues are specified by the audio-director and the ux-designer; this
section states only which ones are required.

| Semantic | Colour | Shape / fill backup (required) | Other backup | Why it needs it |
| -------- | ------ | ------------------------------ | ------------ | --------------- |
| **Ally (teammate marker)** | Turquoise | The downward pentagon with a stepped notch (3.4); fill state (filled / two-band stepped hatch / outline-only) encodes active / concealed / informant, and opacity encodes no state; Obsidian keyline | Name label on hold or focus; an **audio cue is required for teammate callouts** | A teammate marker must **not rely on turquoise alone**. Turquoise reads as a pale grey-blue or grey to some colour-vision deficiencies and sits near grass and cyan in the world |
| **Danger** | Cochineal | The upward triangle with stepped base | Text label ("Wrong target", "Informant" / "Too many wrong targets", "Eliminated for the round"); an **audio cue is required** (audio-director); never a bare red flash | Red is the hardest colour for protan and deutan viewers: it darkens toward brown and merges with the dark brown stone and the Obsidian plate. **Danger red must have a shape and an audio backup** |
| **Wrong-target tally** (replaces the strike counter; confirmed by the user 2026-10-25) | Cochineal (filled), Bone (outline) | `L` stepped squares filled up to min(n, L) versus outline squares; a static upward danger triangle at **n >= L - 1** | The line "Wrong targets: {n}" (numeral); audio twin **tally tick / threshold tick** (one tick per increment, the threshold variant once n >= L - 1) | Fill state, numeral and triangle carry the count; colour only reinforces |
| **Intel / sighting** | Marigold | The open broken ring on the ground | Age by stepped stroke weight (never a fade, 7.2 item 2) | Marigold is distinct on the blue-yellow axis for protan and deutan, but the ring is the primary read |
| **Objective progress** | Marigold | Stepped bar with discrete steps (own 3, rival 2) | Step count and the "Your house" / "Rival house" labels (no "n/5" label) | Quantity is readable without colour |

**Luminance separation.** The signal colours are separated by luminance as well
as by hue where possible: cochineal (L\* 39 to 54) is clearly darker than
turquoise (71) and marigold (72), so protan and deutan viewers can separate
danger from ally by lightness. Turquoise and marigold are close in lightness
and rely on hue and, above all, on their different shapes.

**Tests.** (1) *Greyscale*: every HUD state is screenshot in greyscale and a
tester identifies ally, danger, strike count, sighting and progress with
**100 percent** accuracy. (2) *Simulated colour-vision deficiency*: protan,
deutan and tritan simulations (the editor's or an external simulator) of the
same screens; **no pair of semantic elements may be confused** by a tester who
sees only the simulated frame. (3) *Environment collision*: in the same
simulations, none of the three signal colours may be confused with a world
palette colour: cochineal against Tezontle, turquoise against Canal Green, and
**marigold against ochre (S3), Causeway Dust (P2), market goods and
marigold-type flowers** (every world albedo in hue 60 to 100 is capped at C\*
30 by Law 4, against the UI Marigold's C\* 67). Pass line: no tester confuses
a marigold HUD element with a world surface in any of the simulations.

**Setting (proposed for the accessibility document).** An *ally accent*
alternative for the teammate marker colour (still allies-only, still with the
shape cue) is recommended, so a player for whom turquoise is hard to see can
choose a higher-contrast alternative. The art direction requires that the
alternative keeps the same shape and keyline; the colour list belongs to the
ux-designer. The proposed list (7.7 item 3), each to be run through protan,
deutan and tritan simulation and checked against the world palette (4.2, 4.3)
before adoption: (a) Turquoise default; (b) warm white `#FFFFFF` with a 3 px
Obsidian keyline; (c) magenta `#E454C8`; (d) ultramarine `#5B8CFF`. The setting
changes only the local render and is never replicated. This does not relax the
"turquoise means allies only" rule.

**Audio and caption twins.** Every audio cue named in 7.3 (teammate callout,
tally tick and threshold tick, sighting tick, brace protected and protection-ended, removal) needs
an on-screen text or caption twin, and every informative visual state needs a
non-visual twin; the cues themselves belong to the audio-director.

**Photosensitivity.** No signal element flashes. Pulsing is limited to 0.5 Hz
(Section 3.4) and never uses saturated red.

### 4.8 Constraints Sections 5 to 8 must respect

- **Section 5 (Character Design):** garment colour comes only from S1 to S8
  and carries no value difference (L\* 60 to 62); allies are shown by the
  teammate marker only; the head must follow the 60-percent-behind-the-ear
  heading rule; hems, arms and necks follow 3.2.
- **Section 6 (Environment):** street walls follow the dado/limewash banding;
  backdrops at crowd height stay at L\* 53 or lower, or L\* 69 or higher, and
  in the leg and forearm zones at L\* 38 or lower, or L\* 69 or higher (Law
  3b); world chroma is capped at C\* 34 (30 for painted temple bands and for
  any albedo in hue 60 to 100); the
  static-figure rule and the Dome Ban apply to all props and plants; sun
  glitter, heat shimmer and fast ambient motion are not allowed.
- **Section 7 (UI/HUD):** turquoise and cochineal keep their single jobs; the
  glyph set and its shapes in 3.4 are the icon vocabulary; stepped corners,
  flat fills, 2 px strokes with Obsidian keylines (3 px on the marker and the
  ping); plates 100 percent opaque; opacity encodes no state; contrast >= 4.5 : 1
  for text and >= 3 : 1 for graphics, verified by the dual-edge test on the
  brightest and darkest ground; nothing marks enemies or NPCs.
- **Section 8 (Assets):** the swatch hexes and the T1 re-measurement; the
  grade presets as data checked against the budget in Section 2; the 2.0 m
  grid and 1.6 m course for the kit; the bush specification as a unique asset
  family with a test for the Dome Ban; the 60 degree vertical field of view of
  the thumbnail table is **confirmed** (Camera System GDD default in both
  modes; closed 2026-10-24).

*Trade-offs flagged:*

- **A narrow crowd value versus a visually rich crowd.** The shared L\* 61 makes
  the crowd muted. That is deliberate (Pillar 1) and costs some Sensation
  (aesthetic #4). If the creative director wants more variety, it must come
  from hue and chroma within the corridor, never from value.
- **Turquoise as allies-only versus its beauty.** Reserving the setting's most
  precious colour for a UI marker means the world cannot use it, even though
  it is historically central to Mexica material culture. Section 6 may
  reference turquoise only in muted, low-chroma form (C\* 12 or below within
  the turquoise hue range), and that is a loss the identity accepts.
- **Colour-vision safety versus the marker's elegance.** The marker is
  large (24 x 28 px) and keylined because it is not allowed to depend on a
  colour; if the UI feels heavy, shrink everything else before the marker.

## 5. Character Design Direction

This section turns Section 1.3 (the pool contract), Section 3.2 (silhouette) and
Section 4.3 (garment swatches) into direction for how characters are *presented
and read*. It does not re-state their numbers. It answers four questions the
earlier sections leave open: who the player character is, how any viewer
tells one person from another (the honest answer is "by design, they cannot,
except for allies"), what the baseline behaviour is and which deviations are
tells, and how much character detail survives at game distance.

The governing sentence is the visual rule: *the crowd shares one body, so only
behaviour can speak.* Every direction below is a way of keeping the body quiet
and giving the behaviour a clean, unambiguous stage.

### 5.1 The player character: the Citizen

There is no hero, no protagonist model, no class and no loadout silhouette.
The player character is **the Citizen**: one of the 6 to 8 pool bodies
(Section 1.3.1), an adult of about 175 cm (the working stature value, open item
OI-1) in plain civic dress, who is visually interchangeable with roughly 40
NPCs and nine other players. The eight bodies are specified in the roster
(1.3.10) on the canonical Body_Base (1.3.9).

| Question | Direction |
| -------- | --------- |
| Who is the archetype? | "Anyone in the market." The archetype is a *behaviour* (patient, watchful, ordinary), not a look. Marketing key art may stage a figure in the crowd, but no model in the game is "the assassin" |
| How is an appearance chosen? | **Assigned, not chosen**: a player's appearance is a random member of the match's single appearance deck, the same deck the NPCs are dealt from (Section 1.3.1). A lobby appearance picker would break the "players are random members of the deck" property: players could cluster on a look the deck deals few NPCs of in a given match, or on a look they know is theirs to find, which is an unchosen tell. If the creative director later wants choice, it must be a re-roll of the deal, never a pick |
| What does the player see of themselves? | Third person: the same pool model, with **no self-outline, self-tint, self-marker or ground ring**. The camera is attached to the body, so the player finds themselves by the camera, not by a decoration. First person: the mesh is hidden locally and nothing else changes for other viewers |
| Is there personal expression? | Playstyle only (concept: Expression is aesthetic #7). No cosmetics, emotes, gestures, taunts or poses in the first scope. Any later cosmetic is a Pillar 1 review item and must be locally visible only |
| Gender and body presentation | One unisex Body_Base (Sections 1.3.2 and 1.3.9). Garment cut lines may carry cultural vocabulary (huipil, tilmatli, maxtlatl under a cloak) inside the shared envelope, per the roster (1.3.10). See the cost flagged in 5.8 |
| Faces | One shared expression: neutral, mouth closed, eyes level. The skeleton has no facial bones (zero extra bones, Section 1.3.5), so there is **no facial animation, blink or lip movement** on anyone. Faces differ between pool models only by the static features Section 1.3.1 allows |

*Why "the Citizen" and not a stylised assassin figure.* A recognisable player
fantasy figure is the normal way to give a game a face. Here it would be a tell
generator: any silhouette cue that says "this is the player-type" is a cue a
hunter can scan for. The fantasy (the concept's Core Fantasy) is *being
mistaken for everyone*, so the avatar's job is to disappear. The player's
authorship shows only in the choices of 5.5.

*Alternatives considered:*

- **Role-flavoured archetypes (scribe, porter, water-carrier) as pool models.**
  Rejected: a trade is a role signal and invites role-based reads that are not
  the behavioural reads the game is about (Section 3.2).
- **A visible "mask" or personal sigil for the player's own team.** Rejected:
  colour and mark belong to the city, and team identity is shown only by the
  teammate marker (Section 1.3.4, 3.4).
- **Player-chosen model from the pool.** Rejected for the rarity reason above.

### 5.2 Who is who: the viewer-state presentation matrix

Players cannot tell enemies from NPCs by design. The matrix below is the
contract for what any subject may look like to any viewer. **Anything not
listed in a cell is forbidden for that cell.** The viewer classes are decided
by team relation, not by alive/informant status, because an informant of the
other team is still an opponent to you.

| Subject state | Viewed by SELF | Viewed by a TEAMMATE | Viewed by an OPPONENT (active or informant) |
| ------------- | -------------- | -------------------- | ------------------------------------------- |
| **NPC** | Pool model, nothing else | Pool model, nothing else | Pool model, nothing else |
| **Active player, any teammates-visible state** | Pool model (third person) or hidden mesh (first person). Frame grade of the current state (Section 2) | Pool model plus the **filled turquoise teammate marker** (3.4) above the head | Pool model, **nothing else**. Indistinguishable from an NPC except by chosen behaviour (5.5) |
| **Active player concealed in a bush** | Own body per camera mode; leaf-edge vignette (2.8) | **No body.** Marker in its concealed state (filled with a two-band stepped hatch, 3.4; opacity encodes no state) | Bush only. No body, no shadow, no reflection |
| **Exiled player's released body** (removed by the penalty; the body becomes a crowd NPC) | View held on the body without control for the exile beat, then fade to the neutral view (Penalty Game Feel) | **Marker removed** from the body (300 ms exit, 7.6); the team feed line (7.3 row 12) | Pool model, **nothing else**: the strike clip plays through and the body walks on as a citizen; no snap, re-blend, LOD or appearance change (Penalty VA-NKP-2) |
| **Informant** (eliminated or removed player) | Pool model; informant grade and Bone corner brackets (2.9) | Pool model plus the **outline-only turquoise marker** (3.4) | Pool model, **nothing else**. Not translucent, not grey, not marked |
| **Round-end reveal** | Marker, name and event overlay (2.10) | Same | Same; opponents get Bone-outline markers on Obsidian |

Cells that look identical are identical on purpose. The only world-space
difference between any two cells in a row is a UI marker on the *viewing
client's* screen, and that marker exists only on teammates' clients (never
created for opponents, so it cannot be recovered by memory inspection of
their client; technical-director to confirm, ADR-001).

**Forbidden for all subjects, all viewers (until the reveal):** outlines, rim
lights, tints, x-ray silhouettes through walls, nameplates, health or status
bars, ground rings, footstep or dust VFX that differ per pawn, trails,
emissive or glowing details, translucent or ghosted bodies (the informant
descriptor "translucent" in 2.9 is a *mood word for the frame grade*, never a
material state), per-pawn shadow, reflection or Lumen differences, a different
LOD or animation fidelity, a selection or hover highlight, and a strike prompt
or reticle that appears for some pool characters and not others. The
assassination prompt must appear **identically for any valid target in range**
(an NPC, an opposing player or an opposing informant), with the same shape,
wording and timing, screen-fixed and never anchored to the figure, so that the
prompt cannot become a detector (Section 7 owns the prompt's look; this is the
constraint on it). **The one exclusion is the viewer's own allies** (active or
informant teammates): they are already identified to that viewer by the
teammate marker, so excluding them reveals nothing the viewer does not know
(7.1 item 4).

#### 5.2.1 Presentation of concealment and informant transitions

State changes are the moment most likely to leak identity. Three rules:

1. **Swap at occlusion.** A per-viewer change in whether a body is drawn
   (entering or leaving a bush, becoming an informant, round start) happens
   only while the body is fully occluded from that viewer by geometry or while
   it is out of that viewer's view. The bush planter (2.0 x 2.0 m) and dome
   (1.8 m wide, 1.9 m tall) enclose a 175 cm figure on all sides, so the swap
   is timed to the instant the figure's head passes the leaf plane inward, and
   mirrored on exit. There is no fade, dissolve, sparkle or ghost.
2. **The body leaves nothing behind.** The shadow caster, planar and Lumen
   reflections, water reflection and any audio-visual footstep decal are
   hidden with the body for the same viewers. A visible shadow inside a bush is
   a Pillar 1 failure of the same weight as a visible body.
3. **A kill is not a body change, and a removal is not a body change.** When
   a victim is eliminated the pawn plays
   one authored fall and removal sequence (same clip, same duration, same
   disappearance method) whether it is an NPC, an opponent, an ally or an
   informant. The disappearance method is the **cut at occlusion** of
   Assassination VA-ASN-2 (hidden on the first frame the body is out of that
   viewer's view or fully occluded after the linger, or cut in one frame when
   the grace runs out; never a fade, dissolve or sink). The attacker learns
   the outcome from a UI-only confirmation shown to the attacker alone
   (concept: feedback clarity), never from the body. A player removed by the
   penalty leaves a body that simply continues as a citizen (5.2 matrix,
   released exile body). **Entry:** an informant enters play by the
   **takeover of an unseen crowd pawn** (Penalty PEN-R7, Roster crowd
   adoption): no new figure appears, and the taken body keeps its look,
   velocity and head-look. An NPC replacement emerges from an off-sight
   emergence socket. Neither has a spawn effect, fade or flash (reconciled
   2026-10-24; replaces "walk in from an off-sight portal point").

### 5.3 Informants

Informants are pool models walking the same city, and an informant's whole
visual identity is what the table in 5.2 permits. Direction:

- An informant's body, garment, hem, accessory and silhouette are drawn from
  the pool like anyone else, assigned by the same team-blind rule. It does not
  keep the model it died in unless that is also what the crowd spawner would
  give it.
- Informants use the baseline locomotion set (5.4) only. If the gameplay
  design grants informants any behaviour the crowd cannot do (walk through
  bodies, fly, free-cam, a different top speed, passing through doors), that
  behaviour is itself a visible tell to opponents and is a **flag for the
  game-designer**. Art's requirement is that informants collide, queue and
  yield exactly as citizens do.
- Sightings are drawn on the screens of the sender's team as place pings (3.4),
  never as a mark on a character (any teammate may ping; Informant Pings,
  user decision 2026-10-25).

### 5.4 Expression and pose style: composed, not stiff and not expressive

Three directions were considered.

| Option | Description | For | Against |
| ------ | ----------- | --- | ------- |
| A. Stiff and ceremonial | Rigid, near-static upright figures with minimal motion | Very clean baseline; deviations are loud | A crowd of mannequins reads as an AI crowd, makes every NPC feel like a fixture, and removes the micro-variation a skilled reader uses (the "one beat too late" read). Also fights motion matching, which produces naturalistic motion |
| B. Expressive and naturalistic | Full human gesture, fidgeting, chatting, emotes | Alive, immersive | Gesture variety is the noise the tells must be read through: every flourish an NPC can do is a free disguise, and every flourish a player can do is an unchosen signature. Raises animation cost |
| **C. Composed (recommended)** | A calm, upright civic bearing with *natural but low-amplitude and shared* micro-motion | Alive enough not to look like clones; quiet enough that deviations stand out; cheap; matches Principle 1 | Less expressive; relies on the sample's databases being tuned to a calm style |

**Direction: C.** The bearing is "someone who knows the market and has nowhere
to be": level chin, relaxed shoulders, arms hanging free, even weight. The
amplitudes below are working values; the animation lead tunes them against
the T4 motion test and the 20 m detection test (5.8), but the *shape* of the
rule is fixed: **one baseline, drawn from one animation set, used by every
character.**

**Baseline behaviour catalogue.** These are the actions a citizen does that
are *not* tells. NPCs and players both produce only these unless a tell in
5.5 is chosen.

| ID | Baseline | Visual definition (working values) |
| -- | -------- | ---------------------------------- |
| B1 | Walk at crowd pace | Body orients to travel direction; torso pitch within 5 degrees of upright; arm swing about 18 degrees each way; cadence by pace, **1.8 steps/s at 112 cm/s rising to 2.0 steps/s at 140 cm/s** (SCL step-cadence formula); head bob <= 1.5 cm |
| B2 | Idle stand | Weight shift every 4 to 9 s with an eased onset (>= 0.8 s); slow chest rise; no loop shorter than 4 s (Principle 1); arms hang |
| B3 | Glance | Head yaw up to +/- 70 degrees from torso at <= 140 degrees/s with eased start and stop; hold 0.4 to 2.5 s; return. Head pitch within +/- 20 degrees. The torso does not follow |
| B4 | Slow to a pause | **Every stop to rest lasts >= 0.5 s**, from any speed and any pace, for players and NPCs alike (stop floor `StopMinSec` = 0.5 s, decision D2 2026-10-25, SCL F2: a tap or sub-pace stop is stretched to 0.5 s, never shorter); from a crowd walking pace the stop is **about 0.35 to 0.55 m of travel** (0.6 to 0.75 s), then B2. A lowered head (10 to 20 degrees) toward goods is allowed. No hands |
| B5 | Turn while walking | Heading change <= 60 degrees/s on a turning radius >= 1.0 m. A pause-and-pivot in place is B4 then B2, not a snap; **the in-place pivot after B4 turns at <= 150 degrees/s** (SCL) |
| B6 | Yield or side-step | A <= 0.5 m lateral step when another citizen closes within 1 m, facing travel throughout |
| B7 | Group drift | Match the speed of a neighbour within 0.3 m/s; no formation, no equal spacing |
| B8 | Brief touch | A single-hand touch of goods, a wall or a rail of <= 1.5 s, one arm only. **One shared contextual overlay** for every pawn (Crowd CNB-R5), never an NPC-only clip |

**Not in the MVP catalogue: sitting** (and browsing beyond B8). An NPC-only
behaviour with no player verb would be a tell; it returns only if a public
sit or browse verb is added (declined for MVP by the user 2026-10-25, Crowd OQ2).

**Rules that keep the baseline shared** (each closes a known leak between
player-driven and AI-driven pawns):

1. **Orient-to-movement only.** At walk pace the body faces its direction of
   travel. Strafing, back-pedalling and camera-facing locomotion are *not*
   part of the baseline. If gameplay needs them, they are tells and are listed
   in 5.5.
2. **One look-at solver.** Player aim/camera yaw and pitch drives the same
   look-at solver NPCs use, with the same maximum angular velocity (B3), the
   same yaw and pitch limits, and the same eased onset. A player can spin the
   camera at 700 degrees/s; the visible head cannot. Without this clamp the
   head snap becomes the most common unintended tell in the game.
3. **First and third person drive the same body.** Toggling camera mode
   changes nothing another viewer sees.
4. **No player-only smoothing.** Network smoothing, prediction correction and
   root-motion warping run the same way for every replicated pawn. A visible
   correction snap or foot slide on a player is a Pillar 1 defect; players are
   levelled *down* to NPC fidelity (the concept's design test), never the
   reverse. **Step sound lands within +/- 50 ms of the visible foot plant at
   tier A** (8.x.8), for every pawn.
5. **Motion matching, one database set.** Players and NPCs search the same
   databases with the **same pose-search cadence at the same screen height,
   and no other input** to the cadence (8.x.8 varies it by tier) and the same
   style tags. Art
   selects the calm locomotion style; nothing is authored for players only.

### 5.5 The tell vocabulary

A tell is a *chosen* action that deviates from the baseline (B1 to B8) in
silhouette, rhythm or trajectory, and whose advantage the game has paid for
with that visibility (Pillar 2). Tells are expressed in **motion and shape
only**: never colour, light, particle effect, glow, sound-linked VFX or
UI. Each tell must pass the 20 m greyscale detection test (>= 85 percent of
trained testers name it within 2 s) and must **not** pass at the exaggerated
level: a tell louder than needed rewards the reader less than a subtle one.

| Action (tell) | Visual signature | Why it is a tell | Pillar |
| ------------- | ---------------- | ---------------- | ------ |
| **Run** | Forward torso lean up to the 34 cm side-depth limit, bent arms, wide leg gap (3.2), speed well above B1 | Changes silhouette, cadence and speed at once; cannot be performed while holding a defence | 2 |
| **Parkour: vault, hurdle, climb** | One or both hands leave the torso line and contact a ledge, then a mantle, vault or hurdle; the body's trajectory leaves the ground plane | The shortcut is only available by leaving the baseline path and posture; the arc is unmistakable at 30 m | 2 |
| **Objective interaction** | Sustained, two-handed or kneeling pose at a fixture, held longer than B8's 1.5 s; the body stops moving and faces the fixture, not travel direction | Exposure is the price of progress (concept); the long stationary pose in a lit pool is the loudest honest read | 2, 4 |
| **Brace for defence** | The figure compresses toward about 60 percent of stature (3.2), feet planted wide, forearms raised, **held for the protected window, up to `DefenceDurationSec`** (default 4 s), planted from the first frame; **does not translate or turn while held** (only head-look moves). NPCs use the same clip (Crowd T4). Head stays a separate circle above the garment line | The reversal is paid for by a visible, held, distinctive stance; a defender is readable for the whole window | 2, 3 |
| **Assassination strike (attacker)** | A closing lunge to contact, arm extension to the target's torso, brief hold, recovery. **Three entry variants** (`Strike_F`, `Strike_L60`, `Strike_R60`), chosen by bearing, never by outcome, with identical contact pose and hold; from a bush, a **bush preface** (`Strike_BushPreface`, 0.35 s leaf crossing) plays before the unchanged strike (Assassination VA-ASN-1). Clip identical regardless of whether the victim is an NPC, an unbraced opponent or a braced one, up to the contact frame; a whiff plays the same clip into air | A decisive action must be visible to bystanders ("visibly dramatic"). The shared pre-contact clip prevents the attacker's animation from leaking the outcome | 3 |
| **Reversal (defender)** | From the brace stance, a counter-step and strike; the outcome branches only after contact | The identical brace-to-contact look means a reversal cannot be told from a kill by the grade or by the lead-in (2.7) | 3 |
| **Bush entry and exit** | A step toward the planter, a short authored leaf rustle (single motion, defined in the concealment GDD), then the body leaves view at the occlusion swap (5.2.1). Exit mirrors it | **NPCs do enter bushes**, as context-free red herrings at the Crowd rate (Crowd T5; Policy R7.6; reconciled 2026-10-24, replacing "NPCs never enter bushes"). The entry is still a visible action, but the rustle means "a body went in", never "a player went in"; the action, not the bush's state afterwards, is what is seen | 2, 1 |
| **Sudden stop** | Deceleration in < 0.2 s, silhouette frozen mid-stride with the leg gap open (3.2) | B4 takes >= 0.5 s, so a freeze is a visible break; only possible because there is no cloth sim to keep moving (1.3.3) | 2 |
| **Abrupt reversal** | Heading change > 120 degrees in < 0.5 s, or a pivot with no B4 | B5 allows <= 60 degrees/s; hunters and the hunted both do this | 2 |
| **Head hold beyond B3** (the only head-based tell) | A glance held longer than 2.5 s, or repeated glances in the same direction | Speed is clamped (5.4 rule 2), so head *timing* is the remaining tell instrument ("turned their head one beat too late"). A head faster than 140 degrees/s is not a tell but a defect | 2 |
| **Strafe or back-pedal** (only if gameplay allows) | Torso not facing travel direction at walk pace | Not part of the baseline (5.4 rule 1) | 2 |
| **Loiter without context** | Standing (B2) in the open for an unusually long time | A behavioural tell with no animation signature; art cannot author it, the NPC behaviour tuning knobs control the baseline dwell | 2 |

**Things that must *never* be tells (the leak list).** A viewer must never be
able to read a pawn from: its LOD or LOD transition; its animation update rate
or pose-search cadence; a network correction snap; its spawn or respawn; first
versus third person mode; its input device; or its team. If a review finds
any of these visible, the fix is parity (Section 1.2, Principle 3), never
a mitigation.

### 5.6 Detail survival and LOD philosophy

The player's eye reads **shape and value at distance, detail only at contact.**
The detail the game commits to is that which survives at the distances where
decisions are made (10 to 30 m); anything finer is invisible, so it is not
paid for twice. The thumbnail table in 3.2 is extended here.

| Distance | Figure height (60 degree vertical field of view, 1080p) | Detail that must survive | Detail that is allowed to vanish |
| -------- | ------------------------------------------------------- | ------------------------ | -------------------------------- |
| <= 5 m | >= 330 px | Face shape, bead diameter, garment seams, hands and fingers | Nothing |
| 10 m | about 165 px | Head turn and facing; hand position; hem and trim colour as a hue band | Stitching, individual hair strands, finger articulation |
| 20 m | about 82 px | Head direction (heading rule), leg gap, arm swing, stop versus walk | Facial features, accessories under 3 cm (beads, ear spools) |
| 30 m | about 55 px | Presence, heading, walk versus stop versus run | Trim, garment cut lines, hands |
| > 40 m | < 41 px | A moving upright mass | All tells below the run/vault/brace silhouette level |

Texel math supports the table: at the pool density of 5.12 texels/cm (1.3.5),
a 1024 texture is minified to about mip 2 at 10 m and mip 3 at 20 m, which is
why the face features vanish between 10 and 20 m by design. **Do not add
detail to fight this**; it is the camouflage working.

**LOD policy (the Pillar 1 core of this section).** LOD level is a function of
**screen size only** (distance and field of view), plus the occlusion-based
culling of the engine, and of nothing else.

- **Allowed inputs** to LOD, animation update rate and significance: screen
  size, distance, whether the pawn is on screen. **Forbidden inputs:** pawn
  class, controller type (player or AI), team, role, informant status,
  concealment, whether the pawn is the local viewer's own, current gait or
  action (a character that gets higher fidelity because it is running is a
  tell), or a gameplay "importance" score.
- **LOD bands: the screen-height table in 8.5 is authoritative** (amended
  2026-10-24; the earlier distance bands of 12 / 24 / 40 m contradicted it and
  are withdrawn). For the reference camera (1080p, 60 degree vertical FOV, 175
  cm figure) that is: LOD0 at figure height >= 19 percent (to about 8 m); LOD1
  11 to 19 percent (8 to 14 m); LOD2 6.3 to 11 percent (14 to 24 m); LOD3
  below 6.3 percent (beyond about 24 m). The technical-artist sets final
  thresholds, hysteresis and bias inside those bands so the crowd budget in
  1.3.5 holds. Triangle ceilings are those of 1.3.5 (12k, 6k, 2.5k, 900).
  **Consequence:** the 20 to 30 m reading range is LOD2 for 20 to 24 m and
  **LOD3 for 24 to 30 m**, so tells must read through LOD3 as well: the
  900-triangle mesh keeps the **back-weighted head mass** (the heading rule)
  and the **14 cm leg gap**, not only a walking-versus-running-versus-still
  silhouette. LOD3 is also checked by T1 item 16 (1.3.7).
- **Animation LOD parity.** Pose-search rate, blend-tree complexity, IK
  and foot-placement on/off, and any reduction in update frequency follow the
  same distance table for every pawn. No "AI-lite" graph, no skipped foot
  planting on NPCs, and no always-full-rate exemption for player-owned pawns
  (a common engine default). A pawn that is hidden or culled resumes with the
  same catch-up rule for everyone.
- **Equal LOD pop.** The LOD silhouette stays within 2 percent of LOD0 area
  (1.3.5). In addition, the mean per-pixel luminance change across a LOD
  boundary must be <= 2 percent inside the silhouette, and boundaries use the
  same hysteresis (a pawn crossing a threshold must travel >= 10 percent of the
  band width before switching back), so a LOD flicker never marks a pawn that
  is hovering near a threshold. If the technical-artist verifies a dithered or
  cross-faded transition works for skinned meshes in UE 5.8, adopt it for
  everyone; otherwise use hysteresis alone.
- **Shadow parity.** Shadow-casting LOD matches the body's LOD rules, so a
  shadow does not change shape for one pawn only.
- **Scope of exemption.** The single permitted difference is local: a
  first-person viewer's own mesh is hidden on their own screen (5.1).

**Test (parity, repeated whenever LOD or animation settings change):** place
one player-controlled pawn and one AI pawn on the same path at the same speed,
capture both at 8, 15, 25 and 40 m, and compare frame-to-frame. Pass:
silhouette IoU >= 0.98, no pose difference > 2 cm at the feet, and LOD
transitions at the same distances within the hysteresis band.

### 5.7 Rules summary and tests

- The Citizen is one of 6 to 8 pool bodies, assigned, never chosen, never
  marked to its owner or to opponents.
- The 5.2 matrix is exhaustive: anything not in a cell is forbidden.
- The baseline is B1 to B8; every chosen deviation is a row in 5.5, and every
  row is a motion, shape or rhythm change, never a colour, light or effect.
- Players are levelled *down* to NPC fidelity in LOD, animation update and
  smoothing.
- **Acceptance tests:** (1) the 5.2 forbidden-list audit in a graybox capture
  from three clients (self, teammate, opponent); (2) the 20 m greyscale
  detection test for each tell row; (3) the T4 motion test (1.3.7) with the
  baseline set; (4) the LOD parity test in 5.6; (5) a head-snap test with the
  camera spun at maximum speed.

### 5.8 Dependencies, trade-offs and open flags

- **technical-artist:** the per-viewer visibility path for concealment (body,
  shadow, reflection), LOD and animation-rate policy, look-at clamp.
- **animation lead / gameplay:** movement GDD must confirm the crowd pace, the
  B-catalogue values, whether strafing or crouching exist, and whether
  informants have a restricted move set.
- **game-designer (flags):** (a) can informants be killed, and does a kill on an
  informant count against the NPC-kill strikes; (b) NPC behaviour must keep
  hand-contact touches (B8) short enough that objective interaction stays a
  tell; (c) respawn and NPC replacement entry method (5.2.1).
- **Trade-off, composed style versus Sensation (aesthetic #4):** the calm
  baseline is deliberately restrained; expressiveness is withheld on purpose.
- **Trade-off, no facial animation versus warmth:** a static neutral face
  costs some life at close range. Accepted because facial bones would break the
  zero-extra-bone, shared-skeleton rule.
- **Cultural flag:** a single unisex Body_Base and a knee-to-calf hem band
  (1.3.3) mean garment cut carries little of the historical difference
  between clothing traditions (for example, longer wrap skirts). A sensitivity
  reader should confirm the pool's garment vocabulary (the eight cuts of the
  roster, 1.3.10) is respectful and not merely reduced.

## 6. Environment Design Language

This section gives the *content* of the city: the architectural style and its
cultural grounding, the 12-module kit, the surface and texture philosophy,
prop and plant rules per area, environmental storytelling, and what the seeded
layout may and may not do. Section 3.3 already fixed the kit's outline, grid and
angles; Section 4 fixed value, chroma and area temperature. This section does
not repeat those numbers. It fills them with Mexica-inspired form, under one
instruction from the visual rule: *the city holds still*, so the environment
may be rich in ornament **only where that ornament is out of the way of
reading a person**.

The environment's job, in order: (1) stage figures with a clean value
separation (Section 4.1, law 3); (2) stay still and rhythmic (Principle 1);
(3) never add a unique, memorable feature that works as a landmark or a
fingerprint (3.3); (4) look like the capital of a sophisticated state
(Section 1.4). Where (4) fights (1) to (3), the earlier items win and the
ornament moves up, down or inward (above 2.4 m, below 0.5 m, or into relief).

### 6.1 Architectural style and its cultural anchors

Smoking Mirror's city is *inspired by* Tenochtitlan at the moment of its
greatest order (about 1500): an island capital laid out on an orthogonal plan,
connected to the shore by raised causeways, divided by canals, fed by
chinampa gardens, anchored by a walled ceremonial precinct and a great
market. The kit translates each real feature into a rule.

| Cultural feature | What it was | How the kit reads it |
| ---------------- | ----------- | -------------------- |
| **Calzadas (causeways)** | Raised roads across the lake, wide enough for several people abreast, crossed by removable timber bridges | A 12 m deck of 2 m tiles with low side kerbs; bridges are timber-and-plaster spans (M11), the one place timber appears as structure |
| **Acequias (canals)** | An orthogonal canal grid, used by canoes (acalli) | Straight, right-angled canals 6 m wide with a vertical stone edge and a kerb (M10). The plan is already orthogonal, so the grid is cultural as well as practical. Curves appear only in water movement and canoe lines (3.3) |
| **Chinampas** | Raised, reed-edged garden plots at the city's margin | Low planting beds of 0.4 m on a water-tile footprint (M10 variant) with woven edges and crops from the low-plant vocabulary (6.5) |
| **Temple precinct** | Stepped pyramid platforms, twin stairs with flanking balustrades (alfardas), shrines at the top | Talud and tablero tiers (M07, M08) with a stair (M09); only here do heights exceed 6.4 m, to about 24 m |
| **Azotea roofs** | Flat, plastered roofs on timber beams, used as terraces | Flat roof, 0.5 m parapet, no pitched forms (3.3) |
| **Courtyard houses** | Walled, windowless to the street at ground level, built around an inner court | Blank street walls (M01), sealed doors (M02), windows only on the upper storey (M03) |
| **Tianguis (market)** | A vast, regulated open market with lines of stalls | The market plaza: stall rows under awnings (M12) in a regular rhythm |

**Style statement.** *Heavy dark base, light plaster above, horizontal courses,
stepped silhouettes.* Walls are stone-and-plaster; the street dado is dark
volcanic tezontle, the upper wall is limewash (3.3, 4.2). Light enters the
city as bands and pools laid on a ground plane of Causeway Dust. Ornament is
**geometric, shallow and above or below the eye band**: stepped-fret (the
xicalcoliuhqui pattern), water-line bands and maize or reed borders in low
relief.

*Honesty note on talud-tablero.* The talud-tablero profile (a sloped base
under a vertical panel with a projecting frame) is most famous from earlier
Mesoamerican cities such as Teotihuacan; Mexica monumental architecture also
uses sloped stepped platforms with balustrade stairs and its own panel work.
The kit uses talud and tablero as a **stylised, legible composite of the
Valley of Mexico tradition**, as the concept document calls for, and it does
not claim to reconstruct Templo Mayor. The sensitivity reader should confirm
that framing (6.9).

*Alternatives considered:*

- **A reconstruction-grade Templo Mayor and Tlatelolco.** Rejected: scope, and
  a unique, recognisable monument is a landmark (3.3).
- **A generic ancient-city kit with Mesoamerican dressing.** Rejected: loses
  the grounding that Section 1.4 requires and would drift towards the jungle-
  ruin trope.
- **Curved canal plans and organic streets.** Rejected for sightline fairness
  (3.3) and because the historical plan was orthogonal.

### 6.2 The modular kit

**Twelve modules**, all on the **2.0 m plan grid** with a **0.4 m sub-grid for
trim**, joining on the **1.6 m course line** (Section 3.3). Dimensions are
width x depth x height in metres. Pivot at the bottom-left corner. Every module
is authored at scale 1.0 and never deformed.

| ID | Module | Footprint W x D x H (the **unprojected cell**; talud batter and cornice projections lie outside it) | Silhouette | Trim and material rules |
| -- | ------ | ------------------- | ---------- | ----------------------- |
| M01 | **Muro** (street wall bay) | 2.0 x 0.8 x 3.2 | A flat rectangle. **Shape variants (3):** blank; vault wall 2.0 x 0.8 x 0.8; climb wall 2.0 x 0.8 x 1.6. **Surface variant (1):** relief frieze at 2.4 to 3.0 m, counted as a surface variant because it is a fret-band trim of 3 to 4 cm relief on the unchanged blank envelope (reconciled 2026-10-24, Generator CR 5) | 0 to 1.6 m tezontle dado (blocks 0.4 x 0.2 m modelled in geometry); 1.6 to 3.2 m limewash. The dado/limewash change is a **flush colour change, never a ledge** (6.7) |
| M02 | **Puerta** (door bay) | 2.0 x 0.8 x 3.2 | Rectangle with an opening 1.2 x 2.0 (1 : 1.67) | Opening is closed by a **sealed reed-mat or timber leaf set 0.2 m deep**, not a void (6.7), at **albedo L* 34 to 38** (C* 12 or below, tezontle hue family): **1 to 8 L* lighter than the dado** (L* 30 to 33; about 4.5 at the nominal 36 against 31.5), so value alone does not mark the opening: **it reads as an opening from the 0.2 m reveal and the frame** (the reveal's shade band and the jamb and lintel edges). The leaf stays at least 10 L* below the skin mean and at least 6 L* below the darkest skin (L* 44) in the worst case, so legs and forearms keep their read (Law 3b); behind a head the leaf is **12 to 28 L* lighter than the hair** (L* 10 to 22), a better dark-on-light read than Obsidian. Lintel at 2.0 m. Never placed in two adjacent bays (the 1 m clear spacing in 3.3) |
| M03 | **Piso Alto** (upper storey bay) | 2.0 x 0.8 x 3.2 | Rectangle, stacked on M01 or M02 to reach 6.4 m | Windows 0.8 x 1.2 (1 : 1.5), sill 0.8 m above its floor (so above 4.0 m), reveal 0.3 m. Windows are the only Obsidian-dark openings in the street wall |
| M04 | **Azotea** (cornice and parapet cap) | 2.0 x 0.8 x 0.9 (the cornice projects a further 0.4 m beyond the cell, so the extent is 1.2 m) | A horizontal band: 0.4 m cornice projecting 0.4 m, 0.5 m parapet above | Throws the constant shade band (3.3). Limewash with a tezontle underside. No crenellation or ornament on the skyline |
| M05 | **Esquina** (corner) | 2.0 x 2.0 x 3.2 | An L in plan, squared quoins | Quoin stones 0.4 x 0.4. Closes every wall end (a free wall end is always an Esquina or a pilaster, never a raw cut). **Mitred cornice cap:** M05 carries its own M04-profile cornice and parapet, mitred at 45 degrees round the outer and inner corner, so the M04 runs stop at the corner cell and never overlap or z-fight at corners, T-junctions and wall ends (a shape detail of M05, not a new module or trim; Generator VG-1) |
| M06 | **Losa** (paving tile) | 2.0 x 2.0 x 0.2 | Flat | Causeway Dust paving, joints on the 0.4 sub-grid and **hidden by world-aligned texture** (6.3). Variants: plain, scored, drain-channel |
| M07 | **Talud** (sloped base) | 2.0 x 2.0 x 1.6 (top face), base projects 0.4 on each exposed face | A sloped face at 14 degrees (1 : 4; 1.6 x tan 14 = 0.4 m) | Tezontle blocks laid in horizontal courses. The batter's lower edge sits on the grid line |
| M08 | **Tablero** (panel tier) | 2.0 x 2.0 x 1.6, cornice projects 0.4 | A vertical panel under a projecting frame | The only module that may carry a painted band (Section 4.5: <= C* 30). Tier setback >= 1.6 m (3.3) |
| M09 | **Escalera** (stair) | Civic 2.0 x 4.0 x 1.6; temple 4.0 x 2.0 x 1.6 (2.4 stair + two 0.8 balustrades) | Civic: a gentle ramp-like run; temple: a steep run between solid balustrade walls (alfardas) | Civic: 9 risers (0.178 m), run 0.30 m, 1.2 m landing. Temple: 7 risers (0.229 m), run 0.26 m. The riser is solved as 1.6 / n, within 5 percent of 3.3's nominal 0.17 and 0.22 (see 6.10) |
| M10 | **Acequia** (canal edge and water) | Edge 2.0 x 0.8 x 0.4 above quay; water tile 2.0 x 2.0 at -0.8 m; chinampa variant 2.0 x 4.0 x 0.4 | A straight kerbed edge; right-angled bends only | 0.4 m kerb makes water read as non-walkable. Water P4 Canal Green with no sun glitter (Section 4.5). Chinampa edge in woven reed (P5, P2) |
| M11 | **Calzada / Puente** (causeway deck / bridge) | Deck 2.0 x 2.0 x 0.4 (6 across = 12 m); bridge 2.0 x 6.0 x 0.4 over three water tiles | A horizontal deck; the bridge has timber beams visible below | Bridges are the only timber structure; plaster-sealed top surface; kerbs 0.4 m |
| M12 | **Toldo** (market awning bay) | 2.0 x 4.0, posts to 2.8 m, counter 2.0 x 0.8 x 0.9 | A horizontal cloth plane on four thin posts | Posts are 0.12 m, >= 2.6 m tall (architecture by 3.3's static-figure rule). Cotton **P1 only** (S2 is removed: at L* 61.8 it is the crowd's own value, and 4.5 and Law 3 forbid that as a backdrop). Static, or sways with a period >= 4 s and amplitude <= 3 cm. The counter is a collision wall, not a vault target |

Not in the kit: the **bush and planter** (a unique asset family, 3.6), the
**temple shrine cap** (built from M08 plus a roof of M04), and **interiors**
(no module is enterable in MVP; every opening is sealed).

**Silhouette rule for the whole kit.** Every module is a rectangle or a stepped
rectangle in elevation and plan, with only two non-rectangular
elements: the 14 degree talud and the stair profiles (3.3's angle list).
New angles are exceptions requiring art-director sign-off (3.3).

**Trim vocabulary (exactly three).** (1) *Course line*: a shallow 1 cm reveal
joint every 0.4 m; (2) *Cornice*: 0.4 m projection at 3.2 or 6.4 m only; (3)
*Fret band*: low relief of 3 to 4 cm, 0.4 m tall, used only at 2.4 to 3.0 m
and in the tablero. Nothing else is permitted as trim. A new trim type is a
new angle by another name.

**Structural elements (not trims).** Three small elements recur in the kit and
are named here with fixed dimensions so they are neither unlisted nor counted
against the three-trim rule:

| Element | Dimensions | Where | Rules |
| ------- | ---------- | ----- | ----- |
| **Coping** | 0.10 m thick, flat top >= 0.4 m deep, overhang 0.05 m on each exposed face, limewash | The top of every traversable wall (M01 vault 0.8 m and climb 1.6 m variants, platform edges, canal quay walls) | **Coping means traversable, and only coping does** (6.7). Same section on every traversable surface |
| **Quoin** | 0.4 m x 0.4 m face, one 0.4 m course high, set in the M05 corner and at every wall end | M05 Esquina and free wall ends | Squared; no decorative variants |
| **Pilaster** | 0.4 m x 0.4 m footprint, full wall height (3.2 m or 6.4 m) | A free end of a wall of 3.2 m or more | Architectural by the 3.3 height exemption (>= 2.6 m). **Walls under 2.6 m (the 0.8 m vault and 1.6 m climb walls) never end in a pilaster**, because a 1.6 m x 0.4 m upright falls in the static-figure window (3.3); they end in an M05 corner or a return wall >= 0.9 m wide |

**Bounding box and pivot.** A module's bounding box is the **unprojected
cell**: the 0.4 m talud batter (M07) and the 0.4 m cornice projections (M04,
M08) are excluded from it, and the pivot is the minimum X, minimum Y corner of
that cell at Z = 0. Projections never exceed 0.4 m (one sub-grid step), so the
neighbour-fit and snap tests (8.7) run on the cell and the projection overhangs
the neighbour's air space only.

**Variety budget.** Each module has **at most 3 shape variants and at most 2
surface variants**. Fewer, more reusable pieces keep the city a quiet baseline;
variety in the seeded city comes from *combination* (3.3), not from the
count of pieces.

**Derived assets (classification, reconciled 2026-10-24).** Pre-merged bay
stacks (for example M01+M04, M02+M04, M01+M03+M04, M02+M03+M04) and large
paving slabs built from M06 are **merges of approved modules**: they add no
shape, trim or surface and do not count against the variety budget, and they
must be visually identical to the per-cell assembly (Generator VG-2). They, and the G0 primitive placeholders, **may be produced before the 8.x.14
mass-production gate lifts** (user decision 2026-10-25): they are derived from
approved modules and spend no variety budget, so the gate still governs only new
authored variants and characters 2 to 8 (OI-8).

**District recipes** (which modules a seeded area may use; areas are those of
Section 4.5):

| Area | Modules used | Distinctive tendency |
| ---- | ------------ | -------------------- |
| Causeway and canal | M06, M10, M11, M01, M04 | Open, long, low walls; the most sky |
| Market plaza | M06, M12, M01, M02, M04, M09 civic | Horizontal cloth planes and stall rhythm; **service alleys** (below) |
| Temple precinct | M07, M08, M09 temple, M06, M05, M04 | Only area above 6.4 m; **service alleys** (below) |
| Residential alleys | M01, M02, M03, M04, M05, M06 | Narrow, shaded, two storeys at most |
| Objective sites | Any of the above (6.8) | Identical modules; only the sun pool and fixture differ |

**Service alleys (Market and Temple, reconciled 2026-10-24; LSC amendment,
LSC Open Question 7).** Market and Temple areas may include narrow back lanes
built with the Residential alley recipe (M01, M02, M04, M05, M06; 4 m wide,
2.8 m clear, 6.4 and 6.7), behind stall rows or precinct walls, so emergence
sockets are not concentrated in Residential areas. A service alley carries no
unique module, prop or light and obeys every 6.7 rule; it is an ordinary alley
that happens to sit in another area.

### 6.3 Texture and surface philosophy: stylised-realistic PBR

**Decision: physically-based, stylised-realistic.** Surfaces are authored in the
standard BaseColor / Normal / ORM workflow, de-lit, rendered with Lumen and
Nanite, and then *art-directed* by reducing detail, not by painting light into
albedo.

*Why PBR rather than hand-painted or flat-stylised:*

1. **The colour system is a numeric albedo contract.** Law 3 (backdrops at
   L* <= 53 or >= 69), law 3b (leg and forearm zones at L* <= 38 or >= 69),
   law 4 (C* <= 34, C* <= 30 in hue 60 to 100) and the swatch corridor
   (L* 60 to 62) are measurable only if albedo is clean. Painted light or ambient occlusion
   baked into colour would silently move a wall into the crowd's value band.
2. **The lighting is the mood.** The sun/shade grid and the staged sun pool
   (Section 2) are real lighting that Lumen must bounce plausibly (warm alley
   bounce at 4,600 K, 4.5). Painted surfaces would fight it.
3. **Characters are already PBR** (1.3.4, Default Lit). An unlit painted world
   would make the pool read as separate objects pasted on a backdrop.
4. **The pipeline is PBR.** Meshy output and Blender normalisation hand over
   PBR maps. A painted style (the rejected "Codex Market" direction) is the
   highest-risk option in that pipeline.

*Why "stylised" and not photoreal:* detail is a cost to reading. At crowd height
a surface must not carry high-contrast micro-detail (Principle 2).

| Property | Rule |
| -------- | ---- |
| Texel density | **5.12 texels/cm**: 1024 px across a 2.0 m module face, the same density as the characters (1.3.5), so grain is uniform across figure and wall |
| Maps | BaseColor (sRGB, BC7), Normal (BC5), ORM (linear). One material per module family; <= 8 base material families city-wide (limewash, tezontle, paving, water, timber, reed/cotton, foliage, painted temple bands) |
| Roughness | Stucco and limewash 0.85 to 0.95; tezontle 0.75 to 0.90; paving 0.80 to 0.90; timber 0.65 to 0.80; cloth 0.80 to 0.95; water >= 0.25. Nothing glossy at crowd height |
| Metallic | 0.0 everywhere except two permitted exceptions: small obsidian-mirror motifs and a very sparing gold in the temple shrine tops (Section 1.3.4: reserved for the environment); both away from the eye band |
| Albedo variation | Large-scale macro variation <= **+/- 3 L*** around a surface's nominal value, so a surface never drifts into the 53 to 69 exclusion band (4.1 law 3). **The dado is tighter: L* 31.5 +/- 1.5 (30 to 33)**, so it never drifts toward the skin mean (4.1 law 3b); limewash L* 80 +/- 3; paving L* 72 +/- 3; sealed door leaf L* 36 +/- 2 (1 to 8 L* above the dado, so it reads by its 0.2 m reveal and frame, not by value; 6.2 M02) |
| Weathering | Light and clean: slight edge wear, plaster hairlines, water-line staining at the canal. Stains and streaks must be <= 4 L* from the base and never form a vertical streak 0.35 to 0.6 m wide at 1.5 to 2.2 m height (it reads as a person) |
| Geometry detail | Nanite for stone courses, kerbs, relief and trim (real silhouette, no parallax tricks). Plaster is smooth with <= 2 cm undulation. **Foliage and the bush are not Nanite** (8.x.3: `r.Nanite.Foliage` defaults to 0 and sway needs world-position offset); they carry hand LODs. Revisit only if spike R9 passes |
| Decals | A pool of **>= 8 weathering decals** per surface family, each used so that no decal repeats within 12 m. No decal that reads as text, a hand-print or a figure |

Texture sources: kit modules are **Blender-authored** (Meshy cannot hold the
2.0 m grid or the 14 degree batter). Meshy is acceptable for **props** only,
after the same de-lighting and normalisation as characters (1.3.4).

*Alternatives considered:* hand-painted surfaces (rejected, above); a flat-
colour stylised world (rejected, same reasons as the character pool in 1.3.4);
photogrammetry-grade realism (rejected, cost and noise at crowd height).

### 6.4 Prop density rules per area type

A **prop** is any free-standing, non-kit object. Props are sized by class:
*ground* (<= 0.5 m), *waist* (0.5 to 1.2 m), *eye-band* (1.2 to 2.2 m) and
*architectural* (>= 2.6 m, the static-figure exemption). Waist props hide the
legs and with them the gait tell, so they are rationed. Eye-band props hide
heads and are rationed harder.

| Area | Props per 100 m2 of walkable floor | Max waist+eye props along any 20 m sightline (blocked length) | Clear floor (**props only**: kit modules, planters and architecture are not counted in this measure; they are governed by 3.3 and 6.7) | Allowed prop families |
| ---- | ---------------------------------- | ------------------------------------------------------------- | ----------- | --------------------- |
| Causeway and canal | 1 to 3 | 10 percent | >= 8 m of 12 m deck | Bollards, water jars, moored canoes (on water tiles), coiled rope |
| Market plaza | 6 to 10 | 25 percent | >= 60 percent, aisles >= 3.0 m | Stall counters, baskets (on the ground only), woven mats, pottery, stacked goods, drying racks |
| Temple precinct | 0 to 1 | 5 percent | >= 90 percent | Braziers (monument scale), low stone benches. No offerings, no vessels, no sacrificial items |
| Residential alleys | 2 to 4 | 15 percent | >= 70 percent, clear width >= 2.8 m of 4 m | Water jars, wood stacks, mats, hanging cloth (above 2.4 m) |
| Objective site | Equal to the host area +/- 20 percent | As host | Sun pool >= 6 x 6 m **free of props** other than the fixture (6.8) | Host-area families plus the fixture |

Rules:

1. **No prop in the walkable eye band may fall in the static-figure window**
   (1.5 to 2.2 m tall, 0.35 to 0.6 m wide, aspect 1 : 2.5 to 1 : 5) unless it
   obeys the 3.3 exemptions. Concretely: reed bundles lean or lie flat or
   are >= 0.9 m wide; no vertical standing mats of 0.4 x 1.8; no standing
   rolled cloth; water jars are wide (>= 0.5 m, <= 1.0 m high).
2. **Every prop family appears at least 3 times** in a layout, or not at all,
   so no single prop works as a fingerprint (the same logic as the accessory
   rule in 1.3.6).
3. **Props are static.** Smoke from a brazier rises slowly with a period
   >= 4 s and a plume width >= 0.9 m; no flame flicker in the eye band; no
   hanging item swinging at a human cadence (Principle 1).
4. **Props never take a bush or figure silhouette**: no closed dome of width
   0.8 : 1 to 1.4 : 1 and height 1.3 to 2.3 m (3.6, Dome Ban).
5. **Colour belongs to the city.** Goods may reach C* 34 in garment hue
   families, **except in hue 60 to 100 degrees, where every world albedo
   (goods, flowers, painted bands, awnings) is capped at C* 30** (4.1 law 4);
   none uses the turquoise hue range above C* 12, and reds stay below C* 24
   (4.5). Orange-yellow flowers (including marigold-type blooms) are muted
   under the same C* 30 cap and are tested against the UI Marigold in the
   colour-vision simulations (4.7 test 3).
6. **A prop is never the only occluder of a passage.** A player must be able
   to see through any street intersection without a prop deciding the view.

### 6.5 Plants and bush placement rules

**Decorative planting** uses only the Dome Ban vocabulary (3.6): rosettes
(maguey) <= 1.4 m, slim stalks (maize, reeds) <= 0.4 m wide, broad flat
canopies (ahuehuete) with the crown >= 4 m, and ground cover <= 0.6 m. Foliage
Green P5 only, with the same slow sway as the bush (>= 4 s, eased).

**Bush placement (visual rules; counts and spacing are the level-design and
concealment GDD's):**

1. **One planter, one bush.** Every 2.0 x 2.0 m planter holds a bush, and a
   bush is never without its planter. There is no empty bush-sized planter as
   a decoy (3.6).
2. **Seated in architecture.** The planter sits flush on the 2.0 m grid against
   a wall, a platform edge, a canal kerb or the corner of a court. A bush is
   never alone in the centre of an open plaza (it would read as a beacon and
   an unnatural object).
3. **Readable against its backdrop.** From the principal approach axis, at
   least **half of the dome's silhouette** is backed by a light surface
   (Limewash L* 80, paving L* 72 or sky), not by the tezontle dado alone
   (L* 30 to 33 against the bush's dark core of L* 36 to 38 leaves only 3 to 8 L*
   of separation, too little at 25 m, and the bush would camouflage as a wall; the Limewash cap carries the dome-to-planter edge, 3.6). Validate with the
   25 m greyscale classification test (3.6), including its dome-to-planter check, in situ, on the generated layout.
4. **Lit and shaded alike.** Bushes occur in sun and in shade in proportion to
   the area's light; their placement never correlates with the sun pool, an
   objective site, a spawn or a team.
5. **No tunnel.** The planter does not form a concealed pocket larger than
   itself: no bush in a recess; at least 1.0 m of open floor on at least two
   sides.
6. **Never within the site court.** A bush is not placed inside the
   objective-site court or on its sun pool (6.8), nor within **6 m of the court
   edge** (LSC `BushCourtExclusionCm`, default 600 cm, predicate V-COURT; the
   value is level design's knob, recorded here 2026-10-24).
7. **Rows read as a wall.** Two or three planters in a straight row 2.0 m apart
   read as a hedge-wall in plan, which is acceptable; four or more reads as a
   barrier and is not generated.

### 6.6 Environmental storytelling

**Principle: the city tells you it is ordered, populated and prosperous, and
nothing else.** Storytelling that would identify a place, a team or a role is
forbidden (Pillar 1); storytelling that mis-teaches a culture is forbidden
(Section 1.4).

| Rule | Specification |
| ---- | ------------- |
| **No text** | No writing, numerals, signage, painted names or pseudo-glyph writing anywhere. Wayfinding is by architecture and light only. Real glyph systems are not imitated as "writing" |
| **Murals and reliefs** | **Above the eye band**: friezes at **2.4 to 3.0 m** (M01 relief variant, M08 painted band). Content: stepped-fret, water-line, maize and reed borders, and abstracted non-figural motifs. Contrast inside the mural <= 12 L*, chroma <= 30 |
| **No human figures at human scale** | Any painted or carved human-like figure is <= 0.5 m tall and sits in a horizontal frieze, never as a single standing full-height figure at 1.5 to 2.2 m (it would be read as a static person, 3.3) |
| **Lower walls** | The tezontle dado carries at most a 3 to 4 cm relief of the fret at the same albedo as the wall: texture, not contrast |
| **Props as evidence of life** | Drying maize, a moored canoe, stacked pottery, a market stall with goods. Arranged in plain, regular groupings (rhythm), not in dramatic scenes |
| **Objective sites** | Readable **only up close**: from beyond about 12 m a site is a standard court (6.8); within 12 m, a low relief-carved fixture (waist-height, 0.8 to 1.0 m) is visible; the live-site cue is the range-limited interaction prompt (Section 7). Mood, not information, is carried by the sun pool (2.5) |
| **No lore dressing** | No banners, standards, insignia or heraldic devices. These are role/team signals; the closest permitted element is a plain undecorated awning cloth |
| **Absence** | No corpses, bloodstains, trophy racks, skull racks or sacrificial imagery of any kind (1.4) |

### 6.7 Readable routes, hiding places and sightlines in a seeded layout

The seeded generator assembles modules (3.3). It must be bound by art-side
rules so that a fresh layout is always fair to read.

**Seams and joins**

- Modules join only on the 2.0 m grid and the 1.6 m course line; no module is
  scaled or sheared to fit.
- **World-aligned materials**: all kit materials are aligned to world space
  (or to a shared module UV origin), so a seam between two modules of the same
  surface shows **no texture discontinuity**. This also stops adjacent
  repeats from showing a repeated texture.
- A wall run ends only with M05 or a pilaster (0.4 x 0.4 x full height; a
  pilaster is architectural by the 3.3 height exemption).
- Vertical joints between non-identical bays fall on a quoin or pilaster
  line, never mid-panel.

**Repeating-pattern tells.** Repetition is deliberate (it is the baseline) but
can leak information in four ways. Each has a rule:

| Leak | Rule |
| ---- | ---- |
| **Fingerprint** (a place is recognisable by a unique piece) | **Every module variant occurs >= 3 times** in a layout or not at all (the same occurrence rule as 1.3.6). No one-off cracks, stains, murals or broken corners. Decals follow 6.3 |
| **Person-shaped repetition** (a rhythm of dark uprights reads as people at 30 m) | Passes the static-figure test (3.3): ground-floor door openings are sealed leaves (**L* 34 to 38**, M02) rather than black voids, because a head in front of Obsidian loses its dark-on-light read and a leaf above L* 38 would come within 6 L* of the darkest skin (Law 3b worst case). Upper-storey windows (sill above 4.0 m, above any head) may be Obsidian. A greyscale 30 m capture of every kit run: < 5 percent of testers mistake any element for a person |
| **Periodic sequences** (ABABAB) | The generator caps identical runs at **6 bays (12 m)** and forbids 4 or more repeats of an A-B pair. Door bays alternate with blank bays (>= 1 m clear, 3.3) at irregular 2 to 5 bay intervals |
| **Visible tiling** | World-aligned macro variation (+/- 3 L*, 6.3), decal pool, and no per-module random albedo offset |

The regular bay rhythm is also a feature: it is a **ruler** that the eye uses to
read pace (a figure crossing three bays in a given time is plainly walking or
running).

**Sightline rules**

- Reading range is 20 to 30 m (Principle 2). The longest unbroken straight
  sightline in any walkable corridor is **32 m**. Longer runs are broken by a
  jog, a canal crossing (M11) or a plaza edge; plazas keep >= 24 m
  clear on one axis (3.3), and none exceeds 32 m on any axis. **A jog breaks a
  sightline only if its lateral offset is at least the corridor's clear
  width**; a one-module (2 m) jog in a wider corridor is not a break (LSC
  V-SIGHT, reconciled 2026-10-24).
- No walkable position may see across the whole city: at least one
  full-height wall or tier must interrupt any 60 m line at ground level.
- **Azoteas are not walkable in MVP.** If gameplay later wants rooftop routes,
  they bring overlook positions that break the sightline budget (flag 6.10).
- Streets, alleys and canals use the widths of 3.3 (4, 6, 6, 12 m); causeways
  are the only 12 m corridors. **Clear width (walls to props, the walkable
  passage) is at least 2.8 m in every corridor**, alleys included (6.4; LSC
  `NavMinClearWidthCm`, V-NAV-2).

**Hiding places.** The only concealment is the bush. Therefore:

- All wall recesses, niches and door reveals are **collision-sealed flush with
  the wall plane**; the visual depth never exceeds **0.4 m** (3.3). No recess
  is both >= 0.6 m wide and >= 0.6 m deep, since that would also look like a
  standing person's pocket.
- No dead-end court smaller than 6 x 6 m; no alcove behind a stair.
- Stair undersides are solid. Awnings (M12) have a clear underside from the
  ground, so a stall is never a cave.

**Traversal affordances (parkour must be a legitimate, readable route)**

Parkour is a tell (5.5). A route that is invisible to a new player is a trap,
and a route that is a beacon is a funnel; the kit makes routes plain and
uniform.

| Height | Meaning | Visual rule |
| ------ | ------- | ----------- |
| **0.8 m** | Vault / hurdle | M01 vault variant, planter-height walls, market low walls. Flat top >= 0.4 m deep with a **limewash coping, 0.1 m thick, 0.05 m overhang** |
| **1.6 m** | Climb up (one course) | M01 climb variant, platform edge, canal quay wall. Same coping and depth |
| **> 1.6 m** | Not climbable | No coping and no ledge at climbable depth below 3.2 m. The M04 cornice at 3.2 m is not reachable |

Rules: (1) **coping means traversable, and only coping does**; (2) the dado
and limewash change at 1.6 m is flush, so the colour break is never misread as
a ledge; (3) coping is the same on every traversable surface, so routes are
learned once; (4) heights are placeholders pending the traversal spike
against the Game Animation Sample's traversal limits (flag 6.10).

### 6.8 Objective sites

A site is an ordinary court from a distance and a distinct place up close.

- **Court.** Dimensioned as a standard junction or small plaza: **6 x 6 m to
  8 x 8 m** on the grid. Surrounding modules and prop density are the host
  area's (6.4), so the site is not a density anomaly.
- **Sun pool.** The staged pool (2.5) is cut by the court's parapet and
  cornice geometry, not by a unique structure, and by nothing else: it is the
  single project sun passing the court's geometry (implementation path, 4.5).
  The sun azimuth is the project constant of 2.1 (45 degrees from the grid X
  axis), so pool position is a fixed function of the generated layout.
- **Generator shadow-trace check (per generated layout, per site).** After
  assembly the generator traces the sun direction (azimuth and elevation of 2.1)
  across the layout's real geometry on a 0.4 m raster, for the court and a 6 m
  surround ring, and estimates relative illuminance as sun-lit area plus sky
  and bounce from sky-view factor. Pass: lit pool area is **at least 36 m2**
  (the 6 x 6 m clear pool of 6.4) and the estimated **pool-to-surround ratio is
  5 : 1 +/- 0.5**. On failure the generator **regenerates** the host modules
  around the site (up to 20 attempts). The same
  check applies to every team's stage and to the final site. Calibration of the
  estimate against in-engine grey-card measurement is open item OI-5.
  **Gate versus target (reconciled 2026-10-24, Generator CR 5):** the
  accept/reject gate for a layout is the Level Socket Contract predicate
  **V-POOL** (geometric sun raycast on a 0.5 m grid, contiguous lit rectangle
  >= 6 x 6 m containing the court centre, >= 30 m2, within +/- 20 percent of
  the median court) plus **V-POOL-40** (the 40 m beacon check of 2.5). This
  art check (0.4 m raster, >= 36 m2, 5 : 1 +/- 0.5) is the Generator's
  **repair target** (its S11.3 step): the Generator aims for it, but a layout
  that misses it after the repair attempts and still passes V-POOL is
  accepted. The 36 to 44 m2 LSC target band and this 36 m2 figure agree.
- **Fixture.** A single waist-height (0.8 to 1.0 m) stone block with low relief,
  from a family that also occurs as decor at least 3 times per site, so the
  fixture is not unique. The live-site confirmation is the interaction prompt,
  at interaction range.
- **No hue, no glow, no particle**, no banner, no team sign. The same fixture
  and court serve every team's stage and the shared final site (Section 4.5).

### 6.9 Cultural sensitivity notes

- **Living culture.** The Mexica heritage lives in present-day Nahua and
  Mexican communities. The world is a historical-inspired setting, not a
  "lost civilisation"; no ruin, overgrowth or decay is used as dressing.
- **No sacrificial spectacle** (1.4): no skull racks (tzompantli), sacrificial
  stones, bloodied steps, offerings of bodies or hearts. Temples are civic and
  ceremonial architecture, shown clean.
- **No deity as decoration.** Tlaloc, Huitzilopochtli, Coatlicue and other
  deities, and calendar monuments (such as the Sun Stone), are not used as
  decor, props or wall art; they are sacred and are not set dressing.
- **Glyphs.** Only abstract, widely documented motifs (stepped-fret, water-
  lines, maize, reeds) are used. No real day-sign or place glyph is used as an
  ornamental pattern without a cultural reviewer.
- **Language.** Module names in this section use Spanish architectural terms
  and a few well-attested Nahuatl words (acalli, tianguis). No Nahuatl text
  appears in the game, and any future use needs a qualified Nahuatl speaker.
- **Sourcing.** Reference the Templo Mayor Museum, Museo Nacional de
  Antropologia, excavation reports, and the codices read critically (1.4).
  Do not cite Hollywood or game depictions as authority.
- **Review.** A sensitivity read of the kit, the relief and fresco motif
  set and the fixture design before the vertical slice (concept; 1.4).

### 6.10 Rules summary, tests, dependencies and flags

**Tests**

1. **Static-figure test**: 30 m greyscale capture of each kit run in context;
   < 5 percent of testers mistake any element for a person (3.3).
2. **Value separation test**: a rendered crowd against each wall type;
   backdrop at crowd height L* <= 53 or >= 69, with macro variation inside
   +/- 3 L* (6.3); in the leg and forearm zones L* <= 38 or >= 69 with the
   dado inside +/- 1.5 around 31.5 (4.1 law 3b).
3. **Bush classification test** (3.6), run on generated layouts, not only the
   graybox.
4. **Pattern audit**: scripted check on a batch of 50 seeds for the 6.7
   occurrence rule, identical-run cap and door spacing.
5. **Sightline audit**: scripted check that no corridor exceeds 32 m and no
   60 m line is unbroken.
6. **Leg-and-forearm read test (greyscale)**: the procedure of 4.1 (measured
   L* difference of at least 10 in sun and shade; 90 percent stride-versus-stand
   and 85 percent arm-state identification at 20 and 30 m) against every
   surface in this section, including P4 water edges, P5 planting and the bush
   (open item OI-2).
7. **Sun-pool shadow-trace audit**: scripted run of the 6.8 check over a batch
   of 50 seeds; every site passes LSC V-POOL, and the share of sites meeting
   the 5 : 1 +/- 0.5 repair target is reported (an observation for OI-5
   calibration, not a gate).

**Debug art (reconciled 2026-10-24, LSC amendment).** The only debug art is
**editor-only marker actors** (sockets, zones, routes, courts), which are
server-only authoring data absent from the client cook (LSC; mechanism is an
ADR). No debug mesh, decal, light, colour or label ships in a client build.

**Dependencies:** technical-artist (world-aligned master materials, Nanite
set-up, Lumen warm-bounce check); level-design (bush counts and spacing,
exclusion distances, rooftop routes); game-designer (concealment GDD, site
design).

**Flags and open issues**

- **Stair nominals (Section 3.3). RESOLVED 2026-10-24 (ledger 7.z):** the
  nominal rise values (0.17, 0.22) do not divide the 1.6 m course; the kit
  solves 1.6 / n (0.178 and 0.229, within 5 percent). Section 3.3 now states
  the rises as nominal.
- **Door voids (Section 4.2). RESOLVED 2026-10-24 (ledger 7.z):** P7 Obsidian
  no longer lists "doorways in shade". Ground-floor doors are sealed leaves,
  now at **L* 34 to 38** (not the earlier 42 to 48, which sat on the skin mean
  and failed Law 3b), with Obsidian only in upper windows. Sections 4.2, 6.2
  (M02) and 6.7 carry the new value.
- **Sun pool as a beacon (Section 2.5).** The pool is a long-range cue; 6.8
  keeps the court itself unremarkable, but the playtest threshold in 2.5
  (found by light alone from > 40 m) still applies.
- **Marigold flowers.** The world's marigold-type blooms are culturally
  fitting but sit near the UI Marigold hue; every world albedo in hue 60 to
  100 is now capped at C* 30 (4.1 law 4) and the blooms are included in the
  extended colour-vision test (4.7 test 3). The cap and the test are in place;
  the simulation result is open item OI-7 (ux-designer, art-director).
- **Traversal heights** (0.8 and 1.6 m) are placeholders for the traversal
  spike; the kit may need a 1.2 m hurdle variant.
- **Rooftops** are excluded. Adding them is a sightline decision (6.7).
- **Trade-off, kit simplicity versus cultural richness.** Twelve modules and
  three trims withhold much of Tenochtitlan's ornament. This is the same
  pillar-driven reduction as the pool (1.2): richness goes up (friezes), down
  (low relief) and into single hero set pieces only if they are not landmarks.

## 7. UI/HUD Visual Direction

### 7.1 Governing rule and the diegetic / screen-space decision

> **The HUD rule:** *Every element on a player's screen is about that player or
> that player's allies, or about a place or a team total. Nothing on the screen
> ever identifies, counts or locates a person who is not an ally. Anything shown
> in response to one (the strike hint, the result of your own strike) is
> identical for every kind of person until your own strike resolves.*
> (CD-approved wording, HUD CR-HUD-8 item 1, reconciled 2026-10-24.)

This is Section 1's question 1 applied to the interface. The HUD is the one
visual layer that is, by definition, the same for every viewer in structure but
different in content, so it is where an unchosen difference between players is
most likely to leak. Four consequences follow and are used as tests below:

1. **No person-anchored element exists for anyone but an ally** (no marker, ring,
   outline, name, health, suspicion or role glyph on an enemy or an NPC).
2. **Every element is visible only to the viewer classes in 7.3's inventory**;
   the table is a data contract, not a suggestion.
3. **A screenshot or stream frame of any one player reveals only that player's
   own knowledge** (7.7).
4. **Nothing on the screen reacts to who is in front of the viewer.** There is no
   reticle, hit marker, target highlight or focus state that differs by target
   class. If the design gives the attack an on-screen prompt, it is screen-fixed
   (never anchored to the figure) and appears with identical shape, wording and
   timing for every valid target (NPC or enemy). The only exclusion allowed is
   the viewer's own allies, who are already identified by the marker. Between the
   attack input and the resolved result, the viewer's feedback is identical
   whoever the target was; only the result differs.

*Options considered for the HUD's place in the world:*

- **Fully diegetic** (no HUD; strike counts, objectives and cooldowns shown on
  the body, props or a carried codex). Strongest immersion and zero leak risk
  from the HUD, but anything diegetic is visible to everyone, which would turn
  private state (your strikes, your cooldown) into a tell the player did not
  choose: a Pillar 1 violation. Rejected for private state.
- **Fully screen-space** (all plates in the corners). Safest and most readable,
  but loses the one thing that must be spatial (ally markers) and the ground
  pings (a place must be *in* the place). Rejected.
- **Hybrid (Recommended):** the *world* carries every tell (posture, pace,
  interaction animation, bush use: Sections 5 and 6, never duplicated by the
  HUD); the *HUD* carries private state and ally information, in three classes:
  **corner plates** (screen-space), **world-anchored overlays** (ally marker,
  interact prompt, interaction ring, ground ping: they follow a point in the
  world but are drawn in screen space, flat, on top), and **full-screen cards**
  (removal, round-end).

**Diegetic exceptions allowed:** none for gameplay state. The interaction
animation and the objective site's physical state (if design gives it one) are
world tells owned by Section 6; the HUD never restates or amplifies them
(no "someone is interacting here" glyph shown to anyone).

**Screen layout.** Safe margin **5 percent** (96 x 54 px at 1080p). Anchors:
top-centre = team objective progress; bottom-left = wrong-target tally and
brace indicator;
top-right = team feed (removal lines only; a ping creates no feed line, PNG-R8); lower-centre (70 percent
height) = kill-confirmation and result plates; centre is **always empty** (no
reticle plate, no persistent element, at every text scale). Total persistent
corner-plate area <= **8 percent** of the screen, so the city stays the
picture. **Self-owned feedback never sits on or around the viewer's own body**
(third-person camera included): a ring on the own character would be a
recorded-frame tell and would break the no-rings-on-characters rule.

**Draw order.** The HUD is drawn after post-process. The mood grades, vignette
and transient snap of Sections 2.6 and 2.7 apply to the 3D scene only, so the
HUD is never darkened or desaturated by its own mood grade.

**Camera-mode independence.** One HUD serves first and third person.
World-anchored elements are screen-space overlays at **constant pixel size**,
clamped to the screen edge; they never scale with distance or with the 1P / 3P
field-of-view difference, so marker sizing does not depend on the FOV (60
degrees vertical in both modes, confirmed by the Camera System GDD).

### 7.2 Visual language carried from Sections 3.4 and 4.6

The shape grammar is unchanged: stepped corners (4 px at 1080p), flat fills,
2 px stroke (never below 2 px), 2 px Obsidian keyline on every coloured glyph,
Bone / Bone Dim text, and three signal hues each with one job. Section 7 changes
five values, listed in 7.z for the owning sections:

1. **Plates are fully opaque** (Obsidian Ink, 100 percent). The earlier 85
   percent let sunlit ground (L\* 69 or higher, 4.8) bleed through and lower the
   real contrast below the figures measured on the swatch. No plate is
   semi-transparent.
2. **Opacity encodes no state anywhere in the HUD.** Concealed uses a hatch (7.3
   row 1), distance uses constant size, ping age uses stepped stroke weight.
   Fades exist only as entrance and exit transitions (7.6).
3. **The teammate marker and the ground ping carry a 3 px Obsidian keyline**
   (the ping rises to 4 px beyond 20 m; *proposed*, verified by the dual-edge
   test below). Turquoise (L\* 71) and Marigold (L\* 72) have almost no luminance
   separation from pale ground, so the keyline carries the shape there. On the
   **outline-only (informant) marker the keyline runs on both sides of the
   3 px Turquoise stroke** (outside and inside), so the hollow interior over
   bright ground still passes the dual-edge test (Teammate Markers CR-7). Every
   other coloured glyph keeps 2 px.
4. **Text-colour rule.** Cochineal Bright measures about 4.4 : 1 on Obsidian
   (below the 4.5 : 1 text floor): **text is never coloured Cochineal Bright
   below 28 px at 1080p**, and at 28 px and above only on an opaque plate
   (large text, 3 : 1 floor). Danger text is Bone, set beside a Cochineal glyph
   or on a Cochineal Deep bar (Bone on Cochineal Deep is the same pair as 4.6's
   5.5 : 1 and is to be re-measured in Section 8). Turquoise is never a text or
   bar colour (ally marker only); Marigold text is allowed (about 8 : 1).
5. **Marker and ping minima** are fixed in pixels (7.3), not derived from the
   world.

**Accessibility baseline (adopted from the UX review).** Text >= 4.5 : 1 (large
text >= 3 : 1); icons, rings, pips and markers >= 3 : 1 against every ground
they can sit on; no meaning by hue alone (each colour-coded element already
has a shape and a fill or label); text resizable to 200 percent without loss of
function (7.4). **Dual-edge test:** marker and ping contrast is verified twice,
keyline against the brightest and darkest ground, and fill against keyline, on
the worst-case scene captures rather than on the swatch.

**High-contrast mode:** keylines 3 px everywhere, Bone Dim promoted to Bone
(plates are already opaque). The pentagon, notch, hatch and outline encodings
are unchanged.

### 7.3 HUD element inventory

Viewer classes: **Self** (the viewing player), **Ally** (teammate of the
viewer, active or informant), **Informant** (the viewer is in informant state),
**Enemy** (opposing team), **NPC** (never a viewer). "Shown to" means *the
element is rendered on that viewer's screen*; an element not listed for a class
is not sent to that client at all (7.7).

| # | Element | Location / class | Diegetic? | Shape | Colour | Shown to | Backup cue (non-colour) |
| - | ------- | ---------------- | --------- | ----- | ------ | -------- | ----------------------- |
| 1 | **Teammate marker** | Above ally head, world-anchored overlay, **always on top (no depth test)**, constant pixel size, 28 x 32 px default (min 24 x 28; "large markers" step 150 %). Off-screen allies: the same pentagon clamped to the screen edge with a stepped base | Screen-space | Down pentagon, stepped top notch (3.4). **Three states of the marked player, independent of the viewer:** filled = active; filled with a **two-band stepped hatch** (2 px Obsidian stair bands across the lower half) = concealed; outline-only = informant | Turquoise + **3 px** Obsidian keyline; alternatives from the accent list (7.4) | Allies of that player only (active and informant viewers); **never Enemy** | Shape and fill pattern (hatch survives greyscale at 24 px; no opacity encoding); name on hold; audio callout plus on-screen text (audio-director) |
| 2 | Ally name label | Beside marker, on hold or focus only; ally only, with **no response of any kind on a non-ally** | Screen-space | Opaque stepped plate, 22 px text | Bone on Obsidian | Allies only | Text itself. May fade by distance; the marker never does |
| 3 | **Wrong-target tally** (replaces the strike counter; confirmed by the user 2026-10-25; Penalty UI-NKP-1, HUD-E02) | Bottom-left plate, above the brace indicator | Screen-space | The line **"Wrong targets: {n}"** (the limit is not written) with `L` stepped squares above it, filled up to min(n, L). At **n >= L - 1** a static upward danger triangle appears and stays for n > L (shape, never a pulse); nothing escalates beyond it. Frozen while the viewer is an informant | Cochineal filled, Bone outline | **Self only** (not Ally, not Enemy) | Fill state; numeral; triangle; audio twin **tally tick** per increment and a **threshold tick** once n >= L - 1, chosen by n alone (audio-director); no toast; identical for every wrong target; gamepad rumble (*proposed*, ux-designer) |
| 4 | **Brace indicator** (defence ring; IIF UI-2, HUD-E01, Assassination ASN-R3) | Bottom-left, beside the tally, 56 px, never on the body | Screen-space | Circle (duration). **Ready:** empty outline ring. **Pending:** on press, a local outline emphasis only (no fill, no square). **Protected** (starts only on server confirmation; the brace is protected from `Prepare` execution): the inner stepped square appears at once and the ring fills in **4 stepped segments** of `DefenceDurationSec` / 4 each; it only ever fills, never shrinks, narrows, closes in on a mark or flashes, so no timing window exists to "hit". **End** (release, cap, cancel or a reversal consuming the brace, all identical; a reversal ends it at the contact time): square removed with a 250 ms stepped exit. **Cooling:** Bone Dim linear sweep plus numeral "Brace in 0:42", counting the cooldown from acceptance. (Replaces the earlier charge-then-armed reading.) | Bone ready / pending / protected; Bone Dim cooling; **no signal hue** (cochineal is finality, turquoise is allies, marigold is intel) | **Self only** | Ring state; numeral seconds; inner square = **protected now**; a protection-ended cue with a text twin (audio-director); no audible or visible ready-again cue to anyone else. **No cue of any kind when someone begins a strike against the viewer** |
| 5 | **Team objective progress** (Objectives CR 7, HUD-E04/E05) | Top-centre: own bar left, rival bar right | Screen-space | Stepped bar, discrete steps. **Own bar: 3 steps** (Stage 1, Stage 2, Final), labelled "Your house". **Rival bar: 2 steps**, labelled "Rival house", showing only `RivalPrelimCount` (0 to 2, delayed, may fill two steps at once) and **never a final step** | Marigold fill (both); own bar on Obsidian Raised, 1.25x scale | **The viewer's team only** (active and informant viewers), not "all players" | Step count; position and the "Your house" / "Rival house" labels; no "n/5" label (never a turquoise own-team bar) |
| 6 | Interact prompt | World-anchored to the *object* (never a person) | Screen-space | Square plate, stepped corners | Bone outline | Self, when in range | **Currently bound key or button glyph** + verb text ("Tend", "Slip in", "Slip out"); **identical at every court fixture** (Objectives CR 7) |
| 7 | Interaction ring | At the interacted object, 48 px | Screen-space | Circle (progress), linear sweep | Bone; **Marigold fill only at the own team's live sites, and only after `TendCueDelaySec`** (default 1.0 s) from the start of the tend; Bone before that and everywhere else (Objectives CR 7) | Self only | Sweep; world animation is the real tell for others |
| 8 | **Kill confirmation / result plate** (Assassination UI-ASN-1, UI-ASN-4; HUD-E07, E08) | Lower-centre result slot | Screen-space | Stepped plate. **"Target eliminated"**: stepped-square stamp, Bone plate. **"Wrong target"**: upward triangle. **"No contact"**: Bone text, no stamp, no cochineal (a miss is not finality). **"Reversed"**: upward triangle, followed by the removal card (row 11). Defender's plate **"Strike reversed"**: stepped-square stamp, never names the attacker or its team | Bone plate; Cochineal triangle + Bone text (wrong target, reversed) | **Self (the actor) only**; the defender's plate to the reversing defender only | Text and shape; audio. No count or "Penalty n/3" tail (Penalty CR-16). Shown at the **local contact frame**, never earlier; one entrance and one timing for every outcome and the defender plate, **250 ms / 3 s / 600 ms** (7.6; HUD owns the timing) (7.1 item 4) |
| 9 | **Team ping** (informant ping) | On the ground, world-anchored, **world radius >= 4 m (cluster scale, never body scale)**, drawn at a **screen-space minimum of 32 px** so it never foreshortens to a sliver; static after placement (never follows anything), **no centre mark**. Off-screen pings: edge-clamped ring at 32 px (Teammate Markers F3 clamp) | Screen-space | Open broken ring (gap 60 degrees); **age shown by stepped stroke weight** (heavy, medium, light at thirds of its life; *proposed* 6 / 4 / 3 px), plus a hard design-set expiry (start 8 s) | Marigold + mandatory Obsidian keyline (7.2 item 3) | **Every ally on the pinging team: Active and Informant viewers alike, the sender included** (Informant Pings PNG-R1, user decision 2026-10-25); **never Enemy** | Shape; stroke step; expiry; one quiet **non-positional** 2D UI tick on arrival (UI bus, never spatial, so it cannot be located in the world; PNG-R7); **no feed line and no plate or card** (PNG-R8). The ring carries the sender's name label, never a landmark or a description of a person |
| 10 | Informant-mode frame | Four corner brackets, 2 px | Screen-space | Stepped corner bracket | Bone | Self (informant) only | Shape; grade shift (2.9) |
| 11 | **Removal card** (exile: title "Informant", subtitle "Too many wrong targets" (D11); assassinated or reversed: "Eliminated for the round"; both with "You watch for your team now."; wording provisional, HUD OQ5 / Penalty OQ-NKP-4) | Central band, then collapses into the informant frame (row 10) | Screen-space | Upward stepped triangle + title bar; **never full-bleed** (card <= 30 % of screen area) | Opaque Cochineal Deep bar, Bone text | **The removed player**; Ally gets a one-line feed entry (element 12); a killed informant gets **no card** | Triangle; text; audio (audio-director; the exile cue fires with the card entrance). **Timing (HUD-R9):** exile card at the plate's show time + `ExileBeatSec` (0.8 s; normally contact + 0.8 s); other removals at the own fall start + 0.8 s. The card **pushes the plate off over 0.25 s (stepped), never a cross-fade**, then enters over 600 ms, holds 4 s, collapses over 800 ms. Shows once, never replayed on reconnect |
| 12 | Team feed line | Top-right, max 3 lines | Screen-space | Opaque stepped plate, 20 px text | Bone on Obsidian, upward triangle for removal | Allies only (removal lines; **no sighting lines in the MVP**, the sighting line type stays reserved for VS, Informant Pings PNG-R8). **The enemy team receives no removal notice** (7.x question 5, HUD-R10) | Text. Removal line **"{Ally name} — eliminated for the round"**, identical for every removal reason in MVP; the ally's marker leaves the body (300 ms exit) |
| 13 | **Round-end reveal** | Full overlay after the 3 s grade (2.10) | Screen-space | One pentagon silhouette for everyone | Allies Turquoise-filled; opponents Bone-outline on Obsidian | All players, **only after the round is decided**. Names show as aliases when streamer mode is on | Fill + name label + team column |
| 14 | Defeat / victory header | Top of reveal | Screen-space | Full-width stepped bar | Defeat: Cochineal Deep bar; Victory: Bone bar, no cochineal | All players, at round end | Text "Defeat" / "Victory" |
| 15 | **Returning countdown** (Penalty UI-NKP-4, HUD-E11) | Lower-centre result slot, on the neutral view after the fade completes | Screen-space | Opaque plate "Returning in {s}", whole-second steps, tabular numerals; at 0 a static "Returning" while the takeover is still picking: no animation, no count-up, no "searching" wording | Bone on Obsidian | **Self (informant awaiting a body) only** | Text; never flashes, no colour change near zero |
| 16 | Latency icon (IIF UI-7, HUD-E19) | Top-right safe edge | Screen-space | Small icon on the 7.5 grid, above 200 ms RTT | Bone | **Self only**; never shown on the host | Shape |
| 17 | Third-person hint (Camera, HUD-E17) | Notice slot, lower-centre | Screen-space | Opaque plate with the key glyph, shown once per profile on first third-person use, 4 s | Bone on Obsidian | **Self only** | Text |

Notes on the table: **(a)** rows 3, 4, 7, 8, 10, 15, 16 and 17 are Self-only
because a tally or cooldown shown to anyone else is a tell the player did not
choose. **(b)** The removal message names the removal type only to the removed
player; the *enemy* team receives **no** removal notice (7.x question 5,
answered by HUD-R10), because a notice would let a viewer count eliminations by
team. **(c)** Row 5 shows team totals, not players, and only to the viewer's
own team; the rival bar is delayed and never shows a final step. **(d)** Row 1
is always on top, so an occluded ally needs no extra state (the three fill
states are already used); this is an art and UX rule that gameplay must accept
(it shows ally positions through walls to allies only). **(e)** Prepare (row 4)
adds no screen grade; if 2.5's interaction grade is ever applied to it, it uses
2.5's values unchanged, so a recorded frame of a preparing defender is not
distinguishable from other self-owned states.

### 7.4 Typography

**Personality:** *carved, calm, civic.* The type should feel cut into stone or
plastered onto a wall, not scripted or ornamental, and it must never read as
a novelty "Aztec" or "tribal" face (Section 1.4). A glyph-inspired accent is
used for **display only**; functional text is plain.

**Two faces, one fallback.**

| Role | Face direction | Notes |
| ---- | -------------- | ----- |
| **Functional (body, labels, numerals)** | A neutral humanist or grotesque sans with an open aperture, large x-height, tabular lining numerals and full Latin Extended coverage, under a licence that permits embedding (an OFL face such as Source Sans 3 or Public Sans is the reference class; final choice is Section 8's licence check) | **Regular 400** body, **Semibold 600** emphasis and numerals. **No Light or Thin** (a 2 px keyline philosophy cannot survive 1 px hairlines at 1080p). Tracking +2 percent, line height 1.3 |
| **Display accent (headers only)** | A caps-only face built from the same sans with **stepped terminals**: each stroke end is cut in the two-step profile of 3.4, one step per stroke, no pictographs, no decorative flourishes | Used only at **>= 28 px**, for: round-end header, removal title, plate headings. Tracking +4 percent. **Never** for names, numerals, instructions or anything the player must read under pressure. Commissioned or modified in-house, not a downloaded themed font |
| **Fallback** | Functional face for every non-Latin script | The display accent has no fallback; if a locale lacks coverage the header uses the functional face at Semibold. Glyph-style never gates comprehension |

**Nahuatl and Spanish orthography:** the functional face must render macrons
(a e i o with macron), the saltillo apostrophe and all Spanish accents without
fallback substitution, since place and character names may use them. Test string:
"Tlahtoani, Tlatelolco, Mexihco, Cuauhtemoc, a-macron e-macron i-macron o-macron".

**Size hierarchy (px at the 1080p reference; UI scale = screen height / 1080):**

| Level | Use | 1080p | 1440p | Weight |
| ----- | --- | ----- | ----- | ------ |
| Display | Round-end header | 48 | 64 | Display accent |
| Heading | Removal title, plate heading | 28 | 37 | Display accent |
| Primary | Kill confirmation, objective label, names | 22 | 29 | Semibold |
| Body | Prompts, feed lines, reveal rows | 20 | 27 | Regular |
| Minimum | Secondary labels, step counts | **18** | **24** | Regular, Bone Dim allowed |

**Hard minimums:** no text below **18 px at 1080p / 24 px at 1440p** (this is
stricter than the UX floor of 16 px with an icon, and stands); no text that the
player must act on under **20 px / 27 px**; numerals that change in play
(strikes, cooldown seconds) at **>= 22 px**, tabular, Semibold. All text is on
an **opaque plate**; **no bare text over the world**, ever (contrast must not
depend on the sun). Plate text contrast >= 4.5 : 1, measured on the plate, not
on the swatch (7.2). Plates size to their text, so localisation expansion is
absorbed by the plate and never by truncation.

**Scaling (accessibility).** Two independent controls:

- **UI scale 75 to 150 percent** scales plates, stepped corners, steps and
  geometry. The 2 px stroke, the text floors, the 24 x 28 marker and the ring
  minima of 7.3 are **never reduced below their 100-percent values**; below 100
  percent only plate padding and non-text geometry shrink.
- **Text scale 100 / 125 / 150 / 200 percent** (default 100) for HUD, menus and
  reveal. Plates grow with text and reflow to the bottom-left and top-right
  stacks; the centre stays empty; the safe margin and the 8-percent area cap of
  7.1 hold at 100 percent and may be exceeded only at 125 percent and above.
- The **ally marker has its own scale (100 to 150 percent)** and is never reduced
  by a text- or UI-scale setting; 4.8's rule stands: shrink everything else
  before the marker.

**Gamepad (partial scope).** Prompts show the currently bound key or button
glyph (the key glyph plate, 7.5). The 150-percent scale options are the 10-foot
minimum; a dedicated gamepad layout is a ux-designer item.

### 7.5 Iconography

- **Grid:** 32 x 32 px at 1080p, 28 px live area, 2 px padding. 2 px stroke,
  2 px Obsidian outer keyline (3 px on the marker and ping, 7.2), flat fill,
  **one** internal detail per icon.
- **Angles:** 90 degrees and the two-step stair only; diagonals appear solely in
  the upward triangle (danger) and the pentagon (ally). The marker's concealed
  hatch and the edge-clamp base are built from the two-step stair, not from
  free diagonals. Circles only for duration or place (3.4).
- **Single colour per icon**, from the signal set or Bone; never two signal
  hues in one icon.
- **Vocabulary is the 3.4 core set.** Three additions this section proposes for
  3.4's table (approved and added there): **(i) Defence ring** (brace
  indicator): circle with an empty ready state; on server-confirmed
  protection an inner stepped square at once and a four-segment stepped fill
  over the protected window (the square means "protected now"); Bone, never a
  signal hue; **(ii) Result stamp**: stepped
  square with a single horizontal bar, Bone, for a successful action; **(iii)
  Key glyph plate** for prompts: stepped square, Bone outline, showing the
  bound key or button. Marker states (hatched concealed, edge-clamped) and the
  ping stroke steps are variants of existing 3.4 glyphs, not new glyphs.
- **Banned icons:** eyes, skulls, hearts, crosshairs, radar sweeps, footprints,
  arrows pointing at a figure, any human silhouette other than the ally
  pentagon, deity or day-sign glyphs (1.4, 3.4). The eye and crosshair are banned
  specifically because they imply detection or targeting, which the anti-pillars
  exclude.
- **Pattern limit (photosensitivity):** stepped fret or stripe patterns are
  limited to **5 high-contrast repeats** and never fill a large area (title
  bars use the plain stepped corner, not a striped band).
- **Acceptance:** each icon is legible at 24 px in greyscale (the hatched
  concealed marker must separate from the filled and the outline markers), and
  a first-time tester names its function correctly in >= 4 of 5 trials without
  text.

### 7.6 UI animation feel

The interface is **slow, level and quiet** (Held Breath): it should feel like
plaster setting, not a game show. It must also never be a faster visual event
than the crowd, or it competes with the baseline (Principle 1). Urgent signals
get their salience from **one-shot, non-looping, stepped entrances** (200 to 300
ms), larger static shapes, and non-visual cues, never from a loop or a pulse.

| Event | Motion | Timing | Easing |
| ----- | ------ | ------ | ------ |
| Plate appear (non-urgent) | Fade in plus **8 px** settle, in whole 4 px steps (stair motion) | 400 ms | Ease-out |
| Plate disappear | Fade out, no movement | 600 ms | Ease-in-out |
| **Urgent entrance** (tally square fill, threshold triangle, result plate) | One-shot stepped appearance, no loop; the tally numeral changes in one step (no roll-up) | **250 ms** (tally square fill 300 ms) | Stepped |
| **Preempt exit** (a plate pushed off by the removal card) | Stepped exit, never a cross-fade | **250 ms** | Stepped |
| Objective step added | New step fills, plate unchanged | 500 ms | Ease-out |
| Brace indicator (Protected) | Inner square appears at once on server confirmation; four stepped segments fill over the protected window; square removed with a stepped exit at the end. Never shrinks, closes, narrows or flashes | `DefenceDurationSec` / 4 per segment; end exit 250 ms | Stepped |
| Cooldown ring | Constant sweep (represents real time) | Real duration | **Linear** |
| Interaction ring | Real-time progress sweep, predicted, clamps at 0.98 until the server completes; empties in one step on cancel or rejection | Hold duration | **Linear** |
| Kill confirmation and result plates | Appear, hold, disappear. Same entrance for all four attacker outcomes (target eliminated, wrong target, no contact, reversed) and the defender's "Strike reversed" plate | 250 ms / **3 s** / 600 ms | As above |
| Removal card | Pushes the plate off (preempt exit), appears (fade, never full-bleed), holds, collapses into the informant frame; shown once | 600 ms / 4 s / 800 ms | Ease-in-out |
| Exile sequence (Penalty) | Plate at contact; the view holds on the released body for `ExileBeatSec` (0.8 s); then card entrance; the world fades to black over 0.5 s from the card entrance | plate gone at +0.25 s, world black at +0.5 s, card in at +0.6 s (relative to the card-entrance event) | Stepped / Ease |
| Returning countdown | Whole-second numeral steps; at 0 a static "Returning" | 1 s steps | Stepped (no motion) |
| Takeover fade-in (informant return) | Cut to the new body with the informant grade already on, fade from black | 0.5 s | Ease |
| Ping | Stepped appearance, hold with **stroke steps at thirds** of its life, exit fade | 300 ms / design-set hold (start 8 s) / 600 ms exit | Stepped / Ease |
| Ally marker | Appears or disappears on ally state change or removal only; **no bob, no scale pop, no smoothing lag** (it tracks the head 1:1 and has no distance or line-of-sight fade) | 300 ms | Ease |
| Reveal rows | Staggered rows, one per 300 ms | 500 ms each | Ease-out |

**Rules:** (1) **No flashing, strobing or pulsing faster than 0.5 Hz**, and no
saturated-red pulsing (4.7). (2) No scale-pop, shake, spin, overshoot or bounce.
(3) No element moves more than 8 px except the round-end reveal and cards.
(4) No persistent looping animation except the cooldown sweep; ambient loops
are >= 4 s. Real-time sweeps (the cooldown ring and the interaction ring) are
the **only linear motion** (IIF change request, reconciled 2026-10-24). (5) **Reduce motion** sets every fade to 0 ms, makes stepped entrances
instant and removes the 8 px settle; transition length is otherwise independent
of frame rate. (6) Nothing animates on an element that is attached to a
non-ally figure, because no such element exists. (7) **Combined-sequence
budget:** the screen grade (2.6, 2.7), the removal or defeat card and any plate
entering in the same moment are budgeted as **one sequence**, which stays
inside 3 luminance reversals per second and 10 percent relative luminance
change over a large area (Lighting Law 3); the card fades in, is never
full-bleed saturated red, and never repeats. The sequence is checked with a
photosensitivity analysis tool before sign-off (ux-designer and
technical-artist). (8) **Reduce screen effects** (a setting, separate from
Reduce motion; a warning is shown on first launch): the 2.7 snap eases over
0.5 s, the saturation drop is capped at -30 percent, vignettes are disabled,
and ping exit uses stepped states instead of a fade. (9) If a grade carries
information (for example the pursuit grade, 2.6), a non-visual twin is required
(audio-director owns the cue).

### 7.7 Who sees what: leak-proofing screenshots, streams and the network

The inventory in 7.3 is enforced by four art-direction rules (the networking
rules are handoffs, not art decisions):

1. **Same layout, same style, every player.** All players have identical plate
   positions, fonts and animation, regardless of team, role, strikes or
   informant state, so the frame's *structure* never indicates anything
   about the owner except what the owner chose.
2. **Self-only means not sent.** Elements 3, 4, 7, 8 (including the
   defender's "Strike reversed" plate), 10, 11, 15, 16 and 17 are rendered
   from the owning client's own state. The ally marker (1, 2, 12) is rendered
   only on clients whose team matches, from data the server replicates only to
   that team. The round-end roster (13) is sent to clients **only when the round
   ends**, never preloaded. **Handoff to network programmer and
   gameplay-programmer; art bible states the requirement, not the mechanism.**
3. **Colour accent choice is local.** The proposed ally-accent colour setting
   (4.7) changes only the local render; it is never replicated, so an accent
   colour cannot become a per-player identifier. Proposed list, each to be run
   through protan / deutan / tritan simulation and checked against the world
   palette (4.2, 4.3) before adoption: (a) Turquoise default; (b) warm white
   `#FFFFFF` with a 3 px Obsidian keyline; (c) magenta `#E454C8`; (d)
   ultramarine `#5B8CFF`. All keep the pentagon, notch, keyline and the
   allies-only rule.
4. **Nothing person-anchored for non-allies, nothing time-correlated.** A
   viewer's HUD never changes in response to the presence, proximity or action
   of a non-ally (no nearby-threat glow, no vignette-by-enemy, no audio-visual
   warning tied to a particular figure), **and no cue of any kind when a strike
   against the viewer begins**: the prepare ring (4) describes only the
   viewer's own prepared state and never reacts to being targeted, otherwise the
   defence would become a reflex check (anti-pillar).

**Per-viewer summary (what a screenshot of this viewer shows):**

| Viewer | Marker on allies | Own strikes / defence | Pings | Frame | Enemy information |
| ------ | ---------------- | --------------------- | ----- | ----- | ------------------ |
| Active player | Each ally's own state: filled / hatched (concealed) / outline (informant) | Yes | No (open, 7.x) | None | **None** |
| Informant | Each ally's own state, same encoding | Tally frozen, cooldown hidden; returning countdown while awaiting a body | Yes (own team) | Bone brackets | **None** (pings are places) |
| Ally (as seen *by* the viewer's teammate) | Yes | Not visible | Not visible | Not visible | None |
| Enemy | **Nothing at all** on your team's figures | Not visible | Not visible | Not visible | None |

**Streamer-mask note (adopted as the default stream protection).** Because the
HUD shows the viewer's own legitimate knowledge, three leaks survive any HUD
design: (a) a streamer's ally markers show a watcher on the opposing team who the
streamer's team is; (b) an informant's pings, if streamed live, give an enemy
watcher intel on sightings; (c) the streamer's own character is the one the
camera follows. The art direction cannot remove these without removing the ally
marker, which is the core readability tool. Streamer mode therefore masks the
*identity* layer and leaves the art unchanged: real names and IDs in labels,
feed lines, the lobby and the round-end reveal are replaced by aliases; no
personal data appears on the HUD; the informant role shows an optional "do not
stream live" reminder; the settings tooltip recommends a broadcast delay. All
shapes stay identical. The earlier, stronger option (hide ally markers except
while a hold-key is pressed) is kept only as an **opt-in setting if playtest
shows stream sniping**, because it degrades ally readability and leans harder on
the audio callout. Ignoring the problem as an out-of-scope social issue was
rejected.

**Tests (added to 2.12's list):** (1) *Strip test* (replaced 2026-10-24 by
Information Visibility Policy **F8**): with all of one player's private data
removed, the screenshot of an active player in a crowd must not let a tester
pick an enemy better than chance. **96 trials** (12 scenes x 8 testers), each
scene with chance level c_i = enemies shown / figures shown (target 1/9); the
test **fails at h_fail = ceil(sum n_i c_i + 1.645 sqrt(sum n_i c_i (1 - c_i)))
hits, 16 at 1/9 per scene**. The earlier 25 percent over 36 trials passed a
true 25 percent leak about 60 percent of the time and was too weak to gate. (2) *Packet/memory audit:* a script confirms that a client
on team B never holds team A's marker or ping state before round end (gameplay
programmer). (3) *Parity grep:* the HUD code contains no per-figure widget
class other than the ally marker. (4) *Target-parity test:* the attack input
produces identical self-feedback (timing, plate, audio) for an NPC target
and an enemy target until the result resolves. (5) *Dual-edge contrast test*
(7.2) on the brightest and darkest ground.

### 7.8 Handoffs and trade-offs

Open gameplay and UX questions are listed in 7.x; the handoffs are:

- **Display face** needs a commission or in-house modification: a Section 8
  asset line and a producer scope check.
- **To ux-designer:** screen anchors, UI and text scale steps, Reduce motion and
  Reduce screen effects behaviour, the ally accent list and its simulations,
  gamepad glyph sets and 10-foot sizing, interaction flows, the toggle
  alternative to hold-to-prepare once decided.
- **To audio-director:** the visual mood for non-visual backups is "level, quiet,
  low energy, never alarmist". Every audio cue named in 7.3 (teammate callout,
  tally tick and threshold tick, sighting tick, brace protected and
  protection-ended (no ready-again cue observable by anyone else), removal and
  exile) needs
  an on-screen text or caption twin, and every informative visual state needs a
  non-visual twin; the cues themselves are theirs. No cue may announce an
  incoming strike.
- **To technical-artist:** plates and ring as flat, resolution-scaled, opaque
  materials with no bloom, no emissive glow, no screen-space distortion over the
  plates; UMG drawn after post-process; markers and pings drawn always on top
  with no depth test, at constant pixel size.
- **To network programmer and gameplay-programmer:** per-team replication of
  marker and ping state, round-end roster only after the decision (7.7).

*Trade-offs flagged:* **(1) Slow UI versus feedback speed:** 400 to 600 ms
transitions make the HUD feel calm, but a result plate arrives later than a
twitch game's; urgent signals use 250 ms one-shot entrances to narrow the gap.
This is acceptable because the game is not a reflex game (anti-pillar).
**(2) Self-only private state versus team coordination:** hiding strikes and
cooldowns from allies keeps tells honest but makes a teammate's state a guess;
allies read each other through the world, which is Pillar 2 working as intended.
**(3) Ally-marker readability versus stream safety:** see 7.7; the default
favours readability plus identity masking. **(4) Always-on-top markers versus
world immersion:** allies are visible through walls, which costs a little
realism but removes an unreadable fourth marker state.

### 7.x UX alignment check and resolutions

The ux-designer checked this section against 3.4, 4.6 to 4.8, Lighting Law 3 and
2.5 to 2.9 and found 10 conflicts or gaps (2 High, 5 Medium, 3 Low). Decision
policy: adopt the UX recommendation unless it breaks Pillar 1 (no enemy
identification; no player-versus-NPC difference). **No recommendation broke a
pillar; all ten are adopted**, one with a clamp (C9). Where a UX item exposed a
rule the art draft lacked (attack-prompt parity, no cue on an incoming strike,
no self-ring on the own body, HUD after post-process), it is written into 7.1 to
7.7 above.

| # | Conflict (severity) | Art position | UX position | Decision | Rationale |
| - | ------------------- | ------------ | ----------- | -------- | --------- |
| C1 | Opacity is overloaded (High) | Concealed = 60 % fill; plates 85 %; ping fades with age; implied distance fade | Opacity is a weak, context-dependent axis; 60 vs 100 % is not separable at 24 px over bright ground; low-vision users lose it | **Adopt.** Opacity encodes no state. Concealed = stepped hatch; distance = constant pixel size; ping age = stepped stroke weight; fades are transitions only | Redundant coding needs an axis that survives greyscale and low vision; keeps the fill axis (filled / hatched / outline) |
| C2 | Plate transparency vs contrast (High) | Obsidian 85 %, contrast measured on the opaque swatch; Cochineal Bright 4.4 : 1 | Bleed over L\* 69+ ground drops real contrast (Cochineal text to about 3 to 3.5 : 1) | **Adopt.** All plates 100 % opaque; Cochineal Bright never text below 28 px; danger text is Bone; measure on the plate over the brightest ground | The HUD can spend contrast freely because the world cannot (Held Breath); text must survive the sun |
| C3 | Ground sighting ring vs readability and no-ring-on-people (Medium) | Open Marigold ring on the ground, a place | Foreshortens to a sliver past about 20 m, hidden by crowds, near-zero luminance contrast on pale ground, body-sized ring reads as a selection circle | **Adopt.** Screen-space minimum 32 px; mandatory keyline (3 px, 4 px beyond 20 m); world radius >= 4 m; static; no centre mark; edge indicator | A ping must stay a place and never a person (anti-pillar), and still be readable |
| C4 | Slow low-energy UI motion vs urgent signals (Medium) | Loops >= 4 s, <= 0.5 Hz, no glow | Third strike, ally call-outs and elimination must register peripherally | **Adopt, keeping both positions.** Salience from one-shot stepped entrances (250 to 300 ms), larger static shapes, audio and haptics; no looping pulse | Meets urgency without adding a second source of motion to the baseline |
| C5 | Turquoise (L\* 71) vs bright backdrops (L\* 69+) (Medium) | 2 px keyline on every coloured glyph | Nearly no luminance separation; the keyline carries the marker | **Adopt.** 3 px keyline on the marker (and ping) only; dual-edge test | 4.8 already says shrink everything else before the marker; this is the cheapest fix |
| C6 | Occluded teammate undefined; outline already means informant (Medium) | Open question; occluded must not reuse the 60 % encoding | Always on top, no new fill state; a fourth state is indistinguishable at 24 px | **Adopt.** Always on top (no depth test); documented in 7.3 note (d) | Allies-only information, so no Pillar 1 cost; gameplay must accept it |
| C7 | Hold-to-prepare UI undefined; parry-window risk; 4 s hold with no alternative (Medium) | Defence ring with a "ready" square | Stepped, non-closing ring then static armed glyph; no attack-warning cue; toggle option | **Adopt** the ring and the no-cue rule (7.3 row 4, 7.7 item 4); toggle is open (below). **Updated 2026-10-24 (IIF UI-2, Assassination ASN-R3):** "armed" now means *protected from server acceptance*: the square appears at once on confirmation and the segments fill over the protected window; a protection-ended cue is added | A closing window would turn anticipation into reflex (anti-pillar); neutral Bone avoids stealing a signal hue |
| C8 | Screen grades vs low vision and photosensitivity (Medium) | Each grade within the 3 per second rule; pursuit vignette on | Sequences (grade + card + UI) unbudgeted; vignette removes the periphery; informative grades lack a non-visual twin | **Adopt.** Combined-sequence budget, Reduce screen effects setting, non-visual twin for informative grades (7.6 rules 7 to 9) | Accessibility without changing the world's grade values by default |
| C9 | No typography or text-scale floor (Low) | 18 px minimum, 100 / 125 / 150 text scale, menus to 200 | Body >= 20 px, labels >= 16 px with an icon; UI scale 75 to 150 plus text-only to 200 | **Adopt with a clamp.** Art's 18 px floor stays (stricter); UI scale 75 to 150; text scale to 200 on the HUD too; the text floors, 2 px stroke and 24 x 28 marker never go below their 100 % values | Keeps the readability contract absolute while giving the range asked for |
| C10 | Streamer / gamepad specifics absent (Low) | Delay recommendation now, hide-markers streamer mode if needed | Streamer mode masks names and IDs with aliases; bound-key glyphs; 150 % scale as gamepad minimum | **Adopt.** Identity-mask streamer mode is the default; hide-markers stays opt-in after playtest; bound-key glyphs and 150 % scale (7.4, 7.7) | Masks the identity leak with zero art change; the marker leak is the viewer's own knowledge and is flagged, not solved |

**Informant marker reading (3.4 vs 2.9), resolved:** 3.4 describes outline-only
as a state of the marked player; 2.9 describes it as what the informant viewer
sees on allies. The marker shows the **marked player's** state (filled = active,
hatched = concealed, outline = informant), independent of the viewer. Section
2.9 needed the edit listed in the Amendments ledger (7.z, L2); applied
2026-10-24.

**Open questions for gameplay / UX GDDs (not decided here):**

1. **Informant marker visibility to active allies.** Do active players see an
   allied informant's marker, and in which state? Art default: yes, outline-only
   (the marked player's state), pending the gameplay and UX decision.
   *Answered 2026-10-25 (`design/gdd/teammate-markers.md`, informant ally rule; its OQ4 closed):* yes, outline-only, showing the marked player's state, for every viewer of the ally's team, active and informant alike.
2. **Hold-to-prepare toggle.** May a "toggle to prepare" alternative be added with
   identical UI (motor accessibility)? Does it change balance? (Open: a
   Settings & Accessibility (VS) decision; the indicator is identical either
   way.) *Answered 2026-10-24:* prepare is a **protected window from server
   acceptance**, not charge-then-armed (IIF UI-2, Assassination ASN-R3; 7.3
   row 4). *Toggle answered 2026-10-25 (`design/accessibility-requirements.md`):* hold-to-toggle for Run, Interact and Prepare (IIF R16), delivered at the Vertical Slice with the settings screen. The toggle sends the identical server command stream (T-05), so it does not change balance.
3. **Accessibility tier.** Standard (WCAG-style contrasts, remap, scaling,
   captions, colourblind options, Reduce screen effects) or Comprehensive?
   Record in `design/accessibility-requirements.md`.
   *Answered 2026-10-25 (committed in `design/accessibility-requirements.md`):* **Standard, with named elevations** (for example Reduce motion as three independent settings, High contrast mode, and the ally marker scale option). The elevations are listed in that file, not repeated here.
4. **Ping visibility.** *Answered 2026-10-25 (user decision, Informant Pings
   PNG-R1):* every ally on the sender's team sees the ring, Active and Informant
   viewers alike, in the same presentation (same ring shape, no extra halo);
   pinging is open to Active players too.
5. **Enemy removal notice.** Does the enemy team receive any removal notice?
   *Answered 2026-10-24:* **none** (HUD-R10; 7.3 note b), because a notice lets
   a viewer count eliminations by team.

### 7.z Amendments ledger

The single ledger of amendments that Section 7 and the resolved UX decisions
(7.x) require of other sections. Each line names the owning section, the change,
and its status. **All lines were applied on 2026-10-24** in the same revision
that renamed this ledger; the "Where" column is the section that now carries the
change. A new amendment is added as a new line with status "open" and is not
marked applied until the owning section is edited.

| # | Owning section | Amendment | Status |
| - | -------------- | --------- | ------ |
| L1 | **3.4 core glyph table** | (a) "Concealed: filled at 60 percent opacity" replaced with "filled with a two-band stepped hatch"; opacity is never a state encoding. (b) Outline-only is the state of the *marked player* (informant), independent of the viewer. (c) Marker keyline 3 px (other glyphs 2 px); ping keyline 3 px, 4 px beyond 20 m. (d) Marker: always on top, constant pixel size, edge-clamped variant when off-screen; default 28 x 32 px. (e) Sighting ping: world radius >= 4 m, 32 px screen minimum, static, no centre mark, stepped stroke age cue, expiry. (f) Three new glyphs added: Defence ring, Result stamp, Key glyph plate. (g) "Never thinner than 1.5 px" changed to 2 px; 5-repeat limit on stripe patterns recorded | applied 2026-10-24 |
| L2 | **2.9 Informant state** | Outline-only marker described as the informant's own marker as every ally sees it (allies shown in their own state); ping parameters added; note that 2.5's interaction grade is not applied to prepare; the false "flattest-contrast after the lobby" greyscale claim removed | applied 2026-10-24 |
| L3 | **4.6 UI palette** | Plates 85 percent opacity becomes 100 percent; cochineal-bright text rule (never text below 28 px at 1080p, only on an opaque plate at 28 px and above; danger text is Bone); re-measure of Bone on Cochineal Deep and Cochineal Bright on the final plate assigned to Section 8 (open item OI-3) | applied 2026-10-24 |
| L4 | **4.7 Colourblind safety** | Ally-accent list of 7.7 item 3 (to be simulated before adoption); audio and caption twins for each cue | applied 2026-10-24 |
| L5 | **4.8 Constraints** | 3 px marker keyline and the dual-edge test restated; plates 100 percent opaque; opacity encodes no state | applied 2026-10-24 |
| L6 | **2.12 tests** | The five tests of 7.7 added | applied 2026-10-24 |
| L7 | **5.2 viewer-state matrix** | Row "Active player concealed in a bush", teammate cell: "60 percent opacity" replaced with the two-band stepped hatch (missed in the first pass) | applied 2026-10-24 |
| L8 | **4.7 colourblind table** | Ally row: "filled / 60 percent opacity / outline-only" replaced with "filled / two-band stepped hatch / outline-only"; opacity encodes no state (missed in the first pass) | applied 2026-10-24 |
| L9 | **5.2 forbidden list** | Assassination-prompt parity sentence: the prompt appears identically for every valid target (NPC, opposing player, opposing informant) and **excludes the viewer's own allies**, per 7.1 item 4 (missed in the first pass) | applied 2026-10-24 |
| L10 | **3.3 stair table and 6.10 flag** | The 6.10 flag "stair rises are nominal" applied: 3.3 states 0.17 m and 0.22 m as nominal, built as 1.6 / n (0.178 m and 0.229 m, within 5 percent); the 6.10 flag marked resolved | applied 2026-10-24 |
| L11 | **4.2 P7 and 6.10 flag** | The 6.10 flag "P7 'doorways in shade' versus sealed door leaves" applied: P7 narrowed to upper-storey windows, the lobby mirror and UI plates; ground-floor door leaves specified in 6.2 M02 and 6.7, **now at L\* 34 to 38** because the earlier L\* 42 to 48 sat on the skin mean and failed Law 3b; the 6.10 flag marked resolved | applied 2026-10-24 |

**Deferral check (2026-10-24 reconciliation).** The active.md pending item
"art bible sections 3.4/2.9/4.6 amendments listed in 7.z were NOT applied" is
stale: L1 (3.4), L2 (2.9) and L3 (4.6) were verified in place in those sections
during the reconciliation pass (3.4 hatch, keylines, added glyphs; 2.9 outline
marker and ping parameters; 4.6 opaque plates and cochineal text rule). No
further edit was needed.

#### Change requests received (reconciliation 2026-10-24)

Dispositions: **APPLY** (edited in the named section), **ALREADY DONE**,
**DECLINE**, **PENDING-USER** (always-ask; not decided here). The wrong-target
tally wording was confirmed by the user 2026-10-25 (OQ-NKP-1 closed).

| From | Request | Disposition |
| ---- | ------- | ----------- |
| Penalty CR-16 (1) | 7.3 row 3 replaced by the wrong-target tally (UI-NKP-1) | APPLY (confirmed by the user 2026-10-25); 3.4 pip row also updated |
| Penalty CR-16 (2) | 7.3 row 8 drops "Penalty n/3" | APPLY |
| Penalty CR-16 (3) | 4.7 strike-counter row renamed "Wrong-target tally", triangle at n >= L - 1, tally / threshold tick twin | APPLY (confirmed by the user 2026-10-25); 4.7 audio-twin list updated |
| Penalty CR-16 (4) | 7.6 urgent entrance "tally square fill, threshold triangle"; rows for exile sequence, countdown, takeover fade-in | APPLY |
| Penalty CR-16 (5) | 7.7 "strike pips frozen" to "tally frozen"; row 15 in the Self-only list | APPLY |
| Penalty CR-16 (6) | 5.2.1 rule 3: takeover of an unseen crowd pawn; "a removal is not a body change" | APPLY |
| Penalty CR-16 (7) | 5.2 matrix gains the released exile body | APPLY |
| Penalty CR-16 (8) | 7.3 row 11 exile card at c + 0.8 s | APPLY (worded as plate show time + `ExileBeatSec`, normally c + 0.8 s, per HUD-R9) |
| Penalty CR-16 (9) | 2.9 grade on at the cut frame, never eased; fix duplicated item "4." | APPLY |
| HUD CR-HUD-8 (1) | 7.1 HUD rule, CD-approved wording | APPLY |
| HUD CR-HUD-8 (2) | 7.3 row 3 wrong-target tally | APPLY (same edit as Penalty CR-16 (1)) |
| HUD CR-HUD-8 (3) | 7.3 row 4 Ready / Pending / Protected / End / Cooling | APPLY |
| HUD CR-HUD-8 (4) | 7.3 row 5: own 3 steps "Your house", rival 2 steps "Rival house", team only, no "n/5" | APPLY; note (c), 3.4 and 4.7 aligned |
| HUD CR-HUD-8 (5) | 7.3 row 8 four outcomes, "Strike reversed" defender plate, 250 ms / 3 s / 600 ms | APPLY (HUD owns plate timing; the IIF UI-5 400 ms / 1.5 s / 600 ms timing stays with rejection plates only) |
| HUD CR-HUD-8 (6) | 7.3 row 11 timing per HUD-R9, preempts the plate over 0.25 s | APPLY |
| HUD CR-HUD-8 (7) | 7.3 row 12 removal line text, identical for every reason | APPLY |
| HUD CR-HUD-8 (8) | New rows: returning countdown (15), latency icon (16), third-person hint (17); 7.7 Self-only list | APPLY (no element beyond those the GDDs specify) |
| HUD CR-HUD-8 (9) | 7.6 "Preempt exit 250 ms stepped"; kill-confirmation row covers all outcomes and the defence plate | APPLY |
| HUD CR-HUD-8 (10) | 7.x question 5 answered: no enemy removal notice | APPLY (7.x and 7.3 note b) |
| Assassination VA-ASN-7 (1) / CR 10 | 7.3 row 4 charge-then-armed superseded by the protected window | APPLY (same edit as HUD (3)) |
| Assassination VA-ASN-7 (2) / CR 10 | 5.5 Brace row "held for the protected window, up to `DefenceDurationSec`; planted; NPCs same clip" | APPLY |
| Assassination VA-ASN-7 (3) / CR 10 | 7.3 row 8 `NoContact`, `Reversed`, defender "Strike reversed" | APPLY (same edit as HUD (5)) |
| Assassination VA-ASN-7 (4) | 5.5 Strike: three entry variants and the bush preface | APPLY |
| Assassination VA-ASN-7 (5) | 2.7 grade at contact, never before; no grade on a whiff | APPLY |
| Assassination VA-ASN-7 (6) | 5.2.1 rule 3 names VA-ASN-2 as the disappearance method | APPLY |
| IIF (Visual/Audio list, item 8) | 7.3 row 4 protected window, square = "protected now" | APPLY (same edit as HUD (3)) |
| IIF | 7.6 prepare-ring row and the "only linear motion" line (interaction ring sweep) | APPLY |
| IIF | 7.5 (i) defence-ring glyph | APPLY |
| IIF | 7.x C7: "armed" means acceptance; protection-ended cue | APPLY |
| IIF | 7.8 audio handoff (protection-ended cue, no ready-again cue) | APPLY; 4.7 twin list aligned |
| IIF | 5.5 Brace row | APPLY (same edit as Assassination (2)) |
| IIF | Reconcile 7.1 vs 5.2 on allies and the Strike hint | ALREADY DONE (L9: both exclude allies) |
| Camera CR 7 | Close the FOV-unconfirmed note (60 degrees confirmed) | APPLY (8.5, 4.8, 3.2 thumbnail note, 7.1) |
| Teammate Markers CR-7 | Outline-only keyline on both sides; anchor 195 cm | APPLY (3.4, 7.2) |
| Character Pool (1) | 1.3.1 / 1.3.8 balanced deck instead of duplicates | ALREADY DONE (cap justification sentence and GDD path added) |
| Character Pool (2) | 8.x.6 `TrimSwatchIndex`; remove `AccessorySetMask`; 4-sample cap note | APPLY (`TrimSwatchIndex` was already present; mask parameter removed; B channel unused, also in 1.3.7 item 15) |
| Character Pool (3) | 4.3 accessory variant garment swatch | ALREADY DONE (S(k) was stated; separate-mesh note added) |
| Character Pool (4) | 1.3.7 T2 arc or two rows; thresholds vs 1 / N_models; G-VAR; MVP kit on Manny | APPLY (T2 arc, <= 12 per capture; threshold 1 / N_models + 0.125; T7 G-VAR; MVP tier note) |
| Concealment CR 7 | 5.5: NPCs do enter bushes; rustle means "a body went in" | APPLY |
| Crowd CR 2 | 5.5 / 5.4 bush row; B8 shared contextual overlay; sitting not in MVP | APPLY |
| IVP CR 9 | 7.7 strip test (1) replaced by F8 (96 trials, fail at 16) | APPLY |
| LSC | 6.5 rule 6: 6 m court exclusion | APPLY |
| LSC | 6.4 / 6.7: 2.8 m clear-width minimum | APPLY (6.4 already stated it for alleys; 6.7 now states it for every corridor) |
| LSC | 6.7: a one-module jog is not a sightline break (offset >= clear width) | APPLY |
| LSC | 6.2: service alleys for Market and Temple | APPLY (closes LSC OQ7 for art) |
| LSC | 6.10: editor-only markers are the only debug art | APPLY |
| LSC | 2.5: pool ratio as data; 40 m check as a contract predicate (V-POOL-40) | APPLY |
| Objectives CR 7 | 7.3 element 5 (3 steps, rival `RivalPrelimCount` only), element 7 (Marigold after `TendCueDelaySec`), element 6 (prompt identical at every court) | APPLY |
| Generator CR 5 (a) | M01 lists 4 shape variants against the 3-variant budget | APPLY (frieze reclassified as a surface variant) |
| Generator CR 5 (b) | M05 mitred cornice | APPLY (shape detail of M05, not a new module) |
| Generator CR 5 (c) | Bay stacks and slab sizes as derived assets, not new variants | APPLY (classification); the gate question was decided by the user 2026-10-25: derived assets and G0 placeholders may be built before the 8.x.14 gate lifts (6.2) |
| Generator CR 5 (d) | 6.8 shadow-trace check vs LSC V-POOL | APPLY (V-POOL is the gate; the 6.8 check is the S11.3 repair target; 6.10 test 7 adjusted) |
| SCL CR 11 (a) | 5.4 B4 ">= 0.5 s at every pace, about 0.35-0.55 m" | APPLY |
| SCL CR 11 (b) | 5.4 rule 5 same cadence at the same screen height | APPLY |
| SCL CR 11 (c) | 5.4 B1 cadence by pace | APPLY |
| SCL CR 11 (d) | 5.4 B5 in-place pivot <= 150 degrees/s | APPLY |
| SCL CR 11 (e) | Step sound within +/- 50 ms of the foot plant at tier A | APPLY (5.4 rule 4 and 8.x.8) |
| SCL CR 11 (f) | 5.5 Brace: does not translate or turn while held | APPLY |
| SCL CR 11 (g) | 8.x.8 time-based B / C search intervals; tier D split | APPLY |
| SCL CR 11 (h) | Rename "crouch" to "brace" | APPLY (3.2 table, 1.3.5 pose set, 8.8 pose set) |
| 7.z L1-L3 (earlier deferral) | 3.4 / 2.9 / 4.6 amendments | ALREADY DONE (verified, see above) |
| art-director (consistency) | 4.7 Intel row "Fade-over-time" contradicted 7.2 item 2 | APPLY (age by stepped stroke weight) |
| ADR-0022 sync 2026-10-24 | 8.x.6 calls `M_Character_Master` "opaque" → one **Masked** master used only for the eye-mask dither (8.x.11's "no custom depth" note stays) | APPLY (8.x.6; 1.3 character spec opacity row clarified) |
| ADR-0022 sync 2026-10-24 | 8.x.14 spike R2 redefined: per-asset `SkinCacheUsage` `Enabled` vs `Disabled` on every pool LOD, project default left Exclusive (amend 8.x.7 too) | APPLY (8.x.14 R2 row, 8.x.7 skin-cache bullet) |
| ADR-0022 sync 2026-10-24 | 8.x.15 "Substrate on or off" and "draw-call budget and memory ceiling" answered (off; PROPOSED ceilings adopted); 8.x.6 Substrate note | APPLY (8.x.15 items marked answered; 8.x.6 Substrate bullet); recording ceilings in project config stays a pending user item |
| ADR-0022 sync 2026-10-24 | 8.x.16 F1: keep mesh-SDF traces through a `[GlobalIlluminationQuality@2]` override; never lower view distance below level 2 | APPLY (8.x.16 F1) |
| ADR-0013 sync 2026-10-24 | 8.x.8 "URO skip 1 / skip 3" → update-rate skip by external tick-rate control with engine URO off; AnimationBudgetAllocator not adopted; D-culled montages re-derived at promotion; 8.x.15 4 ms question answered (summed GT + workers ≤ 4.0 ms, GT ≤ 2.0 ms) | APPLY (8.x.8 table and tools/budget bullets; 8.x.15) |

Totals: 58 APPLY rows (57 requested plus 1 art-director consistency fix), 4
ALREADY DONE rows (6 items, counting L1-L3 separately), 0 DECLINE, 1
formerly PENDING-USER (Generator derived-asset gate; decided by the user
2026-10-25: allowed, 6.2).

## 8. Asset Standards

### 8.1 Scope and precedence

This section is the art-direction contract (what an asset must look like and
measure before it is accepted) joined to the technical-artist's hard engine
constraints (8.x). Engine: UE 5.8 (installed 5.8.4), Lumen, Nanite, Chaos, C++;
PC, 60 fps, RTX 3060-class, 1080p. The character budgets of 1.3.5 are adopted
unchanged as hard limits. The 2.0 m plan grid, 0.4 m sub-grid and 1.6 m course of
3.3 are the kit contract.

**Precedence.** Art-bible numbers (1.3.5, 3.3, 3.6, 4.3, 4.8) are the art
contract. Engine budgets in 8.x bind the production pipeline. Where the two
disagreed, the choice is recorded in the 8.x.13 table; the text of 8.2 to 8.10
below already reflects those choices. A number marked *proposed* or tagged
ASSUMED is a starting point that the technical-artist may tighten but not loosen
without art-director sign-off, and it needs either its named owner's acceptance
or the spike in 8.x.14.

**Evidence tags** (defined in 8.x.1): **VERIFIED-P** (art bible or project
config), **VERIFIED-E** (installed 5.8.4 source or engine reference),
**ASSUMED** (proposed, or an engine behaviour from memory). Tags are shown on
numbers that the technical-artist contributed; unmarked art-bible numbers are
VERIFIED-P.

### 8.2 File formats

| Stage | Format | Rule |
| ----- | ------ | ---- |
| Meshy download | **GLB** (glTF 2.0 binary) | Preferred. FBX only if a rig is Meshy-made (we do not use Meshy rigs). Never OBJ for textured work. |
| Blender working file | `.blend` | Kept in `assets/art/source/` beside the Meshy GLB; never imported by Unreal. |
| Blender to Unreal | **FBX 7.4 binary** | Skeletal meshes carry armature plus mesh; static meshes carry mesh plus `UBX_` / `UCX_` collision. glTF is not used for import to Unreal. |
| Textures, source | **PNG 8-bit** (BaseColor, Normal, ORM, masks); **TGA** allowed for 8-bit with alpha; **EXR 16-bit half** only for height, curvature and baked data maps; HDRI captures for the de-light test | No JPG anywhere in the pipeline (compression noise survives into BC7). |
| Unreal assets | `.uasset` only | Textures: BaseColor sRGB **BC7**, Normal **BC5**, ORM and masks linear (no sRGB). ORM is BC7 unless a quality test shows BC1 is acceptable (ASSUMED); the **required** swatch/trim region mask is BC1 at 512 x 512 (three channels: garment, trim, accessory; 1.3.5). Mip generation on; texture streaming on; **no virtual textures for characters**. |

Colour management: all albedo authored and exported as sRGB 8-bit; normals are
OpenGL-style (+Y up) from Blender and Unreal's **Flip Green Channel** is left
off. Any texture that fails to match this is re-exported, not patched in the
material.

### 8.3 Naming convention

**Unreal assets** use the engine prefix, PascalCase, no spaces, no
non-ASCII characters (Nahuatl names are transliterated without diacritics):
`[Prefix]_[Name]_[Variant]_[Suffix]`.

| Prefix | Asset | Example |
| ------ | ----- | ------- |
| `SK_` / `SM_` | Skeletal / static mesh | `SK_Citizen_03`, `SM_Kit_WallStreet_A_W1H2` |
| `T_` | Texture, with suffix `_D` BaseColor, `_N` normal, `_ORM`, `_M` swatch/region mask, `_H` height (trim sheets only) | `T_Citizen_03_D`, `T_Trim_Limewash_01_ORM` |
| `M_` / `MI_` | Master material / material instance. The pool has **one** master, `M_Character_Master`; its instances are `MI_Char_<Model>_<Variant>` with Variant = `Default`, `Swap` or `Accessory` (1.3.1) | `M_Character_Master`, `MI_Char_03_Swap` |
| `BP_` / `WBP_` | Blueprint / UMG widget | `BP_CrowdSpawner`, `WBP_TeammateMarker` |
| `NS_` | Niagara system | `NS_Dust_Street_Low` |
| `PA_` / `ABP_` | Physics asset / anim blueprint | `PA_Citizen` (see 8.6) |
| `L_` | Level | `L_Graybox` |

Kit mesh names encode footprint and height: `W` is width in 2.0 m cells, `H` is
height in 1.6 m courses, so `SM_Kit_WallStreet_A_W1H2` is 2.0 m wide, 3.2 m tall.
Swatch choice is carried by the instance's parameters (`GarmentSwatchIndex`,
`TrimSwatchIndex`, 8.x.6), set from the roster (1.3.10), not by the name.
**Exactly 24 `MI_Char_*` instances exist at full scope: 8 models x 3 variants
(default, palette swap, accessory) = 24 = the cap (VERIFIED-P)**, one instance
per appearance, so every appearance is instantiated and nothing is left over.
LOD meshes inside an FBX are suffixed `_LOD1`, `_LOD2`, `_LOD3`; collision `UCX_[MeshName]_01`
(`UBX_` for a box).

**Raw and source files** (outside Content) follow the studio form
`[category]_[name]_[variant]_[size].[ext]`, lowercase: `char_citizen03_albedo_1k.png`,
`env_wallstreet_a_lod0_large.fbx`. Category is one of `char`, `env`, `prop`,
`foliage`, `ui`, `vfx`.

**Folder layout** (`Content/StealthGame/`):

```text
Characters/Citizen/Model_01 ... Model_08/   SK_, T_, MI_ (one folder per base model)
Characters/Citizen/Shared/                  M_Character_Master, swatch data
Kit/Walls | Floors | Roofs | Temple | Water/ SM_Kit_*, grouped by function
Kit/Trims/                                  trim sheets and their materials
Props/ | Foliage/Bush/                      one-offs; the bush is its own family (3.6)
Materials/Master | Instances | Functions/
Textures/Shared/                            tiling and noise textures used by 2+ assets
UI/Widgets | UI/Textures | UI/Fonts
VFX/ ; Blueprints/ ; Maps/ ; Audio/ (audio-director)
```

No asset lives in `Content/` root or in `Developers/` in a merged build. Raw
sources live in `assets/art/source/[category]/[asset]/` with the provenance
sidecar of 8.9. The project's Manny skeleton and its physics asset stay at their
existing paths (the skeleton is never copied); the sample trees already in
`Content/` (MetaHumans, Paragon, UEFN_Mannequin, LevelPrototyping) are outside
this layout and are audited before the first package (8.x.12).

### 8.4 Texture resolution tiers

One density rule governs everything: **5.12 texels/cm (+/- 10 percent)**, the
same as characters in 1.3.5, so a 1024 map spans 2.0 m. Equal density keeps
crowd and world at one apparent sharpness; a crisp prop beside a soft wall draws
the eye, and the eye should be on heads (Pillar 2).

| Tier | Resolution | Use | Notes |
| ---- | ---------- | --- | ----- |
| Character | **1024 x 1024** | Every pool model (1.3.5) | BC7 / BC5 / ORM; **required** 512 swatch/trim mask (BC1). About 4.2 MB each, pool about 32 to 34 MB (under the 40 MB cap). |
| Prop S | 512 x 512 | Objects up to 1.0 m | Jars, baskets, small furniture. |
| Tiling surface | **1024 x 1024**, repeat **2.0 m** (about 4 MB per BC7 / BC5 / BC7 set) | Limewash, tezontle dado, packed earth, stone, water bed | Seamless; one ORM per surface. A 2048 map is used only if its repeat is 4.0 m. |
| Trim sheet | **2048 x 2048** spanning **4.0 m** of trim (about 16 MB per set) | Cornices, dado caps, door and window frames, painted bands | Horizontal strips on the 0.4 m sub-grid; 1.6 m course joins sit on a strip boundary. |
| Atlas | 2048 x 2048 | Decals, glyph-bearing wall panels, small prop sets, the shared leaf set | Packed with 8 px padding at the top mip. |
| Hero / monument | 2048 x 2048 maximum per material, up to 2 materials | Temple tiers, canoe, large carved reliefs | Still 5.12 texels/cm; bigger objects tile or use trim, not a larger map. |

**4096 maps are not used.** Banned for memory and density mismatch. Art caps:
no single non-character material above 3 maps of its tier, and no more than 2
unique (non-shared) 2048 maps per kit family. Memory ceilings per category are in
8.x.5 (kit total <= 96 MB, props <= 40 MB, bush and foliage <= 20 MB, streaming
pool <= 1.0 GB).

**UV fill check (character).** A dressed figure exposes about 2.0 to 2.6 m^2 of
surface (ASSUMED), which at 5.12 texels/cm fills 50 to 65 percent of a 1024 map
before padding; 75 percent is the practical packing ceiling. If a model cannot
pack at 5.12, cut hidden-face area first, never density; raising to 2048 is
rejected (4x memory). See conflict C1 in 8.x.13.

Colour rules from 4.3 and 4.8 apply at the texture level: world chroma stays
at or below **C\* 34** (30 for painted temple bands and for any albedo in hue
60 to 100); crowd-height backdrops are L\* 53 or lower, or L\* 69 or higher,
and in the leg and forearm zones L\* 38 or lower, or L\* 69 or higher (Law 3b);
no sun glitter, no high-frequency
specular noise (roughness maps stay above 0.35 except on water).

### 8.5 LOD expectations

**Pool character.** LOD0 12,000 +/- 2,000 tris; LOD1 <= 6,000; LOD2 <= 2,500;
LOD3 <= 900 (1.3.5). **Not Nanite** (8.x.4). Four LODs, one screen-size table for
players and NPCs. LOD silhouette within 2 percent of LOD0 area. Switching is
keyed by **figure screen height**, which is the contract, and **this table is
authoritative for every LOD threshold and distance in the document**: 1.3.5
and 5.6 defer to it (amended 2026-10-24); distances are derived
for the reference camera (1080p, 60 degree vertical FOV, 175 cm figure, screen
fraction about 1.5 / distance in metres; the 60 degree vertical FOV is
**confirmed** as the default in both camera modes by the Camera System GDD
(`FpFovVDeg` / `TpFovVDeg` = 60; reconciled 2026-10-24); a player who raises the
FOV makes figures smaller, and because LODs switch by screen height no rule
changes):

| LOD | Screen height of figure | Distance (reference camera) | Tag |
| --- | ----------------------- | --------------------------- | --- |
| LOD0 | >= 19 % (about 1640 / distance px) | to 8 m | ASSUMED |
| LOD1 | 11 to 19 % | 8 to 14 m | ASSUMED |
| LOD2 | 6.3 to 11 % | 14 to 24 m | ASSUMED |
| LOD3 | < 6.3 % | beyond 24 m | ASSUMED |

The 20 to 30 m reading range is therefore LOD2 (to about 24 m) then **LOD3
(about 24 to 30 m)**, and the outline must hold in both. **The 900-triangle
LOD3 mesh must keep the back-weighted head mass (at least 60 percent of hair
volume behind the ear plane) and the 14 cm leg gap at mid-stride**; these are
checked at LOD3 by T1 items 16, 22 and 23 (1.3.7), and the greyscale 30 m
silhouette test (T3) is run on LOD3 as well as LOD0. Animation tiers (8.x.8)
use the same screen-height key, so a figure's mesh and animation detail never
depend on whether it is a player or an NPC.

*Crowd split re-checked against these bands (2026-10-24).* The earlier
6 / 14 / 20 / 10 split (1.3.5, 8.x.2) was a scenario, not a consequence of the
bands. The bands and the 8.x.8 tier caps give a worst case of at most 8
characters at LOD0, at most 14 in the LOD1 band and the remaining 28 at LOD2
(everyone inside 24 m, none at LOD3): 8 x 14k + 14 x 6k + 28 x 2.5k, **about
266k triangles** (about 250k at the nominal 12k), an ASSUMED design-density case that holds only under the camera and spawn-density rule of OI-13 and counts against the 800k non-Nanite ceiling (8.x.2). That figure replaces 215k in
1.3.5 and 8.x.2.

**Other categories** (triangle ceilings, LODs and Nanite on or off): see the
table in 8.x.3. In short: **kit modules and large props are Nanite** (kit
authored up to 30k tris with a fallback mesh of <= 1,500; geometry is spent on the
3.3 angle vocabulary and bevels and reveals >= 4 cm that catch the Lumen sun and
shade band, not on carving); **small props, bushes and decorative foliage are
not Nanite** and carry hand LODs, because their materials need sway (period
>= 4 s, 3.6 and 2.8) or are too small for Nanite to pay off. The bush LOD2
silhouette stays within the 5 percent outline tolerance of 3.6. Temple monument:
Nanite, one tier per mesh so it can occlude, up to 150,000 tris per tier
(art-proposed; the technical-artist draft sets no monument figure, so this
awaits R1 and R7).

Non-Nanite skeletal characters get the identical LOD policy; any Nanite
skeletal experiment (1.3.5) is a separate decision gated by spike R6.

### 8.6 Export settings (Blender to Unreal)

| Setting | Value |
| ------- | ----- |
| Scene units | Metric, unit scale 1.0, length in meters. 1 Blender unit = 1 m = 100 Unreal units. |
| Transforms | Location, rotation and scale **applied**; scale exactly 1.0 and positive on all axes (no negative or non-uniform scale, which also flips Nanite winding, 1.3.2). |
| FBX scale | Apply Scalings = **FBX Units Scale**; Unreal import uniform scale 1.0, rotation 0, no import offset. |
| Axes | Up = Z, Forward = -Y (*proposed*, ASSUMED). **Outcome is the contract**: character faces Unreal +X, stands on Z = 0, imported at **165.6 cm** (165.6 Unreal units, the measured `SK_UEFN_Mannequin` stature, OI-1 closed 2026-10-25; *superseded:* the 175 cm working value) with import uniform scale 1.0 and no rotation. Derivation: the reference FBX is already at the reference stature, so the pipeline never rescales on import. Verify on the first exported UEFN round-trip (the Blender side of the preset is recorded by story 002 in `production/qa/evidence/body-base-evidence.md`), then freeze the setting in a Blender export preset. |
| Mesh | Tangent space MikkTSpace computed in Blender and **imported from file** (one method for the whole pool, frozen), Smoothing = Face, triangulated, modifiers applied, no custom split normals unless needed. |
| Armature | Add Leaf Bones **off**; deform bones only for LOD data; bone names identical to Manny. |
| Pivot, characters | Origin at ground level between the feet; mesh and armature share it. |
| Pivot, kit | **Grid corner**: minimum X, minimum Y, floor Z = 0, so a piece's origin is a multiple of 2.0 m. Pieces never pivot at centre. Rotation in 90-degree steps only. |
| Collision, characters | None in FBX; the shared physics asset (capsule-based) is assigned in Unreal. Use the project's existing `PA_Mannequin` if it passes the 1.3.5 physics-asset check, otherwise create `PA_Citizen`; there is exactly one. Mesh collision profile NoCollision or QueryOnly; movement and hit logic use the root capsule. |
| Collision, kit | `UBX_` / `UCX_` shapes, **4 or fewer per module**, convex hulls <= 32 vertices (ASSUMED), within 2 cm of the visible surface on any face a player can vault, hide behind or be assassinated against. Stairs: one convex wedge per run (civic 30 degrees, temple 40 degrees). Complex-as-simple is never used; Generate Missing Collision off. Floors: single box. |
| UV | UV0 only for texturing; **no UV1 on kit meshes** (saves vertex memory). Lightmap UV generation **off** (Lumen is dynamic; `r.AllowStaticLighting=0`, VERIFIED-P). |
| Materials in FBX | One slot per skeletal mesh. Kit modules **1 slot** (2 only where a trim material is separate); bush 2 (stone planter, leaves). Slot names match the target `MI_`. Unreal material and texture import off; instances assigned afterwards. |
| Unreal import, skeletal | Import Content Type = **Geometry and Skinning Weights** (VERIFIED-E enum); Skeleton = the existing Manny skeleton asset (never new); Import Animations **off**; Import Morph Targets **off**; Create Physics Asset **off**; LODs from `_LOD1` to `_LOD3` in the FBX, one method for the pool. |
| Unreal import, static | Build Nanite **on** for modules and large props; Auto Generate Collision **off**; Generate Missing Collision **off**; Generate Lightmap UVs **off**; Combine Meshes on; Remove Degenerates on; import `UBX_` / `UCX_` shapes. |

### 8.7 Meshy to Blender to Unreal normalisation

**Time budget:** target **4 hours** per character model, hard cap **8 hours**. A
model that exceeds the cap is regenerated or cut, not rescued. If 3 of the
first 6 models fail the Body_Base conform test, switch to the fallback of
1.3.1 (Meshy heads and garments on the one canonical body).

#### Characters (to the Manny skeleton)

0. **Prerequisite gate (no Meshy credit before it, the single test character included).** (a) OI-1 is closed (hard prerequisite, 1.3.2: the measured stature, bone count and chain lengths are recorded in 1.3.2 and 1.3.5; no working value is accepted in its place), and
   the measured values are copied onto the model's sign-off
   sheet; (b) the canonical **Body_Base** is built at the measured stature and accepted (1.3.9, the
   first Blender-only deliverable); (c) the model's roster row (1.3.10) is
   chosen, giving its cut, hair form, accessory set and swatches.
1. **Prompt and generate.** Confirm cost with the user. Full-body figure, A-pose,
   arms 30 to 45 degrees from the torso, flat sandals (replaced in Blender by the shared cactli sandal piece of 1.3.10; never bare feet, a barefoot model is a tell), matte
   undyed cloth, no carried items, no jewellery metal, no face paint (1.3.3, 1.3.6).
   Use the pose option if the tool offers one. Record the task id (8.9).
2. **Import the GLB** to a new `.blend`. Collection layout: `Meshy_Original`
   (kept, hidden, high-poly), `Work`, `Body_Proxy` (the accepted Body_Base of 1.3.9, never exported),
   `Manny_Ref` (project Manny mesh and armature, never exported).
3. **Scale to stature.** Apply one uniform scale so sole-to-crown (hair and
   headwear excluded) equals the measured reference stature: **165.6 cm +/- 2
   (163.6 to 167.6)**, `SK_UEFN_Mannequin`, OI-1 closed 2026-10-25 (derivation:
   1.3.2 OI-1 block; *superseded:* the 175 cm working value). Apply scale. Then
   check head height 19.3 cm +/- 1, shoulders 51.0 +/- 2, chest depth
   27.6 +/- 2, hips 34.8 +/- 2 (re-derived from the chains, 1.3.2). Fix
   proportion errors by sculpt or shrinkwrap to Body_Base, **never** by
   non-uniform scale.
4. **Ground and face.** Soles at Z = 0 (+/- 0.5 cm), origin between the feet,
   figure faces the axis that exports to Unreal +X.
5. **Match the rest pose.** Pose the figure to Manny's reference A-pose using
   `Manny_Ref` as an overlay; every bone within 1 degree and 0.5 cm (1.3.5).
6. **Retopologise** to 12k tris (QuadriFlow or manual, then triangulate), about
   8k vertices (ASSUMED ratio 0.65 per tri with UV seams). Edge loops: at least
   3 around shoulder, elbow, knee, hip and wrist. Delete faces hidden under
   garments. Garment envelope, hem and head limits per 1.3.3.
7. **UV and density.** One UV0 layout, no overlaps, 8 px padding at 1024; scale
   islands so median density is 5.12 texels/cm (4.6 to 5.6); face and hands
   share the same density. Run the **UV fill check** of 8.4: at about 75 percent
   packing, reduce hidden-face area, never density; a model that still does not
   pack counts against the 8-hour cap.
8. **Bake and de-light.** Bake BaseColor, normal and AO from `Meshy_Original`.
   Strip baked light: albedo is flattened to the grey-cotton base with detail
   as **luminance around the garment mean**, so the swatch colour comes from
   the master material (4.3) and not from the texture. Check the neutral-HDRI
   gradient: under 10 percent across any 20 cm region. Write ORM: roughness in
   the 1.3.4 bands per region, metallic **0**, no emissive.
9. **Rig.** Parent to a copy of the Manny armature (names untouched, **no extra
   bones**). Transfer weights with a nearest-face-interpolated Data Transfer
   from the Manny body mesh, limit to **4 influences**, normalise, then
   hand-correct at shoulders, hips, neck. The project sets
   `r.GPUSkin.UnlimitedBoneInfluences=True` (VERIFIED-P), which adds a second
   skinning path; spike R8 decides whether it stays on. Run the pose set (walk,
   run, idle, brace, assassination, vault, reversal counter): intersections under
   1.0 cm, no joint twisting beyond 15 degrees.
10. **LODs.** Decimate to the 1.3.5 ceilings with weights preserved; strip
    fingers and twist bones at LOD2 and below, identically on every model.
11. **Export** one FBX: armature, LOD0 to LOD3 meshes, no animation.
12. **Import to Unreal** (through the Unreal MCP) with the settings of 8.6:
    *Skeleton = the project's Manny skeleton asset* (never create a new one),
    Geometry and Skinning Weights, Import Animations off, Import Morph Targets
    off, Import Materials and Textures off, Create Physics Asset off then assign
    the shared physics asset, **Nanite off**, no morph-target or spline-mesh
    usage flags on the material. Set **skin-cache opt-in on the mesh** (the
    project default `r.SkinCache.DefaultBehavior=0` is Exclusive, which excludes
    every mesh that does not opt in; spike R2 may flip the default instead).
    Import the four textures separately (`_D` BaseColor, `_N` normal, `_ORM`, and the required 512 x 512 BC1 linear `_M` swatch/trim region mask, 1.3.5 and 8.2), set compression per 8.2, create
    `MI_Char_<Model>_<Variant>` from `M_Character_Master`.
13. **Audit.** Run T1 (1.3.7), fix, then T2 to T4. Retain captures in
    `production/qa/evidence/`. Sign off on the model's checklist.

#### Kit modules (2.0 m grid)

Meshy is used for **ornament and one-off detail** (relief panels, carved caps,
props); structural bays, floors, stairs and walls are built from primitives in
Blender so they land on the grid exactly. A Meshy piece is always placed inside
a primitive frame.

1. **Generate or model** at roughly real size; for Meshy output, drop back-face and hidden
   geometry and any ground plate or base the model generated.
2. **Dimension to the grid.** The bounding box of the **unprojected cell** (the
   0.4 m talud and cornice projections are excluded, 6.2) has width and depth
   that are multiples of **2.0 m** (+/- 0.5 cm), height a multiple of **1.6 m**
   (the 0.9 m M04 cap and the 0.8 m and 1.6 m low walls are the listed
   exceptions), thin walls 0.4 m sub-grid steps (never under 0.2 m). Join faces sit exactly on grid planes so
   neighbours need no scale or deformation. The shell is **closed and
   watertight** (open or thin walls leak Lumen GI).
3. **Pivot at the grid corner** (minimum X, minimum Y, Z = 0), then apply all
   transforms.
4. **Clean up.** Merge by distance (0.1 mm), triangulate, remove interior
   faces, check the angle vocabulary of 3.3: a new angle needs art-director
   sign-off. Bevel reveals >= 4 cm. Authored triangles <= 30k (typical 2k to
   20k) plus a **fallback mesh of <= 1,500 tris** for Lumen / SDF and non-Nanite
   fallback. Run the static-figure test (3.3) on any upright prop in the 1.5 to
   2.2 m eye band.
5. **UV** with world-aligned projection onto the tiling material, or place
   islands on the trim sheet strips; no per-piece unique maps unless it is a
   hero or monument piece (8.4). UV0 only.
6. **Collision.** Build `UBX_` / `UCX_` shapes (8.6).
7. **Export FBX**, import to Unreal with Nanite on, static mesh collision from
   the authored shapes, lightmap UV off, instances assigned. The material is
   opaque, with no WPO, no masked opacity and no pixel depth offset (the Nanite
   slow path); modules share <= 4 master instances.
8. **Snap test.** Place the module with a copy at +2.0 m and +1.6 m in the
   graybox: no gaps, no overlap, no z-fighting, light does not leak at the join.

### 8.8 Per-asset acceptance checklist

Every asset passes this common list; characters also pass the 25-point list of
1.3.7 and the bush family also passes 3.6.

1. Name, folder and prefix match 8.3; source files and provenance sidecar exist.
2. Scale 1.0; correct pivot (feet for characters, grid corner for kit); real-world size within 1 percent.
3. Triangles within the category ceiling of 8.5 and 8.x.3; no hidden or interior geometry.
4. Material slots within limit (pool 1, kit 1 or 2 with a separate trim, bush 2); every slot is a `MI_` of an approved master.
5. Texture tier and density per 8.4; formats and compression per 8.2.
6. Albedo free of baked light; no pure white, black, saturated colour or excluded hue (4.3, 4.8).
7. Colour and value in range (world C\* <= 34, <= 30 in hue 60 to 100; crowd-height backdrop L\* <= 53 or >= 69, and L\* <= 38 or >= 69 in the leg and forearm zones).
8. Reads in a greyscale 30 m in-engine capture; not mistaken for a person in more than 5 percent of trials (kit/props).
9. Collision correct (kit); no complex-as-simple.
10. Appears correctly under Lumen sun and shade at the 2.4 baseline lighting; no emissive unless the asset is allow-listed by Section 2.
11. Captured in-engine screenshot retained in `production/qa/evidence/`.
12. Art-director sign-off recorded (name, date) next to the provenance entry.
13. **Automated gate before commit** (a validator is a candidate tool, technical-artist): tris within contract per LOD; one material slot (pool); <= 4 influences; bone hierarchy and names equal to Manny; pivot at origin; no UV overlap on pool meshes; texture dimensions power of two; no alpha on pool materials; no parameter, switch or MPC entry named `Team`, `Role`, `IsPlayer`, `IsNPC` or `Ally` (8.x.6); no `bRenderCustomDepth` or stencil value on any pool or bush primitive (8.x.11).

### 8.9 Provenance and licensing of AI-generated assets

Every Meshy-derived asset has a sidecar `[asset].prov.json` in its source
folder and a mirrored Asset Registry tag (`MeshyTaskId`) in Unreal, recording:

- Meshy task ids for each step (text-to-3D preview, refine, remesh, retexture, image-to-3D), the `ai_model` and settings used, and credits spent.
- The full prompt, negative prompt and any reference image (with its rights status).
- Generation date, operator, Meshy plan tier **at generation time**.
- Blender source path, summary of human edits (retopo, rebake, rig), and checklist sign-off.

Rules:
1. Prompts may not name living artists, existing games or other IP, and
   reference images must be owned, licensed or generated by us.
2. Rights depend on the Meshy plan at the time of generation (free-tier output
   has historically been attribution-licensed; paid tiers assign ownership).
   The producer must verify Meshy's current terms before any asset ships and
   record the result; an asset with no recorded plan tier cannot ship.
3. Keep the unedited Meshy GLB. If rights are ever questioned, it is the evidence.
4. The ledger feeds the AI-content disclosure needed at store-page time;
   producer owns that task. This note is process guidance, not legal advice.
5. Cultural references (1.4) are checked by the same sign-off: AI output drifts
   toward generic "Aztec" costume clichés (feather headdresses, jaguar
   warriors); any such element is rejected, not repainted.

### 8.10 What to reject

Reject (send back or cut) any asset that shows:

- Scale or pivot errors: non-1.0 scale, off-grid kit bounds, centre pivot, feet below or above Z = 0.
- Baked shadows, highlights or ambient occlusion in albedo; vertex colours used as baked light.
- Metal, gloss, emissive or glow on any character; Subsurface, cloth or clear coat shading.
- Alpha-card hair, translucent cloth, or a bush whose LOD0 or LOD1 leaves are masked cards. Masked or opaque-cluster cards are accepted **only at bush LOD2**, and only if the LOD2 silhouette stays within the 5 percent outline tolerance of 3.6. A bush that departs from the 3.6 specification (closed dome on the 2.0 m planter, three leaf tiers, two-value read) is rejected, and so is a *decorative* plant that reads as a dome (the Dome Ban of 3.6).
- Extra bones, cloth or physics bones, non-Manny bone names, more than 4 influences, retargeted scale.
- Character height, build, hem, garment width or accessory outside the 1.3 envelope; carried items; role costume; face paint; feather headdresses.
- Colour outside the swatch table, a turquoise/teal or saturated red element, or any chroma above the cap.
- Textures at 4096, JPG-sourced maps, mismatched texel density, stretched or overlapping UVs, visible seams at the 2.0 m repeat.
- Unique silhouettes in a kit piece visible from 20 m that act as a landmark; any upright that fails the static-figure rule.
- Meshy artefacts: floating fragments, melted hands, asymmetric faces, noisy surface "skin" that survives retopo, text or glyph garbage in textures.
- **Keyed branches (Pillar 1):** any material parameter, static switch, material function, MPC entry or MID creation path named for or keyed by player, NPC, team, role, objective, kill count or "is local"; and any outline, stencil, custom-depth or post-process highlight on a pool or bush primitive.
- Engine-contract breaks: Nanite enabled on a pool character (until R6 says otherwise), WPO or masked opacity in the kit master, a kit module with an open or thin shell, lightmap UVs, UV1 on kit meshes, `Used with Morph Targets` or `Used with Spline Meshes` on `M_Character_Master`.
- Missing provenance, unrecorded plan tier, or prompts naming IP or living artists.
- Waivers: there are none for characters (1.3.7); a kit exception needs the art-director's written sign-off.

### 8.x Technical constraints and resolved trade-offs

This sub-section is the technical-artist's hard-constraint draft, condensed and
reconciled with 8.1 to 8.10. Where it disagrees with an earlier sub-section, the
decision is in the 8.x.13 table and the earlier text has been rewritten to match.

#### 8.x.1 Evidence tags

- **VERIFIED-P**: stated in the art bible or project config (`Config/DefaultEngine.ini`, `project.yaml`, `technical-preferences.md`).
- **VERIFIED-E**: read from the installed 5.8.4 engine source or headers in `C:/Program Files/Epic Games/UE_5.8/Engine`, or from `docs/engine-reference/unreal/`.
- **ASSUMED**: a proposed number or an engine behaviour from memory, not confirmed for 5.8. Needs the owner named in 8.x.15 to accept it or a spike in 8.x.14 to prove it.

`docs/engine-reference/unreal/` was written against 5.7. Where it disagrees with
the installed 5.8.4 source, the source wins and the disagreement is stated.

#### 8.x.2 Budget summary (all categories)

| Category | Budget | Tag | Budget it serves |
| --- | --- | --- | --- |
| Frame | 16.6 ms at 60 fps, 1080p | VERIFIED-P | everything |
| Animation, all characters | <= 4 ms (about 67 us per character average at 60 characters; working value since `NpcCount` 50 (user decision 2026-10-25), re-verify in spike R3) | VERIFIED-P at 50 characters | CPU frame |
| Server AI + nav | <= 1.5 ms | VERIFIED-P | server tick |
| Character tris, 50 on screen worst case | about 266k (8 LOD0 at 14k + 14 LOD1 + 28 LOD2, from the 8.x.8 tier caps and the 8.5 bands; about 250k at the nominal 12k) | ASSUMED (design-density case: arithmetic exact, the 8 / 14 / 28 caps depend on the camera and spawn-density rule, OI-13) | GPU geometry |
| Character LOD tris | 12k +/- 2k / <= 6k / <= 2.5k / <= 900 | VERIFIED-P | GPU geometry |
| Character texture memory | <= 4 MB per model, < 40 MB for pool of 8 | VERIFIED-P | VRAM |
| Texture pool, whole game resident | <= 1.0 GB streaming pool (`r.Streaming.PoolSize`) | ASSUMED | VRAM (3060 has 12 GB; leaves room for 6-8 GB cards) |
| Draw calls, main view, non-Nanite | <= 1,500 (character, prop, foliage, bush combined) | ASSUMED | CPU render thread (`technical-preferences.md` leaves this `[TO BE CONFIGURED]`) |
| Nanite unique materials in view | <= 12 | ASSUMED | Nanite shading bins |
| Non-Nanite scene tris on screen (props, foliage, bushes, characters) | <= 800k before shadows, characters included (about 266k characters, design-density case; props, foliage and bushes share the remaining about 534k) | ASSUMED | GPU geometry |
| Nanite source tris per kit module | <= 30k authored, no runtime cap beyond Nanite | ASSUMED | build time, disk |
| Runtime kit instances (whole city) | <= 5,000 | ASSUMED | Lumen scene, Chaos bodies, spawn time |
| Master-material pixel instructions (Shader Complexity viewmode) | character <= 150, kit <= 120, foliage/bush <= 140 | ASSUMED | GPU pixel cost |
| Cooked package size | <= 3 GB installed | ASSUMED (producer to confirm) | distribution |

#### 8.x.3 Triangle and vertex budgets per asset category

| Category | LOD0 | Lower LODs | Nanite | Tag |
| --- | --- | --- | --- | --- |
| Character (skeletal, pool) | 12,000 +/- 2,000; about 8k vertices (ASSUMED ratio 0.65 vertex/tri with UV seams) | 6,000 / 2,500 / 900 | Off (8.x.4) | VERIFIED-P (tris) |
| Kit module (wall, floor, stair, platform, trim), one 2.0 m bay | <= 30k authored (typical 2k to 20k) | Nanite builds its own hierarchy; also a **fallback mesh at <= 1,500 tris** for Lumen/SDF and non-Nanite fallback | On | ASSUMED |
| Large prop (planter, stall, canoe, stele at monument scale) | <= 8k | LOD1 <= 3k, LOD2 <= 1k | On if > 2k tris and opaque | ASSUMED |
| Small prop (jar, basket, crate) | <= 1.5k | LOD1 <= 500 | Off (Nanite fixed overhead outweighs the saving under about 300-500 tris) | ASSUMED |
| Bush (planter + dome, one family, up to 2 variants) | <= 6k incl. planter; leaves as real opaque geometry | LOD1 <= 2.5k, LOD2 <= 800 (cards allowed at LOD2 only); LOD2 silhouette within the 5 percent outline tolerance of 3.6 | Off | ASSUMED |
| Decorative foliage: maguey/agave rosette | <= 3k | 1.2k / 400 | Off | ASSUMED |
| Decorative foliage: maize/reed cluster | <= 2k | 800 / 300 | Off | ASSUMED |
| Decorative foliage: ahuehuete tree | <= 12k (trunk 3k + canopy) | 5k / 1.5k; no impostor or billboard, so the flat-canopy silhouette of 3.6 holds at distance | Off | ASSUMED |
| Ground cover (<= 0.6 m) | <= 600 | 200 | Off | ASSUMED |
| Temple monument tier | up to 150,000 (art-proposed, one tier per mesh) | Nanite hierarchy | On | ASSUMED; technical-artist draft is silent, awaits R1 and R7 |

Reason foliage and bushes stay off Nanite: `r.Nanite.Foliage` defaults to 0 in
the installed source (VERIFIED-E, `NaniteResources.cpp`), the foliage and bush
materials need world-position-offset sway (period >= 4 s, 3.6 and 2.8), and
masked leaf cards are the expensive Nanite case. Nanite WPO exists in some form
in 5.8 (`bEvaluateWorldPositionOffset` is a `UStaticMeshComponent` property,
VERIFIED-E) but `rendering.md` says "No world position offset", which
contradicts it; treat Nanite + WPO as UNVERIFIED and do not rely on it (R9). The
bush at <= 6k tris must still carry the three-tier leaf read of 3.6; if it
cannot, larger leaf clusters carry the tiers, not a higher triangle count.

#### 8.x.4 Does Nanite apply to skeletal meshes in 5.8?

- **Capability exists in the installed engine (VERIFIED-E):** `USkeletalMesh` has `NaniteSettings`, `IsNaniteEnabled()`, `HasValidNaniteData()` and Nanite assembly support; `r.Nanite.AllowSkinnedMeshes` exists, default 1, read-only; `NaniteSkinnedMeshesSupported()` gates it; `USkinnedMeshComponent` has `bDisallowNanite` and `bForceDisableNanite`.
- **The engine reference is stale on this point:** `rendering.md` lists "No vertex animation (skeletal meshes)" as a Nanite limitation. That contradicts the installed source. Correct that file when the project next updates it.
- **Not known (UNVERIFIED):** production-readiness in 5.8, whether it is still Experimental/Beta, its restrictions (morph targets, bone-influence cap, material features, LOD fallback), and its interaction with motion matching and the Game Animation Sample. Not looked up because the cost is out of reach without the release notes.
- **Decision:** pool characters are **non-Nanite classic skeletal meshes with 4 authored LODs**. This matches the existing 1.3.5 position, and 12k tris is far below where Nanite pays off. `Enable Nanite Support` stays unchecked on every pool mesh. Revisit only after spike R6.

#### 8.x.5 Texture memory

| Asset | Format | Size and cost | Tag |
| --- | --- | --- | --- |
| Pool BaseColor | BC7, sRGB | 1024^2, about 1.33 MB with mips | VERIFIED-P |
| Pool Normal | BC5 | 1024^2, about 1.33 MB | VERIFIED-P |
| Pool ORM | BC7 or BC1 (linear) | 1.33 MB (BC7) or 0.67 MB (BC1); take BC7 unless a quality test passes BC1 | VERIFIED-P / ASSUMED |
| Pool swatch/trim region mask (**required** 4th texture) | BC1, 512^2, three channels (garment, trim, accessory) | about 0.2 MB | ASSUMED |
| Pool total | 8 models | about 32-34 MB, inside the 40 MB cap; palette swaps are MI parameters, no extra textures | VERIFIED-P (arithmetic) |
| Kit tiling sets | **1024^2** BC7 / BC5 / BC7, about 4 MB per set (2.0 m repeat, 5.12 texels/cm) | counted in the kit total below | ASSUMED |
| Kit trim sheets and atlases | 2048^2 BC7 / BC5 / BC7, about 5.3 MB per map, about 16 MB per set (4.0 m span, 5.12 texels/cm) | counted in the kit total below | ASSUMED |
| Kit total | tiling sets + trim sheets + atlases + hero maps | <= 96 MB (the technical-artist's ceiling of 6 sets at 16 MB; the art cap of 2 unique 2048 maps per kit family still applies) | ASSUMED |
| Bush and foliage | one shared 2048^2 leaf atlas set plus one bark/stone set | <= 20 MB | ASSUMED |
| Props | 512-1024 per prop family, sharing a trim sheet where possible | <= 40 MB | ASSUMED |
| Streaming | Texture streaming on; virtual textures off for the pool | pool mips stream down to 512 at normal play range | ASSUMED |

Screen-space check on the 1024 choice: at 1080p, a 1.7 m figure at 5 m with a
60 degree FOV is about 1.9 px/cm, against 5.12 texels/cm, so mip 1 is already
enough. The 1024 set is memory-safe because streaming drops it. Texel density is
therefore a cap on authoring, not on VRAM.

#### 8.x.6 Materials, slots and instruction counts

- **Slots:** 1 per pool model (VERIFIED-P). 1 slot per kit module (ASSUMED; 2 only where a trim material is separate). Bush: 2 slots max (stone planter, leaves) (ASSUMED).
- **One master for the pool: `M_Character_Master`**, **Masked** blend mode used **only** for the camera eye-mask dither (opacity mask = `DitherTemporalAA` of the custom primitive data slot `CPD_EyeMaskAlpha`, default 1.0 = fully drawn, clip 0.5; ADR-0022 §3, ADR-0015), so it is opaque in appearance at α = 1; Default Lit, legacy (non-Substrate) shading, no translucency, no WPO, no pixel depth offset, no static switches that multiply permutations. *(Was "opaque"; amended at the ADR sync 2026-10-24. The masked path's extra cost is checked by ADR-0022 RC-3, ≤ 0.3 ms GPU at 60 characters; if it fails, the fallback is global, never a per-pawn material swap.)* Instances: `MI_Char_<Model>_<Variant>`, **exactly 24 at full scope (8 models x 3 variants = the VERIFIED-P cap)**; one instance per appearance.
- **MI parameters allowed:** `BaseColorTex`, `NormalTex`, `ORMTex`, `SwatchMaskTex` (the required region mask: R garment, G trim; B unused), `GarmentSwatchIndex` (0-7: S(k) or S(k+1)), **`TrimSwatchIndex` (0-7: S(k+3), mandatory in every variant)**. **`AccessorySetMask` is removed** (reconciled 2026-10-24, CPA CP-4): accessory variants are separate merged mesh assets, so no accessory region or parameter exists in the master. The 4.3 assignment (default S(k), swap S(k+1), trim S(k+3) in every variant, accessory variant = garment S(k) + trim S(k+3) on the accessory mesh) is fully expressible per instance: the garment region takes the garment swatch and the trim region takes the trim swatch. **Sample budget:** with BaseColor, Normal, ORM and the garment/trim mask the master is at its cap of 4 texture samples with no headroom (technical-artist to confirm). Colour values come from the 8-swatch table in 4.3, not chosen here. Per-instance values are the roster of 1.3.10.
- **No keyed branches (rule, Pillar 1):** no parameter, static switch, material function, MPC entry, or MID creation path may be named for or keyed by player, NPC, team, role, objective, kill count, or "is local". A shared MI is assigned by appearance only. Reject at material review if `Team`, `Role`, `IsPlayer`, `IsNPC`, `Ally` appear in any parameter name.
- **Instruction budget (ASSUMED):** pool <= 150 pixel instructions, <= 4 texture samples, <= 1 interpolated UV set. Kit master <= 120 and Nanite-friendly: no masked opacity, no pixel depth offset, no WPO. Foliage/bush master <= 140 with WPO sway and masked leaves (LOD2 cards).
- **Substrate: off** (decided by ADR-0022 §1, VERIFIED-E: `r.Substrate` defaults to 0 and the project has no key, so this project runs legacy shading; pinning `r.Substrate=0` explicitly is a pending user config item). Instruction counts are measured on the legacy path.
- **Permutation control:** `M_Character_Master` is the only skeletal-mesh material, so a shader compile pass for GPU-skin variants runs once. Do not use `Used with Morph Targets` or `Used with Spline Meshes` on it.

#### 8.x.7 Skeleton, bones, influences, skin cache

- **Skeleton:** the project's Manny skeleton (`Content/Characters/UE5_Mannequins/Meshes/SK_Mannequin`), unmodified, zero extra bones (VERIFIED-P). The bone count is not read here: measure it at pool setup (OI-1, owner technical-artist), record it in 1.3.5, and treat it as the same number for all models thereafter. Do not quote a number from memory.
- **Influences:** <= 4 per vertex (VERIFIED-P). The engine's default skin stream carries 4 (`MAX_INFLUENCES_PER_STREAM 4`, VERIFIED-E) and the hard ceiling is 12 (`MAX_TOTAL_INFLUENCES`, VERIFIED-E). The project sets `r.GPUSkin.UnlimitedBoneInfluences=True` (VERIFIED-P), which adds a second skinning path. Meshes at <= 4 should stay on the cheap stream under `r.GPUSkin.UnlimitedBoneInfluencesThreshold` (ASSUMED: the threshold's default value is `EXTRA_BONE_INFLUENCES`, not read). Action: turn the project flag off unless a mesh needs more, or prove in spike R8 that it costs nothing at 4.
- **Skin cache conflict (VERIFIED-P + VERIFIED-E):** the project sets `r.SkinCache.DefaultBehavior=0`, which the source defines as **Exclusive: all skeletal meshes are excluded from the skin cache unless each opts in**. With Virtual Shadow Maps and 60 characters, an excluded mesh re-skins in every pass (depth, base, each shadow view). Decided by ADR-0022 §1: the project default stays Exclusive and **one skin-cache value is set per asset on every LOD of every pool mesh** (`SkinCacheUsage`, provisional `Enabled`; `Auto` or mixed values rejected by the pool validator). Spike R2 picks `Enabled` vs `Disabled`.
- **Morph targets and cloth:** none on the pool (no extra bones, no simulation, 1.3.5). Import with morph targets off.
- **Physics asset:** one shared capsule asset (`PA_Mannequin` already exists in `Rigs/`, VERIFIED-P path; verify it meets 1.3.5, else `PA_Citizen`). Mesh collision profile = NoCollision or QueryOnly; movement and hit logic use the root capsule, so 60 characters do not add physics-body overlap cost.

#### 8.x.8 Animation LOD, update rate and significance

Rule: **all budgets are keyed by distance, on-screen size and render state only.
No AI-keyed, role-keyed, or player-keyed term exists.** One shared function, one
asset set, identical for players and NPCs. Tiers are keyed by figure screen
height (the same key as the mesh LODs in 8.5); the distance column is computed
for the reference camera of 8.5 and replaces an earlier column that assumed a
different FOV.

| Tier | Condition (screen height of the figure) | Pose update | Notes | Tag |
| --- | --- | --- | --- | --- |
| A | >= 19 % (about 0-8 m) | every frame, full motion matching | LOD0; <= 8 characters at this tier | ASSUMED |
| B | 8-19 % (about 8-19 m) | update-rate skip 1, motion-matching search every `AnimSearchIntervalTierBSec` (a **time interval**, default 0.067 s, frame-rate independent; SCL R11) | LOD1, then LOD2 | ASSUMED |
| C | 3-8 % (about 19-50 m) | update-rate skip 3, interpolated, search every `AnimSearchIntervalTierCSec` (default 0.133 s) | LOD2 then LOD3, fingers and twist bones stripped (1.3.5) | ASSUMED |
| D-visible | < 3 % (beyond about 50 m) and rendered | locomotion and stops only, at >= `AnimTierDVisibleMinHz` (8 Hz) minimal pose updates; no head-look, no foot IK. **No rendered pawn is ever unposed or frozen** | LOD3 | ASSUMED |
| D-culled | not rendered (off-screen or occluded for 0.2 s) | no pose tick or evaluation on clients (external tick-rate control; montages do not advance while culled, and on becoming visible the pose is evaluated in the same frame with the montage position re-derived from replicated state, so the visible result is unchanged; ADR-0013 §3) | none | ASSUMED |

(Split of tier D and time-based search intervals reconciled 2026-10-24 from
SCL R11 / change request 11 (g).) **Step-sound sync:** at tier A the step sound
lands within +/- 50 ms of the visible foot plant, identically for every pawn
(5.4 rule 4).

- **Tools present in 5.8.4 (VERIFIED-E):** `bEnableUpdateRateOptimizations` on `USkinnedMeshComponent`; `EVisibilityBasedAnimTickOption`; the `AnimationBudgetAllocator` plugin (Runtime, not enabled by default, not flagged Beta or Experimental in the `.uplugin`). **Decided by ADR-0013 (ADR sync 2026-10-24):** the "update-rate skips" above are implemented by **external tick-rate control with engine URO off** on every pawn, because engine URO is keyed to player-controlled pawns in 5.8.4 (a host-side tell); the AnimationBudgetAllocator is **not adopted**. The significance input stays a screen-size function only.
- **Budget math (ASSUMED):** 8 x 120 us + 14 x 80 us + 28 x 30 us = about 2.9 ms, leaving about 1.1 ms for the rest of the 4 ms. The caps (8 / 14 / 28 characters) are budgets, not predictions of the tier counts. The 4 ms is defined by ADR-0013 §5 as **summed** game-thread + worker animation time, with a game-thread share ≤ 2.0 ms.
- **Dedicated server:** characters need no pose; only the montages and root movement that gameplay reads. Server-side meshes do not tick pose (ASSUMED).
- **Motion matching cost per search is not known** for this database size (UNVERIFIED). Spike R3.

#### 8.x.9 Characters are not instanced, and Mass is not used

- Every character is one `ACharacter` with one `USkeletalMeshComponent` and its own animation instance. 50 skeletal draws plus shadow passes is within budget.
- **Mass / instanced skinned mesh is forbidden for characters** (VERIFIED-P brief). Reasons: per-character motion matching needs a real anim instance; per-viewer concealment hides a single character; the animation budget rule above needs a component to tune. `MassGameplay`/`MassCrowd` are Experimental in 5.8 (`PLUGINS.md`, VERIFIED-E), and 5.8.4 ships `InstancedSkinnedMeshComponentHelper.h`, so the temptation exists. Do not enable either plugin.
- GPU instancing of identical characters is also not a goal: skinned draws do not batch regardless.

#### 8.x.10 Modular kit: runtime assembly constraints

- **Grid and pivot (VERIFIED-P, 3.3):** 2.0 m plan grid, 0.4 m trim sub-grid, 1.6 m vertical course, bottom-left pivot, no non-uniform scale, 90-degree rotation steps only. A scale of exactly 1.0 and positive on all axes also avoids flipped winding on Nanite.
- **Component choice:** one `UInstancedStaticMeshComponent` per module type, created once at round start from the server seed. Prefer ISM over HISM: Nanite does its own cluster culling, so HISM's CPU tree adds build time for no gain (ASSUMED; `current-best-practices.md` recommends HISM generally, which predates this reasoning). Non-Nanite foliage and bushes use HISM (VERIFIED-E: `UHierarchicalInstancedStaticMeshComponent` still present in 5.8.4).
- **Batch spawn:** add all instances in one call per component, before the first lit frame. Mobility set to Static after assembly. Late changes invalidate Virtual Shadow Map pages across the city (ASSUMED).
- **Lumen (project uses software SDF tracing, `r.Lumen.TraceMeshSDFs=1`, `r.GenerateMeshDistanceFields=True`, VERIFIED-P):** every module needs a closed, watertight shell, walls >= 0.2 m thick, and <= 12 auto-generated Lumen cards (ASSUMED). Thin or open walls leak GI. Runtime-spawned ISM instances add to the Lumen scene each; hence the 5,000-instance cap, to be measured (R1).
- **Nanite:** modules on, all opaque, no WPO, no masked, all sharing <= 4 master instances so unique materials in view stay <= 12 (ASSUMED). Programmable raster (WPO/masked) is the Nanite slow path, so avoid it in the kit.
- **No static lighting:** `r.AllowStaticLighting=0` (VERIFIED-P). Do not author lightmap UVs; no UV1 on kit meshes (saves vertex memory).
- **Draw-call impact:** Nanite modules cost shading bins, not draw calls; bush and foliage ISMs cost one draw per LOD per component. Merged meshes lower draw calls but defeat per-instance culling, so keep modules per-bay, not merged per-block.
- **Collision:** simple collision only (8.6): <= 4 primitives per module, convex hulls <= 32 vertices (ASSUMED). Stairs: one convex wedge per run (civic 30 degrees, temple 40 degrees). Temple stair 40 degrees is under the default walkable and nav slope limits near 44 degrees, but only by 4 degrees (ASSUMED, check `WalkableFloorAngle` and `RecastNavMesh` agent max slope). 5,000 ISM instances x <= 4 shapes is about 20k Chaos bodies on the server; measure (R4).
- **Navmesh:** floors are flat on the 2.0 m grid with the 0.17 m / 0.22 m step vocabulary. Agent max step height must be >= 0.25 m (ASSUMED) or stairs need nav modifiers. Navmesh builds once after assembly behind the loading screen, budget <= 3 s (ASSUMED); the server 1.5 ms budget is for steady state, not this build.

#### 8.x.11 Bush per-viewer concealment: material and component constraints

- **Mesh and material identical for all viewers (VERIFIED-P, 2.8):** one bush mesh family, one set of materials and the same MI, no occupied/empty parameter, no per-instance custom data tied to occupancy, no MID per bush. Sway is the shared foliage WPO function.
- **The per-viewer thing hidden is the character, not the bush.** One local viewer per client (online, no splitscreen), so "per-viewer" means each client decides locally from the replicated concealment state. No owner-based flag is needed for the enemy-hiding case.
- **Flag facts (VERIFIED-E, `PrimitiveComponent.h`):** `bOwnerNoSee`, `bOnlyOwnerSee`, `bCastHiddenShadow`, `bAffectDynamicIndirectLighting`, `bAffectIndirectLightingWhileHidden`, `bVisibleInRayTracing` all exist. Use `SetHiddenInGame`/visibility on the concealed pawn's mesh on non-self clients. The hider's own client leaves it visible. `bOwnerNoSee` would hide the hider from themselves, so it is the wrong flag.
- **Leak paths to close (ASSUMED behaviour, test R5):** hidden meshes may still cast shadows (set `bCastHiddenShadow=false`); contribute to Lumen/indirect lighting (set `bAffectIndirectLightingWhileHidden=false`); show in reflections or ray-traced paths (set `bVisibleInRayTracing=false` on the concealed pawn); or show through the bush as a shadow cast under it. The dome is a closed mesh, but a shadow cast by the sun beyond the planter edge is a tell.
- **Anti-cheat note:** hiding locally leaves the position on the wire. Net relevancy/filtering of concealed pawns for enemies is a server-side decision for the replication specialist, outside art.
- **No outline, no stencil, no custom depth:** no pool or bush primitive may set `bRenderCustomDepth`, a stencil value, or a post-process outline. Recommend `r.CustomDepth=0` project-wide if nothing else needs it (ASSUMED saves one pass). Concealment grade is a per-viewer post-process (2.8), not a material change.
- **Teammate marker** is UI only, never a mesh tint (Section 7).

#### 8.x.12 Importer settings, pipeline and cook size

- **Source:** Meshy mesh normalised in Blender, exported FBX per 8.6 (175 cm, +X facing, applied transforms). Exact export axes are ASSUMED; confirm with one test import and freeze them in a Blender export preset.
- **Skeletal and static import settings:** as in 8.6 (Geometry and Skinning Weights; Manny skeleton; animations, morph targets, physics asset and materials off; Build Nanite on for modules; collision and lightmap generation off; tangents imported from file, one method frozen for the pool). Naming per `technical-preferences.md`: `SM_`, `SK_`, `T_`, `M_`, `MI_`.
- **Gate checks before commit:** the automated gate of 8.8 item 13.
- **Cook and disk (ASSUMED):** pool textures about 34 MB raw (smaller after Oodle). Nanite data is larger than a plain LOD chain; estimate it by cooking one module before projecting (R7). The `Content/` tree holds `MetaHumans`, `Paragon`, `UEFN_Mannequin`, `LevelPrototyping`; Unreal only cooks referenced assets, but audit references before the first package so unused sample content is not pulled in.

#### 8.x.13 Conflicts and resolutions

Decision policy: adopt the technical-artist's position unless it breaks a pillar
(Pillar 1: no enemy identification; no player-versus-NPC difference). No
technical position broke a pillar; every keyed-branch, outline and stencil
restriction in this table is a Pillar 1 protection that the art side already
held. C1 to C5 are the technical-artist's conflicts; M1 to M12 are
mismatches found while merging the two drafts.

| # | Conflict | Art position | Technical position | Decision | Rationale |
| - | -------- | ------------ | ------------------ | -------- | --------- |
| C1 | Texture density vs UV packing | 1024 at 5.12 texels/cm, no UV overlap, 8 px padding, hidden faces deleted | A dressed figure is about 2.0-2.6 m^2 of surface (ASSUMED): 26.2 texels/cm^2 x 20,000-26,000 cm^2 = 0.52-0.68 M texels of 1.05 M, so 50-65 % fill before padding; 75 % is the practical ceiling | **Adopt.** Keep 1024; if a model cannot pack, reduce hidden-face area first, never density; 2048 rejected (4x memory). Fill check added to 8.4 and 8.7 step 7 | Tight, not impossible; density parity beats a sharper face (Pillar 2) |
| C2 | 12k tris LOD0 with no Nanite | 12k, non-Nanite | Non-Nanite skinned meshes re-render in every VSM view and (with `r.SkinCache.DefaultBehavior=0`) re-skin per pass | **Adopt.** Keep 12k; fix skin cache (opt-in per mesh or flip default; R2) | 12k itself is not the cost, the pass count is |
| C3 | Nanite skeletal | "Not assumed" (1.3.5) | Installed engine has Nanite skeletal; the reference doc says it does not | **Adopt.** Stay classic; correct the reference; R6 for any revisit | Both documents agree on the decision, not the reason |
| C4 | Foliage and bush sway | Period >= 4 s (3.6, 2.8) | WPO keeps foliage off Nanite | **Adopt.** Non-Nanite foliage and bush with hand LODs | Costs more LOD authoring; sway is a hard art requirement |
| C5 | Bush leaves | Closed dome on the 2.0 m planter with three leaf tiers (3.6); my draft: leaves are modelled shapes, never alpha cards | Real-geometry or card leaves; masked cards raise overdraw | **Adopt.** Opaque real geometry at LOD0 and LOD1; cards only at LOD2, within the 5 % silhouette tolerance. 8.5 and 8.10 amended | Keeps the bush identical occupied or empty and the dome read at 25 m |
| M1 | Kit tiling texture size | Tiling 1024 at a 2.0 m repeat (5.12 texels/cm) | Memory table listed kit tiling sets at 2048 | **Art kept; memory table rewritten.** Tiling 1024 (about 4 MB); the 2048 figure (about 16 MB) is the trim and atlas set; total <= 96 MB | A 2048 map at a 2.0 m repeat would be 10 texels/cm, double the density contract |
| M2 | Animation tier distances | LOD switches by figure height, about 1640 / distance px (60 degree vertical FOV) | Tier distances 12 / 35 / 70 m against 25 / 8 / 3 % (a different FOV) | **Adopt TA structure, fix numbers.** Screen height is the key; distances recomputed (19 % about 8 m, 8 % about 19 m, 3 % about 50 m); LOD table of 8.5 uses the same key | The two columns could not both hold; percentages are FOV-independent |
| M3 | Kit triangle ceiling and LODs | Nanite, no hand LODs, 2k to 20k guidance | <= 30k authored plus a fallback mesh <= 1,500 tris | **Adopt.** 30k ceiling (typical 2k to 20k), fallback mesh required | Lumen/SDF needs the fallback; the ceiling is a build and disk limit |
| M4 | Prop budgets | All props Nanite, up to 10k, small ones optional | Large prop <= 8k with LODs, Nanite if > 2k and opaque; small prop <= 1.5k, Nanite off | **Adopt.** | Nanite fixed overhead outweighs savings on small meshes |
| M5 | Bush Nanite, and my Dome Ban wording | Bush Nanite, opaque shell; "bush passes the Dome Ban" | Bush off Nanite (WPO sway); 3.6 shows the Dome Ban applies to *decorative* plants, the bush is the dome | **Adopt TA; correct art text.** Bush off Nanite with LODs; bush tested against the 3.6 specification, decorative plants against the Dome Ban | The art draft misread 3.6 |
| M6 | Character stature example | 175 cm working value (1.3.2) | Export note read "1.7 m = 170 units" | **Art kept as a working value.** 175 cm everywhere until OI-1 measures `SK_Mannequin` (8.6, 8.x.12) | 1.3.2 states 175 cm +/- 2 as a working value, not a frozen one; 170 is out of tolerance; if the measurement falls outside 173 to 177 cm every dependent value is re-derived from the bone chain lengths, which win |
| M7 | Pool material and instance names | `M_Citizen_Master`, `MI_Citizen_03_S4`, up to 8 swatches per model | `M_Character_Master`, `MI_Char_<Model>_<Variant>`, at most 24 instances | **Adopt TA names and cap; question closed 2026-10-24.** Renamed in 8.3. Appearances are 3 per model (default, swap, accessory), so 8 x 3 = 24 = the cap exactly; swatches are parameters (`GarmentSwatchIndex`, `TrimSwatchIndex`, 8.x.6) set from the roster (1.3.10) | The 4.3 rule gives each model 2 garment swatches and 1 trim swatch, not 8; 64 instances was never needed |
| M8 | Physics asset name | `PA_Citizen` | `PA_Mannequin` already exists | **Adopt.** Reuse the existing asset if it passes 1.3.5, else create `PA_Citizen`; one asset only | Avoid a duplicate asset |
| M9 | Collision prefixes and hull limits | `UCX_` only, <= 4 hulls | `UBX_` / `UCX_`, <= 4 primitives, hulls <= 32 vertices, stairs one wedge | **Adopt.** 8.6 merged | Same intent, more precise |
| M10 | Kit material slots | <= 2 | 1 (2 only with a separate trim) | **Adopt.** | Fewer Nanite bins |
| M11 | Temple monument triangles | 150,000 per tier | Kit module <= 30k; monument not covered | **Open.** Art figure kept, flagged for R1 and R7 | Hero pieces are not kit modules, but they bind the same Lumen and disk costs |
| M12 | Nanite WPO, skin cache, unlimited-influence flag | Not addressed in the art draft | Engine reference says no WPO; skin-cache default Exclusive; unlimited-influence path on | **Adopt.** Added to 8.6, 8.7 and 8.x.7; spikes R2, R8, R9 | These are engine facts the art contract has to respect |

*Trade-offs recorded:* (1) 1024 character maps limit facial detail; accepted,
because faces are not the identity and density parity beats a sharp face. (2)
Nanite on kit with no hand LODs trades some memory for production speed and
consistent silhouettes; its cost on the RTX 3060 target is spike R1. (3)
Primitive-built structural kit costs hand time but is the only way to hold the
2.0 m grid; Meshy is limited to ornament. (4) The 8-hour cap may cut otherwise
attractive Meshy characters; that is deliberate (Pillar 1). (5) Non-Nanite bush
and foliage need hand LODs and a 6k-triangle bush has to carry a three-tier leaf
read; accepted, because sway and identical occupied and empty bushes outrank
authoring speed.

#### 8.x.14 Risks needing a profiling spike (owner: technical-artist + performance-analyst)

| ID | Risk | Spike | Pass line |
| --- | --- | --- | --- |
| R1 | Lumen scene cost of up to 5,000 runtime-spawned Nanite ISM instances | Graybox city with 8 modules at 1,000 / 3,000 / 5,000 instances on a 3060 | Lumen scene + Nanite <= 5 ms of the 16.6 ms |
| R2 | Skin cache per-asset `Enabled` vs `Disabled`, with VSM, 60 characters (redefined by ADR-0022, 2026-10-24) | Same scene, project default left Exclusive; per-asset `SkinCacheUsage` `Enabled` vs `Disabled` on every pool LOD; `stat gpu` VSM and skinning | GPU skinning + shadow <= 2.5 ms |
| R3 | Motion-matching cost at 60 characters under the tier table (`NpcCount` 50, user decision 2026-10-25) | Insights trace, 60 characters, tiers A-D | <= 4 ms total, <= 8 characters in tier A |
| R4 | Chaos body count and navmesh build time from runtime ISM collision | Spawn 5,000 instances on a dedicated server | Spawn + nav build <= 3 s; steady AI+nav <= 1.5 ms |
| R5 | Concealment leak: shadow, Lumen, reflection of a hidden pawn | Capture hider under sun, enemy client view, flags on/off | No visible trace under any flag combination |
| R6 | Nanite skeletal in 5.8.4: stability, cost, restrictions | Enable on one pool model, compare pass cost and LOD behaviour | Only adopted if it beats classic LODs by >= 20 percent and has no motion-matching regression |
| R7 | Nanite disk footprint and kit cook size | Cook 1 module, extrapolate to 12 | Within the 3 GB package line |
| R8 | `r.GPUSkin.UnlimitedBoneInfluences=True` cost on a <= 4 influence mesh | A/B toggle (needs editor restart, shader recompile) | No measurable loss, else turn it off |
| R9 | Nanite + WPO and Nanite Foliage status in 5.8.4 (contradicting docs) | Test one sway material on a Nanite mesh | Decides whether bushes/foliage can ever go Nanite |

**Mass-production gate (decided 2026-10-24).** Spikes **R1, R2 and R3** gate
mass production. Until all three have reported against their pass lines, art
production is limited to the Body_Base (1.3.9), **one** test character (its Meshy credit is spent only after OI-1 is closed and Body_Base is accepted, 8.7 step 0) for the
T1 to T4 pipeline proof, and the 8 graybox kit modules at one variant each. No
Meshy credit is spent on characters 2 to 8, and no kit variants are authored
beyond those, until the gate lifts. A failed spike does not stop the project:
it triggers the pre-decided fallback in 8.x.16, and the spike is re-run under
the fallback before the gate lifts. Owners: technical-artist (runs), performance-analyst
(signs the numbers), producer (holds the gate).

#### 8.x.15 Open questions for others

- ~~Draw-call budget and memory ceiling~~ **Answered by ADR-0022 (2026-10-24):** the PROPOSED ceilings (draw calls ≤ 1,500, texture pool ≤ 1.0 GB, package ≤ 3 GB) are adopted as working ceilings; recording them in project config is a pending user item.
- ~~Whether the 4 ms animation budget is game-thread or all-thread time~~ **Answered by ADR-0013 §5:** summed game-thread + worker time ≤ 4.0 ms, game-thread share ≤ 2.0 ms.
- Bush count per city and kit instance count per city (level design, crowd/city GDD).
- ~~Substrate on or off~~ **Answered by ADR-0022 §1:** off for MVP and Vertical Slice.
- Monument tier triangle figure and cook cost (technical-artist, after R1 and R7).
- Correct `docs/engine-reference/unreal/rendering.md` on Nanite skeletal and Nanite WPO once R6 and R9 report (technical-director).

#### 8.x.16 Whole-frame GPU allocation and pre-decided fallbacks

The per-category budgets above each protect one cost; this table allocates the
whole 16.6 ms GPU frame (60 fps, RTX 3060-class, 1080p) so that they add up.
All figures are **ASSUMED** allocations until the spikes in 8.x.14 and the
performance-analyst's captures replace them (open item OI-8); the pass line is
the **headroom row**, not any single line.

| Pass group | Allocation (ms) | Source / spike |
| ---------- | --------------- | -------------- |
| Lumen scene, GI, reflections, Nanite visibility, raster and shading (kit, up to 5,000 instances) | 5.0 | R1 pass line |
| Character skinning and character shadow passes (60 characters, VSM; working value since `NpcCount` 50 (user decision 2026-10-25), re-verify in spike R3) | 2.5 | R2 pass line |
| Non-Nanite base pass and shadows: characters (base pass; their skinning and shadows are in the row above), props, foliage, bushes (<= 800k tris in all: about 266k characters, design-density case, plus about 534k props, foliage and bushes) | 2.0 | 8.x.2 |
| Virtual shadow maps for the kit (excluding characters) | 1.2 | ASSUMED |
| Sky, atmosphere, haze (6 and 12 percent aerial perspective) | 0.5 | ASSUMED |
| Post: grade, vignette, tone map, anti-aliasing / upscaler | 1.8 | ASSUMED |
| UMG HUD (drawn after post-process, 7.1) | 0.5 | ASSUMED |
| Prepass, HZB, translucency, misc | 0.9 | ASSUMED |
| **Total allocated** | **14.4** | |
| **Required headroom (>= 2.0)** | **2.2** | Reserve for the worst-case scene and driver variance; if measured allocations leave less than 2.0 ms, a fallback below is applied |

**Pre-decided fallbacks, in order.** Applied identically to every client and
every pawn (never per viewer, per team or per character: that would be a
Pillar 1 tell); decided now so a failed spike does not reopen the art
direction.

1. **F1, reduced Lumen quality** (lower Lumen scene detail, final-gather and
   reflection quality one scalability step: `sg.GlobalIlluminationQuality` and
   `sg.ReflectionQuality` 3 → 2, with a `[GlobalIlluminationQuality@2]` override
   that keeps mesh-SDF traces on (`r.Lumen.TraceMeshSDFs.Allow=1`), because the
   engine's level 2 would also drop them and change the thin-wall GI the kit is
   authored against; view distance is never lowered below level 2; ADR-0022 §5,
   ADR sync 2026-10-24). Expected
   saving about 1 to 1.5 ms of the 5.0 ms Lumen group. The sun and shade grid
   and the sun pool (geometry-made, 4.5) are unaffected; bounce is a little
   coarser, so the OI-6 outcomes are re-measured after the step.
2. **F2, lower internal resolution with temporal upscaling** (screen
   percentage 100 to 85, then to 77 if still short). The HUD stays at native
   resolution (it is drawn after post-process); thumbnail readability (3.2) is
   re-verified at the new internal resolution because figure pixel heights fall
   by the same factor.

**Never used as a fallback:** changing character LOD thresholds, animation
tiers or triangle ceilings for some pawns, dropping character shadows, or any
adaptive scaling keyed to who is on screen. Character parity (1.3.5, 5.6) is
not a budget variable. Owner of the decision to engage a fallback:
technical-director on the technical-artist's evidence.

#### 8.x.17 Open-items register (opened 2026-10-24)

Items the art bible cannot close without a tool or measurement the author did
not have (Unreal, Blender and Meshy were not available). Each has an owner and
the check that closes it. Section references say where the value is used.

| ID | Open item | Owner | Check that closes it | Used in |
| -- | --------- | ----- | -------------------- | ------- |
| OI-1 | **CLOSED 2026-10-25.** Reference = `SK_UEFN_Mannequin` (user decision): 165.6 cm, 88 bones, nine chains recorded in 1.3.2 / 1.3.5; dependent values re-derived (chains win) and 1.3 re-signed by the art-director 2026-10-25; downstream change requests CR-OI1-1 to CR-OI1-6 (1.3.2) | technical-artist | Measured in UE 5.8.4 (2026-10-25 spike section 4) and Blender (`tools/asset-pipeline/body_measure.py`) | 1.3.2, 1.3.5, 8.6, 8.7, M6 |
| OI-2 | Law 3b: P4 Canal Green (L\* 51), P5 Foliage Green (L\* 43) and bush leaf tips (L\* 42 to 48) fall in the excluded band as leg and forearm backdrops | art-director with technical-artist | Run the leg-and-forearm read test (4.1) on water edges, planting and the bush; adopt dark-value variants (L\* 38 or lower) if it fails | 4.1, 4.2, 3.6, 6.10 |
| OI-3 | Hand-computed colour values need measurement: new P3 `#664335`, the skin band, garment hexes, Bone on Cochineal Deep, Cochineal Bright on the final plate | technical-artist (T1 audit script) | Re-measure; correct hexes, not the bands | 4.2, 4.3, 4.6 |
| OI-4 | Sun azimuth of 45 degrees from the grid X axis is a working value | art-director with level-design | Graybox: confirm cornice shade bands in every street orientation and alley sun slivers | 2.1, 6.8 |
| OI-5 | Generator shadow-trace check needs calibration (lit versus surround illuminance estimate) | technical-artist with level-design | Compare the estimate to in-engine grey-card measurement over 10 seeds; set the constants | 4.5, 6.8 |
| OI-6 | Per-area shade ratio and bounce outcomes are targets, not yet measured | technical-artist | Grey-card capture at 1.5 m, solar noon, no grade, per area; adjust albedo within band if outside tolerance | 4.5 |
| OI-7 | Marigold-versus-world colour-vision simulation (hue 60 to 100, C\* 30 cap) not yet run | ux-designer with art-director | Protan, deutan and tritan captures per 4.7 test 3 | 4.1, 4.7, 6.4 |
| OI-8 | GPU allocation table is assumed; R1 to R3 not yet run | technical-artist with performance-analyst | Run R1 to R3, replace the table, confirm >= 2.0 ms headroom; apply F1 / F2 if not | 8.x.14, 8.x.16 |
| OI-9 | LOD3 (900 tris) retains head mass and leg gap, and the 8.5 thresholds, are ASSUMED | technical-artist | T1 items 16, 22, 23 on LOD3 of the first test model; T3 at 30 m on LOD3 | 1.3.5, 5.6, 8.5 |
| OI-10 | Body_Base not yet built | technical-artist (accepted by T1 and the art-director) | Deliver per 1.3.9 | 1.3.9, 8.7 |
| OI-11 | **CLOSED 2026-10-25.** Character Pool & Appearance GDD (`design/gdd/character-pool-appearance.md`) is Approved (D35). Open sub-item: the T6 simulation check | game-designer / systems designer | T6 passes in simulation (the only remaining check) | 1.3.1, 1.3.7, 1.3.8 |
| OI-12 | The contrast scalar (tone-curve slope at the 18 percent grey pivot) needs an implementation definition and measurement | technical-artist | Define the pivot and measure +/- 20 percent on a grey ramp; add to the grade-preset validator (2.12) | 2.1, 2.11 |
| OI-13 | The camera and spawn-density rule that holds the 8 / 14 / 28 per-band caps (at most 8 figures at LOD0 screen height, 14 in the LOD1 band, the rest at LOD2 or beyond) is not written; until it is, the 266k character figure is a design-density case | level-design with game-designer | Write the rule (camera distance and FOV limits, crowd spawn spacing, maximum local density) in the crowd/city GDD; verify with a scripted per-band figure count over 50 seeds and in T5 | 1.3.5, 8.5, 8.x.2, 8.x.8, 8.x.16 |

**Register: blocking gates.** What each open item blocks and the latest point
at which it must be closed. Work gated by an item does not start while the item
is open; an item past its "needed by" point is escalated to the producer, never
waived.

| ID | Blocks | Needed by |
| -- | ------ | --------- |
| OI-1 | **CLOSED 2026-10-25** (was: the first Meshy credit, the single test character included (1.3.2, 8.7 step 0(a)); final scale of Body_Base) | Met 2026-10-25 |
| OI-2 | Bush family sign-off (3.6, 6.5) and the water-edge and low-planting variants of P4 and P5 (4.2) | Bush sign-off; at the latest the graybox greyscale tests before the vertical slice (3.6) |
| OI-3 | Palette and swatch hex lock (4.2, 4.3, 4.6); T1 colour items 10 to 12 on the test character | Test character T1 audit |
| OI-4 | Sun direction lock; cornice shade-band and alley-sliver checks (2.1, 6.8) | Graybox lighting sign-off, before the generator's layout rules are frozen |
| OI-5 | Generator shadow-trace constants and objective-site sun-pool validation (4.5, 6.8) | Before the generator story that places objective sites closes |
| OI-6 | Per-area albedo and shade-ratio sign-off (4.5) | First lighting pass of each area type |
| OI-7 | Marigold and signal-colour lock (4.1 law 4, 4.7 test 3, 6.4 goods) | Before the UI palette (4.6) is frozen and before goods props are authored; scheduled: before UI palette freeze, first UI implementation story |
| OI-8 | The mass-production gate (8.x.14): characters 2 to 8 and kit variants beyond one each | Mass-production gate |
| OI-9 | LOD3 policy and the 8.5 thresholds for the pool | Test character T1 / T3 sign-off; before characters 2 to 8 |
| OI-10 | The first Meshy credit (8.7 step 0(b)) and every character conformity check | Before the test character is generated |
| OI-11 | The appearance-deal allocator story and test T6 (1.3.1, 1.3.7) | Before the allocator implementation story starts |
| OI-12 | The grade-preset validator (2.12) and the state distinctness checks (2.11) | Before any grade preset is signed off |
| OI-13 | The 266k character figure and the 800k split (8.x.2, 8.x.16); test T5 | With OI-8, before the mass-production gate lifts; scheduled: before spike R3 is trusted |

## 9. Reference Direction

Five references, chosen so that no two point the same way. Each answers a
different question the rule in Section 1 raises: how do we stage a calm
scene (Codex), how does a crowd stay anonymous yet readable (AC:B MP), how
does the stage stay neutral (SpyParty), what does the city's mass and
material look like (Templo Mayor archaeology), and what does stillness feel
like on screen (Hitchcock and midday photography). The visual anchor is
"Held Breath"; every reference is a tool for that, not a mood board to imitate.

**Ratification flag (2026-10-25).** "Held Breath" is ratified as the visual identity anchor under the user's 2026-10-25 authorisation. The user's confirmation is recorded in `design/gdd/game-concept.md` (Visual anchor: "Confirmed by the user 2026-10-25"). The `AD-CONCEPT-VISUAL: STRONG` entry in `production/session-state/active.md` records the original autonomous recommendation only.

**How to use this section.** A reference supplies one named technique, tied to
a numbered section. If an artist cannot state which section the borrowed
element serves, it is not being used. Section 1.1 question 1 (does this let a
viewer tell one person from another by anything unchosen?) vetoes any
reference detail, however good the source.

### 9.1 The five references

**R1. Codex Mendoza and Florentine Codex (daily-life folios)**

- *Take:* the staging of ordinary life: one figure, one legible gesture, one
  ground line, little else. Figures read in profile or three-quarter by a
  single posture cue (carrying, kneeling, selling, sweeping), which is the
  model for the composed-not-stiff pose language of 5.4 and the tell
  vocabulary of 5.5. Take also the register layout (activity grouped along a
  ground line, with empty paper between groups) as a rule for market-stall
  and street prop clustering in 6.4 and 6.6: clusters with breathing room,
  never wall-to-wall dressing.
- *Take (palette check only):* the codices' restraint of line and flat field.
  Use it to test that world value structure reads in greyscale (4.1).
- *Avoid:* the saturated polychrome (rejected in 4.2; polychrome survives only
  as the muted temple bands of 4.5); sacrifice, warfare-tribute and
  punishment folios entirely (1.4, 6.9); copying glyphs, day-signs or place
  glyphs as ornament (6.9); and the colonial framing, since both codices were
  produced under Spanish supervision and the commentary and selection carry
  that bias. Read them critically (1.4), as evidence of what was shown and
  how, not as neutral truth.

**R2. Assassin's Creed Brotherhood multiplayer (crowd readability)**

- *Take:* the proof that a look-alike crowd can be read by behaviour alone:
  shared walk cycles, shared silhouettes, and a player who is findable only by
  a wrong motion (a run, a stop, a head that keeps turning). Applies to the
  uniformity spec in 1.3, the silhouette philosophy in 3.2 and the tell
  vocabulary in 5.5. Also take the discipline of keeping NPC animation
  fidelity identical to player-character fidelity at every LOD (5.6).
- *Avoid:* the compass, the pursuer indicator, the target portrait and any
  highlighted or outlined target (7.1, 7.7). Also diverge from its busy
  palette and skin-varied crowds; Smoking Mirror's pool is one body in eight
  garment swatches (1.3, 4.3), and its seeded cities carry no landmark-led
  navigation. Do not copy its cartoonish tell exaggeration; ours are quiet
  (2.12).

**R3. SpyParty (the neutral stage)**

- *Take:* a legible stage that never competes with the people on it:
  low-contrast set, flat readable floor planes, props that exist to be
  obstacles and cover rather than decoration, and a clear value separation
  between background and figures. Informs 3.3 (environment geometry), 6.7
  (routes, hiding places and sightlines in a seeded layout) and the exploration
  state of 2.4. Also take its restraint in time: nothing animates in the
  background that a player could mistake for a tell.
- *Avoid:* the sniper reticle and any aim-based UI (the game has no aim
  skill; see the target player profile in the concept); the fixed
  single-room camera, since our space is traversed, third-person and
  seeded; and its flat-colour minimalism, since ours is stylised-realistic
  PBR (6.3), not diagrammatic.

**R4. Templo Mayor archaeology and reconstruction photography**

- *Take:* mass and material. The talud-tablero massing (sloped base,
  vertical panel) and stepped silhouette of civic architecture for 3.1 and
  6.1; the limewash (cal) white, tezontle red-brown and canal-green
  relationships for 4.2 (P1, P3, P4); and plaster-over-stone surface
  behaviour (clean stucco, hand-troweled irregularity, dado against upper
  wall) for 6.3. Prefer excavation plans, section drawings and museum
  reconstruction models over hero photography: they show the city as a
  planned, intact state.
- *Avoid:* the ruin image. No decay, overgrowth, rubble or dig-site framing
  (6.9: living culture, not a lost civilisation); no tzompantli, sacrificial
  stone, bloodied steps or skull motifs (1.4, 6.9); no deity or calendar
  monument as decor (6.9). Skip Hollywood reconstructions and game
  depictions as authority. Do not let a photograph's hard raking light
  override the Section 2 and 4.5 light model.

**R5. Hitchcock crowd framing and midday market photography (stillness)**

- *Take (Hitchcock):* the crowd moves as one while a single figure does not
  (the tennis-match crowd in *Strangers on a Train*, where every head tracks
  the ball except one). It is the cleanest statement of the rule in Section 1
  and the model for how a tell should read: one deviation against a calm
  baseline (2.1, 2.5, 2.6). Also take the wide, held frame that lets the
  viewer search rather than be told where to look, for lobby and
  round-end reveal framing (2.2, 2.10).
- *Take (photography):* the flat, overhead noon light, short shadows and
  heat-bleached values of midday plaza and mercado photography (Mexican
  street photography such as Manuel Alvarez Bravo's work is a good starting
  archive, alongside contemporary tianguis documentation). Informs the
  midday "held breath" mood in 2.1 and 2.4 and the warm-light, cool-shadow
  balance of 4.5.
- *Avoid:* Hitchcock's expressionist menace (tilted frames, hard noir
  shadows, close-up dread) and any dutch angle or heavy vignette in
  gameplay, as they break the calm baseline. Avoid sentimental or
  "exotic" tourist-photography framing of Mexican life (1.4). Avoid golden-hour
  warmth; the anchor is noon, not sunset.

### 9.2 Sensitivity and sourcing

- *Primary and scholarly sources to consult:* the Codex Mendoza and Florentine
  Codex (read critically; they carry colonial bias); Templo Mayor Museum and
  Museo Nacional de Antropologia collections; INAH excavation reports; and
  published reconstruction scholarship on Tenochtitlan's urban plan, canals
  and markets. Record the exact folio, plate or report behind each borrowed
  element in the reference log (9.4).
- *Not authorities:* films, games and concept art of "Aztec" settings, including
  the five references above for anything cultural. R2 and R3 are references
  for play and readability only; R5 is for staging and light only.
- *Sensitivity read:* a qualified reviewer (ideally a Mexican or Nahua
  heritage consultant or an archaeologist of Mesoamerica) reads the kit, the
  relief and fresco motif set, the fixture design and the title and lobby
  mirror presentation before the vertical slice (1.4, 6.9, concept
  document). Schedule it as a gate, not a polish pass: any rework should
  cost a kit module, not a shipped level.
- *Living culture:* no reference is to be used in a way that frames Mexica
  heritage as extinct or exotic. Where a source is ambiguous, choose the
  plainer, civic reading.

### 9.3 Reference misuse tests

A reference is being misused if any one of these answers is yes.

1. **Section test.** Can the artist name the numbered section this element
   serves? If not, remove it.
2. **Question 1 test.** Does borrowing it let a viewer tell one NPC from
   another by anything unchosen (1.1)? If yes, it is rejected, however
   faithful to the source.
3. **Copy test.** Remove the credit: would a viewer name the source from a
   single screenshot? AC:B-style hooded silhouettes or a codex-style flat
   polychrome crowd mean too close; take the principle, not the look.
4. **Spectacle test.** Does the borrowed image depend on sacrifice, ruin,
   gore, deity-as-decor or conquest? If yes, reject the source image, not
   only the element (1.4, 6.9).
5. **UI leak test.** Does it add an identification aid, highlight or
   pointer that a reference game uses (compass, target card, reticle)? If
   yes, it is a Section 7.1 and 7.7 violation.
6. **Mood test.** Does it add menace, golden-hour romance or ruin-melancholy
   to a noon, clean, civic scene? If yes, it contradicts Held Breath (2.1).
7. **Provenance test.** Is the cultural element traceable to a primary or
   scholarly source in the log (9.4) and not to a film or game? If no, hold
   it for the sensitivity read.

### 9.4 Reference log

Keep a short log (source, folio or plate, section served, what was taken,
what was diverged from, date, who approved) in this section, and add an
entry at the time of borrowing rather than after. New references are added
here only if they point in a direction none of R1 to R5 already covers.

The log is opened with the five references at **direction level**. No specific
folio, plate or photograph has been borrowed yet, so the "Folio / plate" column
reads "none yet" and is filled in, one row per borrowed element, when an artist
first uses one. An element with no row here is not yet cleared for use (9.3
test 7); no cultural element is cleared until the sensitivity read (9.2).

| Ref | Source | Folio / plate | Section served | Taken (direction) | Diverged from | Date | Approved by |
| --- | ------ | ------------- | -------------- | ----------------- | ------------- | ---- | ----------- |
| R1 | Codex Mendoza; Florentine Codex (daily-life folios) | none yet | 5.4, 5.5, 6.4, 6.6, 4.1 | Single figure, single legible gesture, ground-line staging; register layout with breathing room; restraint of line and flat field as a greyscale value test | Saturated polychrome; sacrifice, tribute and punishment folios; glyphs and day-signs as ornament; colonial framing taken uncritically | 2026-10-24 | pending (art-director sign-off at re-gate; sensitivity read before the vertical slice) |
| R2 | Assassin's Creed Brotherhood multiplayer | none (game reference, readability only) | 1.3, 3.2, 5.5, 5.6 | Look-alike crowd read by behaviour alone; NPC animation fidelity equal to player fidelity at every LOD | Compass, pursuer indicator, target portrait, outlined target; busy palette and skin-varied crowds; cartoonish tell exaggeration | 2026-10-24 | pending (art-director sign-off at re-gate) |
| R3 | SpyParty | none (game reference, readability only) | 3.3, 6.7, 2.4 | Neutral, low-contrast stage; flat readable floor planes; props as cover and obstacle; nothing animating in the background | Sniper reticle and aim UI; fixed single-room camera; flat-colour minimalism | 2026-10-24 | pending (art-director sign-off at re-gate) |
| R4 | Templo Mayor archaeology and reconstruction models; Templo Mayor Museum; INAH reports | none yet | 3.1, 4.2, 6.1, 6.3 | Talud-tablero massing and stepped silhouette; cal, tezontle and canal-green relationships; plaster-over-stone surface behaviour | The ruin image; tzompantli, sacrificial stone, bloodied steps; deity and calendar monuments as decor; hard raking light | 2026-10-24 | pending (sensitivity read) |
| R5 | Hitchcock crowd framing (*Strangers on a Train*); midday plaza and mercado photography (for example Manuel Alvarez Bravo, tianguis documentation) | none yet | 2.1, 2.4, 2.5, 2.6, 2.10, 4.5 | One deviation against a calm baseline; the wide held frame; flat noon light, short shadows, heat-bleached values | Expressionist menace, dutch angles, heavy gameplay vignette; exoticising tourist framing; golden-hour warmth | 2026-10-24 | pending (art-director sign-off at re-gate) |
