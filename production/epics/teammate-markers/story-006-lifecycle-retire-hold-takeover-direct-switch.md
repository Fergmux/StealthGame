# Story 006: Marker lifecycle, retire hold, takeover and direct pawn switch

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

When an ally's binding retires (a kill, a reversal, an exile, a disconnect expiry), the marker does not vanish at once: it holds, frozen in its last fill at the cached anchor, for `TeammateMarkerRetireHoldSec` (0.25 s), then exits with the fade. The hold absorbs a transient unmapped reference (one or two net updates) and lets the strike's wind-up play under the marker. If the same pawn resolves again during the hold the hold is cancelled with no fade; if a different pawn resolves (an informant takeover inside the hold) the held marker completes its exit and a new marker enters on the new pawn, never sliding between them. A `PawnRef` that switches directly from one resolved pawn to another moves the marker at once. This story implements the per-handle state machine of the GDD States and Transitions table (`Absent, Entering, Tracking, Fed, Stale, Held, Exiting`) with the plain hold; the contact-time extension is story 007.

**GDD**: `design/gdd/teammate-markers.md` (TM-R6 steps 1, 3 and 4, TM-R7, States and Transitions, F6 plain hold)
**Requirement**: `TR-tmk-???` (TM-R6 hold and exit and TM-R7 takeover have no registry entry; nearest `TR-tmk-007`; the registry should gain entries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5)
**Also governed by**: ADR-0001 (marker independent of `AppearanceId`; Character Pool CP-16)
**ADR Decision Summary**: `FTeammateMarkerModel` owns validation, anchor source (`Tracking`, `Fed`, `Held` cached anchor), retire hold and fade (TM-R6, F6-F7) and takeover (TM-R7), updated every frame by the presenter.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure C++ with a fake clock; timing assertions are +/-1 frame at a fixed fake frame step. `FDeathState` arrival must change nothing (the 2026-10-24 death-state extension stays removed: it fired about half the time).

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()` for server-time values and a local clock for the hold; widgets never run timelines from widget animation assets — source: ADR-0021, ADR-0002
- Forbidden: Never create or destroy a widget during a round in response to gameplay (the marker object is a brush in the leaf, not a widget) — source: ADR-0021
- Guardrail: The retire hold is a timing, not a motion, and stays under Reduce motion

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-12 a-e, AC-TMK-32, TM-R6, TM-R7), scoped to this story:*

- [ ] **AC1** (AC-TMK-12 a-c, [U] `RetireHold`): **GIVEN** a tracked marker at defaults **WHEN** (a) the `PawnRef` reads null with no feed entry and nothing returns, (b) as (a) while the cached pawn receives an `FDeathState`, (c) the ref reads null and a feed entry is present in the same update **THEN** (a) and (b) the exit fade starts at +0.25 s (+/-1 frame) and the death state changes nothing, (c) the marker is `Fed` with no hold and no fade.
- [ ] **AC2** (AC-TMK-12 d-e): **GIVEN** the same marker **WHEN** (d) the same pawn resolves again 0.13 s later, (e) a different pawn resolves 0.10 s later **THEN** (d) the hold is cancelled, the marker never fades and `OnAllyMarkerRetired` does not fire, (e) the old marker exits at +0.25 s while a new marker fades in on the new pawn and no marker is ever drawn between the two pawns.
- [ ] **AC3** (AC-TMK-32, [U] `DirectPawnSwitch`): **GIVEN** a tracked marker whose `PawnRef` switches directly from P1 to resolved P2 in one update **WHEN** rendered **THEN** on that frame the single marker is at P2's anchor with no hold, no fade and no second marker, its fill is that of the entry's current `State`, and `IsMarkedAlly(P1)` is false and `IsMarkedAlly(P2)` true on the same frame.
- [ ] **AC4** (TM-R7 and States table, revised): **GIVEN** a takeover or an `AppearanceId` change **THEN** the marker follows `PawnRef`, never `AppearanceId`, and enters with the `TeammateMarkerFadeSec` entrance in the fill of the entry's current `State`; and a parametrised test covers every row of the GDD States and Transitions table (`design/gdd/teammate-markers.md` lines 112-126: Absent, Entering, Tracking, Fed, Stale, Held, Exiting) with the table's "Exits to" column as the expected result.

*(Revised after QL-STORY-READY: AC4 now names its oracle, the GDD table.)*

---

## Implementation Notes

*Derived from TM-R6, TM-R7 and the States and Transitions table:*

States: `Absent` (no valid entry, `Reset` or travel), `Entering` (fade in over `TeammateMarkerFadeSec`; if retired mid-fade it goes to `Held` and the entrance finishes during the hold, no shortcut to `Exiting`), `Tracking`, `Fed`, `Stale`, `Held`, `Exiting`. On retirement `t_r` is the time the entry's `PawnRef` was first read null with no feed entry (or the entry left). The marker holds at the cached pawn's anchor while that pawn still exists on this client, otherwise at the last anchor position. `t_end = t_r + T_hold` here (story 007 extends it); `OnAllyMarkerRetired` is story 008. Cancel the hold when the same pawn resolves; if a different pawn resolves, the held marker finishes its exit and a new marker enters on the new pawn with the entrance fade. A direct resolved-to-resolved switch updates the marker's pawn and `IsMarkedAlly` in the same step.

The marker is UI, not appearance (Character Pool CP-16): `AppearanceId` is never an input of the model.

Prototype replaced: the prototype shows or hides markers per frame from a replicated team field and has no hold, no fade and no takeover handling (`StealthHUD.cpp`); nothing promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-retire-contact-alignment.md`: extending the hold from `RetireAtServerTime`.
- `story-008-ally-marker-retired-event-ended-reset.md`: the retire event, the `Ended` cut-short and the `Reset` and travel clear.
- `story-009-fade-draw-order-no-motion.md`: the fade curve.
- `story-030-informant-return-timing.md`: the real-connection informant return.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerLifecycle.h` and `MarkerLifecycle.cpp` (per-handle state machine)
- `Source/StealthPresentation/Private/Markers/TeammateMarkerModel.cpp` (drives it)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerRetireHoldTest.cpp` (names `StealthGame.Markers.RetireHold`, `StealthGame.Markers.DirectPawnSwitch`, `StealthGame.Markers.StateTable`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a tracked marker at defaults. When: (a) null ref, no feed; (b) as (a) plus an `FDeathState` on the cached pawn; (c) null ref plus a feed entry in the same update. Then: (a) and (b) start the exit fade at +0.25 s (+/-1 frame) and the death state changes nothing; (c) `Fed` with no hold and no fade. Edge cases: boundary frames at 0.25 s +/-1.
- **AC2**: Given: the same marker. When: (d) the same pawn resolves at +0.13 s; (e) a different pawn resolves at +0.10 s. Then: (d) the hold is cancelled, alpha never drops, and `OnAllyMarkerRetired` fires 0 times; (e) the old marker exits at +0.25 s, a new marker fades in on the new pawn, and no marker is drawn between the pawns. Edge cases: resolve at +0.249 s.
- **AC3**: Given: a ref switching from P1 to resolved P2 in one update. When: that frame renders. Then: one marker at P2's anchor with no hold, fade or second marker, the fill matches the entry's current `State`, `IsMarkedAlly(P1)` is false and `IsMarkedAlly(P2)` true on that frame. Edge cases: P1 and P2 in different States.
- **AC4**: Given: a table of every row of the GDD States and Transitions table with its listed exits as expected results. When: each trigger is applied. Then: the resulting state equals the table's "Exits to" entry; an `AppearanceId` change alters nothing; a takeover enters with the fade in the fill of the entry's current `State`. Edge cases: `Entering` retired mid-fade goes to `Held`.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (AC4 oracle named); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerRetireHoldTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-002-marker-model-teamview-source-and-validation.md`, `story-003-fill-precedence-and-roundended-freeze.md`, `story-004-anchor-fed-position-and-null-ref.md`
- Unlocks: `production/epics/teammate-markers/story-007-retire-contact-alignment.md`, `story-008-ally-marker-retired-event-ended-reset.md`, `story-009-fade-draw-order-no-motion.md`, `story-014-is-marked-ally-query.md`, `story-030-informant-return-timing.md`
