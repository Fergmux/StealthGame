# Unreal Automation Tests
Tests use the UE Automation Testing Framework and compile with this module.
Wrap each test file in `#if WITH_DEV_AUTOMATION_TESTS` ... `#endif`.
Flags: `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`
(there is no `GameFilter` flag).

Test root: `StealthGame.` -- the filter `commands.test` in `project.yaml` uses
(`Automation RunTests ^StealthGame.`; `^` = prefix match).

Run via: Session Frontend -> Automation -> select "StealthGame." tests
Or headlessly from the project root, after building the editor target (on a
fresh checkout the editor exits before any test runs):
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" StealthGameEditor Win64 Development -Project="$(pwd -W 2>/dev/null || pwd)/StealthGame.uproject"
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$(pwd -W 2>/dev/null || pwd)/StealthGame.uproject" -ExecCmds="Automation RunTests ^StealthGame.; Quit" -unattended -nullrhi -stdout -FullStdOutLogOutput
The project path must be absolute: UE does not find a relative one (exit 1,
"Project file not found"). Or locally: `tools/dev/cycle.ps1 -Tests -NoLaunch`.

Read the output, not the exit code: a failing test and a run that matched nothing both exit 255.
`Result={Fail}` is a failure; `No automation tests matched` means no test was
built or named `StealthGame.*`.

Test class naming: F[System][Scenario]Test -- one class per test; a second
test reusing a class name fails to link
Test name: "StealthGame.[System].[Scenario]"
Editor-only specs live in `Source/StealthGameEditor/Private/Tests/`.
