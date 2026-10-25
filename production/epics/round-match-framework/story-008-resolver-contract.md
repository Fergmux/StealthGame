# Story 008: Resolvers return Voided/in-slot/deferred outcomes with the specified keys and Seq consumption

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-003` (drain once per frame in `TG_PostPhysics`; due-time heap; resolvers per item type; inline follow-ons; Roster events flushed before the next item). GDD AC 46. Rules: R13, R14.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**ADR Decision Summary**: One total-order queue per round keyed (ServerTickIndex, Seq), per-source permuted mailboxes, due-time heap, drained once per frame by a custom `TG_PostPhysics` tick function, typed staged writes, single-assignment latch storage with Round's rule injected, the R9 two-step round end, and the RTT-compensated client `EstimatedServerNow()`. This story applies ADR-0002 section 1 and section 5: `Compare`, `GetKey` and `CurrentKey` are the only ordering primitives; a resolver re-validates against live state and returns `Executed | Voided(reason)`; a `Voided` item changes nothing and emits nothing; an outcome inherits its resolving slot's key, and a deferred outcome is a new item with its own later key and a logged `CauseKey`.
**ADR Version**: ADR-0002 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: None touched. The contract is in the plain core and the stub resolvers of story 004. `IQueueResolver` is a `UINTERFACE` at the facade only (E7, verified: `TWeakInterfacePtr` requires one); tests call the plain core with raw pointers. ADR-0002 Verification Required items 1 to 3 are not touched.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Resolvers re-validate against live state at the slot, run follow-ons inline, and never re-submit or re-stamp an outcome; a deferred outcome is a new item with its own key and a logged `CauseKey` (ADR-0002)
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager`; systems order events only with `Compare`/`GetKey`/`CurrentKey` (ADR-0002)
- Guardrail: Queue drain + staging + teardown + Roster flush at most 0.5 ms listen-host game thread (ADR-0002)

**Why this story**: Assassination, Objectives, IIF, Penalty and Crowd all plug into the queue by implementing a resolver, and the order of "Prepare versus Attack in one tick" is decided by comparing keys (R14). If the contract is loose (an outcome that re-stamps itself, a `Voided` item that still changes state, a key that moves), same-tick races stop being decidable. This story fixes the contract with stubs before any real resolver exists. It depends on stories 001 and 004. Exit A.

