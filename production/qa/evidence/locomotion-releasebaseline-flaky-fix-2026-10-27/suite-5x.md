# StealthGame.Locomotion.PIE.LocomotionPie.ReleaseBaselineStopUnderLag flake fix (2026-10-27)

Symptom: failed 2 of 5 full-suite runs in the ASN 005-011 batch (2026-10-27), passed on re-run and in isolation.

## Nondeterminism found

1. **Server stop timing (the measured one).** The server copy's stop was timed on server frame time, so its start and end
   each moved with the server frame a batch of the owner's ServerMoves landed in. Six isolated runs before the fix
   (same binary): owner 0.758 s every run; server 0.733 / 0.742 x4 / 0.750 s, server path 51.92 cm. Bound: owner vs server
   within 1/60 + 1/60 + 0.001 = 0.034 s; 0.733 s is 0.025 s from the owner, so a loaded full-suite run with longer server
   frames pushes it out.
2. **Layout.** The round seed and spawn slot come from OS entropy (12 different cities in 12 runs), and the walk was an
   open-loop 0.5 deg/frame circle from the spawn, started during Preparing, through adobe pieces and past the host's pawn.

## Fix (test only; product unchanged, bounds unchanged)

- The server copy is sampled once per processed ServerMove and timed on that move's client timestamp
  (`FNetworkPredictionData_Server_Character::CurrentClientTimeStamp`): server frame timing cannot reach the comparison.
- The walk waits for Active, starts on a clear flat circle found on the server (new shared helper
  `Source/StealthTestHarness/Private/Tests/PIE/StealthPIEOpenCircle.h`, extracted from the WalkSlowerModifier fix b764c8d,
  which now uses it), is steered closed-loop, warms up, waits for rest and agreement, snaps the resting owner onto the server
  copy (the teleport left 0.2-0.9 cm under the CMC correction threshold, which then reached the rest-gap check), and releases
  only after 30 frames at a steady 140 cm/s (asserted precondition).

## Results after the fix

Isolated (`run-tests.ps1 StealthGame.Locomotion.PIE`, 6 runs, 6 different cities): every run
`owner stop 0.758 s over 53.08 cm; server stop 0.758 s (move clock) over 53.08 cm; rest positions 0.00 cm apart`,
start agreement after the snap 0.000 cm.

Full-suite runs (`UnrealEditor-Cmd ... Automation RunTests ^StealthGame.`) containing the fix:

| # | When | Suite | ReleaseBaselineStopUnderLag | Measurement |
|---|------|-------|-----------------------------|-------------|
| 1 | 2026-10-27 10:25-10:52 | 358/361 (failures unrelated: SchemaComplete / SchemaGenDeterministic read a schema table another agent was editing on disk; KillAtArmsLength placement 231.9 cm, fixed separately) | Success | 0.758 / 0.758 s, 53.08 / 53.08 cm, 0.00 cm |
| 2 | 2026-10-27 ~13:09 (ASN 024/017 gate) | 387/387 | Success | 0.758 / 0.758 s, 53.08 / 53.08 cm, 0.00 cm |
