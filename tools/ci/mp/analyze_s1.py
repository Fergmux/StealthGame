"""IVP spike S1 observations from a multi-process capture run (story ivp-001; ADR-0008 section 6).

Usage: python -I tools/ci/mp/analyze_s1.py <run dir> [--json <out.json>]

Inputs (written by StealthTestHarness scenario "S1" with -StealthNetCapture):
  server.jsonl   'truth' (pawn NetGUID -> kind: player name or 'npc'), 'event', 'tx' (per-connection send
                 changelists: conn, guid, bNetOwner, parents), 'layout'
  clientN.jsonl  'rx' (received bunches: guid, bits, parents per replicated object, RPCs), 'layout'

Ownership at a receipt is taken from the server truth one emulated one-way delay (+ a frame) before
the receipt; receipts within TRANSITION_S of an ownership change are counted separately (in flight).
Observations only: the spike report judges.
"""
import bisect
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness_records as hr  # noqa: E402

PAWN_CLASS = "BP_CrowdCharacter_C"
IDENTITY = {("Pawn", "PlayerState"), ("Pawn", "Controller"), ("Actor", "Owner")}
REMEASURE = {("Pawn", "RemoteViewPitch16"), ("CBP_SandboxCharacter_C", "CharacterInputState")}
ONE_WAY_S = 0.12
TRANSITION_S = 0.6


class Truth:
    """Server truth timeline (runner 'truth', every 0.1 s): guid -> kind ('C1'.., 'host', 'npc', 'none').

    A client is renamed C<n> only after it logged in, so a player is identified by its PlayerController
    object (stable for the whole run) and the controller is mapped to C<n> from any later snapshot.
    """

    def __init__(self, server):
        self.times = []
        self.snapshots = []
        ctrl_to_client = {}
        raw = []
        for rec in hr.of_type(server, "truth"):
            if rec.get("tag") != "runner":
                continue
            raw.append(rec)
            for p in rec["pawns"]:
                if p["kind"].startswith("C") and p["kind"][1:].isdigit():
                    ctrl_to_client[p["ctrl"]] = p["kind"]
        for rec in raw:
            snap = {}
            for p in rec["pawns"]:
                kind = p["kind"]
                if kind not in ("npc", "none", "host"):
                    kind = ctrl_to_client.get(p["ctrl"], kind)
                snap[p["guid"]] = kind
            self.times.append(rec["t"])
            self.snapshots.append(snap)
        self.changes = collections.defaultdict(list)  # guid -> [t of kind change]
        last = {}
        for t, snap in zip(self.times, self.snapshots):
            for guid, kind in snap.items():
                if guid in last and last[guid] != kind:
                    self.changes[guid].append(t)
                last[guid] = kind

    def kind_at(self, guid, t):
        """Kind in the last snapshot at or before t; a pawn spawned after it takes its first later snapshot."""
        i = bisect.bisect_right(self.times, t) - 1
        if i >= 0 and guid in self.snapshots[i]:
            return self.snapshots[i][guid]
        for j in range(max(i + 1, 0), min(len(self.snapshots), max(i + 1, 0) + 10)):
            if guid in self.snapshots[j]:
                return self.snapshots[j][guid]
        return None

    def near_change(self, guid, t):
        return any(abs(t - c) <= TRANSITION_S for c in self.changes.get(guid, []))

    def kinds_between(self, guid, t0, t1):
        """Every kind the pawn had in snapshots within [t0, t1] (plus the one in force at t0)."""
        out = set()
        first = self.kind_at(guid, t0)
        if first is not None:
            out.add(first)
        lo = bisect.bisect_left(self.times, t0)
        hi = bisect.bisect_right(self.times, t1)
        for i in range(lo, hi):
            k = self.snapshots[i].get(guid)
            if k is not None:
                out.add(k)
        return out

    def ever_seen(self, guid):
        return any(guid in s for s in self.snapshots)


def identity_parents(layout_table):
    out, remeasure = {}, {}
    for idx, (name, _cond) in layout_table.items():
        owner, prop = name.split(".", 1)
        prop = prop.split("[")[0]
        if (owner, prop) in IDENTITY:
            out[idx] = f"{owner}.{prop}"
        if (owner, prop) in REMEASURE:
            remeasure[idx] = f"{owner}.{prop}"
    return out, remeasure


