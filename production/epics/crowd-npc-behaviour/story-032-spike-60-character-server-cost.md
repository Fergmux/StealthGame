# Story 032: Spike: 60-character server cost on the reference host (50 NPCs + 10 players)

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-006 = <= 1.5 ms AI + nav; global-only cuts; never changes replication.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0022 (rendering and performance budgets, evidence standard), ADR-0013 (animation budgets), ADR-0008 (multi-process harness, named Insights scopes)
**ADR Decision Summary**: The six named scopes `Crowd.Director`, `Crowd.StateTree`, `Crowd.Follower`, `Crowd.Perception`, `Crowd.NavQuery`, `Crowd.Dispatch` sum to at most 1.5 ms game-thread time per frame at p99 and 0.8 ms mean on the listen host (AC-CNB-26), provisional pending this spike. A fixed global cut order is the fallback. The virtual input layer's per-tick cost is not in F5's `c_f` term and is added to this spike.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Evidence standard (Control Manifest, ADR-0013, ADR-0022): cooked Development standalone captures on 3060-class reference hardware; PIE, editor or RTX 5090 numbers are not evidence. The reference host's CPU model and clock are recorded with the capture. The listen-server host process only: bot clients run in separate processes, never in-process PIE. If no 3060-class host is available the result is NOT ASSESSED, never PASS.

**Why this story**: This is the epic's conditional gate (ADR-0016 verification (1) and (8), systems-index High-Risk system). The prototype measured 50 characters at 89.6 FPS and 11.2 ms game thread on an RTX 5090, a marginal about 0.19 ms per character for every system together; whether the Crowd share fits 1.5 ms on a 3060-class host with 60 pawns is unmeasured. The result decides whether the cost cut order (story 033) engages and whether `NpcCount` 50 stands (escalation to the creative-director before any `NpcCount` change). It owns the 50/60-character performance measurement: this is the early thin-slice spike; the exit-time pass on the full catalogue is story 038. Read GDD F5, OQ10 and AC-CNB-26, and `production/estimates/2026-10-25_mvp-rebaseline.md` (Crowd line).

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: named Insights scopes `Crowd.*` compiled in non-Shipping; performance evidence from cooked Development standalone captures on 3060-class reference hardware; cost cuts are global values from the fixed cut order (ADR-0016, Global rules)
- Forbidden: making a cost cut depend on distance, visibility, viewer or kind; changing replication settings; lowering `NpcCount` (ADR-0016); using PIE or RTX 5090 numbers as evidence
- Guardrail: Crowd <= 1.5 ms p99 / <= 0.8 ms mean per frame at 60 pawns and 60 fps (provisional; F5 estimate about 0.74 ms mean, 1.2 ms peak); AI + nav (server) <= 1.5 ms of the 16.6 ms frame

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-26 and ADR-0016 verification items (1) and (8), scoped to this thin-slice spike:*

- [ ] (AC-CNB-26 / verification 1) GIVEN the 60-pawn reference scene (50 NPCs + 10 bot players in separate processes) in a cooked Development/Test build on the reference host (CPU model and clock recorded), listen-server host process only, frame rate locked at 60 fps, a strike burst every 20 s, WHEN Insights captures 5 min with the scopes `Crowd.Director`, `Crowd.StateTree`, `Crowd.Follower`, `Crowd.Perception`, `Crowd.NavQuery` and `Crowd.Dispatch`, THEN the report records the summed game-thread time (p99 and mean) against 1.5 ms / 0.8 ms and a per-scope breakdown, with the server movement simulation of AI-moved pawns reported alongside (SCL budget).
- [ ] (verification 8) A 70-pawn run locked at 30 fps and the virtual-input layer's per-tick cost (including `EvaluateViewEnvelope`) are captured and reported with the cut-order steps predicted to engage.
- [ ] The report states PASS or FAIL of the provisional 1.5 ms rule, revises the F5 constants if needed, and states NOT ASSESSED (never PASS) if no 3060-class reference host is available; it is filed in `production/qa/spikes/` and feeds the ADR-0016 conditional-gate record.

---

