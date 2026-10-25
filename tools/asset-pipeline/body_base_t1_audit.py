"""T1 audit of Body_Base (art bible 1.3.7 items 1-5 and 17-18, 1.3.9 pose set; CPA story 002 AC-1 / AC-2).

Run headless:
    blender -b --factory-startup --python tools/asset-pipeline/body_base_t1_audit.py [-- --render]
Reads assets/characters/body_base/Body_Base.blend and Body_Base.fbx, the reference
assets/characters/reference/SKM_UEFN_Mannequin.fbx and the golden numbers in
Source/StealthGameEditor/Private/Tests/CharacterPool/Golden/UefnReferenceChains.txt.
Writes assets/characters/body_base/Body_Base_T1_report.txt; with --render also writes the four-view and pose-set
renders to production/qa/evidence/story-002-body-base-blender-build-oi10/.
Exit code 0 when every gated item passes, 1 otherwise. Measurement definitions: body_measure.py.
"""
import math
import os
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import body_measure as bm  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))
BLEND = os.path.join(REPO, "assets", "characters", "body_base", "Body_Base.blend")
FBX = os.path.join(REPO, "assets", "characters", "body_base", "Body_Base.fbx")
REF_FBX = os.path.join(REPO, "assets", "characters", "reference", "SKM_UEFN_Mannequin.fbx")
GOLDEN = os.path.join(REPO, "Source", "StealthGameEditor", "Private", "Tests", "CharacterPool", "Golden",
                      "UefnReferenceChains.txt")
REPORT = os.path.join(REPO, "assets", "characters", "body_base", "Body_Base_T1_report.txt")
EVIDENCE = os.path.join(REPO, "production", "qa", "evidence", "story-002-body-base-blender-build-oi10")

REST_POS_TOL_CM = 0.5          # 1.3.5 rest pose
REST_ROT_TOL_DEG = 1.0
CONFORM_P95_CM = 1.5           # 1.3.2 Body_Base conformity
CONFORM_MEAN_CM = 1.0
INTERSECT_TOL_CM = 1.0         # 1.3.5 deformation quality
TWIST_TOL_DEG = 15.0
CREASE_EXCLUSION_CM = 6.0      # surface distance from a region seam inside which contact is the joint crease itself
PENETRATION_SEARCH_CM = 5.0

F, B, D, U = Vector((0, -1, 0)), Vector((0, 1, 0)), Vector((0, 0, -1)), Vector((0, 0, 1))
CHILD = {"thigh_l": "calf_l", "thigh_r": "calf_r", "calf_l": "foot_l", "calf_r": "foot_r",
         "upperarm_l": "lowerarm_l", "upperarm_r": "lowerarm_r", "lowerarm_l": "hand_l", "lowerarm_r": "hand_r",
         "spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "spine_04", "spine_04": "spine_05",
         "spine_05": "neck_01"}
SPINE = ["spine_01", "spine_02", "spine_03", "spine_04", "spine_05"]


def _arms_down(deg):
    return [("swing", "upperarm_l", D, deg), ("swing", "upperarm_r", D, deg)]


