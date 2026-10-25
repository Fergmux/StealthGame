# Story 081: A client missing pawns drops its loading screen at ActiveAtServerTime + RevealPawnWaitMaxSec; own-pawn-killed and Ended-first cases release at once

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-009` (`FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal); GDD AC 30c; rules R7 (the bound), R20.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: Uniform pawn net configuration for NPC and player, quantised server cull for non-allies plus a client-local hide, and an allowlist audit of every replicated property. This story rests on its consequence for the reveal: concealed pawns are culled per viewer and kills leave vacancies, so a client may never hold `ExpectedPawnCount` pawns once Active, and the reveal needs a time bound (`RevealPawnWaitMaxSec`) and a kind-blind rule about the client's own pawn. Client time comes only from `EstimatedServerNow()` (ADR-0002 section 9).
**ADR Version**: ADR-0003 2026-10-24; ADR-0002 2026-10-24 (clock estimator consumed)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 knowledge risk)
**Engine Notes**: Touches no post-cutoff setter. It consumes ADR-0002 section 9 (`EstimatedServerNow()`, `IsServerClockSynced()`, which is false until 5 samples have arrived; the documented unsynced fallback times the bound from the `Phase = Active` arrival) and relies on the client tolerating relevancy destroy and re-create (ADR-0003 S10 is the spike for client registries tolerating the destroy; not exercised here). Open: confirm against ADR-0008 how the harness injects pawn-replication loss (a harness hook that drops pawn actor channels versus net-emulation packet loss), since AC 30c needs a client that holds exactly `ExpectedPawnCount` - 3 pawns, its own included.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules):
- Required: Each client drops its loading screen when `EstimatedServerNow()` reaches `ActiveAtServerTime` and it holds `ExpectedPawnCount` pawns or `RevealPawnWaitMaxSec` has passed (provided it holds its own pawn or its own state is not `Active`); always on `Phase = Ended` (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never schedule client presentation from `GetServerWorldTimeSeconds()`, the arrival time of a replicated value or a client wall clock (ADR-0002).
- Guardrail: Clock exchange: < 50 B/s per client after warm-up (ADR-0002).

**Prototype disposition**: None (new Core client-side release rule; the prototype has no scheduled reveal or loading-screen bound).

**Where it sits**: Exit B (Core). It is the safety bound of the scheduled start: story 091 proves the on-time path, this story proves a slow or lossy client is released by a time bound and never holds its screen forever, at no cost to the other clients.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 30c, scoped to this story:*

- [ ] GIVEN a client with injected pawn-replication loss so that it holds `ExpectedPawnCount` − 3 pawns (its own included) at `ActiveAtServerTime`, THEN it drops its loading screen at `ActiveAtServerTime` + `RevealPawnWaitMaxSec` (± 1 frame); AND GIVEN its own pawn killed before it arrives (own state `Informant`), THEN the bound still releases it; AND GIVEN `Phase = Ended` arrives first, THEN it drops at once.

---

## Implementation Notes

*Derived from GDD R7 and R20, ADR-0002 section 9 and ADR-0003 Implementation Guidelines:*

- **The release rule.** A client drops its loading screen when its synchronised server clock (`EstimatedServerNow()`) reaches `ActiveAtServerTime` and either it holds `ExpectedPawnCount` pawns or `RevealPawnWaitMaxSec` (default 2.0 s; safe 0.5 to 10, All audience) has passed since `ActiveAtServerTime`, provided it holds its own pawn or its own Roster state is not `Active` (an own pawn killed while loading must not hold the screen). It always drops when `Phase = Ended` arrives. No countdown or "starting in" text is ever shown.
- **Pure decision function.** Implement the rule as a small pure struct in a client-safe `Public/View/` header (inputs: synchronised-clock flag, `EstimatedServerNow`, `ActiveAtServerTime`, held pawn count, `ExpectedPawnCount`, own-pawn held flag, own state, phase; output: drop or keep), so the unit-level cases are cheap and the PIE test only has to prove the wiring. It must read no controller kind, team or participant state beyond the client's own Roster state (kind-blind: a late arrival is accepted, not an identity tell).
- **Unsynced fallback.** If the clock is not yet synchronised when `Phase = Active` arrives, the bound is timed from that arrival (R7, ADR-0002 section 9). Cover it only if the harness can hold the estimator below 5 samples; otherwise leave it to story 091.
- **Why the bound exists.** Once Active, `ExpectedPawnCount` may never be reached (concealed pawns are culled for every viewer, kills leave vacancies). The client has already passed its single acknowledgement (P5 barrier), so the time bound only covers pawn arrival; a rejoiner drops its screen when its acknowledgement completes (Session rule 11.8), not through this rule.
- **Fixtures.** `RevealPawnWaitMaxSec` is a fixture value in the test constant files (story 005), not inline. Measure drop time on the client's server clock and allow +/- 1 frame.
- **Test shape.** Multi-client PIE with CQTest `NETWORK_TEST_CLASS(LateLoaderRevealBound, "StealthGame.Round.PIE")` ([PIE-3]): three cases (loss, own pawn killed, `Ended` first) each as a method or a parameterised run.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 029 (`production/epics/round-match-framework/story-029-public-state-replication.md`): `ExpectedPawnCount` and `ActiveAtServerTime` as fields of the public struct and their zeroing.
- Story 091 (`production/epics/round-match-framework/story-091-scheduled-reveal.md`): the on-time scheduled reveal within 50 ms of `ActiveAtServerTime` (Blocked on the OQ11(b) clock-sync spike).
- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the server-side Active-entry step that releases the loading screen.
- Session's acknowledgement and world-ready stages (`OnParticipantWorldReady`, Session rule 11) and the rejoiner's release (session-travel epic); the loading-screen widget itself (HUD epic).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Start/RevealGate.h` (client-safe pure decision struct; belongs under `Public/View/` per ADR-0007 section 2, `/dev-story` fixes the exact path)
- `Source/StealthGameplay/Private/Round/Start/RevealGate.cpp`
- `Source/StealthTestHarness/Private/Round/LateLoaderRevealBoundTest.cpp` (CQTest `NETWORK_TEST_CLASS(LateLoaderRevealBound, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.LateLoaderRevealBound.<Method>`; the GDD scenario name is `StealthGame.Round.LateLoaderRevealBound`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A client missing pawns drops its loading screen at `ActiveAtServerTime + RevealPawnWaitMaxSec`, and own-pawn-killed and Ended-first cases release at once.
  - Given: a client with injected pawn-replication loss, holding `ExpectedPawnCount` - 3 pawns (its own included) at `ActiveAtServerTime`.
  - When: the round runs; separately, its own pawn is killed before it arrives; separately, `Phase = Ended` arrives first.
  - Then: the loading screen drops at `ActiveAtServerTime + RevealPawnWaitMaxSec` (+/- 1 frame).
  - Then: with its own pawn killed (own state `Informant`), the bound still releases it.
  - Then: with `Ended` arriving first, it drops at once.
  - Edge cases: `RevealPawnWaitMaxSec` is a fixture value.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/LateLoaderRevealBoundTest.cpp` (CQTest `NETWORK_TEST_CLASS(LateLoaderRevealBound, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-029-public-state-replication.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (single acknowledgement, loading-screen release, world-ready stage; stories not yet created), `production/epics/shared-character-locomotion` (pawns whose arrival is counted; stories not yet created), `production/epics/tuning-data-config` (`RevealPawnWaitMaxSec` key, module skeleton and StealthTestHarness bootstrap; stories not yet created)
