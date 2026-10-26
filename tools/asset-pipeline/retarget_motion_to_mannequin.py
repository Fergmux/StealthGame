"""Retarget a Meshy motion clip onto the UEFN mannequin skeleton and export an animation FBX (D54 spike, SCL story 056).

Sources (Meshy, decision D54 route 2):
  - profile "t2m":      a text-to-motion clip (BVH, Meshy's 72-joint motion skeleton, T-pose rest, cm);
  - profile "smpl":     a text-to-motion "prime" clip (FBX, SMPL-H skeleton, T-pose rest);
  - profile "meshyrig": a library animation baked on a Meshy auto-rig (GLB, 24 bones, A-pose rest).
Target: the armature "root" of assets/characters/body_base/Body_Base.blend (SK_UEFN_Mannequin, 88 nodes: the
armature object is the FBX Root node `root`, 87 bones below it). Body_Base.blend is opened read-only and never saved.

Run headless (never touches an open Blender session):
    blender -b --factory-startup --python tools/asset-pipeline/retarget_motion_to_mannequin.py -- \
        --src <clip.bvh|clip.glb> [--action <glTF animation name>] --profile t2m|meshyrig \
        --clip <Name> --kind strike|fall|brace --root forward|none --out-fbx <A_x.fbx> --evidence <dir>

Method (world-space delta transfer with a retarget pose, the IK Retargeter's idea without IK):
  1. Retarget pose: every mapped limb bone of the target is turned so its chain direction (head -> child head, plus
     an across-the-palm vector for hands) matches the source's rest pose (T-pose or Meshy A-pose). Spine, neck and
     head keep the mannequin's rest orientation.
  2. Per frame: target world rotation = source world delta from its rest (R_s(f) R_s_rest^-1) x retarget pose.
  3. Pelvis translation = mannequin rest pelvis + leg-length ratio x source hip displacement, plus one constant
     vertical offset that puts frame 1's lower ankle at the mannequin's standing ankle height (every clip starts
     with a planted foot; source rest poses are not all grounded).
  4. Root motion ("forward"): the pelvis's forward (-Y) displacement moves to the `root` node (the armature
     object, which Unreal imports as the root bone); lateral sway and height stay on the pelvis. "none": in place.
  5. ik_foot_* / ik_hand_* follow foot_* / hand_* (rest offsets kept); twist, finger (meshyrig), weapon and attach
     bones keep their rest pose relative to their parent.
Writes <evidence>/<clip>_metrics.json (foot contact drift, root motion, timing, compression, penetration; source
values scaled to mannequin units beside the target values) and renders: 3/4 close-ups for review, and greyscale
profile/front figures at 20 m and 30 m with a 60-degree vertical FOV at 1920x1080 (art bible 1.3.7 T3 / 5.5 reading
range), cropped to the figure at their true pixel size. The exported FBX is re-imported and checked: 88 skeleton
nodes, no Null, frame count, and pelvis/hand/foot world positions against the baked values.
"""
import argparse
import json
import math
import os
import sys

sys.dont_write_bytecode = True
import bpy
from mathutils import Matrix, Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from build_body_base import FBX_EXPORT_PRESET  # noqa: E402
from body_measure import fbx_skeleton_nodes  # noqa: E402

BLEND = os.path.join(REPO, "assets", "characters", "body_base", "Body_Base.blend")
FPS = 30
REFERENCE_BONE_COUNT = 88
MANNEQUIN_HEIGHT = 1.656
CONTACT_TOL_M = 0.025  # a foot is planted while ankle and ball are within 2.5 cm of their clip minimum heights
SPEED_ONSET_MPS = 0.25

# (target bone, source bone, target dir child, source dir child, mode) ; mode: "none" | "dir" | "inherit".
# Hands additionally use an across-the-palm vector (HAND_SIDE) when both skeletons have fingers.
_AXIAL_T2M = [("pelvis", "Hips"), ("spine_01", "Spine1"), ("spine_02", "Spine2"), ("spine_03", "Spine2"),
              ("spine_04", "Chest"), ("spine_05", "Chest"), ("neck_01", "Neck1"), ("neck_02", "Neck2"),
              ("head", "Head")]
