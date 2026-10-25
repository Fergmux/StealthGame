# Story 049: G-VAR: the active models still differ at 8 m (paired-trial playtest)

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
**ADR Decision Summary**: G-VAR (minimum variety) is a CPA-owned gate: at 8 m in idle, for every pair of active models the lower 95 % Wilson bound of the correct-discrimination rate is at least 65 % (at least 47 of 60 trials correct), so a pool that passes T1 to T5 only by being near-identical cannot ship; thresholds are provisional (OQ-3).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Playtest tooling only (capture stills or short idle clips at 8 m, randomised pair order); no engine feature at risk.

**Why this story**: T1 to T5 prove no model stands out; T7/G-VAR proves the models still differ (art bible 1.3.7 T7). Art-director risk: faceless mannequin twins read as dolls at assassination range. **AC-2 revised per QL-STORY-READY** to add the sign-off. Read GDD CP-15, AC-CP-24a, OQ-3.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Pool admission gates are evidence-backed and signed (ADR-0012)
- Forbidden: No waivers
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-24a; AC-2 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN the active models at 8 m in idle and a written pre-registered protocol (randomised pair order, 3 s exposure, 60 trials per pair from 6 testers), THEN for every pair at least 47 of 60 trials are correct (lower 95 % Wilson bound at least 65 %).
- [ ] **AC-2** GIVEN the evidence, THEN the raw tally CSV and per-tester rates are retained in `production/qa/evidence/pool-<date>/`, the thresholds are noted as provisional (OQ-3), and an art-lead sign-off (name, date) is recorded over the CSV and rates.

---

## Implementation Notes

- Write the protocol before the trials (pre-registration): commit it before the first trial.
- Trials from the same tester are not independent: report per-tester rates as the GDD asks.
- This is the first calibration data for OQ-3 (thresholds on the first six real models).

**Prototype disposition**: none.

---

## Out of Scope

- Story 047: T1 to T4; Story 048: G-TWIN

---

## Files

- `production/qa/evidence/pool-<date>/gvar-protocol.md`, `gvar-tally.csv`, `gvar-evidence.md`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 sign-off added with the qa-lead's proposed text; not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: paired trials**
- Setup: the written protocol (randomised pair order, 3 s exposure, 60 pre-registered trials per pair from 6 testers, active models at 8 m in idle).
- Verify: the correct count for each pair.
- Pass condition: at least 47 of 60 for every pair (lower 95 % Wilson bound at least 65 %).

**Manual check: evidence**
- Setup: collect the raw tally CSV and per-tester rates.
- Verify: retained, thresholds noted provisional (OQ-3).
- Pass condition: files present and signed.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: tally CSV and retained stills in `production/qa/evidence/pool-<date>/` + art-lead sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`, `production/epics/character-pool-appearance/story-047-tier-a-pool-gates-t1-t4.md`
- Unlocks: `production/epics/character-pool-appearance/story-056-pillar1-playtest-chi-square.md`
