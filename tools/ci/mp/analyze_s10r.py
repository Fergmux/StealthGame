"""IVP S10 re-run (story-056) analyser: Saved/Harness/<run>/ records of scenario "S10R" (with -StealthPacketTiming).

Observations only (the spike report judges). For every server un-cull of a subject and every client it reports:
  - G: un-cull (server QPC) -> the first visible frame of the pawn on that client (the client's first-visible probe,
    taken after every tick group of the frame, independent of the warm-up signal), split by free / held re-open, by
    first-send delivery of the open bunch (the client's receipt packet id against the server's send packet ids) and by
    the number of resends per hop (open, close);
  - the hold (un-cull -> release by in-sequence acknowledgement or timeout) and its decomposition: close sent ->
    in-sequence acknowledgement, acknowledgement -> open sent, KeepProcessingActorChannelBunchesMap time on the client;
  - T_reopen: open-bunch receipt (packet rx time) -> first visible frame; warm-up frames; WarmupTimeout;
  - lost pawns: the client census at each commit (open server channel but no live client instance), network destroys
    of an instance bound to the server's open channel, L_gap (that destroy -> the repaired first visible frame);
    failure reports, repairs, deferrals, UnexpectedChannelFailure, "failed to find/create ACTOR" lines;
  - the first-visible pose probe: head-bone height (alive) or head-to-root distance (dead) against the standing / dead
    reference of the same client (poseRef records, 10th visible frame), as a ratio;
  - the clock: pose/death evaluation error (estimator time used - true server time at that instant, true time from the
    server's frame-start pairs) and the estimator's own error samples;
  - NetGUID reuse; parity: the IVP F2 classifier (logistic regression, 2-fold cross-fit over 30 s blocks, cluster
    bootstrap SE over blocks, 2,000 resamples) over hold, timeout, repair, G and warm-up frames, per-feature KS.
Usage: python -I tools/ci/mp/analyze_s10r.py Saved/Harness/<run> [--json out.json] [--frames <dir for strips>]
"""
import bisect
import json
import math
import os
import random
import re
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402

CASES = {0: "A", 1: "B", 2: "C"}


def truth_clock(server):
    pts = sorted((r["fs"], r["st"]) for r in hr.of_type(server, "srvClk"))
    fs = [p[0] for p in pts]

    def at(t):
        i = bisect.bisect_right(fs, t) - 1
        if i < 0:
            return None
        return pts[i][1] + (t - pts[i][0])
    return at


def q(values):
    return hr.quantiles(values)


def pct(values, p):
    if not values:
        return None
    o = sorted(values)
    return o[min(len(o) - 1, int(math.ceil(p * len(o))) - 1)]


def auc(scores, labels):
    pos = [s for s, l in zip(scores, labels) if l == 1]
    neg = [s for s, l in zip(scores, labels) if l == 0]
    if not pos or not neg:
        return None
    allv = sorted([(s, 1) for s in pos] + [(s, 0) for s in neg])
    # Mann-Whitney with tie-averaged ranks.
    ranks = [0.0] * len(allv)
    i = 0
    while i < len(allv):
        j = i
        while j + 1 < len(allv) and allv[j + 1][0] == allv[i][0]:
            j += 1
        for k in range(i, j + 1):
            ranks[k] = (i + j) / 2.0 + 1.0
        i = j + 1
    rpos = sum(r for r, (_, l) in zip(ranks, allv) if l == 1)
    return (rpos - len(pos) * (len(pos) + 1) / 2.0) / (len(pos) * len(neg))


def logistic_fit(x, y, iters=400, lr=0.5, l2=1e-3):
    d = len(x[0])
    mu = [sum(r[j] for r in x) / len(x) for j in range(d)]
    sd = [math.sqrt(sum((r[j] - mu[j]) ** 2 for r in x) / len(x)) or 1.0 for j in range(d)]
    z = [[(r[j] - mu[j]) / sd[j] for j in range(d)] for r in x]
    w = [0.0] * (d + 1)
    n = len(z)
    for _ in range(iters):
        g = [0.0] * (d + 1)
        for r, t in zip(z, y):
            s = w[0] + sum(w[j + 1] * r[j] for j in range(d))
            p = 1.0 / (1.0 + math.exp(-max(-30.0, min(30.0, s))))
            e = p - t
            g[0] += e
            for j in range(d):
                g[j + 1] += e * r[j]
        for j in range(d + 1):
            w[j] -= lr * (g[j] / n + (l2 * w[j] if j else 0.0))

    def predict(r):
        return w[0] + sum(w[j + 1] * (r[j] - mu[j]) / sd[j] for j in range(d))
    return predict


