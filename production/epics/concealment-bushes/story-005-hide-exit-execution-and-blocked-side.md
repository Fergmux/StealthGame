# Story 005: Hide(Exit) picks an open, unblocked side, is never refused and un-culls in the same step

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R8, F3 exit rows, AC-CON-10)
**Requirement**: `TR-con-001`, `TR-con-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§2 exit, §3 HideExit), with ADR-0003 §5.6 (un-cull in the same step)
**ADR Decision Summary**: The side index is validated (a closed side maps to the nearest open side); the blocked-side search considers only blocking pawns (Outside, or past their crossing); an exit is never refused and depenetration is capped at `BushExitDepenetrationCapCm`. On execution the state is Exiting, the un-cull happens in the same slot, and Cross and End items are scheduled.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ here; engine `MaxDepenetrationWithPawn` [V], default value [S])
**Engine Notes**: Depenetration uses the engine's capsule depenetration capped by `BushExitDepenetrationCapCm`; the default of the engine value is unverified (marked [S] in the GDD), so the cap must be enforced by Concealment, not assumed from the engine.

**Control Manifest Rules (this layer)**:
- Required: Un-cull happens in the same slot as exit execution — source: ADR-0003, ADR-0017
- Forbidden: Never let Hide legality, replies or timing read occupancy, controller kind or team — source: ADR-0017
- Guardrail: Exit is never refused; no reply differs by blocking

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-10, CON-R8), scoped to this story:*

- [ ] **AC1** (AC-CON-10, [U] `StealthGame.Concealment.ExitSideAndBlocked`): **GIVEN** a bush with 2 open sides **WHEN** Exit names a closed side **THEN** the nearest open side is used; **WHEN** an Outside pawn occupies the first side's end point **THEN** the second is used; **WHEN** a concealed co-occupant or an Entering pawn stands at the end point **THEN** it does not block; **WHEN** both sides are blocked by Outside pawns **THEN** the requested side is used and each pawn is displaced by ≤ `BushExitDepenetrationCapCm` (30 cm). No exit is ever refused and no reply differs.
- [ ] **AC2** (derived, CON-R8, CON-R6a): **GIVEN** Exit executing at T **THEN** state = Exiting, `SetCullState(Pawn, false)` is called in the same slot (seam), `FActionState {BushExit, StartServerTime = T}` is written, Cross at T + `BushExitCrossSec` (0.45 s) removes the pawn from the occupancy record and clears the Owner/Team flags, End at T + `BushExitDurSec` (0.9 s) sets state Outside and starts the re-entry cooldown.
- [ ] **AC3** (derived, CON-R8 side index): **GIVEN** look-yaw samples at each quantisation boundary ±1° **THEN** the 2-bit side index equals the mapping: bush-local side k (0..3, one per footprint side) covers yaw sector [k·90−45, k·90+45) degrees in bush-local yaw, lower bound inclusive; a requested closed side maps to the open side nearest the look yaw (angular distance from the look yaw to each side centre k·90) and an angular tie goes to the lower index; the AI path gives the identical index for identical yaw.

---

## Implementation Notes

*Derived from ADR-0017 Decision §2 and §3:*

The side index is chosen by the client from its look yaw (change request to IIF; AI chooses through the same path). The server validates the side is open, else uses the open side nearest it. Blocked-side search: test open sides in angular order from the requested one and take the first whose end-point capsule overlaps no **blocking pawn** (a pawn in Outside, or in Exiting/Emerging after its leaf crossing; pawns in Entering, Concealed or before their crossing never block); if all are blocked use the requested side and let depenetration separate the pawns when collision returns, capped at `BushExitDepenetrationCapCm` (never a launch). On execution: state Exiting, un-cull in the same server step (IVP R7.5), `ActionPose = BushExit` with its start server time, root motion over `BushExitDurSec`. The body shows on every client only at the outward leaf crossing `Start + BushExitCrossSec` (F3: ≥ `T_uncull`). The pawn leaves the occupancy record at the crossing; the re-entry cooldown starts when the transition completes.

**The yaw-to-side mapping in AC3 is the author's proposal and is not GDD text**: the game-designer or IIF owner should confirm it before implementation (logged in the epic's decision log).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-011-server-warp-to-anchor-and-exit-motion.md`: the exit root motion.
- `story-027-uncull-before-leaf-plane.md`: observed un-cull timing under net profile.
- `story-007-hide-legality-predicate.md`: `Cooldown` reply using the cooldown this story starts.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp` (exit resolver, blocked-side search)
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentTransitionResolver.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/ExitSideAndBlockedTest.cpp` (class `FConcealmentExitSideAndBlockedTest`, name `StealthGame.Concealment.ExitSideAndBlocked`)
- `Source/StealthGameplay/Private/Tests/Concealment/ExitTimelineTest.cpp` (class `FConcealmentExitTimelineTest`)
- `Source/StealthGameplay/Private/Tests/Concealment/SideIndexFromYawTest.cpp` (class `FConcealmentSideIndexFromYawTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; AC3 revised and re-checked).*

- **AC1**: ExitSideAndBlocked (verbatim AC-CON-10)
  - Given: a bush with 2 open sides.
  - When: Exit names a closed side, then an Outside pawn stands at the first end point, then a co-occupant or an Entering pawn stands there, then both sides are blocked by Outside pawns.
  - Then: nearest open side; second side when the first is blocked; the co-occupant and Entering pawn do not block; with both blocked the requested side is used and each pawn is displaced by 30 cm or less; no exit refused, no reply differs.
  - Edge cases: the cap follows the tuning key (test 15 and 40 cm); exactly 30 cm passes.
- **AC2**: ExitTimeline
  - Given: Exit executing at T.
  - When: the queue runs to T + 0.9 s.
  - Then: Exiting and `SetCullState(false)` in the same slot; `FActionState{BushExit, T}`; Cross at T + 0.45 s leaves the record and clears Owner/Team flags; End at T + 0.9 s gives Outside and starts the cooldown.
  - Edge cases: a stale generation drops Cross and End.
- **AC3**: SideIndexFromYaw
  - Given: bush-local look yaw normalised to [0, 360), side k covering [k·90−45, k·90+45).
  - When: yaw is sampled at each boundary at −1, 0 and +1 degree.
  - Then: 44 gives side 0, 45 and 46 give side 1, 314 gives side 3, 315 gives side 0 (lower bound inclusive); a closed requested side maps to the open side nearest the look yaw; a tie goes to the lower index (example: yaw 45 with side 1 closed gives side 0); the AI path gives the identical index.
  - Edge cases: the tie reference is angular distance from the look yaw to each side centre; the mapping needs design confirmation.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the three tests above under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-003-conceal-state-machine-and-occupancy-record.md`, `story-004-hide-enter-execution-and-commit.md`
- Unlocks: `story-007-hide-legality-predicate.md` (cooldown source), `story-011-server-warp-to-anchor-and-exit-motion.md`, `story-027-uncull-before-leaf-plane.md`
