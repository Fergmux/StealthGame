# Story 051: Recovery from a 2.5 s burst never produces a catch-up sprint

> **Epic**: Shared Character & Locomotion
> **Status**: Ready — unblocked 2026-10-25: spike 045 reported (production/qa/spikes/2026-10-25_SCL045-net-emulation-traversal.md); forced updates stop at MaxClientForcedUpdateDuration (observed last step 1.36-1.38 s after starvation began).
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R15, F12; AC-SCL-24)
**Requirement**: `TR-scl-010`, `TR-scl-014`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "Input starvation (R15, F12)")
**ADR Decision Summary**: When moves resume, late moves are consumed with movement-time discrepancy detection and resolution on and observed speed capped at `StarvationCatchUpMaxMult` (1.15) times the gait's maximum speed; excess late moves are discarded, never replayed later, so there is no visible catch-up sprint. Server input is zeroed after `InputStarvationSec` (0.25 s) and the owner mirrors at `InputStarvationSec + 0.05 s`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `bMovementTimeDiscrepancyDetection` and `bMovementTimeDiscrepancyResolution` are read from the `AGameNetworkManager` class default object and pinned in `Config/DefaultGame.ini`; `MaxMoveDeltaTime` must not exceed `MAXCLIENTUPDATEINTERVAL`. I-SCL-7 keeps `StarvationCatchUpMaxMult × WalkSpeedCmPerSec` below the run-entry threshold.

**Control Manifest Rules (this layer)**:
- Required: excess late moves are discarded; the pinned `AGameNetworkManager` values equal the SCL keys (boot test).
- Forbidden: never replay excess late moves as a catch-up.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-24** [N][SPIKE] `LossRecoveryNoCatchUp`: GIVEN a 2.5 s burst (below `SessionSoftTimeoutSec`, so no handover) while running, and a second while walking, WHEN moves resume, THEN observed speed (Δx over a 0.2 s window on the server) never exceeds 1.15 × 450 = 518 cm/s for the runner and 1.15 × 140 = 161 cm/s for the walker; AND server input is zeroed 0.25 s ± 1 frame after the last move, and the owner mirror at 0.30 s ± 1 frame.

---

## Implementation Notes

*Derived from ADR-0010 section 6:*

- Implement the late-move cap in the movement component's move consumption (discard excess rather than replay) and the owner-side mirror (zero input after `InputStarvationSec + 0.05 s` without acknowledgement). The server zero-input timing is the starvation check of story 050.
- Test both gaits with the harness's burst profile; measure server speed over 0.2 s windows after the burst.
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 050: the braking during the burst. Round's handover past `SessionSoftTimeoutSec` (3 s).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (late-move cap, owner mirror)
- `Source/StealthTestHarness/Private/Locomotion/LossRecoveryNoCatchUpTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (BLOCKED on spike 045). Test `StealthGame.Locomotion.PIE.LossRecoveryNoCatchUp.RunnerAndWalker`.*

Test: loss recovery without catch-up (AC-SCL-24)
  Given: a 2.5 s burst (below `SessionSoftTimeoutSec`, so no handover) while running, and a second while walking
  When: moves resume
  Then: observed speed (Δx over a 0.2 s window on the server) never exceeds 1.15 × 450 = 518 cm/s for the runner and 1.15 × 140 = 161 cm/s for the walker; server input is zeroed 0.25 s ± 1 frame after the last move, and the owner mirror at 0.30 s ± 1 frame
  Edge cases: none beyond the two gaits

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/LossRecoveryNoCatchUpTest.cpp` — must exist and pass (multi-client PIE); retained capture of the recovery in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md`, `story-050-loss-burst-decel-no-snap.md`
- Unlocks: None
