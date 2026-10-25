# Story 038: Tier caps and a client-local tie key that cannot order players apart from NPCs

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R11.2 caps and `TierTieKey`, R11.4 owner's pawn inside the caps, F8; AC-SCL-28)
**Requirement**: `TR-scl-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (section 2, steps 3 and 4)
**ADR Decision Summary**: Candidates are sorted by `h` descending, ties by `TierTieKey`, then by a per-process registration counter only to make the sort total; tier A fills to `AnimTierACapCount` (8), overflow drops to B, B to 14, C to 28, overflow to D-visible; caps override holds; the owner's own pawn participates like any pawn. `TierTieKey` is a `uint32` drawn when a pawn's client actor is first registered, from a client-local `FIntStream` (ADR-0006) seeded once per world from the local `IEntropySource`; never derived from spawn index, NetGUID, actor name or registration order, never replicated; a re-opened actor draws a new key.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Plain C++ inside `FAnimTierCore`; the draw stream is a client-local presentation stream (not a secret server stream). Registration order exists only as a sort-totalising counter, so the statistical tests must show it does not leak into tier assignment.

**Control Manifest Rules (this layer)**:
- Required: ties break on the client-local `TierTieKey`; tier subsystem at most 0.05 ms GT.
- Forbidden: never let spawn index, NetGUID, actor name, registration order or controller enter a tier decision; never replicate the tie key.
- Guardrail: caps 8 / 14 / 28.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-28** [U] `TierCapsTieKey`: GIVEN 12 pawns at identical h inside 8 m, one of them a player pawn, WHEN tiers are assigned, THEN exactly 8 are in A chosen by the client-local `TierTieKey`; over 10,000 draws from an injected fixed-seed RNG the player pawn's tier-A frequency equals an NPC's within ± 3 %; AND over 1,000 pawns Spearman |ρ| < 0.1 between `TierTieKey` and spawn order.

---

## Implementation Notes

*Derived from ADR-0013 section 2:*

- Extend `FAnimTierCore` with the cap fill (A, B, C, overflow to D-visible) and the sort `(h desc, TierTieKey, registration counter)`. Draw `TierTieKey` through an injected `FIntStream` so tests are deterministic with a fixed seed.
- The 10,000-draw frequency test redraws keys each trial; the Spearman test correlates key with spawn order over 1,000 pawns. Neither the registration counter nor any kind may bias the result.
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037: the tier formula. Story 039: applying tiers and the subsystem registration. Story 040: no rendered pawn frozen.

---

## Files

- `Source/StealthGameplay/Private/Core/Animation/AnimTierCore.cpp` (caps, tie key)
- `Source/StealthGameplay/Private/Tests/Locomotion/TierCapsTieKeyTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.TierCapsTieKey`.*

Test: tier caps and tie key (AC-SCL-28)
  Given: 12 pawns at identical h inside 8 m, one of them a player pawn
  When: tiers are assigned, over 10,000 draws from an injected fixed-seed RNG, and separately over 1,000 pawns
  Then: exactly 8 are in A, chosen by the client-local `TierTieKey`; the player pawn's tier-A frequency equals an NPC's within ± 3 %; Spearman |ρ| < 0.1 between `TierTieKey` and spawn order
  Edge cases: the AC covers only the tier-A cap of 8; the B and C caps are not tested here

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/TierCapsTieKeyTest.cpp`, test name `StealthGame.Locomotion.TierCapsTieKey` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-037-tier-formula.md`
- Unlocks: `production/epics/shared-character-locomotion/story-039-tier-parity-ab.md`
