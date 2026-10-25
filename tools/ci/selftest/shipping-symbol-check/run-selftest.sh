#!/usr/bin/env bash
# Selftest for tools/ci/shipping-symbol-check.sh (ADR-0008 G2, story tdc-006).
# Builds small synthetic "packages" in a temp directory at run time (no committed binaries,
# ADR-0009) and checks the script's exit code for a clean pass, each seeded violation, a blind
# positive control and missing input.
#
# Usage: run-selftest.sh      Exit 0 when every case gives its expected exit code, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
check="$here/../../shipping-symbol-check.sh"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# A sentinel with the scanned prefix and a selftest-only suffix, and the harness module name.
sentinel="STEALTH_TEST_SEAM_SENTINEL_SELFTEST"
harness="StealthTestHarness"

utf16le() { local s="$1" i; for ((i = 0; i < ${#s}; i++)); do printf '%s\0' "${s:i:1}"; done; }

# make_pkg <name> <exe-content-spec...>: specs are ascii:<text>, utf16:<text>, file:<filename>
make_pkg() {
  local name="$1"; shift
  local dir="$work/$name/Binaries/Win64" spec
  mkdir -p "$dir"
  printf 'MZ synthetic binary for the selftest\n' > "$dir/StealthGame.exe"
  for spec in "$@"; do
    case "$spec" in
      ascii:*) printf 'pad%spad' "${spec#ascii:}" >> "$dir/StealthGame.exe" ;;
      utf16:*) { printf 'pad'; utf16le "${spec#utf16:}"; printf 'pad'; } >> "$dir/StealthGame.exe" ;;
      file:*)  printf 'MZ stand-in module\n' > "$dir/${spec#file:}" ;;
    esac
  done
}

make_pkg ship-clean
make_pkg ship-ascii "ascii:$sentinel"
make_pkg ship-utf16 "utf16:$sentinel"
make_pkg ship-harness-file "file:UnrealGame-$harness.dll"
make_pkg ship-harness-symbol "utf16:$harness"
make_pkg dev-ok "utf16:$sentinel" "ascii:$sentinel" "utf16:$harness"
make_pkg dev-module-file "ascii:$sentinel" "file:UnrealGame-$harness.dll"
make_pkg dev-nothing
make_pkg dev-harness-no-sentinel "file:UnrealGame-$harness.dll"
mkdir -p "$work/empty"

# case-name | shipping dir | development dir | expected exit (0 PASS, 1 FAIL, 2 NOT ASSESSED)
cases="
clean-pass|ship-clean|dev-ok|0
clean-pass-harness-file|ship-clean|dev-module-file|0
seeded-shipping-sentinel-ascii|ship-ascii|dev-ok|1
seeded-shipping-sentinel-utf16le|ship-utf16|dev-ok|1
seeded-shipping-harness-file|ship-harness-file|dev-ok|1
seeded-shipping-harness-symbol|ship-harness-symbol|dev-ok|1
control-finds-nothing|ship-clean|dev-nothing|1
control-sentinel-missing|ship-clean|dev-harness-no-sentinel|1
missing-shipping-dir|does-not-exist|dev-ok|2
empty-development-dir|ship-clean|empty|2
"

bad=0; total=0
while IFS='|' read -r name ship dev expected; do
  [ -z "$name" ] && continue
  total=$((total + 1))
  out="$(bash "$check" "$work/$ship" "$work/$dev" 2>&1)"; got=$?
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
