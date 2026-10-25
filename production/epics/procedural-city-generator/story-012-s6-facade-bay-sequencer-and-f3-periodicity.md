# Story 012: S6 Facade bay sequencer and F3 periodicity

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: S1-S12 are pure integer stages; the bay sequencer is a constrained random walk over the recipe's bay types drawn from the S6 integer stream, enforcing the art-bible 6.7 anti-repetition rules by construction.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. F3 is integer arithmetic on bay-type sequences; the entropy audit (batch-only) is story 037.

**Control Manifest Rules (this layer)**:
- Required: Pure data, integer streams, canonical iteration — source: ADR-0014, ADR-0006
- Forbidden: Never choose a module because of a socket's role (Pillar 1: S6 reads no role data) — source: GDD Edge Cases, ADR-0014
- Guardrail: part of the 0.12 s candidate budget

S6 gives every building-cell edge facing a walkable cell one bay, sequenced along its facade run. The rules come from art bible 6.7: identical runs <= 6 bays (12 m), no repeated short pattern, door gaps 2-5 bays with no gap value three times in a row, door bays never adjacent.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G28, R-G6.2, R-G4 S6, F3), scoped to this story:*

- [ ] **AC1** (AC-G28, `Formula.PeriodicityExcessVectors`): **GIVEN** the sequences ABABAB, ABABABAB, AABAABAAB and AAAAAAA **THEN** PE is 1.0 pass, 1.33 fail, 1.0 pass and 7/6 (1.1667 +-0.005) fail, and the door-gap sequence is scored with cap_1 = 2: (3,3,3) PE 1.5 fail, (3,3,4) PE 1.0 pass, (3,4,3,4,3,4) PE 1.0 pass.
- [ ] **AC2** (R-G6.2, `Facade.SequencerRules`): **GIVEN** 1,000 facade runs **THEN** every run has PE <= 1 (RunCap 6, PairCap 3, tau_max 4), door gaps are 2-5 bays with no gap value three times in a row, door bays are never adjacent, and free wall ends are M05 or a pilaster.
- [ ] **AC3** (R-G4 S6, `Facade.StoreysAndSingleVariant`): **GIVEN** G0 data with one variant per module **THEN** storeys including the ground follow the recipe (Residential <= 2, Market exactly 1, Causeway <= 1), the sequencer still produces PE <= 1 runs from rhythm alone, and joints between unlike bays fall on a quoin line.

---

## Implementation Notes

*Derived from GDD F3 and R-G6.2:*

`PE(run) = max over tau = 1..tau_max of C_tau / cap_tau`, where `C_tau` is the longest stretch of bays with `s_i = s_{i+tau}` plus tau; `cap_1 = RunCap` (`CityGenFacadeIdenticalRunMaxCount`, 6), `cap_tau = PairCap * tau` for tau >= 2 (`CityGenFacadePeriodRepeatMaxCount`, 3); a run passes when PE <= 1; the same score is applied to the door-gap sequence with `cap_1 = 2`. Examples: ABABAB gives C_2 = 4 + 2 = 6, PE 6/6 = 1.0; ABABABAB gives 8/6 = 1.33; AABAABAAB gives C_3 = 9, PE 9/9 = 1.0; AAAAAAA gives C_1 = 7, PE 7/6 = 1.1667 (the GDD AC text rounds to 1.17). Door-gap vectors: (3,3,3) gives C_1 = 3 over cap 2 = 1.5; (3,3,4) gives 2/2 = 1.0; (3,4,3,4,3,4) gives C_2 = 4 + 2 = 6 over `PairCap * 2` = 6 = 1.0 (the door-gap tau >= 2 cap is assumed to use the same `PairCap`; confirm in review). When the walk draws a bay that would raise PE above 1 the sequencer re-draws it (a forced ABABABAB draw is re-drawn: the negative control). Test also at RunCap 4, the hard minimum.

The sequencer enforces F3, door gaps of 2-5 bays with no gap value three times in a row, door bays never adjacent (art 3.3) and no unique variant within 6 m of a socket (that last rule and the occurrence rule are enforced after the run by S11.4, story 016). Upper storey by recipe: storeys counted including the ground, Residential <= 2, Market exactly 1, Causeway <= 1 ("Causeway low" in the GDD is unquantified; flagged for the GDD owner). With one variant per module (G0) variety comes from door rhythm, storey presence and layout only. Free wall ends are M05 or a pilaster; joints between unlike bays fall on a quoin line (art 6.7).

Prototype replaced: the prototype's houses were identical modules on a grid; no sequencer exists.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-016-s11-repairs-and-occurrence-rule.md`: the occurrence rule, unique-variant-near-socket rule and AC-G12 over 1,000 seeds.
- `story-037-batch-harness-and-f4-f6-audits.md`: the entropy audit.
- `story-019-placement-bay-stacks-and-manifest-emission.md`: bay stacks as derived entries.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageFacades.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenFormulas.h` (F3 `PeriodicityExcess`)
- `Source/StealthGameplay/Private/Tests/CityGen/PeriodicityExcessVectorsTest.cpp` (class `FCityGenFormulaPeriodicityExcessVectorsTest`, name `StealthGame.CityGen.Formula.PeriodicityExcessVectors`)
- `Source/StealthGameplay/Private/Tests/CityGen/SequencerRulesTest.cpp` (class `FCityGenFacadeSequencerRulesTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/StoreysAndSingleVariantTest.cpp` (class `FCityGenFacadeStoreysAndSingleVariantTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after the revision of AC1 and AC3).*

- **AC1** `Formula.PeriodicityExcessVectors`
  - Given: ABABAB, ABABABAB, AABAABAAB and AAAAAAA, plus door-gap vectors with cap_1 = 2.
  - When: PE is computed.
  - Then: the results are 1.0 pass, 1.33 fail, 1.0 pass and 7/6 fail (compared as 1.1667 +-0.005). Door gaps: (3,3,3) 1.5 fail; (3,3,4) 1.0 pass; (3,4,3,4,3,4) 1.0 pass.
  - Edge cases: each score is the maximum over tau 1-4. The last vector is 1.0 from tau 2 (C = 4 + 2 over cap 6). The door-gap `PairCap` is assumed to be the same value, 3.
- **AC2** `Facade.SequencerRules`
  - Given: 1,000 facade runs, RunCap 6, PairCap 3, tau_max 4.
  - When: S6 runs.
  - Then: every run has PE <= 1. Door gaps are 2-5 with no value three times in a row, door bays are never adjacent, and free wall ends are M05 or a pilaster.
  - Edge cases: a forced ABABABAB draw is re-drawn to PE <= 1 (negative control). Also run at RunCap 4, the hard minimum. A run too short for two doors is handled.
- **AC3** `Facade.StoreysAndSingleVariant`
  - Given: G0 data with one variant per module.
  - When: S6 runs over 50 seeds.
  - Then: storeys including ground are Residential at most 2, Market exactly 1, Causeway at most 1 (Causeway on a canal-enabled fixture). Runs have PE <= 1 from rhythm alone. Joints between unlike bays fall on a quoin line.
  - Edge cases: a Market facade with a second storey fails. Causeway is vacuous at G0, so it runs on the canal-enabled fixture.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-009-s3-area-types.md`, `story-004-kit-tables-and-generator-kit-version.md`
- Unlocks: `story-016-s11-repairs-and-occurrence-rule.md`, `story-019-placement-bay-stacks-and-manifest-emission.md`
