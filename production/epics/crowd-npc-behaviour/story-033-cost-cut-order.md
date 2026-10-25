# Story 033: Global cost-cut order with behaviour invariance (blocked on the 60-character spike)

> **Epic**: Crowd & NPC Behaviour
> **Status**: Blocked — BLOCKED: the 60-character spike (story 032, ADR-0016 conditional gate AC-CNB-26) has not reported; the cut order is built only to the extent the measured cost requires it, and which steps are needed is not yet known
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-006 = <= 1.5 ms AI + nav; global-only cuts; never changes replication.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0003 (replication settings fixed), GDD F6 (pipeline parity features)
**ADR Decision Summary**: Cost cuts are single global values applied in a fixed order for every AI-driven pawn, never by distance, visibility, viewer or kind; never touching replication settings; never lowering `NpcCount`. The fixed order: steering 30 to 20 Hz; neighbour candidate cap; decisions 4 to 3 Hz; coarse waypoint A* on the route graph instead of navmesh queries; defer stimulus rays. Invariance of the F6 motion features under each cut is tested, not assumed: AC-CNB-21 and AC-CNB-27 run at the default and at each step before that step may ship.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff API. All five cuts are Crowd-internal tuning; none touches net update frequency, relevancy or `NpcCount`.

**Why this story**: If the spike shows Crowd over budget on the reference host, the only sanctioned relief is this fixed order, and each step must be proven not to change behaviour statistics (a cost cut that makes NPCs move differently is itself a tell). The story is Blocked because which steps are needed (and in what combination at 70 pawns / 30 fps) is the spike's output; building all five speculatively would add untested code paths. Once story 032 reports the story is rescoped to the needed steps and set Ready. Read GDD F5, F6 and AC-CNB-21, AC-CNB-27.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: cost cuts are global values from the fixed cut order; Crowd uses the registered keys `CrowdSteerPerSec`, `CrowdDecisionPerSec`, `CrowdPathStartsPerFrameLimit` (ADR-0016)
- Forbidden: making a cut depend on distance, visibility, viewer or kind; changing replication settings; lowering `NpcCount`; adapting crowd rates or profiles during a round (ADR-0016)
- Guardrail: a step ships only when AC-CNB-21 and AC-CNB-27 pass at that step

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-21 and ADR-0016 section 10, scoped to this story:*

- [ ] (AC-CNB-21) GIVEN the reference scene at `CrowdDecisionPerSec` 4 and 3 and `CrowdSteerPerSec` 30 and 20, WHEN the nine F6 features are compared between settings, THEN each passes the F6 parity thresholds (|d| <= 0.1, variance ratio in [0.8, 1.25], KS D <= 0.1).
- [ ] Cost cuts are single global values applied in the fixed order (steering 30 to 20 Hz; neighbour candidate cap; decisions 4 to 3 Hz; coarse waypoint A* on the route graph; defer stimulus rays) and never depend on distance, visibility, viewer or kind, never change replication settings and never lower `NpcCount`.
- [ ] A cut-order step is enabled for shipping only when AC-CNB-21 and AC-CNB-27 pass at that step; the per-step results are recorded, and a build check fails if a step is enabled with no record.

---

## Implementation Notes

*Derived from ADR-0016 section 10:*

- A single `ECrowdCostStep` level (0 = default) read from the frozen tuning snapshot, applied identically to every AI-driven pawn. Steps 1 and 3 are just the registered rates; step 2 adds a neighbour candidate cap to the spatial hash query; step 4 swaps the path source for waypoint A* on the `CrowdRoute` graph (the path source is behind an interface from story 007); step 5 defers stimulus rays (story 008) while keeping their stimulus time.
- Timing statistics do not depend on the decision rate because delays run on the per-frame due-time list, and `Move`/`Look` are emitted every controller tick at every step; the F6 tests confirm that.
- Rescope on unblocking: implement only the steps the spike report predicts are needed for the 60 fps / 60 pawn case and record the others as deferred; keep the "step has a parity record" check for all.
- AC-CNB-27 runs in the nightly harness (story 034); this story consumes its result per step.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype had no cost levers beyond lowering the NPC count (forbidden here).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 032: the measurement that decides which steps are needed
- Story 034: the nightly pipeline-parity run
- Story 038: the exit pass that applies the steps and re-measures
- Any change to `NpcCount` or replication (escalate, never edit)

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdCostSteps.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdCostSteps.h`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdCostKnobsTest.cpp`
- `production/qa/evidence/crowd-cost-step-parity-<date>.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). The story is ADEQUATE as written but cannot start until story 032 reports.*

- **AC-1**: `StealthGame.Crowd.CostKnobsDoNotChangeBehaviour`
  - Given: the reference scene at `CrowdDecisionPerSec` 4 and 3 and `CrowdSteerPerSec` 30 and 20
  - When: the nine F6 features are compared between settings
  - Then: each passes the F6 parity thresholds (|d| <= 0.1, variance ratio in [0.8, 1.25], KS D <= 0.1)
  - Edge cases: all four setting pairs
- **AC-2**: `StealthGame.Crowd.CostCutsGlobalOnly`
  - Given: each cut-order step enabled in turn, in a scene mixing distance, visibility, viewers and pawn kinds
  - When: behaviour, replication settings and `NpcCount` are compared
  - Then: each cut is a single global value applied in the fixed order; none depends on distance, visibility, viewer or kind; none changes replication settings or lowers `NpcCount`
  - Edge cases: steps combined
- **AC-3**: `StealthGame.Crowd.CostStepParityRecord`
  - Given: shipping config and per-step result records
  - When: a build check compares enabled steps to records
  - Then: a step is enabled for shipping only if AC-CNB-21 and AC-CNB-27 passed at that step; per-step results are recorded
  - Edge cases: a step enabled with no record fails the check

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdCostKnobsTest.cpp` — must exist and pass; per-step parity record in `production/qa/evidence/crowd-cost-step-parity-<date>.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`, `production/epics/crowd-npc-behaviour/story-034-pipeline-parity.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-038-performance-pass-50-60-character.md`
