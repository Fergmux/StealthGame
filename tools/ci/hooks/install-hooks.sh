#!/usr/bin/env bash
# Opt-in installer for the tracked git hooks (ADR-0009 section 5; story tdc-025).
#
# Points the clone's core.hooksPath at tools/ci/hooks (a relative path, so it follows the working copy).
# core.hooksPath is a local, unversioned git setting: run this once per clone. It never edits a tracked
# file (.gitignore, .gitattributes and the hooks themselves are left alone) and never touches .git/hooks.
#
# Git LFS: git runs ONLY the hooks in core.hooksPath once it is set, so the LFS hooks (pre-push uploads LFS
# objects; post-checkout/post-commit/post-merge keep lockable files read-only) must live there too. The installer
# runs `git lfs install --local`, which writes them into tools/ci/hooks. They are generated per clone, untracked
# (tools/ci/hooks/.gitignore lists them) and must NOT be committed: an extensionless CRLF checkout would corrupt
# them. When the repository uses LFS and git-lfs is missing, the install fails rather than silently stop uploads.
#
# Usage: install-hooks.sh [--status | --uninstall] [--force]
#   (no option)  set core.hooksPath. Idempotent: a second run changes nothing and says "already installed".
#   --status     print whether the hooks are installed; exit 0 when installed, 1 when not
#   --uninstall  unset core.hooksPath when it points at tools/ci/hooks
#   --force      replace a core.hooksPath that points somewhere else (without it that is an error)
# Exit: 0 ok | 1 refused or failed.
set -u

here="$(cd "$(dirname "$0")" && pwd)"
mode=install
force=0
for arg in "$@"; do
  case "$arg" in
    --status) mode=status ;;
    --uninstall) mode=uninstall ;;
    --force) force=1 ;;
    *) echo "install-hooks: unknown option $arg" >&2; exit 1 ;;
  esac
done

top="$(git -C "$here" rev-parse --show-toplevel 2>/dev/null)" || { echo "install-hooks: $here is not inside a git work tree" >&2; exit 1; }
prefix="$(git -C "$here" rev-parse --show-prefix)"   # this folder relative to the top level, e.g. tools/ci/hooks/
want="${prefix%/}"
current="$(git -C "$top" config --local --get core.hooksPath || true)"

case "$mode" in
  status)
    if [ "$current" = "$want" ]; then echo "STATUS: installed (core.hooksPath=$current)"; exit 0; fi
    echo "STATUS: not installed (core.hooksPath=${current:-<unset>})"; exit 1 ;;
  uninstall)
    if [ "$current" = "$want" ]; then git -C "$top" config --local --unset core.hooksPath; echo "STATUS: uninstalled"; exit 0; fi
    echo "STATUS: nothing to uninstall (core.hooksPath=${current:-<unset>})"; exit 0 ;;
esac

for hook in pre-commit commit-msg pre-commit.sh commit-msg.sh; do
  if [ ! -f "$here/$hook" ]; then echo "install-hooks: $here/$hook is missing" >&2; exit 1; fi
  # git silently ignores a hook that is not executable (Linux/macOS); make sure it is. Best effort on Windows.
  [ -x "$here/$hook" ] || chmod +x "$here/$hook" 2>/dev/null || true
done

if [ -n "$current" ] && [ "$current" != "$want" ] && [ "$force" -ne 1 ]; then
  echo "install-hooks: core.hooksPath is already set to '$current'; pass --force to replace it" >&2
  exit 1
fi
if [ "$current" != "$want" ]; then
  git -C "$top" config --local core.hooksPath "$want" || { echo "install-hooks: git config failed" >&2; exit 1; }
  result="installed"
else
  result="already installed"
fi

# Keep the Git LFS hooks working (see the header). Idempotent: git-lfs rewrites identical hooks.
if grep -qs 'filter=lfs' "$top/.gitattributes"; then
  if ! git -C "$top" lfs install --local >/dev/null 2>&1 || [ ! -f "$here/pre-push" ]; then
    echo "install-hooks: this repository uses Git LFS but 'git lfs install --local' did not put the LFS hooks in $want;" >&2
    echo "  without them a push would NOT upload LFS objects. Install git-lfs and run this installer again." >&2
    exit 1
  fi
fi
echo "STATUS: $result (core.hooksPath=$want)"
exit 0
