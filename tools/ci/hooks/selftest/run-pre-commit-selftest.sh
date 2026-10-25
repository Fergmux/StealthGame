#!/usr/bin/env bash
# Selftest for the tracked pre-commit hook framework and its installer (story tdc-025; ADR-0009 Validation 5;
# ADR-0008 G2). Everything happens in temporary git repositories holding a COPY of the hook scripts: the hooks
# are never installed in the real working copy, and no commit is made there.
#
# Usage: run-pre-commit-selftest.sh     Exit 0 when every case holds, else 1.
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib.sh"

seeded="$lint/fixtures/seeded/Source/StealthFoundation/Public/Server/StealthNoGuard.h"   # include-lint rule b violation

# ---- installer --------------------------------------------------------------------------------------
repo="$(new_repo installer with-lint)"
before_ignore="$(git -C "$repo" hash-object "$repo/.gitignore")"
before_attr="$(git -C "$repo" hash-object "$repo/.gitattributes")"
out="$(cd "$repo" && bash tools/ci/hooks/install-hooks.sh 2>&1)"; rc=$?
val="$(git -C "$repo" config --local --get core.hooksPath)"
if [ "$rc" -eq 0 ] && [ "$val" = "tools/ci/hooks" ] && printf '%s' "$out" | grep -Fq "STATUS: installed"; then
  report installer-sets-hookspath 0 ""; else report installer-sets-hookspath 1 "rc=$rc value='$val' out=$out"; fi

out2="$(cd "$repo" && bash tools/ci/hooks/install-hooks.sh 2>&1)"; rc2=$?
count="$(git -C "$repo" config --local --get-all core.hooksPath | wc -l | tr -d ' ')"
val2="$(git -C "$repo" config --local --get core.hooksPath)"
if [ "$rc2" -eq 0 ] && [ "$val2" = "tools/ci/hooks" ] && [ "$count" -eq 1 ] && printf '%s' "$out2" | grep -Fq "already installed"; then
  report installer-idempotent 0 ""; else report installer-idempotent 1 "rc=$rc2 value='$val2' count=$count out=$out2"; fi

after_ignore="$(git -C "$repo" hash-object "$repo/.gitignore")"
after_attr="$(git -C "$repo" hash-object "$repo/.gitattributes")"
dirty="$(git -C "$repo" status --porcelain --untracked-files=no)"
if [ "$before_ignore" = "$after_ignore" ] && [ "$before_attr" = "$after_attr" ] && [ -z "$dirty" ]; then
  report installer-leaves-tracked-files-alone 0 ""; else report installer-leaves-tracked-files-alone 1 "status: $dirty"; fi

lfs_hooks=""; for h in pre-push post-checkout post-commit post-merge; do [ -f "$repo/tools/ci/hooks/$h" ] || lfs_hooks="$lfs_hooks $h"; done
untracked="$(git -C "$repo" status --porcelain)"
if [ -z "$lfs_hooks" ] && [ -z "$untracked" ]; then report installer-writes-lfs-hooks-untracked-and-ignored 0 ""; else report installer-writes-lfs-hooks-untracked-and-ignored 1 "missing:$lfs_hooks status: $untracked"; fi

(cd "$repo" && bash tools/ci/hooks/install-hooks.sh --status >/dev/null 2>&1); rc=$?
[ "$rc" -eq 0 ] && report installer-status-installed 0 "" || report installer-status-installed 1 "rc=$rc"

git -C "$repo" config --local core.hooksPath somewhere/else
out="$(cd "$repo" && bash tools/ci/hooks/install-hooks.sh 2>&1)"; rc=$?
val="$(git -C "$repo" config --local --get core.hooksPath)"
if [ "$rc" -eq 1 ] && [ "$val" = "somewhere/else" ]; then report installer-refuses-foreign-hookspath 0 ""; else report installer-refuses-foreign-hookspath 1 "rc=$rc value='$val'"; fi
out="$(cd "$repo" && bash tools/ci/hooks/install-hooks.sh --force 2>&1)"; rc=$?
val="$(git -C "$repo" config --local --get core.hooksPath)"
if [ "$rc" -eq 0 ] && [ "$val" = "tools/ci/hooks" ]; then report installer-force-replaces 0 ""; else report installer-force-replaces 1 "rc=$rc value='$val'"; fi