def ks(a, b):
    if not a or not b:
        return None
    a, b = sorted(a), sorted(b)
    vals = sorted(set(a + b))
    dmax = 0.0
    for v in vals:
        fa = bisect.bisect_right(a, v) / len(a)
        fb = bisect.bisect_right(b, v) / len(b)
        dmax = max(dmax, abs(fa - fb))
    n = len(a) * len(b) / (len(a) + len(b))
    lam = (math.sqrt(n) + 0.12 + 0.11 / math.sqrt(n)) * dmax
    p = 2.0 * sum((-1) ** (k - 1) * math.exp(-2.0 * k * k * lam * lam) for k in range(1, 101))
    return {"D": dmax, "p": max(0.0, min(1.0, p)), "nPlayer": len(a), "nAi": len(b)}


def classifier(units, feature_names, seed=7):
    """IVP F2: player vs AI from per-re-open features; 2-fold cross-fit by 30 s blocks; cluster bootstrap SE."""
    if len({u["label"] for u in units}) < 2:
        return None
    blocks = sorted({u["block"] for u in units})
    folds = {b: i % 2 for i, b in enumerate(blocks)}
    scores = {}
    for fold in (0, 1):
        train = [u for u in units if folds[u["block"]] != fold]
        test = [u for u in units if folds[u["block"]] == fold]
        if not train or not test or len({u["label"] for u in train}) < 2:
            continue
        predict = logistic_fit([u["x"] for u in train], [u["label"] for u in train])
        for u in test:
            scores[id(u)] = predict(u["x"])
    scored = [u for u in units if id(u) in scores]
    a = auc([scores[id(u)] for u in scored], [u["label"] for u in scored])
    by_block = defaultdict(list)
    for u in scored:
        by_block[u["block"]].append(u)
    rng = random.Random(seed)
    keys = list(by_block)
    boots = []
    for _ in range(2000):
        pick = [rng.choice(keys) for _ in keys]
        sample = [u for k in pick for u in by_block[k]]
        v = auc([scores[id(u)] for u in sample], [u["label"] for u in sample])
        if v is not None:
            boots.append(v)
    m = sum(boots) / len(boots) if boots else 0.0
    se = math.sqrt(sum((v - m) ** 2 for v in boots) / max(1, len(boots) - 1)) if boots else None
    n1 = len([u for u in scored if u["label"] == 1])
    n2 = len(scored) - n1
    return {"features": feature_names, "AUC_hat": a, "SE_boot": se, "upper": a + 1.645 * se if a is not None and se is not None else None,
            "SE_hanleyMcNeil": math.sqrt((n1 + n2 + 1) / (12.0 * n1 * n2)) if n1 and n2 else None, "nPlayer": n1, "nAi": n2, "blocks": len(keys)}


def silhouette(gone, img, foot, head):
    """Vertical extent (px) of the pixels that differ from the 'gone' frame in the subject's column. Threshold 10
    (8-bit, any channel): TSR history fades a newly visible primitive in over ~3 frames, so a high threshold measures
    opacity, not the posed height (found on run s10r-l1; the warm-up-off control proves the probe still sees a
    collapsed body at this threshold)."""
    w, h = img.size
    fx, fy = foot
    hx, hy = head
    if fx < 0 or fy < 0:
        return None
    span = max(10.0, fy - hy)
    half = max(6, int(span * 0.35))
    x0, x1 = max(0, int(fx - half)), min(w - 1, int(fx + half))
    y_top = max(0, int(fy - span * 1.4))
    y_bot = min(h - 1, int(fy + span * 0.1))
    ga = gone.load()
    ia = img.load()
    rows = []
    for y in range(y_top, y_bot + 1):
        n = 0
        for x in range(x0, x1 + 1):
            a, b = ga[x, y], ia[x, y]
            if max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])) > 10:
                n += 1
        rows.append(n >= 2)
    # The tallest run of changed rows (gaps of up to 4 rows bridged): the body, not a stray shadow or noise blob.
    best = 0
    start = None
    last = None
    for i, on in enumerate(rows):
        if not on:
            continue
        if start is None or i - last > 5:
            start = i
        last = i
        best = max(best, last - start + 1)
    return float(best)


