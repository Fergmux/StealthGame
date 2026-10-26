# SCL movement batch evidence (stories 003, 007, 009-018, 046, 047), 2026-10-26

Shared evidence folder for the Shared Character & Locomotion movement batch; each story's Completion Notes point here.

## Run (run-and-observe)

- Editor launched with `tools/dev/cycle.ps1`; 3-player PIE on L_Prototype (listen server + 2 clients, floating windows)
  through the Unreal MCP (`EditorAppToolset.StartPIE`, `LevelEditorPlaySettings.PlayNumberOfClients` 3); the walker is
  driven owner-side with the dev command `StealthBot 1 drive <forward> <run 0|1> [<yaw>]` (added to
  AStealthPlayerController for this observation: it holds a Move intent on the client's own pawn exactly as a held stick
  does, so it travels through owner prediction and ServerMove); windows captured with `tools/dev/capture-window.ps1`.
- Server log: 0 `LogStealthFault` lines in the session (no movement backstop hit: the crowd walks only through Move).

## Screenshots (all checked: not black)

| File | Shows |
|---|---|
| 01-client1-walking-owner.png | Client 1 walking (owner view) toward the crowd at 140 cm/s: walking stride; crowd NPCs walking around the court |
| 02-client2-observes-client1-walking.png | Client 2's view, a walking pawn in the distance |
| 03-client1-running-owner.png | Client 1 running (Run gait, 450 cm/s): running stride; HUD "Defence: unavailable while running or climbing" (the prototype rule now reads the production IsRunning) |
| 04-client1-stopping-0.3s.png | 0.3 s after release: still decelerating (baseline stop, no sudden stop) |
| 05-client1-stopped-idle.png | At rest: idle pose, Defence READY again (IsRunning false) |
| 07/08-host-crowd-walking-*.png | Host view: an NPC walking across the street (two frames 0.7 s apart) |
| 09/10-client1-look-120-*.png | Control yaw set 120 degrees off the body while idle; the close camera of the sample rig clips into the body, so the turn-to-look is not legible in these stills (inconclusive; the headless test StealthGame.Locomotion.IdleTurnInPlace carries the AC) |
| 00, 06 | Client 2 framing shots (own pawn only; not used for a criterion) |

What a still cannot show: timing and feel (stop duration, turn radius, pace changes). Those are measured by the tests
below; the feel half is for the first locomotion playtest.

## Tests

- `full-suite.txt`: 171/171 `StealthGame.*` Success (headless, `cycle.ps1 -Tests`), was 153 before the batch (the
  pre-batch baseline run had one flaky failure, WalkSlowerModifier, caused by crowd collisions; that test now runs with
  NpcCount 0 and asserts the model's target pace).
- `test-measurements.txt`: the measured values every new test logs (stop durations and distances, turn radii, pace,
  IsRunning timing, displacement buckets, the lag test, the NPC controller route).

Key numbers: stop from 140 = 0.75 s / 52.5 cm, from 112 = 0.60 s / 33.6 cm, floor 0.50 s below 94 cm/s; walk turn
radius 133.7 cm (107.0 at 112), run 143.3 cm; reversal 121/150 deg stop-pivot-restart at 120 deg/s; IsRunning falls
0.300 s after Run is released at 450; under PktLag 100 ms the owner stops in 0.746 s over 50.2 cm and the server in
0.751 s over 51.7 cm, rest positions 0.13 cm apart.

## Code review

Focused review (lead-programmer, sonnet): 23 findings. Fixed: move combining now restores the model latches and the
pending move's start state (CombineWith; CanCombineWith compares every latch), Run and the action pose are restored
after a client replay (ClientUpdatePositionAfterServerUpdate) and the pose rides the saved move, an uncontrolled pawn
never turns toward a default look target, displacement buckets are authority-only and the history window is clamped, a
skipped sub-minimum step keeps the facing, the crowd controller has a progress watchdog and reschedules when it loses its
pawn, the rejected-bind path drops the Roster handle bookkeeping, the bind-fail test asserts the pawn never initialised
or began play, the PIE stop trace is seeded before the release, IsRunning on proxies no longer builds a struct per call,
and the production NPC controller has its own test (StealthGame.Locomotion.NpcControllerMoveOnly). Not changed (noted):
NPC idle turn-to-look stays unreachable for the prototype crowd (its control rotation follows its body; the Crowd gaze
owns it); I-SCL-15 reads the engine config CDO at preset load (by design: it is the boot check); the stick dead zone is
applied only to the gamepad path, where the walk-slower scale is the identity.

`stamp.txt`: content hashes of the main changed sources at capture time.
