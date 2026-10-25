# Story 047: Tier A pool passes T1 to T4 (whole-pool gate)

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: A model enters the pool only when it passes T1 (numeric audit) and T2 to T4 (line-up at 15 m, greyscale silhouette, motion and skin) with captures retained and signed by the art lead; the T2 and T3 pass thresholds restate to the actual chance level 1/N_models + 0.125; no waivers.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: UE capture at midday lighting; reuse the capture setup file of story 004 unchanged where it validated. A pool change makes the evidence stale (story 043).

**Why this story**: AC-CP-23 `PT-CP-T1..T4` for the Tier A pool. **AC-2 revised per QL-STORY-READY** with the stated pass lines (N = 6 models: 1/6 + 0.125 = 29.2 %). Read art bible 1.3.7 (T1 checklist, T2 to T4) and GDD CP-15.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Pool admission gates CP-15 per model; evidence manifest records commit, `PoolHash`, model list and date (ADR-0012)
- Forbidden: No waivers; a model that cannot pass is cut
- Guardrail: LOD limits of art bible 8.5

---

## Acceptance Criteria

*From GDD AC-CP-23 (T1 to T4) and art bible 1.3.7; AC-2 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN each of the six models, THEN its signed 20-item T1 checklist exists.
- [ ] **AC-2** GIVEN the captures made with fixed camera, lighting and resolution (at most 12 figures per capture), THEN T2 (15 m, sun and shade) has no model named by more than `1/N_models + 0.125` of selections (N = 6: 29.2 %), T3a pairwise silhouette IoU is at least 0.90 for every pair, T3b correct identification is at most 29.2 % over at least 64 trials, and T4 holds the 1.3.5 deformation rule with animation cost within 10 % of the pool median; captures are signed by the art lead.
- [ ] **AC-3** GIVEN the evidence, THEN `production/qa/evidence/pool-<date>/` holds a manifest of commit, `PoolHash`, model list and date matching the current pool (story 043's check green).

---

## Implementation Notes

- Testers and trial counts per art bible 1.3.7 (6 testers, 3 shuffles each for T2).
- T1 items 11 and 19 are pool-relative and re-run on any membership change (GDD Visual/Audio "Pool gates").
- T5 (crowd performance) is a separate story (050).

**Prototype disposition**: none.

---

## Out of Scope

- Story 048 (G-TWIN), 049 (G-VAR), 050 (T5)

---

## Files

- `production/qa/evidence/pool-<date>/` (T1 sheets, T2 to T4 captures, manifest, sign-off)

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 revised with the qa-lead's proposed pass lines; not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: T1 checklists**
- Setup: collect the per-model checklists.
- Verify: 6 signed 20-item sheets.
- Pass condition: 6 of 6 exist and are signed.

**Manual check: T2 to T4**
- Setup: capture per art bible 1.3.7 (at most 12 figures per capture).
- Verify: the AC-2 numbers.
- Pass condition: every threshold met, signed.

**Manual check: evidence layout**
- Setup: open `production/qa/evidence/pool-<date>/`.
- Verify: the manifest holds commit, `PoolHash`, model list and date.
- Pass condition: present and matching the current pool.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures in `production/qa/evidence/pool-<date>/` + lead sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`, `production/epics/character-pool-appearance/story-043-pool-evidence-freshness-ci.md`
- Unlocks: `production/epics/character-pool-appearance/story-048-g-twin-capture-signoff.md`, `production/epics/character-pool-appearance/story-049-g-var-paired-trials.md`
