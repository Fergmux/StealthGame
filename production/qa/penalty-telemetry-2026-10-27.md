# Penalty telemetry report (AC-NKP-23, story nkp-022)

**Status: evidence slot. Advisory and report-only: this report gates nothing, blocks no story and changes no tuning value.** Tuning decisions from it belong to the game-designer and creative director (story 022 Out of Scope).

## Tool and command

- Tool: `tools/analysis/penalty_telemetry_report.py` (offline, standard library only; path decision: the project's analysis tools live in `tools/analysis/`, so not `tools/telemetry/`). Input schema, the D0..Dn to JSON field mapping and the definitions are in its docstring.
- Self-test (AC2 evidence, synthetic rows incl. a seeded non-PostRoundEligible row, an empty export, a zero-respawn match, two rounds with different L, an observer log): `python tools/analysis/penalty_telemetry_report.py --self-test`
- Real run: `python tools/analysis/penalty_telemetry_report.py <folder of round exports and observer log> --out production/qa/penalty-telemetry-<date>-run.md`
- Fixtures: `tests/fixtures/nkp/telemetry/`.

## Report format

Fields, each with its n: stop reason (>= 200 informant respawns and >= 30 exiles, or the 40-match cap; the ~10-match checkpoint is reported, not judged); median and p95 `T_black` against the F3 budget (<= 30 s, <= 40 s); the share of players reaching tally L without exile per L with an estimated `b` (F4); the informant share of `WrongTarget` victims against 2 x q (F5); graced against after-grace kills of released bodies; respawn-region against city-wide informant share (OQ-NKP-13, with the > 2x on >= 10 matches observation); body-blocking incidents and which groups had voice (OQ-NKP-8), both from the separate observer log only. Rows that are not PostRoundEligible are ignored. Exit code 0 whenever a report is written.

## Dry run over SYNTHETIC fixtures (not playtest data)

Self-test output: `self-test: 37 checks passed (4 rounds, 3 matches, 1 skipped, 3 rows ignored)`

Report over `tests/fixtures/nkp/telemetry/` (SYNTHETIC; every number below is a made-up fixture value):

> # NPC-kill penalty and informant telemetry (AC-NKP-23)
>
> Advisory, report-only: observations beside reference values, no gate, nothing fails on these numbers.
>
> Input: 4 terminated round(s) in 3 match(es); 36 Event Log row(s) read, 3 ignored (not PostRoundEligible).
> Skipped (not terminated): match-d-unterminated.json.
>
> ## Stop condition
>
> n: 3 match(es), 3 informant respawn(s) (target 200), 2 exile(s) (target 30), cap 40 matches. Stop reason: not reached: keep playing (checkpoint reading near 10 matches is reported, not judged).
>
> ## F3 downtime: T_black
>
> n = 3 paired respawn(s) (0 InformantRespawn row(s) without a RespawnScheduled; 1 RespawnCancelled; 1 PawnLost; 1 RespawnStarved).
> - median T_black: 15.50 s (budget <= 30 s)
> - p95 T_black: 35.00 s (budget <= 40 s)
>
> ## F4 residual: tally >= L without exile (estimates b)
>
> | L | Players reaching tally L (n) | Without exile | Observed share | Mean I | Planning P_res at b=1 | at b=3 | Estimated b |
> |---|---|---|---|---|---|---|---|
> | 2 | 2 | 1 | 0.500 | 4.00 | 0.143 | 0.350 | 5.18 |
> | 3 | 2 | 1 | 0.500 | 2.00 | 0.111 | 0.288 | 6.50 |
>
> Per-round L; b is n/a when the share is 0 or 1 or mean I is 0. Graced bodies add to q, so b is an upper estimate of informants alone.
>
> ## F5 hunt: informant share of WrongTarget victims
>
> n = 8 WrongTarget victim(s), 2 informant(s): share 0.250 against 2 x q = 0.094 (q = 0.047, informant share of the population).
> Observation: the share is above 2 x q.
>
> ## Released bodies: graced and after-grace kills
>
> n = 13 StrikeCounted row(s): 2 graced kill(s) of a released body, 1 after-grace kill(s) of a released body.
> If after-grace kills are frequent the exit timing is reviewed, never the grace length (OQ-NKP-2/3).
>
> ## Respawn-region informant share (OQ-NKP-13)
>
> n = 2 round(s) with region samples: respawn region 0.278 (5 of 18), city-wide 0.057 (6 of 106), ratio 4.91.
> Observation: escalation condition (ratio > 2 on >= 10 matches) not met (ratio 4.91, 3 match(es)).
>
> ## Body-blocking incidents and voice (observer log, not the Event Log)
>
> n = 4 observer-form entr(y/ies), 2 incident(s) (passage <= 1 pawn wide and a player's path obstructed).
> Test groups: 3, with voice 2, without voice 1 (OQ-NKP-8).

## First checkpoint (~10 internal matches): not yet run

Not yet run. To fill: run the tool over the first ~10 matches' exports and the observer log, paste the report here. Reported, not judged.

## Full report (200 respawns / 30 exiles or 40 matches): not yet run

Not yet run. To fill: run the tool when the stop condition is reached and paste the report here, with the stop reason it prints.
