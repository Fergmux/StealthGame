# Story 013: Inside a bush the body is still, the view is clamped and Prepare and Interact are refused

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R7, CON-R9 table rows Prepare/Interact/Run, AC-CON-09)
**Requirement**: `TR-con-001` (CON-R7 rules; no TR is specific to the inside view)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§2 Inside, §6)
**ADR Decision Summary**: Inside, `Move` produces no displacement, body yaw is locked where the `BushEnter` clip ends, `Look` is free within the head clamp only, Prepare and Interact are rejected `NotNow`, and there is no dwell, capacity, reveal or timeout. The camera follows CS-R11.3 (3P boom clamped to `ConcealedBoomCm`, height 0, mask from the eye inside the leaves), supplied by the Camera system from the owner's `IsConcealed`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; stillness is plain input handling, the view part is Camera's)
**Engine Notes**: The lens must stay inside the dome (1.8 × 1.9 m), which needs a boom of ≤ 60 cm from the anchor; `ConcealedBoomCm` default 60 with hard maximum 70 (`BushDomeInnerRadiusCm` 90 − 20 margin, Camera invariant I-CS-4, VA-8). Bush foliage ignores the `Camera` and `ViewBlock` channels.

**Control Manifest Rules (this layer)**:
- Required: Owner `IsConcealed` is the only input the Camera takes from Concealment — source: ADR-0017, ADR-0015
- Forbidden: Never add a dwell, capacity, reveal or timeout key; no minimum or maximum stay — source: ADR-0017
- Guardrail: Retained screenshots in `production/qa/evidence/` for the 1P and 3P inside view

The body does not turn: a freely turning body would give a full panorama from a safe spot (hide-and-wait); the narrow view is part of what the bush costs. This story supplies the owner `IsConcealed` input, the body-yaw lock and the Move zeroing; Camera applies CS-R11.3 through its own stories.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-09, CON-R7, CON-R9), scoped to this story:*

- [ ] **AC1** (AC-CON-09 server part, [PIE-2] `StealthGame.Concealment.InsideViewAndStillness`): **GIVEN** a concealed owner holding Move for 5 s and sweeping Look through its full range **THEN** server displacement is 0 cm, body yaw changes by 0°, and no footstep or rustle fires; Prepare and Interact reply `NotNow` (IIF R9) and the camera toggle works.
- [ ] **AC2** (AC-CON-09 view part, [1P/3P][Vis]): **GIVEN** 3P **THEN** the lens stays inside the dome at every sample (boom = `ConcealedBoomCm`, 60 cm from the anchor at the default and never above `BushDomeInnerRadiusCm` − 20 = 70 cm, VA-8), height 0, and the grade and vignette match art bible 2.8; `Look` is free within the head clamp only. Evidence `con-inside-{1p,3p}.png` retained in `production/qa/evidence/` with lead sign-off.
- [ ] **AC3** (derived, CON-R7, CON-R9): **GIVEN** a concealed pawn **THEN** bush foliage ignores the `Camera` and `ViewBlock` channels (Camera and ViewBlock traces pass through), Run input has no effect and there is no dash-out, and after an exit completes normal locomotion (Run allowed) resumes.

---

## Implementation Notes

*Derived from CON-R7 and ADR-0017 §2:*

`Move` input produces no displacement while Concealed (story 008 zeroes it from transition start; this story keeps it zero through the whole Concealed state), so no footsteps, no rustle and no occupied-vs-empty difference. Body yaw is locked where the `BushEnter` clip ends (facing out of the entry side). The head clamp value comes from Camera story-014. Prepare and Interact `NotNow` is IIF R9's default, confirmed in the GDD (a concealed brace would stack two advantages behind one tell; concealed tending defeats P4); the predicate wiring is IIF `story-052-no-brace-or-tend-in-bush.md`. The camera toggle (V) stays local and works. Run held during Entering is rejected by story 007's legality; here only the post-commit state is covered.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Camera `production/epics/camera-system/story-014-concealed-and-informant-view.md`: the CS-R11.3 boom clamp, grade and vignette implementation.
- IIF `story-052-no-brace-or-tend-in-bush.md`: the Prepare/Interact verdict routing.
- `story-012-owner-warp-no-snap.md`: owner warp smoothness.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentInside.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/InsideStillnessTest.cpp` (class `FConcealmentInsideStillnessTest`, name `StealthGame.Concealment.InsideViewAndStillness`)
- `Source/StealthGameplay/Private/Tests/Concealment/InsideFoliageRunTest.cpp` (class `FConcealmentInsideFoliageRunTest`)
- `production/qa/evidence/con-inside-1p.png`, `con-inside-3p.png`, `con-inside-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE; the qa-lead noted AC1 and AC3 are automated and AC2 is the manual evidence).*

- **AC1**: InsideViewAndStillness (automated, PIE-2)
  - Given: a Concealed owner.
  - When: Move is held 5 s and Look is swept through its full range.
  - Then: server displacement 0 cm, body yaw change 0°, no footstep or rustle fires; Prepare and Interact reply NotNow and the camera toggle works.
  - Edge cases: exact equality; Look is free only within the head clamp (value from Camera story 014).
- **AC3**: Foliage and Run (automated)
  - Given: a Concealed pawn.
  - When: Camera and ViewBlock traces cross the foliage, and Run is held.
  - Then: the traces pass through the foliage and Run has no effect; after exit End, Run works again.
  - Edge cases: Run held during Entering.
- **AC2**: Manual check: inside view
  - Setup: concealed owner, 1P then 3P, Look swept across its full range, captured at 60 Hz.
  - Verify: the boom is at most 70 cm and equals 60 cm at defaults, height offset 0, grade and vignette compared against the art bible 2.8 reference.
  - Pass condition: numeric asserts pass at every rendered frame of the sweep; `con-inside-1p.png` and `con-inside-3p.png` are retained with lead sign-off in `con-inside-evidence.md`.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained screenshots `con-inside-{1p,3p}.png` in `production/qa/evidence/` plus sign-off in `con-inside-evidence.md`; the automated tests above also exist

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-004-hide-enter-execution-and-commit.md`, `story-011-server-warp-to-anchor-and-exit-motion.md`, `story-023-owner-and-team-concealed-flags.md`; Camera `production/epics/camera-system/story-014-concealed-and-informant-view.md`; IIF `production/epics/input-interaction-framework/story-052-no-brace-or-tend-in-bush.md`
- Unlocks: `story-035-prototype-retirement-and-regression.md`
