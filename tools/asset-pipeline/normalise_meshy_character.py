"""Normalise one Meshy character onto the UEFN reference skeleton (art bible 8.7 steps 2-11; CPA story 003).

Run headless (never touches an open Blender session or its scenes):
    blender -b --factory-startup --python tools/asset-pipeline/normalise_meshy_character.py -- \
        --glb <meshy.glb> --rig-glb <meshy_rig.glb> --name TestCharacter --out assets/characters/meshy/test_character

Inputs: the unedited Meshy GLB (image-to-3D), the Meshy auto-rig GLB of the same task (read ONLY for its joint
positions, used as landmarks; its skeleton is discarded), and the accepted Body_Base
(assets/characters/body_base/Body_Base.blend, art bible 1.3.9). Numbers come from the golden file
Source/StealthGameEditor/Private/Tests/CharacterPool/Golden/UefnReferenceChains.txt.

Steps (8.7 numbering):
  2. New scene from Body_Base.blend: collections Meshy_Original (the GLB, hidden, high-poly, data untouched),
     Work (the armature copy `root` and the model), Body_Proxy (Body_Base, never exported), Manny_Ref (the
     UEFN reference mesh, never exported).
  3. One uniform scale to the reference stature (bounding height incl. hair and sandal sole), soles at Z = 0,
     centred between the feet. Proportions are then conformed by a landmark-driven inverse-skinning sculpt:
     each body segment (torso, neck, head, clavicle, upper arm, forearm, hand, thigh, calf, foot) gets the
     affine map that carries the reference joints onto the Meshy joints (rotation + stretch ALONG the segment;
     head and hand uniform); Body_Base, forward-mapped by those segments, donates per-vertex segment weights,
     and every Meshy vertex is pulled back through the inverse of its blended map. The model therefore lands
     on the reference chains (chains win, 1.3.2) without any object-level non-uniform scale.
  4/5. The result is in the reference frame by construction: rest pose = the reference A-pose, faces -Y in
     Blender (+X in Unreal through the frozen FBX preset), origin between the feet.
  6. Retopology: voxel remesh (one closed shell) -> garment below HEM_Z_M cut and capped (the generated cloak
     hangs to mid-shin; 1.3.3 hem) -> faces no camera-side ray escapes from deleted (skin under the garment,
     voxel cavities) -> QuadriFlow, or, when QuadriFlow rejects the shell (Blender 5.2.3 headless does), UV
     projection on the dense shell then decimation -> triangulate, 12k triangles.
  7. UV0: smart projection (89 deg), packed with 8 px padding at 1024, scaled down to the median texel density
     5.12 / cm when it packs denser (a lower packed density is reported, never inflated: 8.4 UV fill check).
  8. Bake BaseColor (diffuse colour, no lighting), tangent normal and AO from the conformed high-poly; de-light
     to the 1.3.4 bands: garment flattened to the neutral cotton base (luminance detail around the garment
     mean, swatch colour comes from the master material), skin and hair re-centred into their L* bands; ORM =
     AO / per-region roughness / metallic 0; region mask _M (R garment, G trim, B 0) at 512.
  9. Rig: parent to the armature copy `root` (88 bones, names untouched), nearest-face-interpolated weight
     transfer from Body_Base; scripted hand correction: garment limited to pelvis / spine / neck / clavicle /
     thigh and smoothed, hair rigid to `head`, arm-chain weight capped medial of the reference arm start (neck);
     limit 4 influences, normalise.
 10. LOD1-3 by decimation to the 1.3.5 ceilings; finger and twist weights folded into hand / parent at LOD2+.
 11. Export LOD0 and LOD1-3 as separate FBX files with the frozen preset (build_body_base.FBX_EXPORT_PRESET);
     each file is checked for exactly 88 skeleton nodes and no Null.
Writes <out>/<name>.blend, <name>.fbx, <name>_LOD1..3.fbx, textures/T_<name>_{D,N,ORM,M}.png,
<name>_normalise_report.txt and renders to --evidence.
"""
import argparse
import math
import os
import sys
import time

sys.dont_write_bytecode = True
import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import body_measure as bm  # noqa: E402
from build_body_base import FBX_EXPORT_PRESET  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
BODY_BASE_BLEND = os.path.join(REPO, "assets", "characters", "body_base", "Body_Base.blend")
GOLDEN = os.path.join(REPO, "Source", "StealthGameEditor", "Private", "Tests", "CharacterPool", "Golden",
                      "UefnReferenceChains.txt")

HAIR_ALLOWANCE_M = 0.01        # hair above the skull on the Meshy head (1.3.3 allows up to 4 cm); crown = top - this
WEIGHT_SMOOTH_ITERS = 12       # Laplacian passes on the conform segment weights (keeps cloth from tearing)
VOXEL_SIZE_M = 0.006           # retopology pre-pass
QUADRIFLOW_FACES = 6000        # quads -> about 12k triangles (1.3.5 LOD0 12,000 +/- 2,000)
LOD0_TRIS = (10000, 14000)
LOD_CEILINGS = {1: 6000, 2: 2500, 3: 900}
TEX = 1024
MASK_TEX = 512
TEXEL_DENSITY = 5.12           # texels / cm (1.3.5, 4.6 to 5.6)
UV_PAD_PX = 8
UV_ANGLE_DEG = 89.0            # smart-projection angle: fewer, larger islands pack denser at 8 px padding
CLOTH_OFFSET_M = 0.02          # vertices further than this from Body_Base are cloth: their weights are smoothed
CLOTH_SMOOTH_ITERS = 20
HEM_Z_M = 0.47                 # garment trimmed below this height (1.3.3 / OI-1 block hem 44.4 to 59.6 cm)
FRAGMENT_FACES = 400            # connected pieces smaller than this are removed after the hem cut
NECK_BAND_M = 0.03              # lateral fade of the neck/trapezius weight correction
CAMERA_MIN_DZ = -0.10          # escape directions below this z component do not count as visible
HIDDEN_RAYS = 24               # hemisphere rays per face for the hidden-face test (8.7 step 6)
CLOTH_BAND_RINGS = 12           # edge rings over which the cloth weight restriction fades in from skin
MAX_INFLUENCES = 4
# 1.3.4 bands used by the de-light step (CIELAB)
SKIN_L, SKIN_L_BAND, SKIN_C, SKIN_HUE = 48.0, (44.0, 52.0), 24.0, (45.0, 65.0)
HAIR_L, HAIR_L_BAND, HAIR_C_MAX = 16.0, (10.0, 22.0), 8.0
GARMENT_BASE_L = 61.0          # neutral cotton base at the swatch corridor (4.3: all swatches L* 60 to 62)
GARMENT_DETAIL = 0.35          # luminance detail kept around the garment mean (fraction of the source contrast)
ROUGHNESS = {"garment": 0.80, "skin": 0.57, "hair": 0.68, "sandal": 0.70}   # 1.3.4 bands

# Reference bones driven by each conform segment; bones not listed follow their nearest listed ancestor.
SEGMENT_BONES = {
    "torso": ["pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05"],
    "neck": ["neck_01", "neck_02"],
    "head": ["head"],
}
for _s in ("l", "r"):
    SEGMENT_BONES.update({
        "clav_" + _s: ["clavicle_" + _s],
        "upper_" + _s: ["upperarm_" + _s, "upperarm_twist_01_" + _s, "upperarm_twist_02_" + _s],
        "fore_" + _s: ["lowerarm_" + _s, "lowerarm_twist_01_" + _s, "lowerarm_twist_02_" + _s],
        "hand_" + _s: ["hand_" + _s],
        "thigh_" + _s: ["thigh_" + _s, "thigh_twist_01_" + _s],
        "calf_" + _s: ["calf_" + _s, "calf_twist_01_" + _s, "calf_twist_02_" + _s],
        "foot_" + _s: ["foot_" + _s],
    })
MESHY_SIDE = {"l": "Left", "r": "Right"}   # Meshy auto-rig names; its Left is the figure's left (+X), like _l


def log(*a):
    print("[normalise]", *a, flush=True)


def read_golden(path):
    g = {}
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.split("#", 1)[0].strip()
            if "=" in line:
                k, v = line.split("=", 1)
                g[k.strip()] = v.strip()
    return g


def mesh_world_np(obj):
    me = obj.data
    co = np.empty(len(me.vertices) * 3)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    m = np.array(obj.matrix_world)
    return co @ m[:3, :3].T + m[:3, 3]


def set_mesh_np(obj, world):
    m = np.array(obj.matrix_world.inverted())
    local = world @ m[:3, :3].T + m[:3, 3]
    obj.data.vertices.foreach_set("co", local.ravel())
    obj.data.update()


def move_to(obj, col):
    for uc in list(obj.users_collection):
        uc.objects.unlink(obj)
    col.objects.link(obj)


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    return [o for o in bpy.data.objects if o not in before]


def edges_np(me):
    e = np.empty(len(me.edges) * 2, dtype=np.int64)
    me.edges.foreach_get("vertices", e)
    return e.reshape(-1, 2)


def laplacian(values, edges, iters, mask=None, factor=0.5):
    """Smooth per-vertex rows of `values` over mesh edges (only rows in `mask` change when given)."""
    n = len(values)
    deg = np.bincount(edges.ravel(), minlength=n).astype(np.float64)
    deg[deg == 0] = 1.0
    v = values.copy()
    for _ in range(iters):
        acc = np.zeros_like(v)
        np.add.at(acc, edges[:, 0], v[edges[:, 1]])
        np.add.at(acc, edges[:, 1], v[edges[:, 0]])
        new = (1.0 - factor) * v + factor * acc / deg[:, None]
        if mask is not None:
            v[mask] = new[mask]
        else:
            v = new
    return v


