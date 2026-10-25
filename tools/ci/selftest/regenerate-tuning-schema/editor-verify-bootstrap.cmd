@echo off
rem Stub UnrealEditor-Cmd for the regenerate-tuning-schema selftest (story tdc-042).
setlocal EnableDelayedExpansion
set MODE=gen
for %%A in (%*) do (
  set "ARG=%%~A"
  if not "!ARG:SchemaVerify=!"=="!ARG!" set MODE=verify
)
if "!MODE!"=="verify" goto verify
echo [stub] LogStealthTuningSchemaGen: Display: STEALTH TUNING SCHEMA GEN: WROTE stub.inl
exit /b 0
:verify
echo [stub] LogStealthTuningSchemaGen: Error: this build compiled the empty bootstrap table ^(STEALTH_TUNING_SCHEMA_BOOTSTRAP=1 left set^)
echo [stub] LogStealthTuningSchemaGen: Error: STEALTH TUNING SCHEMA VERIFY: FAIL bootstrap build
exit /b 0
