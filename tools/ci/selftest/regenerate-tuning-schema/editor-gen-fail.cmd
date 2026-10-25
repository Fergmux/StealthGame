@echo off
rem Stub UnrealEditor-Cmd for the regenerate-tuning-schema selftest (story tdc-042).
setlocal EnableDelayedExpansion
set MODE=gen
for %%A in (%*) do (
  set "ARG=%%~A"
  if not "!ARG:SchemaVerify=!"=="!ARG!" set MODE=verify
)
if "!MODE!"=="verify" goto verify
echo [stub] LogStealthTuningSchemaGen: Error: TargetRoundMins: meta Latch must be Round, Action, Use or Session
echo [stub] LogStealthTuningSchemaGen: Error: STEALTH TUNING SCHEMA GEN: FAIL (1 error^(s^))
exit /b 0
:verify
echo [stub] LogStealthTuningSchemaGen: Display: STEALTH TUNING SCHEMA VERIFY: PASS rows=8 bootstrap=0 fingerprint=0x0
exit /b 0
