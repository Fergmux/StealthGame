#!/usr/bin/env bash
# Selftest for include-lint-ij.sh (story tdc-015; ADR-0008 G2/G6; ADR-0007 sections 3 and 6, rules i-j).
# Each seeded macro line must be reported as file:line with its rule letter; clean controls
# ("Binding" inside a longer identifier, a forbidden name outside any macro, strings and comments)
# and the real tree must give zero findings. The real tree has no CityGenerator/ folder yet, so
# the (j) proof is the seeded fixture, not the real-tree pass.
#
# Usage: run-include-ij-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/include-lint-ij.sh")
fx="$here/include-ij/fixtures"
. "$here/lib.sh"

i="Source/StealthGameplay/Private/StealthLogLeaks.cpp"
# (i) every listed macro, a prefix entry (FEventLogEntry, FRoundEventLogTotals), the whole-token
# entry Binding, an argument spanning lines (reported at the token's line) and a nested call.
gate_case i-seeded "$fx/i-violation" 1 10 \
  "$i:4 i UE_LOG argument names forbidden identifier FEventLogEntry" \
  "$i:5 i UE_LOGF argument names forbidden identifier DealDigest" \
  "$i:6 i ensureMsgf argument names forbidden identifier ServerSubSeed" \
  "$i:7 i checkf argument names forbidden identifier FSecretStream" \
  "$i:8 i ClientMessage argument names forbidden identifier MailboxOrder" \
  "$i:9 i TRACE_CPUPROFILER_EVENT_SCOPE_STR argument names forbidden identifier DealDigest" \
  "$i:10 i CSV_SCOPED_TIMING_STAT argument names forbidden identifier FRoundEventLogTotals" \
  "$i:11 i SCOPE_CYCLE_COUNTER argument names forbidden identifier Binding" \
  "$i:13 i UE_LOGF argument names forbidden identifier Binding" \
  "$i:14 i UE_LOG argument names forbidden identifier FEventLogEntry"
gate_case i-clean "$fx/i-clean" 0 0 "GATE: PASS (1 file(s) scanned"

j="Source/StealthGameplay/Private/Features/CityGenerator/StealthCityGenTick.cpp"
gate_case j-seeded "$fx/j-violation" 1 2 "$j:2 j " "$j:3 j "
gate_case j-clean "$fx/j-clean" 0 0 "GATE: PASS (2 file(s) scanned"

gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
