# Crowd & NPC Behaviour batch evidence (stories cnb-001..018), 2026-10-26

## Integration (D53)

The production crowd replaces the prototype crowd in `L_Prototype`: `StealthCompositionRoot::CreateAiController` spawns
only `AStealthCrowdController`; `AStealthGameMode::SpawnNpc` takes its start transforms from
`UCrowdDirector::RequestStartPlacement` at round start (fallback: the prototype crowd point). One path is active; the
prototype `StealthNpcController` is no longer spawned (the class itself is retired by story 039). NPCs move only through
the shared `Move`/`Look` command (`COMMANDPATH`: 216,320 Move/Look calls through the sinks in 45 s, 0 discrete commands,
0 movement backstop faults, 0 path-following components).

## Run (run-and-observe, `.claude/docs/run-and-observe.md`)

- Editor launched with `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients, floating windows) via the Unreal MCP
  (`EditorAppToolset.StartPIE`); `stat unit` through `StealthPlayToolset.RunPlayConsoleCommand`; windows captured with
  `tools/dev/capture-window.ps1`.
- Server log: `Round phase Lobby -> Preparing -> Active`, `Round started (seed 960854786, 40 NPCs)`, both crowd trees
  compiled, no `LogStealthCrowd` warning or error and no `LogStealthFault` line. The NPC count is the project's
  `UStealthSettings::NpcCount` default, 40 (not 50: changing it needs a Config edit, which this batch avoided; the
  50-NPC behaviour is covered headless by every `StealthGame.Crowd.*` scene test at 50 NPCs).
- **Frame time vs ADR-0022 (observation, not budget evidence).** `stat unit` with three PIE worlds and the editor in one
  process, windows in the background: Frame 331.9 ms, Game 46.6 ms, GPU 5.9 ms. ADR-0022 section 6 forbids citing
  PIE/editor numbers as budget evidence (cooked Development standalone on the 3060-class preset only); the Crowd AI + nav
  line (1.5 ms p99, mean <= 0.8 ms, of the 12.0 ms listen-host GT) is measured by the 60-character cost spike, story
  cnb-032. Headless, the crowd decision cadence holds: 4.3-4.5 tree ticks/s per pawn, <= 2 path starts and at most the
  decision bound per frame (spike report `production/qa/spikes/2026-10-26_crowd-engine-verifications.md`).

## Screenshots (checked: not black, looked at)

| File | Shows |
|---|---|
| 01-host-crowd-active-stat-unit.png | Host (Jaguar) at round start, Active; crowd NPCs in the distance |
| 02-client1-crowd.png | Client 1 (Eagle) with `stat unit` (process-wide PIE timings) |
| 03-client2-crowd.png | Client 2 (Jaguar), NPCs walking between the huts |
| 04-host-crowd-later.png | Host 30 s later: NPC walkers moved (right of frame) |

## Tests

`full-suite.txt`: 275/275 `StealthGame.*` Success headless (`cycle.ps1 -Tests -NoLaunch`); 207 before the batch, +68 Crowd
tests (65 headless, 3 PIE). `crowd-test-observations.txt`: the scene tests' measured lines (rates, shares, intervals).

## Code review

Self-review against `.claude/skills/code-review/SKILL.md` while fixing the first full runs; findings fixed in this batch:
the tell clocks compensated for tells not built yet (G6 37 % over rate); the G5 look share was biased high; the stall
regulator reacted P_th late (AC-CNB-12 at 85.7 %); the watchdog lead was shorter than a leg's start; unrequested off-grid
StateTree ticks and unbounded wakes broke the per-frame decision bound; the crowd lint's transitive include walk took
minutes; a headless test world that began play was destroyed without EndPlay (crashed a later map load). Decisions are in
`production/session-logs/decision-log.md` (2026-10-26).
