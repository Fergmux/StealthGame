"""Packet-level net measurement analyser (session-travel story 002; IVP story 006 S12).

Reads the "pkt" (per-packet flush/receive times with packet ids, from UStealthTimingIpConnection under
-StealthPacketTiming), "rtt" (raw engine RTT samples), "frames" and phase records of a T3 run. Every process runs
on one machine, so FPlatformTime (QueryPerformanceCounter) times compare across processes.

Reports, per phase (lobby / travel / round, from the server's serverTravel and roundTick records) and overall:
  - R_c: client-to-server packets received by the server per second, per connection;
  - one-way latency server->client and client->server (receiver processing time minus sender flush time, matched
    by packet id), with the p99 (L99 for IVP F3/F4);
  - loss: sent ids never received, and the burst (run-length) distribution of consecutive lost ids;
  - outages: gaps between consecutive packets received by the server from each client (what Session's silence
    clock sees), with counts over 0.25/0.5/1/2/3 s;
  - RTT quantiles from the engine's own samples (server side per client, and each client's own);
  - map load: server T_srvload (serverTravel -> postSeamlessTravel) and each client's travel -> acknowledgement;
  - listen-host frame time (p99 of per-second p99s, max).
Observations only; the spike reports judge.

Usage: python -I tools/ci/mp/analyze_net.py Saved/Harness/<run> [--json out.json]
"""
import json
import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402

GAP_THRESHOLDS = (0.25, 0.5, 1.0, 2.0, 3.0)


def packets(records, conn_filter=None):
    """{conn name: {'tx': [(id, t)], 'rx': [(id, t)]}} from pkt records (a rejoin's new connection keeps the name)."""
    out = defaultdict(lambda: {"tx": [], "rx": []})
    for rec in hr.of_type(records, "pkt"):
        name = rec["c"]
        if conn_filter and name != conn_filter:
            continue
        out[name]["tx"].extend((int(i), float(t)) for i, t in rec["tx"])
        out[name]["rx"].extend((int(i), float(t)) for i, t in rec["rx"])
    return out


def phases(server):
    joined = hr.of_type(server, "allJoined")
    travel = hr.of_type(server, "serverTravel")
    tick = hr.of_type(server, "roundTick")
    start_round = [r for r in hr.of_type(server, "event") if r.get("ev") == "startRound"]
    end = [r["t"] for r in server if r.get("type") == "end"]
    t0 = joined[0]["t"] if joined else server[0]["t"]
    t_end = end[0] if end else server[-1]["t"]
    out = {}
    if travel:
        out["lobby"] = (t0, travel[0]["t"])
        out["travel"] = (travel[0]["t"], tick[0]["t"] if tick else t_end)
        out["round"] = (tick[0]["t"] if tick else t_end, t_end)
    else:
        out["round"] = (start_round[0]["t"] if start_round else t0, t_end)
    out["all"] = (t0, t_end)
    return out


def match(tx, rx):
    """One-way latencies (s) for ids present on both sides, the lost ids, and the receive times."""
    rx_by_id = {}
    for pid, t in rx:
        rx_by_id.setdefault(pid, t)
    lat = []
    lost = []
    for pid, t in tx:
        if pid in rx_by_id:
            lat.append((t, rx_by_id[pid] - t))
        else:
            lost.append((t, pid))
    return lat, lost


def bursts(lost_ids):
    """Run lengths of consecutive lost packet ids."""
    runs = []
    ids = sorted(lost_ids)
    run = 0
    prev = None
    for pid in ids:
        if prev is not None and pid == prev + 1:
            run += 1
        else:
            if run:
                runs.append(run)
            run = 1
        prev = pid
    if run:
        runs.append(run)
    hist = defaultdict(int)
    for r in runs:
        hist[r] += 1
    return dict(sorted(hist.items()))


def within(values, window):
    a, b = window
    return [v for t, v in values if a <= t <= b]


