#!/usr/bin/env bash
# Shared helpers for the include-lint selftests (stories tdc-013 to tdc-016; ADR-0008 G2/G6).
# A selftest sources this file after setting: lint (the tools/ci/lint folder) and, for gate_case,
# RULES (an array of rule scripts the gate runs together). Then it calls gate_case per case and
# ends with selftest_result.
#
#   gate_case <name> <root> <want-exit> <want-findings> [<required output text>...]
#       runs gate.sh over RULES against <root>; passes when the gate exit code, the number of
#       finding lines and every required text (a "file:line rule " prefix or a GATE line) hold
#   report <name> <0|1> <detail>
#   selftest_result      prints "SELFTEST RESULT: PASS|FAIL (...)" and exits 0|1

bad=0
total=0

report() { # name status detail
  total=$((total + 1))
  if [ "$2" -eq 0 ]; then echo "SELFTEST $1 ok"; else echo "SELFTEST $1 FAILED: $3"; bad=$((bad + 1)); fi
}

gate_case() {
  local name="$1" root="$2" want_rc="$3" want_n="$4"; shift 4
  local out rc n missing="" req
  out="$(bash "$lint/gate.sh" --root "$root" "${RULES[@]}" 2>&1)"; rc=$?
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

selftest_result() {
  if [ "$bad" -eq 0 ]; then
    echo "SELFTEST RESULT: PASS ($total cases)"
    exit 0
  fi
  echo "SELFTEST RESULT: FAIL ($bad of $total cases)"
  exit 1
}
