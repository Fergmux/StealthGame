#!/usr/bin/env python
"""Knob-table parser for the TDC doc lint (story tdc-029; ADR-0004 section 2, GDD CR-15, AC-3b).

Reads the "Tuning Knobs" section of every design/gdd/*.md and turns its Markdown tables into
records, so the doc lints (stories 030-034, 036) never scrape Markdown themselves. Build-time
tool only: standard library only, run as `python -I knob_tables.py ...` (isolated mode). It is
never an editor gate and never edits a GDD or a value. It emits records and observations, never
a verdict (the .claude/scripts rule, ADR-0008): pass or fail lives in tools/ci/lint/gate.sh.

Streaming: every GDD is read line by line through a file iterator and only the table rows that
matter are kept (the GDDs are 90-180 KB; nothing here reads a whole file into one string).

Usage:
  python -I knob_tables.py [--root <dir>] [--records]       JSON-lines records on stdout
  python -I knob_tables.py [--root <dir>] [--observations]  (default) observations on stdout
  python -I knob_tables.py --root <dir> --gdd-dir <rel>     GDD folder relative to the root
                                                            (default design/gdd)

Observation contract (tools/ci/lint/emit.sh): every stdout line is `<path>:<line> <rule> <message>`
or `SCANNED <path>`. Rules emitted here: knob-no-section, knob-duplicate-key, knob-count,
knob-row-shape, knob-key-unparsed.

Record contract (JSON lines, stable; stories 030-036 read it, in-process through the functions
below or as lines): one object per table and one per key.
  {"kind":"table","gdd","line","style","header":[...],"owned":bool,"preamble":{"audience","latch","owner"}}
  {"kind":"knob","gdd","line","key","key_cell","type","owner","default","hard_range","safe_range",
   "audience","latch","unit","effect","style","inherited":[...field names taken from the preamble],
   "table_line"}
The Effect column is not part of the nine-field record contract of the story but is carried as
"effect" because story 036 needs it. `gdd` is the repo-relative path with forward slashes.

Table styles (the inputs did not define them; read from the two real GDDs):
  standard  the header carries the CR-15 columns (Key, Type, Owner, Default, Hard range, Safe range,
            Audience, Latch, Unit, Effect...). Columns are mapped by header name, so order variants
            and extra columns are tolerated. "Effect" is matched by its leading word, because the
            real tables spell it "Effect / breaks if too high or low" or "Effect / what breaks".
  preamble  the owner-level preamble style of the Level Socket Contract and the Procedural City
            Generator: the header omits Owner, Audience and/or Latch and the text between the
            "## Tuning Knobs" heading and the table states them once for every row ("owner X for
            every row" / "with the X as owner", "**Audience** `ServerOnly`", "**Latch** `Round`").
            The record carries those values and lists them in "inherited".
An owned table is one whose header has a Key column and a Hard range or Safe range column. Any other
table in the section (for example Roster's "consumed elsewhere" table: Key, Owner, Default, Roster
use) lists keys the GDD reads, not owns, and yields no records (CR-15.3: a reference is not ownership).

Paired rows: a key cell "`AMinX` / `MaxX`" holds two keys (counted per name, as the Registered Owner
Keys table counts them). A second name that is only the differing tail ("`MaxCount`") is expanded
from the first by replacing its last words (a name of three or more words is a whole key). Every cell
except Effect (Default "12 / 24", Hard range "20-75 / 20-60") is split in step when it has
as many top-level ' / ' parts as there are keys, otherwise every key gets the whole cell.
"""
import io
import json
import os
import re
import sys

CR15_COLUMNS = ("Key", "Type", "Owner", "Default", "Hard range", "Safe range", "Audience", "Latch", "Unit", "Effect")
RECORD_FIELDS = ("key", "type", "owner", "default", "hard_range", "safe_range", "audience", "latch", "unit")
_COLUMN_FIELD = {
    "key": "key", "type": "type", "owner": "owner", "default": "default", "hard range": "hard_range",
    "safe range": "safe_range", "audience": "audience", "latch": "latch", "unit": "unit", "effect": "effect",
}
NEWLINE = chr(10)
_IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def header_column(cell):
    """Canonical CR-15 column name of a header cell, or None. Effect matches by its leading word."""
    text = re.sub(r"[*_`]", "", cell).strip().lower()
    if text in _COLUMN_FIELD:
        return _COLUMN_FIELD[text]
    if re.match(r"^effect\b", text):
        return "effect"
    return None


