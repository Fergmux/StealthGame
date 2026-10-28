# Objectives playtest metrics (story oc-023, ADVISORY) - 2026-10-28

Story: `production/epics/objectives-chain/story-023-playtest-advisory-metrics.md` (AC-OC-19, AC-OC-20, AC-OC-21 playtest
part). Analyser: `Source/StealthGameplay/Features/Objectives/Private/ObjectivesPlaytestAnalyser.h` (F1 pacing, F2 tell timing
and probes, F3 measured shares; fixture-tested by `StealthGame.Objectives.PlaytestAnalyserFixture`).

**Status: recorded only. NOT ENOUGH DATA for all three.** No human graybox playtest round has been played (human playtesters
are an open human item in `production/session-state/active.md`). No accelerated simulation was run either: a simulation would
need logged leg times to calibrate (GDD Open Question 4), and an uncalibrated simulation would only echo its inputs.

## F1 pacing (AC-OC-19)

NOT ENOUGH DATA: 0 objective-ended graybox rounds, 20 needed. T_chain (F1, provisional inputs, L 170) = 402 s; with about 2 min
of final contest the expected median is about 9 min. The F1 pacing levers apply only to a measured median below 6 min.

## F2 tell timing (AC-OC-20)

NOT ENOUGH DATA: 0 rounds with a counted completion, 10 needed. Target E_tell = t_settle + D = 4 s (band 3.0-5.0 s inclusive);
N_probe expected about 1 per stage.

## F3 measured share (AC-OC-21)

NOT ENOUGH DATA: 0 rounds, 60 needed (or a labelled accelerated simulation). The computed posterior from the registered
CrowdDecoyTendIntervalSec (13,125 s) is P_full 0.700 (in the 0.65-0.75 band; `StealthGame.Objectives.TendPosteriorBand`); the
measured shares (full-length, early release P_short, counted vs feint) are reported by the analyser once logs exist.

## How to fill this file

Extract per round from the server Round Event Log (ObjectiveStage, FinalCompleted, TendCompleted, StageSignalReleased,
ClueTooBroad) plus the tenders' movement samples, feed `FObjectivesPlaytestAnalyser::ReportPacing / ReportTellTiming /
ReportShares`, and paste `ToMarkdown`'s output here. Rounds flagged by ObjectivePacing(Code 1) (1v0 dev rounds) and debug
completions are excluded.
