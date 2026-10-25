# Story 085: AC 54 LookParityObserved

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-002`, `TR-iif-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid
**Other ADRs**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0003 section 3 fixes one pitch path and smoothing parity: `PreReplication` always writes `RemoteViewPitch16` from the clamped `HeadLook` pitch for every pawn (controller or not), `HeadLook` yaw replicates as one byte, and every simulated proxy uses exponential smoothing with the same tuning values. ADR-0010 section 6 routes Move and Look through the same CMC input path for human (saved moves) and AI (`UPawnCommandComponent` on the server).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 (networking, HIGH) and ADR-0010 (HIGH). Verification items that condition this measurement: ADR-0003 S13 (CMC cadence and correction parity for PC- versus AI-possessed pawns seen by a third client, adaptive frequency off, `SimulatedProxy` for both) and ADR-0003 E8 (`RemoteViewPitch16` through `PreReplication`). The listen-host simulated smoothing times are set to the same values (ADR-0003 section 3).

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand` — source: ADR-0011
- Forbidden: Never add a Move/Look RPC or a Look event — source: ADR-0011
- Required: Every simulated proxy uses exponential smoothing with the same tuning values, including the listen-server simulated smoothing times — source: ADR-0003
- Required: `PreReplication` writes `RemoteViewPitch16` from `HeadLook` for every pawn after `Super` — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 54 (R3a, Integration, N `LookParityObserved`): **GIVEN** a stationary human turning the camera 90° over 1 s, and an NPC performing a gaze-catalogue turn of 90° over 1 s **WHEN** an opponent client records both pawns' replicated rotation, view pitch and head/body yaw **THEN** update cadence, property set and smoothing curve match within 1 frame and 2°.

---

## Implementation Notes

*Derived from GDD R3a ("Look parity contract": the IIF states the requirement, Shared Character and Locomotion owns the mechanism), ADR-0003 Decision section 3 and ADR-0010 Decision section 6:*

**What this story builds.** The observer-side proof of Look parity (GDD R3a): a stationary human turning the camera 90 degrees over 1 s and a stationary NPC performing a gaze-catalogue turn of 90 degrees over 1 s are recorded by an opponent client (replicated rotation, view pitch, head and body yaw), and update cadence, property set and the smoothing curve match within 1 frame and 2 degrees. Look is a head-turn gaze signal; because a per-turn event would be a tell (no Look event, GDD R3 and story 065), parity must hold at the replication level, which is what an opponent can actually see.

**Why N-form.** Profile N (PktLag 50 ms per direction, 1 percent loss) applies; the AC states no other. Loss and lag are where a different update path (saved moves for the human, direct control-rotation write for the NPC) could diverge.

**Gaps to resolve in the test design (qa-lead).** (1) For the smoothing curves to be comparable the human camera input and the NPC gaze turn must follow the same source profile; the AC does not say so. The qa-lead's proposed wording: the human turn is driven by the NPC gaze-catalogue turn's per-frame yaw profile (injected), so any difference is attributable to the replication path. (2) Define the comparison: sample both recorded rotation, pitch and head/body yaw series at fixed times; the maximum difference is at most 2 degrees and the onset offset at most 1 frame. Cross-epic: the NPC gaze catalogue is owned by crowd-npc-behaviour.

**Test notes.** Edge cases: opposite turn directions for the two pawns. Look reaches the server only inside `ServerMove` (story 081 measures the local latency; story 065 pins that no hub event fires).

**Prototype.** Supersedes the prototype's controller look handling in `StealthPlayerController.*` and the AI facing path in `StealthNpcController.*`; new code for the observer measurement. Final removal is story 093.

**Tuning keys (defaults).** None are IIF keys. SCL-owned smoothing keys (`PawnSmoothLocationSec`, rotation smoothing time, `PawnSmoothSnapThresholdCm`) set the curve under test.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-081-look-latency.md`: local look latency on the owner client.
- `production/epics/input-interaction-framework/story-065-no-look-event.md`: no hub event for Look.
- `production/epics/input-interaction-framework/story-082-action-pose-parity-observed.md`: the same observer measurement for ActionPose.
- `production/epics/input-interaction-framework/story-084-pawn-replicated-value-parity.md`: replicated property set parity for a bound versus an NPC pawn.

---

## Files

- `Source/StealthTestHarness/Private/LookParityObservedTest.cpp` — CQTest `NETWORK_TEST_CLASS(LookParityObserved, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.LookParityObserved.<Method>`; net-emulation profile N (planned)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap 1: for the smoothing curves to be comparable the human camera input and the NPC gaze turn must follow the same source profile; the AC does not say so.
- Gap 2: define the comparison: sample both recorded rotation, pitch and head/body yaw series at fixed times; maximum difference at most 2 degrees and onset offset at most 1 frame.

*QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority):* the human turn is driven by the NPC gaze-catalogue turn's per-frame yaw profile (injected), so any difference is attributable to the replication path; per-sample difference at most 2 degrees and time offset at most 1 frame.

*Draft spec from the qa-lead (unconfirmed), test `LookParityObserved`:* Given an N profile, a stationary human and an NPC each turning 90 degrees over 1 s with the same injected profile, and an opponent client recording rotation, view pitch, head and body yaw each frame, when both turns complete, then update cadence and property set are identical, per-sample difference is at most 2 degrees and time offset is at most 1 frame. Edge cases: opposite turn directions; cross-epic dependency on the Crowd gaze catalogue.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/LookParityObservedTest.cpp` (CQTest `StealthGame.Input.PIE.LookParityObserved`) must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-081-look-latency.md`; cross-epic: crowd-npc-behaviour (stories not yet written; gaze catalogue turn); cross-epic: shared-character-locomotion (stories not yet written; Look parity mechanism, `RemoteViewPitch16`, smoothing); cross-epic: camera-system (stories not yet written; view envelope)
- Unlocks: None
