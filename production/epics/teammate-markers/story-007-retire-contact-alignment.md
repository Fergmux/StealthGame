# Story 007: Retire contact alignment from RetireAtServerTime

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

The Roster and the binding retirement are published at the Attack's queue slot, but the visible blow lands `StrikeContactSec` (0.40 s) later, or 0.75 s for a bush strike. To make the marker leave as the blow lands, the retiring C-TeamView update carries the team-only `RetireAtServerTime` (the kill's `ContactServerTime`, or the slot time for a release). The hold is extended so the exit fade is half complete at that time: `t_end = max(t_r + T_hold, min(t_R - T_fade/2, t_r + T_Rmax))`, where `t_R` is the local time at which the client's synchronised server clock reaches the value. The value is latched once, at `t_r`, only from the update in which the `PawnRef` became null (or the entry left); a stale value can never extend a later hold. Missing, zero or too-early values give the plain hold. `T_Rmax` (1.5 s) guards an absurd value and covers the bush strike's 1.2 s hard-maximum contact plus margin.

**GDD**: `design/gdd/teammate-markers.md` (TM-R6 step 2, F6)
**Requirement**: `TR-tmk-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 4 time rule)
**Also governed by**: ADR-0002 (`EstimatedServerNow()`, section 9); ADR-0005 (the Roster writes `RetireAtServerTime` into the C-TeamView model)
**ADR Decision Summary**: Every timed presentation element uses `ServerNowEst = EstimatedServerNow(World)` and its one slewed offset; presentation never times from packet arrival.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The mapping of a server time to a local time goes through the stealth clock interface so a test can make packet arrival and the estimated server time deliberately differ. `T_Rmax` is a constant (guard), not a tuning key.

**Control Manifest Rules (this layer)**:
- Required: Presentation timing uses only `EstimatedServerNow()`; timelines are sampled from `now - t0` in C++ — source: ADR-0021, ADR-0002
- Forbidden: Never time a presentation event from packet arrival or from a RepNotify firing — source: ADR-0021
- Guardrail: Team-only `RetireAtServerTime` is read once per retirement and never leaves the client

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (AC-TMK-12 f-j, TM-R6 step 2, F6), scoped to this story:*

- [ ] **AC1** (AC-TMK-12 f,g): **GIVEN** a retirement **WHEN** `RetireAtServerTime` = retirement + 0.75 s (a bush strike) **THEN** the exit fade starts at +0.60 s (+/-1 frame); **WHEN** the value is 0 **THEN** it starts at +0.25 s.
- [ ] **AC2** (AC-TMK-12 h,i): **GIVEN** a retirement **WHEN** the value is retirement + 3.0 s **THEN** the exit starts at +1.5 s (`min(t_R - T_fade/2, t_r + T_Rmax)` caps the contact term); **WHEN** the value is retirement - 0.5 s **THEN** it starts at +0.25 s.
- [ ] **AC3** (AC-TMK-12 j): **GIVEN** a handle retired with +0.75 s, re-adopted, and retired again with value 0 while a stale +0.75 s is still present in a later non-retiring update **THEN** the second retirement exits at +0.25 s (the value is latched once at `t_r` from the retiring update only).
- [ ] **AC4** (TM-R6 step 2, revised): **GIVEN** an open kill with `RetireAtServerTime` = `t_r` + 0.40 s and a packet arrival time deliberately different from the `EstimatedServerNow()` mapping **THEN** the exit starts at 0.25 s (half faded as the blow lands) because `t_R` comes from the server-clock mapping, never from arrival.

*(Revised after QL-STORY-READY: AC4 now states t_R and a fixture where arrival and the clock mapping differ.)*

---

## Implementation Notes

*Derived from TM-R6 step 2 and F6:*

`t_end = max(t_r + T_hold, min(t_R - T_fade/2, t_r + T_Rmax))`; `t_R` is omitted (plain `t_r + T_hold`) when `RetireAtServerTime` is 0. A latched value earlier than `t_r + T_hold + T_fade/2` gives `t_r + T_hold`. Defaults: `T_hold` 0.25, `T_fade` 0.30, `T_Rmax` 1.5. Worked examples from the GDD: release at defaults (exile, `ReleaseToCrowd`, `t_R` about `t_r`) exits at 0.25 s and is gone at 0.55 s; open kill `t_R` about `t_r` + 0.40 gives `t_R - 0.15 = t_r + 0.25`, exit at 0.25 s, half faded as the blow lands, gone at 0.55 s; bush strike `t_R` about `t_r` + 0.75 exits at 0.60 s, half faded at contact, gone at 0.90 s.

Latch rule: read `RetireAtServerTime` exactly once, from the entry update in which the `PawnRef` became null (or the entry left); ignore any nonzero value seen in another update and any value left over from an earlier retirement of the same handle. Map to local time with the clock interface (`EstimatedServerNow()`), not with `LocalNow` at arrival. Roster story 021 is the writer of the value (`RetireBinding` passes the kill's `ContactServerTime` or the slot time).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-006-lifecycle-retire-hold-takeover-direct-switch.md`: the plain hold and cancel rules.
- `story-008-ally-marker-retired-event-ended-reset.md`: firing `OnAllyMarkerRetired` at `t_end`.
- `story-030-informant-return-timing.md`: the real-network measurement of "leaves as the blow lands".
- `participant-state-roster/story-021-retire-at-server-time-source.md`: the server-side source of the value.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerRetire.h` and `MarkerRetire.cpp` (`ComputeRetireEnd`, the latch)
- `Source/StealthPresentation/Private/Markers/MarkerLifecycle.cpp` (uses it)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerRetireContactTest.cpp` (name `StealthGame.Markers.RetireContactAlignment`, class `FMarkersRetireContactAlignmentTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a retirement at `t_r`. When: `RetireAtServerTime` = `t_r` + 0.75 s, then 0. Then: the exit starts at +0.60 s, then +0.25 s (+/-1 frame). Edge cases: one frame either side.
- **AC2**: Given: the same. When: the value is `t_r` + 3.0 s, then `t_r` - 0.5 s. Then: the exit starts at +1.5 s (capped by `T_Rmax`), then +0.25 s. Edge cases: the value is exactly `t_r` + 0.40 s.
- **AC3**: Given: a handle retired with +0.75 s, re-adopted, then retired again with 0, with a stale +0.75 s still present in a later non-retiring update. When: the second retirement runs. Then: the exit starts at +0.25 s (latched once at `t_r` from the retiring update only). Edge cases: the stale value arrives before the retiring update.
- **AC4**: Given: `RetireAtServerTime` = `t_r` + 0.40 s and a packet arrival time deliberately different from the `EstimatedServerNow()` mapping. When: the retirement runs. Then: the exit starts at 0.25 s. Edge cases: arrival earlier and later than the mapped time.

QL-STORY-READY: GAPS, revised in place per the agent's proposal (AC4 fixture stated); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerRetireContactTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-001-marker-tuning-keys-and-local-settings.md`, `story-006-lifecycle-retire-hold-takeover-direct-switch.md`; cross-epic `production/epics/participant-state-roster/story-021-retire-at-server-time-source.md`, `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`
- Unlocks: `production/epics/teammate-markers/story-008-ally-marker-retired-event-ended-reset.md`, `story-030-informant-return-timing.md`
