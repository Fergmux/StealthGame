"""Session-travel spike S3 (story 003) analyser: Saved/Harness/<run>/ records of scenario "S3".

Observations only (the .claude/scripts rule): per travel, whether each game-instance store's content equals its
pre-travel content at the round's first tick, whether every expected participant acknowledged its owner-only team
view with the server's hash before that tick, the server map load (T_srvload) and each client's load, the
controller/PlayerState replacement (E5), the arrival order (E19), carried PlayerStates (E21) and their adoption,
engine localized messages and player-array changes on clients. The spike report judges.

Usage: python -I tools/ci/mp/analyze_s3.py Saved/Harness/<run> [--json out.json]
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402


def main(run_dir, out_json=None):
    run = hr.load_run(run_dir)
    server = run["server"]
    clients = {k: v for k, v in run.items() if k.startswith("client")}
    stores = {r["tag"]: r for r in hr.of_type(server, "stores")}
    travels = hr.of_type(server, "serverTravel")
    ticks = {r["n"]: r for r in hr.of_type(server, "roundTick")}
    post_seamless = hr.of_type(server, "postSeamlessTravel")
    arrived = {r["n"]: r for r in hr.of_type(server, "event") if r.get("ev") == "arrived"}
    binds = hr.of_type(server, "bind")
    result = {"run": os.path.basename(run_dir.rstrip("/\\")), "travels": []}
    for travel in travels:
        n = travel["n"]
        pre = stores.get(f"pre{n}")
        post = stores.get(f"post{n}")
        tick = ticks.get(n)
        entry = {"n": n, "seamless": bool(travel["seamless"])}
        if pre and post:
            entry["storesEqual"] = {name: pre[name]["content"] == post[name]["content"] for name in ("session", "roster", "round", "tuning")}
            entry["storeDiffs"] = {name: [pre[name]["content"], post[name]["content"]]
                                   for name in ("session", "roster", "round", "tuning") if pre[name]["content"] != post[name]["content"]}
        if tick:
            entry["allAckedBeforeFirstTick"] = bool(tick["allAcked"]) and not tick.get("ceiling", 0)
            entry["expectedParticipants"] = tick.get("expected")
            entry["acks"] = tick.get("acks")
        # Server load: ServerTravel -> PostSeamlessTravel (seamless) or -> arrival (hard).
        later_post = [r for r in post_seamless if r["t"] > travel["t"]]
        end_load = later_post[0]["t"] if travel["seamless"] and later_post else (arrived[n]["t"] if n in arrived else None)
        if end_load:
            entry["T_srvload_s"] = round(end_load - travel["t"], 3)
        if tick:
            entry["travelToFirstTick_s"] = round(tick["t"] - travel["t"], 3)
        # E5: controllers and PlayerStates bound after this travel vs before.
        next_t = min([t["t"] for t in travels if t["t"] > travel["t"]] or [float("inf")])
        before = [b for b in binds if b["t"] < travel["t"]]
        after = [b for b in binds if travel["t"] < b["t"] < next_t]
        last_before = {}
        for b in before:
            last_before[b["id"]] = b
        entry["rebinds"] = [{"id": b["id"], "pcReplaced": last_before.get(b["id"], {}).get("pc") != b["pc"],
                             "psReplaced": last_before.get(b["id"], {}).get("ps") != b["ps"], "adopted": bool(b["adopted"]),
                             "dt_s": round(b["t"] - travel["t"], 3)} for b in after]
        # E19: host re-bound (PostSeamlessTravel) before any remote arrival.
        host_bind = [b for b in after if b["key"] == "host"]
        remote_bind = [b for b in after if b["key"] != "host"]
        if host_bind and remote_bind:
            entry["hostBoundFirst"] = host_bind[0]["t"] <= min(b["t"] for b in remote_bind)
        # Client loads: clientTravel -> first ackSent after it.
        loads = {}
        for name, recs in clients.items():
            # Early runs wrote the travel type as a second "type" key (it overwrote the record type): accept both.
            starts = [r for r in recs if (r.get("type") == "clientTravel" or ("seamless" in r and isinstance(r.get("type"), int)))
                      and abs(r["t"] - travel["t"]) < 5.0]
            if starts:
                acks = [r for r in hr.of_type(recs, "ackSent") if r["t"] > starts[0]["t"]]
                if acks:
                    loads[name] = round(acks[0]["t"] - starts[0]["t"], 3)
        entry["clientTravelToAck_s"] = loads
        result["travels"].append(entry)
    result["carriedPlayerStates"] = [{"guid": r["guid"], "fromPrevious": r["fromPrevious"]} for r in hr.of_type(server, "carriedPlayerState")]
    result["adoptions"] = [{"guid": r["guid"], "pc": r["pc"]} for r in hr.of_type(server, "psAdopted")]
    result["retained"] = [r["guid"] for r in hr.of_type(server, "psRetained")]
    result["engineMessagesOnClients"] = {name: len(hr.of_type(recs, "localizedMessage")) for name, recs in clients.items()}
    result["clientEnds"] = {name: [r.get("reason") for r in hr.of_type(recs, "end")] for name, recs in clients.items()}
    result["serverEnd"] = [r.get("reason") for r in hr.of_type(server, "end")]
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