def analyse(run_dir):
    run = hr.load_run(run_dir)
    server = run["server"]
    truth = Truth(server)
    server_layouts = hr.layouts(server)
    result = {"run": os.path.basename(os.path.normpath(run_dir))}

    cfg = hr.of_type(server, "config")
    result["identityTrims"] = cfg[0]["identityTrims"] if cfg else None
    boot = hr.of_type(server, "boot")
    result["serverProfile"] = boot[0]["profile"] if boot else None
    result["iris"] = boot[0]["iris"] if boot else None
    pawn_layout = server_layouts.get(PAWN_CLASS, {})
    result["serverConditions"] = {name: cond for _i, (name, cond) in sorted(pawn_layout.items())
                                  if name.split("[")[0] in ("Pawn.PlayerState", "Pawn.Controller", "Actor.Owner", "Pawn.RemoteViewPitch16",
                                                             "CBP_SandboxCharacter_C.CharacterInputState")}
    ident, remeasure = identity_parents(pawn_layout)
    result["events"] = [{k: v for k, v in e.items() if k in ("ev", "player", "newBody", "formerBody", "st")} for e in hr.of_type(server, "event")]

    # ---- Server send side: per-connection changelists -------------------------------------------
    tx_stats = collections.Counter()
    tx_identity = collections.Counter()
    for rec in hr.of_type(server, "tx"):
        if rec["cls"] != PAWN_CLASS:
            continue
        kind = truth.kind_at(rec["guid"], rec["t"]) or "?"
        pawn_kind = "npc" if kind in ("npc", "none") else ("player" if kind != "?" else "?")
        side = "owner" if rec["own"] == 1 else "nonOwner"
        tx_stats[(side, pawn_kind)] += 1
        for obj in rec["objs"]:
            if not obj["o"].endswith(PAWN_CLASS):
                continue
            for p in obj["p"]:
                if p in ident:
                    tx_identity[(side, pawn_kind, ident[p])] += 1
    result["txBunches"] = {f"{a}/{b}": n for (a, b), n in sorted(tx_stats.items())}
    result["txIdentitySends"] = {f"{a}/{b}/{c}": n for (a, b, c), n in sorted(tx_identity.items())}

    # ---- Client receive side ---------------------------------------------------------------------
    per_client = {}
    union = collections.defaultdict(collections.Counter)    # pawn kind -> parent name -> receipts (non-owner)
    bits = collections.defaultdict(list)                     # pawn kind -> non-open bunch bits (non-owner)
    rpcs = collections.defaultdict(collections.Counter)      # pawn kind -> RPC name -> count (non-owner)
    subobjects = collections.defaultdict(collections.Counter)
    remeasure_counts = collections.Counter()
    for role, recs in run.items():
        if not role.startswith("client"):
            continue
        me = "C" + role[len("client"):]
        layout = hr.layouts(recs).get(PAWN_CLASS, pawn_layout)
        c_ident, c_remeasure = identity_parents(layout)
        stats = collections.Counter()
        violations = []
        transition_ident = collections.Counter()
        for rec in hr.of_type(recs, "rx"):
            if rec["cls"] != PAWN_CLASS:
                continue
            guid = rec["guid"]
            t_send = rec["t"] - ONE_WAY_S
            kind = truth.kind_at(guid, t_send)
            if kind is None:
                stats["unknownGuid"] += 1
                kind = "?"
            in_flight = truth.near_change(guid, t_send)
            window = truth.kinds_between(guid, rec["t"] - TRANSITION_S, rec["t"] + TRANSITION_S)
            # owner: this client owned the pawn at send time; transition: it owned it at some point in the
            # +-TRANSITION_S window around an ownership change (a takeover in flight); else a non-owner.
            owner = kind == me
            transition = (not owner) and in_flight and me in window
            pawn_kind = "npc" if kind in ("npc", "none") else ("player" if kind != "?" else "?")
            side = "owner" if owner else ("transitionOwner" if transition else "nonOwner")
            stats[f"{side}/{pawn_kind}"] += 1
            if transition:
                for obj in rec["objs"]:
                    if obj["o"].endswith(PAWN_CLASS):
                        for p in obj["p"]:
                            if p in c_ident:
                                transition_ident[c_ident[p]] += 1
                continue
            for obj in rec["objs"]:
                is_pawn = obj["o"].endswith(PAWN_CLASS)
                if not is_pawn:
                    if not owner:
                        subobjects[pawn_kind][obj["o"].rsplit(".", 1)[-1]] += 1
                    continue
                for p in obj["p"]:
                    name = layout.get(p, (f"#{p}", ""))[0]
                    if not owner:
                        union[pawn_kind][name] += 1
                    if p in c_remeasure:
                        remeasure_counts[(side, pawn_kind, c_remeasure[p])] += 1
                    if p in c_ident and not owner:
                        violations.append({"t": rec["t"], "guid": guid, "kind": kind, "prop": c_ident[p], "inFlight": in_flight, "open": rec["open"]})
            if not owner:
                if not rec["open"]:
                    bits[pawn_kind].append(rec["bits"])
                for r in rec["rpc"]:
                    rpcs[pawn_kind][r] += 1
        per_client[me] = {
            "receipts": dict(stats),
            "identityToNonOwner": len(violations),
            "identityToNonOwnerSteady": sum(1 for v in violations if not v["inFlight"]),
            "identityToTransitionOwner": dict(transition_ident),
            "identityByProp": dict(collections.Counter(v["prop"] for v in violations)),
            "identityExamples": violations[:5],
        }
    result["clients"] = per_client
    result["nonOwnerParentUnion"] = {k: dict(sorted(v.items())) for k, v in union.items()}
    result["nonOwnerBunchBits"] = {k: hr.quantiles(v) for k, v in bits.items()}
    result["nonOwnerRpcs"] = {k: dict(v) for k, v in rpcs.items()}
    result["nonOwnerSubobjects"] = {k: dict(v) for k, v in subobjects.items()}
    result["remeasure"] = {f"{a}/{b}/{c}": n for (a, b, c), n in sorted(remeasure_counts.items())}
    return result


def main():
    args = sys.argv[1:]
    out_json = None
    if "--json" in args:
        i = args.index("--json")
        out_json = args[i + 1]
        del args[i:i + 2]
    result = analyse(args[0])
    text = json.dumps(result, indent=1, sort_keys=False)
    print(text)
    if out_json:
        with open(out_json, "w", encoding="utf-8") as handle:
            handle.write(text + "\n")


if __name__ == "__main__":
    main()
