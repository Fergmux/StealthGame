# Story 069: AC-8 static half NoClientAssetReference

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-009` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-8 companion static check (CR-4, CR-10, CR-13)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (Implementation Guideline 3; ADR-0007 Validation 3 for the include boundary)
**ADR Decision Summary**: Server code reads only `GetServerTuning()` / `FValidatedTuning`, never the raw `UStealthTuningData` asset; client and presentation code reads only `GetClientTuning()` and must never include a `Public/Server/` tuning header or reference `UStealthTuningData`. The compile guard of ADR-0007 covers C++ presentation translation units; a static check covers the rest.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff engine API is used. This is a text scan over source files (no engine, no UBT call) plus an automation wrapper that runs the script through `FPlatformProcess`. ADR-0004 Verification Required items (1)-(3) are not touched by this story. ADR-0007 notes that the `#error` guard only fires for translation units of `StealthPresentation`; code elsewhere that is client-side (for example `Public/View/` headers consumed by other modules) is not covered by the compile guard, which is why this scan exists.

**Control Manifest Rules (this layer)**:
- Forbidden: never let client or presentation code include a `Public/Server/` tuning header or reference `UStealthTuningData`; never let server code read the raw asset — source: ADR-0004
- Required: the include lint (rules a-j) runs in CI and pre-commit; lints and analysis scripts emit observations only, pass/fail lives in the gate script — source: ADR-0007, ADR-0008
- Required: every lint gate includes a negative control the suite detects (a seeded violation) — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (AC-8 companion static check), scoped to this story:*

- [ ] a static check asserts that StealthPresentation source and client-side code hold no reference to `UStealthTuningData` or any `Public/Server/` tuning header (ADR-0007 Validation 3); a seeded violation is detected.

---

## Implementation Notes

*Derived from ADR-0004 Implementation Guideline 3, ADR-0007 sections 2-3, ADR-0008 sections 1 and 3, and global amendments G2, G5, G6:*

