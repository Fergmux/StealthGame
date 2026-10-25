#!/usr/bin/env bash
# Include lint rules (d)-(f) (ADR-0007 section 3; story tdc-013). Emits observations only (see
# emit.sh); tools/ci/lint/gate.sh decides.
#
#   (d) a file under a Core/ folder of StealthGameplay includes a Features/ path. Direction only:
#       a Features/ file including Core/ is allowed. A path counts when the include text has a
#       Features/ segment, or when the include resolves (by header name) to a StealthGameplay
#       header that sits under a Features/ folder.
#   (e) a StealthFoundation file includes anything owned by StealthGameplay, StealthPresentation or
#       StealthGame (the prototype module). Project headers are included without a module prefix
#       ("Server/Foo.h"), so the include is resolved to its owning module by header name, then
#       checked against the owner's folder path; an include whose path names one of those modules
#       as a folder is flagged too.
#   (f) two project headers with the same file name in different modules (case-insensitive, every
#       .h under Source/<module>, Public and Private). One finding per extra module, reported at
#       that module's first header. A duplicate name inside ONE module (different subfolders) is
#       not reported: the rule is about names across modules, where UBT would resolve the include
#       to whichever module comes first. Headers listed in prototype-exempt.txt (the pre-ADR-0007
#       prototype, which may only shrink, G3) are left out of the comparison, so the prototype's
#       StealthMovementComponent.h may share a name with a future production header until the
#       prototype is deleted; two non-listed headers still collide.
#
# Scope (G5): the five production modules (emit.sh lint_scope_modules) plus the prototype module
# Source/StealthGame, which is scanned only as an owner for (e) and as a header source for (f).
# Rule ids are the ADR-0007 letters d, e, f.
#
# Usage: include-lint-def.sh [<root>]    root defaults to the repo root; a fixture tree has the same
#                                        layout (Source/..., optional tools/ci/lint/prototype-exempt.txt)
set -u
. "$(cd "$(dirname "$0")" && pwd)/emit.sh"
root="$(lint_root "${1:-}")"

declare -A hdr_entries=()   # lower-case file name -> lines "<module>|<path relative to Source/<module>>|<repo-relative path>"
declare -A hdr_exempt=()    # repo-relative path -> 1 for headers on the prototype exempt list
exempt_file="$root/tools/ci/lint/prototype-exempt.txt"
if [ -f "$exempt_file" ]; then
  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%$'\r'}"
    case "$line" in ''|'#'*) ;; *) hdr_exempt[$line]=1 ;; esac
  done < "$exempt_file"
fi
modules=($(lint_scope_modules) $(lint_prototype_module))
for module in "${modules[@]}"; do
  [ -d "$root/Source/$module" ] || continue
  while IFS= read -r file; do
    [ -n "$file" ] || continue
    rel="${file#"$root"/}"
    sub="${rel#Source/$module/}"
    name="${file##*/}"; name="${name,,}"
    hdr_entries[$name]+="$module|$sub|$rel"$'\n'
  done < <(lint_module_files "$root" "$module" | grep '\.h$')
done

# (f) duplicate header names across modules.
for name in "${!hdr_entries[@]}"; do
  [[ "${hdr_entries[$name]%$'\n'}" == *$'\n'* ]] || continue # a name used once cannot collide
  first_module=""; first_rel=""
  declare -A seen_module=()
  while IFS='|' read -r module sub rel; do
    [ -n "$module" ] || continue
    [ -n "${hdr_exempt[$rel]:-}" ] && continue
    if [ -z "$first_module" ]; then first_module="$module"; first_rel="$rel"; seen_module[$module]=1; continue; fi
    [ -n "${seen_module[$module]:-}" ] && continue
    seen_module[$module]=1
    lint_finding "$rel" 1 f "header name '${rel##*/}' is also used in module $first_module ($first_rel); project header names must be unique across modules"
  done < <(printf '%s' "${hdr_entries[$name]}" | LC_ALL=C sort -t'|' -k3)
  unset seen_module
done | LC_ALL=C sort

while IFS= read -r file; do
  [ -n "$file" ] && lint_scanned "${file#"$root"/}"
done < <(for module in "${modules[@]}"; do lint_module_files "$root" "$module" | grep '\.h$'; done)

# Prints "<module>|<rel>" of every header a given include target resolves to (suffix match on the
# owner's folder path), one per line.
resolve_include() {
  local inc="${1//\\//}" name entry module sub rel
  name="${inc##*/}"; name="${name,,}"
  while IFS='|' read -r module sub rel; do
    [ -n "$module" ] || continue
    if [[ "/$sub" == */"$inc" ]]; then printf '%s|%s\n' "$module" "$rel"; fi
  done < <(printf '%s' "${hdr_entries[$name]:-}")
}

# (d) Core/ files of StealthGameplay including Features/.
core_files=()
while IFS= read -r file; do
  [ -n "$file" ] || continue
  rel="${file#"$root"/}"
  case "/${rel#Source/StealthGameplay/}" in
    */Core/*) core_files+=("$file"); lint_scanned "$rel" ;;
  esac
done < <(lint_module_files "$root" StealthGameplay)
if [ ${#core_files[@]} -gt 0 ]; then
  while IFS=$'\t' read -r file n inc; do
    [ -n "$file" ] || continue
    rel="${file#"$root"/}"
    case "/$inc" in
      */Features/*) lint_finding "$rel" "$n" d "Core file includes Features path \"$inc\" (Core may not depend on Features)"; continue ;;
    esac
    while IFS='|' read -r owner owner_rel; do
      [ "$owner" = StealthGameplay ] || continue
      case "/${owner_rel#Source/StealthGameplay/}" in
        */Features/*) lint_finding "$rel" "$n" d "Core file includes \"$inc\", which resolves to the Features header $owner_rel"; break ;;
      esac
    done < <(resolve_include "$inc")
  done < <(lint_includes "${core_files[@]}")
fi

# (e) StealthFoundation including an upper module.
found_files=()
while IFS= read -r file; do
  [ -n "$file" ] && found_files+=("$file") && lint_scanned "${file#"$root"/}"
done < <(lint_module_files "$root" StealthFoundation)
if [ ${#found_files[@]} -gt 0 ]; then
  while IFS=$'\t' read -r file n inc; do
    [ -n "$file" ] || continue
    rel="${file#"$root"/}"
    case "/$inc" in
      */StealthGameplay/*|*/StealthPresentation/*|*/StealthGame/*)
        lint_finding "$rel" "$n" e "StealthFoundation includes \"$inc\" from a higher module (Foundation depends on no upper module)"
        continue ;;
    esac
    while IFS='|' read -r owner owner_rel; do
      case "$owner" in
        StealthGameplay|StealthPresentation|StealthGame)
          lint_finding "$rel" "$n" e "StealthFoundation includes \"$inc\", owned by module $owner ($owner_rel); Foundation depends on no upper module"
          break ;;
      esac
    done < <(resolve_include "$inc")
  done < <(lint_includes "${found_files[@]}")
fi
exit 0