# --------------------------------------------------------------------------------------------- step 2 / 3: setup
def setup_scene(name, glb):
    bpy.ops.wm.open_mainfile(filepath=BODY_BASE_BLEND)
    scene = bpy.context.scene
    scene.name = name
    arm = bpy.data.objects["root"]
    body = bpy.data.objects["Body_Base"]
    ref = bpy.data.objects["UEFN_Ref_Mesh"]
    old = [c for c in bpy.data.collections]
    cols = {}
    for n in ("Meshy_Original", "Work", "Body_Proxy", "Manny_Ref"):
        cols[n] = bpy.data.collections.new(n)
        scene.collection.children.link(cols[n])
    move_to(arm, cols["Work"])
    move_to(body, cols["Body_Proxy"])
    move_to(ref, cols["Manny_Ref"])
    body.name = "Body_Proxy"
    for c in old:
        if not c.objects:
            bpy.data.collections.remove(c)
    new = import_glb(glb)
    meshes = [o for o in new if o.type == "MESH"]
    for o in new:
        if o.type != "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    if len(meshes) > 1:
        with bpy.context.temp_override(active_object=meshes[0], selected_editable_objects=meshes,
                                       selected_objects=meshes):
            bpy.ops.object.join()
    orig = meshes[0]
    orig.name = "Meshy_Original"
    mw = orig.matrix_world.copy()
    orig.parent = None
    orig.matrix_world = mw
    move_to(orig, cols["Meshy_Original"])
    return scene, cols, arm, body, ref, orig


def normalise_original(orig, stature_m):
    """One uniform scale (object transform only, mesh data untouched) to the stature; soles at 0; centred."""
    W = mesh_world_np(orig)
    h = W[:, 2].max() - W[:, 2].min()
    s = stature_m / h
    c = np.array([(W[:, 0].min() + W[:, 0].max()) / 2, (W[:, 1].min() + W[:, 1].max()) / 2, W[:, 2].min()])
    orig.matrix_world = (Matrix.Translation(Vector(tuple(-c * s))) @ Matrix.Scale(s, 4)) @ orig.matrix_world
    bpy.context.view_layer.update()
    return s


def meshy_landmarks(rig_glb, frame_ref):
    """Joint heads (m) of the Meshy auto-rig, re-framed onto the normalised original (same bounding box)."""
    new = import_glb(rig_glb)
    arm = next(o for o in new if o.type == "ARMATURE")
    mesh = max((o for o in new if o.type == "MESH"), key=lambda o: len(o.data.vertices))
    J = {b.name: np.array(arm.matrix_world @ b.head_local) for b in arm.data.bones}
    bpy.context.view_layer.update()
    R = mesh_world_np(mesh)
    rmin, rmax = R.min(0), R.max(0)
    fmin, fmax = frame_ref.min(0), frame_ref.max(0)
    s = (fmax[2] - fmin[2]) / (rmax[2] - rmin[2])
    rc = np.array([(rmin[0] + rmax[0]) / 2, (rmin[1] + rmax[1]) / 2, rmin[2]])
    fc = np.array([(fmin[0] + fmax[0]) / 2, (fmin[1] + fmax[1]) / 2, fmin[2]])
    J = {k: (v - rc) * s + fc for k, v in J.items()}
    for o in new:
        data = o.data
        bpy.data.objects.remove(o, do_unlink=True)
        if data is not None and data.users == 0:
            if isinstance(data, bpy.types.Mesh):
                bpy.data.meshes.remove(data)
            elif isinstance(data, bpy.types.Armature):
                bpy.data.armatures.remove(data)
    return J, {"rig_frame_scale": round(float(s), 5)}


def chin_and_crown(V, shoulder_z, hair=0.0):
    """Chin height (front-profile rule of body_measure.body_dimensions_cm, |X| < 5 cm) and crown height (m)."""
    top = V[:, 2].max()
    crown = top - hair
    near = V[np.abs(V[:, 0]) < 0.05]

    def front(z):
        sl = near[np.abs(near[:, 2] - z) < 0.003]
        return sl[:, 1].min() if len(sl) else None

    zs = np.arange(shoulder_z + 0.03, top - 0.02, 0.002)
    prof = [(z, front(z)) for z in zs]
    prof = [(z, f) for z, f in prof if f is not None]
    face = min(f for z, f in prof if z > top - 0.15)
    neck_z, neck = max(((z, f) for z, f in prof if z < top - 0.17), key=lambda zf: zf[1])
    half = 0.5 * (face + neck)
    chin = next(z for z, f in prof if z > neck_z and f <= half)
    return float(chin), float(crown)


def fingertip(V, wrist, elbow):
    """Farthest vertex from the wrist inside the hand cylinder beyond the wrist (m)."""
    u = (wrist - elbow) / np.linalg.norm(wrist - elbow)
    rel = V - wrist
    t = rel @ u
    r = np.linalg.norm(rel - np.outer(t, u), axis=1)
    sel = (t > 0.0) & (r < 0.09) & (np.linalg.norm(rel, axis=1) < 0.30)
    d = np.linalg.norm(rel[sel], axis=1)
    return V[sel][np.argmax(d)]


def affine(rh, rt, mh, mt, mode):
    """4x4 map of reference segment (rh -> rt) onto Meshy segment (mh -> mt)."""
    dr, dm = Vector(rt - rh), Vector(mt - mh)
    rot = dr.normalized().rotation_difference(dm.normalized()).to_matrix().to_4x4()
    k = dm.length / dr.length
    if mode == "uniform":
        S = Matrix.Scale(k, 4)
    elif mode == "axial":
        S = Matrix.Scale(k, 4, dr.normalized())
    elif mode == "translate":
        rot, S = Matrix.Identity(4), Matrix.Identity(4)
    else:
        raise ValueError(mode)
    return np.array(Matrix.Translation(Vector(mh)) @ rot @ S @ Matrix.Translation(-Vector(rh))), k


def build_segments(arm, J, ref_V, meshy_V, ref_chin):
    """Per-segment 4x4 maps reference -> Meshy, with the landmark table for the report.

    `ref_chin` (m) is Body_Base's chin by body_measure.body_dimensions_cm (the featureless proxy head has no face
    profile for chin_and_crown to read)."""
    H = {b.name: np.array(arm.matrix_world @ b.head_local) for b in arm.data.bones}
    seg, info = {}, {}
    hip_r = (H["thigh_l"] + H["thigh_r"]) / 2
    sho_r = (H["upperarm_l"] + H["upperarm_r"]) / 2
    hip_m = (J["LeftUpLeg"] + J["RightUpLeg"]) / 2
    sho_m = (J["LeftArm"] + J["RightArm"]) / 2
    chin_r, crown_r = ref_chin, float(ref_V[:, 2].max())
    chin_m, crown_m = chin_and_crown(meshy_V, sho_m[2], HAIR_ALLOWANCE_M)
    head_r = np.array([0.0, H["head"][1], chin_r])
    head_m = np.array([0.0, J["Head"][1], chin_m])
    seg["torso"], info["torso"] = affine(hip_r, sho_r, hip_m, sho_m, "axial")
    seg["neck"], info["neck"] = affine(sho_r, head_r, sho_m, head_m, "axial")
    seg["head"], info["head"] = affine(head_r, head_r + [0, 0, crown_r - chin_r],
                                       head_m, head_m + [0, 0, crown_m - chin_m], "uniform")
    info["chin_crown_ref_cm"] = (round(chin_r * 100, 2), round(crown_r * 100, 2))
    info["chin_crown_meshy_cm"] = (round(chin_m * 100, 2), round(crown_m * 100, 2))
    for s, S in MESHY_SIDE.items():
        tip_r = fingertip(ref_V, H["hand_" + s], H["lowerarm_" + s])
        tip_m = fingertip(meshy_V, J[S + "Hand"], J[S + "ForeArm"])
        info["fingertip_" + s] = (np.round(tip_r * 100, 2).tolist(), np.round(tip_m * 100, 2).tolist())
        pairs = [
            ("clav_" + s, H["clavicle_" + s], H["upperarm_" + s], J[S + "Shoulder"], J[S + "Arm"], "axial"),
            ("upper_" + s, H["upperarm_" + s], H["lowerarm_" + s], J[S + "Arm"], J[S + "ForeArm"], "axial"),
            ("fore_" + s, H["lowerarm_" + s], H["hand_" + s], J[S + "ForeArm"], J[S + "Hand"], "axial"),
            ("hand_" + s, H["hand_" + s], tip_r, J[S + "Hand"], tip_m, "uniform"),
            ("thigh_" + s, H["thigh_" + s], H["calf_" + s], J[S + "UpLeg"], J[S + "Leg"], "axial"),
        ]
        # Calf ends at the reference ankle in plan but keeps the Meshy ankle height, and the foot only moves in
        # plan: soles stay on the ground and the sandal is not squashed (8.7 step 4, sole at Z = 0 +/- 0.5 cm).
        ank_m = J[S + "Foot"]
        ank_t = np.array([H["foot_" + s][0], H["foot_" + s][1], ank_m[2]])
        pairs.append(("calf_" + s, H["calf_" + s], ank_t, J[S + "Leg"], ank_m, "axial"))
        for name, rh, rt, mh, mt, mode in pairs:
            seg[name], info[name] = affine(rh, rt, mh, mt, mode)
        T = np.eye(4)
        T[:2, 3] = (ank_m - ank_t)[:2]
        seg["foot_" + s], info["foot_" + s] = T, 1.0
    return seg, info


