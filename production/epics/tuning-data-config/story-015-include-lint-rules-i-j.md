# Story 015: Include lint rules (i)-(j): log arguments and generator random isolation

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: N/A (bootstrap)
**Requirement**: `TR-tdc-???` (bootstrap: no registry entry; governed by ADR-0007 sections 3 and 6; traced to architecture.md bootstrap order / epic Exit A)

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: Event Log content, `DealDigest`, server sub-seeds, bindings and mailbox order never reach a log, message, ensure/check text or trace, so the include lint greps log-macro arguments for Event Log type names and server-secret identifiers (rule i); and no `CityGenerator/` file may include `Public/Server/Random/` (rule j, Generator isolation).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The lint is a text script with no engine API. `UE_LOGF` (5.8) is the logging macro for new code [V `LogMacros.h:199`] and is one of the macros the rule inspects alongside `UE_LOG`. A minidump stack can still capture Event Log fields on the host; ADR-0007 section 6 accepts that residual risk until the Dedicated Server (ADR-0024). No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Forbidden: Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names (source: ADR-0007)
- Forbidden: Never let a file under `CityGenerator/` include `Public/Server/Random/` (source: ADR-0006, ADR-0007)
- Forbidden: Never log, trace, replicate, put in a URL or expose to Blueprint `ServerSecret`, `MatchNonce`, server sub-seeds or stream keys (source: ADR-0006)
- Required: Every lint gate includes a negative control the suite detects; lints emit observations only (source: ADR-0008)

---

## Acceptance Criteria

*Bootstrap criterion (no GDD), as revised in `tdc-plan-amendments.md` and AMENDMENTS 2:*

- [ ] per G6, the lint reports (i) a log/trace macro argument (UE_LOG, UE_LOGF, ensureMsgf, checkf, ClientMessage, TRACE_*, CSV_*, SCOPE_CYCLE_COUNTER) that names any identifier in `TOOLS/lint/log-forbidden-identifiers.txt` (seeded with: `FEventLog` and `FRoundEventLog` prefixes, `DealDigest`, `ServerSubSeed`, `FSecretStream`, `MailboxOrder`, `Binding`; owner epics append names), and (j) a file under a `CityGenerator/` folder including anything under `Public/Server/Random/`; zero findings on the real tree; seeded fixtures reported. (AMENDMENTS 2: the `Binding` entry matches the whole identifier token only, no substring match.) TOOLS means `tools/ci`, so the list is `tools/ci/lint/log-forbidden-identifiers.txt`.

---

## Implementation Notes

*Derived from ADR-0007 sections 3 (rules i, j) and 6; ADR-0008 sections 1 and 3; amendments G2, G5, G6 and AMENDMENTS 2:*

- **Script** `tools/ci/lint/include-lint-ij.sh` on the story 011 emitter and gate; findings as `file:line rule message` with the rule letter (`i`, `j`) as the rule id (story 016 asserts one finding per letter); repository root as an argument (default: the real root) so story 016 can aim it at a fixture tree.
- **Forbidden-identifier list** `tools/ci/lint/log-forbidden-identifiers.txt`: seeded with exactly `FEventLog` and `FRoundEventLog` (prefix entries), `DealDigest`, `ServerSubSeed`, `FSecretStream`, `MailboxOrder`, `Binding`. Owner epics append names. The inputs say prefix entries match as prefixes and `Binding` matches the whole identifier token only, but not how the file marks the difference: define it here (a trailing `*` for a prefix entry is a simple choice) and document it in the file's first lines. Every other entry is a whole-token match.
- **Rule (i)**: find each use of the listed macros (`UE_LOG`, `UE_LOGF`, `ensureMsgf`, `checkf`, `ClientMessage`, and the families `TRACE_*`, `CSV_*`, plus `SCOPE_CYCLE_COUNTER`), take the argument text up to the balanced closing parenthesis (arguments can span lines and nest), and report any identifier token matching the list. A forbidden name outside a macro argument is not a finding (spec edge case); a forbidden name inside a longer identifier is a finding only for prefix entries. Scope per G5: the five production modules (prototype exempt except for the cross-module rules (e)/(f)); exclude `Private/Tests/` only if the owner epics agree (the inputs are silent, so scan everything in scope by default).
- **Rule (j)**: any file under a `CityGenerator/` folder (expected to sit under `StealthGameplay/Features/`) whose include path contains `Public/Server/Random/`. At this point neither folder exists, so the real tree has zero files in scope: do not let that read as a pass without the fixture proof (the seeded fixture and the story 011 NOT ASSESSED rule cover it).
- **Seeded fixtures (G6)** under `tools/ci/lint/selftest/`: each listed macro carrying a forbidden identifier (one fixture per macro or a single file with one line each; every line reported as `file:line`); a forbidden prefix name; the `Binding` token inside a longer identifier (must pass); a `CityGenerator/` file including `Public/Server/Random/`; plus clean controls.
- **G2**: selftest as above (a clean pass AND a seeded violation detected) and the wrapper `StealthGame.Build.IncludeLintIj` (class `FBuildIncludeLintIjTest`, `Source/StealthFoundation/Private/Tests/Build/IncludeLintIjTest.cpp`; name proposed) running the script through `FPlatformProcess::ExecProcess` and asserting exit code and observations. Flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`.
- Prototype: none replaced; the prototype's existing `UE_LOG` calls are tolerated by ADR-0007 section 6 until rewritten, and the module is outside this rule's scope.

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`: emitter and gate
- `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`: rules (a)-(c)
- `production/epics/tuning-data-config/story-013-include-lint-rules-d-f.md`: rules (d)-(f)
- `production/epics/tuning-data-config/story-014-include-lint-rules-g-h.md`: rules (g)-(h)
- `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`: the combined seeded tree
- Appending owner-specific names to the forbidden list is done by each owner epic, not here

---

## Files

- `tools/ci/lint/include-lint-ij.sh`
- `tools/ci/lint/log-forbidden-identifiers.txt`
- `tools/ci/lint/selftest/` (fixtures for i and j with clean controls, and the selftest script)
- `Source/StealthFoundation/Private/Tests/Build/IncludeLintIjTest.cpp` (G2 wrapper)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these; do not invent new test cases during implementation.*

- **AC-1**: the lint reports rules (i) and (j), seeded fixtures are reported, and the real tree has zero findings
  - Given: `tools/ci/lint/log-forbidden-identifiers.txt` (FEventLog and FRoundEventLog prefixes, DealDigest, ServerSubSeed, FSecretStream, MailboxOrder, Binding); seeded fixtures: each listed macro with a forbidden identifier; a `CityGenerator/` file including `Public/Server/Random/`
  - When: include-lint-ij runs on the fixtures and the real tree
  - Then: each fixture is reported as `file:line` (rule i or j); the real tree reports zero findings
  - Edge cases: "Binding" matches the whole identifier token only (not substrings of other identifiers); prefix entries match as prefixes; owner epics may append entries

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the selftest under `tools/ci/lint/selftest/` (run by the T0 gate step) and the G2 wrapper `StealthGame.Build.IncludeLintIj` (`Source/StealthFoundation/Private/Tests/Build/IncludeLintIjTest.cpp`) must exist and pass (qa.level is full, so tests are required)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-011-t0-lint-framework-and-gate.md`
- Unlocks: `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`
- Sequencing note (not a plan dependency): the G2 wrapper test needs the StealthFoundation module of `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`.
- Open verification claims from the plan's Note lines: none (no Note line on 015). Open design points: how the forbidden-identifier file marks prefix entries, and whether test files are scanned.
