"""IVP spike S9 observations (story ivp-004; ADR-0003 sections 5.3-5.5, E2/E10/E15).

Usage: python -I tools/ci/mp/analyze_s9.py <run dir> [--json <out.json>]

Reads the harness records of scenario "S9": the server's cull commit, cullClose (per subject and denied
connection, with the server frame), channelPresentAfterCull / channelGone / channelReappeared (per-frame
FindActorChannelRef monitor), forceRelevant, subjectSummary, bases (L27 forced relations) and e15; each
client's pawnDestroy / pawnGuid (the moment the subject left or re-entered that client). Cross-process
times use the shared QueryPerformanceCounter clock (record field t). Observations only.
"""
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402


def analyse(run_dir):
    run = hr.load_run(run_dir)
    server = run["server"]
    out = {"run": os.path.basename(os.path.normpath(run_dir))}
    boot = hr.of_type(server, "boot")
    out["profile"] = boot[0]["profile"] if boot else None
    out["iris"] = boot[0]["iris"] if boot else None
    e15 = hr.of_type(server, "e15")
    out["e15"] = {k: v for k, v in e15[0].items() if k not in ("t", "type")} if e15 else None
    viewers = {v["conn"]: v["team"] for v in hr.of_type(server, "viewer")}
    out["viewerTeams"] = viewers
    subjects = {s["guid"]: s for s in hr.of_type(server, "subject")}
    out["subjects"] = {g: {"label": s["label"], "team": s["team"], "denied": s["denied"], "allowed": s["allowed"]} for g, s in subjects.items()}

    commit = hr.of_type(server, "cullCommit")[0]
    period = 1.0 / round(1.0 / commit["period"])  # the record prints 1/15 rounded to 6 decimals
    k = (commit["deadline"] - commit["gridOrigin"]) / period
    out["grid"] = {
        "commit": commit["commit"], "deadline": commit["deadline"], "gridOrigin": commit["gridOrigin"], "period": period,
        "deadlineIsGridPoint": abs(k - round(k)) < 1e-4,  # 6-decimal records
        "deadlineMinusEarliest": commit["deadline"] - (commit["commit"] + 0.25),
        "firstGridPointAtOrAfterEarliest": 0 <= commit["deadline"] - (commit["commit"] + 0.25) < period + 1e-9,
    }

    closes = hr.of_type(server, "cullClose")
    frames = sorted({c["f"] for c in closes})
    out["closes"] = {
        "count": len(closes),
        "explicit": sum(1 for c in closes if c.get("explicitClose", 1)),
        "hadChannel": sum(1 for c in closes if c["hadChannel"]),
        "serverFrames": frames,
        "lateByMaxS": max(c["lateBy"] for c in closes),
        "frameDeltaMaxS": max(c["frameDelta"] for c in closes),
        "allLandWithinOneFrame": all(0 <= c["lateBy"] <= c["frameDelta"] + 1e-9 for c in closes),
    }
    close_t = {}
    for c in closes:
        close_t[(c["subject"], c["conn"])] = c["t"]

    # Client-side loss: pawnDestroy of the subject on each denied client, relative to the server close.
    loss = []
    reentries = []
    allowed_lost = []
    for role, recs in run.items():
        if not role.startswith("client"):
            continue
        me = "C" + role[len("client"):]
        destroyed = {}
        for d in hr.of_type(recs, "pawnDestroy"):
            if d["guid"] in subjects and d["guid"] not in destroyed:
                destroyed[d["guid"]] = d["t"]
        for guid, subj in subjects.items():
            key = (guid, me)
            if key in close_t:
                if guid in destroyed:
                    loss.append({"client": me, "label": subj["label"], "ms": (destroyed[guid] - close_t[key]) * 1000.0})
                else:
                    loss.append({"client": me, "label": subj["label"], "ms": None})
            elif guid in destroyed and destroyed[guid] > commit["t"]:
                allowed_lost.append({"client": me, "label": subj["label"]})
        for g in hr.of_type(recs, "pawnGuid"):
            if g["guid"] in destroyed and g["t"] > destroyed[g["guid"]]:
                reentries.append({"client": me, "guid": g["guid"], "label": subjects[g["guid"]]["label"]})
    explicit_labels = {s["label"] for s in subjects.values() if s["label"] != "controlOverrideOnly"}
    out["clientLossMs"] = {
        "explicit": hr.quantiles([l["ms"] for l in loss if l["ms"] is not None and l["label"] in explicit_labels]),
        "control": hr.quantiles([l["ms"] for l in loss if l["ms"] is not None and l["label"] not in explicit_labels]),
        "neverLost": [l for l in loss if l["ms"] is None],
    }
    out["allowedClientsThatLostTheSubject"] = allowed_lost
    out["clientReentries"] = reentries

    gone = hr.of_type(server, "channelGone")
    out["channelGoneSinceDeadlineS"] = {
        "explicit": hr.quantiles([g["sinceDeadline"] for g in gone if g["label"] in explicit_labels]),
        "control": hr.quantiles([g["sinceDeadline"] for g in gone if g["label"] not in explicit_labels]),
    }
    out["channelReappeared"] = [{k: r[k] for k in ("label", "conn", "st")} for r in hr.of_type(server, "channelReappeared")]
    out["forceRelevantRequests"] = len(hr.of_type(server, "forceRelevant"))
    summaries = hr.of_type(server, "subjectSummary")
    out["summary"] = [{"label": s["label"], "explicitClose": s["explicitClose"], "monitoredFrames": s["monitoredFrames"],
                       "deniedFramesWithChannel": {d["conn"]: d["framesWithChannel"] for d in s["denied"]},
                       "allowedFramesMissing": {a["conn"]: a["framesMissing"] for a in s["allowed"]}} for s in summaries]
    out["observedSecondsAfterDeadline"] = (summaries[0]["st"] - commit["deadline"]) if summaries else None

    forced = collections.defaultdict(lambda: [0, 0])
    for b in hr.of_type(server, "bases"):
        for f in b.get("forced", []):
            if f["active"]:
                forced[f["label"]][0] += 1
                forced[f["label"]][1] += f["topBasedOnBottom"]
    out["l27"] = {label: {"activeSamples": n, "basedSamples": based} for label, (n, based) in forced.items()}
    out["events"] = [{k: e[k] for k in e if k in ("ev", "label", "top", "bottom", "st")} for e in hr.of_type(server, "event")]
    nets = [n["fps"] for n in hr.of_type(server, "net") if n["fps"] > 0]
    out["serverFps"] = hr.quantiles(nets)
    return out


def main():
    args = sys.argv[1:]
    out_json = None
    if "--json" in args:
        i = args.index("--json")
        out_json = args[i + 1]
        del args[i:i + 2]
    result = analyse(args[0])
    text = json.dumps(result, indent=1)
    print(text)
    if out_json:
        with open(out_json, "w", encoding="utf-8") as handle:
            handle.write(text + "\n")


if __name__ == "__main__":
    main()
