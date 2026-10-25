# Story 090: AC-31 PresetChangeNextRound

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003`; GDD AC-31 (Edge, preset change)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The preset name lives in the game-instance `UStealthTuningStore`, set once at session open from `?Tuning=` (non-Shipping) or by `stealth.tuning.preset`, and consumed by `ResolveRoundPreset` at the next `Preparing`. The frozen set of the running round is never touched, because `Round`-latched values are read from the immutable `FValidatedTuning`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API for this story; logging is `UE_LOGF` (5.8) [V `LogMacros.h`]. Console commands use the engine's standard console-command registration; the inputs name no specific class and none is claimed here. `stealth.tuning.preset` is excluded from Shipping and from Test (CR-13, ADR-0004 s9), so this test runs only in a non-Shipping, non-Test configuration (the editor automation run qualifies).

**Control Manifest Rules (this layer)**:
- Required: Hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks (also `!UE_BUILD_TEST` for `stealth.tuning.preset`) — source: ADR-0004
- Required: The tuning hash is the project's own FNV-1a-64 over the F-2 byte stream, with the committed golden fixture — source: ADR-0004
- Forbidden: Never read a gameplay value from `UDeveloperSettings`, ini, `Saved/Config`, command line (Shipping) or a console variable (the preset command changes only the stored preset name) — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-31 (Edge, preset change)**: GIVEN a non-Shipping session whose stored preset changes via `stealth.tuning.preset` during `Active`, WHEN the round continues, THEN live values are unchanged. AND at the next `Preparing` the new preset is active with `Revision` 0 and a different `TuningHash`. (`StealthGame.Tuning.PresetChangeNextRound`)

---

## Implementation Notes

*Derived from ADR-0004 sections 6 and 9:*

- The command is registered in `Source/StealthFoundation/Private/Tuning/StealthTuningCommands.cpp`, which story 088 creates inside the `!UE_BUILD_SHIPPING && !UE_BUILD_TEST` guard with its sentinel. This story supplies the behaviour behind it: the command sets the stored preset name on `UStealthTuningStore` and does nothing else. It must not re-resolve, re-freeze or publish anything during `Active`.
- At the next `Preparing`, Round calls `BeginResolve()`; the store runs `ResolveRoundPreset` with the newly stored name through the normal CR-6 ladder (case-insensitive catalog match, malformed names resolve `Default` with a warning, `HostSelectable` honoured only in Development and Test). A new preset starts at `Revision` 0 and carries its own `TuningHash`.
- Preset state lives in the game-instance store, not the GameMode, because `ServerTravel` recreates the GameMode (ADR-0004 s6). The test must therefore hold the store, not a GameMode field, as the unit under test.
- The test is T1 at editor automation: `StealthGame.Tuning.PresetChangeNextRound`, class `FTuningPresetChangeNextRoundTest`, file `Source/StealthFoundation/Private/Tests/Tuning/PresetChangeNextRoundTest.cpp`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. It drives a test GameInstance and the store with the injectable preset source (story 050 seam) and the `FStubRound` from story 052, and uses the fake clock (story 007). No wall-clock waits.
- Observations: during `Active`, compare the values returned by `GetServerTuning()` before and after the command (unchanged) and the frozen `TuningHash` (unchanged). After the next `BeginResolve()`, assert `PresetId` is the new preset, `Revision` is 0 and `TuningHash` differs. Two catalog presets with different values are needed; story 056 supplies `Default` (all defaults) and `Playtest` (identical values to Default), so their hashes may not differ. Use a test-only preset with a differing value from the injectable source for the "different TuningHash" assertion. Whether the real `Playtest` and `Default` hash differently is not stated in the inputs and is not assumed.
- Edge case from the spec: changing to the same preset keeps the hash.
- Faults: any expected warning is declared through the fault sink and log-capture seams (stories 008 and 009).
- Prototype: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`: registers the commands inside their guards with sentinels and the Development scan.
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store, preset-name state and `Default` resolution at session start.
- `production/epics/tuning-data-config/story-059-preset-not-selectable.md`, `story-060-preset-case-insensitive.md` and `story-061-preset-malformed.md` (all under `production/epics/tuning-data-config/`): option-name handling rules.
- `production/epics/tuning-data-config/story-063-preset-survives-travel.md`: the multi-process travel behaviour of the stored preset.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthFoundation/Private/Tuning/StealthTuningCommands.cpp
- Source/StealthFoundation/Private/Tests/Tuning/PresetChangeNextRoundTest.cpp

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC-31 preset change takes effect next round
  - Given: a non-Shipping session in `Active` with a Round stub.
  - When: `stealth.tuning.preset` changes the stored preset; the round continues; the next `Preparing` runs.
  - Then: live values are unchanged during `Active`; at the next `Preparing` the new preset is active with `Revision` 0 and a different `TuningHash`.
  - Edge cases: a change to the same preset keeps the hash.

---

## Test Evidence

*Governed by `qa.level`: tests are required (qa.level is full).*

**Story Type**: Integration
**Required evidence**:
- Integration: the automation test `StealthGame.Tuning.PresetChangeNextRound` in `Source/StealthFoundation/Private/Tests/Tuning/PresetChangeNextRoundTest.cpp` must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`, `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md` (added after review: the Round stub)
- Unlocks: None
- Open verification claims: the plan's dependency list omits story 052 (`production/epics/tuning-data-config/story-052-round-session-stubs.md`), although the spec needs the Round stub; confirm it is done before starting. Whether `Default` and `Playtest` hash differently is not stated, so the test uses a test-only differing preset.
