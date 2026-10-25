# Story 043: Pool evidence goes stale when pool membership changes

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
**ADR Decision Summary**: Any change in pool membership changes `N_models`, hence every model's swatch indices, so T1 items 10 to 12 and T2 re-run for all models; the evidence manifest records the `PoolHash` and CI marks it stale on a mismatch.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); a CI script comparing a manifest to the pool hash
**Engine Notes**: The current pool hash is read from a headless editor commandlet or from the validator's output (story 028); the manifest is plain text.

**Why this story**: CP-15 and AC-CP-33: a pool change must not leave stale art evidence behind. Read GDD CP-15 and AC-CP-33.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `PoolHash` is canonical and order-independent (ADR-0012)
- Forbidden: Never waive a stale manifest (art bible 1.3.7: no waivers)
- Guardrail: CI step seconds

---

## Acceptance Criteria

*From GDD AC-CP-33:*

- [ ] **AC-1** GIVEN a manifest with `PoolHash` X and a current pool hash Y, WHEN `CI.PoolEvidenceFreshness` runs, THEN the manifest is stale and the pool gate is blocked until T1 items 10 to 12 and T2 re-run for **all** models; a matching hash is fresh; a manifest with re-run records for only some models stays blocked; a change of model order only (hash unchanged) is fresh.
- [ ] **AC-2** GIVEN a generated manifest in `production/qa/evidence/pool-<date>/`, THEN it records commit, `PoolHash`, model list and date, and a manifest missing any field fails.

---

## Implementation Notes

- Manifest format: a small text/JSON file `manifest.json` with the four fields and a per-model re-run list.
- Used by stories 047 (create the manifest) and 054 (re-run after a swap).

**Prototype disposition**: none.

---

## Out of Scope

- Story 047: producing the T1 to T4 evidence; Story 028: computing the hash

---

## Files

- `tools/ci/pool_evidence_freshness.py`
- `Source/StealthGameplay/Private/Tests/CharacterPool/PoolEvidenceFreshnessTest.cpp` (or the script's self-test)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Test `CI.PoolEvidenceFreshness`.*

**Test: stale manifest**
- Given: a manifest with `PoolHash` X and a current pool hash Y.
- When: the check runs.
- Then: the manifest is stale and the pool gate is blocked until T1 items 10 to 12 and T2 re-run for all models; a matching hash is fresh; a manifest with re-run records for only some models stays blocked.
- Edge cases: model order changes only (hash unchanged, fresh).

**Test: manifest content**
- Given: a generated manifest in `production/qa/evidence/pool-<date>/`.
- When: read.
- Then: it records commit, `PoolHash`, model list and date.
- Edge cases: a missing field fails.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: the script's self-test or `Source/StealthGameplay/Private/Tests/CharacterPool/PoolEvidenceFreshnessTest.cpp` — `CI.PoolEvidenceFreshness` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-028-poolhash-and-content-digest.md`
- Unlocks: `production/epics/character-pool-appearance/story-047-tier-a-pool-gates-t1-t4.md`, `production/epics/character-pool-appearance/story-054-meshy-pool-swap-tier-b-or-c.md`
