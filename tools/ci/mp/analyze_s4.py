"""Session-travel spike S4 (story 004) analyser: Saved/Harness/<run>/ records of scenario "S4" with -Capture.

Observations only: the rejoiner (client1, IdentityKey slot 1) leaves mid-round and rejoins. For every non-owner
client the raw-bunch capture is read in the rejoin window [drop - 0.5 s, rejoin connected + 5 s]: PlayerState
actor-channel open/close bunches for the rejoiner's PlayerState NetGUID (and for any PlayerState), the PlayerState
properties received (against a baseline window of equal length before the drop), player-array changes,
engine localized messages (RPC capture and the client-side override) over the whole run; on the server, whether
the retained PlayerState was adopted (same NetGUID) and how many supersedes happened before and after the control.
The spike report judges.

Usage: python -I tools/ci/mp/analyze_s4.py Saved/Harness/<run> [--json out.json]
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402


def events(records, name):
    return [r for r in hr.of_type(records, "event") if r.get("ev") == name]


def decode(layouts, objs):
    names = []
    for obj in objs:
        cls = obj.get("o", "").split(".")[-1]
        table = layouts.get(cls, {})
        for parent in obj.get("p", []):
            names.append(table.get(parent, (f"{cls}#{parent}", "?"))[0])
    return names


def main(run_dir, out_json=None):
    run = hr.load_run(run_dir)
    server = run["server"]
    c1 = run["client1"]
    drop = events(c1, "c1Drop")
    back = events(c1, "c1RejoinConnected")
    issued = events(c1, "c1RejoinIssued")
    result = {"run": os.path.basename(run_dir.rstrip("/\\"))}
    binds = [b for b in hr.of_type(server, "bind") if b["key"].startswith("00000001")]
    result["rejoinerBinds"] = [{"psGuid": b["psGuid"], "adopted": b["adopted"], "pc": b["pc"], "t": b["t"]} for b in binds]
    original_guid = binds[0]["psGuid"] if binds else None
    result["adoptions"] = [{"guid": r["guid"], "pc": r["pc"]} for r in hr.of_type(server, "psAdopted")]
    result["retained"] = [r["guid"] for r in hr.of_type(server, "psRetained")]
    result["adoptedSameGuid"] = any(r["guid"] == original_guid for r in hr.of_type(server, "psAdopted"))
    control = events(run.get("client3", []), "c3SupersedeIssued")
    supersedes = hr.of_type(server, "supersede")
    control_t = control[0]["t"] if control else float("inf")
    result["supersedesBeforeControl"] = len([s for s in supersedes if s["t"] < control_t])
    result["supersedesAfterControl"] = len([s for s in supersedes if s["t"] >= control_t])
    if issued and back:
        result["t_connect_s"] = round(back[0].get("sinceIssue", back[0]["t"] - issued[0]["t"]), 3)
    if not drop or not back:
        result["error"] = "rejoin did not complete"
        print(json.dumps(result, indent=1))
        return
    w0, w1 = drop[0]["t"] - 0.5, back[0]["t"] + 5.0
    b0, b1 = w0 - (w1 - w0), w0
    result["window_s"] = [round(w0, 3), round(w1, 3)]
    observers = {}
    for name in ("client2", "client3"):
        recs = run.get(name, [])
        layouts = hr.layouts(recs)
        rx = hr.of_type(recs, "rx")

        def in_window(r, a, b):
            return a <= r["t"] <= b

        ps_rx = [r for r in rx if "PlayerState" in r.get("cls", "")]
        target = [r for r in ps_rx if r.get("guid") == original_guid]
        obs = {
            "rejoinerPsOpenClose": len([r for r in target if in_window(r, w0, w1) and (r["open"] or r["close"])]),
            "anyPsOpenClose": len([r for r in ps_rx if in_window(r, w0, w1) and (r["open"] or r["close"])]),
            "rejoinerPsBunches": len([r for r in target if in_window(r, w0, w1)]),
            "rejoinerPsBunchesBaseline": len([r for r in target if in_window(r, b0, b1)]),
            "rejoinerPsProps": sorted(set(p for r in target if in_window(r, w0, w1) for p in decode(layouts, r.get("objs", [])))),
            "rejoinerPsPropsBaseline": sorted(set(p for r in target if in_window(r, b0, b1) for p in decode(layouts, r.get("objs", [])))),
            "playerArrayChangesInWindow": len([r for r in hr.of_type(recs, "playerArray") if in_window(r, w0, w1)]),
            "playerArrayChangesWholeRunAfterFirst": max(0, len([r for r in hr.of_type(recs, "playerArray") if r["t"] < control_t]) - 1),
            "localizedMessageRpcs": len([r for r in rx for rpc in r.get("rpc", []) if "ClientReceiveLocalizedMessage" in rpc]),
            "localizedMessageOverride": len(hr.of_type(recs, "localizedMessage")),
            "rxBunches": len(rx),
        }
        observers[name] = obs
    result["nonOwnerObservers"] = observers
    result["ownerLocalizedMessages"] = len(hr.of_type(c1, "localizedMessage"))
    result["clientEnds"] = {n: [r.get("reason") for r in hr.of_type(run.get(n, []), "end")] for n in ("client1", "client2", "client3")}
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
