#!/usr/bin/env bash
# Selftest for check-never-resave.sh (story tdc-026; ADR-0009 section 4 and Validation 4; ADR-0008 G2).
# Temporary list files and temporary git repositories only: the real list is read, never edited, and no
# commit is made in the real working copy.
#
# Usage: run-never-resave-selftest.sh     Exit 0 when every case holds, else 1.
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib.sh"

check="$hooks/check-never-resave.sh"
A='Content/Blueprints/Cameras/CameraRig_BaseDefaults.uasset'
B='Content/Blueprints/Cameras/CameraAsset_SandboxCharacter.uasset'

# A list like the real one, plus the traps the parser must not fall into: prose naming a path, a header row,
# the separator row, a path in the SECOND column, a backtick-quoted path, and a backslash path.
list="$tmp/list.md"
cat > "$list" <<'LIST'
# Never-resave assets

Prose line naming Content/ProseOnly/Mention.uasset must not become a listed path.

| Asset path | Reason |
|---|---|
| Content/Blueprints/Cameras/CameraRig_BaseDefaults.uasset | restored; see Content/Second/Column.uasset |
| `Content\Blueprints\Cameras\CameraAsset_SandboxCharacter.uasset` | quoted, backslashes |

Rule: sample assets are never edited in place.
LIST

# run <repo> -> rc, out  (env: STEALTH_NEVER_RESAVE_LIST=$list unless overridden by the caller)
run_check() { out="$(cd "$1" && STEALTH_HOOK_REPO="$1" bash "$check" 2>&1)"; rc=$?; }

repo="$(new_repo base)"
mkdir -p "$repo/Content/Blueprints/Cameras" "$repo/Content/Other" "$repo/Content/ProseOnly" "$repo/Content/Second"
for f in "$A" "$B" Content/Other/Free.uasset Content/ProseOnly/Mention.uasset Content/Second/Column.uasset; do printf 'v1\n' > "$repo/$f"; done
git -C "$repo" add -A >/dev/null 2>&1
git -C "$repo" commit -q -m "chore: base" >/dev/null 2>&1
export STEALTH_NEVER_RESAVE_LIST="$list"

run_check "$repo"
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -Fq "PASS (2 listed path(s), 0 changed"; then report no-staged-files-accepted 0 ""; else report no-staged-files-accepted 1 "rc=$rc out=$out"; fi

printf 'v2\n' > "$repo/$A"; git -C "$repo" add "$A"
run_check "$repo"
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "$A:0 never-resave staged modification of a never-resave asset: $A"; then report listed-modification-rejected-names-path 0 ""; else report listed-modification-rejected-names-path 1 "rc=$rc out=$out"; fi
printf '%s\n' "$out" | sed 's/^/    /'
git -C "$repo" reset -q HEAD "$A"; git -C "$repo" checkout -q -- "$A"

printf 'v2\n' > "$repo/Content/Other/Free.uasset"; git -C "$repo" add Content/Other/Free.uasset
run_check "$repo"
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -Fq "PASS (2 listed path(s), 1 changed"; then report unlisted-change-accepted 0 ""; else report unlisted-change-accepted 1 "rc=$rc out=$out"; fi
git -C "$repo" reset -q HEAD Content/Other/Free.uasset; git -C "$repo" checkout -q -- Content/Other/Free.uasset

# Backtick-quoted backslash row in the list is a listed path too.
printf 'v2\n' > "$repo/$B"; git -C "$repo" add "$B"
run_check "$repo"
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "$B:0 never-resave"; then report quoted-backslash-row-is-a-path 0 ""; else report quoted-backslash-row-is-a-path 1 "rc=$rc out=$out"; fi
git -C "$repo" reset -q HEAD "$B"; git -C "$repo" checkout -q -- "$B"

# Paths named only in prose or in the second column are not listed paths.
for p in Content/ProseOnly/Mention.uasset Content/Second/Column.uasset; do
  printf 'v2\n' > "$repo/$p"; git -C "$repo" add "$p"
  run_check "$repo"
  if [ "$rc" -eq 0 ]; then report "not-parsed-as-path-$(basename "$p" .uasset)" 0 ""; else report "not-parsed-as-path-$(basename "$p" .uasset)" 1 "rc=$rc out=$out"; fi
  git -C "$repo" reset -q HEAD "$p"; git -C "$repo" checkout -q -- "$p"
done

# Header and separator rows are never paths: a staged file literally named like the header cell is accepted.
printf 'x\n' > "$repo/Asset path"; git -C "$repo" add "Asset path"
run_check "$repo"
if [ "$rc" -eq 0 ]; then report header-row-not-a-path 0 ""; else report header-row-not-a-path 1 "rc=$rc out=$out"; fi
git -C "$repo" reset -q HEAD "Asset path"; rm -f "$repo/Asset path"

# Case and slash differences (Windows): a canned staged list in another case with backslashes.
staged="$tmp/staged.txt"
printf 'M\tcontent\\blueprints\\cameras\\CAMERARIG_BASEDEFAULTS.UASSET\n' > "$staged"
out="$(cd "$repo" && STEALTH_HOOK_REPO="$repo" STEALTH_HOOK_STAGED="$staged" bash "$check" 2>&1)"; rc=$?
if [ "$rc" -eq 1 ]; then report case-and-slash-differences-matched 0 ""; else report case-and-slash-differences-matched 1 "rc=$rc out=$out"; fi

