#!/usr/bin/env bash
# Selftest for knob-lint-registry.sh (story tdc-033; TDC GDD AC-3b bullet 3 registry half, CR-15.7; ADR-0008 G2/G6).
# One throwaway tree whose owner table and registry differ in exactly one field per key: source GDD, default, hard
# range, safe range, audience, latch (one finding each, naming key and field), a registry name that differs from the
# owner key, a key with no constant (one finding, not one per field) and a key wrong in two fields (two findings).
# Equivalent spellings stay silent: 60 and 60.0, thousands separators, "10 to 20" and "10-20", annotations in
# parentheses, case, and a placeholder cell against an absent field. The real tree must pass.
#
# Usage: knob_lint_registry_selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/knob-lint-registry.sh")
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"
. "$here/knob-lib.sh"

HEAD='| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect |
|---|---|---|---|---|---|---|---|---|---|'
# trow <key> <default> <hard> <safe> <audience> <latch>
trow() { printf '| `%s` | float | X | %s | %s | %s | %s | %s | Sec | Effect. |\n' "$@"; }
# rentry <name> <source file> <value> <hard|-> <safe|-> <audience> <latch>   ("-" writes no field)
rentry() {
  printf '  - name: %s\n    status: active\n    source: design/gdd/%s\n    referenced_by:\n      - design/gdd/%s\n    value: %s\n    unit: s\n' "$1" "$2" "$2" "$3"
  [ "$4" = "-" ] || printf '    hard_range: %s\n' "$4"
  [ "$5" = "-" ] || printf '    safe_range: %s\n' "$5"
  printf '    audience: %s\n    latch: %s\n    added: 2026-10-25\n    revised: ""\n' "$6" "$7"
}
# make_tree <case> <GDD rows...> : alpha.md holds the rows; the registry is read from stdin.
make_tree() {
  local root; root="$(fx_empty "$1")"; shift
  { printf '# Alpha\n\n## Tuning Knobs\n\n%s\n' "$HEAD"; printf '%s\n' "$@"; } | fx_write "$root/design/gdd/alpha.md"
  { echo "constants:"; cat; } | fx_write "$root/design/registry/entities.yaml"
  printf '%s\n' "$root"
}

# --- equivalent spellings and matching keys are silent -----------------------------------------------------------
root="$(make_tree equal \
  "$(trow KSame 60.0 0-100 '10-90 (provisional pending spike)' ServerOnly Round)" \
  "$(trow KThousands '4,096' '256-65,536' '1,024-8,192' ServerOnly Round)" \
  "$(trow KRangeWords 5 '10 to 20' '12 - 18' ServerOnly Round)" \
  "$(trow KPlaceholder 5 '—' '--' ServerOnly Round)" \
  "$(trow KCase true 0-1 0-1 serveronly round)" \
  "$(trow KMarkup '**2.50**' '`1-4`' '*2-3*' ServerOnly Round)" <<EOF
$(rentry KSame alpha.md 60 0-100 10-90 ServerOnly Round)
$(rentry KThousands alpha.md 4096 256-65536 1024-8192 ServerOnly Round)
$(rentry KRangeWords alpha.md 5 10-20 12-18 ServerOnly Round)
$(rentry KPlaceholder alpha.md 5 - - ServerOnly Round)
$(rentry KCase alpha.md True 0-1 0-1 ServerOnly Round)
$(rentry KMarkup alpha.md 2.5 1-4 2-3 ServerOnly Round)
EOF
)"
gate_case equivalent-spellings-silent "$root" 0 0 "GATE: PASS (2 file(s) scanned"

# --- one seeded mismatch per compared field ------------------------------------------------------------------------
root="$(make_tree seeded \
  "$(trow KDefault 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KHard 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KSafe 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KAudience 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KLatch 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KSource 5 1-9 2-4 ServerOnly Round)" \
  "$(trow NameSec 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KMissing 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KTwo 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KExtraPart 5 1-9 2-4 ServerOnly Round)" \
  "$(trow KMatch 5 1-9 2-4 ServerOnly Round)" <<EOF
