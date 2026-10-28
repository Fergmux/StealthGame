#!/usr/bin/env python3
"""NPC-kill penalty and informant telemetry report (AC-NKP-23, story nkp-022; ADR-0019 section 7, ADR-0023 sections 6-7).

Offline only: reads exported, terminated rounds' PostRoundEligible Event Log rows (one JSON file per round, written by the
Event Log store after termination, ADR-0023 section 6) plus an optional observer log, and prints a Markdown report. It emits
observations, never verdicts: budgets and planning values are printed beside the measured values for the creative director
and game designer to read; nothing here changes a tuning value, fails a build or blocks a story. It gates nothing: the exit
code is 0 whenever a report is written. Standard library only. It never runs inside a round.

Path decision: the story's Files list names tools/telemetry/penalty_report.py "or the commandlet named by the ADR-0023
stories"; the project's offline analysis tools live in tools/analysis/ (next to assassination_telemetry_report.py), so this
tool is tools/analysis/penalty_telemetry_report.py. The AC2 evidence is this tool's own --self-test (no C++ aggregation).

Input schema. ADR-0023 section 6 does not spell out the Event Log row block of the export, so this tool defines it as
SchemaVersion 1 in the same style as the assassination tool (a subset the exporter must provide; extra fields are ignored).
One round file:
  {
    "SchemaVersion": 1,
    "MatchId": "<opaque>",              # rounds sharing it are one match (default: the file is its own match)
    "RoundIndex": 1,
    "EndReason": "Elimination",         # a round without one (or "Active"/"Preparing") is not terminated: skipped
    "StrikeLimit": 3,                   # L (NpcKillStrikeLimit) of THIS round; rounds with different L are handled per round
    "NpcCount": 50,                     # N (F4)
    "InformantCountMean": 2.0,          # I: mean embodied enemy informants over the round (exporter derives it from the
                                        #   StateTransition rows' InformantCountAfter, D5). q = I / (I + N) is the informant
                                        #   share of the population (F4/F5 "q"); this field is where q comes from.
    "Regions": {                        # optional, OQ-NKP-13: exporter samples at each respawn pick
      "RespawnRegion": {"Informants": 3, "Pawns": 10}, "CityWide": {"Informants": 2, "Pawns": 52} },
    "EventLogRows": [ {row}, ... ]
  }
A row is one Round Event Log entry decoded into named fields. Common: "Type" (ERoundLogType name), "ServerTime",
"Participant" (Participants[0]), "PostRoundEligible" (bool, FRoundEventLog::GetReleaseClass == PostRoundEligible). Rows
that are not exactly true are ignored (a missing flag counts as not eligible). Per type, the D0..Dn mapping from the real
headers (StealthPenaltyEventRows.h, StealthRoundLogTypes.h):
  StrikeCounted     D0 -> "StrikeAfter", D1 -> "TallyAfter", D2 -> "Graced" (0/1 -> bool). Optional exporter join:
                    "VictimReleased" (bool, from the ReleasedToCrowd rows: the victim was a released body)
  RespawnScheduled  Code ERespawnCause -> "Cause"; D0-D1 -> "LostServerTime" (t_lost), D2-D3 -> "DueServerTime" (T_due)
  InformantRespawn  D0-D1 -> "PickLatencySec" (L_pick), D2 -> "WideningLevel"
  RespawnCancelled  Code ERespawnCancelReason -> "Reason"
  PawnLost          Code EPenaltyPawnLostCause -> "Cause"
  StrikeLimit       one row per exile (Round writes it with the transition)
  RespawnStarved    one row per starvation (LSC writes it)
  KillResolved      Code ERoundKillResult -> "Result" (ParticipantKilled | WrongTarget | Other); D1 victim kind ->
                    "VictimKind" (Player | Npc | Informant, EVictimKind); P0/P1 -> "Attacker"/"Victim"; D0 -> "AttackerTeam"
Observer log (separate file; body blocking has no Event Log row, OQ-NKP-3), recognised by "Kind": "ObserverLog":
  { "SchemaVersion": 1, "Kind": "ObserverLog",
    "Groups": [ {"GroupId": "g1", "MatchIds": ["match-a"], "Voice": true} ],          # OQ-NKP-8: which groups had voice
    "BodyBlocking": [ {"MatchId": "match-a", "GroupId": "g1", "PawnKind": "Informant" | "Released",
                       "PassageWidthPawns": 1, "ObstructedPlayerPath": true} ] }        # one entry per observer-form entry
An incident counts when PassageWidthPawns <= 1 and ObstructedPlayerPath is true (AC1 definition); all entries are listed.

Definitions (GDD F3-F5): T_black = (T_due - t_lost) + PickLatencySec, a RespawnScheduled paired with the next
InformantRespawn of the same participant in the same round (a RespawnCancelled drops the pending one). Median is the usual
median, p95 the nearest-rank value. F4: per round, a participant "reached tally L" with any StrikeCounted TallyAfter >= L;
the residual is such a participant whose largest StrikeAfter stayed < L (no exile). Observed share = residual / reached, an
estimate of P_res; b is solved from P_res = 1 - (1 - q)^L with q = b*I/(b*I + N), using reached-weighted I and N. F5: the
informant share of KillResolved Result == WrongTarget victims, against 2 x q with q the WrongTarget-weighted round q.
Stop reason: at least 200 InformantRespawn rows and 30 StrikeLimit rows, or 40 matches, whichever comes first; a reading
before that (the checkpoint at about 10 matches) is reported but not judged.

Usage:
  python tools/analysis/penalty_telemetry_report.py <round.json, observer.json or folder> [...] [--out report.md]
  python tools/analysis/penalty_telemetry_report.py --self-test
Exit: 0 report written (or self-test passed), 1 self-test mismatch, 2 no terminated round found / bad input.
"""
import argparse
import json
import math
import os
import sys

