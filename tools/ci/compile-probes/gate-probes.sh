#!/usr/bin/env bash
# Compile-probe gate (ADR-0008 sections 7-8, story tdc-005). Decides pass/fail from the
# observation file written by run-probes.ps1; the runner itself decides nothing.
#
# Usage: gate-probes.sh <observations-file>
# Exit:  0 PASS          every probe failed with its expected text AND its positive control compiled
#        1 FAIL          a probe compiled (guard not detected), failed with a different message,
#                        or was not compiled at all; or its positive control did not compile, was
#                        not compiled, or is missing
#        2 NOT ASSESSED  file missing or empty, no probe observations, or the baseline build failed
set -u

obs="${1:-}"
if [ -z "$obs" ] || [ ! -s "$obs" ]; then
  echo "GATE compile-probes: NOT ASSESSED - observation file missing or empty (${obs:-<none>})"
  exit 2
fi

awk '
  { sub(/\r$/, "") }
  /^OBSERVATION baseline-build=failed/ { baseline_failed = 1 }
  /^OBSERVATION probe=/ {
    name = ""; role = ""; outcome = ""
    for (i = 2; i <= NF; i++) {
      split($i, kv, "=")
      if (kv[1] == "probe") name = kv[2]
      else if (kv[1] == "role") role = kv[2]
      else if (kv[1] == "outcome") outcome = kv[2]
    }
    if (!(name in seen)) { seen[name] = 1; order[++n] = name }
    if (role == "probe") probe[name] = outcome
    else if (role == "control") control[name] = outcome
  }
  END {
    if (baseline_failed) { print "GATE compile-probes: NOT ASSESSED - baseline build of the disposable copy failed"; exit 2 }
    if (n == 0) { print "GATE compile-probes: NOT ASSESSED - no probe observations (no probe files found)"; exit 2 }
    fail = 0
    for (i = 1; i <= n; i++) {
      p = order[i]
      po = (p in probe) ? probe[p] : "missing"
      co = (p in control) ? control[p] : "missing"
      if (po != "failed-expected") {
        reason = (po == "compiled") ? "probe compiled: the guard did not fire" : (po == "failed-other") ? "probe failed with a different message" : (po == "not-compiled") ? "UBT ran no compile action for the probe" : "no probe observation"
        print "GATE probe " p ": FAIL - " reason; fail = 1
      } else if (co != "compiled") {
        print "GATE probe " p ": FAIL - positive control " ((co == "missing") ? "not observed" : (co == "not-compiled") ? "was not compiled by UBT" : "did not compile"); fail = 1
      } else {
        print "GATE probe " p ": PASS - failed with the expected text; control compiled"
      }
    }
    if (fail) { print "GATE compile-probes: FAIL"; exit 1 }
    print "GATE compile-probes: PASS (" n " probe(s))"
    exit 0
  }
' "$obs"
