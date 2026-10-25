# Story 008: Collision is a pure function of state and elapsed time, re-derived after correction replay

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R4, AC-CON-28 collision and replay parts)
**Requirement**: `TR-con-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§4 collision is a pure function)
**ADR Decision Summary**: `ComputeConcealCollision(State, ElapsedInTransition)` is evaluated on the server and on the owner for each simulated move: Outside blocks `BushVolume` and `Pawn`; Entering, Concealed, Exiting and Emerging ignore both. During a correction replay the elapsed time comes from each saved move's own timestamp mapped to server time (never current ServerNow); responses are written only when the computed state changes; it is never set by an anim notify.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; the function is plain C++, the replay hook uses ADR-0010's saved-move extension)
**Engine Notes**: Each collision-response change refreshes the body's physics state, which is expensive per tick, hence write-on-change. Visibility-based anim ticking can skip notifies on a hidden mesh (E6), so notifies can never drive collision. The `BushVolume` object channel is a `Config/DefaultEngine.ini` entry added by the implementer.

**Control Manifest Rules (this layer)**:
- Required: Collision state is computed per simulated move from that move's own server-mapped timestamp (or a saved-move field) on server and owner, written only on change, never stored by a notify — source: ADR-0017
- Forbidden: Never drive concealment transitions or the hide by anim notifies, engine timers, feature ticks or overlap — source: ADR-0017
- Guardrail: Write collision responses only on change

Nothing may use a concealed pawn as `Instigator` (IVP L15).

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-28, CON-R4), scoped to this story:*

- [ ] **AC1a** (AC-CON-28 part, [U] `StealthGame.Concealment.CollisionAndAvoidance`, logic form): **GIVEN** `ComputeConcealCollision` across the 5 states and t in {0, mid, end} **THEN** Outside gives `BushVolume` = Block and `Pawn` = Block, and the other four states give both Ignore at every t.
- [ ] **AC1b** (cross-reference, [DEFERRED] to story 011): the physical blocking of an Outside pawn by a `HideVolume` and no pawn-pawn or own-bush collision during a two-pawn warp are verified by the PIE functional map in `story-011-server-warp-to-anchor-and-exit-motion.md` (its AC3).
- [ ] **AC2** (AC-CON-28 replay part, [U]): **GIVEN** an owner correction mid-warp **THEN** the collision state after replay equals the state derived from the Concealment state and each saved move's own timestamp mapped to server time; responses are written only when the computed state actually changes.
- [ ] **AC3** (derived, CON-R4): **GIVEN** Move input in Entering, Concealed, Exiting and Emerging **THEN** it is zeroed on server and owner from transition start; Outside input is untouched.

---

## Implementation Notes

*Derived from ADR-0017 Decision §4:*

Collision is computed for each simulated move from `State` and the move's elapsed time in the transition; during replay use the saved move's own timestamp mapped to server time, or carry the state in a custom `FSavedMove_Character` field (ADR-0010's saved-move extension), never the current `ServerNow`, which would give replayed moves the wrong state. Pawns in the four non-Outside states are also removed from RVO and crowd avoidance (story 009). `Move` input is zeroed from transition start on both sides. The test needs a fake mapping from saved-move timestamp to server time and a counter for response writes.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-009-crowd-perception-filter-and-steering-exclusion.md`: steering and perception.
- `story-011-server-warp-to-anchor-and-exit-motion.md`: the warp itself and the PIE functional-map collision check.
- `story-012-owner-warp-no-snap.md`: owner smoothness.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Public/View/ConcealCollision.h`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealCollision.cpp`
- `Source/StealthGameplay/Private/Core/` (minimal movement-component hook calling the function; exact file set by the SCL epic)
- `Source/StealthGameplay/Private/Tests/Concealment/ConcealCollisionTest.cpp` (class `FConcealmentConcealCollisionTest`, name `StealthGame.Concealment.CollisionAndAvoidance`)
- `Source/StealthGameplay/Private/Tests/Concealment/CollisionReplayTest.cpp` (class `FConcealmentCollisionReplayTest`)
- `Source/StealthGameplay/Private/Tests/Concealment/MoveZeroedTest.cpp` (class `FConcealmentMoveZeroedTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: GAPS on the first draft; revised as above, re-check ADEQUATE).*

- **AC1a**: CollisionFunction
  - Given: `ComputeConcealCollision` across the 5 states and t in {0, mid, end}.
  - When: the responses are read.
  - Then: Outside gives BushVolume Block and Pawn Block; the other four give both Ignore at every t.
  - Edge cases: t at exactly the end of a transition.
- **AC2**: CollisionReplay
  - Given: saved moves with timestamps t1..tn, a fake mapping from move timestamp to server time, and a mid-warp owner correction.
  - When: the moves are replayed.
  - Then: each replayed move's collision state equals the function applied to its own mapped time; a different value of ServerNow does not change the result; responses are written only when the computed state changes (count the writes).
  - Edge cases: a move exactly at the Entering start (elapsed 0); a constant clock offset between client and server.
- **AC3**: MoveZeroed
  - Given: Move input in each of the four non-Outside states, then Outside.
  - When: input is processed on the server and the owner.
  - Then: zeroed from the first tick of the transition; Outside input untouched.
  - Edge cases: input on the exact tick the Hide executes.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the three tests above under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-002-bush-runtime-from-manifest.md`, `story-003-conceal-state-machine-and-occupancy-record.md`; SCL `production/epics/shared-character-locomotion/story-016-commanded-vs-external-displacement.md`
- Unlocks: `story-011-server-warp-to-anchor-and-exit-motion.md`, `story-012-owner-warp-no-snap.md`
