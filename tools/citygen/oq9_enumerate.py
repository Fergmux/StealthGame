#!/usr/bin/env python3
"""OQ9 / C-W24 grid-only final-court enumeration (procedural city generator, story 008).

Measures, on the integer cell grid and with no geometry, how often a candidate skeleton alone can
reach FinalCandidatesMinCount (6) final-eligible courts at the default StartToFinalMinPathCm
(44 m), under two anchor policies ("fixed": the S4 court junctions stay at their mean-lattice
cells; "moving": they follow the jittered cut positions).

Usage:
    python tools/citygen/oq9_enumerate.py run [--out-dir DIR]   write the report + CSV
    python tools/citygen/oq9_enumerate.py selftest              run the self-test (exit 0 = pass)

The script emits observations (counts, fractions, histograms, margins). The verdict word is the
documented comparison of AC3, derived from the CSV fractions by `verdict_from_rows()`.

Model (the S1/S2/S5 pieces this story needs; the C++ stages 003/005/006 do not exist yet, so the
script carries its own minimal copy, to be replaced by the real stages when stories 010/011 land):
  * Integer-only PCG32 streams, ranged draws by multiply-shift with rejection, stage seeds from a
    local 32-bit mixer (stand-in for the Session reference mixer; story 003 owns the real one).
  * S1: N in 62..66 (uniform), 3-cell sealed perimeter, cells 2.0 m, grid index (x, y).
  * S2: 6 corridors per axis (lane, 4 interior cuts, lane), 5 blocks of 7-10 cells with a fixed
    sum, interior corridor width 3 (65 %) or 2 (35 %) cells, lanes 3 cells, shared by both axes
    (transpose symmetry). The skeleton is generated for the half x >= y (half A) and reflected
    (half B); every cut of every half and axis is then jittered by an independent integer in
    [-d, d] cells (identical topology, blocks absorb the shift).
  * S4 courts (the stratified axis-corner pool, 3 per corner): near corner = {(cut1, cut1),
    (cut2, lane0), (lane0, cut2)}, far corner = the 180-degree images {(cut4, cut4), (lane5, cut3),
    (cut3, lane5)}. The GDD's nominal (31,31), (50,9), (9,50) m are the mean-lattice positions of
    the near-corner trio when every block is 8 cells and every corridor is 3 (the GDD quotes (9,50)
    and (50,9); the lattice centreline is at 53 m, the same x + y = 62).
  * S5 zones: default 2 zones at the anti-diagonal corners; anchor = the junction (cut1, cut4) in
    half B and (cut4, cut1) in half A (nominal (31,97) and (97,31) m); start cells = every walkable
    cell within `R` metres of grid path of the anchor (the best case for the player, the worst case
    for eligibility: start cells may sit anywhere in the disc).
  * Grid path = 4-connected BFS over corridor cells x 2 m (the lattice Manhattan metric, no jogs,
    no canals, no corner cutting; each omission lengthens or leaves unchanged the true path, so the
    omissions are conservative for eligibility but not for the sealed-block detour check).
  * Final-eligible = grid path from the nearest start cell of either zone >= 1.08 * F (the 8 %
    path guard; integer test path_cm * 100 >= F_cm * 108), >= 3 ingress arms, and >= 24 m from
    every other counted court of its corner.
"""
import collections
import csv
import hashlib
import io
import os
import re
import sys
import tempfile

M32 = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF
CELL_CM = 200
PERIMETER = 3

# ---------------------------------------------------------------------------------------------
# Committed seed list (200 skeleton seeds). Generated once as mix32(0x0C9A9E09, i), i = 0..199, and
# frozen here; never regenerated at run time (ADR-0008 section 9: no file read, no run-time list).
# ---------------------------------------------------------------------------------------------
SEEDS = (
    0xB6121DCE, 0xB1A4F8F4, 0x89A276C9, 0x9D19F597, 0x25F43A46, 0x77FE2101, 0xB3ABED5B, 0x1F05CCB9,
    0x7FD52C3F, 0x040C11CB, 0xE3B51D7D, 0xE36DDEE1, 0x767BC2F8, 0x734DBD6B, 0xB8606422, 0x3BE909DE,
    0xE9DF9525, 0x59A24325, 0xF1A3BDCB, 0x7E506F7C, 0xA849F65A, 0xACCF7B09, 0x06645106, 0xE448BCDF,
    0x68E849EB, 0xAE3F92C9, 0x9644B58E, 0x9CCA0AC5, 0x09C77B6B, 0x7FBC4BA4, 0xD343EA38, 0x2E31546F,
    0x9C180162, 0x268373E4, 0xD282363F, 0xE155DCB9, 0x9D1CF7EF, 0xC6697858, 0x93C2F320, 0xFF040CD3,
    0x797098B3, 0xF91F051E, 0xCD905EFA, 0x728CDBC3, 0x8CC66967, 0xD06694BE, 0x1F90D6CB, 0x472918DC,
    0x7B640A37, 0x351D78CB, 0xA7C5EDDF, 0xB69B04C4, 0x468B3B87, 0x6F5CD664, 0x084FD374, 0x812DE0C3,
    0xBEB64CC7, 0x8DFA7A3A, 0x923EB935, 0xF1C1917C, 0x6329DD9B, 0x1781A96E, 0x63A8FBA8, 0x333A7846,
    0x48A0D7B8, 0x8D27B7C6, 0xA3B6F7EC, 0x7441E0BE, 0x7DB199B0, 0x1FBAE242, 0xE1A16ABA, 0x1AA5BF80,
    0xE293BD02, 0x4DBE6A9E, 0x54985AB4, 0x437DFD20, 0x7291EA03, 0xD3F2AEC8, 0x1C4CBC19, 0x938B2457,
    0xFBBA2F8B, 0x675B138B, 0xDC8FE764, 0x4E5BB5A6, 0x8A30F262, 0x3C9A82C5, 0xC425FDC0, 0x1F16600A,
    0xAED0BF1A, 0xE2F5A512, 0x40923B8F, 0x7148AD44, 0xD000FE83, 0x2F703461, 0xCB019383, 0xB20DE47A,
    0xD34DADDC, 0xFB71F2E1, 0x2D710DFD, 0xF12A51B7, 0x6B5BBD69, 0x3336167C, 0x29D20F77, 0xD64BCAB7,
    0x3D08F0D7, 0x9BBEF36C, 0x804EAE33, 0x3FF52339, 0xD5EE68BE, 0x2444C7F5, 0xF5AC866E, 0x02C64FC4,
    0x551BEDA1, 0x924D425F, 0x45899B54, 0x2D4AD8E6, 0x105F91FF, 0x93E6B3F7, 0xED11A533, 0x72318493,
    0x2F9E26E6, 0x29965917, 0x1589D8C3, 0x66AF1575, 0xA8A7F7C6, 0x44D9B941, 0x82E195BD, 0x0E626AB7,
    0x5155082B, 0x008D6CC6, 0xA05CC625, 0x54C15FA1, 0x28D89FB7, 0xE2F97C54, 0xEEEFA789, 0x80C04D23,
    0x03CF02E8, 0xA3CD5F18, 0xE542FCD4, 0x4B0D067E, 0xE1041DD9, 0x6CA65482, 0x844F7270, 0xD6759F64,
    0xACCBA68C, 0x024842CD, 0x6B9FCDA9, 0xFB71FA72, 0x56A855B5, 0x816C88F8, 0xA0C5438A, 0x33A2B973,
    0x252863BD, 0xBE5A6B1E, 0x16D22355, 0x24AB48EA, 0xCEA013EE, 0xBC591622, 0x57D92720, 0xC9115AC9,
    0xC19F6BC3, 0x3B1788D2, 0x082A07DC, 0xF6B68AF8, 0x9663201E, 0xA6A30517, 0x9E1201E2, 0xCCAEC71F,
    0x392E0D5B, 0x3D788DFC, 0xEE3C5EF4, 0x1956BBD7, 0xD3E8F9E6, 0x147AAE1C, 0x46E7D55E, 0xE2B591AD,
    0xCAC69DC3, 0x3969A13D, 0x8CAD316A, 0xE8694988, 0xA122777C, 0x1A2C5557, 0xEFFABF33, 0xDD7CA144,
    0x8CB99837, 0x79D948EF, 0xC3DFD736, 0xB9848D74, 0x662BF207, 0x2DCA2D87, 0x567192B7, 0xCFCE1E5D,
    0x9F3A84BF, 0x4884E963, 0x6F59C251, 0xDCA2B903, 0x28F1A08F, 0xAE0DD52B, 0xB89751FE, 0x1CEF5397
)

