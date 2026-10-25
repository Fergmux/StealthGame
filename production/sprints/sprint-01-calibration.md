# Sprint 1 Calibration (data file)

> Written 2026-10-25 by `/sprint-plan new` for Sprint 2, from the figures the user supplied at Sprint 1 close. Aggregate data only: per-story wall-clock was not logged because the build agents ran in batches. Sprint 2 logs per batch (see `sprint-02.md`, Calibration protocol).

## Measured

| Item | Value |
|------|-------|
| Stories planned Must + Should | 28 (14 + 14) |
| Planning-days planned Must + Should | 17.0 (8.0 + 9.0) |
| Stories completed | 28 of 28 (Nice to Have: 0 of 14 closed, 6 in progress at close) |
| Wall-clock | within the first sprint day (2026-10-25) |
| Execution | serialized AI build agents, one UBT lane, batches of about 6-8 stories at about 1-2 hours per batch |
| Peak velocity | 17.0 planning-days and 28 stories per working day |
| Sprint 1 planned velocity | 0.8 planning-days per working day (8.0 over 10 days) |
| Measured / planned | about 21 times |
| Actual-to-planned ratio | at most 0.06 working days per planning-day (about 0.5 hour wall-clock); replaces the 0.6 agent-compression factor in `production/estimates/2026-10-25_mvp-rebaseline.md` |

## Caveats

- N = 1 day. The mix was script, lint, module-skeleton and spike-report work; feature Logic and multi-process integration work is heavier.
- The ratio is an upper bound: hours inside the day were not logged.
- Sprint 2 therefore plans at 8 planning-days per working day sustained (about 47 % of the peak) and corrects at its day-3 checkpoint. The second calibration point on feature work comes from Sprint 2's Roster and Round stories.
