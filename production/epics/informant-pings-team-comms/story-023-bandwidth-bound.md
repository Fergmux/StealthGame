# Story 023: Bandwidth bound

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (F4; AC-PNG-11, ADVISORY gate)
**Requirement**: `TR-png-021`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (bandwidth budget: ping part at most 50 B/s per teammate connection, ADR-0003 Technical Setup), with ADR-0001 (fixed slots, per-element delta)
**ADR Decision Summary**: The 10-connection bandwidth budget is measured with captures, not estimated; the ping part adds a bounded cost per receiving teammate connection and 0 B to opponents because opponent controllers hold no ping part.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 networking)
**Engine Notes**: Measure through the harness's per-connection outgoing-byte counters (the S6 10-connection capture in the IVP epic is the model). A 10-player run is a heavy test: it is advisory and runs on the dedicated runner or nightly, not per commit.

**Control Manifest Rules (this layer)**:
- Required: ping part at most 50 B/s per teammate connection (budget line); the AC limit is 80 B/s to leave header headroom — source: ADR-0003, GDD F4
- Forbidden: Never use a dynamic array without a fast-array serializer for the slots — source: GDD F4

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-11, F4; measurement method pinned at QL-STORY-READY), scoped to this story. Gate: ADVISORY per the GDD.*

- [ ] **AC1** (AC-PNG-11 a, `[Integration/ADVISORY]` `StealthGame.Pings.Bandwidth`): **GIVEN** a 5v5 test round in which all ten players ping at the maximum rate for 60 s (each presses whenever its client mirror allows: 2 + 0.5 x 60 = 32 pings per sender) **WHEN** per-connection outgoing bytes are measured as the run with pings minus a control run with the same seed and no pings, averaged over 60 s **THEN** the ping part adds <= 80 B/s per receiving teammate connection.
- [ ] **AC2** (AC-PNG-11 b): **GIVEN** the same run **THEN** the ping part adds 0 B to every opponent connection (the opponent listen host's connection included).
- [ ] **AC3** (AC-PNG-11 c): **GIVEN** a single changed slot **THEN** it costs <= 32 B on the wire (per-element delta); this reuses the fixture of story 002 AC3.

---

## Implementation Notes

*Derived from GDD F4:*

`B_conn = r_team * S_entry` with `r_team <= TeamSize * PingBucketPerSec`; at defaults about 50 B/s steady and a one-time burst of at most 200 B, so a 60 s window averages at most about 54 B/s. Assert the 60 s average, not the peak (the worst case is 5 senders x 0.5/s x 32 B = 80 B/s exactly). Do not subtract by guessing: the control run is the baseline. AC3 shares code with story 002 so the figure is not measured two ways.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: the slot layout and the per-slot wire cost fixture.
- ADR-0003's own 10-connection capture (information-visibility-policy epic).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Pings/BandwidthTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(Bandwidth, "StealthGame.Pings.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 ping bandwidth per teammate connection (AC-PNG-11 a)**
- Given: a 5v5 round; ten players ping at the maximum rate for 60 s (per the pinned method); a control run.
- When: profile outgoing bytes per connection.
- Then: the delta is <= 80 B/s per receiving teammate connection averaged over 60 s.
- Edge cases: assert the average, not the peak.

**Test: AC2 zero to opponents (AC-PNG-11 b)**
- Given: the same run.
- When: compare opponent connection bytes with the control.
- Then: the delta is 0 B on every opponent connection.
- Edge cases: the opponent listen host included.

**Test: AC3 single-slot cost (AC-PNG-11 c)**
- Given: one changed slot.
- When: measure the serialised size.
- Then: <= 32 B (same fixture as story 002 AC3).
- Edge cases: share the fixture rather than keep two.

QL-STORY-READY: GAPS (revised) — the measurement method (with-pings minus control) and "maximum rate" were pinned as above on the qa-lead's proposal; DEFERRED (10-player run), ADVISORY gate.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/BandwidthTest.cpp` OR a documented playtest (ADVISORY gate)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-002-team-ping-wire-types-and-slot-component.md`, `story-006-server-team-ping-handler-and-host-parity.md`
- Unlocks: None
