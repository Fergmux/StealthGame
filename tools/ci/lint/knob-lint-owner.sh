#!/usr/bin/env bash
# Doc lint: every key is owned by exactly one GDD (story tdc-032; TDC GDD AC-3b bullet 2, CR-15.3).
# Emits observations only (see emit.sh); tools/ci/lint/gate.sh decides. The rule is the "owner" rule of
# knob_lint.py, which documents it in full.
#
# Usage: knob-lint-owner.sh [<root>]    root defaults to the repo root; a fixture tree has the same layout
set -u
. "$(cd "$(dirname "$0")" && pwd)/knob-common.sh"
root="$(lint_root "${1:-}")"
knob_rule owner "$root"
exit $?
