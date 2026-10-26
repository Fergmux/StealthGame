#!/usr/bin/env bash
# Forbidden-RNG CI grep (ADR-0006 Implementation Guideline 1 and Validation Criteria; story st-012). Emits
# observations only (see emit.sh); tools/ci/lint/gate.sh decides. Rule id "rng".
#
#   code that names FMath::Rand*/SRand*/FRand*/VRand*, FRandomStream, std::rand/std::srand or a <random> engine or
#   distribution, and any #include <random>, in StealthFoundation and StealthGameplay sources (tests included).
#
# Scope: the ADR-0006 modules (StealthFoundation, StealthGameplay). The prototype module Source/StealthGame (whose
# AStealthCityGenerator still uses FRandomStream until the procedural-city-generator epic migrates it) is outside
# the scope by construction, so no prototype allowlist entry is needed. The allowlist
# tools/ci/lint/rng-allowlist.txt (repo-relative paths) is for engine-facing cosmetic code that cannot affect
# simulation or presentation parity; it is empty in the MVP. The Generator include rule (j) is include-lint-ij.sh.
#
# Usage: forbidden-rng.sh [<root>]    root defaults to the repo root; a fixture tree has the same layout
set -u
. "$(cd "$(dirname "$0")" && pwd)/emit.sh"
root="$(lint_root "${1:-}")"
list="$root/tools/ci/lint/rng-allowlist.txt"
[ -f "$list" ] || list="$LINT_DIR/rng-allowlist.txt"

files=()
for module in StealthFoundation StealthGameplay; do
  while IFS= read -r file; do
    [ -n "$file" ] || continue
    files+=("$file")
    lint_scanned "${file#"$root"/}"
  done < <(lint_module_files "$root" "$module")
done

if [ ${#files[@]} -gt 0 ]; then
  LINT_LIST="$list" lint_cpp_awk rule-rng.awk "$root" "${files[@]}"
  while IFS=$'\t' read -r file n inc; do
    [ -n "$file" ] || continue
    if [ "$inc" = "<random>" ] || [ "$inc" = "random" ]; then
      lint_finding "${file#"$root"/}" "$n" rng "includes <random> (C++ standard RNG; ADR-0006)"
    fi
  done < <(lint_includes "${files[@]}")
fi
exit 0
