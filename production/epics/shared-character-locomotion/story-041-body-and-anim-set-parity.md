# Story 041: Identical body and animation set; no sprint, crouch or jump assets; time-based search

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R10 Animation set, R5.6, R17; AC-SCL-44)
**Requirement**: `TR-scl-006`, `TR-scl-013`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (section 1 and section 3 search cadence)
**ADR Decision Summary**: One AnimBP, one PoseSearch database set and one chooser set; the sample's sprint, crouch, jump and unused overlay databases are removed. Motion-matching `SearchThrottleTime` is bound from an AnimInstance property in seconds, so cadence is time-based and frame-rate independent.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Gate **ANIM-V2**: `SearchThrottleTime` bound from an AnimInstance property is honoured per instance (tier-B search count over 10 s equal at 30 and 120 fps ± 5 %); this story's test is that verification, recorded in `production/qa/spikes/`. `FAnimNode_MotionMatching::SearchThrottleTime` accumulates the update's delta time (E5, E11). Gate **ANIM-V6**: the pruned sample compiles on 5.8.4 (5.8 removed deprecated animation symbols from 5.0-5.6). Content edits (database pruning) are `.uasset` changes: explicit path via MCP, never mass-save (`production/never-resave-assets.md`).

**Control Manifest Rules (this layer)**:
- Required: one AnimBP, one PoseSearch database set, one chooser set; motion-matching search cadence is time-based (`SearchThrottleTime` in seconds).
- Forbidden: never use a frame count for search cadence; never select an animation asset, layer or graph by controller, team, participant state or `AppearanceId`.
- Guardrail: stays within the 4.0 ms animation line.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-44** [U] `BodyAndAnimSetParity`: GIVEN an NPC pawn and a player pawn, WHEN capsule size, collision profile, physics asset, RVO flag, AnimBP class and database set are read, THEN all are identical, RVO avoidance is off on both, and the database set contains no sprint, crouch or jump assets (R10, R5.6, R17); AND the tier-B search count over 10 s is the same at 30 and 120 fps ± 5 % (time-based cadence).

*QA clarification (create-stories 2026-10-25, not a GDD change):* sprint, crouch and jump assets are detected by an asset tag and name list held as a fixture.

---

## Implementation Notes

*Derived from ADR-0013 section 1 and GDD R10:*

- Prune the sample's PoseSearch databases and chooser rows: remove sprint, crouch, jump and unused overlays; keep walk 112-140 cm/s (play rate 0.9-1.1 plus stride warping), run 450 cm/s, idle variants (at least 3, weight shift every 4-9 s from a per-pawn timer seeded identically for every kind, onset at least 0.8 s, no loop shorter than 4 s), starts, stops, pivots, the shared contact reaction (story 020), brace and traversal. The prune is a Content step; report ANIM-V6.
- Expose `SearchThrottleTime` through a `UStealthAnimInstance` property in seconds (`AnimSearchIntervalTierBSec`, `AnimSearchIntervalTierCSec`); test with a simulated 30 and 120 fps world counting searches over 10 s.
- The class-equality half reads the body defaults and the AnimBP class from two spawned pawns (an NPC and a player stand-in).
- **Prototype replaced**: the sample's full database set (sprint, crouch, jump) is pruned.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: capsule and step height values. Story 005: removal of the local-control branch. Story 039: tier application.

---

## Files

- `Source/StealthGameplay/Private/Core/Animation/StealthAnimInstance.cpp` (`SearchThrottleTime` property)
- Content: pruned PoseSearch databases and chooser (explicit paths via MCP)
- `Source/StealthGameplay/Private/Tests/Locomotion/BodyAndAnimSetParityTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.BodyAndAnimSetParity`.*

Test: body and animation set parity (AC-SCL-44, ANIM-V2)
  Given: an NPC pawn and a player pawn
  When: capsule size, collision profile, physics asset, RVO flag, AnimBP class and database set are read, and the tier-B search count is measured over 10 s at 30 and 120 fps
  Then: all read properties are identical and RVO avoidance is off on both; the database set contains no sprint, crouch or jump assets (by the fixture's asset tag and name list); the tier-B search count is the same at 30 and 120 fps ± 5 %
  Edge cases: the fps clause needs a world with animation evaluation, not a pure unit

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Locomotion/BodyAndAnimSetParityTest.cpp`, test name `StealthGame.Locomotion.BodyAndAnimSetParity` — must exist and pass; ANIM-V2 and ANIM-V6 results in `production/qa/spikes/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-002-body-constants-published.md`, `story-005-animbp-islocallycontrolled-removal.md`
- Unlocks: `production/epics/shared-character-locomotion/story-020-contact-brush-parity.md`, `story-039-tier-parity-ab.md`
