# Story 030: Mirror the missing owner keys into entities.yaml (TD-REG-1 first task)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-002` (text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD CR-15.7 (orchestrator mirror) and OQ-15 (TD-REG-1)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

TD-REG-1 (`docs/tech-debt-register.md`, opened 2026-10-25) records that owner keys outside the Level Socket Contract are not mirrored in `design/registry/entities.yaml`; TDC OQ-15 defers the fix to this validator epic: the AC-3b doc lint is built here and registers and validates the missing keys as its first task. CR-15.7 says each owner's registered knobs are mirrored into `design/registry/entities.yaml` constants with `source:` set to the owner GDD, so that the AC-3b clause "every owned key matches a registry constant" can hold. This story does the mirroring from the parser records of story 029; story 033 then lints it, and story 037 closes the register row once the lint is clean.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The owner GDD tables, the schema and the registry are kept consistent by a design-doc lint (AC-3b); the lint reports and never edits a value or a GDD, so the registry fill is a separate, reviewable data change.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No engine API: a data change to a YAML registry made by a build-time script. Python only as a build-time tool, run with `python -I`. Nothing here is an unverified engine claim.

**Control Manifest Rules (this layer)**:
- Required: lints and analysis scripts emit observations only; pass/fail lives in the gate script (the mirror script reports what it added; story 033 judges) — source: ADR-0008
- Required: every lint gate includes a negative control the suite detects (the selftest seeds a missing key and an altered constant) — source: ADR-0008
- Forbidden: never make a gate depend on the MCP toolsets or Python editor scripting — source: ADR-0008

---

## Acceptance Criteria

*From the story plan (bootstrap-style registry task, GDD OQ-15), scoped to this story:*

- [ ] Every owner key found by the parser that has no constant in `design/registry/entities.yaml` is added as a constant with the owner table's default, ranges, audience, latch and `source:` GDD (about 130 keys; no key renamed, no existing constant altered), the file still parses with `yaml.safe_load`, and the constants list has no duplicate names.

---

## Implementation Notes

*Derived from CR-15.7, OQ-15, TD-REG-1 and the current shape of the registry:*

- **Files.** `design/registry/entities.yaml` (data change) and `tools/ci/lint/registry_mirror.py` (the plan's `TOOLS/lint/`), which reads the story 029 parser records and appends missing constants. Run with `python -I`.
- **Constant shape (observed in the file).** Entries under the top-level `constants:` list carry `name`, `status`, `source`, `referenced_by` (a block list), `value`, `unit`, `hard_range`, `safe_range`, `audience`, `latch`, `note`, `added`, `revised`. `value` is the default; the two range fields are free-text strings (for example `"4,000-10,000"`). New entries follow the same field names. The file's header rules apply: never delete entries, `source:` is the owner GDD, do not create duplicates.
- **Additive only.** Existing constants must stay byte-identical: append new entries, do not re-serialise the file (a round trip through a YAML dumper would reformat the existing text). The mirror is idempotent: a second run adds nothing.
- **Count is observation-derived.** The plan and TD-REG-1/OQ-15 say about 130 keys; the TDC GDD's own Registered Owner Keys prose says about 110 first-named keys across Camera, Crowd, Generator, SCL, IVP, Concealment and TDC remain after later mirroring passes. These figures disagree, so the number actually added is whatever the parser finds missing, and this story reports the final number rather than targeting 130.
- **Do not invent.** Defaults, ranges, audience and latch come from the owner table verbatim. Crowd unkeyed constants (OQ-14) are deliberately not mirrored and must not get invented keys; the registry row for a CR-14 user setting such as `TeammateMarkerLargeMult` is a settings default, not a preset field, and is already present. Sentinel keys (`EndedAutoResetSec`, `SessionHeartbeatIntervalSec`) keep the existing sentinel handling.
- **Register upkeep.** Closing the TD-REG-1 row is story 037, not this story. A change to the TDC GDD (for example the count prose) is a change request to its owner, not an edit made here.
- **Selftest and wrapper (G2).** The mirror script ships a selftest under `tools/ci/lint/selftest/` (a fixture registry plus fixture records: missing key added with all fields, existing constant untouched, duplicate not created, second run adds nothing) and a wrapper `StealthGame.Build.RegistryMirror` in `Source/StealthFoundation/Private/Tests/Build/RegistryMirrorTest.cpp` running it through `FPlatformProcess`. Foundation (story 001) is not a declared dependency of this story: open.
- **Evidence method.** The QA spec is a smoke check: after the run, re-run the parser and compare every record with the registry, diff the old file against the new one to prove the old text is intact, and `yaml.safe_load` the result.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-029-knob-table-parser.md`: the records this story consumes
- `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`: the lint that compares each owned key with its constant, field by field
- `production/epics/tuning-data-config/story-037-close-td-reg-1.md`: resolving the TD-REG-1 row and noting OQ-15 as answered
- `production/epics/tuning-data-config/story-034-doc-lint-schema-row-match.md`: schema-row matching (the schema rows for non-TDC owners arrive with their epics)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `design/registry/entities.yaml`
- `tools/ci/lint/registry_mirror.py`
- `tools/ci/lint/selftest/` (registry-mirror selftest)
- `Source/StealthFoundation/Private/Tests/Build/RegistryMirrorTest.cpp` (G2 wrapper `StealthGame.Build.RegistryMirror`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: every owner key has a registry constant, existing constants are untouched, the file parses and has no duplicates
  - Setup: parser records and `design/registry/entities.yaml`
  - Verify: every parsed owner key has a constant (default, ranges, audience, latch, `source:` GDD); pre-existing constants are byte-identical; `yaml.safe_load` succeeds; there are no duplicate constant names; about 130 keys added
  - Pass condition: zero missing keys, zero altered or renamed constants, zero duplicates

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass recorded in `production/qa/smoke-*.md` (parser re-run shows zero missing keys, byte-identical old constants, `yaml.safe_load` succeeds, zero duplicates), plus (G2) the mirror selftest under `tools/ci/lint/selftest/` and the wrapper `StealthGame.Build.RegistryMirror`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-029-knob-table-parser.md`
- Unlocks: `production/epics/tuning-data-config/story-033-doc-lint-registry-match.md`, `production/epics/tuning-data-config/story-037-close-td-reg-1.md`
- Open verification claims: the plan Note ties this to TD-REG-1 and OQ-15 (a tracked tech debt item, not an engine claim). Open: the "about 130 keys" figure conflicts with the GDD's "about 110" and is replaced by the parser's observed count; the Foundation-module dependency of the G2 wrapper.
