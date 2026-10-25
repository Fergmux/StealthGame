#!/usr/bin/env bash
# Doc lint: an owner Tuning Knobs row read by an invariant says "bounded by <invariant Id>" (story tdc-036;
# ADR-0004 section 4, the C-W18 pattern). Emits observations only (see emit.sh); tools/ci/lint/gate.sh decides.
# The rule is the "bounded-by" rule of knob_lint.py, which documents it in full.
#
# Accepted findings live in tools/ci/lint/knob-bounded-by-baseline.txt (ADR-0004: never silence a finding by
# editing the baseline without citing a GDD change request or an open question; each entry carries its
# citation). G3: the baseline may only shrink. lint_shrink_only reports every entry that is absent from the
# list at the base revision: HEAD when run from the pre-commit hook, else $LINT_BASE_REF (CI passes the
# push's before-SHA, github.event.before, or HEAD~1). The commit that creates the baseline is exempt.
#
# Usage: knob-lint-bounded-by.sh [<root>]    root defaults to the repo root; a fixture tree has the same layout
set -u
. "$(cd "$(dirname "$0")" && pwd)/knob-common.sh"
root="$(lint_root "${1:-}")"
knob_rule bounded-by "$root"
rc=$?
lint_shrink_only "$root" tools/ci/lint/knob-bounded-by-baseline.txt shrink-only
exit $rc
