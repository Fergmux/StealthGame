#!/usr/bin/env python3
"""Crowd behaviour classifier gate (AC-CNB-28, GDD F6). Standard library only.

Input: CSV or JSONL, one row per 30 s window of ordinary walking (no tell):
  source (human|npc), participant (id), and the nine F6 feature columns.
For each feature a single-feature threshold classifier (threshold + direction)
is fit on training folds and scored on the held-out fold; folds are grouped by
participant (k=5), so no participant is in both train and test.

  BA_j  = mean over folds of the held-out (TPR + TNR) / 2, using the
          (threshold, direction) that maximises BA on the TRAIN folds
  SE    = max(0.25 * sqrt(1/n_P + 1/n_N), cluster-bootstrap SD of pooled
          held-out BA, resampling participants with replacement)
  PASS_j iff BA_j <= 0.55 and BA_j + 1.645 * SE <= 0.58
Gate passes iff all nine features pass. Exit: 0 pass, 1 fail, 2 insufficient.
"""
import argparse
import csv
import json
import math
import random
import sys

FEATURES = ["mean_speed", "speed_variance", "pause_rate", "pause_length",
            "heading_change_rate", "head_yaw_rate", "glance_hold_length",
            "path_straightness", "full_pace_share"]
BA_MAX, BA_UPPER, Z95 = 0.55, 0.58, 1.645
MIN_HUMAN, MIN_PARTICIPANTS, MIN_NPC = 600, 30, 2400
K = 5


def load(path):
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        if path.lower().endswith((".jsonl", ".ndjson")):
            recs = [json.loads(line) for line in f if line.strip()]
        else:
            recs = list(csv.DictReader(f))
    for r in recs:
        src = str(r["source"]).strip().lower()
        if src not in ("human", "npc"):
            raise ValueError("bad source: %r" % src)
        rows.append((src, str(r["participant"]), [float(r[k]) for k in FEATURES]))
    return rows


def best_threshold(vals, labels):
    """Returns (theta, sign) maximising train BA. sign=+1: predict human if v > theta."""
    pairs = sorted(zip(vals, labels))
    n_p = sum(1 for _, l in pairs if l == 1)
    n_n = len(pairs) - n_p
    best = (-1.0, 0.0, 1)
    cum_p = cum_n = 0
    i = 0
    n = len(pairs)
    while i < n:
        j = i
        while j < n and pairs[j][0] == pairs[i][0]:
            if pairs[j][1] == 1:
                cum_p += 1
            else:
                cum_n += 1
            j += 1
        theta = pairs[i][0] if j >= n else (pairs[i][0] + pairs[j][0]) / 2.0
        ba = ((n_p - cum_p) / n_p + cum_n / n_n) / 2.0
        for b, s in ((ba, 1), (1.0 - ba, -1)):
            if b > best[0]:
                best = (b, theta, s)
        i = j
    return best[1], best[2]


def ba_of(preds):
    """preds: list of (label, predicted), 1 = human. Returns BA or None."""
    tp = sum(1 for l, p in preds if l == 1 and p == 1)
    fn = sum(1 for l, p in preds if l == 1 and p == 0)
    tn = sum(1 for l, p in preds if l == 0 and p == 0)
    fp = sum(1 for l, p in preds if l == 0 and p == 1)
    if tp + fn == 0 or tn + fp == 0:
        return None
    return (tp / (tp + fn) + tn / (tn + fp)) / 2.0


def assign_folds(rows, rng):
    groups = sorted({(s, p) for s, p, _ in rows})
    rng.shuffle(groups)
    return {g: i % K for i, g in enumerate(groups)}


def evaluate_feature(rows, fidx, fold_of, rng, boot):
    labels = [1 if s == "human" else 0 for s, _, _ in rows]
    folds = [fold_of[(s, p)] for s, p, _ in rows]
    vals = [x[fidx] for _, _, x in rows]
    fold_bas, held = [], []
    for k in range(K):
        tr = [i for i in range(len(rows)) if folds[i] != k]
        te = [i for i in range(len(rows)) if folds[i] == k]
        if not te:
            continue
        theta, sign = best_threshold([vals[i] for i in tr], [labels[i] for i in tr])
        preds = []
        for i in te:
            p = 1 if (vals[i] > theta) == (sign == 1) else 0
            preds.append((labels[i], p))
            held.append(((rows[i][0], rows[i][1]), labels[i], p))
        b = ba_of(preds)
        if b is not None:
            fold_bas.append(b)
    ba = sum(fold_bas) / len(fold_bas)
    clusters = {}
    for c, l, p in held:
        clusters.setdefault(c, []).append((l, p))
    keys = list(clusters)
    stats = []
    for _ in range(boot):
        pr = []
        for _ in keys:
            pr.extend(clusters[keys[rng.randrange(len(keys))]])
        b = ba_of(pr)
        if b is not None:
            stats.append(b)
    m = sum(stats) / len(stats)
    sd = math.sqrt(sum((x - m) ** 2 for x in stats) / max(1, len(stats) - 1))
    return ba, sd


