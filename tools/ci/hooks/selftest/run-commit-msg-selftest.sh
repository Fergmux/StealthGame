#!/usr/bin/env bash
# Selftest for the commit-msg hook (story tdc-028; ADR-0009 section 5; ADR-0008 G2). Runs the hook body on a
# message matrix, then once through a real `git commit` in a temporary repository with the hooks installed.
# No commit is made in the real working copy; the real history is only READ (see the last case).
#
# Usage: run-commit-msg-selftest.sh     Exit 0 when every case holds, else 1.
set -u
. "$(cd "$(dirname "$0")" && pwd)/lib.sh"

hook="$hooks/commit-msg.sh"
TRAILER='Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>'

# msg_case <name> <want-exit> <message text> [<required stderr text>]
msg_case() {
  local name="$1" want="$2" text="$3" need="${4:-}" f="$tmp/msg.txt" out rc
  printf '%s' "$text" > "$f"
  out="$(bash "$hook" "$f" 2>&1)"; rc=$?
  if [ "$rc" -eq "$want" ] && { [ -z "$need" ] || printf '%s' "$out" | grep -Fq -- "$need"; }; then
    report "$name" 0 ""
  else
    report "$name" 1 "exit $rc (want $want), need '$need', out: $out"
  fi
}

# --- the six prefixes, each with a Story line: accepted -------------------------------------------------------
for p in feat fix chore docs test refactor; do
  msg_case "prefix-$p-with-story-accepted" 0 "$p: do a thing"$'\n\n'"Story: EPIC-001-S02"$'\n'
done
# --- prefix rules ---------------------------------------------------------------------------------------------
msg_case wrong-prefix-rejected 1 $'update: do a thing\n\nStory: EPIC-001-S02\n' "subject must start with feat|fix|chore|docs|test|refactor"
msg_case no-prefix-rejected 1 $'do a thing\n\nStory: EPIC-001-S02\n' "subject must start with"
msg_case prefix-needs-colon-space-rejected 1 $'feat do a thing\n\nStory: EPIC-001-S02\n' "subject must start with"
msg_case prefix-needs-description-rejected 1 $'feat: \n\nStory: EPIC-001-S02\n' "subject must start with"
msg_case uppercase-prefix-rejected 1 $'Feat: do a thing\n\nStory: EPIC-001-S02\n' "subject must start with"
msg_case scoped-prefix-accepted 0 $'feat(ci): do a thing\n\nStory: EPIC-001-S02\n'
msg_case breaking-prefix-accepted 0 $'refactor(core)!: do a thing\n\nStory: EPIC-001-S02\n'
# --- the three reference forms, any one is enough --------------------------------------------------------------
msg_case story-line-accepted 0 $'feat: x\n\nStory: production/epics/tuning-data-config/story-028-commit-message-check.md\n'
msg_case task-line-accepted 0 $'chore: x\n\nTask: create-stories hud\n'
msg_case decision-line-accepted 0 $'docs: x\n\nDecision: D51\n'
msg_case adr-token-accepted 0 $'docs: x\n\nRecorded in ADR-0009 section 5.\n'
msg_case design-path-accepted 0 $'docs: x\n\nSee design/gdd/tuning-data-config.md for the rule.\n'
msg_case architecture-path-accepted 0 $'docs: x\n\nSee docs/architecture/adr-0009-version-control-binary-assets.md.\n'
msg_case no-reference-rejected 1 $'feat: x\n\nJust some explanation with no reference at all.\n' "the body has no reference"
msg_case subject-only-rejected 1 $'feat: x\n' "the body has no reference"
msg_case adr-two-digits-not-a-reference 1 $'docs: x\n\nSee ADR-12 for details.\n' "the body has no reference"
msg_case adr-five-digits-not-a-reference 1 $'docs: x\n\nSee ADR-00123 for details.\n' "the body has no reference"
msg_case adr-in-subject-only-not-counted 1 $'docs: ADR-0009 wording\n' "the body has no reference"
msg_case story-without-id-not-a-reference 1 $'feat: x\n\nStory:\n' "the body has no reference"
msg_case unrelated-path-not-a-reference 1 $'feat: x\n\nTouches tools/ci/hooks/pre-commit.sh only.\n' "the body has no reference"
msg_case path-after-punctuation-accepted 0 $'feat: x\n\n(design/gdd/foo.md)\n'
msg_case path-inside-other-word-not-a-reference 1 $'feat: x\n\nsee mydesign/gdd/foo.md\n' "the body has no reference"
# --- attribution trailers: not rejected, not counted --------------------------------------------------------------
msg_case trailer-after-story-accepted 0 "feat: x"$'\n\n'"Story: EPIC-001-S02"$'\n\n'"$TRAILER"$'\n'
msg_case trailer-alone-not-a-reference 1 "feat: x"$'\n\n'"$TRAILER"$'\n' "the body has no reference"
msg_case trailer-naming-an-adr-not-counted 1 "feat: x"$'\n\n''Co-Authored-By: Someone ADR-0009 <a@b.invalid>'$'\n' "the body has no reference"
# --- edge cases the story names (decisions in the hook header) -----------------------------------------------------
msg_case empty-message-rejected 1 "" "empty commit message"
msg_case blank-message-rejected 1 $'\n\n   \n' "empty commit message"
msg_case comments-only-message-rejected 1 $'# Please enter the commit message\n# Lines starting with # are ignored\n' "empty commit message"
msg_case merge-subject-exempt 0 $'Merge branch \'feature\' into main\n' "exempt"
msg_case revert-subject-exempt 0 $'Revert "feat: x"\n\nThis reverts commit 0123abc.\n' "exempt"
msg_case merge-word-inside-other-subject-not-exempt 1 $'Merged things\n\nStory: EPIC-001-S02\n' "subject must start with"
msg_case comment-lines-ignored 0 $'feat: x\n\nStory: EPIC-001-S02\n# On branch main\n# Changes to be committed:\n'
msg_case crlf-message-accepted 0 $'feat: x\r\n\r\nStory: EPIC-001-S02\r\n'
out="$(bash "$hook" 2>&1)"; rc=$?
if [ "$rc" -eq 2 ]; then report no-argument-exit-2 0 ""; else report no-argument-exit-2 1 "rc=$rc out=$out"; fi

