#!/usr/bin/env python
"""AC-3b doc lints over the owner Tuning Knobs tables (stories tdc-031, 032, 033 and 036).

One build-time tool (standard library only, run as `python -I knob_lint.py <rule> --root <dir>`) with
four rules. Each prints observations only, in the contract of tools/ci/lint/emit.sh:
  <path>:<line> <rule> <message>      a finding (path repo-relative, forward slashes, no spaces)
  SCANNED <path>                      one input file the rule examined
It never edits a GDD, a registry or a value and never decides pass or fail: tools/ci/lint/gate.sh does
(the .claude/scripts rule that helpers emit observations, ADR-0008). The shell wrappers
knob-lint-columns.sh, knob-lint-owner.sh, knob-lint-registry.sh and knob-lint-bounded-by.sh are the rule
scripts the gate runs. All four read the records of knob_tables.py (story tdc-029); none scrapes Markdown.
Exit code: 0 when the rule ran, 3 when it could not run (a missing registry or TDC GDD), which the gate
reports as NOT ASSESSED rather than a pass.

columns (story tdc-031, rule id knob-columns, GDD AC-3b bullet 1, CR-15.1)
  Every owner Tuning Knobs table must carry the ten CR-15 columns Key, Type, Owner, Default, Hard range,
  Safe range, Audience, Latch, Unit, Effect. Columns are matched by header name; the Effect column by its
  leading word, because the real tables spell it "Effect / breaks if too high or low" or "Effect / what
  breaks". Presence only: a reordered table or an extra column is not a finding (the parser maps by
  name). Owner-level preamble tables (Level Socket Contract, Procedural City Generator) state Owner,
  Audience and Latch once above the table instead of per row; a column the table lacks is accepted when the
  preamble supplies it (the TDC Registered Owner Keys table records this style as accepted) and reported
  when it does not. A table that has no Hard range and no Safe range column is a list of keys the GDD
  reads, not owns (for example Roster's "consumed elsewhere" table), and is not checked.

owner (story tdc-032, rule id knob-owner, AC-3b bullet 2, CR-15.3)
  A key is owned by a GDD when it is a row of that GDD's owner Tuning Knobs table (a key mentioned in
  prose, in Formulas or in a "**Owned by** ..." reference row is a reference, not ownership). A key owned
  by two GDDs, or twice in one, is reported once naming every file and line. The zero-owner check needs a
  key universe the inputs did not define; decision: the universe is the constants of
  design/registry/entities.yaml whose status is not deprecated (the registry is the index of every key
  that crosses a system boundary, and story tdc-030 completed its mirror). A universe constant named by no
  owner row is reported at its registry line. Keys quoted only in invariants are not a universe: they are
  checked against the key set by rule bounded-by. A key renamed in one GDD and not the other is two keys:
  the old name shows as zero-owner (when the registry still has it) and the report stays factual.

registry (story tdc-033, rule id knob-registry, AC-3b bullet 3 registry half, CR-15.7)
  Every owner key must match a registry constant. A key with no constant is one finding; a constant
  whose name differs only by case is a name finding; otherwise each of source GDD, default (value), hard
  range, safe range, audience and latch that differs is its own finding naming the key and the field
  (Unit is not compared). Cell equality is registry_io.same_cell: markup, parenthetical text, thousands
  separators, trailing zeros, range spelling and case are ignored (60 equals 60.0, "10 to 20" equals
  "10-20"), a placeholder cell equals an absent field, nothing else is equal. Only preset fields are
  compared (rows of scope key); the TDC schema constant and the catalog flags are not.

bounded-by (story tdc-036, rule id knob-bounded-by, ADR-0004 section 4, the C-W18 pattern)
  The key-to-invariant map is read from the TDC GDD Formulas section, never hard-coded: the "**I-1**" to
  "**I-5**" bullets and the "Owner invariants" table (ID | Owner | Constraint | On failure). The keys an
  invariant reads are the backtick-quoted names of its text that exist in the key set (the owner rows of
  scope key). A row whose On failure is offline is checked by its owner's CI and is skipped. For each
  (key, invariant) the key's owner Tuning Knobs row must say "bounded by <Id>" in its Effect cell; the
  match is the literal text, case-insensitive, with an optional "above" or "below" and at most three
  words between "by" and the id ("bounded below by Generator I-PCG-2" conforms), and the id must end
  there (I-SCL-1 does not match I-SCL-12). A key read by two invariants needs the text for both ids. A
  table row that covers an id range ("I-CS-1 to I-CS-5") is one invariant row: the key's row must name
  any id of that range. One finding per missing pair, at the owner row. The baseline
  tools/ci/lint/knob-bounded-by-baseline.txt (entries "<gdd-path> | <key> | <invariant-id> | <citation>",
  the id being the first id of the row) lists accepted findings: a listed pair is not reported, a listed
  pair that is not a finding any more is reported as knob-baseline-stale (so the list shrinks), an entry
  without a citation or in another shape is knob-baseline-malformed. The baseline may only shrink: that
  comparison with the base revision is lint_shrink_only (emit.sh), run by knob-lint-bounded-by.sh.
"""
import io
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import knob_tables as kt  # noqa: E402
import registry_io as rio  # noqa: E402

