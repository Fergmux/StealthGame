# Story 039: G-PRE false-pass rate and guard-band sign

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: spike R4 (ADR-0014 verification item 1, art bible R4; `level-socket-contract/story-011-spike-r4-graybox-navmesh-timing.md`) has not reported; the navmesh agent, cell and tile values it recommends determine the navmesh path lengths this story compares against the grid. Story 011 (LSC) is Ready and releases this one.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: G-PRE uses the same predicate code as LSC Validate behind a grid backend plus guard bands, because grid and navmesh path lengths differ by 5-8%; the first 50-seed batch measures the sign of the grid-versus-navmesh difference, and a side that never binds may then be set to 0. Guard changes are batch-time actions, never runtime ones.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; navigation)
**Engine Notes**: Needs a real server navmesh per spawned candidate (agent size, cell and tile values from spike R4) and LSC Validate. A nightly job on the reference PC.

**Control Manifest Rules (this layer)**:
- Required: Every V-* predicate exists once; the grid and world backends share predicate code — source: ADR-0014
- Forbidden: Never change a guard value at runtime or automatically; the report recommends, a data change follows review — source: GDD R-G5.2, ADR-0014
- Guardrail: a 1,000-candidate run is a nightly job, not per-commit

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G10, R-G5.2), scoped to this story:*

- [ ] **AC1** (AC-G10, `GPre.FalsePassRate`): **GIVEN** 1,000 spawned candidates that passed G-PRE **THEN** LSC Validate rejects <= 15% (p_v >= 0.85, `CityGenGPreGuardBandPct` at default).
- [ ] **AC2** (R-G5.2, `GPre.GuardSignReport`): **GIVEN** the first 50-seed batch **THEN** the grid-versus-navmesh path difference is recorded per predicate with its sign; a guard-band side binds when removing it flips at least one predicate's pass/fail on at least one batch candidate; a side that never binds in the first 50-seed batch is flagged as a candidate to set to 0 (recommendation only); a p_v below 0.85 names the batch-time action (raise `CityGenGPreGuardBandPct` or fix backend drift).

---

## Implementation Notes

*Derived from GDD R-G5.2 and F6, and the QL-STORY-READY review:*

Run 1,000 candidates that passed G-PRE through spawn, the real navmesh build and LSC Validate at the default guard keys (`CityGenGPreGuardBandPct` 4, `CityGenGPrePathGuardPct` 8); count Validate rejections: <= 150 passes, 151 fails; only G-PRE-passing candidates count toward the 1,000. For AC2, re-evaluate each guard-banded predicate with each band side set to 0 as a counterfactual (the lower or upper edge of a ratio predicate's band, and the raised floor or lowered ceiling of an absolute predicate) on the first 50-seed batch's candidates, count flips per side, and write a report listing the path difference and its sign per predicate. The test changes no configuration. p_v exactly 0.85 passes with no action line. If p_v < 0.85, the report names the action: raise the guard band or fix predicate-backend drift (never a runtime adaptation). The report goes to `production/qa/evidence/citygen/` and the CSV beside it.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-020-g-pre-grid-measure-backend-and-guard-bands.md`: the backend and guard constants.
- `story-037-batch-harness-and-f4-f6-audits.md`: the batch harness this runs on.
- Changing the guard keys: a data change after review (not part of this story).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/GPreFalsePassRateNightly.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenGuardCounterfactual.cpp` (non-Shipping)
- `production/qa/evidence/citygen/gpre-false-pass-and-guard-sign-[date].md` and `.csv` (the report)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC1 ADEQUATE; AC2 revised and ADEQUATE; both DEFERRED until nav timing is known from R4).*

- **AC1** `GPre.FalsePassRate`
  - Given: 1,000 candidates that passed G-PRE, spawned with nav built, at default guard band (4 points) and path guard (8%). This is a nightly job.
  - When: LSC Validate runs on each.
  - Then: <= 150 are rejected (p_v >= 0.85).
  - Edge cases: 150 passes and 151 fails. Only G-PRE-passing candidates count toward the 1,000.
- **AC2** `GPre.GuardSignReport`
  - Given: the first 50-seed batch with LSC Validate results (needs R4 nav).
  - When: each guard-banded predicate is re-evaluated with each band side set to 0, as a counterfactual.
  - Then: the report lists the path difference and its sign per predicate. A side whose removal flips 0 candidates is flagged "candidate to set to 0". The test changes no config. If p_v < 0.85 the report names the action (raise `CityGenGPreGuardBandPct` or fix backend drift).
  - Edge cases: "side" is the lower or upper edge of a ratio predicate's band. Flips are counted per candidate. p_v == 0.85 passes with no action line.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the nightly test plus the committed report in `production/qa/evidence/citygen/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-020-g-pre-grid-measure-backend-and-guard-bands.md`, `story-028-navigation-integration-water-stairs-client-none.md`, `story-037-batch-harness-and-f4-f6-audits.md`; spike report: `level-socket-contract/story-011-spike-r4-graybox-navmesh-timing.md`
- Unlocks: `story-042-attempt-time-budget-and-t-cand-measurement.md` (acceptance rate inputs)
