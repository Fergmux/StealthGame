# Systems Index: Smoking Mirror (working title)

> **Status**: Under Review -- TD-SYSTEM-BOUNDARY CONCERNS (applied), PR-SCOPE OPTIMISTIC (partly applied, see Pending), CD-SYSTEMS CONCERNS (mostly applied, see Creative Director Note under Priority Tiers)
> **Created**: 2026-10-24
> **Last Updated**: 2026-10-25 (revision 3: user scope decisions of 2026-10-25 — Informant Pings & Team Communication moved to MVP; MVP HUD gains a text-only round summary and a teammate-reconnecting line; keyboard walk-slower modifier; Teammate Markers effort M; `ShowTeamNames` hold moved to VS) — revision 2 (2026-10-24): boundary changes from TD-SYSTEM-BOUNDARY and PR-SCOPE applied
> **Source Concept**: design/gdd/game-concept.md (user-locked rules: GAME_BUILD_HANDOFF.md "Confirmed game rules" and the confirmed decisions in production/session-state/active.md)

---

## Overview

Smoking Mirror is an online 5v5 social stealth game in which ten players share one crowd of look-alike characters with roughly fifty NPCs (`NpcCount` 50, changed by the user 2026-10-25), race through team objectives, and assassinate rivals who betray themselves by behaviour. Its mechanical scope is small in *verbs* (walk, interact, assassinate, brace, hide) and large in *constraints*: almost every system carries an obligation from Pillar 1 ("Hide in Plain Sight": nothing may make a player differ from an NPC in a way they did not choose) and Pillar 2 ("Every Advantage Is a Tell": every advantage must be a readable deviation from the crowd baseline). The index therefore promotes four things that would normally be implementation details to first-class systems: **Information Visibility Policy** (the one server-side rule set for who may know what; the design home of ADR-001), **Shared Character & Locomotion** (the single pawn class, possession model, animation and net-parity rules that make players and NPCs the same object), **Character Pool & Appearance** (the art bible's uniformity contract plus team-blind appearance assignment), and **Level Socket Contract** (the vocabulary a level -- hand-built graybox or generated -- must provide, so the riskiest system, the generator, is off the critical path).

Revision 2 changes (from the director gates): Procedural City Layout split into *Level Socket Contract* (Core) and *Procedural City Generator* (built last); Movement & Traversal renamed and widened to *Shared Character & Locomotion*; Input & Interaction Framework redefined around a *pawn command interface* that both human input and AI call; Assassination now depends on Concealment (not the reverse) and emits `OnPawnKilled` instead of depending on Crowd; Session & Lobby Flow split into *Session & Travel* (Core, MVP) and *Lobby UI* (Vertical Slice); Teammate Markers split from HUD; Round & Match owns the server-only Round Event Log; Dedicated Server (Alpha) split from Matchmaking (Full Vision).

System-by-system rationale (why each exists, which pillar it serves):

- **Tuning Data & Config** -- every number the user left open (about 12 min, 50 NPCs (user-changed from ~40 on 2026-10-25), about 2 m range, about 3 s interaction, 4 s defence, 60 s cooldown) is data-driven (coding standard); crowd density is the principal readability knob (Pillar 1). Values are read on the server and replicated, never trusted from clients.
- **Participant State & Roster** -- the single owner of participant identity, team and lifecycle state (Active / Informant / Round-ended). It owns data only: strike count, defence cooldown and concealment state live in the owning feature's component, never in the Roster record.
- **Input & Interaction Framework** -- the *pawn command interface* (Move, Look, Interact-begin/end, Prepare-begin/end, Attack, Hide) that Enhanced Input maps into and the AI controller calls identically (Pillar 1); hold-to-interact progress and cancellation; generic server-side action events (`OnActionStarted`, `OnActionCancelled`) other systems subscribe to.
- **Character Pool & Appearance** -- the pool (6-8 models, at most 24 distinct appearances), team-blind appearance allocation from one balanced shuffled deck (per-appearance counts differ by at most 1; players are random members of the deck; NPC draws are never conditioned on player appearances -- the earlier "at least three NPC duplicates" rule leaked statistically and is retired); allocator interface called by Round at round start. Guard for the future: any cosmetic must enter the NPC appearance deck at equal weight or be cut.
- **Session & Travel** -- join, `ServerTravel` with the seed as a URL option, seamless-travel persistence of match-scoped fields, disconnect and rejoin detection. Gameplay consequences of a disconnect belong to Round.
- **Round & Match Framework** -- teams of up to five, round phases (Lobby, Preparing, Active, Ended, Reset), win conditions (objective and elimination), the single server-side resolution queue that makes ties impossible, disconnect consequences, and the server-only Round Event Log.
- **Information Visibility Policy** -- one server-side query (`CanViewerKnow(viewer, subject, fact)`) and per-field replication audiences (ServerOnly / Owner / Team / All); states the UE defaults it must override (`APawn::PlayerState`, `PlayerArray` replicate to everyone); decides concealment enforcement; owns the MVP leak tests.
- **Level Socket Contract** -- spawn points, shared emergence sockets (used identically for round-start placement, NPC replacement and informant respawn so appearing at a spawn is never a player tell), objective sites, bush sites, crowd routes, traversal points and nav areas, plus a "layout ready" interface; implemented by the graybox map and by the generator.
- **Shared Character & Locomotion** -- one pawn class, possession by PlayerController or AIController, the AnimBP and motion-matching setup, significance-only animation LOD and the 4 ms / 50-character budget, net-parity settings (update frequency, priority, relevancy, cull distance identical for NPC and player pawns), simulated-proxy smoothing parity, speed tiers (walking at crowd pace vs running, a tell), traversal set, state restrictions (no run/parkour while prepared; informants restricted), footstep path parity.
- **Camera System** -- first/third-person toggle on one body with identical permissions and collision; the third-person camera can see nothing the body's line of sight cannot (corner-peeking is not a free observation advantage, Pillar 2); camera mode is not observable by other clients.
- **Crowd & NPC Behaviour** -- NPC population, baseline behaviour catalogue incl. a gaze catalogue (the "normal" that Pillar 2 measures deviation from) and the **Baseline and Tell Matrix** (one row per advantage: visible deviation, whether NPCs ever do it, owning system), route following, red herrings, NPC replacement after a kill (subscribes to `OnPawnKilled`), identity parity.
- **Concealment (Bushes)** -- hide/leave rules, per-viewer invisibility, blind-attack vulnerability via `IsConcealedFrom(viewer)`, what breaks concealment (listens to generic Attack action events), enforcement per the Visibility Policy.
- **Assassination & Defence Reversal** -- targeting, range (about 2 m), one-strike resolution, hold-to-prepare defence (about 4 s, 60 s cooldown from activation, not while running or in parkour), reversal (attacker eliminated, becomes informant), in-encounter ordering, kill confirmation; answers "NPC or player?" via a server-only Roster lookup and emits `OnPawnKilled(victim, killer, victimKind)`.
- **NPC-Kill Penalty & Informants** -- strike counter (removal on the third NPC kill; card "Informant" / "Too many wrong targets", HUD OQ5), informant role (walk and observe, targetable, killing one is not an NPC kill, respawns at spawn as informant, cannot see concealed players, not counted for elimination); MVP informant communication is the in-game team ping (system 20, MVP since the user decision of 2026-10-25) plus external voice.
- **Objectives Chain** -- objective definition, two team-specific preliminary stages, one shared final objective, locked-stage access, about 3 s interaction, and the **Stall pressure** rule (anti-camping mechanism as a testable rule, never a timer, never a reveal).
- **Procedural City Generator** -- seeded modular-kit assembly implementing the Level Socket Contract, server-only dynamic navigation, validation and reject-and-reseed; built last, designed with the contract.
- **Teammate Markers** -- teammate-only overhead markers created only on teammates' clients (security-critical, needed for the first blending test).
- **HUD** -- strike counter, defence indicator, objective progress, kill confirmation, removal message, a text-only end-of-round summary at `Ended` and a teammate-reconnecting feed line (both MVP by user decision 2026-10-25); depends on owner-only view-state interfaces each feature exposes, nothing that identifies a non-ally person.
- **Informant Pings & Team Communication** (MVP since the user decision of 2026-10-25) -- a momentary, place-based, team-only ground ring that any Active player or informant can drop where they look; the informant's in-game channel to the team (Fellowship). No in-game text chat or voice in MVP; pinging changes nothing an opponent can see or receive (Pillar 1).
- **Lobby UI, Post-Round Reveal & Results, Diegetic Audio, Event Feedback & Cues, Settings & Accessibility** -- Vertical Slice. Pillar-1 rules that live in these systems (footstep parity) are stated in the MVP GDDs that own the pawn.
- **Onboarding & Practice Mode, Telemetry & Analytics, Anti-Cheat & Info-Leak Security, Dedicated Server** -- Alpha. **Matchmaking** and **Progression & Cosmetics** -- Full Vision (the latter is a scope-only anti-pillar item, listed so it is not silently forgotten; guard: any cosmetic must enter the NPC appearance deck at equal weight or be cut). **Settings & Accessibility guard**: no accessibility feature may give directional or player-only information beyond the default presentation (parity review by the Visibility Policy owner).

---

## Systems Enumeration

The `#` column is the recommended design order. Planned design-doc paths are listed; the Status column is the source of truth for whether a file exists.

| # | System Name | Category | Priority | Status | Design Doc | Depends On |
|---|-------------|----------|----------|--------|------------|------------|
| 1 | Tuning Data & Config (inferred) | Core | MVP | In Review | design/gdd/tuning-data-config.md | — |
| 2 | Participant State & Roster (inferred) | Core | MVP | In Review | design/gdd/participant-state-roster.md | — |
| 3 | Input & Interaction Framework (inferred) | Core | MVP | In Review | design/gdd/input-interaction-framework.md | Tuning Data & Config, Participant State & Roster, Round & Match Framework (queue-item contract), Information Visibility Policy (hard, `CanViewerKnow` for `PawnRef`) |
| 4 | Character Pool & Appearance | Core | MVP | In Review | design/gdd/character-pool-appearance.md | Tuning Data & Config, Participant State & Roster (soft), Round & Match Framework (caller) |
| 5 | Session & Travel (inferred) | Core | MVP | In Review | design/gdd/session-travel.md | Participant State & Roster, Tuning Data & Config (soft), Character Pool & Appearance (soft), Shared Character & Locomotion (soft, rule 25d peer) |
| 6 | Round & Match Framework | Core | MVP | In Review | design/gdd/round-match-framework.md | Participant State & Roster, Session & Travel, Tuning Data & Config, Level Socket Contract, Character Pool & Appearance, Input & Interaction Framework, Shared Character & Locomotion |
| 7 | Information Visibility Policy (inferred) | Core | MVP | In Review | design/gdd/information-visibility-policy.md | Participant State & Roster, Round & Match Framework, Shared Character & Locomotion (soft), Session & Travel (soft), Input & Interaction Framework (soft), Character Pool & Appearance (soft), Level Socket Contract (soft) |
| 8 | Level Socket Contract (inferred) | Gameplay | MVP | In Review | design/gdd/level-socket-contract.md | Tuning Data & Config, Participant State & Roster (soft), Session & Travel (soft), Information Visibility Policy (soft) |
| 9 | Shared Character & Locomotion | Core | MVP | In Review | design/gdd/shared-character-locomotion.md | Input & Interaction Framework, Participant State & Roster, Character Pool & Appearance, Tuning Data & Config, Level Socket Contract (soft peer) |
| 10 | Camera System | Core | MVP | In Review | design/gdd/camera-system.md | Input & Interaction Framework, Shared Character & Locomotion, Tuning Data & Config |
| 11 | Crowd & NPC Behaviour | Gameplay | MVP | In Review | design/gdd/crowd-npc-behaviour.md | Character Pool & Appearance, Shared Character & Locomotion, Input & Interaction Framework, Level Socket Contract, Information Visibility Policy, Tuning Data & Config, Round & Match Framework, Participant State & Roster (soft), Session & Travel (soft), Assassination & Defence Reversal (soft), Concealment (Bushes) (soft), Objectives Chain (soft) |
| 12 | Concealment (Bushes) | Gameplay | MVP | In Review | design/gdd/concealment-bushes.md | Information Visibility Policy, Shared Character & Locomotion, Input & Interaction Framework, Level Socket Contract, Participant State & Roster, Round & Match Framework, Tuning Data & Config, Crowd & NPC Behaviour (hard for any external build), Camera System (soft) |
| 13 | Assassination & Defence Reversal | Gameplay | MVP | In Review | design/gdd/assassination-defence-reversal.md | Participant State & Roster, Input & Interaction Framework, Shared Character & Locomotion, Concealment (Bushes), Information Visibility Policy, Round & Match Framework, Tuning Data & Config, Camera System (soft) |
| 14 | NPC-Kill Penalty & Informants | Gameplay | MVP | In Review | design/gdd/npc-kill-penalty-informants.md | Participant State & Roster, Assassination & Defence Reversal, Shared Character & Locomotion, Round & Match Framework, Information Visibility Policy, Tuning Data & Config, Level Socket Contract, Crowd & NPC Behaviour, Character Pool & Appearance, Input & Interaction Framework (soft), Camera System (soft) |
| 15 | Objectives Chain | Gameplay | MVP | In Review | design/gdd/objectives-chain.md | Input & Interaction Framework, Round & Match Framework, Level Socket Contract, Participant State & Roster, Information Visibility Policy, Tuning Data & Config, Crowd & NPC Behaviour, Session & Travel |
| 16 | Procedural City Generator | Gameplay | MVP | In Review | design/gdd/procedural-city-generator.md | Level Socket Contract, Round & Match Framework (ordering only, via Level Socket Contract), Tuning Data & Config, Session & Travel (soft), Shared Character & Locomotion (soft, values) |
| 17 | Teammate Markers | UI | MVP | In Review | design/gdd/teammate-markers.md | Participant State & Roster, Information Visibility Policy, Concealment (Bushes), Camera System, Tuning Data & Config |
| 18 | HUD | UI | MVP | In Review | design/gdd/hud.md | Participant State & Roster, Round & Match Framework, Information Visibility Policy, Assassination & Defence Reversal, NPC-Kill Penalty & Informants, Objectives Chain, Concealment (Bushes), Input & Interaction Framework, Tuning Data & Config, Teammate Markers (soft), Camera System (soft), Session & Travel (soft) |
| 19 | Lobby UI (inferred) | UI | Vertical Slice | Not Started | design/gdd/lobby-ui.md | Session & Travel, Round & Match Framework |
| 20 | Informant Pings & Team Communication | Gameplay | MVP | In Review | design/gdd/informant-pings-team-comms.md | Participant State & Roster, Information Visibility Policy, Round & Match Framework, Input & Interaction Framework, Camera System, HUD, NPC-Kill Penalty & Informants, Tuning Data & Config, Teammate Markers (soft), Session & Travel (soft) |
| 21 | Post-Round Reveal & Results (inferred) | UI | Vertical Slice | Not Started | design/gdd/post-round-reveal.md | Round & Match Framework, Information Visibility Policy |
| 22 | Diegetic Audio (inferred) | Audio | Vertical Slice | Not Started | design/gdd/diegetic-audio.md | Shared Character & Locomotion, Crowd & NPC Behaviour, Assassination & Defence Reversal, Information Visibility Policy |
| 23 | Event Feedback & Cues (inferred) | UI | Vertical Slice | Not Started | design/gdd/event-feedback-cues.md | Assassination & Defence Reversal, NPC-Kill Penalty & Informants, HUD, Diegetic Audio, Information Visibility Policy (soft) |
| 24 | Settings & Accessibility (inferred) | Meta | Vertical Slice | Not Started | design/gdd/settings-accessibility.md | Input & Interaction Framework, HUD, Information Visibility Policy (soft) |
| 25 | Onboarding & Practice Mode (inferred) | Meta | Alpha | Not Started | design/gdd/onboarding-practice-mode.md | Crowd & NPC Behaviour, Round & Match Framework, HUD |
| 26 | Telemetry & Analytics (inferred) | Meta | Alpha | Not Started | design/gdd/telemetry-analytics.md | Round & Match Framework, Input & Interaction Framework (soft) |
| 27 | Anti-Cheat & Info-Leak Security (inferred) | Meta | Alpha | Not Started | design/gdd/anti-cheat-security.md | Information Visibility Policy, Round & Match Framework, Input & Interaction Framework (soft) |
| 28 | Dedicated Server (inferred) | Persistence | Alpha | Not Started | design/gdd/dedicated-server.md | Session & Travel, Round & Match Framework |
| 29 | Matchmaking (inferred) | Persistence | Full Vision | Not Started | design/gdd/matchmaking.md | Dedicated Server, Session & Travel |
| 30 | Progression & Cosmetics | Progression | Full Vision | Not Started | design/gdd/progression-cosmetics.md | Character Pool & Appearance, Participant State & Roster |

---

## Categories

| Category | Description | Systems in this game |
|----------|-------------|-----------------|
| **Core** | Foundation systems everything depends on | Tuning Data & Config, Participant State & Roster, Input & Interaction Framework, Character Pool & Appearance, Session & Travel, Round & Match Framework, Information Visibility Policy, Shared Character & Locomotion, Camera System |
| **Gameplay** | The systems that make the game fun | Level Socket Contract, Crowd & NPC Behaviour, Concealment (Bushes), Assassination & Defence Reversal, NPC-Kill Penalty & Informants, Objectives Chain, Procedural City Generator, Informant Pings & Team Communication |
| **Progression** | How the player grows over time | Progression & Cosmetics (deferred) |
| **Economy** | Resource creation and consumption | None (no economy in scope) |
| **Persistence** | Save state and continuity | Dedicated Server, Matchmaking |
| **UI** | Player-facing information displays | Teammate Markers, HUD, Lobby UI, Post-Round Reveal & Results, Event Feedback & Cues |
| **Audio** | Sound and music systems | Diegetic Audio |
| **Narrative** | Story and dialogue delivery | None (no authored plot in MVP) |
| **Meta** | Systems outside the core game loop | Settings & Accessibility, Onboarding & Practice Mode, Telemetry & Analytics, Anti-Cheat & Info-Leak Security |

---

## Priority Tiers

| Tier | Definition | Target Milestone | Design Urgency |
|------|------------|------------------|----------------|
| **MVP** | Required for the core loop to function. Without these, you can't test "is this fun?" | First playable prototype | Design FIRST |
| **Vertical Slice** | Required for one complete, polished area. Demonstrates the full experience. | Vertical slice / demo | Design SECOND |
| **Alpha** | All features present in rough form. Complete mechanical scope, placeholder content OK. | Alpha milestone | Design THIRD |
| **Full Vision** | Polish, edge cases, nice-to-haves, and content-complete features. | Beta / Release | Design as needed |

MVP is 19 systems; Vertical Slice 5; Alpha 4; Full Vision 2 (Informant Pings & Team Communication moved from Vertical Slice to MVP by the user 2026-10-25). Several MVP systems are deliberately small (Tuning Data & Config, Session & Travel, Level Socket Contract, Camera System, Teammate Markers): they exist to give a contract an owner, not to add features. The user-locked MVP content (the 7 items in the concept's MVP Definition) is unchanged; the user added four MVP items on 2026-10-25 (informant ping, text-only round summary, teammate-reconnecting indicator, keyboard walk-slower modifier; concept MVP Definition). The concept's PR-SCOPE re-scope is applied as a **build order only**: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

**Decided by the user 2026-10-25** (formerly pending): PR-SCOPE adjustment 6 declined (Tuning Data & Config stays a GDD; ADR-0004 covers the implementation); adjustment 4 confirmed as a tier move (Lobby UI is Vertical Slice; MVP keeps direct join plus start/reset inside Round & Match and Session & Travel); adjustment 9 declined (the Generator, Character Pool and traversal GDDs are not held for the three spikes; they stay *provisional on spike results*).

> **Creative Director Note** (MVP): CD-SYSTEMS returned CONCERNS (9). Applied as GDD requirements: Look in the pawn command interface and a gaze catalogue (Pillar 1), the Baseline and Tell Matrix in the Crowd GDD (Pillar 2), the third-person camera sees no more than body line of sight (Pillar 2), shared emergence sockets (Pillar 1), the Stall pressure rule in Objectives (Pillar 4), the deck-only appearance rule, and future-tier guards. Concerns 1-2 were **decided by the user 2026-10-25 ("accept all recommendations")**: the minimal in-game informant ping is MVP (system 20) and the text-only end-of-round summary is in the MVP HUD (HUD-R14, Round `FRoundResult` team aggregates). The MVP playtest protocol now records the in-game informant hook as tested; external team voice remains a stated requirement for nuance (no in-game text chat in MVP).

**Also decided by the user 2026-10-25 (scope):** the teammate-reconnecting indicator is MVP (HUD feed line + marker label, Team audience; Session OQ7 closed); a keyboard walk-slower modifier is MVP (IIF/SCL; Crowd's interim full-pace rule lifted); the `ShowTeamNames` hold action moves to VS (focus labels stay MVP; Teammate Markers OQ11); Teammate Markers effort S → M (OQ12); the strike-hint off option waits for playtest data (HUD OQ12); the informant awaiting audio bed is VS (Penalty OQ-NKP-12).

> **Producer Note** (PR-SCOPE: OPTIMISTIC): the 8-10 week MVP estimate was made before the design work was counted; revised expected estimate is 10-12 weeks for all 7 MVP items (16+ pessimistic), with a named "MVP-core" checkpoint at about week 6 (round, assassination/informants, objectives, crowd, bushes on a graybox map, 2-client PIE) as a planned fallback playtest build -- a checkpoint, not a substitute for the locked MVP. **Items 8-11** (in-game informant ping, text-only round summary, teammate-reconnecting indicator, keyboard walk-slower key), added to the MVP by the user on 2026-10-25 (game concept MVP Definition), are **not** in the 10-12 week figure: the producer must re-estimate them before sprint planning (Informant Pings is effort S in the order table; the other three are small additions inside Round/HUD, Roster/HUD/Markers and IIF/SCL/Crowd).

---

## Dependency Map

### Foundation Layer (no dependencies)

1. **Tuning Data & Config** -- every tunable is data; no gameplay GDD can be finished without naming its knobs here.
2. **Participant State & Roster** -- the single authority for team membership and Active/Informant/Round-ended state.

### Core Layer (depends on foundation)

3. **Input & Interaction Framework** -- depends on: Tuning Data & Config, Participant State & Roster, Round & Match Framework (queue-item contract).
4. **Character Pool & Appearance** -- depends on: Tuning Data & Config, Participant State & Roster (soft), Round & Match Framework (caller).
5. **Session & Travel** -- depends on: Participant State & Roster, Tuning Data & Config (soft), Character Pool & Appearance (soft), Shared Character & Locomotion (soft, rule 25d peer).
6. **Round & Match Framework** -- depends on: Participant State & Roster, Session & Travel, Tuning Data & Config, Level Socket Contract, Character Pool & Appearance, Input & Interaction Framework, Shared Character & Locomotion.
7. **Information Visibility Policy** -- depends on: Participant State & Roster, Round & Match Framework (post-round reveal is a phase rule), Shared Character & Locomotion (soft), Session & Travel (soft), Input & Interaction Framework (soft), Character Pool & Appearance (soft), Level Socket Contract (soft).
8. **Level Socket Contract** -- depends on: Tuning Data & Config, Participant State & Roster (soft), Session & Travel (soft), Information Visibility Policy (soft).
9. **Shared Character & Locomotion** -- depends on: Input & Interaction Framework, Participant State & Roster, Character Pool & Appearance, Tuning Data & Config, Level Socket Contract (soft peer).
10. **Camera System** -- depends on: Input & Interaction Framework, Shared Character & Locomotion, Tuning Data & Config.
11. **Crowd & NPC Behaviour** -- depends on: Character Pool & Appearance, Shared Character & Locomotion, Input & Interaction Framework, Level Socket Contract, Information Visibility Policy, Tuning Data & Config, Round & Match Framework, Participant State & Roster (soft), Session & Travel (soft), Assassination & Defence Reversal (soft), Concealment (Bushes) (soft), Objectives Chain (soft).

### Feature Layer (depends on core)

12. **Concealment (Bushes)** -- depends on: Information Visibility Policy, Shared Character & Locomotion, Input & Interaction Framework, Level Socket Contract, Participant State & Roster, Round & Match Framework, Tuning Data & Config, Crowd & NPC Behaviour (hard for any external build), Camera System (soft).
13. **Assassination & Defence Reversal** -- depends on: Participant State & Roster, Input & Interaction Framework, Shared Character & Locomotion, Concealment (Bushes), Information Visibility Policy, Round & Match Framework, Tuning Data & Config.
14. **NPC-Kill Penalty & Informants** -- depends on: Participant State & Roster, Assassination & Defence Reversal, Shared Character & Locomotion, Round & Match Framework, Information Visibility Policy, Tuning Data & Config, Level Socket Contract, Crowd & NPC Behaviour, Character Pool & Appearance, Input & Interaction Framework (soft), Camera System (soft).
15. **Objectives Chain** -- depends on: Input & Interaction Framework, Round & Match Framework, Level Socket Contract, Participant State & Roster, Information Visibility Policy, Tuning Data & Config.
16. **Procedural City Generator** -- depends on: Level Socket Contract, Round & Match Framework (ordering only, via Level Socket Contract), Tuning Data & Config, Session & Travel (soft), Shared Character & Locomotion (soft, values). (Round does not depend on it: Round's Preparing phase waits for a "layout ready" callback defined in the Level Socket Contract.)
17. **Informant Pings & Team Communication** (MVP since 2026-10-25) -- depends on: Participant State & Roster, Information Visibility Policy, Round & Match Framework, Input & Interaction Framework, Camera System, HUD, NPC-Kill Penalty & Informants, Tuning Data & Config, Teammate Markers (soft), Session & Travel (soft). (Listed in the Feature layer by its index row; it draws in the HUD's L0 layer, so it is built after HUD's layer root.)

### Presentation Layer (depends on features)

18. **Teammate Markers** -- depends on: Participant State & Roster, Information Visibility Policy.
19. **HUD** -- depends on: Participant State & Roster, Round & Match Framework, Information Visibility Policy, Assassination & Defence Reversal, NPC-Kill Penalty & Informants, Objectives Chain, Concealment (Bushes) (via owner-only view-state interfaces), Input & Interaction Framework, Tuning Data & Config, Teammate Markers (soft), Camera System (soft), Session & Travel (soft).
20. **Lobby UI** (VS) -- depends on: Session & Travel, Round & Match Framework.
21. **Post-Round Reveal & Results** (VS) -- depends on: Round & Match Framework (Round Event Log), Information Visibility Policy.
22. **Diegetic Audio** (VS) -- depends on: Shared Character & Locomotion, Crowd & NPC Behaviour, Assassination & Defence Reversal, Information Visibility Policy.
23. **Event Feedback & Cues** (VS) -- depends on: Assassination & Defence Reversal, NPC-Kill Penalty & Informants, HUD, Diegetic Audio, Information Visibility Policy (soft).

### Polish Layer (depends on everything)

24. **Settings & Accessibility** (VS) -- depends on: Input & Interaction Framework, HUD, Information Visibility Policy (soft).
25. **Onboarding & Practice Mode** (Alpha) -- depends on: Crowd & NPC Behaviour, Round & Match Framework, HUD.
26. **Telemetry & Analytics** (Alpha) -- depends on: Round & Match Framework (Round Event Log), Input & Interaction Framework (soft).
27. **Anti-Cheat & Info-Leak Security** (Alpha) -- depends on: Information Visibility Policy, Round & Match Framework, Input & Interaction Framework (soft).
28. **Dedicated Server** (Alpha) -- depends on: Session & Travel, Round & Match Framework.
29. **Matchmaking** (Full Vision) -- depends on: Dedicated Server, Session & Travel.
30. **Progression & Cosmetics** (Full Vision) -- depends on: Character Pool & Appearance, Participant State & Roster.

**Bottleneck systems** (many dependents; changes ripple): Round & Match Framework, Participant State & Roster, Information Visibility Policy, Tuning Data & Config, Shared Character & Locomotion, Input & Interaction Framework.
**Leaf systems** (no dependents; safe to design late): Informant Pings & Team Communication (MVP, soft VS dependents only), Telemetry & Analytics, Anti-Cheat & Info-Leak Security, Matchmaking, Progression & Cosmetics, Onboarding & Practice Mode, Settings & Accessibility, Post-Round Reveal & Results, Event Feedback & Cues, Lobby UI, Procedural City Generator.

---

## Recommended Design Order

Design order equals the `#` column in the enumeration table.

| Order | System | Priority | Layer | Agent(s) | Est. Effort |
|-------|--------|----------|-------|----------|-------------|
| 1 | Tuning Data & Config | MVP | Foundation | systems-designer | S |
| 2 | Participant State & Roster | MVP | Foundation | game-designer, ue-replication-specialist | M |
| 3 | Input & Interaction Framework | MVP | Core | game-designer, ux-designer | S |
| 4 | Character Pool & Appearance | MVP | Core | art-director, game-designer | M |
| 5 | Session & Travel | MVP | Core | network-programmer, ue-replication-specialist | S |
| 6 | Round & Match Framework | MVP | Core | game-designer, systems-designer | M |
| 7 | Information Visibility Policy | MVP | Core | game-designer, ue-replication-specialist | M |
| 8 | Level Socket Contract | MVP | Core | level-designer | S |
| 9 | Shared Character & Locomotion | MVP | Core | game-designer, gameplay-programmer | L |
| 10 | Camera System | MVP | Core | game-designer, ux-designer | S |
| 11 | Crowd & NPC Behaviour | MVP | Core | game-designer, ai-programmer | L |
| 12 | Concealment (Bushes) | MVP | Feature | game-designer, ue-replication-specialist | M |
| 13 | Assassination & Defence Reversal | MVP | Feature | game-designer, systems-designer | L |
| 14 | NPC-Kill Penalty & Informants | MVP | Feature | game-designer | M |
| 15 | Objectives Chain | MVP | Feature | game-designer, level-designer | M |
| 16 | Procedural City Generator | MVP | Feature | level-designer, systems-designer | L |
| 17 | Teammate Markers | MVP | Presentation | ux-designer, ue-umg-specialist | M (re-estimated from S by user decision 2026-10-25; F4 edge spreading cuttable) |
| 18 | HUD | MVP | Presentation | ux-designer, ue-umg-specialist | M |
| 20 | Informant Pings & Team Communication | MVP (user decision 2026-10-25) | Feature | game-designer, ux-designer | S |
| 19, 21-24 | Lobby UI, Post-Round Reveal, Diegetic Audio, Event Feedback & Cues, Settings & Accessibility | Vertical Slice | Presentation/Polish | ux-designer, audio-director, accessibility-specialist | S-M each |
| 25-28 | Onboarding & Practice Mode, Telemetry & Analytics, Anti-Cheat & Security, Dedicated Server | Alpha | Polish | game-designer, analytics-engineer, security-engineer, network-programmer | S-L |
| 29-30 | Matchmaking, Progression & Cosmetics | Full Vision | Polish | network-programmer, economy-designer | L |

Effort: S = 1 session, M = 2-3 sessions, L = 4+ sessions.

---

## Circular Dependencies

No hard cycles. Soft cycles identified in the problem domain were removed or contained by contract (TD-SYSTEM-BOUNDARY concern 4):

- **Concealment / Assassination** -- removed. Concealment owns the concealed-target contract (`IsConcealedFrom(viewer)`) and listens to the *generic* Attack action event from the Input & Interaction Framework; Assassination depends on Concealment; Concealment does not depend on Assassination.
- **Round / Level Socket Contract / Generator** -- contained. Round has a Preparing phase and waits for a "layout ready" callback defined in the Level Socket Contract; Round does not depend on the Generator.
- **Participant State / Round / NPC-Kill Penalty** -- contained. Participant State owns the state machine and `RequestTransition`; Round and Penalty call it; the dependency arrows point from triggers to the owner. Round owns the single total resolution order; Assassination owns ordering only inside one encounter and submits outcomes to Round's queue; both GDDs must state the same tie-break key.
- **Information Visibility Policy / consumers** -- the Policy exposes one query and audiences; consumers call it and never re-derive rules. Its hard dependencies are Roster and Round; the soft entries added in the 2026-10-24 reconciliation (SCL, Session, IIF, Character Pool, LSC) are inputs it audits, not services it calls.

**Mutual edges introduced by the 2026-10-24 reconciliation pass** (orchestrator note; each is contained by interface ownership, so no build-order cycle exists — the interface owner is built first, as a header/contract, and the caller second):

- **IIF / Round** -- Round owns the queue (`Submit`, `GetKey`, `Compare`, item lifecycle); IIF owns the `PawnCommand` resolver and the `HoldTimer` items. Round calls IIF only through the registered-resolver interface it defines. Contract owner: Round.
- **Character Pool / Round** -- Pool owns the allocator interface (`AllocateDeck`, `TakeNpcCard`); Round only calls it at start and on re-pawn. Pool's dependency on Round is "caller identity" only. Contract owner: Pool.
- **Crowd / Concealment, Crowd / Objectives, Crowd / Assassination** -- Crowd's entries are soft (red-herring duty uses each feature's public command path); the features depend on Crowd only for the NPC-parity requirement (rates that make a tell ambiguous), which is a tuning contract, not a call. Contract owners: the feature GDDs (paths) and Crowd (rates).
- **IIF / IVP** (closeout cross-doc pass 2026-10-25) -- IIF calls `CanViewerKnow` to honour a `PawnRef` (hard); IVP lists IIF only as an audited input (soft). Contract owner: IVP.
- **IVP / LSC** -- both soft: LSC supplies geometry facts the Policy audits; the Policy supplies the audience rules LSC's layout-ready payload respects. Contract owner: IVP.

---

## High-Risk Systems

| System | Risk Type | Risk Description | Mitigation |
|--------|-----------|-----------------|------------|
| Information Visibility Policy | Technical + Design | Pillar 1 is violated by any leak (PlayerState link, relevancy, markers, anim LOD); easy to introduce, hard to see | ADR-001 first; one server-side query; automated MVP tests asserting a non-team client cannot read player-ness |
| Shared Character & Locomotion | Technical | CMC traversal replication under loss is unproven; 50 motion-matched characters vs the 4 ms animation budget; net-update cadence can become a tell | Net-emulation spike (100 ms, 1 % loss); 50-character stress test; identical net settings for NPC and player pawns |
| Crowd & NPC Behaviour | Design + Technical | Readability tuning and AI cost (1.5 ms); AI must use the player path | Prototype in graybox; density/variety as data; ADR-001 parity |
| Procedural City Generator | Technical + Scope | Identical layout on all clients, runtime navmesh, validation, objective reachability | Built last; Level Socket Contract keeps it off the critical path; integer-grid generation on the project's PCG32 streams (ADR-0006; `FRandomStream` is not used for layout); server-only dynamic nav; hand-built map fallback |
| Concealment (Bushes) | Technical | Per-viewer hiding must close shadow/Lumen/reflection leaks (art bible R5); client-side hide readable by cheaters vs relevancy culling affecting blind-attack hit detection | Decision lives in the Visibility Policy; spike |
| Assassination & Defence Reversal | Design | Simultaneous-event resolution; defence must not become a reflex parry | Round's single resolution queue; prepare is a held state with no attacker warning |
| Objectives Chain | Design | Camping incentive; objectives may expose too much or too little | Final site hidden until preliminaries done; tune interaction time; playtest |
| Character Pool & Appearance | Scope | Meshy characters may not retarget to Manny cleanly | Mannequin-variant pool first; shared body + Meshy heads fallback (art bible 1.3) |
| Session & Travel | Technical | `ServerTravel` recreates PlayerStates each round; seamless-travel persistence is easy to get wrong | Roster classifies fields as match-scoped or round-scoped; test with 2 clients |

---

## Progress Tracker

| Metric | Count |
|--------|-------|
| Total systems identified | 30 |
| Design docs started | 19 (Informant Pings & Team Communication added 2026-10-25) |
| Design docs reviewed | 19 (one review + fix pass each; all NEEDS/MAJOR REVISION with revisions applied; independent re-reviews pending per the Re-review policy; Informant Pings: lean review NEEDS REVISION 2026-10-25, revisions applied) |
| Design docs reconciled (cross-GDD change requests routed) | 18 (2026-10-24) |
| Design docs approved | 0 |
| MVP systems designed | 19/19 |
| Vertical Slice systems designed | 0/5 |

*Updated 2026-10-25 (phase-2 scope pass: row 20 moved to MVP and designed). Previously updated 2026-10-24 by the design-pipeline orchestrator.*

---

## Next Steps

- [x] Design MVP-tier systems in order (use `/design-system [system-name]`)
- [x] Run `/design-review` on each completed GDD (first pass; independent re-reviews pending)
- [ ] Run `/review-all-gdds` when MVP systems are designed
- [ ] Run `/gate-check technical-setup` when MVP systems are designed
- [ ] Validate the highest-risk systems with `/vertical-slice` before committing to Production
