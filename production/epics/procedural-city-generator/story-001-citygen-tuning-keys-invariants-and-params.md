# Story 001: CityGen tuning keys, invariants I-PCG-1..7 and params struct

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
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. The keys feed `LayoutParamsHash`, which is the "params" input of the pure function in TR-citygen-001.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (the keys come from ADR-0004, the tuning pipeline)
**ADR Decision Summary**: A provider reads only `FLayoutAttemptParams`: `AttemptSeed`, `LayoutAttempt`, `GeneratorKitVersion`, `LayoutParamsHash` and a plain copy of the `CityGen*` and LSC keys taken from the frozen tuning snapshot (ADR-0004). It has no tuning-service access, so purity (LSC R2.4) holds.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; this story is data and plain structs)
**Engine Notes**: No post-cutoff API. Schema, lint and owner-invariant machinery belong to the tuning-data-config epic; this story adds the Generator's 32 rows and seven invariants to it. No ADR-0014 verification item gates it.

**Control Manifest Rules (this layer)**:
- Required: Providers read only `FLayoutAttemptParams`; every `CityGen*` key is ServerOnly with latch `Round` — source: ADR-0014, ADR-0004
- Forbidden: Never hardcode a gameplay value that has a key; never repair a violating tuning set at runtime (reject at load) — source: ADR-0004
- Guardrail: invariant checks run at load, never per frame

Every `CityGen*` key enters `LayoutParamsHash`, so changing one changes `LayoutId` and requires regenerated goldens (story 031). The Generator does not use `TargetRoundMinutes` (TDC OQ-7 answered: not a sizing hint).

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (Tuning Knobs, R-G2.1) and `design/gdd/tuning-data-config.md` (I-PCG-1 to I-PCG-7), scoped to this story:*

- [ ] **AC1** (`Keys.Registered`): **GIVEN** the TDC schema **WHEN** the CR-15 lint runs **THEN** every `CityGen*` key of the GDD Tuning Knobs table (32 keys) exists with owner Generator, audience ServerOnly, latch Round, its default and hard range (for example `CityGenGridSideMinCount` 62 / `MaxCount` 66 hard 56-76, `CityGenFinalEligibleTargetCount` 6 hard 3-8, `CityGenCourtTargetCount` 16 hard 16-20, `CityGenGPrePathGuardPct` 8 hard 0-15).
- [ ] **AC2** (`Keys.InvariantsRejected`): **GIVEN** presets that each break exactly one rule and stay inside every hard range: I-PCG-1 (`CityGenGridSideMinCount` 66 with `CityGenGridSideMaxCount` 62), I-PCG-2 (`CityGenWitnessCellsTargetCount` 30 with `EmergenceWitnessCellsMaxCount` lowered to 28, hard range 10-55), I-PCG-3 (`CityGenSkeletonFairnessMaxMult` 1.08 with `ChainFairnessMaxMult` 1.08, the invariant needs strictly below), I-PCG-5 (`CityGenFinalEligibleTargetCount` 5 with `FinalCandidatesMinCount` 6), I-PCG-6 (`CityGenCourtTargetCount` 16 with `PrelimCandidatesPerStageMinCount` raised to 4, floor 6 + 16 - 2 = 20) and I-PCG-7 (`CityGenZoneCandidatesCount` 2 with `ZoneCandidatesMinCount` 3) **WHEN** loaded **THEN** each preset is rejected whole naming exactly that invariant; **AND** `CityGenCourtTargetCount` 15 is handled by the TDC range rule (named key, not an invariant), I-PCG-4 holds at the hard-range maximum 6 (loads), and the default set loads with no warning.
- [ ] **AC3** (`Keys.ParamsHash`): **GIVEN** the frozen server snapshot **WHEN** `FCityGenParams` is built from `FLayoutParams` **THEN** it needs no tuning-service access, changing any one `CityGen*` key changes `LayoutParamsHash`, and `TargetRoundMinutes` is not an input.

---

## Implementation Notes

*Derived from ADR-0014 section 2 and the GDD Tuning Knobs table:*

