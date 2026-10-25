# Story 004: Test character passes T1 to T4 and is signed off (pipeline proof)

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
**ADR Decision Summary**: A model enters the pool only through the T1 numeric audit and the T2 to T4 visual tests (CP-15); the pool validator automates the numeric half. No waivers.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Captures are made with UE capture at midday lighting; the test character is a pipeline proof, so pass thresholds that are relative to the whole pool (T2/T3 chance level 1/N_models + 0.125) are applied in story 047, not here.

**Why this story**: The Art Director's concern was "no visual measurements yet (OI-1/OI-10 test character first)". This story produces the first real measurements: the T1 audit of one generated character, the T2 to T4 capture set with a committed setup file, and the closure of the items that only this character can close (OI-3 colour items 10 to 12, OI-9 LOD3 policy on items 16, 22 and 23). It also records the first conform result as the data point for GDD OQ-1 ("do normalised Meshy bodies pass the Body_Base conform test"). Read art bible 1.3.7 (T1 checklist, T2 to T4) and 8.x.14 (mass-production gate) by section.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Pool characters are one merged single-slot skeletal mesh on the project skeleton; the pool admission gates CP-15 run per model (ADR-0012)
- Forbidden: Never waive a T1 to T4 failure (art bible 1.3.7: no waivers)
- Guardrail: LOD0 12,000 ± 2,000, LOD1 ≤ 6,000, LOD2 ≤ 2,500, LOD3 ≤ 900 triangles (art bible 8.5)

---

## Acceptance Criteria

*From art bible 1.3.7 and GDD CP-15; AC-2 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN the imported test character (story 003), WHEN the 20-item T1 numeric audit runs (including LOD3 items 16, 22 and 23 for OI-9 and colour items 10 to 12 for OI-3), THEN every item is in tolerance and the report is retained.
- [ ] **AC-2** GIVEN the same character, WHEN T2 (15 m, full sun and shade), T3 (greyscale 20 m and 30 m) and T4 (standard pose and locomotion set) captures are made with a committed camera, lighting and 1080p setup file, THEN the captures exist in `production/qa/evidence/` and an art-director PASS or FAIL on "the same setup file can be reused unchanged for the pool gate (story 047)" is recorded.
- [ ] **AC-3** GIVEN the evidence, THEN the art-director sign-off (name, date) is recorded and the Body_Base conform result is logged as the first data point for GDD OQ-1.

---

## Implementation Notes

*Derived from art bible 1.3.7 and ADR-0012 §1 (validator = automated half of T1):*

- Run the T1 audit by script (reuse Body_Base's script from story 002) on the imported asset and on the LOD chain; keep the per-item numbers, not just pass/fail.
- OI-3 colour items 10 to 12 need the neutral-HDRI gradient check (under 10 % across any 20 cm region) and the skin-band, garment-hex measurements; correct hexes, not the bands, if a value is off (art bible OI-3).
- Save the capture setup (camera transform, lighting, 1080p, time) as a file in `production/qa/evidence/` so story 047 reuses it unchanged.
- A FAIL is a valid outcome of this story as a pipeline proof only if the cause and the corrective step are recorded; the character is then regenerated or cut (8.7), not waived.

**Prototype disposition**: none of the prototype assets are measured here; `SK_Citizen02-05` are throwaway and are not entered as T1 subjects.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 047 (`story-047-tier-a-pool-gates-t1-t4.md`): the whole-pool T1 to T4 gate with chance-level thresholds
- Story 050 (`story-050-t5-crowd-performance-gauntlet.md`): T5 crowd performance
- Story 053: the first-six conform gate

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/test-character-t1-report.md`
- `production/qa/evidence/test-character-capture-setup.*` and the T2 to T4 captures
- `production/qa/evidence/test-character-evidence.md` (sign-off, OQ-1 data point)

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 revised with the qa-lead's proposed text; not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: T1 numeric audit**
- Setup: run the 20-item T1 script, including items 16, 22 and 23 (OI-9) and items 10 to 12 (OI-3).
- Verify: every item against its tolerance.
- Pass condition: 20 of 20 in tolerance and the report is retained.

**Manual check: T2 to T4 captures**
- Setup: 15 m sun and shade; greyscale at 20 m and 30 m; the standard pose and locomotion set; fixed camera, lighting and 1080p from the setup file.
- Verify: files exist in `production/qa/evidence/` and the setup file is committed.
- Pass condition: all captures present and the reuse verdict is recorded as PASS.

**Manual check: sign-off and OQ-1 data**
- Setup: the evidence note.
- Verify: art-director name and date are recorded; the Body_Base conform result is logged as the OQ-1 data point.
- Pass condition: both present.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures in `production/qa/evidence/` + sign-off in `production/qa/evidence/test-character-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-003-meshy-test-character-generate-normalise.md`
- Unlocks: `production/epics/character-pool-appearance/story-053-meshy-first-six-conform-gate.md` (the Tier A stories 044 to 046 deliberately do not wait on this story: Exit A must not depend on a Meshy credit)
