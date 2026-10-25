# Story 005: Fixture constants live in test constant files, not inline

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-013` (Event Log ring bound; kill timeline protected from eviction; `TruncatedCount`; telemetry section; nearest TR, approximate: this story guards the fixtures of the Round test suites, and the header-field fixture it protects is the Event Log's). GDD AC: Test seams (binding on implementation), fixture sentence (revised by the AC overrides file: this story is [STATIC] and Type Logic). Rules: R19 (header fields).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; `Public/View/` vs `Public/Server/` with a compile guard; consumer-owned interfaces wired by the composition root; plain-C++ cores behind thin facades; logging policy. This story applies the ADR-0007 test and lint conventions: T1 tests live in the owning module's `Private/Tests/`, lints and analysis scripts emit observations while pass/fail lives in the gate, and every lint or statistical gate includes a negative control the suite detects (ADR-0008 via the control manifest).
**ADR Version**: ADR-0007 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: None touched. This is a text scan of source files plus a constants header; it calls no engine API beyond the automation framework and file reads. The scan runs inside the automation run; reading source files is a build-tree access, so Open: confirm against ADR-0008 how a [STATIC] check reaches the source tree on the CI runner (an automation test reading `Source/` through `FPaths::ProjectDir()`, or a lint script wired into the gate script). The post-cutoff item of ADR-0007 (`UE_LOGF`) is not used.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Every leak, parity, statistical and lint gate includes a negative control the suite detects (ADR-0008)
- Forbidden: Never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail (ADR-0008)
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); the scan reads a few dozen small files

**Why this story**: The GDD says fixture constants (team layouts, the header field list, codes) live in test constant files and no [U] test inlines them. Statistical tests also use fixed fixture secrets in `RoundTestSeeds`. If those literals drift into test bodies, two tests can silently disagree about the same header or secret. A static scan makes the rule enforceable instead of a convention. Exit A seam story; stories 002, 006, 011, 018, 022 and 036 consume the constant files.

**Prototype disposition**: none (new Foundation code). `StealthRulesTests.cpp` in `Source/StealthGame` is not scanned and is not touched.

---

## Acceptance Criteria

*From the AC overrides file (replaces the GDD's fixture sentence), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a [STATIC] scan of Source/*/Private/Tests/Round/*.cpp and Source/StealthFoundation/Private/Tests/*.cpp, THEN no literal from the RoundLogHeaderFields names, the RoundTestSeeds secrets or the fixture code strings appears outside the test constant files (BLOCKING in the test run). Keep one planted-violation fixture directory (outside the scanned globs) to prove the scan can fail; the scan fails if a glob matches zero files. `StealthGame.Round.FixtureConstants` [STATIC]

---

## Implementation Notes

*Derived from GDD R19 (header fields), the control manifest test rules and the AC text above:*

- Constant files (an explicit allowlist of paths, named in the scan): one for `RoundLogHeaderFields` (the header field names listed in GDD R19 and ADR-0002 section 8.9: `RoundIndex`, `RoundEpoch`, `Seed`, `LayoutId`, `LayoutAttempt`, `LayoutHash`, `ProviderKind`, `LayoutFallbackLevel`, `PresetId`, `TuningHash`, `SchemaVersion`, `FallbackLevel`, `HotReloaded`, and the rest of the R19 header list), one for `RoundTestSeeds` (the fixture secrets, including S1 and S2 used by stories 002 and 006), one for fixture code strings (refusal and abort codes), and team layouts. Open: confirm against GDD R19 and ADR-0002 section 8.9 the full header field list and its order when creating the fixture, and confirm which code strings (`FStartRefusal` codes, abort codes, `EndReason` names) are fixture strings for the scan.
- The scan reads raw text (comments count: propose yes, per the QA spec) of every file matching the two globs, minus the allowlisted constant files, and fails on any hit. It fails if either glob matches zero files. Open: confirm the glob meaning. As written, `Source/StealthFoundation/Private/Tests/*.cpp` is non-recursive and matches only files directly in `Tests/`, while this epic's Foundation tests live in subfolders (`Tests/Clock/`, `Tests/Queue/`, `Tests/EventLog/`); with the "fails on zero matches" rule that glob would fail on day one. Proposed resolution for confirmation: use `Source/StealthFoundation/Private/Tests/**/*.cpp`, or place Foundation tests directly in `Tests/`. Do not change the AC; ask the owner.
- Negative control: a planted-violation fixture directory outside both globs holds one file with a banned literal. It must sit under a path that no scanned glob matches (a folder under `Private/Tests/` would be matched by the recursive glob proposed above); the planned path `tests/static-fixtures/fixture-constants-violation/` is a proposal, Open: confirm the location with the harness owner (ADR-0008). The test runs the scan against that directory and asserts it reports a violation, so a scan that always passes is detected. This directory is data, not compiled.
- The scan emits observations (hit list: file, line, matched literal), and pass/fail is decided in the test body (control manifest: lints emit observations only; pass/fail lives in the gate). Banned-literal lists are read from the constant files themselves, so adding a header field cannot leave the scan stale.
- Seed secrets in the scan output: print the file and line only, never the secret value (control manifest Forbidden: never log or expose secrets, even fixture ones).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (`production/epics/round-match-framework/story-002-test-seam-fixed-server-secret.md`): the fixed-secret injection seam that uses the `RoundTestSeeds` constants.
- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the Event Log header and `WinLatched` tests that consume `RoundLogHeaderFields`.
- Stories 006, 018, 022 and 036: tests that consume the seeds and team-layout fixtures.
- Core-layer test constant files for later stories are added by those stories; this story ships the files the Foundation stories need and the scan over the two globs.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Fixtures/StealthRoundTestConstants.h` (new; `RoundLogHeaderFields`, `RoundTestSeeds`, fixture code strings, team layouts)
- `Source/StealthFoundation/Private/Tests/Fixtures/FixtureConstantsTest.cpp` (new; the [STATIC] scan; class `FRoundFixtureConstantsTest`)
- `tests/static-fixtures/fixture-constants-violation/` (new; planted-violation data directory, not compiled, outside both scanned globs)
- Tests live under the engine test roots (`tests/README.md`); the planted-violation directory is data and is the one exception. `/dev-story` replaces this planned list with the real one. `Source/StealthFoundation` does not exist yet (tuning-data-config epic creates the skeleton). Open: confirm where a constants header shared by StealthFoundation and StealthGameplay tests lives, since a `Private/` header is not visible across modules (ADR-0008).

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a [STATIC] scan of Source/*/Private/Tests/Round/*.cpp and Source/StealthFoundation/Private/Tests/*.cpp, THEN no literal from the RoundLogHeaderFields names, the RoundTestSeeds secrets or the fixture code strings appears outside the test constant files (BLOCKING in the test run). Keep one planted-violation fixture directory (outside the scanned globs) to prove the scan can fail; the scan fails if a glob matches zero files.
  - Given: the [STATIC] scan over `Source/*/Private/Tests/Round/*.cpp` and `Source/StealthFoundation/Private/Tests/*.cpp`, excluding the named constant files.
  - When: it runs as part of the automation test run.
  - Then: zero literals from the `RoundLogHeaderFields` names, the `RoundTestSeeds` secrets or the fixture code strings are found outside the constant files.
  - Then: a failure fails the run (BLOCKING).
  - Edge cases: add a planted-violation fixture to prove the scan can fail (non-vacuity); the scan fails if either glob matches zero files; define whether comments count (propose: yes, scan raw text; Open: confirm); the constant files are an explicit allowlist of paths.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic (the AC is tagged [STATIC] and is BLOCKING in the test run)
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Fixtures/FixtureConstantsTest.cpp` — automation test `StealthGame.Round.FixtureConstants` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundFixtureConstantsTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`. The gate must also be watched to fail once against the planted-violation directory before it is trusted.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (within this epic)
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-006-intake-fairness.md`, `production/epics/round-match-framework/story-011-event-log-content.md`, `production/epics/round-match-framework/story-018-team-assignment.md`, `production/epics/round-match-framework/story-022-start-validity.md`, `production/epics/round-match-framework/story-036-spawn-order-uniform.md`
