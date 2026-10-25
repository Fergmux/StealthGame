# OQ9 / C-W24 grid-only final-court enumeration, 2026-10-25

> **Story:** `production/epics/procedural-city-generator/story-008-oq9-final-court-enumeration-script.md` | **Script:** `tools/citygen/oq9_enumerate.py` (deterministic, integer-only, no geometry, no engine) | **Data:** `2026-10-25_citygen-oq9-final-court-enumeration.csv` (sha256 `5556754914d05430`)

**Verdict: CONCERNS**

The word is the AC3 comparison, recomputed from the CSV by the script and by its self-test: take the lower of the two anchor policies' baseline fractions (the conservative reading, so the choice of policy cannot flatter the result); PASS if it is >= 0.30, CONCERNS if it is < 0.30 but >= 0.30 after a lever (a) start-cell radius 16 m, jitter 0 or 2, or (b) a lower `StartToFinalMinPathCm` (not below the 40 m LSC hard minimum), FAIL if < 0.30 after every lever.

- Baseline (F 44 m, jitter 1, start-cell radius 18 m, 8 % path guard), 200 skeletons: **fixed** 15/200 = 0.075, **moving** 20/200 = 0.100 reach `FinalCandidatesMinCount` 6; lower = 0.075 (15/200).
- Lever that reached 0.30 first (in the order tried): **j0**.

## 1. Method

