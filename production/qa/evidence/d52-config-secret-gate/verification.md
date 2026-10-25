# D52 verification: AndroidFileServer disabled, config secret gate added

Decision: D52 (`production/design-closeout/2026-10-25_rereview-followups.md`): disable the engine AndroidFileServer
plugin in `StealthGame.uproject` (commit ed0aa92) so editor runs stop re-adding the Android File Server
`SecurityToken` to `Config/DefaultEngine.ini` (GitGuardian finding), and add a gate check that rejects
`SecurityToken=` in `Config/*.ini`.
Date: 2026-10-25 (unattended build run).

## 1. Build and editor launch still work with the plugin disabled

- `tools/dev/cycle.ps1 -Tests` (build, headless `^StealthGame.` run, GUI editor launch): `Result: Succeeded`,
  `tests exit=0`, 43/43 Success, `mcp started: True` (editor log: `Starting MCP server on port 8000` at 11:59:28,
  `Engine is initialized`, `Total Editor Startup Time, took 14.906`).
- No `Mounting Engine plugin AndroidFileServer` line in the editor log: the plugin is not loaded.

## 2. An editor run no longer writes SecurityToken

- SHA-1 of every `Config/*.ini` taken before the run and re-checked after the headless test run, the GUI editor
  launch (MCP up) and its close, and the second headless run: all `OK` (unchanged). `git status --short Config`
  empty after each.
- `grep -rn SecurityToken Config/ Saved/Config/`: no hit.

## 3. The gate check

- Rule script `tools/ci/lint/config-secrets.sh` (rule id `secret`; emits observations, `tools/ci/lint/gate.sh`
  decides): every `*.ini` under `Config/` (platform folders included); a line with `SecurityToken` followed by
  optional spaces and `=` (any case; commented or `+`/`-` array lines included) is a finding naming file and line
  only, never the value. No `Config/` or no `.ini` is NOT ASSESSED.
- Wired into the per-commit gate (`tools/ci/gate-per-commit.ps1` `$LintRules`, so CI `tests.yml` and the nightly
  run it) and the opt-in pre-commit hook (`tools/ci/hooks/pre-commit.sh`).
- Selftest `tools/ci/lint/selftest/run-config-secrets-selftest.sh`: `SELFTEST RESULT: PASS (11 cases)` (output in
  `selftest.txt`): seeded tokens in DefaultEngine.ini, a platform ini, lower case with spaces, commented and array
  lines are each reported and FAIL the gate; the value is never echoed; a key without an assignment passes; ini files
  outside `Config/` are not scanned; no `Config/` and an empty `Config/` are NOT ASSESSED; the real repository is
  clean.
- Pre-commit selftest gained `config-secret-commit-rejected` (a commit staging a `SecurityToken=` line is rejected)
  and its gate/hook rule-list comparison now reads the `$LintRules` line instead of the first five quoted names:
  `SELFTEST RESULT: PASS (18 cases)`. `gate-per-commit.ps1 -SelfTest`: `SELFTEST RESULT: PASS (12 cases)`.
- Automation wrapper `StealthGame.Build.ConfigSecrets`
  (`Source/StealthFoundation/Private/Tests/Build/ConfigSecretsTest.cpp`).

## Build + test after the change

`tools/dev/cycle.ps1 -Tests -NoLaunch`: `Result: Succeeded`, `tests exit=0`, **44/44 Success** (43 before +
`StealthGame.Build.ConfigSecrets`); full list in `build-and-tests.txt`.

Run result: N/A — config and CI tooling only; nothing player-visible.
