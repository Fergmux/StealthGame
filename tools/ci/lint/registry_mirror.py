#!/usr/bin/env python
"""Registry mirror (story tdc-030; GDD CR-15.7 and OQ-15, tech debt TD-REG-1).

Appends a constant to design/registry/entities.yaml for every owner key the knob-table parser
(knob_tables.py, story tdc-029) finds that has no constant yet, with the owner table's default, ranges,
audience and latch and `source:` set to the owner GDD. Build-time tool, standard library only, run as
`python -I registry_mirror.py ...`. It reports what it added; whether the registry now matches the GDDs is
judged by the registry-match lint (story tdc-033) and its gate, never here.

Additive only. The file is never re-serialised: new entries are appended after the last existing byte, so
every existing constant stays byte-identical (the script refuses to write when the result would not start
with the original bytes). The mirror is idempotent: a second run finds nothing missing and writes nothing.
A key keeps its owner-table name: nothing is renamed, and a constant that already exists is never altered
(a difference between an existing constant and its owner row is a lint finding, story tdc-033).

What is mirrored. Rows of kind knob with scope "key" (a preset field). Not mirrored: rows listed for
reference (`**Owned by** ...`, owned and registered elsewhere), struck-through retired rows, the TDC schema
constant and the catalog flags (scope "constant" / "catalog-flag": not preset fields), and the Crowd
constants that have no key yet (OQ-14: they have no table row, so the parser never sees them).

How a cell becomes a field (same normal form as the lint, registry_io.normalize(compare=False)):
  value        the Default cell with markup, parenthetical text and thousands separators removed; a
               number or true/false is written bare. Parenthetical text is kept, capped at 200 characters,
               as `default_note`.
  hard_range, safe_range
               the cell in the same form; a placeholder cell (an em dash, "--", "-") writes no field,
               as for the bool keys already in the file.
  audience, latch
               the cell in the same form (a placeholder writes no field).
  unit         the Unit cell mapped to the file's spelling (Sec -> s, Cm -> cm, Count -> count, PerSec ->
               per_s ...); a placeholder unit becomes bool, enum or none from the Type cell.
  note         "mirrored from <gdd>:<line> (CR-15.7, TD-REG-1, story tdc-030)".
  referenced_by the owner GDD only: other GDDs that read the key are added by their authors, as the file
               header says.
  added        --date (default today), revised "".

Usage:
  python -I registry_mirror.py [--root <dir>] [--registry <rel>] [--date YYYY-MM-DD] [--dry-run]
Output (stdout, informational lines, not lint observations):
  MIRROR missing <n>
  ADDED <key> <gdd>:<line>          one per constant
  MIRROR result: added <n>, existing <m> untouched
Exit: 0 when it ran (including a run that added nothing), 2 when the registry cannot be read, has an
unsupported layout, or the append would alter existing text.
"""
import datetime
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import knob_tables  # noqa: E402
import registry_io as rio  # noqa: E402

NEWLINE = chr(10)
NOTE_CAP = 200
UNIT_MAP = {
    "sec": "s", "s": "s", "pct": "pct", "count": "count", "cm": "cm", "deg": "deg", "mult": "mult",
    "persec": "per_s", "1/s": "per_s", "rate": "per_s", "cmpersec": "cm/s", "degpersec": "deg/s",
    "hz": "Hz", "limit": "count", "depth": "count", "min": "min", "ms": "ms", "asset": "asset",
    "flag": "bool", "size": "size",
}


def unit_for(unit_cell, type_cell):
    cell = rio.normalize(unit_cell, compare=False)
    if rio.is_placeholder(cell):
        low = type_cell.casefold()
        return "bool" if "bool" in low else ("enum" if "enum" in low else "none")
    return UNIT_MAP.get(cell.replace(" ", "").casefold(), cell.casefold().replace(" ", "_"))


