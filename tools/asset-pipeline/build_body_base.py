"""Build Body_Base, the canonical pool conform target (art bible 1.3.9, OI-10; CPA story 002).

Run headless so the build never touches an artist's open session:
    blender -b --factory-startup --python tools/asset-pipeline/build_body_base.py
Writes assets/characters/body_base/Body_Base.blend and Body_Base.fbx (the FBX is the export-preset proof and the
conform/weight-donor copy for story 003; Body_Base itself is never imported to Unreal).

Procedure (all numbers from art bible 1.3.2 after OI-1, golden copy in
Source/StealthGameEditor/Private/Tests/CharacterPool/Golden/UefnReferenceChains.txt):
  1. Import the reference SKM_UEFN_Mannequin.fbx (A-pose, 165.6 cm, 88 bones).
  2. Flatten the importer's hierarchy: drop the 0.01-scale `SKM_UEFN_Mannequin` empty, apply its scale so the
     armature is metres at scale 1.0, and name the armature object `root`. The armature object IS the FBX
     `root` bone (exported as an FBX Root node), so the file carries root + 87 bones = 88 and no Null above it.
     This is the fix for the prototype's 89th bone (blender_bind_to_mannequin.py exported the empty).
  3. Body surface: voxel-remesh the 61-piece mannequin into one watertight unisex body (bare under-garment
     surface, neutral featureless head form), then decimate to a proxy density.
  4. Weights: nearest-face-interpolated Data Transfer from the UEFN body mesh, then the correction pass (the
     scripted equivalent of the hand correction): smoothing at shoulders, elbows, hips and knees, and a blend
     toward bone-heat weights at shoulders and hips (HEAT_BLEND); limit to 4 influences, normalise.
  5. Save the .blend (collections `Body_Base` = armature + body, `Reference` = hidden UEFN mesh, never exported)
     and export the FBX with the frozen preset below.
"""
import os
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
REF_FBX = os.path.join(REPO, "assets", "characters", "reference", "SKM_UEFN_Mannequin.fbx")
OUT_DIR = os.path.join(REPO, "assets", "characters", "body_base")

VOXEL_SIZE_M = 0.006          # remesh resolution; fills the mannequin's joint gaps, keeps fingers apart
TARGET_TRIS = 24000           # proxy density (Body_Base is never exported, so the 12k pool budget does not apply)
MAX_INFLUENCES = 4            # art bible 1.3.5
# Weight-correction pass per joint group: (joints, radius in m, smoothing passes). The shoulder seam of the
# segmented mannequin is the widest crease, so it gets the widest blend.
CORRECTION = [
    (["upperarm_l", "upperarm_r"], 0.12, 20),
    (["lowerarm_l", "lowerarm_r"], 0.06, 3),
    (["thigh_l", "thigh_r"], 0.08, 4),
    (["calf_l", "calf_r"], 0.06, 3),
]
CORRECTION_FACTOR = 0.5
# Shoulders and hips: the transferred weights keep the mannequin's rigid segment seams (deltoid ball, buttock),
# which intersect when the arm hangs (idle, walk) or the hip flexes 90 degrees (vault). Inside these radii the
# weights are blended toward Blender bone-heat weights computed on the watertight body: full heat weights
# within half the radius, a linear falloff to the transferred weights at the radius.
HEAT_BLEND = [(["upperarm_l", "upperarm_r"], 0.14), (["thigh_l", "thigh_r"], 0.16)]

# Frozen Blender side of the FBX export preset (art bible 8.6). Recorded in production/qa/evidence/body-base-evidence.md.
FBX_EXPORT_PRESET = dict(
    use_selection=True, object_types={"ARMATURE", "MESH"},
    apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", global_scale=1.0,
    axis_forward="-Z", axis_up="Y", use_space_transform=True, bake_space_transform=False,
    add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
    armature_nodetype="ROOT", use_armature_deform_only=False,
    mesh_smooth_type="FACE", use_tspace=True, use_triangles=True, use_mesh_modifiers=True,
    bake_anim=False, path_mode="AUTO", embed_textures=False, use_custom_props=False,
)


def _apply_modifier(obj, mod):
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj]):
        bpy.ops.object.modifier_apply(modifier=mod.name)


