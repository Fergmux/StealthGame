#!/usr/bin/env bash
# No-caller-identity grep (ADR-0011 section 1, IIF R2, GDD AC 2; story iif-006). Emits observations only, in the
# tools/ci/lint/emit.sh contract; tools/ci/lint/gate.sh decides:
#   bash tools/ci/lint/gate.sh tools/ci/check-no-caller-identity.sh
#
# Scope: the pawn command interface source of StealthGameplay, i.e. the TOP-LEVEL files of Public/Server/Core and
# Private/Core (the command component, its core, the predicate and reply seams, the action hub) plus
# Public/Server/ActionHub.h. Subfolders (Round/, SharedPawn/, PawnFactory/, Animation/) belong to other systems and
# are not scanned, nor is StealthPawnFactory.h (the ADR-0010 spawn factory, which binds pawns through the Roster by
# design). Generated headers (*.gen.h, *.generated.h) are excluded.
#
# Rule "caller-identity": a code token (comments are stripped, so a doc comment naming a forbidden call is not a
# finding) matching IsPlayerControlled | IsLocallyControlled | IsBotControlled | IsNpc | GetTeam | NM_ListenServer,
# or a Roster binding query: an include of a Server/Roster/ header or the FRosterCore / URosterStore types.
#
# Usage: check-no-caller-identity.sh [<root>]   root defaults to the repo root (a fixture tree has the same layout)
set -u
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lint/emit.sh
. "$script_dir/lint/emit.sh"

root="$(lint_root "${1:-}")"
module="$root/Source/StealthGameplay"
pattern='IsPlayerControlled|IsLocallyControlled|IsBotControlled|IsNpc|GetTeam|NM_ListenServer|FRosterCore|URosterStore|Server/Roster/'

files=()
for dir in "$module/Public/Server/Core" "$module/Private/Core"; do
  [ -d "$dir" ] || continue
  while IFS= read -r f; do files+=("$f"); done < <(find "$dir" -maxdepth 1 -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.inl' \) ! -name '*.gen.h' ! -name '*.generated.h' ! -name 'StealthPawnFactory.h' | sort)
done
[ -f "$module/Public/Server/ActionHub.h" ] && files+=("$module/Public/Server/ActionHub.h")

for f in "${files[@]}"; do
  rel="$(lint_rel "$root" "$f")"
  lint_scanned "$rel"
  # Strip block and line comments (string literals are left alone: none of the patterns belongs in one).
  awk '
    {
      line = $0; out = ""
      while (length(line) > 0) {
        if (inblock) {
          e = index(line, "*/")
          if (e == 0) { line = ""; break }
          line = substr(line, e + 2); inblock = 0; continue
        }
        b = index(line, "/*"); l = index(line, "//")
        if (l > 0 && (b == 0 || l < b)) { out = out substr(line, 1, l - 1); line = ""; break }
        if (b > 0) { out = out substr(line, 1, b - 1); line = substr(line, b + 2); inblock = 1; continue }
        out = out line; line = ""
      }
      printf "%d\t%s\n", NR, out
    }' "$f" | while IFS=$'\t' read -r n code; do
    if printf '%s' "$code" | grep -Eq "$pattern"; then
      hit="$(printf '%s' "$code" | grep -Eo "$pattern" | head -1)"
      lint_finding "$rel" "$n" caller-identity "forbidden identity call or Roster binding query: $hit"
    fi
  done
done
exit 0
