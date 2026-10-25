# Story 039: Tier application: an AI pawn and a player pawn at equal depth are tiered identically

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R11 Significance-only animation LOD, R11.5 host evaluation (D3); AC-SCL-27; Open Question OQ16 closed)
**Requirement**: `TR-scl-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (sections 2-4)
**ADR Decision Summary**: `UAnimSignificanceSubsystem` (client-local `UWorldSubsystem`, a custom `FTickFunction` in `TG_PrePhysics` as prerequisite of every pawn's CMC and mesh tick) tiers every rendered pawn. Engine URO is off on every pawn mesh on every process; on non-authority clients every mesh is under external tick-rate control (`AlwaysTickPose`, accumulated `ExternalDeltaTime`, alpha set every frame); on authority processes external control is off except the listen host, which tiers every non-locally-controlled pawn with the client bands (uniform `TickPose` escalation, closeout D3). Root-motion, traversal and `PoseLocked` states force rate 1 on every client, winning over every tier.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Gates **ANIM-V1** (external tick-rate control drives Game Animation Sample motion matching without visible pops, inertialised, without breaking montages, root motion or notifies), **ANIM-V3** (listen-host view parity, KS α = 0.001, given ADR-0010 E4) and **ANIM-V6** (the sample compiles on 5.8.4); engine URO is player-keyed in 5.8.4 (`bHumanControlled` from `GetInstigatorController<APlayerController>`) which is why it is off. ANIM-V1 and ANIM-V3 are verified by this story's work (record results in `production/qa/spikes/`); if they fail the fallback is the ADR's rate-1 design applied to every pawn equally, a design decision, not automatic. Needs `USkinnedMeshComponent::EnableExternalTickRateControl`, `SetExternalTickRate`, `EnableExternalEvaluationRateLimiting`, `EnableExternalInterpolation`, `EnableExternalUpdate`, `SetExternalInterpolationAlpha`, `SetExternalDeltaTime` (SkinnedMeshComponent.h:1011-1039).

**Control Manifest Rules (this layer)**:
- Required: engine URO off (`bEnableUpdateRateOptimizations = false`) for every pawn mesh; external tick-rate control on non-authority clients; rate-1 override for root-motion, traversal and `PoseLocked`; `bUseInertialBlend = true` set explicitly on the motion-matching node.
- Forbidden: never use `USkeletalMeshComponentBudgeted`, the AnimationBudgetAllocator, the SignificanceManager or engine URO for pawns; never apply a performance fallback to some pawns or clients only; never set `bOnlyAllowAutonomousTickPose`.
- Guardrail: tier subsystem at most 0.05 ms GT; `C_anim` at most 4.0 ms (story 044).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-27** [P] `TierParityAB`: GIVEN a fixed camera, one AI pawn and one player pawn (replayed input) mirrored at equal depth doing B1 for 60 s, then swapped, WHEN per-frame h, tier, mesh LOD, update rate and motion-matching search count are traced, THEN they are equal for both on every frame (technical-artist protocol).

*QA clarification (create-stories 2026-10-25, not a GDD change):* B1 is the art bible 5.4 calm walk; h is compared within 1e-4; motion-matching search counts are compared per 1 s window (the QA lead noted this relaxes "every frame" for the search count only; the AC author should accept that relaxation explicitly at `/story-readiness`).

---

## Implementation Notes

*Derived from ADR-0013 sections 2-4:*

- Build `UAnimSignificanceSubsystem` over story 037/038's `FAnimTierCore`: per-frame inputs `z`, vertical FOV, `IsEyeMaskVisible(Pawn)` (Camera, ADR-0015; the `WasRecentlyRendered` fallback is development builds only); apply tiers per the ADR section 3 table (A rate 1, B rate 2 with `SearchThrottleTime` `AnimSearchIntervalTierBSec`, C rate 4 with `AnimSearchIntervalTierCSec`, D-visible at least 8 Hz, D-culled no tick with montage position re-derived from replicated state at promotion). Phase spreading by `(FrameCounter + TierTieKey) mod k`.
- Authority: no external control on dedicated server/standalone; the listen host applies the uniform `TickPose` escalation to non-locally-controlled pawns via a pawn mesh subclass override of `TickPose` (requires ADR-0010's agreement to the mesh-component subclass).
- This story depends on story 005 (the AnimBP no longer branches on local control), otherwise "equal for both" cannot hold on the host.
- **Prototype replaced**: none (the prototype uses engine URO defaults, which key on player controllers in 5.8.4).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 037 and 038: the pure tier and cap logic.
- Story 040: frozen-pawn guard. Story 043: footsteps. Story 044: budget gate (R3 hardware).

---

## Files

- `Source/StealthGameplay/Public/View/Animation/AnimSignificanceSubsystem.h` (planned)
- `Source/StealthGameplay/Private/Core/Animation/AnimSignificanceSubsystem.cpp` (planned)
- `Source/StealthGameplay/Private/Core/Animation/StealthSkeletalMeshComponent.cpp` (mesh subclass, planned)
- `Source/StealthGameplay/Private/Tests/Locomotion/TierParityABTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; the per-window search-count relaxation needs the AC author's explicit acceptance). Test `StealthGame.Locomotion.TierParityAB`.*

Test: tier parity between an AI pawn and a player pawn (AC-SCL-27)
  Given: a fixed camera, one AI pawn and one player pawn (replayed input) mirrored at equal depth doing B1 (art bible 5.4 calm walk) for 60 s, then swapped
  When: per-frame h, tier, mesh LOD, update rate and motion-matching search count are traced
  Then: h (within 1e-4), tier, mesh LOD and update rate are equal for both on every frame; search counts are equal per 1 s window
  Edge cases: a time-based search cadence can flicker at frame edges, hence the window

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/TierParityABTest.cpp`, test name `StealthGame.Locomotion.TierParityAB` — must exist and pass; ANIM-V1 and ANIM-V3 results recorded in `production/qa/spikes/`; retained screenshot of tiered pawns in `production/qa/evidence/` (player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-005-animbp-islocallycontrolled-removal.md`, `story-037-tier-formula.md`, `story-038-tier-caps-tie-key.md`; `production/epics/camera-system/EPIC.md` (`IsEyeMaskVisible`, soft)
- Unlocks: `production/epics/shared-character-locomotion/story-040-no-rendered-pawn-frozen.md`, `story-042-camera-toggle-changes-nothing.md`, `story-044-anim-budget-50.md`
