# Game Concept: Smoking Mirror (working title)

*Created: 2026-10-24*
*Status: Approved 2026-10-25 (second-round /design-review, lean mode, as a concept document; the first round on the same day, full mode, was NEEDS REVISION and its revisions were verified; the second round applied text-sync edits only, none touching a user-confirmed rule or pillar wording). Director gates CD-PILLARS (CONCERNS → revisions applied), AD-CONCEPT-VISUAL (STRONG), PR-SCOPE (OPTIMISTIC), TD-FEASIBILITY (see Technical Considerations). Items the reviews left for the user or for downstream documents are listed under "Review Follow-ups (2026-10-25)". Review log: `design/gdd/reviews/game-concept-review-log.md`.*

*User decisions 2026-10-25 applied: `NpcCount` ~50 (changed by the user from ~40); MVP Definition items 8-11 (informant ping, round summary, reconnect indicator, walk-slower key).*

*Sources: `GAME_BUILD_HANDOFF.md` (the user's outline, authoritative for confirmed rules) and the user's answers recorded in `production/session-state/active.md` on 2026-10-24.*

> **Title note.** Tezcatlipoca — "Smoking Mirror" — is the Mexica deity of night, sorcery, deception and hidden identity. The title is a working name, not a branding decision.

---

## Elevator Pitch

> It's an online 5v5 social stealth game where you hide among a crowd of identical-looking citizens in an Aztec-inspired city, race the other team through secret objectives, and assassinate rivals who give themselves away through how they move.

10-second test: *"Two teams race through a crowd that all looks the same to one hidden target — every advantage you take is a tell, and the people who don't belong can be spotted."* (Reframed 2026-10-25, decision D39, to match Pillar 4 "A Race, Not a Hunt".)

---

## Core Identity

| Aspect | Detail |
| ---- | ---- |
| **Genre** | Social stealth / team PvP objective race (hidden-identity multiplayer) |
| **Platform** | PC (Windows) — recorded for `/setup-engine`; storefront TBD |
| **Target Audience** | Mid-core to hardcore PvP players who prize observation over reflexes (see Target Player Profile) |
| **Player Count** | Online multiplayer, 10 players (5v5), client-server (listen server for prototype; dedicated server later) |
| **Session Length** | ~12-minute rounds (`TargetRoundMinutes`, a tuning target: no time limit and no match-end rule in MVP); a session is whatever rounds the group chooses to play, typically 3–5 rounds, about 36–60 min of play plus intermissions |
| **Monetization** | None yet (decide post-vertical-slice) |
| **Estimated Scope** | Large (18–24 months, solo developer + AI agents) — see Scope Tiers |
| **Comparable Titles** | Assassin's Creed Brotherhood multiplayer (2010), SpyParty (2018), The Ship (2006) |

---

## Core Fantasy

**You are the knife in the crowd: invisible because you look like everyone, lethal because you can tell who doesn't belong.**

The game sells two skills that pull against each other: *blending* (moving, pausing and choosing like a citizen) and *reading* (noticing the citizen who does not). Every advantage you take — running, interacting with an objective, hiding, bracing for an attack — makes you a little more readable. The fantasy is the moment of certainty: you have watched a stranger for twenty seconds, they turned their head one beat too late, and you are right.

The fantasy is solo-framed while the game is 5v5; the team angle (teammates you can see, informants feeding you sightings) is the supporting layer and should be found by narrative/marketing later.

---

## Unique Hook

**Like Assassin's Creed Brotherhood's multiplayer, *and also* there is no compass: in 5v5 team races your only radar is your eyes — and your eliminated teammates become spies in the crowd.**

- AC:B multiplayer is built around a compass and contract-tracking system. This game removes every identification aid; reading behaviour is the entire skill.
- Eliminated players are not removed: they walk the city as informants, still part of the match, still huntable.
- Supporting element: each round's city is assembled from a modular kit with a server seed, so reading the crowd beats memorising the map.

---

## Player Experience Analysis (MDA Framework)

### Target Aesthetics (What the player FEELS)

| Aesthetic | Priority | How We Deliver It |
| ---- | ---- | ---- |
| **Challenge** (obstacle course, mastery) | 1 | Reading behavioural tells under pressure; one-strike resolution; defence timing by anticipation, not reflex |
| **Fellowship** (social connection) | 2 | 5-player teams, teammate-only markers, informant sightings, shared objective race |
| **Fantasy** (make-believe, role-playing) | 3 | Being an assassin in a living Mexica-inspired city; acting like a citizen |
| **Sensation** (sensory pleasure) | 4 | Sunlit, still city ("Held Breath"); motion-matched animation so bodies read naturally |
| **Discovery** (exploration, secrets) | 5 | Fresh seeded layout each round; finding objective sites and hiding spots |
| **Narrative** (drama, story arc) | 6 | Emergent stories ("I watched him for a full minute") — no authored plot in MVP |
| **Expression** (self-expression, creativity) | 7 | Playstyle only (patient blender vs aggressive reader); no cosmetics in first scope |
| **Submission** (relaxation, comfort zone) | N/A | The game is deliberately tense |

*Alternatives considered:* Fantasy as #1 (setting-led) was rejected because the setting is a frame, not the engine of play; the art direction explicitly subordinates spectacle to legibility.

### Key Dynamics (Emergent player behaviors)

- Players will **mimic NPC rhythms** — walking at crowd pace, pausing at stalls, drifting with groups — to stay anonymous.
- Players will **bait** — perform a deliberately suspicious action near a prepared position to draw an attacker into a reversal.
- Players will **test suspects** — approach a suspect and watch whether they brace, flee or keep walking.
- Teams will **split roles** — one player progresses the objective while teammates watch the approaches.
- Informants will **shadow suspects** and ping sightings, turning elimination into intelligence work.
- Players will **use the objective as a lens** — whoever interacts with an objective is exposed, so passers-by at an objective court get the strongest read. The preliminary courts are team-specific and hidden, so exposure there comes from chance sightings, and a watcher who loiters at a court is itself a tell. Deliberately watching or camping the shared final is a pressure the objective structure must counter (Pillar 4, "NOT a hide-and-wait game"; see Design Risks), not an intended strategy.

### Core Mechanics (Systems we build)

1. **Crowd anonymity** — a fixed pool of character models shared by players and ~50 wandering NPCs (`NpcCount` 50, user-changed from ~40 on 2026-10-25); teammate-only markers; no enemy identification aids of any kind.
2. **One-strike assassination with prepared reversal** — eliminate an active enemy in one action; a defender who is holding the ~4 s preparation (once per 60 s, not while running/parkour) reverses it and eliminates the attacker.
3. **NPC-kill penalty and informants** — the 3rd NPC kill removes a player; eliminated/removed players become informants (observe, communicate, huntable, respawn as informants, never return to active play).
4. **Team objective race** — two team-specific preliminary stages, then one shared final objective; first team to complete it wins, or eliminate every active enemy. A delayed, count-only rival-progress signal (how many preliminary stages the other team has finished, never the final step; Objectives Chain OC-12, user-confirmed 2026-10-25) tells the trailing team it is behind without revealing any player or place.
5. **Concealment and city** — per-viewer bush invisibility (blind-attackable); seeded modular city layout identical on every client; motion-matched locomotion and traversal; first/third-person toggle.

**Sources of readable deviation (the tell inventory).** Under Pillar 1 nobody differs from the crowd involuntarily, so every tell is a choice with a price. The authoritative list, with each tell's NPC red-herring rate, is the Baseline and Tell Matrix (T1–T18; T18 is the ping, which has no world-visible deviation, D28) in `design/gdd/crowd-npc-behaviour.md` (CNB-R16). In summary: running and traversal (speed, lean, arc), objective interaction (a 3 s two-handed pose), bracing (planted stance), entering or leaving a bush, a gaze held too long on a passer-by, abrupt reversals, loitering longer than the crowd does, and, above all, the strike itself. The Core Fantasy's "turned their head one beat too late" is a gaze tell (T13).

---

## Player Motivation Profile

### Primary Psychological Needs Served

| Need | How This Game Satisfies It | Strength |
| ---- | ---- | ---- |
| **Autonomy** (freedom, meaningful choice) | When to act, whom to suspect, which route, whether to commit to a kill or keep reading — every 30 s is a real decision with no correct script | Core |
| **Competence** (mastery, skill growth) | Reading tells is a learnable, deep skill; a correct read is unambiguous (kill confirmed); mistakes are clearly attributable | Core |
| **Relatedness** (connection, belonging) | Teammates you can see, coordination around objectives, informants feeding sightings, shared "we read them" moments | Supporting |

### Player Type Appeal (Bartle Taxonomy)

- [x] **Killers/Competitors** (primary) — How: PvP with decisive one-strike resolution; outplaying a human's read is the core reward.
- [x] **Achievers** (secondary) — How: objective chains completed under pressure; win the race.
- [x] **Socializers** (secondary) — How: team play, informant coordination, post-round stories.
- [ ] **Explorers** — How: light appeal only (new layouts each round); not a design target.

**Quantic Foundry:** strongest on *Strategy* (planning, careful decisions) and *Competition*; low on *Action* (reflex) by design — see anti-pillar "NOT a reflex game".

**Who it is not for:** players who want twitch aim, power progression, loot, or solo story; players who dislike waiting and observing.

### Flow State Design

- **Onboarding curve**: First round with a short in-world primer card: "Walk like them. Watch for who doesn't." A private-lobby practice mode with NPCs only (post-MVP) teaches the tells safely.
- **Difficulty scaling**: Skill scales with opponents (PvP); crowd density and NPC behaviour variety are tuning knobs that set the baseline readability.
- **Feedback clarity**: Kill confirmation after an attack (player vs NPC); wrong-target tally (an Owner-only "Wrong targets: n" line that ticks identically for NPC and informant kills, an upper bound on strikes; the true strike count stays server-only — user decision 2026-10-25, Penalty CR-15); team objective progress; reversal is visibly dramatic.
- **Recovery from failure**: Elimination keeps you in the match as an informant; next round starts fresh in ~12 minutes with a new layout.

---

## Core Loop

### Moment-to-Moment (30 seconds)

Walk at crowd pace, scan figures, and decide: move toward the objective, close on a suspect, hold the defence, break line of sight, or slip into a bush. The action is satisfying on its own because each decision is a gamble on a read, the feedback is binary and immediate (a kill, a reversal, a strike), and the depth is human: the other player is reading you back.

### Short-Term (5-15 minutes)

One round. Each team works through two preliminary objective stages that force suspicious interactions in semi-public spaces (exposure is the price of progress), then both teams race for the one shared final target. A team learns the final court's location only once its own preliminaries are done (Objectives Chain SP-1), so contact at the final comes from the race and from region knowledge, not from a public meeting point. "One more round" is meant to live in a post-round reveal: who was who, and the read you almost made. The reveal is a Vertical Slice system (Post-Round Reveal & Results), not in the MVP list below; MVP rounds end with the result banner only.

### Session-Level (about 36-60 minutes)

Typically 3–5 rounds (~36–60 min of play plus intermissions) on freshly seeded layouts; the group chooses when to stop (no match-end rule in MVP). Natural stopping point at each round end; the reason to come back is that every round was a different read on different people.

### Long-Term Progression

Mastery of reading and blending is the progression. Systemic progression (rankings, cosmetics, unlocks) is outside first scope (scope-only anti-pillar) and is decided after the vertical slice.

### Retention Hooks

- **Curiosity**: New city layout and new opponents every round; "who was that?" reveals.
- **Investment**: Personal skill; team chemistry.
- **Social**: Playing with a fixed group of friends (private lobbies are the fallback product).
- **Mastery**: Reads get faster and subtler; bait and counter-bait play.

---

## Game Pillars

The pillars pull against each other on purpose: anonymity is sacred (1), yet every advantage betrays you (2); lethal stakes push toward passivity (3), and the race punishes passivity (4).

### Pillar 1: Hide in Plain Sight

The game never tells you who an enemy is: until someone commits, an enemy player cannot be told apart from an NPC by model, animation, audio, UI, or network presentation — only their choices give them away.

*Design test*: If a feature, asset or optimisation makes a player differ from an NPC in a way they did not choose (a cheaper NPC animation graph, LOD differences, network correction snaps, unique footstep set, an outlier Meshy model), we cut it or bring it to parity — by levelling players down to NPC fidelity if necessary, never by accepting the difference.

### Pillar 2: Every Advantage Is a Tell

The crowd is a readable baseline, and anything that helps a player — speed, shortcuts, interacting, hiding, preparing a defence — must be a visible deviation from that baseline.

*Design test*: If a mechanic grants an advantage without a readable deviation, we add a tell or cut it. NPC behaviour may create red herrings but must never turn reading into a coin flip. The seeded city exists so that reading the crowd beats memorising the map.

### Pillar 3: One Strike, Lasting Consequence

Every encounter resolves in one decisive action and every mistake sticks — a wrong read costs a strike, a failed attack costs your active life, and elimination is permanent for the round — but elimination never removes you from the match.

*Design test*: Between a decisive resolution and a forgiving one, choose decisive; between removing an eliminated player and giving them a meaningful reduced role, choose the role.

### Pillar 4: A Race, Not a Hunt

Objectives drive the match and force exposure; killing is how you protect your race, not the default way to win.

*Design test*: If a strategy lets a team win by waiting (camping bushes, sitting on the final site), we add objective pressure that pushes them out — never a timer, never a reveal. Validation: most playtest rounds should end by objective, not elimination.

### Anti-Pillars (What This Game Is NOT)

- **NOT a combat game**: no melee, health, dodge or weapons systems — they would replace the one-strike read with attrition and break Pillar 3.
- **NOT an identification-tools game**: no radar, compass, contract tracker, nameplates or outlines on enemies — they would compromise Pillars 1 and 2.
- **NOT a reflex game**: aim and reaction speed never decide an encounter; anticipation does. Protects the hold-to-prepare defence from becoming a parry window and keeps first-person mode from becoming twitch play.
- **NOT a hide-and-wait game**: hiding is a pause, not a strategy — protects Pillar 4 against bush camping.
- **NOT a large-map game**: compact cities keep crowd density and contact high (Pillars 2 and 4).
- **NOT (in first scope) a progression or cosmetics grind** — *scope-only*, not an identity statement; revisit after the vertical slice.

*Alternatives considered (CD-PILLARS):* the draft pillars "Read the Crowd" and "A Race, Not a Brawl" were replaced — the first duplicated Pillar 1's test, the second named an alternative the locked rules already exclude. A fifth "Procedural City" pillar was rejected in favour of justifying procgen under Pillar 2.

---

## Visual Identity Anchor

**Direction: "Held Breath"** (AD-CONCEPT-VISUAL: STRONG; chosen autonomously as the art director's dominant recommendation — see Pending User Decisions).

**Visual rule:** *The city holds still so behaviour can speak.*

Midday Tenochtitlan — causeways, canals, markets, stepped temples — hot, ordered, sunlit civic calm. Environment shapes are broad, horizontal, rectilinear and rhythmic (tiered stone and stucco, flat roofs, regular canal edges); characters are sturdy, simple silhouettes with legible limbs at distance. Repetition is deliberate: it is the baseline, and it suits the modular kit.

**Supporting principles:**

1. **Baseline before deviation** — ambient motion (water, banners, smoke) is slow, periodic and uniform.
   *Test:* could any looping environmental motion at crowd eye-height be mistaken for a human tell (sudden stop, head snap)? If yes, slow it or move it out of the eye-line.
2. **Silhouette and value over detail** — environment at low-to-mid contrast; characters read as figures with a legible heading.
   *Test:* at 20–30 m in greyscale, can a tester count figures and read direction of travel?
3. **Uniformity is a production contract** — every pool model shares height, proportions, material response, texture density and finish; Meshy output is normalised in Blender to that spec.
   *Test:* line up all pool models at 15 m in sun and shade; no tester can pick an outlier by finish alone. A model that "came out different" is a tell and a Pillar 1 violation.

**Colour philosophy:** Warm, earthy mid-value base (limewash, adobe, stone, water-green). Saturation is rationed for meaning: **turquoise** is reserved for teammate/allied UI only (fitting its precious status in Mexica culture); **cochineal red** is reserved for finality and danger in UI. No hue on any world character is unique to a team or role — colour belongs to the city, never to a player.

**Bush affordance:** hideable bushes have one distinct, consistent silhouette and foliage read that decorative foliage never uses — learnable, not a beacon.

*Alternatives considered:* "Obsidian Hour" (dusk/torchlight; strongest mood but darkness hides tells — kept as a possible later time-of-day variant) and "Codex Market" (flat codex pigment and linework; strongest cultural anchor but highest production risk for Meshy PBR output). A hybrid (Codex Market palette and glyph references inside Held Breath rendering) remains available to the art bible.

**Cultural framing flag:** the Mexica were an imperial, urban, canal-and-causeway civilisation, not "tribal" in the reductive sense. Avoid "savage" and sacrifice-spectacle tropes; ground references in the codices, archaeology and museum collections; consider a sensitivity read before the vertical slice.

---

## Inspiration and References

| Reference | What We Take From It | What We Do Differently | Why It Matters |
| ---- | ---- | ---- | ---- |
| Assassin's Creed Brotherhood MP | Look-alike crowds, behavioural tells, one-strike kills, stun/defence counterplay | No compass or contracts; 5v5 team objective race; informants; seeded cities | Proves the hidden-in-a-crowd PvP fantasy has a devoted audience |
| SpyParty | Reading a human among AI guests by behaviour; subtle tells as the whole skill | Team-based, 10 players, assassination rather than a single sniper shot | Validates behaviour-reading as a deep, competitive skill |
| The Ship | Hunt-and-be-hunted among NPCs with penalties for wrong kills | Team race instead of free-for-all; no weapons | Validates NPC-kill penalties as a design lever |

**Non-game inspirations**: the Florentine Codex and Codex Mendoza (visual reference, daily life of Tenochtitlan); Hitchcock's crowd-scene suspense (the threat in plain sight); the stillness of midday market photography.

---

## Target Player Profile

| Attribute | Detail |
| ---- | ---- |
| **Age range** | 18–40 |
| **Gaming experience** | Mid-core to hardcore |
| **Time availability** | 45–60 minute evening sessions with friends; weekend longer sessions |
| **Platform preference** | PC |
| **Current games they play** | Hunt: Showdown, Among Us / social deduction games, Dead by Daylight, AC:B MP nostalgia |
| **What they're looking for** | PvP where patience and observation beat aim; the spiritual successor to AC:B multiplayer |
| **What would turn them away** | Twitch aim requirements, pay-to-win, identification aids that remove the read, long matchmaking waits |

---

## Technical Considerations

| Consideration | Assessment |
| ---- | ---- |
| **Recommended Engine** | Unreal Engine 5.8 — the user's choice (project already exists in UE 5.8); C++ for server-authoritative rules, Blueprint for presentation and tuning. Foundation: Epic's Game Animation Sample (motion matching, replicated traversal on CharacterMovementComponent) |
| **Key Technical Challenges** | ~60 motion-matched characters (10 players + 50 NPCs) at parity (Pillar 1); per-viewer visibility for bushes; server-seeded runtime layout with navmesh; replicated traversal and assassination sync |
| **Art Style** | 3D stylised-realistic ("Held Breath") |
| **Art Pipeline Complexity** | High (custom 3D) mitigated by AI generation: Meshy MCP → Blender MCP (normalise, rig/retarget prep) → Unreal MCP |
| **Audio Needs** | Moderate — diegetic city ambience; footsteps at player/NPC parity; reversal/kill stingers |
| **Networking** | Client-server; legacy NetDriver with Iris pinned off (ADR-0001, ADR-0003; Iris migration deferred); listen server through vertical slice, with the host able to see server-only state (see Technical Risks); dedicated servers at Alpha (needs a source-built engine) |
| **Content Volume** | MVP: 1 compact city kit (~8–12 modules), 6–8 pool characters, 1 objective chain; full vision: 3+ districts/kits, 10+ objective types |
| **Procedural Systems** | Server-seeded modular city assembly with objective-site and spawn placement and validation |

**TD-FEASIBILITY: CONCERNS** (no blocker; UE 5.8 is a good fit). Key findings that shape the build:

- **Crowd identity is a networking/animation property, not only art** (highest risk): one pawn class for players and NPCs; AI drives it through the same movement/input path; no "is AI"-keyed LOD or anim budget (distance/significance only); hide the PlayerState link from non-team clients; teammate markers created only on teammates' clients. Recorded as ADR-0001 (Proposed; acceptance gated on spikes S1, S2 and S14).
- **The migrated sample is CharacterMovementComponent-based** (`CBP_SandboxCharacter`, with replicated traversal functions `Traversal_ServerImplementation` / `OnRep_TraversalResult`); commit to CMC for MVP, do not adopt Mover. Spike networked locomotion with net emulation (~100 ms, 1 % loss) before relying on traversal.
- **Crowd performance on a listen server**: the concept's original proposal (16.6 ms frame, ≤ 4 ms animation, ≤ 1.5 ms AI+nav, ≤ 50 KB/s per client) is superseded by the accepted budgets in ADR-0022 (the binding case is the listen-host game thread, ≤ 12.0 ms p95; all figures are assumed on an RTX 3060-class, 1080p60 reference that nobody yet owns). Crowd size is now 10 players + 50 NPCs = 60 characters; the only measurements (RTX 5090, 50 characters, graybox, no crowd AI or real connections) leave about 0.8 ms of game-thread headroom, so the budget is unproven at 60 and the lower-`NpcCount` fallback must stay an explicit option. Stress-test 60 characters; no MetaHumans or runtime retargeting in the crowd.
- **No version control** on a 3.7 GB agent-edited binary project — Git + LFS recommended (needs the user's go-ahead to commit).
- **Seeded generation**: integer-grid deterministic generation from a public geometry seed only (hidden placement uses server-only sub-seeds, ADR-0006), server-spawned replicated gameplay actors, server-side dynamic navmesh rebuild gated by a build lock (ADR-0014), round start via `ServerTravel` with the public seed as a URL option; generation last in MVP.
- **Meshy pool**: weight pool meshes to the Manny skeleton in Blender so motion-matching databases work natively.
- **Design flag (resolved in design)**: NPCs do enter bushes as context-free red herrings (Baseline and Tell Matrix T5, Crowd GDD), so entering a bush is evidence, not a confession; keep that a deliberate choice.

---

## Risks and Open Questions

### Design Risks

- 5v5 with ~50 NPCs may make enemies too easy or too hard to read — crowd density and NPC behaviour variety are the tuning knobs.
- Objectives may force reveals too strongly (everyone camps objective sites) or too weakly.
- No time limit + elimination win + full bush invisibility + a known final site create a camping incentive (Pillar 4 must be enforced through objective structure). The final site is now hidden until a team completes its preliminaries (Objectives Chain SP-1), but a whole team can still post campers across the candidate final courts; the structural counters and the playtest metrics (`ObjectiveEndShare`, final-dwell, kills near final candidates) are tracked in the Objectives Chain GDD and are first-5v5-playtest gate items.
- Informant respawn loop could be griefable (informants body-blocking or spamming pings).
- Teammate markers in small test configurations (2v2) may reveal enemies by elimination.

### Technical Risks

- Animation parity: players and NPCs must share the same motion-matching quality; 60 replicated motion-matched characters (10 players + 50 NPCs) is a performance risk (spike first, on 3060-class hardware).
- Per-viewer visibility (bush concealment) requires owner/relevancy-aware hiding, not global hiding.
- **Listen-host secrecy**: the host process holds server-only facts (the hidden final site, which pawns are players), so Pillar 1 is not literally enforceable against the host until the dedicated server exists (ADR-0014, ADR-0007). Until then, competitive play and evidence playtests need a non-competing host.
- Seeded runtime layout + navmesh rebuild + validation before round start.
- Mover/NetworkPrediction plugins are experimental — use CharacterMovementComponent for MVP; the plugins are disabled and no Mover or NetworkPrediction type may be referenced (ADR-0010). Motion Warping, used by traversal, is Beta in 5.8.4.
- Camping the final site (see Design Risks) is the main threat to Pillar 4; the downstream Objectives Chain GDD owns the structural counter.
- Dedicated server builds require a source-built UE 5.8.

### Market Risks

- Niche genre with a cult audience; few direct competitors (validation and risk).
- Every match needs 10 humans — matchmaking liquidity; fallback product is private lobbies with friends.

### Scope Risks

- Meshy-generated characters may not retarget cleanly to the UE mannequin skeleton.
- The full-rigor design pipeline and a prototype build run in parallel: prototype code must be promoted only after the matching ADR exists.
- The full MVP (7 core items plus items 8–11) is re-baselined at about 24 weeks expected, 18.5 optimistic and 36 pessimistic (producer's re-baseline of 2026-10-25, low confidence, solo developer plus AI agents; the Producer Note in the Systems Index and `production/estimates/2026-10-25_mvp-rebaseline.md` carry the detail). The earlier 8–10 and 10–12 week figures are superseded. Items 8–11 add about 3.5 weeks. The riskiest items (procgen, networked traversal) are ordered last.

### Open Questions

- ~~Does the final objective site stay hidden until a team completes its preliminaries?~~ Resolved in design: yes (Objectives Chain SP-1); whether that is enough anti-camping pressure is a playtest question.
- Do NPCs ever enter bushes? If not, entering a bush is itself a visible confession and a blind-strike target; the downstream Concealment GDD currently models NPC bush use as a red herring (T5, 0.19/min), so this is decided in design and the concept's Technical Considerations flag stands only as a reminder to keep it deliberate.
- What is the minimum supported team size for friend groups (private-lobby fallback), and does `NpcCount` scale with it? (Extends the 2v2 measurement caveat; see Review Follow-ups.)
- What is the right crowd count for readability at 5v5? (Perf spike + blending playtest.)
- ~~Kill-removal wording: "Arrested" or "Exiled"?~~ Resolved in design 2026-10-25 (HUD OQ5): the removal card reads "Informant" / "Too many wrong targets"; the rival-kill plate reads "Target eliminated" (D29, cross-review fix 2026-10-25: "exile" is reserved for the NPC-kill removal; was "Target exiled").
- How do informants communicate beyond pings and team chat? (Voice out of first scope.)

---

## MVP Definition

**Core hypothesis**: Reading and exploiting crowd anonymity under objective pressure is tense and fun in online 2v2–5v5 play. As written this cannot be falsified, so it is split into two testable halves:

- **MVP hypothesis (reading beats chance).** In mechanics-verified rounds a human who watches a suspect before striking hits a rival player on the first strike materially more often than a random pick among non-allies would (base rate `TeamSize / (TeamSize + NpcCount)` = 5/55 ≈ 0.09 at 5v5 with 50 NPCs, higher at smaller teams). Kill threshold for the prototype's PROCEED/PIVOT decision: if informed first-strike accuracy is not clearly above the base rate (set the exact margin before the first playtest), reading is not a skill and the concept needs rework.
- **Vertical Slice hypothesis (the 5v5 race is tense and fun).** At the first 10-player test: at least 70 % of at least 20 rounds end by objective with a median duration near Objectives Chain F1's ~9 min estimate (about 6.7 min of chain plus about 2 min of final contest; `TargetRoundMinutes` ~12 stays a non-binding tuning target, and a median under 6 min triggers Objectives' F1 pacing levers, AC-OC-19), `ObjectiveEndShare` reported per team-size bucket (only 4v4 and larger count for Pillar 4, per the cross-review), and a post-session survey on tension and wanting another round. The exact thresholds are set in the test plan, not here.

**Required for MVP** (user-specified, from the handoff checklist; build order is by risk — see below):

1. Networked round framework: two teams of up to five, active/informant/round-ended states, objective and elimination victories, server-authoritative state
2. Motion-matched locomotion with a small traversal set, first/third-person toggle, replicated
3. Shared character pool with ~50 wandering NPCs (`NpcCount` 50) and teammate-only markers
4. Assassination, prepared-defence reversal, NPC-kill removal, informants
5. One complete objective chain (two preliminary stages per team + shared final)
6. Seeded modular city layout from a small kit, identical on all clients
7. Bush concealment, per viewer

**Added to MVP by the user (decision 2026-10-25, "accept all recommendations"):**

8. In-game informant ping: a momentary, place-based, team-only ground ring any Active player or informant can drop where they look (`design/gdd/informant-pings-team-comms.md`); no sticky tracking marker (decision 4 stands). Team chat stays outside the game in MVP (external voice is a stated playtest requirement); in-game text chat and voice are Vertical Slice candidates.
9. Text-only end-of-round summary at `Ended`: the round result plus per-team totals (steps completed, members eliminated for the round, wrong targets), released only when the round ends (HUD-R14, Round `FRoundResult`).
10. Teammate-reconnecting indicator: a team-only line and marker label while an ally is reconnecting inside the grace (Session, HUD, Teammate Markers).
11. Keyboard "walk slower" modifier, so keyboard players can walk at the crowd's slower paces (input and locomotion; the crowd's slower paces return).

**Build order (risk-first, per PR-SCOPE):** MVP-core first — (1) round framework, (4) assassination/informants, (5) objective chain on a hand-built graybox map, (3) crowd with mannequin-variant pool, (7) bushes — verified in 2-client PIE; then (2) camera toggle and traversal replication; then (3) Meshy pool swap once one Meshy character retargets end-to-end; then (6) procedural layout.

**Explicitly NOT in MVP** (defer to later):

- Matchmaking, lobbies beyond direct join, progression, cosmetics
- Built-in voice chat and in-game text chat (external team voice is the MVP playtest requirement; the in-game ping is MVP)
- Dedicated servers
- Large kits, multiple districts, elaborate parkour

### Scope Tiers (if budget/time shrinks)

| Tier | Content | Features | Timeline |
| ---- | ---- | ---- | ---- |
| **MVP** | Graybox → small kit, 1 objective chain, mannequin → Meshy pool | The 7 core items plus the user-added items 8–11, mechanics verified in 2-client PIE (this checks the rules, not the 5v5 hypothesis; the 10-player test is a Vertical Slice item) | About 24 weeks (about 5.5 months) expected for all 11 items, 18.5 optimistic and 36 pessimistic; low confidence. Re-baselined 2026-10-25 for a solo developer plus AI agents: about 20.5 weeks for the 7 core items plus about 3.5 for items 8–11 (`production/estimates/2026-10-25_mvp-rebaseline.md`) |
| **Vertical Slice** | One polished district (kit-based), 3 objective variants | Core + invite/lobby join, 10-player test, layout validation hardening, post-round reveal, sensitivity read of the Mexica framing (a named exit gate) | +2–3 months after MVP |
| **Alpha** | Full kit, several objective types | All features rough; dedicated server (source-built engine); onboarding; private-lobby fallback product | +3–4 months after the Vertical Slice (cumulative about 9–15 months from the start, about 11–12 likely, on the re-baselined MVP; the Vertical Slice and Alpha increments were not re-estimated and were set against the earlier MVP pace) |
| **Full Vision** | Multiple districts, polished | Matchmaking, progression decision, polish | 18–24 months total (cumulative, solo developer plus AI agents; the Alpha-to-Full-Vision remainder is unplanned) |

---

## Pending User Decisions

Decided autonomously while the user was away (logged in `production/session-logs/decision-log.md`). **All six items below were confirmed by the user 2026-10-25** (no item changed):

1. **PR-SCOPE re-scope** (always-ask category, not applied): the producer recommends moving procedural layout to the vertical slice, using mannequin variants instead of Meshy characters in the MVP, and deferring traversal and the camera toggle to an "MVP-plus". Applied only as a *build order*; all 7 MVP items remain. — *Confirmed by the user 2026-10-25.*
2. **Pillar rewordings** (CD-PILLARS): adopted "Every Advantage Is a Tell", "One Strike, Lasting Consequence", "A Race, Not a Hunt", the widened Pillar 1, and two new anti-pillars. — *Confirmed by the user 2026-10-25.*
3. **Kill confirmation**: the attacker learns after the attack whether the target was a player or an NPC. — *Confirmed by the user 2026-10-25.*
4. **Informant communication**: momentary player-sent pings + team chat only; no sticky tracking marker. — *Confirmed by the user 2026-10-25.* The ping is MVP (user decision 2026-10-25); team chat is external voice in MVP.
5. **Visual anchor**: "Held Breath". — *Confirmed by the user 2026-10-25.*
6. **Working title**: "Smoking Mirror". — *Confirmed by the user 2026-10-25.*

---

## Review Follow-ups (2026-10-25)

Findings of the `/design-review` that the reviewer did not apply because they touch a user-confirmed item, belong in another document, or need the user's call. Full list: `design/gdd/reviews/game-concept-review-log.md`.

1. **Pillar 3 wording** ("every mistake sticks" overclaims, given the two free wrong strikes; "mistakes accumulate" is suggested). Not applied: pillar wording was confirmed by the user (Pending Decision 2).
2. **Pillar 4 validation wording** ("most playtest rounds should end by objective"): suggested measurable form is at least 70 % of at least 20 rounds, per team-size bucket. Not applied to the pillar text (same reason); the Vertical Slice hypothesis above already carries the measurable form.
3. **Pillar 1 parity verification**: the concept gives a design test; the verification path now lives downstream in ADR-0008 (test automation and leak parity) and ADR-0023 (playtest telemetry hooks). Recommended content for the test plan: an automated player-versus-NPC parity diff (pawn class, anim graph, LOD and significance, footstep set, replicated properties visible to non-team clients), a net-emulation correction-snap comparison, and a blind clip test whose identification accuracy is statistically indistinguishable from chance.
4. **Measurable rewrites of the Visual Identity "Test:" lines, the 10-second test and the feedback-latency claims** (tester counts, thresholds, p95 latency): belong in the art bible and test plan, not the concept.
5. **Fellowship ranked #2**: since the user moved the informant ping into MVP (2026-10-25), its MVP delivery is teammate markers, shared steps, the team ping, the reconnect line and the round summary; a coordination playtest target (for example pings per informant-minute and pings followed by an ally strike near the ring) is still recommended. Aesthetic ranking is a creative call.
6. **Informant agency and "huntable" framing**: informants being targetable is a confirmed rule; whether the reduced role is engaging enough (about 10 minutes of ping and chat after an early elimination) is a playtest metric to log, not a concept change.
7. **The pitch sells a hunt while Pillar 4 says "not a hunt"**: consider putting the race into the 10-second test line when the pitch is next revised.

---

## Next Steps

- [x] Concept director gates (CD-PILLARS, AD-CONCEPT-VISUAL, TD-FEASIBILITY, PR-SCOPE)
- [x] Fill in CLAUDE.md technology stack based on engine choice (`/setup-engine`)
- [x] `/art-bible` — `design/art/art-bible.md`
- [x] `/design-review design/gdd/game-concept.md` — NEEDS REVISION 2026-10-25 (full), revisions applied; second round (lean) Approved 2026-10-25
- [ ] **Prototype core idea** (`/prototype`) — the parallel build in `Source/StealthGame` is the prototype
- [x] `/map-systems` — `design/gdd/systems-index.md` (done in the parallel pipeline; the PROCEED/PIVOT call on the prototype is still open)
- [x] `/design-system` per MVP system (18 GDDs in `design/gdd/`)
- [ ] `/vertical-slice` in Pre-Production
- [ ] `/playtest-report`
- [ ] `/sprint-plan new`
