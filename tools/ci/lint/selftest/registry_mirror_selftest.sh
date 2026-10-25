#!/usr/bin/env bash
# Selftest for registry_mirror.py (story tdc-030; CR-15.7, OQ-15, TD-REG-1; ADR-0008 G2/G6).
# Fixture tree: tools/ci/lint/fixtures/knob/base. Seeded controls: a registry missing a key (the mirror adds it with
# every field), an existing constant that differs from its owner row (the mirror leaves it alone), a bool key with
# placeholder ranges, a CRLF registry and an unsupported layout. The old text must stay byte-identical, the file
# must still parse, no name may be duplicated and a second run must add nothing. The real tree must need nothing.
#
# Usage: registry_mirror_selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"
. "$here/knob-lib.sh"

mirror() { py_tool registry_mirror.py --root "$(knob_native "$1")" --date 2026-10-25 "${@:2}"; }
names() { # <registry>  one constant name per line, sorted, via the same reader the lints use
  $PY -I - "$(knob_native "$1")" <<'PYEOF'
import sys
sys.path.insert(0, sys.argv[2] if len(sys.argv) > 2 else ".")
PYEOF
}
names() { # <registry file>
  # shellcheck disable=SC2086
  $PY -I -c "
import sys
sys.path.insert(0, sys.argv[1])
import registry_io
constants, problems = registry_io.read_constants(sys.argv[2])
print(len(constants), len({c['name'] for c in constants}), len(problems))
" "$(knob_native "$lint")" "$(knob_native "$1")"
}
drop_constant() { # <registry> <name>  removes one constant block (lines from its '- name:' to the next entry)
  # shellcheck disable=SC2086
  $PY -I -c "
import re, sys
path, name = sys.argv[1], sys.argv[2]
text = open(path, encoding='utf-8', newline='').read()
m = re.search(r'^  - name: %s\n(?:(?!  - name:).*\n)*' % re.escape(name), text, re.M)
assert m, name
open(path, 'w', encoding='utf-8', newline='').write(text[:m.start()] + text[m.end():])
" "$(knob_native "$1")" "$2"
}

# --- a missing key is added with every field ---------------------------------------------------------------
root="$(fx_tree missing)"
reg="$root/design/registry/entities.yaml"
drop_constant "$reg" BetaLimitCount
# An existing constant that differs from its owner row (the audience): the mirror must not touch it.
fx_replace "$reg" "    value: 2
    unit: s
    hard_range: 1-6
    safe_range: 1.5-4
    audience: ServerOnly" "    value: 2
    unit: s
    hard_range: 1-6
    safe_range: 1.5-4
    audience: All"
cp "$reg" "$tmp/missing.before"
size="$(wc -c < "$reg" | tr -d ' ')"
out="$(mirror "$root")"; rc=$?
[ "$rc" -eq 0 ]
report mirror-exit-zero $? "exit $rc"
printf '%s\n' "$out" | sed 's/^/    /'
expect missing-key-reported-and-added "$out" "MIRROR missing 1" "ADDED BetaLimitCount design/gdd/beta-system.md:10" "MIRROR result: added 1, existing 7 untouched"
entry="$(sed -n '/^  - name: BetaLimitCount$/,$p' "$reg")"
expect added-constant-has-every-field "$entry" \
  "    status: active" "    source: design/gdd/beta-system.md" "      - design/gdd/beta-system.md" \
  "    value: 5" "    unit: count" "    hard_range: 1-9" "    safe_range: 3-6" "    audience: ServerOnly" "    latch: Round" \
  "    added: 2026-10-25" "design/gdd/beta-system.md:10 (CR-15.7, TD-REG-1, story tdc-030)"
cmp -s -n "$size" "$tmp/missing.before" "$reg"
report old-text-is-byte-identical $? "the first $size bytes changed"
expect_not existing-constant-not-altered "$out" "AlphaHoldSec"
grep -Fxq "    audience: All" "$reg"
report altered-existing-constant-left-alone $? "the audience of AlphaHoldSec was rewritten"
[ "$(names "$reg")" = "8 8 0" ]
report file-parses-with-no-duplicate-names $? "reader said: $(names "$reg") (want constants, distinct names, layout problems = 8 8 0)"
if $PY -I -c "import yaml" >/dev/null 2>&1; then
  $PY -I -c "
import sys, yaml
d = yaml.safe_load(open(sys.argv[1], encoding='utf-8'))
names = [c['name'] for c in d['constants']]
sys.exit(0 if len(names) == len(set(names)) == 8 else 1)" "$(knob_native "$reg")"
  report yaml-safe-load-parses $? "yaml.safe_load failed or the constant count is wrong"
else
  echo "SELFTEST yaml-safe-load-parses skipped (PyYAML is not installed; the lint reader above is the check)"
