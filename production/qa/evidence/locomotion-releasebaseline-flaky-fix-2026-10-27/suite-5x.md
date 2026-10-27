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
| 3 | 2026-10-27 ~13:52 (NKP 001-004 gate) | 387/387 | Success | 0.758 / 0.758 s, 53.08 / 53.08 cm, 0.00 cm (KillAtArmsLength with the open-disc placement: 230.0 cm) |
| 4 | 2026-10-27 (ASN 022/026 gate, `story-022-.../cycle-tests-first-run.txt`) | 391/393 (the two new 022 PIE tests, fixed in that story) | Success | - |
| 5 | 2026-10-27 (NKP 005-008 gate, `npc-kill-penalty-informants-005-008-2026-10-27/full-suite.txt`) | 408/409 (027's then-new PIE test F, fixed in 027) | Success | - |
| 6 | 2026-10-27 (ASN 027 gate, `assassination-defence-reversal-027-2026-10-27/full-suite.txt`) | 407/409 (schema tests reading another agent's uncommitted schema table) | Success | - |
| 7 | 2026-10-27 (NKP 009-012 gate, `npc-kill-penalty-informants-009-012-2026-10-27/full-suite.txt`) | 421/421 | Success | - |

**Verdict:** ReleaseBaselineStopUnderLag passed 7 consecutive full-suite runs after the fix (and 12 of 12 isolated
runs), each on a different random city; every measurement captured (isolated runs and full-suite runs 1-3) was identical (0.758 / 0.758 s, 53.08 / 53.08 cm,
0.00 cm). Before the fix it failed 2 of 5 full-suite runs. The other failures in the table belong to tests being written
by concurrent stories in the same tree, each fixed in its own story. Two consecutive fully green runs: #2 and #3
(387/387), and #7 (421/421). A dedicated 5x run on a quiet tree was started (10:25) but stopped after run 1 because
concurrent agents needed the build; runs #2-#7 replace it.
