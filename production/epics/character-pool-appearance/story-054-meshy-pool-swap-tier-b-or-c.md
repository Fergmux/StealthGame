# Story 054: Pool swap to Tier B or Tier C content (Exit B)

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: the story 053 verdict (Tier B or Tier C) has not been recorded and the art bible's mass-production gate (spikes R1 to R3, ADR-0022 conditional) has not lifted, so no Meshy credit may be spent on the swap content. Credit confirmation with the user is also required.
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: A tier change is a content change gated by CP-15: existing `AppearanceId` values stay stable (CP-1), new rows take unused ids, and the allocator, replication and validators are identical across tiers.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Content swap through `DA_CharacterPool` only; follow `production/never-resave-assets.md` and diff `Config/` after saves.

**Why this story**: Exit B of the epic (about 5 days, late, credit-gated; EPIC Build Order). It replaces Tier A mannequin variants with Meshy models (Tier B) or Meshy heads and garments on Body_Base (Tier C). **AC-3 sign-off added per QL-STORY-READY.** Read GDD CP-5, CP-15 and Edge Cases (Tier swap).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One merged single-slot mesh per model/accessory variant; one static MI per appearance (ADR-0012)
- Forbidden: No waivers; never renumber existing ids
- Guardrail: Pool textures at most 40 MB; LOD limits of art bible 8.5

---

## Acceptance Criteria

*From GDD CP-1, CP-5, CP-15 and Edge Cases; revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN the story 053 verdict, THEN pool rows are swapped to Tier B or Tier C content with every existing `AppearanceId` unchanged and new rows on unused ids (story 005's stable-id test passes on the real pool).
- [ ] **AC-2** GIVEN the swapped pool, THEN the validators (stories 006 to 008) and AC-CP-21, 22, 29 pass unchanged.
- [ ] **AC-3** GIVEN the swap, THEN T1 items 10 to 12 and T2 re-run for all models, T5 (story 050) re-runs, the freshness check (story 043) is green, and an art-director sign-off (name, date) is recorded.

---

## Implementation Notes

- Swatch indices depend on `N_models`: the pre-save hook rewrites them; re-run T1 items 10 to 12 for **all** models, not only the new ones (CP-15).
- The T2 and T3 thresholds restate to the new chance level (1/N or 1/appearances).
- Delete the Tier A meshes only after the evidence is signed; keep the `.blend` sources.

**Prototype disposition**: none (the prototype citizens were removed in story 052).

---

## Out of Scope

- Story 053: the verdict; the allocator and ledger (unchanged by design)

---

## Files

- `Content/StealthGame/Characters/Pool/DA_CharacterPool` (rows), new `SK_Pool_*` and `MI_Char_*`
- `production/qa/evidence/pool-<date>/` (re-run evidence and manifest)

---

## QA Test Cases

*QL-STORY-READY: GAPS → sign-off added with the qa-lead's proposed text; not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: row swap**
- Setup: compare the pool before and after.
- Verify: existing `AppearanceId`s unchanged and new rows on unused ids (story 005's stable-id test).
- Pass condition: zero changed ids.

**Manual check: validators**
- Setup: run stories 006 to 008 and AC-CP-21, 22, 29.
- Verify: they pass unchanged.
- Pass condition: all green.

**Manual check: re-runs**
- Setup: re-run T1 items 10 to 12, T2 and T5 for all models.
- Verify: the freshness check (043) is green.
- Pass condition: all pass and the sign-off is recorded.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures in `production/qa/evidence/pool-<date>/` + lead sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-053-meshy-first-six-conform-gate.md`, `production/epics/character-pool-appearance/story-024-tier-c-fixture-tier-independence.md`, `production/epics/character-pool-appearance/story-043-pool-evidence-freshness-ci.md`, `production/epics/character-pool-appearance/story-050-t5-crowd-performance-gauntlet.md`
- Unlocks: None
