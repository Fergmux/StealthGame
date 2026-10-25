# Body_Base evidence (CPA story 002, art bible 1.3.9, OI-10)

**Story**: `production/epics/character-pool-appearance/story-002-body-base-blender-build-oi10.md`
**Date**: 2026-10-25
**Tool**: Blender 5.2.3 LTS, headless (`--factory-startup`), so the artist's open session was never touched
**Reference**: `SK_UEFN_Mannequin`, 165.6 cm, 88 bones (art bible 1.3.2 / 1.3.5, OI-1 closed by story 001)

## Retained files

| File | What |
|---|---|
| `assets/characters/body_base/Body_Base.blend` | Accepted file: collection `Body_Base` (armature `root` + mesh `Body_Base`), collection `Reference` (hidden `UEFN_Ref_Mesh`, the weight donor and conformity reference, never exported) |
| `assets/characters/body_base/Body_Base.fbx` | Export-preset proof and the conform / weight-donor copy for story 003. Never imported to Unreal |
| `assets/characters/body_base/Body_Base_T1_report.txt` | T1 audit output (items 1-5, 17-18, pose set), `RESULT: PASS` |
| `production/qa/evidence/story-002-body-base-blender-build-oi10/body-base-four-view.png` | Four-view render (front, side, back, three-quarter), rest A-pose, Workbench |
| `production/qa/evidence/story-002-body-base-blender-build-oi10/body-base-pose-set.png` | One tile per proxy pose: idle, walk, run, brace, assassination, vault, reversal counter |
| `tools/asset-pipeline/build_body_base.py` | Reproducible build (import, hierarchy fix, remesh, weights, save, export) |
| `tools/asset-pipeline/body_base_t1_audit.py` | T1 audit + pose-set check + renders (`-- --render`); exit 0 = pass |
| `tools/asset-pipeline/body_measure.py` | Measurement definitions shared with story 001 and story 008 |

Reproduce: `blender -b --factory-startup --python tools/asset-pipeline/build_body_base.py`, then
`blender -b --factory-startup --python tools/asset-pipeline/body_base_t1_audit.py -- --render`.
Two consecutive build + audit runs gave identical gated values (one informational crease-pair count differed by 1).

## What Body_Base is

The 61-piece `SKM_UEFN_Mannequin` mesh voxel-remeshed (0.6 cm) into one watertight, manifold, unisex
under-garment body with the mannequin's neutral featureless head form, decimated to 24,000 triangles
(12,002 vertices; a proxy, never exported, so the 12k pool budget does not apply). One material slot.
Bare feet: allowed here only because Body_Base never ships (the barefoot ban applies to pool models; the
shared cactli sandal is added per model).

## T1 items 1-5 and 17-18 (art bible 1.3.7), from the report

| Item | Result | Measured | Contract |
|---|---|---|---|
| 1 Stature | PASS | 165.52 cm | 165.6 +/- 2 |
| 2 Head height | PASS | 19.26 cm (chin 146.25) | 19.3 +/- 1 |
| 3 Bone chains | PASS | worst 0.041 % (all nine chains + hand straight line) | +/- 3 % |
| 4 Shoulder / chest / hip (+ foot) | PASS | 50.79 / 27.53 / 34.71 (foot 26.15) cm | 51.0 / 27.6 / 34.8 +/- 2 (foot 26.3 +/- 1.5) |
| 5 Conformity | PASS | mean 0.043, p95 0.210, max 0.472 cm to the UEFN reference body | mean <= 1.0, p95 <= 1.5 |
| 17 Skeleton, extra bones, influences | PASS | FBX skeleton nodes 88 (`root` = FBX Root + 87 LimbNode), no Null; names and parent hierarchy equal the reference; max 4 influences, 0 vertices above 4 | 88, zero extra, <= 4 |
| 18 Rest pose, scale, origin | PASS | .blend bones vs reference: 0.0000 cm / 0.0000 deg; exported FBX re-imported vs reference: 0.0009 cm / 0.0000 deg; object transforms identity; sole Z -0.01 cm | 0.5 cm / 1 deg; scale 1.0; Z = 0 +/- 0.5 |

