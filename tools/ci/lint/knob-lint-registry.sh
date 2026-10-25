#!/usr/bin/env bash
# Doc lint: every owned key matches a registry constant, field by field (story tdc-033; TDC GDD AC-3b bullet 3, registry half, CR-15.7).
# Emits observations only (see emit.sh); tools/ci/lint/gate.sh decides. The rule is the "registry" rule of
# knob_lint.py, which documents it in full.
#
# Usage: knob-lint-registry.sh [<root>]    root defaults to the repo root; a fixture tree has the same layout
set -u
. "$(cd "$(dirname "$0")" && pwd)/knob-common.sh"
root="$(lint_root "${1:-}")"
knob_rule registry "$root"
exit $?
