# Story 046: DA_CharacterPool authored with 12 rows and validated on real assets

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Config/Data
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`, `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`. TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: Exactly one instance ships, `DA_CharacterPool` at `/Game/StealthGame/Characters/Pool/`, registered with the Asset Manager as primary asset type `CharacterPool` with cook rule `AlwaysCook` (an Asset Manager settings entry applied by the implementing story); the editor validators run on it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Asset Manager settings live in `Config/DefaultGame.ini`; an editor re-save of project settings can add unrelated lines (spike S2 incident with `DefaultEngine.ini`): diff the config after every save and revert unrelated changes. Never resave sample assets (`production/never-resave-assets.md`).

**Why this story**: It joins Exit A's pieces: the validators (006 to 008), the master and MIs (044) and the six models (045) become one cooked asset, which is also what stories 025, 031 and 047 consume. Read GDD CP-1, CP-2, AC-CP-21, AC-CP-22.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The pool is one `UCharacterPoolData` with hard references, held by `UCharacterPoolSubsystem`; one static MI per appearance (ADR-0012)
- Forbidden: Never use a `UDataTable` pool; do not reference the throwaway prototype assets (ADR-0012 alternative 5)
- Guardrail: Pool textures at most 40 MB

---

## Acceptance Criteria

*From GDD AC-CP-21, AC-CP-22 and ADR-0012 §1:*

- [ ] **AC-1** GIVEN `DA_CharacterPool` with 12 rows (6 models x `Default`, `GarmentSwap`), stable ids and 12 static MICs parented to `M_Character_Master`, THEN the validators of stories 006 to 008 pass on the real assets (AC-CP-21 and AC-CP-22), including the pre-save swatch rewrite.
- [ ] **AC-2** GIVEN the Asset Manager settings, THEN a primary asset type `CharacterPool` with cook rule `AlwaysCook` is registered and a cooked build contains the asset.
- [ ] **AC-3** GIVEN the Reference Viewer on `DA_CharacterPool`, THEN `SK_Citizen02` to `SK_Citizen05` and `M_CrowdTint` are not referenced, and the pool's `PoolHash` is recorded in `production/qa/smoke-pool-<date>.md`.

---

## Implementation Notes

- Rows: ids authored once (0 to 11 for the MVP, 12 to 23 reserved for new rows), never renumbered when a model is added (CP-1).
- The cooked-build check can reuse the story 031 cook recipe.
- Check `git diff Config/` after saving; revert unrelated ini lines.

**Prototype disposition**: replaces the prototype pool entries and `Content/StealthGame/Characters/Pool/` citizens (throwaway).

---

## Out of Scope

- Story 047: art gates; Story 052: removing prototype code and assets

---

## Files

- `Content/StealthGame/Characters/Pool/DA_CharacterPool` and `MI_Char_<Model>_<Variant>` (12)
- `Config/DefaultGame.ini` (Asset Manager entry only)
- `production/qa/smoke-pool-<date>.md`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; Config/Data, advisory gate); smoke checks.*

**Manual check: pool asset and validators**
- Setup: open `DA_CharacterPool` and run the 006 to 008 validators.
- Verify: 12 rows (6 models x Default and GarmentSwap), stable ids, 12 static MICs under `M_Character_Master`.
- Pass condition: all validators pass on the real assets.

**Manual check: Asset Manager and cook**
- Setup: check the Asset Manager settings and cook.
- Verify: primary asset type `CharacterPool` with cook rule `AlwaysCook`; the cooked build contains the asset.
- Pass condition: the asset is found in the cooked output.

**Manual check: prototype isolation**
- Setup: Reference Viewer on `DA_CharacterPool`.
- Verify: `SK_Citizen02` to `05` and `M_CrowdTint` are not referenced.
- Pass condition: zero references; the `PoolHash` is in the smoke note.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass, `production/qa/smoke-pool-<date>.md` (advisory gate by default)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-006-swatch-rule-f7-validator.md`, `production/epics/character-pool-appearance/story-007-pool-shape-and-material-rules-validator.md`, `production/epics/character-pool-appearance/story-008-pool-mesh-conformance-validator.md`, `production/epics/character-pool-appearance/story-044-master-material-and-static-instances.md`, `production/epics/character-pool-appearance/story-045-tier-a-six-mannequin-variant-models.md`, `production/epics/character-pool-appearance/story-025-pool-subsystem-residency.md`
- Unlocks: `production/epics/character-pool-appearance/story-047-tier-a-pool-gates-t1-t4.md`, `production/epics/character-pool-appearance/story-048-g-twin-capture-signoff.md`, `production/epics/character-pool-appearance/story-049-g-var-paired-trials.md`, `production/epics/character-pool-appearance/story-052-retire-prototype-pool-and-dealer.md`
