#!/usr/bin/env python
"""Registry reader and comparison rules shared by the TDC doc lints (stories tdc-030 and tdc-033).

Build-time helper, standard library only, imported by registry_mirror.py and knob_lint.py (run with
`python -I`). It never edits a file and never decides pass or fail.

Reader. design/registry/entities.yaml has one layout (see its LAYOUT header): sections are keys at
column 0, a list entry starts "  - name: <name>", its fields sit at column 4, a field's block list
items at column 6, and comment lines start with '#'. PyYAML is not part of the build environment, so
this reader understands exactly that layout and reports every other line of the constants section as
a problem with its line number (an unsupported layout is never read silently). Scalars come back as
the text of the value (quotes removed, an inline " # comment" dropped); the reader does not coerce
types, so `true` stays "true" and `0.20` stays "0.20".

Comparison rules (story tdc-033, the AC does not define them; decided against the real tree and
covered by the selftest). `same_cell(gdd_cell, registry_value)` is true when, after normalising both:
  1. Markup is stripped: backticks, bold/italic asterisks, strikethrough tildes.
  2. Parenthetical text is dropped, nesting included: "100-250 (TDC canonical ...)" equals "100-250".
     Annotations are commentary on the value, not the value; the numbers and words outside the
     parentheses must still agree.
  3. Numbers are canonical: thousands separators removed (4,096 = 4096) and trailing zeros dropped
     (60 = 60.0 = 60.00, 0.40 = 0.4).
  4. Range spelling is canonical: "10 to 20", "10 - 20" and "10-20" are one range; ", or " is " or ".
  5. Case and spacing are ignored (true = True).
  6. A placeholder cell (an em dash, "--", "-", "n/a" or an empty cell) equals an absent registry field.
Nothing else is equal: a different number, word, key name or an extra range part is a mismatch. The
normaliser never rewrites the prose that remains, so a registry range that says something the owner row
does not (for example "true, false" against "3,000-6,000") is reported.
"""
import re
import sys
from decimal import Decimal, InvalidOperation

PLACEHOLDERS = {"", "-", "--", "—", "–", "n/a", "na", "none"}


def _strip_inline_comment(text):
    quote = None
    for i, ch in enumerate(text):
        if quote:
            if ch == "\\" and quote == '"':
                continue
            if ch == quote:
                quote = None
        elif ch in "\"'":
            quote = ch
        elif ch == "#" and (i == 0 or text[i - 1] in " \t"):
            return text[:i].rstrip()
    return text.rstrip()


def parse_scalar(raw):
    """Text of a YAML scalar as the registry writes it (quotes removed, escapes of a double-quoted
    string resolved for \\\\ and \\" only)."""
    text = _strip_inline_comment(raw.strip())
    if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
        return text[1:-1].replace('\\"', '"').replace("\\\\", "\\")
    if len(text) >= 2 and text[0] == "'" and text[-1] == "'":
        return text[1:-1].replace("''", "'")
    return text


def read_constants(path):
    """Constants of the registry. Returns (constants, problems). A constant is
    {"name", "line", "fields": {field: text}, "field_lines": {field: line}, "lists": {field: [text]}}.
    problems is a list of (line, message) for lines the reader does not understand."""
    constants, problems = [], []
    section = None
    current = None
    last_field = None
    with open(path, "r", encoding="utf-8", errors="replace", newline=None) as handle:
        for n, raw in enumerate(handle, 1):
            line = raw.rstrip("\n").rstrip("\r")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$", line)
            if m and not line.startswith(" "):
                section = m.group(1)
                current, last_field = None, None
                continue
            if section != "constants":
                continue
            m = re.match(r"^  - name:\s*(.+)$", line)
            if m:
                current = {"name": parse_scalar(m.group(1)), "line": n, "fields": {}, "field_lines": {}, "lists": {}}
                constants.append(current)
                last_field = None
                continue
            m = re.match(r"^    ([A-Za-z_][A-Za-z0-9_]*):\s*(.*)$", line)
            if m and current is not None:
                last_field = m.group(1)
                current["fields"][last_field] = parse_scalar(m.group(2))
                current["field_lines"][last_field] = n
                continue
            m = re.match(r"^      - (.+)$", line)
            if m and current is not None and last_field:
                current["lists"].setdefault(last_field, []).append(parse_scalar(m.group(1)))
                continue
            problems.append((n, "unsupported layout: %s" % line.strip()[:80]))
    return constants, problems


