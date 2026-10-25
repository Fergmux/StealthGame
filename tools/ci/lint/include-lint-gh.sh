#!/usr/bin/env bash
# Include lint rules (g)-(h) (ADR-0007 section 3; story tdc-014). Emits observations only (see
# emit.sh); tools/ci/lint/gate.sh decides. The C++-aware parsing is in cpplex.awk (comments and
# string literals are never read as code); the rule bodies are rule-g.awk and rule-h.awk.
#
#   (g) a header under Public/Server/ whose UCLASS lacks NotBlueprintType, NotBlueprintable, whose
#       UINTERFACE lacks meta=(CannotImplementInterfaceInBlueprint), or whose UPROPERTY/UFUNCTION
#       carries BlueprintCallable, BlueprintPure, BlueprintReadOnly, BlueprintReadWrite or
#       BlueprintAssignable
#   (h) StealthPresentation source (Public and Private) using TActorIterator, TActorRange,
#       GetAllActorsOfClass, GetAllActorsWithInterface, GetPlayerControllerIterator, GetGameMode or
#       GetAuthGameMode, or any GetPlayerState/GetController use without a
#       "// LOCAL-PAWN-OK: <reason>" comment on the same line (empty reason is a finding)
#
# Scope (G5): (g) the Public/Server/ headers of the five production modules; (h) all source of
# StealthPresentation. The prototype module is not scanned by either rule. Rule ids are g and h.
#
# Usage: include-lint-gh.sh [<root>]    root defaults to the repo root; a fixture tree has the same layout
set -u
. "$(cd "$(dirname "$0")" && pwd)/emit.sh"
root="$(lint_root "${1:-}")"

server_headers=()
for module in $(lint_scope_modules); do
  while IFS= read -r file; do
    [ -n "$file" ] || continue
    rel="${file#"$root"/}"
    case "$rel" in
      Source/"$module"/Public/Server/*.h) server_headers+=("$file"); lint_scanned "$rel" ;;
    esac
  done < <(lint_module_files "$root" "$module")
done
if [ ${#server_headers[@]} -gt 0 ]; then
  lint_cpp_awk rule-g.awk "$root" "${server_headers[@]}"
fi

presentation=()
while IFS= read -r file; do
  [ -n "$file" ] || continue
  presentation+=("$file")
  lint_scanned "${file#"$root"/}"
done < <(lint_module_files "$root" StealthPresentation)
if [ ${#presentation[@]} -gt 0 ]; then
  lint_cpp_awk rule-h.awk "$root" "${presentation[@]}"
fi
exit 0
