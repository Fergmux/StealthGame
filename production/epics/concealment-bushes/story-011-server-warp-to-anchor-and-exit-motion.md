# Story 011: Every pawn is warped to the anchor on enter and root-moved out on exit by the same server path

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R4a, CON-R5, CON-R8, F3(d), AC-CON-28 PIE collision part)
**Requirement**: `TR-con-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§4 first bullet)
**ADR Decision Summary**: The warp is `FRootMotionSource_MoveToForce` from the execution position to the fixed bush anchor over a calm-gait time bounded by F3(d), applied on the server for every pawn; NPC and player use the same server path. Owner smoothness is a separate, spike-gated concern (story 012).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; the server-side `MoveToForce` source is [V], owner behaviour is not touched here)
**Engine Notes**: `FRootMotionSource_MoveToForce` exists (`RootMotionSource.h:545` [V]); `MoveToDynamicForce` is not needed because the anchor is fixed. `bServerAcceptClientAuthoritativePosition` is banned (ADR-0003 §3).

**Control Manifest Rules (this layer)**:
- Required: NPC and player pawns use the same server warp path — source: ADR-0017
- Forbidden: Never accept a client position (`bServerAcceptClientAuthoritativePosition`) — source: ADR-0003, ADR-0017
- Guardrail: Mean warp speed ≤ 1.5 × walk speed (F3d)

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (CON-R5, CON-R8, F3(d), AC-CON-28), scoped to this story:*

- [ ] **AC1** (derived, CON-R5, F3(d)): **GIVEN** Hide(Enter) executing for an NPC and a player pawn **THEN** both receive the same `FRootMotionSource_MoveToForce` to the same fixed anchor (not `MoveToDynamicForce`), the warp's mean speed (`BushEntryReachCm` + r_c) / `BushEnterSwapSec` = 190 cm/s is ≤ 1.5 × walk speed, and at +`BushEnterSwapSec` the capsule lies wholly inside the `HideVolume` (the commit guard of story 004 passes).
- [ ] **AC2** (derived, CON-R8): **GIVEN** Hide(Exit) **THEN** root motion over `BushExitDurSec` (0.9 s) moves the pawn to within 1 cm of the exit point on the chosen side, collision follows story 008's pure function, and no client position is ever accepted (`bServerAcceptClientAuthoritativePosition` is false, asserted).
- [ ] **AC3** (AC-CON-28 PIE part, [U][PIE-2] `StealthGame.Concealment.CollisionAndAvoidance` functional map): **GIVEN** an Outside pawn walking into a `HideVolume` **THEN** it is blocked (displacement into the volume is 0); **GIVEN** two pawns entering one bush **THEN** neither collides with the other or its own bush during the warp, and a third Outside pawn still blocks (this is the physical check deferred from story 008 AC1b).

---

## Implementation Notes

*Derived from ADR-0017 Decision §4:*

The server starts the source for every pawn at Hide execution; the target is the footprint centre anchor at ground level inside the curb ring, and the duration is `BushEnterSwapSec`-bounded. F3(d) at defaults: (60 + 35) / 0.5 = 190 cm/s ≤ 210 (1.36 × walk 140). v_walk and r_c are SCL's provisional values and must be read, never hardcoded. A blocked exit side uses the cap of story 005. Depenetration is capped, never a launch. The PIE functional map for AC3 places a bush, an Outside walker and a second hider in the graybox city.

**Prototype replaced**: the prototype teleports/overlap-moves the pawn into the bush (`SetInBush`); nothing promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-owner-warp-no-snap.md`: owner-side smoothness (spike S-CON-1 gated).
- `story-008-conceal-collision-pure-function.md`: the collision function.
- `story-010-spike-s-con-1-owner-side-warp.md`: the spike.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentWarp.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentWarp.h`
- `Source/StealthGameplay/Private/Tests/Concealment/WarpSpeedTest.cpp` (class `FConcealmentWarpSpeedTest`, Logic part)
- `Source/StealthTestHarness/Private/Tests/PIE/CollisionAndAvoidanceMapTest.cpp` (functional map test)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; AC3 added after the qa-lead's re-check of story 008).*

- **AC1**: WarpSpeed
  - Given: Hide(Enter) for an NPC and a player.
  - When: the slot runs.
  - Then: both get `FRootMotionSource_MoveToForce` (not Dynamic) to the same fixed anchor; mean speed (60 + 35)/0.5 = 190 cm/s and at most 1.5 × walk speed; at +0.5 s the capsule is wholly inside the HideVolume.
  - Edge cases: reach 60 is the worst case; v_walk is read from SCL (provisional 140).
- **AC2**: ExitMotion
  - Given: Hide(Exit) on a chosen side.
  - When: 0.9 s of root motion completes.
  - Then: the pawn is within 1 cm of the exit point, collision follows the story-008 function, no client position is ever accepted (flag false, asserted).
  - Edge cases: a blocked side displaces by at most 30 cm.
- **AC3**: CollisionMap
  - Given: a PIE functional map with an Outside pawn walking into the volume, then two pawns entering one bush.
  - When: the pawns move.
  - Then: displacement into the volume is 0 for the first; no blocking hit for either pawn in the second.
  - Edge cases: a third Outside pawn still blocks during the warp.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `WarpSpeedTest.cpp` and the PIE functional-map test — must exist and pass, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-001-concealment-tuning-keys-and-timing-validator.md`, `story-004-hide-enter-execution-and-commit.md`, `story-005-hide-exit-execution-and-blocked-side.md`, `story-008-conceal-collision-pure-function.md`
- Unlocks: `story-012-owner-warp-no-snap.md`, `story-013-inside-the-bush-stillness-and-view.md`
