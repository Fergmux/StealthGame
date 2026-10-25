# Story 053: AC-6c FallbackFailed

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-6c, CR-6

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-6 defines the fallback ladder: requested preset, then `Default`, then schema defaults (the C++ initialisers), then `Failed`, where the round does not start, Round returns to `Lobby` and `TuningLoadFailed` is logged. Stories 050 and 051 build the first two rungs; this story closes the last rung, so a total failure is a defined, observable outcome rather than a crash or a silent default. The schema-defaults rung compiles in, so it cannot fail by itself: the test needs the substituted-schema seam added to `FTuningResolveInput` by story 050 (amendment), with defaults that violate an invariant.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: A plain-C++ `FTuningPipelineCore` resolves through a fallback ladder (requested, `Default`, schema defaults, `Failed`); invariants reject or warn but never repair, and warnings go to `LogStealthFoundation` and into the `FValidationReport`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: Pre-cutoff APIs only; `UE_LOGF` (5.8) is used for the warning (format literals, `%ls` for `TCHAR*` [V `LogMacros.h:291`]). This story touches none of ADR-0004's three Verification Required items (snapshot atomicity, AlwaysCook, hot-reload delegate).

**Control Manifest Rules (this layer)**:
- Required: invariant `Check` functions are pure and take `const` input; only F-1 changes a value; invariants reject or warn, never repair — source: ADR-0004
- Required: the tuning validator is a plain C++ core (`F...Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: new code logs with `UE_LOGF` and a `LogStealth...` category — source: ADR-0007
- Guardrail: tuning validation under 1 ms at `Preparing` — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-6c (CR-6)**: GIVEN every rung fails, WHEN resolved against a Round stub, THEN the pipeline is `Failed`, the stub receives the `Failed` result and `TuningLoadFailed` is logged. (`StealthGame.Tuning.FallbackFailed`)

---

## Implementation Notes

*Derived from ADR-0004 section 3 (Validator, fallback ladder), Key Interfaces and amendment 050:*

- **Where the code goes.** The terminal rung lives in `Source/StealthFoundation/Private/Tuning/StealthTuningPipelineCore.cpp` (the same file as stories 050 and 051). Result type per ADR-0004: `FTuningResolveResult { FallbackLevel, PresetId, FValidationReport, TSharedPtr<const FValidatedTuning> }`. The ADR does not name an explicit `Failed` flag, so `Failed` is represented by a null `TSharedPtr<const FValidatedTuning>` with the report holding the reason; whether to add an explicit field is open and is a decision for `/dev-story`.
- **Making the last rung fail.** Schema defaults are compile-time constants, so the test passes a substituted schema through the `TConstArrayView<FTuningSchemaRow> Schema` member that story 050 added to `FTuningResolveInput`; the substituted rows carry defaults that violate an invariant (for example the AC-6a pair, `DefenceDurationSec` 5 with `DefenceCooldownSec` 35, which breaks I-1). The requested preset and `Default` are made to fail through the `ITuningPresetSource` seam (a source that returns missing or corrupted presets). No production code path may expose the substitution outside the test.
- **Stub and log.** Feed the result to `FStubRound` (story 052: `OnTuningFailed` receives the reason `LoadFailed`) through a test-local adapter; the real service facade is story 066. Capture the log with `FStealthLogCapture` (story 009, available through story 046) and assert `TuningLoadFailed` was logged. How many times it is logged is not fixed by the GDD criterion text or by the revised QA spec: assert at least one and record the observed count as a note (the earlier draft spec said once; the revised spec dropped the count).
- **Pipeline state.** The Idle/Resolving/Validating/Frozen/Failed state machine belongs to the game-instance store (story 058). This story asserts the core's result; the store's state transition into `Failed` is asserted where the store exists, and the note belongs in 058's test if added there.
- **Edge case.** No `OnTuningReady` is delivered to the stub on failure; assert the stub's recorded calls contain only the `Failed` receipt.
- **Test placement and naming (ADR-0008 section 2, `.claude/rules/test-standards.md`).** `Source/StealthFoundation/Private/Tests/Tuning/FallbackFailedTest.cpp`, class `FTuningFallbackFailedTest`, name `StealthGame.Tuning.FallbackFailed`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file inside `#if WITH_DEV_AUTOMATION_TESTS`. Assert zero unexpected faults at teardown using the fault-sink helper (story 008). No wall-clock predicate.
- **Negative control.** The test must also prove the seam is real: with the unsubstituted schema the same inputs do not end `Failed` (the schema-defaults rung accepts), so a seam that silently did nothing would be detected.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: the first rung and the `FTuningResolveInput` schema seam
- `production/epics/tuning-data-config/story-051-fallback-to-schema.md`: the schema-defaults rung (`FallbackLevel` 2, `SchemaDefaults`)
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: `FStubRound` itself
- `production/epics/tuning-data-config/story-084-round-log-header.md`: what the Round Event Log header records after a completed round
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store's state machine and preset loading

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningPipelineCore.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/FallbackFailedTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: every rung fails, the pipeline is `Failed`, the stub receives `Failed` and `TuningLoadFailed` is logged (test `StealthGame.Tuning.FallbackFailed`)
  - Given: a preset source that fails, a substituted schema whose defaults violate an invariant, `FStubRound`, log capture
  - When: resolved
  - Then: pipeline `Failed`; the stub receives `Failed`; `TuningLoadFailed` is logged
  - Edge cases: no `OnTuningReady` delivered

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/FallbackFailedTest.cpp` must exist and pass (`StealthGame.Tuning.FallbackFailed`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-051-fallback-to-schema.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: `production/epics/tuning-data-config/story-084-round-log-header.md`
- Open verification claims: the plan has no Note line for this story. Open items found while writing: the representation of `Failed` in `FTuningResolveResult`, and how often `TuningLoadFailed` is logged.