## AC-2: weights and the pose set

- **Weights**: nearest-face-interpolated Data Transfer from the UEFN body mesh, then the correction pass
  (scripted, in place of hand painting): neighbour smoothing at shoulders (12 cm, 20 passes), elbows, hips
  and knees, and a blend toward bone-heat weights at shoulders (14 cm) and hips (16 cm). Transfer alone kept
  the mannequin's rigid segment seams: the deltoid sank 1.7 to 1.9 cm into the back in idle / walk and the
  buttock 4+ cm into the thigh in vault; the blend brought every pose under 1 cm. Limit 4, normalised.
- **Bones**: 88; the armature object is the FBX `root` node (Root type); the prototype's extra
  `SKM_UEFN_Mannequin` empty is not exported.
- **Pose set** (proxy joint angles for walk, run, idle, brace, assassination, vault, reversal counter; the
  motion-matched clips live in Unreal):

| Pose | Max intersection (cm) | Max joint twist (deg) |
|---|---|---|
| idle | 0.87 (torso / arm_l) | 0.00 |
| walk | 0.86 (torso / arm_r) | 0.00 |
| run | 0.00 | 0.00 |
| brace | 0.00 | 0.00 |
| assassination | 0.00 | 4.00 (spine) |
| vault | 0.00 | 0.00 |
| reversal counter | 0.00 | 6.00 (spine) |

  Intersections are exact triangle-triangle crossings between body regions (arms, legs, torso) more than
  6 cm apart along the surface, depth by flood-fill of the buried patch; crossings inside one region (knee
  pit, elbow crease at 90+ degrees) are crease compression and are listed, not gated. Twist is the swing-twist
  decomposition of each deform joint's rotation relative to its parent.
- **Informational**: at 90+ degrees of knee flexion (brace, vault) faces in the knee pit compress to 2 to 3 %
  of their rest area; this is crease compression, not a candy-wrap twist. Re-check on real clips in T4.

## Blender side of the FBX export preset (art bible 8.6), frozen

`build_body_base.FBX_EXPORT_PRESET` (also used by `blender_bind_to_mannequin.py`):
selection only, object types Armature + Mesh (no Empty), Apply Unit Scale on, Apply Scalings = FBX Units
Scale, scale 1.0, Forward -Z, Up Y, Add Leaf Bones off, primary bone axis Y, secondary X, **armature node
type Root** (the armature object named `root` becomes the FBX Root node `root`), deform-only off, smoothing
Face, tangent space on, triangulate on, modifiers applied, no animation. Scene: metric, unit scale 1.0, the
armature unparented at scale 1.0 (metres). Written file: UnitScaleFactor 100 (cm), UpAxis Y, FrontAxis Z,
CoordAxis X; Root node scale 1.0.

Verified in Blender (round trip: re-import of `Body_Base.fbx` matches the reference skeleton to 0.0009 cm and
0 degrees; parse of the file shows 88 skeleton nodes and no Null). `blender_bind_to_mannequin.py` smoke test
on `citizen_02_tpose.glb` with this preset: 88 skeleton nodes (was 89).
**DEFERRED (engine run pending):** the Unreal side of the round trip (import onto `SK_UEFN_Mannequin` with no
"Skeleton is missing bones" message, character faces +X at 165.6 uu) needs the editor, which this run was
not allowed to use. Body_Base itself is never imported (1.3.9); the check uses its FBX or a bound test mesh.

## Not in Content/

`find Content -iname "*body*base*"` returns only `Content/MetaHumans/Kellan/Body/Textures/T_Body_BaseColor.uasset`
(unrelated MetaHuman texture). No Body_Base asset exists under `Content/`.

## Sign-off

| Role | Name | Date | Verdict |
|---|---|---|---|
| technical-artist (build, T1 run) | Claude (unattended build run) | 2026-10-25 | [x] Approved |
| art-director (1.3.9 acceptance: T1 items 1-5, 17-18, renders viewed) | art-director role, unattended run authorised by the user (decision log 2026-10-25) | 2026-10-25 | [x] Approved |