RESPAWN_TARGET = 200
EXILE_TARGET = 30
MATCH_CAP = 40
BUDGET_MEDIAN_SEC = 30.0
BUDGET_P95_SEC = 40.0
ESCALATION_RATIO = 2.0
ESCALATION_MATCHES = 10
UNTERMINATED = {"", "Active", "Preparing", None}
HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURE_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "tests", "fixtures", "nkp", "telemetry"))


def load_inputs(paths):
    """Terminated round dicts, observer-log dicts and skipped file names, in a stable order."""
    files = []
    for path in paths:
        if os.path.isdir(path):
            files.extend(sorted(os.path.join(path, n) for n in os.listdir(path) if n.endswith(".json")))
        else:
            files.append(path)
    rounds, observers, skipped = [], [], []
    for path in files:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        if data.get("SchemaVersion") != 1:
            raise ValueError("%s: unsupported SchemaVersion %r" % (path, data.get("SchemaVersion")))
        if data.get("Kind") == "ObserverLog":
            observers.append(data)
            continue
        if data.get("EndReason") in UNTERMINATED:
            skipped.append(os.path.basename(path))
            continue
        data["_file"] = os.path.basename(path)
        rounds.append(data)
    return rounds, observers, skipped


def median(values):
    s = sorted(values)
    n = len(s)
    return s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2.0


def p95(values):
    s = sorted(values)
    return s[max(0, math.ceil(0.95 * len(s)) - 1)]


def p_res(q, l):
    return 1.0 - (1.0 - q) ** l


