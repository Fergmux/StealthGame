# Story 043: Footsteps come from one step clock, identical for every pawn

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R18 Footstep path parity, F11 Step clock; AC-SCL-31; OQ12)
**Requirement**: `TR-scl-011` (footstep path parity from one step clock)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (section 6)
**ADR Decision Summary**: `UStealthStepClockComponent` (on every pawn, every process that plays audio) implements R18/F11 from movement only: distance travelled by the rendered smoothed position divided by `stride(v)`; start step after `FootstepStartDelaySec`, settling step below `PivotSpeedCmPerSec`, one step per 60° of in-place pivot; suppressed while falling, in traversal, in `PoseLocked`, and while the client-local `bPresentationHidden` latch is set; absent for pawns culled from this client. It never reads animation state, tier, controller, team or Roster. Every footstep uses one `SC_Footsteps` concurrency, one sound class and submix, one attenuation asset, identical priority, with no per-pawn parameter. Its phase is published to `UStealthAnimInstance` so stride warping follows the clock at tiers A-C.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Gates **ANIM-V4** (step clock holds visible plants within ±50 ms at tiers A-C under motion matching; fallback if not: tier-A pawns fire the event from a foot-contact curve snapped to the clock phase, still identical for every pawn, SCL OQ12) and **ANIM-V5** (footstep audibility parity under `au.EnableRelativeRenderCostVoiceLimit` = 1 with 50 walking pawns: played-step ratio independent of kind, χ², α = 0.001). `au.EnableRelativeRenderCostVoiceLimit` defaults to 1 in 5.8 (E7), so every footstep source needs identical render cost: the same MetaSound graph per surface type. Audio is MEDIUM risk (5.8 WASAPI default). Both gates are verified by this story's tests and recorded in `production/qa/spikes/`.

**Control Manifest Rules (this layer)**:
- Required: footsteps come only from `UStealthStepClockComponent` using the rendered smoothed position; every footstep uses `SC_Footsteps`, one sound class, one attenuation, one priority, no per-pawn parameter.
- Forbidden: never drive footsteps from anim notifies (audibility would depend on tier); never read animation state, tier or controller in the clock.
- Guardrail: step clocks count inside the animation line (`C_anim`).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-31** [N] `FootstepParity`: GIVEN an NPC and a player walking, one in tier A and one in D-culled (off-screen), at 112, 126 and 140 cm/s and running at 450, plus a tier change mid-walk, WHEN step events are counted for 30 s on an observer, THEN cadence is 1.8 / 1.9 / 2.0 / 3.46 ± 0.05 steps/s (± 1 step per start or stop), identical for both pawns, with identical event fields and no controller or team field; AND for a tier-A pawn |t_sound − t_visible plant| ≤ 50 ms on ≥ 95 % of steps; AND no step fires during a vault, a fall or a strike.

*QA clarification (create-stories 2026-10-25, not a GDD change):* t_visible plant is the descending foot-bone vertical-speed zero crossing at ground contact only (the swing-apex crossing is excluded); the vault, fall and strike clause is deferred to the traversal and owning-feature stories.

---

## Implementation Notes

*Derived from ADR-0013 section 6 and GDD F11:*

- Implement `UStealthStepClockComponent` and `FStepEvent {Pawn, Foot, SurfaceType, Gait}` (no controller, team, role or `AppearanceId` field) firing one `UFootstepAudioSubsystem` entry point; walk cadence 1.8 steps/s at the slowest pace to 2.0 at full pace, stride = speed / cadence; run stride `FootstepStrideRunCm` (130). Any resync slews over at least 3 strides. Position source is the rendered smoothed position (simulated proxies after R14 smoothing; the host uses the same smoothed position for remote-client pawns).
- Create `SC_Footsteps` (`StopFarthestThenOldest`, one max-count, `bLimitToOwner = false`) and the shared sound class, submix and attenuation assets: Content, by explicit path via MCP. Diegetic Audio (Vertical Slice) designs the sounds; this story binds the path.
- Publish the clock phase to the AnimBP for stride warping and distance matching (tiers A-C).
- **Prototype replaced**: none (the sample's anim-notify foley is the path this replaces for audibility; the sample's `BP_AnimNotify_FoleyEvent` stays presentation only).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Diegetic Audio sound design (Vertical Slice).
- Story 039: tier application. Traversal/strike suppression asserted by traversal stories.

---

## Files

- `Source/StealthGameplay/Public/View/Animation/StealthStepClockComponent.h` (planned)
- `Source/StealthGameplay/Private/Core/Animation/StealthStepClockComponent.cpp` (planned)
- `Source/StealthTestHarness/Private/Locomotion/FootstepParityTest.cpp` (planned)
- Content: `SC_Footsteps` concurrency, sound class, attenuation (explicit paths via MCP)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; vault, fall and strike clause deferred). Test `StealthGame.Locomotion.PIE.FootstepParity.NpcVsPlayer`.*

Test: footstep parity (AC-SCL-31, ANIM-V4, ANIM-V5)
  Given: an NPC and a player walking (one in tier A, one D-culled) at 112, 126 and 140 cm/s and running at 450, plus a tier change mid-walk
  When: step events are counted for 30 s on an observer
  Then: cadence is 1.8 / 1.9 / 2.0 / 3.46 ± 0.05 steps/s (± 1 step per start or stop), identical for both pawns; event fields are identical with no controller or team field; for a tier-A pawn |t_sound − t_visible plant| ≤ 50 ms on ≥ 95 % of steps (t_visible plant = descending foot-bone vertical-speed zero crossing at ground contact only)
  Edge cases: "no step fires during a vault, a fall or a strike" is asserted by the traversal and owning-feature stories

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/FootstepParityTest.cpp` — must exist and pass (multi-client PIE with audio events); ANIM-V4 and ANIM-V5 results in `production/qa/spikes/`; retained screenshot of the plant/step alignment capture in `production/qa/evidence/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-010-speed-tiers.md`, `story-039-tier-parity-ab.md`, `story-035-no-observer-snap.md` (smoothed position)
- Unlocks: Diegetic Audio (Vertical Slice)
