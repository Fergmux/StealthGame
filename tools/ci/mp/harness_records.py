"""Shared loader for StealthTestHarness T3 record files (Saved/Harness/<RunId>/*.jsonl).

Observation helpers only (the .claude/scripts rule): they summarise records, the spike reports judge.
"""
import json
import os
import statistics


def load(path):
    """Records of one process file, in file order (a truncated last line is skipped)."""
    out = []
    with open(path, encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                out.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    return out


def load_run(run_dir):
    """{'server': [...], 'client1': [...], ...} for every *.jsonl in run_dir."""
    files = {}
    for name in sorted(os.listdir(run_dir)):
        if name.endswith(".jsonl"):
            files[name[:-6]] = load(os.path.join(run_dir, name))
    return files


def of_type(records, kind):
    return [r for r in records if r.get("type") == kind]


def layouts(records):
    """{class name: {parent index: 'Owner.Prop[idx] (cond)'}} from 'layout' records."""
    out = {}
    for rec in of_type(records, "layout"):
        table = {}
        for rep in rec["reps"]:
            suffix = f"[{rep['idx']}]" if rep["idx"] else ""
            table[rep["i"]] = (f"{rep['owner']}.{rep['prop']}{suffix}", rep["cond"])
        out[rec["cls"]] = table
    return out


def quantiles(values):
    """n, min, p10, median, p90, p99, max of a list of numbers (None when empty)."""
    if not values:
        return None
    ordered = sorted(values)

    def pick(q):
        return ordered[min(len(ordered) - 1, int(round(q * (len(ordered) - 1))))]

    return {
        "n": len(ordered),
        "min": ordered[0],
        "p10": pick(0.10),
        "median": statistics.median(ordered),
        "p90": pick(0.90),
        "p99": pick(0.99),
        "max": ordered[-1],
    }


def fmt_q(q, unit=""):
    if not q:
        return "n=0"
    return (f"n={q['n']} min={q['min']:.1f}{unit} p10={q['p10']:.1f}{unit} median={q['median']:.1f}{unit} "
            f"p90={q['p90']:.1f}{unit} p99={q['p99']:.1f}{unit} max={q['max']:.1f}{unit}")
