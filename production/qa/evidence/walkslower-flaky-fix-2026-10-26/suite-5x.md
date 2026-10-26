# StealthGame.Input.PIE.WalkSlowerModifier flake fix: 5 consecutive full-suite runs (2026-10-26)

Command per run: UnrealEditor-Cmd StealthGame.uproject -ExecCmds="Automation RunTests ^StealthGame.; Quit" -unattended -nullrhi (one build, 5 runs back to back)

```
start 2026-10-26T14:20:50
run 1 2026-10-26T14:35:02: total=207 pass=207 walkslower= fail=[]
run 2 2026-10-26T14:49:58: total=207 pass=207 walkslower= fail=[]
run 3 2026-10-26T15:04:34: total=207 pass=207 walkslower= fail=[]
run 4 2026-10-26T15:19:18: total=207 pass=207 walkslower= fail=[]
run 5 2026-10-26T15:34:24: total=207 pass=207 walkslower= fail=[]
end 2026-10-26T15:34:24
```

WalkSlowerModifier per run (result line and warm-up agreement):
```
run 1:
Display: Test Completed. Result={Success} Name={KeyboardModifiersScaleTheServerReceivedMove} Path={StealthGame.Input.PIE.WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove}
WALKSLOWER warm-up done: owner/server distance 0.80 cm, owner speed 0.00, server speed 0.00
WALKSLOWER neither (reference): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER both: server-received magnitude 0.800 (expected 0.80), target pace 112.0 cm/s (ratio 0.800), measured speed 112.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER neither: server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER informant: neither (settle): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 
run 2:
Display: Test Completed. Result={Success} Name={KeyboardModifiersScaleTheServerReceivedMove} Path={StealthGame.Input.PIE.WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove}
WALKSLOWER warm-up done: owner/server distance 0.84 cm, owner speed 0.00, server speed 0.00
WALKSLOWER neither (reference): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER both: server-received magnitude 0.800 (expected 0.80), target pace 112.0 cm/s (ratio 0.800), measured speed 112.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER neither: server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER informant: neither (settle): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 
run 3:
Display: Test Completed. Result={Success} Name={KeyboardModifiersScaleTheServerReceivedMove} Path={StealthGame.Input.PIE.WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove}
WALKSLOWER warm-up done: owner/server distance 0.84 cm, owner speed 0.00, server speed 0.00
WALKSLOWER neither (reference): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER both: server-received magnitude 0.800 (expected 0.80), target pace 112.0 cm/s (ratio 0.800), measured speed 112.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER neither: server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER informant: neither (settle): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 
run 4:
Display: Test Completed. Result={Success} Name={KeyboardModifiersScaleTheServerReceivedMove} Path={StealthGame.Input.PIE.WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove}
WALKSLOWER warm-up done: owner/server distance 0.82 cm, owner speed 0.00, server speed 0.00
WALKSLOWER neither (reference): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER both: server-received magnitude 0.800 (expected 0.80), target pace 112.0 cm/s (ratio 0.800), measured speed 112.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER neither: server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER informant: neither (settle): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 
run 5:
Display: Test Completed. Result={Success} Name={KeyboardModifiersScaleTheServerReceivedMove} Path={StealthGame.Input.PIE.WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove}
WALKSLOWER warm-up done: owner/server distance 0.63 cm, owner speed 0.00, server speed 0.00
WALKSLOWER neither (reference): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER both: server-received magnitude 0.800 (expected 0.80), target pace 112.0 cm/s (ratio 0.800), measured speed 112.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER neither: server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 cm/s)
WALKSLOWER informant: neither (settle): server-received magnitude 1.000 (expected 1.00), target pace 140.0 cm/s (ratio 1.000), measured speed 140.0 cm/s, max step over limit 0.00 (frame 0, 0.0 -> 0.0 
```

## Root cause

The test's round seed and spawn slot come from the session's OS entropy (RMF/session batch), so every run lands the two
pawns at a different spot of a different city. The test drove an open-loop circle (a fixed yaw step per frame) from
wherever the pawn spawned; on some layouts the circle ran into a wall or prop, the speed dropped to 0 and the collision
step exceeded the step limit (failure seen 12:35: "informant: Left Ctrl: no speed step above ...", step 8.69). The test
depended on the spawn layout, not on frame timing.

## Fix (test only, `Source/StealthTestHarness/Private/Tests/Input/WalkSlowerModifierTest.cpp`)

- `FindOpenCircle`: a box overlap of the circle radius plus margin and floor traces round the rim, searched in rings out
  to 60 m; the server and owning-client copies are teleported onto it.
- Closed-loop orbit steering (`OrbitYawDeg`, rate-limited by `SteerToward`, 0.6 deg/frame) and `AlignControlToFacing()`
  at each phase start (no idle pivot, so no client/server divergence corrections).
- An unchecked warm-up walk, then a stop until both copies are at rest (< 0.1 cm/s) and agree within 10 cm for 60
  frames (give-up 600 frames, logged "WALKSLOWER warm-up done").
- Step diagnostics (frame, from, to) and a new assertion that the measured speed equals the target pace (+-3 cm/s).
