@echo off
if defined STEALTH_TUNING_SCHEMA_BOOTSTRAP (
  echo stub build: bootstrap=[%STEALTH_TUNING_SCHEMA_BOOTSTRAP%]
  echo stub build: Result: Succeeded
  exit /b 0
)
echo stub build: bootstrap=[]
echo stub build: error C2039: stale row
echo stub build: Result: Failed
exit /b 6
