# Story 022: RemoteViewPitch16 written from HeadLook for every pawn, clamped

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R8.4 One pitch path; AC-SCL-18; Open Question OQ11)
**Requirement**: `TR-scl-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 3, "One pitch path"); also ADR-0001
**ADR Decision Summary**: `AStealthPawn::PreReplication` calls `Super` (which writes pitch from control rotation only when a controller exists) and then always calls `SetRemoteViewPitch` from the clamped `HeadLook` pitch, for every pawn, with or without a controller. Pitch for other clients is therefore the head's pitch (±20°), never the raw camera pitch (−60° to +40°, local only).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `RemoteViewPitch16` is 16-bit, resolution ≈ 0.0055° (verified 5.8.4, Crowd change request 15a); spike S1 observed it reach non-owners only for player-controlled pawns, which makes this write load-bearing (`production/qa/spikes/2026-10-25_S1-S2-S14.md` section 1). OQ11 `[verify 5.8]`: the write for controller-less pawns in `PreReplication`; this story's test is that verification.

**Control Manifest Rules (this layer)**:
- Required: one pitch path: `RemoteViewPitch16` is written in `PreReplication` from the clamped `HeadLook` pitch for every pawn.
- Forbidden: no kind-dependent pitch; camera view pitch (−60° to +40°) never replicates; no per-pawn replication override.
- Guardrail: pitch is inside the F9 pawn network line.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-18** [N] `RemoteViewPitchClamped`: GIVEN a player pitching the first-person camera −60° to +40° and an AI pitching focus ±60°, WHEN an observer reads every `RemoteViewPitch16` sample (resolution ≈ 0.0055°, 16-bit [verified 5.8.4]), THEN both stay within ±20.1° and the per-sample step is ≤ 9.4° at 15 Hz (140 °/s ÷ 15 Hz plus quantisation; corrected at reconciliation 2026-10-24, Crowd change request 15a), for both pawns, including an AI pawn with no controller at the moment of a handover.

---

## Implementation Notes

*Derived from ADR-0003 section 3:*

- Override `AStealthPawn::PreReplication`: call `Super`, then `SetRemoteViewPitch` from `HeadLook` pitch every time, so a controller-less pawn (handover instant, disconnect hand-off) still carries the head pitch. Confirm no other code path calls `SetRemoteViewPitch` or writes raw controller pitch.
- Test through an opponent client logging every received sample for a human pitching −60° to +40° and an AI pitching ±60°, plus a forced handover moment with no controller. Include a check that the received property is dirty-capable for AI pawns (the prototype's AI pitch never changed from 0, so the property was never dirty for NPCs).
- **Prototype replaced**: the engine default (copy of raw controller pitch only when a controller exists) is the behaviour overridden here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 021: the F6 filter that produces `HeadLook`.
- Story 023: observer interpolation of received yaw and pitch.
- Story 027: the handover test itself (this story only asserts the pitch property across it).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (`PreReplication`)
- `Source/StealthTestHarness/Private/Locomotion/RemoteViewPitchClampedTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.PIE.RemoteViewPitchClamped.PlayerVsAi`.*

Test: `RemoteViewPitch16` clamped for every pawn (AC-SCL-18)
  Given: a player pitching the first-person camera −60° to +40° and an AI pitching focus ±60°
  When: an observer reads every `RemoteViewPitch16` sample
  Then: both stay within ±20.1°; the per-sample step is ≤ 9.4° at 15 Hz for both pawns; this also holds for an AI pawn with no controller at the moment of a handover
  Edge cases: sample resolution is about 0.0055°; the handover moment uses story 027's mechanism (that part is exercised once story 027 exists)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/RemoteViewPitchClampedTest.cpp` — must exist and pass (multi-client PIE); retained screenshot of an observed head at the pitch limit in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-021-look-envelope-700-deg-spin.md`
- Unlocks: `production/epics/shared-character-locomotion/story-027-handover-blend.md`, `story-042-camera-toggle-changes-nothing.md`
