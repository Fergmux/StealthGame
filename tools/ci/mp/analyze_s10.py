"""IVP spike S10 (story 005) analyser: Saved/Harness/<run>/ records of scenario "S10" (with -StealthPacketTiming).

For every server un-cull (cases A same-step, C in-flight race, B death-write re-open) and every client: whether a new
client actor was created (a different actor instance than before the cull), whether the NetGUID is the same, the
end-to-end time from the server's un-cull to the client's actor spawn, the client-side T_reopen (packet receipt to
actor spawn, and spawn to the first ticked frame), the first-frame root error against the server's held position,
the pose-time error of the stub RefreshPresentation (client estimated server time vs the true server time at that
instant, from the server's time records), the death fast-forward, whether the client holds exactly one live actor
for the subject after each cycle (stale/missing), and the ensure/error/warning counts. Observations only.

Usage: python -I tools/ci/mp/analyze_s10.py Saved/Harness/<run> [--json out.json]
"""
import bisect
import json
import math
import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402

CASES = {0: "A", 1: "B", 2: "C"}


def server_clock(server):
    """QPC t -> server world time, from the server's 0.1 s truth records (same process, rate 1)."""
    pts = sorted((r["t"], r["st"]) for r in server if r.get("type") in ("truth", "net") and r.get("st", -1) >= 0)
    ts = [p[0] for p in pts]

    def at(t):
        i = bisect.bisect_right(ts, t) - 1
        if i < 0:
            return None
        return pts[i][1] + (t - pts[i][0])
    return at


