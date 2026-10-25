#!/usr/bin/env bash
# Selftest for check-lfs-locks.sh (story tdc-027; ADR-0009 section 4 and Validation 2; ADR-0008 G2). The lock
# list comes from an injected command (STEALTH_LFS_LOCKS_CMD), so no remote is needed. Temporary git
# repositories only; no commit is made in the real working copy.
#
# Usage: run-lfs-locks-selftest.sh     Exit 0 when every case holds, else 1.
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib.sh"

check="$hooks/check-lfs-locks.sh"
T=$'\t'
P1='Content/Maps/L_Test.umap'
P2='Content/Props/SM_Crate.uasset'
P3='Content/Props/SM_Barrel.uasset'

repo="$(new_repo base)"
mkdir -p "$repo/Content/Maps" "$repo/Content/Props" "$repo/Source"
for f in "$P1" "$P2" "$P3" Source/readme.txt; do printf 'v1\n' > "$repo/$f"; done
git -C "$repo" add -A >/dev/null 2>&1
git -C "$repo" commit -q -m "chore: base" >/dev/null 2>&1

# set_source <canned lock-list text> [exit code]  -> STEALTH_LFS_LOCKS_CMD printing that text
set_source() {
  printf '%s' "$1" > "$tmp/locks.txt"
  export STEALTH_LFS_LOCKS_CMD="cat '$tmp/locks.txt'; exit ${2:-0}"
}
run_check() { out="$(cd "$repo" && STEALTH_HOOK_REPO="$repo" bash "$check" 2>&1)"; rc=$?; }
reset_tree() { git -C "$repo" reset -q --hard HEAD; }
stage_change() { for f in "$@"; do printf 'v-%s\n' "$RANDOM" > "$repo/$f"; done; git -C "$repo" add -A >/dev/null 2>&1; }

# (a) a lock held by the committer is accepted
set_source "O $P2${T}Selftest${T}ID:11"$'\n'
stage_change "$P2"; run_check
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -Fq "lfs-locks: PASS"; then report lock-held-by-committer-accepted 0 ""; else report lock-held-by-committer-accepted 1 "rc=$rc out=$out"; fi
reset_tree

# (b) no lock at all, or another owner's lock, is rejected and names the path
set_source ""
stage_change "$P2"; run_check
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "$P2:0 lfs-lock staged modification of $P2 without a lock held by you (it is not locked)"; then report no-lock-rejected-names-path 0 ""; else report no-lock-rejected-names-path 1 "rc=$rc out=$out"; fi
printf '%s\n' "$out" | sed 's/^/    /'
reset_tree
set_source "  $P2${T}SomeoneElse${T}ID:12"$'\n'
stage_change "$P2"; run_check
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "the lock is held by someone else"; then report other-owner-lock-rejected 0 ""; else report other-owner-lock-rejected 1 "rc=$rc out=$out"; fi
reset_tree

# (c) a remote that reports no lock support degrades to a printed single-writer reminder
set_source "Remote \"origin\" does not support the Git LFS locking API. Consider disabling it with:"$'\n' 2
stage_change "$P2"; run_check
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -Fq "NOTE single-writer" && printf '%s' "$out" | grep -Fq "degraded"; then report unsupported-remote-degrades-with-reminder 0 ""; else report unsupported-remote-degrades-with-reminder 1 "rc=$rc out=$out"; fi
printf '%s\n' "$out" | sed 's/^/    /'
reset_tree
set_source 'Error while retrieving locks: missing protocol: ""'$'\n' 2
stage_change "$P2"; run_check
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -Fq "NOTE single-writer"; then report no-remote-degrades-with-reminder 0 ""; else report no-remote-degrades-with-reminder 1 "rc=$rc out=$out"; fi
reset_tree

# Any other source failure is NOT ASSESSED, not a pass (an authentication or network error).
set_source "Error while retrieving locks: authentication required: 401"$'\n' 2
stage_change "$P2"; run_check
if [ "$rc" -eq 2 ] && printf '%s' "$out" | grep -Fq "NOT ASSESSED"; then report source-failure-not-assessed 0 ""; else report source-failure-not-assessed 1 "rc=$rc out=$out"; fi
reset_tree
set_source "garbage line that is neither ours nor theirs"$'\n'
stage_change "$P2"; run_check
if [ "$rc" -eq 2 ] && printf '%s' "$out" | grep -Fq "unrecognised lock source line"; then report malformed-source-line-not-assessed 0 ""; else report malformed-source-line-not-assessed 1 "rc=$rc out=$out"; fi
reset_tree

# A change with no .uasset/.umap never queries the source (even a broken one).
set_source "garbage"$'\n' 2
stage_change Source/readme.txt; run_check
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -Fq "lock source was not queried"; then report non-asset-change-ignored 0 ""; else report non-asset-change-ignored 1 "rc=$rc out=$out"; fi
reset_tree

