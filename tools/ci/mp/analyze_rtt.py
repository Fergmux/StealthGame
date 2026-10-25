"""RTT observations for net-emulation profile runs (story tdc-017; ADR-0008 section 5).

Usage: python -I tools/ci/mp/analyze_rtt.py <run dir> [<run dir> ...]

Per run: the packet-simulation settings each process actually applied (boot record, read back from its
game net driver), the profile load check (LoadEmulationProfile by name, plus a missing-name control),
and the engine-reported round-trip time after a 5 s warm-up: UNetConnection::AvgLag and
APlayerState::ExactPing, on the client (server connection) and on the server (client connection).
Observations only; the smoke record states the result.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402

WARMUP_S = 5.0


def summarise(run_dir):
    run = hr.load_run(run_dir)
    print("== " + os.path.basename(os.path.normpath(run_dir)))
    for role, recs in run.items():
        boot = hr.of_type(recs, "boot")
        if boot:
            b = boot[0]
            print(f"  {role}: profile='{b['profile']}' applied PktLag={b['PktLag']} PktLagVariance={b['PktLagVariance']} "
                  f"PktLoss={b['PktLoss']} iris={b['iris']} driver={b['driver']}")
    checks = hr.of_type(run.get("server", []), "profileCheck")
    for c in checks:
        print(f"  load check: {c['profile']}: loaded={c['loaded']} PktLag={c['PktLag']} PktLagVariance={c['PktLagVariance']} PktLoss={c['PktLoss']}")
    for role, recs in run.items():
        nets = hr.of_type(recs, "net")
        start = None
        if role == "server":
            joined = hr.of_type(recs, "allJoined")
            start = joined[0]["t"] if joined else None
        else:
            conn = hr.of_type(recs, "connected")
            start = conn[0]["t"] if conn else None
        if start is None:
            print(f"  {role}: never connected")
            continue
        lag, ping, fps = [], [], []
        for n in nets:
            if n["t"] < start + WARMUP_S:
                continue
            fps.append(n["fps"])
            for c in n["conns"]:
                if c["avgLagMs"] > 0:
                    lag.append(c["avgLagMs"])
                if c["exactPingMs"] > 0:
                    ping.append(c["exactPingMs"])
        print(f"  {role}: AvgLag RTT ms {hr.fmt_q(hr.quantiles(lag))}")
        print(f"  {role}: ExactPing ms {hr.fmt_q(hr.quantiles(ping))}")
        print(f"  {role}: fps {hr.fmt_q(hr.quantiles(fps))}")


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        summarise(arg)
