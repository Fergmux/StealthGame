# Story 044: M_Character_Master and the static material instances

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: An `AppearanceId` resolves to one pool row whose static `MI_Char_<Model>_<Variant>` (`UMaterialInstanceConstant`, parent `M_Character_Master`) carries constant garment and trim swatch parameters; no runtime MID exists, so nothing can be set per pawn, per team or per viewer.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012; Substrate is off for MVP per ADR-0022)
**Engine Notes**: The master is at the 4-texture-sample cap with no headroom (GDD Visual/Audio Requirements; technical-artist to confirm). Substrate off for MVP and Vertical Slice (ADR-0022 §1): author a classic material. Do not resave sample content or `DefaultEngine.ini` side effects (`production/never-resave-assets.md`).

**Why this story**: The swatch colours come from the master, not the texture (art bible 4.3, 8.7 step 8). The hex lock (OI-3) needs a T1 colour audit of a real model (a Tier A model, or the story 004 test character). **AC-3 revised per QL-STORY-READY**: a measurable gradient method and a sign-off. Read art bible 4.3, 8.x.6 and GDD Visual/Audio "Appearance to material mapping".

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One `UMaterialInstanceConstant` per appearance; the master is opaque with no WPO and no masked opacity (ADR-0012)
- Forbidden: No parameter or switch named `Team`, `Role`, `IsPlayer`, `IsNPC`, `Ally`; no runtime MID (ADR-0012)
- Guardrail: at most 4 texture samples; pool textures at most 40 MB (ADR-0012)

---

## Acceptance Criteria

*From art bible 4.3 / 8.x.6, GDD CP-1, CP-3, CP-16; AC-3 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN `M_Character_Master`, THEN it is opaque with no WPO and no masked opacity, uses at most 4 texture samples, has separate garment and trim regions in `SwatchMaskTex` (trim at most 25 % of garment area) and no parameter or switch named `Team`, `Role`, `IsPlayer`, `IsNPC` or `Ally` (checked by story 007's validator on the real asset).
- [ ] **AC-2** GIVEN the 8 swatch values of art bible 4.3 (hex locked by the T1 colour audit, OI-3, run on the first Tier A model or on the story 004 test character, whichever exists first), THEN each static MI sets constant garment and trim swatch parameters and no runtime MID is used.
- [ ] **AC-3** GIVEN a real character mesh (first Tier A model or the test character) under a neutral HDRI at a fixed camera, THEN a screenshot of all 8 swatches is retained, the luminance of a grey reference sphere differs by at most 10 % across any 20 cm region of its surface, and an art-director sign-off (name, date) is recorded.

---

## Implementation Notes

- Material parameters: garment swatch index and trim swatch index (material index = swatch minus 1, F-7); the swatch table is a master constant array or a small texture, not per-instance data.
- Create the test-character MI from the same master so the screenshot uses the production path.
- Garment colour comes from the master (luminance detail only in the texture, art bible 8.7 step 8).

**Prototype disposition**: replaces the prototype's `M_CrowdTint` (a hue-tint material driven by a per-pawn MID).

---

## Out of Scope

- Story 046: the 12 pool MIs for the Tier A pool; Story 007: the validator

---

## Files

- `Content/StealthGame/Characters/Materials/M_Character_Master` (asset; follow `production/never-resave-assets.md`)
- `Content/StealthGame/Characters/Materials/MI_Char_TestCharacter`
- `production/qa/evidence/master-material-swatches-*.png`, `production/qa/evidence/master-material-evidence.md`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-3 revised with the qa-lead's proposed text; not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: master material rules**
- Setup: open `M_Character_Master` and its stats.
- Verify: opaque, no WPO, no masked opacity, at most 4 texture samples, no banned parameter names; count `SwatchMaskTex` pixels: trim at most 25 % of garment area.
- Pass condition: all checks hold.

**Manual check: static instances**
- Setup: open the 8 static MICs.
- Verify: each sets constant garment and trim swatch parameters with the hex from the OI-3 audit; no MID is created at runtime.
- Pass condition: 8 of 8 match the art bible 4.3 table.

**Manual check: swatch screenshot**
- Setup: the first Tier A model (or the test character) under the neutral HDRI, fixed camera.
- Verify: all 8 swatches visible, with the grey-sphere luminance check.
- Pass condition: gradient at most 10 %, screenshot retained, sign-off recorded.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: a retained screenshot in `production/qa/evidence/` + sign-off in `production/qa/evidence/master-material-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-002-body-base-blender-build-oi10.md`, `production/epics/character-pool-appearance/story-007-pool-shape-and-material-rules-validator.md` (Exit A must not wait on a Meshy credit: the OI-3 colour audit runs on the first Tier A model, or on the test character from story 004 if that exists first)
- Unlocks: `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`
