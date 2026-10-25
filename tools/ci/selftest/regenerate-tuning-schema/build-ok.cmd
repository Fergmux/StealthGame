@echo off
if defined STEALTH_TUNING_SCHEMA_BOOTSTRAP (echo stub build: bootstrap=[%STEALTH_TUNING_SCHEMA_BOOTSTRAP%]) else (echo stub build: bootstrap=[])
echo stub build: Result: Succeeded
exit /b 0
