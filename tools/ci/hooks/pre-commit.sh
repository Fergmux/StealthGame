#!/usr/bin/env bash
# Pre-commit hook body (ADR-0009 section 5 and Validation 5, ADR-0008 section 8; story tdc-025).
# Installed by install-hooks.sh (core.hooksPath = tools/ci/hooks); the extensionless `pre-commit` file is a
# shim that runs this script. Opt-in: nothing runs until a person runs the installer in their clone.
#
# What it does, in order (every commit):
#   1. T0 lint: tools/ci/lint/gate.sh over the same rule scripts the per-commit gate runs (the include
#      lint rules a-j, include-lint-*.sh, and vcs-conformance.sh), root = the repository top level.
#      The gate decides; this hook only relays its output and exit code. It lints the WORKING TREE, so a
#      partly staged change is judged on what is on disk, not on what is in the index (open point,
#      recorded in story tdc-025); the full gate in CI judges the commit as pushed.
#   2. Checks: every tools/ci/hooks/check-*.sh, in name order (never-resave: story 026, LFS locks: story 027).
#
# CHECK CONTRACT (how stories 026-028 and later checks plug in WITHOUT editing this file):
#   * Drop a script named check-<name>.sh next to this file. It is run as `bash check-<name>.sh` with the
#     repository top level as the current directory and these environment variables set:
#       STEALTH_HOOK_REPO    absolute path of the repository top level
#       STEALTH_HOOK_STAGED  path of a text file listing the staged changes, one per line, as
#                            `git diff --cached --name-status -M` prints them with core.quotePath=false:
#                            `<status><TAB><path>` or, for a rename, `R<score><TAB><old><TAB><new>`
#     A check run outside this hook (a selftest) may leave STEALTH_HOOK_STAGED unset and read the index.
#   * It reports on stdout/stderr. A finding is an observation line `<path>:<line> <rule> <message>`
#     (the emit.sh contract); other lines are free text (NOTE ..., NOT ASSESSED: ...).
#   * Exit code: 0 accept, 1 reject (findings), 2 NOT ASSESSED (the check could not decide: never a
#     pass). Any nonzero aborts the commit. Every check runs even when an earlier one failed, so one
#     attempt shows all the problems.
#   * The commit message rules are NOT a check: they are the separate commit-msg hook (story 028).
#
# Exit: 0 PASS | 1 REJECTED (lint findings or a check rejected) | 2 NOT ASSESSED (something could not decide).
# To bypass once, knowingly: git commit --no-verify. The hook never edits a tracked file.
set -u

hookdir="$(cd "$(dirname "$0")" && pwd)"
lintdir="$hookdir/../lint"
top="$(git rev-parse --show-toplevel 2>/dev/null)" || { echo "PRE-COMMIT: NOT ASSESSED - not inside a git work tree" >&2; exit 2; }
cd "$top" || exit 2

staged="$(mktemp)"
trap 'rm -f "$staged"' EXIT
git -c core.quotePath=false diff --cached --name-status -M > "$staged"

rejected=0
not_assessed=0

# ---- 1. T0 lint ---------------------------------------------------------------------------------
rules=()
for rule in "$lintdir"/include-lint-*.sh "$lintdir/vcs-conformance.sh"; do
  [ -f "$rule" ] && rules+=("$rule")
done
if [ ${#rules[@]} -eq 0 ] || [ ! -f "$lintdir/gate.sh" ]; then
  echo "PRE-COMMIT lint: NOT ASSESSED - no lint gate or rule scripts found under $lintdir" >&2
  not_assessed=1
else
  lint_out="$(bash "$lintdir/gate.sh" --root "$top" "${rules[@]}" 2>&1)"; lint_rc=$?
  printf '%s\n' "$lint_out" >&2
  case "$lint_rc" in
    0) echo "PRE-COMMIT lint: PASS" >&2 ;;
    1) echo "PRE-COMMIT lint: REJECTED - the T0 lint reported findings (above)" >&2; rejected=1 ;;
    *) echo "PRE-COMMIT lint: NOT ASSESSED - the lint gate exited $lint_rc" >&2; not_assessed=1 ;;
  esac
fi

# ---- 2. checks ----------------------------------------------------------------------------------
for chk in "$hookdir"/check-*.sh; do
  [ -f "$chk" ] || continue
  name="$(basename "$chk" .sh)"
  out="$(STEALTH_HOOK_REPO="$top" STEALTH_HOOK_STAGED="$staged" bash "$chk" 2>&1)"; rc=$?
  [ -n "$out" ] && printf '%s\n' "$out" >&2
  case "$rc" in
    0) echo "PRE-COMMIT $name: PASS" >&2 ;;
    1) echo "PRE-COMMIT $name: REJECTED" >&2; rejected=1 ;;
    *) echo "PRE-COMMIT $name: NOT ASSESSED (exit $rc)" >&2; not_assessed=1 ;;
  esac
done

if [ "$rejected" -ne 0 ]; then echo "PRE-COMMIT: REJECTED - the commit was not made" >&2; exit 1; fi
if [ "$not_assessed" -ne 0 ]; then echo "PRE-COMMIT: NOT ASSESSED - the commit was not made (not assessed is never a pass)" >&2; exit 2; fi
echo "PRE-COMMIT: PASS" >&2
exit 0
