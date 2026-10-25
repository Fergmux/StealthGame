# Story 004: Ping token bucket core

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (F1, PNG-R5 step 4, PNG-R10 bucket lifetime; AC-PNG-05, AC-PNG-20)
**Requirement**: `TR-png-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline (Technical Setup 2026-10-25: the ping bucket and its tolerance)
**ADR Decision Summary**: The `ServerTeamPing` handler's fourth check is the sender's own `ParticipantId`-keyed token bucket (`PingBucketSize`, `PingBucketPerSec`, tolerance `epsilon = PingBucketPerSec * 0.1 s`); only an accepted ping spends a token; the bucket lives in the Pings service, starts full at `Active` entry, survives reconnect and resets only at `OnRosterReset`; the client keeps a strict mirror of the same maths.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 overall; this story is plain C++ arithmetic with no engine API)
**Engine Notes**: None. The bucket is a headless `F...` class with an injected clock so tests are deterministic (no wall clock, no random seed). Use `double` for time and compare against `1 - epsilon` as in F1; never assert the exact refill boundary (0.95 is not exactly representable).

**Control Manifest Rules (this layer)**:
- Required: the ping handler validates in order (1) phase and latch, (2) state, bound pawn, not concealed, (3) finite point within range, then (4) the bucket; only an accepted ping spends a token — source: ADR-0011
- Required: the bucket is keyed by `ParticipantId`, never replicated, and resets only at `OnRosterReset` — source: ADR-0011, ADR-0007
- Guardrail: the handler is part of the queue-drain line of at most 0.5 ms on the listen-host game thread — source: ADR-0002, ADR-0022

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-05, AC-PNG-20, F1), scoped to this story:*

- [ ] **AC1** (AC-PNG-05 a): **GIVEN** defaults (size 2, 0.5/s, epsilon 0.05) and a full bucket at t = 0 **WHEN** pings arrive at t = 0.0, 0.1, 0.2, 2.1, 2.2 and 6.0 **THEN** the pings at 0.0, 0.1, 2.1 and 6.0 are accepted and 0.2 and 2.2 are dropped; the client mirror suppresses sending at 0.2 and 2.2.
- [ ] **AC2** (AC-PNG-05 b): **GIVEN** an empty bucket at server time 10.0 **WHEN** a ping arrives at 11.91 (tokens 0.955) **THEN** it is accepted, and one at 11.89 (0.945) is dropped (the exact boundary 11.90 is not asserted); a client mirror running 0.1 s behind allows its first press only at its 12.0, which the server accepts.
- [ ] **AC3** (AC-PNG-05 c): **GIVEN** a participant keyed by `ParticipantId` **WHEN** it disconnects and rejoins **THEN** its bucket is unchanged (never refilled by the rejoin); the bucket starts full at `Active` entry, is full again after `OnRosterReset`, and an accepted ping never takes it below `-epsilon`.
- [ ] **AC4** (AC-PNG-20): **GIVEN** `PingBucketPerSec` 0.5, 1.0 and 2.0 (epsilon 0.05, 0.1, 0.2 token), an empty bucket at server time 10.0 and a client mirror running 0.09 s ahead of the server clock **WHEN** the client's first allowed press arrives **THEN** it is accepted at every rate, and a press arriving 0.11 s before the exact refill boundary is dropped at every rate.

---

## Implementation Notes

*Derived from ADR-0011 Technical Setup 2026-10-25 and GDD F1:*

`tokens(t) = min(PingBucketSize, tokens(t_last) + PingBucketPerSec * (t - t_last))`; the server accepts when `tokens(t) >= 1 - epsilon` with `epsilon = PingBucketPerSec * PingSkewTolSec`, `PingSkewTolSec` a constant 0.1 s in the core (not a key, story 001 AC3), and acceptance spends one token. The same arithmetic class serves the server and the client mirror, with a strict flag (mirror requires `>= 1`, epsilon 0). Put it in `Source/StealthGameplay/Private/Features/Pings/StealthPingBucket.h/.cpp` as a plain class reused by story 005's `FTeamPingCore` and story 009's mirror (the mirror needs the type in `Public/View/`, so the header may need to sit there; keep it free of any `Server/` include). Server bucket state is held per `ParticipantId` in the core (story 005), reset by the Roster's `OnRosterReset` subscription (story 007). Never reset it at a disconnect, rebind, takeover or travel.

Ownership split: this story is the maths and its lifetime rules as pure functions; the validation order is story 005, the mirror usage is story 009.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: the validation order and drop reasons that call the bucket.
- Story 007: the subscription that resets the bucket at `OnRosterReset`.
- Story 009: the client mirror's integration into the gate, its reseed on HUD rebuild and its `GetBucketMirrorTokens()` accessor.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Public/View/Features/Pings/StealthPingBucket.h` (header in `View/` because the client mirror reuses the type; no `Server/` include)
- `Source/StealthGameplay/Private/Features/Pings/StealthPingBucket.cpp`
- `Source/StealthGameplay/Private/Tests/Pings/PingBucketTest.cpp` (class `FPingsBucketTest`, name `StealthGame.Pings.Bucket`)
- `Source/StealthGameplay/Private/Tests/Pings/PingSkewToleranceAcrossRatesTest.cpp` (class `FPingsSkewToleranceAcrossRatesTest`, name `StealthGame.Pings.SkewToleranceAcrossRates`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 default bucket sequence**
- Given: size 2, 0.5/s, epsilon 0.05, a full bucket at t = 0.
- When: pings at 0.0, 0.1, 0.2, 2.1, 2.2, 6.0.
- Then: 0.0, 0.1, 2.1, 6.0 accepted; 0.2 and 2.2 dropped; the client mirror suppresses sending at 0.2 and 2.2.
- Edge cases: refill is capped at size (6.0 accepted from a bucket refilled to its cap).

**Test: AC2 tolerance boundary and lagging mirror**
- Given: an empty bucket at server time 10.0.
- When: a ping at 11.91 (0.955 tokens) and, separately, at 11.89 (0.945); a mirror 0.1 s behind presses.
- Then: 11.91 accepted, 11.89 dropped; the mirror's first press is at its 12.0 and the server accepts it.
- Edge cases: do not assert exactly 11.90 (0.95 is not exactly representable).

**Test: AC3 persistence, reset, floor**
- Given: a bucket keyed by `ParticipantId`.
- When: disconnect and rejoin; enter `Active`; fire `OnRosterReset`; accept a ping at the tolerance floor.
- Then: the bucket is unchanged by rejoin (never refilled); full at `Active` entry; full after reset; tokens never below `-epsilon` after an accept.
- Edge cases: a rejoin with an empty bucket stays empty.

**Test: AC4 skew tolerance across rates**
- Given: rates 0.5, 1.0, 2.0 with epsilon 0.05, 0.1, 0.2; an empty bucket at server time 10.0; the mirror estimate 0.09 s ahead after the send that emptied the bucket.
- When: the mirror's first allowed press arrives; separately a press arrives 0.11 s before the exact refill boundary.
- Then: the first press is accepted at all three rates; the early press is dropped at all three rates.
- Edge cases: assert the rate-scaled epsilon, not a fixed 0.05.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Pings/PingBucketTest.cpp` and `PingSkewToleranceAcrossRatesTest.cpp` — must exist and pass (`/story-done` checks that they EXIST; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-001-ping-tuning-keys-and-invariant.md`
- Unlocks: `production/epics/informant-pings-team-comms/story-005-server-validation-core.md`, `story-009-client-gate-and-bucket-mirror.md`, `story-007-ping-lifecycle-clears-and-reset.md`
