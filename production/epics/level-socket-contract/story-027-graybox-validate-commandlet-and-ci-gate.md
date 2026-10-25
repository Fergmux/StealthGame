# Story 027: Graybox Validate button, commandlet parity and CI gate

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; <= 33 ms slices)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The editor Validate button and the validation commandlet call one predicate function and bake the markers; the graybox is also validated in CI (`StealthGame.Layout.Graybox.Validates`) so a failing graybox never ships.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The commandlet runs under `UnrealEditor-Cmd.exe` (`commands.test` in `project.yaml`); criteria 2 and 3 need CI and the editor binary and are DEFERRED until they are available.

**Why this story**: It makes the hand-built map's validity a build gate rather than a round-time surprise. Read first: R11 item 4, R8 item 4, AC-28.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every V-* predicate exists once against `ILayoutMeasureBackend`; graybox marker classes are editor-only (ADR-0014)
- Forbidden: Never fork predicate logic between the grid and world backends (ADR-0014)
- Guardrail: graybox navmesh build target <= 1 s (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-28 (EditorCommandletParity, CI gate), revised after QL-STORY-READY (criteria 2-3 reclassified), scoped to this story:*

- [ ] GIVEN the editor Validate button and the validation commandlet, THEN both call one predicate function and their reports are byte-identical (`Graybox.EditorCommandletParity`)
- [ ] GIVEN a PR with a deliberately broken marker, THEN the CI required status check for `StealthGame.Layout.Graybox.Validates` is red and merge is blocked (smoke check recorded in `production/qa/smoke-*.md`; Config/Data, DEFERRED until CI exists)
- [ ] GIVEN a baked asset whose `GrayboxLayoutHash` does not match the map content, THEN the commandlet exits non-zero naming the stale hash (needs the editor commandlet binary; DEFERRED)

---

## Implementation Notes

- One predicate entry point (`ValidateLayoutCandidate`) called by both the editor button (`StealthGameEditor`) and `UStealthLayoutValidateCommandlet`; the report format is stable text with predicate ids in canonical order so the two are byte-identical (report line ordering is the usual defect).
- The commandlet bakes `UStealthLayoutSocketData` (story 026) and recomputes `GrayboxLayoutHash`; a mismatched hash exits non-zero naming the stale hash; a matching hash exits 0.
- CI runs `StealthGame.Layout.Graybox.Validates` (`.github/workflows/tests.yml`, self-hosted Windows runner); the required-status-check setting is a repository setting, recorded in a smoke check.
- The `Graybox.Validates` test itself is delivered by story 028 once a real map exists; until then it runs against the synthetic fixture.

---

## Out of Scope

- Story 026: the markers and bake; story 028: authoring the map and `Graybox.Validates` passing; story 039: cook leak scan

---

## Files

- `Source/StealthGameEditor/Private/LayoutValidateCommandlet.cpp`, `LayoutValidateCommandlet.h`
- `Source/StealthGameEditor/Private/LayoutValidateButton.cpp`
- `Source/StealthGameEditor/Private/Tests/CommandletParityTest.cpp`
- `.github/workflows/tests.yml` (required check)
- `production/qa/smoke-lsc-ci-gate.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Graybox.EditorCommandletParity`
  - Given: a graybox with a failing marker and a passing one.
  - When: the editor Validate and the commandlet run.
  - Then: both call one predicate function and the reports are byte-identical.
  - Edge cases: report line ordering.
- **AC-2** CI gate (Config/Data, DEFERRED until CI exists)
  - Setup: a PR with a deliberately broken marker.
  - Verify: the required status check for `StealthGame.Layout.Graybox.Validates`.
  - Pass condition: the check is red and merge is blocked; recorded in `production/qa/smoke-*.md`.
- **AC-3** stale hash (Integration, DEFERRED until the editor binary is available)
  - Given: a baked asset whose `GrayboxLayoutHash` mismatches the map.
  - When: the commandlet runs.
  - Then: exit code non-zero and the output names the stale hash.
  - Edge cases: matching hash exits 0.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameEditor/Private/Tests/CommandletParityTest.cpp` — `StealthGame.Layout.Graybox.EditorCommandletParity` must exist and pass; the CI gate is a smoke check (`production/qa/smoke-lsc-ci-gate.md`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-026-graybox-provider-markers-and-bake.md`
- Cross-epic: `production/epics/tuning-data-config/EPIC.md` (CI workflow)
- Unlocks: `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
