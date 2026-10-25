# Story 008: OQ9 / C-W24 grid-only final-court enumeration script

> **Epic**: Procedural City Generator
> **Status**: In Progress — script, self-test, report and CSV done; the C++ Automation wrappers are DEFERRED (engine run pending)
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-25

## Context

**GDD**: `design/gdd/procedural-city-generator.md` (Open Question 9, S4 "OPEN verification (cross-review C-W24)"; also `design/gdd/level-socket-contract.md` Open Question 15 and the V-SEP feasibility note)
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. The script is the integer-grid pre-validator's feasibility proof for the S4/S5 role counts.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: G-PRE evaluates the LSC predicates on the integer cell grid (integer grid path lengths, an 8% path guard on absolute floors) so a candidate that cannot reach `FinalCandidatesMinCount` final-eligible courts is discarded before geometry exists. This story measures how often the skeleton alone can reach it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++, no engine API)
**Engine Notes**: The script is a headless C++ harness (an Automation test or a commandlet in `StealthTestHarness` that links the pure Generator core), never an editor or MCP script (control manifest: no gate may depend on MCP or Python editor scripting). It builds skeletons only: no facades, sockets, geometry or navmesh.

**Control Manifest Rules (this layer)**:
- Required: Integer-only RNG and canonical iteration (stage streams from story 003); G-PRE guard values from the GDD (StartToFinal 44 -> 47.5 m) — source: ADR-0006, ADR-0014
- Forbidden: Never lower `FinalCandidatesMinCount` below 6 to make a layout pass; never claim a pass rate the script did not measure; scripts emit observations, the verdict is a documented comparison — source: GDD S4, `.claude/docs/context-management.md` (helpers emit observations, never verdicts)
- Guardrail: the 200-skeleton run completes in a few seconds on a developer PC (no geometry)

**Why this story**: the GDD closed OQ9 by arithmetic on 2026-10-25 (decision D4: `StartToFinalMinPathCm` 60 -> 44 m, `FinalCandidatesMinCount` stays 6), but the margin at the default is 0.48 m against a 2 m jitter step and the GDD does not say whether the S4 anchor junctions ((31, 31), (9, 50), (50, 9) m) move with the jitter. Until this script runs, `p_g` (F6, design >= 0.30) is unverified and C-W24 stays open. **This story releases stories 010, 011, 022 and 038.** It depends on no S4/S5 story: it carries its own minimal anchor and start-cell probe, which stories 010 and 011 adopt as their starting point.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (Open Question 9 steps, S4 caveat) and LSC Open Question 15, scoped to this story:*

