# Story 048: AC-17 InvariantBoundaries: invariants I-1 to I-5

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-17 (invariant-table half), Formulas "Invariants" I-1 to I-5, CR-6 (c)

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 3, invariants table)
**ADR Decision Summary**: All cross-field invariants sit in one table, `TuningInvariants.cpp`, rows `{ Id, Owner, Check }`, where `Id` equals the GDD ID and `Check` is a pure function over the clamped `const` set, the derived indicators and a context (`bIsPublic`, `PresetId`) returning Pass, Warn or Reject. A Reject fails the whole preset, a Warn logs and keeps it; an invariant never writes a value.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API; plain C++ over the clamped value set; `UE_LOGF` for warnings (5.8). No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair (source: ADR-0004)
- Required: Every reject/warn invariant carries its GDD ID; a new cross-field constraint goes through a TDC change request and a row in `TuningInvariants.cpp` (source: ADR-0004)
- Forbidden: Never repair a failed invariant by clamping a dependent key (source: ADR-0004)
- Guardrail: Validation under 1 ms at `Preparing`; the validator suite runs headless in under 10 s (source: ADR-0004)

---

## Acceptance Criteria

*From the plan amendment for story 048 (REVISED), scoped to this story (the whole-preset and FallbackLevel clause is story 097):*

- [ ] (GDD AC-17, invariant-table half, test `StealthGame.Tuning.InvariantBoundaries`): over a test schema fixture holding only I-1..I-5 (owner invariants excluded), non-HostSelectable test presets except where stated, `NpcKillStrikeLimit` 1 with `bTestAllowsStrikeLimitOne` so I-4 cannot interfere (except the HostSelectable bullet, which uses limit 2 and no flag), and the I-5 pair being a fixture range pair `FixtureMinSec`/`FixtureMaxSec`: the invariant table returns Pass/Warn/Reject exactly as the GDD AC-17 bullets state for every listed value (DefenceDurationSec 5 with cooldown 40 pass and 39.9 reject; TeamSize 5/NpcCount 10 warn I-3 and 9 reject I-2; TeamSize 1/NpcCount 10 warn I-3; TeamSize 4/NpcCount 8 warn and 7 reject; HostSelectable TeamSize 1/NpcCount 21 reject I-3 and 22 pass I-4 0.085; Min greater than Max reject I-5; authored DefenceDurationSec 7.5 clamps to 5 and passes with cooldown 40). The end-to-end clause "each rejection rejects the whole preset at FallbackLevel 1" is story 097.

---

## Implementation Notes

*Derived from ADR-0004 section 3, Key Interfaces and GDD Formulas:*