(cd "$repo" && bash tools/ci/hooks/install-hooks.sh --uninstall >/dev/null 2>&1)
if ! git -C "$repo" config --local --get core.hooksPath >/dev/null 2>&1; then report installer-uninstall 0 ""; else report installer-uninstall 1 "still set"; fi

# ---- the PowerShell twin sets the same value ------------------------------------------------------------
ps="$(command -v powershell || command -v powershell.exe || true)"
if [ -n "$ps" ]; then
  psrepo="$(new_repo installer-ps with-lint)"
  psout="$("$ps" -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$psrepo/tools/ci/hooks/install-hooks.ps1" 2>/dev/null || echo "$psrepo/tools/ci/hooks/install-hooks.ps1")" 2>&1)"
  psval="$(git -C "$psrepo" config --local --get core.hooksPath)"
  psout2="$("$ps" -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$psrepo/tools/ci/hooks/install-hooks.ps1" 2>/dev/null || echo "$psrepo/tools/ci/hooks/install-hooks.ps1")" 2>&1)"
  pscount="$(git -C "$psrepo" config --local --get-all core.hooksPath | wc -l | tr -d ' ')"
  if [ "$psval" = "tools/ci/hooks" ] && [ "$pscount" -eq 1 ] && printf '%s' "$psout2" | grep -Fq "already installed"; then
    report installer-powershell-twin 0 ""; else report installer-powershell-twin 1 "value='$psval' count=$pscount out=$psout / $psout2"; fi
else
  report installer-powershell-twin 1 "powershell not found"
fi

# ---- the installed hook ---------------------------------------------------------------------------------
repo="$(new_repo hook with-lint)"
(cd "$repo" && bash tools/ci/hooks/install-hooks.sh >/dev/null 2>&1)

out="$(cd "$repo" && bash tools/ci/hooks/pre-commit 2>&1)"; rc=$?
if [ "$rc" -eq 0 ] && printf '%s' "$out" | grep -Fq "PRE-COMMIT: PASS"; then report no-staged-files-passes 0 ""; else report no-staged-files-passes 1 "rc=$rc out=$out"; fi

echo "// clean change" >> "$repo/Source/StealthGameplay/Public/View/StealthSafeView.h"
git -C "$repo" add -A >/dev/null 2>&1
try_commit "$repo" "feat: clean change"
if [ "$commit_rc" -eq 0 ] && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT lint: PASS" && [ "$(git -C "$repo" log --format=%s -1)" = "feat: clean change" ]; then
  report clean-commit-accepted 0 ""; else report clean-commit-accepted 1 "rc=$commit_rc out=$commit_out"; fi

mkdir -p "$repo/Source/StealthFoundation/Public/Server"
cp "$seeded" "$repo/Source/StealthFoundation/Public/Server/"
git -C "$repo" add -A >/dev/null 2>&1
head_before="$(git -C "$repo" rev-parse HEAD)"
try_commit "$repo" "feat: seeded violation"
head_after="$(git -C "$repo" rev-parse HEAD)"
if [ "$commit_rc" -ne 0 ] && [ "$head_before" = "$head_after" ] \
   && printf '%s' "$commit_out" | grep -Fq "Source/StealthFoundation/Public/Server/StealthNoGuard.h:2 b first include" \
   && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT: REJECTED"; then
  report seeded-lint-violation-rejected 0 ""
else
  report seeded-lint-violation-rejected 1 "rc=$commit_rc head_changed=$([ "$head_before" != "$head_after" ] && echo yes || echo no) out=$commit_out"
fi
printf '%s\n' "$commit_out" | sed 's/^/    /'

