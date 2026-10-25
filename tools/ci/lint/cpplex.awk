# Shared C++ lexical helper for the T0 lint rules (stories tdc-013 to tdc-015).
#
# Used as:  awk -f cpplex.awk -f <rule>.awk -v root=<root> file...   (see lint_cpp_awk in emit.sh)
# The rule file defines   function rule_file(path)   and prints finding lines through lint_find().
# cpplex.awk reads every file given on the command line and, per file, builds three strings of
# identical length (so one offset addresses all of them) with "\n" between lines:
#   CODE  comments and the CONTENT of string/char literals replaced by spaces (the quotes stay):
#         identifiers, parentheses and punctuation of real code only
#   STR   comments replaced by spaces, string literal content kept: for rules where a name may
#         legitimately sit inside a string (trace and stat names)
#   CMT   only the comment text, everything else spaces: annotations such as LOCAL-PAWN-OK
# Raw strings (R"delim(...)delim") are handled; a digit separator (1'000) is not a char literal.
# Nothing here decides pass or fail. Findings are observations: "<path>:<line> <rule> <message>".

FNR == 1 { if (NR > 1) lex_flush(); lex_name = FILENAME }
{ sub(/\r$/, ""); lex_raw[++lex_n] = $0 }
END { lex_flush() }

function lex_flush() {
  if (lex_n == 0) return
  lex_build()
  rule_file(lex_name)
  lex_n = 0
  delete lex_raw
  delete lex_start
}

# Builds CODE, STR, CMT and lex_start[line] (1-based offset of each line's first character).
function lex_build(   ln, line, n, i, c, d, p, st, code, str, cmt, q, rd, rl, e) {
  CODE = ""; STR = ""; CMT = ""
  st = ""; rd = ""
  for (ln = 1; ln <= lex_n; ln++) {
    line = lex_raw[ln]; n = length(line)
    lex_start[ln] = length(CODE) + 1
    code = ""; str = ""; cmt = ""
    i = 1
    while (i <= n) {
      c = substr(line, i, 1); d = substr(line, i + 1, 1)
      if (st == "B") {                       # block comment
        if (c == "*" && d == "/") { code = code "  "; str = str "  "; cmt = cmt "*/"; i += 2; st = "" }
        else { code = code " "; str = str " "; cmt = cmt c; i++ }
      } else if (st == "R") {                # raw string: ends at )delim"
        e = ")" rd "\""
        if (substr(line, i, length(e)) == e) {
          code = code sprintf("%*s", length(e) - 1, "") "\""; str = str substr(line, i, length(e)); cmt = cmt sprintf("%*s", length(e), "")
          i += length(e); st = ""
        } else { code = code " "; str = str c; cmt = cmt " "; i++ }
      } else if (st == "S" || st == "C") {   # string or char literal
        q = (st == "S") ? "\"" : "'"
        if (c == "\\" && i < n) { code = code "  "; str = str c d; cmt = cmt "  "; i += 2 }
        else if (c == q) { code = code q; str = str q; cmt = cmt " "; i++; st = "" }
        else { code = code " "; str = str c; cmt = cmt " "; i++ }
      } else {                               # code
        if (c == "/" && d == "/") {
          rl = n - i + 1
          code = code sprintf("%*s", rl, ""); str = str sprintf("%*s", rl, ""); cmt = cmt substr(line, i)
          i = n + 1
        } else if (c == "/" && d == "*") {
          code = code "  "; str = str "  "; cmt = cmt "/*"; i += 2; st = "B"
        } else if (c == "\"") {
          p = substr(line, i - 1, 1)
          if (p == "R" && match(substr(line, i + 1), /^[^() \\\t]*\(/)) {
            rd = substr(line, i + 1, RLENGTH - 1)
            code = code "\"" sprintf("%*s", RLENGTH, ""); str = str substr(line, i, RLENGTH + 1); cmt = cmt sprintf("%*s", RLENGTH + 1, "")
            i += RLENGTH + 1; st = "R"
          } else { code = code "\""; str = str "\""; cmt = cmt " "; i++; st = "S" }
        } else if (c == "'") {
          p = substr(line, i - 1, 1)
          if (p ~ /[0-9]/ && d ~ /[0-9A-Za-z]/) { code = code c; str = str c; cmt = cmt " "; i++ }   # digit separator
          else { code = code "'"; str = str "'"; cmt = cmt " "; i++; st = "C" }
        } else { code = code c; str = str c; cmt = cmt " "; i++ }
      }
    }
    if (st == "S" || st == "C") st = ""      # unterminated literal ends with the line
    CODE = CODE code "\n"; STR = STR str "\n"; CMT = CMT cmt "\n"
  }
}

# 1-based line of offset off in CODE/STR/CMT.
function lex_line(off,   lo, hi, mid) {
  lo = 1; hi = lex_n
  while (lo < hi) {
    mid = int((lo + hi + 1) / 2)
    if (lex_start[mid] <= off) lo = mid; else hi = mid - 1
  }
  return lo
}

# Text of line ln from one of the three strings (pass CODE, STR or CMT).
function lex_text_line(s, ln,   len) {
  len = (ln < lex_n ? lex_start[ln + 1] : length(s) + 1) - lex_start[ln] - 1
  return substr(s, lex_start[ln], len)
}

# Offset of the ")" matching the "(" at offset open in s (CODE, so literals cannot unbalance it), or 0.
function lex_balanced(s, open,   depth, i, n, c) {
  depth = 0; n = length(s)
  for (i = open; i <= n; i++) {
    c = substr(s, i, 1)
    if (c == "(") depth++
    else if (c == ")") { depth--; if (depth == 0) return i }
  }
  return 0
}

# True when identifier token tok occurs in text as a whole token.
function lex_has_token(text, tok) {
  return match(text, "(^|[^A-Za-z0-9_])" tok "([^A-Za-z0-9_]|$)") > 0
}

# Path of an absolute file relative to root, with spaces made underscores (the emitter's contract).
function lint_relpath(path,   r) {
  r = root
  sub(/\/+$/, "", r)
  if (index(path, r "/") == 1) path = substr(path, length(r) + 2)
  gsub(/ /, "_", path)
  return path
}

# One finding: "<rel-path>:<line> <rule> <message>".
function lint_find(path, line, rule, msg) {
  printf "%s:%d %s %s\n", lint_relpath(path), line, rule, msg
}
