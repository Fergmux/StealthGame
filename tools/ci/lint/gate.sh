#!/usr/bin/env bash
# T0 lint gate (ADR-0008 sections 1 and 8, story tdc-011): the ONLY place a lint verdict is made.
#
# Usage:
#   gate.sh [--root <dir>] <rule-script> [<rule-script>...]
#       Runs each rule script as `bash <rule-script> <root>` (root defaults to the repo root) and
#       decides on the combined observations.
#   gate.sh --obs <file>
#       Decides on a recorded observation file (selftests, or a rule run elsewhere).
#
# Observations follow the contract in tools/ci/lint/emit.sh: "<path>:<line> <rule> <message>"
# findings and "SCANNED <path>" lines.
#
# Exit codes and the last line printed:
#   0  GATE: PASS          files were scanned and there are no findings
#   1  GATE: FAIL          one or more findings (each finding line is printed above)
#   2  GATE: NOT ASSESSED  no file was scanned, a rule script failed to run, or a line broke the
#                          contract: an empty or broken scope is never a pass
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root=""
obs=""
scripts=()
while [ $# -gt 0 ]; do
  case "$1" in
    --root) root="${2:-}"; shift 2 ;;
    --obs) obs="${2:-}"; shift 2 ;;
    *) scripts+=("$1"); shift ;;
  esac
done

problems=()
collected="$(mktemp)"
trap 'rm -f "$collected"' EXIT

if [ -n "$obs" ]; then
  if [ -f "$obs" ]; then
    cat "$obs" > "$collected"
  else
    problems+=("observation file not found: $obs")
  fi
elif [ ${#scripts[@]} -eq 0 ]; then
  problems+=("no rule script given")
else
  [ -z "$root" ] && root="$(cd "$here/../../.." && pwd)"
  for script in "${scripts[@]}"; do
    out="$(bash "$script" "$root")"; rc=$?
    printf '%s\n' "$out" >> "$collected"
    if [ "$rc" -ne 0 ]; then
      problems+=("rule script $script exited $rc")
    fi
  done
fi

findings=0
malformed=0
declare -A scanned_files=()
while IFS= read -r line || [ -n "$line" ]; do
  line="${line%$'\r'}"
  [ -z "$line" ] && continue
  if [[ "$line" =~ ^SCANNED\ ([^ ]+)$ ]]; then
    scanned_files["${BASH_REMATCH[1]}"]=1
  elif [[ "$line" =~ ^[^\ ]+:[0-9]+\ [^\ ]+\ .+$ ]]; then
    findings=$((findings + 1))
    printf '%s\n' "$line"
  else
    malformed=$((malformed + 1))
    problems+=("line breaks the observation contract: $line")
  fi
done < "$collected"
scanned=${#scanned_files[@]}

if [ "$findings" -gt 0 ]; then
  for p in "${problems[@]+"${problems[@]}"}"; do echo "note: $p"; done
  echo "GATE: FAIL ($findings finding(s); $scanned file(s) scanned)"
  exit 1
fi
if [ ${#problems[@]} -gt 0 ] || [ "$scanned" -eq 0 ]; then
  for p in "${problems[@]+"${problems[@]}"}"; do echo "note: $p"; done
  echo "GATE: NOT ASSESSED ($scanned file(s) scanned; an empty or broken lint scope is not a pass)"
  exit 2
fi
echo "GATE: PASS ($scanned file(s) scanned, 0 findings)"
exit 0