fi
cp "$reg" "$tmp/missing.after1"
out="$(mirror "$root")"
expect second-run-adds-nothing "$out" "MIRROR missing 0" "MIRROR result: added 0, existing 8 untouched"
cmp -s "$tmp/missing.after1" "$reg"
report second-run-leaves-the-file-identical $? "the second run changed the file"

# --- dry run writes nothing ----------------------------------------------------------------------------------
root="$(fx_tree dry)"
reg="$root/design/registry/entities.yaml"
drop_constant "$reg" BetaLimitCount
cp "$reg" "$tmp/dry.before"
out="$(mirror "$root" --dry-run)"
expect dry-run-reports "$out" "MIRROR missing 1" "(dry run)"
cmp -s "$tmp/dry.before" "$reg"
report dry-run-writes-nothing $? "the registry changed"

# --- the owner of a pair key is the GDD that owns the row, not the reference row ---------------------------
root="$(fx_tree pair)"
reg="$root/design/registry/entities.yaml"
drop_constant "$reg" AlphaMaxCount
out="$(mirror "$root")"
expect pair-key-added-for-its-owner "$out" "ADDED AlphaMaxCount design/gdd/alpha-system.md:14"
expect_not reference-row-not-mirrored-as-owned "$out" "ADDED AlphaMaxCount design/gdd/beta-system.md"
entry="$(sed -n '/^  - name: AlphaMaxCount$/,$p' "$reg")"
expect pair-key-takes-its-own-half-of-each-cell "$entry" "    value: 8" "    hard_range: 4-16" "    safe_range: 6-10"

# --- bool key: value is bare, placeholder ranges write no field; annotations kept as a note ---------------
root="$(fx_tree bool)"
reg="$root/design/registry/entities.yaml"
fx_replace "$root/design/gdd/beta-system.md" "| ~~\`BetaOldSec\`~~ |" "| \`bBetaFlag\` | bool | true (on) | -- | — | — | A switch. |
| ~~\`BetaOldSec\`~~ |"
out="$(mirror "$root")"
expect bool-key-added "$out" "ADDED bBetaFlag design/gdd/beta-system.md:11"
entry="$(sed -n '/^  - name: bBetaFlag$/,$p' "$reg")"
expect bool-value-bare-and-unit-bool "$entry" "    value: true" "    unit: bool" '    default_note: "on"'
expect_not placeholder-ranges-write-no-field "$entry" "hard_range:"
expect_not placeholder-safe-range-writes-no-field "$entry" "safe_range:"

# --- CRLF registry: the appended text uses the file's line endings ------------------------------------------
root="$(fx_tree crlf)"
reg="$root/design/registry/entities.yaml"
drop_constant "$reg" BetaLimitCount
# shellcheck disable=SC2086
$PY -I -c "
import sys
p = sys.argv[1]
t = open(p, encoding='utf-8', newline='').read().replace(chr(13) + chr(10), chr(10)).replace(chr(10), chr(13) + chr(10))
open(p, 'w', encoding='utf-8', newline='').write(t)" "$(knob_native "$reg")"
cp "$reg" "$tmp/crlf.before"
size="$(wc -c < "$reg" | tr -d ' ')"
out="$(mirror "$root")"
expect crlf-registry-mirrored "$out" "MIRROR result: added 1"
cmp -s -n "$size" "$tmp/crlf.before" "$reg"
report crlf-old-text-byte-identical $? "the first $size bytes changed"
# shellcheck disable=SC2086
$PY -I -c "
import sys
data = open(sys.argv[1], 'rb').read()[int(sys.argv[2]):]
crlf = chr(13) + chr(10)
bare = data.decode('utf-8').replace(crlf, '').count(chr(10))
sys.exit(0 if data.decode('utf-8').count(crlf) > 10 and bare == 0 else 1)" "$(knob_native "$reg")" "$size"
report crlf-appended-lines-end-in-crlf $? "the appended text has a bare LF or no CRLF"

# --- an unsupported layout refuses to write -----------------------------------------------------------------
root="$(fx_tree layout)"
reg="$root/design/registry/entities.yaml"
drop_constant "$reg" BetaLimitCount
printf '  stray text that is not a field\n' >> "$reg"
cp "$reg" "$tmp/layout.before"
out="$(mirror "$root")"; rc=$?
[ "$rc" -eq 2 ]
report unsupported-layout-exits-2 $? "exit $rc"
expect unsupported-layout-reported "$out" "unsupported layout"
cmp -s "$tmp/layout.before" "$reg"
report unsupported-layout-writes-nothing $? "the registry changed"

# --- the real tree needs nothing (the mirror is complete) ---------------------------------------------------
out="$(mirror "$lint/../../.." --dry-run)"
expect real-tree-mirror-complete "$out" "MIRROR missing 0"

selftest_result
