# Story 027: Stop-and-blend hand-over between player and AI

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R3 Possession parity, R4 Stop-and-blend handover; AC-SCL-14 with Roster AC-PSR-43)
**Requirement**: `TR-scl-003` (possession parity and stop-and-blend hand-over; `ForceNetUpdate` deferral)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (sections 4.1-4.4); also ADR-0003 (section 2: dropped possession `ForceNetUpdate`)
**ADR Decision Summary**: A live pawn's possession change is committed in a queue slot, bumps the control epoch, waits for landing if in traversal, and executes in the hand-over pass (`FPawnHandoverTickFunction`, `TG_PrePhysics`) at the next frame start. Every `Possess` runs through `FScopedPossessionParity`; `Restart`/`PawnClientRestart` are overridden so velocity, movement mode and `HeadLook` survive; control rotation is set from the pawn's `HeadLook`. A last-command hold covers every frame until the new controller's first command or `HandoverHoldSec`. AI to player keeps simulating with `PerformMovement` until the owner's first `ServerMove`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Gate **H1** (ADR-0010): the AI to player gap. The server must keep simulating with the held input until the new owner's first `ServerMove` (E7: the engine otherwise leaves the pawn unsimulated for up to `MAXCLIENTUPDATEINTERVAL`) **without** touching the CMC's client timestamps and without `ForcePositionUpdate`; no dropped first moves, no owner correction above `PawnSmoothSnapThresholdCm`, no visible stop for observers. This story's test is the H1 verification; fallback if it fails: keep the crowd controller driving until the client acknowledges the pawn, then possess. Record the result in `production/qa/spikes/`. `Possess`/`UnPossess` are `virtual final`: override `OnPossess`/`OnUnPossess`.

**Control Manifest Rules (this layer)**:
- Required: a live pawn's possession change commits in a queue slot, bumps the control epoch, waits for landing if `IsInTraversal`, executes in the hand-over pass at the next frame start; last-command hold; control rotation set from `HeadLook` after every possession; the AI to player hold simulates with `PerformMovement`.
- Forbidden: never call `StopMovementImmediately`, `SetDefaultMovementMode` or `Super::Restart` from `Restart`; never zero velocity in `PawnClientRestart`; never advance the CMC's client timestamps or call `ForcePositionUpdate` during the AI to player hold; never execute a hand-over inline in the queue slot; never set `bOnlyAllowAutonomousTickPose`.
- Guardrail: hand-over pass cost negligible (tens of microseconds per request).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-14** [N] `HandoverBlend` (with Roster AC-PSR-43): GIVEN an Active player running mid-interaction and a control run with no handover, WHEN control passes to AI and later back, THEN [P, server] no frame shows deceleration above the run braking rate (600 cm/s² + 5 %) and head rate ≤ 140 °/s torso-relative, and the interaction ends with the ordinary cancel blend; AND [N, third client] in ±0.2 s of each switch the bunch count is ≤ the control run's + 1, the replicated property-name set is identical, root and pelvis per-frame delta ≤ 1.5 × the control run's p99, and no bone rotates > 15° in one frame.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the control run's p99 is computed over the full control run; control returns to the player 3 s after the switch to AI; the IIF's ordinary cancel-blend duration is cited from the IIF epic.

---

## Implementation Notes

*Derived from ADR-0010 sections 4.1-4.4:*

- Implement `FPawnHandoverTickFunction` (custom tick in `TG_PrePhysics`, registered on the authority in `OnWorldBeginPlay`, CMC tick as dependent prerequisite) and `UStealthPawnFactory::RequestHandOver(Pawn, EControlTarget {Ai, Player(Participant)}, ExpectedEpoch)`: validate, bump the control epoch at commit, record the pending change; the pass executes next frame. A pending request on a pawn in traversal or a root-motion action pose waits for it to end. Later requests replace pending ones; body releases execute before possessions in one pass.
- Execution order per pawn: (1) outgoing holds end through the IIF as a release; (2) unpossess/possess through `FScopedPossessionParity`; (3) control rotation from `HeadLook`; (4) last-command hold (`UStealthMovementComponent` caches the last commanded input every time it consumes a move, from either source).
- Player to AI: `ICrowdControl::DriveBoundPawn` issues `Move(current heading, Walk)` for `HandoverHoldSec`, so a running pawn slows by the baseline run-to-walk deceleration. AI to player: the per-pawn `bAwaitingFirstOwnerMove` flag set at possession and cleared when the first `ServerMove` is consumed; while set, the component's own server tick runs `PerformMovement(DeltaTime)` with the held input.
- Override `AStealthPawn::Restart()` and `PawnClientRestart()` as in story 001; do not mask engine behaviour after the fact (authoritative state replicates before any mask).
- Test with the harness: control run versus handover run on a third client, comparing bunch counts, property-name sets, root and pelvis deltas and per-frame bone rotation; server-side checks for deceleration and head rate. Scenario: Active player running mid-interaction; the handover waits for no traversal.
- **Prototype replaced**: the prototype's `RestartPlayer` and engine possession (which reset velocity and movement mode) are replaced; spike S14 verified takeover in the prototype for pointers only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 026: the disconnect hook (the other possession path).
- Story 028: `TakeOverCrowdPawn` eligibility and bind.
- Round's handover policy and Crowd's `DriveBoundPawn` behaviour (their epics).
- Traversal wait: traversal stories 047-049.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnFactory.cpp` (`RequestHandOver`, hand-over pass)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (`Restart`, `PawnClientRestart`, `FScopedPossessionParity`)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthMovementComponent.cpp` (last-command hold, `bAwaitingFirstOwnerMove`)
- `Source/StealthTestHarness/Private/Locomotion/HandoverBlendTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.PIE.HandoverBlend.PlayerToAiAndBack`.*

Test: stop-and-blend hand-over (AC-SCL-14, spike H1)
  Given: an Active player running mid-interaction, and a control run with no handover
  When: control passes to AI and returns to the player 3 s later
  Then: server — no frame shows deceleration above 600 cm/s² + 5 % and head rate ≤ 140 °/s torso-relative; the interaction ends with the ordinary cancel blend (duration from the IIF epic); third client in ±0.2 s of each switch — bunch count ≤ the control run's + 1, replicated property-name set identical, root and pelvis per-frame delta ≤ 1.5 × the control run's p99 (computed over the full control run), no bone rotates > 15° in one frame
  Edge cases: ±0.2 s is only about 24 frames, so the p99 comes from the whole control run; record the H1 result (no dropped first moves, no owner correction above `PawnSmoothSnapThresholdCm`)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/HandoverBlendTest.cpp` — must exist and pass (multi-client PIE); H1 result recorded in `production/qa/spikes/`; retained screenshot of the pawn across the switch on the third client in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-006-single-spawn-site.md`, `story-011-release-baseline-stop.md`, `story-022-remote-view-pitch-clamped.md`, `story-026-disconnect-keeps-pawn-under-crowd-ai.md`; `production/epics/input-interaction-framework/EPIC.md` (hold End path, cancel blend), `production/epics/round-match-framework/EPIC.md` (handover caller), `production/epics/participant-state-roster/EPIC.md` (AC-PSR-43)
- Unlocks: `production/epics/shared-character-locomotion/story-028-informant-respawn-takes-over-unseen-crowd-pawn.md`