def bone_segment_map(arm):
    owner = {b: s for s, bones in SEGMENT_BONES.items() for b in bones}
    out = {}
    for b in arm.data.bones:
        p = b
        while p is not None and p.name not in owner:
            p = p.parent
        out[b.name] = owner[p.name] if p is not None else "torso"
    return out


def segment_weights(obj, seg_names, bone_seg):
    """Per-vertex segment weights (n x S) from the object's bone vertex groups."""
    col = {s: i for i, s in enumerate(seg_names)}
    gseg = {g.index: col[bone_seg[g.name]] for g in obj.vertex_groups if g.name in bone_seg}
    W = np.zeros((len(obj.data.vertices), len(seg_names)))
    for v in obj.data.vertices:
        for g in v.groups:
            j = gseg.get(g.group)
            if j is not None:
                W[v.index, j] += g.weight
    s = W.sum(1)
    W[s > 0] /= s[s > 0, None]
    W[s == 0, col["torso"]] = 1.0
    return W


def srgb_to_lab(rgb):
    """sRGB (0..1, n x 3) to CIELAB D65 (n x 3)."""
    c = np.where(rgb <= 0.04045, rgb / 12.92, ((rgb + 0.055) / 1.055) ** 2.4)
    M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]])
    xyz = c @ M.T / np.array([0.95047, 1.0, 1.08883])
    f = np.where(xyz > 216 / 24389, np.cbrt(xyz), (24389 / 27 * xyz + 16) / 116)
    return np.stack([116 * f[:, 1] - 16, 500 * (f[:, 0] - f[:, 1]), 200 * (f[:, 1] - f[:, 2])], 1)


def lab_to_srgb(lab):
    fy = (lab[:, 0] + 16) / 116
    fx, fz = fy + lab[:, 1] / 500, fy - lab[:, 2] / 200
    f = np.stack([fx, fy, fz], 1)
    xyz = np.where(f ** 3 > 216 / 24389, f ** 3, (116 * f - 16) / (24389 / 27)) * np.array([0.95047, 1.0, 1.08883])
    Mi = np.array([[3.2406, -1.5372, -0.4986], [-0.9689, 1.8758, 0.0415], [0.0557, -0.2040, 1.0570]])
    c = np.clip(xyz @ Mi.T, 0, 1)
    return np.where(c <= 0.0031308, 12.92 * c, 1.055 * c ** (1 / 2.4) - 0.055)


def classify_lab(lab):
    """Region per CIELAB colour: 0 skin / other, 1 garment (light, low chroma cloth), 2 hair (dark)."""
    L, C = lab[:, 0], np.hypot(lab[:, 1], lab[:, 2])
    cls = np.zeros(len(lab), dtype=np.int8)
    cls[(L > 52) & (C < 22)] = 1
    cls[L < 22] = 2
    return cls


def base_color_image(obj):
    for slot in obj.material_slots:
        mat = slot.material
        if mat and mat.use_nodes:
            for n in mat.node_tree.nodes:
                if n.type == "TEX_IMAGE" and n.image is not None:
                    return n.image
    return None


