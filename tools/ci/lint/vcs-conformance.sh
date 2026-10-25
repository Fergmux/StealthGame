#!/usr/bin/env bash
# VCS conformance check for the ignore and attribute rules (ADR-0009 sections 2-3; story tdc-024).
# Emits observations only (see emit.sh): one "<file>:<line> vcs <message>" per violation and one
# "SCANNED <file>" per input file; tools/ci/lint/gate.sh decides (0 PASS, 1 FAIL, 2 NOT ASSESSED).
# `vcs-conformance.sh --check [<root>]` runs the gate over this script and exits with its code, so a
# hook or a person gets the nonzero exit the story asks for without this script deciding anything.
#
# Checks (rule id "vcs"):
#   1. `git check-ignore -v Build/Scripts/x.cs` prints nothing: UAT/Gauntlet scripts live under
#      Build/, and a template build/ rule would hide them silently
#   2. these paths are ignored: _snapshots/ _backup_pre_rebase/ Plugins/*/Binaries/
#      Plugins/*/Intermediate/ Binaries/ Intermediate/ DerivedDataCache/ Saved/
#   3. .gitattributes: every non-comment line is ONE pattern followed by attributes only (the
#      failure ADR-0009 names: "*.fbx *.blend ... filter=lfs" makes *.blend an attribute name and
#      sends those files to no LFS), and the *.uasset and *.umap lines carry `lockable`
#   4. no Godot or Unity section remains in .gitignore: a section HEADER comment (a decorated
#      heading such as "# === Engine: Godot ===" or "# --- Unity ---") that matches Godot or Unity
#      case-insensitively, or any of the patterns .godot/ *.import Library/ Temp/ UnityLockfile
#      (an ordinary comment that merely mentions an engine is not a section header)
#
# Not a repository (the root is not the top level of a git checkout): nothing is scanned, so the
# gate reports NOT ASSESSED; never a silent pass. The check uses `git check-ignore --no-index` with
# the user's global excludes file disabled, so it judges the repository's own .gitignore.
#
# Usage: vcs-conformance.sh [--check] [<root>]    root defaults to the repo root
set -u
here="$(cd "$(dirname "$0")" && pwd)"
if [ "${1:-}" = "--check" ]; then
  shift
  exec bash "$here/gate.sh" --root "${1:-}" "$here/vcs-conformance.sh"
fi
. "$here/emit.sh"
root="$(lint_root "${1:-}")"

# The root must itself be a git checkout, not a folder that happens to sit inside one.
if [ -n "$(git -C "$root" rev-parse --show-cdup 2>/dev/null || echo not-a-repo)" ]; then
  echo "vcs-conformance: $root is not the top level of a git checkout" >&2
  exit 0
fi

ignore="$root/.gitignore"
attrs="$root/.gitattributes"
check_ignore() { git -c core.excludesFile= -C "$root" check-ignore --no-index "$@"; }

if [ -f "$ignore" ]; then
  lint_scanned ".gitignore"
else
  lint_finding ".gitignore" 0 vcs "no .gitignore at the repository root"
fi
if [ -f "$attrs" ]; then
  lint_scanned ".gitattributes"
else
  lint_finding ".gitattributes" 0 vcs "no .gitattributes at the repository root"
fi

# 1. Build/ must not be ignored.
hit="$(check_ignore -v Build/Scripts/x.cs 2>/dev/null | tr -d '\r')"
if [ -n "$hit" ]; then
  lint_finding ".gitignore" 0 vcs "Build/Scripts/x.cs is ignored ($hit); Unreal's Build/ folder must stay tracked"
fi

# 2. Regenerable and local-safety folders must be ignored.
for path in _snapshots/x _backup_pre_rebase/x Plugins/Sample/Binaries/x Plugins/Sample/Intermediate/x Binaries/x Intermediate/x DerivedDataCache/x Saved/x; do
  if ! check_ignore -q "$path" 2>/dev/null; then
    lint_finding ".gitignore" 0 vcs "${path%/x}/ is not ignored (ADR-0009: never commit regenerable or local-safety folders)"
  fi
done

# 3. .gitattributes: one pattern per line, lockable on *.uasset and *.umap.
if [ -f "$attrs" ]; then
  declare -A seen_pattern=() lockable=()
  n=0
  while IFS= read -r line || [ -n "$line" ]; do
    n=$((n + 1)); line="${line%$'\r'}"
    trimmed="${line#"${line%%[![:space:]]*}"}"
    case "$trimmed" in ''|'#'*) continue ;; esac
    read -r -a toks <<< "$trimmed"
    pattern="${toks[0]}"
    seen_pattern[$pattern]=1
    for tok in "${toks[@]:1}"; do
      [ "$tok" = lockable ] && lockable[$pattern]=1
      if ! [[ "$tok" =~ ^[-!]?[A-Za-z0-9_.-]+(=.*)?$ ]] || [[ "$tok" == *'*'* ]] || [[ "${tok%%=*}" == */* ]]; then
        lint_finding ".gitattributes" "$n" vcs "'$tok' after pattern '$pattern' is not an attribute: one pattern per line (a second pattern becomes an attribute name and is never applied)"
      fi
    done
  done < "$attrs"
  for pattern in '*.uasset' '*.umap'; do
    if [ -z "${seen_pattern[$pattern]:-}" ]; then
      lint_finding ".gitattributes" 0 vcs "no line for $pattern (ADR-0009: stored via LFS and lockable)"
    elif [ -z "${lockable[$pattern]:-}" ]; then
      lint_finding ".gitattributes" 0 vcs "the $pattern line lacks 'lockable' (ADR-0009: unmergeable assets are lockable)"
    fi
  done
fi

# 4. No leftover Godot or Unity section in .gitignore.
if [ -f "$ignore" ]; then
  n=0
  while IFS= read -r line || [ -n "$line" ]; do
    n=$((n + 1)); line="${line%$'\r'}"
    trimmed="${line#"${line%%[![:space:]]*}"}"
    trimmed="${trimmed%"${trimmed##*[![:space:]]}"}"
    case "$trimmed" in
      '') continue ;;
      '#'*)
        # a decorated heading: "# === Name ===", "# --- Name ---", "# *** Name ***"
        if [[ "$trimmed" =~ ^#[[:space:]]*(={2,}|-{2,}|\*{2,}) ]] && [[ "${trimmed,,}" == *godot* || "${trimmed,,}" == *unity* ]]; then
          lint_finding ".gitignore" "$n" vcs "Godot/Unity section header remains: $trimmed"
        fi
        ;;
      *)
        bare="${trimmed#/}"
        case "$bare" in
          .godot/|'*.import'|Library/|Temp/|UnityLockfile)
            lint_finding ".gitignore" "$n" vcs "Godot/Unity ignore pattern remains: $trimmed" ;;
        esac
        ;;
    esac
  done < "$ignore"
fi
exit 0
