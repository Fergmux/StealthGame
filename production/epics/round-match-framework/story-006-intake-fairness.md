# Story 006: Intake permutation is fair per source, deterministic per (secret, tick, mailbox set), keys strictly increasing

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-002` (one queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake). GDD AC 14. Rules: R11, R12, F1.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging (also ADR-0006: Seeds and deterministic/secret random streams)
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies ADR-0002 section 3: at drain start the non-empty mailboxes are sorted into the canonical base order (ascending `FParticipantId`, server-local last) and permuted by a per-tick Fisher-Yates permutation drawn from `GetServerSubSeed(SeqOrder, ServerTickIndex)` (ADR-0006), each mailbox's items stamped in FIFO order.
**ADR Version**: ADR-0002 2026-10-24; ADR-0006 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002; ADR-0006 is LOW)
**Engine Notes**: None touched. The permutation runs in the plain `FResolutionQueueCore` on integer arithmetic (ADR-0006 `FisherYates` over a canonically sorted array, `UniformBelow` with rejection, no floats, no `FRandomStream`). ADR-0002 Verification Required items (spawn inside `TG_PostPhysics`, client estimator accuracy, once-per-frame drain on the listen host) are not touched.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Mailbox and tie permutations come from `GetServerSubSeed(SeqOrder | TieRank, ServerTickIndex)` (ADR-0002, ADR-0006); every draw comes from `FSecretStream` (hidden decisions) or `FIntStream` (public geometry only); shuffles are Fisher-Yates over canonically sorted arrays
- Forbidden: Never use the public seed, join order or a rotating index for the mailbox permutation (a rotating start index was rejected: it favours adjacent pairs by (M-1)/M to 1/M, GDD R12); never log, trace or expose server sub-seeds or mailbox order (ADR-0006, ADR-0007)
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms listen-host game thread (ADR-0002); one permutation of at most 11 mailboxes per tick
- Manifest 2026-10-25g re-read at implementation: it changes no Foundation queue or Event Log rule (its changes are the S10 replication rules)

**Why this story**: Two commands from different players landing in the same frame must be ordered with probability exactly one half each way, reproducibly from the log, and unpredictably to any client (R12 effect (a)). This is Round's fairness promise for the final-site race and the mutual last strike, and a biased intake would favour the host or the earliest joiner every round. The story builds the stamping order and proves it statistically with fixed fixture secrets so the test is deterministic (no flake rate). Exit A.

**Prototype disposition**: none (new Foundation code). The prototype resolves in first-come order with no total-order key (EPIC.md Prototype Disposition).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` Acceptance Criteria no. 14, scoped to this story (one criterion, `story_granularity: fine`):*

- [x] GIVEN fixture secret S1 and 10,000 ticks with one item from each of 2 mailboxes, THEN source A is first in [4835, 5165] ticks; with 11 mailboxes, each source is first in [814, 1004] ticks; the same (secret, tick, mailbox set) gives the same permutation; keys are strictly increasing with no duplicate. `StealthGame.Round.IntakeFairness` [U]

---

## Implementation Notes

*Derived from ADR-0002 sections 1 and 3, ADR-0006 Key Interfaces, and GDD R11, R12 and F1:*