def entry_lines(rec, date):
    """The registry lines of one constant, as the file writes them (without line endings)."""
    gdd = rec["gdd"]
    value = rio.normalize(rec["default"], compare=False) or re.sub(r"[`*~]", "", rec["default"]).strip()
    notes = rio.parentheticals(rec["default"])
    lines = [
        "  - name: %s" % rec["key"],
        "    status: active",
        "    source: %s" % gdd,
        "    referenced_by:",
        "      - %s" % gdd,
        "    value: %s" % rio.yaml_scalar(value, allow_bool=True),
        "    unit: %s" % rio.yaml_scalar(unit_for(rec["unit"], rec["type"])),
    ]
    if notes:
        note = "; ".join(notes)
        if len(note) > NOTE_CAP:
            note = note[: NOTE_CAP - 3].rstrip() + "..."
        lines.append("    default_note: %s" % rio.yaml_scalar(note))
    for field in ("hard_range", "safe_range", "audience", "latch"):
        text = rio.normalize(rec[field], compare=False)
        if not rio.is_placeholder(text):
            lines.append("    %s: %s" % (field, rio.yaml_scalar(text)))
    lines.append("    note: %s" % rio.yaml_scalar(
        "mirrored from %s:%d (CR-15.7, TD-REG-1, story tdc-030)" % (gdd, rec["line"])))
    lines.append("    added: %s" % date)
    lines.append('    revised: ""')
    return lines


def missing_records(scan, constants):
    have = {c["name"] for c in constants}
    out = []
    for rec in scan["knobs"]:
        if rec["scope"] == "key" and rec["key"] not in have:
            out.append(rec)
    return out


def build_append(records, date, newline):
    if not records:
        return b""
    head = [
        "",
        "  # " + "─" * 3 + " Owner-table mirror (CR-15.7, TD-REG-1, story tdc-030) " + "─" * 3,
        "  # Appended by tools/ci/lint/registry_mirror.py: every owner Tuning Knobs key that had no constant above,",
        "  # with the owner table's default, ranges, audience and latch. This supersedes the 2026-10-24 scope note",
        "  # in the constants header (single-GDD keys are now mirrored); the owner table stays authoritative.",
    ]
    body = list(head)
    for rec in records:
        body.extend(entry_lines(rec, date))
    return (newline.join(body) + newline).encode("utf-8")


def run(root, registry_rel, date, dry_run, out):
    path = os.path.join(root, registry_rel)
    if not os.path.isfile(path):
        out.write("MIRROR error: registry not found: %s\n" % registry_rel)
        return 2
    constants, problems = rio.read_constants(path)
    if problems:
        out.write("MIRROR error: %s has an unsupported layout (%d line(s), first at line %d); nothing written\n"
                  % (registry_rel, len(problems), problems[0][0]))
        return 2
    scan = knob_tables.scan_tree(root)
    todo = missing_records(scan, constants)
    out.write("MIRROR missing %d\n" % len(todo))
    with open(path, "rb") as handle:
        original = handle.read()
    newline = "\r\n" if b"\r\n" in original else NEWLINE
    addition = build_append(todo, date, newline)
    if addition and not original.endswith(newline.encode()):
        addition = newline.encode() + addition
    for rec in todo:
        out.write("ADDED %s %s:%d\n" % (rec["key"], rec["gdd"], rec["line"]))
    if addition and not dry_run:
        with open(path, "ab") as handle:
            handle.write(addition)
        with open(path, "rb") as handle:
            if not handle.read().startswith(original):
                out.write("MIRROR error: the written file does not start with the original bytes\n")
                return 2
        constants_after, problems_after = rio.read_constants(path)
        names = [c["name"] for c in constants_after]
        if problems_after or len(names) != len(set(names)) or len(names) != len(constants) + len(todo):
            out.write("MIRROR error: the written registry fails its self-check (layout problems %d, duplicate names %d)\n"
                      % (len(problems_after), len(names) - len(set(names))))
            return 2
    out.write("MIRROR result: added %d, existing %d untouched%s\n" % (len(todo), len(constants), " (dry run)" if dry_run else ""))
    return 0


def main(argv):
    root = os.getcwd()
    registry = "design/registry/entities.yaml"
    date = datetime.date.today().isoformat()
    dry_run = False
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--root":
            root = argv[i + 1]
            i += 1
        elif a == "--registry":
            registry = argv[i + 1]
            i += 1
        elif a == "--date":
            date = argv[i + 1]
            i += 1
        elif a == "--dry-run":
            dry_run = True
        else:
            sys.stderr.write("unknown argument: %s\n" % a)
            return 2
        i += 1
    out = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", newline=NEWLINE, line_buffering=True)
    return run(os.path.abspath(root), registry, date, dry_run, out)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
