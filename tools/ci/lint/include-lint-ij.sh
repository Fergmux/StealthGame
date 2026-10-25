#!/usr/bin/env bash
# Include lint rules (i)-(j) (ADR-0007 sections 3 and 6; story tdc-015). Emits observations only
# (see emit.sh); tools/ci/lint/gate.sh decides.
#
#   (i) a log, check or trace macro argument (UE_LOG, UE_LOGF, ensureMsgf, checkf, ClientMessage,
#       TRACE_*, CSV_*, SCOPE_CYCLE_COUNTER and the siblings listed in rule-i.awk) that names an
#       identifier from tools/ci/lint/log-forbidden-identifiers.txt (prefix entries end in "*";
#       every other entry is a whole-token match, so "Binding" does not match "KeyBinding")
#   (j) a file under a CityGenerator/ folder including anything under Server/Random/ (the
#       Generator-isolation rule; Public/Server/Random/ is the folder, and a Public/ header is
#       included as "Server/Random/<Name>.h")
#
# Scope (G5): all .h/.cpp/.inl under the five production modules, tests included (the story inputs
# are silent on tests, so they are scanned); the prototype module is outside both rules. The real
# tree has no CityGenerator/ folder yet, so (j) has nothing to scan there: its proof is the seeded
# fixture and the gate's NOT ASSESSED rule, not the real-tree pass. Rule ids are i and j.
# The forbidden list is read from <root>/tools/ci/lint/log-forbidden-identifiers.txt, else from
# this script's own folder, so a fixture tree can carry its own list.
#
# Usage: include-lint-ij.sh [<root>]    root defaults to the repo root; a fixture tree has the same layout
set -u
. "$(cd "$(dirname "$0")" && pwd)/emit.sh"
root="$(lint_root "${1:-}")"
list="$root/tools/ci/lint/log-forbidden-identifiers.txt"
[ -f "$list" ] || list="$LINT_DIR/log-forbidden-identifiers.txt"

files=()
generator=()
for module in $(lint_scope_modules); do
  while IFS= read -r file; do
    [ -n "$file" ] || continue
    rel="${file#"$root"/}"
    files+=("$file")
    lint_scanned "$rel"
    case "/$rel" in
      */CityGenerator/*) generator+=("$file") ;;
    esac
  done < <(lint_module_files "$root" "$module")
done

if [ ${#files[@]} -gt 0 ]; then
  LINT_LIST="$list" lint_cpp_awk rule-i.awk "$root" "${files[@]}"
fi
if [ ${#generator[@]} -gt 0 ]; then
  while IFS=$'\t' read -r file n inc; do
    [ -n "$file" ] || continue
    case "/$inc" in
      */Server/Random/*) lint_finding "${file#"$root"/}" "$n" j "CityGenerator file includes \"$inc\" (Generator isolation: no Public/Server/Random/ include)" ;;
    esac
  done < <(lint_includes "${generator[@]}")
fi
exit 0
