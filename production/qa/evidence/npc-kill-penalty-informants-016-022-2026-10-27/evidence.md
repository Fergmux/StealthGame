# Evidence: NPC-Kill Penalty & Informants stories 016-022, 2026-10-27 / 28

Stories: `production/epics/npc-kill-penalty-informants/story-016..022`. Types: Integration (016, 017, 021, 022), UI (018),
Visual/Feel (019), Config/Data (020). One commit for the batch (decision logged: the stories share the controller, HUD and
tally changes, and every pushed commit must be the gate-tested tree).

Build: `tools/dev/cycle.ps1 -Tests -NoLaunch` -> `Result: Succeeded`. Project files regenerated after the
StealthPresentation.Build.cs change: `Build.bat -projectfiles ... Result: Succeeded`. `Stealth.Tuning.SchemaGen` (after the
TuningSchemaVersion 7 rows): `UNCHANGED` (the hand-edited table equals the reflection). Stamp: `stamp.txt`.

## Full-suite gate runs (`^StealthGame.`, 449 tests: 433 + 17 new - 1 retired)

| Run | Result | Failures | WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove | LocomotionPie.ReleaseBaselineStopUnderLag | StrikeClipParity |
|---|---|---|---|---|---|
| 1 (`full-suite-run1.txt`) | 448 / 449 | `Assassination.PIE.NoObserverCue`: an awaiting-informant observer's own countdown ticks were listed as cues (defect in the test's allowlist for this batch's new element; fixed) | Success | Success | Success |
| 2 (`full-suite-run2.txt`) | 446 / 449 | `NoObserverCue` (engine ensure `TMatrix::InverseFast` NIL matrix in MotionWarping, see below), `OneBodySequence` and `OwnerResultShape` (the informant of a 0-NPC harness round never respawned: harness-only fallback fixed, below) | Success | Success | Success |
| 3 (`full-suite-run3.txt`) | 448 / 449 | `PossessionThroughFall` (the same MotionWarping ensure, once per process, right after a strike) | Success | Success | Success |
| 4 (`full-suite-run4.txt`) | 447 / 449 | `KillAtArmsLength` (the server's Death_Fall 136 ms after contact) and `StrikeClipParity` (all four pawn targets 59 / 48 cm off the empty-spot WarpPoint (1441,-48) vs (1500,0); lines in `run4-strikeclipparity-lines.txt`); same binaries as run 3 | Success | Success | **Fail** |
| 5 (`full-suite-run5.txt`) | 448 / 449 | `KillAtArmsLength` (Death_Fall 45 ms after contact on the host) | Success | Success | Success |
| 6 (`full-suite-run6-partial.txt`) | stopped at 102 | `KillAtArmsLength` (machine 1, late Death_Fall) | (not reached) | (not reached) | Success |
| **7 FINAL (`full-suite.txt`)** | **449 / 449** | none (`tests exit=0`) | Success | Success | Success |

`WalkSlowerModifier.KeyboardModifiersScaleTheServerReceivedMove` and `LocomotionPie.ReleaseBaselineStopUnderLag` passed in
every run that reached them (runs 1-5 and 7), so no WALKSLOWER / LOCOPIE lines needed copying. `StrikeClipParity` failed once
(run 4); its lines are retained.

Baseline for the timing failures of runs 4-6: HEAD `bd3bff7` (before this batch) in a detached worktree, the same gate,
stopped after 167 tests: `PrototypeScenariosACF.F_BlindStrike...` timed out there ("the enemy is concealed") and
KillAtArmsLength passed (`baseline-bd3bff7-partial-gate.txt`; its two StealthGame.Build lint failures are worktree-path
artefacts). The Assassination PIE timing tests are load-sensitive in the full gate before and after this batch. To leave the
least load possible, the per-frame Penalty HUD work was then cut (the tally view is rebuilt only when its inputs change; the
presentation tick computes the countdown without the tally text); run 7 passed 449 / 449.

### Two intermittent failures found by the gate

1. **The 0-NPC harness respawn hang** (OneBodySequence, OwnerResultShape, CulledVictimOpensFalling, PossessionThroughFall: each
   timed out at "the ... respawns" in some runs, passed in others). Cause: story nkp-014's harness-only crowd-less respawn
   required a world with no crowd agent at all, but these rounds kill an NPC and the prototype game mode spawns a
   replacement NPC (`ScheduleNpcReplacement`, regardless of NpcCount) a few seconds later; when that one NPC stood where an
   opponent could see it, the pick found nothing and the informant waited forever. Fix (harness-only, compiled out of
   Shipping, off in every production path): the fallback fires whenever the pick finds no eligible unseen crowd pawn. After
   the fix the six 0-NPC tests passed 12 / 12 in two back-to-back filtered runs and in full-suite run 3.
2. **An engine ensure** `TMatrix<T>::InverseFast(), trying to invert a NIL matrix` raised from `UnrealEditor-MotionWarping.dll`
   about 0.4 s after a strike's slot (run 2 in NoObserverCue, run 3 in PossessionThroughFall, a filtered run in
   OneBodySequence; 16 log lines per occurrence). An ensure is reported once per process, so whichever test is running
   fails. Baseline: the same 27 Assassination PIE tests on HEAD `bd3bff7` (a detached worktree build) passed 27 / 27 with 0
   ensure lines. Cause: this batch's first `AStealthPlayerController::SetPawn` kept the view on ANY released body, so a
   dead, hidden body stayed a client's view target and its pose evaluation produced the NIL matrix. Fix: only a LIVE released
   body (the exile's hand-over) keeps the view; a dead body's release keeps the engine behaviour (Camera's fade owns that view).
   After the fix: 54 / 54 Assassination PIE tests in two filtered runs, 0 ensure lines; ExileBeatTiming, InformantKilledView
   and RemovalAudiences still pass.

## New tests

| Story | Test | Rig |
|---|---|---|
| nkp-016 AC1 | `StealthGame.Penalty.StrikeCountNeverLeaves` (reflection over the four runtime packages: no replicated property / RPC parameter named for the strike count, only `FStrikeResult.TallyAfter` and `FTeamViewOwnerPart.WrongTargetTally` reach a client and both through owner-only roots (`ClientCommandReplies`, `URosterTeamViewComponent::View` COND_OwnerOnly); two-sentinel live probe (k = 7 vs 200, tally 250): reply and owner-part bytes identical, a one-lower tally changes them; negative control) | core + reflection |
| nkp-016 AC3 | `StealthGame.Penalty.PIE.RemovalAudiences.ExileReachesEachAudienceAsAnNpcKillOrAnAssassination` | 4-player PIE (2v2), 40 NPCs |
| nkp-016 (Roster defect) | `StealthGame.Roster.ExileRetiringUpdateMatchesAssassinated` | Roster core + push step |
| nkp-017 AC1 | `StealthGame.Penalty.PIE.ExileBeatTiming.{OnTime,Late300Ms,Late900Ms}CardEntersAtPlatePlusBeat`, `.DroppedResultHoldsTheViewWithoutACard` | 3-player PIE, reply lateness / drop seams |
| nkp-017 AC2 | `StealthGame.Penalty.PIE.InformantKilledView.NoCardViewOnFallingBodyCountdownAfterFade` | 3-player PIE |
| nkp-017 AC3 | `StealthGame.Penalty.PIE.TallyRepushGate.OlderRepushFirstNeverMovesTheTallyBeforeThePlate`, `StealthGame.Roster.OwnerTallyOwnerOnly` | 3-player PIE; push step |
| nkp-018 | `StealthGame.Penalty.HudViewModel`, `StealthGame.Penalty.PIE.TallyWidgetParity.NpcKillAndInformantKillDumpEqualAtSameTally` | view-models; 3-player PIE |
| nkp-020 | `StealthGame.Tuning.StrikeLimitOneWarning`, `StealthGame.Tuning.ResidualHuntModel` (smoke `production/qa/smoke-2026-10-27-penalty-tuning.md`) | real tuning pipeline; test-only evaluator |
| nkp-021 | `StealthGame.Penalty.PIE.PrototypeParityExile.ThirdUngracedNpcKillExilesTeammateStaysActive`, `StealthGame.Penalty.PIE.PrototypeParityInformantKill.InformantKillRebodiesByTakeoverNoStrike`, `StealthGame.Penalty.PrototypeRetired` | 3-player PIE; source lint |
| nkp-022 | `python tools/analysis/penalty_telemetry_report.py --self-test` -> 37 checks passed | synthetic rows |

Retired: `StealthGame.Rules.NpcKillRemoval` (story 021; its boundary is covered by StrikeRuleMatrix, ExileOnLthNpcKill and
PrototypeParityExile).

Measured lines of the PIE tests (run 3): `pie-test-measurements.txt`. Highlights: the exile card enters on the frame of
s + ExileBeatSec at 0 / 300 / 900 ms of RPC lateness, with 0 frames off the released body, 0 black frames and 0 frames of
control after the plate; the killed informant's countdown first shows at 26.074 for a fade end of 26.064; the older re-push
arrived 0.47 s before kill 2's plate and changed nothing; the ally shape of an exile now equals an Assassinated removal
(`State+PawnRef+RetireAtServerTime | PawnRef+RetireAtServerTime`); the opponent's non-movement changes over the exile window are
those of an NPC kill (`Actor.ActionState`) plus `Actor.PlayerState` (IVP story 018, Blocked).

## Defects found by the new tests and fixed in this batch

- **The owner's view left the released body** (ExileBeatTiming watched 46 of 48 frames off the body before the fix): the
  owning client's `APlayerController::SetPawn(nullptr)` moved the auto-managed camera target to the controller when the pawn
  replicated null. `AStealthPlayerController::SetPawn` keeps the view on a LIVE released body (the exile's hand-over; ADR-0019
  section 4.4); a dead body's release is unchanged (see the MotionWarping ensure above for why).
- **An exile reached the ally in a different shape from an Assassinated removal** (RemovalAudiences: `[State]` against
  `[State+PawnRef+RetireAtServerTime | ...]`): the StrikeLimit release left no retiring update. `FRosterCore::FindLatestRemovedPawn`
  gives it the same RetireAtServerTime update (Roster R8 / AC-PSR-57's StrikeLimit clause), pinned by
  `StealthGame.Roster.ExileRetiringUpdateMatchesAssassinated`.
- **A killed informant got a removal card** ("You were silenced..."): removed (PEN-R16, CR-18).
- **The countdown showed behind the exile card** (run-and-observe): the countdown waits for the card's collapse.
- Test-code defects fixed during the build: `GetLifetimeReplicatedProps` on every runtime class's CDO asserted inside
  AStealthGameState (the scan now reads lifetime conditions only for the tally roots); a case-insensitive name match flagged
  the public `NpcKillStrikeLimit`; the ally-shape sampler captured its viewer before the test had chosen it.

## Run and observe (3-player listen-server PIE, L_Prototype, 50 NPCs)

Driven through the Unreal MCP (`StartPIE` floating, `StealthPlayToolset.RunPlayConsoleCommand` with `StealthBot`); windows
captured with `tools/dev/capture-window.ps1` and a burst variant (background PIE windows refresh at about 3 fps, so strips show
order, not exact frames). Teams: host #0 and #2 Jaguar (team 0), #1 Eagle (team 1). Every image was looked at; none is black.

- UI (story 018): `nkp-19-*` (tally 0-4 and frozen), `nkp-20-*` (plate, push-off, card, collapse), `nkp-31-*` (owner
  countdown 6 / 5 / static "Returning", ally and opponent without it). Sign-off file `production/qa/evidence/penalty-ui-evidence.md`.
- Killed informant (story 017): `nkp-44-*` (no card, countdown after the fall).
- Visual (story 019): `nkp-12-*` (the cut to the new body, the ally's marker), `nkp-43-*` (the held view on the released body).
  Sign-off file `production/qa/evidence/penalty-visual-evidence.md` (art lead PENDING).

Run result: OBSERVED — the tally line, the exile beat and card, the returning countdown and the takeover cut in 3-player PIE
(`nkp-19-2.png`, `nkp-20-3-card-entering-plate-pushed-off.png`, `nkp-31-owner-returning-in-6.png`, `nkp-12-2-after-cut-new-body.png`).

## Code review (self, `.claude/skills/code-review/SKILL.md` focus)

Presentation builds only from View/ data (the view-model takes plain numbers; no Server/ header in StealthPresentation; the
overlay is hit-test-invisible and removed in EndPlay). Owner-only data stays owner-only (the owner tally re-push and T_due
ride the COND_OwnerOnly owner part; nothing new replicates). Penalty adds no replicated type (StrikeCountNeverLeaves). The
harness seams (reply lateness / drop, counters, strike count reads) are behind `WITH_STEALTH_TEST_SEAMS`. Doc comments on
public APIs. Verdict: APPROVED, with one routed finding (IVP 018's `Actor.PlayerState` on the released pawn).

## Blockers recorded on the stories

- nkp-016: AC2 DEFERRED (two-run differential capture harness); AC3 opponent clause on IVP story 018, cadence on the
  multi-process capture.
- nkp-017: Camera epic (ViewRelease, the ExileCardTimeoutSec fallback fade, the NoPawnFadeSec fade).
- nkp-018: AC2's reconnect-during-card clause (Session PIE rejoin not built); UX / HUD lead review pending.
- nkp-019: art-lead sign-off PENDING; Camera's grade / fade-in; AC2 opponent per-frame log; AC3 bounds.
- nkp-022: AC1 (advisory) waits for internal playtests; export gaps listed in the tool.
