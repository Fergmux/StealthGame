# Story 010: An item stamped with the previous RoundEpoch is Rejected_StaleEpoch when drained

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25g
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-002` (one queue per round, `Key = (ServerTickIndex, Seq)`, per-source mailboxes, permuted intake). GDD AC 13b. Rules: R10 (Reset invalidates `RoundEpoch`-stamped work), R13 (`TimerFired` carries `RoundEpoch`; stale epoch returns `Rejected_StaleEpoch`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging (also ADR-0005: Session, seamless travel and match-scoped state stores)
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies ADR-0002 section 4 and section 2: every queued item carries an `FEpochToken Expected` (`{MatchId, RoundEpoch}`), and a stale token resolves `Rejected_StaleEpoch` and changes nothing. ADR-0005 supplies the epoch itself: `RoundEpoch` lives in the Roster store and the next round's `ResetAll` increments it.
**ADR Version**: ADR-0002 2026-10-24; ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002; ADR-0005 is HIGH, but this story touches no ADR-0005 mechanism beyond the epoch value)
**Engine Notes**: None touched. The check is in the plain core with an injected current-epoch source. ADR-0005's seamless-travel spikes (S3, S4, S5) concern the real travel path and are not touched. ADR-0002 Verification Required items 1 to 3 are not touched.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Every timed item carries an `FEpochToken` (and a hold's `ActionEpoch`); a stale token resolves `Rejected_StaleEpoch` and changes nothing (ADR-0002)
- Forbidden: Never let a resolver re-submit or re-stamp an outcome; a stale item must never reach its resolver (ADR-0002)
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms listen-host game thread (ADR-0002); the epoch check is one integer compare per item
- Manifest 2026-10-25g re-read at implementation: it changes no Foundation queue or Event Log rule (its changes are the S10 replication rules)

**Why this story**: Between rounds, timers and queued commands from the previous round are still in mailboxes or the heap. After `Reset` and the next `ResetAll`, they must not act on the new round (a grace timer from round 1 converting a participant in round 2). The epoch stamp is the single guard. This is the Foundation half of the Reset guarantee; story 058 asserts the full Reset effect in Core. It depends on stories 004 and 008. Exit A.

**Prototype disposition**: none (new Foundation code). The prototype restarts the round in place and has no epoch.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` Acceptance Criteria no. 13b, scoped to this story (one criterion, `story_granularity: fine`):*

- [x] GIVEN an item stamped with the previous `RoundEpoch` that was already submitted (in a mailbox) before `ResetAll`, WHEN it is drained, THEN `Rejected_StaleEpoch`. `StealthGame.Round.StaleEpochItem` [U]

---

## Implementation Notes

*Derived from ADR-0002 sections 2, 4 and 5, and GDD R10 and R13:*

- `FQueueItem` carries `FEpochToken Expected` for every kind. At stamping (or at the slot, before the resolver is called; confirm with the queue core's structure) the core compares `Expected.RoundEpoch` to the current epoch and returns `EVoidReason::Rejected_StaleEpoch`; the resolver is not called and no state changes. The result is logged (`Voided`-class entry through `IRoundLogWriter`, subject to the rate limit of story 015).
- The current epoch comes from an injected source (a stub in the test); in production the Roster's `RoundEpoch` after `ResetAll`. Open: confirm against ADR-0002 sections 1 and 4 and ADR-0005 how the queue core reads the current epoch (injected getter versus `FEpochToken` held by the core), since the ADR specifies the token on the item but not the comparison source.
- Order of checks (QA edge case): the epoch check comes before the connection-generation check of story 007 (AC 49 relies on this), so a stale-epoch item from a stale connection reports `Rejected_StaleEpoch`. A current-epoch item in the same drain resolves normally.
- The scenario is: submit under epoch N, simulate `ResetAll` (epoch N+1), drain. The test does not need the real Roster; it advances the injected epoch.
- Staged writes are also dropped on `Reset` by epoch mismatch (ADR-0002 section 6); that is the staging pass, out of scope here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 007 (`production/epics/round-match-framework/story-007-intake-ordering-rules.md`): the stale `ConnectionGeneration` drop (checked after the epoch check).
- Story 058 (`production/epics/round-match-framework/story-058-reset-clears.md`): the full Reset behaviour (pawn destruction before lobby travel, empty timer heap, Lobby only when all participants are terminal or timed out). This story only unlocks it.
- Story 071 (`story-071-grace-generation-guard.md`): the grace timer's stale-generation rejection.
- Roster internals (`ResetAll` incrementing the epoch, Roster stale-epoch rejection): participant-state-roster epic.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Queue/StealthResolutionQueueCore.h` (epoch check at the slot, before the connection-generation and latch checks)
- `Source/StealthFoundation/Private/Queue/StealthResolutionQueueCore.cpp`
- `Source/StealthFoundation/Private/Tests/Round/StaleEpochItemTest.cpp` (test `StealthGame.Round.StaleEpochItem`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN an item stamped with the previous `RoundEpoch` that was already submitted (in a mailbox) before `ResetAll`, WHEN it is drained, THEN `Rejected_StaleEpoch`.
  - Given: an item submitted to a mailbox under epoch N, then `ResetAll` (epoch N+1).
  - When: the queue drains.
  - Then: the item's result is `Rejected_StaleEpoch`.
  - Edge cases: the epoch check comes before the connection-generation check (AC 49); a current-epoch item in the same drain resolves normally.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Round/StaleEpochItemTest.cpp` — automation test `StealthGame.Round.StaleEpochItem` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundStaleEpochItemTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [x] Created and passing (`StealthGame.Round.StaleEpochItem`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`, `production/epics/round-match-framework/story-008-resolver-contract.md`
- Cross-epic: `production/epics/participant-state-roster` (contract only: `RoundEpoch` and `ResetAll`; the test injects the epoch, so the Roster stories are not a build prerequisite)
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-058-reset-clears.md`

---

## Completion Notes
**Completed**: 2026-10-26 (round-match-framework Exit A batch 001-017)
**Criteria**: 1/1 passing (`StealthGame.Round.StaleEpochItem`)
**Notes**: Items and a grace timer submitted under epoch N then drained after the epoch advances are `Rejected_StaleEpoch`: no resolver call, a `Voided(Rejected_StaleEpoch)` entry each; a stale-epoch item from a stale connection reports StaleEpoch (epoch first); the current-epoch item executes. The current epoch comes from the injected `IQueueEpochSource` (production: the Roster `RoundEpoch` after `ResetAll`).
**Build/test**: see `production/qa/evidence/story-010-stale-epoch-item/full-suite.txt`