- Key: `FQueueKey { uint64 ServerTickIndex; uint64 Seq; }`, compared lexicographically, unique, drained in ascending key. `Seq` is a 64-bit counter that restarts at 1 each round and is assigned on the game thread when the item is stamped, never on arrival (R11).
- Intake at drain start (F1): collect the non-empty mailboxes `B_1..B_m` (m from 0 to 11: up to 10 participants plus one server-local), sort into the canonical base order (ascending `FParticipantId`, server-local last), then `pi_T = FisherYates(m, SubSeed(SeqOrder, T))`; stamp the items of `B_pi_T(1)` in FIFO order, then `B_pi_T(2)`, and so on. Fairness is per source per frame, not per command: a burst from one source stays contiguous.
- The draw is `GetServerSubSeed(SeqOrder, ServerTickIndex)` (`ESeedDomain::SeqOrder` = 11, append-only registry) into an `FSecretStream`, then `FisherYates` using `UniformBelow` rejection sampling (no `%`, no floats). The stream is derived from the secret injected by story 002 (the fixed fixture secret S1 from `RoundTestSeeds`, story 005), never from the public seed.
- Determinism contract: the same (secret, tick, mailbox set) gives the same permutation on a re-run; a changed mailbox set changes the permutation. The permutation must not depend on any hash-container iteration order.
- Mailboxes are keyed by `FParticipantId` (plus one server-local mailbox), not connection or controller. Only the intake order is built here; the 32-item cap, stale-generation drops and due-time stamping are story 007.
- Tests use the fake clock (story 001) to supply `ServerTickIndex`, the stub resolvers (story 004) to record resolve order, and the S1 constant from the constants file (story 005). The statistical bounds are fixed numbers for fixture S1, so the run is deterministic. Do not log the permutation or the sub-seed (control manifest Forbidden).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 007 (`production/epics/round-match-framework/story-007-intake-ordering-rules.md`): per-source order across 10,000 ticks, `DueEpsilonMs` stamping, `TieRank` ties, stale-connection drops and the 32-per-drain cap (it depends on this story).
- Story 001 (fake clock), story 002 (fixed secret), story 004 (stub resolvers), story 005 (fixture constants): the seams this story consumes.
- Story 043 (`production/epics/round-match-framework/story-043-no-draw-property.md`): outcome uniqueness over 100,000 interleavings, which reuses the permutation.
- The real drain tick function and the lag-compensation question: Round never reorders by estimated one-way delay (R12, "No lag compensation").

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Queue/StealthResolutionQueueCore.h` (mailboxes in canonical order, per-tick Fisher-Yates from `GetServerSubSeed(SeqOrder, tick)`, `Seq` stamping)
- `Source/StealthFoundation/Private/Queue/StealthResolutionQueueCore.cpp`
- `Source/StealthFoundation/Private/Tests/Round/IntakeFairnessTest.cpp` (test `StealthGame.Round.IntakeFairness`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN fixture secret S1 and 10,000 ticks with one item from each of 2 mailboxes, THEN source A is first in [4835, 5165] ticks; with 11 mailboxes, each source is first in [814, 1004] ticks; the same (secret, tick, mailbox set) gives the same permutation; keys are strictly increasing with no duplicate.
  - Given: fixture secret S1, 10,000 ticks, one item per mailbox per tick.
  - When: 2 mailboxes are drained, then 11 mailboxes, recording which source is first each tick.
  - Then: with 2 mailboxes, source A is first in [4835, 5165] ticks.
  - Then: with 11 mailboxes, each source is first in [814, 1004] ticks.
  - Then: the same (secret, tick, mailbox set) gives the same permutation on a re-run.
  - Then: keys are strictly increasing with no duplicate.
  - Edge cases: the bounds are fixed for fixture S1, so the test is deterministic; a changed mailbox set changes the permutation; the secret is not shared across tests.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Round/IntakeFairnessTest.cpp` — automation test `StealthGame.Round.IntakeFairness` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundIntakeFairnessTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [x] Created and passing (`StealthGame.Round.IntakeFairness`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-001-test-seam-fake-clock.md`, `production/epics/round-match-framework/story-002-test-seam-fixed-server-secret.md`, `production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`, `production/epics/round-match-framework/story-005-test-fixture-constants.md`
- Cross-epic: `production/epics/session-travel` (`FSecretStream`, SipHash-2-4 and `GetServerSubSeed`: `story-008-secret-stream-counter-independence.md`, `story-010-seed-derivation-golden-vectors.md`, `story-011-server-sub-seed-server-only.md`)
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-007-intake-ordering-rules.md`

---

## Completion Notes
**Completed**: 2026-10-26 (round-match-framework Exit A batch 001-017)
**Criteria**: 1/1 passing (`StealthGame.Round.IntakeFairness`)
**Notes**: With S1 over 10,000 ticks: 2 mailboxes, source A first within [4835, 5165]; 11 mailboxes, every source first within [814, 1004]; a re-run gives identical permutations; dropping the server-local mailbox changes the permutation; keys strictly increasing across the run. Deterministic (fixed S1, no flake rate); the permutation and sub-seeds are never printed.
**Build/test**: see `production/qa/evidence/story-006-intake-fairness/full-suite.txt`
