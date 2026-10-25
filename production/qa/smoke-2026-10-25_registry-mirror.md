# Smoke check: registry mirror completion (story tdc-030, TD-REG-1 first task)

**Date**: 2026-10-25
**Story**: `production/epics/tuning-data-config/story-030-registry-mirror-completion.md`
**Result**: PASS (Config/Data smoke check: zero missing keys, old constants byte-identical, file parses, zero duplicates)

## What was run

```
python -I tools/ci/lint/registry_mirror.py --date 2026-10-25 --dry-run   # before: MIRROR missing 124
python -I tools/ci/lint/registry_mirror.py --date 2026-10-25             # MIRROR result: added 124, existing 291 untouched
python -I tools/ci/lint/registry_mirror.py --date 2026-10-25             # second run: MIRROR missing 0, MIRROR result: added 0, existing 415 untouched
```

The parser (`tools/ci/lint/knob_tables.py`, story tdc-029) found 413 owner keys (preset fields) in the 18 GDDs that own keys. 124 had no
constant in `design/registry/entities.yaml`; the plan figure was "about 130" and the TDC GDD prose says "about 110" for the first-named keys,
so the number added is the observed 124 (Camera 32, Shared Character & Locomotion 32, Crowd 31, Procedural City Generator 25, Concealment 4).
The Level Socket Contract keys were already mirrored. Pair rows (`A` / `B`) were added as two constants each.

## Checks (AC-1 of story 030)

| Check | Result |
|---|---|
| Every owned key has a constant (parser re-run against the registry) | 0 missing (`MIRROR missing 0`; the registry-match lint also reports 0 missing constants) |
| Old text intact | `cmp -n <size of the original> original new` was identical right after the mirror run (append-only: the file was never re-serialised). A diff of the original against today's file shows exactly 5 removed lines, all from the separate story tdc-033 fixes (see its smoke notes), and everything else added after the original last line |
| File parses | `yaml.safe_load` (PyYAML 6.0.3, installed outside the repo for this check only): 415 constants, 415 distinct names; `registry.sh validate`: duplicate 0, layout 0, status 0; the lint reader (`registry_io.read_constants`): 415 constants, 0 layout problems |
| No duplicate constant names | 415 names, 415 distinct |
| Idempotent | second run added nothing and left the file identical (selftest case `second-run-leaves-the-file-identical`) |

## Test evidence

- Selftest `tools/ci/lint/selftest/registry_mirror_selftest.sh`: 25 cases (`production/qa/evidence/story-030-registry-mirror-completion/selftest.txt`).
- Wrapper `StealthGame.Build.RegistryMirror` (`Source/StealthFoundation/Private/Tests/Build/RegistryMirrorTest.cpp`).
