# Story 007: F7 skeleton fairness pre-check

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: G-PRE and the stage pre-checks are integer-only pure functions on the cell grid and its node graph, so an unfair candidate is discarded before any facade, socket or geometry exists. The LSC owns fairness (D1, `ChainFairnessMaxMult` 1.10); the Generator pre-checks it tighter.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. Chain lengths are junction-to-junction sums on the skeleton node graph (centimetres, integer).

**Control Manifest Rules (this layer)**:
- Required: The Generator runs as a single cancellable worker task over pure data — source: ADR-0014
- Forbidden: Never fork predicate logic between the grid and world backends (the D1 chain function is the LSC's; this story calls the node-level form of it) — source: ADR-0014
- Guardrail: the pre-check is a few microseconds per candidate; it runs before S6

F7 rejects a skeleton whose two teams' chains differ by more than `CityGenSkeletonFairnessMaxMult` (1.06, under LSC's 1.10) before any fill is built, which is the cheapest rejection in the pipeline.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G32, F7), scoped to this story:*

- [ ] **AC1** (AC-G32, `Formula.SkeletonSkew`): **GIVEN** chain lengths (214, 224) and (205, 222) **THEN** R_skel is 1.047 (kept, <= 1.06) and 1.083 (rejected before fill, 0 facades built).
- [ ] **AC2** (F7, `Skeleton.ChainPreCheck`): **GIVEN** a skeleton node graph **THEN** C-tilde is computed on junction-to-junction lengths for the best-balanced court triple, the worst-case jitter skew is bounded by n_cut x delta x 2 m (16 m at n_cut 8, delta 1), and the R_skel histogram per chain-length tercile is written into the candidate metrics.

---

## Implementation Notes

*Derived from GDD F7:*

`R_skel = max(C0, C1) / min(C0, C1) <= SkelMax` where C0 and C1 are the D1 chain lengths for the two teams' best-balanced court triple computed on the node graph. Compare in integers (`max * 1000 <= min * 1060` style, or compare with a tolerance of 0.0005) so the boundary is exact: (250, 265) is exactly 1.06 and kept, (250, 266) is rejected. Worst-case jitter skew `DeltaC <= n_cut * delta * 2 m` is a reported bound, not a gate: 0 at delta 0, 16 m at (8, 1), 32 m at delta 2. The pre-check runs after S2 and before S3 so a rejected skeleton costs no fill. The histogram bins are the terciles of chain length across the batch (the candidate metrics carry the raw chain length and R_skel; the batch harness, story 037, bins them). If short chains dominate rejections the GDD's remedy is `CityGenSkeletonJitterCount` 0 on cuts crossed by chains under 150 m, never a higher `SkelMax`.

Prototype replaced: none (the prototype had no fairness notion beyond a logged reachability check).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md`: producing the skeleton and node graph.
- `story-022-g-pre-predicates-b-and-role-counts.md`: V-FAIR on the grid paths of the filled candidate.
- LSC D1 formula: `level-socket-contract/story-006-formulas-d1-to-d4.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenSkeletonFairness.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenSkeletonFairness.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/SkeletonSkewTest.cpp` (class `FCityGenFormulaSkeletonSkewTest`, name `StealthGame.CityGen.Formula.SkeletonSkew`)
- `Source/StealthGameplay/Private/Tests/CityGen/ChainPreCheckTest.cpp` (class `FCityGenSkeletonChainPreCheckTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Formula.SkeletonSkew`
  - Given: chain pairs (214, 224) and (205, 222), threshold 1.06.
  - When: R_skel is computed.
  - Then: 1.047 is kept. 1.083 is rejected before S6 with the facade-built counter at 0.
  - Edge cases: (250, 265) is exactly 1.06 and is kept. (250, 266) is rejected. Compare to +-0.0005.
- **AC2** `Skeleton.ChainPreCheck`
  - Given: a node-graph fixture with a known best-balanced court triple, plus 50 seeds.
  - When: the pre-check runs.
  - Then: C-tilde uses junction-to-junction lengths. Worst-case skew is at most n_cut x delta x 2 m, which is 16 m at (8, 1). The metrics hold a 3-bin tercile R_skel histogram summing to the candidate count.
  - Edge cases: the bound is 0 at delta = 0 and 32 m at delta = 2.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the two test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-006-s2-skeleton-lattice-widths-jogs-transpose-and-jitter.md`; cross-epic `level-socket-contract/story-006-formulas-d1-to-d4.md`
- Unlocks: `story-008-oq9-final-court-enumeration-script.md` (uses the node-level chain function), `story-009-s3-area-types.md`
