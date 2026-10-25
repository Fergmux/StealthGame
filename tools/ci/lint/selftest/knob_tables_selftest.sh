#!/usr/bin/env bash
# Selftest for knob_tables.py, the Tuning Knobs table parser (story tdc-029; ADR-0004 section 2, ADR-0008 G2/G6).
# Fixture GDDs (tools/ci/lint/fixtures/knob/base): a standard table, a paired row, a reordered table with an
# extra column, an owner-level preamble table, a consumed-elsewhere table, a retired row and a reference row.
# Seeded controls: a count mismatch, a duplicate key row and a GDD without the section must each be reported as
# an observation; the real design/gdd/*.md must yield records for every GDD that has the section.
#
# Usage: knob_tables_selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"
. "$here/knob-lib.sh"

# --- fixture tree: records -------------------------------------------------------------------------------
root="$(fx_tree base)"
rec="$(py_tool knob_tables.py --root "$(knob_native "$root")" --records)"
obs="$(py_tool knob_tables.py --root "$(knob_native "$root")")"

# One knob record per key: Alpha 4 (the pair row counts per name), Beta 2, TDC 1; the retired row, the
# reference row and the consumed table give none.
n_alpha="$(count_lines "$rec" '"gdd": "design/gdd/alpha-system.md".*"kind": "knob"|"kind": "knob".*"gdd": "design/gdd/alpha-system.md"')"
n_beta="$(count_lines "$rec" '"gdd": "design/gdd/beta-system.md".*"kind": "knob"|"kind": "knob".*"gdd": "design/gdd/beta-system.md"')"
n_tdc="$(count_lines "$rec" '"gdd": "design/gdd/tuning-data-config.md".*"kind": "knob"|"kind": "knob".*"gdd": "design/gdd/tuning-data-config.md"')"
[ "$n_alpha" -eq 4 ] && [ "$n_beta" -eq 2 ] && [ "$n_tdc" -eq 1 ]
report record-count-per-gdd $? "alpha $n_alpha beta $n_beta tdc $n_tdc (want 4 2 1)"

expect pair-row-split-per-key "$rec" \
  '"default": "4"' '"default": "8"' '"hard_range": "2-10"' '"hard_range": "4-16"' '"safe_range": "3-5"' '"safe_range": "6-10"'
expect column-order-variant-mapped-by-name "$rec" \
  '"key": "AlphaScaleMult"' '"default": "**60.0** (provisional)"' '"hard_range": "30-90"' '"audience": "All (prompts)"' '"latch": "Use"'
expect preamble-style-inherits-owner-audience-latch "$rec" \
  '"inherited": ["owner", "audience", "latch"]' '"owner": "Beta System"' '"style": "preamble"'
expect standard-style-has-nothing-inherited "$rec" '"inherited": []' '"style": "standard"'
# The consumed-elsewhere table of the Alpha GDD lists BetaWindowSec: a reference is not ownership (CR-15.3).
[ "$(count_lines "$rec" '"key": "BetaWindowSec"')" -eq 1 ]
report consumed-table-row-is-not-ownership $? "BetaWindowSec has more than one record"
expect retired-row-is-listed-not-owned "$rec" '"kind": "retired"'
expect_not retired-row-has-no-knob-record "$(printf '%s\n' "$rec" | grep '"kind": "knob"')" '"key": "BetaOldSec"'
expect reference-row-is-listed-not-owned "$rec" '"kind": "ref"'
[ "$(printf '%s\n' "$rec" | grep '"key": "AlphaMaxCount"' | grep -c '"kind": "knob"')" -eq 1 ]
report reference-row-does-not-double-own-AlphaMaxCount $? "AlphaMaxCount is a knob record of two GDDs"
expect record-carries-gdd-and-line "$rec" '"gdd": "design/gdd/alpha-system.md"' '"line": 11'
expect table-record-lists-header-cells "$rec" '"kind": "table"' '"header": ["Key", "Type", "Owner", "Default", "Hard range", "Safe range", "Audience", "Latch", "Unit", "Effect / breaks if too high or low"]'

# --- fixture tree: observations (a clean tree reports nothing) ----------------------------------------------
expect clean-fixture-scanned "$obs" "SCANNED design/gdd/alpha-system.md" "SCANNED design/gdd/beta-system.md" "SCANNED design/gdd/tuning-data-config.md"
[ "$(count_lines "$obs" '^[^ ]+:[0-9]+ ')" -eq 0 ]
report clean-fixture-no-observations $? "$(printf '%s\n' "$obs" | grep -E '^[^ ]+:[0-9]+ ')"

# --- seeded: count mismatch ---------------------------------------------------------------------------------
root="$(fx_tree count)"
fx_replace "$root/design/gdd/tuning-data-config.md" '| Alpha System (1) | 4 (incl.' '| Alpha System (1) | 5 (incl.'
obs="$(py_tool knob_tables.py --root "$(knob_native "$root")")"
expect seeded-count-mismatch-reported "$obs" "design/gdd/tuning-data-config.md:9 knob-count design/gdd/alpha-system.md: Registered Owner Keys says 5, the owner Tuning Knobs table has 4 record(s)"
[ "$(count_lines "$obs" '^[^ ]+:[0-9]+ ')" -eq 1 ]
report seeded-count-mismatch-is-the-only-observation $? "$obs"
printf '%s\n' "$obs" | grep -v '^SCANNED' | sed 's/^/    /'