- **Script.** `tools/ci/lint/no-client-asset-reference.sh` emits one `file:line rule message` observation per finding using the shared emitter of story 011 and never decides pass/fail; the gate script of story 011 (`tools/ci/lint/gate.sh`) decides (exit 0 with no findings, nonzero with any, NOT ASSESSED nonzero when zero files were scanned). Per the `.claude/scripts` rule, helpers emit observations, never verdicts.
- **What is forbidden.** In the scanned files: (1) any mention of `UStealthTuningData` (include of `StealthTuningData.h`, a use of the type, or a `TSoftObjectPtr`/`TObjectPtr` of it), and (2) an `#include` of any header under a `Public/Server/` folder that is a tuning header. The inputs do not define "tuning header" exactly; the proposed rule is any `Public/Server/` header whose file name contains `Tuning` (for example `StealthTuningData.h`, `StealthTuningCatalog.h`, `StealthServerTuning.h`, `StealthValidatedTuning.h`, `StealthTuningStore.h`, `StealthTuningService.h`). Record the final rule in the script header. Open: the exact matching rule.
- **Scope.** `Source/StealthPresentation/` plus every `Public/View/` folder of every module are "StealthPresentation source and client-side code". The composition root `Source/StealthGame` includes both faces legitimately (ADR-0007 section 2) and the harness module `Source/StealthTestHarness` reads server types by design (story 082 reads a tampered asset as a control), so neither is scanned. Per G5 the prototype module is scanned only by include-lint rules (e) and (f), not by this script. Open: whether other client-only code exists that the inputs do not name; none is assumed.
- **Transitive includes (spec edge case).** A client file that includes a non-tuning `View/` header which itself includes a `Server/` tuning header must be caught. Walk the project include graph from each scanned file and report the first offending edge with the file:line of the include that introduces it. Note that rule (c) of story 012 already forbids a `View/` header including any `Server/` header; this scan reports the same case from the client file's side with a tuning-specific message.
- **Forward declarations (spec edge case).** The spec lists forward declarations as an edge case without stating the expected result. Open: decide at implementation whether `class UStealthTuningData;` in client code is a finding (the AC says "hold no reference to"); the conservative reading is that it is a finding. Record the decision and cover it with a fixture line either way.
- **Seeded violation (G6).** A fixture tree with one seeded violation per sub-rule (direct `UStealthTuningData` use, direct `Server/` tuning include, transitive include, forward declaration if it is a finding) lives under `tools/ci/lint/fixtures/` (proposed folder `tools/ci/lint/fixtures/no-client-asset-reference/`; the plan names no path). The fixture must be reported by `file:line`.
- **G2 selftest.** `tools/ci/lint/selftest/no-client-asset-reference-selftest.sh` runs the script over the fixture and over a clean two-file input and asserts: clean input gives zero observations, each seeded line is reported with its file:line, the gate fails on the fixture and passes on the clean input. The T0 gate step runs it.
- **G2 automation wrapper.** `StealthGame.Build.NoClientAssetReference` in `Source/StealthFoundation/Private/Tests/Build/` runs the script through `FPlatformProcess` against the real tree and asserts exit code 0 and zero observations. The plan and the GDD name the test `StealthGame.Tuning.NoClientAssetReference` (file `FND/Private/Tests/Tuning/NoClientAssetReferenceTest.cpp`); keep that test as the AC-named test and also add the G2 `Build` wrapper. Whether the two collapse into one is open; do not drop the AC-named test.
- **Test mechanics.** `IMPLEMENT_SIMPLE_AUTOMATION_TEST` classes `FTuningNoClientAssetReferenceTest` and `FBuildNoClientAssetReferenceTest`, flags `EditorContext | ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. The wrapper reads no wall clock and no environment beyond the project directory. A negative control runs in the same suite (the seeded fixture must be detected; use the helper of story 010 where it applies).
- **Real tree today.** `StealthPresentation` does not exist until story 002 and the tuning headers until stories 038-040; the real-tree run therefore scans little at first. The gate's NOT ASSESSED rule (zero files scanned) prevents an empty scan from passing silently.
- **Prototype.** None. No prototype code is replaced or promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`: include-lint rules (a)-(c), including (c) a `View/` header that includes a `Server/` header
- `production/epics/tuning-data-config/story-082-local-tamper-ignored.md`: the multi-process runtime half of AC-8 (tampered asset, ini and `set` override against an unmodified server)
- `production/epics/tuning-data-config/story-068-server-key-not-on-client.md`: the compile-fail probe for a server-only key on the client face (AC-28)
- `production/epics/tuning-data-config/story-016-lint-seeded-violation-controls.md`: the seeded fixtures for include-lint rules (a)-(j), not this script

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `tools/ci/lint/no-client-asset-reference.sh`
- `tools/ci/lint/selftest/no-client-asset-reference-selftest.sh` (G2)
- `tools/ci/lint/fixtures/no-client-asset-reference/**` (proposed seeded fixture folder)
- `Source/StealthFoundation/Private/Tests/Tuning/NoClientAssetReferenceTest.cpp`
- `Source/StealthFoundation/Private/Tests/Build/NoClientAssetReferenceBuildTest.cpp` (G2 wrapper `StealthGame.Build.NoClientAssetReference`; file name proposed)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a static check asserts that StealthPresentation source and client-side code hold no reference to `UStealthTuningData` or any `Public/Server/` tuning header; a seeded violation is detected
  - Given: StealthPresentation and client-side sources; a seeded violation fixture (test `StealthGame.Tuning.NoClientAssetReference`)
  - When: the static check runs
  - Then: no reference to `UStealthTuningData` or any `Public/Server/` tuning header on the real tree; the seeded violation is detected (reported by file:line)
  - Edge cases: forward declarations; include via another header (transitive)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/NoClientAssetReferenceTest.cpp` (`StealthGame.Tuning.NoClientAssetReference`) must exist and pass
- Script story (G2): the selftest `tools/ci/lint/selftest/no-client-asset-reference-selftest.sh` (clean pass and seeded violation detected) and the wrapper `StealthGame.Build.NoClientAssetReference` in `Source/StealthFoundation/Private/Tests/Build/` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-012-include-lint-rules-a-c.md`
- Unlocks: None (story 082 names this check as its companion in the QA spec, but its dependency list does not include 069)
- Open verification claims: no plan Note line applies. Open decisions: the exact "tuning header" matching rule, the forward-declaration verdict, whether the AC-named test and the G2 `Build` wrapper are two tests or one.
