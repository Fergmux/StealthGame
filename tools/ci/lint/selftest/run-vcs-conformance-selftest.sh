#!/usr/bin/env bash
# Selftest for vcs-conformance.sh (story tdc-024; ADR-0009 sections 2-3; ADR-0008 G2/G6).
# Builds throwaway git repositories from the compliant fixture pair under vcs-conformance/compliant/,
# applies ONE violation per case and checks the gate reports exactly that finding; a compliant pair
# passes; a folder that is not a git checkout is NOT ASSESSED; the real repository conforms.
#
# Usage: run-vcs-conformance-selftest.sh     Exit 0 when every case holds, else 1.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
lint="$here/.."
RULES=("$lint/vcs-conformance.sh")
fx="$here/vcs-conformance/compliant"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
. "$here/lib.sh"

# make_repo <name> : a fresh git repo holding the compliant pair; prints its path.
make_repo() {
  local dir="$tmp/$1"
  mkdir -p "$dir"
  cp "$fx/gitignore.txt" "$dir/.gitignore"
  cp "$fx/gitattributes.txt" "$dir/.gitattributes"
  git -C "$dir" init -q
  printf '%s\n' "$dir"
}

# violation_case <name> <expected-finding-count> <required text...> ; the caller mutates $repo first.
violation_case() {
  local name="$1" n="$2"; shift 2
  gate_case "$name" "$repo" 1 "$n" "$@"
}

repo="$(make_repo compliant)"
gate_case compliant-passes "$repo" 0 0 "GATE: PASS (2 file(s) scanned"

repo="$(make_repo build-ignored)"
printf 'Build/\n' >> "$repo/.gitignore"
violation_case build-ignored-reported 1 ".gitignore:0 vcs Build/Scripts/x.cs is ignored"

repo="$(make_repo build-ignored-by-template-rule)"
printf 'build/\n' >> "$repo/.gitignore"
# On a case-insensitive filesystem (Windows) build/ hides Build/; on a case-sensitive one it does not.
# Either result is acceptable here; the case only proves the script does not crash on the pattern.
out="$(bash "$lint/gate.sh" --root "$repo" "${RULES[@]}" 2>&1)"; rc=$?
if [ "$rc" -eq 0 ] || [ "$rc" -eq 1 ]; then report lowercase-build-rule-handled 0 ""; else report lowercase-build-rule-handled 1 "exit $rc"; fi

for folder in _snapshots _backup_pre_rebase 'Plugins/\*/Binaries' 'Plugins/\*/Intermediate' Binaries Intermediate DerivedDataCache Saved; do
  tag="${folder//[^A-Za-z_]/-}"
  repo="$(make_repo "missing-$tag")"
  grep -Ev "^/?$folder/$" "$repo/.gitignore" > "$repo/.gitignore.new" && mv "$repo/.gitignore.new" "$repo/.gitignore"
  want="${folder//\\/}"
  want="${want/\*/Sample}"
  violation_case "missing-ignore-$tag-reported" 1 ".gitignore:0 vcs $want/ is not ignored"
done

repo="$(make_repo two-patterns-on-one-line)"
printf '*.blend *.psd filter=lfs diff=lfs merge=lfs -text\n' >> "$repo/.gitattributes"
violation_case two-patterns-on-one-line-reported 1 ".gitattributes:9 vcs '*.psd' after pattern '*.blend'"
repo="$(make_repo two-patterns-no-wildcard)"
printf '*.tga stray/path.txt filter=lfs\n' >> "$repo/.gitattributes"
violation_case slash-token-reported 1 ".gitattributes:9 vcs 'stray/path.txt'"

repo="$(make_repo uasset-not-lockable)"
sed -i 's/^\*\.uasset \(.*\) lockable$/*.uasset \1/' "$repo/.gitattributes"
violation_case uasset-not-lockable-reported 1 ".gitattributes:0 vcs the *.uasset line lacks 'lockable'"
repo="$(make_repo umap-not-lockable)"
sed -i 's/^\*\.umap \(.*\) lockable$/*.umap \1/' "$repo/.gitattributes"
violation_case umap-not-lockable-reported 1 ".gitattributes:0 vcs the *.umap line lacks 'lockable'"
repo="$(make_repo umap-line-missing)"
grep -v '^\*\.umap ' "$repo/.gitattributes" > "$repo/.gitattributes.new" && mv "$repo/.gitattributes.new" "$repo/.gitattributes"
violation_case umap-line-missing-reported 1 ".gitattributes:0 vcs no line for *.umap"

repo="$(make_repo godot-header)"
printf '# === Engine: Godot 4 ===\n' >> "$repo/.gitignore"
violation_case godot-header-reported 1 ".gitignore:19 vcs Godot/Unity section header remains"
repo="$(make_repo unity-header-lowercase)"
printf '# --- unity ---\n' >> "$repo/.gitignore"
violation_case unity-header-case-insensitive-reported 1 ".gitignore:19 vcs Godot/Unity section header remains"
for pattern in .godot/ '*.import' Library/ Temp/ UnityLockfile; do
  tag="${pattern//[^A-Za-z]/-}"
  repo="$(make_repo "pattern-$tag")"
  printf '%s\n' "$pattern" >> "$repo/.gitignore"
  violation_case "pattern-$tag-reported" 1 ".gitignore:19 vcs Godot/Unity ignore pattern remains: $pattern"
done

# Not a git checkout (a plain folder): NOT ASSESSED, never a pass.
plain="$tmp/plain"
mkdir -p "$plain"
cp "$fx/gitignore.txt" "$plain/.gitignore"
cp "$fx/gitattributes.txt" "$plain/.gitattributes"
out="$(cd "$tmp" && GIT_CEILING_DIRECTORIES="$tmp" bash "$lint/gate.sh" --root "$plain" "${RULES[@]}" 2>&1)"; rc=$?
[ "$rc" -eq 2 ] && printf '%s\n' "$out" | grep -Fq "GATE: NOT ASSESSED"
report not-a-repository-not-assessed $? "exit $rc: $(printf '%s' "$out" | tr '\n' '|')"
printf '%s\n' "$out" | sed 's/^/    /'

# The real repository conforms (observation; the script is the authority, ADR-0009 section 2).
gate_case real-repo-conforms "$lint/../../.." 0 0 "GATE: PASS"

selftest_result
