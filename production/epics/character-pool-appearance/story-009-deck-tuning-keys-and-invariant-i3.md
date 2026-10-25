# Story 009: Deck tuning keys registered with TDC and invariant I-3

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-002 = `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: The deck is built from `FDeckParams {CopiesFloor, ActiveMin, ActiveMax}` read from `FCharacterPoolTuning_Server` (`DeckCopiesFloorCount`, `DeckActiveAppearancesMinCount`, `DeckActiveAppearancesMaxCount`) and injected into the core; the keys live in the TDC registry (ADR-0004, Accepted) under the CR-15 naming convention.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012); this story is data and validation only
**Engine Notes**: No engine API beyond the tuning schema's own types. TDC is not coded yet: use a validator fake and re-verify when the TDC validator lands.

**Why this story**: Gameplay values must be data-driven (coding standards); the allocator may hard-code none of the deck keys. Read GDD Tuning Knobs (keys, ranges, invariant I-3) by section, and the TDC Knob Registry Convention (CR-15) in `design/gdd/tuning-data-config.md`.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The allocator receives the three deck values only through injected `FDeckParams` (ADR-0012); keys follow TDC CR-15 and are ServerOnly, Round-latched
- Forbidden: Never hard-code a gameplay value; never read tuning through a config/ini back door (TDC CR-13)
- Guardrail: Validation at preset load; no per-frame cost

---

## Acceptance Criteria

*From GDD Tuning Knobs (keys, hard ranges, invariant I-3), scoped to this story:*

- [ ] **AC-1** GIVEN the TDC registry, THEN `DeckCopiesFloorCount` (default 4, hard 3 to 6), `DeckActiveAppearancesMaxCount` (12, 6 to 16) and `DeckActiveAppearancesMinCount` (6, 5 to 12) exist as ServerOnly, Round-latched keys in `FCharacterPoolTuning_Server`, and values outside the hard range are rejected by the TDC validator.
- [ ] **AC-2** GIVEN TDC invariant I-3 (`2 * TeamSize + NpcCount >= DeckCopiesFloorCount * DeckActiveAppearancesMinCount`, 24 at defaults), THEN `Default` and `HostSelectable` presets that violate it fail CI and are rejected at runtime, while non-selectable test presets (`Solo`, `TeamSize` 1) pass with a warning.
- [ ] **AC-3** GIVEN Min and Max, THEN `Min <= Max` is enforced by the existing TDC I-5 (no new invariant) and the allocator receives the three values only through an injected `FDeckParams` (its includes show no TDC dependency).

---

## Implementation Notes

*Derived from the GDD Tuning Knobs section and ADR-0012 §2:*

- Registry rows carry: type int, owner Character Pool & Appearance, default, hard range, safe range (4 to 5, 8 to 12, 6 to 8), audience ServerOnly, latch Round, unit count. Effects text per the GDD row.
- I-3 replaces TDC's provisional I-3 (change request in the GDD); the "scoped like I-4" rule decides which presets fail versus warn.
- Do not add `PoolResidentTimeoutSec` (retired in design review 2026-10-24).

**Prototype disposition**: replaces the prototype's deck constants in `StealthSettings.h` / `DefaultGame.ini`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010: using the values to size the deck; the TDC validator pipeline itself (tuning-data-config epic)

---

## Files

- `Source/StealthFoundation/Public/View/Tuning/CharacterPoolTuning.h` (location per ADR-0004; confirm with the TDC epic)
- `Config/Tuning/` preset data (per TDC)
- `Source/StealthGameplay/Private/Tests/CharacterPool/DeckKeysAndInvariantI3Test.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Test `StealthGame.CharacterPool.Tuning.DeckKeysAndInvariantI3`.*

**Test: deck key registry and ranges**
- Given: the TDC registry and `FCharacterPoolTuning_Server`.
- When: the keys are enumerated and set to each boundary.
- Then: three keys exist as ServerOnly and Round-latched; Floor 2 and 7 rejected, 3 and 6 accepted; Max 5 and 17 rejected, 6 and 16 accepted; Min 4 and 13 rejected, 5 and 12 accepted.
- Edge cases: the defaults 4, 12, 6 are accepted.

**Test: invariant I-3**
- Given: presets with `2 * TeamSize + NpcCount` at 23 and at 24 under Floor 4 and Min 6.
- When: validating Default, HostSelectable and non-selectable presets.
- Then: 23 fails and 24 passes for Default and HostSelectable; a non-selectable Solo preset with TeamSize 1 passes with a warning; a CI lint over the shipped presets fails on a seeded violating asset.
- Edge cases: Floor or Min changed so the threshold moves.

**Test: Min <= Max and injected params**
- Given: Min 7, Max 6, and an allocator called with `FDeckParams` of Floor 3, Max 8, Min 5.
- When: validating, then allocating.
- Then: Min > Max is rejected by TDC I-5; the allocator output reflects the injected values, not the defaults.
- Edge cases: the allocator has no TDC include.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/CharacterPool/DeckKeysAndInvariantI3Test.cpp` — automation test `StealthGame.CharacterPool.Tuning.DeckKeysAndInvariantI3` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-001-foundation-module-skeleton.md`, `production/epics/tuning-data-config/story-002-gameplay-presentation-module-skeletons.md`
- Unlocks: `production/epics/character-pool-appearance/story-010-deck-sizing-and-floor-policy-f1-f3.md`