def frame_probe(run_dir, clients, out_dir=None):
    try:
        from PIL import Image
    except ImportError:
        return {"error": "PIL not available"}
    out = []
    for name, recs in clients.items():
        caps = hr.of_type(recs, "frameCapture")
        if not caps:
            continue
        gones = [c for c in caps if c["stage"] == "gone"]
        seqs = defaultdict(dict)
        order = []
        for c in caps:
            if c["stage"] == "gone":
                continue
            key = (c["name"], c["guid"])
            if key not in seqs:
                order.append(key)
            seqs[key][c["stage"]] = c
        for key in order:
            seq = seqs[key]
            if "f00" not in seq or "f10" not in seq:
                continue
            g = [x for x in gones if x["guid"] == key[1] and x["t"] < seq["f00"]["t"]]
            if not g:
                continue
            g = g[-1]

            def path(rec):
                p = rec["file"]
                if not os.path.exists(p):
                    p = os.path.join(run_dir, "frames", os.path.basename(p))
                return p
            try:
                gi = Image.open(path(g)).convert("RGB")
                imgs = {st: Image.open(path(seq[st])).convert("RGB") for st in ("f00", "f01", "f05", "f10") if st in seq}
            except (OSError, FileNotFoundError):
                continue
            hts = {st: silhouette(gi, im, seq[st]["foot"], seq[st]["head"]) for st, im in imgs.items()}
            ref = hts.get("f10") or 0.0
            # The f10 silhouette must be a standing body of the projected height (else occluded or off screen: not
            # assessable by this probe; the head-bone probe still covers that re-open).
            projected = seq["f00"]["foot"][1] - seq["f00"]["head"][1]
            row = {"client": name, "instance": key[0], "guid": key[1], "silhouette_px": hts, "ref_px": ref, "projected_px": projected,
                   "assessable": bool(ref and projected > 0 and 0.6 * projected <= ref <= 1.3 * projected),
                   "f00_ratio": (hts["f00"] / ref) if ref else None, "goneAgeMs": (seq["f00"]["t"] - g["t"]) * 1000.0}
            if out_dir and seq["f00"]["foot"][0] >= 0 and seq["f00"]["foot"][0] < gi.size[0]:
                os.makedirs(out_dir, exist_ok=True)
                fx, fy = seq["f00"]["foot"]
                hx, hy = seq["f00"]["head"]
                span = max(20, int(fy - hy))
                box = (int(max(0, fx - span)), int(max(0, hy - span * 0.4)), int(min(gi.size[0], fx + span)), int(min(gi.size[1], fy + span * 0.2)))
                crops = [gi.crop(box)] + [imgs[st].crop(box) for st in ("f00", "f01", "f05", "f10") if st in imgs]
                strip = Image.new("RGB", (sum(c.size[0] for c in crops) + 4 * len(crops), crops[0].size[1]), (255, 255, 255))
                x = 0
                for c in crops:
                    strip.paste(c, (x, 0))
                    x += c.size[0] + 4
                fn = os.path.join(out_dir, f"strip_{key[1]}_{key[0]}.png")
                strip.save(fn)
                row["strip"] = fn
            out.append(row)
    return out