# Remove the seeded file again: the staged tree is clean once more.
git -C "$repo" rm -q -f --cached Source/StealthFoundation/Public/Server/StealthNoGuard.h; rm -rf "$repo/Source/StealthFoundation"

# ---- check contract: a new check needs no change to the hook body ---------------------------------------------
echo "// again" >> "$repo/Source/StealthGameplay/Public/View/StealthSafeView.h"
git -C "$repo" add -A >/dev/null 2>&1
printf '#!/usr/bin/env bash\necho "demo: rejecting on purpose"\nexit 1\n' > "$repo/tools/ci/hooks/check-demo.sh"
try_commit "$repo" "feat: demo reject"
if [ "$commit_rc" -ne 0 ] && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT check-demo: REJECTED"; then report check-contract-reject-aborts 0 ""; else report check-contract-reject-aborts 1 "rc=$commit_rc out=$commit_out"; fi

printf '#!/usr/bin/env bash\necho "demo: cannot decide"\nexit 2\n' > "$repo/tools/ci/hooks/check-demo.sh"
try_commit "$repo" "feat: demo not assessed"
if [ "$commit_rc" -ne 0 ] && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT check-demo: NOT ASSESSED"; then report check-contract-not-assessed-aborts 0 ""; else report check-contract-not-assessed-aborts 1 "rc=$commit_rc out=$commit_out"; fi

printf '#!/usr/bin/env bash\ntest -n "${STEALTH_HOOK_REPO:-}" && test -f "${STEALTH_HOOK_STAGED:-/nonexistent}" && echo "demo: contract env present"\nexit 0\n' > "$repo/tools/ci/hooks/check-demo.sh"
try_commit "$repo" "feat: demo pass"
if [ "$commit_rc" -eq 0 ] && printf '%s' "$commit_out" | grep -Fq "demo: contract env present" && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT check-demo: PASS"; then report check-contract-pass-and-env 0 ""; else report check-contract-pass-and-env 1 "rc=$commit_rc out=$commit_out"; fi

# ---- a CRLF checkout of the extensionless shims still works ------------------------------------------------------
crepo="$(new_repo crlf with-lint)"
(cd "$crepo" && bash tools/ci/hooks/install-hooks.sh >/dev/null 2>&1)
sed -i 's/$/\r/' "$crepo/tools/ci/hooks/pre-commit" "$crepo/tools/ci/hooks/commit-msg"
echo "// crlf" >> "$crepo/Source/StealthGameplay/Public/View/StealthSafeView.h"
git -C "$crepo" add -A >/dev/null 2>&1
try_commit "$crepo" "feat: crlf shims clean"
clean_ok=$commit_rc
mkdir -p "$crepo/Source/StealthFoundation/Public/Server"
cp "$seeded" "$crepo/Source/StealthFoundation/Public/Server/"
git -C "$crepo" add -A >/dev/null 2>&1
try_commit "$crepo" "feat: crlf shims seeded"
if [ "$clean_ok" -eq 0 ] && [ "$commit_rc" -ne 0 ] && printf '%s' "$commit_out" | grep -Fq "PRE-COMMIT: REJECTED"; then
  report crlf-shims-still-work 0 ""; else report crlf-shims-still-work 1 "clean rc=$clean_ok seeded rc=$commit_rc out=$commit_out"; fi

# ---- the hook's rule list is the per-commit gate's rule list -----------------------------------------------------
gate_rules="$(grep -o "'[a-z-]*\.sh'" "$proj/tools/ci/gate-per-commit.ps1" | head -5 | tr -d "'" | sort | tr '\n' ' ')"
hook_rules="$( (cd "$lint" && ls include-lint-*.sh; echo vcs-conformance.sh) | sort | tr '\n' ' ')"
if [ -n "$gate_rules" ] && [ "$gate_rules" = "$hook_rules" ]; then report hook-rules-match-gate-rules 0 ""; else report hook-rules-match-gate-rules 1 "gate: $gate_rules hook: $hook_rules"; fi

selftest_result
