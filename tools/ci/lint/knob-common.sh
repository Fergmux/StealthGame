#!/usr/bin/env bash
# Shared launcher for the doc-lint rule scripts (knob-lint-*.sh, stories tdc-031/032/033/036) and the
# knob-table selftests. SOURCE this file. The rules themselves are the Python build-time tool knob_lint.py
# (standard library only, run in isolated mode as `python -I`); this file only finds an interpreter and
# converts paths for it. It never decides pass or fail: the rule scripts print observations and
# tools/ci/lint/gate.sh decides. An interpreter that cannot be found is a rule that could not run (exit 3,
# which the gate reports as NOT ASSESSED), never a silent pass.
#
#   knob_python              prints the interpreter command, or returns 1 when none works. STEALTH_PYTHON
#                            overrides; else python, python3, then "py -3".
#   knob_native <path>       the path in the form the interpreter wants (cygpath -m under Git for Windows)
#   knob_rule <rule> <root>  runs `knob_lint.py <rule> --root <root>` and prints its observations
. "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/emit.sh"

knob_python() {
  local cand
  for cand in "${STEALTH_PYTHON:-}" python python3 "py -3"; do
    [ -n "$cand" ] || continue
    # shellcheck disable=SC2086
    if $cand -I -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then
      printf '%s\n' "$cand"
      return 0
    fi
  done
  return 1
}

knob_native() {
  if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi
}

knob_rule() {
  local rule="$1" root="$2" py
  if ! py="$(knob_python)"; then
    echo "knob-lint: no Python 3.8+ interpreter found (set STEALTH_PYTHON); the doc lint could not run" >&2
    return 3
  fi
  # shellcheck disable=SC2086
  $py -I "$(knob_native "$LINT_DIR/knob_lint.py")" "$rule" --root "$(knob_native "$root")" | tr -d '\r'
  return "${PIPESTATUS[0]}"
}
