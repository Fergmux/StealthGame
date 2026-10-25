# Story 038: Role-prior and LSC batch audits

> **Epic**: Procedural City Generator
> **Status**: Blocked — BLOCKED: the OQ9 / C-W24 final-court enumeration (story 008) has not reported; AC3 (every accepted layout has >= 6 final-eligible courts, none in the temple ring) and AC2's axis-corner share cannot be judged until the S4 anchor-jitter rule and start-cell radius are fixed. Story 008 is Ready and releases this one.
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0008 for pre-registered priors and thresholds)
**ADR Decision Summary**: Roles are bounded by LSC `RoleShareMaxPct`, prefix ambiguity and the Generator's batch audits (single-metric final prior <= 30%, axis-corner share <= 60%, mirror assignments <= 15%); the audits protect Pillar 1 (no generated structure identifies a role) and are pre-registered with checked-in thresholds before data is collected.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014)
**Engine Notes**: The audits run on a 50-seed batch of accepted layouts from the pure core plus LSC Selection draws (the Selection enumeration is the LSC's). A p >= 0.05 bar fails about 5% of the time under the null: freeze the first seed once and diagnose on failure rather than re-picking seeds.

**Control Manifest Rules (this layer)**:
- Required: Pre-registered priors and thresholds are checked in beside the tools before the data is collected — source: ADR-0008
- Forbidden: Never reseed to pass an audit; never put a role in the Generator — source: ADR-0008, ADR-0014
- Guardrail: nightly batch cost only

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G34, R-G11.4), scoped to this story:*

- [ ] **AC1** (AC-G34 first clause, `Batch.LscAudits`): **GIVEN** a 50-seed batch **THEN** the LSC batch audits pass (bush correlation p >= 0.05, emergence uniformity p >= 0.01).
- [ ] **AC2** (AC-G34 second clause, `Batch.RolePrior`): **GIVEN** the batch **THEN** for each single public court metric (size, sun-pool area, distance to centre, ingress count) the top-ranked court is the drawn final in <= 30% of seeds and neither axis corner holds > 60% of drawn finals.
- [ ] **AC3** (AC-G34 third clause, `Batch.FinalPoolAndMirror`): **GIVEN** the batch **THEN** every accepted layout has >= `CityGenFinalEligibleTargetCount` final-eligible courts (none in the temple ring) and <= `CityGenMirrorAssignMaxPct` (15%) of drawn assignments are mirror (+-1 c transpose) prelim pairs.

---

## Implementation Notes

*Derived from GDD R-G11.4 and the QL-STORY-READY review:*

The role-prior audit replaced the old "temple ring vs rest <= 50%" class audit, which corner zones make unsatisfiable because no ring court is final-eligible (S4). Tie-break for "top-ranked court by a metric": canonical order (lowest SocketId). 30% of 50 seeds is 15 (15 passes, 16 fails). Mirror assignments: drawn assignments where the two teams' prelim courts are exact or +-1 c transposes of each other (the LSC Selection draw uses the skeleton symmetry transform from `FLayoutCandidate`, story 017; LSC R9.3 also rejects mirror-intercept combos per layout, so the audit confirms the residual rate); the denominator is drawn assignments across the batch. The LSC batch audits (bush correlation, emergence uniformity) are LSC-owned tests run here on Generator output; freeze the first seed of the batch once. Seeds come from a committed list; thresholds are pre-registered in the same file as story 037's.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-037-batch-harness-and-f4-f6-audits.md`: the harness and the F4/F6 audits.
- `story-022-g-pre-predicates-b-and-role-counts.md`: the per-candidate role-count pre-check.
- LSC Selection and mirror-intercept rejection: `level-socket-contract/story-021-selection-combos-and-fairness.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenRolePriorAudit.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/LscAuditsTest.cpp` (class `FCityGenBatchLscAuditsTest`, name `StealthGame.CityGen.Batch.LscAudits`)
- `Source/StealthGameplay/Private/Tests/CityGen/RolePriorTest.cpp` (class `FCityGenBatchRolePriorTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/FinalPoolAndMirrorTest.cpp` (class `FCityGenBatchFinalPoolAndMirrorTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Batch.LscAudits`
  - Given: the fixed 50-seed batch, with the first seed frozen in advance.
  - When: the LSC bush-correlation and emergence-uniformity audits run.
  - Then: bush correlation p >= 0.05 and emergence uniformity p >= 0.01.
  - Edge cases: a p >= 0.05 bar fails about 5% of the time under the null. Freeze the first seed once, and diagnose on failure rather than re-picking seeds.
- **AC2** `Batch.RolePrior`
  - Given: the batch with drawn finals and the four public court metrics (size, sun-pool area, distance to centre, ingress count).
  - When: each metric's top-ranked court is compared with the drawn final, and corners are tallied.
  - Then: for each metric the top-ranked court is the drawn final in <= 15 of 50 seeds. Neither axis corner holds more than 60% of drawn finals.
  - Edge cases: a tie-break rule is needed (canonical order). 15 passes and 16 fails.
- **AC3** `Batch.FinalPoolAndMirror`
  - Given: the batch.
  - When: final-eligible courts and drawn assignments are tallied.
  - Then: every layout has >= `CityGenFinalEligibleTargetCount` final-eligible courts, none in the temple ring. Mirror prelim pairs are <= 15% of drawn assignments.
  - Edge cases: the denominator is drawn assignments across the batch.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-008-oq9-final-court-enumeration-script.md` (report), `story-022-g-pre-predicates-b-and-role-counts.md`, `story-037-batch-harness-and-f4-f6-audits.md`; cross-epic `level-socket-contract/story-021-selection-combos-and-fairness.md`
- Unlocks: `story-044-tier-g1-validation-visual-tests-and-fairness-playtest.md`
