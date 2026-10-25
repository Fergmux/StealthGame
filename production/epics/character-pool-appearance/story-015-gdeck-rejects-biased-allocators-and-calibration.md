# Story 015: G-DECK rejects the biased allocators B1 to B4 and is calibrated nightly

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-007 = Statistical indistinguishability gate G-DECK automated.)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: A statistical gate is trusted only after it is watched rejecting known-bad implementations; the suite must reject each biased allocator on the statistics the GDD names, and the false-failure rate of the suite over fresh seed sets is bounded.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008); plain C++
**Engine Notes**: No engine API. The nightly test is not in the smoke set (it is registered with the nightly filter).

**Why this story**: "Passing AC-CP-04 while failing AC-CP-05 is a failure of the harness" (GDD). This story makes the suite falsifiable. B1 weighted dealing, B2 the retired duplicates rule, B3 teammates forbidden to share, B4 extras on the first appearances. Read GDD F-8 (the "Catches" column and B1 to B4 definitions) and AC-CP-04b, AC-CP-05.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Test-only allocators B1 to B4 live only under `Private/Tests/` (never linked into the game module) (ADR-0012, ADR-0008)
- Forbidden: Never reseed; never disable a failing test (testing standards: "a gate you have not watched fail is not a gate")
- Guardrail: Calibration runs nightly, not per commit

---

## Acceptance Criteria

*From GDD AC-CP-05 and AC-CP-04b:*

- [ ] **AC-1** GIVEN allocators B1 (weighted dealing), B2 (the retired duplicates rule), B3 (teammates forbidden to share) and B4 (extras on the first appearances), WHEN the same suite as story 014 runs, THEN B1 is rejected by T1, B2 by T1 and T5, B3 by T1, T5 and T6, B4 by T7 (the named statistics, not merely "any"); passing story 014 while any of these is not rejected is a harness failure (test `StealthGame.CharacterPool.Deck.SuiteRejectsBiasedAllocators`).
- [ ] **AC-2** GIVEN 100 other seed sets `CharacterPoolTestSeeds::CalibrationSets`, WHEN the suite runs at alpha 0.001, THEN at most 4 suite failures occur (literal 4 in the test; nightly, not in the smoke set) (test `...Deck.HarnessCalibration`).

---

## Implementation Notes

*Derived from GDD F-8:*

- Implement B1 to B4 as small test-only functions with the same signature as the production deal; each is the minimal departure that makes the named leak real (B4 puts the `r` extras on the first `r` appearances in id order, and so on).
- A seeded fixture where a defect is the thing under test must actually exercise the defect (the "vacuous fixture" lesson in `.claude/rules/test-standards.md`): at `D = 50`, `r = 2`, so T1 and T7 are live; verify by watching B1 and B4 get rejected.
- The calibration run may take minutes; tag it nightly and keep its seed sets constexpr.

**Prototype disposition**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 014: the suite on the correct allocator

---

## Files

- `Source/StealthGameplay/Private/Tests/CharacterPool/BiasedAllocators.h`
- `Source/StealthGameplay/Private/Tests/CharacterPool/SuiteRejectsBiasedAllocatorsTest.cpp`
- `Source/StealthGameplay/Private/Tests/CharacterPool/HarnessCalibrationTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead).*

**Test: biased allocators rejected**
- Given: B1 to B4 as test-only allocator implementations.
- When: the same suite as story 014 runs on each.
- Then: B1 is rejected by T1, B2 by T1 and T5, B3 by T1, T5 and T6, B4 by T7; if the story 014 suite passes while any of these is not rejected, the harness fails.
- Edge cases: each rejection is on the named statistics only.

**Test: harness calibration (nightly)**
- Given: 100 seed sets in `CharacterPoolTestSeeds::CalibrationSets`.
- When: the suite runs at alpha 0.001.
- Then: at most 4 suite failures.
- Edge cases: the count 4 is a literal in the test, not derived.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/SuiteRejectsBiasedAllocatorsTest.cpp`, `HarnessCalibrationTest.cpp` — automation tests `StealthGame.CharacterPool.Deck.SuiteRejectsBiasedAllocators`, `...Deck.HarnessCalibration` must exist and pass (`/story-done` checks they exist; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-014-gdeck-statistical-suite-on-allocator.md`
- Unlocks: None
