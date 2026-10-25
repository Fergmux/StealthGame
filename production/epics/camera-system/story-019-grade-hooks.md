# Story 019: Grade hooks

> **Epic**: Camera System
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/camera-system.md`
**Requirement**: `TR-camera-???` (no registry entry: the grade hooks of CS-R11.1 and CS-R15 are not among the eight TR-IDs; TR-camera-006 covers `GetViewState` and fade only)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0015: Camera system and LOS envelope
**ADR Decision Summary**: `ApplyTransientGrade()` and `SetOwnerGrade(None|Informant)` are post-process blends the camera manager adds with `AddCachedPPBlend`, local to the viewer, never a world post-process volume and never driven by another pawn's state. The transient grade runs at the local contact frame only, on participating players' cameras (Assassination owns when, VA-ASN-4); the informant grade is on from the cut frame at full strength.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0015 overall; `AddCachedPPBlend` is verified at `PlayerCameraManager.h:381`, the post-process settings structure and Substrate/Lumen interplay are unverified in 5.8)
**Engine Notes**: The grade's look (art bible 2.7 transient 1.1 s, 2.9 informant) is the art bible's, delivered as a post-process settings asset or C++ struct; create it as a new asset or in C++, never by editing a sample asset. Reduce screen effects scales the grades as the art bible specifies. Camera shakes, lens effects and `bAlwaysApplyModifiers` stay off (story 004). No ADR-0015 verification item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Grades are per-viewer post-process blends added by the camera manager; the hook callers and timing are Assassination's and Penalty's — source: ADR-0015
- Forbidden: No world post-process volume for grades; no hook driven by another pawn's state; no shake, slow motion, zoom or cut with the grade — source: ADR-0015
- Guardrail: One blend at a time per grade; no per-frame allocation

---

## Acceptance Criteria

*From GDD `design/gdd/camera-system.md` (AC-CS-33, split by hook), scoped to this story:*

- [ ] **AC1** (AC-CS-33 transient grade, [N] `GradeHooks`): **GIVEN** a kill, a reversal and a whiff **THEN** `ApplyTransientGrade` runs on the participating players' cameras on their local contact frames only, never earlier, never on a bystander or ally, and never for the whiff (Assassination AC-ASN-23).
- [ ] **AC2** (AC-CS-33 informant grade): **GIVEN** an informant takeover **THEN** `SetOwnerGrade(Informant)` is active on the cut frame at full strength (no ease-in).

---

## Implementation Notes

*Derived from ADR-0015 Decision §6 Grades and GDD CS-R11.1, CS-R12.2, CS-R15:*

Participants: the attacker on a kill or reversal; a player victim before its CS-R12 fade; a bound defender on a reversal (CS-R11.1). The camera exposes the two hooks on `IStealthCameraView` (client facade, story 016's header); Assassination's local dispatch (`OnRoleChangePresented`) calls `ApplyTransientGrade` at the local contact frame, so the client never calls it from a server fact that precedes the contact frame (ASN-R6). `SetOwnerGrade` is driven only by the owner's own Informant state (owner-audience data) and is on from the cut frame (story 017's cut); it never eases in (Penalty VA-NKP-4). The leaf-edge vignette is Concealment's and the camera only hosts its post-process blend if told to (not built here).

The PIE test records, per camera, the frame number at which the blend was added and compares with the contact frame of the scripted kill/reversal/whiff (Assassination epic harness).

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- assassination-defence-reversal epic: when the transient grade is requested.
- npc-kill-penalty-informants epic: informant state source and the cut.
- art bible 2.7 / 2.9: the look of each grade.
- `story-018-dying-hold-and-view-release.md`: the `Dying` hold that shares the contact frame.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Core/StealthCameraView.h` (`ApplyTransientGrade`, `SetOwnerGrade`, `EOwnerGrade`)
- `Source/StealthGameplay/Private/Core/Camera/StealthCameraGrades.cpp`
- `Source/StealthTestHarness/Private/Camera/GradeHooksTest.cpp` (CQTest, `StealthGame.Camera.PIE.GradeHooks.*`, one method per AC)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: ADEQUATE

Test: AC1 `PIE.TransientGradeTiming` (AC-CS-33)
  Given: a kill, a reversal and a whiff in 2-client PIE, with a spy on `ApplyTransientGrade`.
  When: each scripted event runs.
  Then: each participant's camera calls it exactly once on its own local contact frame and never earlier; bystander and ally cameras get 0 calls; the whiff gets 0.
  Edge cases: contact frames differ per client under latency; two grade triggers on one frame.

Test: AC2 `PIE.InformantGradeOnCutFrame`
  Given: an informant takeover.
  When: the camera cuts to the new pawn.
  Then: on the cut frame the cached post-process blend weight for `SetOwnerGrade(Informant)` is 1.0, with no ramp.
  Edge cases: takeover while a transient grade is active. Grade removal is not covered by the AC; flagged to the author.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `GradeHooksTest.cpp` under `Source/StealthTestHarness/Private/Camera/` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/camera-system/story-017-view-state-neutral-view-fade.md`, `story-018-dying-hold-and-view-release.md`; cross-epic: assassination-defence-reversal (contact frame dispatch), npc-kill-penalty-informants (informant takeover)
- Unlocks: assassination-defence-reversal and npc-kill-penalty-informants presentation stories
