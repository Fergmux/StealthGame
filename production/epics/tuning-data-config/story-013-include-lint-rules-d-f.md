# Story 013: Include lint rules (d)-(f): core/features direction, foundation upward includes, unique header names

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0007 section 3; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: The include lint also fails when a file under `StealthGameplay/*/Core/` includes a `Features/` path, when `StealthFoundation` includes from `StealthGameplay`/`StealthPresentation`/`StealthGame`, or when two project headers share a file name, because UBT adds every dependency's `Public/` folder to the include path and would silently resolve a duplicate name to whichever module comes first.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The lint is a text script with no engine API. Rule (e) is also refused by UBT itself (no dependency from Foundation upward); the lint gives the earlier, clearer message. The wrapper test uses `FPlatformProcess` and `EAutomationTestFlags`. No unverified engine claim is touched.

**Control Manifest Rules (this layer)**:
- Forbidden: Never let a file under `StealthGameplay/*/Core/` include a `Features/` path; never let a lower layer name a feature type (source: ADR-0007)
- Required: Project header file names are unique across all modules (`Stealth` prefix plus system name) (source: ADR-0007)
- Required: The include lint (rules a-j) runs in CI and pre-commit (source: ADR-0007)
- Required: Lints emit observations only; every lint gate includes a negative control the suite detects (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), as revised in `tdc-plan-amendments.md`:*

- [ ] per G6, the lint reports (d) a file under `StealthGameplay/*/Core/` including a `Features/` path, (e) StealthFoundation including anything from StealthGameplay/StealthPresentation/StealthGame, (f) two project headers with the same file name across modules; zero findings on the real tree; a seeded fixture for each of d, e, f is reported.

---

## Implementation Notes

*Derived from ADR-0007 sections 1-3; ADR-0008 sections 1 and 3; amendments G2, G5, G6:*

- **Script** `tools/ci/lint/include-lint-def.sh` on the story 011 emitter and gate; findings as `file:line rule message` with the rule letter (`d`, `e`, `f`) as the rule id (story 016 asserts one finding per letter); the repository root is an argument (default: the real repo root) so story 016 can aim it at a fixture tree.
- **Scan scope (G5)**: the five production module folders; the prototype module `Source/StealthGame` is scanned for rules (e) and (f) only.
- **Rule (d)**: any file under a `Core/` folder inside `StealthGameplay` (the ADR pattern is `StealthGameplay/*/Core/`, which covers `Core/` source folders and `Public/Server/Core/`, where ADR-0007 section 4 places the consumer-owned interfaces) whose include path contains `Features/`. Direction only: a `Features/` file including `Core/` is allowed (spec edge case).
- **Rule (e)**: any file in `StealthFoundation` that includes something owned by `StealthGameplay`, `StealthPresentation` or `StealthGame`. Because project headers are included without a module prefix (`#include "Server/Foo.h"`), the lint has to resolve an include to its owning module by header name (which is what rule (f) makes unambiguous). The resolution method is an implementation choice, not fixed by the inputs.
- **Rule (f)**: collect every project header's file name across the five modules plus the prototype module and report any name that appears in more than one module. Spec edge case "same name inside one module": the AC says across modules, so decide, document and test one behaviour for a duplicate inside a single module (different subfolders) rather than leaving it unspecified.
- **Seeded fixtures (G6)**: one per rule under `tools/ci/lint/selftest/` (a Core file including a Features path; a Foundation file including a Gameplay/Presentation/StealthGame header; two headers with one file name in different modules), each reported as `file:line`, plus clean controls (a Features file including Core).
- **G2**: selftest as above (a clean pass AND a seeded violation detected) and the wrapper `StealthGame.Build.IncludeLintDef` (class `FBuildIncludeLintDefTest`, `Source/StealthFoundation/Private/Tests/Build/IncludeLintDefTest.cpp`; name proposed) running the script through `FPlatformProcess::ExecProcess` and asserting exit code and observations. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- Prototype: scanned only by (e) and (f); its headers need no exemption for these rules as long as their file names are unique.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: emitter and gate
- `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`: rules (a)-(c)
- `production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`: rules (g)-(h)
- `production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md`: rules (i)-(j)
- `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`: the combined seeded tree

