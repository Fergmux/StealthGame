#!/usr/bin/env python3
"""Assassination probe and brace telemetry report (AC-ASN-33, story asn-026; ADR-0023 sections 3 and 7).

Offline only: reads exported, terminated rounds' telemetry sections (one JSON file per round, written by the Event Log
store after termination, ADR-0023 section 6) and prints a Markdown table per match. It emits observations, never
verdicts: the review bands are printed beside the values for the creative director and game designer to read; nothing
here changes a tuning value or decides pass/fail. Standard library only. It never runs inside a round.

Input schema (SchemaVersion 1; the fields this report reads, a subset of the ADR-0023 section 6 export):
  {
    "SchemaVersion": 1,
    "MatchId": "<opaque>",            # optional: rounds sharing it are one match (default: the file is its own match)
    "RoundIndex": 1,
    "EndReason": "ObjectiveWin",      # a round without one (or "Active"/"Preparing") is not terminated: skipped
    "NpcCount": 50,
    "Subjects": [ {"Key": 1, "Label": "Participant", "Team": 0}, {"Key": 9, "Label": "Npc"} ],   # the sink's labels
    "StrikeResolved": [ {"ServerTime": 12.3, "Attacker": 1, "Victim": 9, "Outcome": "Kill"} ],   # Victim null: whiff
    "BraceEnded": [ {"ServerTime": 30.1, "Subject": 2, "StartTime": 27.0, "Cause": "Reversal"} ]
  }
Per match it reports: strikes by outcome (NPC kill, rival kill, other kill, reversal, whiff), NPC kills per rival kill,
reversals, braces (BraceEnded records) and pair trades (a reversal followed within 10.0 s, inclusive, by a kill whose victim
is the same defender). Review bands (AC-ASN-33, not gates): NPC kills per rival kill 3 to 15 (F5 baseline 10.7 at NpcCount
50), pair trades <= 25 % of reversals, reversals per match 0.5 to 2.

Usage:
  python tools/analysis/assassination_telemetry_report.py <export.json or folder> [...] [--out report.md]
  python tools/analysis/assassination_telemetry_report.py --self-test
Exit: 0 report written (or self-test passed), 1 self-test mismatch, 2 no terminated round found / bad input.
"""
import argparse
import json
import os
import sys

PAIR_TRADE_WINDOW_SEC = 10.0
UNTERMINATED = {"", "Active", "Preparing", None}
BANDS = "Review bands (not gates): NPC kills per rival kill 3-15 (F5 baseline 10.7 at NpcCount 50); pair trades <= 25 % of reversals; reversals per match 0.5-2."
HERE = os.path.dirname(os.path.abspath(__file__))
FIXTURE_DIR = os.path.normpath(os.path.join(HERE, "..", "..", "tests", "fixtures", "asn", "telemetry"))


def load_rounds(paths):
    """Every terminated round from the files and folders given, in a stable order; plus the skipped file names."""
    files = []
    for path in paths:
        if os.path.isdir(path):
            files.extend(sorted(os.path.join(path, n) for n in os.listdir(path) if n.endswith(".json")))
        else:
            files.append(path)
    rounds, skipped = [], []
    for path in files:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        if data.get("SchemaVersion") != 1:
            raise ValueError("%s: unsupported SchemaVersion %r" % (path, data.get("SchemaVersion")))
        if data.get("EndReason") in UNTERMINATED:
            skipped.append(os.path.basename(path))
            continue
        data["_file"] = os.path.basename(path)
        rounds.append(data)
    return rounds, skipped


