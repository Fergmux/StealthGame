#!/usr/bin/env bash
# Selftest for forbidden-rng.sh (story st-012; ADR-0006 Guideline 1; ADR-0008 G2/G6). The gate is watched failing
# against the planted violations (every forbidden API and #include <random> reported as file:line rng) before the
# real tree's pass is trusted; clean controls (names in comments/strings, longer identifiers, the prototype module
# outside scope) and an allowlisted file give zero findings.
#
# Usage: run-forbidden-rng-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/forbidden-rng.sh")
fx="$here/forbidden-rng/fixtures"
. "$here/lib.sh"

v="Source/StealthGameplay/Private/StealthRngLeaks.cpp"
gate_case rng-seeded "$fx/violation" 1 12 \
  "$v:2 rng includes <random>" \
  "$v:3 rng uses FMath::Rand " \
  "$v:4 rng uses FMath::FRand " \
  "$v:5 rng uses FMath::RandRange " \
  "$v:6 rng uses FMath::RandInit " \
  "$v:6 rng uses FMath::SRandInit " \
  "$v:7 rng uses FRandomStream" \
  "$v:8 rng uses std::rand " \
  "$v:9 rng uses std::mt19937 " \
  "$v:10 rng uses std::uniform_int_distribution " \
  "$v:11 rng uses FMath::VRand " \
  "Source/StealthFoundation/Private/StealthRngLeaks2.h:3 rng uses FMath::RandBool "
gate_case rng-clean "$fx/clean" 0 0 "GATE: PASS (1 file(s) scanned"
gate_case rng-allowlist "$fx/allowlisted" 1 1 "Source/StealthGameplay/Private/StealthNotListed.cpp:2 rng uses FMath::FRand "
gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
