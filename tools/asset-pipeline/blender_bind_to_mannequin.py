"""Bind a Meshy T-pose character to the UEFN mannequin skeleton for Unreal (Smoking Mirror pool pipeline).

Run inside Blender (e.g. via Blender MCP): set GLB_PATH, FBX_PATH and NAME, then exec this file.
Prerequisites in the current scene (created once by the pipeline session, see
production/session-state/active.md "MESHY"):
  - "root": the SKM_UEFN_Mannequin armature imported from assets/characters/reference/SKM_UEFN_Mannequin.fbx,
    with its rest pose already converted to a T-pose (arms horizontal);
  - "MannequinPosedSource": a frozen copy of the mannequin mesh in that T-pose, used as the weight source;
  - "SKM_UEFN_Mannequin": the 0.01-scale empty the FBX importer created above "root".
Steps: import GLB -> uniform-scale to mannequin height (1.656 m) -> feet on the ground, centred ->
transfer skin weights from the posed mannequin -> parent with an Armature modifier -> export FBX
(empty + armature + this mesh only). Unreal import: SkeletalMeshTools.import_file with skeleton
/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.
"""
import bpy
from mathutils import Vector

MANNEQUIN_HEIGHT = 1.656


def bind_character(glb_path, fbx_path, name):
    arm = bpy.data.objects["root"]
    src = bpy.data.objects["MannequinPosedSource"]
    empty = bpy.data.objects["SKM_UEFN_Mannequin"]

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
    for o in (empty, arm, cit):
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=fbx_path, use_selection=True, object_types={'EMPTY', 'ARMATURE', 'MESH'},
                             add_leaf_bones=False, bake_anim=False, use_armature_deform_only=False,
                             mesh_smooth_type='FACE', path_mode='COPY', embed_textures=True)
    cit.hide_set(True)  # keep later exports from picking it up visually
    return {"name": name, "verts": len(cit.data.vertices), "groups": len(cit.vertex_groups), "scale": round(s, 4)}


if __name__ == "__main__" or "GLB_PATH" in globals():
    print(bind_character(GLB_PATH, FBX_PATH, NAME))