def round_stats(data):
    """The AC-ASN-33 counts of one round."""
    labels = {s["Key"]: s for s in data.get("Subjects", [])}
    strikes = sorted(data.get("StrikeResolved", []), key=lambda s: s["ServerTime"])
    out = {"npc_kills": 0, "rival_kills": 0, "other_kills": 0, "reversals": 0, "whiffs": 0, "pair_trades": 0,
           "braces": len(data.get("BraceEnded", []))}
    for s in strikes:
        outcome = s["Outcome"]
        if outcome == "Whiff":
            out["whiffs"] += 1
        elif outcome == "Reversal":
            out["reversals"] += 1
        elif outcome == "Kill":
            victim = labels.get(s.get("Victim"), {})
            attacker = labels.get(s.get("Attacker"), {})
            if victim.get("Label") == "Npc":
                out["npc_kills"] += 1
            elif victim.get("Label") == "Participant" and attacker.get("Label") == "Participant" and victim.get("Team") != attacker.get("Team"):
                out["rival_kills"] += 1
            else:
                out["other_kills"] += 1  # an informant or an unlabelled subject
        else:
            raise ValueError("%s: unknown outcome %r" % (data["_file"], outcome))
    # Pair trades: each reversal counted once if its defender is killed within the window after it.
    for i, s in enumerate(strikes):
        if s["Outcome"] != "Reversal" or s.get("Victim") is None:
            continue
        defender, t = s["Victim"], s["ServerTime"]
        if any(k["Outcome"] == "Kill" and k.get("Victim") == defender and t < k["ServerTime"] <= t + PAIR_TRADE_WINDOW_SEC
               for k in strikes[i + 1:]):
            out["pair_trades"] += 1
    return out


def report(rounds, skipped):
    """The Markdown report (deterministic for a given input)."""
    matches = {}
    for r in rounds:
        matches.setdefault(r.get("MatchId") or r["_file"], []).append(r)
    lines = ["# Assassination probe and brace telemetry (AC-ASN-33)", "", BANDS, "",
             "| Match | Rounds | NPC kills | Rival kills | Other kills | Reversals | Whiffs | Braces | Pair trades | NPC kills per rival kill | Pair trades / reversals |",
             "|---|---|---|---|---|---|---|---|---|---|---|"]
    total = None
    for match_id in sorted(matches):
        acc = None
        for r in sorted(matches[match_id], key=lambda r: r.get("RoundIndex", 0)):
            st = round_stats(r)
            acc = st if acc is None else {k: acc[k] + st[k] for k in acc}
        total = acc if total is None else {k: total[k] + acc[k] for k in total}
        lines.append(row(match_id, len(matches[match_id]), acc))
    lines.append(row("**All**", len(rounds), total))
    lines += ["", "Matches: %d. Reversals per match: %s." % (len(matches), "%.2f" % (total["reversals"] / len(matches)))]
    if skipped:
        lines += ["", "Skipped (not terminated, never serialised as a finished round): " + ", ".join(sorted(skipped)) + "."]
    return "\n".join(lines) + "\n"


def row(name, rounds, s):
    ratio = "%.2f" % (s["npc_kills"] / s["rival_kills"]) if s["rival_kills"] else "n/a (0 rival kills)"
    trades = "%.0f %%" % (100.0 * s["pair_trades"] / s["reversals"]) if s["reversals"] else "n/a (0 reversals)"
    return "| %s | %d | %d | %d | %d | %d | %d | %d | %d | %s | %s |" % (
        name, rounds, s["npc_kills"], s["rival_kills"], s["other_kills"], s["reversals"], s["whiffs"], s["braces"], s["pair_trades"], ratio, trades)


def self_test():
    """The synthetic fixture with known counts must produce the expected report exactly (QA rewrite of AC-ASN-33)."""
    rounds, skipped = load_rounds([FIXTURE_DIR])
    got = report(rounds, skipped)
    with open(os.path.join(FIXTURE_DIR, "expected-report.md"), encoding="utf-8") as f:
        want = f.read()
    if got != want:
        sys.stdout.write("SELF-TEST MISMATCH\n--- expected\n%s--- got\n%s" % (want, got))
        return 1
    sys.stdout.write("self-test: the fixture report matches (%d rounds, %d skipped)\n" % (len(rounds), len(skipped)))
    return 0


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("inputs", nargs="*", help="exported round JSON files or folders of them")
    parser.add_argument("--out", help="write the report here instead of stdout")
    parser.add_argument("--self-test", action="store_true", help="check the report against the synthetic fixture")
    args = parser.parse_args(argv)
    if args.self_test:
        return self_test()
    if not args.inputs:
        parser.error("no input")
    try:
        rounds, skipped = load_rounds(args.inputs)
    except (OSError, ValueError, KeyError) as error:
        sys.stderr.write("bad input: %s\n" % error)
        return 2
    if not rounds:
        sys.stderr.write("no terminated round in the input\n")
        return 2
    text = report(rounds, skipped)
    if args.out:
        with open(args.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