def aggregate(rounds, observers):
    """All counts of the report as a plain dict (the self-test asserts on it)."""
    a = {"rounds": len(rounds), "matches": len({r.get("MatchId") or r["_file"] for r in rounds}),
         "rows_total": 0, "rows_ignored": 0, "respawns": 0, "tblack": [], "unpaired": 0, "cancelled": 0,
         "pawn_lost": 0, "starved": 0, "exiles": 0, "strike_rows": 0, "graced": 0, "after_grace_released": 0,
         "f4": {}, "wrong": 0, "wrong_informant": 0, "wrong_q_sum": 0.0,
         "region_rounds": 0, "region_inf": 0, "region_pawns": 0, "city_inf": 0, "city_pawns": 0}
    for r in rounds:
        l, n, inf = r.get("StrikeLimit"), r.get("NpcCount", 0), float(r.get("InformantCountMean", 0.0))
        q = inf / (inf + n) if (inf + n) > 0 else 0.0
        rows = r.get("EventLogRows", [])
        a["rows_total"] += len(rows)
        eligible = [(row.get("ServerTime", 0.0), i, row) for i, row in enumerate(rows) if row.get("PostRoundEligible") is True]
        a["rows_ignored"] += len(rows) - len(eligible)
        pending, players = {}, {}
        for _, _, row in sorted(eligible, key=lambda t: (t[0], t[1])):
            kind, who = row.get("Type"), row.get("Participant")
            if kind == "RespawnScheduled":
                pending[who] = row
            elif kind == "RespawnCancelled":
                a["cancelled"] += 1
                pending.pop(who, None)
            elif kind == "InformantRespawn":
                a["respawns"] += 1
                sched = pending.pop(who, None)
                if sched is None:
                    a["unpaired"] += 1
                else:
                    a["tblack"].append((sched["DueServerTime"] - sched["LostServerTime"]) + row["PickLatencySec"])
            elif kind == "PawnLost":
                a["pawn_lost"] += 1
            elif kind == "RespawnStarved":
                a["starved"] += 1
            elif kind == "StrikeLimit":
                a["exiles"] += 1
            elif kind == "StrikeCounted":
                a["strike_rows"] += 1
                p = players.setdefault(who, {"reached": False, "max_strike": 0})
                if row["TallyAfter"] >= l:
                    p["reached"] = True
                p["max_strike"] = max(p["max_strike"], row["StrikeAfter"])
                if row.get("Graced"):
                    a["graced"] += 1
                elif row.get("VictimReleased"):
                    a["after_grace_released"] += 1
            elif kind == "KillResolved" and row.get("Result") == "WrongTarget":
                a["wrong"] += 1
                a["wrong_q_sum"] += q
                if row.get("VictimKind") == "Informant":
                    a["wrong_informant"] += 1
        reached = [p for p in players.values() if p["reached"]]
        if reached:
            e = a["f4"].setdefault(l, {"reached": 0, "residual": 0, "i_sum": 0.0, "n_sum": 0.0})
            e["reached"] += len(reached)
            e["residual"] += sum(1 for p in reached if p["max_strike"] < l)
            e["i_sum"] += inf * len(reached)
            e["n_sum"] += n * len(reached)
        reg = r.get("Regions")
        if reg:
            a["region_rounds"] += 1
            a["region_inf"] += reg["RespawnRegion"]["Informants"]
            a["region_pawns"] += reg["RespawnRegion"]["Pawns"]
            a["city_inf"] += reg["CityWide"]["Informants"]
            a["city_pawns"] += reg["CityWide"]["Pawns"]
    groups, entries = {}, []
    for o in observers:
        for g in o.get("Groups", []):
            groups[g["GroupId"]] = bool(g.get("Voice"))
        entries.extend(o.get("BodyBlocking", []))
    a["observer_files"] = len(observers)
    a["groups"] = len(groups)
    a["groups_voice"] = sum(1 for v in groups.values() if v)
    a["bb_entries"] = len(entries)
    a["bb_incidents"] = sum(1 for e in entries if e.get("PassageWidthPawns", 99) <= 1 and e.get("ObstructedPlayerPath") is True)
    return a


