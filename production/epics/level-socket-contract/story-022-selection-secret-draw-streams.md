# Story 022: Selection draw — secret streams and the two-stage uniform draw

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-009`
*(At write time: TR-lsc-009 = Server-only query API for site assignment and team spawns)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Every runtime random choice comes from Session's server sub-seeds `ServerSubSeed(Domain, LayoutAttempt)` (derived from the 128-bit `ServerSecret`, independent of `Seed`), drawn counter-based by rejection sampling; Selection is a uniform two-stage draw (a usable zone pair uniformly, then a combo uniformly within it), never the "best" combo.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No engine API; uses `FSecretStream(GetServerSubSeed(ObjectiveSites, LayoutAttempt), Tag("LSC_SITE"))` from Session & Travel (ADR-0006). The secret override exists only in non-Shipping builds.

**Why this story**: The draw is what makes roles unguessable yet unbiased. Read first: R9 items 1 and 4, D8, AC-24, AC-26.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use the ADR-0006 shapes and stream tags `LSC_SITE`, `LSC_PLCE`, `LSC_ZONE`, `LSC_EMRG`, `LSC_RSPN` (ADR-0014)
- Forbidden: `%` on a draw, floats in a ranged draw, `FMath::Rand`/`FRandomStream` in gameplay code; logging sub-seed values outside the Event Log header (ADR-0006, ADR-0014)
- Guardrail: hidden draws well under 0.05 ms/frame (ADR-0006)

---

## Acceptance Criteria

*From GDD AC-24, AC-26 and R9.4 (revised after QL-STORY-READY: Shipping clause moved to a CI check, AC-3 given a sample size), scoped to this story:*

- [ ] GIVEN the graybox and a pinned `ServerSecret`, WHEN `Seed` varies, THEN `FSiteAssignment` is unchanged; GIVEN 200 pinned secrets, THEN at least 12 distinct assignments appear and two pinned secrets known to differ give different used zones or live sites; draws in one domain are unchanged when another domain's call count changes; the secret override exists only in non-Shipping builds (verified by a CI Shipping compile/symbol check) (`Selection.SecretStreamsOnly`)
- [ ] GIVEN exactly 12 passing combos and 12,000 pinned-sub-seed draws, THEN every combo appears and chi-square < 24.725 (df 11); GIVEN a scripted RNG sweeping its whole range, THEN bucket counts are exactly equal and values >= floor(2^n / 12) x 12 are rejected (`Selection.UniformDraw`)
- [ ] GIVEN two usable zone pairs with 20 and 4 passing combos and 10,000 pinned draws, THEN the pair split is uniform (chi-square df 1 < 6.635) and within each pair the combos are uniform (chi-square at df = combos - 1, p = 0.01: df 19 < 36.191, df 3 < 11.345) (`Selection.TwoStageUniform`)

---

## Implementation Notes

- Stage 1: pick a used zone pair uniformly among the usable pairs that have at least one passing combo (Generator CR 1d), so a pair with more combos is not favoured. Stage 2: a combo uniformly among that pair's passing combos. Rejection sampling, no `%`; a pair with 0 passing combos is never drawn.
- `ObjectiveSites` and `PlayerPlacement` are drawn per `LayoutAttempt`; `EmergencePick` and `RespawnPick` are counter-as-Attempt per request (stories 030, 033). The sub-seed values are logged only in the server-only Event Log header (story 004); the three new domains are a change request to Session F5 (cross-epic).
- A test override of `ServerSecret` exists only in non-Shipping builds; the CI Shipping check is a Config/Data item recorded in the smoke check when CI exists (DEFERRED).
- Independence test: change the call count of another domain between draws and assert the Selection draw is unchanged.

---

## Out of Scope

- Story 021: combo enumeration and V-FAIR; story 023: server query API and leak tests; Session's stream implementation (`production/epics/session-travel/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/SelectionDraw.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/SelectionDrawTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Selection.SecretStreamsOnly`
  - Given: the graybox and a pinned ServerSecret; 200 pinned secrets.
  - When: Seed varies; domain call counts vary.
  - Then: `FSiteAssignment` unchanged across Seeds; >= 12 distinct assignments over 200 secrets; two known-different secrets give different zones or sites; draws unchanged when another domain's call count changes.
  - Edge cases: the Shipping clause is DEFERRED to a CI Shipping compile/symbol check.
- **AC-2** `Selection.UniformDraw`
  - Given: exactly 12 passing combos; a scripted RNG sweeping its range.
  - When: 12,000 pinned draws; sweep.
  - Then: every combo appears and chi-square < 24.725 (df 11); sweep gives exactly equal buckets and values >= floor(2^n/12) x 12 rejected.
  - Edge cases: no `%`.
- **AC-3** `Selection.TwoStageUniform`
  - Given: two usable pairs with 20 and 4 passing combos; 10,000 pinned draws.
  - When: drawn.
  - Then: pair split uniform (chi-square df 1 < 6.635, about 5,000 each); within pair A chi-square < 36.191 (df 19), within pair B < 11.345 (df 3).
  - Edge cases: a pair with 0 passing combos is never drawn.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/SelectionDrawTest.cpp` — `StealthGame.Layout.Selection.SecretStreamsOnly`, `.UniformDraw`, `.TwoStageUniform` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-021-selection-combos-and-fairness.md`
- Cross-epic: `production/epics/session-travel/EPIC.md` (`GetServerSubSeed`, `FSecretStream`, SipHash/PCG32 vectors)
- Unlocks: `production/epics/level-socket-contract/story-023-server-query-api-and-leak-blindness.md`, `production/epics/level-socket-contract/story-029-start-placement-same-function-zone-fill.md`
