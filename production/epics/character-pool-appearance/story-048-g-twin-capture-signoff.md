# Story 048: G-TWIN: four copies of one look read as an ordinary crowd

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: Twins are guaranteed (every active appearance has at least 4 copies), so copies must be perfectly interchangeable: no per-instance random tint, scale, dirt or wetness; variation comes only from behaviour, animation phase and gaze (Crowd).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Capture with the pool's real pawn path (static MI, no MID) so the screenshot reflects production.

**Why this story**: Gate G-TWIN (CP-15, AC-CP-24b). **AC-2 revised per QL-STORY-READY**: a committed binary checklist replaces "reads as an ordinary crowd". Read GDD CP-15, CP-16 and Visual/Audio "Twins".

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One `UMaterialInstanceConstant` per appearance; no per-instance variation (ADR-0012)
- Forbidden: No per-instance tint, scale, dirt or wetness; never cluster or separate twins by team or participant
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-24b; AC-2 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN 4 copies of one appearance within 6 m, THEN screenshots at 15 m and at close range are retained in the pool evidence folder `production/qa/evidence/pool-<date>/`.
- [ ] **AC-2** GIVEN the committed checklist `production/qa/evidence/pool-<date>/gtwin-checklist.md` with binary items (1) all 4 copies share one MI pointer, (2) bounding-box heights of the 4 copies are equal, (3) no per-instance material parameter exists, (4) the four silhouettes at 15 m are the same shape and spacing is irregular with no row or grid, THEN 4 of 4 pass and a named, dated art-lead sign-off is recorded (author to confirm the items).

---

## Implementation Notes

- Place the four copies by hand for the capture (irregular spacing); the placement rule for the real crowd is Crowd's.
- Items 1 to 3 can be read from the editor; item 4 is a judgement against the capture.

**Prototype disposition**: none.

---

## Out of Scope

- Story 049: G-VAR; Crowd's spacing rule (crowd-npc-behaviour epic)

---

## Files

- `production/qa/evidence/pool-<date>/gtwin-checklist.md` and captures

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 replaced with the qa-lead's committed checklist; not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: twin capture**
- Setup: 4 copies of one appearance within 6 m, captured at 15 m and close range.
- Verify: screenshots retained in the pool evidence folder.
- Pass condition: both captures exist.

**Manual check: checklist**
- Setup: apply the committed checklist to the captures.
- Verify: each binary item.
- Pass condition: 4 of 4 pass, with named and dated sign-off.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained screenshots in `production/qa/evidence/pool-<date>/` + art-lead sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`, `production/epics/character-pool-appearance/story-047-tier-a-pool-gates-t1-t4.md`
- Unlocks: None
