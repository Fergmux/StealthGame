# Story 001: OI-1 closed: the UEFN mannequin measurements become the art-bible reference

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Config/Data
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`. The art gates (CP-5, CP-15) have no TR of their own; they trace to TR-cpa-001, the pool-content requirement.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: The pool is one hard-referenced data asset of merged single-slot meshes (Tier A mannequin kit, Tier B Meshy models normalised to the project skeleton, Tier C one shared body with Meshy heads and garments), all on the one project skeleton with zero extra bones; the validator checks bone chains and mesh conformance against that reference.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012 knowledge risk; this story edits documents only)
**Engine Notes**: No engine API. The measurements come from the 2026-10-25 editor run in `production/qa/spikes/2026-10-25_S1-S2-S14.md` section 4 (LOD0 imported bounds for stature, bone-origin distances for chains) and `production/qa/logs/2026-10-25_A_mannequin-bone-dump.txt`.

**Why this story**: Art bible 1.3.2 still says the contract is "the measured `SK_Mannequin`" with a 175 cm working value and a 173 to 177 cm band (OI-1, open since 2026-10-24). The spike measured what the playable character and the crowd pool really use: `SK_UEFN_Mannequin` (`SKM_UEFN_Mannequin`), 88 bones, **165.6 cm**, not Manny's 180.5 cm. The Art Director's gate verdict (production/gate-checks/2026-10-25_technical-setup-to-pre-production.md) puts "OI-1/OI-10 test character first": no Meshy credit, including the single test character, may be spent while OI-1 is open, so this story is the first thing in the epic. The user's instruction fixes the reference: **UEFN mannequin, 165.6 cm**. Read these sections only: art bible 1.3.2, 1.3.5, 8.6, 8.7 step 3, 8.x.17 (Grep `^#`, never the whole 3,800-line file).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Pool characters use one merged single-slot skeletal mesh per model/accessory variant on the project's one skeleton asset (ADR-0012)
- Forbidden: Never build appearances at run time (leader-pose, skeletal mesh merge, Mutable) (ADR-0012); never quote a bone count or stature from memory, record the measured value
- Guardrail: Art bible LOD0 12,000 ± 2,000 triangles per pool character (8.5); this story changes no budget

---

## Acceptance Criteria

*From art bible 1.3.2 / 8.x.17 (OI-1) and GDD OQ-1 / OQ-2, scoped to this story:*

- [ ] **AC-1** GIVEN the 2026-10-25 measurements (production/qa/spikes/2026-10-25_S1-S2-S14.md section 4 and production/qa/logs/2026-10-25_A_mannequin-bone-dump.txt), WHEN art bible 1.3.2 and 1.3.5 are amended, THEN they name `SK_UEFN_Mannequin` as the pool reference skeleton and record stature 165.6 cm (band ± 2 cm = 163.6 to 167.6 cm), 88 bones and the nine chain lengths (thigh 39.05, shin 42.56, foot 15.15, spine 49.47, clavicle to upper arm 15.51, upper arm 26.65, forearm 22.35, hand 16.23 (15.44 straight line), neck 9.95 cm), and the 175 cm working value and Manny's 180.5 cm are marked superseded.
- [ ] **AC-2** GIVEN the stature is outside the old 173 to 177 cm band, WHEN dependent values are re-derived from the chain lengths (chains win, 1.3.2), THEN head height, shoulder, chest depth, hip, foot, hem, garment envelope, the 8.6 import scale and the 8.7 step 3 target each carry a re-derived number with its derivation line, a numeric per-chain tolerance for the validator is stated (proposed ± 3 %, art-director confirms), OI-1 is marked CLOSED with the date in 8.x.17 and the art-director re-sign of 1.3 is recorded.
- [ ] **AC-3** GIVEN downstream values that assume a 175 cm figure (`TeammateMarkerAnchorCm` 195 and teammate-markers OQ7, the 8.5 reference-camera distance table, the T2 to T3 distances), WHEN OI-1 closes, THEN each is listed in a change-request list with its owning document and owner (this story edits none of those GDDs).

