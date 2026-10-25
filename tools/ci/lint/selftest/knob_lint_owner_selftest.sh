#!/usr/bin/env bash
# Selftest for knob-lint-owner.sh (story tdc-032; TDC GDD AC-3b bullet 2, CR-15.3; ADR-0008 G2/G6).
# Throwaway trees: a key owned by one GDD is silent; a key owned by two GDDs (or twice in one) is reported with
# every file name and line; a registry constant that no GDD owns (zero owners) is reported at its registry line;
# a key only referenced in a second GDD (a consumed table, a "**Owned by**" reference row, prose) is silent; a
# renamed key leaves its old registry name as a zero-owner report; a deprecated constant is not part of the key
# universe. A tree without a registry is NOT ASSESSED. The real design/gdd/*.md must pass.
#
# Usage: knob_lint_owner_selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/knob-lint-owner.sh")
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"
. "$here/knob-lib.sh"

HEAD='| Key | Type | Owner | Default | Hard range | Safe range | Audience | Latch | Unit | Effect |
|---|---|---|---|---|---|---|---|---|---|'
row() { printf '| `%s` | float | X | 1 | 0-2 | 0.5-1.5 | ServerOnly | Round | Sec | Hold. |\n' "$1"; }

# gdd <root> <file name> <title> <body after the table heading>: heading at line 3, table header line 5, first row line 7.
gdd() {
  printf '# %s\n\n## Tuning Knobs\n\n%s\n%s\n' "$3" "$HEAD" "$4" | fx_write "$1/design/gdd/$2"
}
# registry <root> <name:status:source>...: three lines per constant, the first entry at line 2.
registry() {
  local root="$1" spec; shift
  { echo "constants:"; for spec in "$@"; do
      IFS=: read -r n st src <<< "$spec"
      printf '  - name: %s\n    status: %s\n    source: design/gdd/%s\n' "$n" "$st" "$src"
    done; } | fx_write "$root/design/registry/entities.yaml"
}

root="$(fx_empty single)"
gdd "$root" alpha.md Alpha "$(row AlphaSec)"
registry "$root" AlphaSec:active:alpha.md
gate_case single-owner-silent "$root" 0 0 "GATE: PASS (2 file(s) scanned"

root="$(fx_empty two-owners)"
gdd "$root" alpha.md Alpha "$(row SharedSec)"
gdd "$root" beta.md Beta "$(row SharedSec)"
registry "$root" SharedSec:active:alpha.md
gate_case two-owners-reported-with-both-files "$root" 1 1 \
  "design/gdd/alpha.md:7 knob-owner key SharedSec is owned by 2 Tuning Knobs rows (design/gdd/alpha.md:7 and design/gdd/beta.md:7)"

root="$(fx_empty same-gdd-twice)"
gdd "$root" alpha.md Alpha "$(row SharedSec)
$(row SharedSec)"
registry "$root" SharedSec:active:alpha.md
gate_case one-gdd-owning-a-key-twice-reported "$root" 1 1 \
  "design/gdd/alpha.md:7 knob-owner key SharedSec is owned by 2 Tuning Knobs rows (design/gdd/alpha.md:7 and design/gdd/alpha.md:8)" "[same GDD twice]"

root="$(fx_empty zero-owner)"
gdd "$root" alpha.md Alpha "$(row AlphaSec)"
registry "$root" AlphaSec:active:alpha.md OrphanSec:active:alpha.md
gate_case zero-owner-reported-at-the-registry-line "$root" 1 1 \
  "design/registry/entities.yaml:5 knob-owner registry constant OrphanSec (source design/gdd/alpha.md) is owned by no GDD Tuning Knobs table (zero owners)"

root="$(fx_empty deprecated)"
gdd "$root" alpha.md Alpha "$(row AlphaSec)"
registry "$root" AlphaSec:active:alpha.md RetiredSec:deprecated:alpha.md
gate_case deprecated-constant-is-not-in-the-universe "$root" 0 0 "GATE: PASS"

# A key referenced (not owned) in a second GDD: a consumed table, a reference row, and prose all stay silent.
root="$(fx_empty referenced)"
gdd "$root" alpha.md Alpha "$(row AlphaSec)"
{
  printf '# Beta\n\n## Tuning Knobs\n\nBeta reads `AlphaSec` (owned by Alpha).\n\n%s\n%s\n' "$HEAD" "$(row BetaSec)"
  printf '| `AlphaSec` | float | X | 1 | 0-2 | 0.5-1.5 | ServerOnly | Round | Sec | **Owned by Alpha** (one quantity, one name); listed for reference, not registered here. |\n'
  printf '\nConsumed elsewhere:\n\n| Key | Owner | Default | Use |\n|---|---|---|---|\n| `AlphaSec` | Alpha | 1 | The hold. |\n'
} | fx_write "$root/design/gdd/beta.md"
registry "$root" AlphaSec:active:alpha.md BetaSec:active:beta.md
gate_case referenced-not-owned-silent "$root" 0 0 "GATE: PASS (3 file(s) scanned"

# A renamed key: the GDD now says NewSec, the registry still holds OldSec: OldSec is reported as a zero-owner
# constant and nothing else (the report stays factual; the new name is the registry-match lint's business).
root="$(fx_empty renamed)"
gdd "$root" alpha.md Alpha "$(row NewSec)"
registry "$root" OldSec:active:alpha.md
gate_case renamed-key-shows-as-zero-owner-under-its-old-name "$root" 1 1 "knob-owner registry constant OldSec"

# A retired (struck-through) row is not ownership.
root="$(fx_empty retired-row)"
gdd "$root" alpha.md Alpha "$(row AlphaSec)
| ~~\`GoneSec\`~~ | float | X | 1 | 0-2 | 0.5-1.5 | ServerOnly | Round | Sec | Retired. |"
gdd "$root" beta.md Beta "$(row GoneSec)"
registry "$root" AlphaSec:active:alpha.md GoneSec:active:beta.md
gate_case retired-row-is-not-a-second-owner "$root" 0 0 "GATE: PASS"

root="$(fx_empty no-registry)"
gdd "$root" alpha.md Alpha "$(row AlphaSec)"
gate_case missing-registry-is-not-assessed "$root" 2 0 "GATE: NOT ASSESSED"

gate_case base-fixture-clean "$FIXTURE_BASE" 0 0 "GATE: PASS (4 file(s) scanned"
gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
