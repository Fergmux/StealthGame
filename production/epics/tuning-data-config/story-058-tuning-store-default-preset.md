# Story 058: AC-1 DefaultPreset: the game-instance preset store

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003`, `TR-tdc-001` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-1, CR-2, CR-4

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-4 puts the preset name in a server-side preset store scoped to the game instance, because `ServerTravel` creates a new GameMode and re-runs map initialisation, so anything parsed once at init would be lost for round 2. This story builds that store, `UStealthTuningStore : UGameInstanceSubsystem`, wires the validator core (stories 046 to 051) and the assets (056, 057) into it, and proves the simplest path: no `?Tuning=` option means `Default` is active at `FallbackLevel` 0 with no warning. Stories 059 to 063 then add the option-parsing edge cases on top of this store.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Pipeline state lives in a game-instance store that survives travel (preset name, async-loaded catalog and presets, session-frozen keys, the plain-C++ `FTuningPipelineCore` state machine, the current frozen set), while the accessor is a separate world-scoped subsystem; Shipping always resolves `Default`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: `UGameInstanceSubsystem` [V `Engine/Public/Subsystems/GameInstanceSubsystem.h:16`] and `UAssetManager::LoadPrimaryAssetsWithType` are pre-cutoff. `UE_LOGF` (5.8) is used for warnings. ADR-0004 Verification Required (2) (catalog-only presets cooked with `AlwaysCook`) is [U] until story 064 and is not assumed here: the store must fall back to a logged synchronous load if the async load is unfinished. Server types are reachable from Blueprint unless `NotBlueprintType` (ADR-0007 engine fact [V `K2Node_GetSubsystem.cpp:282`]), so the store carries `UCLASS(NotBlueprintType, NotBlueprintable)`.

**Control Manifest Rules (this layer)**:
- Required: server code reads only `GetServerTuning()` / `FValidatedTuning`; the store hands out the frozen value, never the raw `UStealthTuningData` — source: ADR-0004
- Required: hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks — source: ADR-0004
- Required: every UCLASS in a `Server/` header declares `NotBlueprintType, NotBlueprintable`; the validator stays a plain C++ core behind the subsystem facade — source: ADR-0007
- Forbidden: never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable — source: ADR-0004
- Guardrail: validation under 1 ms at `Preparing`; tuning memory under 100 KB — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-1 (CR-2, CR-4)**: GIVEN a Dev build, WHEN the server starts with no `?Tuning=` option, THEN the active preset is `Default`, `FallbackLevel` is 0, `PresetId` is `Default` and no warning is logged. (`StealthGame.Tuning.DefaultPreset`)

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Runtime objects and lifetime) and section 9, and the amendment to AC-1:*

- **Reading of "Dev build" (amendment).** The GDD defines "Dev build" as a packaged Development build. For this story the amendment fixes the condition to a non-Shipping build and the test to editor automation against a test GameInstance with the log-capture hook; story 064 repeats the observation in a cooked Development build. Do not weaken or reinterpret the criterion text above; this note only states how the test is run.
- **Files.** `Source/StealthFoundation/Public/Server/StealthTuningStore.h` and `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`. Header starts with `StealthServerOnlyGuard.h`.
- **Store contents (ADR-0004 section 6 diagram).** `PresetName` (set once at session open: from `?Tuning=` in non-Shipping builds, else `Default`); the catalog and preset objects, async-loaded at session open through the Asset Manager with strong references held (primary types registered by story 057); the session-frozen keys; an `FTuningPipelineCore` instance with the state machine Idle, Resolving, Validating, Frozen, Failed; and the current `TSharedPtr<const FValidatedTuning>` with its `FTuningResolveResult`. Public API per ADR-0004 Key Interfaces: `FName GetStoredPresetName() const`, `int32 GetSchemaVersion() const` (returns `TuningSchemaVersion`), `uint64 GetServerClientHash(int32 RoundIndex, int32 Revision) const`. The semantics of the epoch-keyed `GetServerClientHash` are story 075; here it needs only the declaration and a result for the current epoch (its exact stub behaviour is open).
- **Resolve path.** `ResolveRoundPreset(Name)` is the single entry (stories 059 to 061 extend it). With no option, the stored name is `Default`; copy the preset out of the loaded `UStealthTuningData` into a value `FStealthServerTuning`, call `FTuningPipelineCore::Resolve` with an `ITuningPresetSource` backed by the preloaded catalog, and keep the result. The sync-load fallback (async not finished) must log a warning; in this story's test the load is finished, so the no-warning assertion also guards against an accidental sync fallback.
- **Shipping rule.** Under `#if UE_BUILD_SHIPPING` `ResolveRoundPreset` returns `Default` unconditionally and the `?Tuning=` parsing is not compiled. Story 088 adds the sentinel literals for the binary scan; structure the parse so its block can carry a sentinel without moving code.
- **Why in the game instance.** ServerTravel recreates the GameMode and world subsystems, so the pipeline state lives here, not in the world accessor (story 066).
- **Test.** `Source/StealthFoundation/Private/Tests/Tuning/DefaultPresetTest.cpp`, class `FTuningDefaultPresetTest`, name `StealthGame.Tuning.DefaultPreset`, `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, inside `#if WITH_DEV_AUTOMATION_TESTS`. The test creates a `UGameInstance` so the store subsystem is constructed, runs the resolve with no option, wraps it in `FStealthLogCapture` (story 009) and asserts: active preset `Default`, `FallbackLevel` 0, `PresetId` `Default`, zero warnings on `LogStealthFoundation`, zero unexpected faults at teardown (story 008). How a test creates a `UGameInstance` with a subsystem collection but no real world is not in the inputs: open. It runs against the real catalog from story 056 (loaded in the editor).
- **State assertion (spec edge).** Pipeline state moves Idle to Frozen only; no Failed or Validating residue after the resolve. State surviving `ServerTravel` is covered where travel exists (stories 063 and 081).
- **No wall clock.** The resolve is synchronous in the test; no timing predicate.
- **Server-only types.** Everything in the store header is server-only; client code gets only the public snapshot (stories 070 and 073).
- **Prototype.** `UStealthSettings` (`GAME/Public/StealthSettings.h`) stays until story 095; the store does not read it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-059-preset-not-selectable.md`: AC-2a, `?Tuning=` naming a preset that is not HostSelectable
- `production/epics/tuning-data-config/story-060-preset-case-insensitive.md`: AC-2b, case-insensitive matching
- `production/epics/tuning-data-config/story-061-preset-malformed.md`: AC-2c, padded or path-bearing option values
- `production/epics/tuning-data-config/story-062-client-option-ignored.md`: AC-2d, client login options never read
- `production/epics/tuning-data-config/story-063-preset-survives-travel.md`: AC-2e, the store across ServerTravel (multi-process)
- `production/epics/tuning-data-config/story-064-presets-cooked.md`: the cooked Development build repeat of this observation and the catalog-only cook
- `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`: the world-scoped accessor
- `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`: sentinel literals in the guarded blocks

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/StealthTuningStore.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/DefaultPresetTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: with no `?Tuning=` option the active preset is `Default`, `FallbackLevel` 0, `PresetId` `Default`, and no warning is logged (test `StealthGame.Tuning.DefaultPreset`)
  - Given: editor automation, a test GameInstance with the store, the log-capture hook, a non-Shipping build
  - When: no `?Tuning=` option is given and the store resolves
  - Then: `Default`, `FallbackLevel` 0, `PresetId` `Default`, the captured log holds no warning
  - Edge cases: the cooked repeat is story 064

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the editor automation test `Source/StealthFoundation/Private/Tests/Tuning/DefaultPresetTest.cpp` (`StealthGame.Tuning.DefaultPreset`) must exist and pass; the cooked repeat is evidenced by story 064

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`, `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`, `production/epics/tuning-data-config/story-057-asset-manager-primary-types.md`, `production/epics/tuning-data-config/story-009-log-capture-hook.md`
- Unlocks: `production/epics/tuning-data-config/story-059-preset-not-selectable.md`, `production/epics/tuning-data-config/story-060-preset-case-insensitive.md`, `production/epics/tuning-data-config/story-061-preset-malformed.md`, `production/epics/tuning-data-config/story-062-client-option-ignored.md`, `production/epics/tuning-data-config/story-063-preset-survives-travel.md`, `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`, `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`, `production/epics/tuning-data-config/story-090-preset-change-next-round.md`, `production/epics/tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`
- Open verification claims: the plan Note says server types are `UCLASS(NotBlueprintType, NotBlueprintable)` and that the pipeline state lives in the game-instance store because ServerTravel recreates the GameMode (ADR-0004 section 6, a design claim, observed through stories 063 and 081). Open items found while writing: how a test builds a GameInstance subsystem collection without a world, and the stub behaviour of `GetServerClientHash` before story 075.
