# Story 009: Follower: path shaping, pace set, corners and the shared stop floor

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0010 (stop-and-blend, last-command hold), shared-character-locomotion GDD (SCL F2 stop floor, F3 arc, R5.1 pace set)
**ADR Decision Summary**: The `FCrowdFollower` turns a path into human-shaped inputs: string-pulled corners, per-pawn lane offset (+-0.5 m), pure-pursuit carrot at `max(150 cm, v * 1 s)`, early turn to fit the SCL F3 arc, and `Move(0)` at the SCL F2 stop distance. Pace magnitude comes from the shared set {0.8, 0.9, 1.0}, chosen at a leg start or after a stop.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine path-following is used (E3, E8); the follower is plain C++ stepped from the controller tick. Recast corner output may or may not be string-pulled (E11, unverified): if not, the follower funnels the corners itself, and AC-CNB-04 is the test that tells.

**Why this story**: NPC motion is the most observed signal in the game; pace share, corner speed and stop length are all things a human watcher can read as "bot". Read GDD CNB-R3 items 1 and 2, the "Stops use the shared stop floor" note, and AC-CNB-03, AC-CNB-04. SCL F2/F3 constants are read from the SCL tuning keys, never copied.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: the controller steps its due-time list and follower before `Super::Tick`; the virtual camera, input quantisation and `Move`/`Look` emission run every controller tick; carrot and path recompute at `CrowdSteerPerSec` with a per-pawn phase offset (ADR-0016); gameplay values from validated tuning
- Forbidden: `AddMovementInput`, `RequestDirectMove`, CMC velocity writes; never time crowd delays in decision-tick counts (ADR-0016)
- Guardrail: follower cost sits in the `Crowd.Follower` scope within the 1.5 ms p99 budget

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-03 and AC-CNB-04 and CNB-R3.2, scoped to this story:*

- [ ] (AC-CNB-03) GIVEN 10 min of the reference scene, WHEN NPC walk magnitude is logged, THEN only {0.8, 0.9, 1.0} occur, the share of walking time at 1.0 is within +-3 points of `CrowdFullPacePct` and never below 50 %, C4 pace matches occur and each lasts at most `CrowdGroupDriftSec`, and every pace change happens at a stop or a leg boundary.
- [ ] (AC-CNB-04 a, b) GIVEN 200 recorded route legs with a 45-90 degree corner, WHEN replayed, THEN at least 95 % of corners are taken without speed dropping below 0.8 x leg pace, and no commanded heading change exceeds `ReversalAngleDeg` while moving except C7 reversals.
- [ ] (AC-CNB-04 c) Every NPC stop to rest lasts at least `StopMinSec` (0.5 s) from the first braking frame to zero speed, including stops begun inside a start (SCL F2 stop floor).
- [ ] Arrival issues `Move(0)` at the SCL F2 stop distance of the current pace (53 / 42 / 34 cm at 140 / 126 / 112 cm/s) before the stop point; each pawn's lane offset (+-0.5 m) is drawn once from its stream.

---

## Implementation Notes

*Derived from ADR-0016 section 4 items 2 and 5 and GDD CNB-R3:*

- Pace chosen only at a leg start or after a stop (`ECrowdDecision::Pace`); time share at 1.0 equals `CrowdFullPacePct` (55 % at defaults; SCL R5.1 requires at least 50 %). C4 group drift (a walker within 2 m heading within 30 degrees may match a neighbour's pace for at most `CrowdGroupDriftSec` per leg) reads neighbours from the spatial hash of story 008; it is part of this story because it changes pace.
- Stops: every stop goes through SCL's one braking path and obeys `d_eff = min(d, v0 / StopMinSec)`; planners must never issue a `Move(0)` sequence that produces a shorter visible stop. Crowd reads `StopMinSec` from SCL and owns no pace-set or stop key.
- Corners: begin turning early enough to fit the SCL F3 arc (134 cm at 1.0) so NPCs do not stop at corners; never command a heading change above `ReversalAngleDeg` while moving except the deliberate C7 reversal (story 019).
- Lane offset: `ECrowdDecision::LaneOffset`, drawn once per pawn, +-0.5 m.
- Output goes through the virtual input layer (story 010) when it exists; until then the follower may emit direct `Move` directions through the sink in tests. The shaping logic must not depend on the device profile.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the acceleration-based `MoveToLocation` wandering in `Source/StealthGame/Private/StealthNpcController.cpp`. The prototype ran 50 characters at 89.6 FPS on an RTX 5090 but with no pace set, no corner shaping and no stop floor; none of it is reused.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 007: path acquisition and start budget
- Story 010: device profiles and the virtual camera (how the direction becomes input)
- Story 011: avoidance offsets and yield
- Story 019: the C7 reversal behaviour

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdFollower.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdFollower.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdFollowerTest.cpp`
- `Source/StealthTestHarness/Private/CrowdPaceMotionTest.cpp` (reference-scene run)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.PaceSetAndFullPaceShare`
  - Given: 10 min reference scene with walk magnitude logged and fixed seeds
  - When: the log is analysed
  - Then: only {0.8, 0.9, 1.0} occur; 1.0 share of walking time is `CrowdFullPacePct` +-3 points and at least 50 %; C4 matches occur, each at most `CrowdGroupDriftSec`; pace changes only at a stop or leg boundary
  - Edge cases: share by time, not by count
- **AC-2**: `StealthGame.Crowd.NoCornerStops`
  - Given: 200 recorded legs with a 45-90 degree corner
  - When: replayed
  - Then: at least 95 % of corners taken at no less than 0.8 x leg pace; no commanded heading change exceeds `ReversalAngleDeg` while moving, except C7
  - Edge cases: corners at exactly 45 and 90 degrees
- **AC-3**: `StealthGame.Crowd.StopFloor`
  - Given: the same replay, including stops begun inside a start
  - When: first braking frame to zero speed is measured at a 1/60 s step
  - Then: every rest stop lasts at least `StopMinSec` (0.5 s)
  - Edge cases: a start followed immediately by a stop
- **AC-4**: `StealthGame.Crowd.ArrivalDistanceAndLane`
  - Given: arrivals at paces 1.0, 0.9 and 0.8, and NPCs with lane offsets
  - When: `Move(0)` is issued and offsets are logged
  - Then: `Move(0)` at 53, 42 and 34 cm before the stop point (+- one tick of travel); each lane offset in +-0.5 m, drawn once per pawn
  - Edge cases: offset unchanged across legs; pace change during approach

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdFollowerTest.cpp` — must exist and pass (AC-1 may run in `Source/StealthTestHarness/Private/CrowdPaceMotionTest.cpp` as a headless accelerated run)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-005-command-sinks-epoch-and-halt-filter.md`, `production/epics/crowd-npc-behaviour/story-006-crowd-controller-and-look-pitch.md`, `production/epics/crowd-npc-behaviour/story-007-path-source-and-start-budget.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-010-virtual-input-layer.md`, `production/epics/crowd-npc-behaviour/story-011-avoidance-and-yield.md`, `production/epics/crowd-npc-behaviour/story-013-identity-and-command-path-gate.md`, `production/epics/crowd-npc-behaviour/story-019-run-traversal-reversal-herrings.md`, `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`
