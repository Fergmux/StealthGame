#!/usr/bin/env bash
# Selftest for gate-probes.sh (ADR-0008 G2, story tdc-005). Feeds canned observation files for
# every outcome to the gate and checks its exit code, so no UBT run is needed per commit.
# Proves a clean pass AND that each seeded violation is detected.
#
# Usage: run-selftest.sh      Exit 0 when every case gives its expected exit code, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
gate="$here/../gate-probes.sh"
fx="$here/fixtures"

# case-name | fixture | expected gate exit (0 PASS, 1 FAIL, 2 NOT ASSESSED)
cases="
pass|pass.obs|0
probe-compiled|probe-compiled.obs|1
wrong-error-text|wrong-error.obs|1
control-failed|control-failed.obs|1
control-missing|control-missing.obs|1
probe-not-compiled|probe-not-compiled.obs|1
control-not-compiled|control-not-compiled.obs|1
mixed-pass-and-fail|mixed.obs|1
no-probes|no-probes.obs|2
empty-file|empty.obs|2
missing-file|does-not-exist.obs|2
baseline-build-failed|baseline-failed.obs|2
"

bad=0; total=0
while IFS='|' read -r name fixture expected; do
  [ -z "$name" ] && continue
  total=$((total + 1))
  out="$(bash "$gate" "$fx/$fixture" 2>&1)"; got=$?
  if [ "$got" -eq "$expected" ]; then
    echo "SELFTEST $name expected=$expected got=$got ok"
  else
    echo "SELFTEST $name expected=$expected got=$got MISMATCH"
    echo "$out" | sed 's/^/    /'
    bad=$((bad + 1))
  fi
done <<EOF
$cases
EOF

if [ "$bad" -eq 0 ]; then
  echo "SELFTEST RESULT: PASS ($total cases)"
  exit 0
fi
echo "SELFTEST RESULT: FAIL ($bad of $total cases)"
exit 1