The 32 keys (29 table rows, three of them min/max pairs) are listed in the GDD Tuning Knobs table; the registry mirror is `design/registry/entities.yaml`. Register them in the TDC schema with owner Generator, `ServerOnly`, latch `Round`, and the table's default, hard and safe ranges. The seven owner invariants are TDC Formulas rows I-PCG-1 to I-PCG-7 (all reject-class): I-PCG-1 `GridSideMin <= GridSideMax`; I-PCG-2 `CityGenWitnessCellsTargetCount <= EmergenceWitnessCellsMaxCount`; I-PCG-3 `CityGenSkeletonFairnessMaxMult < ChainFairnessMaxMult`; I-PCG-4 `CityGenFacadeIdenticalRunMaxCount <= 6` (the hard range 4-6 makes it hold at the boundary only); I-PCG-5 `CityGenFinalEligibleTargetCount >= FinalCandidatesMinCount`; I-PCG-6 `CityGenCourtTargetCount >= FinalCandidatesMinCount + 4 * PrelimCandidatesPerStageMinCount - 2` (16 at the defaults 6 + 4 x 3 - 2; the `4` is a schema constant); I-PCG-7 `CityGenZoneCandidatesCount >= ZoneCandidatesMinCount`. The test fixture builder must assert "exactly one violation" so each preset proves one invariant. `FCityGenParams` is a plain struct (no `UObject`) built once at `RequestLayout` from `FLayoutParams`; the hash is the TDC hasher over the frozen values (ADR-0004), not a Generator-local hash.

Prototype replaced: any `StealthSettings` fields the prototype generator read (grid size, house density) are retired by `tuning-data-config/story-095-retire-prototype-settings-canonical-fields.md`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Schema machinery, generator and fingerprint: tuning-data-config epic (`story-038` to `story-044`, `story-055-owner-invariant-mechanism.md`).
- `story-004-kit-tables-and-generator-kit-version.md`: `GeneratorKitVersion` itself.
- `story-031-golden-seeds-and-bump-rule.md`: regenerating goldens when a key changes.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/Tuning/StealthCityGenParams.h` (parameter struct; final home set by the TDC schema story)
- `Source/StealthFoundation/Private/Tuning/StealthCityGenKeys.cpp` (key rows and invariants I-PCG-1 to I-PCG-7)
- `Source/StealthGameplay/Private/Tests/CityGen/KeysRegisteredTest.cpp` (class `FCityGenKeysRegisteredTest`, name `StealthGame.CityGen.Keys.Registered`)
- `Source/StealthGameplay/Private/Tests/CityGen/InvariantsRejectedTest.cpp` (class `FCityGenKeysInvariantsRejectedTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/ParamsHashTest.cpp` (class `FCityGenKeysParamsHashTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after the revision of AC2).*

- **AC1** `Keys.Registered`
  - Given: the real TDC schema.
  - When: the CR-15 lint runs.
  - Then: 32 `CityGen*` keys each have owner Generator, audience ServerOnly, latch Round, and default and hard range equal to the GDD table. Spot-check `GridSideMin`/`GridSideMax` 62/66 hard 56-76, `FinalEligibleTarget` 6 hard 3-8, `CourtTarget` 16 hard 16-20, `GPrePathGuardPct` 8 hard 0-15.
  - Edge cases: a 33rd or missing key fails. A fixture with one key set to the wrong audience fails (negative control). The test reads a checked-in expected-key fixture, never the GDD markdown at run time.
- **AC2** `Keys.InvariantsRejected`
  - Given: six presets, each breaking one rule (I-PCG-1, -2, -3, -5, -6, -7 as in the AC), plus three more fixtures: `CourtTarget` 15, I-PCG-4 at 6, and the default set.
  - When: each is loaded.
  - Then: each of the six is rejected whole and the reported violation set equals exactly {that invariant}; no value is clamped or repaired. `CourtTarget` 15 is rejected as a range error naming the key, with no I-PCG-6 in the list. I-PCG-4 at 6 loads. The default set loads with 0 warnings, checked by log capture.
  - Edge cases: the fixture builder asserts "exactly one violation", which proves every key stays inside its hard range (including the LSC keys) and no other invariant fires. I-PCG-3 at 1.07 against 1.08 loads, which shows the check is strict. Default `PrelimMin` 3 gives floor 16 and loads.
- **AC3** `Keys.ParamsHash`
  - Given: a frozen `FLayoutParams` fixture and a world with no tuning-service instance.
  - When: `FCityGenParams` is built.
  - Then: the build succeeds. Changing each of the 32 keys by one step gives a different `LayoutParamsHash`, 32 comparisons in all. Changing `TargetRoundMinutes` leaves both the hash and the struct unchanged.
  - Edge cases: the min and max halves of a pair key are perturbed separately, always inside the hard range.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: cross-epic `tuning-data-config/story-038-schema-row-types.md` to `story-044-schema-complete.md`, `story-055-owner-invariant-mechanism.md`; `level-socket-contract/story-001-service-skeleton-provider-registry.md` (`FLayoutParams`)
- Unlocks: `production/epics/procedural-city-generator/story-002-provider-shell-registration-and-secret-input-isolation.md` (reads `FCityGenParams`), `story-005-grid-model-s1-frame-and-f1-walkable-target.md` and every stage story
