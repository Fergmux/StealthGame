# Story 039: Packaged-client leak scan — markers and role strings

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-008`
*(At write time: TR-lsc-008 = Pillar-1 placement constraints (no area type for spawns/objectives))*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Marker classes are editor-only so neither class nor instances are cooked; the baked layout-data asset ships as an opaque blob with no role tokens; strict exclusion from client packages arrives with the Dedicated Server (ADR-0024). A cook test (package the graybox map in a Development client, scan its imports for marker classes) is part of AC-38's suite before the graybox epic closes.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Whether a cooked map referencing editor-module marker classes strips cleanly (no load warning, no dangling import) is [U]. The scan needs a packaged or cooked build, so every criterion is DEFERRED until a Development client cook is available; AC-38's own gate is the milestone gate. (QL-STORY-READY noted GDD AC-38 was "blocked on the R11.2 ADR": that ADR-0014 section 6 is Accepted, so the block is lifted.)

**Why this story**: A role word in a shipped asset name would hand modified clients the assignment (R10 rule 2). Read first: R10 item 2, R11 item 2, AC-38; ADR-0014 section 6.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Graybox marker classes editor-only; baked asset an opaque versioned byte blob (ADR-0014)
- Forbidden: Never cook graybox markers as runtime actors or a server-only data layer (ADR-0014)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-38 (revised after QL-STORY-READY: tokenisation and scope defined), scoped to this story; all criteria DEFERRED to a packaged/cooked build:*

- [ ] GIVEN a packaged client build, THEN it contains no marker class or marker actor and the baked layout-data asset is present only as an opaque blob with no role tokens in names or tagged properties (`Leak.NoMarkersInClientCook`)
- [ ] GIVEN every asset, actor, nav-area and collision-profile name under `/Game/**` and the project's modules (engine content excluded), tokenised on `_`, `/`, `.` and camelCase, THEN none equals a role word (Spawn, Team, Zone, Final, Prelim, Objective, Emergence, Site, Respawn) as a whole case-sensitive token outside the checked-in allowlist; binary symbols are excluded (`Leak.NoRoleStringsInCook`)
- [ ] GIVEN a Development client cook of the graybox map, THEN a package-import scan finds no reference to `StealthGameEditor` marker classes and loading logs no warning or dangling import (`Leak.NoEditorImportsInCook`, proposed)

---

## Implementation Notes

- Run as a post-cook script or commandlet over the staged build; the allowlist is a checked-in file reviewed on change (terms like `ZoneId` in editor-only code are outside the cooked scope).
- Names scanned: asset paths, actor names in the map, nav area class names, collision profile names; binary symbols are out of scope.
- The listen-server reality (blob present in every client package) is accepted under LSC R1's data classes; the opaque-blob check is what limits it.
- Record the cook command and result in `production/qa/evidence/level-socket/cook-scan-*.md`.

---

## Out of Scope

- Story 026: marker/bake implementation; Dedicated Server exclusion (ADR-0024)

---

## Files

- `tools/ci/lsc-cook-leak-scan.ps1` (or commandlet `Source/StealthGameEditor/Private/CookLeakScanCommandlet.cpp`)
- `Source/StealthGameEditor/Private/Tests/CookLeakScanTest.cpp`
- `production/qa/evidence/level-socket/cook-scan-allowlist.txt`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision; DEFERRED to a packaged/cooked build).*

- **AC-1** `Leak.NoMarkersInClientCook`
  - Given: a packaged client build.
  - When: the cooked contents are scanned.
  - Then: no marker class or actor; the baked layout-data asset is present only as an opaque blob with no role tokens in names or tagged properties.
  - Edge cases: listen-server builds follow ADR-0014 section 6.
- **AC-2** `Leak.NoRoleStringsInCook`
  - Given: every name under `/Game/**` and the project modules, tokenised on `_`, `/`, `.` and camelCase.
  - When: matched against the role word list.
  - Then: no token equals Spawn, Team, Zone, Final, Prelim, Objective, Emergence, Site or Respawn (case-sensitive) outside the allowlist; binary symbols excluded.
  - Edge cases: engine content is excluded.
- **AC-3** `Leak.NoEditorImportsInCook` (proposed)
  - Given: a Development client cook of the graybox map.
  - When: a package-import scan and a load run.
  - Then: 0 references to `StealthGameEditor` marker classes; 0 warnings or dangling imports.
  - Edge cases: none.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameEditor/Private/Tests/CookLeakScanTest.cpp` / scan script output — `StealthGame.Layout.Leak.NoMarkersInClientCook`, `.NoRoleStringsInCook`, `.NoEditorImportsInCook` must exist and pass (milestone gate)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-026-graybox-provider-markers-and-bake.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
- Unlocks: None (milestone gate)
