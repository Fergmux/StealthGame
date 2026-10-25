#!/usr/bin/env bash
# Selftest for knob-lint-bounded-by.sh (story tdc-036; ADR-0004 section 4, the C-W18 pattern; ADR-0008 G2/G3/G6).
# Fixture tree tools/ci/lint/fixtures/knob/base: its TDC GDD has the I-1 bullet and an Owner invariants table
# (I-AB-1, I-AB-2 and an offline O-AB-3) and every owner row that an invariant reads says "bounded by <Id>".
# Seeded controls: a row that stops saying it is reported per key and invariant at the owner row (file:line); a
# wording variant ("bounded below by Generator I-AB-1") conforms while a longer id ("I-AB-10") does not; a key read
# by two invariants needs both ids; an id range row is satisfied by any id of the range; an offline row is skipped;
# the baseline hides a listed pair, reports a stale entry and a malformed entry; and, against a temporary git
# repository with a base commit, a baseline that GROWS fails the shrink-only check (G3) while a shrinking one and the
# creating commit are clean. The real tree must pass (its accepted findings are baselined, citing TD-REG-2).
#
# Usage: knob_lint_bounded_by_selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/knob-lint-bounded-by.sh")
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"
. "$here/knob-lib.sh"

A="design/gdd/alpha-system.md"
B="design/gdd/beta-system.md"
T="design/gdd/tuning-data-config.md"
BASELINE="tools/ci/lint/knob-bounded-by-baseline.txt"

gate_case conforming-rows-pass "$FIXTURE_BASE" 0 0 "GATE: PASS (3 file(s) scanned"

# --- a non-conforming row is reported per key and invariant, at the owner row ---------------------------------
root="$(fx_tree nonconforming)"
fx_replace "$root/$B" "A limit; bounded by I-AB-2. |" "A limit. |"
gate_case nonconforming-row-reported-with-file-and-line "$root" 1 1 \
  "$B:10 knob-bounded-by key BetaLimitCount is read by invariant I-AB-2 ($T:22) but its Effect cell does not say 'bounded by I-AB-2'"

# The I-1 bullet is read the same way as a table row.
root="$(fx_tree bullet)"
fx_replace "$root/$A" "Scale; bounded by I-1. Low" "Scale. Low"
gate_case bullet-invariant-read-like-a-table-row "$root" 1 1 "$A:20 knob-bounded-by key AlphaScaleMult is read by invariant I-1 ($T:16)"

# --- wording: variants conform, a longer id does not -------------------------------------------------------------
root="$(fx_tree variant)"
fx_replace "$root/$A" "Hold time; bounded by I-AB-1 (it stays below" "Hold time, bounded below by Generator I-AB-1 (it stays below"
gate_case below-by-owner-word-variant-conforms "$root" 0 0 "GATE: PASS"

root="$(fx_tree longer-id)"
fx_replace "$root/$A" "Hold time; bounded by I-AB-1 (it stays below" "Hold time; bounded by I-AB-10 (it stays below"
gate_case longer-id-does-not-satisfy-a-shorter-one "$root" 1 1 "$A:13 knob-bounded-by key AlphaHoldSec is read by invariant I-AB-1"

root="$(fx_tree no-bounded)"
fx_replace "$root/$A" "Hold time; bounded by I-AB-1 (it stays below" "Hold time; see I-AB-1 (it stays below"
gate_case mentioning-the-id-without-bounded-by-does-not-conform "$root" 1 1 "$A:13 knob-bounded-by key AlphaHoldSec"

# --- a key read by two invariants needs the text for both ---------------------------------------------------------
root="$(fx_tree two-invariants)"
fx_replace "$root/$T" "| O-AB-3 | Alpha |" "| I-AB-4 | Alpha | \`AlphaHoldSec >= 1\` | reject |
| O-AB-3 | Alpha |"
gate_case second-invariant-needs-its-own-text "$root" 1 1 "$A:13 knob-bounded-by key AlphaHoldSec is read by invariant I-AB-4 ($T:23)"
fx_replace "$root/$A" "Hold time; bounded by I-AB-1 (it stays below" "Hold time; bounded by I-AB-1 and bounded by I-AB-4 (it stays below"
gate_case both-ids-present-passes "$root" 0 0 "GATE: PASS"

# --- an id range row is one invariant row; any id of the range satisfies it -----------------------------------
root="$(fx_tree range-row)"
fx_replace "$root/$T" "| O-AB-3 | Alpha |" "| I-AB-5 to I-AB-7 | Beta | \`BetaLimitCount <= 9\` | reject |
| O-AB-3 | Alpha |"
gate_case range-row-reported-once-naming-the-range "$root" 1 1 "$B:10 knob-bounded-by key BetaLimitCount is read by invariant I-AB-5 to I-AB-7 ($T:23)"
fx_replace "$root/$B" "A limit; bounded by I-AB-2. |" "A limit; bounded by I-AB-2 and bounded by I-AB-6. |"
gate_case any-id-of-the-range-satisfies-it "$root" 0 0 "GATE: PASS"

