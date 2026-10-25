# Test Infrastructure

**Engine**: Unreal Engine 5.8 (installed 5.8.4)
**Test Framework**: UE Automation (Simple tests, Automation Specs, CQTest for PIE network tests per ADR-0008)
**CI**: `.github/workflows/tests.yml` (self-hosted Windows runner only)
**Setup date**: 2026-10-25

## Directory Layout

```
tests/
  smoke/          # Critical path test list for the /smoke-check gate
```

Unit and integration tests live under the engine's test roots, because
UnrealBuildTool compiles only module folders (a `tests/` folder is never built):

```
Source/StealthGame/Private/Tests/         # game-module tests (StealthRulesTests.cpp)
Source/StealthGameEditor/Private/Tests/   # editor-module specs (toolset specs)
```

Every `tests/unit/[system]/` path in the studio docs means
`Source/StealthGame/Private/Tests/[System]/` (or the editor module's equivalent
for editor-only code).

```
production/qa/
  evidence/       # Screenshot logs and manual test sign-off records
```

> **Manual evidence lives under `production/qa/evidence/`, not `tests/`** -- that
> is where every consumer reads it.

## Running Tests

Locally (builds the editor target, runs `StealthGame.` tests, optionally
relaunches the editor -- it closes a running editor first, so use `-NoLaunch`
and do not run it while someone else is using the editor):

```powershell
tools/dev/cycle.ps1 -Tests -NoLaunch
```

Headlessly by hand, from the project root (the project path must be absolute),
after building the editor target:

```bash
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" StealthGameEditor Win64 Development -Project="$(pwd -W 2>/dev/null || pwd)/StealthGame.uproject"
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$(pwd -W 2>/dev/null || pwd)/StealthGame.uproject" -ExecCmds="Automation RunTests ^StealthGame.; Quit" -unattended -nullrhi -stdout -FullStdOutLogOutput
```

This is `commands.test` in `project.yaml`. The leading `^` makes the filter a
prefix match so only `StealthGame.*` tests run (ADR-0008 section 2).

**Read the output, not the exit code**: a failing test and a run that matched
nothing both exit 255. `Result={Fail}` is a failure; `No automation tests
matched` means no test was built or named `StealthGame.*` -- report it as NOT
ASSESSED, never as a pass. Or use Session Frontend -> Automation in the editor.

## Test Naming

Names follow `.claude/rules/test-standards.md`:
- Class `F[System][Scenario]Test` (one class per test; a duplicate class name
  fails to link), test name `StealthGame.[System].[Scenario]`
  -- `FStealthRulesAssassinationTest` -> `StealthGame.Rules.Assassination`
- CQTest PIE network tests: `StealthGame.<System>.PIE.<Scenario>.<Method>`
  (exempt from the class-name rule, see ADR-0008)
- Wrap each test file in `#if WITH_DEV_AUTOMATION_TESTS` ... `#endif`.
  Flags: `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`
  (there is no `GameFilter` flag).

## Story Type -> Test Evidence

| Story Type | Required Evidence | Location |
|---|---|---|
| Logic | Automated unit test -- must pass | `Source/<Module>/Private/Tests/[System]/` |
| Integration | Integration test OR playtest doc | `Source/<Module>/Private/Tests/[System]/` (CQTest PIE) or `production/qa/evidence/` |
| Visual/Feel | Screenshot + lead sign-off | `production/qa/evidence/` |
| UI | Retained screenshot of each screen touched | `production/qa/evidence/` |
| Config/Data | Smoke check pass | `production/qa/smoke-*.md` |

## CI

`.github/workflows/tests.yml` runs on every push to `main` and every pull
request, **on a self-hosted Windows runner with Unreal Engine installed**
(`runs-on: [self-hosted, windows]`). GitHub-hosted runners cannot run UE. Set
the `UE_ROOT` environment variable on the runner to the engine folder
(`C:/Program Files/Epic Games/UE_5.8`) and keep the runner at Actions Runner
2.327.1 or newer. A failed test suite blocks merging once the job is a required
check.
