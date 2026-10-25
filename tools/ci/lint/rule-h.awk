# Rule (h) of the include lint (ADR-0007 section 3; story tdc-014), run through cpplex.awk over each
# StealthPresentation source file: presentation code may not discover servers or other players by
# searching the world. Flagged identifiers (code only; comments and string literals are not code):
#   TActorIterator TActorRange GetAllActorsOfClass GetAllActorsWithInterface
#   GetPlayerControllerIterator GetGameMode GetAuthGameMode
# and every GetPlayerState or GetController use, which a text lint cannot tell from a use on the
# local pawn, so each such line needs a "// LOCAL-PAWN-OK: <reason>" comment on the SAME line. The
# annotation exempts only GetPlayerState/GetController, never the other APIs. An annotation with an
# empty reason is itself a finding (also on a line with no such call).
BEGIN {
  n = split("TActorIterator TActorRange GetAllActorsOfClass GetAllActorsWithInterface GetPlayerControllerIterator GetGameMode GetAuthGameMode", a, " ")
  for (i = 1; i <= n; i++) forbidden[a[i]] = 1
  pawn["GetPlayerState"] = 1; pawn["GetController"] = 1
}
function rule_file(path,   ln, code, cmt, rest, tok, pawncall, reason, empty, good) {
  for (ln = 1; ln <= lex_n; ln++) {
    code = lex_text_line(CODE, ln); cmt = lex_text_line(CMT, ln)
    pawncall = ""
    rest = code
    while (match(rest, /[A-Za-z_][A-Za-z0-9_]*/)) {
      tok = substr(rest, RSTART, RLENGTH)
      rest = substr(rest, RSTART + RLENGTH)
      if (tok in forbidden) lint_find(path, ln, "h", "presentation code uses " tok " (no world searches or game-mode access; ADR-0007 rule h)")
      else if ((tok in pawn) && pawncall == "") pawncall = tok
    }
    empty = 0; good = 0
    if (match(cmt, /LOCAL-PAWN-OK:/)) {
      reason = substr(cmt, RSTART + RLENGTH)
      sub(/\*\/.*$/, "", reason)
      gsub(/[ \t]/, "", reason)
      if (reason == "") empty = 1; else good = 1
    }
    if (pawncall != "" && !good) {
      if (empty) lint_find(path, ln, "h", pawncall " use has a LOCAL-PAWN-OK annotation with an empty reason")
      else lint_find(path, ln, "h", pawncall " use needs a // LOCAL-PAWN-OK: <reason> annotation on the same line")
    } else if (empty && pawncall == "") {
      lint_find(path, ln, "h", "LOCAL-PAWN-OK annotation has an empty reason")
    }
  }
}
