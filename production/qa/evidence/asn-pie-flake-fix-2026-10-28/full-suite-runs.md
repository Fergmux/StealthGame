# Five consecutive full-suite runs, zero flakes (2026-10-28)

Command: `tools/dev/run-tests.ps1 -Filter "StealthGame."` (the full suite, 517 tests), five sequential runs of the batch's final
binaries (after the section 4 and 5 fixes of `evidence.md`; `cycle.ps1 -NoLaunch` -> `Result: Succeeded` before run 1). Raw
per-run results: `full-suite-run1.txt` .. `full-suite-run5.txt` (every test's Success line, the NIL-matrix count and the
diagnostic lines of the fixed tests).

| Run | Time | Result | NIL-matrix ensure lines | [StrikeWarp] guard lines | KillAtArmsLength (2) | StrikeClipParity | KillPlateTellsOneThing | ContextSwapOnStateAndPossession | OneBodySequence / NoObserverCue / PossessionThroughFall |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 17:49-18:36 | 517 / 517, 0 fail | 0 | 1 (the unit test) | pass | pass | pass (S pose 2 at the press) | pass | pass |
| 2 | 18:36-19:24 | 517 / 517, 0 fail | 0 | 1 (the unit test) | pass | pass | pass (S pose 2) | pass | pass |
| 3 | 19:24-20:14 | 517 / 517, 0 fail | 0 | 1 (the unit test) | pass | pass | pass (S pose 2) | pass | pass |
| 4 | 20:14-21:03 | 517 / 517, 0 fail | 0 | 1 (the unit test) | pass | pass | pass (S pose 2) | pass | pass |
| 5 | 21:03-21:53 | 517 / 517, 0 fail | 0 | 3 (the unit test + 2 in PIE, below) | pass | pass | pass (S pose 2) | pass | pass |

Total: 2,585 test executions, 0 failures, 0 engine NIL-matrix ensures.

**The in-PIE trigger, caught by the guard (run 5).** `Assassination.PIE.RoleChangeGate.OwnRoleDispatchFiresOnceAtDeathOnRemoteClientAndListenHost`,
on the striker's autonomous proxy and on the server, the same step: window [0, 0.9], previous 0.8627 -> current 0.8815 (a normal
forward step at rate 2.25), step translation 0.019 cm, remaining translation 0 - the strike clip's root motion is spent before
the window's end, so the engine skew would have built its matrix from a zero vector (the ensure of `evidence.md` section 1). The
guard passed the 0.019 cm step through unwarped on both machines and the test passed. This is the input the two diagnostic group
runs did not catch: it needs the montage to be sampled inside the last few hundredths of the window, which depends on frame timing.

Earlier gate attempts of this batch (not counted): attempt 1 run 1 517/517, run 2 failed KillPlateTellsOneThing (section 4);
attempt 2 runs 1-3 517/517 each, run 4 failed ContextSwapOnStateAndPossession (section 5). Neither failure was any of the five
tests this item set out to fix, and neither showed a NIL-matrix line.
