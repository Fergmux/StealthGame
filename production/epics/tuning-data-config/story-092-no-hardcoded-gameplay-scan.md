# Story 092: AC-25 NoHardcodedGameplay (CI literal scan)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-001`; GDD AC-25 (CR-1)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Every gameplay number must be a field of a `_Public` or `_Server` tuning sub-struct, and a hardcoded gameplay literal is a defect (CR-1). AC-25 enforces this with a CI literal scan that has a reasoned allowlist.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API is used: the scan is a T0 text script (ADR-0008 s1, `tools/ci/lint/`), runs without the engine and never depends on the MCP toolsets or Python editor scripting (ADR-0008). If implemented in Python it is a build-time tool only and runs with `python -I`.

**Control Manifest Rules (this layer)**:
- Required: Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct; a hardcoded gameplay literal is a defect — source: ADR-0004
- Required: Lints and analysis scripts emit observations only; pass/fail lives in the gate script — source: ADR-0008
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-25), as amended (amendment 092 REVISED and AMENDMENTS 2):*

- [ ] **AC-25 (CR-1)**: the scan covers `Source/StealthGameplay` and `Source/StealthFoundation` (excluding `Private/Tests/`, `*.generated.inl` and `StealthTuningConstants.h`; the prototype module is exempt via the G5 list), flags every integer or float literal other than 0, 1, -1 (and `INDEX_NONE`) in gameplay timing, range or count code that is not listed in `tools/ci/lint/gameplay-literal-allowlist.txt` (each entry `file:literal | reason`), reports NOT ASSESSED (nonzero) when zero source files were scanned (so an empty module is never a pass), and detects a seeded fixture literal; `0.0f`, `1.0f` and `-1.0f` count as exempt literals. (`StealthGame.Tuning.NoHardcodedGameplay`, CI script)

---

## Implementation Notes

*Derived from ADR-0004 Implementation Guideline 1, ADR-0008 sections 1 and 3, and amendments G2, G5:*

- Scan script: `tools/ci/lint/gameplay-literal-scan.sh` or `.py` (the plan gives `gameplay-literal-scan.*`; the extension is open). It emits one `file:line rule message` observation per non-allowlisted literal and per malformed allowlist entry. It never decides pass/fail. The gate script from story 011 (`tools/ci/lint/gate.sh`) decides: exit 0 with no findings, nonzero with any, NOT ASSESSED nonzero when no input files were scanned.
- Scope: `Source/StealthGameplay` and `Source/StealthFoundation`, skipping `Private/Tests/`, `*.generated.inl` and `StealthTuningConstants.h`. The prototype `Source/StealthGame` is outside the scan scope; G5 also lists its existing Public headers in `tools/ci/lint/prototype-exempt.txt`. The plan does not say which story creates that file (it is not in story 012's file list), so check whether it exists before relying on it; this story needs no entry from it.
- Exempt literals: `0`, `1`, `-1`, `INDEX_NONE`, and the float spellings `0.0f`, `1.0f`, `-1.0f`. Every other integer or float literal that appears in scanned code outside comments and string literals is a finding unless allowlisted. How the scan treats array indices, bit shifts, hex masks and template arguments is not specified in the inputs and is open. Default to flagging them and put legitimate ones on the allowlist with a reason, so the allowlist stays the single source of exceptions.
- Allowlist: `tools/ci/lint/gameplay-literal-allowlist.txt`, one entry per line, `file:literal | reason`. The reason must be one of the GDD categories (engine constant, unit conversion, test) or a stated equivalent; an entry with an empty reason is a finding. The FNV-1a-64 hasher constants in `Source/StealthFoundation/Private/Tuning/StealthTuningHasher.cpp` are not exempt by exclusion and need allowlist entries (engine/algorithm constants) once story 045 lands.
- Empty-module behaviour: `Source/StealthGameplay` may contain no gameplay code yet (story 002 creates it with `Core/` and `Features/` folders). A scan that finds zero source files must report NOT ASSESSED nonzero. A scan that finds files only in `StealthFoundation` is a valid scan.
- Negative control (ADR-0008 s3): a seeded fixture file with one non-allowlisted literal is detected, and the selftest passes only if the control is detected.
- G2 for this CI-script story: (a) a selftest under `tools/ci/lint/selftest/` run by the T0 gate step: a clean fixture passes, a seeded literal is reported by file:line, an allowlist entry without a reason fails, an empty input set is NOT ASSESSED; (b) an automation wrapper `StealthGame.Build.NoHardcodedGameplayScan` (wrapper name chosen here; the GDD label `StealthGame.Tuning.NoHardcodedGameplay` names the CI script) in `Source/StealthFoundation/Private/Tests/Build/NoHardcodedGameplayScanTest.cpp` that runs the script through `FPlatformProcess` and asserts exit code and observations. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- Prototype: the prototype module's hardcoded literals (`Source/StealthGame`) are not touched or scanned here; they go away when each prototype class is replaced (ADR-0007 s8).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: the shared emitter and gate script this scan plugs into.
- `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`: creates the `StealthGameplay` module the scan covers.
- `production/epics/tuning-data-config/story-038-schema-row-types.md`: `StealthTuningConstants.h` (excluded from the scan).
- `production/epics/tuning-data-config/story-091-no-variant-keys.md`: key-name rules, a different AC-15 check.
- `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`: removal of the prototype tuning fields.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- tools/ci/lint/gameplay-literal-scan.sh (or `.py`)
- tools/ci/lint/gameplay-literal-allowlist.txt
- tools/ci/lint/selftest/gameplay-literal-selftest.sh (plus clean and seeded fixtures)
- Source/StealthFoundation/Private/Tests/Build/NoHardcodedGameplayScanTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-25 literal scan
  - Given: `Source/StealthGameplay` and `Source/StealthFoundation` excluding `Private/Tests/`, `*.generated.inl`, `StealthTuningConstants.h`; the prototype exempt via the G5 list; allowlist entries `file:literal | reason`.
  - When: the scan runs on the real tree, on a seeded fixture literal, and on an empty input set.
  - Then: it flags any int or float literal other than 0, 1, -1, `INDEX_NONE` that is not allowlisted; an entry without a reason fails; the seeded literal is detected; zero files scanned is NOT ASSESSED (nonzero).
  - Edge cases: `0.0f` and `1.0f` spellings of the exempt values are exempt; hasher constants need allowlist entries.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Logic
**Required evidence**:
- Logic: the automation wrapper `StealthGame.Build.NoHardcodedGameplayScan` in `Source/StealthFoundation/Private/Tests/Build/NoHardcodedGameplayScanTest.cpp` must exist and pass.
- Script story (G2): the selftest under `tools/ci/lint/selftest/` (clean pass and seeded violation detected) must run in the T0 gate step.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`, `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`
- Unlocks: None
- Open verification claims: how the scan treats indices, masks and template arguments is open; the creator of `tools/ci/lint/prototype-exempt.txt` is not named in the plan.
