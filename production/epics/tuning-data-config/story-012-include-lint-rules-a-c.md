# Story 012: Include lint rules (a)-(c): header placement, guard first, view must not include server

> **Epic**: Tuning Data & Config
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0007 section 3; traced to architecture.md bootstrap order / epic Exit A) and `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`; the lint keeps client-safe `View/` headers free of server headers, supporting the compile-time client/server face split; GDD CR-10 / AC-28)

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: The include boundary has three enforcement layers; the second is an include lint that runs in CI and pre-commit and fails when a header sits outside `View/`/`Server/` in a `Public/` folder, a `Public/Server/` header does not include the guard first, or a `Public/View/` header includes a `Server/` header.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The lint is a text script with no engine API. It relies on UBT exposing every dependency's `Public/` folder to dependents (the reason a module line alone is not a boundary). The wrapper test uses `FPlatformProcess` (pre-cutoff) and `EAutomationTestFlags`. No unverified engine claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Every header in a `Public/` folder sits under `View/` or `Server/`, except the module header and `StealthServerOnlyGuard.h`; every `Public/Server/` header includes `StealthServerOnlyGuard.h` first (source: ADR-0007)
- Forbidden: Never let a `Public/View/` header include a `Server/` header; never put a header in a module's root `Public/` folder other than the module header or guard (source: ADR-0007)
- Required: The include lint (rules a-j) runs in CI and pre-commit; lints emit observations only, pass/fail lives in the gate script (source: ADR-0007, ADR-0008)
- Required: Every lint gate includes a negative control the suite detects (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), as revised in `tdc-plan-amendments.md`:*

- [x] per G5/G6, the lint reports with file:line (a) a header directly in a `Public/` folder outside `View/`/`Server/` (module header and StealthServerOnlyGuard.h excepted), (b) a `Public/Server/` header whose first include is not StealthServerOnlyGuard.h, (c) a `Public/View/` header that includes a `Server/` header; zero findings on the real tree; a seeded fixture for each of a, b, c is reported.

---

## Implementation Notes

*Derived from ADR-0007 sections 2-3 and Key Interfaces; ADR-0008 sections 1 and 3; amendments G2, G3, G5, G6:*

- **Script** `tools/ci/lint/include-lint-abc.sh`, built on the story 011 emitter and gate. One observation per finding as `file:line rule message`; use the ADR-0007 section 3 rule letter (`a`, `b`, `c`) as the rule id so story 016 can assert one finding per letter. Accept the repository root as an argument (default: the real repo root) so story 016 can aim the same script at its fixture tree.
- **Scan scope (G5)**: `Source/StealthFoundation`, `StealthGameplay`, `StealthPresentation`, `StealthGameEditor`, `StealthTestHarness`. The prototype module `Source/StealthGame` has public headers that predate ADR-0007: list them in `tools/ci/lint/prototype-exempt.txt` (one repo-relative path per line; this story creates it from the headers currently in `Source/StealthGame/Public/`), and skip listed headers for rules (a) and (b). A new prototype header with no entry is flagged. Note the wording tension: G5's sentence says the prototype is scanned only for rules (e) and (f), while the amended spec for this story requires the exempt list to suppress (a)/(b) on prototype headers and to flag unlisted ones; implement the spec (the module is scanned for (a)/(b) with the exemptions).
- **G3 for the exempt list**: it may only shrink. The gate compares the working-tree file with its content at `HEAD` when run from the pre-commit hook, and at the push's before-SHA (`github.event.before`, else `HEAD~1`) in CI; any entry absent at the base fails; creating the file at the base-less first commit is exempt. This story is the first user of G3: where the comparison helper lives is open (stories 034, 036 and 094 reuse it), so put it where story 011's library can serve them.
- **Rule semantics**: (a) any `*.h` directly in a module's `Public/` folder other than the module header (`Stealth<Module>Module.h`) and `StealthServerOnlyGuard.h`; (b) for every header under `Public/Server/`, the first `#include` must be `StealthServerOnlyGuard.h`, where comments and `#pragma once` before it are allowed (spec edge case); (c) a header under `Public/View/` with any include whose path contains `Server/`.
- **Zero findings on the real tree**: at this point the tree has the story 001 placeholders and possibly the headers of stories 007/008; confirm the guard is first in each. `.gitkeep` placeholders are not headers.
- **Seeded fixtures (G6)**: one per rule under `tools/ci/lint/selftest/` (header in `Public/` root; Server header with the guard not first; View header including a Server header), plus a clean control for each. Each seeded fixture must be reported as `file:line` with its rule id. Spec edge cases: the guard header after a comment or `#pragma once` is clean; modules not in scope are ignored. Story 016 later composes the combined ten-rule fixture tree.
- **G2**: ship the selftest above under `tools/ci/lint/selftest/` (a clean pass AND a seeded violation detected) and the wrapper automation test `StealthGame.Build.IncludeLintAbc` (class `FBuildIncludeLintAbcTest`, `Source/StealthFoundation/Private/Tests/Build/IncludeLintAbcTest.cpp`; name proposed) that runs the script through `FPlatformProcess::ExecProcess` and asserts exit code and observations. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- Prototype: `Source/StealthGame/Public/*.h` stay as they are; the exempt list is the only concession (ADR-0007 section 8: each class leaves the list when it is promoted by rewrite).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: the emitter and gate this script uses
- `production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md`: rules (d)-(f)
- `production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`: rules (g)-(h)
- `production/epics/tuning-data-config/story-015-include-lint-rules-i-j.md`: rules (i)-(j)
- `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`: the combined seeded fixture tree and the watched-to-fail control
- `production/epics/tuning-data-config/story-069-no-client-asset-reference.md`: the tuning-asset reference check for client code