---

## Implementation Notes

*Derived from ADR-0012 §1 (validator) and the art bible's own precedence rule:*

- **Chains win.** Where a derived absolute dimension (head height 23 cm, shoulder 40, chest depth 24, hip 34, foot 26, hem 55 ± 8, envelope 54 ± 3) disagrees with the chain lengths, the chain lengths decide and the number is re-derived by scaling (165.6 / 175 = 0.946) and then sanity-checked against the chains. Show the derivation, not just the result.
- **Machine-readable copy.** Write the nine chains, stature, bone count and tolerance into one data file the validators read (story 008): `Source/StealthGameEditor/Private/Tests/CharacterPool/Golden/UefnReferenceChains.txt` (golden for tests) or the tuning/pipeline data location the TD names; never copy numbers into code.
- **Extra bone finding.** The spike found the prototype citizens carry one extra root bone (`SKM_UEFN_Mannequin`, the Blender armature object exported as a bone, 89 bones against 88). Record "zero extra bones, reference = 88" next to the measurement so stories 002, 003 and 008 test against it.
- Do not resave the sample skeleton or any `Content/` sample asset (`production/never-resave-assets.md`).

**Prototype disposition**: `assets/characters/reference/SKM_UEFN_Mannequin.fbx` and `tools/asset-pipeline/blender_bind_to_mannequin.py` carry over as the reference mesh and the bind procedure. The four prototype Meshy citizens (`SK_Citizen02-05`) are throwaway: their proportions (arms about 17 % short) are the failure this reference exists to prevent.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (`story-002-body-base-blender-build-oi10.md`): building Body_Base (OI-10)
- Story 008 (`story-008-pool-mesh-conformance-validator.md`): the automated chain and arm-geometry check
- Editing `design/gdd/teammate-markers.md` or any other GDD: the change-request list only names them

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `design/art/art-bible.md` (sections 1.3.2, 1.3.5, 8.6, 8.7, 8.x.17 only)
- `Source/StealthGameEditor/Private/Tests/CharacterPool/Golden/UefnReferenceChains.txt`
- `production/qa/smoke-oi1-closure.md`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead, Config/Data, advisory gate); document checks.*

**Manual check: art bible amended with the measured reference**
- Setup: open `design/art/art-bible.md` 1.3.2 and 1.3.5 beside spike section 4 and the bone dump.
- Verify: the name, stature, bone count and nine chain lengths equal the spike values; 175 cm and 180.5 cm are marked superseded.
- Pass condition: every value equals its source number and the band reads 163.6 to 167.6 cm.

**Manual check: derived values and OI-1 closure**
- Setup: the same 1.3.x sections and 8.x.17.
- Verify: head height, shoulder, chest depth, hip, foot, hem, envelope, the 8.6 import scale and the 8.7 step 3 target each carry a number plus a derivation line; OI-1 reads CLOSED with a date; the art-director re-sign of 1.3 is recorded.
- Pass condition: no re-derivable value lacks a derivation line; the closure date and signer are present.

**Manual check: downstream change-request list**
- Setup: open the change-request list.
- Verify: it contains `TeammateMarkerAnchorCm` 195 and teammate-markers OQ7, the 8.5 distance table and the T2/T3 distances, each with its owning document.
- Pass condition: all entries present, and `git diff` shows no GDD edited.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass, `production/qa/smoke-oi1-closure.md` (advisory gate by default)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None
- Unlocks: `production/epics/character-pool-appearance/story-002-body-base-blender-build-oi10.md`, `production/epics/character-pool-appearance/story-003-meshy-test-character-generate-normalise.md`, `production/epics/character-pool-appearance/story-008-pool-mesh-conformance-validator.md`