_AXIAL_RIG = [("pelvis", "Hips"), ("spine_01", "Hips"), ("spine_02", "Spine02"), ("spine_03", "Spine02"),
              ("spine_04", "Spine01"), ("spine_05", "Spine"), ("neck_01", "neck"), ("neck_02", "neck"),
              ("head", "Head")]


def _t2m_map():
    m = [(t, s, None, None, "none") for t, s in _AXIAL_T2M]
    for t, s in (("l", "Left"), ("r", "Right")):
        m += [(f"clavicle_{t}", f"{s}Shoulder", f"upperarm_{t}", f"{s}Arm", "dir"),
              (f"upperarm_{t}", f"{s}Arm", f"lowerarm_{t}", f"{s}ForeArm", "dir"),
              (f"lowerarm_{t}", f"{s}ForeArm", f"hand_{t}", f"{s}Hand", "dir"),
              (f"hand_{t}", f"{s}Hand", f"middle_01_{t}", f"{s}HandMiddle2", "dir"),
              (f"thigh_{t}", f"{s}Leg", f"calf_{t}", f"{s}Shin", "dir"),
              (f"calf_{t}", f"{s}Shin", f"foot_{t}", f"{s}Foot", "dir"),
              (f"foot_{t}", f"{s}Foot", f"ball_{t}", f"{s}ToeBase", "dir"),
              (f"ball_{t}", f"{s}ToeBase", None, None, "inherit")]
        for fin, sfin in (("index", "Index"), ("middle", "Middle"), ("ring", "Ring"), ("pinky", "Pinky")):
            m += [(f"{fin}_metacarpal_{t}", f"{s}Hand{sfin}1", f"{fin}_01_{t}", f"{s}Hand{sfin}2", "dir"),
                  (f"{fin}_01_{t}", f"{s}Hand{sfin}2", f"{fin}_02_{t}", f"{s}Hand{sfin}3", "dir"),
                  (f"{fin}_02_{t}", f"{s}Hand{sfin}3", f"{fin}_03_{t}", f"{s}Hand{sfin}4", "dir"),
                  (f"{fin}_03_{t}", f"{s}Hand{sfin}4", None, None, "inherit")]
        m += [(f"thumb_01_{t}", f"{s}HandThumb1", f"thumb_02_{t}", f"{s}HandThumb2", "dir"),
              (f"thumb_02_{t}", f"{s}HandThumb2", f"thumb_03_{t}", f"{s}HandThumb3", "dir"),
              (f"thumb_03_{t}", f"{s}HandThumb3", None, None, "inherit")]
    return m


_AXIAL_SMPL = [("pelvis", "Pelvis"), ("spine_01", "Pelvis"), ("spine_02", "Spine1"), ("spine_03", "Spine1"),
               ("spine_04", "Spine2"), ("spine_05", "Spine3"), ("neck_01", "Neck"), ("neck_02", "Neck"),
               ("head", "Head")]


def _smpl_map():
    m = [(t, s, None, None, "none") for t, s in _AXIAL_SMPL]
    for t, s in (("l", "L"), ("r", "R")):
        m += [(f"clavicle_{t}", f"{s}_Collar", f"upperarm_{t}", f"{s}_Shoulder", "dir"),
              (f"upperarm_{t}", f"{s}_Shoulder", f"lowerarm_{t}", f"{s}_Elbow", "dir"),
              (f"lowerarm_{t}", f"{s}_Elbow", f"hand_{t}", f"{s}_Wrist", "dir"),
              (f"hand_{t}", f"{s}_Wrist", f"middle_01_{t}", f"{s}_Middle1", "dir"),
              (f"thigh_{t}", f"{s}_Hip", f"calf_{t}", f"{s}_Knee", "dir"),
              (f"calf_{t}", f"{s}_Knee", f"foot_{t}", f"{s}_Ankle", "dir"),
              (f"foot_{t}", f"{s}_Ankle", f"ball_{t}", f"{s}_Foot", "dir"),
              (f"ball_{t}", f"{s}_Foot", None, None, "inherit")]
    return m


