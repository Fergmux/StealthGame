# Evidence: Concealment (Bushes) stories 027-030, 032-034, 2026-10-29

Stories: `production/epics/concealment-bushes/story-027`, `-028`, `-029`, `-030`, `-032`, `-033`, `-034`. One code commit
with Camera stories cs-001/002/003/006/010/012 (decision D41: one gate-tested tree). Story 031 stays Blocked (10 logged
playtest matches); 035 stays Blocked (028, 029 and 034 are In Review).

## Build and gate

`tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`; full `^StealthGame.` suite: **594 tests, 593 passed on the first full run; the one failure was ExitUncullBeforeLeafPlane's snap check, which asserted on every drawn copy (wider than AC1 (b), which speaks of the late actor): narrowed to late actors, all other snaps reported, and the test re-run alone at the full 1,000 / 300 counts: PASS (`exituncull-rerun.txt`); every other test of the tree passed in the full run** (`full-suite.txt`).
No Build.cs / Target.cs / uproject change in this commit. No `Config/` change (no SecurityToken line). The sample camera
assets and PC_Sandbox are unmodified (`git status` clean under `Content/`).

## What was built

| Story | Built | Tests |
|---|---|---|
| 027 | the 1,000-exit / 300-emerge measurement under StealthLeak1 (20 unbound hiders, cooldown lifted by the existing seam), the attack-from-bush timing PIE | PIE.ExitUncullBeforeLeafPlane, PIE.AttackBreaksConcealment (2 methods) |
| 028 | the frame-stepped swap check per machine with its negative control | PIE.EnterSwapHidesBodyPerViewer |
| 029 | `FBushRustleRing` / `BushRustle` (8 slots, onsets, face point, eye-masked tag), `UBushRuntimeSubsystem::PushRustlePulse` (ring, optional MPC, shared SFX pool spatialised at the face, seeked late writes), the component's arm-and-fire from any pawn's hide pose | RustlePulseSlot, PIE.RustleParity |
| 030 | `ConcealmentLog` (the one row builder and decoder), the BushStrike row (Assassination's resolver reports rule 2's counts through `IBushSpotQuery::OnBushStrike`) and the Emerge kill flag (`OnStrikeKilled`), `ConcealmentMetrics` (F1, m0, the CON-R12 cap, F2, per-player time concealed and r_P, multi-occupant share, ambush share) | ConcealmentLog, MetricCalculators |
| 032 | `URosterStore::OnPawnReleasedToCrowd`, `FConcealmentCore::OnPawnReleased` (flags cleared, cull re-pushed for former allies), rejoin flag restore in `OnParticipantRebound`; fix: `UStealthMatchAdapters::GetBoundPawn` reads the Roster binding first; test seam `UStealthCompositionRoot::DropConnectionForTest` | TransitionSurvivesHandover, PIE.DisconnectRelease |
| 033 | latch fix (death write without bDiedConcealed cancels the pending swap), StartWarp `bOwnerPredicts` (no correction freeze after an emerge) | PIE.BlindStrikeParity, PIE.KillWhileConcealedTiming, PIE.NoImmunityWhileEntering; BodyHiddenLatchTable rows |
| 034 | the production-hide capture scenario `Stealth.R5Matrix` and `tools/analysis/r5_matrix.py` | (capture, not a gate test) - `con-r5-results.md` |

Test-rig fixes made on the way (each watched failing first): unbound test hiders now simulate movement
(`bRunPhysicsWithNoController`; without it the warp never ran and 274 root-motion sources piled up), the player's hide in
RustleParity goes through the owner's real press (the server's correction window assumes the owner predicts), and
PerViewerVisibility's CopyNear skips the local viewer's own pawn (an attacker warped to the planter edge was inside its
radius).

## Measurements

