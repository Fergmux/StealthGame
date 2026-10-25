# Rule (i) of the include lint (ADR-0007 sections 3 and 6; story tdc-015), run through cpplex.awk:
# no log, check or trace macro argument may name an identifier from the forbidden list
# (list file passed as -v list=..., tools/ci/lint/log-forbidden-identifiers.txt). The list has one
# entry per line; an entry ending in "*" matches as a PREFIX of an identifier token, any other entry
# matches the WHOLE token only; "#" starts a comment.
# Macros inspected: UE_LOG UE_LOGF UE_CLOG UE_LOGFMT ensureMsgf ensureAlwaysMsgf checkf verifyf
# ClientMessage SCOPE_CYCLE_COUNTER and the families TRACE_* and CSV_*. The argument text runs to the
# balanced closing parenthesis (it may span lines and nest). For the log macros only code tokens are
# read (a string literal is message text); for TRACE_*, CSV_* and SCOPE_CYCLE_COUNTER the words
# inside string literals are read too, because those macros take their NAMES as strings.
# A forbidden name outside a macro argument is not a finding.
BEGIN {
  nprefix = 0
  while ((getline entry < list) > 0) {
    sub(/\r$/, "", entry); sub(/^[ \t]+/, "", entry); sub(/[ \t]+$/, "", entry)
    if (entry == "" || substr(entry, 1, 1) == "#") continue
    if (substr(entry, length(entry)) == "*") prefix[++nprefix] = substr(entry, 1, length(entry) - 1)
    else exact[entry] = 1
  }
  close(list)
}
function forbidden_token(tok,   i) {
  if (tok in exact) return 1
  for (i = 1; i <= nprefix; i++) if (index(tok, prefix[i]) == 1) return 1
  return 0
}
function rule_file(path,   pos, abs, txt, name, open, endp, ln, rest, tok, namedstr, seen, off) {
  pos = 1
  while (match(substr(CODE, pos), /(^|[^A-Za-z0-9_])(UE_LOGF|UE_LOGFMT|UE_LOG|UE_CLOG|ensureAlwaysMsgf|ensureMsgf|checkf|verifyf|ClientMessage|SCOPE_CYCLE_COUNTER|TRACE_[A-Za-z0-9_]*|CSV_[A-Za-z0-9_]*)[ \t\n]*\(/)) {
    abs = pos + RSTART - 1
    txt = substr(CODE, abs, RLENGTH)
    open = abs + RLENGTH - 1
    sub(/^[^A-Za-z0-9_]/, "", txt)
    name = txt; sub(/[ \t\n]*\($/, "", name)
    endp = lex_balanced(CODE, open)
    if (endp == 0) { pos = open + 1; continue }
    namedstr = (name ~ /^(TRACE_|CSV_)/ || name == "SCOPE_CYCLE_COUNTER")
    rest = substr(namedstr ? STR : CODE, open + 1, endp - open - 1)
    delete seen
    off = open
    while (match(rest, /[A-Za-z_][A-Za-z0-9_]*/)) {
      tok = substr(rest, RSTART, RLENGTH)
      ln = lex_line(off + RSTART)
      off += RSTART + RLENGTH - 1
      rest = substr(rest, RSTART + RLENGTH)
      if (forbidden_token(tok) && !(tok in seen)) {
        seen[tok] = 1
        lint_find(path, ln, "i", name " argument names forbidden identifier " tok " (tools/ci/lint/log-forbidden-identifiers.txt)")
      }
    }
    pos = endp + 1
  }
}