def _rig_map():
    m = [(t, s, None, None, "none") for t, s in _AXIAL_RIG]
    for t, s in (("l", "Left"), ("r", "Right")):
        m += [(f"clavicle_{t}", f"{s}Shoulder", f"upperarm_{t}", f"{s}Arm", "dir"),
              (f"upperarm_{t}", f"{s}Arm", f"lowerarm_{t}", f"{s}ForeArm", "dir"),
              (f"lowerarm_{t}", f"{s}ForeArm", f"hand_{t}", f"{s}Hand", "dir"),
              (f"hand_{t}", f"{s}Hand", None, None, "inherit"),
              (f"thigh_{t}", f"{s}UpLeg", f"calf_{t}", f"{s}Leg", "dir"),
              (f"calf_{t}", f"{s}Leg", f"foot_{t}", f"{s}Foot", "dir"),
              (f"foot_{t}", f"{s}Foot", f"ball_{t}", f"{s}ToeBase", "dir"),
              (f"ball_{t}", f"{s}ToeBase", None, None, "inherit")]
    return m


PROFILES = {
    "t2m": {"map": _t2m_map, "hips": "Hips", "leg": ("LeftLeg", "LeftFoot"), "feet": {"l": ("LeftFoot", "LeftToeBase"),
                                                                       "r": ("RightFoot", "RightToeBase")},
            "hand_side": {"l": ("LeftHandIndex2", "LeftHandPinky2"), "r": ("RightHandIndex2", "RightHandPinky2")}},
    "smpl": {"map": _smpl_map, "hips": "Pelvis", "leg": ("L_Hip", "L_Ankle"),
             "feet": {"l": ("L_Ankle", "L_Foot"), "r": ("R_Ankle", "R_Foot")},
             "hand_side": {"l": ("L_Index1", "L_Pinky1"), "r": ("R_Index1", "R_Pinky1")}},
    "meshyrig": {"map": _rig_map, "hips": "Hips", "leg": ("LeftUpLeg", "LeftFoot"), "feet": {"l": ("LeftFoot", "LeftToeBase"),
                                                                               "r": ("RightFoot", "RightToeBase")},
                 "hand_side": None},
}
FOLLOW = {"ik_foot_l": "foot_l", "ik_foot_r": "foot_r", "ik_hand_gun": "hand_r", "ik_hand_r": "hand_r",
          "ik_hand_l": "hand_l"}


def _rot(m):
    return m.to_3x3().normalized().to_quaternion()


def _frame(d, side):
    """Orthonormal basis (columns) with Y along d and X toward side (Gram-Schmidt)."""
    y = d.normalized()
    x = (side - side.dot(y) * y).normalized()
    z = x.cross(y)
    return Matrix((x, y, z)).transposed()


def import_source(args):
    before = set(bpy.data.objects)
    if args.src.lower().endswith(".fbx"):
        bpy.ops.import_scene.fbx(filepath=args.src, use_anim=True, ignore_leaf_bones=False,
                                 automatic_bone_orientation=False)
        src = next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")
        act = src.animation_data.action
        first, last = int(round(act.frame_range[0])), int(round(act.frame_range[1]))
    elif args.src.lower().endswith(".bvh"):
        bpy.ops.import_anim.bvh(filepath=args.src, global_scale=0.01, axis_forward="-Z", axis_up="Y",
                                rotate_mode="NATIVE", use_fps_scale=False, update_scene_fps=False,
                                update_scene_duration=False, frame_start=1)
        src = bpy.context.active_object
        act = src.animation_data.action
        first, last = int(act.frame_range[0]), int(act.frame_range[1])
    else:
        bpy.ops.import_scene.gltf(filepath=args.src)
        src = next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")
        act = bpy.data.actions[args.action]
        ad = src.animation_data or src.animation_data_create()
        for tr in list(ad.nla_tracks):
            ad.nla_tracks.remove(tr)
        ad.action = act
        if hasattr(ad, "action_slot") and ad.action_slot is None and len(act.slots):
            ad.action_slot = act.slots[0]
        first, last = int(round(act.frame_range[0])), int(round(act.frame_range[1]))
    for o in bpy.data.objects:
        if o not in before:
            o.hide_render = True
    return src, first, last


