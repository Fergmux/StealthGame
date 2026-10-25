#!/usr/bin/env bash
# Doc lint: every Tuning Knobs table has the CR-15 columns (story tdc-031; TDC GDD AC-3b bullet 1, CR-15.1).
# Emits observations only (see emit.sh); tools/ci/lint/gate.sh decides. The rule is the "columns" rule of
# knob_lint.py, which documents it in full.
#
# Usage: knob-lint-columns.sh [<root>]    root defaults to the repo root; a fixture tree has the same layout
set -u
. "$(cd "$(dirname "$0")" && pwd)/knob-common.sh"
root="$(lint_root "${1:-}")"
knob_rule columns "$root"
exit $?