# A rename of a listed asset counts (old path listed), and so does a rename ONTO a listed path.
git -C "$repo" mv "$A" Content/Other/Moved.uasset
run_check "$repo"
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "$A:0 never-resave staged rename of"; then report rename-of-listed-asset-rejected 0 ""; else report rename-of-listed-asset-rejected 1 "rc=$rc out=$out"; fi
git -C "$repo" reset -q --hard HEAD
printf 'R100	Content/Other/Free.uasset	%s
' "$A" > "$staged"
out="$(cd "$repo" && STEALTH_HOOK_REPO="$repo" STEALTH_HOOK_STAGED="$staged" bash "$check" 2>&1)"; rc=$?
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "$A:0 never-resave staged rename of"; then report rename-onto-listed-path-rejected 0 ""; else report rename-onto-listed-path-rejected 1 "rc=$rc out=$out"; fi

# A deletion of a listed asset is rejected (decision recorded in story tdc-026).
git -C "$repo" rm -q "$A"
run_check "$repo"
if [ "$rc" -eq 1 ] && printf '%s' "$out" | grep -Fq "$A:0 never-resave staged deletion of"; then report deletion-of-listed-asset-rejected 0 ""; else report deletion-of-listed-asset-rejected 1 "rc=$rc out=$out"; fi
git -C "$repo" reset -q --hard HEAD

# A missing list, and a list with no asset paths, are NOT ASSESSED: never a silent pass.
out="$(cd "$repo" && STEALTH_HOOK_REPO="$repo" STEALTH_NEVER_RESAVE_LIST="$tmp/does-not-exist.md" bash "$check" 2>&1)"; rc=$?
if [ "$rc" -eq 2 ] && printf '%s' "$out" | grep -Fq "NOT ASSESSED"; then report missing-list-not-assessed 0 ""; else report missing-list-not-assessed 1 "rc=$rc out=$out"; fi
printf '# Empty\n\n| Asset path | Reason |\n|---|---|\n' > "$tmp/empty.md"
out="$(cd "$repo" && STEALTH_HOOK_REPO="$repo" STEALTH_NEVER_RESAVE_LIST="$tmp/empty.md" bash "$check" 2>&1)"; rc=$?
if [ "$rc" -eq 2 ] && printf '%s' "$out" | grep -Fq "NOT ASSESSED"; then report empty-list-not-assessed 0 ""; else report empty-list-not-assessed 1 "rc=$rc out=$out"; fi

# The REAL list parses to its five camera assets (read only).
out="$(cd "$repo" && STEALTH_HOOK_REPO="$repo" STEALTH_NEVER_RESAVE_LIST="$proj/production/never-resave-assets.md" bash "$check" 2>&1)"; rc=$?
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -Fq "PASS (5 listed path(s)"; then report real-list-parses-five-paths 0 ""; else report real-list-parses-five-paths 1 "rc=$rc out=$out"; fi
unset STEALTH_NEVER_RESAVE_LIST

# Through the installed hook (story 025 contract): a listed change aborts the commit, an unlisted one is accepted.
hrepo="$(new_repo hook with-lint)"
mkdir -p "$hrepo/Content/Blueprints/Cameras" "$hrepo/Content/Other"
printf 'v1\n' > "$hrepo/Content/Blueprints/Cameras/CameraRig_BaseDefaults.uasset"
printf 'v1\n' > "$hrepo/Content/Other/Free.uasset"
git -C "$hrepo" add -A >/dev/null 2>&1
git -C "$hrepo" commit -q -m "chore: assets" -m "Story: SELFTEST-1" >/dev/null 2>&1
(cd "$hrepo" && bash tools/ci/hooks/install-hooks.sh >/dev/null 2>&1)
# The lock check also runs in the hook: give it a deterministic "locking unsupported" source (no remote here).
export STEALTH_LFS_LOCKS_CMD='echo "Remote does not support the Git LFS locking API"; exit 2'
wr "$hrepo/Content/Blueprints/Cameras/CameraRig_BaseDefaults.uasset" v2
git -C "$hrepo" add -A >/dev/null 2>&1
head_before="$(git -C "$hrepo" rev-parse HEAD)"
try_commit "$hrepo" "fix: resave camera"
if [ "$commit_rc" -ne 0 ] && [ "$head_before" = "$(git -C "$hrepo" rev-parse HEAD)" ] && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT check-never-resave: REJECTED"; then
  report hook-rejects-listed-change 0 ""; else report hook-rejects-listed-change 1 "rc=$commit_rc out=$commit_out"; fi
printf '%s\n' "$commit_out" | sed 's/^/    /'
git -C "$hrepo" reset -q HEAD . ; git -C "$hrepo" checkout -q -- .
wr "$hrepo/Content/Other/Free.uasset" v2
git -C "$hrepo" add -A >/dev/null 2>&1
try_commit "$hrepo" "fix: unlisted asset"
if [ "$commit_rc" -eq 0 ] && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT check-never-resave: PASS"; then report hook-accepts-unlisted-change 0 ""; else report hook-accepts-unlisted-change 1 "rc=$commit_rc out=$commit_out"; fi

selftest_result
