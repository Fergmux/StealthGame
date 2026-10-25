"""SCL story 045 observations: net-emulation starvation cadence (ST1) and traversal under emulation (E12).

Usage: python -I tools/ci/mp/analyze_scl045.py <run dir> [--json <out.json>]

Reads Saved/Harness/<run>/scl045-starvation.jsonl and scl045-traversal.jsonl written by the CQTest spike
StealthSpike.Locomotion.PIE.NetEmulationTraversal (Source/StealthTestHarness/Private/Tests/PIE/Spikes/).
Observations only; the spike report states the answers.
"""
import collections
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402

BOUND_CM = 64.0  # PawnSmoothSnapThresholdCm / 4 (ADR-0010)
DRAIN_S = 0.15   # one-way PktLag 100 ms + 50 ms: client moves already in flight when the loss starts


def dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def starvation(path):
    recs = hr.load(path)
    out = {}
    session = hr.of_type(recs, "session")
    out["session"] = {k: v for k, v in session[0].items() if k not in ("t", "type")} if session else None
    runs = collections.defaultdict(lambda: {"steps": [], "interval": None})
    for r in recs:
        key = (r.get("config"), r.get("repeat"))
        if r["type"] == "starveBegin":
            runs[key]["interval"] = r["interval"]
            runs[key]["serverVel"] = r["serverVel"]
        elif r["type"] == "serverStep":
            runs[key]["steps"].append((r["sinceStarve"], r["stepCm"]))
    per_config = collections.defaultdict(lambda: {"first": [], "intervals": [], "last": [], "count": [], "stepCm": [], "interval": None})
    for (config, _repeat), run in runs.items():
        # Moves already in the 100 ms outgoing-lag queue still arrive after the loss starts: only steps after
        # that drain (one-way lag + 50 ms) are server-forced updates.
        run["steps"] = [(t, cm) for t, cm in run["steps"] if t > DRAIN_S]
        times = [t for t, _ in run["steps"]]
        c = per_config[config]
        c["interval"] = run["interval"]
        if not times:
            c["count"].append(0)
            continue
        c["first"].append(times[0])
        c["last"].append(times[-1])
        c["count"].append(len(times))
        c["intervals"] += [b - a for a, b in zip(times, times[1:])]
        c["stepCm"] += [cm for _, cm in run["steps"]]
    for config, c in sorted(per_config.items()):
        med = hr.quantiles(c["intervals"])
        out[f"config{config}"] = {
            "MAXCLIENTUPDATEINTERVAL": c["interval"],
            "repeats": len(c["count"]),
            "stepsPerRun": c["count"],
            "firstForcedStepAfterStarveS": hr.quantiles(c["first"]),
            "lastStepAfterStarveS": hr.quantiles(c["last"]),
            "stepIntervalS": med,
            "cadenceHz": (1.0 / med["median"]) if med and med["median"] > 0 else None,
            "stepCm": hr.quantiles(c["stepCm"]),
        }
    return out


def traversal(path):
    recs = hr.load(path)
    out = {"staged": [{k: r[k] for k in ("block", "kitHeight", "floorZ")} for r in hr.of_type(recs, "staged")],
           "lanes": [{k: r[k] for k in ("kind", "lane", "block", "blockHeight", "blockDepth")} for r in hr.of_type(recs, "lane")],
           "signature": [r["params"] for r in hr.of_type(recs, "tryTraversalSignature")]}
    kinds = collections.defaultdict(list)
    for r in hr.of_type(recs, "trial"):
        kinds[r["kind"]].append(r)
    for kind, trials in kinds.items():
        ok = [t for t in trials if t["result"] == "ok"]
        res = collections.Counter(t["result"] for t in trials)
        actions = collections.Counter((t["serverResult"]["action"], round(t["serverResult"]["obstacleHeight"]), round(t["serverResult"]["obstacleDepth"])) for t in ok)
        owner_err = [dist(t["ownerEnd"], t["serverEnd"]) for t in ok if t["ownerEnd"] and t["serverEnd"]]
        proxy_err, proxy_settled, bp = [], [], 0
        proxies = 0
        for t in ok:
            for p in t["proxies"]:
                proxies += 1
                bp += p.get("bpTraversal", 0)
                if p["end"] and t["serverEnd"]:
                    proxy_err.append(dist(p["end"], t["serverEnd"]))
                if p["settled"] and t["serverSettled"]:
                    proxy_settled.append(dist(p["settled"], t["serverSettled"]))
        owner_settled = [dist(t["ownerSettled"], t["serverSettled"]) for t in ok]
        rm_frac = [t["proxyRepRootMotionFrames"] / t["proxyFrames"] for t in ok if t["proxyFrames"]]
        nrm_frac = [t["proxyNetworkedRootMotionFrames"] / t["proxyFrames"] for t in ok if t["proxyFrames"]]
        modes = collections.Counter()
        for t in ok:
            for side in ("serverModes", "ownerModes", "proxyModes"):
                for m in filter(None, t[side].split("|")):
                    modes[f"{side[:-5]}:{m}"] += 1
        all_err = owner_err + proxy_err
        out[kind] = {
            "results": dict(res),
            "nCompleted": len(ok),
            "actions": {f"{a} h={h} d={d}": n for (a, h, d), n in actions.items()},
            "ownerEndErrorCm": hr.quantiles(owner_err),
            "proxyEndErrorCm": hr.quantiles(proxy_err),
            "endErrorsWithinBound": sum(1 for e in all_err if e <= BOUND_CM),
            "endErrorsTotal": len(all_err),
            "ownerSettledErrorCm": hr.quantiles(owner_settled),
            "proxySettledErrorCm": hr.quantiles(proxy_settled),
            "midActionOwnerOffsetCm": hr.quantiles([t["maxOwnerDeviation"] for t in ok]),
            "serverDurationS": hr.quantiles([t["serverEndT"] - t["serverStartT"] for t in ok]),
            "ownerLeadS": hr.quantiles([t["serverStartT"] - t["ownerStartT"] for t in ok]),
            "movementModes": dict(modes),
            "proxyRepRootMotionActiveFraction": hr.quantiles(rm_frac),
            "proxyNetworkedRootMotionMontageFraction": hr.quantiles(nrm_frac),
            "proxyBlueprintTraversalRan": f"{bp}/{proxies}",
        }
    return out


def main():
    args = sys.argv[1:]
    out_json = None
    if "--json" in args:
        i = args.index("--json")
        out_json = args[i + 1]
        del args[i:i + 2]
    run = args[0]
    result = {"run": os.path.basename(os.path.normpath(run))}
    sp = os.path.join(run, "scl045-starvation.jsonl")
    tp = os.path.join(run, "scl045-traversal.jsonl")
    if os.path.exists(sp):
        result["starvation"] = starvation(sp)
    if os.path.exists(tp):
        result["traversal"] = traversal(tp)
    text = json.dumps(result, indent=1)
    print(text)
    if out_json:
        with open(out_json, "w", encoding="utf-8") as handle:
            handle.write(text + "\n")


if __name__ == "__main__":
    main()
