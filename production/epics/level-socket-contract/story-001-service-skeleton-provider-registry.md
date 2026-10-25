# Story 001: Layout service skeleton, provider registry and Round isolation

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-lsc-001 = Layout service world subsystem with registered providers; Round talks only to it)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: A server-only `UStealthLayoutService` (tickable world subsystem) owns a plain-C++ `FLayoutServiceCore` and a registry of consumer-owned `ILayoutProvider`s (Graybox now, City Generator later); Round and every feature reach layout only through the service, never a provider.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `UTickableWorldSubsystem` is verified in 5.8.4 (`WorldSubsystem.h:80-91`). Net mode may not be final when subsystems are created during seamless travel [U], so `ShouldCreateSubsystem` excludes `NM_Client` and non-game worlds and `OnWorldBeginPlay` re-checks authority and leaves the service inert without it.

**Why this story**: It lays the types every later story hangs off: the provider interface, the build sink, the service shell, weak registration by the composition root, and the include rule that keeps Round and the Generator apart. Read these GDD sections first (GDD is 140 KB; use `bash .claude/scripts/md-section.sh extract design/gdd/level-socket-contract.md "<heading>"` or Grep `^#` then Read a range): R1 Terms, R4 The layout service and the Preparing handshake (items 1-2), Acceptance Criteria AC-3.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Round and every feature reach layout only through `UStealthLayoutService` (ADR-0014)
- Forbidden: Never let Round and the Generator include each other's headers (ADR-0014)
- Guardrail: Layout runtime services at most 0.1 ms average in `Active` (ADR-0014)

---

## Acceptance Criteria

*From GDD `design/gdd/level-socket-contract.md` (AC-3 part, R4.1) and ADR-0014 section 1, scoped to this story:*

- [ ] GIVEN the module sources, WHEN the include graph is scanned, THEN no Round header includes a Generator or provider-implementation header (nor the reverse) and Round reaches layout only through `UStealthLayoutService` (`Module.RoundGeneratorIsolated`)
- [ ] GIVEN a server-authority game world, a client world and a non-game world, WHEN world subsystems are created, THEN `UStealthLayoutService` exists only in the server-authority game world; a service whose world lacks authority at `OnWorldBeginPlay` stays inert and refuses `RequestLayout` (`Service.AuthorityOnly`)
- [ ] GIVEN a provider registered through `RegisterProvider` by the composition root, WHEN `LayoutProviderMode` is Graybox, Generated or GeneratedWithFallback, THEN the mode selects the matching provider, the registration is held weakly (a destroyed provider is not called), and a second provider kind does not change the first's behaviour (`Service.ProviderSelectedByMode`)

---

## Implementation Notes

*Derived from ADR-0014 section 1, section 2 and Implementation Guidelines 1-2:*

- `UStealthLayoutService final : UTickableWorldSubsystem`, `NotBlueprintType, NotBlueprintable`, in `StealthGameplay/Public/Server/Core/StealthLayoutService.h`. `Tick` runs only while the service is in a working state (`GetTickableTickType` conditional).
- `ILayoutProvider` (`BeginAttempt`, `TickAttempt(WorkUnitBudget)`, `CancelAttempt`, `TearDownAttempt`, `GetKind`) and `ILayoutBuildSink` are **consumer-owned by Core** in `StealthGameplay/Public/Server/Core/StealthLayoutProvider.h` (ADR-0007). Providers read only `FLayoutAttemptParams` and never call `GetServerSubSeed` or read `FSiteAssignment`.
- `RegisterProvider(ILayoutProvider&)` is called by the composition root only (`AStealthGameMode::InitGame` or its `OnWorldBeginPlay` handler, ADR-0007 section 4); the service keeps a weak reference. A mode that names an unregistered provider fails `NoProvider` at `RequestLayout` (the abort path itself is story 004).
- `FLayoutServiceCore` is plain C++ taking `ILayoutMeasureBackend`, `ILayoutClock` and `ILayoutPawnQuery` by injection so state, timeout and fallback logic run headless. Declare empty interfaces now; later stories fill them.
- The include check is a test or CI lint over `#include` lines and Build.cs dependencies in the Round files and the provider-implementation files; forward declarations are not includes.
- Duplicate registration of one provider kind: pick reject-and-log and state it in the test (confirm with the lead programmer).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: the state table, out-of-state rejection and time slicing
- Story 003: attempt timeout, budget and reseed
- Story 026: the Graybox provider implementation

---

## Files

*The code and test files this story creates or changes. `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/StealthLayoutProvider.h`
- `Source/StealthGameplay/Public/Server/Core/StealthLayoutService.h`
- `Source/StealthGameplay/Private/Core/StealthLayoutService.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/ServiceRegistryTest.cpp`
- Module placement follows ADR-0007 section 2; confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

- **AC-1** `Module.RoundGeneratorIsolated`
  - Given: Round, Generator and provider-implementation sources and their Build.cs.
  - When: every `#include` in Round .h/.cpp and Generator/provider files is scanned.
  - Then: 0 Round files include Generator or provider-impl headers, 0 Generator files include Round, and Round's only layout include is `UStealthLayoutService`.
  - Edge cases: forward declarations and comments are not includes; a Build.cs module dependency edge counts as an include.
- **AC-2** `Service.AuthorityOnly`
  - Given: a server-authority game world, a client world, and a non-game world (editor preview).
  - When: world subsystems are created; separately, a service whose world lacks authority at `OnWorldBeginPlay` is given `RequestLayout`.
  - Then: the subsystem exists only in the server world; the authority-less instance is Inert, `RequestLayout` returns a refusal code (propose `NotAuthority`), no state change, no provider called.
  - Edge cases: authority gained after BeginPlay does not activate it.
- **AC-3** `Service.ProviderSelectedByMode`
  - Given: stub Graybox and Generated providers registered through `RegisterProvider` by a test composition root.
  - When: mode = Graybox, Generated, GeneratedWithFallback; then a provider is destroyed and GC'd; then a third kind is registered.
  - Then: Graybox calls only the graybox stub; Generated only the generated stub; GeneratedWithFallback generated first, graybox only after fallback; the destroyed provider is never called (NoProvider, no crash); the third kind changes the first's call count by 0.
  - Edge cases: duplicate registration of one kind (reject and log).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/ServiceRegistryTest.cpp` — automation tests `StealthGame.Layout.Module.RoundGeneratorIsolated`, `StealthGame.Layout.Service.AuthorityOnly`, `StealthGame.Layout.Service.ProviderSelectedByMode` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Layout.<Area>.<Scenario>`, one `IMPLEMENT_SIMPLE_AUTOMATION_TEST` class per test (`FLayout<Area><Scenario>Test`), flags `EditorContext | ProductFilter`, file wrapped in `#if WITH_DEV_AUTOMATION_TESTS`, injected clock and fixed-step ticks, no wall-clock time as a pass predicate.

---

## Dependencies

- Depends on: None within this epic
- Cross-epic: `production/epics/tuning-data-config/EPIC.md` delivers the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), harness and CI. Until it lands, write code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives (every story in this epic follows this note).
- Unlocks: `production/epics/level-socket-contract/story-002-service-state-machine-and-time-slicing.md`, `production/epics/level-socket-contract/story-026-graybox-provider-markers-and-bake.md`
