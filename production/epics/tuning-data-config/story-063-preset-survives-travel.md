# Story 063: AC-2e PresetSurvivesTravel (multi-process)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-2e, CR-4

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-4 puts the preset name in a game-instance store precisely because `ServerTravel` creates a new GameMode and re-runs map initialisation: a name parsed once at init would be lost for round 2. This story observes that claim for real, with a listen host and travel between rounds, in separate processes. GDD notes record that single-process PIE shares one asset object and does not support seamless travel, and ADR-0008 forbids proving a multi-process criterion in in-process PIE only, so this is a T3 (Gauntlet) test. It also checks the precedence rule: when the travel URL carries a forced `?Preset=Default`, the store still wins and a warning is logged.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: Pipeline state lives in a `UGameInstanceSubsystem` store that survives seamless and hard travel; the store wins over a travel-URL `?Preset=`, with a warning on disagreement; per-client login options are never read.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: ADR-0004 itself is LOW risk. The harness it rides is not: Gauntlet launching one cooked Development listen host plus clients on one machine is [V Gauntlet ships in 5.8; U topology] (ADR-0008 Verification 3, settled by story 020), and UAT discovers project scripts under `<Project>/Build/` [V `CompileScriptModules.cs:694`]. The claim that a `UGameInstanceSubsystem` survives `ServerTravel` is the ADR's design claim and is observed here for the first time; it is not an unverified engine API but it is unobserved. Seamless-travel details (S3, S4) belong to ADR-0005 spikes, not to this story.

**Control Manifest Rules (this layer)**:
- Required: test names `StealthGame.<System>.MP.<Scenario>`; T3 code lives in `StealthTestHarness`, Gauntlet node scripts under `Build/Scripts/` — source: ADR-0008
- Required: T2/T3 tests bound by frames or server time and assert and record the active replication system — source: ADR-0008
- Forbidden: never prove multi-process criteria in in-process PIE only; never select tiers with `Automation RunFilter` — source: ADR-0008
- Forbidden: never use wall-clock time as a pass/fail predicate (only a failure ceiling) — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-2e (CR-4)**: GIVEN the store holds `Playtest`, WHEN two `ServerTravel` round changes occur (multi-process), THEN both rounds resolve `Playtest`. AND WHEN the travel URL is forced to `?Preset=Default`, THEN the store still wins and a warning is logged. (`StealthGame.Tuning.PresetSurvivesTravel`)

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Runtime objects, Preset store precedence) and ADR-0008 sections 1 (T3) and 2 (naming):*

- **Names.** Test name `StealthGame.Tuning.MP.PresetSurvivesTravel` (ADR-0008 section 2 form for T3; the GDD lists the shorter `StealthGame.Tuning.PresetSurvivesTravel`). Scenario name for `-StealthHarnessScenario=<Name>`: `PresetSurvivesTravel` (proposal; the inputs give the file name, not the scenario string).
- **Files.** `Source/StealthTestHarness/Private/Scenarios/PresetSurvivesTravelScenario.cpp` registers the scenario with the runner of story 018 and the Gauntlet node `Build/Scripts/StealthTuningTravelNode.cs` follows the pattern of story 020's `StealthMultiProcessSmoke.cs`. Class and base type of the node come from the 5.8.4 Gauntlet sources at implementation; the inputs do not fix them.
- **Setting up `Playtest`.** The host is launched with `?Tuning=Playtest`; `Playtest` is HostSelectable in the real catalog (story 056) and the cooked Development build honours the option (it is non-Shipping). The scenario never edits the store through a back door.
- **Observation.** Per round (travel 1, travel 2) the scenario drives the store's per-round resolve and records the resolved `PresetId` and `FallbackLevel` into the result JSON (`captures[]` of the story 018 format). Both must read `Playtest`. Because `Playtest` equals `Default` in value (story 056), the assertion is on `PresetId`, never on values. What triggers the per-round resolve is not defined, since the real Round epic and the Round stub (story 052, not a declared dependency of this story) do not exist here: the scenario calls the store's resolve directly. Open.
- **Forced `?Preset=Default`.** For the third travel, force the travel URL to carry `?Preset=Default` and assert: the store still resolves `Playtest`, and a warning about the disagreement is observed. The warning is read from the result JSON `captures[]` (QA spec), not scraped from a log file. A fourth observation (the QA edge case): a travel without an override afterwards still resolves `Playtest`. The text of the disagreement warning is open; the capture must name the key, as in other tuning warnings.
- **Result handling.** The Gauntlet node reads the host's JSON and asserts the three `PresetId` values and the warning; a missing JSON or a crashed process fails; wall clock is only a launch ceiling. The result also records `replicationSystem` (legacy assertion, story 018).
- **G4 / deferral.** This is DEFERRED in the QA spec (multi-process, Gauntlet). The story may be written before the cook exists but is not Done until the observation exists. If story 020 closes FAIL, this story closes with the recorded FAIL and the ADR-0008 amendment fallback and never by skipping.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the Gauntlet host-plus-client harness and its result JSON handling
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store itself
- `production/epics/tuning-data-config/story-081-travel-epoch.md`: snapshot invalidation and tuning-ready bookkeeping across travel
- `production/epics/tuning-data-config/story-061-preset-malformed.md`: malformed option values
- `production/epics/tuning-data-config/story-062-client-option-ignored.md`: client login options (in-process)
- `production/epics/tuning-data-config/story-064-presets-cooked.md`: the cooked load of `Playtest`

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Scenarios/PresetSurvivesTravelScenario.cpp`
- `Build/Scripts/StealthTuningTravelNode.cs`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the stored preset survives two `ServerTravel` round changes and wins over a forced `?Preset=Default` (test `StealthGame.Tuning.MP.PresetSurvivesTravel`; DEFERRED: multi-process, Gauntlet)
  - Given: the store holding `Playtest`, a listen host plus clients
  - When: two `ServerTravel` round changes occur; then the travel URL is forced to `?Preset=Default`
  - Then: both rounds resolve `Playtest`; with the forced URL the store still wins and a warning is logged
  - Edge cases: the warning is observed via the result JSON `captures[]`; a third travel without an override still resolves `Playtest`

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the T3 test `StealthGame.Tuning.MP.PresetSurvivesTravel` (`Source/StealthTestHarness/Private/Scenarios/PresetSurvivesTravelScenario.cpp` and the node `Build/Scripts/StealthTuningTravelNode.cs`) must exist and pass in the Gauntlet run, with the recorded observation (command, date, machine, result); a story 020 FAIL closes this story FAIL with the amendment request

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. It inherits ADR-0008 Verification 3 (Gauntlet topology on one machine, [U], story 020). Open items found while writing: what triggers the per-round resolve without a Round, the scenario name, and the text of the disagreement warning.
