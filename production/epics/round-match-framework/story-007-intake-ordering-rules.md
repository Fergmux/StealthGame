# Story 007: Per-source order, DueEpsilonMs stamping, TieRank ties, stale-connection and per-drain cap rules hold

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-002` (one queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake), `TR-round-003` (drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item). GDD AC 45. Rules: R12 (ruling D22, cross-review C-B5), R11.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies ADR-0002 sections 3 and 4: per-mailbox FIFO with the 32-item cap and stale-generation drops, and the due-time heap whose items due at or before the drain start are stamped before that frame's intake, ordered by `(DueTime, TieRank)`.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: None touched. All logic lives in the plain `FResolutionQueueCore` driven by the fake clock. ADR-0002 notes that the drain tick function runs once per server frame in `TG_PostPhysics` and that on a listen host the server frame is the render frame (ADR-0002 Verification Required item 3); that is a harness measurement outside this story. `TWeakInterfacePtr` is not used by the core.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Gameplay timers are `ScheduleAt` items in the due-time heap; mailbox and tie permutations come from `GetServerSubSeed(SeqOrder | TieRank, ServerTickIndex)` (ADR-0002)
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager` (cross-actor tick order is unspecified and join-order biased) (ADR-0002)
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms listen-host game thread (ADR-0002); at most 32 discrete items per mailbox per drain bounds the intake work
- Manifest 2026-10-25g re-read at implementation: it changes no Foundation queue or Event Log rule (its changes are the S10 replication rules)

**Why this story**: Fairness (story 006) says who is first among simultaneous commands. This story pins the rest of the ordering contract that every timed gameplay item relies on: a source's own commands never swap, a timer that was already due is stamped before the frame's commands so a brace that expired before the frame is not extended by a frame, the one 1 ms tolerance (`DueEpsilonMs`) absorbs floating-point drift (a 3.0 s hold begun on tick 0 at 60 Hz is due on tick 180, never tick 181), exact ties are broken per tick from the secret, a replaced controller keeps its mailbox but a superseded connection's stale items are dropped, and a flood is capped. The IIF and Assassination cite this tolerance instead of their own. It is the largest queue story (L). Exit A.

