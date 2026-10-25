#!/usr/bin/env bash
# Demonstration rule for the lint framework selftest (story tdc-011). Not a real lint: it reports
# every line containing SEEDED-LINT-VIOLATION in the *.txt files directly under <root>, using the
# shared emitter, so the selftest can prove the emitter/gate contract without any real rule.
#
# Usage: demo-rule.sh <root>
set -u
. "$(cd "$(dirname "$0")/../.." && pwd)/emit.sh"
root="$(lint_root "${1:-}")"
for f in "$root"/*.txt; do
  [ -f "$f" ] || continue
  rel="$(lint_rel "$root" "$f")"
  lint_scanned "$rel"
  n=0
  while IFS= read -r line || [ -n "$line" ]; do
    n=$((n + 1))
    case "$line" in
      *SEEDED-LINT-VIOLATION*) lint_finding "$rel" "$n" demo "seeded violation marker" ;;
    esac
  done < "$f"
done
exit 0
