# Story 091: Each client's loading screen drops within 50 ms after ActiveAtServerTime and never before it holds ExpectedPawnCount pawns

> **Epic**: Round & Match Framework
> **Status**: Blocked — BLOCKED: the OQ11(b) client clock-sync spike (ADR-0002 section 9 estimator [U]) has not reported. Unblocks when the spike reports the estimator's accuracy under net emulation (ADR-0002 budget: p95 <= 15 ms, p99 <= 30 ms under `LeakNetProfile`) and the 50 ms bound is confirmed or revised; then re-check the story with `/story-readiness`.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-009` (`FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal); GDD AC 30a ([PIE-3] [SPIKE]; it also carries the 50 ms loading-screen clause moved out of AC 1a / story 084); rules R7, R20, P7.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging; ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0002: one total-order queue per round plus an RTT-compensated client `EstimatedServerNow()`. This story applies section 9: an NTP-style four-timestamp clock exchange (`ServerClockPing`/`ClientClockPong`, timestamps only), minimum-delay sample selection over the last 16 samples, monotonic slewing at <= 50 ms per second, `IsServerClockSynced()` false until 5 samples, and an accuracy budget under `LeakNetProfile` of <= 15 ms (p95) and <= 30 ms (p99). ADR-0003 supplies the concealment cull context for the pawn count (`ExpectedPawnCount`) the client must hold before the screen drops.
**ADR Version**: ADR-0002 2026-10-24; ADR-0003 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: Touches ADR-0002 Verification item (2): the net-emulation acceptance test of the client estimator (`LeakNetProfile`: PktLag 100 ms, PktLoss 1-2 %; plus a 200 ms and an asymmetric profile) against the accuracy budget in section 9; the client clock is the [U] item. Presentation must use `EstimatedServerNow()`, never `GetServerWorldTimeSeconds()` (engine GameState server time is uncorrected one-way lag in a float delta). Asymmetric paths bias any two-way method by half the asymmetry; the budget does not cover it (stated limit). The fallback if the exchange fails the budget (GameState `ReplicatedWorldTimeSecondsDouble` corrected by half RTT) is recorded in ADR-0002 Alternative 6, not chosen. No post-cutoff API is called directly.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules):
- Required: Client presentation schedules only from `EstimatedServerNow()`; the clock-exchange RPCs carry only timestamps (ADR-0002).
- Forbidden: Never schedule client presentation from `GetServerWorldTimeSeconds()`, the arrival time of a replicated value or a client wall clock (ADR-0002).
- Guardrail: Clock exchange: < 50 B/s per client after warm-up (ADR-0002).

**Prototype disposition**: None (new Core client-side release; the prototype has no scheduled reveal).

**Where it sits**: Exit B (Core). It is anchor moment 1: the loading screen lifts for every participant at one scheduled server time within the clock-sync error. It carries AC 1a's 50 ms clause, which was moved out of story 084; it reuses story 084's 100 ms RTT session setup. Story 081 proves the bound for a slow loader; this story proves the on-time path.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 30a (unchanged text; revised note in `rmf-ac-overrides.md`: it also carries the 50 ms clause moved out of AC 1a), scoped to this story:*

- [ ] GIVEN the host and two clients under fixed net emulation, THEN each client's loading-screen dismiss time (on its server clock) is >= `ActiveAtServerTime` and <= `ActiveAtServerTime` + 50 ms, and never before it holds `ExpectedPawnCount` pawns.

---

## Implementation Notes

*Derived from ADR-0002 section 9, GDD R7, R20 and the P7 row of R6:*