def loop_lab(obj):
    """Base-colour CIELAB at every face corner (nearest texel of the loop's UV) and the loops' vertex indices."""
    img = base_color_image(obj)
    me = obj.data
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    vi = np.empty(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", vi)
    x = np.clip((uv[:, 0] % 1.0) * w, 0, w - 1).astype(int)
    y = np.clip((uv[:, 1] % 1.0) * h, 0, h - 1).astype(int)
    return srgb_to_lab(np.clip(px[y, x, :3].astype(np.float64), 0, 1)), vi


def edge_rings(E, n, seeds, limit):
    """Edge-ring distance of every vertex from the `seeds` mask (0 on seeds), capped at `limit`."""
    ring = np.full(n, float(limit))
    ring[seeds] = 0.0
    frontier = seeds.copy()
    for k in range(1, limit):
        nxt = np.zeros(n, dtype=bool)
        a, b = E[:, 0], E[:, 1]
        nxt[b[frontier[a]]] = True
        nxt[a[frontier[b]]] = True
        nxt &= ring > k
        ring[nxt] = k
        frontier = nxt
        if not nxt.any():
            break
    return ring


def open_mask(mask, E, n, passes=2):
    """Morphological opening on a vertex mask: drop members whose neighbours are mostly outside it (isolated
    misclassified vertices such as a light fingernail)."""
    deg = np.bincount(E.ravel(), minlength=n).astype(np.float64)
    deg[deg == 0] = 1.0
    m = mask.copy()
    for _ in range(passes):
        cnt = np.zeros(n)
        np.add.at(cnt, E[:, 0], m[E[:, 1]])
        np.add.at(cnt, E[:, 1], m[E[:, 0]])
        m &= cnt / deg >= 0.5
    return m


def vertex_classes(obj):
    """Region per vertex: majority of its face corners' colour classes (classify_lab). Welded seam vertices see
    texels of several UV islands, so a vote is used rather than an averaged UV."""
    lab, vi = loop_lab(obj)
    votes = np.zeros((len(obj.data.vertices), 3))
    np.add.at(votes, (vi, classify_lab(lab).astype(np.int64)), 1.0)
    return votes.argmax(1).astype(np.int8)


CLOTH_SEGMENTS = ("torso", "neck", "clav_l", "clav_r", "thigh_l", "thigh_r")


def conform(work, body, arm, seg, vclass):
    """Landmark-driven inverse-skinning sculpt of `work` onto the reference proportions (module docstring).

    Garment vertices (vclass 1) follow only the torso, neck, clavicle and thigh segments, so the cloak does not
    swing with the arm or the shin when those are re-proportioned."""
    names = list(seg)
    A = np.stack([seg[n] for n in names])                          # S x 4 x 4
    bone_seg = bone_segment_map(arm)
    Wb = segment_weights(body, names, bone_seg)
    B = mesh_world_np(body)
    Bh = np.c_[B, np.ones(len(B))]
    Mb = np.einsum("ns,sij->nij", Wb, A)
    Bf = np.einsum("nij,nj->ni", Mb, Bh)[:, :3]                     # Body_Base carried into Meshy proportions
    polys = [tuple(p.vertices) for p in body.data.polygons]
    bvh = BVHTree.FromPolygons([Vector(p) for p in Bf], polys)
    V = mesh_world_np(work)
    Wv = np.zeros((len(V), len(names)))
    for i, p in enumerate(V):
        loc, _n, fi, _d = bvh.find_nearest(Vector(p))
        if fi is None:
            Wv[i, names.index("torso")] = 1.0
            continue
        vid = polys[fi]
        d = np.linalg.norm(Bf[list(vid)] - np.array(loc), axis=1) + 1e-6
        Wv[i] = (Wb[list(vid)] / d[:, None]).sum(0) / (1.0 / d).sum()
    E = edges_np(work.data)
    Wv = laplacian(Wv, E, WEIGHT_SMOOTH_ITERS)
    cloth = vclass == 1
    keep = np.array([n in CLOTH_SEGMENTS for n in names])
    Wv[np.ix_(cloth, ~keep)] = 0.0
    empty = cloth & (Wv.sum(1) < 1e-6)
    Wv[empty, names.index("torso")] = 1.0
    Wv /= Wv.sum(1, keepdims=True)
    # Below the hip joints the cloth follows a smooth height field instead of the nearest leg: torso at the hip
    # line blending to the thighs at the knee line, left/right blended across the midline, so the hem moves with
    # the re-proportioned legs without tearing between the legs.
    H = {b.name: np.array(arm.matrix_world @ b.head_local) for b in arm.data.bones}
    hip_z = float((seg["torso"] @ np.r_[(H["thigh_l"] + H["thigh_r"]) / 2, 1.0])[2])   # Meshy space
    knee_z = float((seg["thigh_l"] @ np.r_[H["calf_l"], 1.0])[2])
    low = cloth & (V[:, 2] < hip_z)
    t = np.clip((hip_z - V[low, 2]) / (hip_z - knee_z), 0.0, 1.0)
    side = np.clip(0.5 + V[low, 0] / 0.20, 0.0, 1.0)            # 0 = right (-X), 1 = left (+X)
    fade = np.clip((V[low, 2] - (hip_z - 0.08)) / 0.08, 0.0, 1.0)  # keep the nearest-body field in the top 8 cm
    field = np.zeros((int(low.sum()), len(names)))
    field[:, names.index("torso")] = 1.0 - t
    field[:, names.index("thigh_l")] = t * side
    field[:, names.index("thigh_r")] = t * (1.0 - side)
    Wv[low] = fade[:, None] * Wv[low] + (1.0 - fade[:, None]) * field
    # Where the cloth surface is welded to skin (armpits, knot), the restriction fades in over CLOTH_BAND_RINGS
    # edge rings so the change of map is spread over a band instead of tearing one edge ring.
    ring = edge_rings(E, len(V), ~cloth, CLOTH_BAND_RINGS)
    r = np.clip(ring / CLOTH_BAND_RINGS, 0.0, 1.0)[cloth][:, None]
    for _ in range(3):   # smooth the cloth field, re-imposing the cloth segment set so no arm or shin weight leaks in
        Wv = laplacian(Wv, E, WEIGHT_SMOOTH_ITERS, mask=cloth)
        restricted = Wv[cloth].copy()
        restricted[:, ~keep] = 0.0
        restricted[restricted.sum(1) < 1e-6, names.index("torso")] = 1.0
        restricted /= restricted.sum(1, keepdims=True)
        Wv[cloth] = (1.0 - r) * Wv[cloth] + r * restricted
        Wv /= Wv.sum(1, keepdims=True)
    M = np.einsum("ns,sij->nij", Wv, A)
    R = np.linalg.solve(M[:, :3, :3], (V - M[:, :3, 3])[:, :, None])[:, :, 0]
    set_mesh_np(work, R)
    return Wv, names


def make_work_copy(orig, col, name):
    work = orig.copy()
    work.data = orig.data.copy()
    work.name = name + "_HighPoly"
    work.data.name = work.name
    col.objects.link(work)
    work.data.transform(work.matrix_world)
    work.matrix_world = Matrix.Identity(4)
    # glTF import splits vertices along UV seams; weld them so the conform weights smooth across the seams
    # (UVs live on the loops and survive the weld).
    import bmesh
    bmw = bmesh.new()
    bmw.from_mesh(work.data)
    before = len(bmw.verts)
    bmesh.ops.remove_doubles(bmw, verts=bmw.verts, dist=1e-5)
    bmw.to_mesh(work.data)
    bmw.free()
    work.data.update()
    log("welded seam vertices", before, "->", len(work.data.vertices))
    return work


def render_compare(objs, out_png, views=("front", "side"), textured=True, tile=(700, 900), spread=0.0):
    """Workbench orthographic renders of `objs` (placed side by side by `spread` m), tiled into one PNG."""
    scene = bpy.context.scene
    shown = set(objs)
    saved = {o: o.hide_render for o in scene.objects}
    for o in scene.objects:
        o.hide_render = o not in shown
    offs = {o: o.location.copy() for o in objs}
    for k, o in enumerate(objs):
        o.location.x += (k - (len(objs) - 1) / 2) * spread
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE" if textured else "SINGLE"
    sh.single_color = (0.62, 0.55, 0.48)
    sh.show_cavity = False
    scene.render.resolution_x, scene.render.resolution_y = tile
    cd = bpy.data.cameras.new("CmpCam")
    cd.type = "ORTHO"
    cd.ortho_scale = max(1.95, 0.75 + spread * len(objs)) * max(1.0, tile[1] / tile[0])
    cam = bpy.data.objects.new("CmpCam", cd)
    scene.collection.objects.link(cam)
    scene.camera = cam
    target = Vector((0, 0, 0.85))
    dirs = {"front": Vector((0, -1, 0)), "side": Vector((1, 0, 0)), "back": Vector((0, 1, 0)),
            "three_quarter": Vector((0.7, -0.7, 0.15))}
    tiles = []
    for v in views:
        cam.location = target + dirs[v].normalized() * 6.0
        cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
        p = out_png[:-4] + f"_tile_{v}.png"
        scene.render.filepath = p
        bpy.ops.render.render(write_still=True)
        tiles.append(p)
    w, h = tile
    canvas = np.zeros((h, w * len(tiles), 4), dtype=np.float32)
    for k, p in enumerate(tiles):
        img = bpy.data.images.load(p)
        canvas[:, k * w:(k + 1) * w, :] = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
        bpy.data.images.remove(img)
        os.remove(p)
    out = bpy.data.images.new("Cmp", w * len(tiles), h, alpha=True)
    out.pixels = canvas.ravel().tolist()
    out.filepath_raw = out_png
    out.file_format = "PNG"
    out.save()
    bpy.data.images.remove(out)
    bpy.data.objects.remove(cam, do_unlink=True)
    for o, loc in offs.items():
        o.location = loc
    for o, hr in saved.items():
        o.hide_render = hr


# ------------------------------------------------------------------------------------ steps 6-11 (finish stage)
def _apply(obj, mod):
    with bpy.context.temp_override(object=obj, active_object=obj, selected_objects=[obj],
                                   selected_editable_objects=[obj]):
        bpy.ops.object.modifier_apply(modifier=mod.name)


def tri_count(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def fibonacci_dirs(n):
    i = np.arange(n) + 0.5
    phi = np.arccos(1 - 2 * i / n)
    th = np.pi * (1 + 5 ** 0.5) * i
    return np.stack([np.cos(th) * np.sin(phi), np.sin(th) * np.sin(phi), np.cos(phi)], 1)


def delete_hidden_faces(obj):
    """Delete faces from which no hemisphere ray escapes the mesh toward a possible game camera (skin enclosed
    under the garment, the inside of the cloak seen only from below the hem, interior voxel cavities): 8.7 step 6
    'delete faces hidden under garments'. Rays steeper downward than CAMERA_MIN_DZ do not count (the third-person
    camera is never under the figure). Returns the number deleted."""
    import bmesh
    me = obj.data
    V = mesh_world_np(obj)
    polys = [tuple(p.vertices) for p in me.polygons]
    bvh = BVHTree.FromPolygons([Vector(p) for p in V], polys)
    dirs = fibonacci_dirs(2 * HIDDEN_RAYS)
    hidden = []
    for p in me.polygons:
        c = np.array(p.center)
        nrm = np.array(p.normal)
        org = Vector(c + nrm * 1e-3)
        escaped = False
        for d in dirs:
            if d @ nrm <= 0.05 or d[2] < CAMERA_MIN_DZ:
                continue
            if bvh.ray_cast(org, Vector(d), 5.0)[0] is None:
                escaped = True
                break
        if not escaped:
            hidden.append(p.index)
    if hidden:
        b = bmesh.new()
        b.from_mesh(me)
        b.faces.ensure_lookup_table()
        bmesh.ops.delete(b, geom=[b.faces[i] for i in hidden], context="FACES")
        loose = [v for v in b.verts if not v.link_faces]
        bmesh.ops.delete(b, geom=loose, context="VERTS")
        b.to_mesh(me)
        b.free()
        me.update()
    return len(hidden)


def region_lookup(hp):
    """KD-tree over the high-poly vertices and their `region` values (nearest-vertex region of any point)."""
    from mathutils.kdtree import KDTree
    H = mesh_world_np(hp)
    reg = np.empty(len(H), dtype=np.int64)
    hp.data.attributes["region"].data.foreach_get("value", reg)
    kd = KDTree(len(H))
    for i, p in enumerate(H):
        kd.insert(Vector(p), i)
    kd.balance()
    return kd, reg


def trim_hem(lo, hp):
    """Remove the garment below HEM_Z_M from the closed voxel shell and cap the cut (1.3.3 hem; the Meshy cloak
    hangs to mid-shin). Garment = nearest high-poly vertex region. Returns (faces removed, lowest garment height
    before, after) in cm."""
    import bmesh
    kd, reg = region_lookup(hp)
    V = mesh_world_np(lo)
    vreg = np.array([reg[kd.find(Vector(p))[1]] for p in V])
    g = vreg == 1
    before = float(V[g, 2].min()) * 100.0 if g.any() else float("nan")
    b = bmesh.new()
    b.from_mesh(lo.data)
    b.verts.ensure_lookup_table()
    cut = [f for f in b.faces if f.calc_center_median().z < HEM_Z_M
           and sum(g[v.index] for v in f.verts) * 2 >= len(f.verts)]
    n = len(cut)
    bmesh.ops.delete(b, geom=cut, context="FACES")
    bmesh.ops.delete(b, geom=[v for v in b.verts if not v.link_faces], context="VERTS")
    # fragments of the hanging panels left on the calves: connected pieces under FRAGMENT_FACES faces go
    seen, small = set(), []
    for f in b.faces:
        if f.index in seen:
            continue
        comp, stack = [], [f]
        seen.add(f.index)
        while stack:
            h = stack.pop()
            comp.append(h)
            for e in h.edges:
                for k in e.link_faces:
                    if k.index not in seen:
                        seen.add(k.index)
                        stack.append(k)
        if len(comp) < FRAGMENT_FACES:
            small.extend(comp)
    bmesh.ops.delete(b, geom=small, context="FACES")
    bmesh.ops.delete(b, geom=[v for v in b.verts if not v.link_faces], context="VERTS")
    n += len(small)
    bmesh.ops.holes_fill(b, edges=[e for e in b.edges if e.is_boundary], sides=0)
    b.to_mesh(lo.data)
    b.free()
    lo.data.update()
    V = mesh_world_np(lo)
    vreg = np.array([reg[kd.find(Vector(p))[1]] for p in V])
    after = float(V[vreg == 1, 2].min()) * 100.0
    return n, before, after


def trim_highpoly_hem(hp):
    """After the voxel pass: drop the high-poly garment below HEM_Z_M too, so the bake rays and the region lookup
    of the legs no longer meet the cut-away cloak. Returns the vertices removed."""
    import bmesh
    reg = np.empty(len(hp.data.vertices), dtype=np.int64)
    hp.data.attributes["region"].data.foreach_get("value", reg)
    z = mesh_world_np(hp)[:, 2]
    cut = np.nonzero((reg == 1) & (z < HEM_Z_M))[0]
    b = bmesh.new()
    b.from_mesh(hp.data)
    b.verts.ensure_lookup_table()
    bmesh.ops.delete(b, geom=[b.verts[i] for i in cut], context="VERTS")
    b.to_mesh(hp.data)
    b.free()
    hp.data.update()
    return len(cut)


def retopologise(hp, col, name):
    """8.7 step 6: voxel remesh -> QuadriFlow (decimation fallback) -> triangulate, about 12k triangles."""
    lo = hp.copy()
    lo.data = hp.data.copy()
    lo.name = name
    lo.data.name = name
    col.objects.link(lo)
    lo.hide_set(False)
    lo.data.materials.clear()
    rm = lo.modifiers.new("Remesh", "REMESH")
    rm.mode = "VOXEL"
    rm.voxel_size = VOXEL_SIZE_M
    _apply(lo, rm)
    log("voxel remesh", len(lo.data.polygons), "faces")
    lo["hem_trim"] = trim_hem(lo, hp)
    hidden = delete_hidden_faces(lo)
    log("hidden faces deleted", hidden)
    method = "quadriflow"
    try:
        with bpy.context.temp_override(object=lo, active_object=lo, selected_objects=[lo],
                                       selected_editable_objects=[lo]):
            res = bpy.ops.object.quadriflow_remesh(target_faces=QUADRIFLOW_FACES, use_mesh_symmetry=False,
                                                   use_preserve_sharp=False, use_preserve_boundary=False,
                                                   preserve_attributes=False, smooth_normals=False, mode="FACES",
                                                   seed=0)
        ok = "FINISHED" in res and LOD0_TRIS[0] <= 2 * len(lo.data.polygons) <= LOD0_TRIS[1]
    except RuntimeError as exc:
        log("quadriflow failed:", exc)
        ok = False
    if not ok:
        log("quadriflow result not usable (", len(lo.data.polygons), "faces ); decimating instead")
        method = "decimate (UV projected on the voxel shell first)"
        uv_unwrap(lo, project=True)
        dec = lo.modifiers.new("Dec", "DECIMATE")
        dec.ratio = 12000 / max(1, tri_count(lo))
        _apply(lo, dec)
    tri = lo.modifiers.new("Tri", "TRIANGULATE")
    _apply(lo, tri)
    for p in lo.data.polygons:
        p.use_smooth = True
    keep = {"position", "sharp_face", "material_index"} | {u.name for u in lo.data.uv_layers}
    for nm in [x.name for x in lo.data.attributes if not x.name.startswith(".") and x.name not in keep]:
        try:
            lo.data.attributes.remove(lo.data.attributes[nm])
        except RuntimeError:
            pass
    return lo, method


def texel_density(lo):
    """Median over faces of sqrt(texels / cm^2) at TEX x TEX."""
    me = lo.data
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    V = mesh_world_np(lo) * 100.0
    dens = []
    for p in me.polygons:
        li = list(p.loop_indices)
        a, b, c = uv[li[0]], uv[li[1]], uv[li[2]]
        ua = abs((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])) / 2.0
        pa, pb, pc = V[p.vertices[0]], V[p.vertices[1]], V[p.vertices[2]]
        wa = np.linalg.norm(np.cross(pb - pa, pc - pa)) / 2.0
        if wa > 1e-6:
            dens.append(math.sqrt(ua * TEX * TEX / wa))
    return float(np.median(dens))


def uv_unwrap(lo, project=True):
    """8.7 step 7: one UV0, smart projection, packed with 8 px padding at 1024, scaled to 5.12 texels / cm.

    With project=False the existing islands are only re-packed (the projection is made on the dense voxel shell
    before decimation, whose smooth normals give far fewer, larger islands than the decimated triangles)."""
    me = lo.data
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    lo.select_set(True)
    bpy.context.view_layer.objects.active = lo
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    if project:
        while len(me.uv_layers) > 0:
            bpy.ops.object.mode_set(mode="OBJECT")
            me.uv_layers.remove(me.uv_layers[0])
            bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.object.mode_set(mode="OBJECT")
        me.uv_layers.new(name="UVMap")
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(angle_limit=math.radians(UV_ANGLE_DEG), island_margin=0.0, area_weight=0.0,
                                 correct_aspect=True, scale_to_bounds=False)
    try:
        bpy.ops.uv.select_all(action="SELECT")
    except RuntimeError:
        pass   # outside a UV editor the poll can fail; every face is selected in the mesh, which pack uses
    bpy.ops.uv.pack_islands(rotate=True, shape_method="CONCAVE", margin_method="FRACTION", margin=UV_PAD_PX / TEX)
    bpy.ops.object.mode_set(mode="OBJECT")
    packed = texel_density(lo)
    if packed > TEXEL_DENSITY:
        k = TEXEL_DENSITY / packed
        uv = np.empty(len(me.loops) * 2)
        me.uv_layers.active.data.foreach_get("uv", uv)
        me.uv_layers.active.data.foreach_set("uv", uv * k)
    return packed, texel_density(lo)


def _image(name, size, non_color):
    img = bpy.data.images.get(name) or bpy.data.images.new(name, size, size, alpha=True)
    img.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
    return img


def _bake_target_material(lo, img):
    mat = bpy.data.materials.get("M_Bake_Target") or bpy.data.materials.new("M_Bake_Target")
    nt = mat.node_tree
    tex = next((n for n in nt.nodes if n.type == "TEX_IMAGE"), None) or nt.nodes.new("ShaderNodeTexImage")
    tex.image = img
    nt.nodes.active = tex
    lo.data.materials.clear()
    lo.data.materials.append(mat)


def bake(hp, lo, kind, img, samples=1):
    """Cycles selected-to-active bake of `kind` from the conformed high-poly onto the low-poly's UV0."""
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    _bake_target_material(lo, img)
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    hp.hide_set(False)
    hp.select_set(True)
    lo.hide_set(False)
    lo.select_set(True)
    bpy.context.view_layer.objects.active = lo
    kw = dict(type=kind, use_selected_to_active=True, cage_extrusion=0.02, max_ray_distance=0.06,
              margin=UV_PAD_PX, use_clear=True)
    if kind == "DIFFUSE":
        kw["pass_filter"] = {"COLOR"}
    if kind == "NORMAL":
        kw["normal_space"] = "TANGENT"
    bpy.ops.object.bake(**kw)
    hp.select_set(False)
    hp.hide_set(True)


def region_emit_material():
    """Temporary emission material showing the high-poly's per-vertex region colour (bake source)."""
    mat = bpy.data.materials.new("M_RegionEmit")
    nt = mat.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    at = nt.nodes.new("ShaderNodeAttribute")
    at.attribute_name = "region_col"
    em = nt.nodes.new("ShaderNodeEmission")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    nt.links.new(at.outputs["Color"], em.inputs["Color"])
    nt.links.new(em.outputs["Emission"], out.inputs["Surface"])
    return mat


def img_np(img):
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    return px.reshape(h, w, 4)


def save_png(arr, path, non_color):
    h, w = arr.shape[:2]
    img = bpy.data.images.new(os.path.basename(path)[:-4], w, h, alpha=False)
    img.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
    rgba = np.ones((h, w, 4), dtype=np.float32)
    rgba[:, :, :3] = arr[:, :, :3]
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    return img


def delight(D, region, covered):
    """8.7 step 8 de-light into the 1.3.4 bands. Returns (albedo sRGB h x w x 3, per-region L* statistics)."""
    h, w = D.shape[:2]
    lab = srgb_to_lab(np.clip(D[:, :, :3].reshape(-1, 3).astype(np.float64), 0, 1))
    reg = region.reshape(-1)
    cov = covered.reshape(-1)
    out = lab.copy()
    stats = {}
    L, a, b = lab[:, 0], lab[:, 1], lab[:, 2]
    C, hue = np.hypot(a, b), np.degrees(np.arctan2(b, a)) % 360.0
    for rid, name in ((1, "garment"), (0, "skin"), (2, "hair"), (3, "sandal")):
        m = cov & (reg == rid)
        if not m.any():
            continue
        mean = float(L[m].mean())
        if name == "garment":
            nl = np.clip(GARMENT_BASE_L + (L[m] - mean) * GARMENT_DETAIL, GARMENT_BASE_L - 5, GARMENT_BASE_L + 5)
            out[m] = np.stack([nl, np.zeros_like(nl), np.zeros_like(nl)], 1)
        elif name == "skin":
            nl = np.clip(SKIN_L + (L[m] - mean) * 0.5, *SKIN_L_BAND)
            nh = np.radians(np.clip(hue[m], *SKIN_HUE))
            out[m] = np.stack([nl, SKIN_C * np.cos(nh), SKIN_C * np.sin(nh)], 1)
        elif name == "hair":
            nl = np.clip(HAIR_L + (L[m] - mean) * 0.5, *HAIR_L_BAND)
            nc = np.minimum(C[m], HAIR_C_MAX)
            out[m] = np.stack([nl, nc * np.cos(np.radians(hue[m])), nc * np.sin(np.radians(hue[m]))], 1)
        else:
            nl = np.clip(L[m], 40.0, 62.0)
            nc = np.minimum(C[m], 28.0)
            out[m] = np.stack([nl, nc * np.cos(np.radians(hue[m])), nc * np.sin(np.radians(hue[m]))], 1)
        stats[name] = {"src_mean_L": round(mean, 1), "out_L": (round(float(out[m, 0].min()), 1),
                                                               round(float(out[m, 0].max()), 1)),
                       "texels": int(m.sum())}
    return lab_to_srgb(out).reshape(h, w, 3), stats


def bake_and_delight(hp, lo, tex_dir, name):
    """8.7 step 8: bake from the conformed high-poly, de-light, write D / N / ORM / M."""
    os.makedirs(tex_dir, exist_ok=True)
    t_d = _image(f"T_{name}_D_raw", TEX, False)
    bake(hp, lo, "DIFFUSE", t_d, samples=1)
    t_n = _image(f"T_{name}_N", TEX, True)
    bake(hp, lo, "NORMAL", t_n, samples=1)
    t_ao = _image(f"T_{name}_AO", TEX, True)
    bake(hp, lo, "AO", t_ao, samples=32)
    keep = list(hp.data.materials)
    hp.data.materials.clear()
    hp.data.materials.append(region_emit_material())
    t_r = _image(f"T_{name}_region", TEX, True)
    bake(hp, lo, "EMIT", t_r, samples=1)
    hp.data.materials.clear()
    for m in keep:
        hp.data.materials.append(m)
    R = img_np(t_r)
    covered = R[:, :, :3].max(2) > 0.25                 # every region colour is a saturated primary or white
    region = np.zeros(R.shape[:2], dtype=np.int8)      # white = skin / other
    region[(R[:, :, 0] > 0.5) & (R[:, :, 1] < 0.5) & (R[:, :, 2] < 0.5)] = 1   # red garment
    region[(R[:, :, 1] > 0.5) & (R[:, :, 0] < 0.5) & (R[:, :, 2] < 0.5)] = 2   # green hair
    region[(R[:, :, 2] > 0.5) & (R[:, :, 0] < 0.5) & (R[:, :, 1] < 0.5)] = 3   # blue sandal
    D = img_np(t_d)
    albedo, stats = delight(D, region, covered)
    albedo[~covered] = lab_to_srgb(np.array([[GARMENT_BASE_L, 0.0, 0.0]]))[0]
    paths = {"D": os.path.join(tex_dir, f"T_{name}_D.png"), "N": os.path.join(tex_dir, f"T_{name}_N.png"),
             "ORM": os.path.join(tex_dir, f"T_{name}_ORM.png"), "M": os.path.join(tex_dir, f"T_{name}_M.png")}
    save_png(albedo, paths["D"], False)
    save_png(img_np(t_n), paths["N"], True)
    ao = img_np(t_ao)[:, :, 0]
    rough = np.full(region.shape, ROUGHNESS["skin"], dtype=np.float32)
    for rid, key in ((1, "garment"), (2, "hair"), (3, "sandal")):
        rough[region == rid] = ROUGHNESS[key]
    save_png(np.stack([ao, rough, np.zeros_like(ao)], 2), paths["ORM"], True)
    step = TEX // MASK_TEX
    g = ((region == 1) & covered)[::step, ::step].astype(np.float32)
    save_png(np.stack([g, np.zeros_like(g), np.zeros_like(g)], 2), paths["M"], True)
    garment_frac = float(((region == 1) & covered).sum()) / max(1, int(covered.sum()))
    for img in (t_d, t_ao, t_r):
        bpy.data.images.remove(img)
    return paths, stats, garment_frac


def final_material(lo, name, paths):
    """One slot named for the material instance; a Principled preview of the baked set (Unreal import of
    materials and textures stays off, 8.6)."""
    mat = bpy.data.materials.new(f"MI_Char_{name}_Default")
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")

    def tex(key, non_color):
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = bpy.data.images.load(paths[key], check_existing=True)
        n.image.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
        return n
    d = tex("D", False)
    nt.links.new(d.outputs["Color"], bsdf.inputs["Base Color"])
    orm = tex("ORM", True)
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(orm.outputs["Color"], sep.inputs["Color"])
    nt.links.new(sep.outputs[1], bsdf.inputs["Roughness"])
    nt.links.new(sep.outputs[2], bsdf.inputs["Metallic"])
    n = tex("N", True)
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(n.outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    lo.data.materials.clear()
    lo.data.materials.append(mat)
    for m in [m for m in bpy.data.materials if m.name.startswith(("M_Bake_Target", "M_RegionEmit"))]:
        bpy.data.materials.remove(m)


GARMENT_RIG_BONES = ("pelvis", "spine_", "neck_", "clavicle_", "thigh_")


def read_weights(obj):
    names = [g.name for g in obj.vertex_groups]
    W = np.zeros((len(obj.data.vertices), len(names)))
    for v in obj.data.vertices:
        for g in v.groups:
            W[v.index, g.group] = g.weight
    return W, names


def write_weights(obj, W, names):
    """Limit to MAX_INFLUENCES, normalise, write; empty groups are removed."""
    top = np.argsort(-W, axis=1)[:, :MAX_INFLUENCES]
    rows = np.arange(len(W))[:, None]
    vals = W[rows, top]
    vals[vals < 1e-4] = 0.0
    s = vals.sum(1, keepdims=True)
    vals = np.where(s > 0, vals / np.maximum(s, 1e-9), 0.0)
    for g in obj.vertex_groups:
        g.remove(range(len(obj.data.vertices)))
    groups = {nm: obj.vertex_groups[nm] for nm in names}
    used = set()
    for i in range(len(W)):
        for j, val in zip(top[i], vals[i]):
            if val > 0.0:
                groups[names[j]].add([i], float(val), "REPLACE")
                used.add(names[j])
    for g in list(obj.vertex_groups):
        if g.name not in used:
            obj.vertex_groups.remove(g)


def rig(lo, hp, body, arm):
    """8.7 step 9: weights from Body_Proxy (nearest face interpolated), cloth correction, limit 4, normalise.

    Garment vertices keep only pelvis / spine / neck / clavicle / thigh weights (a cloak does not swing with the
    forearm), and every vertex of the garment or further than CLOTH_OFFSET_M from Body_Proxy gets its weights
    smoothed (the scripted equivalent of the hand correction for loose cloth)."""
    for g in body.vertex_groups:
        if g.name not in lo.vertex_groups:
            lo.vertex_groups.new(name=g.name)
    dt = lo.modifiers.new("WeightTransfer", "DATA_TRANSFER")
    dt.object = body
    dt.use_vert_data = True
    dt.data_types_verts = {"VGROUP_WEIGHTS"}
    dt.vert_mapping = "POLYINTERP_NEAREST"
    dt.layers_vgroup_select_src = "ALL"
    dt.layers_vgroup_select_dst = "NAME"
    _apply(lo, dt)
    W, names = read_weights(lo)
    V = mesh_world_np(lo)
    from mathutils.kdtree import KDTree
    H = mesh_world_np(hp)
    reg = np.empty(len(H), dtype=np.int64)
    hp.data.attributes["region"].data.foreach_get("value", reg)
    kd = KDTree(len(H))
    for i, p in enumerate(H):
        kd.insert(Vector(p), i)
    kd.balance()
    lreg = np.array([reg[kd.find(Vector(p))[1]] for p in V])
    bvh = BVHTree.FromPolygons([Vector(p) for p in mesh_world_np(body)],
                               [tuple(p.vertices) for p in body.data.polygons])
    dist = np.array([bvh.find_nearest(Vector(p))[3] for p in V])
    garment = lreg == 1
    hair = lreg == 2
    allowed = np.array([nm.startswith(GARMENT_RIG_BONES) for nm in names])
    if "head" not in names:
        lo.vertex_groups.new(name="head")
        names.append("head")
        W = np.c_[W, np.zeros(len(W))]
        allowed = np.r_[allowed, False]
    E = edges_np(lo.data)
    loose = garment | (dist > CLOTH_OFFSET_M)
    for _ in range(2):
        W = laplacian(W, E, CLOTH_SMOOTH_ITERS, mask=loose)
        W[np.ix_(garment, ~allowed)] = 0.0
        W[garment & (W.sum(1) < 1e-6), names.index("pelvis")] = 1.0
        W /= np.maximum(W.sum(1, keepdims=True), 1e-9)
    W[hair] = 0.0
    W[hair, names.index("head")] = 1.0     # sculpted hair (and the nape knot) is rigid with the head
    W, nneck = neck_correction(lo, arm, W, names)
    deform = np.array([nm in arm.data.bones for nm in names])
    W[:, ~deform] = 0.0
    write_weights(lo, W, names)
    mod = lo.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    lo.parent = arm
    lo.matrix_parent_inverse.identity()
    return int(garment.sum()), int(loose.sum()), lreg, int(hair.sum()), nneck


def neck_correction(lo, arm, W, names, band=NECK_BAND_M, cap=0.45):
    """8.7 step 9 hand correction at the neck. Body_Base's smoothed shoulder weights (and the cloth smoothing)
    let the arm chain own the trapezius up to the neck, further medially than SKM_UEFN_Mannequin's own skinning,
    where the arm chain starts at a fixed point of the clavicle-to-hand axis (the axis body_measure.arm_extent_cm
    uses). Medial of that point the arm chain's summed weight is capped at `cap` (the excess goes to spine_05, or
    neck_01 above the clavicle head), the cap fading out over `band` laterally. Returns (W, vertices changed)."""
    ref = bpy.data.objects["UEFN_Ref_Mesh"]
    V = mesh_world_np(lo)
    Wr, rn = read_weights(ref)
    R = mesh_world_np(ref)
    changed = 0
    col = {nm: j for j, nm in enumerate(names)}
    for side, sign in (("l", 1.0), ("r", -1.0)):
        a = np.array(arm.matrix_world @ arm.data.bones["clavicle_" + side].head_local)
        b = np.array(arm.matrix_world @ arm.data.bones["middle_03_" + side].head_local)
        axis = (b - a) / np.linalg.norm(b - a)
        bones = bm.arm_chain_bones(arm, side)
        rc = [j for j, nm in enumerate(rn) if nm in bones]
        start = float(((R[Wr[:, rc].sum(1) > 0.5] - a) @ axis).min())
        chain = np.array([nm in bones for nm in names])
        proj = (V - a) @ axis
        lim = cap + (1.0 - cap) * np.clip((proj - start) / band, 0.0, 1.0)
        tot = W[:, chain].sum(1)
        sel = (V[:, 0] * sign > 0.0) & (tot > lim)
        if not sel.any():
            continue
        k = lim[sel] / tot[sel]
        excess = tot[sel] - lim[sel]
        W[np.ix_(sel, chain)] *= k[:, None]
        to_neck = V[sel, 2] > a[2]
        W[np.nonzero(sel)[0][to_neck], col["neck_01"]] += excess[to_neck]
        W[np.nonzero(sel)[0][~to_neck], col["spine_05"]] += excess[~to_neck]
        changed += int(sel.sum())
    return W, changed


def make_lod(lo, arm, level, ceiling):
    """8.7 step 10: decimate under the ceiling with weights preserved; LOD2+ fold fingers and twists into the
    hand / parent bone (the same stripping for every pool model)."""
    lod = lo.copy()
    lod.data = lo.data.copy()
    lod.name = f"{lo.name}_LOD{level}"
    lod.data.name = lod.name
    for c in lo.users_collection:
        c.objects.link(lod)
    for m in list(lod.modifiers):
        lod.modifiers.remove(m)
    dec = lod.modifiers.new("Dec", "DECIMATE")
    dec.ratio = 0.95 * ceiling / tri_count(lo)
    _apply(lod, dec)
    tri = lod.modifiers.new("Tri", "TRIANGULATE")
    _apply(lod, tri)
    W, names = read_weights(lod)
    if level >= 2:
        bones = arm.data.bones
        for j, nm in list(enumerate(names)):
            b = bones.get(nm)
            if b is None:
                continue
            target = None
            if "twist" in nm:
                target = b.parent.name
            else:
                p = b.parent
                while p is not None and not p.name.startswith("hand_"):
                    p = p.parent
                if p is not None:
                    target = p.name
            if target is not None:
                if target not in names:
                    lod.vertex_groups.new(name=target)
                    names.append(target)
                    W = np.c_[W, np.zeros(len(W))]
                W[:, names.index(target)] += W[:, j]
                W[:, j] = 0.0
    write_weights(lod, W, names)        # decimation merges influences: every LOD is re-limited to 4
    mod = lod.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    return lod


def export_fbx(arm, mesh, path):
    """Armature + one mesh with the frozen preset; returns (skeleton node count, Null nodes, root node type)."""
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    arm.hide_set(False)
    mesh.hide_set(False)
    arm.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.export_scene.fbx(filepath=path, **FBX_EXPORT_PRESET)
    nodes = bm.fbx_skeleton_nodes(path)
    skel = [nm for nm, t in nodes if t in ("Root", "LimbNode")]
    nulls = [nm for nm, t in nodes if t == "Null"]
    return len(skel), nulls, dict(nodes).get("root")


def store_regions(work, vclass):
    """Persist the region per vertex on the high-poly: int `region` (0 skin/other, 1 garment, 2 hair, 3 sandal)
    and the colour attribute `region_col` the bake reads. Sandal = non-garment, non-hair vertices below 12 cm whose
    colour is clearly lighter than the leg skin."""
    lab, vi = loop_lab(work)
    n = len(work.data.vertices)
    L = np.zeros(n)
    cnt = np.bincount(vi, minlength=n).astype(np.float64)
    np.add.at(L, vi, lab[:, 0])
    L /= np.maximum(cnt, 1)
    z = mesh_world_np(work)[:, 2]
    leg = (vclass == 0) & (z > 0.15) & (z < 0.45)
    skin_l = float(np.median(L[leg])) if leg.any() else 40.0
    reg = vclass.astype(np.int64).copy()
    reg[(vclass == 0) & (z < 0.12) & (L > skin_l + 6.0)] = 3
    me = work.data
    for nm in ("region", "region_col"):
        if nm in me.attributes:
            me.attributes.remove(me.attributes[nm])
    me.attributes.new("region", "INT", "POINT").data.foreach_set("value", reg)
    cols = np.array([[1, 1, 1, 1], [1, 0, 0, 1], [0, 1, 0, 1], [0, 0, 1, 1]], dtype=np.float32)[reg]
    me.attributes.new("region_col", "FLOAT_COLOR", "POINT").data.foreach_set("color", cols.ravel())
    return reg, skin_l


def finish(a, out, scene, report, t0):
    """Steps 6-11 on the saved conform stage."""
    name = a.name
    arm = bpy.data.objects["root"]
    body = bpy.data.objects["Body_Proxy"]
    ref = bpy.data.objects["UEFN_Ref_Mesh"]
    hp = bpy.data.objects[name + "_HighPoly"]
    work_col = bpy.data.collections["Work"]
    lo, method = retopologise(hp, work_col, name)
    ncut, hem0, hem1 = lo["hem_trim"]
    del lo["hem_trim"]
    nhp = trim_highpoly_hem(hp)
    report.append(f"step 6 hem: garment below {HEM_Z_M * 100:.0f} cm cut from the voxel shell and capped ({ncut} "
                  f"faces; the generated cloak reached {hem0:.1f} cm; contract 44.4 to 59.6, measured hem in the T1 inputs below); the working high-poly loses the same garment ({nhp} vertices) before the bake")
    report.append(f"step 6 retopology: voxel {VOXEL_SIZE_M * 100:.1f} cm -> hidden faces deleted (no escaping ray of "
                  f"{HIDDEN_RAYS}) -> {method}; LOD0 {tri_count(lo)} tris, "
                  f"{len(lo.data.vertices)} verts (contract {LOD0_TRIS[0]} to {LOD0_TRIS[1]})")
    log("retopo", method, tri_count(lo), round(time.time() - t0, 1))
    packed, dens = uv_unwrap(lo, project=not method.startswith("decimate"))
    report.append(f"step 7 UV0: 1 layer, smart projection {UV_ANGLE_DEG:.0f} deg, 8 px padding at {TEX}; packed density {packed:.2f} "
                  f"texels/cm -> scaled to median {dens:.2f} (contract {TEXEL_DENSITY}, 4.6 to 5.6)")
    paths, stats, gfrac = bake_and_delight(hp, lo, os.path.join(out, "textures"), name)
    log("bake", round(time.time() - t0, 1))
    report.append(f"step 8 bake (Cycles CPU, selected-to-active from {hp.name}): D / N / AO / region; de-light "
                  f"per region (source mean L*, output L* range, texels): {stats}; garment share of covered texels "
                  f"{gfrac * 100:.1f} %; mask _M {MASK_TEX} R = garment, G = trim (empty: no trim authored yet), "
                  f"B = 0; ORM = AO / roughness {ROUGHNESS} / metallic 0")
    final_material(lo, name, paths)
    ng, nloose, lreg, nhair, nneck = rig(lo, hp, body, arm)
    report.append(f"step 9 rig: parent `root` (copy of SK_UEFN_Mannequin's armature), nearest-face weights from "
                  f"Body_Proxy; garment vertices {ng} limited to {GARMENT_RIG_BONES}; {nloose} loose vertices "
                  f"smoothed; {nhair} hair vertices rigid to `head`; neck/trapezius correction on {nneck} vertices (arm-chain weight capped at 0.45 medial of where the reference mesh's arm chain starts, fading over 3 cm); <= {MAX_INFLUENCES} influences, normalised")
    lods = [lo]
    for level, ceiling in LOD_CEILINGS.items():
        lods.append(make_lod(lo, arm, level, ceiling))
    report.append("step 10 LODs: " + ", ".join(f"LOD{i} {tri_count(m)} tris" for i, m in enumerate(lods))
                  + f" (ceilings {LOD_CEILINGS}); LOD2+ fingers -> hand, twists -> parent")
    log("lods", round(time.time() - t0, 1))
    hp.hide_set(True)
    hp.hide_render = True
    return lo, lods, lreg


def audit(a, out, lo, lods, lreg, report, golden):
    """Numbers story 003 AC-2 gates and story 004 (T1) will need, measured here once."""
    arm = bpy.data.objects["root"]
    body = bpy.data.objects["Body_Proxy"]
    ref = bpy.data.objects["UEFN_Ref_Mesh"]
    fails = []
    tol = float(golden["chain_tolerance_pct"])
    report.append("")
    report.append("AC-2 arm extent (body_measure.arm_extent_cm: clavicle-to-hand axis, vertices weighted > 0.5 to "
                  "the chain; reference = SKM_UEFN_Mannequin body mesh with its own weights)")
    report.append(f"   Body_Proxy (Body_Base) on the same metric: l {bm.arm_extent_cm(body, arm, 'l')[0]:.2f} cm, r "
                  f"{bm.arm_extent_cm(body, arm, 'r')[0]:.2f} cm (its smoothed shoulder weights reach the neck; "
                  "see the neck correction in step 9)")
    for side in ("l", "r"):
        r_ext, r_n = bm.arm_extent_cm(ref, arm, side)
        for i, m in enumerate(lods):
            ext, n = bm.arm_extent_cm(m, arm, side)
            dev = (ext - r_ext) / r_ext * 100.0
            ok = abs(dev) <= tol
            if not ok and i == 0:
                fails.append(f"arm extent {side}")
            report.append(f"   [{'PASS' if ok else 'FAIL'}] arm_{side} LOD{i}: {ext:.2f} cm ({n} verts) vs reference "
                          f"{r_ext:.2f} cm ({r_n} verts): {dev:+.2f} % (tolerance +/- {tol} %)"
                          + ("" if i == 0 else " [info]"))
    report.append("")
    report.append("Export (8.7 step 11, frozen preset build_body_base.FBX_EXPORT_PRESET)")
    bones = int(golden["bone_count"])
    files = []
    for i, m in enumerate(lods):
        path = os.path.join(out, f"{a.name}.fbx" if i == 0 else f"{a.name}_LOD{i}.fbx")
        nskel, nulls, roottype = export_fbx(arm, m, path)
        ok = nskel == bones and not nulls and roottype == "Root"
        if not ok:
            fails.append(f"bones LOD{i}")
        worst, over = bm.max_influences(m, set(arm.data.bones.keys()))
        if worst > MAX_INFLUENCES:
            fails.append(f"influences LOD{i}")
        files.append(os.path.relpath(path, REPO))
        report.append(f"   [{'PASS' if ok else 'FAIL'}] {os.path.relpath(path, REPO)}: {nskel} skeleton nodes "
                      f"(reference {bones}, root node type {roottype}), Null nodes {nulls or 'none'}; "
                      f"{tri_count(m)} tris; {len(m.material_slots)} material slot; {len(m.data.uv_layers)} UV "
                      f"layer; max influences {worst} ({over} vertices above 4)")
    # T1 inputs for story 004 (information, not gated here)
    dg = bpy.context.evaluated_depsgraph_get()
    V = mesh_world_np(lo) * 100.0
    report.append("")
    report.append("T1 inputs for story 004 (information; the audit is story 004's)")
    report.append(f"   bounding: sole Z {V[:, 2].min():.2f} cm, top Z {V[:, 2].max():.2f} cm (hair included), "
                  f"X {V[:, 0].min():.1f} to {V[:, 0].max():.1f}, Y {V[:, 1].min():.1f} to {V[:, 1].max():.1f}")
    hpv = mesh_world_np(bpy.data.objects[a.name + "_HighPoly"])
    chin, crown = chin_and_crown(hpv, 1.3445, HAIR_ALLOWANCE_M)
    report.append(f"   head height on the conformed high-poly (front-profile chin {chin * 100:.2f}, crown minus "
                  f"{HAIR_ALLOWANCE_M * 100:.0f} cm "
                  f"hair {crown * 100:.2f}): {(crown - chin) * 100:.2f} cm (contract 19.3 +/- 1)")
    g = V[lreg == 1]
    if len(g):
        report.append(f"   garment: hem (lowest garment vertex) {g[:, 2].min():.1f} cm (contract 44.4 to 59.6 per "
                      f"1.3.2 OI-1 block); front envelope width {g[:, 0].max() - g[:, 0].min():.1f} cm, side depth "
                      f"{g[:, 1].max() - g[:, 1].min():.1f} cm")
    skin = lreg == 0
    bvh = BVHTree.FromPolygons([Vector(p) for p in mesh_world_np(body)],
                               [tuple(p.vertices) for p in body.data.polygons])
    d = np.sort([bvh.find_nearest(Vector(p / 100.0))[3] * 100.0 for p in V[skin]])
    if len(d):
        report.append(f"   Body_Base deviation of bare-skin vertices ({len(d)}): mean {d.mean():.2f} cm, p95 "
                      f"{d[int(0.95 * (len(d) - 1))]:.2f} cm (conform test 1.3.2: mean <= 1.0, p95 <= 1.5; gate is "
                      f"story 053 / T1-5)")
    report.append(f"   rest pose: armature `root` is Body_Base's (OI-1 reference frames, 0.0000 cm / 0.0000 deg in "
                  f"Body_Base_T1_report.txt); object transforms identity: "
                  f"{all(abs(x - 1) < 1e-9 for x in lo.scale) and lo.location.length < 1e-9}")
    return fails, files


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--glb", required=True)
    ap.add_argument("--rig-glb", required=True)
    ap.add_argument("--name", default="TestCharacter")
    ap.add_argument("--out", required=True)
    ap.add_argument("--evidence", default=None)
    ap.add_argument("--stage", default="all", choices=["conform", "finish", "all"])
    ns = ap.parse_args(argv)
    if ns.evidence:
        ns.evidence = os.path.abspath(ns.evidence)   # Blender resolves relative render paths against the .blend
    return ns


def main():
    t0 = time.time()
    a = parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    golden = read_golden(GOLDEN)
    stature = float(golden["stature_cm"]) / 100.0
    stage_blend = os.path.join(out, a.name + "_conform_stage.blend")
    report_path = os.path.join(out, a.name + "_normalise_report.txt")
    report = [f"{a.name} normalisation (art bible 8.7 steps 2-11; CPA story 003)",
              f"Blender {bpy.app.version_string}; golden {os.path.relpath(GOLDEN, REPO)}; "
              f"script tools/asset-pipeline/normalise_meshy_character.py",
              f"source GLB {os.path.relpath(os.path.abspath(a.glb), REPO)}; landmarks "
              f"{os.path.relpath(os.path.abspath(a.rig_glb), REPO)}", ""]
    if a.stage in ("conform", "all"):
        scene, cols, arm, body, ref, orig = setup_scene(a.name, os.path.abspath(a.glb))
        s = normalise_original(orig, stature)
        Vo = mesh_world_np(orig)
        report.append(f"step 2 collections Meshy_Original (hidden, data untouched), Work, Body_Proxy, Manny_Ref")
        report.append(f"step 3 uniform scale {s:.5f} (bounding height {stature * 100:.1f} cm incl. hair and sandal); "
                      f"Meshy_Original: {len(orig.data.vertices)} verts, {len(orig.data.polygons)} faces")
        J, jinfo = meshy_landmarks(os.path.abspath(a.rig_glb), Vo)
        work = make_work_copy(orig, cols["Work"], a.name)
        Vb = mesh_world_np(body)
        dg = bpy.context.evaluated_depsgraph_get()
        bdims = bm.body_dimensions_cm(bm.SectionMesh(body, dg), bm.bone_heads_cm(arm))
        seg, info = build_segments(arm, J, Vb, mesh_world_np(work), bdims["chin_z"] / 100.0)
        vclass = vertex_classes(work)
        vclass[(vclass == 1) & (mesh_world_np(work)[:, 2] < 0.20)] = 0   # light sandal soles are not garment
        garment = open_mask(vclass == 1, edges_np(work.data), len(vclass))
        vclass[(vclass == 1) & ~garment] = 0
        report.append(f"   vertex classes (base colour vote): skin/other {int((vclass == 0).sum())}, garment "
                      f"{int((vclass == 1).sum())}, hair {int((vclass == 2).sum())}")
        report.append(f"   landmarks: Meshy auto-rig joints re-framed (scale {jinfo['rig_frame_scale']}); per segment "
                      "the Meshy / reference length ratio (the conform applies its inverse):")
        for k, v in info.items():
            report.append(f"      {k:18s} x{v:.4f}" if isinstance(v, float) else f"      {k}: {v}")
        conform(work, body, arm, seg, vclass)
        reg, skin_l = store_regions(work, vclass)
        report.append(f"   regions stored on {work.name}: sandal vertices {int((reg == 3).sum())} (leg skin L* "
                      f"{skin_l:.1f})")
        orig.hide_set(True)
        orig.hide_render = True
        log("conform done", round(time.time() - t0, 1), "s")
        if a.evidence:
            os.makedirs(a.evidence, exist_ok=True)
            render_compare([body, work], os.path.join(a.evidence, f"{a.name.lower()}-conform-vs-body-base.png"),
                           views=("front", "side"), textured=True, spread=0.8)
        bpy.ops.wm.save_as_mainfile(filepath=stage_blend, compress=True)
        with open(report_path + ".conform", "w", encoding="utf-8", newline="\n") as fh:
            fh.write("\n".join(report) + "\n")
        if a.stage == "conform":
            print("\n".join(report))
            return 0
    else:
        bpy.ops.wm.open_mainfile(filepath=stage_blend)
        with open(report_path + ".conform", encoding="utf-8") as fh:
            report = fh.read().rstrip("\n").split("\n")
    scene = bpy.context.scene
    lo, lods, lreg = finish(a, out, scene, report, t0)
    fails, files = audit(a, out, lo, lods, lreg, report, golden)
    for m in lods[1:]:
        m.hide_set(True)
        m.hide_render = True
    if a.evidence:
        os.makedirs(a.evidence, exist_ok=True)
        mat = lo.data.materials[0]
        mat.node_tree.nodes.active = next(n for n in mat.node_tree.nodes
                                          if n.type == "TEX_IMAGE" and n.image.filepath.endswith("_D.png"))
        render_compare([lo], os.path.join(a.evidence, f"{a.name.lower()}-lod0-four-view.png"),
                       views=("front", "side", "back", "three_quarter"), textured=True, tile=(480, 760))
        body = bpy.data.objects["Body_Proxy"]
        render_compare([body, lo], os.path.join(a.evidence, f"{a.name.lower()}-lod0-vs-body-base.png"),
                       views=("front", "side"), textured=False, spread=0.8)
        import body_base_t1_audit as audit_mod
        audit_mod.EVIDENCE = a.evidence
        body.hide_render = True
        bpy.data.objects["UEFN_Ref_Mesh"].hide_render = True
        audit_mod.render_views(bpy.data.objects["root"], lo,
                               os.path.join(a.evidence, f"{a.name.lower()}-lod0-pose-set.png"), audit_mod.POSES)
        report.append("")
        report.append(f"renders: {os.path.relpath(a.evidence, REPO)} (conform-vs-body-base, lod0-four-view, "
                      "lod0-vs-body-base, lod0-pose-set: proxy 1.3.5 poses in body_base_t1_audit.POSES order)")
    elapsed = time.time() - t0
    report.append("")
    report.append(f"script wall time {elapsed / 60:.1f} min; files: {', '.join(files)}")
    report.append(f"RESULT (AC-2 gates: arm extent L/R on LOD0, 88 skeleton nodes and <= 4 influences per file): "
                  f"{'PASS' if not fails else 'FAIL'} ({len(fails)} failing: {', '.join(fails) or 'none'})")
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, a.name + ".blend"), compress=True)
    text = "\n".join(report) + "\n"
    with open(report_path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print(text)
    return 0 if not fails else 1


if __name__ == "__main__":
    code = main()
    sys.stdout.flush()
    os._exit(code)