# --- seeded: duplicate key row ------------------------------------------------------------------------------
root="$(fx_tree dup)"
fx_replace "$root/design/gdd/alpha-system.md" '| `AlphaScaleMult` | **60.0**' '| `AlphaHoldSec` | **2.0**'
obs="$(py_tool knob_tables.py --root "$(knob_native "$root")")"
expect seeded-duplicate-key-reported "$obs" "design/gdd/alpha-system.md:20 knob-duplicate-key key AlphaHoldSec"
printf '%s\n' "$obs" | grep -v '^SCANNED' | sed 's/^/    /'

# --- seeded: a GDD without the section ----------------------------------------------------------------------
root="$(fx_tree nosection)"
printf '# Gamma System\n\n## Overview\n\nNo knobs.\n' | fx_write "$root/design/gdd/gamma-system.md"
obs="$(py_tool knob_tables.py --root "$(knob_native "$root")")"
expect seeded-missing-section-reported "$obs" "design/gdd/gamma-system.md:0 knob-no-section GDD has no Tuning Knobs section" "SCANNED design/gdd/gamma-system.md"
printf '%s\n' "$obs" | grep -v '^SCANNED' | sed 's/^/    /'

# --- seeded: a row with the wrong number of cells -----------------------------------------------------------
root="$(fx_tree shape)"
fx_replace "$root/design/gdd/beta-system.md" '| `BetaLimitCount` | int | 5 | 1-9 | 3-6 | Count |' '| `BetaLimitCount` | int | 5 | 1-9 | 3-6 |'
obs="$(py_tool knob_tables.py --root "$(knob_native "$root")")"
expect seeded-short-row-reported "$obs" "design/gdd/beta-system.md:10 knob-row-shape row has 6 cells, header has 7"
printf '%s\n' "$obs" | grep -v '^SCANNED' | sed 's/^/    /'

# --- the real tree --------------------------------------------------------------------------------------------
real="$lint/../../.."
rec="$(py_tool knob_tables.py --root "$(knob_native "$real")" --records)"
obs="$(py_tool knob_tables.py --root "$(knob_native "$real")")"
n="$(count_lines "$rec" '"kind": "knob"')"
[ "$n" -ge 400 ]
report real-tree-record-count $? "$n knob records (want at least 400)"
n_tables="$(count_lines "$rec" '"kind": "table".*"owned": true|"owned": true.*"kind": "table"')"
[ "$n_tables" -ge 19 ]
report real-tree-owned-table-per-gdd $? "$n_tables owned tables (want at least 19)"
sections="$(grep -l '^## Tuning Knobs' "$real"/design/gdd/*.md | wc -l | tr -d ' ')"
files_with_records="$(printf '%s\n' "$rec" | grep '"kind": "knob"' | sed 's/.*"gdd": "\([^"]*\)".*/\1/' | sort -u | wc -l | tr -d ' ')"
[ "$sections" -eq "$files_with_records" ]
report real-tree-every-section-yields-records $? "$sections sections, $files_with_records GDDs with records"
expect real-tree-preamble-style-lsc "$rec" '"key": "EmergenceSocketsMaxCount"' '"default": "24"'
expect real-tree-preamble-style-generator "$rec" '"key": "CityGenGridSideMaxCount"' '"default": "66"'
expect real-tree-pair-tail-expansion "$rec" '"key": "TraversalPointsMaxCount"' '"key": "BushBackdropFloorPct"' '"key": "ChainLengthMaxCm"'
expect real-tree-ellipsis-expansion "$rec" '"key": "CrowdAreaWeightMarketMult"' '"key": "CrowdAreaWeightResidentialMult"'
expect real-tree-whole-key-after-slash "$rec" '"key": "CrowdBushDwellSigma"' '"key": "FinalCandidatesMinCount"'
expect_not real-tree-no-retired-key-record "$(printf '%s\n' "$rec" | grep '"kind": "knob"')" '"key": "LatencyWarnRttSec"'
# Every observation the parser prints on the real tree keeps the contract (path:line rule message, or SCANNED).
bad_lines="$(printf '%s\n' "$obs" | grep -Ev '^(SCANNED [^ ]+|[^ ]+:[0-9]+ [a-z-]+ .+)$' | head -3)"
[ -z "$bad_lines" ]
report real-tree-observations-keep-the-contract $? "$bad_lines"
# The discrepancies the parser exists to list (for example the LSC key count in TDC's Registered Owner Keys
# table) are printed below as information. They are owner-table facts that change with the GDDs, so no case pins
# them; the seeded count mismatch above is the proof that one is reported.
printf '%s\n' "$obs" | grep 'knob-' | sed 's/^/    observed: /' | cut -c1-200

selftest_result
