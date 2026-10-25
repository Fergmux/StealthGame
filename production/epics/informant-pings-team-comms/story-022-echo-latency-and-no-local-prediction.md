# Story 022: Echo latency and no local prediction

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (Game Feel responsiveness, PNG-R7 echo rule; AC-PNG-14)
**Requirement**: `TR-png-024` (also the echo-only half of `TR-png-014`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0008: Test and automation strategy for leak and parity tests (Technical Setup 2026-10-25b: `EchoLatency` nightly row), with ADR-0021 (the own ring comes only from the server echo) and ADR-0002 §9 (clock)
**ADR Decision Summary**: `StealthGame.Pings.PIE.EchoLatency` is a nightly-only T2 row asserting a p95 over 20 samples: own ring at most measured RTT + 50 ms after the press, teammate at most one-way delay + one server frame after acceptance, samples written as JSON under `production/qa/`. The sender's own ring comes only from the server echo (`IA_Ping` creates no local ring or model state).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0008; real-time emulated network)
**Engine Notes**: Uses the ADR-0008 net-emulation profiles (PktLag and PktLoss through the emulation settings, not wall-clock sleeps). Latency is a real-time measurement, so the row runs nightly, not per commit; it must not be run on a loaded machine and counted.

**Control Manifest Rules (this layer)**:
- Required: `StealthGame.Pings.PIE.EchoLatency` is nightly-only, asserts the p95 over 20 samples and writes the samples as JSON under `production/qa/` — source: ADR-0008
- Required: the sender's own ring comes only from the server echo — source: ADR-0021
- Forbidden: Never predict the own ping ring locally — source: ADR-0021

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-14, Game Feel; percentile and fields revised at QL-STORY-READY), scoped to this story:*

- [ ] **AC1** (AC-PNG-14, `[Integration]` `StealthGame.Pings.PIE.EchoLatency`, nightly): **GIVEN** a 2-client PIE with 60 ms emulated RTT on the sender and 1 % loss off **WHEN** the sender pings 20 times at 2.5 s intervals **THEN** the p95 (the 19th smallest of the 20 samples, nearest rank) of the time from the `IA_Ping` press to the first presenter frame showing the own ring is <= 110 ms, and a teammate on 60 ms RTT shows each ring <= 80 ms after the server's acceptance.
- [ ] **AC2** (ADR-0021, TR-png-014 echo only): **GIVEN** `IA_Ping` **WHEN** pressed **THEN** no local ring or model state is created, and a ping the server drops (an empty bucket forced by a modified client, a forged out-of-range point) never flashes a ring on the sender's screen.
- [ ] **AC3** (ADR-0008 nightly row): **GIVEN** the nightly T2 row **WHEN** run **THEN** it asserts the p95 over exactly 20 samples and writes them as JSON under `production/qa/` with the fields `{sample, press_to_own_ring_ms, accept_to_teammate_ring_ms}`; fewer than 20 samples fails the row.

---

## Implementation Notes

*Derived from ADR-0008 Technical Setup 2026-10-25b:*

Timestamps: press time from the controller's input callback; own-ring time from the ring model's creation frame (story 016) as seen by the presenter step; acceptance time from the server's `ServerTime`; teammate-ring time on the teammate's presenter. Use the 60 Hz fixed step where the harness provides it and report in milliseconds from frame counts and `EstimatedServerNow()`. The test lives with the other nightly rows and is excluded from the per-commit run by tag. Percentile method is nearest rank on 20 samples (the 19th smallest), never interpolation, so a single outlier cannot flip the verdict by rounding.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 016: the model that creates the ring on the echo.
- Story 006: the handler.
- Story 023: bandwidth.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Pings/EchoLatencyTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(EchoLatency, "StealthGame.Pings.PIE")`, nightly tag)
- `Source/StealthTestHarness/Private/Pings/NoLocalPredictionTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(NoLocalPrediction, "StealthGame.Pings.PIE")`)
- `production/qa/` samples JSON (written by the nightly run)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 echo latency (AC-PNG-14; DEFERRED, nightly)**
- Given: 2-client PIE, sender 60 ms RTT, teammate 60 ms RTT, loss off.
- When: 20 pings at 2.5 s intervals.
- Then: p95 (nearest rank) of press to own ring <= 110 ms; each teammate ring <= 80 ms after acceptance.
- Edge cases: 2.5 s spacing keeps the bucket available; a 1 % loss run is out of scope.

**Test: AC2 no local prediction**
- Given: `IA_Ping`; a modified client with an empty bucket; a forged out-of-range point.
- When: press, send the forged and bucket-empty pings.
- Then: no ring or model state on press; dropped pings never flash a ring on the sender.
- Edge cases: the first frame after the press, before any echo.

**Test: AC3 nightly row output**
- Given: the nightly T2 row.
- When: run.
- Then: asserts the 20-sample p95 and writes the samples as JSON under `production/qa/` with fields `{sample, press_to_own_ring_ms, accept_to_teammate_ring_ms}`.
- Edge cases: fewer than 20 samples fails the row.

QL-STORY-READY: GAPS (revised) — percentile method and JSON fields were pinned as above on the qa-lead's proposal; AC1 and AC3 are DEFERRED to the nightly run.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/EchoLatencyTest.cpp` and `NoLocalPredictionTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-006-server-team-ping-handler-and-host-parity.md`, `story-010-ia-ping-action-and-send-path.md`, `story-016-ring-model-and-age.md`; cross-epic `production/epics/tuning-data-config/story-017-net-emulation-profiles.md`
- Unlocks: None
