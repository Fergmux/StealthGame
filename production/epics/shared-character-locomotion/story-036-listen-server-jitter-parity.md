# Story 036: The listen host sees a remote player's pawn as smoothly as an NPC

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R14, F10; AC-SCL-22)
**Requirement**: `TR-scl-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 3: listen-server smoothing parity)
**ADR Decision Summary**: On the listen host, remote-client pawns are moved by `ServerMove` at irregular arrival times while NPC pawns move every server tick; the host smooths remote-client pawns with the same `PawnSmoothLocationSec` (the CMC's listen-server simulated smoothing times set from that one key) so their per-frame speed variance stays within 1.25 × that of an NPC.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `ListenServerNetworkSimulatedSmoothLocationTime` and `ListenServerNetworkSimulatedSmoothRotationTime` are CMC members (E8/E9 of ADR-0003). Host-view parity is provisional until ANIM-V3 (story 039/044) passes; this story covers the movement half.

**Control Manifest Rules (this layer)**:
- Required: listen-server smoothing times from `PawnSmoothLocationSec`.
- Forbidden: no kind-dependent smoothing setting; never `NM_ListenServer` special-casing in input ingress.
- Guardrail: host sustains at least 60 fps at the profiled load (I-SCL-10).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-22** [N] `ListenServerJitterParity`: GIVEN a remote-client player and an NPC walking the same straight path, WHEN the host records rendered positions for 60 s (≥ 3,000 host frames) at 60 fps, repeated 3 times with fixed seeds, THEN the variance of per-frame rendered speed on the player pawn is ≤ 1.25 × the NPC's in every run.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the remote player's link uses AC-21's PktLag 50 / PktLoss 1 % profile.

---

## Implementation Notes

*Derived from ADR-0003 section 3:*

- Set the two listen-server smoothing times from `PawnSmoothLocationSec` in the same movement-component init as story 035. The test records the host's rendered pawn positions per frame (rendered, i.e. the smoothed mesh position) for the remote-client pawn and an NPC on the same path, three fixed-seed repeats.
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 035: observer-side snap. Story 039: animation parity on the host (ANIM-V3).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (listen-server smoothing)
- `Source/StealthTestHarness/Private/Locomotion/ListenServerJitterParityTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.PIE.ListenServerJitterParity.PlayerVsNpc`.*

Test: listen-server jitter parity (AC-SCL-22)
  Given: a remote-client player (PktLag 50 / PktLoss 1 %) and an NPC walking the same straight path
  When: the host records rendered positions for 60 s (≥ 3,000 host frames) at 60 fps, repeated 3 times with fixed seeds
  Then: the variance of per-frame rendered speed on the player pawn is ≤ 1.25 × the NPC's in every run
  Edge cases: rendered means the smoothed mesh position

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/ListenServerJitterParityTest.cpp` — must exist and pass (multi-client PIE); retained host-view capture in `production/qa/evidence/` (player-observable on the host)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-035-no-observer-snap.md`
- Unlocks: None
