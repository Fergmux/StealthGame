#!/usr/bin/env bash
# Selftest for include-lint-def.sh (story tdc-013; ADR-0008 G2/G6; ADR-0007 section 3 rules d-f).
# Each seeded fixture must be reported as file:line with its rule letter; each clean control and the
# real tree must give zero findings.
#
# Usage: run-include-def-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/include-lint-def.sh")
fx="$here/include-def/fixtures"
. "$here/lib.sh"

# (d) Core/ including Features/: by include text and by resolution to a Features header.
gate_case d-seeded "$fx/d-violation" 1 2 \
  "Source/StealthGameplay/Core/StealthCoreBad.cpp:3 d " \
  "Source/StealthGameplay/Private/Core/StealthResolvedBad.cpp:1 d "
# Clean: Features including Core (direction only), commented-out includes, a "Features" substring, Public/Server/Core.
gate_case d-clean "$fx/d-clean" 0 0 "GATE: PASS (2 file(s) scanned"

# (e) Foundation including Gameplay, Presentation and the prototype module; module-named include path.
gate_case e-seeded "$fx/e-violation" 1 4 \
  "Source/StealthFoundation/Private/StealthUpward.cpp:1 e " \
  "Source/StealthFoundation/Private/StealthUpward.cpp:2 e " \
  "Source/StealthFoundation/Private/StealthUpward.cpp:3 e " \
  "Source/StealthFoundation/Private/StealthUpward.cpp:4 e "
gate_case e-clean "$fx/e-clean" 0 0 "GATE: PASS (3 file(s) scanned"

# (f) one name in two modules (also differing only by case): one finding each; the same name twice
# inside ONE module is not reported (decision recorded in include-lint-def.sh).
gate_case f-seeded "$fx/f-violation" 1 2 \
  "Source/StealthPresentation/Private/StealthDupName.h:1 f " \
  "Source/StealthPresentation/Private/stealthcasedup.h:1 f "
gate_case f-clean "$fx/f-clean" 0 0 "GATE: PASS (2 file(s) scanned"
# The prototype exempt list lets a listed prototype header share a name with a production header;
# the same pair without the list entry is reported.
gate_case f-prototype-exempt-allowed "$fx/f-exempt" 0 0 "GATE: PASS (2 file(s) scanned"
gate_case f-prototype-unlisted-reported "$fx/f-exempt-not-listed" 1 1 \
  "Source/StealthGameplay/Public/View/StealthMovementComponent.h:1 f "

# The real tree: zero findings (and scannable files, so this is a pass and not NOT ASSESSED).
gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