# Proxy pose set for 1.3.5 (walk, run, idle, brace, assassination, vault, reversal counter): joint angles
# representative of each motion's extreme frame, applied in armature space. The motion-matched clips live in
# Unreal and are checked on the real animations by T4 (engine run, deferred).
POSES = {
    "idle": _arms_down(35) + [("hinge", "lowerarm_l", F, 10), ("hinge", "lowerarm_r", F, 10)],
    "walk": _arms_down(35) + [("swing", "upperarm_l", F, 15), ("swing", "upperarm_r", B, 15),
                              ("hinge", "lowerarm_l", F, 15), ("hinge", "lowerarm_r", F, 15),
                              ("swing", "thigh_r", F, 25), ("swing", "thigh_l", B, 15),
                              ("hinge", "calf_l", B, 35), ("hinge", "calf_r", B, 10)],
    "run": _arms_down(30) + [("swing", "upperarm_l", F, 35), ("swing", "upperarm_r", B, 30),
                             ("hinge", "lowerarm_l", F, 90), ("hinge", "lowerarm_r", F, 90),
                             ("swing", "thigh_r", F, 45), ("swing", "thigh_l", B, 20),
                             ("hinge", "calf_l", B, 90), ("hinge", "calf_r", B, 30), ("swing", "spine_01", F, 10)],
    "brace": _arms_down(25) + [("hinge", "lowerarm_l", F, 45), ("hinge", "lowerarm_r", F, 45),
                               ("swing", "thigh_l", F, 60), ("swing", "thigh_r", F, 60),
                               ("hinge", "calf_l", B, 90), ("hinge", "calf_r", B, 90), ("swing", "spine_01", F, 15)],
    "assassination": [("swing", "upperarm_r", F, 80), ("hinge", "lowerarm_r", F, 20),
                      ("swing", "upperarm_l", D, 30), ("swing", "upperarm_l", F, 20),
                      ("swing", "thigh_r", F, 50), ("hinge", "calf_r", B, 60),
                      ("swing", "thigh_l", B, 20), ("hinge", "calf_l", B, 20)]
                     + [("twist", s, U, 4) for s in SPINE],
    "vault": [("swing", "thigh_l", F, 90), ("swing", "thigh_r", F, 90), ("hinge", "calf_l", B, 100),
              ("hinge", "calf_r", B, 100), ("swing", "upperarm_l", F, 60), ("swing", "upperarm_r", F, 60),
              ("hinge", "lowerarm_l", F, 20), ("hinge", "lowerarm_r", F, 20), ("swing", "spine_01", F, 20)],
    "reversal_counter": _arms_down(30) + [("swing", "upperarm_l", F, 80), ("hinge", "lowerarm_l", F, 70),
                                          ("swing", "upperarm_r", F, 40), ("hinge", "lowerarm_r", F, 70),
                         ("swing", "thigh_l", F, 20), ("hinge", "calf_l", B, 30),
                         ("swing", "thigh_r", B, 10), ("hinge", "calf_r", B, 15)]
                        + [("twist", s, U, 6) for s in SPINE],
}
REGION_PAIRS = [("arm_l", "torso"), ("arm_r", "torso"), ("leg_l", "leg_r"), ("arm_l", "leg_l"),
                ("arm_r", "leg_r"), ("arm_l", "leg_r"), ("arm_r", "leg_l"), ("arm_l", "arm_r"),
                ("leg_l", "torso"), ("leg_r", "torso")]
ARM_KEYS = ("upperarm", "lowerarm", "hand", "index", "middle", "ring", "pinky", "thumb", "weapon")
LEG_KEYS = ("thigh", "calf", "foot", "ball")


def read_golden(path):
    g = {}
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            line = line.split("#", 1)[0].strip()
            if "=" in line:
                k, v = line.split("=", 1)
                g[k.strip()] = v.strip()
    return g


def region_of(bone):
    side = bone[-2:]
    if side in ("_l", "_r"):
        if any(k in bone for k in ARM_KEYS):
            return "arm" + side
        if any(k in bone for k in LEG_KEYS):
            return "leg" + side
    return "torso"


def import_fbx(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, use_anim=False, ignore_leaf_bones=False,
                             automatic_bone_orientation=False)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    mesh = next(o for o in new if o.type == "MESH")
    return new, arm, mesh


def bone_frames_cm(arm):
    """Rest-pose bone head (cm) and rotation (3x3, world) per bone."""
    out = {}
    for b in arm.data.bones:
        m = arm.matrix_world @ b.matrix_local
        out[b.name] = (m.translation * 100.0, m.to_3x3().normalized())
    return out


def compare_rest(a, b):
    worst_pos, worst_rot = 0.0, 0.0
    for name, (pa, ra) in a.items():
        pb_, rb = b[name]
        worst_pos = max(worst_pos, (pa - pb_).length)
        q = (ra.to_quaternion().rotation_difference(rb.to_quaternion()))
        worst_rot = max(worst_rot, math.degrees(q.angle) if q.angle <= math.pi else 360 - math.degrees(q.angle))
    return worst_pos, worst_rot


def hierarchy(arm):
    return {b.name: (b.parent.name if b.parent else None) for b in arm.data.bones}


