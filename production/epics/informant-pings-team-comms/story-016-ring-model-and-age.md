# Story 016: Ring model and age

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R7, PNG-R10 client half, F3, states table; AC-PNG-07)
**Requirement**: `TR-png-014` (also the client half of `TR-png-019`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0021: UI/HUD layering, CommonUI and Enhanced Input (Technical Setup 2026-10-25b: `FTeamPingRingModel`), with ADR-0002 §9 (`EstimatedServerNow()`)
**ADR Decision Summary**: Ring state is the plain C++ `FTeamPingRingModel` (beside `FTeammateMarkerModel`) keeping `LastShownSeq` per `SenderHandle`: a ring is created only for a new `PingSeq` whose age `ServerNowEst - ServerTime` is below `PingLifetimeSec`; older new entries are marked shown and never drawn; display age is `max(0, a)`; age is shown only by stroke weight at thirds (6 / 4 / 3 px) at constant opacity; a new entry from the same sender replaces the ring at once; one 2D UI-bus tick per created ring. Rings hide in the `Step` that sees `Phase = Ended`; the model clears on `Reset` and travel start; a HUD rebuild sets `LastShownSeq := PingSeq` for every present entry.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0021 overall; this story is plain C++, no engine API)
**Engine Notes**: The model is headless and takes inputs as plain arguments (entries from the own controller's `UTeamPingViewComponent` via `IPingClientView`/the component, a clock value, the Round phase) so it needs no `UWorld`. ADR-0021 verification items UI-V1..UI-V4 concern the painter, not this model, so this story is Ready.

**Control Manifest Rules (this layer)**:
- Required: ring state is `FTeamPingRingModel` with `LastShownSeq` per `SenderHandle`; a ring is created only for a new `PingSeq` younger than `PingLifetimeSec`; display age is `max(0, a)`; age shown only by stroke weight at constant opacity — source: ADR-0021
- Required: rings hide in the `Step` that sees `Phase = Ended`; the model clears on `Reset` and travel start; a rebuild marks present entries shown (no ring, no tick, no replay) — source: ADR-0021
- Forbidden: Never predict the own ping ring locally, encode ring age in opacity, or cross-fade a replaced ring — source: ADR-0021

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-07, PNG-R7, PNG-R10, F3), scoped to this story:*

- [ ] **AC1** (AC-PNG-07, `[Logic]` `StealthGame.Pings.RingAge`): **GIVEN** `PingLifetimeSec` 8 and an entry with `ServerTime` 120.00 **WHEN** the model samples at estimated server times 120.06, 122.66, 122.68, 125.34, 127.99 and 128.00 **THEN** it reports stroke 6, 6, 4, 3, 3 and then hidden; an entry received at age 8.2 s is never shown; an estimate 0.3 s behind `ServerTime` gives age 0.
- [ ] **AC2** (PNG-R7): **GIVEN** `FTeamPingRingModel` with `LastShownSeq` per `SenderHandle` **WHEN** entries arrive **THEN** a ring is created only for a new `PingSeq` (including the uint8 wrap 255 to 0) whose age is below `PingLifetimeSec`, a second entry from the same sender replaces the first ring on the frame it arrives, each created ring raises exactly one 2D UI-tick event, and stale, replayed or rebuild-marked entries raise none.
- [ ] **AC3** (PNG-R10, ADR-0021): **GIVEN** a HUD rebuild (reconnect, travel, `RoundEpoch` change) **WHEN** it runs **THEN** `LastShownSeq` is set to `PingSeq` for every present entry (no ring, no tick, no replay); the model hides every ring in the `Step` that sees `Phase` `Ended` and clears on `Reset` and travel start; the model has no opacity field (age is stroke weight only).

---

## Implementation Notes

*Derived from ADR-0021 Technical Setup 2026-10-25b and GDD F3:*

`stroke = 6 px if a < L/3, 4 px if a < 2L/3, else 3 px` with `L = PingLifetimeSec` (UI scale is applied by the painter, story 019/020, exactly once); `visible iff a < L`; `a = max(0, EstimatedServerNow() - ServerTime)`. Compare `PingSeq` by inequality with the stored `LastShownSeq` (a wrapping byte, so 255 -> 0 is new). A ring lives from creation until `ServerTime + PingLifetimeSec`. The sender's own ring comes only from the server echo: the model has no input from `IA_Ping` (story 022 asserts it). The HUD builds the model once at init and feeds it each frame from its carriers, identical on the listen host. The label text and its drop are story 017; edge placement is story 018. Place the model in `Source/StealthPresentation/Private/StealthPingRingModel.h/.cpp` beside the Teammate Markers model (HUD epic's layout governs the exact folder). Ring state is client-local and never replicated.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 017: label resolution and the label drop.
- Story 018: edge placement maths.
- Stories 019 and 020: painting, UI scale, the audible tick itself (this story raises the event only).
- Story 007: the server clears and the end-to-end lifecycle test.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/StealthPingRingModel.h` and `StealthPingRingModel.cpp` (`FTeamPingRingModel`)
- `Source/StealthPresentation/Private/Tests/Pings/PingRingAgeTest.cpp` (class `FPingsRingAgeTest`, name `StealthGame.Pings.RingAge`)
- `Source/StealthPresentation/Private/Tests/Pings/PingRingModelTest.cpp` (class `FPingsRingModelTest`, name `StealthGame.Pings.RingModel`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 ring age and stroke (AC-PNG-07)**
- Given: `PingLifetimeSec` 8; an entry with `ServerTime` 120.00.
- When: sample at 120.06, 122.66, 122.68, 125.34, 127.99, 128.00.
- Then: strokes 6, 6, 4, 3, 3, hidden; an entry received at age 8.2 s is never shown; an estimate 0.3 s behind gives age 0.
- Edge cases: the thirds fall at 2.667 and 5.333 s; assert only the listed samples, not the exact boundaries.

**Test: AC2 sequence handling and events**
- Given: `FTeamPingRingModel` with `LastShownSeq` per `SenderHandle`.
- When: entries arrive: a new seq; the seq wrap 255 to 0; the same seq replayed; a stale age; a rebuild-marked entry; a second entry from the same sender.
- Then: a ring only for a new seq with age below the lifetime; the second entry replaces the first ring on its arrival frame; one 2D UI-tick event per created ring; none for stale, replayed or rebuild-marked entries.
- Edge cases: age exactly 8.0 is not shown.

**Test: AC3 rebuild, hide and clear**
- Given: a HUD rebuild (reconnect, travel, `RoundEpoch` change); `Phase` `Ended`; reset; travel start.
- When: each occurs.
- Then: `LastShownSeq` is set to `PingSeq` for every present entry with no ring, tick or replay; rings are hidden in the `Step` that sees `Ended`; cleared on reset and travel start; the model type has no opacity field (reflection).
- Edge cases: an empty slot during a rebuild.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthPresentation/Private/Tests/Pings/PingRingAgeTest.cpp` and `PingRingModelTest.cpp` — must exist and pass (`/story-done` checks that they EXIST; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-001-ping-tuning-keys-and-invariant.md`, `story-002-team-ping-wire-types-and-slot-component.md`; cross-epic `production/epics/information-visibility-policy/story-055-clock-exchange-owner-only-audit.md` (`EstimatedServerNow()`), hud epic (presenter and Step), teammate-markers epic (model beside which this sits)
- Unlocks: `production/epics/informant-pings-team-comms/story-015-one-ring-per-sender.md`, `story-017-ring-labels-and-label-drop.md`, `story-019-ring-painter-l0-leaf.md`, `story-022-echo-latency-and-no-local-prediction.md`
