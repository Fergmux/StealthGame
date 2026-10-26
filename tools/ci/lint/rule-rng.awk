# Forbidden-RNG rule "rng" (ADR-0006 Implementation Guideline 1 and Validation Criteria; story st-012), run through
# cpplex.awk over each StealthFoundation and StealthGameplay source file. Code only (comments and string literal
# content are not code). Flags:
#   FMath::Rand*, FMath::SRand*, FMath::FRand*, FMath::VRand*   the engine's global RNG (FMath::Rand, FRand,
#                                                            RandRange, FRandRange, RandBool, RandInit, SRand, VRand...)
#   FRandomStream                                            the engine's seedable stream (the prototype's choice)
#   std::rand, std::srand and the <random> engines and distributions (std::mt19937, std::random_device,
#   std::default_random_engine, std::*_distribution, ...)
# Every draw comes from FSecretStream or FIntStream (ADR-0006 section 1). A file listed in the allowlist
# (list=<path>, one repo-relative path per line, "#" comments) is skipped: the MVP list is empty.
BEGIN {
  if (list != "") {
    while ((getline line < list) > 0) {
      sub(/\r$/, "", line); sub(/#.*/, "", line); gsub(/[ \t]/, "", line)
      if (line != "") allow[line] = 1
    }
  }
}
function rule_file(path,   rel, ln, code, rest, m) {
  rel = path
  if (index(rel, root "/") == 1) rel = substr(rel, length(root) + 2)
  if (rel in allow) return
  for (ln = 1; ln <= lex_n; ln++) {
    code = lex_text_line(CODE, ln)
    rest = code
    while (match(rest, /FMath[ \t]*::[ \t]*(S?Rand|FRand|VRand)[A-Za-z0-9_]*/)) {
      m = substr(rest, RSTART, RLENGTH); gsub(/[ \t]/, "", m)
      lint_find(path, ln, "rng", "uses " m " (engine RNG; draws come from FSecretStream or FIntStream, ADR-0006)")
      rest = substr(rest, RSTART + RLENGTH)
    }
    rest = code
    while (match(rest, /(^|[^A-Za-z0-9_])FRandomStream([^A-Za-z0-9_]|$)/)) {
      lint_find(path, ln, "rng", "uses FRandomStream (draws come from FSecretStream or FIntStream, ADR-0006)")
      rest = substr(rest, RSTART + RLENGTH)
    }
    rest = code
    while (match(rest, /std[ \t]*::[ \t]*(s?rand|mt19937(_64)?|minstd_rand0?|ranlux[A-Za-z0-9_]*|knuth_b|random_device|default_random_engine|[A-Za-z_]*_engine|[A-Za-z_]+_distribution)([^A-Za-z0-9_]|$)/)) {
      m = substr(rest, RSTART, RLENGTH); sub(/[^A-Za-z0-9_]$/, "", m); gsub(/[ \t]/, "", m)
      lint_find(path, ln, "rng", "uses " m " (C++ standard RNG; draws come from FSecretStream or FIntStream, ADR-0006)")
      rest = substr(rest, RSTART + RLENGTH)
    }
  }
}
