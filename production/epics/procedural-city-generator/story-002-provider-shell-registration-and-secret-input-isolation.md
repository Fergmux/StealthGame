# Story 002: Provider shell, registration and secret-input isolation

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0006 for the public-only scope and the secret-RNG header ban)
**ADR Decision Summary**: `ILayoutProvider` is consumer-owned by Core; the City Generator is the second provider (kind `Generated`, Feature layer) and reads only `FLayoutAttemptParams` (AttemptSeed, LayoutAttempt, GeneratorKitVersion, LayoutParamsHash, a plain params copy). It never calls `GetServerSubSeed`, never sees `FSiteAssignment`, Round or Session, and lives in `StealthGameplay/Private/Features/CityGenerator/` with its provider header in `Public/Server/Features/CityGenerator/`. It includes no Round, Session-secret, Selection, Crowd or Objectives header.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; this story is plain C++ plus one registration call)
**Engine Notes**: No post-cutoff API. `RegisterProvider(ILayoutProvider&)` is called by the composition root (`AStealthGameMode::InitGame` or its `OnWorldBeginPlay` handler, ADR-0007 section 4) and holds the provider weakly. No ADR-0014 verification item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Providers read only `FLayoutAttemptParams` — source: ADR-0014
- Forbidden: Never let a provider call `GetServerSubSeed`, read `FSiteAssignment`, or read world state other than its own geometry; never let Round and the Generator include each other's headers; never let a file under `CityGenerator/` include `Public/Server/Random/` — source: ADR-0014, ADR-0006, ADR-0007
- Guardrail: none for this story (the worker budget belongs to story 024)

This story creates the empty-but-registered provider and proves its isolation before any stage exists, so every later story inherits the include and secret-input guarantees. The default `LayoutProviderMode` stays `Graybox` (GDD R-G1.3): registering the Generator changes no round.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G2, R-G1.3) and LSC AC-3, scoped to this story:*

- [ ] **AC1** (AC-G2, `Leak.NoSecretInputs`): **GIVEN** an instrumented build **WHEN** the Generator `BeginAttempt` runs **THEN** no call reaches `GetServerSubSeed`, `RoundIndex` or `FSiteAssignment`, and an injected call of each fires the server assert.
- [ ] **AC2** (LSC AC-3, `Module.RoundGeneratorIsolated`): **GIVEN** the include graph **WHEN** the lint runs **THEN** no file under `CityGenerator/` includes a Round, Session-secret, Selection, Crowd, Objectives or `Public/Server/Random/` header, and no Round file includes a Generator header; a seeded violation of each is reported (negative control).
- [ ] **AC3** (R-G1, `Provider.RegistrationAndDefaultMode`): **GIVEN** the composition root **WHEN** the Generator provider registers through `RegisterProvider` as kind `Generated` **THEN** `BeginAttempt` reads only `FLayoutAttemptParams` and the default `LayoutProviderMode` is still `Graybox` (registration alone changes no round).

---

## Implementation Notes

*Derived from ADR-0014 section 2, section 9.3 and Implementation Guidelines 1-2:*

Create `FCityGeneratorProvider : ILayoutProvider` with `GetKind() == Generated`; `BeginAttempt` copies what it needs from `FLayoutAttemptParams` into plain structs and returns `OnAttemptFailed(GeneratorConfigInvalid)` until later stories add stages (the failure path itself is story 025). Open `FPublicOnlyScope` around everything the provider does (ADR-0006: the guard is `thread_local`; the worker opens its own, story 024). Use the story-011 session guard (`session-travel/story-011-server-sub-seed-server-only.md`) for the assert and the TDC lint framework (`tuning-data-config` stories 011-016) for the include rules; this story supplies the Generator-side fixtures and the Round-side check, not new lint machinery. `static_assert` that `BeginAttempt` takes only `const FLayoutAttemptParams&` and `ILayoutBuildSink&`. Do not read tuning, Round or Session state from the provider.

Prototype replaced: none yet. `StealthCityGenerator` (428 + 133 lines, grid city built identically on every machine from a replicated server seed) is retired by story 045 once the provider is complete.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-integer-stage-streams-and-canonical-iteration.md`: the integer streams the provider will use.
- `story-024-worker-task-cancellation-and-state-machine.md`: the worker task, `FPublicOnlyScope` on the worker thread and the state table.
- `story-025-failure-modes-and-teardown.md`: `GeneratorConfigInvalid` semantics and fallback.
- LSC service, provider registry and `NoProvider`: `level-socket-contract/story-001-service-skeleton-provider-registry.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/Features/CityGenerator/StealthCityGeneratorProvider.h`
- `Source/StealthGameplay/Private/Features/CityGenerator/StealthCityGeneratorProvider.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/NoSecretInputsTest.cpp` (class `FCityGenLeakNoSecretInputsTest`, name `StealthGame.CityGen.Leak.NoSecretInputs`)
- `Source/StealthGameplay/Private/Tests/CityGen/RoundGeneratorIsolatedTest.cpp` (class `FCityGenModuleRoundGeneratorIsolatedTest`, name `StealthGame.CityGen.Module.RoundGeneratorIsolated`)
- `Source/StealthGameplay/Private/Tests/CityGen/RegistrationAndDefaultModeTest.cpp` (class `FCityGenProviderRegistrationAndDefaultModeTest`, name `StealthGame.CityGen.Provider.RegistrationAndDefaultMode`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Leak.NoSecretInputs`
  - Given: spies on `GetServerSubSeed`, `RoundIndex` and `FSiteAssignment`.
  - When: `BeginAttempt` runs a full attempt on a golden seed.
  - Then: every spy count is 0.
  - Edge cases: three injected calls, one per symbol, each fire the server assert (negative control, caught by an expected-assert hook).
- **AC2** `Module.RoundGeneratorIsolated`
  - Given: the include graph of `CityGenerator/` and the Round files.
  - When: the lint runs.
  - Then: 0 forbidden includes (Round, Session-secret, Selection, Crowd, Objectives, `Public/Server/Random/`) and 0 Round-to-Generator includes.
  - Edge cases: one seeded violation per category (7 fixtures) is each reported with file and header (negative control). Transitive includes are in the graph.
- **AC3** `Provider.RegistrationAndDefaultMode`
  - Given: a composition root where the Generator registers as kind `Generated`, and a world with no tuning, Round or Session objects.
  - When: `BeginAttempt` runs with an `FLayoutAttemptParams` fixture.
  - Then: it runs using only AttemptSeed, GeneratorKitVersion, LayoutParamsHash and the params copy. Default `LayoutProviderMode` is `Graybox`, and a round started after registration still uses graybox.
  - Edge cases: a `static_assert` that the `BeginAttempt` signature takes only that struct.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic `level-socket-contract/story-001-service-skeleton-provider-registry.md` (`ILayoutProvider`, `RegisterProvider`), `session-travel/story-011-server-sub-seed-server-only.md`, `tuning-data-config/story-012-include-lint-rules-a-c.md` and `story-015-include-lint-rules-i-j.md`
- Unlocks: `production/epics/procedural-city-generator/story-003-integer-stage-streams-and-canonical-iteration.md`, `story-024-worker-task-cancellation-and-state-machine.md`, `story-025-failure-modes-and-teardown.md`
