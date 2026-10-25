# Story 043: At most one terminal outcome per round and equal to the F2 reference over 100,000 interleavings; the enums have no draw

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-005` (Objective and elimination wins; no draw, no clock) and `TR-round-004` (Single-assignment latch and the two-step round end); GDD AC 17; rules R15 (the terminal latch), R16 (no ties, no draws, no clock), F2 (win predicates and outcome uniqueness).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies section 8.1: Round's latch record (`Terminated`, outcome, `TerminalKey`) is authoritative; `UResolutionQueue::Latch(FQueueKey)` is single-assignment and is called solely by Round's `TryTerminate` in the same call that sets `Terminated`; a later `TryTerminate` returns false and logs `LateWinSuppressed`.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 knowledge risk)
**Engine Notes**: None touched. The latch, the queue core (`FResolutionQueueCore`) and the F2 predicates are plain C++ with no `UWorld`; the test runs headless against the fake clock. ADR-0002 verification items (1) spawn inside `TG_PostPhysics`, (2) the client estimator and (3) one drain per server frame are not exercised. The test uses reflection only to enumerate enum values (`StaticEnum<>()`, pre-cutoff API).

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Resolution queue and Forbidden Approaches):
- Required: only Round's `TryTerminate` calls `Latch`; after the latch the queue rejects every item with `Rejected_Latched` except parked `HostCommand`s, the post-latch lane and the `ConcealmentTransition` exemption (ADR-0002).
- Forbidden: Never re-submit or re-stamp an outcome (ADR-0002).
- Guardrail: queue drain, staging, teardown and Roster flush stay at or below 0.5 ms of listen-host game thread (ADR-0002, ADR-0022); this headless property test asserts no timing, only the outcome.

**Prototype disposition**: Replaces the win and end logic of `AStealthGameMode::EndRound(WinningTeam, Reason)`, `CheckEliminationVictory` and `OnObjectiveCompleted` in `Source/StealthGame/Private/StealthGameMode.cpp` (first-come resolution, `EStealthWinReason`, no latch, no key order). Promotes `FStealthRules::EvaluateEliminationWinner` (`Source/StealthGame/Private/StealthRules.cpp`) and the elimination tests in `Source/StealthGame/Private/Tests/StealthRulesTests.cpp` as reference cases, re-expressed against the queue-driven latch.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 17, scoped to this story:*

- [ ] GIVEN fixture seeds and 100,000 random interleavings over the alphabet {FinalCompletion(t), DecisionElimination(t), GraceExpiry(p), Kick(p), Rejoin(p), PawnLost(p), Abort}, THEN at most one terminal outcome per round, and outcome and winner equal an independent reference implementation of F2 over the sorted keys; a static check confirms the result and `EndReason` enums have no draw value.

---

## Implementation Notes

*Derived from ADR-0002 section 8 and GDD R15, R16, F2:*

- **The latch (R15).** `TryTerminate(outcome, key)` with `outcome` in {`Win(result)`, `Void(code)`} is single-assignment: it succeeds only if the phase is `Active` and `Terminated` is false; it then stores the outcome and `TerminalKey`, sets `Terminated`, calls `UResolutionQueue::Latch(TerminalKey)` in the same call (closing intake) and performs R9's latch step. A later attempt returns false and is logged `LateWinSuppressed(outcome, key)`. Round implements `ITerminalDecision` and is the only caller of `Latch`; `IsLatched()` mirrors Round's record for resolvers that must not include Round headers. Because the queue is processed serially on one thread, at most one terminal outcome exists per round. A void also latches but stages no `RoundEndStep`; a host `AbortRound` is a host command and not a `TryTerminate` outcome (ADR-0002 section 8.8).
- **What this story builds.** The latch record and `TryTerminate` (a plain C++ core under `Round/Win/`), the outcome and `EndReason` enums, and the interleaving property test. The latch step's later effects (round-end step, score, `EndRoundAll`, rejections) are story 052; strike staging is 053. Here `TryTerminate` need only prove single assignment and the F2 outcome.
- **The independent reference (test-only).** Write a second implementation of F2 in the test support code, not calling any production predicate: over the items sorted by key it returns the first item for which `ObjectiveWin` (Type FinalCompletion, `Valid(i)`, completer on `t`, not latched, phase Active), `EliminationWin(W, e)` or `Void(e)` holds, per the F2 text; `PawnLost` yields `Void(PawnLost)` only per R17.9 (third loss or retry timeout), and `Abort` leaves `Active` without a terminal outcome (F2 Output Range: "other than by host abort or session end"). The reference never shares helper code with production.
- **Interleaving fixture.** Generate 100,000 random sequences over the seven-symbol alphabet with a fixed seed from `RoundTestSeeds` (story 002) and a test-local PRNG (not the production secret streams). Sequence length and participant counts are fixed fixture constants in the test constants file (story 005), never inline. Feed each sequence through the real latch with strictly increasing keys from the fake clock (story 001) and stub resolvers (stories 004 and 008); record every `TryTerminate` result.
- **Enums have no draw.** Enumerate the result outcome enum and the `EndReason` enum with `StaticEnum<>()` and assert no enumerator means draw, tie or both (`FRoundResult` has no such value, R16 (1)); the expected enumerator list is a reviewed fixture so adding a value fails the test until it is reviewed. Open: confirm against GDD R9 and R15 and the Foundation `Public/View/` declarations the exact enum type names; the AC says only "the result and `EndReason` enums".
- **Why determinism matters.** Seeds are fixed, so the run is reproducible and has no flake rate (Test seams paragraph). Keep each iteration allocation-light so 100,000 runs stay fast in the headless run.
- **Where the code goes.** `Source/StealthGameplay/Public/Round/Win/RoundTerminalLatch.h` and `Private/Round/Win/RoundTerminalLatch.cpp` (plain C++ core), `Public/Round/Win/RoundOutcome.h` for the enums; names are planned, `/dev-story` replaces them with the real ones.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 044 (`production/epics/round-match-framework/story-044-elimination-truth-table.md`): the F2 zeroing-event truth table, clause by clause.
- Story 045 (`production/epics/round-match-framework/story-045-same-tick-final-vs-elimination.md`) and story 046: the two named same-tick cases (final versus elimination, mutual last strike).
- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`): the effects of the latch step as the round-end step (result, score, `EndRoundAll`, rejections, `PostRound`).
- Story 053: staging of a strike win until the death write.
- Stories 047, 048 and 049: the empty-team, informant and completer-validity rules.
- Story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): the resolver and `Seq` contract this test builds on.
- Roster internals (transition events and counts) and the real Objectives and Assassination resolvers are other epics (AC 2 is deferred, so there is no story for it).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Win/RoundOutcome.h`
- `Source/StealthGameplay/Public/Round/Win/RoundTerminalLatch.h`
- `Source/StealthGameplay/Private/Round/Win/RoundTerminalLatch.cpp`
- `Source/StealthGameplay/Private/Tests/Round/F2Reference.h` (independent reference model, test support)
- `Source/StealthGameplay/Private/Tests/Round/NoDrawPropertyTest.cpp` (class `FRoundNoDrawPropertyTest`, test name `StealthGame.Round.NoDrawProperty`)
- `Source/StealthGame/Private/StealthGameMode.cpp` (`EndRound`, `CheckEliminationVictory`, `OnObjectiveCompleted`: retired/replaced by this story and its neighbours)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: At most one terminal outcome per round, equal to the F2 reference over 100,000 interleavings, and the enums have no draw.
  - Given: fixture seeds, an independent F2 reference implementation, and random interleavings over {FinalCompletion(t), DecisionElimination(t), GraceExpiry(p), Kick(p), Rejoin(p), PawnLost(p), Abort}.
  - When: 100,000 interleavings run through the real latch.
  - Then: each round has at most one terminal outcome.
  - Then: outcome and winner equal the reference over the sorted keys.
  - Then: a static check confirms the result and `EndReason` enums have no draw value.
  - Edge cases: interleaving length and participant counts must be fixed fixture constants; the reference must be written independently from F2 and not call the production code; the seeds are fixed, so the run is reproducible.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/NoDrawPropertyTest.cpp` (automation test `StealthGame.Round.NoDrawProperty`, class `FRoundNoDrawPropertyTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-001-test-seam-fake-clock.md`, `production/epics/round-match-framework/story-002-test-seam-fixed-server-secret.md`, `production/epics/round-match-framework/story-008-resolver-contract.md`
- Unlocks: `production/epics/round-match-framework/story-044-elimination-truth-table.md`, `production/epics/round-match-framework/story-045-same-tick-final-vs-elimination.md`, `production/epics/round-match-framework/story-052-ended-latch-effects.md`
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