NEWLINE = chr(10)
REGISTRY_REL = "design/registry/entities.yaml"
TDC_REL = "design/gdd/tuning-data-config.md"
BASELINE_REL = "tools/ci/lint/knob-bounded-by-baseline.txt"
COLUMN_LABELS = (("key", "Key"), ("type", "Type"), ("owner", "Owner"), ("default", "Default"),
                 ("hard_range", "Hard range"), ("safe_range", "Safe range"), ("audience", "Audience"),
                 ("latch", "Latch"), ("unit", "Unit"), ("effect", "Effect"))


def short(text, limit=60):
    t = re.sub(r"\s+", " ", re.sub(r"[`*]", "", str(text))).strip()
    return t if len(t) <= limit else t[: limit - 3] + "..."


class Out:
    """Observation printer (LF line endings, never a verdict)."""

    def __init__(self):
        self.stream = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", newline=NEWLINE, line_buffering=True)

    def scanned(self, path):
        self.stream.write("SCANNED %s\n" % path.replace(" ", "_"))

    def finding(self, path, line, rule, message):
        self.stream.write("%s:%d %s %s\n" % (path.replace(" ", "_"), line, rule, message))


def _scan(root):
    return kt.scan_tree(root)


def _scanned_gdds(scan, out):
    for rel in scan["files"]:
        out.scanned(rel)


# ---- columns ------------------------------------------------------------------------------------------

def rule_columns(root, out):
    scan = _scan(root)
    for table in scan["tables"]:
        if not table["owned"]:
            continue
        present = {kt.header_column(c) for c in table["header"]}
        preamble = table["preamble"]
        for field, label in COLUMN_LABELS:
            if field in present:
                continue
            if table["style"] == "preamble" and field in ("owner", "audience", "latch") and field in preamble:
                continue
            out.finding(table["gdd"], table["line"], "knob-columns",
                        "Tuning Knobs table is missing the CR-15 column '%s'" % label)
    with_tables = {t["gdd"] for t in scan["tables"] if t["owned"]}
    for rel in scan["files"]:
        if rel in with_tables:
            out.scanned(rel)
    return 0


# ---- owner --------------------------------------------------------------------------------------------

def _registry(root):
    path = os.path.join(root, REGISTRY_REL)
    if not os.path.isfile(path):
        sys.stderr.write("knob-lint: registry not found: %s\n" % REGISTRY_REL)
        return None
    constants, problems = rio.read_constants(path)
    if problems:
        sys.stderr.write("knob-lint: %s has an unsupported layout at line %d\n" % (REGISTRY_REL, problems[0][0]))
        return None
    return constants


def rule_owner(root, out):
    constants = _registry(root)
    if constants is None:
        return 3
    scan = _scan(root)
    owners = {}
    for rec in scan["knobs"]:
        owners.setdefault(rec["key"], []).append(rec)
    for key, recs in sorted(owners.items(), key=lambda kv: (kv[1][0]["gdd"], kv[1][0]["line"])):
        if len(recs) > 1:
            where = " and ".join("%s:%d" % (r["gdd"], r["line"]) for r in recs)
            files = sorted({r["gdd"] for r in recs})
            out.finding(recs[0]["gdd"], recs[0]["line"], "knob-owner",
                        "key %s is owned by %d Tuning Knobs rows (%s); one key has one owner GDD (CR-15.3)%s"
                        % (key, len(recs), where, "" if len(files) > 1 else " [same GDD twice]"))
    for const in constants:
        if const["fields"].get("status", "active") == "deprecated":
            continue
        if const["name"] not in owners:
            out.finding(REGISTRY_REL, const["line"], "knob-owner",
                        "registry constant %s (source %s) is owned by no GDD Tuning Knobs table (zero owners)"
                        % (const["name"], const["fields"].get("source", "?")))
    _scanned_gdds(scan, out)
    out.scanned(REGISTRY_REL)
    return 0


# ---- registry -----------------------------------------------------------------------------------------

REGISTRY_FIELDS = (("source", "source GDD"), ("value", "default"), ("hard_range", "hard range"),
                   ("safe_range", "safe range"), ("audience", "audience"), ("latch", "latch"))
GDD_FIELD = {"value": "default"}


