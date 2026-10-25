# Story 064: AC-27 PresetsCooked (cooked Dev build)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-001` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-27, CR-2

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. The presets are data assets that only the catalog references, so a cooked build can silently omit one and a host choosing it would fall back to `Default`. Story 057 registers the two primary types with the `AlwaysCook` rule; this story proves the effect in a cooked Development build, and in doing so observes ADR-0004 Verification Required (2) for the first time. It is a spike-style verification story (amendment G4): it closes PASS, or it closes with a recorded FAIL, a written amendment request and the named fallback, and it never closes by skipping or disabling the check. The cooked Development build also repeats the no-option observation of AC-1 (story 058), as the amendment to story 058 states.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The catalog and every preset are registered as Asset Manager primary types with cook rule `AlwaysCook`, so a preset referenced only by the catalog is cooked; `HostSelectable` is honoured in Development and Test.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: `EPrimaryAssetCookRule::AlwaysCook` exists [V `AssetManagerTypes.h:28-53`]. ADR-0004 Verification Required (2), that presets referenced only by the catalog are cooked under the rule, is [U] and is the point of this story. ADR-0004 also tags "soft references held by an always-cooked asset are followed by the cooker" [U] and does not rely on it, because presets are registered `AlwaysCook` themselves; the negative control below touches exactly this question and its outcome must be recorded as found. The harness is a cooked Development process, so ADR-0008 Verification 3 (Gauntlet topology, [U], story 020) applies.

**Control Manifest Rules (this layer)**:
- Required: test names `StealthGame.<System>.MP.<Scenario>`; T3 code lives in `StealthTestHarness`, Gauntlet nodes under `Build/Scripts/` — source: ADR-0008
- Required: T2/T3 tests assert and record the active replication system and are bounded by frames or server time — source: ADR-0008
- Forbidden: never disable, skip or reseed a failing test to pass a gate; a spike never closes by skipping — source: ADR-0008
- Guardrail: nightly cook plus T3 estimated at 1 to 2 hours [U] — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-27 (CR-2)**: GIVEN a cooked Dev build in which `Playtest` is referenced only by the catalog, WHEN the host opens with `?Tuning=Playtest` (`HostSelectable`), THEN `Playtest` loads with `FallbackLevel` 0. (`StealthGame.Tuning.PresetsCooked`)

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Loading) and section 9, and the G4 spike rule:*

- **Files.** `Source/StealthTestHarness/Private/Scenarios/PresetsCookedScenario.cpp` (the scenario run inside the cooked host) and `Build/Scripts/StealthTuningCookNode.cs` (a Gauntlet node that cooks, launches the host with `?Tuning=Playtest`, reads the result JSON and asserts). The scenario records the loaded `PresetId` and `FallbackLevel` in the result JSON of story 018; the node asserts `PresetId` `Playtest` and `FallbackLevel` 0. Because `Playtest` equals `Default` in value (story 056), the assertion must be on `PresetId`, which is also why `FallbackLevel` 0 matters: a fallback would show `Default` with level 1.
- **"Referenced only by the catalog".** Verify the setup: no other asset, map or code path in the cooked project references the `Playtest` asset. Record how this was checked (for example a referencer query in the editor) in the observation, so the test cannot pass because of an incidental reference.
- **Negative control (QA edge case).** On a copy, remove the `AlwaysCook` rule from the Asset Manager settings and run the same scenario; the expected outcome is that `Playtest` is missing from the cook and the host falls back to `Default` (level 1) with a warning. The suite passes only if that control is detected. Caution: the ADR marks "soft references held by an always-cooked asset are followed by the cooker" as [U]; if the cooker follows the catalog's soft reference anyway, the control will not fall back. If so, do not adjust the setup to force a fallback: record that finding, and record that the rule is then not what keeps the preset cooked, as an ADR-0004 amendment note. The setup for removing the rule uses a disposable copy, never the developer tree.
- **G4 closure.** PASS: the node asserts `Playtest` at level 0 in the cooked Development host, and the control result is recorded; write the observation (command, date, machine, result) into this story as the ADR-0004 Verification Required (2) record. FAIL: record the FAIL, write an amendment request to ADR-0004 naming a fallback (the ADR names none for this item, so the fallback is open and is proposed in the amendment request itself), and close FAIL; never close by skipping. The QA spec marks this DEFERRED: it needs a cooked Development build.
- **Also observe (story 058 amendment).** In the same cooked run, the no-`?Tuning=` start must give `Default`, `FallbackLevel` 0 and no warning; record it as the cooked repeat of AC-1.
- **Bounds.** Pass/fail uses process exit and the JSON only; wall clock is a launch and cook ceiling.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-057-asset-manager-primary-types.md`: the primary types and cook rule configuration
- `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`: the preset and catalog assets
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the Gauntlet harness and result JSON conventions
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store and the editor-automation AC-1 test
- `production/epics/tuning-data-config/story-089-no-shipping-back-door-scan.md`: Shipping behaviour (the Shipping cook of non-Default presets is deferred by GDD OQ-13)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Scenarios/PresetsCookedScenario.cpp`
- `Build/Scripts/StealthTuningCookNode.cs`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a preset referenced only by the catalog loads in a cooked Development build (test `StealthGame.Tuning.PresetsCooked`; DEFERRED: cooked Development build)
  - Given: a cooked Dev build where `Playtest` is referenced only by the catalog (HostSelectable)
  - When: the host opens with `?Tuning=Playtest`
  - Then: `Playtest` loads with `FallbackLevel` 0
  - Edge cases: negative control: remove the `AlwaysCook` rule on a copy and watch it fall back; records ADR-0004 Verification Required (2)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the T3 scenario and node (`Source/StealthTestHarness/Private/Scenarios/PresetsCookedScenario.cpp`, `Build/Scripts/StealthTuningCookNode.cs`) must exist and pass in a cooked Development run, with the recorded observation and the negative-control result in this story; a recorded FAIL plus amendment request is the only other valid closure (G4)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-057-asset-manager-primary-types.md`, `production/epics/tuning-data-config/story-056-default-preset-assets-and-catalog.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: None
- Open verification claims: the plan Note says this story verifies ADR-0004 Verification Required (2) ([U] until observed here). The catalog-soft-reference-followed-by-cooker claim is also [U] and not relied on. Inherits ADR-0008 Verification 3 (story 020). Open items found while writing: the ADR names no fallback for a FAIL of Verification (2), and how "referenced only by the catalog" is proven.
