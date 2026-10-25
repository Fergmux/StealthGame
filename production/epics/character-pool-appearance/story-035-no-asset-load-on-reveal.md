# Story 035: No pool-asset load on reveal; a still-delayed pawn flags the round

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: spikes CPA-V2 and CPA-V3 (story 031) have not reported; zero loads and zero delayed proxies after warm-up are what they must prove in a cooked client.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation (budget context ADR-0022)
**ADR Decision Summary**: All pool entries are resident and warmed before the reveal, so a pawn's first draw never triggers an asset load; a pawn still PSO-delayed after warm-up is a defect logged as `PoolNotResident` and the round is excluded from Pillar-1 evidence.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012, ADR-0022)
**Engine Notes**: The package-load delegate hook counts loads; the PSO-delay seam (forcing one pawn's `IsPSOPrecaching` true after warm-up) comes from the harness module (`StealthTestHarness`); PSO precache is off under `GIsEditor` so the load test runs in a cooked or `-game` build.

**Why this story**: AC-CP-15c and the Edge Case "an appearance's assets are not yet resident when its first pawn spawns". Read GDD CP-18, AC-CP-15c and Edge Cases (Information leaks).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Clients warm all `A` pool rows; no gameplay code loads a pool asset after the front end (ADR-0012)
- Forbidden: Never load a pool asset from gameplay code after the front end
- Guardrail: pool textures at most 40 MB

---

## Acceptance Criteria

*From GDD AC-CP-15c and the Edge Case on residency:*

- [ ] **AC-1** GIVEN a hook on the async package-load delegate, THEN it counts zero pool-asset loads between the reveal and 10 s after it, and texture mips of all `A` pool entries (A = 24 case included) are resident at the reveal (test `StealthGame.CharacterPool.PIE.NoAssetLoadOnReveal`).
- [ ] **AC-2** GIVEN a harness seam that forces one pawn's `IsPSOPrecaching` true after warm-up (seeded), WHEN the reveal occurs, THEN `PoolNotResident` is logged and the round carries a readable flag excluding it from Pillar-1 evidence; with no seam active no flag is set.

---

## Implementation Notes

- Reuse the load counter written for the spike in story 031; make it a permanent harness utility.
- The exclusion flag is consumed by the Round Event Log and the AC-CP-25 analysis (story 056).

**Prototype disposition**: none.

---

## Out of Scope

- Story 032: the warm-up itself; Story 031: the measurement spike

---

## Files

- `Source/StealthTestHarness/Private/CharacterPool/NoAssetLoadOnRevealTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; the PSO-delay seam must come from the harness module).*

**Test: no loads on reveal**
- Given: a hook on the async package-load delegate.
- When: reveal happens and 10 s pass.
- Then: zero pool-asset loads, and texture mips of all `A` entries are resident at reveal.
- Edge cases: `A` is 24.

**Test: seeded PSO delay**
- Given: a harness seam that forces one pawn's `IsPSOPrecaching` true after warm-up.
- When: reveal occurs.
- Then: `PoolNotResident` is logged and the round is flagged excluded from Pillar-1 evidence.
- Edge cases: no seam active gives no flag.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CharacterPool/NoAssetLoadOnRevealTest.cpp` — automation test `StealthGame.CharacterPool.PIE.NoAssetLoadOnReveal` (OR documented playtest from a cooked client)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-031-spike-cpa-v2-v3-pso-precache-and-residency.md`, `production/epics/character-pool-appearance/story-032-pool-warmup-subsystem-and-rig.md`
- Unlocks: `production/epics/character-pool-appearance/story-056-pillar1-playtest-chi-square.md`
