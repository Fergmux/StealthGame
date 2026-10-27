# Spike: 60-character server cost (story cnb-032, ADR-0016 conditional gate, GDD AC-CNB-26 / F5)

- **Date**: 2026-10-27
- **Story**: `production/epics/crowd-npc-behaviour/story-032-spike-60-character-server-cost.md`
- **Verdict on the 1.5 ms rule**: **NOT ASSESSED** — no 3060-class reference host is available. This machine is an
  RTX 5090 with an AMD Ryzen 9 7950X3D (16 cores / 32 threads, 4.2 GHz base, 63 GB RAM): **not reference hardware;
  every number below is informational only** and is never PASS evidence for the reference budget.
- **Measured in**: separate game processes, never the editor and never in-process PIE (ADR-0022 section 6): one listen
  host plus 10 bot clients, each `UnrealEditor.exe StealthGame.uproject -game -nullrhi` (Development editor binaries
  in game mode, uncooked), launched by `tools/ci/mp/run-mp.ps1 -Scenario CrowdCost`. A cooked Development (or Test)
  standalone, which ADR-0022 requires for budget evidence, was not packaged on this machine (deviation, see below).
- **Evidence**: `production/qa/evidence/2026-10-27_crowd-60-character-capture/` (host and result records, per-second
  samples and launch lines of each run).

## Method

- Scene: `L_Prototype` (graybox), `NpcCount` 50, 10 bot clients + the listen host's own pawn, round started by the
  scenario once every client had joined (`bAutoStartRound=False`), measurement from 15 s after Active (the Active-entry
  hitch excluded) for 300 s, frame rate locked at 60 fps (`-StealthHarnessMaxFps=60`), a strike every 20 s through the
  bot player path (`DebugBotAction strike_npc`; the prototype strike announces `OnActionStarted(Attack)`, so R8 fan-out
  and reactions run, and the prototype replaces the killed NPC).
- Cost: the crowd cost meter (`Source/StealthGameplay/Features/Crowd/Public/Server/CrowdCostMeter.h`): the six F5 scopes
  `Crowd.Director`, `Crowd.StateTree`, `Crowd.Follower`, `Crowd.Perception`, `Crowd.NavQuery`, `Crowd.Dispatch` are
  Insights CPU scopes and, outside Shipping, cycle accumulators; top-level scopes sum to the frame's crowd game-thread
  time (nested scopes only to their breakdown). The host's harness reads and resets it every frame. An Insights trace
  was not recorded (the meter gives the same sums per frame without a trace session; deviation, see below).
- The catalogue under test is the full one built so far (stories 001-031: the herrings, R8 and the bound-pawn driver are
  in), more than the thin slice the story asks for. Vault, climb, decoy and bush clocks do not run in L_Prototype (no
  LSC / Objectives / Concealment sites yet), and there is no ambient schedule (no sockets).

## Results

| Run | Pawns | Crowd mean / p99 / worst (ms per frame) | Frame mean / p99 (ms) | Strikes |
|---|---|---|---|---|
| 60-pawn, 60 fps lock (crowdcost-60b) | 59 (48 NPC controllers at the end: 1 start placement collided, struck NPCs being replaced; 11 players) | **0.371 / 1.047 / 6.59** | 18.3 / 27.6 | 15 |
| 60-pawn, 60 fps lock, first run (crowdcost-60) | 51 (the 10 bot clients joined after the round auto-started and got no pawn: rerun above) | 0.366 / 0.506 / 8.35 | 16.9 / 21.1 | 15 |
| 70-pawn, 30 fps lock (crowdcost-70; advisory) | 70 (59 NPCs + 11 players) | 0.467 / 0.599 / 0.97 | 33.4 / 33.4 | 6 |

Budget (provisional F5 / AC-CNB-26): summed crowd game-thread time <= 1.5 ms at p99 and <= 0.8 ms mean per frame on the
listen-server host.

Per-scope means (ms per frame), 60-pawn run: Director 0.083, StateTree 0.066, Follower 0.221 (agent tick: due list,
herrings, follower, avoidance, gaze and the virtual input layer), Perception 0.072 (hash, salient and strike fan-out,
rays; nested in the Director and the agents), NavQuery 0.003 (path starts and callbacks), Dispatch 0.062 (Move/Look
through the sinks; nested in Follower). Worst single frames: 4.8-6.5 ms in single scopes, coinciding with the strike /
NPC-replacement frames and with 11 game processes competing for one CPU.

70-pawn run at 30 fps: Director 0.101, StateTree 0.101, Follower 0.265, Perception 0.087, NavQuery 0.003, Dispatch 0.084.

## Against F5

F5 predicts about 44 ms of crowd work per second at 60 pawns (0.74 ms per frame at 60 fps). Measured: 0.371 ms per frame
x 54.5 fps = about 20 ms per second at 59 pawns, about 0.46 of the model on this CPU. The decision term (StateTree:
about 4.0 ms/s for 48-50 trees at 4 Hz, about 20 us per decision against F5's 25 us) and the steering term (Follower +
Dispatch: about 13-16 ms/s against F5's 20.2 ms/s) both sit under the F5 constants. No F5 constant is revised from a
non-reference host; revise them from the reference capture.

## Cut-order steps the numbers predict

On this host none: the mean is under half the 0.8 ms limit and the p99 (1.05 ms) is under 1.5 ms even with 10 client
processes sharing the CPU. Scaling to a reference-class CPU (assume a per-core speed about half of a 7950X3D), the mean
would be about 0.75 ms (at the limit) and the p99 about 2.1 ms (over): the first steps of the cut order (steering 30 ->
20 Hz, then the neighbour cap) would be the ones to engage. This is a projection, not a measurement.

## Not measured / deviations

- No 3060-class reference host: the gate stays open (NOT ASSESSED). The story's escalation rule (a FAIL to the
  technical-director and creative-director before any `NpcCount` change) is not triggered.
- Uncooked `-game` processes instead of a cooked Development/Test standalone; the 10 bot clients share the host's CPU.
- No Unreal Insights trace: the in-process meter of the same six scopes stands in (the scopes are Insights scopes too).
- The server movement simulation of AI-moved pawns (SCL budget) is not separated: it is inside the reported frame time.
- The virtual-input layer's own per-tick cost is inside `Crowd.Follower` (not split out); `EvaluateViewEnvelope`
  (Camera) is not built.
- Story 033 (cost-cut order) and story 038 (the exit performance pass) stay blocked: the reference capture is missing and
  story 034 (parity per step) is blocked.
