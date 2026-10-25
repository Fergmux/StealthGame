# Story 005: Remove the sample AnimBP's IsLocallyControlled branch for every pawn

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R1, R10; AC-SCL-01 passes only after this story)
**Requirement**: `TR-scl-006` (one AnimBP, motion-matching database, chooser and pose set; `ActionPose`-driven states)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (section 1, "Removal of the sample's `IsLocallyControlled` animation branch", closeout 2026-10-25; also ADR-0010 E10 audit)
**ADR Decision Summary**: The Game Animation Sample AnimBP branches on `IsLocallyControlled`, so on the listen host NPCs and remote players animate differently: an identity tell found in the S1/S2/S14 spike. The branch is removed for every pawn, not patched or gated by kind; whatever it selected is replaced by the replicated-movement inputs listed in ADR-0013 section 1.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Animation is HIGH risk for 5.4-5.8 (5.8 removed deprecated animation symbols from 5.0-5.6; ADR-0013 ANIM-V6 requires the sample AnimBP and Blueprints to compile on 5.8.4 after the removals and the database pruning, and this story reports that result). Spike evidence: `production/qa/spikes/2026-10-25_S1-S2-S14.md` section 3 (`ABP_SandboxCharacter` property access `SandboxCharacter.IsLocallyControlled`; `CBP_SandboxCharacter.CanSprint` selects pending input or CMC acceleration with `IsLocallyControlled`).

**Control Manifest Rules (this layer)**:
- Required: every Shared Pawn uses the one AnimBP, one PoseSearch database set and one chooser set (ADR-0013); the AnimBP reads only the exhaustive input list in ADR-0013 section 1 (replicated movement, `HeadLook`, `RemoteViewPitch16`, `TraversalState`, `FActionState`, `FDeathState`, step-clock phase, per-pawn idle timers seeded identically, tier flags, `EstimatedServerNow()`).
- Forbidden: never select an animation asset, layer or graph by controller, team, participant state or `AppearanceId`; the derived AnimBP graph and `UStealthAnimInstance` contain no read of `IsLocallyControlled`, `IsPlayerControlled`, `GetController`, `PlayerState` or any local-control flag (the only exception is the owner-local first-person head hide, which is not an animation-state input).
- Guardrail: no change to the 4.0 ms animation line from this story (measured in story 044).

---

## Acceptance Criteria

*Derived from ADR-0013 section 1 (the GDD's AC-SCL-01 is the check; its negative control is built in story 004):*

- [ ] GIVEN the derived AnimBP and `UStealthAnimInstance`, WHEN the story-004 class-parity scan runs, THEN it reports zero reads of `IsLocallyControlled`, `IsPlayerControlled`, `GetController`, `PlayerState` or any local-control flag in the AnimBP graph, linked layers and `UStealthAnimInstance` (the owner-local first-person head hide excepted and not an animation-state input), and the same scan run on the unmodified sample AnimBP still fails (negative control).

---

## Implementation Notes

*Derived from ADR-0013 section 1 and ADR-0010 E10 / "Kind-blind code":*

- Create `UStealthAnimInstance` (`StealthGameplay/Core/Animation`) as the C++ parent of the derived AnimBP. Replace every sample read of local control with the replicated-movement inputs: velocity and acceleration from the CMC, movement mode, `HeadLook`, `RemoteViewPitch16`, `TraversalState`, `FActionState`, `FDeathState`. Where the sample used the branch to pick a local-input-driven state or blend (desired gait, input-driven rotation), derive the same value from replicated movement for every pawn.
- `CBP_SandboxCharacter.CanSprint` (the other `IsLocallyControlled` use) disappears with the sample's sprint, which the MVP removes (R10, R5.1); `IsLocallyControlled` stays only in owner-local presentation (camera, own head hide, input binding).
- Also audit `EventBeginPlay`'s `GetLocalRole == SimulatedProxy` binding: the role is captured once at BeginPlay, so after a takeover a body keeps its old role's binding. Re-run role-dependent setup on role change or remove the dependence (spike S14 finding, ADR-0010 note).
- This is a **Content step**: the AnimBP and Blueprint are `.uasset` files covered by `production/never-resave-assets.md`. Edit through the project's MCP toolset by explicit path, never mass-save, take a pre-change snapshot as for `_snapshots/2026-10-24_sample-blueprints-pre-resave/`, and follow ADR-0009 for binary assets. ANIM-V3 (host-view parity) verifies the result afterwards (story 039 and story 044 host capture); report ANIM-V6 (compiles on 5.8.4) in the completion notes.
- **Prototype replaced**: the sample's AnimBP branch is the only artefact replaced; the prototype's `AStealthCharacter` uses the sample AnimBP unmodified (the spike measured the host-visible difference).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004: the scan harness and negative control.
- Story 041: pruning the database set (sprint, crouch, jump assets) and time-based search cadence.
- Story 039 and story 040: tier application and the frozen-pawn guard.

---

## Files

- `Source/StealthGameplay/Public/View/Animation/StealthAnimInstance.h` (planned)
- `Source/StealthGameplay/Private/Core/Animation/StealthAnimInstance.cpp` (planned)
- `Content/` AnimBP and `CBP_SandboxCharacter` edit (by explicit path via MCP; no `.uasset` is written by this story file)
- `Source/StealthGameplay/Private/Tests/Locomotion/ClassParityTest.cpp` (extended by story 004's file; no new test file)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.AnimBpNoLocalControlRead`.*

Test: AnimBP and `UStealthAnimInstance` contain no local-control read (ADR-0013 section 1)
  Given: the derived AnimBP graph, linked layers and `UStealthAnimInstance`
  When: the story-004 scan runs with the AnimBP as root
  Then: no read of `IsLocallyControlled`, `IsPlayerControlled`, `GetController`, `PlayerState` or any local-control flag (owner-local first-person head hide excepted); AC-SCL-01 passes after the removal; the same scan fails on the unmodified sample AnimBP (negative control)
  Edge cases: "any local-control flag" is bounded by the fixture of flags enumerated in spike S1/S2; `CBP_SandboxCharacter.CanSprint` is outside the AnimBP and is covered by story 004's roots

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the `StealthGame.Locomotion.ClassParity` run (story 004) passing on the derived AnimBP; recorded failure on the unmodified sample; before/after snapshot paths in the completion notes

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-004-class-parity-scan.md`, `story-001-pawn-class-default-subobjects.md`
- Unlocks: `production/epics/shared-character-locomotion/story-039-tier-parity-ab.md`, `story-041-body-and-anim-set-parity.md`