CON07 (story 027 AC1, StealthLeak1, re-run at full counts, `exituncull-rerun.txt`): 1,009 exits and 300 bush-emerge strikes
from 20 unbound hiders; every transition's copy was closed by the cull first (1,009 / 300 re-opens per client). (a) 0 draws
before the crossing on any machine. (b) late actors: 0 % of exits and 0 % of emerges on both clients; no late actor snapped.
(c) T_uncull p99: client 1 0.150 s (exit) / 0.236 s (emerge), client 2 0.074 / 0.089 s (bounds 0.400 / 0.300); max 0.306 s.
Finding (reported, not an AC failure): 8 of 2,618 drawn emerge copies moved 61-144 cm in one frame 0.37-0.68 s after the
strike started (after the crossing, actors created in time): the strike's motion warp toward a whiff point 5 m away on a
simulated proxy. The first full run had 5 such frames (`full-suite.txt`).

CON13 (027 AC2): the opponent first drew the attacker at t0 + 0.352 s (due 0.350), the victim's Death_Fall at the contact
(10.885 vs 10.880; frame 0.020); control: a braced victim reversed (H died, V countered).

CON01a (028): first hidden frame 8.5797 / 8.5827 against the key 8.5768 (frame 0.0197) on the opponent / ally; the key + 5
frames control fails as it must.

CON18 (029): 24 transitions, one pulse each per machine; references enter {onset +0.400, side 0, face +100 cm}, exit {+0.350,
side 0}, emerge {+0.250, side 0}, amplitude 10 cm, the shared SFX; 0 pulses in the three 2.5 s occupied windows.

CON26 (033 AC2, StealthLeak1): NPC and rival: first opponent channel 11.278 / 23.303 against the death write due 11.231 /
23.295 (after it, within a tick); re-opened with bDiedConcealed; never drawn; fields equal {pose 0, bDiedConcealed 1,
DeathServerTime - Contact 0}.

CON25 (033 AC3): the struck entrant drawn at contact on all three machines, Death_Fall on the contact frame; hidden for
0.067 / 0.025 / 0.000 s in total around the swap (the death write's round trip).

CON16 (032): grace 30.0 s, released at 39.62 s; the exit 20 s later rustled on every machine, never drawn before the crossing.

## Run-and-observe (3-player PIE, L_Prototype, 50 NPCs; editor MCP over HTTP)

`StealthBot 2 hide` (client 2, team Jaguar, hides in bush 4), `StealthBot 1 watch2 450` (the enemy, Eagle), `StealthBot 0 watch2
350` (the ally, the listen host); then `StealthBot 2 unhide`. Every capture looked at, none black:

- `con-enter-opp-3p-after-swap.png` — the enemy's client: the bushes, the NPCs, nothing of the hider (no body, no shadow).
- `con-enter-ally-3p-after-swap.png` — the ally (host): the hatched "(hidden)" marker over the bush, no body.
- `con-enter-owner-3p-inside.png` — the hider's own client inside the bush.
- `con-exit-opp-3p-after-crossing.png` / `con-exit-ally-3p-after-crossing.png` — after the exit's crossing the hider stands
  outside the bush on both.
- 1P: not captured (the prototype's V toggle has no console path; the production toggle is Camera stories 008/009).

R5 matrix (story 034): standalone PIE, see `con-r5-results.md` and the `con-r5-*.png` captures.

## Code review (self, `.claude/skills/code-review/SKILL.md`)

Verdict: APPROVED WITH NOTES.

- Layering: the rustle is View code (client-local, no replication); the log and metrics are Server code behind the
  StealthServerOnlyGuard; Assassination talks to Concealment only through `IBushSpotQuery` (two new default-no-op methods);
  the Roster delegate is additive and server-only. No new replicated property (the allowlist audit passes in the gate).
- Data-driven: amplitude and timings are tuning rows; the rustle's onset lead (0.1 s), slot count (8) and life (1 s) are ADR-0017
  section 9 constants, named; the metric review thresholds are named GDD constants.
- Defects fixed in this batch: the pending-swap cancellation (con-033 AC3), the owner correction freeze after a bush-emerge
  strike, the grace release that never found the held pawn.
- Residual risks: the first rustle synchronously loads the sample SFX (a small hitch until the Art epic's asset is preloaded);
  the real-logout path still destroys a concealed player's pawn (Round R17 hold of a logged-out pawn not built); the enter swap
  hides a pawn struck before its commit for up to one death-write round trip before it reappears falling (outcome-blind by
  design); Crowd's virtual look uses its own envelope (see `production/qa/evidence/camera-tell-matrix-review.md`).