def fbx_props(path):
    """GlobalSettings UnitScaleFactor / axes and the root node's Lcl Scaling from an FBX file."""
    from io_scene_fbx import parse_fbx
    root, _ = parse_fbx.parse(path)
    res = {}
    for e in root.elems:
        if e.id == b"GlobalSettings":
            for p70 in e.elems:
                if p70.id == b"Properties70":
                    for p in p70.elems:
                        key = p.props[0].decode()
                        if key in ("UpAxis", "UpAxisSign", "FrontAxis", "FrontAxisSign", "CoordAxis",
                                   "CoordAxisSign", "UnitScaleFactor"):
                            res[key] = p.props[4]
    objs = [e for e in root.elems if e.id == b"Objects"][0]
    for e in objs.elems:
        if e.id == b"Model" and e.props[2] in (b"Root", b"Null"):
            name = e.props[1].split(b"\x00\x01")[0].decode()
            for p70 in e.elems:
                if p70.id == b"Properties70":
                    for p in p70.elems:
                        if p.props[0] == b"Lcl Scaling":
                            res["scale:" + name] = tuple(round(x, 4) for x in p.props[4:7])
    return res


def surface_deviation_cm(body, ref, dg):
    """Distance from each Body_Base vertex to the reference surface (cm): mean and 95th percentile."""
    ev = ref.evaluated_get(dg)
    rm = ev.to_mesh()
    mw = ref.matrix_world
    bvh = BVHTree.FromPolygons([mw @ v.co for v in rm.vertices], [tuple(p.vertices) for p in rm.polygons])
    ev.to_mesh_clear()
    bw = body.matrix_world
    d = sorted(bvh.find_nearest(bw @ v.co)[3] * 100.0 for v in body.data.vertices)
    return sum(d) / len(d), d[int(0.95 * (len(d) - 1))], d[-1]


def _merge_swings(ops):
    """All swings of one bone become one ("swings", bone, [(vec, deg), ...]) step at its first occurrence
    (pose lists name a bone's swings before any hinge or twist of its descendants)."""
    out, by_bone = [], {}
    for op in ops:
        if op[0] == "swing":
            if op[1] in by_bone:
                by_bone[op[1]][2].append((op[2], op[3]))
            else:
                by_bone[op[1]] = ["swings", op[1], [(op[2], op[3])]]
                out.append(by_bone[op[1]])
        else:
            out.append(list(op))
    return out


def apply_pose(arm, ops):
    """Pose the armature from rest. A bone's swings are composed on its direction and applied as one
    minimal-arc rotation (no twist about the bone, as a ball joint swings); hinges turn about the parent-fixed
    knee/elbow axis; twists turn about the given axis (the spine)."""
    for pb in arm.pose.bones:
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    for step in _merge_swings(ops):
        kind, name = step[0], step[1]
        pb = arm.pose.bones[name]
        head = pb.head.copy()
        if kind == "swings":
            d0 = (arm.pose.bones[CHILD[name]].head - head).normalized()
            d = d0.copy()
            for vec, deg in step[2]:
                axis = d.cross(vec)
                if axis.length < 1e-6:
                    continue
                d = Matrix.Rotation(math.radians(deg), 3, axis.normalized()) @ d
            rot3 = d0.rotation_difference(d).to_matrix()
            rot = Matrix.Translation(head) @ rot3.to_4x4() @ Matrix.Translation(-head)
        else:
            vec, deg = step[2], step[3]
            if kind == "hinge":
                # knee / elbow: the hinge is fixed in the parent's frame (rest axis carried by the parent's
                # pose), oriented so a positive angle moves the rest-pose segment toward `vec`
                rest_d = (arm.data.bones[CHILD[name]].head_local - pb.bone.head_local).normalized()
                rest_axis = rest_d.cross(vec).normalized()
                par = pb.parent
                axis = ((par.matrix @ par.bone.matrix_local.inverted()).to_3x3() @ rest_axis).normalized()
            else:
                axis = vec
            rot = Matrix.Translation(head) @ Matrix.Rotation(math.radians(deg), 4, axis) @ Matrix.Translation(-head)
        pb.matrix = rot @ pb.matrix
        bpy.context.view_layer.update()


