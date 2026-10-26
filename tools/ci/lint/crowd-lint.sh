#!/usr/bin/env bash
# Crowd lint (ADR-0016 section 2 and Implementation Guidelines, ADR-0001 section 2; stories cnb-005, cnb-008, cnb-013).
# Emits observations only (see emit.sh); tools/ci/lint/gate.sh decides. Rule ids "crowd-api" and "crowd-include"
# (documented in rule-crowd.awk): Crowd code reaches a pawn only through the sinks' command-component entry points, and
# includes no Roster, Objectives-assignment, Visibility Policy fact or engine perception header.
#
# Scope: every .h/.cpp/.inl under Source/StealthGameplay/Features/Crowd/ (tests under Private/Tests are outside the
# feature folder and may name the forbidden APIs, for instance in a spy).
#
# Usage: crowd-lint.sh [<root>]    root defaults to the repo root; a fixture tree has the same layout
set -u
. "$(cd "$(dirname "$0")" && pwd)/emit.sh"
root="$(lint_root "${1:-}")"

files=()
dir="$root/Source/StealthGameplay/Features/Crowd"
if [ -d "$dir" ]; then
  while IFS= read -r file; do
    [ -n "$file" ] || continue
    files+=("$file")
    lint_scanned "${file#"$root"/}"
  done < <(find "$dir" -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.inl' \) | LC_ALL=C sort)
fi

if [ ${#files[@]} -gt 0 ]; then
  lint_cpp_awk rule-crowd.awk "$root" "${files[@]}"
fi

# crowd-include, transitive (story cnb-013, AC-CNB-01: "transitive includes are checked"): every project header a Crowd
# file reaches through #include chains is resolved by path suffix among the project's headers (Source/<module>/...);
# reaching a Roster query header (Server/Roster/*, the Roster team view), an Objectives-assignment header or a Visibility
# Policy fact header is reported at the Crowd file's first include on that chain. Engine headers are not followed.
# One find, one include scan (lint_includes over every project header plus the Crowd files) and one awk pass: the
# header index by trailing path, then a breadth-first walk per direct project include (no per-header subprocess).
hdrs=()
while IFS= read -r h; do
  [ -n "$h" ] && hdrs+=("$h")
done < <(find "$root/Source" -type f -name '*.h' 2>/dev/null | grep -v '/Intermediate/' | LC_ALL=C sort)
if [ ${#files[@]} -gt 0 ]; then
  {
    for h in "${hdrs[@]}"; do printf 'H\t%s\n' "${h#"$root"/}"; done
    lint_includes "${hdrs[@]}" "${files[@]}" | sed 's/^/I\t/'
  } | LC_ALL=C awk -F'\t' -v root="$root" -v crowd="Source/StealthGameplay/Features/Crowd/" '
    function forbidden(p) {
      return p ~ /\/Server\/Roster\// || p ~ /StealthRosterTeamView\.h$/ || p ~ /Objective/ || p ~ /SiteAssignment/ || p ~ /Visibility/ || p ~ /PolicyFact/
    }
    function relpath(p) { gsub(/[\\]/, "/", p); if (index(p, root "/") == 1) p = substr(p, length(root) + 2); return p }
    function resolve(inc) { gsub(/[\\]/, "/", inc); return (inc in idx) ? idx[inc] : "" }
    $1 == "H" {
      n = split($2, part, "/"); b = part[n]
      if (!(b in idx)) idx[b] = $2
      if (n >= 2) { k = part[n - 1] "/" b; if (!(k in idx)) idx[k] = $2 }
      if (n >= 3) { k = part[n - 2] "/" part[n - 1] "/" b; if (!(k in idx)) idx[k] = $2 }
      next
    }
    $1 == "I" {
      f = relpath($2); c = ++ninc[f]; inc[f, c] = $4; at[f, c] = $3
      if (index(f, crowd) == 1) crowdfile[f] = 1
    }
    END {
      for (f in crowdfile) {
        for (i = 1; i <= ninc[f]; i++) {
          start = resolve(inc[f, i])
          if (start == "" || forbidden(start)) continue # a direct forbidden include is rule-crowd.awk finding
          delete seen; head = 1; tail = 1; q[1] = start; seen[start] = 1; hit = ""
          while (head <= tail && hit == "") {
            cur = q[head++]
            for (j = 1; j <= ninc[cur]; j++) {
              nx = resolve(inc[cur, j])
              if (nx == "" || (nx in seen)) continue
              seen[nx] = 1
              if (forbidden(nx)) { hit = nx " via " cur; break }
              q[++tail] = nx
            }
          }
          if (hit != "") printf "%s\t%s\t%s\n", f, at[f, i], hit
        }
      }
    }' | LC_ALL=C sort -t$'\t' -k1,1 -k2,2n | while IFS=$'\t' read -r rel line hit; do
      lint_finding "$rel" "$line" crowd-include "transitively includes $hit (identity blindness, CNB-R2)"
    done
fi
exit 0
