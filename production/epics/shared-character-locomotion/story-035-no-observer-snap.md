# Story 035: Observers never see a snap, and smoothing is the same for NPC and player pawns

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R14 Smoothing parity, F10 Simulated-proxy smoothing and snap; AC-SCL-21)
**Requirement**: `TR-scl-010` (smoothing parity and input-starvation rule)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 3), with ADR-0010 (movement model)
**ADR Decision Summary**: Every simulated proxy uses `NetworkSmoothingMode = Exponential` with `PawnSmoothLocationSec`, the rotation smoothing time and `PawnSmoothSnapThresholdCm`; on the listen host the CMC's listen-server simulated smoothing times are set to the same values. Client-authoritative position is never enabled and the transform timestamp is never forced.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: CMC `NetworkSimulatedSmoothLocationTime`, `ListenServerNetworkSimulatedSmoothLocationTime` and `NetworkMaxSmoothUpdateDistance` are the members set from the SCL keys (E8, E9 of ADR-0003). Check that the engine's emulated packet loss accepts a fixed seed before relying on reproducibility.

**Control Manifest Rules (this layer)**:
- Required: one smoothing mode and one set of times and thresholds for every simulated proxy; listen-server smoothing times from the same key (`PawnSmoothLocationSec`).
- Forbidden: never enable client-authoritative position; never force the transform timestamp.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-21** [N] `NoObserverSnap`: GIVEN 10 minutes of a 3-client round under PktLag 50 / PktLoss 1 % with a fixed loss seed, WHEN observers log simulated-proxy corrections, THEN no correction > 256 cm occurs outside the listed designed spawns (round-start placement, out-of-view replacements), each pawn kind yields ≥ 200 corrections (else the result is INCONCLUSIVE, not Pass), the two-sample KS D ≤ 0.15, and p95 correction sizes are within 20 % between NPC and player pawns.

---

## Implementation Notes

*Derived from ADR-0003 section 3 and GDD R14/F10:*

- Set the CMC smoothing members from `PawnSmoothLocationSec`, the rotation smoothing time and `PawnSmoothSnapThresholdCm` for every pawn at the same place as the net config (story 030); on the host set both listen-server smoothing times from `PawnSmoothLocationSec`.
- Observers log each correction; list the designed spawns (round-start placement and out-of-view replacements) as excluded events. An INCONCLUSIVE outcome (fewer than 200 corrections for a kind) is reported as not Pass. The 20 % ratio is relative to the larger p95. Run as a nightly soak, not per commit.
- **Prototype replaced**: none (sample defaults).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 036: host-side jitter parity.
- Stories 050 and 051: loss bursts and recovery (Blocked on spike 045).
- Story 055: the combined net-emulation pass.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (smoothing from tuning keys)
- `Source/StealthTestHarness/Private/Locomotion/NoObserverSnapTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.PIE.NoObserverSnap.ThreeClientRound`.*

Test: no observer snap and smoothing parity (AC-SCL-21)
  Given: 10 minutes of a 3-client round under PktLag 50 / PktLoss 1 % with a fixed loss seed
  When: observers log simulated-proxy corrections
  Then: no correction > 256 cm occurs outside the listed designed spawns (round-start placement, out-of-view replacements); each pawn kind yields ≥ 200 corrections (else INCONCLUSIVE, not Pass); two-sample KS D ≤ 0.15; p95 correction sizes within 20 % between NPC and player pawns (relative to the larger p95)
  Edge cases: check the engine's emulated loss accepts a fixed seed; run as a nightly soak, not per commit

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/NoObserverSnapTest.cpp` — must exist and pass (10-minute multi-client PIE soak); not player-observable beyond absence of snaps, so a retained capture summary in `production/qa/evidence/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-030-net-config-parity.md`; `production/epics/round-match-framework/EPIC.md` (3-client round), `production/epics/crowd-npc-behaviour/EPIC.md`
- Unlocks: `production/epics/shared-character-locomotion/story-055-net-emulation-pass-exit.md`
