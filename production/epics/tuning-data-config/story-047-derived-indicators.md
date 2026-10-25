# Story 047: AC-18 DerivedIndicators: the F-3 indicators

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-004` (requirement text lives in `docs/architecture/tr-registry.yaml`; read fresh at review time); GDD AC-18 (indicator clauses), F-3, I-4

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 3, `FTuningDerived`)
**ADR Decision Summary**: Invariant checks read an `FTuningDerived` that holds the F-3 indicators computed in double precision on the clamped set; checks are pure and never write a value. `BlindGuessWarnPct` is a schema constant (50), not a preset field, so no preset can loosen its own I-4 gate.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: No post-cutoff API; pure double-precision arithmetic and one reflection lookup. No unverified claim is touched.

**Control Manifest Rules (this layer)**:
- Required: Invariant `Check` functions are pure and take `const` input; only F-1 changes a value (to the key's hard range); invariants reject or warn, never repair (source: ADR-0004)
- Required: Every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct; a hardcoded gameplay literal is a defect (source: ADR-0004)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores with no `UWorld` dependency (source: ADR-0007)
- Guardrail: Validation under 1 ms at `Preparing` (source: ADR-0004)

---

## Acceptance Criteria

*From the plan amendment for story 047 (REVISED), scoped to this story (the preset-scope clauses of AC-18 are story 098):*

- [ ] (GDD AC-18, indicator clauses, test `StealthGame.Tuning.DerivedIndicators`): at defaults CrowdRatio is 5.0, DutyCycle within 0.0001 of 0.0667, BlindGuessSuccess within 0.001 of 0.249 and ExpectedBlindKills within 0.001 of 0.300; for NpcCount 15 and NpcKillStrikeLimit 3 BlindGuessSuccess is within 0.001 of 0.578; and no preset field named `BlindGuessWarnPct` exists (reflection). The preset-scope clauses of AC-18 are story 098.

---

## Implementation Notes

*Derived from GDD F-3 and ADR-0004 section 3:*

- Files: `Public/Server/StealthTuningDerived.h` (guard header first, defines `FTuningDerived`), `Private/Tuning/StealthTuningDerived.cpp`, test `Private/Tests/Tuning/DerivedIndicatorsTest.cpp`, in `Source/StealthFoundation/`. Plain C++, no `UWorld`.
- Formulas (GDD F-3, double precision, `L = NpcKillStrikeLimit`): `CrowdRatio = NpcCount / (2 * TeamSize)`; `DutyCycle = DefenceDurationSec / DefenceCooldownSec`; `p = TeamSize / (TeamSize + NpcCount)`; `BlindGuessSuccess = 1 - (1 - p)^L`; `ExpectedBlindKills = L * p / (1 - p)`; `MissToHit = NpcCount / TeamSize`. Inputs come from the clamped set (so `1 - p >= 2/3` and there is no division by zero once I-2 holds; the function itself must still not divide by zero on unclamped input, but F-1 always runs first in the pipeline).
- Expected values from the GDD examples. At defaults (TeamSize 5, NpcCount 50, L 3, duration 4.0, cooldown 60): `CrowdRatio = 50 / 10 = 5.0`, `DutyCycle = 4 / 60 = 0.0667`, `p = 5/55 = 0.0909`, `BlindGuessSuccess = 1 - 0.9091^3 = 0.249`, `ExpectedBlindKills = 0.300`, `MissToHit = 10`. For NpcCount 15, TeamSize 5, L 3: `p = 0.25`, `BlindGuessSuccess = 1 - 0.75^3 = 0.578`, `ExpectedBlindKills = 1.0`. Take the defaults from the real schema (story 040), not from literals in the code under test; the expected numbers are literals in the test (boundary-value exception to the no-magic-numbers rule, the exact number is the point).
- `BlindGuessWarnPct` is the schema constant from story 038 (`StealthTuningConstants.h`). The reflection check: walk `UStealthTuningData` and every sub-struct and assert no property named `BlindGuessWarnPct` exists. This story only computes indicators; the I-4 threshold comparison and the accept/reject scope (`HostSelectable`, `Default`, non-selectable) are story 098, after the invariant mechanism of story 048 and `Resolve()` of story 050 exist.
- Test class `FTuningDerivedIndicatorsTest`, name `StealthGame.Tuning.DerivedIndicators`, flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`, whole file in `#if WITH_DEV_AUTOMATION_TESTS`, headless `-nullrhi`. Tolerances as stated (0.0001 for DutyCycle, 0.001 for the others).
- Ordering note resolved by the plan amendment: the original plan had a circular dependency (the accept/reject half needed the story 048 context while 048 depends on 047); the accept/reject clauses moved to story 098 so this story needs only story 040.
- Prototype: none (no `Prototype:` line in the plan).

---

## Out of Scope

*Handled by neighbouring stories, do not implement here:*

- `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`: the eight canonical keys read by this story.
- `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`: invariants I-1 to I-5 (they consume `FTuningDerived`).
- `production/epics/tuning-data-config/story-098-derived-indicators-preset-scope.md`: accept-with-warning and reject scope clauses of AC-18 (non-selectable, `HostSelectable`, `Default`, and the `TeamSize` 5 / `NpcCount` 20 versus 19 fixture).
- `production/epics/tuning-data-config/story-038-schema-row-types.md`: the `BlindGuessWarnPct` constant itself.

---

## Files

- `Source/StealthFoundation/Public/Server/StealthTuningDerived.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningDerived.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/DerivedIndicatorsTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these, do not invent new test cases during implementation.*

- **AC-1**: The F-3 indicators match the GDD values and no `BlindGuessWarnPct` preset field exists.
  - Given: defaults; `NpcCount` 15 with `NpcKillStrikeLimit` 3.
  - When: the F-3 indicators are computed.
  - Then: CrowdRatio 5.0; DutyCycle within 0.0001 of 0.0667; BlindGuessSuccess within 0.001 of 0.249; ExpectedBlindKills within 0.001 of 0.300; `NpcCount` 15 / limit 3 gives BlindGuessSuccess within 0.001 of 0.578; reflection finds no preset field named `BlindGuessWarnPct`.
  - Edge cases: preset-scope accept/reject is story 098.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/DerivedIndicatorsTest.cpp` (`StealthGame.Tuning.DerivedIndicators`) must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`
- Unlocks: `production/epics/tuning-data-config/story-048-invariants-i1-i5.md`, `production/epics/tuning-data-config/story-098-derived-indicators-preset-scope.md`
- Open verification claims from the plan Note lines: none. The plan Note (the circular dependency on story 048's context) is resolved by the Amendments (REVISED 047 and NEW 098), not an open claim.