**Prototype disposition**: none (new Foundation code). The prototype has no due-time heap, no mailbox cap and no connection generation.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` Acceptance Criteria no. 45, scoped to this story (one criterion, `story_granularity: fine`; a multi-clause criterion kept as one):*

- [x] GIVEN source A submits A1 then A2 in one frame over 10,000 ticks, THEN A1 always precedes A2; GIVEN `DueEpsilonMs` = 1 (R12, ruling D22): an item with `DueTime <= DrainStartTime` is stamped before that frame's intake, an item due 0.5 ms after the drain start fires in that drain (stamped before intake), one due exactly 1 ms after it also does (inclusive), and one due 2 ms after it does not (it waits for the next drain); a 3.0 s hold begun on tick 0 at 60 Hz is stamped on tick 180 and never on tick 181 over 10,000 accumulated-time fixtures; GIVEN two items with equal `DueTime`, THEN they order by that tick's `TieRank`; a mailbox survives a controller replacement and an item with an old `ConnectionGeneration` is `Dropped(StaleConnection)`; a 33rd item in one drain is dropped and counted. `StealthGame.Round.IntakeOrderingRules` [U]

---

## Implementation Notes

*Derived from ADR-0002 sections 3 and 4, ADR-0002 Implementation Guidelines, and GDD R12:*

- Due rule (R12): an item is due at a drain when `DueServerTime <= DrainStartTime + DueEpsilonMs`, with `DueEpsilonMs` = 1 ms, a design constant (not a tuning key; GDD Tuning Knobs "Constants, not knobs"). Hold the constant in one place in the core; never give a feature its own tolerance.
- Stamping groups in one drain: (1) items already due by the rule are stamped first, ordered `(DueTime, TieRank)`; (2) then intake items from the mailboxes in the story-006 permutation; (3) items that become due during the drain's own processing (scheduled inline with `DueTime <= DrainStartTime + 1 ms`) are stamped after intake; an item due more than 1 ms after the drain start waits for a later drain. Within a group, equal `DueTime` is broken by `TieRank` (the `HoldTimer` exception, equal `DueTime` ordered by the key of the `Begin` command, is the IIF's rule in R12 and is not asserted here).
- `TieRank` is a per-tick server-secret shuffle of `ParticipantId`s from `GetServerSubSeed(TieRank, ServerTickIndex)` (`ESeedDomain::TieRank` = 12): not the id itself (it would always favour the earliest joiner) and not fixed per round.
- Accumulated-time fixtures: a 3.0 s hold begun on tick 0 at 60 Hz accumulates `DueTime` from frame deltas; across 10,000 fixtures generated from a fixed `RoundTestSeeds` seed the item must be stamped on tick 180 and never tick 181. Use the fake clock (story 001) to advance time and ticks; never wall-clock.
- Mailboxes: per-source FIFO keyed by `FParticipantId`, so A1 always precedes A2. Each item carries the sender's Session `ConnectionGeneration`; an item with an older generation than the participant's current one is `Dropped(StaleConnection)` and logged. A controller replacement (seamless travel) keeps the mailbox. At most `MailboxMaxItemsPerDrain` = 32 discrete items per mailbox per drain; the excess is dropped and counted (`MailboxOverflow`); AI `Move` and `Look` are continuous inputs and never count (R12).
- The Event Log entries `Dropped(StaleConnection)` and `MailboxOverflow` are appended through `IRoundLogWriter` (ADR-0002 section 8.9). This story depends on stories 001, 004 and 006 only, so use the minimal writer seam declared in story 008 or a counter on the core; the assertion here is the drop result and the count. Open: confirm against ADR-0002 section 8.9 which story first declares `IRoundLogWriter` (story 008 proposes the minimal declaration; story 011 extends it).
- Nothing in this story logs through `UE_LOG`/`UE_LOGF`; counters hold counts only.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 006 (`production/epics/round-match-framework/story-006-intake-fairness.md`): the Fisher-Yates intake permutation and strictly increasing keys (this story depends on it).
- Story 008 (`production/epics/round-match-framework/story-008-resolver-contract.md`): what resolvers return and how `Seq` is consumed by outcomes.
- Story 010 (`production/epics/round-match-framework/story-010-stale-epoch-item.md`): `Rejected_StaleEpoch` (the epoch check runs before the connection-generation check).
- Story 071 (`production/epics/round-match-framework/story-071-grace-generation-guard.md`): the grace timer's stale-generation case, which is Round's use of this drop.
- IIF `HoldTimer` equal-`DueTime` ordering by `Begin` key and the IIF rate limit/kick (input-interaction-framework epic).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Queue/StealthResolutionQueueCore.h` (due-time heap, `DueEpsilonMs` = 1 rule (integer microseconds), `TieRank`, 32-item cap, connection-generation check)
- `Source/StealthFoundation/Private/Queue/StealthResolutionQueueCore.cpp`
- `Source/StealthFoundation/Private/Tests/Round/IntakeOrderingRulesTest.cpp` (test `StealthGame.Round.IntakeOrderingRules`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN source A submits A1 then A2 in one frame over 10,000 ticks, THEN A1 always precedes A2; GIVEN `DueEpsilonMs` = 1: an item with `DueTime <= DrainStartTime` is stamped before that frame's intake, an item due 0.5 ms after the drain start fires in that drain, one due exactly 1 ms after it also does (inclusive), one due 2 ms after it does not; a 3.0 s hold begun on tick 0 at 60 Hz is stamped on tick 180 and never on tick 181; two items with equal `DueTime` order by that tick's `TieRank`; a mailbox survives a controller replacement and an item with an old `ConnectionGeneration` is `Dropped(StaleConnection)`; a 33rd item in one drain is dropped and counted.
  - Given: the fake clock, stub resolvers and `DueEpsilonMs` = 1.
  - When: the listed submissions are drained: A1 then A2 over 10,000 ticks; due-time offsets; equal `DueTime` pairs; old-generation items; 33 items in one drain.
  - Then: A1 always precedes A2 over 10,000 ticks.
  - Then: `DueTime <= DrainStartTime` is stamped before that frame's intake.
  - Then: items due +0.5 ms and exactly +1 ms are stamped in that drain, and an item due +2 ms waits for the next drain.
  - Then: a 3.0 s hold begun on tick 0 at 60 Hz is stamped on tick 180 and never on tick 181, over 10,000 accumulated-time fixtures.
  - Then: equal-`DueTime` items order by that tick's `TieRank`.
  - Then: a mailbox survives a controller replacement, and an item with an old `ConnectionGeneration` is `Dropped(StaleConnection)`.
  - Then: a 33rd item in one drain is dropped and counted.
  - Edge cases: the 10,000 accumulated-time fixtures must be generated from a fixed `RoundTestSeeds` seed; the +1 ms boundary is inclusive; the cap is 32 per drain.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Round/IntakeOrderingRulesTest.cpp` — automation test `StealthGame.Round.IntakeOrderingRules` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundIntakeOrderingRulesTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [x] Created and passing (`StealthGame.Round.IntakeOrderingRules`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-001-test-seam-fake-clock.md`, `production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`, `production/epics/round-match-framework/story-006-intake-fairness.md`
- Cross-epic: `production/epics/session-travel` (`ConnectionGeneration` is Session's counter and `GetServerSubSeed` supplies `TieRank`; the test injects both, so Session need not be Done first)
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (round-match-framework Exit A batch 001-017)
**Criteria**: 1/1 passing (`StealthGame.Round.IntakeOrderingRules`)
**Notes**: All clauses: A1 before A2 in 10,000 of 10,000 ticks; already-due, +0.5 ms and exactly +1 ms items fire before intake, +2 ms waits a drain, inline-due items stamp after intake; the 3.0 s hold lands on tick 180 in all 10,000 jittered accumulated-time fixtures; equal `DueTime` follows that tick's `TieRank` (recomputed independently; it varies per tick); a controller replacement keeps the mailbox and an old generation is `Dropped(StaleConnection)` and logged; the 33rd item is dropped, counted and logged `MailboxOverflow`. Code-review fixes with regression cases: the due rule compares integer microseconds (a ulp cannot move the inclusive bound), the comparator is a strict weak order (holds first by Begin key at equal `DueTime`), and inline-due passes are bounded (`MaxInlineDuePasses` 16, fault).
**Build/test**: see `production/qa/evidence/story-007-intake-ordering-rules/full-suite.txt`
