# Evidence: Meshy test character (CPA story 003)

Story: `production/epics/character-pool-appearance/story-003-meshy-test-character-generate-normalise.md`
Date: 2026-10-25. Blender half complete; the Unreal import (AC-3) is DEFERRED (engine run pending: another
agent held the Unreal toolchain during this run).

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

## AC-3: Unreal import (DEFERRED)

Pending an engine run (another agent held the Unreal toolchain): import `TestCharacter.fbx` with
`TestCharacter_LOD1..3.fbx` against `SK_UEFN_Mannequin` (8.6 settings, never save the skeleton), check there is no
"Skeleton is missing bones" message and the skeleton is not dirty, capture the viewport screenshot here, record the
wall time (Blender half: about 55 minutes of the 8-hour cap, 06:55 to 07:50 UTC) and the art-director sign-off.

| Role | Name | Date | Verdict |
|---|---|---|---|
| technical-artist (Blender half) | Claude (unattended build run) | 2026-10-25 | [x] Approved |
| art-director (after the UE import) | | | [ ] Pending (AC-3 deferred) |
