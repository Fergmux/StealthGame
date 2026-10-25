"""Body measurements for the Smoking Mirror character pool (art bible 1.3.2, 1.3.7 T1 items 1-5, 17-18).

Pure bpy/mathutils helpers shared by `body_base_t1_audit.py` and any later pool audit (story 008 mirrors the
same definitions on the Unreal side). Everything is returned in **centimetres**, measured in Blender world space
on the evaluated mesh, with the UE mannequin convention as Blender imports it: Z up, the figure faces -Y, the
figure's left side is +X.

Definitions (they are the contract wording of art bible 1.3.2 after OI-1, keep them in sync):
  stature        max Z of the mesh minus the sole plane (min Z); rest pose, no hair or headwear on Body_Base.
  head height    crown (max Z) minus chin. Chin = lowest height between neck_01 and head+5 cm at which the
                 front profile (most forward surface point with |X| < 12 cm) has moved at least half-way from
                 the neck front (at neck_01 height) to the face front (most forward point above the head bone).
  shoulder width outer-skin width (2 x max |X|) of the horizontal section at the upperarm_l joint height.
  chest depth    max front-to-back extent of the torso section (|X| < 14 cm) between spine_03 and upperarm_l.
  hip width      max outer width (2 x max |X|, |X| < 30 cm so the hands are excluded) between thigh_twist_01_l
                 and pelvis + 2 cm.
  foot length    heel-to-toe Y extent of the left foot (X > 0, Z < ball_l + 6 cm).
  chains         distances between bone origins in the armature rest pose, through intermediate bones
                 (spine: pelvis..spine_05..neck_01; neck: neck_01..neck_02..head; hand: hand..middle_03),
                 the hand's straight-line distance is reported beside it (spike 2026-10-25 section 4).
"""
from mathutils import Vector

# name -> bone path whose consecutive head-to-head distances are summed
CHAINS = {
    "thigh": ["thigh_l", "calf_l"],
    "shin": ["calf_l", "foot_l"],
    "foot": ["foot_l", "ball_l"],
    "spine": ["pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "spine_05", "neck_01"],
    "clavicle_to_upperarm": ["clavicle_l", "upperarm_l"],
    "upperarm": ["upperarm_l", "lowerarm_l"],
    "forearm": ["lowerarm_l", "hand_l"],
    "hand": ["hand_l", "middle_metacarpal_l", "middle_01_l", "middle_02_l", "middle_03_l"],
    "neck": ["neck_01", "neck_02", "head"],
}


def bone_heads_cm(arm_obj, use_pose=False):
    """World-space bone head positions in cm, keyed by bone name (rest pose unless use_pose)."""
    out = {}
    mw = arm_obj.matrix_world
    if use_pose:
        for pb in arm_obj.pose.bones:
            out[pb.name] = (mw @ pb.head) * 100.0
    else:
        for b in arm_obj.data.bones:
            out[b.name] = (mw @ b.head_local) * 100.0
    return out


def chain_lengths_cm(heads):
    """The nine reference chains (cm) plus 'hand_straight', from a bone_heads_cm() dict."""
    res = {}
    for name, path in CHAINS.items():
        res[name] = sum((heads[path[i + 1]] - heads[path[i]]).length for i in range(len(path) - 1))
    res["hand_straight"] = (heads["middle_03_l"] - heads["hand_l"]).length
    return res


class SectionMesh:
    """World-space (cm) vertices and edges of an evaluated mesh object, with horizontal sectioning."""

    def __init__(self, obj, depsgraph):
        ev = obj.evaluated_get(depsgraph)
        me = ev.to_mesh()
        mw = obj.matrix_world
        self.V = [(mw @ v.co) * 100.0 for v in me.vertices]
        self.E = [tuple(e.vertices) for e in me.edges]
        self.tris = sum(len(p.vertices) - 2 for p in me.polygons)
        ev.to_mesh_clear()

    def section(self, z):
        """Points where mesh edges cross the horizontal plane at height z (cm)."""
        pts = []
        V = self.V
        for a, b in self.E:
            za, zb = V[a].z, V[b].z
            if (za - z) * (zb - z) < 0.0:
                pts.append(V[a].lerp(V[b], (z - za) / (zb - za)))
        return pts


def body_dimensions_cm(sm, heads):
    """Stature, head height, shoulder, chest depth, hip width and foot length (cm) per the module docstring."""
    V = sm.V
    zmin = min(v.z for v in V)
    zmax = max(v.z for v in V)
    d = {"sole_z": zmin, "stature": zmax - zmin}

    def front(z):
        p = [q for q in sm.section(z) if abs(q.x) < 12.0]
        return min(q.y for q in p) if p else None

    face_front = min(f for f in (front(heads["head"].z + 0.25 * i) for i in range(1, 40)) if f is not None)
    neck_front = front(heads["neck_01"].z)
    half = 0.5 * (face_front + neck_front)
    chin = None
    z = heads["neck_01"].z
    while z <= heads["head"].z + 5.0:
        f = front(z)
        if f is not None and f <= half:
            chin = z
            break
        z += 0.1
    d["chin_z"] = chin
    d["head_height"] = (zmax - chin) if chin is not None else None

    sh = sm.section(heads["upperarm_l"].z)
    d["shoulder_width"] = 2.0 * max(abs(q.x) for q in sh)

    best = 0.0
    z = heads["spine_03"].z
    while z <= heads["upperarm_l"].z:
        p = [q for q in sm.section(z) if abs(q.x) < 14.0]
        if p:
            best = max(best, max(q.y for q in p) - min(q.y for q in p))
        z += 0.5
    d["chest_depth"] = best

    best = 0.0
    z = heads["thigh_twist_01_l"].z
    while z <= heads["pelvis"].z + 2.0:
        p = [q for q in sm.section(z) if abs(q.x) < 30.0]
        if p:
            best = max(best, 2.0 * max(abs(q.x) for q in p))
        z += 0.5
    d["hip_width"] = best

    fv = [v for v in V if v.x > 0.0 and v.z < heads["ball_l"].z + 6.0]
    d["foot_length"] = max(v.y for v in fv) - min(v.y for v in fv)
    return d


def max_influences(mesh_obj, bone_names, eps=1e-6):
    """Largest number of deform-bone weights > eps on any vertex, and the count of vertices above 4."""
    idx = {g.index for g in mesh_obj.vertex_groups if g.name in bone_names}
    worst, over = 0, 0
    for v in mesh_obj.data.vertices:
        n = sum(1 for g in v.groups if g.group in idx and g.weight > eps)
        worst = max(worst, n)
        over += n > 4
    return worst, over


def fbx_skeleton_nodes(path):
    """(name, node type) of every FBX Model node, read with Blender's bundled FBX parser.

    Unreal builds the skeleton from the Root/LimbNode chain; a Null above it (an exported armature object or
    empty) is what produced the prototype's extra `SKM_UEFN_Mannequin` bone.
    """
    from io_scene_fbx import parse_fbx
    root, _ver = parse_fbx.parse(path)
    objs = [e for e in root.elems if e.id == b"Objects"][0]
    out = []
    for e in objs.elems:
        if e.id == b"Model":
            out.append((e.props[1].split(b"\x00\x01")[0].decode(), e.props[2].decode()))
    return out
