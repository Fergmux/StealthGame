# Story 007: Path source and start budget: pre-baked edges, async queries and leg ids

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-003 = Async path queries <= 2 starts/frame; navmesh frozen at OnLayoutReady; 2 m spatial hash.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0014 (frozen navmesh at `OnLayoutReady`, `CrowdRoute` graph with pre-baked edge paths in the Manifest, server-only)
**ADR Decision Summary**: The follower takes pre-baked `CrowdRoute` edge paths from the Manifest when a leg follows an edge (no query); otherwise `FindPathAsync` through the Director's path-start budget (at most `CrowdPathStartsPerFrameLimit`, default 2, starts per frame, FIFO with round-robin deferral; a hitch defers, never drops). Results arrive at least two frames later and are accepted only through the epoch check and the request's leg id.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `UNavigationSystemV1::FindPathAsync` only queues the query; the navigation tick dispatches queued queries on the task graph and delivers the previous frame's results on the game thread. The navigation tick runs before `TG_PrePhysics`, so a query issued there is delivered at least 2 frames later (E7, verified in story 001). Recast async queries are assumed to return string-pulled corners (E11, unverified: the follower funnels the path itself if not). The navmesh is frozen at `OnLayoutReady` (ADR-0014); never carve at runtime.

**Why this story**: The path layer is the first place cost and correctness meet: it caps worst-case frame cost (the F5 peak term) and is where a stale result from a previous driver could move a pawn. Read GDD CNB-R3 items 2 and 3 (progress monitor), CNB-R15 and F5.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: legs along a `CrowdRoute` edge follow the edge's pre-baked path (server-only, stored with the route graph); other legs use `FindPathAsync`; async path results also match the request's leg id and the control epoch; navmesh frozen at `OnLayoutReady` (ADR-0014, ADR-0016)
- Forbidden: `MoveTo*`, `AITask_MoveTo`, a StateTree MoveTo task, `RequestDirectMove`; Mass Entity / Detour Crowd (ADR-0016)
- Guardrail: at most 2 path starts per frame; path work runs on the task graph (worker time, not counted in the 1.5 ms)

---

## Acceptance Criteria

*From GDD CNB-R3, CNB-R15, ADR-0016 section 4.1 and Risks, scoped to this story:*

- [x] At most `CrowdPathStartsPerFrameLimit` (default 2) async path starts occur per frame, FIFO with round-robin deferral; a stress of 50 simultaneous requests defers and never drops any.
- [x] A path result is accepted only if the controller's control epoch and the request's leg id both match; stale results are discarded; while waiting (at least 2 frames) the follower keeps its current `Move`.
- [x] Legs along a `CrowdRoute` edge use the pre-baked edge path with 0 `FindPathAsync` calls; an invalid path is re-queried, holding the last `Move` for at most 0.5 s and then `Move(0)`.
- [x] No path query is issued before `OnLayoutReady`, and no runtime navmesh rebuild occurs during a round.

---

## Implementation Notes

*Derived from ADR-0016 section 4 item 1 and Risks:*

- Put the budget in the Director core (headless-testable): a FIFO of requests `{Ordinal, LegId, Epoch, From, To}` with a per-frame start counter; the deferral keeps request order, never drops, and a hitch frame still honours the cap.
- Callback path: `FindPathAsync` delegate -> controller -> sink-style epoch and leg-id gate -> follower. Every accepted path replaces the follower's corner list; a rejected one is counted (`StalePathDrop`).
- Invalid or partial query: re-query; hold the last `Move` up to 0.5 s, then `Move(0)`. After two failed repaths or a failed or partial query the NPC abandons the leg and draws a new destination (GDD progress monitor).
- Coarse waypoint A* on the route graph instead of navmesh queries is the fourth step of the cost cut order (story 033); do not build it here, but keep the path source behind an interface so it can swap in.
- Pre-baked edge paths come from ADR-0014's Manifest data; reading them needs the level-socket-contract / layout service epochs. If they are not yet available, the story's tests use a fixture route graph.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces `MoveToLocation` and `GetRandomReachablePointInRadius` wandering in `Source/StealthGame/Private/StealthNpcController.cpp`, and the `FindCrowdPoint` placement helper in `StealthGameMode.cpp`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 009 (`story-009-follower-shaping-pace-and-stops.md`): corner funnelling, carrot, lane offsets, stops
- Story 033: the coarse-waypoint cost cut
- The layout-service epoch (`production/epics/level-socket-contract/EPIC.md`) and the procedural generator: route graph and baked paths

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdPathBudget.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdPathBudget.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdDirector.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdDirector.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdPathBudgetTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.PathStartBudget`
  - Given: 50 simultaneous path requests with the limit at 2
  - When: frames run until the queue drains
  - Then: at most 2 starts per frame; all 50 served, none dropped; stable FIFO with round-robin deferral
  - Edge cases: a hitch frame (> 33 ms) still honours the cap
- **AC-2**: `StealthGame.Crowd.PathResultGate`
  - Given: a request with leg id L and epoch E; results that match, carry a wrong epoch, or a wrong leg id
  - When: results are delivered 2 or more frames later
  - Then: only the matching result is accepted; the other two are discarded; the follower keeps its current Move while waiting
  - Edge cases: leg changed or epoch bumped in the delivery frame
- **AC-3**: `StealthGame.Crowd.PathEdgeAndInvalid`
  - Given: a leg along a `CrowdRoute` edge, and separately an invalid path result
  - When: the leg begins and the invalid result returns
  - Then: the pre-baked path is used with 0 `FindPathAsync` calls; an invalid path is re-queried, last Move held at most 0.5 s, then Move(0)
  - Edge cases: invalid again at exactly 0.5 s; repeated requery failure
- **AC-4**: `StealthGame.Crowd.PathNavFrozen`
  - Given: round startup with a path-query hook and a navmesh-rebuild counter
  - When: a query is requested before `OnLayoutReady`, then the round runs
  - Then: no query is issued before `OnLayoutReady`; the runtime rebuild count is 0 for the whole round
  - Edge cases: the pre-layout request is rejected or queued, not silently run

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdPathBudgetTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Path.StartBudget`, `StealthGame.Crowd.Path.AcceptanceGate`, `StealthGame.Crowd.Path.PrebakedEdges`, `StealthGame.Crowd.Path.NoQueryBeforeLayout`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-001-spike-engine-verifications.md`, `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`, `production/epics/crowd-npc-behaviour/story-006-crowd-controller-and-look-pitch.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md`, `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 4/4 passing
**Deviations**: No Level Socket Contract layout, CrowdRoute or area types exist yet: the pre-baked edges are a graybox grid of route nodes and the plaza stands in for the Market area.
**Test Evidence**: `StealthGame.Crowd.Path.StartBudget`, `StealthGame.Crowd.Path.AcceptanceGate`, `StealthGame.Crowd.Path.PrebakedEdges`, `StealthGame.Crowd.Path.NoQueryBeforeLayout`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
