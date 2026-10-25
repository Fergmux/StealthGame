# Story 026: A disconnect never destroys a pawn; a crowd AI possesses it in the same frame

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R2b Disconnect never destroys a pawn; AC-SCL-42; Roster R9 hook; Session rule 22)
**Requirement**: `TR-scl-002` (disconnect never destroys a pawn: `PawnLeavingGame` override, same-frame AI possession)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 5)
**ADR Decision Summary**: `AStealthPlayerController::PawnLeavingGame()` does not destroy the pawn: it ends the outgoing controller's holds as releases, unpossesses through the parity wrapper and, in the same server frame, spawns an `AStealthCrowdController` that possesses the pawn and applies the last-command hold. No control-epoch bump there; the binding is untouched; teardown and seamless travel only unpossess. `APawn::Reset` and `OutsideWorldBounds` are overridden so no engine path destroys a Shared Pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Gate **S8** (ADR-0010 Verification Required): whether spawning a controller is safe while the connection is being cleaned up (E3: `OnNetCleanup` nulls `Player` and destroys the controller, so `PawnLeavingGame` runs, then `Logout`). This story's real-connection-close test is the S8 verification; record the result in `production/qa/spikes/` for ADR-0010's gate. If S8 fails, the fix is a superseding ADR, not an in-place workaround.

**Control Manifest Rules (this layer)**:
- Required: `PawnLeavingGame` hands the pawn to an `AStealthCrowdController` in the same frame without issuing a command or changing the binding; it unpossesses only during world teardown or seamless travel and does not bump the control epoch; `Possess`/`UnPossess` only through `FScopedPossessionParity`.
- Forbidden: never destroy the pawn in `PawnLeavingGame`; never destroy-and-respawn on disconnect or takeover; never destroy a Shared Pawn in `Reset`.
- Guardrail: none specific.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-42** [N] `DisconnectKeepsPawnUnderCrowdAI`: GIVEN a connected player's pawn walking at 140 cm/s in a 3-client round, WHEN that client's connection closes, THEN the pawn is not destroyed (same NetGUID after 5 s, 0 channel closes for it on the other clients), a crowd AI controller possesses it in the same server frame, velocity is kept and braking never exceeds 187 cm/s² + 5 %; mid-traversal the possession waits for landing; AC-SCL-17's pointer-shape check holds throughout.

*QA clarification (create-stories 2026-10-25, not a GDD change; confirm at `/story-readiness`):* the same-frame possession is the disconnect path of ADR-0010 section 5, distinct from the takeover hand-over pass of R2a (next frame start). The mid-traversal clause is deferred to the traversal stories (Blocked on spike 045) and asserted there.

---

## Implementation Notes

*Derived from ADR-0010 section 5 and Implementation Guidelines:*

- Override `AStealthPlayerController::PawnLeavingGame`: teardown guard first (world teardown or seamless travel: unpossess only); otherwise end holds as releases through the IIF's End path, unpossess through `FScopedPossessionParity`, spawn one `AStealthCrowdController` (`bWantsPlayerState = false`) and possess in the same frame, applying the last-command hold from `UStealthMovementComponent`'s cached last commanded input (so a running pawn brakes by the baseline, never above 187 cm/s² + 5 %).
- The binding is untouched (Roster R9); Round's `ConnectionTransition` slot does the epoch bump and `DriveBoundPawn` later. The pawn issues no command of its own until then.
- Override `APawn::Reset` to do nothing to a Shared Pawn and `OutsideWorldBounds` to route to `RequestTeardown`; do not override `DetachFromControllerPendingDestroy`.
- Test on a real connection close in the 3-client harness (ADR-0008), tracking NetGUID, channel closes on the other clients, possessing controller class and braking per frame. Include the pointer-shape check of story 029 across the event.
- **Prototype replaced**: the engine default (`PawnLeavingGame` destroys the pawn) is the behaviour overridden; the prototype relies on it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 027: the hand-over pass (Player to AI and back) at frame start.
- Round's disconnect policy items and `DriveBoundPawn` (`production/epics/round-match-framework/EPIC.md`).
- Story 029: the pointer-shape test (consumed here).
- The mid-traversal wait: traversal stories 047-049.

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPlayerController.cpp` (`PawnLeavingGame`) (planned; the controller class itself belongs to the Round/IIF epics, this story adds the override)
- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (`Reset`, `OutsideWorldBounds`)
- `Source/StealthTestHarness/Private/Locomotion/DisconnectKeepsPawnUnderCrowdAITest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; mid-traversal clause deferred). Test `StealthGame.Locomotion.PIE.DisconnectKeepsPawnUnderCrowdAI.WalkingPawn`.*

Test: disconnect keeps the pawn under crowd AI (AC-SCL-42, spike S8)
  Given: a player's pawn walking at 140 cm/s in a 3-client round
  When: that client's connection closes (a real connection close)
  Then: the pawn is not destroyed (same NetGUID after 5 s, 0 channel closes for it on the other clients); a crowd AI controller possesses it in the same server frame (`PawnLeavingGame`, ADR-0010 section 5); velocity is kept and braking never exceeds 187 cm/s² + 5 %; AC-SCL-17's pointer-shape check holds throughout
  Edge cases: the mid-traversal clause ("possession waits for landing") is deferred to the traversal stories and must not be authored until the GDD owner reconciles it with ADR-0010 section 5 (possession immediate, hold continues through landing)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/DisconnectKeepsPawnUnderCrowdAITest.cpp` — must exist and pass (3-client PIE); the S8 result recorded in `production/qa/spikes/`; a retained screenshot of the pawn walking on under AI control on a remaining client in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-006-single-spawn-site.md`, `story-009-stop-floor.md`; `production/epics/session-travel/EPIC.md` (connection events), `production/epics/crowd-npc-behaviour/EPIC.md` (`AStealthCrowdController`)
- Unlocks: `production/epics/shared-character-locomotion/story-027-handover-blend.md`, `story-029-pawn-pointer-shape.md`
