# Story 032: Emergence presentation — no visible pop

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-006`
*(At write time: TR-lsc-006 = Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: A pawn is created at O already occluded from every viewer and walks to the ExitPoint on the normal locomotion path: no teleport, VFX, fade or sound; if the occlusion predicate cannot be met the layout fails and an effect is never added to hide it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Scripted fixed-step cameras use the `ViewBlock` trace channel for the sight test; frustum membership uses the engine view-frustum test. The visual half needs a real renderer (not `-nullrhi`).

**Why this story**: A single visible pop would identify an emergence (and therefore an NPC replacement) at a glance. Read first: R5 item 5, AC-11 (VA-6, VA-7), art bible 5.2.1.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every sight test uses the `ViewBlock` channel (ADR-0015); pawns spawn through the factory (ADR-0010)
- Forbidden: Never add VFX, audio or a fade to an emergence; never teleport a pawn (GDD R5.5)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-11 (revised after QL-STORY-READY: camera coverage specified, sign-off pass condition stated), scoped to this story:*

- [ ] GIVEN a 2-client PIE, 50 NPCs, and one scripted fixed-step camera per W(O) cell at 1.6 m eye height facing O (plus 3.2 m climb tops), WHEN 200 emergences and 50 takeovers occur, THEN no frame shows a pawn (in frustum, passing a sight trace) before it leaves O, no VFX/audio/fade component is attached, and no pawn moves more than 1.2 x v_max x dt per tick (`Emergence.NoVisiblePop`)
- [ ] GIVEN a retained capture (debug draw off) of 3 emergences and 1 takeover, THEN no frame shows a pawn appearing, only a normal-speed walk out of the alley mouth with no VFX, fade or sound; the art-director signs it off in `production/qa/evidence/level-socket/`

---

## Implementation Notes

- The automated half is Integration (200 emergences, 50 takeovers) and uses cameras at every W(O) cell so the test cannot pass vacuously; a pawn exactly at the O boundary is an edge case.
- The visual half is a retained capture set with debug draw off, reviewed frame by frame; the sign-off record `production/qa/evidence/level-socket/emergence-no-pop-evidence.md` names the reviewer, date, build and clips.
- Takeover (respawn) invisibility on the wire is story 035 and ADR-0019's own verification; here only the screen is judged.
- Mannequin pool pawns (not the Meshy characters) are acceptable for this evidence at this stage.

---

## Out of Scope

- Story 030: the request/clear logic; story 035: takeover integration; art-bible kit visuals

---

## Files

- `Source/StealthTestHarness/Private/EmergenceNoVisiblePopTest.cpp`
- `production/qa/evidence/level-socket/emergence-no-pop-*.png` (or video frames)
- `production/qa/evidence/level-socket/emergence-no-pop-evidence.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Emergence.NoVisiblePop`
  - Given: a 2-client PIE, 50 NPCs, one fixed-step camera per W(O) cell at 1.6 m facing O plus 3.2 m climb tops.
  - When: 200 emergences and 50 takeovers occur.
  - Then: no frame shows a pawn (in frustum, passing a sight trace) before it leaves O; no VFX, audio or fade component attached; no pawn moves more than 1.2 x v_max x dt per tick.
  - Edge cases: a pawn exactly at the O boundary.
- **AC-2** retained capture (manual check)
  - Setup: capture 3 emergences and 1 takeover, debug draw off.
  - Verify: watch each clip frame by frame.
  - Pass condition: no pawn appears, only a normal-speed walk out of the alley mouth with no VFX, fade or sound; art-director sign-off in `production/qa/evidence/level-socket/`.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: a retained capture in `production/qa/evidence/level-socket/` plus sign-off in `production/qa/evidence/level-socket/emergence-no-pop-evidence.md`; the automated half `StealthGame.Layout.Emergence.NoVisiblePop` must also exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-030-emergence-request-clear-check-uniform-pick.md`, `production/epics/level-socket-contract/story-028-graybox-map-authoring.md`
- Cross-epic: `production/epics/camera-system/EPIC.md` (scripted cameras), `production/epics/crowd-npc-behaviour/EPIC.md`
- Unlocks: `production/epics/level-socket-contract/story-044-static-figure-beacon-and-paired-capture-tests.md`
