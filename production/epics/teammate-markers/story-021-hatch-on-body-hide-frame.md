# Story 021: Hatch on the body-hide frame

> **Epic**: Teammate Markers
> **Status**: Blocked — BLOCKED: ADR-0017 conditional gates S-CON-1 and R5 have not reported, the concealment-bushes epic (which owns the `IsAllyBodyHidden` latch and the bush swap notifies) has no stories yet, and the painted marker needs UI-V1 (story 018).
> **Layer**: Presentation
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The first anchor moment of the Player Fantasy is the watch over an empty bush: your ally slips into the leaves, their body vanishes on your screen at the same instant it vanishes on the enemy's, but their marker stays and turns hatched. For a tracked ally the hatch is driven by one source, the Concealment client latch `IsAllyBodyHidden(Pawn)`, the same latch that hides the body at the `BushEnter` swap notify and keeps it hidden until the `BushExit` outward leaf crossing, so the hatch appears on the same rendered frame the body disappears and clears on the frame the body reappears. An opponent's screen shows the same body hide and no marker at all. This story is the frame-stepped evidence that the real latch and the painted marker agree.

**GDD**: `design/gdd/teammate-markers.md` (TM-R5, Player Fantasy anchor 1, Game Feel; AC-TMK-11)
**Requirement**: `TR-tmk-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment per-viewer presentation (public read `IsAllyBodyHidden`)
**Also governed by**: ADR-0021 (section 5: fill source per entry kind)
**ADR Decision Summary**: A fail-closed client-local latch `IsAllyBodyHidden(const AStealthPawn*)` (a Foundation-safe `Public/View` facade) returns the latch; Teammate Markers TM-R5 consumes it. Opponents run the same latch but lose the actor when the cull lands.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Frame-stepped capture needs a fixed timestep and per-frame capture of the viewer's rendered frame plus a per-frame dump of `IsAllyBodyHidden` and the marker fill; hidden-primitive handling (R5) is the open ADR-0017 check.

**Control Manifest Rules (this layer)**:
- Required: The hatch for a tracked entry comes from `IsAllyBodyHidden(Pawn)` alone; the server-timed `IsConcealed` is never ORed in — source: ADR-0021, ADR-0017
- Forbidden: Never read a concealment field from a pawn; never cross-fade a fill change — source: ADR-0001, GDD TM-R5
- Guardrail: The hatch and the body-hide change on the same rendered frame (0 frames of skew)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-11), scoped to this story:*

- [ ] **AC1** (AC-TMK-11, enter, [PIE-2][Net][1P/3P][Vis] `HatchOnSwapFrame`): **GIVEN** an ally walking into a bush in view **WHEN** `BushEnter` reaches its swap notify **THEN** on the viewer's client the hatch appears on the same rendered frame the body hides (frame-stepped capture), in 1P and in 3P.
- [ ] **AC2** (AC-TMK-11, exit): **GIVEN** the ally leaving **WHEN** `BushExit` reaches its outward crossing **THEN** the hatch clears on the frame the body reappears, in 1P and in 3P.
- [ ] **AC3** (AC-TMK-11, opponent): **GIVEN** an opponent client in the same scene **THEN** it shows no marker at any captured frame.

---

## Implementation Notes

*Derived from TM-R5 and AC-TMK-11:*

The marker model already switches fill from the latch (story 003); this story verifies the whole chain on real clients. Instrument the test to dump, per rendered frame, the viewer's capture, `IsAllyBodyHidden(Pawn)` and the marker's fill, and assert the two transitions share a frame index. Evidence names: `tmk-hatch-enter-1p-*.png`, `tmk-hatch-enter-3p-*.png`, `tmk-hatch-exit-1p-*.png`, `tmk-hatch-exit-3p-*.png`, with a sign-off in `production/qa/evidence/tmk-hatch-evidence.md`. Cross-check the Concealment AC-CON-01 and the IVP AC-IVP-12 and AC-IVP-15 captures (the IVP story 033 already proves the fed and culled cases end to end).

Edge cases to run once: two allies sharing a bush (each sees the other's hatched marker and no body; markers overlap and draw far-to-near), an ally killed while concealed (hatched marker holds then exits), and an informant viewer (keeps the hatched marker).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-fill-precedence-and-roundended-freeze.md`: the fill logic.
- the concealment-bushes epic: the latch itself, the swap notifies and the cull.
- `information-visibility-policy/story-033-teammate-marker-concealed-ally.md`: the culled and unresolved cases.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/HatchOnSwapFrameTest.cpp` (CQTest, `StealthGame.Markers.PIE.HatchOnSwapFrame.*`)
- `production/qa/evidence/tmk-hatch-*.png` and `production/qa/evidence/tmk-hatch-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Setup: an ally walking into a bush in view, in 1P and 3P, frame-stepped capture. Verify: the frame where `BushEnter` hits its swap notify, comparing hatch and body-hidden state. Pass condition: both change on the same rendered frame; evidence `tmk-hatch-enter-{1p,3p}-*.png`.
- **AC2**: Setup: the ally leaving via `BushExit`, frame-stepped, 1P and 3P. Verify: the outward-crossing frame. Pass condition: the hatch clears on the frame the body reappears; evidence `tmk-hatch-exit-{1p,3p}-*.png`.
- **AC3**: Setup: an opponent client in the same scene, same capture. Verify: every captured frame. Pass condition: no marker on any frame.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures `production/qa/evidence/tmk-hatch-{enter,exit}-{1p,3p}-*.png` and a sign-off in `production/qa/evidence/tmk-hatch-evidence.md` with lead sign-off

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-003-fill-precedence-and-roundended-freeze.md`, `story-019-paint-markers-in-world-anchor-leaf.md`; cross-epic: the concealment-bushes epic's latch and swap-notify stories (not yet created), `information-visibility-policy/story-027-concealment-local-hide-at-swap.md`
- Unlocks: Concealment AC-CON-01 cross-check (concealment-bushes epic)
