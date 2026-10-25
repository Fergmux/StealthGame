#!/usr/bin/env bash
# Selftest for the T0 lint framework (story tdc-011, ADR-0008 G2): emit.sh + gate.sh.
# Proves PASS on a clean file, FAIL with the observation line printed, NOT ASSESSED on an empty
# input set (distinct from FAIL by text and exit code), that the emitter alone never gives a
# verdict, and that two findings give two lines.
#
# Usage: run-framework-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
fx="$here/framework/fixtures"
rule="$here/framework/demo-rule.sh"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

bad=0; total=0
report() { # name ok(0/1) detail
  total=$((total + 1))
  if [ "$2" -eq 0 ]; then echo "SELFTEST $1 ok"; else echo "SELFTEST $1 FAILED: $3"; bad=$((bad + 1)); fi
}
gate_case() { # name root expected-exit required-text
  local out rc
  out="$(bash "$lint/gate.sh" --root "$2" "$rule" 2>&1)"; rc=$?
  if [ "$rc" -eq "$3" ] && printf '%s\n' "$out" | grep -Fq -- "$4"; then
    report "$1" 0 ""
  else
    report "$1" 1 "exit $rc (want $3), output: $(printf '%s' "$out" | tr '\n' '|')"
  fi
  printf '%s\n' "$out" | sed 's/^/    /'
}

# (a) clean file only: PASS.
gate_case clean-pass "$fx/clean" 0 "GATE: PASS (1 file(s) scanned, 0 findings)"
# (b) two-file fixture with one finding: FAIL and the observation line printed.
gate_case one-finding-fail "$fx/one" 1 "finding.txt:3 demo seeded violation marker"
gate_case one-finding-fail-text "$fx/one" 1 "GATE: FAIL (1 finding(s); 2 file(s) scanned)"
# (c) zero input files: NOT ASSESSED, distinguishable from FAIL.
mkdir -p "$tmp/empty"
gate_case zero-files-not-assessed "$tmp/empty" 2 "GATE: NOT ASSESSED (0 file(s) scanned"
# Two findings: two observation lines.
out="$(bash "$lint/gate.sh" --root "$fx/two" "$rule" 2>&1)"
lines="$(printf '%s\n' "$out" | grep -c '^two-findings.txt:[0-9]* demo ')"
[ "$lines" -eq 2 ]; report two-findings-two-lines $? "got $lines finding lines"

# The emitter alone never returns a verdict: the rule exits 0 on a finding and prints no GATE line.
out="$(bash "$rule" "$fx/one" 2>&1)"; rc=$?
if [ "$rc" -eq 0 ] && ! printf '%s\n' "$out" | grep -q 'GATE:\|PASS\|FAIL' && printf '%s\n' "$out" | grep -Fxq 'finding.txt:3 demo seeded violation marker'; then
  report emitter-no-verdict 0 ""
else
  report emitter-no-verdict 1 "rule exit $rc, output: $(printf '%s' "$out" | tr '\n' '|')"
fi

# Observation-file mode and contract breaks.
printf 'SCANNED a.h\n' > "$tmp/clean.obs"
bash "$lint/gate.sh" --obs "$tmp/clean.obs" >/dev/null 2>&1; rc=$?
[ "$rc" -eq 0 ]; report obs-clean-pass $? "exit $rc"
printf 'SCANNED a.h\nthis is not an observation\n' > "$tmp/bad.obs"
bash "$lint/gate.sh" --obs "$tmp/bad.obs" >/dev/null 2>&1; rc=$?
[ "$rc" -eq 2 ]; report malformed-line-not-assessed $? "exit $rc"
bash "$lint/gate.sh" --obs "$tmp/missing.obs" >/dev/null 2>&1; rc=$?
[ "$rc" -eq 2 ]; report missing-obs-not-assessed $? "exit $rc"
printf 'SCANNED a.h\nexit 3\n' > "$tmp/crash-rule.sh"
bash "$lint/gate.sh" --root "$fx/clean" "$tmp/crash-rule.sh" >/dev/null 2>&1; rc=$?
[ "$rc" -eq 2 ]; report crashed-rule-not-assessed $? "exit $rc"
# Scanned lines are counted per distinct file.
printf 'SCANNED a.h\nSCANNED a.h\nSCANNED b.h\n' > "$tmp/dup.obs"
out="$(bash "$lint/gate.sh" --obs "$tmp/dup.obs" 2>&1)"
printf '%s\n' "$out" | grep -Fq "GATE: PASS (2 file(s) scanned"; report scanned-counts-distinct-files $? "$out"

if [ "$bad" -eq 0 ]; then
  echo "SELFTEST RESULT: PASS ($total cases)"
  exit 0
fi
echo "SELFTEST RESULT: FAIL ($bad of $total cases)"
exit 1