---

## Files

- `tools/ci/lint/include-lint-abc.sh`
- `tools/ci/lint/prototype-exempt.txt`
- `tools/ci/lint/selftest/` (fixtures for a, b, c with clean controls, and the selftest script)
- `Source/StealthFoundation/Private/Tests/Build/IncludeLintAbcTest.cpp` (G2 wrapper)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: the lint reports rules (a), (b) and (c) with file:line, seeded fixtures are reported, and the real tree has zero findings
  - Given: scan scope per G5; prototype Public headers in `tools/ci/lint/prototype-exempt.txt`; seeded fixtures for a, b, c
  - When: include-lint-abc runs on the fixtures and on the real tree
  - Then: each fixture is reported as `file:line` with its rule id; the module header and `StealthServerOnlyGuard.h` are not flagged; exempt prototype headers are not flagged by (a)/(b); the real tree reports zero findings
  - Edge cases: the exempt list may only shrink (G3); a header added to the prototype without an exempt entry is flagged

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/lint/selftest/` (run by the T0 gate step) and the G2 wrapper `StealthGame.Build.IncludeLintAbc` (`Source/StealthFoundation/Private/Tests/Build/IncludeLintAbcTest.cpp`) must exist and pass (qa.level is full, so tests are required)

**Status**: [x] Created and passing (`tools/ci/lint/selftest/run-include-abc-selftest.sh` 12/12 cases; `StealthGame.Build.IncludeLintAbc`, 2026-10-25)

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`, `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`
- Unlocks: `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`, `production/epics/tuning-data-config/story-069-no-client-asset-reference.md`
- Open verification claims from the plan's Note lines: none (no Note line on 012). Open design points: the G3 helper location and the G5 wording tension about the prototype module (see Implementation Notes).

---

## Completion Notes
**Completed**: 2026-10-25
**Criteria**: 1/1 passing (AC-1: seeded (a) `Source/StealthGameplay/Public/StealthLooseHeader.h:1 a` and a non-View/Server subfolder header; seeded (b) guard not first `...StealthGuardNotFirst.h:3 b` and no include at all; seeded (c) `...View/StealthLeakyView.h:4 c`; each clean control (module header, guard in Foundation, out-of-scope module, guard after a comment/`#pragma once`, nested `Server/Testing/`, commented-out and `GameServerUtils.h` includes in a View header) gives zero findings; exempt prototype header not flagged, unlisted prototype header flagged (a); real tree `GATE: PASS (29 file(s) scanned, 0 findings)`, watched to fail with 17 findings when the exempt list is removed; G3: unchanged list passes, removal allowed, growth reported as `prototype-exempt.txt:3 shrink-only`, unresolvable base reported)
**Decisions**: rule ids `a`/`b`/`c` plus `shrink-only`; (a) also flags headers in `Public/` subfolders other than `View/`/`Server/` (ADR-0007 §3 wording); the guard is excepted only in StealthFoundation; (c) matches an include path whose first or any later segment is `Server/`; comments (line and block) are stripped before reading includes. G5 tension resolved per the spec: `Source/StealthGame` is scanned for (a)/(b) with the exempt list. G3 helper `lint_shrink_only` lives in `emit.sh` (base: argument, else `LINT_BASE_REF`, else `HEAD`; CI passes the before-SHA or `HEAD~1`); a file absent at the base is exempt (first commit).
**Deviations**: `prototype-exempt.txt` also lists `Source/StealthGameEditor/Public/StealthAssetToolset.h` and `StealthPlayToolset.h` (pre-ADR-0007 MCP toolset headers in a G5 module; the story named only the `Source/StealthGame/Public/` headers). Decision logged; they leave the list when the toolsets move to `Private/` or `Server/`.
**Test Evidence**: `production/qa/evidence/story-012-include-lint-rules-a-c/test-run.txt` (build `Result: Succeeded`; 24/24 `StealthGame.` tests Success), `selftest.txt`, `stamp.txt`
**Run result**: N/A — CI tooling; nothing player-visible
**Reviewed by**: focused code review (self-applied, unattended run): no further defects found
