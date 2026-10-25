#!/usr/bin/env bash
# Shared helpers for the git-hook selftests (stories tdc-025 to tdc-028; ADR-0008 G2). A selftest sources this
# file, which: clears the GIT_* variables a calling git hook could leak in, sets
#   proj    the project root (the real working copy, only READ by these tests)
#   hooks   <proj>/tools/ci/hooks
#   tmp     a throwaway folder removed on exit
# and provides report / selftest_result (from the lint selftest lib) plus:
#   new_repo <name> [with-lint]   a fresh temporary git repository (own identity, no signing, no autocrlf)
#                                 holding a copy of the hook scripts; with-lint also copies the lint scripts, the
#                                 compliant ignore/attribute pair, the never-resave list and one clean header, then
#                                 makes the first commit. Prints the repo path. HOOKS ARE NEVER INSTALLED IN THE REAL
#                                 WORKING COPY: every commit these tests make happens in a temporary repository.
#   try_commit <repo> <subject> [<body>]   git commit in <repo>; sets commit_rc and commit_out (stdout+stderr)
set -u
for v in $(env | grep -o '^GIT_[A-Z_]*' ); do unset "$v"; done
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
proj="$(cd "$here/../../../.." && pwd)"
hooks="$proj/tools/ci/hooks"
lint="$proj/tools/ci/lint"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$lint/selftest/lib.sh"

new_repo() {
  local name="$1" withlint="${2:-}" dir="$tmp/$1"
  mkdir -p "$dir/tools/ci/hooks"
  git -C "$dir" init -q
  git -C "$dir" config user.name "Selftest"
  git -C "$dir" config user.email "selftest@example.invalid"
  git -C "$dir" config commit.gpgsign false
  git -C "$dir" config core.autocrlf false
  cp "$hooks"/pre-commit "$hooks"/pre-commit.sh "$hooks"/commit-msg "$hooks"/commit-msg.sh "$hooks"/install-hooks.sh "$hooks"/install-hooks.ps1 "$hooks"/.gitignore "$hooks"/check-*.sh "$dir/tools/ci/hooks/"
  if [ "$withlint" = "with-lint" ]; then
    mkdir -p "$dir/tools/ci/lint" "$dir/production" "$dir/Source/StealthGameplay/Public/View"
    cp "$lint"/*.sh "$lint"/*.awk "$lint"/*.txt "$dir/tools/ci/lint/"
    cp "$lint/selftest/vcs-conformance/compliant/gitignore.txt" "$dir/.gitignore"
    cp "$lint/selftest/vcs-conformance/compliant/gitattributes.txt" "$dir/.gitattributes"
    cp "$proj/production/never-resave-assets.md" "$dir/production/never-resave-assets.md"
    cp "$lint/selftest/include-abc/fixtures/c-clean/Source/StealthGameplay/Public/View/StealthSafeView.h" "$dir/Source/StealthGameplay/Public/View/"
    git -C "$dir" add -A >/dev/null 2>&1
    git -C "$dir" commit -q -m "chore: selftest base" -m "Story: SELFTEST-0" >/dev/null 2>&1
  fi
  printf '%s\n' "$dir"
}

try_commit() {
  local repo="$1" subject="$2" body="${3:-Story: SELFTEST-1}"
  commit_out="$(git -C "$repo" commit -q -m "$subject" -m "$body" 2>&1)"; commit_rc=$?
}

# expect <name> <condition-exit-status> <detail>   (report wrapper: 0 = ok)
expect() { report "$1" "$2" "$3"; }

# wr <file> <text>   write a file even when `lockable` made it read-only (an LFS checkout of a lockable path)
wr() { rm -f "$1"; printf '%s
' "$2" > "$1"; }
