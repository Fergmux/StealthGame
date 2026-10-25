# Evidence: Meshy test character (CPA story 003)

Story: `production/epics/character-pool-appearance/story-003-meshy-test-character-generate-normalise.md`
Date: 2026-10-25. Blender half complete; the Unreal import (AC-3) done in the same day's build run (section AC-3).

## AC-1: one credit-gated generation (art bible 8.7 step 1, 8.9)

| Step | Meshy task id | Model | Credits |
| --- | --- | --- | --- |
| Dress the Body_Base front render (image-to-image) | `01a171df-b711-767f-93e8-dc910f798c47` | nano-banana-2 | 6 |
| **Image-to-3D, the one generation** (a-pose, textured 2K, remove_lighting, glb + fbx) | `01a171e0-2d5f-747c-a4bf-aa4b5d69bd07` | meshy-6 | 30 |
| Auto-rig of that generation, joint positions used as conform landmarks only | `01a171e9-d1a6-75da-8497-8d44b3e5a580` | (rigging) | 5 |
| **Total** | | | **41** of the 60 the user pre-authorised for this story |

- Cost confirmation: pre-authorised by the user for this unattended run (at most 60 credits for this story, from the
  1,760 left of the 2,000 unattended cap). After this story: 281 of 2,000 used, 1,719 left. Meshy balance 3,070 before and
  3,029 after (41 spent).
- Reference: `assets/characters/meshy/test_character/TestCharacter_ref_body_base_front.png`, rendered from Body_Base
  (reference A-pose) by `tools/asset-pipeline/render_body_base_reference.py`; dressed reference
  `TestCharacter_ref_dressed.png`. Full prompts, settings and rights notes: `TestCharacter.prov.json`.
- Cultural framing (1.4, 8.9 rule 5): a plain macehualli of Tenochtitlan in an undyed cotton tilmatli knotted at the
  shoulder over a maxtlatl, cactli sandals, hair tied at the nape, prompt grounded in the Codex Mendoza and the
  Florentine Codex; no feathers, headdress, jewellery, paint, weapons or sacrifice imagery. The output was checked:
  none of these appear.
- No second test-character generation was made; the Meshy task list shows one image-to-3D task for it.

## AC-2: normalisation (art bible 8.7 steps 2 to 11)

Script: `tools/asset-pipeline/normalise_meshy_character.py` (headless Blender 5.2.3, about 1 minute; the user's
open Blender scenes `StealthPipeline` / `StealthKit` were never touched). Full numbers:
`assets/characters/meshy/test_character/TestCharacter_normalise_report.txt`.

| Gate | Result |
| --- | --- |
| Arm extent, left (story 008 metric, `body_measure.arm_extent_cm`) | 69.55 cm vs reference 70.40 cm: **-1.21 %** (tolerance +/- 3 %) PASS |
| Arm extent, right | 69.73 cm vs 70.40 cm: **-0.95 %** PASS |
| Skeleton nodes, every exported FBX (LOD0, LOD1, LOD2, LOD3) | **88** (one `Root` named `root` + 87 `LimbNode`), no `Null`, PASS |
| Influences, every LOD | max 4, PASS |
| Never non-uniform scale | one object-level uniform scale (0.87195); proportions by the landmark inverse-skinning sculpt |

The Meshy figure came back with the R8 problem the Body_Base reference was meant to prevent, but milder: per segment
(Meshy / reference) upper arm 0.83 to 0.85, thigh 0.92, clavicle 0.92 to 0.93, head 1.14, hand 1.06 to 1.09. The
conform carries every segment onto the reference chains, so the arm metric lands within 1.3 %.

Other numbers recorded for the T1 audit (story 004; information here):

| Item | Value | Contract |
| --- | --- | --- |
| LOD0 / LOD1 / LOD2 / LOD3 triangles | 12,000 / 5,700 / 2,375 / 855 | 10k-14k / <= 6,000 / <= 2,500 / <= 900 |
| Head height (conformed high-poly, crown less 1 cm hair) | 19.14 cm | 19.3 +/- 1 |
| Hem (lowest garment point) | 46.9 cm (generated cloak reached 26 cm; trimmed at 47 cm) | 44.4 to 59.6 |
| Garment envelope front / side | 53.8 / 32.7 cm | 1.3.3 table (CR-OI1-4 pending) |
| Texel density (median, 8 px padding at 1024) | **4.40 / cm** | 5.12 (4.6 to 5.6): **short**, T1-15 risk |
| Bare-skin deviation from Body_Base | mean 1.10 cm, p95 3.78 cm | 1.0 / 1.5 (story 053 / T1-5): **over**, Tier C risk |
| Material slots / UV layers | 1 / 1 | 1 / 1 |
| Texture set | `textures/T_TestCharacter_D/_N/_ORM` 1024, `_M` 512 (R garment, G trim empty, B 0) | 1.3.5 |

## Retained images

`production/qa/evidence/story-003-meshy-test-character-generate-normalise/` (opened and viewed):

- `testcharacter-conform-vs-body-base.png`: Body_Base next to the conformed high-poly, front and side.
- `testcharacter-lod0-vs-body-base.png`: Body_Base next to LOD0, front and side (same arm, leg and head proportions).
- `testcharacter-lod0-four-view.png`: LOD0 with the baked, de-lit texture set (front, side, back, three-quarter).
- `testcharacter-lod0-pose-set.png`: LOD0 on the seven proxy poses of 1.3.5 (idle, walk, run, brace, assassination,
  vault, reversal counter).
