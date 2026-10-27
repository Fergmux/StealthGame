#!/usr/bin/env bash
# Selftest for crowd-lint.sh (stories cnb-005, cnb-013; ADR-0016 section 2, ADR-0001; ADR-0008 G2/G6). The gate is
# watched failing against the planted violations (every forbidden pawn API, the Attack call, the Velocity write, a
# SetControlRotation outside the controller, and the Roster, Objectives and perception includes) before the real
# tree's pass is trusted; the clean control (names in comments and strings, OnPossess, VelocityCmPerSec) is silent.
#
# Usage: run-crowd-lint-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/crowd-lint.sh")
fx="$here/crowd/fixtures"
. "$here/lib.sh"

v="Source/StealthGameplay/Features/Crowd/Private/CrowdLeaks.cpp"
gate_case crowd-seeded "$fx/violation" 1 14 \
  "$v:1 crowd-include includes the Roster header" \
  "$v:2 crowd-include includes the engine perception header" \
  "$v:3 crowd-api uses MoveToLocation" \
  "$v:4 crowd-api uses AddMovementInput" \
  "$v:4 crowd-api uses SetActorRotation" \
  "$v:5 crowd-api writes a movement Velocity" \
  "$v:5 crowd-api uses LaunchCharacter" \
  "$v:6 crowd-api calls Attack" \
  "$v:7 crowd-api uses SetFocus" \
  "$v:7 crowd-api uses Possess" \
  "$v:8 crowd-api calls SetControlRotation outside" \
  "$v:9 crowd-include includes the Objectives header" \
  "$v:10 crowd-identity names GetTeam" \
  "$v:10 crowd-identity names FSiteAssignment"
gate_case crowd-clean "$fx/clean" 0 0 "GATE: PASS (1 file(s) scanned"
gate_case crowd-transitive "$fx/transitive" 1 1   "Source/StealthGameplay/Features/Crowd/Private/CrowdTransitive.cpp:1 crowd-include transitively includes Source/StealthFoundation/Public/Server/Roster/StealthRosterStore.h"
u="Source/StealthGameplay/Features/Crowd/Private/CrowdDraws.cpp"
gate_case crowd-ui-seeded "$fx/ui" 1 4 \
  "$u:1 crowd-ui includes the drawing/UI header" \
  "$u:2 crowd-ui includes the drawing/UI header" \
  "$u:3 crowd-ui draws DrawDebugSphere" \
  "$u:4 crowd-ui draws AddOnScreenDebugMessage"
gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
