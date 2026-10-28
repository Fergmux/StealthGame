# StealthGame.Input.PIE.WalkSlowerModifier load flake: second fix (2026-10-27)

Story: `production/epics/input-interaction-framework/story-018-walk-slower-two-client.md` (follow-up to the 2026-10-26 fix
b764c8d, `../walkslower-flaky-fix-2026-10-26/suite-5x.md`). Pattern: the ReleaseBaselineStopUnderLag fix 8015f9f.

## The failure

NKP 013-015 first gate run (2026-10-27 20:25, `Saved/Logs/StealthGame-backup-2026.10.27-20.33.56.log`):
`Run + Left Ctrl: measured speed 406.3 cm/s at the target pace 450.0`; the next full-suite run passed.

## Root cause: the frame rate reached the measurement

The 2026-10-26 fix removed the layout dependence but kept two frame-counted quantities:

1. **Phase length and window in frames.** Each phase was 180 frames sampled over the last 60. The PIE frame rate is not
   fixed: the passing run of 17:42 ran at about 120 fps (each "3 s" phase finished after 1490 ms), the failing run of 20:25 at
   about 98 fps (1826 ms). The phases were 1.5-1.8 s, never 3 s, and the window moved with the frame rate.
2. **Steering cap per frame.** The orbit steering moved the control yaw at most 0.6 deg per frame: 36 deg/s at 60 fps, 59 at
   98 fps, 72 at 120 fps. The run's 300 cm orbit at 450 cm/s needs 86 deg/s, so in the run phase the runner could not hold the
   circle and spiralled out past the cleared disc (radius 300 + 120 cm margin), into whatever the city had there: the scrape
   that cut the averaged speed to 406.3. Whether it hit something depended on the frame rate (how wide the spiral) and the
   layout beyond the disc.

Watched fail (`isolated-runs.txt`, run `watchfail60`): the new build with the run steering rate held at the old 36 deg/s
(the per-frame cap at 60 fps) fails the new guard: `Run + Left Ctrl: the walker stayed on the open circle (max radial error
594.1 cm)`. The old code at 60 fps (`old60`) passed only because that run's layout was open beyond the disc.

## The fix (`Source/StealthTestHarness/Private/Tests/Input/WalkSlowerModifierTest.cpp`)

- Each phase lasts 3 s of the owner's move clock **as the server processes it** (`FNetworkPredictionData_Server_Character::
  CurrentClientTimeStamp`, against the owner's `FNetworkPredictionData_Client_Character::CurrentTimeStamp` at the phase
  start); the server copy is sampled once per processed move over the last 1 s. Neither frame rate nor server frame timing
  reaches the window. Shared helpers `ServerMoveTime` / `OwnerMoveTime` in `Tests/PIE/StealthPIEOpenCircle.h` (LocomotionPie
  now uses the shared one).
- The steering cap is a rate per second of the owner's frame time: 36 deg/s walking, 120 deg/s running (and in the settle
  phase that brakes from the run), inside WalkTurnRateDegPerSec 60 / RunTurnRateDegPerSec 180.
- The no-step transition window is 0.667 s of owner time (was 40 frames).
- Warm-up walk in owner time (at least 3 s, and until the walker is within 5 cm of the orbit; it can start from the spawn
  when a teleport correction lands), then the resting owner is snapped onto the server copy (as 8015f9f).
- New guards: every checked phase asserts the owner stayed within (margin - capsule radius) = 90 cm of the orbit, and that the
  steady window sampled at least 20 processed moves.

## Isolated runs after the fix (`isolated-runs.txt`)

| Run | Frame cap | Result | Moves per window | Max radial error, checked phases | Run phase |
|---|---|---|---|---|---|
| fixb0 (x2) | none (~120 fps) | Success, Success | 61 | 7.4 cm (reference start), else <= 0.6 | 450.0 cm/s |
| fixb60 | 60 fps | Success | 61 | 7.4 cm, else <= 1.2 | 450.0 cm/s |
| fixb30 | 30 fps (heavy load stand-in) | Success | 31 | 7.6 cm, else <= 2.5 | 450.0 cm/s |

Every phase now lasts 3.0 s of wall time at every frame cap (`Finished phase ... after 3010-3033 milliseconds`), and the
measured values are identical across frame rates (140.0 / 112.0 / 126.0 / 112.0 / 140.0 / 450.0 cm/s, magnitudes 1.000 /
0.800 / 0.900 / 0.800 / 1.000 / 1.000).

## Full-suite runs

| # | When | Suite | WalkSlowerModifier | ReleaseBaselineStopUnderLag |
|---|---|---|---|---|
| 1 | 2026-10-27 22:03-22:44 (this fix's gate, `full-suite-1.txt`, measurements `full-suite-1-measurements.txt`) | 429/433 | Success (450.0 cm/s run, max radial error 0.6 cm) | Success (0.758 / 0.758 s, 53.08 / 53.08 cm) |
| 2-6 | 2026-10-28, NKP 016-022 gate runs 1-5 (`../npc-kill-penalty-informants-016-022-2026-10-27/full-suite-run1..5.txt`) | 446-448/449 (failures belong to that batch's tests and the load-sensitive Assassination PIE timing, see its evidence) | Success x5 | Success x5 |
| 7 | 2026-10-28, NKP 016-022 final gate (`../npc-kill-penalty-informants-016-022-2026-10-27/full-suite.txt`) | 449/449 | Success | Success |

**Verdict:** WalkSlowerModifier passed 7 of 7 complete full-suite runs after the fix (and 5 of 5 isolated runs at three frame
caps); its measured values were identical in every run. NKP run 6 was stopped at test 102, before it.

Run 1's four failures are not this change's: `StealthGame.Tuning.SchemaComplete`, `StealthGame.Tuning.SchemaGenDeterministic`
and `StealthGame.Build.LintSeededControls` read source files from disk, and a concurrent agent (NKP 016-022) was editing the
tuning schema and Penalty sources in the same tree during the run; `StealthGame.Assassination.PIE.StrikeClipParity` ("the
five strikes share one variant, horizontal WarpPoint and contact time") does not include anything this change touched (the
only users of `StealthPIEOpenCircle.h` are WalkSlowerModifier, LocomotionPie, KillAtArmsLength and PrototypeScenariosACF, all
Success). The NKP 016-022 gate runs that follow on this commit are recorded below as the repeated-run proof.

