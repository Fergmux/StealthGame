#!/usr/bin/env bash
# Include lint rules (a)-(c) (ADR-0007 section 3; story tdc-012). Emits observations only (see
# emit.sh); tools/ci/lint/gate.sh decides.
#
#   (a) a header in a module's Public/ folder that is not under Public/View/ or Public/Server/,
#       except the module header (<Module>Module.h) and, in StealthFoundation,
#       StealthServerOnlyGuard.h
#   (b) a Public/Server/ header whose first #include is not "StealthServerOnlyGuard.h" (comments
#       and #pragma once before it are fine)
#   (c) a Public/View/ header that includes a Server/ header (an include path whose first segment,
#       or any later segment, is Server/)
#   shrink-only: tools/ci/lint/prototype-exempt.txt gained an entry relative to the base revision
#       (G3; base = $LINT_BASE_REF, else HEAD)
#
# Scope (G5): Source/<M>/Public/**.h for the scope modules in emit.sh, plus the prototype module
# Source/StealthGame, whose pre-ADR-0007 headers are listed in prototype-exempt.txt. A header
# listed there is skipped by (a) and (b); a prototype header with no entry is reported. Rule ids
# are the ADR-0007 letters so the seeded-control story (tdc-016) can assert one finding per letter.
#
# Usage: include-lint-abc.sh [<root>]    root defaults to the repo root; a fixture tree has the same
#                                        layout (Source/..., optional tools/ci/lint/prototype-exempt.txt)
set -u
. "$(cd "$(dirname "$0")" && pwd)/emit.sh"
root="$(lint_root "${1:-}")"
exempt_rel="tools/ci/lint/prototype-exempt.txt"
exempt_file="$root/$exempt_rel"

is_exempt() {
  [ -f "$exempt_file" ] && tr -d '\r' < "$exempt_file" | grep -v '^#' | grep -Fxq -- "$1"
}

# Prints "<line>\t<include target>" for every #include outside comments, in order.
list_includes() {
  awk '
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
        printf "%d\t%s\n", NR, inc
      }
    }' "$1" | tr -d '\r'
}

check_header() { # module public-dir abs-file
  local module="$1" pub="$2" file="$3"
  local rel sub
  rel="$(lint_rel "$root" "$file")"
  sub="${file#"$pub"/}"
  lint_scanned "$rel"
  case "$sub" in
    View/*)
      while IFS=$'\t' read -r n inc; do
        [ -z "$n" ] && continue
        case "/$inc" in
          */Server/*) lint_finding "$rel" "$n" c "View header includes server header \"$inc\"" ;;
        esac
      done < <(list_includes "$file")
      ;;
    Server/*)
      is_exempt "$rel" && return 0
      local first
      first="$(list_includes "$file" | head -n 1)"
      if [ -z "$first" ]; then
        lint_finding "$rel" 1 b "Server header does not include StealthServerOnlyGuard.h first (no #include found)"
      elif [ "${first#*$'\t'}" != "StealthServerOnlyGuard.h" ]; then
        lint_finding "$rel" "${first%%$'\t'*}" b "first include is \"${first#*$'\t'}\", must be \"StealthServerOnlyGuard.h\""
      fi
      ;;
    *)
      is_exempt "$rel" && return 0
      if [ "$sub" = "${module}Module.h" ]; then return 0; fi
      if [ "$module" = "StealthFoundation" ] && [ "$sub" = "StealthServerOnlyGuard.h" ]; then return 0; fi
      lint_finding "$rel" 1 a "header in Public/ outside View/ and Server/ (only the module header and the guard may sit in Public/)"
      ;;
  esac
}

for module in $(lint_scope_modules) $(lint_prototype_module); do
  pub="$root/Source/$module/Public"
  [ -d "$pub" ] || continue
  while IFS= read -r file; do
    [ -n "$file" ] && check_header "$module" "$pub" "$file"
  done < <(find "$pub" -type f -name '*.h' | LC_ALL=C sort)
done

lint_shrink_only "$root" "$exempt_rel" shrink-only
exit 0
