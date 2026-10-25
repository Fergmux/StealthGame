# Story 003: One Meshy test character generated and normalised to the UEFN skeleton

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: art bible 1.3.2 / 8.7 step 0 forbids the first Meshy credit, this single test character included, until OI-1 (story 001) is closed and Body_Base (story 002, OI-10) is accepted; neither has reported. Cost confirmation with the user is also required (Meshy credits).
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
**ADR Decision Summary**: Tier B models are Meshy generations normalised offline in Blender to one merged single-slot mesh on the project skeleton; the Meshy proportion risk (R8, arms about 17 % short) is mitigated by the Body_Base conform check and the Tier C fallback.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: The import into UE must leave `SK_UEFN_Mannequin` untouched (a sample asset: never resave it, `production/never-resave-assets.md`). The prototype pipeline exported the Blender armature object as an extra root bone; this story must prove the fix (88 bones, no "missing bones" message).

**Why this story**: The Art Director's gate verdict asked for the "OI-1/OI-10 test character first" and for no Meshy mass production: this is the one test character of the art bible's mass-production gate (8.x.14) that proves the T1 to T4 pipeline before characters 2 to 8 (those wait for spikes R1 to R3). **The prototype Meshy citizens (`SK_Citizen02-05`) are throwaway** (arms about 17 % short, extra root bone) and are not reused or promoted; the generation here starts from a mannequin-proportion reference rendered from Body_Base so the arm shortfall does not recur. Read art bible 8.7 (characters, steps 0 to 9), 1.3.3, 1.3.6, 8.9 (task-id record).

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: One merged single-slot skeletal mesh per model on the project's one skeleton, zero extra bones (ADR-0012, ADR-0010/0013)
- Forbidden: Never fix proportions by non-uniform scale (art bible 8.7 step 3); never leave the character barefoot (replaced by the shared cactli sandal piece)
- Guardrail: 4 h per character target, 8 h hard cap (art bible 8.7); one credit-bearing generation only

---

## Acceptance Criteria

*From art bible 8.7 and 1.3.2 / 1.3.9; AC-2 and AC-3 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN Body_Base accepted, WHEN exactly one Meshy image-to-3D generation is made from a mannequin-proportion reference rendered from Body_Base (A-pose, arms 30 to 45 degrees from the torso, flat sandals, matte undyed cloth) after the user confirms the cost, THEN the task id, model, credits and prompt are recorded (8.9) and no second test-character credit is spent before T1 passes (story 004).
- [ ] **AC-2** GIVEN the GLB normalised per art bible 8.7 steps 2 to 9 (one uniform scale to 165.6 cm, shrinkwrap or sculpt to Body_Base, never non-uniform scale; 12k retopology; UV density 5.12 texels/cm; bake; rig to a copy of the UEFN armature), THEN each arm region's skinned-vertex extent, measured with story 008's metric, is within ± 3 % of the same metric on `SK_UEFN_Mannequin` (clavicle-to-hand chain 80.74 cm) and the exported FBX has zero bones beyond the reference 88.
- [ ] **AC-3** GIVEN the FBX imported into UE 5.8.4 against `SK_UEFN_Mannequin`, WHEN the editor loads it, THEN no "Skeleton is missing bones" message appears, the skeleton asset is not marked dirty or saved, total wall time is logged and is at most 8 h (over 8 h the story is recorded FAIL with the cause), and a retained viewport screenshot of the imported mesh and an art-director sign-off (name, date) are recorded.

---

## Implementation Notes

*Derived from ADR-0012 (R8, Tier independence) and art bible 8.7:*

- Confirm cost and `ai_model` with the user before the Meshy call (project Meshy rule: image-to-3D 20 to 30 credits depending on texture; texture resolution 2K is enough for the proof). Prefer a reference image rendered from Body_Base in the roster-row cut of the first character (art bible 1.3.10).
- Keep `Meshy_Original` hidden and high-poly in the `.blend`; work in `Work`; `Body_Proxy` is the accepted Body_Base. Bake and de-light so swatch colour comes from the master material (albedo flattened to the grey-cotton base, luminance detail only).
- Rig: parent to a copy of the UEFN armature, names untouched, no extra bones; export with the preset recorded in story 002 so the armature object is not exported as a bone.
- Record every number the T1 audit (story 004) will need; do not re-measure by eye.

**Prototype disposition**: `assets/characters/meshy/citizen_01..05` and `SK_Citizen02-05` are throwaway; `tools/asset-pipeline/blender_bind_to_mannequin.py` is the bind procedure that carries over (with the story 002 export fix).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004 (`story-004-test-character-t1-t4-signoff.md`): the T1 numeric audit and T2 to T4 captures
- Story 053 (`story-053-meshy-first-six-conform-gate.md`): characters 1 to 6 and the Tier B versus Tier C decision
- Characters 2 to 8: no Meshy credit until the mass-production gate lifts (spikes R1 to R3)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `assets/characters/meshy/test_character/` (`.blend`, task record, export FBX)
- `Content/StealthGame/Characters/Pool/` (test import; not referenced by `DA_CharacterPool`)
- `production/qa/evidence/test-character-import-evidence.md`

---

## QA Test Cases

*QL-STORY-READY: GAPS → revised with the qa-lead's proposed text (AC-2 metric, AC-3 cap and sign-off); not re-run (unattended; decision logged). Visual/Feel: manual verification; sign-off required.*

**Manual check: single credit-gated generation**
- Setup: record cost confirmation, then make one image-to-3D call.
- Verify: task id, model, credits and prompt are recorded (8.9); the Meshy task list shows exactly one test-character task.
- Pass condition: one generation only, and no second credit spent before T1 passes (story 004).

**Manual check: normalisation**
- Setup: run the 8.7 steps 2 to 9 pipeline, then export the FBX.
- Verify: uniform scale to 165.6 cm; arm extent within ± 3 % per region; bone count equals 88 with no root bone named for the armature object.
- Pass condition: the arm-extent report shows no region outside ± 3 %.

**Manual check: UE 5.8.4 import**
- Setup: import against `SK_UEFN_Mannequin` and read the log.
- Verify: no "Skeleton is missing bones" message; the skeleton asset is not dirty or saved; wall time at most 8 h (target 4 h).
- Pass condition: all three hold, the screenshot is retained and the sign-off is recorded.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: a retained screenshot in `production/qa/evidence/` + sign-off in `production/qa/evidence/test-character-import-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-001-oi1-uefn-reference-measurements-recorded.md`, `production/epics/character-pool-appearance/story-002-body-base-blender-build-oi10.md`
- Unlocks: `production/epics/character-pool-appearance/story-004-test-character-t1-t4-signoff.md`
