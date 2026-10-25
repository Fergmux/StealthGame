# Story 022: G-PRE predicates B: court, pool, bush, route, zones, fairness and role counts

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: the OQ9 / C-W24 final-court enumeration (story 008) has not reported; the role-count pre-check (>= `CityGenFinalEligibleTargetCount` final-eligible courts) is only meaningful once the report confirms the pool can reach 6 under jitter. Story 008 is Ready and releases this one.
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: G-PRE reuses the LSC predicate code behind the grid backend with guard bands; predicates with no grid form (V-BUSH-3 backdrop, V-POOL-40) are left to LSC Validate. "Final-eligible" is not placed as a role: it falls out of the distance predicates, and a role-count pre-check runs in G-PRE.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++, fixed-sun raycast quantised to cm)
**Engine Notes**: No engine API. D6 floors scale with the candidate's own A (V-ZONEFILL: 1,467 m^2 only at A = 8,800 m^2).

**Control Manifest Rules (this layer)**:
- Required: Every V-* predicate exists once; V-CLUE shared function — source: ADR-0014
- Forbidden: Never fork predicate logic; never lower the effective final-eligible floor below `FinalCandidatesMinCount` — source: ADR-0014, LSC D4
- Guardrail: part of the 0.12 s candidate budget

Predicate group B (GDD R-G5.3): V-COURT and V-POOL (fixed-sun raycast), V-BUSH-1/2, V-ROUTE and the S12 coverage target, V-TRAV, V-ZONE, and per candidate zone V-SEP, V-ZONEFILL and V-RESPAWN; then V-FAIR on grid paths with the S4 role-count pre-check (>= `CityGenFinalEligibleTargetCount` final-eligible, never below LSC `FinalCandidatesMinCount`; >= 3 per prelim stage; prefix ambiguity >= `FinalAmbiguityMinCount`).

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G5.3, R-G4 S4 and S5), scoped to this story:*

- [ ] **AC1** (S4 role-count pre-check, `GPre.RoleCountPreCheck`): **GIVEN** candidates with 5 and with 6 final-eligible courts **THEN** the 5-court candidate fails and the 6-court candidate passes (>= `CityGenFinalEligibleTargetCount`, never below `FinalCandidatesMinCount`), >= 3 per prelim stage and prefix ambiguity >= `FinalAmbiguityMinCount` are required, and V-FAIR runs on grid paths.
- [ ] **AC2** (R-G5.3 per zone, `GPre.ZonePredicates`): **GIVEN** each candidate zone **THEN** V-SEP (no line of sight, >= 70 m path, T_contact >= 15 s), V-ZONEFILL (A_z >= the D6 floor for the candidate's own A x 1.04; 1,467 m^2 only at A = 8,800 m^2), V-RESPAWN (E_R >= 4 on RespawnRegion = walkable cells <= 45 m grid path of the anchor) and V-ZONE are evaluated, each with a failing fixture.
- [ ] **AC3** (R-G5.3, `GPre.CourtPoolBushRouteTrav`): **GIVEN** fixtures **THEN** V-COURT and V-POOL (fixed-sun raycast), V-BUSH-1/2, V-ROUTE with the S12 coverage target and V-TRAV each fail by id, and V-BUSH-3 backdrop and V-POOL-40 are left to LSC Validate.

---

## Implementation Notes

*Derived from GDD R-G5.3 and R-G4 S4/S5:*

All checks go through the story-020 backend and the LSC predicate code; this story wires them into G-PRE with failure ids and fixtures. The S4 role-count pre-check: the effective floor is `max(CityGenFinalEligibleTargetCount, FinalCandidatesMinCount)` (so target 3, the hard minimum, still requires 6); >= 3 final-eligible per prelim stage; prefix ambiguity >= `FinalAmbiguityMinCount` (3). V-FAIR is the chain-ratio predicate with the guard (a ratio of 1.07 fails and 1.05 passes at the 1.06 guarded check). V-ZONEFILL: a fixture at A = 8,000 m^2 expects a different floor than the one at A = 8,800 m^2 (floor scales with the candidate's own A). V-SEP: nearest start cells >= 70 m by path (guarded 76 m), `T_contact` >= 15 s, no line of sight between start sets dilated 3 m; boundaries 75 m fails, 77 m passes, 14 s fails, 15 s passes. V-RESPAWN: E_R of 3 fails, 4 passes. A fixture violating only V-BUSH-3 or V-POOL-40 must pass G-PRE (negative control that those are left to Validate).

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-021-g-pre-predicates-a.md`: group A.
- `story-008-oq9-final-court-enumeration-script.md`: the feasibility report this story is blocked on.
- `story-038-role-prior-and-lsc-batch-audits.md`: the batch-level role-prior audits.
- LSC predicate definitions: LSC stories 016-019 and 021.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenGPrePredicatesB.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenRoleCountPreCheck.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/RoleCountPreCheckTest.cpp` (class `FCityGenGPreRoleCountPreCheckTest`, name `StealthGame.CityGen.GPre.RoleCountPreCheck`)
- `Source/StealthGameplay/Private/Tests/CityGen/ZonePredicatesTest.cpp` (class `FCityGenGPreZonePredicatesTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/CourtPoolBushRouteTravTest.cpp` (class `FCityGenGPreCourtPoolBushRouteTravTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `GPre.RoleCountPreCheck`
  - Given: synthetic candidates with 5 and 6 final-eligible courts.
  - When: G-PRE runs.
  - Then: the 5-court candidate fails and the 6-court candidate passes. A prelim stage with 2 courts fails and one with 3 passes. Prefix ambiguity below `FinalAmbiguityMinCount` fails. V-FAIR on grid paths fails at a ratio of 1.07 and passes at 1.05, with the 1.06 guard-band check.
  - Edge cases: target 3 (the hard minimum) still requires 6, because the effective floor is max(target, `FinalCandidatesMinCount`).
- **AC2** `GPre.ZonePredicates`
  - Given: per-zone fixtures.
  - When: G-PRE runs.
  - Then: each predicate has a failing fixture. V-SEP: a line of sight fails; a 75 m path fails and 77 m passes; T_contact of 14 s fails and 15 s passes. V-ZONEFILL: the D6 floor for the candidate's own A, x 1.04; 1,467 m^2 applies only at A = 8,800. V-RESPAWN: E_R of 3 fails and 4 passes, on cells within 45 m of the anchor. V-ZONE: its own failing fixture.
  - Edge cases: a fixture at A = 8,000 expects a different floor from the one at A = 8,800.
- **AC3** `GPre.CourtPoolBushRouteTrav`
  - Given: one fixture per predicate.
  - When: G-PRE runs.
  - Then: V-COURT, V-POOL (fixed-sun raycast), V-BUSH-1/2, V-ROUTE and V-TRAV each fail by id.
  - Edge cases: a fixture violating only V-BUSH-3 or V-POOL-40 passes G-PRE (negative control). V-ROUTE uses the S12 coverage target.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-008-oq9-final-court-enumeration-script.md` (report), `story-010-s4-courts.md`, `story-011-s5-zones-anchors-and-start-cells.md`, `story-015-s9-bushes-and-s10-props-role-blind.md`, `story-018-s12-crowd-route.md`, `story-020-g-pre-grid-measure-backend-and-guard-bands.md`
- Unlocks: `story-023-candidate-loop-work-unit-cap-and-metrics.md`, `story-038-role-prior-and-lsc-batch-audits.md`
