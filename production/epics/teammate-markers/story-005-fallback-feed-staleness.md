# Story 005: Fallback feed staleness

> **Epic**: Teammate Markers
> **Status**: Ready
> **Layer**: Presentation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

A fed marker shows where the server last put the ally. Replicated properties are sent only on change, so a stationary ally's unchanged quantised position produces no update; staleness therefore uses the per-entry sequence `Seq` the server advances every feed tick (adopted by user decision 2026-10-25, IVP R8, ADR-0003), measured on the client's receive time, never server time. When the sequence stops advancing for `TeammateMarkerFeedStaleSec` the marker goes to `Stale` and is not drawn, while the entry and `IsMarkedAlly` are unaffected, and the next feed update returns it to `Fed`. The check is disabled at feed rates below 0.6 Hz (where `3 / h_feed` would exceed the 5 s hard maximum), and a feed rate of 0 means fed markers never appear.

**GDD**: `design/gdd/teammate-markers.md` (F5, TM-R4)
**Requirement**: `TR-tmk-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (section 5)
**Also governed by**: ADR-0003: Replication, relevancy and the concealment cull hybrid (feed with `uint8 Seq`)
**ADR Decision Summary**: The model reads the Policy's fallback feed (`UTeamPositionFeedComponent`) only; the feed entry carries `{PublicHandle, Pos, Seq}` to teammate connections only, with `Seq` advanced on every feed tick.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The staleness clock is the client's local receive time, injected as a clock seam (the stealth clock interface, tuning-data-config story 007); the test advances a fake clock. `uint8` `Seq` wraps; compare by "advanced" (not equal), not by magnitude.

**Control Manifest Rules (this layer)**:
- Required: Every feed entry carries a `uint8 Seq` advanced on every feed tick; the feed is sent to teammate connections only — source: ADR-0003
- Forbidden: Never extrapolate or interpolate a fed position — source: ADR-0021 (TM-R4)
- Guardrail: Feed bandwidth is IVP's (at most 104 B/s per client) and is not restated here

---

## Acceptance Criteria

*From GDD `design/gdd/teammate-markers.md` (F5; the stale half of AC-TMK-15), scoped to this story:*

- [ ] **AC1** (AC-TMK-15, stale half): **GIVEN** a `Fed` marker at defaults **WHEN** the per-entry feed sequence stops advancing **THEN** the marker hides 2.0 s (+/-1 frame) after the last advance measured on client receive time, `IsMarkedAlly` is unaffected, and the next feed update returns it to `Fed`.
- [ ] **AC2** (F5): **GIVEN** a stationary fed ally whose quantised `Pos` is unchanged **THEN** it never hides while `Seq` keeps advancing, and a feed whose `Seq` does not advance is stale even if `Pos` changed.
- [ ] **AC3** (F5, revised): **GIVEN** `TeamPositionFeedHz` **THEN** for `0 < h_feed < 0.6` the staleness check is disabled; for `h_feed = 0` fed markers never appear and a null `PawnRef` is always retirement case 3 (story 004); and a `TeammateMarkerFeedStaleSec` below `3 / h_feed` (for `h_feed >= 0.6`) is a TDC CR-5 invariant violation (owned and tested by TDC), while the module applies the validated value unchanged.

*(Revised after QL-STORY-READY: AC3 no longer says "enforced" without an owner; the invariant is TDC's, this story asserts the module neither raises nor clamps it.)*

---

## Implementation Notes

*Derived from F5:*

`hidden_i = (t_now - t_seq,i) > T_stale`, with `t_seq,i` the client receive time of the last update in which entry i's `Seq` advanced. Default `T_stale` is 2.0 s: at the 2 Hz default feed rate that is four missed advances, and `3 / 2 = 1.5 <= 2.0` holds. Keep a per-handle `LastSeq` and `LastSeqAdvanceTime`; treat any change of `Seq` (including a wrap from 255 to 0) as an advance. When the feed rate read from the client tuning is 0 the model discards all feed entries so the null-ref rule falls through to case 3.

The `Stale` state draws nothing but keeps the entry and `IsMarkedAlly`; leaving `Stale` is by a feed update (to `Fed`), a resolving ref (to `Tracking`) or the feed entry being removed while the ref is null (to `Held`, undrawn, so `OnAllyMarkerRetired` still fires at `t_end`, story 008).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-anchor-fed-position-and-null-ref.md`: the `Fed` classification and stepped positions.
- `story-029-feed-handoff-and-null-ref-on-real-connection.md`: staleness over a real connection and the handoff to tracking.
- TDC: invariant CR-5 itself (a `tuning-data-config` story).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/Markers/MarkerFeed.h` and `MarkerFeed.cpp` (staleness tracking per handle)
- `Source/StealthPresentation/Private/Tests/Markers/MarkerFeedStaleTest.cpp` (name `StealthGame.Markers.FeedStaleness`, class `FMarkersFeedStalenessTest`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1**: Given: a `Fed` marker at defaults and a fake client-receive clock. When: the sequence stops advancing. Then: it hides 2.0 s (+/-1 frame) after the last advance, `IsMarkedAlly` is unchanged, and the next feed update returns it to `Fed`. Edge cases: a stop at 1.99 s and 2.01 s.
- **AC2**: Given: a stationary ally with unchanged quantised `Pos`, then a feed whose `Seq` is frozen but `Pos` changes. When: time runs. Then: the first never hides while `Seq` advances, and the second goes stale. Edge cases: `Seq` wrap.
- **AC3**: Given: `TeamPositionFeedHz` values 0, 0.5, 0.6, 2.0. When: the model steps with a silent feed. Then: at 0.5 the check is disabled (the marker never goes stale); at 0 no fed marker appears and a null ref is retirement case 3; at 0.6 and 2.0 staleness uses the validated `TeammateMarkerFeedStaleSec` unchanged. Edge cases: a configured value below `3 / h_feed` is left as given (the violation is reported by TDC, not altered here).

QL-STORY-READY: GAPS, revised in place per the agent's proposal (AC3 owner of the invariant stated); not re-run through the gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Markers/MarkerFeedStaleTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/teammate-markers/story-001-marker-tuning-keys-and-local-settings.md`, `story-004-anchor-fed-position-and-null-ref.md`; cross-epic `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`
- Unlocks: `production/epics/teammate-markers/story-029-feed-handoff-and-null-ref-on-real-connection.md`