- `stamp.txt`: content hashes of the scripts and the exported files.

Run result: OBSERVED (Blender renders above). Observations for story 004: the cloak follows the thighs, so the knee
shows through the cloth in walk / run and the hem flips up in vault (T4 judges the real clips); one stray triangle
in the vault tile; a small baked skin patch on the cloak's back panel; the right hand's fingers collapse at LOD2 /
LOD3 (arm metric -8 % / -12 % there, information only).

## AC-3: Unreal import (UE 5.8.4)

Tool: console command `Stealth.Characters.Import` (`Source/StealthGameEditor/Private/Characters/CharacterImport.cpp`),
run headless: `UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Stealth.Characters.Import
/Game/StealthGame/Characters/TestCharacter SKM_TestCharacter assets/characters/meshy/test_character/TestCharacter.fbx
..._LOD1.fbx ..._LOD2.fbx ..._LOD3.fbx,QUIT_EDITOR" -unattended -nullrhi`. Interchange with a transient copy of the
engine's `DefaultFBXOBJAssetsPipeline`: skeletal mesh only, skeleton `SK_UEFN_Mannequin`, no animation, no morph targets,
no physics asset, uniform scale 1.0, no rotation, materials and textures imported; LOD1-3 through
`USkeletalMeshEditorSubsystem::ImportLOD`. Only the packages under `/Game/StealthGame/Characters/TestCharacter/` were
saved, by explicit path (`SKM_TestCharacter`, `MI_Char_TestCharacter_Default`, `T_TestCharacter_D`, `T_TestCharacter_N`).

| Check | Result |
| --- | --- |
| "Skeleton is missing bones" (or any missing-bone) message | **none** in the run log |
| Bones / skeleton | **88**, `SK_UEFN_Mannequin`; 0 bones absent from the skeleton |
| Reference pose vs `SK_UEFN_Mannequin` (component space) | max position delta **0.0053 cm** (index_03_r), max rotation delta **0.0147 deg** (ik_foot_r) (art bible 1.3.2: 0.5 cm / 1 deg) |
| Skeleton package dirty after import / saved | **no** / never saved (`git status` shows no change under `Content/Characters/`) |
| Height (LOD0 imported bounds) | **166.29 cm** incl. hair and sandals (Body_Base 165.52, `SKM_UEFN_Mannequin` 165.57; contract 165.6 +/- 2); sole Z -0.01 |
| Facing | mesh-space forward (foot_l -> ball_l) (0.039, 0.999, 0) = `SKM_UEFN_Mannequin`'s exactly; `CBP_SandboxCharacter`'s mesh component has yaw -90, which maps mesh +Y to actor **+X** (art bible 8.6: "character faces Unreal +X") |
| LODs (triangles) | LOD0 12,000 / LOD1 5,700 / LOD2 2,375 / LOD3 855; one section and one material slot each |
| Wall time | engine import 1.7 s; AC-3 engine work about 20 min (12:35-12:55 UTC) + Blender half about 55 min: **about 1 h 15 min** of the 8 h cap (target 4 h) |

Log observations (information): Interchange warned "BindPose Matrix generation acquired 2 different Matrices from
FbxCluster vs FbxPose" for each FBX (the reference pose above still matches the skeleton to 0.005 cm), and "Possible LOD
pattern mismatch" for the `TestCharacter_LODn` names (the LODs were imported explicitly, so the pattern is unused).
`T_TestCharacter_ORM` / `_M` are not referenced by the FBX and were not imported (the master material, story 044, binds them).

Retained screenshot (opened and viewed):
`production/qa/evidence/story-003-meshy-test-character-generate-normalise/ue-skm-testcharacter-mesh-editor.png` — the
UE 5.8.4 skeletal mesh editor on `SKM_TestCharacter`, "Previewing Reference Pose", LOD0 12,000 triangles, 10,144
vertices, approx size 117 x 33 x 166: the dressed macehualli in the UEFN A-pose, undyed tilmatli knotted on the left
shoulder over the maxtlatl, hair tied at the nape, arms and hands at mannequin proportion, standing on the floor.
Visible issues (for story 004 T1-T4): a dark baked patch around the mouth / cheek of the face, and a few light texel
specks on the figure's left forearm (image right) and along the cloak's open edge. Only the front is in this view; the
back-panel skin patch noted in the Blender renders is not checked here.

## Sign-off

| Role | Name | Date | Verdict |
|---|---|---|---|
| technical-artist (Blender half) | Claude (unattended build run) | 2026-10-25 | [x] Approved |
| technical-artist (UE import, AC-3) | Claude (unattended build run) | 2026-10-25 | [x] Approved |
| art-director (after the UE import) | art-director role, unattended run authorised by the user | 2026-10-25 | [x] Approved |

Art-director note (unattended authorisation): approved as the **pipeline proof** this story asks for (one Meshy
character through generation, normalisation and a clean UE import onto the project skeleton): proportions read as the
mannequin's, silhouette and cultural framing match the roster-row cut (k = 2 knot), no prohibited elements. It is **not**
a T1-T4 pass: the face patch, texel density 4.40/cm, the bare-skin deviation (p95 3.78 cm) and the forearm / hem
seams go to story 004's audit, and the mass-production gate (characters 2-8) stays closed until story 004 and spikes
R1-R3 report.