def main(run_dir, out_json=None):
    run = hr.load_run(run_dir)
    server = run["server"]
    clients = {k: v for k, v in run.items() if k.startswith("client")}
    phase = phases(server)
    srv = packets(server)
    result = {"run": os.path.basename(run_dir.rstrip("/\\")), "phases_s": {k: round(b - a, 1) for k, (a, b) in phase.items()},
              "connections": {}, "rtt_ms": {}, "load": {}}
    boot = hr.of_type(server, "boot")
    if boot:
        result["serverProfile"] = {k: boot[0].get(k) for k in ("profile", "PktLag", "PktLoss")}
    all_down, all_up = defaultdict(list), defaultdict(list)
    for name, recs in clients.items():
        index = name.replace("client", "")
        conn = f"C{index}"
        cli = packets(recs, "server")["server"]
        s = srv.get(conn, {"tx": [], "rx": []})
        down, down_lost = match(s["tx"], cli["rx"])
        up, up_lost = match(cli["tx"], s["rx"])
        rx_times = sorted(t for _, t in s["rx"])
        gaps = [(rx_times[i], rx_times[i] - rx_times[i - 1]) for i in range(1, len(rx_times))]
        entry = {"serverTx": len(s["tx"]), "clientRx": len(cli["rx"]), "clientTx": len(cli["tx"]), "serverRx": len(s["rx"]),
                 "matchedDown": len(down), "matchedUp": len(up)}
        for pname, window in phase.items():
            dur = max(window[1] - window[0], 1e-6)
            rx_in = [t for t in rx_times if window[0] <= t <= window[1]]
            d = [v * 1000.0 for v in within(down, window)]
            u = [v * 1000.0 for v in within(up, window)]
            g = within(gaps, window)
            dl = [pid for t, pid in down_lost if window[0] <= t <= window[1]]
            ul = [pid for t, pid in up_lost if window[0] <= t <= window[1]]
            sent_down = len([1 for _, t in s["tx"] if window[0] <= t <= window[1]])
            sent_up = len([1 for _, t in cli["tx"] if window[0] <= t <= window[1]])
            entry[pname] = {
                "R_c_pps": round(len(rx_in) / dur, 2),
                "R_s_pps": round(sent_down / dur, 2),
                "oneWayDown_ms": hr.quantiles(d),
                "oneWayUp_ms": hr.quantiles(u),
                "lossDown": round(len(dl) / max(sent_down, 1), 4),
                "lossUp": round(len(ul) / max(sent_up, 1), 4),
                "burstsDown": bursts(dl),
                "burstsUp": bursts(ul),
                "serverRxGap_ms": hr.quantiles([x * 1000.0 for x in g]),
                "serverRxGapsOver_s": {str(th): len([x for x in g if x > th]) for th in GAP_THRESHOLDS},
            }
            all_down[pname].extend(d)
            all_up[pname].extend(u)
        result["connections"][conn] = entry
        # The client's own RTT samples.
        samples = [(r["t"], v) for r in hr.of_type(recs, "rtt") for v in r["ms"]]
        result["rtt_ms"][f"{conn}-client"] = {p: hr.quantiles(within(samples, w)) for p, w in phase.items()}
        # Client load: clientTravel -> first ackSent after it (old runs wrote the travel type into "type").
        travels = [r for r in recs if r.get("type") == "clientTravel" or ("seamless" in r and isinstance(r.get("type"), int))]
        acks = hr.of_type(recs, "ackSent")
        loads = []
        for tr in travels:
            after = [a["t"] for a in acks if a["t"] > tr["t"]]
            if after:
                loads.append(round(after[0] - tr["t"], 3))
        result["load"][f"{conn}-travelToAck_s"] = loads
    for conn in srv:
        if conn.startswith("C"):
            samples = [(r["t"], v) for r in hr.of_type(server, "rtt") if r["c"] == conn for v in r["ms"]]
            result["rtt_ms"][f"{conn}-server"] = {p: hr.quantiles(within(samples, w)) for p, w in phase.items()}
    result["aggregate"] = {p: {"oneWayDown_ms": hr.quantiles(all_down[p]), "oneWayUp_ms": hr.quantiles(all_up[p])} for p in phase}
    travel = hr.of_type(server, "serverTravel")
    post = hr.of_type(server, "postSeamlessTravel")
    if travel and post:
        result["load"]["T_srvload_s"] = round(post[0]["t"] - travel[0]["t"], 3)
    tick = hr.of_type(server, "roundTick")
    if travel and tick:
        result["load"]["travelToRoundTick_s"] = round(tick[0]["t"] - travel[0]["t"], 3)
    frames = hr.of_type(server, "frames")
    if frames:
        result["serverFrame_ms"] = {"p99_of_p99": hr.quantiles([f["p99Ms"] for f in frames])["p99"], "max": max(f["maxMs"] for f in frames),
                                    "mean": round(sum(f["meanMs"] for f in frames) / len(frames), 3)}
    result["ends"] = {n: [r.get("reason") for r in recs if r.get("type") == "end"] for n, recs in run.items()}
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