# Several assets, one unlocked: rejected, naming only the unlocked path.
set_source "O $P1${T}Selftest${T}ID:21"$'\n'"O $P3${T}Selftest${T}ID:22"$'\n'
stage_change "$P1" "$P2" "$P3"; run_check
n="$(printf '%s\n' "$out" | grep -c '^[^ ]*:0 lfs-lock ')"
if [ "$rc" -eq 1 ] && [ "$n" -eq 1 ] && printf '%s' "$out" | grep -Fq "$P2:0 lfs-lock"; then report several-assets-one-unlocked-names-only-it 0 ""; else report several-assets-one-unlocked-names-only-it 1 "rc=$rc findings=$n out=$out"; fi
reset_tree

# .umap is checked like .uasset, and an upper-case extension is still an asset.
set_source ""
stage_change "$P1"; run_check
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "$P1:0 lfs-lock"; then report umap-checked 0 ""; else report umap-checked 1 "rc=$rc out=$out"; fi
reset_tree
staged="$tmp/staged.txt"; printf 'M\tContent/Props/SM_Crate.UASSET\n' > "$staged"
out="$(cd "$repo" && STEALTH_HOOK_REPO="$repo" STEALTH_HOOK_STAGED="$staged" bash "$check" 2>&1)"; rc=$?
if [ "$rc" -eq 1 ]; then report uppercase-extension-checked 0 ""; else report uppercase-extension-checked 1 "rc=$rc out=$out"; fi

# Lock path comparison ignores case and slash direction (Windows).
set_source "O content/props/SM_CRATE.uasset${T}Selftest${T}ID:31"$'\n'
stage_change "$P2"; run_check
if [ "$rc" -eq 0 ]; then report lock-path-case-insensitive 0 ""; else report lock-path-case-insensitive 1 "rc=$rc out=$out"; fi
reset_tree

# A deletion needs the lock; a rename needs the lock on the OLD path.
set_source ""
git -C "$repo" rm -q "$P2"; run_check
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "staged deletion of $P2"; then report deletion-needs-lock 0 ""; else report deletion-needs-lock 1 "rc=$rc out=$out"; fi
reset_tree
git -C "$repo" mv "$P2" Content/Props/SM_Renamed.uasset; run_check
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "staged rename of $P2"; then report rename-needs-lock-on-old-path 0 ""; else report rename-needs-lock-on-old-path 1 "rc=$rc out=$out"; fi
reset_tree
set_source "O $P2${T}Selftest${T}ID:41"$'\n'
git -C "$repo" mv "$P2" Content/Props/SM_Renamed.uasset; run_check
if [ "$rc" -eq 0 ]; then report rename-with-lock-on-old-path-accepted 0 ""; else report rename-with-lock-on-old-path-accepted 1 "rc=$rc out=$out"; fi
reset_tree

# An ADDED asset needs no lock of its own (no one can hold it yet) unless someone else locked that path.
set_source ""
mkdir -p "$repo/Content/New"; printf 'n\n' > "$repo/Content/New/SM_New.uasset"; git -C "$repo" add -A >/dev/null 2>&1; run_check
if [ "$rc" -eq 0 ]; then report new-asset-needs-no-lock 0 ""; else report new-asset-needs-no-lock 1 "rc=$rc out=$out"; fi
set_source "  Content/New/SM_New.uasset${T}SomeoneElse${T}ID:51"$'\n'
run_check
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "locked by someone else"; then report new-asset-locked-by-other-rejected 0 ""; else report new-asset-locked-by-other-rejected 1 "rc=$rc out=$out"; fi
reset_tree; rm -rf "$repo/Content/New"

# The same check inside the installed hook (story 025 contract): rejected without a lock, accepted with one.
hrepo="$(new_repo hook with-lint)"
mkdir -p "$hrepo/Content/Props"; printf 'v1\n' > "$hrepo/$P2"
git -C "$hrepo" add -A >/dev/null 2>&1
git -C "$hrepo" commit -q -m "chore: asset" -m "Story: SELFTEST-1" >/dev/null 2>&1
(cd "$hrepo" && bash tools/ci/hooks/install-hooks.sh >/dev/null 2>&1)
set_source ""
wr "$hrepo/$P2" v2; git -C "$hrepo" add -A >/dev/null 2>&1
head_before="$(git -C "$hrepo" rev-parse HEAD)"
try_commit "$hrepo" "fix: edit asset"
if [ "$commit_rc" -ne 0 ] && [ "$head_before" = "$(git -C "$hrepo" rev-parse HEAD)" ] && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT check-lfs-locks: REJECTED"; then report hook-rejects-unlocked-asset 0 ""; else report hook-rejects-unlocked-asset 1 "rc=$commit_rc out=$commit_out"; fi
printf '%s\n' "$commit_out" | sed 's/^/    /'
set_source "O $P2${T}Selftest${T}ID:61"$'\n'
try_commit "$hrepo" "fix: edit asset locked"
if [ "$commit_rc" -eq 0 ] && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT check-lfs-locks: PASS"; then report hook-accepts-locked-asset 0 ""; else report hook-accepts-locked-asset 1 "rc=$commit_rc out=$commit_out"; fi

selftest_result