# --- the baseline (each case is a git repository: the shrink-only comparison needs a base revision, G3) -----------
git_commit() { # <root>: initialises a repository in <root> and commits everything as the base revision
  git -C "$1" init -q
  git -C "$1" config user.name Selftest
  git -C "$1" config user.email selftest@example.invalid
  git -C "$1" config commit.gpgsign false
  git -C "$1" config core.autocrlf false
  git -C "$1" add -A >/dev/null 2>&1
  git -C "$1" commit -q -m "chore: selftest base" >/dev/null 2>&1
}

root="$(fx_tree baselined)"
fx_replace "$root/$B" "A limit; bounded by I-AB-2. |" "A limit. |"
printf '# accepted findings\n%s | BetaLimitCount | I-AB-2 | TD-REG-2: owner row predates the wording\n' "$B" | fx_write "$root/$BASELINE"
git_commit "$root"
gate_case baselined-pair-is-not-reported "$root" 0 0 "GATE: PASS"

root="$(fx_tree stale)"
printf '%s | BetaLimitCount | I-AB-2 | TD-REG-2: owner row predates the wording\n' "$B" | fx_write "$root/$BASELINE"
git_commit "$root"
gate_case stale-entry-reported-so-the-list-shrinks "$root" 1 1 "$BASELINE:1 knob-baseline-stale baseline entry '$B | BetaLimitCount | I-AB-2' is not a finding any more"

root="$(fx_tree malformed)"
printf '%s | BetaLimitCount | I-AB-2 |\n%s | BetaLimitCount\n' "$B" "$B" | fx_write "$root/$BASELINE"
git_commit "$root"
gate_case entry-without-a-citation-or-in-another-shape-reported "$root" 1 2 "$BASELINE:1 knob-baseline-malformed" "$BASELINE:2 knob-baseline-malformed"

# --- G3: the baseline may only shrink ------------------------------------------------------------------------------
# The base commit lists BetaLimitCount (a violation) in the baseline.
git_repo() { # <name>
  local root; root="$(fx_tree "$1")"
  fx_replace "$root/$B" "A limit; bounded by I-AB-2. |" "A limit. |"
  printf '# accepted\n%s | BetaLimitCount | I-AB-2 | TD-REG-2: owner row predates the wording\n' "$B" | fx_write "$root/$BASELINE"
  git_commit "$root"
  printf '%s\n' "$root"
}
root="$(git_repo grow)"
# A NEW violation is hidden by adding an entry: AlphaScaleMult is not in the base baseline, so the check fails.
fx_replace "$root/$A" "Scale; bounded by I-1. Low" "Scale. Low"
printf '%s | AlphaScaleMult | I-1 | TD-REG-2: added to hide a new violation\n' "$A" >> "$root/$BASELINE"
gate_case baseline-growth-fails-against-the-base "$root" 1 1 \
  "$BASELINE:3 shrink-only entry '$A | AlphaScaleMult | I-1 | TD-REG-2: added to hide a new violation' is not in the list at HEAD; the list may only shrink (G3)"

root="$(git_repo unchanged)"
gate_case unchanged-baseline-is-clean "$root" 0 0 "GATE: PASS"

root="$(git_repo shrink)"
fx_replace "$root/$B" "A limit. |" "A limit; bounded by I-AB-2. |"
printf '# accepted\n' | fx_write "$root/$BASELINE"
gate_case shrinking-baseline-is-clean "$root" 0 0 "GATE: PASS"

# The commit that creates the baseline has no base list to compare with: exempt (G3).
root="$(fx_tree creating-commit)"
fx_replace "$root/$B" "A limit; bounded by I-AB-2. |" "A limit. |"
git_commit "$root"
printf '%s | BetaLimitCount | I-AB-2 | TD-REG-2: first baseline\n' "$B" | fx_write "$root/$BASELINE"
gate_case commit-creating-the-baseline-is-exempt "$root" 0 0 "GATE: PASS"

# CI passes the push's before-SHA: an unresolvable base is a finding, never a silent pass.
root="$(git_repo bad-base)"
out="$(LINT_BASE_REF=refs/heads/does-not-exist bash "$lint/gate.sh" --root "$root" "${RULES[@]}" 2>&1)"; rc=$?
[ "$rc" -eq 1 ]
report unresolvable-base-ref-fails-exit $? "exit $rc"
expect unresolvable-base-ref-is-reported "$out" "shrink-only shrink-only baseline: base revision 'refs/heads/does-not-exist' cannot be resolved"

# A tree that is not a git checkout has no base revision either.
root="$(fx_tree not-git)"
printf '%s | BetaLimitCount | I-AB-2 | TD-REG-2: x\n' "$B" | fx_write "$root/$BASELINE"
out="$(bash "$lint/gate.sh" --root "$root" "${RULES[@]}" 2>&1)"; rc=$?
[ "$rc" -eq 1 ]
report non-git-tree-with-a-baseline-fails-exit $? "exit $rc"
expect non-git-tree-baseline-is-unverifiable "$out" "cannot be resolved"

gate_case real-tree-zero-findings "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