def split_row(line):
    """Cells of a Markdown table row. A '|' inside backticks or escaped as '\\|' does not split."""
    s = line.strip()
    if s.startswith("|"):
        s = s[1:]
    if s.endswith("|") and not s.endswith("\\|"):
        s = s[:-1]
    cells, cur, in_code, i = [], [], False, 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s) and s[i + 1] == "|":
            cur.append("|")
            i += 2
            continue
        if c == "`":
            in_code = not in_code
        if c == "|" and not in_code:
            cells.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    cells.append("".join(cur).strip())
    return cells


def is_separator(cells):
    return bool(cells) and all(re.match(r"^:?-{2,}:?$", c.strip()) or c.strip() == "" for c in cells) and any(cells)


def _camel_words(text):
    return re.findall(r"[A-Z]+(?![a-z])|[A-Z]?[a-z0-9]+|[A-Z]+", text)


def expand_key_cell(cell):
    """Key names in a key cell. Returns the list of names in cell order (empty when none parse).

    A later name is a whole key when it has three or more CamelCase words (and no leading '...'),
    otherwise it is the differing tail of the first name ('MaxCount', 'FloorPct', '...MarketMult')
    and replaces the same number of words of the first name, keeping the shared suffix.
    """
    spans = [s.strip() for s in re.findall(r"`([^`]+)`", cell)]
    names = []
    for span in spans:
        bare = span[3:] if span.startswith("...") else span
        if _IDENT.match(bare):
            names.append(("..." + bare) if span.startswith("...") else bare)
    if len(names) < 2:
        return [n for n in names if not n.startswith("...")]
    first = names[0]
    out = [first]
    for other in names[1:]:
        ellipsis = other.startswith("...")
        bare = other[3:] if ellipsis else other
        ow = _camel_words(bare)
        fw = _camel_words(first)
        if not ellipsis and len(ow) >= 3:
            out.append(bare)  # a whole key
            continue
        common = 0
        while common < len(ow) - 1 and common < len(fw) and fw[-1 - common] == ow[-1 - common]:
            common += 1
        ow_core = ow[: len(ow) - common]
        n_replace = len(ow_core)
        head = fw[: max(len(fw) - common - n_replace, 0)]
        tail = fw[len(fw) - common:] if common else []
        out.append("".join(head + ow_core + tail))
    return out


def split_pair(cell, count):
    """Split 'a / b' into count parts when it has exactly that many (a ' / ' inside parentheses or
    backticks does not split); else the whole cell for each key."""
    if count < 2:
        return [cell]
    parts, depth, in_code, cur, i = [], 0, False, [], 0
    while i < len(cell):
        c = cell[i]
        if c == "`":
            in_code = not in_code
        elif not in_code and c in "([":
            depth += 1
        elif not in_code and c in ")]":
            depth = max(depth - 1, 0)
        if (not in_code and depth == 0 and c == "/" and i > 0 and cell[i - 1] == " "
                and i + 1 < len(cell) and cell[i + 1] == " "):
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    parts.append("".join(cur).strip())
    if len(parts) == count:
        return parts
    return [cell] * count


def _preamble_values(lines):
    text = " ".join(lines)
    out = {}
    m = re.search(r"\*\*Audience\*\*\s*`([A-Za-z]+)`", text)
    if m:
        out["audience"] = m.group(1)
    m = re.search(r"\*\*Latch\*\*\s*`([A-Za-z]+)`", text)
    if m:
        out["latch"] = m.group(1)
    m = re.search(r"owner\s+(.+?)\s+for every row", text) or re.search(r"with the\s+(.+?)\s+as owner", text)
    if m:
        out["owner"] = m.group(1).strip()
    return out


def iter_lines(path):
    """Line iterator (1-based numbers) that streams the file."""
    with open(path, "r", encoding="utf-8", errors="replace", newline=None) as handle:
        for n, line in enumerate(handle, 1):
            yield n, line.rstrip("\n").rstrip("\r")


def gdd_title(path):
    for _, line in iter_lines(path):
        if line.startswith("# "):
            return line[2:].strip()
        break
    return ""


def section_lines(path, heading):
    """Yield (line, text) of the body of a '## <heading>' section (a '### ' subsection stays inside)."""
    inside, fence = False, False
    for n, line in iter_lines(path):
        if line.lstrip().startswith("```"):
            fence = not fence
        if not fence and line.startswith("## "):
            if inside:
                return
            if line[3:].strip().lower() == heading.lower():
                inside = True
                continue
        if inside:
            yield n, line