---

## Files


- `tools/ci/lint/include-lint-def.sh`
- `tools/ci/lint/emit.sh` (added `lint_includes`, `lint_module_files`, `lint_is_exempt`, `lint_cpp_awk`, `LINT_DIR`; shared by the d-j rule scripts)
- `tools/ci/lint/selftest/lib.sh`, `tools/ci/lint/selftest/run-include-def-selftest.sh`, `tools/ci/lint/selftest/include-def/fixtures/**` (d/e/f seeded and clean fixtures)
- `Source/StealthFoundation/Private/Tests/Build/IncludeLintDefTest.cpp` (G2 wrapper), `Source/StealthFoundation/Private/Tests/Build/StealthBuildFileTestUtils.h` (added `RunLintSelftestSuite`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: the lint reports rules (d), (e) and (f), seeded fixtures are reported, and the real tree has zero findings
  - Given: seeded fixtures: a Core/ file including a Features/ path; StealthFoundation including from StealthGameplay/StealthPresentation/StealthGame; two project headers sharing a file name across modules
  - When: include-lint-def runs on the fixtures and the real tree (the prototype module is scanned for (e) and (f) only)
  - Then: each fixture is reported as `file:line` with rule d, e or f; the real tree reports zero findings
  - Edge cases: a Features/ file including Core/ is allowed; the same name inside one module

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/lint/selftest/` (run by the T0 gate step) and the G2 wrapper `StealthGame.Build.IncludeLintDef` (`Source/StealthFoundation/Private/Tests/Build/IncludeLintDefTest.cpp`) must exist and pass (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`tools/ci/lint/selftest/run-include-def-selftest.sh` 9/9 cases; `StealthGame.Build.IncludeLintDef`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`
- Unlocks: `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`
- Sequencing note (not a plan dependency): the G2 wrapper test needs the StealthFoundation module of `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, and the real-tree scan is only meaningful once the Gameplay folders of story 002 exist.
- Open verification claims from the plan's Note lines: none (no Note line on 013). Open design point: behaviour for a duplicate name inside one module.

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: seeded (d) `Source/StealthGameplay/Core/StealthCoreBad.cpp:3 d` (include text) and `.../Private/Core/StealthResolvedBad.cpp:1 d` (resolved by header name to a Features header); seeded (e) four upward includes (Gameplay, Presentation, prototype StealthGame by resolution, and a module-named path) each `:N e`; seeded (f) `StealthDupName.h` and a case-only duplicate each `:1 f`; clean controls give zero findings (Features including Core, commented includes, a `Features` substring, `Public/Server/Core/`, a same-name pair inside ONE module, the prototype exempt list); real tree `GATE: PASS (57 file(s) scanned, 0 findings)`)
**Decisions**: rule ids `d`/`e`/`f`; (d) covers any `Core/` path segment under StealthGameplay (Core/, Private/Core/, Public/View/Core/, Public/Server/Core/); an include counts as Features when its text has a `Features/` segment or it resolves by header name to a StealthGameplay header under `Features/`; (e) resolves includes by header name with a path-suffix match, plus module-named path segments; (f) compares every `.h` under the five modules and the prototype case-insensitively and reports ONE finding per extra module (at that module's first header); a duplicate name inside one module is NOT reported (AC says across modules; documented in the script header and covered by the f-seeded fixture); headers on `prototype-exempt.txt` are left out of (f) so the prototype's StealthMovementComponent.h may share a name with a future production header until the prototype is deleted (f-prototype-exempt-allowed / f-prototype-unlisted-reported). Logged in the decision log.
**Deviations**: none
**Test Evidence**: `production/qa/evidence/story-013-016-include-lint-rules-d-j/test-run.txt` (build `Result: Succeeded`; 34/34 `StealthGame.` tests Success), `selftest-def.txt`, `stamp.txt`
**Run result**: N/A — CI tooling; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no further defects found