def max_twist_deg(arm):
    """Largest twist about its own rest direction that any single deform joint adds relative to its parent."""
    worst, where = 0.0, None
    for pb in arm.pose.bones:
        if pb.name not in arm.data.bones or not pb.bone.use_deform:
            continue
        rest = pb.bone.matrix_local
        q = (pb.matrix @ rest.inverted()).to_quaternion()
        if pb.parent:
            qp = (pb.parent.matrix @ pb.parent.bone.matrix_local.inverted()).to_quaternion()
            q = qp.inverted() @ q
        kids = [c for c in pb.bone.children]
        if kids:  # the farthest child gives the segment axis (twist bones sit on the parent's head)
            far = max(kids, key=lambda c: (c.head_local - pb.bone.head_local).length)
            dvec = far.head_local - pb.bone.head_local
        else:
            dvec = pb.bone.tail_local - pb.bone.head_local
        if dvec.length < 1e-6:
            continue
        dvec.normalize()
        proj = Vector((q.x, q.y, q.z)).dot(dvec)
        tw = abs(math.degrees(2.0 * math.atan2(proj, q.w)))
        tw = min(tw, 360.0 - tw)
        if tw > worst:
            worst, where = tw, pb.name
    return worst, where


def geodesic_from(sources, nbr, pos, limit):
    import heapq
    dist = {s: 0.0 for s in sources}
    heap = [(0.0, s) for s in sources]
    heapq.heapify(heap)
    while heap:
        d, i = heapq.heappop(heap)
        if d > dist.get(i, 1e9) or d > limit:
            continue
        for j in nbr[i]:
            nd = d + (pos[i] - pos[j]).length
            if nd < dist.get(j, 1e9) and nd <= limit:
                dist[j] = nd
                heapq.heappush(heap, (nd, j))
    return dist


def intersection_setup(body, arm):
    """Per-vertex body region (dominant weight), adjacency, each region's crease band and rest face areas."""
    me = body.data
    names = {g.index: g.name for g in body.vertex_groups}
    region = []
    for v in me.vertices:
        best = max(v.groups, key=lambda g: g.weight, default=None)
        region.append(region_of(names[best.group]) if best else "torso")
    nbr = [[] for _ in me.vertices]
    for e in me.edges:
        a, b = e.vertices
        nbr[a].append(b)
        nbr[b].append(a)
    pos = [v.co * 100.0 for v in me.vertices]
    band = {}
    for r in sorted(set(region)):
        band[r] = geodesic_from([i for i, x in enumerate(region) if x == r], nbr, pos, CREASE_EXCLUSION_CM)
    rest_area = [p.area for p in me.polygons]
    return {"region": region, "nbr": nbr, "band": band, "rest_area": rest_area}


def measure_pose(body, dg, setup):
    """Deepest intersection between body regions (cm) in the current pose, plus crease and collapse figures.

    Exact detection: triangle pairs of the posed surface that intersect each other (BVH self-overlap, pairs that
    share a vertex ignored). A pair inside one region is a crease fold; a pair whose triangles lie within
    CREASE_EXCLUSION_CM (along the rest surface) of the other's region is the joint crease itself; both are
    counted, not gated. For every other pair of REGION_PAIRS regions, the buried patch is flood-filled from the
    intersecting triangle's vertices through same-region neighbours that stay behind the other region's surface
    (signed distance to its nearest face < 0); the depth is the largest such distance.
    """
    region, nbr, band, rest_area = setup["region"], setup["nbr"], setup["band"], setup["rest_area"]
    ev = body.evaluated_get(dg)
    m = ev.to_mesh()
    mw = body.matrix_world
    P = [(mw @ v.co) * 100.0 for v in m.vertices]
    polys = [tuple(p.vertices) for p in m.polygons]
    bvh = BVHTree.FromPolygons(P, polys)
    pairs = {frozenset(p) for p in REGION_PAIRS}

    def face_region(f):
        rs = [region[k] for k in polys[f]]
        return max(set(rs), key=rs.count)

    fr = [face_region(f) for f in range(len(polys))]
    region_bvh = {}

    def bvh_of(r):
        if r not in region_bvh:
            region_bvh[r] = BVHTree.FromPolygons(P, [polys[f] for f in range(len(polys)) if fr[f] == r])
        return region_bvh[r]

    worst = (0.0, None, None)
    folds = creases = 0
    for f1, f2 in bvh.overlap(bvh):
        if f1 >= f2 or set(polys[f1]) & set(polys[f2]):
            continue
        a, b = fr[f1], fr[f2]
        if a == b:
            folds += 1
            continue
        if frozenset((a, b)) not in pairs:
            continue
        if any(k in band[b] for k in polys[f1]) or any(k in band[a] for k in polys[f2]):
            creases += 1
            continue
        for fa, ra, rb in ((f1, a, b), (f2, b, a)):
            other = bvh_of(rb)
            stack = [k for k in polys[fa] if region[k] == ra]
            seen = set(stack)
            while stack:
                k = stack.pop()
                loc, nrm, _idx, dist = other.find_nearest(P[k], PENETRATION_SEARCH_CM)
                if loc is None or (P[k] - loc).dot(nrm) >= 0.0:
                    continue
                if dist > worst[0]:
                    worst = (dist, f"{ra} into {rb}", k)
                for j in nbr[k]:
                    if j not in seen and region[j] == ra:
                        seen.add(j)
                        stack.append(j)
    ratios = [p.area / rest_area[k] for k, p in enumerate(m.polygons) if rest_area[k] > 1e-10]
    ev.to_mesh_clear()
    collapsed = sum(1 for r in ratios if r < 0.25)
    return worst, folds, creases, min(ratios), collapsed


