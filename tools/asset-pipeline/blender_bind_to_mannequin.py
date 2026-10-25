"""Bind a Meshy T-pose character to the UEFN mannequin skeleton for Unreal (Smoking Mirror pool pipeline).

Run inside Blender (e.g. via Blender MCP): set GLB_PATH, FBX_PATH and NAME, then exec this file.
Prerequisites in the current scene:
  - "root": the SK_UEFN_Mannequin armature with NO parent and scale 1.0 (metres), as built by
    tools/asset-pipeline/build_body_base.py (open or append assets/characters/body_base/Body_Base.blend);
  - WEIGHT_SOURCE (default "MannequinPosedSource"): the skinned mesh the weights are transferred from, in the
    same rest pose as the armature (Body_Base for an A-pose figure, art bible 8.7 step 9).
Steps: import GLB -> uniform-scale to mannequin height (1.656 m) -> feet on the ground, centred ->
transfer skin weights from the weight source -> parent with an Armature modifier -> export FBX
(armature + this mesh only, the frozen preset of build_body_base.FBX_EXPORT_PRESET). Unreal import:
SkeletalMeshTools.import_file with skeleton /Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.

Armature-export fix (CPA story 002): the prototype exported the 0.01-scale `SKM_UEFN_Mannequin` empty above
"root", which Unreal imported as an 89th bone. The armature object itself is now the FBX Root node `root`, no
empty or Null is exported, and the written file is checked for exactly 88 skeleton nodes.
"""
import os
import sys

import bpy
from mathutils import Vector

sys.dont_write_bytecode = True
# exec()'d from the Blender MCP there is no __file__; fall back to the repo-relative folder of the working directory
_HERE = (os.path.dirname(os.path.abspath(globals()["__file__"])) if "__file__" in globals()
         else os.path.join(os.getcwd(), "tools", "asset-pipeline"))
sys.path.insert(0, _HERE)
from build_body_base import FBX_EXPORT_PRESET  # noqa: E402
from body_measure import fbx_skeleton_nodes  # noqa: E402

MANNEQUIN_HEIGHT = 1.656
REFERENCE_BONE_COUNT = 88  # art bible 1.3.5, OI-1 (golden: UefnReferenceChains.txt bone_count)


def bind_character(glb_path, fbx_path, name, weight_source="MannequinPosedSource"):
    """Bind one Meshy GLB to the reference skeleton and export it; returns a summary dict.

    Raises RuntimeError when the armature is still parented or scaled (it would export an extra root bone or a
    scaled root) or when the written FBX does not carry exactly REFERENCE_BONE_COUNT skeleton nodes.
    """
    arm = bpy.data.objects["root"]
    src = bpy.data.objects[weight_source]
    if arm.parent is not None or any(abs(v - 1.0) > 1e-6 for v in arm.scale):
        raise RuntimeError("armature 'root' must be unparented at scale 1.0 (see build_body_base.py)")

    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=glb_path)
    meshes = [o for o in bpy.data.objects if o not in before and o.type == 'MESH']
    if not meshes:
        raise RuntimeError("GLB contained no mesh: " + glb_path)
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    cit = bpy.context.view_layer.objects.active
    cit.name = name

    # Fit: uniform scale to the mannequin's height, feet on the ground, centred in XY.
    bpy.context.view_layer.update()
    pts = [cit.matrix_world @ Vector(c) for c in cit.bound_box]
    height = max(p.z for p in pts) - min(p.z for p in pts)
    s = MANNEQUIN_HEIGHT / height
    cit.scale = tuple(v * s for v in cit.scale)
    bpy.context.view_layer.update()
    pts = [cit.matrix_world @ Vector(c) for c in cit.bound_box]
    cit.location -= Vector(((min(p.x for p in pts) + max(p.x for p in pts)) / 2,
                            (min(p.y for p in pts) + max(p.y for p in pts)) / 2,
                            min(p.z for p in pts)))
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Skin weights from the T-posed mannequin.
    dt = cit.modifiers.new("WeightTransfer", 'DATA_TRANSFER')
    dt.object = src
    dt.use_vert_data = True
    dt.data_types_verts = {'VGROUP_WEIGHTS'}
    dt.vert_mapping = 'POLYINTERP_NEAREST'
    dt.layers_vgroup_select_src = 'ALL'
    dt.layers_vgroup_select_dst = 'NAME'
    bpy.ops.object.datalayout_transfer(modifier=dt.name)
    bpy.ops.object.modifier_apply(modifier=dt.name)

    # Bind and export only this character with the skeleton.
    mw = cit.matrix_world.copy()
    cit.parent = arm
    cit.matrix_world = mw
    mod = cit.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in (arm, cit):
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    preset = dict(FBX_EXPORT_PRESET, path_mode="COPY", embed_textures=True)
    bpy.ops.export_scene.fbx(filepath=fbx_path, **preset)
    nodes = fbx_skeleton_nodes(fbx_path)
    skeleton = [n for n, t in nodes if t in ("Root", "LimbNode")]
    nulls = [n for n, t in nodes if t == "Null"]
    if len(skeleton) != REFERENCE_BONE_COUNT or nulls:
        raise RuntimeError(f"{fbx_path}: {len(skeleton)} skeleton nodes, Null nodes {nulls}; "
                           f"expected {REFERENCE_BONE_COUNT} and none")
    cit.hide_set(True)  # keep later exports from picking it up visually
    return {"name": name, "verts": len(cit.data.vertices), "groups": len(cit.vertex_groups), "scale": round(s, 4),
            "skeleton_nodes": len(skeleton)}


if __name__ == "__main__" or "GLB_PATH" in globals():
    print(bind_character(GLB_PATH, FBX_PATH, NAME))