DEFAULTS = dict(F_m=44, R_m=18, jitter=1)
FINAL_CANDIDATES_MIN_COUNT = 6   # LSC FinalCandidatesMinCount: never lowered here
PASS_FRACTION_PCT = 30           # F6 p_g design floor (>= 0.30)
COURT_SPACING_CELLS = 12         # 24 m court spacing (S4)
POLICIES = ("fixed", "moving")

# Scenario table: (name, tier, F_m, R_m, jitter). Tier 0 = baseline, 1 = levers (a) / jitter,
# 2 = lever (b) lower StartToFinalMinPathCm (LSC hard minimum 40 m). Order = order the levers are tried.
SCENARIOS = (
    ("baseline", 0, 44, 18, 1),
    ("a_R16", 1, 44, 16, 1),
    ("j0", 1, 44, 18, 0),
    ("j2", 1, 44, 18, 2),
    ("a_R16+j0", 1, 44, 16, 0),
    ("a_R16+j2", 1, 44, 16, 2),
    ("b_F43", 2, 43, 18, 1),
    ("b_F42", 2, 42, 18, 1),
    ("b_F40", 2, 40, 18, 1),
    ("a_R16+b_F40", 2, 40, 16, 1),
    ("a_R16+j0+b_F40", 2, 40, 16, 0),
)

CSV_FIELDS = ("scenario", "tier", "F_m", "R_m", "jitter", "policy", "seed_index", "seed", "N",
              "final_count", "reaches_min", "adaptive_count", "axis_margin_cm", "formula_count",
              "formula_court_agree")


# ---------------------------------------------------------------------------------------------
# Integer RNG (PCG32, ranged draws by multiply-shift with rejection) and the local stage mixer
# ---------------------------------------------------------------------------------------------
def mix32(*vals):
    """Local pure 32-bit mixer (murmur3 finaliser rounds); stand-in for story 003's mixer."""
    h = 0x9E3779B9
    for v in vals:
        h ^= v & M32
        h = (h * 0x85EBCA6B) & M32
        h ^= h >> 13
        h = (h * 0xC2B2AE35) & M32
        h ^= h >> 16
    return h


class Pcg32:
    """PCG32 (XSH-RR), integer only."""

    def __init__(self, initstate, initseq):
        self.state = 0
        self.inc = ((initseq << 1) | 1) & M64
        self.next()
        self.state = (self.state + initstate) & M64
        self.next()

    def next(self):
        old = self.state
        self.state = (old * 6364136223846793005 + self.inc) & M64
        xorshifted = (((old >> 18) ^ old) >> 27) & M32
        rot = old >> 59
        return ((xorshifted >> rot) | (xorshifted << ((-rot) & 31))) & M32

    def bounded(self, n):
        """Uniform integer in [0, n) by multiply-shift with rejection (no mod bias, no floats)."""
        x = self.next()
        m = x * n
        low = m & M32
        if low < n:
            threshold = ((-n) & M32) % n
            while low < threshold:
                x = self.next()
                m = x * n
                low = m & M32
        return m >> 32


def stage_stream(seed, stage, sub):
    s = mix32(seed, stage, sub)
    return Pcg32(s, mix32(s, 0x57A6E))


