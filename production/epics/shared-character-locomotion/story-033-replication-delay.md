# Story 033: ActionPose writes reach observers on the same schedule for NPC and player pawns

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R13 first bullet, F9 effective delay `U_eff`, I-SCL-10; AC-SCL-20)
**Requirement**: `TR-scl-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 1 and 2)
**ADR Decision Summary**: Fixed 15 Hz cadence for every pawn; forced net updates are forwarded only inside a scoped reason token (`ActionStateWrite`, `TraversalStateWrite`, `DeathWrite`) and capped at one per pawn per server frame; every other `ForceNetUpdate` call is dropped. The effective delay is `U_eff ≤ 1/PawnNetUpdateHz + T_frame` = 0.083 s at 15 Hz with the host sustaining 60 fps.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: On a listen host the server tick is the render frame, so quantisation is by host frame time (`NetServerMaxTickRate` has no effect without the clamp flag); the harness frame-time capture at p99 checks I-SCL-10. `NetClientTicksPerSecond` 2400 keeps every client considered each frame (ADR-0003 E15).

**Control Manifest Rules (this layer)**:
- Required: `AStealthPawn::ForceNetUpdate` is overridden and forwards only inside `FStealthForcedNetUpdate` with the three reasons; at most one forwarded update per pawn per frame.
- Forbidden: no possession `ForceNetUpdate` forwarded; no `ForceNetRelevant`.
- Guardrail: delay p95 at most 0.083 s.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-20** [N] `ReplicationDelay`: GIVEN `PawnNetUpdateHz` 15 on a 60 Hz host, WHEN 200 `ActionPose` writes on NPC and player pawns are timed from server write to send, THEN p95 ≤ 0.083 s and the NPC and player distributions differ by ≤ 1 frame.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the NPC and player distributions are compared at p50 and p95, each within 1 frame (16.7 ms); the 200 writes use fixed-seed phase offsets.

---

## Implementation Notes

*Derived from ADR-0003 section 2:*

- Implement the `ForceNetUpdate` override and the scoped `FStealthForcedNetUpdate` token (counted drops in development builds). The `ActionPose` write path itself is the IIF's `WriteActionState`; use it through a stub writer until that epic lands.
- Add a send-time hook in the harness (test-only) that records the net tick at which the dirty property is sent; compare to the server write time. Include the host-frame-time capture for I-SCL-10.
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- The `FActionState` serialiser and `WriteActionState` (IIF and Assassination epics).
- Story 034: bandwidth.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (`ForceNetUpdate`, `FStealthForcedNetUpdate`)
- `Source/StealthTestHarness/Private/Locomotion/ReplicationDelayTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.PIE.ReplicationDelay.NpcVsPlayer`.*

Test: replication delay (AC-SCL-20)
  Given: `PawnNetUpdateHz` 15 on a 60 Hz host, 200 `ActionPose` writes on NPC and player pawns at fixed-seed phase offsets
  When: each write is timed from server write to send (harness send-time hook)
  Then: p95 ≤ 0.083 s; p50 and p95 of the NPC and player distributions each differ by ≤ 1 frame (16.7 ms)
  Edge cases: include the host frame-time capture for I-SCL-10

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/ReplicationDelayTest.cpp` — must exist and pass (multi-client PIE); not player-observable

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-030-net-config-parity.md`; `production/epics/input-interaction-framework/EPIC.md` (`WriteActionState`)
- Unlocks: None
