#!/usr/bin/env bash
# Pre-commit check: compare changed .uasset/.umap files with the Git LFS locks (ADR-0009 section 4 and
# Validation 2; story tdc-027). Run by pre-commit.sh through the check contract in its header (exit 0 accept,
# 1 reject, 2 NOT ASSESSED); it may also be run on its own and then reads the index.
#
# Lock source (injectable, so tests need no remote). The check runs a command and parses its output in the
# format of `git lfs locks --verify`, observed against the real GitHub remote on 2026-10-25:
#     O <path><TAB><owner><TAB>ID:<n>        a lock held by the person running git (ours)
#       <path><TAB><owner><TAB>ID:<n>        a lock held by someone else (two leading spaces)
#   (the `O` marker is how git-lfs reports "yours" without this script matching owner names to git identities)
#   STEALTH_LFS_LOCKS_CMD   the command (run with bash -c); default: git lfs locks --verify
#
# Three source states (story tdc-027):
#   (a) the changed asset is locked by the committer ("O " line)      -> accepted
#   (b) no lock, or a lock held by someone else                       -> rejected, naming the path
#   (c) the remote reports locking unsupported (the command fails and says "does not support" / "not
#       supported" / "locksverify"), or there is no remote at all (git-lfs says `missing protocol`, observed in
#       a repository with no origin)                                  -> accepted with a printed single-writer
#                                                                        reminder (ADR-0009 section 4 interim rule)
#   Any other source failure (network, authentication) or an unparseable line is NOT ASSESSED (exit 2): not a pass.
#
# Decisions recorded in story tdc-027:
#   * Only paths ending .uasset or .umap are checked (override: STEALTH_LOCK_EXTENSIONS, space separated);
#     a commit with no such path never runs the lock source.
#   * A modification, deletion or rename (the OLD path) needs a lock held by the committer. An ADDED asset
#     cannot be locked by anyone else before it exists on the remote, so an add is rejected only when someone
#     else holds a lock on that path. The NEW path of a rename is treated as an add.
#   * Paths compare ignoring case and slash direction (Windows). Never-resave assets are NOT special-cased
#     here; story 026 owns them.
#
# Environment: STEALTH_HOOK_REPO, STEALTH_HOOK_STAGED (set by the hook), STEALTH_LFS_LOCKS_CMD, STEALTH_LOCK_EXTENSIONS.
# Observations: `<path>:0 lfs-lock staged <verb> <path> without a lock held by you ...` per offending path.
set -u

repo="${STEALTH_HOOK_REPO:-$(git rev-parse --show-toplevel 2>/dev/null)}"
staged="${STEALTH_HOOK_STAGED:-}"
extensions="${STEALTH_LOCK_EXTENSIONS:-uasset umap}"
locks_cmd="${STEALTH_LFS_LOCKS_CMD:-git lfs locks --verify}"
tmp="$(mktemp)"
trap 'rm -f "$tmp" "$tmp.need" "$tmp.ours" "$tmp.theirs"' EXIT

if [ -z "$staged" ]; then
  staged="$tmp"
  ( cd "$repo" && git -c core.quotePath=false diff --cached --name-status -M ) > "$staged"
fi

norm() { printf '%s' "$1" | tr '\\' '/' | sed 's#^\./##' | tr 'A-Z' 'a-z'; }
is_lockable() {
  local lower ext
  lower="$(norm "$1")"
  for ext in $extensions; do
    case "$lower" in *."$(printf '%s' "$ext" | tr 'A-Z' 'a-z')") return 0 ;; esac
  done
  return 1
}

# need: "<kind> <path>" per changed lockable path; kind = modify (needs our lock) or add (needs no foreign lock).
: > "$tmp.need"
while IFS=$'\t' read -r status p1 p2 || [ -n "${status:-}" ]; do
  status="${status%$'\r'}"
  [ -z "$status" ] && continue
  case "$status" in
    R*) is_lockable "$p1" && printf 'modify\trename of\t%s\n' "$p1" >> "$tmp.need"
        is_lockable "$p2" && printf 'add\tadd of\t%s\n' "$p2" >> "$tmp.need" ;;
    A)  is_lockable "$p1" && printf 'add\taddition of\t%s\n' "$p1" >> "$tmp.need" ;;
    D)  is_lockable "$p1" && printf 'modify\tdeletion of\t%s\n' "$p1" >> "$tmp.need" ;;
    *)  is_lockable "$p1" && printf 'modify\tmodification of\t%s\n' "$p1" >> "$tmp.need" ;;
  esac
done < "$staged"

if [ ! -s "$tmp.need" ]; then
  echo "lfs-locks: PASS (no changed $extensions path; the lock source was not queried)"
  exit 0
fi

out="$(cd "$repo" && bash -c "$locks_cmd" 2>&1)"; rc=$?
if [ "$rc" -ne 0 ]; then
  if printf '%s' "$out" | grep -Eiq 'does not support|not supported|locksverify|missing protocol'; then
    echo "NOTE single-writer: the remote reports no lock support, so locks cannot be checked. Only ONE session"
    echo "     saves Content/ assets at a time (ADR-0009 section 4): confirm no other session is editing these assets."
    echo "lfs-locks: PASS (degraded: lock support unavailable; single-writer reminder printed)"
    exit 0
  fi
  echo "NOT ASSESSED: the lock source failed (exit $rc): $(printf '%s' "$out" | head -3 | tr '\n' ' ')"
  exit 2
fi

: > "$tmp.ours"; : > "$tmp.theirs"
while IFS= read -r line || [ -n "$line" ]; do
  line="${line%$'\r'}"
  [ -z "$line" ] && continue
  case "$line" in
    "O "*) p="${line#O }"; p="${p%%$'\t'*}"; p="${p%"${p##*[! ]}"}"; norm "$p" >> "$tmp.ours"; echo >> "$tmp.ours" ;;
    "  "*) p="${line#  }"; p="${p%%$'\t'*}"; p="${p%"${p##*[! ]}"}"; norm "$p" >> "$tmp.theirs"; echo >> "$tmp.theirs" ;;
    *) echo "NOT ASSESSED: unrecognised lock source line (expected 'O <path>' or '  <path>'): $line"; exit 2 ;;
  esac
done <<< "$out"

findings=0
while IFS=$'\t' read -r kind verb path || [ -n "${kind:-}" ]; do
  [ -z "$kind" ] && continue
  n="$(norm "$path")"
  if [ "$kind" = "modify" ]; then
    if ! grep -Fxq -- "$n" "$tmp.ours"; then
      if grep -Fxq -- "$n" "$tmp.theirs"; then why="the lock is held by someone else"; else why="it is not locked"; fi
      echo "$path:0 lfs-lock staged $verb $path without a lock held by you ($why): git lfs lock $path first"
      findings=$((findings + 1))
    fi
  else
    if grep -Fxq -- "$n" "$tmp.theirs"; then
      echo "$path:0 lfs-lock staged $verb $path, which is locked by someone else"
      findings=$((findings + 1))
    fi
  fi
done < "$tmp.need"

if [ "$findings" -gt 0 ]; then
  echo "lfs-locks: REJECT ($findings changed asset(s) not covered by a lock you hold)"
  exit 1
fi
echo "lfs-locks: PASS ($(grep -c . "$tmp.need") changed asset path(s), each covered by a lock you hold or needing none)"
exit 0
