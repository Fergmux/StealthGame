# Crowd classifier gate evidence (story cnb-035)

Gate: AC-CNB-28 — BLOCKING for Alpha, ADVISORY for MVP
Date: 2026-10-27
Playtest build: none (no human playtest has run)
Status: evidence pending playtest — not met (not waived)

## Method

Script: `tools/analysis/crowd_classifier_gate.py` (Python 3, standard library only). Implements GDD F6.
Per feature: single-feature threshold classifier, participant-grouped 5-fold CV, threshold and direction fit on train folds, balanced accuracy (BA) measured on the held-out fold, mean over folds.

- SE = max(0.25 * sqrt(1/n_P + 1/n_N), cluster-bootstrap SD over participants)
- PASS per feature iff BA <= 0.55 and BA + 1.645 * SE <= 0.58; gate passes iff all nine features pass
- Minimum data: >= 600 human windows, >= 30 human participants, >= 2,400 NPC windows; otherwise `INSUFFICIENT DATA`, exit code 2 (never PASS)
- Exit codes: 0 pass, 1 fail, 2 insufficient

Features (columns, snake_case): mean_speed, speed_variance, pause_rate, pause_length, heading_change_rate, head_yaw_rate, glance_hold_length, path_straightness, full_pace_share.

## How to run on real data

1. Human windows: export from the playtest telemetry pipeline (OQ12): 30 s windows of ordinary walking only (no run, traversal, action or tell), as CSV or JSONL with columns `source=human`, `participant`, and the nine feature columns.
2. NPC windows: take the nightly NPC recording (story cnb-034 stream, same feature computation) as rows with `source=npc` and any `participant` id.
3. Concatenate both into one file and run:

```
python tools/analysis/crowd_classifier_gate.py windows.csv --out production/qa/evidence/crowd-classifier-gate-<date>.md
```

If a feature fails, the fix is a new `UCrowdMotionProfileData` (data) and a re-run; never a runtime filter. Update this file's header (build id, status) with the real results once a playtest has run.

## Selftest output (synthetic data only; proves the script, not the game)

Command: `python tools/analysis/crowd_classifier_gate.py --selftest` (exit 0)

## Case A: identical distributions (expect PASS)
| Check | Value | Required |
|---|---|---|
| human windows | 900 | >= 600 |
| human participants | 36 | >= 30 |
| NPC windows | 3000 | >= 2400 |

Unclustered SE = 0.25*sqrt(1/n_P + 1/n_N) = 0.0095; SE used = max(unclustered, cluster-bootstrap SD).

| Feature | held-out BA | SE | upper (BA+1.645*SE) | Result |
|---|---|---|---|---|
| mean_speed | 0.4930 | 0.0112 | 0.5115 | PASS |
| speed_variance | 0.4996 | 0.0366 | 0.5598 | PASS |
| pause_rate | 0.5111 | 0.0113 | 0.5297 | PASS |
| pause_length | 0.4981 | 0.0485 | 0.5779 | PASS |
| heading_change_rate | 0.5043 | 0.0120 | 0.5240 | PASS |
| head_yaw_rate | 0.5006 | 0.0114 | 0.5194 | PASS |
| glance_hold_length | 0.4989 | 0.0252 | 0.5403 | PASS |
| path_straightness | 0.5043 | 0.0215 | 0.5397 | PASS |
| full_pace_share | 0.5303 | 0.0095 | 0.5459 | PASS |

GATE: PASS (BA <= 0.55 and upper <= 0.58 for all 9 features)

## Case B: pause_length shifted +1 SD for humans (expect FAIL on that feature)
| Check | Value | Required |
|---|---|---|
| human windows | 900 | >= 600 |
| human participants | 36 | >= 30 |
| NPC windows | 3000 | >= 2400 |

Unclustered SE = 0.25*sqrt(1/n_P + 1/n_N) = 0.0095; SE used = max(unclustered, cluster-bootstrap SD).

| Feature | held-out BA | SE | upper (BA+1.645*SE) | Result |
|---|---|---|---|---|
| mean_speed | 0.4930 | 0.0112 | 0.5115 | PASS |
| speed_variance | 0.4996 | 0.0366 | 0.5598 | PASS |
| pause_rate | 0.5111 | 0.0113 | 0.5297 | PASS |
| pause_length | 0.6873 | 0.0099 | 0.7036 | FAIL |
| heading_change_rate | 0.5043 | 0.0120 | 0.5240 | PASS |
| head_yaw_rate | 0.5006 | 0.0114 | 0.5194 | PASS |
| glance_hold_length | 0.4989 | 0.0252 | 0.5403 | PASS |
| path_straightness | 0.5043 | 0.0215 | 0.5397 | PASS |
| full_pace_share | 0.5303 | 0.0095 | 0.5459 | PASS |

GATE: FAIL (BA <= 0.55 and upper <= 0.58 for all 9 features)

## Case C: 300 human windows (expect INSUFFICIENT DATA, exit 2)
| Check | Value | Required |
|---|---|---|
| human windows | 300 | >= 600 |
| human participants | 36 | >= 30 |
| NPC windows | 3000 | >= 2400 |

INSUFFICIENT DATA - gate not evaluated (never PASS).

SELFTEST OK