def import_reference():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "Body_Base"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    bpy.ops.import_scene.fbx(filepath=REF_FBX, use_anim=False, ignore_leaf_bones=False,
                             automatic_bone_orientation=False)
    empties = [o for o in bpy.data.objects if o.type == "EMPTY"]
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    src = next(o for o in bpy.data.objects if o.type == "MESH")
    return empties, arm, src


def flatten_hierarchy(empties, arm, src):
    for o in (arm, src):
        mw = o.matrix_world.copy()
        o.parent = None
        o.matrix_world = mw
    for e in empties:
        bpy.data.objects.remove(e, do_unlink=True)
    with bpy.context.temp_override(selected_editable_objects=[arm, src], selected_objects=[arm, src],
                                   active_object=arm, object=arm):
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for pb in arm.pose.bones:  # rest pose = reference A-pose
        pb.matrix_basis.identity()
    arm.name = "root"
    arm.data.name = "root"
    src.name = "UEFN_Ref_Mesh"
    src.parent = arm
    for m in src.modifiers:
        if m.type == "ARMATURE":
            m.object = arm
    if "root" in arm.data.bones:
        raise RuntimeError("armature already has a 'root' bone; the export would carry 89 bones")


def build_surface(src):
    body = src.copy()
    body.data = src.data.copy()
    body.name = "Body_Base"
    body.data.name = "Body_Base"
    bpy.context.scene.collection.objects.link(body)
    body.parent = None
    body.modifiers.clear()
    body.vertex_groups.clear()
    rm = body.modifiers.new("Remesh", "REMESH")
    rm.mode = "VOXEL"
    rm.voxel_size = VOXEL_SIZE_M
    rm.use_smooth_shade = True
    _apply_modifier(body, rm)
    tris = sum(len(p.vertices) - 2 for p in body.data.polygons)
    if tris > TARGET_TRIS:
        dec = body.modifiers.new("Decimate", "DECIMATE")
        dec.decimate_type = "COLLAPSE"
        dec.ratio = TARGET_TRIS / tris
        _apply_modifier(body, dec)
    for p in body.data.polygons:
        p.use_smooth = True
    return body


def transfer_weights(body, src, arm):
    for g in src.vertex_groups:
        body.vertex_groups.new(name=g.name)
    dt = body.modifiers.new("WeightTransfer", "DATA_TRANSFER")
    dt.object = src
    dt.use_vert_data = True
    dt.data_types_verts = {"VGROUP_WEIGHTS"}
    dt.vert_mapping = "POLYINTERP_NEAREST"
    dt.layers_vgroup_select_src = "ALL"
    dt.layers_vgroup_select_dst = "NAME"
    _apply_modifier(body, dt)


def _read_weights(body):
    W = []
    for v in body.data.vertices:
        W.append({g.group: g.weight for g in v.groups if g.weight > 0.0})
    return W


def _write_weights(body, W):
    vgs = body.vertex_groups
    for vg in vgs:
        vg.remove(range(len(body.data.vertices)))
    for i, w in enumerate(W):
        for gi, val in w.items():
            vgs[gi].add([i], val, "REPLACE")


def _heat_weights(body, arm):
    """Bone-heat weights of a temporary copy of the body, keyed by vertex index -> {bone name: weight}.

    Only bones that carry weights on the reference mesh deform (ik_*, attach, weapon and root are excluded).
    """
    weighted = {g.name for g in body.vertex_groups}
    saved = {b.name: b.use_deform for b in arm.data.bones}
    for b in arm.data.bones:
        b.use_deform = b.name in weighted
    tmp = body.copy()
    tmp.data = body.data.copy()
    bpy.context.scene.collection.objects.link(tmp)
    tmp.modifiers.clear()
    tmp.vertex_groups.clear()
    tmp.parent = None
    with bpy.context.temp_override(selected_objects=[tmp, arm], selected_editable_objects=[tmp, arm],
                                   active_object=arm, object=arm):
        bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    for b in arm.data.bones:
        b.use_deform = saved[b.name]
    names = {g.index: g.name for g in tmp.vertex_groups}
    heat = [{names[g.group]: g.weight for g in v.groups if g.weight > 0.0} for v in tmp.data.vertices]
    mesh = tmp.data
    bpy.data.objects.remove(tmp, do_unlink=True)
    bpy.data.meshes.remove(mesh)
    if not any(heat):
        raise RuntimeError("bone heat weighting produced no weights")
    return heat