$(rentry KDefault alpha.md 6 1-9 2-4 ServerOnly Round)
$(rentry KHard alpha.md 5 1-10 2-4 ServerOnly Round)
$(rentry KSafe alpha.md 5 1-9 2-5 ServerOnly Round)
$(rentry KAudience alpha.md 5 1-9 2-4 All Round)
$(rentry KLatch alpha.md 5 1-9 2-4 ServerOnly Use)
$(rentry KSource beta.md 5 1-9 2-4 ServerOnly Round)
$(rentry nameSec alpha.md 5 1-9 2-4 ServerOnly Round)
$(rentry KTwo alpha.md 7 1-9 2-4 ServerOnly Use)
$(rentry KExtraPart alpha.md 5 '1-9 or 12' 2-4 ServerOnly Round)
$(rentry KMatch alpha.md 5 1-9 2-4 ServerOnly Round)
EOF
)"
gate_case one-finding-per-mismatching-field "$root" 1 11 \
  "knob-registry key KDefault: default differs: owner row '5' vs registry '6'" \
  "knob-registry key KHard: hard range differs: owner row '1-9' vs registry '1-10'" \
  "knob-registry key KSafe: safe range differs: owner row '2-4' vs registry '2-5'" \
  "knob-registry key KAudience: audience differs: owner row 'ServerOnly' vs registry 'All'" \
  "knob-registry key KLatch: latch differs: owner row 'Round' vs registry 'Use'" \
  "knob-registry key KSource: source GDD differs: owner row 'design/gdd/alpha.md' vs registry 'design/gdd/beta.md'" \
  "knob-registry key NameSec: name differs from registry constant 'nameSec'" \
  "knob-registry key KMissing: missing constant in design/registry/entities.yaml" \
  "knob-registry key KTwo: default differs: owner row '5' vs registry '7'" \
  "knob-registry key KTwo: latch differs: owner row 'Round' vs registry 'Use'" \
  "knob-registry key KExtraPart: hard range differs: owner row '1-9' vs registry '1-9 or 12'" \
  "GATE: FAIL (11 finding(s)"
out="$(bash "$lint/gate.sh" --root "$root" "${RULES[@]}" 2>&1)"
[ "$(count_lines "$out" 'knob-registry key KMissing')" -eq 1 ]
report missing-constant-is-one-observation-not-one-per-field $? "$out"
[ "$(count_lines "$out" 'knob-registry key KMatch')" -eq 0 ]
report matching-key-is-silent $? "$out"
# Findings are placed at the owner row (the line of the key's row in alpha.md), and name the registry line.
expect finding-is-at-the-owner-row-line "$out" "design/gdd/alpha.md:7 knob-registry key KDefault" "design/gdd/alpha.md:8 knob-registry key KHard" "(registry line "

# --- rows that are not preset fields are not compared ----------------------------------------------------------
root="$(make_tree not-a-key \
  "$(trow KReal 5 1-9 2-4 ServerOnly Round)" \
  '| `BlindSec` | float (schema constant, code) | TDC | 50 | -- | 40 to 70 | ServerOnly | -- | percent | A constant. |' \
  '| Catalog `bFlag` flag | bool per preset | TDC | false | -- | -- | ServerOnly | Round | flag | A flag. |' <<EOF
$(rentry KReal alpha.md 5 1-9 2-4 ServerOnly Round)
EOF
)"
gate_case schema-constant-and-catalog-flag-rows-not-compared "$root" 0 0 "GATE: PASS"

# --- a registry that cannot be read is NOT ASSESSED -------------------------------------------------------------
root="$(fx_empty no-registry)"
printf '# Alpha\n\n## Tuning Knobs\n\n%s\n%s\n' "$HEAD" "$(trow KReal 5 1-9 2-4 ServerOnly Round)" | fx_write "$root/design/gdd/alpha.md"
gate_case missing-registry-is-not-assessed "$root" 2 0 "GATE: NOT ASSESSED"

gate_case base-fixture-clean "$FIXTURE_BASE" 0 0 "GATE: PASS (4 file(s) scanned"
gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