def run_gate(rows, boot=200, seed=12345):
    """Returns (exit_code, markdown, results)."""
    n_p = sum(1 for s, _, _ in rows if s == "human")
    n_n = len(rows) - n_p
    parts = len({p for s, p, _ in rows if s == "human"})
    lines = ["| Check | Value | Required |", "|---|---|---|",
             "| human windows | %d | >= %d |" % (n_p, MIN_HUMAN),
             "| human participants | %d | >= %d |" % (parts, MIN_PARTICIPANTS),
             "| NPC windows | %d | >= %d |" % (n_n, MIN_NPC), ""]
    if n_p < MIN_HUMAN or parts < MIN_PARTICIPANTS or n_n < MIN_NPC:
        lines.append("INSUFFICIENT DATA - gate not evaluated (never PASS).")
        return 2, "\n".join(lines), {}
    rng = random.Random(seed)
    fold_of = assign_folds(rows, rng)
    se0 = 0.25 * math.sqrt(1.0 / n_p + 1.0 / n_n)
    lines += ["Unclustered SE = 0.25*sqrt(1/n_P + 1/n_N) = %.4f; SE used = max(unclustered, cluster-bootstrap SD)." % se0, "",
              "| Feature | held-out BA | SE | upper (BA+1.645*SE) | Result |", "|---|---|---|---|---|"]
    results, ok = {}, True
    for j, name in enumerate(FEATURES):
        ba, sd = evaluate_feature(rows, j, fold_of, rng, boot)
        se = max(se0, sd)
        up = ba + Z95 * se
        p = ba <= BA_MAX and up <= BA_UPPER
        ok = ok and p
        results[name] = (ba, se, up, p)
        lines.append("| %s | %.4f | %.4f | %.4f | %s |" % (name, ba, se, up, "PASS" if p else "FAIL"))
    lines += ["", "GATE: %s (BA <= %.2f and upper <= %.2f for all 9 features)" % ("PASS" if ok else "FAIL", BA_MAX, BA_UPPER)]
    return (0 if ok else 1), "\n".join(lines), results


def synth(shift_feature=None, seed=7, n_h=900, n_h_parts=36, n_n=3000, n_n_parts=20):
    rng = random.Random(seed)
    rows = []
    for i in range(n_h):
        x = [rng.gauss(0, 1) for _ in FEATURES]
        if shift_feature is not None:
            x[FEATURES.index(shift_feature)] += 1.0
        rows.append(("human", "h%d" % (i % n_h_parts), x))
    for i in range(n_n):
        rows.append(("npc", "n%d" % (i % n_n_parts), [rng.gauss(0, 1) for _ in FEATURES]))
    return rows


def selftest():
    out = []
    c1, t1, _ = run_gate(synth(None), boot=100)
    out += ["## Case A: identical distributions (expect PASS)", t1, ""]
    assert c1 == 0, "identical case must PASS\n" + t1
    c2, t2, r2 = run_gate(synth("pause_length"), boot=100)
    out += ["## Case B: pause_length shifted +1 SD for humans (expect FAIL on that feature)", t2, ""]
    assert c2 == 1 and not r2["pause_length"][3], "shifted case must FAIL on pause_length\n" + t2
    assert all(r2[f][3] for f in FEATURES if f != "pause_length"), "other features must still pass\n" + t2
    c3, t3, _ = run_gate(synth(None, n_h=300), boot=10)
    assert c3 == 2, "small sample must be INSUFFICIENT"
    out += ["## Case C: 300 human windows (expect INSUFFICIENT DATA, exit 2)", t3, "", "SELFTEST OK"]
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", nargs="?", help="CSV or JSONL of windows")
    ap.add_argument("--out", help="also write the Markdown report to FILE")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--bootstrap", type=int, default=200)
    a = ap.parse_args()
    if a.selftest:
        text, code = selftest(), 0
    else:
        if not a.input:
            ap.error("input file required (or --selftest)")
        code, text, _ = run_gate(load(a.input), boot=a.bootstrap)
    print(text)
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    sys.exit(code)


if __name__ == "__main__":
    main()
