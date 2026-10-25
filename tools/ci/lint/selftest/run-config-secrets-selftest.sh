#!/usr/bin/env bash
# Selftest for config-secrets.sh (decision D52; ADR-0008 G2/G6). Builds throwaway project roots with a clean
# Config/ folder, seeds ONE SecurityToken= line per case and checks the gate reports exactly that finding
# (file and line, never the value); a clean Config/ passes; a root with no Config/ (or no .ini in it) is
# NOT ASSESSED; the real repository has no SecurityToken= line.
#
# Usage: run-config-secrets-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/config-secrets.sh")
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"

# The token value seeded below; the gate output must never contain it.
TOKEN_VALUE="0123456789ABCDEFSEEDEDTOKEN"

# make_root <name> : a project root with a clean Config/ folder; prints its path.
make_root() {
  local dir="$tmp/$1"
  mkdir -p "$dir/Config/Windows"
  printf '[/Script/EngineSettings.GameMapsSettings]\nGameDefaultMap=/Game/Maps/Default\n\n[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]\nbEnablePlugin=False\nbAllowNetworkConnection=False\n' > "$dir/Config/DefaultEngine.ini"
  printf '[/Script/Engine.GameSession]\nMaxPlayers=10\n' > "$dir/Config/DefaultGame.ini"
  printf '[SystemSettings]\nr.Example=1\n' > "$dir/Config/Windows/WindowsEngine.ini"
  printf '%s\n' "$dir"
}

root="$(make_root clean)"
gate_case clean-config-passes "$root" 0 0 "GATE: PASS (3 file(s) scanned"

root="$(make_root default-engine)"
printf 'SecurityToken=%s\n' "$TOKEN_VALUE" >> "$root/Config/DefaultEngine.ini"
gate_case token-in-default-engine-reported "$root" 1 1 "Config/DefaultEngine.ini:7 secret SecurityToken= line" "GATE: FAIL"

root="$(make_root platform)"
printf 'SecurityToken=%s\n' "$TOKEN_VALUE" >> "$root/Config/Windows/WindowsEngine.ini"
gate_case token-in-platform-ini-reported "$root" 1 1 "Config/Windows/WindowsEngine.ini:3 secret"

root="$(make_root spaced-lowercase)"
printf 'securitytoken = %s\n' "$TOKEN_VALUE" >> "$root/Config/DefaultGame.ini"
gate_case token-spaced-lowercase-reported "$root" 1 1 "Config/DefaultGame.ini:3 secret"

root="$(make_root commented)"
printf ';SecurityToken=%s\n+SecurityToken=%s\n' "$TOKEN_VALUE" "$TOKEN_VALUE" >> "$root/Config/DefaultEngine.ini"
gate_case commented-and-array-token-reported "$root" 1 2 "Config/DefaultEngine.ini:7 secret" "Config/DefaultEngine.ini:8 secret"

# The finding names file and line only: the token value never reaches the log.
out="$(bash "$lint/gate.sh" --root "$root" "${RULES[@]}" 2>&1)"
if printf '%s' "$out" | grep -Fq "$TOKEN_VALUE"; then report token-value-not-echoed 1 "the value appears in the output"; else report token-value-not-echoed 0 ""; fi

root="$(make_root key-without-assignment)"
printf 'bUseSecurityTokenCheck=True\n; SecurityToken is disabled here\n' >> "$root/Config/DefaultEngine.ini"
gate_case mention-without-assignment-passes "$root" 0 0 "GATE: PASS"

root="$(make_root outside-config)"
mkdir -p "$root/Saved/Config/WindowsEditor"
printf 'SecurityToken=%s\n' "$TOKEN_VALUE" > "$root/Saved/Config/WindowsEditor/Engine.ini"
printf 'SecurityToken=%s\n' "$TOKEN_VALUE" > "$root/Config/notes.txt"
gate_case outside-config-ini-ignored "$root" 0 0 "GATE: PASS (3 file(s) scanned"

# No Config/ folder, or one without any .ini: NOT ASSESSED, never a pass.
mkdir -p "$tmp/no-config"
gate_case no-config-not-assessed "$tmp/no-config" 2 0 "GATE: NOT ASSESSED"
mkdir -p "$tmp/empty-config/Config"
gate_case empty-config-not-assessed "$tmp/empty-config" 2 0 "GATE: NOT ASSESSED"

# The real repository carries no SecurityToken= line.
gate_case real-repo-clean "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