- Files: `Public/Server/StealthTuningInvariants.h` (guard header first: `ETuningCheck`, `FTuningCheckContext`, `FTuningInvariant`, `GetTuningInvariants()`), `Private/Tuning/StealthTuningInvariants.cpp` (the table, `TuningInvariants.cpp` rows), test `Private/Tests/Tuning/InvariantBoundariesTest.cpp`, all in `Source/StealthFoundation/`. Shapes from the ADR Key Interfaces: `enum class ETuningCheck : uint8 { Pass, Warn, Reject }`; `struct FTuningCheckContext { bool bIsPublic; FName PresetId; }`; `struct FTuningInvariant { const ANSICHAR* Id; const ANSICHAR* Owner; ETuningCheck (*Check)(const FStealthServerTuning&, const FTuningDerived&, const FTuningCheckContext&); }`; `GetTuningInvariants()` returns the non-offline rows only. `FTuningDerived` is story 047.
- The five rows (GDD Formulas, evaluated on clamped values; re-read the GDD, it wins): I-1 `DefenceCooldownSec >= 8 * DefenceDurationSec` (reject); I-2 `NpcCount >= 2 * TeamSize` (reject); I-3 `2 * TeamSize + NpcCount >= DeckCopiesFloorCount * DeckActiveAppearancesMinCount` (24 at defaults; reject in `Default` and `HostSelectable` presets, warn in non-selectable presets); I-4 `100 * BlindGuessSuccess < BlindGuessWarnPct` (schema constant 50; reject for `HostSelectable` and `Default`, warn in non-selectable); I-5 `Min <= Max` for every registered range pair (reject). Each row's `Id` is exactly `I-1` to `I-5` so the report names it (AC-30 relies on this).
- Fixture requirements the inputs imply: I-3 reads `DeckCopiesFloorCount` and `DeckActiveAppearancesMinCount`, which are Character Pool owner keys not among the eight canonical keys (story 040). The AC-17 bullets use the 24 threshold (4 x 6 at the defaults), so the test schema fixture must supply those two keys with defaults 4 and 6 until the Character Pool epic registers them (open: whether the fixture carries them as fixture rows or the Check reads them through the fixture schema view). I-5 needs the fixture pair `FixtureMinSec` / `FixtureMaxSec`; how range pairs are recognised (the GDD says "`...MinSec`/`...MaxSec` and similar") is an implementation choice to document in the header (open).
- Test selection: the test evaluates only the I-1 to I-5 rows against the fixture (owner invariants excluded), by selecting rows by `Id` from `GetTuningInvariants()` or through an injected row view. The ADR does not fix the mechanism (open). It must not break when owner epics add rows later.
- Case values (all from the AC and the GDD bullets): `DefenceDurationSec` 5 with cooldown 40 passes, 39.9 rejects (I-1); `TeamSize` 5 with `NpcCount` 10 warns I-3 (20 < 24), 9 rejects (I-2); `TeamSize` 1 with `NpcCount` 10 warns I-3 (12 < 24); `TeamSize` 4 with `NpcCount` 8 warns I-3 (I-2 tie, 16 < 24), 7 rejects (I-2); `HostSelectable` `TeamSize` 1 with `NpcKillStrikeLimit` 2 and no flag: `NpcCount` 21 rejects I-3 (23 < 24), 22 passes with I-4 `BlindGuessSuccess` 0.085; Min greater than Max rejects I-5; authored `DefenceDurationSec` 7.5 clamps to 5 (via F-1, story 046) and passes with cooldown 40.
- Non-selectable presets use `NpcKillStrikeLimit` 1 with `bTestAllowsStrikeLimitOne` so I-4 cannot interfere; the flag reaches F-1 as in story 046 (open carrier).
- Purity: `Check` takes `const` input and writes nothing (CR-6); the test asserts the inputs are unchanged after evaluation (copy before, compare after).
- Negative control: a row variant that writes a value or returns the wrong severity must be detected (use `FStealthNegativeControl` once story 010 has landed; not a listed dependency).
- Test class `FTuningInvariantBoundariesTest`, name `StealthGame.Tuning.InvariantBoundaries`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`, headless `-nullrhi`. Warnings to `LogStealthFoundation` via `UE_LOGF`.
- Prototype: none (no `Prototype:` line in the plan).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-046-f1-validate-clamp-nan.md`: F-1 clamping (the 7.5 to 5 clamp relies on it).
- `production/epics/tuning-data-config/story-047-derived-indicators.md`: `FTuningDerived`.
- `production/epics/tuning-data-config/story-097-invariants-reject-whole-preset.md`: the end-to-end "rejects the whole preset at FallbackLevel 1" clause through `Resolve()`.
- `production/epics/tuning-data-config/story-098-derived-indicators-preset-scope.md`: AC-18 scope clauses and the `TeamSize` 5 / `NpcCount` 20 versus 19 fixture.
- `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`: `Resolve()` and the fallback ladder.
- `production/epics/tuning-data-config/story-055-owner-invariant-mechanism.md`: owner-invariant rows (Pass/Warn/Reject with context), AC-30 mechanism.
- `production/epics/tuning-data-config/story-096-owner-invariants-full-schema-closure.md`: closure over all owner invariants.

---

## Files

- `Source/StealthFoundation/Public/Server/StealthTuningInvariants.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningInvariants.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/InvariantBoundariesTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: The invariant table returns Pass, Warn or Reject exactly as the AC-17 bullets state.
  - Given: a test schema fixture with I-1..I-5 only, I-5 pair `FixtureMinSec`/`FixtureMaxSec`; non-selectable presets with `NpcKillStrikeLimit` 1 and `bTestAllowsStrikeLimitOne` (the HostSelectable bullet uses limit 2, no flag).
  - When: the invariant table is evaluated for each AC-17 value.
  - Then: `DefenceDurationSec` 5 cooldown 40 Pass, 39.9 Reject; `TeamSize` 5/`NpcCount` 10 Warn I-3, 9 Reject I-2; `TeamSize` 1/`NpcCount` 10 Warn I-3; `TeamSize` 4/`NpcCount` 8 Warn, 7 Reject; HostSelectable `TeamSize` 1/`NpcCount` 21 Reject I-3, 22 Pass (I-4 0.085); Min greater than Max Reject I-5; authored 7.5 clamps to 5, Pass with cooldown 40.
  - Edge cases: the whole-preset/FallbackLevel clause is story 097; checks are pure and write no value.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/InvariantBoundariesTest.cpp` (`StealthGame.Tuning.InvariantBoundaries`) must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-047-derived-indicators.md`, `production/epics/tuning-data-config/story-046-f1-validate-clamp-nan.md`
- Unlocks: `production/epics/tuning-data-config/story-050-resolve-fallback-to-default.md`, `production/epics/tuning-data-config/story-055-owner-invariant-mechanism.md`, `production/epics/tuning-data-config/story-065-catalog-clean.md`, `production/epics/tuning-data-config/story-094-safe-range-lint-baseline.md`, `production/epics/tuning-data-config/story-097-invariants-reject-whole-preset.md`, `production/epics/tuning-data-config/story-098-derived-indicators-preset-scope.md`
- Open verification claims from the plan Note lines: none. The plan Note ("Check functions are pure, take const input and never write a value (CR-6)") is a rule, not an open claim. Open design points: the I-3 Character Pool key supply in the fixture, range-pair recognition, and the fixture row-selection mechanism.