# --- through git: installed hooks in a temporary repository -------------------------------------------------------
repo="$(new_repo hook with-lint)"
(cd "$repo" && bash tools/ci/hooks/install-hooks.sh >/dev/null 2>&1)
echo "// change" >> "$repo/Source/StealthGameplay/Public/View/StealthSafeView.h"; git -C "$repo" add -A >/dev/null 2>&1
head_before="$(git -C "$repo" rev-parse HEAD)"
out="$(git -C "$repo" commit -q -m "update: not a conventional prefix" -m "Story: S-1" 2>&1)"; rc=$?
if [ "$rc" -ne 0 ] && [ "$head_before" = "$(git -C "$repo" rev-parse HEAD)" ] && printf '%s' "$out" | grep -Fq "commit-msg: REJECTED"; then report git-rejects-bad-message 0 ""; else report git-rejects-bad-message 1 "rc=$rc out=$out"; fi
printf '%s\n' "$out" | sed 's/^/    /'
out="$(git -C "$repo" commit -q -m "feat(selftest): good message" -m "Story: S-1" -m "$TRAILER" 2>&1)"; rc=$?
if [ "$rc" -eq 0 ] && [ "$(git -C "$repo" log --format=%s -1)" = "feat(selftest): good message" ]; then report git-accepts-good-message 0 ""; else report git-accepts-good-message 1 "rc=$rc out=$out"; fi

# --- the project's own history is READ and judged by the hook: the commit style it uses is accepted --------------------
# (two older commits carry no reference at all and are expected rejections: they predate the hook.)
if git -C "$proj" rev-parse --git-dir >/dev/null 2>&1; then
  total=0; accepted=0; unexpected=""
  for h in $(git -C "$proj" log --format=%h -n 80); do
    total=$((total + 1))
    git -C "$proj" log -1 --format=%B "$h" > "$tmp/hist.txt"
    if bash "$hook" "$tmp/hist.txt" >/dev/null 2>&1; then accepted=$((accepted + 1)); else unexpected="$unexpected $h"; fi
  done
  # The style is accepted when no commit that carries a reference form is rejected (and something passed).
  bad_style=""
  for h in $unexpected; do
    git -C "$proj" log -1 --format=%B "$h" | grep -Eq '^(Story|Task|Decision): |ADR-[0-9]{4}|design/gdd/|docs/architecture/' && bad_style="$bad_style $h"
  done
  if [ "$accepted" -gt 0 ] && [ -z "$bad_style" ]; then
    report real-history-style-accepted 0 ""
  else
    report real-history-style-accepted 1 "accepted $accepted of $total; rejected despite a reference:$bad_style"
  fi
  echo "    history: $accepted of $total recent commits accepted; rejected (no reference in the message):$unexpected"
else
  report real-history-style-accepted 0 "(no git history here: skipped)"
fi

selftest_result
