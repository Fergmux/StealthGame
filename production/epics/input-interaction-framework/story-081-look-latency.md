# Story 081: AC 58 LookLatency

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-012`, `TR-iif-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0011 makes Look local: the owner's control rotation is set each frame by Camera's view envelope (`AStealthPlayerController::UpdateRotation`, ADR-0015) from `IA_Look`, so the camera transform changes in the input frame (`L_look <= 33 ms`) and Look reaches the server only inside `ServerMove`. ADR-0010 carries Move and Look through the CMC saved-move path for every human pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This story reports the Look half of verification item **V4** (with story 080). Engine fact E8: human Move/Look reach the server through the CMC's `ServerMove` (unreliable, batched) with the control rotation. The measurement is a client-side frame count; no engine API beyond Enhanced Input injection and the camera transform is involved.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand` — source: ADR-0011
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn — source: ADR-0011
- Required: After every possession the control rotation is set from the pawn's `HeadLook` world rotation — source: ADR-0010

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 58 (F5, Integration, P `LookLatency`): **GIVEN** 60 fps **WHEN** mouse input is injected **THEN** the camera transform changes in the same frame (≤ 1 frame, measured by frame counter).

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 1, 2 and 11, GDD F5 (`L_look = t_frame + t_display`, at most about 33 ms):*

**What this story builds.** A 2-client PIE test (no net emulation: the form is P, not N) that injects a mouse delta in frame N on the owner client and asserts the camera transform differs from frame N-1 by frame N+1 at the latest (at most one frame, measured by a frame counter at fixed 60 fps). It proves that Look is local: nothing waits for the server, a timer or a reply. Together with story 080 it is the V4 report; ADR-0011 Validation Criteria cite AC 58 explicitly.

**Test notes (qa-lead).** Fixed 60 fps PIE, owner client, frame counter. Edge cases: 100 injections, all within the bound; injection with a hold active (a Prepare or Interact in progress must not delay or block Look); both a listen host and a remote client. The camera view envelope itself is owned by the camera-system epic (ADR-0015); this story measures the end-to-end input-to-camera-transform latency on the input path IIF owns (`IA_Look` binding, control rotation write), not the envelope's shaping.

**Prototype.** Supersedes the prototype's look input handling in `StealthCharacter.cpp` / `StealthPlayerController.*` (the controller input mode); gameplay input moves to `AStealthPlayerController::SetupInputComponent` (story 094). Final removal of the prototype code is story 093.

**Tuning keys (defaults).** None. The 33 ms figure is F5's `L_look` bound (`t_frame` 0.0167 s at 60 fps plus about 0.016 s display latency).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-080-latency-budget.md`: command confirm and observer latency under net emulation (the other half of V4).
- `production/epics/input-interaction-framework/story-085-look-parity-observed.md`: how Look looks to an opponent client, human versus NPC gaze turn.
- `production/epics/input-interaction-framework/story-065-no-look-event.md`: no hub event for Look.
- `production/epics/input-interaction-framework/story-017-walk-slower-scaling.md`: Move magnitude scaling (Move, not Look).

---

## Files

- `Source/StealthTestHarness/Private/LookLatencyTest.cpp` — CQTest `NETWORK_TEST_CLASS(LookLatency, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.LookLatency.<Method>` (planned)
- `production/qa/spikes/iif-v4-latency-report.md` — V4 report (planned; shared with story 080)

---

## QA Test Cases

**Automated test specs:**

- **AC-1**: The camera transform changes within one frame of an injected mouse delta (`StealthGame.Input.PIE.LookLatency`; reports V4 with 080).
  - Given: PIE at fixed 60 fps, owner client, frame counter.
  - When: a mouse delta is injected in frame N.
  - Then: the camera transform differs from frame N-1 by frame N+1 at the latest (at most 1 frame).
  - Edge cases: 100 injections, all within the bound; injection with a hold active; listen host and remote client both.

*QL-STORY-READY: ADEQUATE (qa-lead).*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/LookLatencyTest.cpp` (CQTest `StealthGame.Input.PIE.LookLatency`) must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-017-walk-slower-scaling.md`; cross-epic: camera-system (stories not yet written; view envelope writes the control rotation in `UpdateRotation`); cross-epic: shared-character-locomotion (stories not yet written; saved-move Look path); cross-epic: tuning-data-config (stories not yet written; module skeleton and harness)
- Unlocks: `production/epics/input-interaction-framework/story-085-look-parity-observed.md`
