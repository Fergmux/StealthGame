#!/usr/bin/env bash
# Seeded controls for the include lint rules (a)-(j) (story tdc-016; ADR-0008 section 3 "a gate you
# have not watched fail is not a gate"; ADR-0007 Validation 4 and 7).
#
# tools/ci/lint/fixtures/seeded/ is a miniature repository holding EXACTLY ONE seeded violation per
# rule (the seeds are listed in its seeds.txt) and nothing else that violates any rule. This
# selftest runs the four rule scripts (abc, def, gh, ij) over it and over the real tree:
#   - exactly ten findings, one per rule id a..j (each as file:line);
#   - the gate exits 1 (FAIL) on the fixture and 0 (PASS) on the real tree;
#   - for each rule: delete that rule's seed in a temporary copy and exactly that rule's finding
#     disappears while the other nine remain (the control is detected, not vacuous);
#   - the fixture is outside the real-tree scan scope (no rule scans a path under tools/).
# The seeded UE_LOGF with an Event Log argument (rule i) is ADR-0007 Validation 7.
#
# Usage: seeded-controls.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/include-lint-abc.sh" "$lint/include-lint-def.sh" "$lint/include-lint-gh.sh" "$lint/include-lint-ij.sh")
seeded="$lint/fixtures/seeded"
real="$lint/../../.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"

letters=(a b c d e f g h i j)

# Runs the four rule scripts over <root> IN PARALLEL (process start-up dominates on Windows), then
# decides with gate.sh --obs on the combined observations. A rule script that exits nonzero adds a
# malformed line, so the gate reports NOT ASSESSED exactly as it does when it runs the scripts itself.
# Sets OBS_OUT (the gate output) and OBS_RC (its exit code); the raw observations stay in $tmp/raw.obs.
obs_gate() {
  local root="$1" i=0 r
  rm -f "$tmp"/rule.* "$tmp/raw.obs"
  for r in "${RULES[@]}"; do
    ( bash "$r" "$root" > "$tmp/rule.$i" 2>/dev/null || echo "RULE-SCRIPT-FAILED $r" >> "$tmp/rule.$i" ) &
    i=$((i + 1))
  done
  wait
  cat "$tmp"/rule.* > "$tmp/raw.obs"
  OBS_OUT="$(bash "$lint/gate.sh" --obs "$tmp/raw.obs" 2>&1)"; OBS_RC=$?
}

# Rule ids present in a gate output, one letter per finding line, sorted.
rule_ids() { printf '%s\n' "$1" | grep '^[^ ]*:[0-9][0-9]* [^ ]* ' | awk '{print $2}' | LC_ALL=C sort | tr '\n' ' '; }

# 1. The seeded tree: ten findings, one per letter, gate FAIL.
obs_gate "$seeded"; out="$OBS_OUT"; rc=$OBS_RC
ids="$(rule_ids "$out")"
[ "$ids" = "a b c d e f g h i j " ]
report seeded-one-finding-per-rule $? "rule ids were: $ids"
[ "$rc" -eq 1 ] && printf '%s\n' "$out" | grep -Fq "GATE: FAIL (10 finding(s)"
report seeded-gate-fails $? "exit $rc"
printf '%s\n' "$out" | sed 's/^/    /'
# Each seed is reported as file:line at the file named in seeds.txt.
while read -r letter path; do
  case "$letter" in ''|'#'*) continue ;; esac
  printf '%s\n' "$out" | grep -q "^$path:[0-9][0-9]* $letter "
  report "seed-$letter-reported-at-its-file" $? "no '$letter' finding for $path"
done < "$seeded/seeds.txt"

# 2. The real tree: the gate passes (and the seeded tree is not part of its scope).
obs_gate "$real"
[ "$OBS_RC" -eq 0 ] && printf '%s\n' "$OBS_OUT" | grep -Fq "GATE: PASS"
report real-tree-passes $? "exit $OBS_RC: $(printf '%s' "$OBS_OUT" | tr '\n' '|')"
printf '%s\n' "$OBS_OUT" | sed 's/^/    /'
! grep '^SCANNED ' "$tmp/raw.obs" | grep -q 'tools/ci/lint/fixtures'
report fixture-excluded-from-real-tree-scan $? "a rule scanned a path under tools/ci/lint/fixtures"

# 3. Remove each seed in a temporary copy: exactly that rule's finding disappears.
while read -r letter path; do
  case "$letter" in ''|'#'*) continue ;; esac
  copy="$tmp/without-$letter"
  mkdir -p "$copy"
  cp -R "$seeded/." "$copy/"
  rm -f "$copy/$path"
  obs_gate "$copy"
  ids="$(rule_ids "$OBS_OUT")"
  want=""
  for l in "${letters[@]}"; do [ "$l" != "$letter" ] && want="$want$l "; done
  [ "$ids" = "$want" ]
  report "seed-$letter-removal-drops-only-$letter" $? "rule ids were: $ids (want: $want)"
done < "$seeded/seeds.txt"

selftest_result