**Prototype disposition**: none (new Foundation code). The prototype has no resolver contract and resolves in first-come order.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` Acceptance Criteria no. 46, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a resolver returning `Voided`, THEN no state changes, nothing is emitted and one `Voided` entry is logged; an in-slot outcome consumes no `Seq` and carries the resolving key; a deferred outcome has a later key and `CauseKey`; `GetKey`, `Compare` and `CurrentKey` return the expected keys for a fixture sequence. `StealthGame.Round.ResolverContract` [U]

---

## Implementation Notes

*Derived from ADR-0002 sections 1 and 5, Key Interfaces, and GDD R13 and R14:*

- `EResolveResult { Executed, Voided }` and `EVoidReason { Rejected_Latched, Rejected_StaleEpoch, Dropped_StaleConnection, Revalidation }` are declared in `StealthFoundation/Public/Server/StealthQueuePayloads.h`; the owner-specific code travels in the log entry. `IQueueResolver::Resolve(const FQueueItem&, FQueueKey)` is the only entry.
- `Voided`: the queue changes no state and emits nothing on the resolver's behalf, and appends exactly one `Voided(type, reason)` entry through `IRoundLogWriter`. `Voided` entries are rate-limited per source (story 015); the limiter is not built here, but the single-entry behaviour must be compatible with it.
- In-slot outcome (R14.2): the outcome executes inside the resolving command's slot, inherits that command's key, and consumes no `Seq`; it is not re-submitted and not re-stamped.
- Deferred outcome (R14.3): a new item with its own later key, recording `CauseKey` for the log; it must not reorder items by `CauseKey`. MVP note: ADR-0002 section 1 says deferred outcomes are not used in MVP and adding one is an amendment that adds a `DeferredOutcome` type, so the test drives a deferred outcome through a stub that submits a fresh item from inside a slot (the generic submit path), not through a production type. Open: confirm against ADR-0002 section 1 and GDD R14 (3) the exact shape the deferred fixture takes in MVP.
- Key primitives: `GetKey(Item)`, `Compare(KeyA, KeyB)` (lexicographic over `(ServerTickIndex, Seq)`) and `CurrentKey()` (valid inside a slot). The fixture sequence of expected keys lives in the constants file (story 005), not inline.
- `IRoundLogWriter::Append(const FRoundLogEntry&)` (game thread only, from inside the producing slot, in key order; ADR-0002 section 8.9) is needed to observe the `Voided` entry. This story declares the minimal writer interface and an in-memory recording implementation sufficient for this assertion; story 011 builds the real `FRoundEventLog` store and header. Open: confirm against ADR-0002 section 8.9 that the writer interface is introduced here rather than in story 011 (the dependency table has 011 depending on 008).
- Never route log content through `UE_LOG`/`UE_LOGF`, `ensureMsgf` text or trace channels.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 009 (`production/epics/round-match-framework/story-009-follow-ons-inline.md`): inline follow-on depth, flush barrier and `CascadeOverflow`.
- Story 010 (`production/epics/round-match-framework/story-010-stale-epoch-item.md`): `Rejected_StaleEpoch`.
- Story 011 (`production/epics/round-match-framework/story-011-event-log-content.md`): the Event Log header, ring and read gating.
- Story 015 (`production/epics/round-match-framework/story-015-voided-rate-limit.md`): the one-per-second-per-source limit on `Voided` entries.
- Story 017 (`production/epics/round-match-framework/story-017-latched-final-complete-logged.md`): `Rejected_Latched` and its `TendCompleted` log entry.
- The real resolvers (IIF, Objectives, Assassination and the others) in their own epics.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/StealthQueuePayloads.h` (extended; `EResolveResult`, `EVoidReason`)
- `Source/StealthFoundation/Public/Server/Queue/StealthResolutionQueueCore.h` and `Source/StealthFoundation/Private/Queue/StealthResolutionQueueCore.cpp` (extended; `Compare`, `GetKey`, `CurrentKey`, outcome handling)
- `Source/StealthFoundation/Public/Server/EventLog/StealthRoundLogWriter.h` (new; minimal `IRoundLogWriter`, extended by story 011)
- `Source/StealthFoundation/Private/Tests/Queue/ResolverContractTest.cpp` (new; class `FRoundResolverContractTest`)
- Tests live under the engine test roots (`tests/README.md`), never under `tests/`. `/dev-story` replaces this planned list with the real one. `Source/StealthFoundation` does not exist yet (tuning-data-config epic creates the skeleton); write under `Source/StealthGame/` until it lands.

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a resolver returning `Voided`, THEN no state changes, nothing is emitted and one `Voided` entry is logged; an in-slot outcome consumes no `Seq` and carries the resolving key; a deferred outcome has a later key and `CauseKey`; `GetKey`, `Compare` and `CurrentKey` return the expected keys for a fixture sequence.
  - Given: stub resolvers and a fixture sequence from the constants file.
  - When: the resolver returns `Voided`, then an in-slot outcome, then a deferred outcome.
  - Then: `Voided` changes no state, emits nothing and logs exactly one `Voided` entry.
  - Then: the in-slot outcome consumes no `Seq` and carries the resolving key.
  - Then: the deferred outcome has a later key and a `CauseKey`.
  - Then: `GetKey`, `Compare` and `CurrentKey` return the fixture's expected keys.
  - Edge cases: the fixture sequence values live in the constants file (story 005); `Compare` is checked for equal and reversed keys.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Queue/ResolverContractTest.cpp` — automation test `StealthGame.Round.ResolverContract` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundResolverContractTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-001-test-seam-fake-clock.md`, `production/epics/round-match-framework/story-004-test-seam-stub-resolvers.md`
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-009-follow-ons-inline.md`, `production/epics/round-match-framework/story-010-stale-epoch-item.md`, `production/epics/round-match-framework/story-011-event-log-content.md`, `production/epics/round-match-framework/story-017-latched-final-complete-logged.md`, `production/epics/round-match-framework/story-043-no-draw-property.md`, `production/epics/round-match-framework/story-045-same-tick-final-vs-elimination.md`, `production/epics/round-match-framework/story-050-brace-expiry-ordered.md`, `production/epics/round-match-framework/story-051-no-timer-ends-active-round.md`