# ---- comparison -----------------------------------------------------------------------------------

def _drop_parentheticals(text):
    out, depth = [], 0
    for ch in text:
        if ch == "(":
            depth += 1
        elif ch == ")":
            if depth:
                depth -= 1
        elif depth == 0:
            out.append(ch)
    return "".join(out)


def _canon_number(match):
    token = match.group(0).replace(",", "")
    try:
        value = Decimal(token)
    except InvalidOperation:
        return match.group(0)
    text = format(value.normalize(), "f")
    if "." in text:
        text = text.rstrip("0").rstrip(".")
    return text or "0"


def normalize(text, compare=True):
    """Canonical form of a GDD cell or a registry value (rules 1-5 of the module header). compare=True is
    the comparison form (numbers canonical, case folded); compare=False is the mirror form the registry
    mirror writes (markup, parentheticals and thousands separators removed, range spelling canonical,
    number digits and case kept as the owner table has them)."""
    s = str(text)
    s = re.sub(r"[`*~]", "", s)
    s = _drop_parentheticals(s)
    s = s.replace(chr(0x2013), "-").replace(chr(0x2212), "-")
    s = re.sub(r"(?<=\d)(,\d{3})+(?!\d)", lambda m: m.group(0).replace(",", ""), s)
    if compare:
        s = re.sub(r"\d+(?:\.\d+)?", _canon_number, s)
    s = re.sub(r"(?<=[\d}])\s+to\s+(?=[\d{-])", "-", s)
    s = re.sub(r"(?<=\d)\s*-\s*(?=\d)", "-", s)
    s = re.sub(r",\s+or(?![A-Za-z])", " or", s)
    s = re.sub(r"\s+", " ", s).strip()
    return s.casefold() if compare else s


def parentheticals(text):
    """The text of the top-level parenthetical groups of a cell, in order (markup removed)."""
    out, depth, cur = [], 0, []
    for ch in re.sub(r"[`*~]", "", str(text)):
        if ch == "(":
            if depth:
                cur.append(ch)
            depth += 1
        elif ch == ")":
            if depth:
                depth -= 1
                if depth:
                    cur.append(ch)
                else:
                    out.append("".join(cur).strip())
                    cur = []
        elif depth:
            cur.append(ch)
    return [o for o in out if o]


_SAFE_PLAIN = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_./+=<> -]*$")
_YAML_WORDS = {"yes", "no", "on", "off", "null", "true", "false", "y", "n", "~"}


def yaml_scalar(text, allow_bool=False):
    """A YAML scalar for text: plain when that parses back to the same string (or, for a number or
    allow_bool, to the same number or boolean), else double-quoted."""
    s = str(text)
    if re.fullmatch(r"-?\d+(\.\d+)?", s):
        return s
    if allow_bool and s in ("true", "false"):
        return s
    if _SAFE_PLAIN.match(s) and s.casefold() not in _YAML_WORDS and not s.endswith(" ") and not re.fullmatch(r"\d{4}-\d\d-\d\d", s):
        return s
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def is_placeholder(text):
    return normalize(text) in PLACEHOLDERS or str(text).strip() in PLACEHOLDERS


def same_cell(gdd_cell, registry_value):
    """True when an owner-table cell and a registry field are equal under the comparison rules.
    registry_value is None for an absent field."""
    g_empty = is_placeholder(gdd_cell)
    r_empty = registry_value is None or is_placeholder(registry_value)
    if g_empty or r_empty:
        return g_empty and r_empty
    return normalize(gdd_cell) == normalize(registry_value)


def main(argv):
    if len(argv) != 1:
        sys.stderr.write("usage: registry_io.py <entities.yaml>   (prints the constant count and problems)\n")
        return 2
    constants, problems = read_constants(argv[0])
    print("constants %d, problems %d" % (len(constants), len(problems)))
    for n, msg in problems:
        print("%s:%d %s" % (argv[0], n, msg))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
