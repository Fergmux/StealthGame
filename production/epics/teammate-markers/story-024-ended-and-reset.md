# Story 024: Ended and Reset

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

When a round ends, the Roster's `EndRoundAll` sets every `Active` or `Informant` record to `RoundEnded`, and Round publishes the `Ended` phase in the same server step. Markers stay while the viewer has a live view (allies-only knowledge): each keeps the fill it last had this round, frozen, until `Reset`, which clears every marker on the same frame with no hold, no fade and no `OnAllyMarkerRetired`. Because the Roster records and the GameState are different actors and may arrive a net frame apart, a `RoundEnded` entry that arrives before the `Ended` phase must also change nothing. This story proves the behaviour on a real two-ally scenario; the unit logic is stories 003 and 008.

**GDD**: `design/gdd/teammate-markers.md` (TM-R5, TM-R6 steps 5 and 6; AC-TMK-16b)
**Requirement**: `TR-tmk-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (sections 4 and 5)
**ADR Decision Summary**: `RoundEnded` is keyed on the round phase from `FRoundPublicState`; the model is cleared with no hold or fade on `Phase = Reset`; presentation never depends on a RepNotify having fired.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The two-arrival-orders case needs a test hook that holds back one of the two replicated updates by one frame; use the net-emulation profiles and the snapshot holdback pattern from tuning-data-config (stories 017 and 072) rather than a new mechanism.

**Control Manifest Rules (this layer)**:
- Required: Presentation builds view-models by sampling its carriers every frame by value — source: ADR-0021
- Forbidden: Never depend on a RepNotify having fired — source: ADR-0021
- Guardrail: Markers are not all hidden at `Ended`; only a retiring marker is cut short (story 008)

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-16b, TM-R5), scoped to this story:*

- [ ] **AC1** (AC-TMK-16b, [PIE-2] `EndedAndReset`): **GIVEN** two allies, one `Active` (filled) and one `Informant` (outline) **WHEN** the round ends (`EndRoundAll` sets both to `RoundEnded`) and later resets **THEN** during `Ended` both markers remain with their last fills (filled and outline) while the viewer's view is `Live`, and at `Reset` every marker goes to `Absent` on the same frame with no hold, no fade and no `OnAllyMarkerRetired`.
- [ ] **AC2** (TM-R5, Round R9): **GIVEN** the Roster `RoundEnded` update arriving a net frame before the GameState `Ended` phase (and, in a second run, the reverse order) **THEN** the markers keep their last fill with no change in either order.

---

## Implementation Notes

*Derived from TM-R5 and AC-TMK-16b:*

Run a 2-client PIE (viewer plus two allies) with one ally `Active` and one `Informant`, end the round by the objective path, observe, then reset. Dump the marker model each frame (fill, drawn, event count). For AC2 hold back one of the two updates by one net frame in each order; the expected result is identical in both. Include one hatched ally at `Ended` as a case in AC1's edge list (it keeps the hatch). A retiring marker at `Ended` is story 008's cut-off, not this story's.

Do not hide markers at `Ended`; the Vertical Slice's Post-Round Reveal later replaces them with its own payload-fed glyphs.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-003-fill-precedence-and-roundended-freeze.md` and `story-008-ally-marker-retired-event-ended-reset.md`: the unit logic.
- `round-match-framework/story-056-round-ended-arrival-order.md`: the Round-side arrival-order guarantee.
- Vertical Slice: Post-Round Reveal.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Markers/EndedAndResetTest.cpp` (CQTest, `StealthGame.Markers.PIE.EndedAndReset.*`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: two allies, one `Active` (filled) and one `Informant` (outline), viewer `Live`. When: `EndRoundAll` sets both to `RoundEnded`, then `Reset`. Then: during `Ended` both keep their last fills; at `Reset` every marker is `Absent` on the same frame with no hold, fade or `OnAllyMarkerRetired`. Edge cases: a hatched ally at `Ended`.
- **AC2**: Given: the Roster `RoundEnded` update delivered one net frame before the GameState `Ended` phase. When: both arrive. Then: markers keep their last fill with no change in between. Edge cases: the reverse order.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `EndedAndResetTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-003-fill-precedence-and-roundended-freeze.md`, `story-008-ally-marker-retired-event-ended-reset.md`, `story-016-presenter-local-controller-only.md`; cross-epic `production/epics/round-match-framework/story-058-reset-clears.md`, `story-056-round-ended-arrival-order.md`
- Unlocks: None