def parse_tuning_knobs(path, rel):
    """Parse one GDD. Returns (tables, knobs, has_section): dicts as in the record contract."""
    tables, knobs = [], []
    has_section = False
    preamble_lines = []
    current = None  # table state
    for n, line in section_lines(path, "Tuning Knobs"):
        has_section = True
        stripped = line.strip()
        if stripped.startswith("|"):
            cells = split_row(stripped)
            if current is None:
                header = cells
                current = {"line": n, "header": header, "colmap": None, "expect_sep": True, "rows": 0}
                continue
            if current["expect_sep"]:
                current["expect_sep"] = False
                if is_separator(cells):
                    current = _open_table(current, rel, preamble_lines, tables)
                    continue
                current = None
                continue
            _add_row(current, cells, n, rel, knobs)
        else:
            current = None
            if stripped:
                preamble_lines.append(stripped)  # text above a table: an owner-level preamble may state Owner, Audience, Latch
    return tables, knobs, has_section


def _open_table(state, rel, preamble_lines, tables):
    header = state["header"]
    cols = [header_column(c) for c in header]
    colmap = {}
    for idx, col in enumerate(cols):
        if col and col not in colmap:
            colmap[col] = idx
    owned = "key" in colmap and ("hard_range" in colmap or "safe_range" in colmap)
    pre = _preamble_values(preamble_lines)
    missing_cols = [c for c in ("owner", "audience", "latch") if c not in colmap]
    style = "preamble" if owned and missing_cols and any(c in pre for c in missing_cols) else "standard"
    table = {
        "kind": "table", "gdd": rel, "line": state["line"], "style": style, "header": header,
        "owned": owned, "preamble": pre,
    }
    tables.append(table)
    state.update({"colmap": colmap, "owned": owned, "pre": pre, "style": style, "table": table, "expect_sep": False})
    return state


def _add_row(state, cells, n, rel, knobs):
    if not state["owned"]:
        return
    colmap = state["colmap"]
    if len(cells) != len(state["header"]):
        knobs.append({"kind": "shape", "gdd": rel, "line": n,
                      "message": "row has %d cells, header has %d" % (len(cells), len(state["header"]))})
        return

    def cell(field):
        idx = colmap.get(field)
        return cells[idx] if idx is not None and idx < len(cells) else ""

    key_cell = cell("key")
    if "~~" in key_cell:
        knobs.append({"kind": "retired", "gdd": rel, "line": n, "key_cell": key_cell})
        return
    names = expand_key_cell(key_cell)
    if not names:
        knobs.append({"kind": "unparsed", "gdd": rel, "line": n, "message": "no key name in cell '%s'" % key_cell})
        return
    kind = "knob"
    last = cells[-1] if cells else ""
    if re.match(r"^\*\*Owned by(?![A-Za-z])", cell("effect") or last):
        kind = "ref"  # listed for reference, owned (and registered) by another GDD
    scope = "key"
    if re.match(r"^Catalog(?![A-Za-z])", key_cell):
        scope = "catalog-flag"
    elif re.search(r"schema constant", cell("type"), re.I):
        scope = "constant"
    per_field = {f: split_pair(cell(f), len(names)) for f in ("type", "default", "hard_range", "safe_range", "audience", "latch", "unit")}
    for i, name in enumerate(names):
        rec = {"kind": kind, "gdd": rel, "line": n, "key": name, "key_cell": key_cell, "scope": scope,
               "style": state["style"], "inherited": [], "table_line": state["table"]["line"]}
        for field in ("type", "owner", "default", "hard_range", "safe_range", "audience", "latch", "unit", "effect"):
            value = per_field[field][i] if field in per_field else cell(field)
            if field not in colmap and field in state["pre"]:
                value = state["pre"][field]
                rec["inherited"].append(field)
            rec[field] = value
        knobs.append(rec)
    state["rows"] += 1


def gdd_files(root, gdd_dir="design/gdd"):
    """File names of every *.md in the GDD folder, sorted. scan_tree then leaves out the cross-review
    reports (gdd-cross-review-*.md) and systems-index.md: they are not GDDs and carry no Tuning Knobs
    section by design. A GDD without the section (game-concept.md) is kept and reported by knob-no-section."""
    base = os.path.join(root, gdd_dir)
    if not os.path.isdir(base):
        return []
    return sorted(f for f in os.listdir(base) if f.endswith(".md"))


