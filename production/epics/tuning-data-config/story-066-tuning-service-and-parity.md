# Story 066: UStealthTuningService and AC-21 PlayerNpcParity

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-007`, `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-21, CR-5, CR-10

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-10 gives every consumer one access path: a world-scoped accessor with two faces, `GetServerTuning()` for server code and `GetClientTuning()` for client code, and CR-5 forbids a tuning accessor that can return a different value for a different pawn, team or viewer (players and NPCs read the same numbers; Pillar 1). This story creates the service, `UStealthTuningService : UWorldSubsystem, IStealthClientTuning`, with its declared surface, and proves the parity rule at compile and reflection level: neither accessor takes a pawn, controller, team or viewer parameter. The service's runtime behaviours are deliberately verified elsewhere (the amendment keeps this criterion to AC-21 only): the invalid-before-valid client read and the ready and failed events belong to stories 053, 074 and 076.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: A world-scoped `UStealthTuningService` (one per world, so each PIE world reads its own GameInstance and GameState) exposes `BeginResolve()`, `GetServerTuning()`, `GetValidatedTuning()`, `OnTuningReady` and `OnTuningFailed` on the server side and `GetClientTuning()` through `IStealthClientTuning`, with no pawn, team or viewer parameter.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: `UWorldSubsystem` [V `Engine/Public/Subsystems/WorldSubsystem.h:16`] and `UGameInstanceSubsystem` are pre-cutoff. A subsystem gets a Blueprint "Get <X> Subsystem" node unless it is `NotBlueprintType` (ADR-0007 engine fact [V `K2Node_GetSubsystem.cpp:282`]), so the server service is `UCLASS(NotBlueprintType, NotBlueprintable)`. ADR-0004's Verification Required items (snapshot atomicity, AlwaysCook, hot-reload delegate) are not touched.

**Control Manifest Rules (this layer)**:
- Required: server code reads only `GetServerTuning()` / `FValidatedTuning`; client and presentation code reads only `GetClientTuning()` — source: ADR-0004
- Forbidden: never give a tuning accessor a pawn, controller, team, viewer or participant parameter — source: ADR-0004
- Required: a service with both faces is split into a `View/` facade (`IStealthClientTuning`) and a `Server/` implementation; every server UCLASS is `NotBlueprintType, NotBlueprintable` — source: ADR-0007
- Required: subsystem ordering uses `Collection.InitializeDependency<T>()`; registrations are weak and cleared in `Deinitialize` — source: ADR-0007

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-21 (Pillar 1, CR-5, CR-10)**: GIVEN the accessor API, WHEN reflected or compiled, THEN `GetServerTuning()` and `GetClientTuning()` take no pawn, controller, team or viewer parameter, so no per-participant value can be requested. (`StealthGame.Tuning.PlayerNpcParity`)

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Runtime objects) and Key Interfaces, and ADR-0007 section 2 (facade split):*

- **Files.** `Source/StealthFoundation/Public/View/StealthClientTuningInterface.h` declares `IStealthClientTuning` with `const FStealthClientTuning* GetClientTuning() const` and `uint64 GetClientHash() const` (View, client-safe, no guard). `Source/StealthFoundation/Public/Server/StealthTuningService.h` (guard first) declares `UStealthTuningService`; the implementation is `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`.
- **Declared surface (ADR Key Interfaces).** `void BeginResolve()` (Round only, on entering `Preparing`, server and game thread); `const FStealthServerTuning* GetServerTuning() const` (nullptr before Frozen); `TSharedPtr<const FValidatedTuning> GetValidatedTuning() const`; `FOnTuningReady OnTuningReady` (`uint64 TuningHash`); `FOnTuningFailed OnTuningFailed` (`ETuningFailReason`: LoadFailed or SnapshotFault); and, only under `WITH_EDITOR`, `OnTuningChanged` (story 085 uses it). `GetClientTuning()` returns nullptr (invalid) before the snapshot is valid and never a local default; in this story it may simply return nullptr, since the GameState snapshot arrives in story 073. `GetClientHash()` is recomputed from received fields by story 074; its stub behaviour here is open.
- **The service delegates to the store.** The service is a thin world-scoped facade over the game-instance store (story 058), resolved through the world's GameInstance, so `ServerTravel` (which recreates world subsystems) does not lose pipeline state. How the world subsystem obtains and orders itself against the game-instance subsystem is not stated beyond the manifest ordering rule: open for `/dev-story`. No static or global cache (this would also fail story 067).
- **Parity test.** `Source/StealthFoundation/Private/Tests/Tuning/PlayerNpcParityTest.cpp`, class `FTuningPlayerNpcParityTest`, name `StealthGame.Tuning.PlayerNpcParity`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. Per the QA spec: reflect and check at compile time with `static_assert` on signatures. A technique to propose: `static_assert(std::is_same_v<decltype(&UStealthTuningService::GetServerTuning), const FStealthServerTuning* (UStealthTuningService::*)() const>)` and the same for `IStealthClientTuning::GetClientTuning`; taking the address of an overloaded member by name is ambiguous, so an added pawn overload breaks the assertion (the QA edge case). Also walk the class reflection for any `UFUNCTION` named `GetServerTuning` or `GetClientTuning` and assert its parameter list is empty (server types expose none to Blueprint, so this may find none, which is itself correct).
- **What is not asserted here.** Invalid-before-valid for `GetClientTuning()` (stories 074 and 076), the ready and failed events (stories 053 and 076) and per-world isolation (story 067). The amendment removed them from this criterion on purpose.
- **Prototype.** None; `UStealthSettings` is not read by the service.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the game-instance store the service wraps
- `production/epics/tuning-data-config/story-067-world-scoped-accessor-pie.md`: per-world isolation in multi-client PIE
- `production/epics/tuning-data-config/story-068-server-key-not-on-client.md`: the compile-fail probe for a server-only key on the client face
- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated snapshot behind `GetClientTuning()`
- `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`: the first consumer of `GetServerTuning()`
- `production/epics/tuning-data-config/story-053-fallback-failed.md`: the Failed result path

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/StealthTuningService.h`
- `Source/StealthFoundation/Public/View/StealthClientTuningInterface.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/PlayerNpcParityTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the accessors take no pawn, controller, team or viewer parameter (test `StealthGame.Tuning.PlayerNpcParity`)
  - Given: the `UStealthTuningService` accessor API
  - When: reflected and checked with `static_assert` on signatures
  - Then: `GetServerTuning()` and `GetClientTuning()` take no pawn, controller, team or viewer parameter
  - Edge cases: an added pawn overload fails the check; the service behaviours are covered by stories 053, 074 and 076

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/PlayerNpcParityTest.cpp` must exist and pass (`StealthGame.Tuning.PlayerNpcParity`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`
- Unlocks: `production/epics/tuning-data-config/story-067-world-scoped-accessor-pie.md`, `production/epics/tuning-data-config/story-068-server-key-not-on-client.md`, `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`
- Open verification claims: the plan Note says the service is a `UWorldSubsystem` with `BeginResolve()`, `GetServerTuning()`, `GetValidatedTuning()`, `OnTuningReady`, `OnTuningFailed`, and that `GetClientTuning()` returns nullptr before the snapshot is valid, never a local default (design claims; behaviours verified by stories 053, 074, 076 per the amendment). Open items found while writing: how the world subsystem is ordered against the game-instance store, and the stub for `GetClientHash()` before story 074.