def _blend_heat_weights(body, arm, W):
    heat = _heat_weights(body, arm)
    index = {g.name: g.index for g in body.vertex_groups}
    for names, radius in HEAT_BLEND:
        joints = [arm.matrix_world @ arm.data.bones[n].head_local for n in names]
        for i, v in enumerate(body.data.vertices):
            d = min((v.co - j).length for j in joints)
            if d >= radius:
                continue
            t = 1.0 if d < 0.5 * radius else (radius - d) / (0.5 * radius)
            new = {}
            for name, val in heat[i].items():
                if name not in index:
                    index[name] = body.vertex_groups.new(name=name).index
                new[index[name]] = val
            keys = set(W[i]) | set(new)
            W[i] = {g: (1.0 - t) * W[i].get(g, 0.0) + t * new.get(g, 0.0) for g in keys}


def correct_and_limit(body, arm):
    """Correction pass (smoothing + heat blend, see CORRECTION / HEAT_BLEND), then limit to 4 and normalise."""
    W = _read_weights(body)
    nbr = [[] for _ in body.data.vertices]
    for e in body.data.edges:
        a, b = e.vertices
        nbr[a].append(b)
        nbr[b].append(a)
    total = set()
    for names, radius, passes in CORRECTION:
        joints = [arm.matrix_world @ arm.data.bones[n].head_local for n in names]
        mask = [i for i, v in enumerate(body.data.vertices) if any((v.co - j).length < radius for j in joints)]
        total.update(mask)
        for _ in range(passes):
            newW = {}
            for i in mask:
                acc = {}
                for n in nbr[i]:
                    for g, val in W[n].items():
                        acc[g] = acc.get(g, 0.0) + val
                k = len(nbr[i]) or 1
                keys = set(acc) | set(W[i])
                newW[i] = {g: (1 - CORRECTION_FACTOR) * W[i].get(g, 0.0) + CORRECTION_FACTOR * acc.get(g, 0.0) / k
                           for g in keys}
            for i, w in newW.items():
                W[i] = w
    _blend_heat_weights(body, arm, W)
    deform = {g.index for g in body.vertex_groups if g.name in arm.data.bones}
    for i, w in enumerate(W):
        w = {g: val for g, val in w.items() if g in deform and val > 1e-4}
        top = sorted(w.items(), key=lambda kv: kv[1], reverse=True)[:MAX_INFLUENCES]
        s = sum(val for _, val in top)
        W[i] = {g: val / s for g, val in top} if s > 0 else {}
    _write_weights(body, W)
    for vg in list(body.vertex_groups):  # drop groups that ended up empty
        if not any(vg.index in W[i] for i in range(len(W))):
            body.vertex_groups.remove(vg)
    return len(total)


def bind(body, arm):
    body.parent = arm
    body.matrix_parent_inverse.identity()
    mod = body.modifiers.new("Armature", "ARMATURE")
    mod.object = arm


def organise(arm, body, src):
    scene = bpy.context.scene
    col_main = bpy.data.collections.new("Body_Base")
    col_ref = bpy.data.collections.new("Reference")
    scene.collection.children.link(col_main)
    scene.collection.children.link(col_ref)
    for o, c in ((arm, col_main), (body, col_main), (src, col_ref)):
        for uc in list(o.users_collection):
            uc.objects.unlink(o)
        c.objects.link(o)
    col_ref.hide_render = True
    src.hide_set(True)
    src.hide_render = True


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    empties, arm, src = import_reference()
    flatten_hierarchy(empties, arm, src)
    body = build_surface(src)
    transfer_weights(body, src, arm)
    corrected = correct_and_limit(body, arm)
    bind(body, arm)
    organise(arm, body, src)
    blend = os.path.join(OUT_DIR, "Body_Base.blend")
    fbx = os.path.join(OUT_DIR, "Body_Base.fbx")
    bpy.ops.wm.save_as_mainfile(filepath=blend, compress=True)
    for o in bpy.context.view_layer.objects:
        o.select_set(o in (arm, body))
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=fbx, **FBX_EXPORT_PRESET)
    print("BODY_BASE_BUILT", {"verts": len(body.data.vertices),
                              "tris": sum(len(p.vertices) - 2 for p in body.data.polygons),
                              "bones": len(arm.data.bones) + 1, "corrected_verts": corrected,
                              "blend": blend, "fbx": fbx})


if __name__ == "__main__":
    main()