def scan_tree(root, gdd_dir="design/gdd"):
    """Parse every GDD of the tree. Returns a dict: files (list of rel paths), per-file results."""
    result = {"files": [], "tables": [], "knobs": [], "problems": [], "refs": [], "retired": [], "no_section": [], "titles": {}}
    for name in gdd_files(root, gdd_dir):
        if name.startswith("gdd-cross-review") or name == "systems-index.md":
            continue  # review and index documents are not GDDs (no Tuning Knobs section by design)
        rel = "%s/%s" % (gdd_dir, name)
        path = os.path.join(root, gdd_dir, name)
        tables, knobs, has = parse_tuning_knobs(path, rel)
        result["files"].append(rel)
        result["titles"][rel] = gdd_title(path)
        if not has:
            result["no_section"].append(rel)
        result["tables"].extend(tables)
        for rec in knobs:
            if rec["kind"] == "knob":
                result["knobs"].append(rec)
            elif rec["kind"] in ("ref", "retired"):
                result["refs" if rec["kind"] == "ref" else "retired"].append(rec)
            else:
                result["problems"].append(rec)
    return result


_ID_TOKEN = re.compile(r"[IWO]-(?:[A-Z]+-?[0-9a-z]+|[0-9]+)")


def _expand_id_ranges(cell):
    """Invariant ids named in the ID cell of the Owner invariants table. 'I-SCL-1 to I-SCL-9, I-SCL-12'
    and 'I-CON-a to I-CON-e' expand to every id of the range; a retired row (struck through) has none."""
    if "~~" in cell:
        return []
    text = re.sub(r"[`*]", "", cell)
    ids = []
    tokens = list(_ID_TOKEN.finditer(text))
    i = 0
    while i < len(tokens):
        tok = tokens[i]
        between = text[tok.end(): tokens[i + 1].start()] if i + 1 < len(tokens) else ""
        if i + 1 < len(tokens) and between.strip() == "to":
            a, b = tok.group(0), tokens[i + 1].group(0)
            ma = re.match(r"^(.*?)(\d+|[a-z])$", a)
            mb = re.match(r"^(.*?)(\d+|[a-z])$", b)
            if ma and mb and ma.group(1) == mb.group(1):
                if ma.group(2).isdigit() and mb.group(2).isdigit():
                    for n in range(int(ma.group(2)), int(mb.group(2)) + 1):
                        ids.append("%s%d" % (ma.group(1), n))
                else:
                    for code in range(ord(ma.group(2)), ord(mb.group(2)) + 1):
                        ids.append("%s%s" % (ma.group(1), chr(code)))
                i += 2
                continue
        ids.append(tok.group(0))
        i += 1
    return ids


def parse_invariants(path):
    """Invariants of the TDC GDD Formulas section: the '**I-1**' to '**I-5**' bullets and the rows of the
    'Owner invariants' table (ID | Owner | Constraint | On failure). Returns a list of dicts
    {"ids": [...], "line", "text", "origin": "bullet"|"table", "failure"}. A row whose On failure column
    starts with 'offline' is checked offline by its owner, is not a preset invariant and is left out."""
    out = []
    header = None
    for n, line in section_lines(path, "Formulas"):
        stripped = line.strip()
        m = re.match(r"^- \*\*(I-[0-9]+)\*\*(.*)$", stripped)
        if m:
            out.append({"ids": [m.group(1)], "line": n, "text": m.group(2), "origin": "bullet", "failure": ""})
            continue
        if not stripped.startswith("|"):
            header = None
            continue
        cells = split_row(stripped)
        if header is None:
            header = cells
            continue
        if is_separator(cells):
            continue
        if len(header) < 4 or header[0].strip().lower() != "id" or "constraint" not in header[2].lower() or len(cells) < 4:
            continue
        ids = _expand_id_ranges(cells[0])
        if not ids:
            continue
        failure = cells[3]
        if re.match(r"^offline\b", re.sub(r"[*`]", "", failure).strip(), re.I):
            continue
        out.append({"ids": ids, "line": n, "text": cells[2], "origin": "table", "failure": failure})
    return out


def invariant_keys(invariant, key_set):
    """Key names read by an invariant: every backtick-quoted identifier of its text that is in key_set."""
    names = []
    for span in re.findall(r"`([^`]+)`", invariant["text"]):
        for tok in re.findall(r"[A-Za-z_][A-Za-z0-9_]*", span):
            if tok in key_set and tok not in names:
                names.append(tok)
    return names


def _norm_name(text):
    return re.sub(r"[^a-z0-9]+", " ", text.lower()).strip()


