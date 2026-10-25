# Story 029: Every enter, exit and emerge plays one identical rustle, and an occupied bush plays nothing

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R13, Visual/Audio Requirements, AC-CON-18)
**Requirement**: `TR-con-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§9 rustle and audio parity)
**ADR Decision Summary**: A stateless `MPC_BushRustle` with 8 ring-buffered pulse slots `{WorldPos.xyz, StartTime; SideDir.xy, Amplitude, unused}`; every bush material runs the same WPO function and responds only to a pulse within about 1.5 m of its face. Each client writes a slot when `RefreshPresentation` sees a `BushEnter`, `BushExit` or bush-emerge pose for any pawn, timed from the pose's start server time, at the bush face on the transition's side. One shared SFX pool is spatialised at the bush face. Identical for every pawn kind, both camera modes and every LOD, never scaled by occupancy, absent while a bush is merely occupied.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; MPC with 8 pulse slots x 2 `float4` is trivially small [U, E7: no limit constant found in 5.8.4])
**Engine Notes**: The Material Parameter Collection is updated client-side without replication. The rustle is tagged eye-masked for the third-person mask (Camera CS-R5b, ADR-0015). The foliage art is pending (prototype known issue 8), so the story is built against the graybox bush and its pulse hook.

**Control Manifest Rules (this layer)**:
- Required: Rustle is driven by the pose on every client for every pawn kind; it is the only sign a bush gives — source: ADR-0017
- Forbidden: Never scale the rustle by occupant count; never fire it while a bush is merely occupied — source: ADR-0017
- Guardrail: Pulse slots are written client-locally; no replication

The rustle makes entry visible to anyone with line of sight and leaves no trace for anyone without; it is part of why NPC and player bush use are indistinguishable.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-18, CON-R13) and ADR-0017 §9, scoped to this story:*

- [ ] **AC1** (AC-CON-18 automated part, [PIE-2] `StealthGame.Concealment.RustleParity`): **GIVEN** an NPC and a player each entering, exiting and bush-emerging at the same bush with 0, 1 and 4 occupants, at LOD 0 and the farthest LOD **THEN** the MPC pulse, SFX asset, onset and amplitude are identical in 1P and 3P and independent of occupant count; no pulse or sound fires while the bush is merely occupied.
- [ ] **AC2** (AC-CON-18 read test, [Vis] VA-2): **GIVEN** 12 testers at 20 m in greyscale **THEN** ≥ 85 % name the rustle within 2 s and ≥ 50 % call the 2× amplitude too loud; the results sheet is retained in `production/qa/evidence/` with sign-off.
- [ ] **AC3** (derived, ADR-0017 §9, [U] `StealthGame.Concealment.RustlePulseSlot`): **GIVEN** any `BushEnter`, `BushExit` or bush-emerge pose on any client **THEN** one slot of the 8-slot ring is written from the pose's `StartServerTime` (onset 0.1 s before the swap or crossing) at the bush face on the transition's side, the SFX comes from one shared pool spatialised at the face, and the rustle is tagged eye-masked; a 9th pulse overwrites the oldest slot.

---

## Implementation Notes

*Derived from ADR-0017 Decision §9:*

`UBushRuntimeSubsystem::PushRustlePulse(FBushId, uint8 Side, double StartServerTime)` writes the client-local MPC slot, called from the `RefreshPresentation` path for any pawn's hide pose. `BushRustleAmplitudeCm` (10, safe 8-12) must pass the 20 m read test and fail at 2×. Slots are reused round-robin client-locally. The same handler fires the SFX; nothing audible while occupied (VA-6). The read test needs a build and 12 testers: schedule it with the producer; the automated ACs can close first, and the story closes only with all three.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Art epic assets: the final foliage material, MPC authoring and SFX recordings (graybox stand-ins here).
- `story-028-enter-swap-hides-body-per-viewer.md`: the hide at the swap.
- Camera `story-015-mask-apply-path.md`: the eye-mask application.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/BushRustle.cpp`
- `Source/StealthGameplay/Features/Concealment/Private/BushRuntimeSubsystem.cpp` (`PushRustlePulse`)
- `Source/StealthGameplay/Private/Tests/Concealment/RustlePulseSlotTest.cpp` (class `FConcealmentRustlePulseSlotTest`, name `StealthGame.Concealment.RustlePulseSlot`)
- `Source/StealthTestHarness/Private/Tests/PIE/RustleParityTest.cpp` (`NETWORK_TEST_CLASS(RustleParity, "StealthGame.Concealment.PIE")`)
- `production/qa/evidence/con-rustle-*.png`, `con-rustle-readtest.md`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; the qa-lead noted AC1 and AC3 are automated and AC2 is manual).*

- **AC1**: RustleParity (automated, PIE-2)
  - Given: an NPC and a player entering, exiting and bush-emerging with 0, 1 and 4 occupants, at LOD 0 and the farthest LOD.
  - When: pulses and sounds are logged.
  - Then: the MPC pulse, SFX asset, onset and amplitude are identical in 1P and 3P and independent of occupancy; no pulse fires while the bush is merely occupied.
- **AC3**: RustlePulseSlot (automated)
  - Given: any BushEnter, BushExit or bush-emerge pose.
  - When: the pulse is written.
  - Then: one slot of the 8-slot ring is written from StartServerTime (onset 0.1 s before the swap or crossing) at the bush face on the transition's side; the SFX comes from one shared pool and the rustle is tagged eye-masked.
  - Edge cases: a 9th pulse overwrites the oldest slot.
- **AC2**: Manual check: read test
  - Setup: 12 testers at 20 m, greyscale.
  - Verify: the share who name the rustle within 2 s, and the share who call the 2× amplitude too loud.
  - Pass condition: at least 85 % and at least 50 % respectively, with the sheet retained and sign-off recorded. Deferred until a build and 12 testers are available (producer to schedule).

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained read-test sheet and captures `con-rustle-*` in `production/qa/evidence/` plus sign-off in `con-rustle-readtest.md`; the automated tests above also exist

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-002-bush-runtime-from-manifest.md`, `story-004-hide-enter-execution-and-commit.md`, `story-005-hide-exit-execution-and-blocked-side.md`; Camera `production/epics/camera-system/story-015-mask-apply-path.md`
- Unlocks: `story-035-prototype-retirement-and-regression.md`
