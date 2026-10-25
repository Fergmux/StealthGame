# Story 015: One ring per sender

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R6 ruling D25, F1 output range; AC-PNG-16)
**Requirement**: `TR-png-013`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (§5: one slot per team member, per-element delta), with ADR-0021 (the ring model replaces a sender's ring at once, no cross-fade)
**ADR Decision Summary**: The fixed-size slot array gives a sender at most one live entry, so a new accepted ping overwrites that sender's slot; on every client the ring model replaces that sender's previous ring at once, never cross-fading, and the replacing ping still spends its token with nothing refunded.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0001 and ADR-0021 overall; no post-cutoff API in this story)
**Engine Notes**: The test inspects the server part, each client's ring model list and the bucket state, all through the same dump hooks as story 013. It needs `PingBucketSize` 3 supplied through the tuning preset mechanism (a test preset, never a CVar or ini).

**Control Manifest Rules (this layer)**:
- Required: a new entry from the same sender replaces the ring at once; the sender's own ring comes only from the server echo — source: ADR-0021
- Forbidden: Never cross-fade a replaced ring; never refund a token for a replaced ring — source: ADR-0021, ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-16, PNG-R6), scoped to this story:*

- [ ] **AC1** (AC-PNG-16 a, `[Integration]` `StealthGame.Pings.PIE.OneRingPerSender`): **GIVEN** `PingBucketSize` 3 and a team of 3 whose members each press Ping three times within 1 s from a full bucket at three different points **WHEN** every team client's ring list and the server's part are inspected **THEN** at most 3 rings show on any client (one per sender), each sender's visible ring is its latest point, and the server part holds exactly one entry per sender.
- [ ] **AC2** (AC-PNG-16 b): **GIVEN** the same run **THEN** each replaced ring was removed on the frame its replacement appeared (no cross-fade) and each sender's bucket spent 3 tokens (a replaced ring refunds nothing).

---

## Implementation Notes

*Derived from GDD PNG-R6 ruling D25 and ADR-0021 Technical Setup 2026-10-25b:*

The behaviour is structural (a slot per sender) plus the ring model's replace rule (story 016 AC2); this story is the end-to-end proof, including the burst: with `PingBucketSize` 3 a sender's second and third burst pings replace its first, so the burst exists to correct a misplaced ring, not to place several. The team ceiling is `TeamSize` rings at once (at most 5). Observe the ring list from the model (story 016) through a test hook, and the "removed on the frame its replacement appeared" check from the model's per-frame event list. Do not add a server-side ring cap or a second limiter.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 016: the model's replace rule at unit level.
- Story 004: the bucket arithmetic.
- Story 019: how the replaced ring is painted.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Pings/OneRingPerSenderTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(OneRingPerSender, "StealthGame.Pings.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 one ring per sender (AC-PNG-16 a)**
- Given: `PingBucketSize` 3; a team of 3; full buckets.
- When: each presses three times within 1 s at three distinct points.
- Then: at most 3 rings on any team client (one per sender), each showing its latest point; the server part holds exactly one entry per sender.
- Edge cases: 3 is the safe-range maximum; refill during the 1 s does not add a fourth accepted press.

**Test: AC2 immediate replacement, no refund (AC-PNG-16 b)**
- Given: the same run.
- When: inspect per-frame ring lists and buckets.
- Then: each replaced ring is removed on the frame its replacement appears (no cross-fade); each sender's bucket spent 3 tokens.
- Edge cases: a replacement in the same frame as a prior accept.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/OneRingPerSenderTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-006-server-team-ping-handler-and-host-parity.md`, `story-016-ring-model-and-age.md`
- Unlocks: None
