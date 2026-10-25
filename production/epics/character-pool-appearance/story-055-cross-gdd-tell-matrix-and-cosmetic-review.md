# Story 055: Cross-GDD tell-matrix review and the cosmetic/colour-aid checklist

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: No appearance choice, unlock or cosmetic exists in MVP; any later cosmetic enters the pool as a new appearance at equal weight or is cut, and a colour-vision aid is a uniform full-screen transform on the viewing client, never per pawn (CP-17).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); documents only
**Engine Notes**: No engine API. Reclassified from Integration to Config/Data (advisory) per QL-STORY-READY: both checks are document reviews, not automated tests. The automated `StealthGame.CharacterPool.Policy.ColourAidIsFullScreen` test is created by the first story that adds a colour-vision aid (none in MVP); it is out of scope here.

**Why this story**: AC-CP-26 (Baseline and Tell Matrix) and AC-CP-27b (cosmetic checklist). **AC-2 revised per QL-STORY-READY** (the checklist item exists and a worked example fails it). Read GDD CP-17, AC-CP-26, AC-CP-27b and Cross-References.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Cosmetics enter the deck at equal weight or are a uniform full-screen transform on the viewing client only (ADR-0012, GDD CP-17)
- Forbidden: Never allow a cosmetic or accessibility option that changes one pawn's appearance for one viewer
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-26 and AC-CP-27b; AC-2 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN the Crowd GDD, THEN its Baseline and Tell Matrix contains the row "pawn look, replacement look and informant look: never a tell (red herring: not applicable; owner: Character Pool & Appearance)", and each of Round, Crowd, Roster, Shared Character, Visibility Policy, Session & Travel and NPC-Kill Penalty & Informants lists this GDD's interfaces (a document-review record, 1 row and 7 of 7 GDDs).
- [ ] **AC-2** GIVEN the design-review template, THEN a checklist item exists verifying that any proposed cosmetic or accessibility option enters the deck at equal weight or is a uniform full-screen transform on the viewing client only, and a worked example with a hypothetical cosmetic that enters the deck at a heavier weight is marked FAIL on it.

---

## Implementation Notes

- The review is read-only on the other GDDs; list gaps as change requests to their owners (coordination rules: no unilateral cross-domain edits).
- Put the checklist item where design reviews already look (`.claude/docs/templates/` design-review or the GDD template's review checklist); record the file in the evidence note.

**Prototype disposition**: none.

---

## Out of Scope

- Editing the other GDDs; the colour-aid automation (a future story)

---

## Files

- `production/qa/evidence/cpa-xgdd-review-<date>.md`
- the design-review checklist file chosen above (one added item)

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 and type revised with the qa-lead's proposed text; not re-run (unattended; decision logged). Config/Data: advisory gate; document checks.*

**Manual check: tell matrix row**
- Setup: open the Crowd GDD Baseline and Tell Matrix.
- Verify: the row text matches AC-CP-26; each of Round, Crowd, Roster, Shared Character, Visibility Policy, Session & Travel and NPC-Kill Penalty & Informants lists this GDD's interfaces.
- Pass condition: 1 row and 7 of 7 GDDs, with a review record.

**Manual check: cosmetic checklist**
- Setup: the review template.
- Verify: the checklist item exists and the worked example fails it.
- Pass condition: item present and the example is marked FAIL.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass, `production/qa/smoke-cpa-xgdd-<date>.md` (advisory gate by default)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: None
