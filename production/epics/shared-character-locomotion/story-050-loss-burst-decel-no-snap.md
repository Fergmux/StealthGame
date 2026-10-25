# Story 050: A 2 s loss burst brakes the pawn by the baseline, with no dead stop and no snap

> **Epic**: Shared Character & Locomotion
> **Status**: Blocked — BLOCKED: the net-emulation spike (story 045; ADR-0010 spike ST1 "starvation braking cadence", part of the net-emulation spike) has not reported; AC-SCL-23 is tagged [SPIKE] and the choice between the engine's 4 Hz forced-update stepping and a per-frame zero-input update is decided by it
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R15 Input starvation, F12 Starvation and catch-up; AC-SCL-23; Session AC 47)
**Requirement**: `TR-scl-010`, `TR-scl-014`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 6, "Input starvation (R15, F12)"), with ADR-0004 for the pinned values
**ADR Decision Summary**: `UStealthMovementComponent::ForcePositionUpdate` zeroes the input acceleration and then calls `Super`, so a starved pawn brakes by the baseline into idle while its position keeps replicating. The `AGameNetworkManager` class-default values are pinned in `Config/DefaultGame.ini` (`MAXCLIENTUPDATEINTERVAL` = `InputStarvationSec`, `MaxClientForcedUpdateDuration` 1.40 s, discrepancy detection and resolution on, `MaxMoveDeltaTime` at most the interval) and boot-tested against the SCL keys. The owner mirrors starvation with no acknowledgement for `InputStarvationSec + 0.05 s`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Engine forced position updates step the server position once per `MAXCLIENTUPDATEINTERVAL` (4 Hz at 0.25 s) over the whole elapsed time (E6), so observers could see a 4 Hz step; spike ST1 decides cadence versus a per-frame zero-input update. `ForcePositionUpdate` is virtual and `check`s the autonomous proxy role.

**Control Manifest Rules (this layer)**:
- Required: starvation zeroes acceleration in `ForcePositionUpdate`; `AGameNetworkManager` forced-update and discrepancy values come from `Config/DefaultGame.ini` and equal the SCL tuning keys.
- Forbidden: never replay excess late moves as a catch-up; never call `ForcePositionUpdate` during the AI to player hold.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-23** [N][SPIKE] `LossBurstDecelNoSnap` (Session AC 47): GIVEN a 2 s loss burst on one client while its pawn walks at 140, WHEN a non-owner observes, THEN measured from the last server-accepted move the pawn is stopped within 1.0 s + RTT/2 ± 1 net tick (66.7 ms), the per-sample speed drop is ≤ 187 × (1/15) × 1.05 = 13.1 cm/s (no dead stop mid-stride, no gliding), and recovery shows no positional snap > 256 cm.

*QA clarification (create-stories 2026-10-25, not a GDD change):* RTT is the harness-measured observer-to-server connection RTT under the emulation profile (sampled in steady state just before the burst), not an assumed value; the hitch variant promised by ADR-0010's risk table is not included.

---

## Implementation Notes

*Derived from ADR-0010 section 6:*

- Override `ForcePositionUpdate` to zero input acceleration then call `Super`; shape braking by the baseline (story 009's stop floor applies); if spike ST1 shows 4 Hz stepping is visible, step the starved pawn per frame with zero input instead, for every pawn.
- Read the final cadence decision from the spike report (045). Pin and boot-test the `AGameNetworkManager` values (story 003 covers the boot test).
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 051: recovery without catch-up. Story 009: the stop floor. Round's handover past `SessionSoftTimeoutSec`.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (`ForcePositionUpdate`)
- `Source/StealthTestHarness/Private/Locomotion/LossBurstDecelNoSnapTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; BLOCKED on spike 045). Test `StealthGame.Locomotion.PIE.LossBurstDecelNoSnap.WalkingPawn`.*

Test: loss-burst deceleration without a snap (AC-SCL-23)
  Given: a 2 s loss burst on one client while its pawn walks at 140
  When: a non-owner observes
  Then: measured from the last server-accepted move the pawn is stopped within 1.0 s + RTT/2 ± 1 net tick (66.7 ms), where RTT is the harness-measured observer-to-server connection RTT sampled in steady state just before the burst; the per-sample speed drop is ≤ 187 × (1/15) × 1.05 = 13.1 cm/s (no dead stop mid-stride, no gliding); recovery shows no positional snap > 256 cm
  Edge cases: the connection RTT may be unavailable during the burst, hence the pre-burst sample

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/LossBurstDecelNoSnapTest.cpp` — must exist and pass (multi-client PIE under loss burst); retained capture of the observed stop in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md`, `story-009-stop-floor.md`, `story-003-tuning-keys-registered.md`, `story-035-no-observer-snap.md`
- Unlocks: `production/epics/shared-character-locomotion/story-051-loss-recovery-no-catch-up.md`