def f4_estimate(e, l):
    """(observed share, weighted I, weighted N, b estimate or None, planning P_res at b=1, at b=3)."""
    share = e["residual"] / e["reached"]
    iw, nw = e["i_sum"] / e["reached"], e["n_sum"] / e["reached"]
    b_hat = None
    if 0.0 < share < 1.0 and iw > 0.0:
        q_hat = 1.0 - (1.0 - share) ** (1.0 / l)
        b_hat = (q_hat * nw) / ((1.0 - q_hat) * iw)
    plan = [p_res(b * iw / (b * iw + nw), l) if (b * iw + nw) > 0 else 0.0 for b in (1.0, 3.0)]
    return share, iw, nw, b_hat, plan[0], plan[1]


def ratio_text(value):
    return "%.2f" % value if value is not None else "n/a"


def report(a, skipped):
    """The Markdown report (deterministic for a given input)."""
    L = ["# NPC-kill penalty and informant telemetry (AC-NKP-23)", "",
         "Advisory, report-only: observations beside reference values, no gate, nothing fails on these numbers.", "",
         "Input: %d terminated round(s) in %d match(es); %d Event Log row(s) read, %d ignored (not PostRoundEligible)."
         % (a["rounds"], a["matches"], a["rows_total"] - a["rows_ignored"], a["rows_ignored"])]
    if skipped:
        L.append("Skipped (not terminated): " + ", ".join(sorted(skipped)) + ".")
    L += ["", "## Stop condition", ""]
    reached = a["respawns"] >= RESPAWN_TARGET and a["exiles"] >= EXILE_TARGET
    if reached:
        why = "stop: %d informant respawns >= %d and %d exiles >= %d" % (a["respawns"], RESPAWN_TARGET, a["exiles"], EXILE_TARGET)
    elif a["matches"] >= MATCH_CAP:
        why = "stop: match cap %d reached" % MATCH_CAP
    else:
        why = "not reached: keep playing (checkpoint reading near 10 matches is reported, not judged)"
    L.append("n: %d match(es), %d informant respawn(s) (target %d), %d exile(s) (target %d), cap %d matches. Stop reason: %s."
             % (a["matches"], a["respawns"], RESPAWN_TARGET, a["exiles"], EXILE_TARGET, MATCH_CAP, why))

    L += ["", "## F3 downtime: T_black", ""]
    t = a["tblack"]
    if t:
        L.append("n = %d paired respawn(s) (%d InformantRespawn row(s) without a RespawnScheduled; %d RespawnCancelled; %d PawnLost; %d RespawnStarved)."
                 % (len(t), a["unpaired"], a["cancelled"], a["pawn_lost"], a["starved"]))
        L.append("- median T_black: %.2f s (budget <= %.0f s)" % (median(t), BUDGET_MEDIAN_SEC))
        L.append("- p95 T_black: %.2f s (budget <= %.0f s)" % (p95(t), BUDGET_P95_SEC))
    else:
        L.append("n = 0 paired respawns (%d InformantRespawn row(s) without a RespawnScheduled; %d RespawnCancelled; %d PawnLost; %d RespawnStarved): no T_black." %
                 (a["unpaired"], a["cancelled"], a["pawn_lost"], a["starved"]))

    L += ["", "## F4 residual: tally >= L without exile (estimates b)", ""]
    if a["f4"]:
        L += ["| L | Players reaching tally L (n) | Without exile | Observed share | Mean I | Planning P_res at b=1 | at b=3 | Estimated b |",
              "|---|---|---|---|---|---|---|---|"]
        for l in sorted(a["f4"]):
            e = a["f4"][l]
            share, iw, nw, b_hat, p1, p3 = f4_estimate(e, l)
            L.append("| %d | %d | %d | %.3f | %.2f | %.3f | %.3f | %s |" % (l, e["reached"], e["residual"], share, iw, p1, p3, ratio_text(b_hat)))
        L.append("")
        L.append("Per-round L; b is n/a when the share is 0 or 1 or mean I is 0. Graced bodies add to q, so b is an upper estimate of informants alone.")
    else:
        L.append("n = 0 players reached tally L.")

    L += ["", "## F5 hunt: informant share of WrongTarget victims", ""]
    if a["wrong"]:
        share = a["wrong_informant"] / a["wrong"]
        q = a["wrong_q_sum"] / a["wrong"]
        L.append("n = %d WrongTarget victim(s), %d informant(s): share %.3f against 2 x q = %.3f (q = %.3f, informant share of the population)."
                 % (a["wrong"], a["wrong_informant"], share, 2 * q, q))
        L.append("Observation: the share is %s 2 x q." % ("above" if share > 2 * q else "not above"))
    else:
        L.append("n = 0 WrongTarget victims.")

    L += ["", "## Released bodies: graced and after-grace kills", ""]
    L.append("n = %d StrikeCounted row(s): %d graced kill(s) of a released body, %d after-grace kill(s) of a released body."
             % (a["strike_rows"], a["graced"], a["after_grace_released"]))
    L.append("If after-grace kills are frequent the exit timing is reviewed, never the grace length (OQ-NKP-2/3).")

    L += ["", "## Respawn-region informant share (OQ-NKP-13)", ""]
    if a["region_rounds"] and a["region_pawns"] and a["city_pawns"] and a["city_inf"]:
        rs, cs = a["region_inf"] / a["region_pawns"], a["city_inf"] / a["city_pawns"]
        ratio = rs / cs
        L.append("n = %d round(s) with region samples: respawn region %.3f (%d of %d), city-wide %.3f (%d of %d), ratio %.2f."
                 % (a["region_rounds"], rs, a["region_inf"], a["region_pawns"], cs, a["city_inf"], a["city_pawns"], ratio))
        if ratio > ESCALATION_RATIO and a["matches"] >= ESCALATION_MATCHES:
            L.append("Observation: ratio > %.0f on >= %d matches (%d): the GDD's escalation to the creative director applies." % (ESCALATION_RATIO, ESCALATION_MATCHES, a["matches"]))
        else:
            L.append("Observation: escalation condition (ratio > %.0f on >= %d matches) not met (ratio %.2f, %d match(es))." % (ESCALATION_RATIO, ESCALATION_MATCHES, ratio, a["matches"]))
    else:
        L.append("n = 0 round(s) with usable region samples.")

    L += ["", "## Body-blocking incidents and voice (observer log, not the Event Log)", ""]
    if a["observer_files"]:
        L.append("n = %d observer-form entr(y/ies), %d incident(s) (passage <= 1 pawn wide and a player's path obstructed)." % (a["bb_entries"], a["bb_incidents"]))
        L.append("Test groups: %d, with voice %d, without voice %d (OQ-NKP-8)." % (a["groups"], a["groups_voice"], a["groups"] - a["groups_voice"]))
    else:
        L.append("n = 0: no observer log given; body blocking and voice are not in the Event Log.")
    return "\n".join(L) + "\n"


