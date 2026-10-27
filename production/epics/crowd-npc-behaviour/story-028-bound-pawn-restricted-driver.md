# Story 028: Participant-bound pawn driving: takeover blend, restricted driver and hand-back

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-007 = Participant-bound AI driving and takeover eligibility supply.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0010 (`RequestHandOver`, hand-over pass, stop-and-blend, `HandoverHoldSec`, control epoch), round-match-framework GDD (R17 restricted driver), roster GDD (AC-PSR-43)
**ADR Decision Summary**: `DriveBoundPawn(Pawn)` registers a pending restricted driver in the same slot as Round's `RequestHandOver(Pawn, Ai)`; on possession the controller picks it up: `Move(current heading, Walk)` at the nearest pace for `HandoverHoldSec` (or `Move(0)` below the pivot speed or when the heading leaves the navmesh), never decelerating faster than `CrowdStopDecel`, then `ST_CrowdRestricted` with `FCrowdMotionSink` (Move and Look only). `ReleaseControl` latches the sink, clears the held `Move`, stops the tree and the pass detaches at the next frame start.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Crowd never calls `Possess`/`UnPossess`; possession is ADR-0010's hand-over pass at the start of the next server frame, never during a traversal. A concealed pawn stays concealed (no `Hide(Exit)`; GDD OQ4 is open and the current rule is no).

**Why this story**: A disconnected or unresponsive player's pawn must keep walking like any citizen without the handoff itself being a tell (T10), and never issue an action a bound AI should not (it can only `Move` and `Look`). Read GDD CNB-R10 items 1-3 and 5, AC-CNB-20.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: driver changes go through `UStealthPawnFactory::RequestHandOver` and ADR-0010's hand-over pass, `ICrowdControl` only registers the driver; the restricted StateTree receives `FCrowdMotionSink`; every sink call checks the control epoch (ADR-0016)
- Forbidden: `Possess`/`UnPossess` from Crowd code; any command other than `Move` and `Look` from a restricted driver; `Hide(Exit)` for a concealed bound pawn (ADR-0016, GDD R17)
- Guardrail: restricted driver is the cheap subset of the tree; cost inside `Crowd.StateTree`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-20 and CNB-R10 items 1-3 and 5, scoped to this story:*

- [x] (AC-CNB-20) GIVEN an Active player running mid-interaction at a 1/60 s step, WHEN the connection goes Unresponsive, THEN max |dv/dt| <= `CrowdStopDecel`, the hold ends with the ordinary cancel pose, the head never exceeds `LookMaxRateDegPerSec`, the pawn is never destroyed, and from takeover to hand-back it issues only `Move` and `Look`. — headless at 1/60 s (decel, hold heading, never destroyed, only Move/Look); the head-rate clause is the SCL head filter's (SCL-021) and the cancel pose Round's IIF release path
- [x] `DriveBoundPawn` registers a pending restricted config; on possession the pawn holds `Move(current heading, Walk)` at the nearest pace for `HandoverHoldSec` (0.8 s), or `Move(0)` below the pivot speed or when the hold heading leaves the navmesh within the hold distance, then runs `ST_CrowdRestricted` through `FCrowdMotionSink`.
- [x] A concealed bound pawn stays concealed (no `Hide(Exit)`); `ReleaseControl` issues `Move(0)`, stops the tree and detaches at the next frame start, never during a traversal.

---

## Implementation Notes

*Derived from ADR-0016 section 7 and GDD CNB-R10:*

