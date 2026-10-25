#!/usr/bin/env bash
# Shipping-symbol check (ADR-0008 sections 4 and 7, story tdc-006). Nightly, on packaged builds.
#
# Usage: shipping-symbol-check.sh <packaged-shipping-dir> <packaged-development-dir>
#
# Emits one observation per build and check, then decides in a single gate step:
#   Shipping:    must contain NO StealthTestHarness module (a file named *StealthTestHarness*, or
#                the module name inside a binary) and NO test-seam sentinel (ASCII or UTF-16LE).
#   Development: the positive control; must contain the harness (file or name in a binary) AND the
#                sentinel in at least one encoding, else the scan is proven blind.
# Exit: 0 PASS | 1 FAIL | 2 NOT ASSESSED (a build directory is missing or holds no files).
#
# Only binaries are scanned (*.exe, *.dll, *.so, *.dylib): the sentinel is a string literal in
# compiled code. A monolithic package has no separate harness binary, so the module name inside a
# binary (registered by IMPLEMENT_MODULE) also counts; record the observed binary name pattern in
# story tdc-006 when the first real packages are scanned.
set -u

SENTINEL_PREFIX="STEALTH_TEST_SEAM_SENTINEL_"
HARNESS="StealthTestHarness"

shipping_dir="${1:-}"
development_dir="${2:-}"
observations=""

emit() {
  echo "OBSERVATION $*"
  observations+="$*"$'\n'
}

# Occurrences of a fixed string in a file, as raw (ASCII/UTF-8) bytes.
count_ascii() {
  LC_ALL=C grep -a -o -F -- "$1" "$2" 2>/dev/null | wc -l | tr -d ' '
}

# Occurrences encoded as UTF-16LE: deleting NUL bytes turns UTF-16LE ASCII-range text into plain
# bytes, so the extra matches after stripping are the UTF-16LE ones.
count_utf16le() {
  local raw stripped
  raw=$(count_ascii "$1" "$2")
  stripped=$(tr -d '\000' < "$2" | LC_ALL=C grep -a -o -F -- "$1" 2>/dev/null | wc -l | tr -d ' ')
  echo $((stripped - raw))
}

yesno() { if [ "$1" -gt 0 ]; then echo yes; else echo no; fi; }

scan_build() {
  local label="$1" dir="$2"
  if [ -z "$dir" ] || [ ! -d "$dir" ] || [ -z "$(find "$dir" -type f -print -quit 2>/dev/null)" ]; then
    emit "build=$label status=missing dir=${dir:-<none>}"
    return
  fi
  local harness_file=0 harness_symbol=0 sentinel_ascii=0 sentinel_utf16=0 f
  harness_file=$(find "$dir" -type f -iname "*${HARNESS}*" | wc -l | tr -d ' ')
  while IFS= read -r -d '' f; do
    harness_symbol=$((harness_symbol + $(count_ascii "$HARNESS" "$f") + $(count_utf16le "$HARNESS" "$f")))
    sentinel_ascii=$((sentinel_ascii + $(count_ascii "$SENTINEL_PREFIX" "$f")))
    sentinel_utf16=$((sentinel_utf16 + $(count_utf16le "$SENTINEL_PREFIX" "$f")))
  done < <(find "$dir" -type f \( -iname '*.exe' -o -iname '*.dll' -o -iname '*.so' -o -iname '*.dylib' \) -print0)
  emit "build=$label status=scanned"
  emit "build=$label check=harness-file found=$(yesno "$harness_file")"
  emit "build=$label check=harness-symbol found=$(yesno "$harness_symbol")"
  emit "build=$label check=sentinel-ascii found=$(yesno "$sentinel_ascii")"
  emit "build=$label check=sentinel-utf16le found=$(yesno "$sentinel_utf16")"
}

scan_build shipping "$shipping_dir"
scan_build development "$development_dir"

# ---- gate: the only place a verdict is decided ----
found() { grep -q "^build=$1 check=$2 found=yes$" <<< "$observations"; }

if grep -q "status=missing" <<< "$observations"; then
  echo "GATE shipping-symbol-check: NOT ASSESSED - a build directory is missing or empty"
  exit 2
fi
fail=0
for check in harness-file harness-symbol sentinel-ascii sentinel-utf16le; do
  if found shipping "$check"; then
    echo "GATE shipping: FAIL - $check found in the Shipping build"
    fail=1
  fi
done
if ! { found development harness-file || found development harness-symbol; }; then
  echo "GATE development control: FAIL - no StealthTestHarness module found (the scan is blind or the harness is missing)"
  fail=1
fi
if ! { found development sentinel-ascii || found development sentinel-utf16le; }; then
  echo "GATE development control: FAIL - no test-seam sentinel found (sentinel optimised out or the scan is blind)"
  fail=1
fi
if [ "$fail" -ne 0 ]; then
  echo "GATE shipping-symbol-check: FAIL"
  exit 1
fi
echo "GATE shipping-symbol-check: PASS"
exit 0
