#!/usr/bin/env bash
# Selftest for knob-lint-columns.sh (story tdc-031; TDC GDD AC-3b bullet 1, CR-15.1; ADR-0008 G2/G6).
# Throwaway trees with one GDD each: a complete table is silent; a table missing one CR-15 column is reported
# naming file and column (at the header line); reordered columns, an extra column, the two spellings of the Effect
# column, an owner-level preamble table that states Owner, Audience and Latch once, and a consumed-elsewhere table
# are silent; a preamble that does not state a missing column is reported; several tables in one file are checked
# one by one. A scope with no knob table is NOT ASSESSED, never a pass. The real design/gdd/*.md must pass.
#
# Usage: knob_lint_columns_selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/knob-lint-columns.sh")
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"
. "$here/knob-lib.sh"

F="design/gdd/one.md"
# one_gdd <case> <body after the heading>: a root whose only GDD is design/gdd/one.md (heading at line 3, body from line 5).
one_gdd() {
  local root; root="$(fx_empty "$1")"
  printf '# One\n\n## Tuning Knobs\n\n%s\n' "$2" | fx_write "$root/$F"
  printf '%s\n' "$root"
}

FULL_HEAD='| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|---|---|---|'
FULL_ROW='| `OneSec` | float | One | 1 | 0-2 | 0.5-1.5 | ServerOnly | Round | Sec | Hold. |'

root="$(one_gdd complete "$FULL_HEAD
$FULL_ROW")"
gate_case complete-table-silent "$root" 0 0 "GATE: PASS (1 file(s) scanned"

root="$(one_gdd effect-spelling "| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect / what breaks |
|---|---|---|---|---|---|---|---|---|---|
$FULL_ROW")"
gate_case effect-matched-by-leading-word "$root" 0 0 "GATE: PASS"

root="$(one_gdd missing-latch "| Key | Type | Owner | Default | Hard range | Safe range | Audience | Unit | Effect |
|---|---|---|---|---|---|---|---|---|
| \`OneSec\` | float | One | 1 | 0-2 | 0.5-1.5 | ServerOnly | Sec | Hold. |")"
gate_case missing-latch-reported "$root" 1 1 "$F:5 knob-columns Tuning Knobs table is missing the CR-15 column 'Latch'" "GATE: FAIL (1 finding(s)"

root="$(one_gdd missing-effect "| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit |
|---|---|---|---|---|---|---|---|---|
| \`OneSec\` | float | One | 1 | 0-2 | 0.5-1.5 | ServerOnly | Round | Sec |")"
gate_case missing-effect-reported "$root" 1 1 "$F:5 knob-columns Tuning Knobs table is missing the CR-15 column 'Effect'"

root="$(one_gdd missing-hard-range "| Key | Type | Owner | Default | Safe range | Audience | Latch | Unit | Effect |
|---|---|---|---|---|---|---|---|---|
| \`OneSec\` | float | One | 1 | 0.5-1.5 | ServerOnly | Round | Sec | Hold. |")"
gate_case missing-hard-range-reported "$root" 1 1 "$F:5 knob-columns Tuning Knobs table is missing the CR-15 column 'Hard range'"

root="$(one_gdd missing-two "| Key | Default | Hard range | Safe range | Audience | Latch | Unit | Effect |
|---|---|---|---|---|---|---|---|
| \`OneSec\` | 1 | 0-2 | 0.5-1.5 | ServerOnly | Round | Sec | Hold. |")"
gate_case each-missing-column-is-its-own-finding "$root" 1 2 "'Type'" "'Owner'"

root="$(one_gdd reordered "| Latch | Audience | Key | Effect | Unit | Type | Owner | Safe range | Hard range | Default |
|---|---|---|---|---|---|---|---|---|---|
| Round | ServerOnly | \`OneSec\` | Hold. | Sec | float | One | 0.5-1.5 | 0-2 | 1 |")"
gate_case reordered-columns-silent "$root" 0 0 "GATE: PASS"

root="$(one_gdd extra-column "| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Notes | Effect |
|---|---|---|---|---|---|---|---|---|---|---|
| \`OneSec\` | float | One | 1 | 0-2 | 0.5-1.5 | ServerOnly | Round | Sec | n | Hold. |")"
gate_case extra-column-silent "$root" 0 0 "GATE: PASS"

# Several tables in one file: the first is complete, the second lacks Unit (its header is at line 12).
root="$(one_gdd two-tables "$FULL_HEAD
$FULL_ROW

More knobs.

| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Effect |
|---|---|---|---|---|---|---|---|---|
| \`TwoSec\` | float | One | 1 | 0-2 | 0.5-1.5 | ServerOnly | Round | Hold. |")"
gate_case two-tables-checked-one-by-one "$root" 1 1 "$F:11 knob-columns Tuning Knobs table is missing the CR-15 column 'Unit'"

# Owner-level preamble style (Level Socket Contract, Procedural City Generator): Owner, Audience and Latch stated once.
PRE_HEAD='| Key | Type | Default | Hard range | Safe range | Unit | Effect / breaks if too high or low |
|---|---|---|---|---|---|---|'
PRE_ROW='| `OneSec` | float | 1 | 0-2 | 0.5-1.5 | Sec | Hold. |'
root="$(one_gdd preamble-complete "Registered under CR-15; owner One for every row. **Audience** \`ServerOnly\`. **Latch** \`Round\`.

$PRE_HEAD
$PRE_ROW")"
gate_case preamble-supplies-owner-audience-latch-silent "$root" 0 0 "GATE: PASS"

root="$(one_gdd preamble-no-latch "Registered under CR-15; owner One for every row. **Audience** \`ServerOnly\`.

$PRE_HEAD
$PRE_ROW")"
gate_case preamble-without-latch-reported "$root" 1 1 "$F:7 knob-columns Tuning Knobs table is missing the CR-15 column 'Latch'"

root="$(one_gdd preamble-audience-only "**Audience** \`ServerOnly\` for every row.

$PRE_HEAD
$PRE_ROW")"
gate_case preamble-audience-only-reports-owner-and-latch "$root" 1 2 "'Owner'" "'Latch'"

root="$(one_gdd no-preamble "$PRE_HEAD
$PRE_ROW")"
gate_case table-without-preamble-reports-all-three "$root" 1 3 "'Owner'" "'Audience'" "'Latch'"

# A consumed-elsewhere table (no Hard range, no Safe range) lists keys the GDD reads: not checked.
root="$(one_gdd consumed "$FULL_HEAD
$FULL_ROW

Knobs it consumes:

| Key | Owner | Default | Use |
|---|---|---|---|
| \`BetaSec\` | Beta | 3 | The window. |")"
gate_case consumed-elsewhere-table-silent "$root" 0 0 "GATE: PASS (1 file(s) scanned"

# Only a consumed table: nothing was examined, so the gate is NOT ASSESSED (exit 2), never a pass.
root="$(one_gdd only-consumed "| Key | Owner | Default | Use |
|---|---|---|---|
| \`BetaSec\` | Beta | 3 | The window. |")"
gate_case no-owned-table-is-not-assessed "$root" 2 0 "GATE: NOT ASSESSED"

root="$(fx_empty empty-scope)"
gate_case empty-scope-is-not-assessed "$root" 2 0 "GATE: NOT ASSESSED"

# The shared base fixture (alpha: standard and reordered tables; beta: preamble style; tdc) is clean.
gate_case base-fixture-clean "$FIXTURE_BASE" 0 0 "GATE: PASS (3 file(s) scanned"

gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