- [ ] **AC1** (`FinalPool.Enumeration200`): **GIVEN** 200 skeletons from a committed seed list at the defaults (`StartToFinalMinPathCm` 44 m, jitter 1, start-cell radius 18 m, 8% path guard) **WHEN** the script runs on the integer grid with no geometry **THEN** it reports per skeleton the number of final-eligible courts with >= 3 ingresses under both anchor policies (S4 anchors fixed vs moving with the skeleton jitter), the fraction reaching `FinalCandidatesMinCount` 6, the histogram of counts and the margin distribution of the (31, 31) junction.
- [ ] **AC2** (`FinalPool.ArithmeticFixtures`): **GIVEN** a mean-geometry fixture (no jitter) **THEN** the script counts 6 final-eligible courts with a 0.48 m margin at (31, 31), and **GIVEN** the same fixture with the (31, 31) junction moved 2 m outward **THEN** it counts 5 (the C-W24 figures, a negative control).
- [ ] **AC3** (`FinalPool.LeverReport`): **GIVEN** the AC1 result **THEN** the report in `production/qa/spikes/` states PASS (the lower of the two anchor policies' baseline fractions is >= 0.30), CONCERNS (that lower fraction is < 0.30 but >= 0.30 after lever (a), start-cell radius 16 m, and/or jitter 0 or 2) or FAIL (< 0.30 after every lever); it always states the proposed anchor-jitter rule and start-cell radius, never proposes a lower `FinalCandidatesMinCount`, and the report and a CSV are committed.

---

## Implementation Notes

*Derived from the GDD S4 feasibility arithmetic and Open Question 9:*

Model (GDD S4): with anchors at about (31, 97) m and (97, 31) m in the corner frame, start cells up to 18 m nearer, and lattice paths about Manhattan distance, a court at (x, y) in an axis corner is `128 - x - y` m of path from the far anchor's cell, so its nearest start cell is `110 - x - y` m away; G-PRE needs that >= 1.08 F (F = `StartToFinalMinPathCm`), i.e. `x + y <= 110 - 1.08 F`. At F = 44 m that bound is 62.48, which admits (31, 31) (x + y = 62, margin 0.48 m) and the set {(9, 50), (31, 31), (50, 9)}, >= 29 m pairwise: 3 per axis corner, 6 in all. The script must measure on the actual skeleton grid (integer grid path lengths, the S2 jitter records from story 006), not on the formula, and report the formula's prediction beside the measurement as a check.

Run both anchor policies from the same 200 skeletons: "fixed" (S4 anchor junctions stay at the mean positions) and "moving" (they follow the jittered junction). Seeds come from a committed list in the test source (a `constexpr` array, never a file read at test time, ADR-0008 section 9). The levers if the fraction is low, in order (GDD OQ9 / LSC OQ15): (a) start-cell radius 16 m instead of 18 m (+2 m); then a lower `StartToFinalMinPathCm` (safe minimum 40 m, LSC hard 40-100 m); never a lower `FinalCandidatesMinCount`. The p_g design floor of 0.30 (F6) is an upper-bound check: the pool fraction bounds `p_g`, so a pool fraction below 0.30 cannot meet the design. The verdict word is derived from the CSV fractions by the documented rule in AC3 (take the lower of the two policies, the conservative reading, so the choice of policy cannot flatter the result); the test recomputes the word from the CSV and compares it with the report's.

The script result does not edit the GDD or LSC: closing C-W24 means committing the report; updating GDD OQ9 and LSC OQ15 text is a design-doc change for the level-designer (named in the report's follow-ups).

Prototype replaced: none (the prototype logged a reachability check only).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-010-s4-courts.md` and `story-011-s5-zones-anchors-and-start-cells.md`: the real S4/S5 stages (blocked on this report).
- `story-022-g-pre-predicates-b-and-role-counts.md`: the production role-count pre-check.
- `story-038-role-prior-and-lsc-batch-audits.md`: the 50-seed batch audits on real layouts.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/citygen/oq9_enumerate.py` (the implemented script and its `selftest`; created)
- DEFERRED C++ ports: `Source/StealthGameplay/Private/Features/CityGenerator/CityGenFinalPoolProbe.h` (minimal anchor, start-cell and final-eligibility probe on the skeleton grid)
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenFinalPoolProbe.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/FinalPoolEnumerationTest.cpp` (class `FCityGenFinalPoolEnumeration200Test`, name `StealthGame.CityGen.FinalPool.Enumeration200`)
- `Source/StealthGameplay/Private/Tests/CityGen/FinalPoolArithmeticFixturesTest.cpp` (class `FCityGenFinalPoolArithmeticFixturesTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/FinalPoolLeverReportTest.cpp` (class `FCityGenFinalPoolLeverReportTest`)
- `production/qa/spikes/[date]_citygen-oq9-final-court-enumeration.md` and `.csv` (the report, written by the story)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE for AC1 and AC2; AC3 revised twice and the final rule is the one in the criterion above).*

- **AC1** `FinalPool.Enumeration200`
  - Given: the committed 200-seed list and defaults.
  - When: the script runs twice on the integer grid.
  - Then: there are 200 rows under each of the two anchor policies. Each row has the final-eligible count (courts with at least 3 ingresses). The output also holds the fraction at or above 6, a histogram summing to 200, and the (31, 31) margin distribution. The two runs are identical.
  - Edge cases: no float RNG. The test drives the script as a C++ library call (not a commandlet) so an Automation test can run it.
- **AC2** `FinalPool.ArithmeticFixtures`
  - Given: a mean-geometry fixture with no jitter.
  - When: the script counts.
  - Then: 6 courts and a 0.48 m margin at (31, 31), +-0.005.
  - Edge cases: the (31, 31) junction moved 2 m outward gives 5 (negative control). The bound is x + y <= 110 - 1.08 F = 62.48 at F = 44.
- **AC3** `FinalPool.LeverReport`
  - Given: the CSV and the report.
  - When: the test recomputes the verdict word from the fractions by the rule in the AC.
  - Then: it equals the report's word; the report states an anchor-jitter rule and a start-cell radius, contains no proposal of a lower `FinalCandidatesMinCount`, and both files exist.
  - Edge cases: a fraction of exactly 0.30 gives PASS. A CONCERNS report must cite the lever that reached 0.30.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass, plus the committed report and CSV in `production/qa/spikes/` (`/story-done` checks that they EXIST)

**Status**: [x] Script, self-test and report created; [ ] C++ Automation wrappers DEFERRED (engine run pending)

**Implementation decision (2026-10-25, unattended run, non-UE tools only):** the story's C++ probe/test files require the Unreal toolchain, which another agent held, and the stages it names (003, 005, 006) do not exist yet. The enumeration is implemented as a deterministic integer-only Python script, `tools/citygen/oq9_enumerate.py`, carrying a minimal copy of S1/S2/S4/S5 (model documented in its docstring), with `selftest` (27 checks: PCG32 known answers, AC2 fixtures, determinism, 200 rows per policy, histograms sum to 200, verdict recomputed from the CSV equals the report's word, exactly 0.30 passes, no lower `FinalCandidatesMinCount` proposed). Run: `python tools/citygen/oq9_enumerate.py run` then `selftest` (about 17 s).

- AC1 (`FinalPool.Enumeration200`): MET by the script (200 skeletons x 2 policies x 11 scenarios in the CSV; histogram, fraction >= 6, (31, 31) margin distribution in the report). C++ Automation wrapper DEFERRED.
- AC2 (`FinalPool.ArithmeticFixtures`): MET by the script (6 courts and 0.48 m at the mean fixture; 5 when (31, 31) moves 2 m outward in x). Finding: a diagonal 2 m outward move keeps 6 on the grid. C++ wrapper DEFERRED.
- AC3 (`FinalPool.LeverReport`): MET. Verdict **CONCERNS** (baseline lower policy 15/200 = 0.075; jitter 0 reaches 0.34, start-cell radius 16 m + jitter 0 reaches 0.615). Report and CSV: `production/qa/spikes/2026-10-25_citygen-oq9-final-court-enumeration.md` and `.csv`. C++ wrapper DEFERRED.
- DEFERRED (engine run pending): port `CityGenFinalPoolProbe` and the three Automation tests to `Source/StealthGameplay/Private/Tests/CityGen/` once stories 003, 005 and 006 land; the committed CSV is the oracle they must reproduce.

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-003-integer-stage-streams-and-canonical-iteration.md`, `story-005-grid-model-s1-frame-and-f1-walkable-target.md`, `story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md`, `story-007-f7-skeleton-fairness-pre-check.md`
- Unlocks: `story-010-s4-courts.md`, `story-011-s5-zones-anchors-and-start-cells.md`, `story-022-g-pre-predicates-b-and-role-counts.md`, `story-038-role-prior-and-lsc-batch-audits.md`
