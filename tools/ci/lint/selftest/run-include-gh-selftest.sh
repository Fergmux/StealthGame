#!/usr/bin/env bash
# Selftest for include-lint-gh.sh (story tdc-014; ADR-0008 G2/G6; ADR-0007 section 3 rules g-h).
# Each seeded fixture line must be reported as file:line with its rule letter; clean controls (a
# correct UCLASS/UINTERFACE, annotated lines, API names in comments and strings) and the real tree
# must give zero findings.
#
# Usage: run-include-gh-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/include-lint-gh.sh")
fx="$here/include-gh/fixtures"
. "$here/lib.sh"

g="Source/StealthFoundation/Public/Server/StealthBadServerTypes.h"
# (g) UCLASS missing one or both specifiers, UINTERFACE without the meta, each of the five Blueprint
# specifiers on a member, and a specifier list that spans lines.
gate_case g-seeded "$fx/g-violation" 1 11 \
  "$g:3 g server UCLASS" "$g:5 g server UCLASS" "$g:7 g server UCLASS" \
  "$g:9 g server UINTERFACE" "$g:11 g server UINTERFACE" \
  "$g:17 g member of a server type carries BlueprintReadOnly" \
  "$g:19 g member of a server type carries BlueprintReadWrite" \
  "$g:21 g member of a server type carries BlueprintCallable" \
  "$g:23 g member of a server type carries BlueprintPure" \
  "$g:25 g member of a server type carries BlueprintAssignable" \
  "$g:27 g member of a server type carries BlueprintReadOnly"
# Clean: correct UCLASS (also multi-line), a correct UINTERFACE (alone and sharing a meta list),
# plain members, an Event-Log-like plain C++ type, mentions inside comments and strings.
gate_case g-clean "$fx/g-clean" 0 0 "GATE: PASS (1 file(s) scanned"

h="Source/StealthPresentation/Private/StealthBadHud.cpp"
gate_case h-seeded "$fx/h-violation" 1 12 \
  "$h:4 h presentation code uses TActorIterator" "$h:5 h presentation code uses TActorRange" \
  "$h:6 h presentation code uses GetAllActorsOfClass" "$h:7 h presentation code uses GetAllActorsWithInterface" \
  "$h:8 h presentation code uses GetPlayerControllerIterator" "$h:9 h presentation code uses GetGameMode" \
  "$h:10 h presentation code uses GetAuthGameMode" \
  "$h:11 h GetPlayerState use needs" "$h:12 h GetController use needs" \
  "$h:13 h GetController use has a LOCAL-PAWN-OK annotation with an empty reason" \
  "$h:16 h presentation code uses GetGameMode" "$h:17 h LOCAL-PAWN-OK annotation has an empty reason"
# Lines 14 and 15 of the seeded file carry a real reason and are not reported (n == 12 above).
gate_case h-clean "$fx/h-clean" 0 0 "GATE: PASS (1 file(s) scanned"

gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