def main(run_dir, out_json=None):
    run = hr.load_run(run_dir)
    server = run["server"]
    clients = {k: v for k, v in run.items() if k.startswith("client")}
    true_st = server_clock(server)
    unculls = hr.of_type(server, "uncull")
    closes = hr.of_type(server, "cullClose")
    result = {"run": os.path.basename(run_dir.rstrip("/\\")), "unculls": len(unculls),
              "serverGuidStableAtUncull": all(u["guid"] == u["subject"] for u in unculls if u["subject"] != "0"),
              "subjectsSelectedBeforeFirstReplication": len(set(u["subject"] for u in unculls if u["subject"] == "0")),
              "byCase": {c: len([u for u in unculls if CASES[u["case"]] == c]) for c in "ABC"},
              "closeLateBy_ms": hr.quantiles([c["lateBy"] * 1000 for c in closes]), "clients": {}}
    for name, recs in clients.items():
        guid_of = {}
        for r in hr.of_type(recs, "pawnGuid"):
            guid_of[r["name"]] = r["guid"]
        spawns = [(r["t"], r["name"], guid_of.get(r["name"])) for r in hr.of_type(recs, "pawnSpawn")]
        destroys = [(r["t"], r["name"], r["guid"]) for r in hr.of_type(recs, "pawnDestroy")]
        first_frames = {}
        for r in hr.of_type(recs, "reopenPresent"):
            if r["phase"] == "firstFrame":
                first_frames.setdefault(r["name"], r)
        rx_times = sorted(float(t) for rec in hr.of_type(recs, "pkt") for _, t in rec["rx"])
        rows = defaultdict(list)
        missing = 0
        multiple = 0
        for k, u in enumerate(unculls):
            # The subject key is the NetGUID at selection; a pawn picked before its first replication has "0" there,
            # so match client actors by the NetGUID the server had at the un-cull.
            guid = u["guid"] if u["guid"] != "0" else u["subject"]
            case = CASES[u["case"]]
            nxt = min([c["t"] for c in closes if c["subject"] == u["subject"] and c["t"] > u["t"]] or [u["t"] + 3.0])
            cand = [s for s in spawns if s[2] == guid and u["t"] - 0.05 <= s[0] <= nxt]
            # Which actor instance existed for this guid just before the cull of this cycle?
            prior = [s for s in spawns if s[2] == guid and s[0] < u["t"] - u["sinceClose"] - 0.001]
            prior_name = prior[-1][1] if prior else None
            alive_at = u["t"] + 1.0
            live = [s for s in spawns if s[2] == guid and s[0] <= alive_at
                    and not any(d[1] == s[1] and s[0] < d[0] <= alive_at for d in destroys)]
            if not live:
                missing += 1
            elif len(live) > 1:
                multiple += 1
            if not cand:
                rows[case].append({"spawned": False, "kept": prior_name is not None and any(s[1] == prior_name for s in live)})
                continue
            t_spawn, actor, _ = cand[0]
            i = bisect.bisect_right(rx_times, t_spawn) - 1
            ff = first_frames.get(actor)
            row = {"spawned": True, "newActor": actor != prior_name, "sameGuid": True,
                   "endToEnd_ms": (t_spawn - u["t"]) * 1000.0,
                   "rxToSpawn_ms": (t_spawn - rx_times[i]) * 1000.0 if i >= 0 else None}
            if ff:
                loc = ff["loc"]
                row["spawnToFirstFrame_ms"] = (ff["t"] - t_spawn) * 1000.0
                row["rootError_cm"] = math.dist(loc, u["loc"])
                ideal = true_st(ff["t"])
                if ideal is not None and ff["estServerNow"] >= 0:
                    # Signed: negative = the client's estimate of server time lags the true server time.
                    row["serverClockError_ms"] = (ff["estServerNow"] - ideal) * 1000.0
                    if ff.get("pingMs", -1) >= 0:
                        row["serverClockErrorHalfRttCompensated_ms"] = (ff["estServerNow"] + ff["pingMs"] / 2000.0 - ideal) * 1000.0
                    if ff["death"] >= 0:
                        row["deathElapsed_ms"] = ff["deathElapsed"] * 1000.0
                        row["deathElapsedIdeal_ms"] = (ideal - ff["death"]) * 1000.0
            rows[case].append(row)
        summary = {"staleOrMissingAfter1s": missing, "multipleLiveActors": multiple}
        for case, items in rows.items():
            spawned = [r for r in items if r["spawned"]]
            summary[case] = {
                "cycles": len(items), "reopened": len(spawned),
                "notReopenedButKept": len([r for r in items if not r["spawned"] and r.get("kept")]),
                "newActor": len([r for r in spawned if r["newActor"]]),
                "endToEnd_ms": hr.quantiles([r["endToEnd_ms"] for r in spawned]),
                "rxToSpawn_ms": hr.quantiles([r["rxToSpawn_ms"] for r in spawned if r.get("rxToSpawn_ms") is not None]),
                "spawnToFirstFrame_ms": hr.quantiles([r["spawnToFirstFrame_ms"] for r in spawned if "spawnToFirstFrame_ms" in r]),
                "rootError_cm": hr.quantiles([r["rootError_cm"] for r in spawned if "rootError_cm" in r]),
                "serverClockError_ms": hr.quantiles([r["serverClockError_ms"] for r in spawned if "serverClockError_ms" in r]),
                "serverClockErrorHalfRttCompensated_ms": hr.quantiles([r["serverClockErrorHalfRttCompensated_ms"] for r in spawned if "serverClockErrorHalfRttCompensated_ms" in r]),
                "deathElapsedAtFirstFrame_ms": hr.quantiles([r["deathElapsed_ms"] for r in spawned if "deathElapsed_ms" in r]),
                "deathElapsedIdealAtFirstFrame_ms": hr.quantiles([r["deathElapsedIdeal_ms"] for r in spawned if "deathElapsedIdeal_ms" in r]),
            }
        faults = hr.of_type(recs, "faults")
        if faults:
            # Faults during the cycles only: the last sample minus the last sample before the first un-cull (start-up noise).
            first_u = unculls[0]["t"] if unculls else 0.0
            base = [f for f in faults if f["t"] <= first_u]
            base = base[-1] if base else {"ensures": 0, "errors": {}, "warnings": {}}
            last = faults[-1]
            summary["faultsDuringCycles"] = {
                "ensures": last["ensures"] - base["ensures"],
                "errors": {k: v - base["errors"].get(k, 0) for k, v in last["errors"].items() if v - base["errors"].get(k, 0)},
                "warnings": {k: v - base["warnings"].get(k, 0) for k, v in last["warnings"].items() if v - base["warnings"].get(k, 0)},
                "pawnCensusFirst": base.get("pawns"), "pawnCensusLast": last.get("pawns")}
        else:
            summary["faultsDuringCycles"] = None
        summary["framesCaptured"] = len(hr.of_type(recs, "frameCapture"))
        result["clients"][name] = summary
    cfg = [r for r in hr.of_type(server, "event") if r.get("ev") == "config"]
    result["raceGuard"] = bool(cfg and cfg[0].get("raceGuard"))
    result["raceGuardHeld_ms"] = hr.quantiles([r["heldMs"] for r in hr.of_type(server, "raceGuardRelease")])
    sf = hr.of_type(server, "faults")
    result["serverFaults"] = {k: sf[-1][k] for k in ("ensures", "errors", "warnings")} if sf else None
    # L32: the server's NetGUIDs of pawns spawned during the run, in order, against the run's events.
    result["pawnGuidsAllocated"] = [r["guid"] for r in hr.of_type(server, "pawnGuid")]
    print(json.dumps(result, indent=1))
    if out_json:
        with open(out_json, "w", encoding="utf-8") as handle:
            json.dump(result, handle, indent=1)


if __name__ == "__main__":
    args = sys.argv[1:]
    out = None
    if "--json" in args:
        out = args[args.index("--json") + 1]
        args = args[:args.index("--json")]
    main(args[0], out)