def depth_order(arm):
    def depth(b):
        d = 0
        while b.parent:
            b, d = b.parent, d + 1
        return d
    bones = sorted(arm.data.bones, key=depth)
    # FOLLOW bones (ik_*) copy a deform bone, so they come after every other bone
    return [b for b in bones if b.name not in FOLLOW] + [b for b in bones if b.name in FOLLOW]


def retarget(arm, src, first, last, profile, root_mode):
    prof = PROFILES[profile]
    mapping = prof["map"]()
    tb, sb = arm.data.bones, src.data.bones
    aw, sw = arm.matrix_world, src.matrix_world
    t_head = {b.name: aw @ b.head_local for b in tb}
    s_head = {b.name: sw @ b.head_local for b in sb}
    t_rest = {b.name: _rot(aw @ b.matrix_local) for b in tb}
    s_rest = {b.name: _rot(sw @ b.matrix_local) for b in sb}

    # 1. retarget pose (alignment correction per mapped target bone), in hierarchy order for "inherit".
    corr, src_of = {}, {}
    for t, s, tc, sc, mode in mapping:
        src_of[t] = s
        if mode == "dir":
            dt, ds = t_head[tc] - t_head[t], s_head[sc] - s_head[s]
            side = None
            if t.startswith("hand_") and prof["hand_side"]:
                si = prof["hand_side"][t[-1]]
                side = (t_head[f"index_01_{t[-1]}"] - t_head[f"pinky_01_{t[-1]}"], s_head[si[0]] - s_head[si[1]])
            if side:
                c = (_frame(ds, side[1]) @ _frame(dt, side[0]).inverted()).to_quaternion()
            else:
                c = dt.rotation_difference(ds)
        elif mode == "inherit":
            c = corr[tb[t].parent.name]
        else:
            c = Quaternion()
        corr[t] = c
    align = {t: corr[t] @ t_rest[t] for t in corr}

    # leg-length scale and rest references
    ls = prof["leg"]
    s_leg = s_head[ls[0]].z - s_head[ls[1]].z
    t_leg = t_head["thigh_l"].z - t_head["foot_l"].z
    scale = t_leg / s_leg
    hips = prof["hips"]
    s_hips_rest = s_head[hips]
    t_pelvis_rest = t_head["pelvis"]
    t_ankle_rest = min(t_head[f"foot_{k}"].z for k in "lr")

    order = depth_order(arm)
    rest_rel = {b.name: (b.parent.matrix_local.inverted() @ b.matrix_local) if b.parent else b.matrix_local.copy()
                for b in tb}
    under_pelvis = set()
    for b in order:
        if b.name == "pelvis" or (b.parent and b.parent.name in under_pelvis) or b.name in FOLLOW:
            under_pelvis.add(b.name)
    scene = bpy.context.scene
    frames = list(range(first, last + 1))
    src_track = []  # per frame: source joint positions mapped into mannequin units (scaled about the hips rest)
    world_M = []    # per frame: bone -> WORLD matrix (the root node's motion included)
    roots = []
    p0 = None
    for f in frames:
        scene.frame_set(f)
        sm = {b.name: sw @ src.pose.bones[b.name].matrix for b in sb}
        p = t_pelvis_rest + scale * (sm[hips].translation - s_hips_rest)
        if p0 is None:
            p0 = p.copy()
        r = Vector((0.0, p.y - p0.y if root_mode == "forward" else 0.0, 0.0))
        roots.append(r)
        R = Matrix.Translation(r)
        M = {}
        for b in order:
            n = b.name
            Mp = M[b.parent.name] if b.parent else R
            if n in align:
                q = _rot(sm[src_of[n]]) @ s_rest[src_of[n]].inverted() @ align[n]
                pos = p if n == "pelvis" else (Mp @ rest_rel[n]).translation
                M[n] = Matrix.LocRotScale(pos, q, Vector((1, 1, 1)))
            elif n in FOLLOW:
                M[n] = M[FOLLOW[n]] @ tb[FOLLOW[n]].matrix_local.inverted() @ b.matrix_local
            else:
                M[n] = Mp @ rest_rel[n]
        world_M.append(M)
        src_track.append({k: t_pelvis_rest + scale * (sm[k].translation - s_hips_rest)
                          for k in [x for pair in prof["feet"].values() for x in pair]})

    # 3. constant vertical offset: every clip starts with at least one foot planted, so frame 1's lower ankle is
    # put at the mannequin's standing ankle height (source rest poses are not all on the ground: SMPL's is not)
    t_ankle_f1 = min(world_M[0][f"foot_{k}"].translation.z for k in "lr")
    c = t_ankle_rest - t_ankle_f1
    offs = Matrix.Translation((0, 0, c))
    for M in world_M:
        for n in under_pelvis:
            M[n] = offs @ M[n]

    # bake keys: armature-space A = root^-1 x world; basis = rest_rel^-1 x A_parent^-1 x A
    ad = arm.animation_data or arm.animation_data_create()
    ad.action = None
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    prev = {}
    for i in range(len(frames)):
        fo = i + 1
        Rinv = Matrix.Translation(-roots[i])
        A = {n: Rinv @ m for n, m in world_M[i].items()}
        arm.location = roots[i]
        arm.keyframe_insert("location", frame=fo)
        for b in order:
            n = b.name
            if b.parent:
                basis = rest_rel[n].inverted() @ A[b.parent.name].inverted() @ A[n]
            else:
                basis = b.matrix_local.inverted() @ A[n]
            loc, q, _s = basis.decompose()
            if n in prev:
                q.make_compatible(prev[n])
            prev[n] = q
            pb = arm.pose.bones[n]
            pb.rotation_quaternion = q
            pb.location = loc
            pb.keyframe_insert("rotation_quaternion", frame=fo, group=n)
            pb.keyframe_insert("location", frame=fo, group=n)
    act = ad.action
    scene.frame_start, scene.frame_end = 1, len(frames)
    world = world_M
    return act, world, roots, src_track, {"scale": scale, "z_offset_m": c, "first": first, "last": last}