- Restricted tree: C1 route walk, C2 pauses, C5 yield, C6 re-centre and G1-G3, G5, G7 only (no tending, bracing, bush, reactions, emergence).
- `CrowdStopDecel` is a derived constant, not a key: the SCL run braking `RunSpeedCmPerSec / RunStopSec` = 600 cm/s^2.
- Idle bound: no visible stationary stretch of a restricted-driver pawn exceeds `CrowdMaxIdleSec` (story 014's watchdog applies).
- Hand-back: the driver issues `Move(0)` and detaches at the start of a server frame; the human's first command shapes from current velocity (SCL R4). `DriveBoundPawn` also applies to informant-bound pawns that go Unresponsive (ADR-0019).
- Outstanding holds of the human end through the IIF release path (Round), not through the sink.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype has no disconnect handling (listen server only, no AI takeover). New code.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029: `AdoptReleasedPawn` and `ReleaseToCrowd`
- Story 030: informant takeover eligibility
- Story 031: Halt and the 100-handover epoch gate
- The Round and Roster epochs: when Unresponsive and hand-back happen

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdBoundDriver.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdTakeoverBlendTest.cpp`
- `Source/StealthTestHarness/Private/CrowdTakeoverBlendMPTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.TakeoverBlend`
  - Given: an Active player running mid-interaction at a 1/60 s step
  - When: the connection goes Unresponsive
  - Then: max |dv/dt| <= `CrowdStopDecel`; the hold ends with the ordinary cancel pose; head rate never exceeds `LookMaxRateDegPerSec`; the pawn is never destroyed; only `Move` and `Look` issued from takeover to hand-back
  - Edge cases: hand-back while braking
- **AC-2**: `StealthGame.Crowd.BoundHandoverHold`
  - Given: `DriveBoundPawn` called with a pending restricted config; heading on the navmesh, off it, and speed below pivot
  - When: the pawn is possessed
  - Then: `Move(current heading, Walk)` at the nearest pace for `HandoverHoldSec`, or `Move(0)` below pivot speed or when the heading leaves the navmesh, then `ST_CrowdRestricted` through `FCrowdMotionSink`
  - Edge cases: heading leaves the navmesh mid-hold
- **AC-3**: `StealthGame.Crowd.BoundConcealedAndRelease`
  - Given: a concealed bound pawn, and a release during a traversal
  - When: control is released
  - Then: no `Hide(Exit)`; `ReleaseControl` issues `Move(0)`, stops the tree and detaches at the next frame start, never during a traversal
  - Edge cases: release requested mid-traversal waits for the landing

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdTakeoverBlendTest.cpp` (automation) plus `Source/StealthTestHarness/Private/CrowdTakeoverBlendMPTest.cpp` (multi-process `StealthGame.Crowd.MP.TakeoverBlend`) — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Takeover.TakeoverBlend`, `StealthGame.Crowd.Takeover.SlowOrOffNavHoldsStill`, `StealthGame.Crowd.Halt.HandoverEpoch` (hand-back at a frame start, never during a traversal); 2026-10-27)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-005-command-sinks-epoch-and-halt-filter.md`, `production/epics/crowd-npc-behaviour/story-006-crowd-controller-and-look-pitch.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-029-release-to-crowd.md`, `production/epics/crowd-npc-behaviour/story-030-takeover-eligibility-and-atomic-release.md`, `production/epics/crowd-npc-behaviour/story-031-halt-and-handover-epoch.md`

## Completion Notes
**Completed**: 2026-10-27 (Crowd & NPC Behaviour batch, stories cnb-019..031)
**Criteria**: 3/3 passing
**Deferred**: CrowdTakeoverBlendMPTest (multi-process) not built: the head-rate clause is the SCL head filter's (story SCL-021) and the cancel pose is Round's IIF release path.
**Deviations**: The hold heading is checked within the device's 22.5 degree keyboard quantisation. HandoverHoldSec is read from FLocomotionTuning_Server.
**Test Evidence**: `StealthGame.Crowd.Takeover.TakeoverBlend`, `StealthGame.Crowd.Takeover.SlowOrOffNavHoldsStill`, `StealthGame.Crowd.Halt.HandoverEpoch` (hand-back at a frame start, never during a traversal); full suite and measured lines: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/`
**Run result**: 3-player PIE on L_Prototype with 50 NPCs: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/evidence.md`
**Reviewed by**: self-review against the code-review skill (findings in the evidence note)