- **The scheduled start.** Round waits until every `Connected` client is world-ready or `WorldReadyTimeoutSec` (10 s) passes, then sets `ActiveAtServerTime = ServerTime + StartLeadSec` (0.25 s) in `FRoundPublicState`. At the first drain at or after that time R7 runs: `ActiveAtServerTime` zeroed, queue opened, loading screen released on the server side. Each client drops its screen locally when `EstimatedServerNow()` reaches `ActiveAtServerTime` and it holds `ExpectedPawnCount` pawns (the `RevealPawnWaitMaxSec` bound is story 081).
- **What the test measures.** In a [PIE-3] run (host plus two clients under fixed net emulation, the 100 ms RTT fixture shared with story 084), record each client's dismiss time on its own `EstimatedServerNow()` clock: it must be >= `ActiveAtServerTime` and <= `ActiveAtServerTime` + 50 ms, and never earlier than the moment that client holds `ExpectedPawnCount` pawns. `ActiveAtServerTime` is zeroed in the Active-entry step, so the test captures it from the `Preparing` replication before the zeroing.
- **Clock exchange.** `EstimatedServerNow(const UWorld*)` is the only server-time source presentation may use; the host calls the same function and its estimate equals `ServerNow()` exactly (host parity). The estimator uses the minimum-delay of the last 16 samples and slews at <= 50 ms/s; it is unsynced until 5 samples, and the unsynced fallback times the bound from the `Phase = Active` arrival (R7).
- **The 50 ms bound is provisional** until the OQ11(b) spike reports: the GDD states the residual spread between clients is the clock-sync error (about 30 ms, [S] spike), not the one-way delay. ADR-0002's budget (p95 <= 15 ms, p99 <= 30 ms under `LeakNetProfile`) sits inside it; the spike decides whether 50 ms holds. Do not change the number in the test without a GDD change.
- **Fixtures.** Freeze the emulation profile as a fixture constant (AC 1a uses 100 ms RTT); `StartLeadSec` must exceed the worst one-way delay plus one send interval. Open: confirm against ADR-0008 whether this run uses `LeakNetProfile` or the AC 1a profile fixture.
- **Test shape.** CQTest `NETWORK_TEST_CLASS(ScheduledReveal, "StealthGame.Round.PIE")` in `StealthTestHarness`.
- **Gate.** Do not start until the spike reports; `/story-readiness` re-confirms.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 084 (`production/epics/round-match-framework/story-084-pie-lifecycle-start.md`): Start through travel to Active, whose session setup this story reuses (this story's dependency).
- Story 081 (`production/epics/round-match-framework/story-081-late-loader-reveal-bound.md`): the bounded reveal for a client missing pawns, an own pawn killed and `Ended` arriving first.
- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the Active-entry step.
- The clock-exchange implementation itself (`ServerClockPing`/`ClientClockPong`, estimator; Foundation clock work) and the loading-screen widget (HUD epic); Session's single acknowledgement and world-ready stage (session-travel epic).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/ScheduledRevealTest.cpp` (CQTest `NETWORK_TEST_CLASS(ScheduledReveal, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.ScheduledReveal.<Method>`; the GDD scenario name is `StealthGame.Round.ScheduledReveal`)
- `Source/StealthFoundation/Private/Tests/Fixtures/StealthRoundTestConstants.h` (fixture constants file from story 005; the net-emulation profile)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Each client's loading screen drops within 50 ms after `ActiveAtServerTime` and never before it holds `ExpectedPawnCount` pawns.
  - Given: the host and two clients under fixed net emulation.
  - When: the round enters Active.
  - Then: each client's dismiss time (on its server clock) is >= `ActiveAtServerTime`.
  - Then: it is <= `ActiveAtServerTime + 50 ms`.
  - Then: it is never before the client holds `ExpectedPawnCount` pawns.
  - Edge cases: freeze the emulation profile as a fixture (AC 1a uses 100 ms RTT); the 50 ms bound is provisional until the spike reports.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/ScheduledRevealTest.cpp` (CQTest `NETWORK_TEST_CLASS(ScheduledReveal, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-084-pie-lifecycle-start.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (single acknowledgement, world-ready stage, loading-screen release; stories not yet created), `production/epics/tuning-data-config` (`StartLeadSec`, `WorldReadyTimeoutSec`, module skeleton and StealthTestHarness bootstrap; stories not yet created)
