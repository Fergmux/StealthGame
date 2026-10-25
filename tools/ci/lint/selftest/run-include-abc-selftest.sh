#!/usr/bin/env bash
# Selftest for include-lint-abc.sh (story tdc-012; ADR-0008 G2/G6; ADR-0007 section 3 rules a-c).
# Each seeded fixture must be reported as file:line with its rule letter; each clean control and
# the real tree must give zero findings; the exempt list may only shrink (G3).
#
# Usage: run-include-abc-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
rule="$lint/include-lint-abc.sh"
fx="$here/include-abc/fixtures"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

bad=0; total=0
report() { # name status detail
  total=$((total + 1))
  if [ "$2" -eq 0 ]; then echo "SELFTEST $1 ok"; else echo "SELFTEST $1 FAILED: $3"; bad=$((bad + 1)); fi
}
# case name, root, expected gate exit, expected finding count, then required observation prefixes
gate_case() {
  local name="$1" root="$2" want_rc="$3" want_n="$4"; shift 4
  local out rc n missing=""
  out="$(bash "$lint/gate.sh" --root "$root" "$rule" 2>&1)"; rc=$?
  n="$(printf '%s\n' "$out" | grep -c '^[^ ]*:[0-9][0-9]* [^ ]* ')"
  for req in "$@"; do
    printf '%s\n' "$out" | grep -Fq -- "$req" || missing="$missing [$req]"
  done
  if [ "$rc" -eq "$want_rc" ] && [ "$n" -eq "$want_n" ] && [ -z "$missing" ]; then
    report "$name" 0 ""
  else
    report "$name" 1 "exit $rc (want $want_rc), findings $n (want $want_n), missing:$missing"
  fi
  printf '%s\n' "$out" | sed 's/^/    /'
}

gate_case a-seeded "$fx/a-violation" 1 2 \
  "Source/StealthGameplay/Public/StealthLooseHeader.h:1 a " \
  "Source/StealthGameplay/Public/Misc/StealthOtherFolder.h:1 a "
gate_case a-clean "$fx/a-clean" 0 0 "GATE: PASS (4 file(s) scanned"
gate_case b-seeded "$fx/b-violation" 1 2 \
  "Source/StealthFoundation/Public/Server/StealthGuardNotFirst.h:3 b " \
  "Source/StealthFoundation/Public/Server/StealthNoGuard.h:1 b "
gate_case b-clean "$fx/b-clean" 0 0 "GATE: PASS (3 file(s) scanned"
gate_case c-seeded "$fx/c-violation" 1 1 \
  "Source/StealthGameplay/Public/View/StealthLeakyView.h:4 c "
gate_case c-clean "$fx/c-clean" 0 0 "GATE: PASS (1 file(s) scanned"
gate_case prototype-exempt-and-unlisted "$fx/prototype" 1 1 \
  "Source/StealthGame/Public/StealthUnlistedNew.h:1 a "
gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

# G3 shrink-only, in a throwaway git repository.
repo="$tmp/repo"
mkdir -p "$repo/tools/ci/lint" "$repo/Source/StealthGame/Public"
printf '#pragma once\n' > "$repo/Source/StealthGame/Public/StealthOne.h"
printf '#pragma once\n' > "$repo/Source/StealthGame/Public/StealthTwo.h"
printf 'Source/StealthGame/Public/StealthOne.h\nSource/StealthGame/Public/StealthTwo.h\n' > "$repo/tools/ci/lint/prototype-exempt.txt"
git -C "$repo" init -q && git -C "$repo" -c core.autocrlf=false add -A \
  && git -C "$repo" -c user.email=selftest@example.invalid -c user.name=selftest commit -q -m base
gate_case shrink-only-unchanged "$repo" 0 0 "GATE: PASS"
printf 'Source/StealthGame/Public/StealthOne.h\n' > "$repo/tools/ci/lint/prototype-exempt.txt"
gate_case shrink-only-removal-allowed "$repo" 1 1 "Source/StealthGame/Public/StealthTwo.h:1 a "
printf 'Source/StealthGame/Public/StealthOne.h\nSource/StealthGame/Public/StealthTwo.h\nSource/StealthGame/Public/StealthThree.h\n' > "$repo/tools/ci/lint/prototype-exempt.txt"
gate_case shrink-only-growth-reported "$repo" 1 1 "tools/ci/lint/prototype-exempt.txt:3 shrink-only "
rm -rf "$repo/.git"
printf 'Source/StealthGame/Public/StealthOne.h\nSource/StealthGame/Public/StealthTwo.h\n' > "$repo/tools/ci/lint/prototype-exempt.txt"
(cd "$tmp" && GIT_CEILING_DIRECTORIES="$tmp" bash "$lint/gate.sh" --root "$repo" "$rule" > "$tmp/nogit.out" 2>&1); rc=$?
grep -Fq "tools/ci/lint/prototype-exempt.txt:0 shrink-only " "$tmp/nogit.out"
[ $? -eq 0 ] && [ "$rc" -eq 1 ]; report shrink-only-unverifiable-base-reported $? "exit $rc: $(tr '\n' '|' < "$tmp/nogit.out")"

if [ "$bad" -eq 0 ]; then
  echo "SELFTEST RESULT: PASS ($total cases)"
  exit 0
fi
echo "SELFTEST RESULT: FAIL ($bad of $total cases)"
exit 1