def self_test():
    """The synthetic fixtures with hand-computed values must aggregate exactly (QA AC2 of nkp-022)."""
    rounds, observers, skipped = load_inputs([FIXTURE_DIR])
    a = aggregate(rounds, observers)
    bad, checks = [], [0]

    def eq(name, got, want):
        checks[0] += 1
        if got != want:
            bad.append("%s: got %r want %r" % (name, got, want))

    eq("rounds", a["rounds"], 4)
    eq("matches", a["matches"], 3)
    eq("skipped", skipped, ["match-d-unterminated.json"])
    eq("rows ignored (seeded non-eligible)", a["rows_ignored"], 3)
    eq("respawns (seeded InformantRespawn ignored)", a["respawns"], 3)
    eq("T_black values", sorted(a["tblack"]), [15.25, 15.5, 35.0])
    eq("median", median(a["tblack"]), 15.5)
    eq("p95", p95(a["tblack"]), 35.0)
    eq("unpaired / cancelled / lost / starved", (a["unpaired"], a["cancelled"], a["pawn_lost"], a["starved"]), (0, 1, 1, 1))
    eq("exiles", a["exiles"], 2)
    eq("strike rows (seeded one ignored)", a["strike_rows"], 13)
    eq("F4 L=3 reached/residual", (a["f4"][3]["reached"], a["f4"][3]["residual"]), (2, 1))
    eq("F4 L=2 reached/residual (same tally, per-round L)", (a["f4"][2]["reached"], a["f4"][2]["residual"]), (2, 1))
    s3 = f4_estimate(a["f4"][3], 3)
    s2 = f4_estimate(a["f4"][2], 2)
    eq("F4 L=3 b", "%.2f" % s3[3], "6.50")
    eq("F4 L=3 planning", ("%.3f" % s3[4], "%.3f" % s3[5]), ("0.111", "0.288"))
    eq("F4 L=2 b", "%.2f" % s2[3], "5.18")
    eq("F4 L=2 planning", ("%.3f" % s2[4], "%.3f" % s2[5]), ("0.143", "0.350"))
    eq("F5 wrong/informant (seeded informant ignored)", (a["wrong"], a["wrong_informant"]), (8, 2))
    eq("F5 q", "%.3f" % (a["wrong_q_sum"] / a["wrong"]), "0.047")
    eq("graced / after-grace released", (a["graced"], a["after_grace_released"]), (2, 1))
    eq("regions", (a["region_rounds"], a["region_inf"], a["region_pawns"], a["city_inf"], a["city_pawns"]), (2, 5, 18, 6, 106))
    eq("body blocking", (a["bb_entries"], a["bb_incidents"]), (4, 2))
    eq("voice groups", (a["groups"], a["groups_voice"]), (3, 2))
    text = report(a, skipped)
    for needle in ("Stop reason: not reached", "median T_black: 15.50 s (budget <= 30 s)", "p95 T_black: 35.00 s (budget <= 40 s)",
                   "share 0.250 against 2 x q = 0.094", "ratio 4.91", "escalation condition (ratio > 2 on >= 10 matches) not met"):
        eq("report contains %r" % needle, needle in text, True)
    # Edge cases: an empty export, a zero-respawn match, no observer log.
    only = lambda name: [r for r in rounds if r["_file"] == name]
    empty = aggregate(only("match-c-empty.json"), [])
    eq("empty export counts", (empty["rounds"], empty["rows_total"], empty["respawns"], empty["exiles"], empty["tblack"]), (1, 0, 0, 0, []))
    empty_text = report(empty, [])
    for needle in ("n = 0 paired respawns", "n = 0 players reached tally L", "n = 0 WrongTarget victims", "n = 0: no observer log"):
        eq("empty report contains %r" % needle, needle in empty_text, True)
    zero = aggregate(only("match-b-round-1.json"), [])
    eq("zero-respawn match", (zero["respawns"], zero["tblack"], zero["wrong"], zero["wrong_informant"]), (0, [], 1, 0))
    eq("zero-respawn report ok", "n = 0 paired respawns" in report(zero, []), True)
    eq("no rounds at all", "Input: 0 terminated round(s)" in report(aggregate([], []), []), True)
    if bad:
        sys.stdout.write("SELF-TEST MISMATCH\n" + "\n".join(bad) + "\n")
        return 1
    sys.stdout.write("self-test: %d checks passed (%d rounds, %d matches, %d skipped, %d rows ignored)\n"
                     % (checks[0], a["rounds"], a["matches"], len(skipped), a["rows_ignored"]))
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("inputs", nargs="*", help="exported round JSON files, observer logs or folders of them")
    parser.add_argument("--out", help="write the report here instead of stdout")
    parser.add_argument("--self-test", action="store_true", help="check the aggregation against the synthetic fixtures")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    if not args.inputs:
        parser.error("no input")
    try:
        rounds, observers, skipped = load_inputs(args.inputs)
        if not rounds:
            sys.stderr.write("no terminated round in the input\n")
            return 2
        text = report(aggregate(rounds, observers), skipped)
    except (OSError, ValueError, KeyError, TypeError) as error:
        sys.stderr.write("bad input: %s\n" % error)
        return 2
    if args.out:
        with open(args.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
