#!/usr/bin/env bash
# Shared helpers for the doc-lint and knob-table selftests (stories tdc-029 to tdc-036; ADR-0008 G2/G6).
# A selftest sets `here` (its own folder), `lint` (tools/ci/lint) and `tmp` (a mktemp -d it removes on exit), sources lib.sh (report, gate_case, selftest_result) and then this
# file. Everything runs in throwaway trees under a temp folder; the real repository is only ever READ
# (and only by the cases that name the real tree), never written.
#
#   PY                       the Python 3 interpreter command (knob_python); empty when none (the selftest then fails)
#   fx_tree <name>           a copy of the committed base fixture (tools/ci/lint/fixtures/knob/base) as $tmp/<name>; prints its path
#   fx_empty <name>          an empty tree with design/gdd/ and design/registry/ folders; prints its path
#   fx_replace <file> <old> <new>   replaces the ONE occurrence of <old>; reports a failure when it is not found exactly once
#   fx_write <file>          writes stdin to <file> (creating folders)
#   py_tool <script> <args>  runs tools/ci/lint/<script> with python -I and native paths
#   expect <name> <text> <needle>...   reports ok when every needle (fixed string) occurs in <text>
#   expect_not <name> <text> <needle>  reports ok when the needle does not occur
#   count_lines <text> <regex>         number of lines matching the (grep -E) regex
. "$lint/knob-common.sh"
FIXTURE_BASE="$lint/fixtures/knob/base"
PY="$(knob_python || true)"

fx_tree() {
  mkdir -p "$tmp/$1"
  cp -R "$FIXTURE_BASE/." "$tmp/$1/"
  printf '%s\n' "$tmp/$1"
}

fx_empty() {
  mkdir -p "$tmp/$1/design/gdd" "$tmp/$1/design/registry"
  printf '%s\n' "$tmp/$1"
}

fx_write() {
  mkdir -p "$(dirname "$1")"
  cat > "$1"
}

fx_replace() {
  # shellcheck disable=SC2086
  $PY -I - "$1" "$2" "$3" <<'PYEOF'
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(path, encoding="utf-8", newline="").read()
if text.count(old) != 1:
    sys.stderr.write("fx_replace: %d occurrence(s) of %r in %s\n" % (text.count(old), old, path))
    sys.exit(1)
open(path, "w", encoding="utf-8", newline="").write(text.replace(old, new))
PYEOF
}

py_tool() {
  local script="$1"; shift
  # shellcheck disable=SC2086
  $PY -I "$(knob_native "$lint/$script")" "$@" | tr -d '\r'
  return "${PIPESTATUS[0]}"
}

expect() {
  local name="$1" text="$2" needle missing=""; shift 2
  for needle in "$@"; do
    printf '%s\n' "$text" | grep -Fq -- "$needle" || missing="$missing [$needle]"
  done
  if [ -z "$missing" ]; then report "$name" 0 ""; else report "$name" 1 "missing:$missing"; printf '%s\n' "$text" | sed 's/^/    /' | head -40; fi
}

expect_not() {
  if printf '%s\n' "$2" | grep -Fq -- "$3"; then report "$1" 1 "unexpected: $3"; else report "$1" 0 ""; fi
}

count_lines() {
  printf '%s\n' "$1" | grep -Ec -- "$2"
}

[ -n "$PY" ] || { report python-interpreter-found 1 "no Python 3.8+ interpreter (set STEALTH_PYTHON)"; selftest_result; }