def render_views(arm, body, out_png, poses=None):
    """Workbench renders composited into one PNG: four views of the rest pose, or one front-3/4 tile per pose."""
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_WORKBENCH"
    except TypeError as exc:
        print("render engine:", exc)
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "SINGLE"
    scene.display.shading.single_color = (0.62, 0.55, 0.48)
    scene.display.shading.show_cavity = True
    scene.world = scene.world or bpy.data.worlds.new("World")
    tile_w, tile_h = 480, 720
    scene.render.resolution_x, scene.render.resolution_y = tile_w, tile_h
    scene.render.film_transparent = False
    cam_data = bpy.data.cameras.new("AuditCam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 1.95
    cam = bpy.data.objects.new("AuditCam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    target = Vector((0, 0, 0.85))
    if poses is None:
        shots = [("front", Vector((0, -1, 0)), None), ("side", Vector((1, 0, 0)), None),
                 ("back", Vector((0, 1, 0)), None), ("three_quarter", Vector((0.7, -0.7, 0.15)), None)]
    else:
        shots = [(n, Vector((0.6, -0.8, 0.1)), ops) for n, ops in poses.items()]
    tiles = []
    for name, direction, ops in shots:
        if ops is not None:
            apply_pose(arm, ops)
        cam.location = target + direction.normalized() * 6.0
        cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
        path = os.path.join(EVIDENCE, f"_tile_{name}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        tiles.append(path)
    apply_pose(arm, [])
    import numpy as np
    cols = len(tiles)
    canvas = np.zeros((tile_h, tile_w * cols, 4), dtype=np.float32)
    for k, path in enumerate(tiles):
        img = bpy.data.images.load(path)
        px = np.array(img.pixels[:], dtype=np.float32).reshape(tile_h, tile_w, 4)
        canvas[:, k * tile_w:(k + 1) * tile_w, :] = px
        bpy.data.images.remove(img)
        os.remove(path)
    out = bpy.data.images.new("FourView", tile_w * cols, tile_h, alpha=True)
    out.pixels = canvas.ravel().tolist()
    out.filepath_raw = out_png
    out.file_format = "PNG"
    out.save()
    bpy.data.objects.remove(cam, do_unlink=True)
    return [s[0] for s in shots]


def main():
    render = "--render" in sys.argv
    golden = read_golden(GOLDEN)
    lines, fails = [], []

    def item(label, ok, detail):
        lines.append(f"[{'PASS' if ok else 'FAIL'}] {label}: {detail}")
        if not ok:
            fails.append(label)

    bpy.ops.wm.open_mainfile(filepath=BLEND)
    dg = bpy.context.evaluated_depsgraph_get()
    arm = bpy.data.objects["root"]
    body = bpy.data.objects["Body_Base"]
    ref_mesh = bpy.data.objects["UEFN_Ref_Mesh"]
    heads = bm.bone_heads_cm(arm)
    sm = bm.SectionMesh(body, dg)
    dims = bm.body_dimensions_cm(sm, heads)
    chains = bm.chain_lengths_cm(heads)

    lines.append("Body_Base T1 audit (art bible 1.3.7 items 1-5, 17-18; 1.3.5 pose set)")
    lines.append(f"blend: {os.path.relpath(BLEND, REPO)}  fbx: {os.path.relpath(FBX, REPO)}")
    lines.append(f"Blender {bpy.app.version_string}; golden: {os.path.relpath(GOLDEN, REPO)}")
    lines.append(f"mesh: {len(body.data.vertices)} verts, {sm.tris} tris, "
                 f"{len(body.material_slots)} material slot(s), {len(body.data.uv_layers)} UV layer(s)")
    lines.append("")

    st, tol = float(golden["stature_cm"]), float(golden["stature_tolerance_cm"])
    item("T1-1 stature", abs(dims["stature"] - st) <= tol,
         f"{dims['stature']:.2f} cm (contract {st} +/- {tol})")
    v, t = map(float, golden["dim.head_height_cm"].split(":"))
    item("T1-2 head height", abs(dims["head_height"] - v) <= t,
         f"{dims['head_height']:.2f} cm (chin at {dims['chin_z']:.2f}; contract {v} +/- {t})")
    ctol = float(golden["chain_tolerance_pct"])
    worst_chain = 0.0
    for k, val in golden.items():
        if k.startswith("chain."):
            ref = float(val.split(":")[1])
            got = chains[k[len("chain."):]]
            pct = abs(got - ref) / ref * 100.0
            worst_chain = max(worst_chain, pct)
            lines.append(f"       chain {k[6:]:22s} {got:7.2f} cm  golden {ref:6.2f}  dev {pct:5.2f} %")
    item("T1-3 bone chains", worst_chain <= ctol, f"worst deviation {worst_chain:.3f} % (tolerance {ctol} %)")
    ok4 = True
    det = []
    for key, dk in (("dim.shoulder_width_cm", "shoulder_width"), ("dim.chest_depth_cm", "chest_depth"),
                    ("dim.hip_width_cm", "hip_width"), ("dim.foot_length_cm", "foot_length")):
        v, t = map(float, golden[key].split(":"))
        ok = abs(dims[dk] - v) <= t
        ok4 &= ok
        det.append(f"{dk} {dims[dk]:.2f} ({v} +/- {t}{'' if ok else ' OUT'})")
    item("T1-4 shoulder/chest/hip (+ foot)", ok4, "; ".join(det))
    mean, p95, mx = surface_deviation_cm(body, ref_mesh, dg)
    item("T1-5 conformity to the UEFN reference body", p95 <= CONFORM_P95_CM and mean <= CONFORM_MEAN_CM,
         f"mean {mean:.3f} cm, p95 {p95:.3f} cm, max {mx:.3f} cm (<= {CONFORM_MEAN_CM} / {CONFORM_P95_CM}; "
         "Body_Base is the conform target, measured against the reference body it was built from)")

    # T1-17: skeleton unmodified, zero extra bones, <= 4 influences
    blend_frames = bone_frames_cm(arm)
    blend_hier = hierarchy(arm)
    nodes = bm.fbx_skeleton_nodes(FBX)
    skel = [n for n, t in nodes if t in ("Root", "LimbNode")]
    nulls = [n for n, t in nodes if t == "Null"]
    ref_nodes = bm.fbx_skeleton_nodes(REF_FBX)
    ref_skel = [n for n, t in ref_nodes if t in ("Root", "LimbNode")]
    inf_worst, inf_over = bm.max_influences(body, set(arm.data.bones.keys()))
    ref_new, ref_arm, ref_m = import_fbx(REF_FBX)
    ref_frames = bone_frames_cm(ref_arm)
    ref_hier = hierarchy(ref_arm)
    same_hier = blend_hier == ref_hier
    ok17 = (len(skel) == int(golden["bone_count"]) and not nulls and sorted(skel) == sorted(ref_skel)
            and same_hier and inf_worst <= 4)
    item("T1-17 skeleton unmodified, zero extra bones, <= 4 influences", ok17,
         f"FBX skeleton nodes {len(skel)} (golden {golden['bone_count']}; root type "
         f"{dict(nodes).get('root')}); Null nodes {nulls or 'none'}; names equal reference: "
         f"{sorted(skel) == sorted(ref_skel)}; parent hierarchy equals reference: {same_hier}; "
         f"max influences {inf_worst} ({inf_over} vertices above 4)")

    # T1-18: rest pose, scale, origin; plus the exported FBX round trip
    dpos, drot = compare_rest(blend_frames, ref_frames)
    for o in ref_new:
        bpy.data.objects.remove(o, do_unlink=True)
    rt_new, rt_arm, rt_mesh = import_fbx(FBX)
    rt_frames = bone_frames_cm(rt_arm)
    rpos, rrot = compare_rest(rt_frames, ref_frames)
    rt_h = max((rt_mesh.matrix_world @ Vector(c)).z for c in rt_mesh.bound_box) * 100.0
    rt_l = min((rt_mesh.matrix_world @ Vector(c)).z for c in rt_mesh.bound_box) * 100.0
    for o in rt_new:
        bpy.data.objects.remove(o, do_unlink=True)
    scale_ok = all(abs(s - 1.0) < 1e-6 for o in (arm, body) for s in o.scale) and \
        all(abs(r) < 1e-6 for o in (arm, body) for r in o.rotation_euler) and \
        all(abs(c) < 1e-6 for o in (arm, body) for c in o.location)
    ok18 = (dpos <= REST_POS_TOL_CM and drot <= REST_ROT_TOL_DEG and rpos <= REST_POS_TOL_CM
            and rrot <= REST_ROT_TOL_DEG and scale_ok and abs(dims["sole_z"]) <= 0.5)
    item("T1-18 rest pose, scale 1.0, origin at the ground", ok18,
         f".blend vs reference: max bone offset {dpos:.4f} cm, max rotation {drot:.4f} deg; exported FBX "
         f"re-imported vs reference: {rpos:.4f} cm, {rrot:.4f} deg, mesh Z {rt_l:.2f} to {rt_h:.2f} cm; "
         f"object transforms identity: {scale_ok}; sole Z {dims['sole_z']:.2f} cm")
    props = fbx_props(FBX)
    lines.append(f"       FBX settings: {props}")

    # AC-2: pose set
    lines.append("")
    lines.append("Pose set (1.3.5 proxy poses; intersections between body regions more than "
                 f"{CREASE_EXCLUSION_CM} cm apart along the surface; twist per joint)")
    setup = intersection_setup(body, arm)
    worst_int, worst_tw = 0.0, 0.0
    for name, ops in POSES.items():
        apply_pose(arm, ops)
        dg.update()
        (depth, pair, _vi), folds, creases, min_ratio, collapsed = measure_pose(body, dg, setup)
        tw, tw_bone = max_twist_deg(arm)
        worst_int, worst_tw = max(worst_int, depth), max(worst_tw, tw)
        lines.append(f"       {name:17s} max intersection {depth:5.2f} cm ({pair or '-'}); max joint twist "
                     f"{tw:5.2f} deg ({tw_bone}); crossing face pairs: crease {creases}, same-region fold {folds}; min face area ratio {min_ratio:.3f}, faces below 0.25: "
                     f"{collapsed}")
    apply_pose(arm, [])
    item("AC-2 pose set intersections", worst_int <= INTERSECT_TOL_CM,
         f"worst {worst_int:.2f} cm (tolerance {INTERSECT_TOL_CM})")
    item("AC-2 pose set joint twist", worst_tw <= TWIST_TOL_DEG,
         f"worst {worst_tw:.2f} deg (tolerance {TWIST_TOL_DEG})")

    if render:
        os.makedirs(EVIDENCE, exist_ok=True)
        ref_mesh.hide_render = True
        render_views(arm, body, os.path.join(EVIDENCE, "body-base-four-view.png"))
        render_views(arm, body, os.path.join(EVIDENCE, "body-base-pose-set.png"), POSES)
        lines.append("")
        lines.append("renders: production/qa/evidence/story-002-body-base-blender-build-oi10/"
                     "body-base-four-view.png, body-base-pose-set.png (tiles in POSES order)")

    lines.append("")
    lines.append(f"RESULT: {'PASS' if not fails else 'FAIL'} ({len(fails)} failing: {', '.join(fails) or 'none'})")
    text = "\n".join(lines) + "\n"
    with open(REPORT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print(text)
    return 0 if not fails else 1


if __name__ == "__main__":
    code = main()
    sys.stdout.flush()
    os._exit(code)
