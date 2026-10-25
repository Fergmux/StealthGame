# Story 037: Batch harness and the F4/F6 audits

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0008 for statistical seed sets and gate wiring)
**ADR Decision Summary**: Batch audits run on 50 seeds per Generator change (LSC acceptance >= 34/50, F3, F4, F6, art 6.10 tests 4, 5 and 7); statistical analyses use pre-registered priors and thresholds checked in beside the tools before the data is collected; lints and analysis scripts emit observations only, pass/fail lives in the gate script.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: The batch runs the pure core (no world) for the grid audits; acceptance on attempt 0 needs LSC Validate on the navmesh (editor world or commandlet). Seeds come from a committed `constexpr` list, never a file read at test time.

**Control Manifest Rules (this layer)**:
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script; pre-registered priors and thresholds are checked in beside the tools — source: ADR-0008
- Forbidden: Never reseed or skip to pass an audit; never use wall-clock as a pass predicate — source: ADR-0008
- Guardrail: a 50-seed batch fits the nightly job; the per-commit gate runs only the cheap checks

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G11.4, AC-G26 second clause, AC-G29, AC-G31, F3 batch audit, UI Requirements), scoped to this story:*

- [ ] **AC1** (R-G11.4, `Batch.HarnessCsv`): **GIVEN** `stealth.citygen.batch <N> <FirstSeed>` **THEN** the CSV has a fixed header and one row per seed; each test result (F3, F4, F6, art 6.10 tests 4, 5, 7) is a column with pass/fail against its art-bible or GDD threshold; the run returns non-zero if a BLOCKING test fails; two runs of the same N and first seed give byte-identical CSVs.
- [ ] **AC2** (AC-G29 and AC-G26, `Formula.LayoutDiversity`): **GIVEN** 50 accepted layouts **THEN** p95 J* over the 8 symmetries is <= `CityGenJaccardP95MaxPct` (60%; ADVISORY until calibrated at baseline + 3 sigma from a 200-seed run) and every accepted layout's N_walk is inside its F1 band.
- [ ] **AC3** (AC-G31, `Formula.AcceptanceRate`): **GIVEN** a 50-seed batch **THEN** >= 34 seeds are accepted on attempt 0 (LSC D8), the design target >= 39/50 is advisory, and the facade entropy audit holds H(s_i+1 | s_i) >= 0.55 x log2 K over >= 5,000 transitions.

---

## Implementation Notes

*Derived from GDD R-G11.4, F3, F4, F6 and the QL-STORY-READY review:*

`stealth.citygen.batch` (non-Shipping, with the story 036 commands) writes a CSV to `Saved/CityGen/`. The CSV carries no wall-clock or timing columns and no timestamps, so two runs are byte-identical (state that in the header spec). Mark in the story which of the six tests are BLOCKING (F3 per run and acceptance are BLOCKING; F4 is ADVISORY until calibrated and must not fail the exit code). F4: `J*(a, b) = max over g in G of |M_a intersect g(M_b)| / |M_a union g(M_b)|` on walkable masks centred and cropped to the smaller grid over the 8 grid symmetries; the batch passes when the 95th percentile over all 1,225 pairs of 50 seeds is <= `CityGenJaccardP95MaxPct` (60% default, provisional; set at baseline + 3 sigma from a 200-seed simulation before it blocks). F1 band (AC-G26 second clause): every accepted layout's `N_walk` inside its band (T = 2,007 / 2,153 / 2,304 at N = 62 / 64 / 66). F6: >= 34 of 50 accepted on attempt 0 (33 fails, 34 passes; design target >= 39 is advisory; the Generator design target is `p` >= 0.78). Entropy: pooled `H(s_{i+1} | s_i) >= EntropyFloor x log2 K` with `EntropyFloor` = `CityGenEntropyRatioMinPct` 55%, over >= 5,000 transitions; with fewer than 5,000 the audit extends the batch by whole 50-seed blocks rather than failing (the GDD does not say; flagged). Pre-register thresholds in a checked-in file beside the tool before collecting data (ADR-0008 section 9).

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-038-role-prior-and-lsc-batch-audits.md`: the role-prior and LSC batch audits (blocked on story 008).
- `story-039-g-pre-false-pass-rate-and-guard-band-sign.md`: p_v and guard sign (blocked on R4).
- `story-027-instance-entry-and-shape-budget-audit.md`: the budget audits.
- `story-036-developer-commands-and-debug-overlay.md`: `stealth.citygen.run` and the overlay.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenBatchAudit.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenBatchAudit.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenConsoleCommands.cpp` (`stealth.citygen.batch`)
- `Source/StealthGameplay/Private/Tests/CityGen/BatchPriors.h` (pre-registered thresholds)
- `Source/StealthGameplay/Private/Tests/CityGen/BatchHarnessCsvTest.cpp` (class `FCityGenBatchHarnessCsvTest`, name `StealthGame.CityGen.Batch.HarnessCsv`)
- `Source/StealthGameplay/Private/Tests/CityGen/LayoutDiversityTest.cpp` (class `FCityGenFormulaLayoutDiversityTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/AcceptanceRateTest.cpp` (class `FCityGenFormulaAcceptanceRateTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC2 and AC3 ADEQUATE; AC1 revised and ADEQUATE).*

- **AC1** `Batch.HarnessCsv`
  - Given: `stealth.citygen.batch 50 <FirstSeed>`, run twice.
  - When: both runs finish.
  - Then: each CSV in `Saved/CityGen/` has the same fixed header and 50 rows. F3, F4, F6 and art 6.10 tests 4, 5, 7 each have a pass/fail column. The exit code is non-zero if any BLOCKING test fails. The two CSVs are byte-identical.
  - Edge cases: the CSV carries no wall-clock or timing columns and no timestamps. The story marks which of the six tests are BLOCKING. F4 is ADVISORY until calibrated and must not fail the exit code.
- **AC2** `Formula.LayoutDiversity`
  - Given: a fixed 50-seed batch with accepted layouts.
  - When: J* over the 8 symmetries and N_walk are computed.
  - Then: p95 J* <= 60% (`CityGenJaccardP95MaxPct`), reported as ADVISORY until the calibrated flag is set. Every N_walk is inside its F1 band (BLOCKING).
  - Edge cases: band checks use T = 2,007 / 2,153 / 2,304 for N = 62 / 64 / 66.
- **AC3** `Formula.AcceptanceRate`
  - Given: a fixed 50-seed batch plus the facade transitions.
  - When: acceptance and pooled entropy are computed.
  - Then: >= 34 seeds are accepted at attempt 0 (BLOCKING). 39 is advisory. H(s_{i+1} | s_i) >= 0.55 x log2 K over >= 5,000 transitions.
  - Edge cases: 33 fails and 34 passes. With fewer than 5,000 transitions the batch is extended by whole 50-seed blocks (the author's reading, flagged).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-021-g-pre-predicates-a.md`, `story-016-s11-repairs-and-occurrence-rule.md`, `story-031-golden-seeds-and-bump-rule.md`, `story-036-developer-commands-and-debug-overlay.md`; cross-epic `level-socket-contract/story-021-selection-combos-and-fairness.md` (acceptance on attempt 0)
- Unlocks: `story-038-role-prior-and-lsc-batch-audits.md`, `story-039-g-pre-false-pass-rate-and-guard-band-sign.md`
