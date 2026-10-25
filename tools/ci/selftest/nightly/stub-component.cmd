@echo off
rem Stub nightly component for the nightly.ps1 dry run (story tdc-023). Usage: stub-component.cmd <component-id> <verdict>
rem Verdicts: pass -> exit 0 | fail -> prints a failing observation, exit 1 | na -> exit 2 | anything else (crash) -> exit 7.
echo STUB component %1 verdict=%2
if /i "%2"=="pass" exit /b 0
if /i "%2"=="fail" (
  echo OBSERVATION %1 seeded failing observation
  exit /b 1
)
if /i "%2"=="na" (
  echo NOT ASSESSED %1: seeded
  exit /b 2
)
echo STUB component %1 crashed
exit /b 7
