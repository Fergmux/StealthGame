# Evidence: Objectives Chain stories 013-023, 2026-10-28

Stories: `production/epics/objectives-chain/story-013..023`. Types: Logic (013, 014, 015, 016, 017, 022), Integration (018, 019,
020, 021, 023). One commit for the batch (decision D41; the stories share the core, the service, the view helpers and the PIE
fixture, and every pushed commit must be the gate-tested tree). The Assassination PIE flake fixes of the same batch are a separate
commit (`../asn-pie-flake-fix-2026-10-28/`).

Build: `tools/dev/cycle.ps1 -NoLaunch` -> `Result: Succeeded` (no .uproject / Build.cs / Target.cs change, so no project-file
regeneration was needed). Filtered runs: `objectives-filtered.txt` (StealthGame.Objectives.*, 67 tests: 66 + the fixed 1v0 test),
`build-lints-filtered.txt` (StealthGame.Build.*, 28/28: the include lints after the court socket moved to View/ and the
prototype-exempt list shrank). Full-suite gate: `../asn-pie-flake-fix-2026-10-28/full-suite-runs.md` (the same final binaries).

## What was built

| Story | Built | Tests |
|---|---|---|
| 013 | `FObjectivesClue` (role-blind ClueOf / CountMatches), `FCourtClueRecord` + `IObjectivesCourtClues`, the EClueAreaType x EClueFeatureKind key set, BindClues + ClueTooBroad once per LayoutHash, the view's Clue, the stand-in records (quadrant + distance, public geometry only) | ClueBoundAndRoleBlind, ClueTooBroadLogged, ClueFallbackAreaOnly |
| 014 | `FObjectivesRivalSignal` (integer-ms draw keyed by the ordinal only, grid ceiling), `IObjectivesSecretDraw` over Session's GetServerSubSeed(StageSignal, k), golden vectors re-derived by `tools/analysis/oc_rival_signal_golden.py` | RivalSignalReleaseGrid, RivalSignalDistribution, RivalSignalGolden |
| 015 | the service schedules TimerFired{StageSignal, none, 0, k, viewing team} and resolves it (count <= 2 invariant, StageSignalReleased, view write); 1v0 suppression; RivalPrelimCount written for real | RivalSignalReleaseBehaviour, WaitingEarnsNothing, StageSignalPayloadAndLatch |
| 016 | `ObjectiveViewHelpers` (IsOwnLiveSite, GetStageCardRow, BuildClueText, RingIsMarigold); the IIF stand-in's FHoldStatus replies; the controller's own tend ring; the HUD tend bar Bone / Marigold and the clue on the card | TendCueDelay, ViewHelpersPure |
| 017 | `FObjectivesMetrics` (F5 stall metrics from the Event Log, tend counts; F3 posterior, inverse, band) | StallMetrics, TendPosteriorBand, RejectedFinalNotCounted |
| 018 | the Concealment rejection stand-in predicate; the stall review (`../oc-stall-review.md`, 185 hits, 0 FAIL; AC3 DEFERRED) | HidingNeverWins |
| 019 | ingress drops an Interact Begin naming no court fixture (UActionHub validator); PIE fixture | PIE.TeamViewContents, PIE.IngressAcceptsAnyCourt, PIE.InformantsShareTeamView |
| 020 | (FHoldStatus, identical at every court) | PIE.HiddenSiteOracle, PIE.CompletionNoWorldSignature |
| 021 | the 1v0 exclusion flag (ObjectivePacing Code 1) | PIE.EndToEndCacaoChain, PIE.OneVersusZeroDevRound |
| 022 | AStealthObjective deleted and redirected to the placement-only AStealthCourtSocket; prototype objective paths, CanCompleteObjective, the replicated TeamStage and the HUD markers removed; FStealthRulesObjectiveTest retired (mapping in the story) | ObjectivesStaticScan, ObjectivesIncludeLint |
| 023 | `FObjectivesPlaytestAnalyser` (F1 / F2 / F3) and the record `../oc-playtest-metrics.md` (NOT ENOUGH DATA) | PlaytestAnalyserFixture |

Measured highlights (`objectives-filtered.txt`): 1,000 assignments, 0 clue violations; golden k = 1..3 Delta 44 172 / 66 957 /
112 399 ms; KS p 0.117 per team; the release resolves exactly on its grid tick at 0.1 s and 1/60 s frames; 7 200 s of waiting
schedules no Objectives timer; ingress: four courts each 1 queue submit, 2 replies, 50 identical bytes, D = 3.000-3.001 s; 999 and
0xFFFF never queued, no reply; oracle: five courts 50 identical bytes, ring Bone -> Bone except the own live court Bone -> Marigold;
the opponent received only the clock and ping fields in both completion windows; e2e log ObjectiveStage x4 + FinalCompleted, winner
0; 1v0: one ObjectivePacing exclusion entry, no StageSignalReleased.

## Run and observe (3-player listen-server PIE, L_Prototype, 50 NPCs)

Driven through the Unreal MCP (`StartPIE` floating, `StealthGameEditor.StealthPlayToolset.RunPlayConsoleCommand` with `StealthBot`),
windows captured with `tools/dev/capture-window.ps1` and a burst loop. Every image was looked at; none is black.

- `oc13-01-team0-card-clue.png`, `oc13-02-team1-card-clue.png`: each team's card with its clue (Stage 1).
- `oc15-01-team1-rival-steps-1-of-2.png`, `oc15-02-team0-final-card-rival-steps-1-of-2.png`,
  `oc15-03-team1-rival-steps-2-of-2.png`: the rival signal released on the grid, never more than the opponent's banked preliminaries.
- `../oc-tend-*.png` (+ `-early`): the tend ring at five courts (`../oc-tend-evidence.md`).
- `../oc-e2e-*.png`: the Cacao chain to an objective win (`../oc-e2e-evidence.md`).
- The L_Prototype courts load through the class redirect as `AStealthCourtSocket` (`pie-run-and-observe-log.txt`).

Run result: OBSERVED - clue, rival steps 1 and 2 of 2, Bone/Marigold ring and a full objective win in 3-player PIE.

## Code review (self, `.claude/skills/code-review/SKILL.md` focus)

- Server-only stays server-only: the clue records, the secret draw and the release timer live behind `Public/Server/` seams; the
  helpers include no server header (ViewHelpersPure, ObjectivesIncludeLint); nothing new replicates (the court socket is not
  replicated, the prototype TeamStage is gone).
- Role-blindness is structural: ClueOf takes one public record; the draw takes the ordinal only; ingress never reads a role.
- No Objectives timer except the one TimerFired(StageSignal) per pending release; no FTimerManager; a reset clears the pending list
  and a stale timer resolves Voided.
- Doc comments on every public API; tuning read from the frozen sets (bounds converted to ms once).
- Findings fixed during the batch: the HUD's clue line pushed "Rival steps" under the Penalty tally overlay (moved onto the card
  line); the 1v0 test's 2v0 start was refused as an imbalance (now the client leaves the Lobby roster).
- Open: the court roles of the prototype map remain derivable from its layout until LSC Selection (recorded on story 022).

Verdict: APPROVED.
