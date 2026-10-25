# Story 040: No rendered pawn is ever frozen or unposed

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R11.3 "No rendered pawn is ever unposed or frozen", R11.4; AC-SCL-46)
**Requirement**: `TR-scl-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (section 3: tier table, D-visible and D-culled rows)
**ADR Decision Summary**: D-visible pawns update at a rate of `max(1, floor(FrameHz / AnimTierDVisibleMinHz))` so pose updates stay at 8 Hz or better; D-culled pawns (not rendered) do not tick, and on promotion `EnableExternalUpdate(true)` runs that frame with the montage position re-derived from replicated state (`FActionState`/`FDeathState`/`TraversalState` at `EstimatedServerNow()`), inertialised from the last pose, so a returning pawn is posed in its first rendered frame.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Needs a real renderer to know what is "rendered": `-nullrhi` cannot establish this, so the test is a rendered nightly (the CI rule in `.claude/docs/coding-standards.md` runs headless suites with `-nullrhi`). "Rendered" is Camera's eye-mask flag, not the engine's was-recently-rendered test.

**Control Manifest Rules (this layer)**:
- Required: the rate-1 override for root-motion, traversal and `PoseLocked` states wins over every tier including D-culled; a D-culled pawn's montage position is re-derived from replicated state at promotion.
- Forbidden: never freeze a rendered pawn; never skip the eye-origin test by distance; never apply a fallback to some pawns only.
- Guardrail: tier D-visible stays within the 4.0 ms line.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-46** [P] `NoRenderedPawnFrozen`: GIVEN a camera that turns away for 0.3 s and back to 49 pawns, and a scene with 60 pawns (`NpcCount` 50), WHEN pose evaluation per pawn is traced, THEN every rendered pawn's pose updates at ≥ 8 Hz in every 1 s window, pawns returning to view are posed in their first rendered frame, and the owner's pawn occupies one of the 8 tier-A slots when its h ranks it there.

*QA clarification (create-stories 2026-10-25, not a GDD change):* the 1 s windows are sliding; the test runs in a rendered nightly (deferred from `-nullrhi` CI).

---

## Implementation Notes

*Derived from ADR-0013 section 3:*

- Trace per-pawn pose evaluations (test-only counter on the mesh component) over sliding 1 s windows for every rendered pawn, including D-visible; assert at least 8 evaluations per window. The camera-turn scenario culls and un-culls 49 pawns at 0.3 s: assert each returning pawn's first rendered frame has an evaluated pose (no stale or T pose).
- Confirm the owner's pawn takes one of the 8 tier-A slots when `h` ranks it there (R11.4, no exemption for the local viewer).
- **Prototype replaced**: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 039: tier application. Story 044: cost budget.

---

## Files

- `Source/StealthGameplay/Private/Tests/Locomotion/NoRenderedPawnFrozenTest.cpp` (planned; rendered nightly)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; rendered nightly). Test `StealthGame.Locomotion.NoRenderedPawnFrozen`.*

Test: no rendered pawn frozen (AC-SCL-46)
  Given: a camera that turns away for 0.3 s and back to 49 pawns, and a scene with 60 pawns (`NpcCount` 50)
  When: pose evaluation per pawn is traced
  Then: every rendered pawn's pose updates at ≥ 8 Hz in every sliding 1 s window; pawns returning to view are posed in their first rendered frame; the owner's pawn occupies one of the 8 tier-A slots when its h ranks it there
  Edge cases: needs a real renderer (`-nullrhi` cannot establish "rendered")

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/NoRenderedPawnFrozenTest.cpp`, test name `StealthGame.Locomotion.NoRenderedPawnFrozen` — must exist and pass (rendered nightly); retained screenshots of the camera turning back in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-039-tier-parity-ab.md`; `production/epics/camera-system/EPIC.md` (eye-mask flag)
- Unlocks: None
