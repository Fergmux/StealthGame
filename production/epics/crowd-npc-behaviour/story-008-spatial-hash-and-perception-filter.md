# Story 008: Spatial hash, ray budget and the concealment-blind perception filter

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-003`, `TR-crowd-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-003 = Async path queries <= 2 starts/frame; navmesh frozen at OnLayoutReady; 2 m spatial hash. TR-crowd-004 = Identity blindness: no crowd code reads player/binding/team/state.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0001 (identity blindness; public facts only), ADR-0017 (Concealment implements `ICrowdPerceptionFilter`)
**ADR Decision Summary**: Perception uses only public facts (position, velocity, `ActionPose`, `TraversalState`, body facing) plus server line of sight from the NPC's eye socket, through the Director's 2 m spatial hash and a ray budget (at most 8 per frame, round-robin, deferred rays keep their stimulus time). The consumer-owned `ICrowdPerceptionFilter` excludes concealed pawns (not perceivable) and entering/exiting/emerging/concealed pawns (not an obstacle), identically for every pawn kind. No `UAIPerceptionComponent`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Sight checks are `LineTraceSingleByChannel` on a crowd sight channel (a project collision channel; its definition is a config change owned with the layout/collision work, ask before adding). `UAIPerceptionComponent` and team affiliation are banned by the ADR because they form a second, kind-aware perception model.

**Why this story**: NPC perception that could see a hiding player would make bushes useless and leak identity (GDD CNB-R2, AC-CNB-36). Doing the filter and hash before any behaviour that reads neighbours (yield, glance, conversation, reaction) keeps one query path. Read GDD CNB-R2 and R3 item 3, and AC-CNB-36.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: perception uses only public facts and server line of sight, excluding pawns `ICrowdPerceptionFilter` marks not perceivable / not an obstacle, identically for every kind; the driver kind comes from the `ICrowdControl` call (ADR-0016)
- Forbidden: `UAIPerceptionComponent` or team affiliation; including Roster, Objectives-assignment or Visibility Policy fact headers in Crowd code (ADR-0016)
- Guardrail: hash built once per frame; at most 8 rays per frame; inside the `Crowd.Perception` scope

---

## Acceptance Criteria

*From GDD CNB-R2, CNB-R3.3, AC-CNB-36 and ADR-0016 section 5, scoped to this story:*

- [x] The 2 m spatial hash is built once per frame from public positions and velocities, excludes pawns the filter marks not-an-obstacle, and returns identical results for player-driven and AI-driven pawns.
- [x] (AC-CNB-36) GIVEN fixed-seed scenes with a bush holding 0, 1 and 3 concealed pawns (player and NPC), WHEN NPC steering, glance targets, conversation partners and bush choices are recorded, THEN they are identical across the three occupancies and no glance targets a pawn without line of sight.
- [x] Line-of-sight rays are capped at 8 per frame, round-robin; a deferred ray keeps its stimulus time so the reaction delay is unaffected.
- [x] No `UAIPerceptionComponent` or team affiliation is used; with Concealment absent the filter defaults to everything-perceivable and bush red herrings are disabled.

---

## Implementation Notes

*Derived from ADR-0016 section 5 and Key Interfaces:*

- `ICrowdPerceptionFilter { IsPerceivable(const APawn*); IsSteeringObstacle(const APawn*); }` is defined in Crowd (consumer-owned), implemented by Concealment (ADR-0017) and registered weakly by the composition root via `UCrowdDirector::SetPerceptionFilter`. Until Concealment lands, tests use a stub filter.
- Hash: uniform grid, 2 m cells, rebuilt once per frame from pawn positions and velocities; planted and tending pawns appear as static 50 cm obstacles plus margin; bush `HideVolume`s are static obstacles read from the Manifest, never through the pawn list.
- Ray budget: queue of `{Requester, Target, StimulusTime}`; 8 per frame round-robin; a deferred ray reports with its original stimulus time so reaction delays (story 024) are measured from the stimulus.
- Glance targets, conversation partners, avoidance obstacles and reaction sources all come from this one query (AC-CNB-36 depends on it).
- A build check (extends the story 013 include check) asserts no `UAIPerceptionComponent` include.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype has no NPC perception. The prototype's per-viewer bush hiding is replaced by the Concealment epoch and is not read here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The Concealment epoch (`production/epics/concealment-bushes/EPIC.md`): the filter implementation
- Story 011, 015, 016, 022, 024: the behaviours that consume the query
- Story 013: the project-wide identity include check

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdSpatialHash.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdSpatialHash.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdPerceptionFilter.h`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdPerceptionTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.SpatialHashKindBlind`
  - Given: player-driven and AI-driven pawns, one marked not-an-obstacle by a filter stub
  - When: the hash is built for a frame
  - Then: built once per frame at 2 m cells from public positions and velocities; the flagged pawn is excluded; results identical for player-driven and AI-driven pawns
  - Edge cases: a pawn exactly on a cell boundary
- **AC-2**: `StealthGame.Crowd.PerceptionIgnoresConcealed`
  - Given: fixed-seed scenes with a bush holding 0, 1 and 3 concealed pawns (player and NPC mix)
  - When: steering, glance targets, conversation partners and bush choices are recorded
  - Then: records identical across the three occupancies; no glance targets a pawn without line of sight
  - Edge cases: a pawn leaves the bush mid-scene
- **AC-3**: `StealthGame.Crowd.RayBudget`
  - Given: more than 8 pending LOS rays, one deferred with stimulus time t0
  - When: frames run
  - Then: at most 8 rays per frame, round-robin; the deferred ray's reaction delay is measured from t0 and unchanged
  - Edge cases: deferred across several frames; the stimulus expires before the ray is cast
- **AC-4**: `StealthGame.Crowd.NoAiPerceptionComponent`
  - Given: the Crowd module and a scene without Concealment
  - When: the code is scanned and the scene runs
  - Then: no `UAIPerceptionComponent` or team affiliation; the filter defaults to everything-perceivable; bush red herrings disabled
  - Edge cases: Concealment becomes available later in the run

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdPerceptionTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Perception.HashKindBlind`, `StealthGame.Crowd.Perception.ConcealedOccupancy`, `StealthGame.Crowd.Perception.RayBudget`, `StealthGame.Crowd.Perception.NoPerceptionComponent`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-011-avoidance-and-yield.md`, `production/epics/crowd-npc-behaviour/story-015-conversation-and-turn-in-place.md`, `production/epics/crowd-npc-behaviour/story-022-bush-herring.md`, `production/epics/crowd-npc-behaviour/story-024-strike-stimulus-and-reactions.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 4/4 passing
**Deviations**: No Concealment filter yet: the perceivable flag is the stand-in seam (ICrowdPerceptionFilter). No crowd sight trace channel: rays use ECC_Visibility.
**Test Evidence**: `StealthGame.Crowd.Perception.HashKindBlind`, `StealthGame.Crowd.Perception.ConcealedOccupancy`, `StealthGame.Crowd.Perception.RayBudget`, `StealthGame.Crowd.Perception.NoPerceptionComponent`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
