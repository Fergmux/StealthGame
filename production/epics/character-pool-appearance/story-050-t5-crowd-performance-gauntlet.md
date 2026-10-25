# Story 050: T5 crowd performance on the 3060-class reference PC

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: ADR-0022 spikes R1 to R3 (Lumen/Nanite scene cost, skin cache with VSM at 60 characters, motion-matching cost at 60 characters; art bible 8.x.14) have not reported, and ADR-0022 is conditionally accepted: the frame budget this test checks has no measured basis until they do. The art bible's mass-production gate rests on the same three spikes.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load.)*

**ADR Governing Implementation**: ADR-0022: Rendering configuration and performance budgets
**ADR Decision Summary**: Lumen, Nanite and VSM are configured and the CPU and GPU frame budgets are fixed for the RTX 3060-class 1080p60 target (GPU 14.6 ms); pool characters are non-Nanite with 4 LODs, animation at most 4 ms summed game-thread plus worker (ADR-0013 §5).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0022)
**Engine Notes**: Skin-cache mode (`SkinCacheUsage` `Enabled` versus `Disabled` on every pool LOD, project default Exclusive) is ADR-0022's spike R2; CPA's validator only enforces uniformity (story 008). The crowd count follows the art bible and the user decision of 2026-10-25: `NpcCount` 50, so about 60 bodies (10 players + 50 NPCs, `D = 60`); the GDD AC-CP-23 text still says "50 characters" and is stale (flag to the GDD owner).

**Why this story**: T5 (crowd integration, art bible 1.3.7) re-runs whenever the pool, LOD or pool material changes, with one run on real allocator output. **AC-1 revised per QL-STORY-READY**: 60 characters and median animation. Read art bible 1.3.7 T5 and 8.x.14, ADR-0022 (budget table) by section.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Median frame at most 16.67 ms and animation at most 4 ms with the real pool; Gauntlet performance tests specify thresholds and fail if exceeded (testing standards)
- Forbidden: Never weaken the threshold to pass; never run the gate on PIE
- Guardrail: GPU 14.6 ms, animation at most 4 ms (ADR-0022, ADR-0013)

---

## Acceptance Criteria

*From GDD AC-CP-23 `PT-CP-T5`; AC-1 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN a Gauntlet test on the RTX 3060-class reference PC (10 s warm-up, then 60 s) with 60 characters (10 players + 50 NPCs, `D = 60`) and one run using real allocator output, THEN median frame time is at most 16.67 ms and median animation time is at most 4 ms over the 60 s window.
- [ ] **AC-2** GIVEN the run, THEN the CSV and an Unreal Insights trace are retained under `production/qa/evidence/pool-<date>/`, and the test re-executes on any pool, material or LOD change (wired to story 043's staleness check).

---

## Implementation Notes

- Producer or art-director to resolve the 50-versus-60 character count between GDD AC-CP-23 and the art bible (decided by the user 2026-10-25: 60 bodies).
- Reuse the Gauntlet multi-process node from tuning-data-config story 020.

**Prototype disposition**: none.

---

## Out of Scope

- Stories 047 and 049: other pool gates; the spikes R1 to R3 themselves (art bible 8.x.14, other owners)

---

## Files

- `Source/StealthTestHarness/Private/CharacterPool/CrowdPerformanceGauntlet.cpp`
- `production/qa/evidence/pool-<date>/t5-*.csv` and the Insights trace

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-1 revised (60 characters, median animation) with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: crowd gauntlet**
- Given: the RTX 3060-class reference PC, 10 s warm-up then 60 s, real allocator output.
- When: the Gauntlet node runs.
- Then: median frame time at most 16.67 ms and median animation time at most 4 ms.
- Edge cases: the run repeats on any pool, material or LOD change.

**Test: evidence**
- Given: the run.
- When: read.
- Then: the CSV and an Unreal Insights trace are retained.
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: Gauntlet test `PT-CP-T5` with CSV and Insights trace retained in `production/qa/evidence/pool-<date>/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: None