The C++ stages (stories 003, 005, 006) do not exist yet, so this script carries a minimal copy of S1 (frame, N 62-66), S2 (5 x 5 regular lattice, blocks 7-10 cells, interior corridors 3 cells 65 % / 2 cells 35 %, lanes 3 cells, transpose symmetry, per-half per-axis per-cut integer jitter), the S4 axis-corner court pool and the S5 zone anchors and start cells, with PCG32 streams (a stage's draws never depend on the jitter amplitude, so levers compare like with like). Grid path = 4-connected BFS over corridor cells x 2 m. See the script docstring for the full model. The 200 seeds are a committed literal list.

- **fixed** policy: the six S4 court junctions sit at the cells of the same skeleton with zero jitter (snapped to the nearest walkable cell), whatever the jitter did; zone anchors and start cells always sit on the actual jittered streets.
- **moving** policy: the court junctions follow the jittered cuts.
- Final-eligible court = path from the nearest start cell (either zone) >= 1.08 F, >= 3 ingress arms, >= 24 m from every other counted court of its corner. The count is over the six S4 pool courts (max 6); the `adaptive` column is a diagnostic only (the exact maximum set of >= 24 m-spaced eligible lattice junctions, whatever S4 would place).

## 2. Arithmetic fixtures (AC2)

| Fixture | final-eligible courts | margin at (31, 31) |
|---|---|---|
| Mean geometry (N 64, all blocks 8, all corridors 3, no jitter) | 6 | 0.48 m |
| (31, 31) moved 2 m outward in x, to (33, 31) (negative control) | 5 | -1.52 m |
| (31, 31) moved 2 m outward on both axes, to (33, 33) (observation) | 6 | 0.48 m |

The mean fixture reproduces the GDD S4 arithmetic exactly (6 courts, 0.48 m). The negative control reproduces 5. Observation for the GDD author: the formula `x + y <= 110 - 1.08 F` assumes the far anchor's row/column; a court that moves outward along the sun axis stays the same Manhattan distance from the two symmetric anchors, so the diagonal move does **not** lose the court on the grid (the row above). Only an off-axis outward move (x or y alone) does. The formula is therefore a bound on the axis-aligned moves, not on the diagonal ones.

## 3. Baseline result (AC1)

| Policy | mean final-eligible count | courts histogram 0..6 (sums to 200) | fraction >= 6 |
|---|---|---|---|
| fixed | 3.11 | [13, 23, 43, 29, 49, 28, 15] | 0.075 (15/200) |
| moving | 3.27 | [5, 22, 48, 38, 32, 35, 20] | 0.100 (20/200) |

Per-skeleton counts are in the CSV (`final_count`, 200 rows per policy and scenario).

### Margin distribution of the (31, 31) junction (near axis court), baseline

margin = grid path from the nearest start cell - 1.08 F, metres; negative = the court fails the 8 % guarded floor.

| Policy | min | p10 | p50 | p90 | max | share >= 0 |
|---|---|---|---|---|---|---|
| fixed | -7.52 | -3.52 | 0.48 | 4.48 | 10.48 | 116/200 |
| moving | -11.52 | -5.52 | -1.52 | 4.48 | 10.48 | 99/200 |

1 m margin histogram (bucket lower edge in m, count), baseline:

- fixed: -8: 4, -6: 12, -4: 27, -2: 41, 0: 44, 2: 29, 4: 24, 6: 14, 8: 4, 10: 1
- moving: -12: 1, -10: 3, -8: 10, -6: 24, -4: 26, -2: 37, 0: 39, 2: 26, 4: 24, 6: 7, 8: 2, 10: 1

### Formula versus measurement

The GDD formula (Manhattan distance to the mean-geometry anchors minus the start-cell radius, against 1.08 F) agrees with the grid measurement on 969 of 1200 court checks in the baseline (moving policy; the formula ignores walls, jitter of the anchors and ingress/spacing). Mean formula-predicted eligible courts per skeleton: 3.73; mean measured: 3.27.

## 4. Levers (fraction of 200 skeletons reaching 6; lower of the two policies marked)

| Scenario | tier | F m | R m | jitter | fixed | moving | lower | >= 0.30 |
|---|---|---|---|---|---|---|---|---|
| baseline | 0 | 44 | 18 | 1 | 15/200 (0.075) | 20/200 (0.100) | 0.075 | no |
| a_R16 | 1 | 44 | 16 | 1 | 44/200 (0.220) | 51/200 (0.255) | 0.220 | no |
| j0 | 1 | 44 | 18 | 0 | 68/200 (0.340) | 68/200 (0.340) | 0.340 | yes |
| j2 | 1 | 44 | 18 | 2 | 0/200 (0.000) | 4/200 (0.020) | 0.000 | no |
| a_R16+j0 | 1 | 44 | 16 | 0 | 123/200 (0.615) | 123/200 (0.615) | 0.615 | yes |
| a_R16+j2 | 1 | 44 | 16 | 2 | 1/200 (0.005) | 22/200 (0.110) | 0.005 | no |
| b_F43 | 2 | 43 | 18 | 1 | 15/200 (0.075) | 20/200 (0.100) | 0.075 | no |
| b_F42 | 2 | 42 | 18 | 1 | 44/200 (0.220) | 51/200 (0.255) | 0.220 | no |
| b_F40 | 2 | 40 | 18 | 1 | 66/200 (0.330) | 101/200 (0.505) | 0.330 | yes |
| a_R16+b_F40 | 2 | 40 | 16 | 1 | 88/200 (0.440) | 140/200 (0.700) | 0.440 | yes |
| a_R16+j0+b_F40 | 2 | 40 | 16 | 0 | 197/200 (0.985) | 197/200 (0.985) | 0.985 | yes |

Diagnostic (not the S4 policy, not part of the verdict): skeletons where the exact maximum set of >= 24 m-spaced eligible lattice junctions reaches 6 (an upper bound on any S4 placement rule): baseline 1/200, a_R16 2/200, j0 13/200, j2 0/200, a_R16+j0 15/200, a_R16+j2 0/200, b_F43 1/200, b_F42 2/200, b_F40 5/200, a_R16+b_F40 6/200, a_R16+j0+b_F40 16/200.

## 5. Proposed rules

- **Anchor-jitter rule (proposed): MOVING.** S4 court junctions follow the jittered cut positions (they are junctions of the actual skeleton; there is no separate fixed anchor table to maintain).
- **Start-cell radius (proposed): 16 m**, with `StartToFinalMinPathCm` 44 m and `CityGenSkeletonJitterCount` 0 (scenario `a_R16+j0`).
- At jitter 0 the two policies coincide (identical rows), so the anchor-jitter rule only matters while jitter >= 1, where moving is the higher of the two at the baseline (fixed 15/200, moving 20/200, both far below 0.30).
- Robustness note: the first lever to clear 0.30 (`j0`) does so by 8 skeletons of 200 (sampling error about 3 points), so when the margin is under 10 points the proposal above is the strongest tier-1 combination (`a_R16+j0`, 0.615) rather than the bare lever.
- `FinalCandidatesMinCount` stays 6 and `CityGenFinalEligibleTargetCount` stays 6; this report proposes no change to either.
- The defaults alone do not reach 0.30; the cited lever does. Stories 010, 011, 022 and 038 take the proposed rule and radius above as their starting values and carry the lever as a documented G-PRE tuning change (data file value, not code).

## 6. Findings and follow-ups

- Modelling assumptions that move the numbers (change them in the real stages and re-run): block size weights (mean = 8 only at N = 64 with all-3 corridors; N and the alley share shift the cut positions by whole cells), independent x/y jitter per half, the axis court as the half-A junction snapped to the walkable cell, anchors at the junction cell (the GDD puts them 1-2 cells along the arm), no jogs, no T-stagger, no canals. Jogs, stagger and canals only lengthen paths on the lattice, so ignoring them does not flatter the failure rate.
- Two independent effects cut the fraction, both measured above: the block and N draw alone (jitter 0 row, blocks still drawn) gives 0.340 of skeletons against 6/6 at the mean fixture, because the (31, 31) cut sits at 15 + 2 b1 m with b1 the first block (7-10 cells); and jitter 1 on top takes the baseline to 0.075. A design lever outside this story's list, not measured here: pin the first and last block of each axis to 8 cells so the S4 anchor cuts stay near the mean positions.
- No placement rule can rescue the baseline by choosing other junctions: the adaptive diagnostic (any eligible lattice junctions, exact maximum spaced set) reaches 6 on at most 16 of 200 skeletons in any scenario.
- Follow-up (level-designer, design-doc change, not made here): update GDD S4 / Open Question 9 and LSC Open Question 15 text with this report's numbers and the proposed anchor-jitter rule and radius; C-W24 closes when this report is committed.
- Follow-up (stories 010 and 011): adopt `evaluate()`'s court cells, anchor and start-cell probe as the starting point; replace the minimal S1/S2 copy here with the real stages (stories 005 and 006) and re-run before the golden seeds (story 031).
- Re-run: `python tools/citygen/oq9_enumerate.py run` (output is byte-identical); `python tools/citygen/oq9_enumerate.py selftest`.

