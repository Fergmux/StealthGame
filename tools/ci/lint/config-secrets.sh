#!/usr/bin/env bash
# Config secret check (decision D52, GitGuardian finding follow-up): no project config file may carry an
# Android File Server `SecurityToken=` line. The AndroidFileServer plugin is disabled in StealthGame.uproject
# (D52) so the editor no longer writes one; this check is the gate that keeps it out if anything re-adds it.
# Emits observations only (see emit.sh): one "<file>:<line> secret <message>" per offending line and one
# "SCANNED <file>" per .ini examined; tools/ci/lint/gate.sh decides (0 PASS, 1 FAIL, 2 NOT ASSESSED).
# `config-secrets.sh --check [<root>]` runs the gate over this script and exits with its code.
#
# Scope: every *.ini under <root>/Config/ (DefaultEngine.ini and the platform folders such as
# Config/Windows/). A line is a finding when it contains `SecurityToken` followed by optional spaces and `=`,
# matched case-insensitively and whatever precedes it (a `;` comment or a `+`/`-`/`.` array prefix still
# carries the value into git). The value itself is never printed: the finding names the file and line only.
#
# No Config/ folder, or no .ini in it: nothing is scanned, so the gate reports NOT ASSESSED; never a pass.
#
# Usage: config-secrets.sh [--check] [<root>]    root defaults to the repo root
set -u
here="$(cd "$(dirname "$0")" && pwd)"
if [ "${1:-}" = "--check" ]; then
  shift
  exec bash "$here/gate.sh" --root "${1:-}" "$here/config-secrets.sh"
fi
. "$here/emit.sh"
root="$(lint_root "${1:-}")"

[ -d "$root/Config" ] || { echo "config-secrets: no Config/ folder under $root" >&2; exit 0; }

while IFS= read -r file; do
  rel="$(lint_rel "$root" "$file")"
  lint_scanned "$rel"
  # grep -n prints "<line>:<text>"; only the line number is kept so the token value never reaches the log.
  grep -inE 'SecurityToken[[:space:]]*=' "$file" 2>/dev/null | cut -d: -f1 | while IFS= read -r n; do
    lint_finding "$rel" "$n" secret "SecurityToken= line in a config file (D52: never commit the Android File Server token; delete the line, AndroidFileServer stays disabled)"
  done
done < <(find "$root/Config" -type f -iname '*.ini' | LC_ALL=C sort)
exit 0
