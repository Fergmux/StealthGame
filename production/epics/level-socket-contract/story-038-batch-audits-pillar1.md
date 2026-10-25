# Story 038: Batch audits — bush and emergence uncorrelated with roles

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-008`
*(At write time: TR-lsc-008 = Pillar-1 placement constraints (no area type for spawns/objectives))*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Batch audits run on a 50-seed batch (graybox: 50 pinned secrets) and on each graybox revision rather than per layout, because a per-layout check would push the Generator into a visible pattern: bush placement uncorrelated with sun pools, sites, zones or team, and per-socket emergence use near-uniform against area share.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Offline statistical test on the validated graybox with pinned secrets and a pinned permutation seed; no engine API beyond loading the graybox data.

**Why this story**: Bush placement and emergence use must carry no information about where objectives or teams are (R10 rule 1). Read first: R7 batch audits, R10, D9, AC-40.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Selection draws are counter-based from the secret streams (ADR-0014)
- Forbidden: Never fork predicate logic; never log assignments outside the Event Log header (ADR-0014)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-40 (revised after QL-STORY-READY: permutation count, df and area-spread threshold defined), scoped to this story:*

- [ ] GIVEN a 50-seed batch (graybox: 50 pinned secrets), THEN the 95% CI of (observed mean bush distance to sites, zones and sun pools minus the random-walkable-cell baseline) lies within +/- 3 m and a permutation test (10,000 permutations, pinned seed) gives p >= 0.05 (`Leak.BatchAudits`)
- [ ] THEN per-socket emergence use against area share gives chi-square below the p = 0.01 critical value at df = (area types - 1) = 3 (11.345), and each area type holds at least one used zone, site, bush and socket in at least X of the 50 seeds (X set by the level designer and recorded before the first run) (`Leak.BatchAudits.AreaSpread`, proposed)
- [ ] THEN the audit runs on each graybox revision and is BLOCKING for it, ADVISORY for the Generator until it exists (CI configuration check)

---

## Implementation Notes

- Baseline: the random-walkable-cell distance distribution (uniformly random walkable cells). Observed: the graybox's fixed bush set against the 50 drawn assignments' sites, zones and sun pools; the pinned permutation seed makes the result deterministic.
- Emergence use: per-socket counts over simulated emergences (story 030 service) vs area share; df follows the area-type count.
- The result is only meaningful once the graybox exists (story 028); until then run on the synthetic fixture and mark real-map evidence pending.
- Gate level: BLOCKING on graybox revisions (`testing.strict.logic` default), ADVISORY for the Generator (the Generator epic registers its own 50-seed batch).

---

## Out of Scope

- Story 039: packaged-build leak scan; the Generator's batch (`production/epics/procedural-city-generator/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Tests/Layout/BatchAuditsTest.cpp`
- `Source/StealthGameplay/Private/Core/Layout/BatchAudits.cpp`
- `production/qa/evidence/level-socket/batch-audit-X-values.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Leak.BatchAudits`
  - Given: 50 pinned secrets on the graybox; the random-walkable-cell baseline.
  - When: mean bush distance to sites, zones and sun pools is computed; a 10,000-permutation test on a pinned seed runs.
  - Then: the 95% CI of (observed - baseline) lies within +/-3 m; permutation p >= 0.05.
  - Edge cases: the pinned seed makes the result deterministic.
- **AC-2** `Leak.BatchAudits.AreaSpread` (proposed)
  - Given: the 50-seed batch.
  - When: per-socket emergence use is tallied by area type.
  - Then: chi-square < 11.345 (df 3); each area type holds >= 1 used zone, site, bush and socket in >= X of 50 seeds.
  - Edge cases: X is recorded before the first run.
- **AC-3** CI config check
  - Given: CI configuration.
  - When: the audit job gate level is read.
  - Then: BLOCKING on graybox revisions, ADVISORY for the Generator.
  - Edge cases: Generator absent.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/BatchAuditsTest.cpp` — `StealthGame.Layout.Leak.BatchAudits`, `.AreaSpread` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-023-server-query-api-and-leak-blindness.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`, `production/epics/level-socket-contract/story-030-emergence-request-clear-check-uniform-pick.md`
- Unlocks: None