def main(run_dir, out_json=None, frames_out=None):
    run = hr.load_run(run_dir)
    server = run["server"]
    clients = {k: v for k, v in run.items() if k.startswith("client")}
    true_st = truth_clock(server)
    cfg = (hr.of_type(server, "s10rConfig") or [{}])[0]
    timeout_ms = cfg.get("reopenAckTimeoutMs", 350)
    unculls = hr.of_type(server, "uncull")
    closes = hr.of_type(server, "cullClose")
    commits = hr.of_type(server, "cullCommit")
    # A subject picked before its first replication is keyed "0" until the rig refreshes it: alias it to the
    # NetGUID the server had at its un-culls (and at its subject record, when known).
    alias = {}
    for u in unculls:
        if u["subject"] == "0" and u["guid"] != "0":
            alias.setdefault("0", u["guid"])
    for rec in server:
        if rec.get("subject") in alias:
            rec["subject"] = alias[rec["subject"]]
    opens = hr.of_type(server, "open")
    acks = hr.of_type(server, "closeAck")
    timeouts = hr.of_type(server, "reopenTimeout")
    releases = hr.of_type(server, "holdRelease")
    repairs = hr.of_type(server, "repair")
    resends = defaultdict(list)
    for r in hr.of_type(server, "openResend"):
        resends[(r["subject"], r["c"], r["ch"])].append(r["pkt"])
    kind_of = {}
    for s in hr.of_type(server, "subject"):
        kind_of[s["guid"]] = s["kind"]
    closes_by_subject = defaultdict(list)
    for c in closes:
        closes_by_subject[c["subject"]].append(c["t"])
    frame_s = 1.0 / 60.0
    t0 = unculls[0]["t"] if unculls else 0.0
    result = {"run": os.path.basename(run_dir.rstrip("/\\")), "config": cfg, "cycles": len(unculls),
              "cyclesByKind": {k: len([u for u in unculls if u["kind"] == k]) for k in ("ai", "player")},
              "cyclesByCase": {c: len([u for u in unculls if CASES[u["case"]] == c]) for c in "ABC"},
              "closeLateBy_ms": q([c["lateBy"] * 1000 for c in closes]), "clients": {}}
    units = []
    all_rows = []
    lgaps = []
    lost_census = 0
    lost_destroys = []
    probe_rows = []
    clock_err = []
    est_err = []
    t_reopen = []
    ffca = {}
    for cname, recs in clients.items():
        conn = "C" + cname[len("client"):]
        spawns = defaultdict(list)  # guid -> [(t, name)]
        guid_of = {r["name"]: r["guid"] for r in hr.of_type(recs, "pawnGuid")}
        for r in hr.of_type(recs, "pawnSpawn"):
            g = guid_of.get(r["name"])
            if g:
                spawns[g].append((r["t"], r["name"]))
        destroy_t = {r["name"]: r["t"] for r in hr.of_type(recs, "pawnDestroy")}
        first_vis = {r["name"]: r for r in hr.of_type(recs, "firstVisible")}
        chan = defaultdict(list)
        for r in hr.of_type(recs, "chanOpen"):
            chan[r["name"]].append(r)
        rx = {}
        for rec in hr.of_type(recs, "pkt"):
            for i, t in rec["rx"]:
                rx.setdefault(int(i), float(t))
        applied = {r["name"]: r for r in hr.of_type(recs, "warm") if r.get("phase") == "applied"}
        warm_timeout = [r for r in hr.of_type(recs, "warm") if r.get("phase") == "warmupTimeout"]
        keep = defaultdict(list)
        for r in hr.of_type(recs, "keepProcessing"):
            keep[r["guid"]].append((r["t"], r["on"]))
        refs_alive = [r["headH"] for r in hr.of_type(recs, "poseRef") if not r["dying"] and r["headH"] > 0]
        refs_dead = [r["headRoot"] for r in hr.of_type(recs, "poseRef") if r["dying"] and r["headRoot"] > 0]
        ref_alive = pct(refs_alive, 0.5)
        ref_dead = pct(refs_dead, 0.5)
        cs = {"census": 0, "censusMissing": 0, "reopens": 0}
        # Census: every commit's open channels must have a live client instance at that instant.
        for cm in commits:
            for c in cm.get("census", []):
                if c["c"] != conn or not c["open"]:
                    continue
                cs["census"] += 1
                live = [n for (t, n) in spawns.get(cm["subject"], []) if t <= cm["t"] and destroy_t.get(n, 1e18) > cm["t"]]
                if not live:
                    cs["censusMissing"] += 1
                    lost_census += 1
        # Network destroys of an instance bound to the server's open channel (lost pawns) and L_gap.
        server_opens = [o for o in opens if o["c"] == conn]
        for name, td in destroy_t.items():
            g = guid_of.get(name)
            if g not in kind_of or not chan.get(name):
                continue
            last = chan[name][-1]
            so = [o for o in server_opens if o["subject"] == g and o["t"] < td]
            if not so or so[-1]["ch"] != last["ch"] or len(chan[name]) < 2:
                continue
            # The instance was bound (re-bound) to the server's current open channel when it was destroyed.
            nxt = [(t, n) for (t, n) in spawns.get(g, []) if t > td]
            fv = first_vis.get(nxt[0][1]) if nxt else None
            gap = (fv["t"] - td) * 1000.0 if fv else None
            rep = [r for r in repairs if r["c"] == conn and r["subject"] == g and td <= r["t"] <= td + 3.0]
            lost_destroys.append({"client": conn, "guid": g, "kind": kind_of.get(g), "t": td, "L_gap_ms": gap, "repaired": bool(rep)})
            if gap is not None:
                lgaps.append(gap)
        rows = []
        for u in unculls:
            g = u["guid"] if u["guid"] != "0" else u["subject"]
            nxt_close = min([t for t in closes_by_subject[u["subject"]] if t > u["t"]] or [u["t"] + 6.0])
            cand = [(t, n) for (t, n) in spawns.get(g, []) if u["t"] - 0.02 <= t <= nxt_close + 0.5]
            row = {"client": conn, "kind": u["kind"], "case": CASES[u["case"]], "cycle": u["cycle"], "subject": u["subject"],
                   "held": conn in u.get("held", []), "t": u["t"]}
            if not cand:
                row["reopened"] = False
                rows.append(row)
                continue
            t_spawn, inst = cand[0]
            # Lost-and-repaired re-open: the first instance may be lost; G uses the first visible instance.
            fv = None
            for (t, n) in cand:
                if n in first_vis:
                    fv = first_vis[n]
                    inst = n
                    t_spawn = t
                    break
            row["reopened"] = True
            row["instance"] = inst
            row["sameGuid"] = guid_of.get(inst) == g
            so = [o for o in server_opens if o["subject"] == u["subject"] and o["t"] >= u["t"] - 0.001 and o["t"] <= t_spawn + 0.5]
            row["repaired"] = any(o.get("repair") for o in so)
            row["timeout"] = any(x["c"] == conn and x["subject"] == u["subject"] and u["t"] - 1.0 <= x["t"] <= t_spawn for x in timeouts)
            rel = [x for x in releases if x["c"] == conn and x["subject"] == u["subject"] and u["t"] <= x["t"] <= t_spawn + 0.1]
            if row["held"]:
                row["hold_ms"] = (rel[0]["t"] - u["t"]) * 1000.0 if rel else None
                row["holdWhy"] = rel[0]["why"] if rel else None
                ack = [a for a in acks if a["c"] == conn and a["subject"] == u["subject"] and a["t"] >= u["t"] - 0.001 and not a["repair"]]
                if ack:
                    row["closeSentToAck_ms"] = ack[0]["ackMs"]
                    row["closeSends"] = ack[0]["closeSends"]
                    if so:
                        row["ackToOpen_ms"] = (so[0]["t"] - ack[0]["t"]) * 1000.0
            else:
                row["hold_ms"] = 0.0
            ch_open = [c for c in chan.get(inst, [])]
            if so and ch_open:
                o = so[-1] if row["repaired"] else so[0]
                pk = [o["pkt"]] + resends.get((u["subject"], conn, o["ch"]), [])
                cpk = ch_open[0]["pkt"]
                row["openSendIndex"] = pk.index(cpk) if cpk in pk else None
                row["firstSend"] = row["openSendIndex"] == 0
                if cpk in rx and fv:
                    row["T_reopen_ms"] = (fv["t"] - rx[cpk]) * 1000.0
            kp = keep.get(g, [])
            row["keepProcessing"] = any(u["t"] - 0.5 <= t <= t_spawn for t, on in kp if on)
            if fv:
                row["G_ms"] = (fv["t"] - u["t"]) * 1000.0
                row["warmupFrames"] = fv["framesSincePostNetInit"]
                row["warm"] = bool(fv["warm"])
                if fv["dying"]:
                    row["poseRatio"] = fv["headRoot"] / ref_dead if ref_dead else None
                else:
                    row["poseRatio"] = fv["headH"] / ref_alive if ref_alive else None
                ap = applied.get(inst)
                if ap:
                    ts = true_st(ap["evalT"])
                    if ts is not None:
                        row["clockErr_ms"] = (ap["est"] - ts) * 1000.0
            rows.append(row)
            cs["reopens"] += 1
        for r in rows:
            all_rows.append(r)
            if r.get("G_ms") is not None:
                units.append({"label": 1 if r["kind"] == "player" else 0, "block": int((r["t"] - t0) // 30.0),
                              "x": [r.get("hold_ms") or 0.0, 1.0 if r.get("timeout") else 0.0, 1.0 if r.get("repaired") else 0.0, r["G_ms"], float(r["warmupFrames"])]})
        # Estimator samples.
        for r in hr.of_type(recs, "clk"):
            if r.get("synced"):
                ts = true_st(r["t"])
                if ts is not None:
                    est_err.append((r["est"] - ts) * 1000.0)
        for r in rows:
            if r.get("clockErr_ms") is not None:
                clock_err.append(r["clockErr_ms"])
            if r.get("T_reopen_ms") is not None:
                t_reopen.append(r["T_reopen_ms"])
        # Every first-visible frame on this client (all pawns, every creation path).
        fvs = hr.of_type(recs, "firstVisible")
        unposed = []
        for r in fvs:
            ratio = (r["headRoot"] / ref_dead if ref_dead else None) if r["dying"] else (r["headH"] / ref_alive if ref_alive else None)
            if ratio is not None and ratio < 0.8:
                unposed.append({"name": r["name"], "subject": r["subject"], "dying": r["dying"], "ratio": ratio, "warm": r["warm"]})
        probe_rows.append({"client": conn, "firstVisible": len(fvs), "unposed": len(unposed), "unposedSamples": unposed[:10],
                           "refHeadHeight_cm": ref_alive, "refDeadHeadRoot_cm": ref_dead, "warmupTimeouts": len(warm_timeout),
                           "warmed": len([r for r in fvs if r["warm"]])})
        faults = hr.of_type(recs, "faults")
        cs["faultsLast"] = faults[-1] if faults else None
        ffca[conn] = faults[-1].get("failedToFindCreateActor") if faults else None
        log = os.path.join(run_dir, f"{cname}.log")
        chans = []
        if os.path.exists(log):
            with open(log, encoding="utf-8", errors="replace") as fh:
                for line in fh:
                    m = re.search(r"failed to find/create ACTOR.*Channel: (\d+)", line)
                    if m:
                        chans.append(int(m.group(1)))
        reps = [r for r in repairs if r["c"] == conn]
        cs["failedToFindCreateActorLines"] = len(chans)
        cs["failedToFindCreateActorRepairedSameChannel"] = len([c for c in chans if any(r["ch"] == c for r in reps)])
        cs["lostProbe"] = {k: len([r for r in hr.of_type(recs, "lostProbe") if r["result"] == k]) for k in ("reported", "slotEmpty", "closing", "otherGuid", "connectionNotOpen")}
        result["clients"][cname] = cs

    def sel(pred):
        return [r["G_ms"] for r in all_rows if r.get("G_ms") is not None and pred(r)]
    one_resend = lambda r: (r.get("openSendIndex") is not None and r["openSendIndex"] <= 1) and (r.get("closeSends", 1) <= 2)
    result["G"] = {
        "freeFirstSend_ms": q(sel(lambda r: not r["held"] and r.get("firstSend"))),
        "freeAll_ms": q(sel(lambda r: not r["held"])),
        "freeResendFraction": (lambda xs: len([r for r in xs if not r.get("firstSend")]) / len(xs) if xs else None)([r for r in all_rows if r.get("G_ms") is not None and not r["held"]]),
        "heldFirstSend_ms": q(sel(lambda r: r["held"] and r.get("firstSend"))),
        "heldAll_ms": q(sel(lambda r: r["held"])),
        "atMostOneResendPerHop_ms": q(sel(one_resend)),
        "burstLossCycles_ms": q(sel(lambda r: not one_resend(r))),
        "repaired_ms": q(sel(lambda r: r.get("repaired"))),
        "byCase": {c: q(sel(lambda r, c=c: r["case"] == c)) for c in "ABC"},
        "byKind": {k: q(sel(lambda r, k=k: r["kind"] == k)) for k in ("ai", "player")},
    }
    held = [r for r in all_rows if r["held"] and r.get("reopened")]
    result["hold"] = {
        "heldReopens": len(held), "hold_ms": q([r["hold_ms"] for r in held if r.get("hold_ms") is not None]),
        "holdBound_ms": timeout_ms + 1000.0 / 60.0,
        "holdOverBound": len([r for r in held if r.get("hold_ms") is not None and r["hold_ms"] > timeout_ms + 1000.0 / 60.0 + 0.5]),
        "timeouts": len([r for r in held if r.get("timeout")]),
        "timeoutRate": len([r for r in held if r.get("timeout")]) / len(held) if held else None,
        "closeSentToAck_ms": q([r["closeSentToAck_ms"] for r in held if r.get("closeSentToAck_ms") is not None]),
        "ackToOpen_ms": q([r["ackToOpen_ms"] for r in held if r.get("ackToOpen_ms") is not None]),
        "keepProcessingCycles": len([r for r in all_rows if r.get("keepProcessing")]),
        "allCloseSentToAck_ms": q([a["ackMs"] for a in acks]),
        "allCloseSends": {str(k): len([a for a in acks if a["closeSends"] == k]) for k in sorted({a["closeSends"] for a in acks})},
    }
    result["T_reopen_ms"] = q(t_reopen)
    result["clock"] = {"poseEvalError_ms": q(clock_err), "poseEvalAbsError_p95_ms": pct([abs(x) for x in clock_err], 0.95), "poseEvalAbsError_p99_ms": pct([abs(x) for x in clock_err], 0.99),
                       "estimatorError_ms": q(est_err), "estimatorAbsError_p95_ms": pct([abs(x) for x in est_err], 0.95), "estimatorAbsError_p99_ms": pct([abs(x) for x in est_err], 0.99)}
    result["lost"] = {"censusMissing": lost_census, "lostDestroys": len(lost_destroys), "lostDestroysRepaired": len([x for x in lost_destroys if x["repaired"]]),
                      "lostDestroysNeverVisibleAgain": len([x for x in lost_destroys if x["L_gap_ms"] is None]),
                      "L_gap_ms": q(lgaps), "samples": lost_destroys[:20],
                      "notReopened": len([r for r in all_rows if not r.get("reopened")]),
                      "failedToFindCreateActor": ffca}
    sf = hr.of_type(server, "faults")
    result["serverCounters"] = sf[-1] if sf else None
    result["serverRecords"] = {k: len(hr.of_type(server, k)) for k in ("failureReport", "repair", "repairDeferred", "unexpectedChannelFailure", "inject", "reopenTimeout", "closeNotSent", "observationCeiling", "repairDuplicate")}
    result["probe"] = probe_rows
    reopen_rows = [r for r in all_rows if r.get("reopened")]
    missing = defaultdict(int)
    for r in reopen_rows:
        if r.get("G_ms") is None:
            missing["%s/%s/%s/%s/%s" % (r["kind"], r["case"], r["client"], r["subject"], r.get("instance"))] += 1
    result["reopenedWithoutFirstVisible"] = dict(missing)
    result["netGuid"] = {"reopensSameGuid": len([r for r in reopen_rows if r.get("sameGuid")]), "reopens": len(reopen_rows),
                         "serverPawnGuidAllocations": len(hr.of_type(server, "pawnGuid")),
                         "serverPawnSpawns": len(hr.of_type(server, "pawnSpawn"))}
    names = ["hold_ms", "timeout", "repair", "G_ms", "warmupFrames"]
    result["parity"] = classifier(units, names)
    # Sanity of the classifier: a synthetic +40 ms on every player G must be detected.
    shifted = [dict(u, x=u["x"][:3] + [u["x"][3] + (40.0 if u["label"] else 0.0)] + u["x"][4:]) for u in units]
    result["paritySanityShift40ms"] = classifier(shifted, names)
    result["ksDescriptive"] = {n: ks([u["x"][i] for u in units if u["label"] == 1], [u["x"][i] for u in units if u["label"] == 0]) for i, n in enumerate(names)}
    result["frameProbe"] = frame_probe(run_dir, clients, frames_out)
    fp = [x for x in result["frameProbe"] if isinstance(x, dict) and x.get("assessable")]
    result["frameProbeSummary"] = {"captured": len(result["frameProbe"]) if isinstance(result["frameProbe"], list) else 0,
                                   "assessable": len(fp), "f00RatioBelow0_8": len([x for x in fp if x["f00_ratio"] is not None and x["f00_ratio"] < 0.8]),
                                   "f00Ratio": q([x["f00_ratio"] for x in fp if x["f00_ratio"] is not None])}
    print(json.dumps({k: v for k, v in result.items() if k not in ("frameProbe",)}, indent=1, default=str)[:20000])
    if out_json:
        with open(out_json, "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=1, default=str)


if __name__ == "__main__":
    args = sys.argv[1:]
    out = None
    frames = None
    if "--json" in args:
        out = args[args.index("--json") + 1]
        del args[args.index("--json"):args.index("--json") + 2]
    if "--frames" in args:
        frames = args[args.index("--frames") + 1]
        del args[args.index("--frames"):args.index("--frames") + 2]
    main(args[0], out, frames)
