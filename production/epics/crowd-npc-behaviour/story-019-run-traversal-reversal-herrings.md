# Story 019: Red herrings: late-porter runs, public traversals and "forgot something" reversals

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: shared-character-locomotion GDD (R5.3 braking, R7.3 traversal non-zero rate), input-interaction-framework GDD (Traverse command)
**ADR Decision Summary**: Red herrings run through the same sinks as any command: runs are `Move` with the Run gait, traversals use `Traverse` after a `CanBegin` dry run, reversals are a stop-pivot-start. Tell rates come from the frozen tuning snapshot and the compensated sampler (story 018).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Traversal uses nav links at `TraversalPoint`s on public coping; the pawn's traversal is the shared SCL state, so the crowd supplies only the command and the hint (`FTraversalHint`). No post-cutoff API.

**Why this story**: Run (T1), traversal (T2) and reversal (T12) are advantages a player has; if NPCs never did them each sighting would be a confession. Rates are non-zero by SCL R7.3 and set by F2. Read GDD CNB-R7 run and traversal bullets, C7 and R16 rows T1, T2, T12.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: discrete commands through `FCrowdCommandSink` after a `CanBegin` dry run; every draw via `FCrowdDraws`; legs along a route edge follow the pre-baked path (ADR-0016)
- Forbidden: `LaunchCharacter`, CMC velocity writes, `MoveTo*`; never move a pawn except through `Move`/`Traverse` (ADR-0016)
- Guardrail: herring decisions are due-time entries; no per-frame cost when idle

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` CNB-R7 and CNB-R5 (C7), scoped to this story (the rate gate is story 023):*

- [ ] Run bouts last 2-6 s, start and end at leg boundaries, destinations are drawn by R13 weights (each destination equals the weighted inverse-CDF result for its logged u, and none is a court), and the run ends by ordinary SCL braking.
- [ ] Traversal herrings occur on legs crossing public coping (a nav link at a `TraversalPoint`); accepted vault and climb rates lie in the 99 % Poisson interval of F2 (as AC-CNB-10); climbs occur only where coping is at least 1.6 m; `Traverse` goes through the sink after the `CanBegin` dry run.
- [ ] C7 reversal: stop (at least `StopMinSec`), pivot, start back along the last leg; the reversal rate lies in the 99 % Poisson interval of `60 / CrowdReversalIntervalSec` per NPC, and it is the only behaviour allowed a heading change above `ReversalAngleDeg` while moving.

---

## Implementation Notes

*Derived from GDD CNB-R7 and CNB-R5:*

- Run: "a late porter", 2-6 s bouts starting and ending at leg boundaries; ends by ordinary SCL braking (no instant stop); rate 0.148/min (`CrowdRunIntervalSec` 405).
- Traversal: only on legs that cross public coping with the F2 probability (0.042 vault, 0.018 climb per minute at defaults); climb only at coping 1.6 m or higher (temple steps, canal walls). The dry run uses 10 cm / 10 degree margins; a rejection abandons the behaviour and the target is not retried for 10 s.
- Reversal: "forgot something", rate 0.15/min (`CrowdReversalIntervalSec` 395); the stop obeys the SCL stop floor, the pivot uses the SCL pivot state.
- All three feed the calibration counters of story 018 and are bounded by the chain-gap guard.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; prototype NPCs only walked. New behaviours.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 018: clocks and guard
- Story 021: brace and startle
- Story 023: the 600-minute rate gate
- The SCL epoch (`production/epics/shared-character-locomotion/EPIC.md`): traversal and run states themselves

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdHerringRunTasks.cpp`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdHerringTraverseTasks.cpp`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdReversalTasks.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdHerringMotionTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). All three ACs revised after the GAPS verdict (pass conditions added).*

- **AC-1**: `StealthGame.Crowd.RunBouts`
  - Given: logged run bouts with their draws in a fixed-seed scene
  - When: bouts and destinations are analysed
  - Then: each bout 2-6 s, starting and ending at leg boundaries; each destination equals the R13 weighted result for its u and is never a court; the run ends by SCL braking
  - Edge cases: bout begun near a leg end
- **AC-2**: `StealthGame.Crowd.TraversalHerrings`
  - Given: a scene with public coping of heights below and above 1.6 m, 600 simulated minutes
  - When: vaults, climbs and `CanBegin` calls are logged
  - Then: vault and climb rates in the 99 % Poisson interval of F2; no climb below 1.6 m; every `Traverse` preceded by a dry run
  - Edge cases: coping exactly 1.6 m; dry-run rejection triggers the 10 s no-retry
- **AC-3**: `StealthGame.Crowd.ReversalBehaviour`
  - Given: 600 simulated minutes
  - When: reversals and heading changes are logged
  - Then: rate in the 99 % Poisson interval of `60 / CrowdReversalIntervalSec`; stop at least `StopMinSec`; heading changes above `ReversalAngleDeg` occur only in reversals
  - Edge cases: reversal begun during a pace change

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdHerringMotionTest.cpp` (headless accelerated, fixed seeds) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-009-follower-shaping-pace-and-stops.md`, `production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md`