def rule_registry(root, out):
    constants = _registry(root)
    if constants is None:
        return 3
    by_name = {c["name"]: c for c in constants}
    folded = {}
    for c in constants:
        folded.setdefault(c["name"].casefold(), c["name"])
    scan = _scan(root)
    for rec in scan["knobs"]:
        if rec["scope"] != "key":
            continue
        const = by_name.get(rec["key"])
        if const is None:
            near = folded.get(rec["key"].casefold())
            if near:
                out.finding(rec["gdd"], rec["line"], "knob-registry",
                            "key %s: name differs from registry constant '%s' (registry line %d)"
                            % (rec["key"], near, by_name[near]["line"]))
            else:
                out.finding(rec["gdd"], rec["line"], "knob-registry",
                            "key %s: missing constant in %s (mirror it with tools/ci/lint/registry_mirror.py)"
                            % (rec["key"], REGISTRY_REL))
            continue
        fields = const["fields"]
        for reg_field, label in REGISTRY_FIELDS:
            registry_value = fields.get(reg_field)
            if reg_field == "source":
                same = registry_value is not None and registry_value.replace("\\", "/") == rec["gdd"]
                gdd_value = rec["gdd"]
            else:
                gdd_value = rec[GDD_FIELD.get(reg_field, reg_field)]
                same = rio.same_cell(gdd_value, registry_value)
            if not same:
                out.finding(rec["gdd"], rec["line"], "knob-registry",
                            "key %s: %s differs: owner row '%s' vs registry '%s' (registry line %d)"
                            % (rec["key"], label, short(gdd_value), short(registry_value if registry_value is not None else "<absent>"),
                               const["field_lines"].get(reg_field, const["line"])))
    with_rows = {r["gdd"] for r in scan["knobs"]}
    for rel in scan["files"]:
        if rel in with_rows:
            out.scanned(rel)
    out.scanned(REGISTRY_REL)
    return 0


# ---- bounded-by ---------------------------------------------------------------------------------------

def _bounded_by(effect, inv_id):
    pattern = r"bounded\s+(?:above\s+|below\s+)?by\s+(?:[A-Za-z&'.]+\s+){0,3}" + re.escape(inv_id) + r"(?![A-Za-z0-9-])"
    return re.search(pattern, re.sub(r"[`*]", "", effect), re.I) is not None


def read_baseline(root, out):
    """Entries of the bounded-by baseline: {(gdd, key, id): (line, citation)}; malformed lines are findings."""
    path = os.path.join(root, BASELINE_REL)
    entries = {}
    if not os.path.isfile(path):
        return entries
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for n, raw in enumerate(handle, 1):
            line = raw.rstrip("\n").rstrip("\r")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            parts = [p.strip() for p in line.split("|")]
            if len(parts) < 4 or not all(parts[:3]) or not "|".join(parts[3:]).strip():
                out.finding(BASELINE_REL, n, "knob-baseline-malformed",
                            "entry must read '<gdd-path> | <key> | <invariant-id> | <citation>' with a citation")
                continue
            entries[(parts[0], parts[1], parts[2])] = (n, "|".join(parts[3:]).strip())
    return entries


def rule_bounded_by(root, out):
    tdc_path = os.path.join(root, TDC_REL)
    if not os.path.isfile(tdc_path):
        sys.stderr.write("knob-lint: TDC GDD not found: %s\n" % TDC_REL)
        return 3
    scan = _scan(root)
    owner_rows = {}
    for rec in scan["knobs"]:
        if rec["scope"] == "key":
            owner_rows.setdefault(rec["key"], rec)
    invariants = kt.parse_invariants(tdc_path)
    baseline = read_baseline(root, out)
    used = set()
    seen_pairs = set()
    for inv in invariants:
        for key in kt.invariant_keys(inv, owner_rows):
            row = owner_rows[key]
            pair_id = inv["ids"][0]
            ident = (row["gdd"], key, pair_id)
            if (ident, inv["line"]) in seen_pairs:
                continue
            seen_pairs.add((ident, inv["line"]))
            if any(_bounded_by(row["effect"], i) for i in inv["ids"]):
                continue
            if ident in baseline:
                used.add(ident)
                continue
            ids = inv["ids"][0] if len(inv["ids"]) == 1 else "%s to %s" % (inv["ids"][0], inv["ids"][-1])
            wanted = "'bounded by %s'" % pair_id if len(inv["ids"]) == 1 else "'bounded by' any id of the range (baseline id %s)" % pair_id
            out.finding(row["gdd"], row["line"], "knob-bounded-by",
                        "key %s is read by invariant %s (%s:%d) but its Effect cell does not say %s"
                        % (key, ids, TDC_REL, inv["line"], wanted))
    for ident, (n, _citation) in sorted(baseline.items(), key=lambda kv: kv[1][0]):
        if ident not in used:
            out.finding(BASELINE_REL, n, "knob-baseline-stale",
                        "baseline entry '%s | %s | %s' is not a finding any more; remove it (the baseline only shrinks)" % ident)
    _scanned_gdds(scan, out)
    return 0


RULES = {"columns": rule_columns, "owner": rule_owner, "registry": rule_registry, "bounded-by": rule_bounded_by}


def main(argv):
    if not argv or argv[0] not in RULES:
        sys.stderr.write("usage: knob_lint.py {%s} [--root <dir>]\n" % "|".join(RULES))
        return 2
    rule = argv[0]
    root = os.getcwd()
    i = 1
    while i < len(argv):
        if argv[i] == "--root":
            root = argv[i + 1]
            i += 1
        else:
            sys.stderr.write("unknown argument: %s\n" % argv[i])
            return 2
        i += 1
    return RULES[rule](os.path.abspath(root), Out())


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