# ---------------------------------------------------------------------------------------------
# Skeleton (S1 frame + S2 lattice + jitter) on the integer grid
# ---------------------------------------------------------------------------------------------
class Skel:
    """One skeleton: N, widths[6], blocks[5], jitter jit[half][axis][cut 1..4] (cells).

    half 0 = A (x >= y), half 1 = B (x < y); axis 0 = x cuts (vertical corridors), 1 = y cuts.
    """

    def __init__(self, n, widths, blocks, jit):
        self.n = n
        self.widths = tuple(widths)
        self.blocks = tuple(blocks)
        self.jit = jit
        e = []
        pos = PERIMETER
        for k in range(6):
            e.append(pos)
            pos += self.widths[k]
            if k < 5:
                pos += self.blocks[k]
        assert pos == n - PERIMETER, (pos, n)
        self.edges = tuple(e)
        # centres[h][ax][k]: centre cell of corridor k (k = 0..5; lanes are never jittered)
        self.centers = [[[0] * 6 for _ in range(2)] for _ in range(2)]
        inb = [[[False] * n for _ in range(2)] for _ in range(2)]
        for h in range(2):
            for ax in range(2):
                for k in range(6):
                    j = jit[h][ax][k - 1] if 1 <= k <= 4 else 0
                    s = self.edges[k] + j
                    self.centers[h][ax][k] = s + (self.widths[k] - 1) // 2
                    for c in range(s, s + self.widths[k]):
                        if 0 <= c < n:
                            inb[h][ax][c] = True
        lo, hi = PERIMETER, n - PERIMETER - 1
        walk = [False] * (n * n)
        for y in range(lo, hi + 1):
            for x in range(lo, hi + 1):
                h = 0 if x >= y else 1
                walk[y * n + x] = inb[h][0][x] or inb[h][1][y]
        self.walk = walk

    def is_walk(self, x, y):
        return 0 <= x < self.n and 0 <= y < self.n and self.walk[y * self.n + x]


def zero_jit():
    return [[[0] * 4 for _ in range(2)] for _ in range(2)]


def draw_skeleton(seed, jitter):
    """Skeleton of `seed` with jitter amplitude `jitter` cells (0..2). Blocks/widths/N do not depend on it."""
    r1 = stage_stream(seed, 1, 0)
    n = 62 + r1.bounded(5)
    r2 = stage_stream(seed, 2, 0)
    widths = [3] + [3 if r2.bounded(100) < 65 else 2 for _ in range(4)] + [3]
    bsum = n - 2 * PERIMETER - sum(widths)
    cum = (30, 70, 90, 100)  # block size weights 7:30 8:40 9:20 10:10
    blocks = []
    for _ in range(5):
        r = r2.bounded(100)
        blocks.append(7 + sum(1 for c in cum[:3] if r >= c))
    while sum(blocks) != bsum:
        start = r2.bounded(5)
        step = -1 if sum(blocks) > bsum else 1
        for t in range(5):
            i = (start + t) % 5
            if 7 <= blocks[i] + step <= 10:
                blocks[i] += step
                break
    rj = stage_stream(seed, 2, 1)
    raw = [rj.next() for _ in range(16)]
    jit = zero_jit()
    for h in range(2):
        for ax in range(2):
            for k in range(4):
                u = raw[h * 8 + ax * 4 + k]
                jit[h][ax][k] = ((u * (2 * jitter + 1)) >> 32) - jitter
    return Skel(n, widths, blocks, jit)


# ---------------------------------------------------------------------------------------------
# Grid measurement
# ---------------------------------------------------------------------------------------------
def bfs(skel, sources, limit=None):
    """Multi-source 4-connected BFS over walkable cells; returns dist (steps) list, -1 = unreached."""
    n = skel.n
    dist = [-1] * (n * n)
    q = collections.deque()
    for (x, y) in sources:
        dist[y * n + x] = 0
        q.append((x, y))
    while q:
        x, y = q.popleft()
        d = dist[y * n + x]
        if limit is not None and d >= limit:
            continue
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if skel.is_walk(nx, ny) and dist[ny * n + nx] < 0:
                dist[ny * n + nx] = d + 1
                q.append((nx, ny))
    return dist


def snap_walkable(skel, x, y):
    """Nearest walkable cell to (x, y): L1 rings, row-major within a ring (canonical)."""
    if skel.is_walk(x, y):
        return (x, y)
    for rad in range(1, 8):
        ring = []
        for dy in range(-rad, rad + 1):
            for dx in range(-rad, rad + 1):
                if abs(dx) + abs(dy) == rad and skel.is_walk(x + dx, y + dy):
                    ring.append((y + dy, x + dx))
        if ring:
            ring.sort()
            return (ring[0][1], ring[0][0])
    raise ValueError("no walkable cell near (%d, %d)" % (x, y))


def court_cells(skel):
    """The six S4 axis-corner court junction cells of `skel` (before snapping): near trio then far trio."""
    n = skel.n
    c = skel.centers
    lane0, lane5 = c[0][0][0], c[0][0][5]
    A, B = 0, 1
    return (
        ("near_axis", (c[A][0][1], c[A][1][1])),
        ("near_laneA", (c[A][0][2], lane0)),
        ("near_laneB", (lane0, c[B][1][2])),
        ("far_axis", (c[A][0][4], c[A][1][4])),
        ("far_laneA", (lane5, c[A][1][3])),
        ("far_laneB", (c[B][0][3], lane5)),
    )


def zone_anchors(skel):
    c = skel.centers
    return ((c[1][0][1], c[1][1][4]),   # zone 1 (x small, y large): half B
            (c[0][0][4], c[0][1][1]))   # zone 2 (x large, y small): half A


def ingress_count(skel, x, y):
    cnt = 0
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        if all(skel.is_walk(x + dx * t, y + dy * t) for t in (1, 2, 3)):
            cnt += 1
    return cnt


def path_ok(path_cm, f_cm):
    """Integer G-PRE test: path >= 1.08 * F (8 % path guard)."""
    return path_cm * 100 >= f_cm * 108


def margin_cm(path_cm, f_cm):
    assert (f_cm * 108) % 100 == 0
    return path_cm - f_cm * 108 // 100


