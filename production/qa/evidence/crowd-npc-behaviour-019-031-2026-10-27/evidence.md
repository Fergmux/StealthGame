# Crowd & NPC Behaviour batch evidence (stories cnb-019..031, 036 scan), 2026-10-27

## What was built

- **Red herrings (019-022)**: every accepted tell of the sampler is performed through the commands a human uses: T1 run
  legs (Move with the Run gait, 2-6 s bouts released by time so they end by ordinary SCL braking at a leg boundary;
  destinations from the F1 density pick), T2 vaults and climbs (a leg over public coping, the SCL dry run, then
  `Traverse` with the point as hint; climbs only at coping >= 1.6 m; a refused point is blocked for CrowdRetryBlockSec),
  T12 reversals (stop through SCL braking, pivot at rest, walk back along the last leg), T4 braces (due by the clock,
  performed at a flinch trigger within 30 s or as an ambient startle; `Prepare` after the dry run; 55 % to the cap), T3
  decoy tends (uniform over all courts, free stand point, `Interact` after the dry run, NotNow -> a different court, 75 %
  full length) and T5 bush entries (nearest bush within 10 m of the leg, occupancy-blind, `Hide` after the dry run,
  uncapped log-normal stay, re-enter cooldown). Sites come through `ICrowdHerringSites`; the production Director runs no
  vault, climb, decoy or bush clock until LSC / Objectives / Concealment register sites (no accepted tell is left
  unperformable).
- **Tell calibration (023)**: the chain gap is show-anchored (a tell on its way holds the gap until its visible moment)
  and F2's compensation counts the measured mean show delays (GDD F2 amended; decision log 2026-10-27).
- **Strike reactions (024)**: `UActionHub::OnActionStarted(Attack)` -> `FCrowdStrikeFanOut` (keyed priority, line of
  sight in the ray budget, <= 8 responders, 8 s refractory) -> R8 reaction draw (none/glance/pause/reroute/panic run/
  startle brace; the last two only within 6 m and never inside the chain gap). The prototype strike in L_Prototype
  announces its start on the hub (bridge until the Assassination slot exists).
- **Ambient schedule and kill ledger (026-027)**: `FCrowdAmbientSchedule` (Poisson exit slots over the sockets, entries
  G after absorption, blocked-entry retry, kill vacancy + one exit cancel in the next 60 s within the LSC share bound,
  replacement = first entry at/after t_k + 10 s, fallback after 60 s) against `ICrowdScheduleQueue` /
  `ICrowdScheduleHost` seams; the agent half of an exit (walk into the alley, absorb on the clear check, give up after
  8 s watched). LSC sockets and CPA DrawReplacement are not built: the sim stands in, and the Director runs no schedule.
- **Bound pawns (028-031)**: the takeover hold (heading at walk for HandoverHoldSec, or Move(0)), hand-backs at a Director
  frame start and never during a traversal, ReleaseToCrowd (lockout under the restricted driver, promotion at a leg
  boundary, exile exit slot, AlleyExit eligibility, conditioned bush stay), `TryReleaseNpc` (atomic: eligibility,
  sinks latched without Move(0), tree stopped, control epoch bumped, ledger B + 1) and its reversal, Halt.
- **036 (scan half)**: crowd lint rule `crowd-ui` (watched failing on planted draws) and a crowd debug sentinel for the
  nightly Shipping-symbol check.

## Tests

`full-suite.txt`: `cycle.ps1 -Tests`: Result: Succeeded, **309/309** `StealthGame.*` Success (275 before the batch, +34:
Herring 4, Decoy 3, Brace 2, Bush 3, TellCalibration 2, Strike 3, Ambient 3, Replacement 3, Takeover 2, Release 2,
Informant 2, Halt 3, NoUi 2). `crowd-test-observations.txt`: every spec's measured line. Highlights: the 600-minute
reference run puts every tell's performed city-wide rate inside its AC-CNB-10 interval (run 4387 [4273-4616], vault
1258 [1156-1338], climb 478 [469-587], brace 607 [537-663], decoy 127 [107-167], bush 6018 [5801-6200], reversal 4606
[4383-4731], head hold 2889 [2859-3141]); 0 pairs of different tells within the 20 s gap with scripted strikes every
30 s and no excess just after the gap (513 vs 759-907); R8 shares within 20 % in both strata; replacement delay mean
16.3 s, p95 29.1 s, victim-blind (|d| <= 0.081).

## Run (run-and-observe, `.claude/docs/run-and-observe.md`)

Editor launched by `tools/dev/cycle.ps1`; 3-player PIE (listen server + 2 clients) through the Unreal MCP; windows
captured with `tools/dev/capture-window.ps1`; each screenshot looked at, none black.

| File | Shows |
|---|---|
| 01-host-crowd-50-herrings.png | Host at round start (Round started, 50 NPCs): the crowd walking in the market |
| 02-client1-crowd.png | Client 1: NPCs walking and standing near the objective |
| 03-client2-crowd.png | Client 2 |
| 04-strike-reactions-1s.png | Client 1 about 1 s after the host struck an NPC (`StealthBot 0 strike_npc`): the bystander crowd keeps walking; the struck body is the prototype ragdoll |
| 05-strike-reactions-5s.png | 5 s later |
| 06-host-after-strike.png | The host after the strike (Civilians killed 1/3) |

Server log: `Round started (seed 2107537304, 50 NPCs)`, `[Bot] #0 strikes BP_CrowdCharacter_C_2`, no `LogStealthCrowd`
warning or error, no `LogStealthFault` line. The framed strike-reaction look (story 025) is in
`production/qa/evidence/crowd-strike-reaction-look-2026-10-27/`.

## Code review

Self-review against `.claude/skills/code-review/SKILL.md` while the specs ran; defects found and fixed in the batch:
herring legs started while walking commanded more than 120 degrees after keyboard quantisation (follower reversal margin
15 -> 40 degrees); tells shown late broke the chain gap and then piled up just after it (show-anchored gap + delay
compensation); a second Brace acceptance while one was due dropped the first window; the brace re-drew its hold every
tick while waiting for the stand bound; a herring walk that failed lost its tell silently (requeued now); the schedule
opened a cycle after asking the host (an immediate absorption found no cycle); a kill's cancelled exit removed an entry
too (F3's Poisson entries broken: p95 32.4 -> 29.1 s); the crowd lint caught `StealthPawnFactory.h` reaching Roster
types (TryReleaseNpc now bumps the epoch through `StealthPawnControl::BumpControlEpoch`); start placement retried one
colliding node (21 of 50 NPCs; fixed in commit 7a80b25).