def foot_drift(track, ankle, ball):
    """Max horizontal drift (cm) of the ball over any planted run, and peak planted speed (cm/s)."""
    az = [p[ankle].z for p in track]
    bz = [p[ball].z for p in track]
    amin, bmin = min(az), min(bz)
    planted = [(az[i] - amin) < CONTACT_TOL_M and (bz[i] - bmin) < CONTACT_TOL_M for i in range(len(track))]
    runs, cur = [], []
    for i, p in enumerate(planted):
        if p:
            cur.append(i)
        elif cur:
            runs.append(cur)
            cur = []
    if cur:
        runs.append(cur)
    worst, peak = 0.0, 0.0
    for r in runs:
        if len(r) < 3:
            continue
        base = track[r[0]][ball]
        for i in r:
            d = track[i][ball] - base
            worst = max(worst, math.hypot(d.x, d.y) * 100)
        for a, b in zip(r, r[1:]):
            d = track[b][ball] - track[a][ball]
            peak = max(peak, math.hypot(d.x, d.y) * 100 * FPS)
    return {"planted_frames": sum(planted), "runs": len([r for r in runs if len(r) >= 3]),
            "max_run_drift_cm": round(worst, 2), "peak_planted_speed_cmps": round(peak, 1)}


def mesh_stats(body, frames):
    out = {}
    scene = bpy.context.scene
    for f in frames:
        scene.frame_set(f)
        dg = bpy.context.evaluated_depsgraph_get()
        ev = body.evaluated_get(dg)
        me = ev.to_mesh()
        mw = ev.matrix_world
        zs = [(mw @ v.co).z for v in me.vertices]
        out[f] = (min(zs), max(zs))
        ev.to_mesh_clear()
    return out


