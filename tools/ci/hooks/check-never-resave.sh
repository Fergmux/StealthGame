#!/usr/bin/env bash
# Pre-commit check: reject a commit that modifies a never-resave asset (ADR-0009 section 4 and Validation 4;
# story tdc-026). Run by pre-commit.sh through the check contract in its header (exit 0 accept, 1 reject,
# 2 NOT ASSESSED); it may also be run on its own and then reads the index.
#
# The list is production/never-resave-assets.md (read only): a Markdown table whose FIRST column is the asset
# path. A table row counts only when its first cell looks like an asset path (contains a "/" or "\" and ends in a
# file extension), so the header row (`Asset path`), the separator row (`---`) and the prose around the
# table are never parsed as paths.
#
# Match: each changed path in the staged list against each listed path, ignoring case and slash direction
# (Windows). Decisions recorded in story tdc-026:
#   * a rename counts as modifying the listed asset when EITHER the old or the new path is listed;
#   * a DELETION of a listed asset is rejected too (removing a camera asset breaks the CameraDirector chain
#     as surely as resaving it); an ADD of a listed path is rejected (it replaces the restored asset).
# A missing list, or a list that yields no path, is NOT ASSESSED (exit 2): an empty list must not look like
# a pass.
#
# Environment (all optional; the hook sets the first two):
#   STEALTH_HOOK_REPO            repository top level (default: git rev-parse --show-toplevel)
#   STEALTH_HOOK_STAGED          file with `git diff --cached --name-status -M` output (default: read the index)
#   STEALTH_NEVER_RESAVE_LIST    the list file (default: <repo>/production/never-resave-assets.md); tests use a temporary one
# Observations: `<path>:0 never-resave <status text>: <path> is listed in <list>` per offending path.
set -u

repo="${STEALTH_HOOK_REPO:-$(git rev-parse --show-toplevel 2>/dev/null)}"
list="${STEALTH_NEVER_RESAVE_LIST:-$repo/production/never-resave-assets.md}"
staged="${STEALTH_HOOK_STAGED:-}"
tmp=""
cleanup() { [ -n "$tmp" ] && rm -f "$tmp" "$tmp.list"; }
trap cleanup EXIT

if [ ! -f "$list" ]; then
  echo "NOT ASSESSED: never-resave list not found: $list"
  exit 2
fi

tmp="$(mktemp)"
# Listed paths, normalised: backticks and CR removed, backslashes to slashes, leading ./ removed, lowercase.
awk -F'|' '
  /^[[:space:]]*\|/ {
    cell = $2
    gsub(/\r/, "", cell); gsub(/`/, "", cell)
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", cell)
    if (cell ~ /[\\\/]/ && cell ~ /\.[A-Za-z0-9]+$/) print cell
  }' "$list" | tr '\\' '/' | sed 's#^\./##' | tr 'A-Z' 'a-z' > "$tmp.list"
listed_count="$(grep -c . "$tmp.list" || true)"
if [ "${listed_count:-0}" -eq 0 ]; then
  echo "NOT ASSESSED: $list yields no asset paths (an empty list is not a pass)"
  exit 2
fi

if [ -z "$staged" ]; then
  staged="$tmp"
  git -c core.quotePath=false diff --cached --name-status -M > "$staged"
fi

findings=0
checked=0
while IFS=$'\t' read -r status p1 p2 || [ -n "${status:-}" ]; do
  status="${status%$'\r'}"
  [ -z "$status" ] && continue
  case "$status" in
    R*) candidates=("$p1" "$p2"); verb="rename of" ;;
    D) candidates=("$p1"); verb="deletion of" ;;
    A) candidates=("$p1"); verb="addition of" ;;
    *) candidates=("$p1"); verb="modification of" ;;
  esac
  for cand in "${candidates[@]}"; do
    [ -z "$cand" ] && continue
    checked=$((checked + 1))
    norm="$(printf '%s' "$cand" | tr '\\' '/' | sed 's#^\./##' | tr 'A-Z' 'a-z')"
    if grep -Fxq -- "$norm" "$tmp.list"; then
      echo "$cand:0 never-resave staged $verb a never-resave asset: $cand is listed in production/never-resave-assets.md"
      findings=$((findings + 1))
    fi
  done
done < "$staged"

if [ "$findings" -gt 0 ]; then
  echo "never-resave: REJECT ($findings listed path(s) in the commit; never resave or commit these, restore from _snapshots/)"
  exit 1
fi
echo "never-resave: PASS ($listed_count listed path(s), $checked changed path(s) checked)"
exit 0