def parse_registered_owner_keys(path):
    """Rows of the TDC 'Registered Owner Keys' table: list of (line, owner_name, count_or_None, cell)."""
    rows = []
    in_section, header = False, None
    for n, line in iter_lines(path):
        if line.startswith("#"):
            if in_section and not line.startswith("### Registered"):
                if re.match(r"^#{1,3} ", line):
                    break
            in_section = line.startswith("### Registered Owner Keys")
            header = None
            continue
        if not in_section or not line.strip().startswith("|"):
            continue
        cells = split_row(line)
        if header is None:
            header = cells
            continue
        if is_separator(cells):
            continue
        first = cells[0]
        owner = re.sub(r"\s*\([^()]*\d[^()]*\)\s*$", "", first).strip()
        keys_cell = cells[1] if len(cells) > 1 else ""
        m = re.match(r"^\s*(\d+)\b", keys_cell)
        rows.append((n, owner, int(m.group(1)) if m else None, keys_cell))
    return rows


def count_observations(scan, owner_rows):
    """(path, line, rule, message) tuples for per-GDD counts that differ from the Registered Owner Keys."""
    by_title = {}
    for rel, title in scan["titles"].items():
        by_title[_norm_name(title)] = rel
    counts = {}
    for k in scan["knobs"]:
        counts[k["gdd"]] = counts.get(k["gdd"], 0) + 1
    obs = []
    seen = set()
    for line, owner, expected, cell in owner_rows:
        rel = by_title.get(_norm_name(owner))
        if rel is None:
            obs.append(("design/gdd/tuning-data-config.md", line, "knob-count",
                        "Registered Owner Keys row '%s' names no GDD with that title" % owner))
            continue
        seen.add(rel)
        got = counts.get(rel, 0)
        if expected is None:
            obs.append(("design/gdd/tuning-data-config.md", line, "knob-count",
                        "Registered Owner Keys row '%s' gives no leading key count; owner table has %d record(s)" % (owner, got)))
        elif expected != got:
            obs.append(("design/gdd/tuning-data-config.md", line, "knob-count",
                        "%s: Registered Owner Keys says %d, the owner Tuning Knobs table has %d record(s) (the owner table is authoritative, CR-15)"
                        % (rel, expected, got)))
    for rel in scan["files"]:
        if counts.get(rel, 0) and rel not in seen:
            obs.append((rel, 0, "knob-count", "owner table has %d record(s) but the Registered Owner Keys table has no row for it" % counts[rel]))
    return obs


def observations(root, gdd_dir="design/gdd", tdc="tuning-data-config.md"):
    """All parser observations for a tree, as (path, line, rule, message), plus the scanned list and the scan."""
    scan = scan_tree(root, gdd_dir)
    obs = []
    for rel in scan["no_section"]:
        obs.append((rel, 0, "knob-no-section", "GDD has no Tuning Knobs section"))
    seen = {}
    for k in scan["knobs"]:
        ident = (k["gdd"], k["key"])
        if ident in seen:
            obs.append((k["gdd"], k["line"], "knob-duplicate-key",
                        "key %s has a second row in this GDD (first at line %d)" % (k["key"], seen[ident])))
        else:
            seen[ident] = k["line"]
    for p in scan["problems"]:
        rule = "knob-row-shape" if p["kind"] == "shape" else "knob-key-unparsed"
        obs.append((p["gdd"], p["line"], rule, p["message"]))
    tdc_path = os.path.join(root, gdd_dir, tdc)
    if os.path.isfile(tdc_path):
        obs.extend(count_observations(scan, parse_registered_owner_keys(tdc_path)))
    return obs, scan


def _emit_obs(obs, scan):
    for rel in scan["files"]:
        print("SCANNED %s" % rel)
    for path, line, rule, message in obs:
        print("%s:%d %s %s" % (path.replace(" ", "_"), line, rule, message))


def main(argv):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", newline=NEWLINE, line_buffering=True)
    root = os.getcwd()
    mode = "observations"
    gdd_dir = "design/gdd"
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--root":
            root = argv[i + 1]
            i += 1
        elif a == "--gdd-dir":
            gdd_dir = argv[i + 1]
            i += 1
        elif a == "--records":
            mode = "records"
        elif a == "--observations":
            mode = "observations"
        else:
            sys.stderr.write("unknown argument: %s\n" % a)
            return 2
        i += 1
    root = os.path.abspath(root)
    obs, scan = observations(root, gdd_dir)
    if mode == "records":
        for t in scan["tables"]:
            print(json.dumps(t, ensure_ascii=False, sort_keys=True))
        for k in scan["knobs"] + scan["refs"] + scan["retired"]:
            print(json.dumps(k, ensure_ascii=False, sort_keys=True))
    else:
        _emit_obs(obs, scan)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