## Implementation Notes

*Derived from ADR-0016 section 10 and Engine Compatibility verification items:*

- Thin slice: the Director, controller, StateTree skeleton, follower, virtual input layer, path budget, perception hash and the pause/route/yield behaviours (stories 004-012, 014) are enough; the catalogue and red herrings are not required for this early measurement (the exit pass in story 038 adds them). Strike bursts use scripted strikes through the bot player path.
- Scopes: the six `TRACE_CPUPROFILER_EVENT_SCOPE` names registered in story 004; capture with Unreal Insights on the host process only; the bots are separate processes.
- Compare against F5 (`c_f`, `c_p`, `c_d` terms) and record the per-scope numbers that would revise it. Report p99 and mean over the 5 min window and the worst single frame.
- 70-pawn run: 60 NPCs + 10 bound, 30 fps lock; report which cut-order steps (steering 30 to 20 Hz, neighbour cap, decisions 4 to 3 Hz, coarse A*, deferred stimulus rays) the numbers predict.
- The report is the deliverable; a spike has no permanent test. If the host is not available, still capture on the best available machine and label the evidence "not reference hardware, informational only".
- Escalation rule: a FAIL goes to the technical-director and creative-director before any `NpcCount` change, and a superseding ADR is requested (a failed spike is not handled by editing ADR-0016).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Reference only. The prototype's 41 and 50 character measurements (105 and 89.6 FPS, 9.5 and 11.2 ms game thread, RTX 5090) are the baseline to compare, not evidence. `Source/StealthGame/Private/Spikes/StealthMeasureSpike.cpp` shows how the previous measurement was taken; do not promote it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 033 (`story-033-cost-cut-order.md`): building the cut order
- Story 038 (`story-038-performance-pass-50-60-character.md`): the exit-time capture on the full catalogue
- ADR-0022 and the rendering epoch: GPU, draw-call and animation budgets

---

## Files

- `Source/StealthTestHarness/Private/CrowdCostCaptureTest.cpp` (scripted scene driver, non-Shipping)
- `production/qa/spikes/<date>_crowd-60-character-cost.md` (the report)
- `production/qa/evidence/<date>_crowd-60-character-insights-summary.md` (capture summary)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). The criteria are evidence-gathering and run on the reference host.*

- **AC-1**: `StealthGame.Crowd.MP.ServerCost` (thin-slice)
  - Given: the 60-pawn reference scene (50 NPCs, 10 bots in separate processes), cooked Development/Test build, listen-server host only, 60 fps locked, a strike burst every 20 s, reference-host CPU model and clock recorded
  - When: Insights captures 5 min with the six `Crowd.*` scopes
  - Then: the report records summed game-thread p99 and mean against 1.5 ms and 0.8 ms, a per-scope breakdown and the AI-moved-pawn movement cost
  - Edge cases: no PIE or editor numbers
- **AC-2**: seventy-pawn run
  - Given: a 70-pawn run locked at 30 fps
  - When: captured with the virtual-input layer's per-tick cost including `EvaluateViewEnvelope`
  - Then: both are reported with the cut-order steps predicted to engage
  - Edge cases: advisory, so a miss is not a failure
- **AC-3**: report content
  - Given: the capture results
  - When: the report is written
  - Then: it states PASS or FAIL of the 1.5 ms rule and revises the F5 constants if needed; it is filed in `production/qa/spikes/`; with no 3060-class host it states NOT ASSESSED, never PASS
  - Edge cases: borderline results

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration (spike): `production/qa/spikes/<date>_crowd-60-character-cost.md` with the Insights capture summary retained in `production/qa/evidence/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-004-director-core-ledger-and-phase-gating.md`, `production/epics/crowd-npc-behaviour/story-006-crowd-controller-and-look-pitch.md`, `production/epics/crowd-npc-behaviour/story-007-path-source-and-start-budget.md`, `production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md`, `production/epics/crowd-npc-behaviour/story-010-virtual-input-layer.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-033-cost-cut-order.md`, `production/epics/crowd-npc-behaviour/story-038-performance-pass-50-60-character.md`