def metrics(kind, world, roots, src_track, info, body, prof):
    n = len(world)
    trk = [{k: M[k].translation.copy() for k in ("pelvis", "head", "hand_l", "hand_r", "foot_l", "foot_r",
                                                 "ball_l", "ball_r")} for M in world]
    out = {"frames": n, "fps": FPS, "duration_s": round(n / FPS, 3), "leg_scale": round(info["scale"], 4),
           "z_offset_cm": round(info["z_offset_m"] * 100, 2)}
    feet = prof["feet"]
    out["foot_drift_target"] = {s: foot_drift(trk, f"foot_{s}", f"ball_{s}") for s in "lr"}
    out["foot_drift_source_scaled"] = {s: foot_drift(src_track, feet[s][0], feet[s][1]) for s in "lr"}
    samp = sorted(set(list(range(1, n + 1, 3)) + [n]))
    ms = mesh_stats(body, samp)
    out["mesh_min_z_cm"] = round(min(v[0] for v in ms.values()) * 100, 2)
    out["mesh_top_cm_first_last"] = [round(ms[1][1] * 100, 1), round(ms[n][1] * 100, 1)]
    out["root_total_cm"] = round(roots[-1].length * 100, 1)
    rs = [(roots[i + 1] - roots[i]).length * FPS for i in range(n - 1)]
    out["root_peak_speed_cmps"] = round(max(rs) * 100, 1) if rs else 0.0
    ps = [math.hypot(*(trk[i + 1]["pelvis"] - trk[i]["pelvis"]).xy) * FPS for i in range(n - 1)]
    onset = next((i for i, v in enumerate(ps) if v > SPEED_ONSET_MPS), None)
    out["move_onset_s"] = None if onset is None else round(onset / FPS, 3)

    def yaw(M):
        fwd = M["pelvis"].to_3x3() @ Vector((0, 0, 1))  # mannequin pelvis local Z points forward-ish; relative only
        return math.degrees(math.atan2(fwd.x, -fwd.y))
    if kind == "strike":
        reach = []
        for i, M in enumerate(world):
            hr, hl = M["hand_r"].translation, M["hand_l"].translation
            reach.append(max(-(hr.y - world[0]["pelvis"].translation.y), -(hl.y - world[0]["pelvis"].translation.y)))
        ci = max(range(n), key=lambda i: reach[i])
        out["contact_frame"] = ci + 1
        out["contact_s_from_clip_start"] = round(ci / FPS, 3)
        out["contact_s_from_onset"] = None if onset is None else round((ci - onset) / FPS, 3)
        out["hand_reach_from_start_pelvis_cm"] = round(reach[ci] * 100, 1)
        # first frame reaching 90 % of the peak forward reach gain: the visible "arm extended" moment
        gain0 = reach[0]
        i90 = next(i for i in range(n) if reach[i] - gain0 >= 0.9 * (reach[ci] - gain0))
        out["reach90_frame"] = i90 + 1
        out["reach90_s_from_clip_start"] = round(i90 / FPS, 3)
        out["root_at_reach90_cm"] = round(roots[i90].length * 100, 1)
        out["root_at_contact_cm"] = round(roots[ci].length * 100, 1)
        out["striking_hand"] = "hand_r" if -(world[ci]["hand_r"].translation.y) >= -(world[ci]["hand_l"].translation.y) else "hand_l"
    if kind == "brace":
        h0 = ms[1][1]
        hold = [f for f in samp if f > n // 2]
        tops = [ms[f][1] for f in hold]
        out["stature_ratio_hold_min_mean"] = [round(min(tops) / h0, 3), round(sum(tops) / len(tops) / h0, 3)]
        out["stature_ratio_min_clip"] = round(min(v[1] for v in ms.values()) / h0, 3)
        hs = n // 2
        d = trk[-1]["pelvis"] - trk[hs]["pelvis"]
        out["hold_pelvis_drift_cm"] = round(math.hypot(d.x, d.y) * 100, 2)
        out["hold_yaw_drift_deg"] = round(abs(yaw(world[-1]) - yaw(world[hs])), 2)
        out["hold_feet_drift_cm"] = {s: round(math.hypot(*(trk[-1][f"ball_{s}"] - trk[hs][f"ball_{s}"]).xy) * 100, 2)
                                     for s in "lr"}
        out["feet_width_hold_cm"] = round(math.hypot(*(trk[-1]["ball_l"] - trk[-1]["ball_r"]).xy) * 100, 1)
        out["feet_width_first_cm"] = round(math.hypot(*(trk[0]["ball_l"] - trk[0]["ball_r"]).xy) * 100, 1)
    if kind == "fall":
        pz = [t["pelvis"].z for t in trk]
        start = next((i for i in range(n) if pz[0] - pz[i] > 0.03), 0)
        settle = max((i for i in range(1, n) if abs(pz[i] - pz[i - 1]) * FPS > 0.05), default=0)
        out["fall_start_s"] = round(start / FPS, 3)
        out["fall_settle_s"] = round(settle / FPS, 3)
        out["fall_duration_s"] = round((settle - start) / FPS, 3)
        out["final_pelvis_z_cm"] = round(pz[-1] * 100, 1)
        d = trk[-1]["pelvis"] - trk[0]["pelvis"]
        out["pelvis_travel_cm"] = round(math.hypot(d.x, d.y) * 100, 1)
        out["final_top_cm"] = round(ms[n][1] * 100, 1)
    return out


def export_and_verify(arm, act, world, roots, fbx_path):
    act.name = os.path.splitext(os.path.basename(fbx_path))[0]
    for o in bpy.context.selected_objects:
        o.select_set(False)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    preset = dict(FBX_EXPORT_PRESET, object_types={"ARMATURE"}, bake_anim=True, bake_anim_use_all_bones=True,
                  bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                  bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0)
    bpy.ops.export_scene.fbx(filepath=fbx_path, **preset)
    nodes = fbx_skeleton_nodes(fbx_path)
    skeleton = [nm for nm, t in nodes if t in ("Root", "LimbNode")]
    nulls = [nm for nm, t in nodes if t == "Null"]
    res = {"skeleton_nodes": len(skeleton), "nulls": nulls, "bytes": os.path.getsize(fbx_path)}
    # re-import next to the baked rig (background Blender has no window to switch scenes) and compare
    scene = bpy.context.scene
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=fbx_path, use_anim=True, ignore_leaf_bones=False,
                             automatic_bone_orientation=False)
    scene.render.fps = FPS
    imp = next(o for o in bpy.data.objects if o not in before and o.type == "ARMATURE")
    ia = imp.animation_data.action if imp.animation_data else None
    res["reimport_frame_range"] = [round(v, 2) for v in ia.frame_range] if ia else None
    n = len(world)
    err = 0.0
    # the FBX importer starts the take at the exported frame offset (a 1-based bake re-imports from frame 2)
    off = (int(round(ia.frame_range[0])) - 1) if ia else 0
    res["reimport_frame_offset"] = off
    for f in sorted(set([1, n // 4, n // 2, (3 * n) // 4, n])):
        scene.frame_set(f + off)
        for b in ("pelvis", "hand_r", "foot_l", "ball_r", "head"):
            p = (imp.matrix_world @ imp.pose.bones[b].matrix).translation
            err = max(err, (p - world[f - 1][b].translation).length)
    res["reimport_max_pos_err_cm"] = round(err * 100, 3)
    for o in list(bpy.data.objects):
        if o not in before:
            bpy.data.objects.remove(o)
    if len(skeleton) != REFERENCE_BONE_COUNT or nulls:
        raise RuntimeError(f"{fbx_path}: {len(skeleton)} skeleton nodes, nulls {nulls}")
    return res


def _setup_render(scene):
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.color_type = "SINGLE"
    sh.show_cavity = False
    sh.show_shadows = False
    sh.show_specular_highlight = False
    sh.background_type = "VIEWPORT"
    scene.view_settings.view_transform = "Standard"
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.resolution_percentage = 100


def _look(cam, loc, target):
    cam.location = loc
    cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()


def render_views(clip, world, roots, outdir, extra_frames=()):
    from bpy_extras.object_utils import world_to_camera_view
    scene = bpy.context.scene
    _setup_render(scene)
    sh = scene.display.shading
    for o in scene.objects:
        if o.type == "MESH":
            o.hide_render = o.name != "Body_Base"
    cd = bpy.data.cameras.new("SpikeCam")
    cam = bpy.data.objects.new("SpikeCam", cd)
    scene.collection.objects.link(cam)
    scene.camera = cam
    n = len(world)
    frames = sorted(set([1 + round(i * (n - 1) / 7) for i in range(8)] + list(extra_frames)))
    ys = [M["pelvis"].translation.y for M in world]
    ymid = (min(ys) + max(ys)) / 2
    files = []
    views = [("close", None), ("profile20", 20.0), ("profile30", 30.0), ("front20", 20.0)]
    for view, dist in views:
        if view == "close":
            sh.light = "STUDIO"
            sh.single_color = (0.80, 0.78, 0.75)
            sh.background_color = (1.0, 1.0, 1.0)
            cd.type, cd.sensor_fit, cd.lens = "PERSP", "AUTO", 40
            scene.render.resolution_x = scene.render.resolution_y = 640
            scene.render.use_border = False
            _look(cam, (2.9, ymid - 2.9, 1.3), (0, ymid, 0.8))
        else:
            sh.light = "FLAT"
            sh.single_color = (0.16, 0.16, 0.16)
            sh.background_color = (0.62, 0.62, 0.62)
            cd.type, cd.sensor_fit = "PERSP", "VERTICAL"
            cd.angle_y = math.radians(60.0)
            scene.render.resolution_x, scene.render.resolution_y = 1920, 1080
            if view.startswith("profile"):
                _look(cam, (dist, ymid, 1.6), (0, ymid, 1.6))
            else:
                _look(cam, (0.0, ymid - dist, 1.6), (0, ymid, 1.6))
            bpy.context.view_layer.update()
            pts = [Vector((x, y, z)) for x in (-1.2, 1.2) for y in (min(ys) - 1.3, max(ys) + 1.3) for z in (-0.05, 1.95)]
            cs = [world_to_camera_view(scene, cam, p) for p in pts]
            scene.render.use_border = True
            scene.render.use_crop_to_border = True
            scene.render.border_min_x = max(0.0, min(c.x for c in cs))
            scene.render.border_max_x = min(1.0, max(c.x for c in cs))
            scene.render.border_min_y = max(0.0, min(c.y for c in cs))
            scene.render.border_max_y = min(1.0, max(c.y for c in cs))
        for f in frames:
            scene.frame_set(f)
            path = os.path.join(outdir, f"{clip}_{view}_f{f:03d}.png")
            scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            files.append(os.path.relpath(path, REPO).replace("\\", "/"))
    scene.render.use_border = False
    return frames, files


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--action")
    ap.add_argument("--profile", choices=sorted(PROFILES), required=True)
    ap.add_argument("--clip", required=True)
    ap.add_argument("--kind", choices=("strike", "fall", "brace"), required=True)
    ap.add_argument("--root", choices=("forward", "none"), default="none")
    ap.add_argument("--out-fbx", required=True)
    ap.add_argument("--evidence", required=True)
    args = ap.parse_args(argv)
    os.makedirs(os.path.dirname(os.path.abspath(args.out_fbx)), exist_ok=True)
    os.makedirs(args.evidence, exist_ok=True)

    bpy.ops.wm.open_mainfile(filepath=BLEND, load_ui=False)
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.render.fps_base = 1.0
    arm = bpy.data.objects["root"]
    body = bpy.data.objects["Body_Base"]
    if arm.parent is not None or any(abs(v - 1.0) > 1e-6 for v in arm.scale):
        raise RuntimeError("armature 'root' must be unparented at scale 1.0")
    src, first, last = import_source(args)
    act, world, roots, src_track, info = retarget(arm, src, first, last, args.profile, args.root)
    # retarget() bakes onto `arm`; the source no longer needs to animate (keeps frame_set cheap)
    src.animation_data.action = None
    m = metrics(args.kind, world, roots, src_track, info, body, PROFILES[args.profile])
    m.update({"clip": args.clip, "kind": args.kind, "profile": args.profile, "root_mode": args.root,
              "source": os.path.relpath(os.path.abspath(args.src), REPO).replace("\\", "/"),
              "source_action": args.action, "source_frames": [first, last]})
    m["fbx"] = export_and_verify(arm, act, world, roots, os.path.abspath(args.out_fbx))
    m["fbx_path"] = os.path.relpath(os.path.abspath(args.out_fbx), REPO).replace("\\", "/")
    extra = [m["contact_frame"], m["reach90_frame"]] if "contact_frame" in m else []
    m["render_frames"], m["renders"] = render_views(args.clip, world, roots, os.path.abspath(args.evidence), extra)
    with open(os.path.join(args.evidence, f"{args.clip}_metrics.json"), "w", encoding="utf-8") as fh:
        json.dump(m, fh, indent=2)
    print("RETARGET_DONE", json.dumps({k: v for k, v in m.items() if k != "renders"}))


if __name__ == "__main__":
    main()
