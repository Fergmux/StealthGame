#!/usr/bin/env bash
# Shared observation emitter for the T0 lint rules (ADR-0008 sections 1 and 8, story tdc-011).
#
# Rule scripts SOURCE this file. It prints observations only and never decides pass or fail:
# tools/ci/lint/gate.sh is the only decider (the .claude/scripts rule: helpers emit observations,
# never verdicts). Nothing here exits nonzero because of a finding.
#
# Observation contract (stable; stories 012-016, 024, 025, 029 and 092 build on it). Every line
# a rule script prints on stdout is exactly one of:
#   <path>:<line> <rule> <message>    a finding. <path> is repo-relative (relative to the root the
#                                     rule was given) with forward slashes and no spaces; <line> is
#                                     1-based (0 when the finding is about the file as a whole);
#                                     <rule> is a short id without spaces (an ADR rule letter such
#                                     as "a", or a name such as "shrink-only").
#   SCANNED <path>                    one input file the rule examined (the gate counts these, so a
#                                     rule that examined nothing is NOT ASSESSED, never a pass).
# Anything else belongs on stderr. The gate treats any other stdout line as malformed.
#
# Functions:
#   lint_root [<dir>]                    absolute root to scan: <dir> when given, else the repo root
#   lint_rel <root> <abs-path>           path relative to <root>, forward slashes
#   lint_finding <path> <line> <rule> <message...>
#   lint_scanned <path>
#   lint_scope_modules                   the G5 scan scope (module folder names under Source/)
#   lint_prototype_module                the prototype module (scanned only where a rule says so)
#   lint_shrink_only <root> <rel-file> <rule> [<base-ref>]
#                                        G3: findings for entries of <rel-file> absent at the base
#   lint_includes <abs-file>...          "<abs-file><TAB><line><TAB><target>" per #include outside comments
#   lint_module_files <root> <module>    every .h/.cpp/.inl under <root>/Source/<module>, sorted
#   lint_is_exempt <root> <rel-path>     0 when <rel-path> is listed in prototype-exempt.txt (G3 list)
#   lint_cpp_awk <rule.awk> <root> <abs-file>...
#                                        runs a C++-aware rule (cpplex.awk + <rule.awk>) over the files

# Repository root of this checkout (tools/ci/lint/ is three levels below it).
LINT_REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
# Folder of the shared lint helpers (emit.sh, cpplex.awk, the rule scripts).
LINT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

lint_root() {
  if [ -n "${1:-}" ]; then
    (cd "$1" && pwd)
  else
    printf '%s\n' "$LINT_REPO_ROOT"
  fi
}

lint_rel() {
  local root="${1%/}" path="$2"
  path="${path//\\//}"
  case "$path" in
    "$root"/*) path="${path#"$root"/}" ;;
  esac
  printf '%s\n' "$path"
}

lint_finding() {
  local path="$1" line="$2" rule="$3"
  shift 3
  printf '%s:%s %s %s\n' "${path// /_}" "$line" "$rule" "$*"
}

lint_scanned() {
  printf 'SCANNED %s\n' "${1// /_}"
}

lint_scope_modules() {
  printf '%s\n' StealthFoundation StealthGameplay StealthPresentation StealthGameEditor StealthTestHarness
}

lint_prototype_module() {
  printf '%s\n' StealthGame
}

# G3 shrink-only comparison. Lists every non-empty, non-comment line of <root>/<rel-file> that is
# absent from the same file at the base revision, as a finding of <rule>. The base is <base-ref>
# when given, else $LINT_BASE_REF, else HEAD (the pre-commit case). CI passes the push's
# before-SHA (github.event.before) or HEAD~1. When the file does not exist at the base (its
# creating commit) nothing is reported. When <root> is not a git checkout, or the base revision
# cannot be resolved, one finding says so: an unverifiable baseline is never a silent pass.
lint_shrink_only() {
  local root="$1" rel="$2" rule="$3" base="${4:-${LINT_BASE_REF:-HEAD}}"
  local file="$root/$rel"
  [ -f "$file" ] || return 0
  if ! git -C "$root" rev-parse --verify --quiet "$base^{commit}" >/dev/null 2>&1; then
    lint_finding "$rel" 0 "$rule" "shrink-only baseline: base revision '$base' cannot be resolved in $root"
    return 0
  fi
  local prefix
  prefix="$(git -C "$root" rev-parse --show-prefix 2>/dev/null)"
  local base_text
  if ! base_text="$(git -C "$root" show "$base:$prefix$rel" 2>/dev/null)"; then
    return 0 # created after the base: the creating commit is exempt
  fi
  local n=0 line
  while IFS= read -r line || [ -n "$line" ]; do
    n=$((n + 1))
    line="${line%$'\r'}"
    case "$line" in ''|'#'*) continue ;; esac
    if ! printf '%s\n' "$base_text" | tr -d '\r' | grep -Fxq -- "$line"; then
      lint_finding "$rel" "$n" "$rule" "entry '$line' is not in the list at $base; the list may only shrink (G3)"
    fi
  done < "$file"
}

# Every #include outside comments, one line per include: "<abs-file><TAB><line><TAB><target>"
# (target without the quotes or angle brackets). One awk run for any number of files.
lint_includes() {
  [ $# -gt 0 ] || return 0
  LC_ALL=C awk '
    FNR == 1 { inblock = 0 }
    {
      line = $0; out = ""
      while (length(line) > 0) {
        if (inblock) {
          e = index(line, "*/"); if (e == 0) { line = ""; break }
          line = substr(line, e + 2); inblock = 0
        } else {
          b = index(line, "/*"); s = index(line, "//")
          if (s > 0 && (b == 0 || s < b)) { out = out substr(line, 1, s - 1); line = ""; break }
          if (b == 0) { out = out line; line = ""; break }
          out = out substr(line, 1, b - 1); line = substr(line, b + 2); inblock = 1
        }
      }
      if (match(out, /^[ \t]*#[ \t]*include[ \t]*[<"][^>"]*[>"]/)) {
        inc = substr(out, RSTART, RLENGTH); sub(/^[ \t]*#[ \t]*include[ \t]*[<"]/, "", inc); sub(/[>"]$/, "", inc)
        printf "%s\t%d\t%s\n", FILENAME, FNR, inc
      }
    }' "$@" | tr -d '\r'
}

lint_module_files() {
  local root="$1" module="$2"
  [ -d "$root/Source/$module" ] || return 0
  find "$root/Source/$module" -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.inl' \) | LC_ALL=C sort
}

lint_is_exempt() {
  local file="$1/tools/ci/lint/prototype-exempt.txt"
  [ -f "$file" ] && tr -d '\r' < "$file" | grep -v '^#' | grep -Fxq -- "$2"
}

# Runs a C++-aware rule: cpplex.awk strips comments and string contents and calls the rule's
# rule_file(path) once per file; the rule prints finding lines. -v root=<root> is set for the rule
# and LINT_LIST (a rule-specific side file, optional) is passed as -v list=.
lint_cpp_awk() {
  local rule="$1" root="$2"; shift 2
  [ $# -gt 0 ] || return 0
  LC_ALL=C awk -v root="$root" -v list="${LINT_LIST:-}" -f "$LINT_DIR/cpplex.awk" -f "$LINT_DIR/$rule" "$@" | tr -d '\r'
}
