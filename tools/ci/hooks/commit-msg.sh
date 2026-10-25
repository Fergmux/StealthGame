#!/usr/bin/env bash
# commit-msg hook body (ADR-0009 section 5, .claude/docs/coding-standards.md; story tdc-028). Git runs the
# `commit-msg` shim with the path of the message file; the shim runs this script. A commit-msg hook is itself
# the gate (nothing sits behind it), so it prints the reason it rejected and exits nonzero.
#
# RULES
#   1. The subject (first non-comment line) starts with one of feat, fix, chore, docs, test, refactor,
#      then an optional scope in parentheses, an optional "!" (breaking change), a colon, a space and a
#      description:  `feat: x`, `fix(ci): x`, `refactor!: x`.  A SCOPED prefix IS accepted (decision recorded in
#      story tdc-028): Conventional Commits permits it and the project's history uses it (`feat(ci): ...`).
#   2. The BODY (every line after the subject) holds at least one reference, any one is enough:
#        * a line `Story: <ID>`, `Task: <ID>` or `Decision: <ID>`  (the existing history uses all three)
#        * the token ADR- followed by exactly four digits (ADR-0009 yes; ADR-12 and ADR-00123 no)
#        * a path beginning design/gdd/ or docs/architecture/
#      Trailer lines (`Co-Authored-By:`, `Signed-off-by:`) are neither rejected nor counted as references.
#      A reference in the subject line alone does not count: the AC reads the body.
#
# DECISIONS recorded in story tdc-028 (stated, not silent)
#   * An EMPTY message (nothing left after dropping blank and `#` comment lines) is rejected.
#   * Git-generated `Merge ...` and `Revert "..."` subjects are EXEMPT from rules 1 and 2 (git writes those
#     subjects itself and a merge/revert has no story of its own); a NOTE line says so. `fixup!`/`squash!`
#     subjects are NOT exempt: reword the commit or use --no-verify for a throwaway fixup.
#   * Lines starting with `#` are comments (git strips them in editor mode) and are ignored; CR is stripped.
#
# Usage (as git runs it): commit-msg.sh <message-file>
# Exit: 0 accepted | 1 rejected (reason printed on stderr) | 2 no message file given.
set -u

msgfile="${1:-}"
if [ -z "$msgfile" ] || [ ! -f "$msgfile" ]; then
  echo "commit-msg: no message file given (git passes it as the first argument)" >&2
  exit 2
fi

reject() {
  echo "commit-msg: REJECTED - $1" >&2
  cat >&2 <<'HELP'
  Expected: a subject `<type>[(scope)]: <description>` with type feat|fix|chore|docs|test|refactor, and a body
  with at least one of:  `Story: <ID>` / `Task: <ID>` / `Decision: <ID>` line,  an ADR-NNNN token (four digits),
  or a path under design/gdd/ or docs/architecture/.  (.claude/docs/coding-standards.md; ADR-0009 section 5)
HELP
  exit 1
}

# Message without CR, `#` comment lines and trailing blank lines; leading blank lines dropped.
body_all="$(tr -d '\r' < "$msgfile" | grep -v '^#' | sed -e :a -e '/^[[:space:]]*$/{$d;N;ba' -e '}' | sed '/./,$!d')"
[ -z "$body_all" ] && reject "empty commit message"

subject="$(printf '%s\n' "$body_all" | head -n 1)"
rest="$(printf '%s\n' "$body_all" | tail -n +2)"

case "$subject" in
  "Merge "*|"Revert \""*)
    echo "commit-msg: NOTE exempt - git-generated merge/revert subject: $subject" >&2
    exit 0 ;;
esac

if ! printf '%s' "$subject" | grep -Eq '^(feat|fix|chore|docs|test|refactor)(\([^()]+\))?!?: .+'; then
  reject "subject must start with feat|fix|chore|docs|test|refactor (optionally scoped, e.g. feat(ci):) then ': ' and a description; got: $subject"
fi

refs="$(printf '%s\n' "$rest" | grep -Eiv '^(co-authored-by|signed-off-by):' || true)"
if printf '%s\n' "$refs" | grep -Eq '^(Story|Task|Decision): +[^[:space:]]+' ; then exit 0; fi
if printf '%s\n' "$refs" | grep -Eq '(^|[^A-Za-z0-9-])ADR-[0-9]{4}([^0-9]|$)'; then exit 0; fi
if printf '%s\n' "$refs" | grep -Eq '(^|[^A-Za-z0-9_./-])(design/gdd/|docs/architecture/)[^[:space:]]+'; then exit 0; fi

reject "the body has no reference: add a 'Story: <ID>' line, an ADR-NNNN token, or a design/gdd/ or docs/architecture/ path"