def max_independent(cands):
    """Exact max set of cells pairwise >= 12 cells apart (Euclid), by branch and bound."""
    best = [0]

    def far(a, b):
        return (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 >= COURT_SPACING_CELLS ** 2

    def rec(i, chosen):
        if len(chosen) + (len(cands) - i) <= best[0]:
            return
        if i == len(cands):
            best[0] = max(best[0], len(chosen))
            return
        if all(far(cands[i], o) for o in chosen):
            rec(i + 1, chosen + [cands[i]])
        rec(i + 1, chosen)

    rec(0, [])
    return best[0]


def evaluate(skel_j, skel_mean, f_cm, r_cm, policy, court_override=None):
    """One skeleton, one anchor policy -> dict of observations.

    skel_j: the jittered skeleton (what is measured). skel_mean: the same skeleton with zero jitter
    (source of the 'fixed' court cells). court_override: optional list of 6 cells (fixtures).
    """
    n = skel_j.n
    anchors = [snap_walkable(skel_j, *a) for a in zone_anchors(skel_j)]
    r_steps = r_cm // CELL_CM
    sources = []
    for a in anchors:
        d = bfs(skel_j, [a], limit=r_steps)
        sources += [(i % n, i // n) for i, v in enumerate(d) if v >= 0]
    field = bfs(skel_j, sorted(set(sources), key=lambda p: (p[1], p[0])))
    if court_override is not None:
        names = [nm for nm, _ in court_cells(skel_j)]
        cells = list(zip(names, court_override))
    elif policy == "fixed":
        cells = court_cells(skel_mean)
    else:
        cells = court_cells(skel_j)
    cells = [(nm, snap_walkable(skel_j, x, y)) for nm, (x, y) in cells]
    # formula check: Manhattan from the mean-geometry anchors, nominal-radius start cells
    mean_anchors = zone_anchors(skel_mean)
    kept, rows = [], []
    formula_count = agree = 0
    for idx, (nm, (x, y)) in enumerate(cells):
        path_cm = field[y * n + x] * CELL_CM
        eligible = path_ok(path_cm, f_cm)
        ingress = ingress_count(skel_j, x, y)
        corner = idx // 3
        spaced = all((x - ox) ** 2 + (y - oy) ** 2 >= COURT_SPACING_CELLS ** 2
                     for (c2, ox, oy) in kept if c2 == corner)
        ok = eligible and ingress >= 3 and spaced
        if ok:
            kept.append((corner, x, y))
        man = min(abs(x - ax) + abs(y - ay) for ax, ay in mean_anchors) * CELL_CM
        f_pred = path_ok(max(0, man - r_cm), f_cm)
        formula_count += 1 if f_pred else 0
        agree += 1 if f_pred == eligible else 0
        rows.append((nm, path_cm, eligible, ingress, spaced))
    # adaptive diagnostic: best S4-free pick among every eligible lattice junction (not the S4 policy)
    cand = set()
    for h in range(2):
        for a in range(6):
            for b in range(6):
                x, y = skel_j.centers[h][0][a], skel_j.centers[h][1][b]
                if (h == 0) == (x >= y) and skel_j.is_walk(x, y):
                    cand.add((x, y))
    elig = sorted(p for p in cand
                  if ingress_count(skel_j, *p) >= 3 and path_ok(field[p[1] * n + p[0]] * CELL_CM, f_cm))
    adaptive = max_independent([p for p in elig if p[0] + p[1] < n]) if elig else 0
    ax_cell = cells[0][1]
    axis_path = field[ax_cell[1] * n + ax_cell[0]] * CELL_CM
    return dict(final_count=len(kept), adaptive_count=adaptive,
                axis_margin_cm=margin_cm(axis_path, f_cm), formula_count=formula_count,
                formula_court_agree=agree, courts=rows)


# ---------------------------------------------------------------------------------------------
# The enumeration, CSV, verdict
# ---------------------------------------------------------------------------------------------
def run_enumeration():
    """Return the CSV rows (list of dicts) for every scenario x policy x seed."""
    rows = []
    cache = {}
    for (name, tier, f_m, r_m, jit) in SCENARIOS:
        for policy in POLICIES:
            for si, seed in enumerate(SEEDS):
                key = (seed, jit)
                if key not in cache:
                    cache[key] = (draw_skeleton(seed, jit), draw_skeleton(seed, 0))
                sj, sm = cache[key]
                ev = evaluate(sj, sm, f_m * 100, r_m * 100, policy)
                rows.append(dict(
                    scenario=name, tier=tier, F_m=f_m, R_m=r_m, jitter=jit, policy=policy,
                    seed_index=si, seed="0x%08X" % seed, N=sj.n, final_count=ev["final_count"],
                    reaches_min=1 if ev["final_count"] >= FINAL_CANDIDATES_MIN_COUNT else 0,
                    adaptive_count=ev["adaptive_count"], axis_margin_cm=ev["axis_margin_cm"],
                    formula_count=ev["formula_count"], formula_court_agree=ev["formula_court_agree"]))
    return rows


def rows_to_csv(rows):
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=CSV_FIELDS, lineterminator="\n")
    w.writeheader()
    for r in rows:
        w.writerow(r)
    return buf.getvalue()


def csv_to_rows(text):
    return [dict(r) for r in csv.DictReader(io.StringIO(text))]


def reach_counts(rows):
    """{(scenario, policy): (n_reaching, n_total)} from CSV rows."""
    out = {}
    for r in rows:
        k = (r["scenario"], r["policy"])
        a, b = out.get(k, (0, 0))
        out[k] = (a + int(r["reaches_min"]), b + 1)
    return out


def meets_floor(n_reaching, n_total):
    """fraction >= 0.30, exact integer test (exactly 0.30 passes)."""
    return n_reaching * 100 >= PASS_FRACTION_PCT * n_total


def lower_policy_count(counts, scenario):
    """The conservative reading: the lower of the two policies' (n, total) for a scenario."""
    a = counts[(scenario, "fixed")]
    b = counts[(scenario, "moving")]
    return a if a[0] * b[1] <= b[0] * a[1] else b


def verdict_from_rows(rows):
    """(word, lever) per AC3. PASS: baseline lower fraction >= 0.30. CONCERNS: it is < 0.30 but some
    lever (a) / jitter 0 or 2 (tier 1), then lever (b) lower F (tier 2), reaches >= 0.30 (lever cited
    = the first one in SCENARIOS order). FAIL: < 0.30 after every lever."""
    counts = reach_counts(rows)
    n, t = lower_policy_count(counts, "baseline")
    if meets_floor(n, t):
        return "PASS", "baseline"
    for (name, tier, _f, _r, _j) in SCENARIOS:
        if tier >= 1:
            n, t = lower_policy_count(counts, name)
            if meets_floor(n, t):
                return "CONCERNS", name
    return "FAIL", "none"


def percentile(sorted_vals, pct):
    return sorted_vals[min(len(sorted_vals) - 1, (len(sorted_vals) * pct) // 100)]


def histogram(rows, scenario, policy, field, lo, hi):
    h = collections.Counter(int(r[field]) for r in rows if r["scenario"] == scenario and r["policy"] == policy)
    return [h.get(v, 0) for v in range(lo, hi + 1)]


# ---------------------------------------------------------------------------------------------
# Fixtures (AC2)
# ---------------------------------------------------------------------------------------------
def mean_skeleton():
    """N = 64, every corridor 3 cells, every block 8 cells, no jitter: the GDD mean geometry."""
    return Skel(64, [3] * 6, [8] * 5, zero_jit())


def fixture_results():
    sk = mean_skeleton()
    base = evaluate(sk, sk, 4400, 1800, "moving")
    cells = [c for _, c in court_cells(sk)]
    shifted_x = list(cells)
    shifted_x[0] = (cells[0][0] + 1, cells[0][1])      # (31,31) -> (33,31): one cell (2 m) outward in x
    moved_x = evaluate(sk, sk, 4400, 1800, "moving", court_override=shifted_x)
    shifted_d = list(cells)
    shifted_d[0] = (cells[0][0] + 1, cells[0][1] + 1)  # (31,31) -> (33,33): one cell outward on both axes
    moved_d = evaluate(sk, sk, 4400, 1800, "moving", court_override=shifted_d)
    return dict(mean=base, moved_x=moved_x, moved_diag=moved_d, cells=cells)


# ---------------------------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------------------------
def pct(n, t):
    return "%.3f" % (n / t)


def build_report(rows, csv_text, fx, date):
    counts = reach_counts(rows)
    word, lever = verdict_from_rows(rows)
    lines = []
    a = lines.append
    base_fixed = counts[("baseline", "fixed")]
    base_moving = counts[("baseline", "moving")]
    low = lower_policy_count(counts, "baseline")
    a("# OQ9 / C-W24 grid-only final-court enumeration, %s" % date)
    a("")
    a("> **Story:** `production/epics/procedural-city-generator/story-008-oq9-final-court-enumeration-script.md` | "
      "**Script:** `tools/citygen/oq9_enumerate.py` (deterministic, integer-only, no geometry, no engine) | "
      "**Data:** `%s_citygen-oq9-final-court-enumeration.csv` (sha256 `%s`)" % (date, hashlib.sha256(csv_text.encode()).hexdigest()[:16]))
    a("")
    a("**Verdict: %s**" % word)
    a("")
    a("The word is the AC3 comparison, recomputed from the CSV by the script and by its self-test: take the lower of the two "
      "anchor policies' baseline fractions (the conservative reading, so the choice of policy cannot flatter the result); "
      "PASS if it is >= 0.30, CONCERNS if it is < 0.30 but >= 0.30 after a lever (a) start-cell radius 16 m, jitter 0 or 2, "
      "or (b) a lower `StartToFinalMinPathCm` (not below the 40 m LSC hard minimum), FAIL if < 0.30 after every lever.")
    a("")
    a("- Baseline (F 44 m, jitter 1, start-cell radius 18 m, 8 %% path guard), 200 skeletons: **fixed** %d/200 = %s, "
      "**moving** %d/200 = %s reach `FinalCandidatesMinCount` 6; lower = %s (%d/200)." %
      (base_fixed[0], pct(*base_fixed), base_moving[0], pct(*base_moving), pct(*low), low[0]))
    if word == "CONCERNS":
        a("- Lever that reached 0.30 first (in the order tried): **%s**." % lever)
    elif word == "FAIL":
        a("- No lever reached 0.30. The best fractions per lever are in the lever table below.")
    a("")
    a("## 1. Method")
    a("")
    a("The C++ stages (stories 003, 005, 006) do not exist yet, so this script carries a minimal copy of S1 (frame, N 62-66), "
      "S2 (5 x 5 regular lattice, blocks 7-10 cells, interior corridors 3 cells 65 % / 2 cells 35 %, lanes 3 cells, transpose "
      "symmetry, per-half per-axis per-cut integer jitter), the S4 axis-corner court pool and the S5 zone anchors and start cells, "
      "with PCG32 streams (a stage's draws never depend on the jitter amplitude, so levers compare like with like). Grid path = "
      "4-connected BFS over corridor cells x 2 m. See the script docstring for the full model. The 200 seeds are a committed literal list.")
    a("")
    a("- **fixed** policy: the six S4 court junctions sit at the cells of the same skeleton with zero jitter (snapped to the nearest "
      "walkable cell), whatever the jitter did; zone anchors and start cells always sit on the actual jittered streets.")
    a("- **moving** policy: the court junctions follow the jittered cuts.")
    a("- Final-eligible court = path from the nearest start cell (either zone) >= 1.08 F, >= 3 ingress arms, >= 24 m from every other "
      "counted court of its corner. The count is over the six S4 pool courts (max 6); the `adaptive` column is a diagnostic only "
      "(the exact maximum set of >= 24 m-spaced eligible lattice junctions, whatever S4 would place).")
    a("")
    # Fixtures
    mean, mx, md = fx["mean"], fx["moved_x"], fx["moved_diag"]
    a("## 2. Arithmetic fixtures (AC2)")
    a("")
    a("| Fixture | final-eligible courts | margin at (31, 31) |")
    a("|---|---|---|")
    a("| Mean geometry (N 64, all blocks 8, all corridors 3, no jitter) | %d | %.2f m |" % (mean["final_count"], mean["axis_margin_cm"] / 100))
    a("| (31, 31) moved 2 m outward in x, to (33, 31) (negative control) | %d | %.2f m |" % (mx["final_count"], mx["axis_margin_cm"] / 100))
    a("| (31, 31) moved 2 m outward on both axes, to (33, 33) (observation) | %d | %.2f m |" % (md["final_count"], md["axis_margin_cm"] / 100))
    a("")
    a("The mean fixture reproduces the GDD S4 arithmetic exactly (6 courts, 0.48 m). The negative control reproduces 5. "
      "Observation for the GDD author: the formula `x + y <= 110 - 1.08 F` assumes the far anchor's row/column; a court that "
      "moves outward along the sun axis stays the same Manhattan distance from the two symmetric anchors, so the diagonal "
      "move does **not** lose the court on the grid (the row above). Only an off-axis outward move (x or y alone) does. "
      "The formula is therefore a bound on the axis-aligned moves, not on the diagonal ones.")
    a("")
    # Baseline results
    a("## 3. Baseline result (AC1)")
    a("")
    a("| Policy | mean final-eligible count | courts histogram 0..6 (sums to 200) | fraction >= 6 |")
    a("|---|---|---|---|")
    for pol in POLICIES:
        sub = [int(r["final_count"]) for r in rows if r["scenario"] == "baseline" and r["policy"] == pol]
        hist = histogram(rows, "baseline", pol, "final_count", 0, 6)
        assert sum(hist) == 200
        a("| %s | %.2f | %s | %s (%d/200) |" % (pol, sum(sub) / len(sub), hist, pct(*counts[("baseline", pol)]), counts[("baseline", pol)][0]))
    a("")
    a("Per-skeleton counts are in the CSV (`final_count`, 200 rows per policy and scenario).")
    a("")
    a("### Margin distribution of the (31, 31) junction (near axis court), baseline")
    a("")
    a("margin = grid path from the nearest start cell - 1.08 F, metres; negative = the court fails the 8 % guarded floor.")
    a("")
    a("| Policy | min | p10 | p50 | p90 | max | share >= 0 |")
    a("|---|---|---|---|---|---|---|")
    for pol in POLICIES:
        vals = sorted(int(r["axis_margin_cm"]) for r in rows if r["scenario"] == "baseline" and r["policy"] == pol)
        ge0 = sum(1 for v in vals if v >= 0)
        a("| %s | %.2f | %.2f | %.2f | %.2f | %.2f | %d/200 |" % (pol, vals[0] / 100, percentile(vals, 10) / 100,
                                                                percentile(vals, 50) / 100, percentile(vals, 90) / 100,
                                                                vals[-1] / 100, ge0))
    a("")
    a("1 m margin histogram (bucket lower edge in m, count), baseline:")
    a("")
    for pol in POLICIES:
        h = collections.Counter(int(r["axis_margin_cm"]) // 100 for r in rows
                                if r["scenario"] == "baseline" and r["policy"] == pol)
        a("- %s: %s" % (pol, ", ".join("%d: %d" % (k, h[k]) for k in sorted(h))))
    a("")
    # Formula check
    fc = [r for r in rows if r["scenario"] == "baseline" and r["policy"] == "moving"]
    tot_agree = sum(int(r["formula_court_agree"]) for r in fc)
    a("### Formula versus measurement")
    a("")
    a("The GDD formula (Manhattan distance to the mean-geometry anchors minus the start-cell radius, against 1.08 F) agrees with the "
      "grid measurement on %d of %d court checks in the baseline (moving policy; the formula ignores walls, jitter of the anchors and "
      "ingress/spacing). Mean formula-predicted eligible courts per skeleton: %.2f; mean measured: %.2f." %
      (tot_agree, 6 * len(fc), sum(int(r["formula_count"]) for r in fc) / len(fc),
       sum(int(r["final_count"]) for r in fc) / len(fc)))
    a("")
    # Lever table
    a("## 4. Levers (fraction of 200 skeletons reaching 6; lower of the two policies marked)")
    a("")
    a("| Scenario | tier | F m | R m | jitter | fixed | moving | lower | >= 0.30 |")
    a("|---|---|---|---|---|---|---|---|---|")
    for (name, tier, f_m, r_m, jit) in SCENARIOS:
        f, m = counts[(name, "fixed")], counts[(name, "moving")]
        lo = lower_policy_count(counts, name)
        a("| %s | %d | %d | %d | %d | %d/200 (%s) | %d/200 (%s) | %s | %s |" %
          (name, tier, f_m, r_m, jit, f[0], pct(*f), m[0], pct(*m), pct(*lo), "yes" if meets_floor(*lo) else "no"))
    a("")
    ad = {}
    for (name, tier, f_m, r_m, jit) in SCENARIOS:
        sub = [int(r["adaptive_count"]) for r in rows if r["scenario"] == name and r["policy"] == "moving"]
        ad[name] = sum(1 for v in sub if v >= FINAL_CANDIDATES_MIN_COUNT)
    a("Diagnostic (not the S4 policy, not part of the verdict): skeletons where the exact maximum set of >= 24 m-spaced eligible "
      "lattice junctions reaches 6 (an upper bound on any S4 placement rule): " +
      ", ".join("%s %d/200" % (n, ad[n]) for (n, *_r) in SCENARIOS) + ".")
    a("")
    # Recommendation
    if word == "PASS":
        rec_name = "baseline"
    elif word == "CONCERNS":
        rec_name = lever
        # a lever that clears 0.30 by under 10 points is inside sampling error (about 3 points) and the p_v
        # factor of F6: propose the strongest tier-1 combination instead
        if lower_policy_count(counts, lever)[0] - 60 < 20:
            best1 = max((s for s in SCENARIOS if s[1] == 1), key=lambda s: lower_policy_count(counts, s[0])[0])
            if meets_floor(*lower_policy_count(counts, best1[0])):
                rec_name = best1[0]
    else:
        best = max((n for (n, *_r) in SCENARIOS), key=lambda n: (lower_policy_count(counts, n)[0], -[s[0] for s in SCENARIOS].index(n)))
        rec_name = best
    rec = [s for s in SCENARIOS if s[0] == rec_name][0]
    fx_n, mv_n = counts[(rec_name, "fixed")][0], counts[(rec_name, "moving")][0]
    rule = "moving" if mv_n >= fx_n else "fixed"
    a("## 5. Proposed rules")
    a("")
    a("- **Anchor-jitter rule (proposed): %s.** %s" % (
        rule.upper(),
        "S4 court junctions follow the jittered cut positions (they are junctions of the actual skeleton; there is no separate "
        "fixed anchor table to maintain)." if rule == "moving" else
        "S4 court junctions stay at their mean-lattice cells and are snapped to the nearest walkable cell of the jittered skeleton."))
    a("- **Start-cell radius (proposed): %d m**, with `StartToFinalMinPathCm` %d m and `CityGenSkeletonJitterCount` %d (scenario `%s`)." % (rec[3], rec[2], rec[4], rec_name))
    if rec[4] == 0:
        a("- At jitter 0 the two policies coincide (identical rows), so the anchor-jitter rule only matters while jitter >= 1, where "
          "%s is the higher of the two at the baseline (fixed %d/200, moving %d/200, both far below 0.30)." %
          ("moving" if counts[("baseline", "moving")][0] >= counts[("baseline", "fixed")][0] else "fixed",
           counts[("baseline", "fixed")][0], counts[("baseline", "moving")][0]))
    if word == "CONCERNS":
        best_t1 = max((s for s in SCENARIOS if s[1] == 1), key=lambda s: lower_policy_count(counts, s[0])[0])
        bn = lower_policy_count(counts, best_t1[0])
        a("- Robustness note: the first lever to clear 0.30 (`%s`) does so by %d skeletons of 200 (sampling error about 3 points), so "
          "when the margin is under 10 points the proposal above is the strongest tier-1 combination (`%s`, %s) rather than the "
          "bare lever." % (lever, lower_policy_count(counts, lever)[0] - 60, rec_name, pct(*lower_policy_count(counts, rec_name))))
    a("- `FinalCandidatesMinCount` stays 6 and `CityGenFinalEligibleTargetCount` stays 6; this report proposes no change to either.")
    if word == "PASS":
        a("- No lever is needed at the defaults (F 44 m, jitter 1, radius 18 m). Stories 010, 011, 022 and 038 may start.")
    elif word == "CONCERNS":
        a("- The defaults alone do not reach 0.30; the cited lever does. Stories 010, 011, 022 and 038 take the proposed rule and radius "
          "above as their starting values and carry the lever as a documented G-PRE tuning change (data file value, not code).")
    else:
        a("- No lever reaches 0.30 with the S4 stratified pool as written. The design change needed is recorded in section 6; "
          "stories 010, 011, 022 and 038 stay Blocked until the level-designer decides it.")
    a("")
    a("## 6. Findings and follow-ups")
    a("")
    a("- Modelling assumptions that move the numbers (change them in the real stages and re-run): block size weights (mean = 8 only at "
      "N = 64 with all-3 corridors; N and the alley share shift the cut positions by whole cells), independent x/y jitter per half, "
      "the axis court as the half-A junction snapped to the walkable cell, anchors at the junction cell (the GDD puts them 1-2 cells "
      "along the arm), no jogs, no T-stagger, no canals. Jogs, stagger and canals only lengthen paths on the lattice, so ignoring them "
      "does not flatter the failure rate.")
    a("- Two independent effects cut the fraction, both measured above: the block and N draw alone (jitter 0 row, blocks still drawn) "
      "gives %s of skeletons against 6/6 at the mean fixture, because the (31, 31) cut sits at 15 + 2 b1 m with b1 the first block "
      "(7-10 cells); and jitter 1 on top takes the baseline to %s. A design lever outside this story's list, not measured here: "
      "pin the first and last block of each axis to 8 cells so the S4 anchor cuts stay near the mean positions." %
      (pct(*lower_policy_count(counts, "j0")), pct(*low)))
    a("- No placement rule can rescue the baseline by choosing other junctions: the adaptive diagnostic (any eligible lattice junctions, "
      "exact maximum spaced set) reaches 6 on at most %d of 200 skeletons in any scenario." % max(ad.values()))
    a("- Follow-up (level-designer, design-doc change, not made here): update GDD S4 / Open Question 9 and LSC Open Question 15 text with this report's "
      "numbers and the proposed anchor-jitter rule and radius; C-W24 closes when this report is committed.")
    a("- Follow-up (stories 010 and 011): adopt `evaluate()`'s court cells, anchor and start-cell probe as the starting point; replace the minimal "
      "S1/S2 copy here with the real stages (stories 005 and 006) and re-run before the golden seeds (story 031).")
    a("- Re-run: `python tools/citygen/oq9_enumerate.py run` (output is byte-identical); `python tools/citygen/oq9_enumerate.py selftest`.")
    a("")
    return "\n".join(lines) + "\n", word


def report_verdict_word(text):
    m = re.search(r"\*\*Verdict: (PASS|CONCERNS|FAIL)\*\*", text)
    return m.group(1) if m else None


DATE = "2026-10-25"


def out_paths(out_dir):
    base = os.path.join(out_dir, "%s_citygen-oq9-final-court-enumeration" % DATE)
    return base + ".md", base + ".csv"


def run(out_dir):
    rows = run_enumeration()
    csv_text = rows_to_csv(rows)
    fx = fixture_results()
    report, word = build_report(rows, csv_text, fx, DATE)
    md, cs = out_paths(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    with open(cs, "w", newline="") as f:
        f.write(csv_text)
    with open(md, "w", newline="") as f:
        f.write(report)
    return word, md, cs


# ---------------------------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------------------------
def selftest():
    fails = []

    def check(cond, msg):
        print(("ok   " if cond else "FAIL ") + msg)
        if not cond:
            fails.append(msg)

    # PCG32 known-answer (pcg32-demo, seed 42, seq 54)
    g = Pcg32(42, 54)
    kat = [g.next() for _ in range(6)]
    check(kat == [0xA15C02B7, 0x7B47F409, 0xBA1D3330, 0x83D2F293, 0xBFA4784B, 0xCBED606E], "pcg32 known answers")
    check(len(set(SEEDS)) == 200, "200 distinct committed seeds")
    # bounded() stays in range and hits both ends
    r = Pcg32(1, 2)
    vals = {r.bounded(5) for _ in range(500)}
    check(vals == {0, 1, 2, 3, 4}, "bounded(5) uniform support")
    # skeleton invariants
    for seed in SEEDS[:40]:
        s = draw_skeleton(seed, 1)
        check_ok = (62 <= s.n <= 66 and all(7 <= b <= 10 for b in s.blocks) and sum(s.blocks) + sum(s.widths) == s.n - 6)
        if not check_ok:
            check(False, "skeleton invariants seed %08X" % seed)
            break
    else:
        check(True, "skeleton invariants (N 62..66, blocks 7..10, sums) on 40 seeds")
    s0, s1 = draw_skeleton(SEEDS[3], 0), draw_skeleton(SEEDS[3], 1)
    check((s0.n, s0.widths, s0.blocks) == (s1.n, s1.widths, s1.blocks), "jitter amplitude does not change N / widths / blocks")
    # AC2 fixtures
    fx = fixture_results()
    check(fx["mean"]["final_count"] == 6, "AC2 mean geometry counts 6 final-eligible courts")
    check(fx["mean"]["axis_margin_cm"] == 48, "AC2 margin at (31,31) is 0.48 m (got %d cm)" % fx["mean"]["axis_margin_cm"])
    check(fx["moved_x"]["final_count"] == 5, "AC2 (31,31) moved 2 m outward counts 5 (got %d)" % fx["moved_x"]["final_count"])
    cells = fx["cells"]
    ms = mean_skeleton()
    check(cells[0] == (15, 15) and cells[1] == (26, 4) and cells[2] == (4, 26), "mean-geometry court cells are (31,31) (53,9) (9,53) m")
    check(zone_anchors(ms) == ((15, 48), (48, 15)), "mean-geometry anchors are (31,97) and (97,31) m")
    # integer test boundary: 1.08 * 44 m = 47.52 m
    check(path_ok(4752, 4400) and not path_ok(4751, 4400), "path guard test is exactly 47.52 m at F 44 m")
    # verdict rule, incl. exactly 0.30 -> PASS
    check(meets_floor(60, 200) and not meets_floor(59, 200), "fraction exactly 0.30 passes, 0.295 does not")

    def synth(base_f, base_m, lever_f=0, lever_m=0, lever_name="a_R16"):
        rr = []
        for name, *_ in SCENARIOS:
            for pol in POLICIES:
                if name == "baseline":
                    k = base_f if pol == "fixed" else base_m
                elif name == lever_name:
                    k = lever_f if pol == "fixed" else lever_m
                else:
                    k = 0
                for i in range(200):
                    rr.append(dict(scenario=name, policy=pol, reaches_min=1 if i < k else 0))
        return rr

    check(verdict_from_rows(synth(60, 200)) == ("PASS", "baseline"), "verdict PASS at lower policy exactly 0.30")
    check(verdict_from_rows(synth(59, 200, 60, 60))[0] == "CONCERNS", "verdict CONCERNS when a lever reaches 0.30")
    check(verdict_from_rows(synth(200, 59, 100, 59))[0] == "FAIL", "verdict FAIL when the lower policy never reaches 0.30")
    check(verdict_from_rows(synth(59, 59, 200, 59))[0] == "FAIL", "lever must lift the LOWER policy (conservative reading)")
    # determinism + 200 rows per policy, histogram sums, report/CSV agreement
    with tempfile.TemporaryDirectory() as t1, tempfile.TemporaryDirectory() as t2:
        w1, md1, cs1 = run(t1)
        w2, md2, cs2 = run(t2)
        b1, b2 = open(cs1, "rb").read(), open(cs2, "rb").read()
        check(b1 == b2 and open(md1, "rb").read() == open(md2, "rb").read(), "two runs are byte-identical (CSV and report)")
        rows = csv_to_rows(b1.decode())
        for sc in SCENARIOS:
            for pol in POLICIES:
                n_rows = sum(1 for r in rows if r["scenario"] == sc[0] and r["policy"] == pol)
                if n_rows != 200:
                    check(False, "200 rows for %s/%s (got %d)" % (sc[0], pol, n_rows))
        check(True, "200 rows for every scenario and policy")
        for pol in POLICIES:
            check(sum(histogram(rows, "baseline", pol, "final_count", 0, 6)) == 200, "histogram sums to 200 (%s)" % pol)
        check(max(int(r["final_count"]) for r in rows) <= 6, "S4 pool count never exceeds 6")
        word, _lv = verdict_from_rows(rows)
        check(report_verdict_word(open(md1).read()) == word, "report verdict word equals the word recomputed from the CSV (%s)" % word)
        text = open(md1).read()
        check("Anchor-jitter rule (proposed)" in text and "Start-cell radius (proposed)" in text, "report states anchor-jitter rule and start-cell radius")
        bad = re.findall(r"(?i)(lower|reduce|drop|decrease)[^.\n]{0,40}FinalCandidatesMinCount", text)
        check(not bad, "report proposes no lower FinalCandidatesMinCount")
        if word == "CONCERNS":
            check(("lever that reached 0.30" in text.lower()), "CONCERNS report cites the lever")
    # committed copies, when present, must match the regenerated ones
    here = os.path.dirname(os.path.abspath(__file__))
    spikes = os.path.normpath(os.path.join(here, "..", "..", "production", "qa", "spikes"))
    md, cs = out_paths(spikes)
    if os.path.exists(md) and os.path.exists(cs):
        check(open(cs, "rb").read() == b1, "committed CSV equals a fresh run")
        check(report_verdict_word(open(md).read()) == word, "committed report verdict equals the CSV-derived word")
    print("SELFTEST %s (%d failure(s))" % ("FAILED" if fails else "PASSED", len(fails)))
    return 1 if fails else 0


def main(argv):
    if len(argv) >= 2 and argv[1] == "selftest":
        return selftest()
    if len(argv) >= 2 and argv[1] == "run":
        here = os.path.dirname(os.path.abspath(__file__))
        out = os.path.normpath(os.path.join(here, "..", "..", "production", "qa", "spikes"))
        if "--out-dir" in argv:
            out = argv[argv.index("--out-dir") + 1]
        word, md, cs = run(out)
        print("verdict: %s\nreport: %s\ncsv: %s" % (word, md, cs))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
