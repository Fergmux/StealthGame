# Story 024: Tier C fixture proves the allocator and validators are tier-independent

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: Tier A, B and C are all one merged single-slot skeletal mesh per model or accessory variant; the allocator, `AppearanceId`, replication and every rule are identical across tiers, only the pool table's content changes, so Tier C (one shared body, Meshy heads and garments) is a fully supported fallback.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); fixtures are in memory
**Engine Notes**: No engine API beyond the pool descriptor types.

**Why this story**: The Meshy retarget spike may force Tier C (GDD OQ-1; the first-playable report's Meshy arms are 17 % short): this story proves now that nothing in the allocator or validators depends on the tier. Read GDD CP-5 and AC-CP-29.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One merged single-slot mesh per model/accessory variant regardless of tier (ADR-0012)
- Forbidden: Never build appearances at run time (leader-pose, merge, Mutable)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-29:*

- [ ] **AC-1** GIVEN a Tier C fixture pool (one shared body, 12 head/garment rows built in memory), THEN AC-CP-01 (counts), AC-CP-21 (swatch rule) and AC-CP-22 (shape and material rules, story 007's descriptor fixtures) pass unchanged against it (test `StealthGame.CharacterPool.Pool.TierIndependence`).
- [ ] **AC-2** GIVEN the allocator, pool validator and apply code, THEN `LINT-CPA-NoTierRef` finds no tier identifier (a tier change is content only) and a seeded identifier fails the lint.

---

## Implementation Notes

- The fixture is a factory function in the test (no shared mutable state). Comments are excluded from the lint by rule.
- Register `LINT-CPA-NoTierRef` with story 042's harness.

**Prototype disposition**: none.

---

## Out of Scope

- Story 054: the real Tier B or Tier C content swap

---

## Files

- `Source/StealthGameplay/Private/Tests/CharacterPool/TierIndependenceTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead).*

**Test: Tier C fixture**
- Given: a fixture pool with one shared body mesh and 12 head and garment rows.
- When: the AC-CP-01 counts, AC-CP-21 swatch validation and AC-CP-22 shape rules run.
- Then: all three pass with no code change.
- Edge cases: the fixture is built in memory.

**Test: no tier references**
- Given: the allocator, pool validator and apply code.
- When: the lint scans for tier identifiers.
- Then: zero matches; a seeded identifier fails the lint.
- Edge cases: comments are excluded.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/TierIndependenceTest.cpp` — automation test `StealthGame.CharacterPool.Pool.TierIndependence` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-007-pool-shape-and-material-rules-validator.md`, `production/epics/character-pool-appearance/story-010-deck-sizing-and-floor-policy-f1-f3.md`
- Unlocks: `production/epics/character-pool-appearance/story-054-meshy-pool-swap-tier-b-or-c.md`
