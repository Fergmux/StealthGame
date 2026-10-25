# Story 003: Attempt loop — timeout, total budget and reseed

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-004`, `TR-lsc-005`
*(At write time: TR-lsc-004 = Server nav build, Validate and Select within attempt/total budgets; <= 33 ms slices; TR-lsc-005 = Reseed and graybox fallback travel)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: An injected `ILayoutClock` cancels each attempt at `LayoutAttemptTimeoutSec` (7 s) and the whole sequence at `LayoutTotalBudgetSec` (45 s); a Generated provider is reseeded from `PublicSubSeed(Layout, LayoutAttempt)` up to `LayoutMaxReseedAttemptsCount` (5) times with the round `Seed` unchanged; a cancelled attempt tears down everything it created.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Logic runs headless in `FLayoutServiceCore` against stub providers and an injected clock; the seed derivation itself (`PublicSubSeed`) is Session & Travel's F5.

**Why this story**: Timeouts and reseeds define when a layout is accepted, retried or abandoned. Read first: R4 item 3, R8 items 1-2, AC-6, AC-41; TDC invariant `(1 + LayoutMaxReseedAttemptsCount) x LayoutAttemptTimeoutSec <= LayoutTotalBudgetSec`.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use the ADR-0006 shapes and stream tags `LSC_SITE`, `LSC_PLCE`, `LSC_ZONE`, `LSC_EMRG`, `LSC_RSPN` (ADR-0014)
- Forbidden: Providers must never call `GetServerSubSeed` (ADR-0014 guideline 2)
- Guardrail: <= 45 s worst case including reseeds (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-6 and AC-41 (revised after QL-STORY-READY: tick tolerance is cumulative, publish counter named), scoped to this story:*

- [ ] GIVEN a provider stub that takes 8 s, THEN attempt 0 fails with `Timeout` at 7.0 s +/- 1 tick; GIVEN the defaults and a stub that always times out, THEN attempt k+1 starts on the same tick attempt k times out and R8.3 is reached at 42.0 s +/- 1 tick total; GIVEN a fixture bypassing the invariant (7 s timeout, 10 reseeds), THEN R8.3 is reached at 45.0 s +/- 1 tick total and the attempt in progress is cancelled (`CancelAttempt` called once, late results discarded) (`Handshake.AttemptTimeout`, `Handshake.TotalBudget`)
- [ ] GIVEN the TDC invariant validator, THEN (timeout 8 s, reseeds 5) is rejected (6 x 8 = 48 > 45), (7.5 s, 5) is accepted (6 x 7.5 = 45), and the rejection names `LayoutAttemptTimeoutSec`, `LayoutMaxReseedAttemptsCount` and `LayoutTotalBudgetSec` (`Handshake.TdcInvariantRejects`)
- [ ] GIVEN an always-failing Generated stub, THEN `LayoutAttempt` runs 0 to 5 on `PublicSubSeed(Layout, k)` with `Seed` unchanged, 6 `FLayoutAttemptRecord`s carry their failed ids, and the stub transport's publish-call counter is 0; GIVEN a stub that fails twice then passes, THEN `LayoutFallbackLevel` is 1 and the publish counter is 1; first-attempt acceptance gives level 0 (`Fallback.ReseedSequence`)

---

## Implementation Notes

*Derived from ADR-0014 sections 1 (Watchdog) and 3.4 and GDD R8:*

- `ILayoutClock` supplies server time; the watchdog cancels at the attempt and total limits; the Generator's own candidate loop is bounded by work units, never wall clock (R-G5.5), so the watchdog bounds only the attempt.
- `FLayoutAttemptRecord { Attempt k, PublicSubSeed(Layout, k), Result, FailedPredicateIds[], MetricsSnapshot }`; nothing about failed attempts is replicated.
- On `AttemptFailed(k)`: `TearDownAttempt`, discard the socket set, then `Building(k+1)` only if Generated, k < 5 and budget is left; otherwise R8.3 (story 004/005). The graybox never reseeds.
- The invariant validator belongs to Tuning Data & Config (cross-epic); this story supplies the keys' use and a test of the validator's contract through a TDC stub or the real validator when present. A late result after `CancelAttempt` is ignored.
- Edge: a stub finishing at 6.9 s passes; a result arriving after cancel is ignored; tick size is pinned in the fixture; failing on the sixth attempt goes to R8.3 (five reseeds maximum).

---

## Out of Scope

- Story 004: abort dispositions; story 005: fallback travel; story 012: navmesh wait
- The seed derivation (`production/epics/session-travel/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/LayoutServiceCore.cpp` (attempt loop)
- `Source/StealthGameplay/Private/Tests/Layout/AttemptLoopTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Handshake.AttemptTimeout`, `Handshake.TotalBudget`
  - Given: injected clock; (a) stub taking 8 s; (b) defaults (7 s, 5 reseeds, 45 s) with an always-timeout stub; (c) fixture bypassing the invariant (7 s, 10 reseeds).
  - When: fixed-step ticks run to the outcome.
  - Then: (a) attempt 0 Timeout at 7.0 s +/-1 tick; (b) attempt k+1 starts on the tick attempt k times out, R8.3 at 42.0 s +/-1 tick total; (c) R8.3 at 45.0 s +/-1 tick total, the 7th attempt (started at 42.0 s) cancelled with `CancelAttempt` called once and late results discarded.
  - Edge cases: stub finishing at 6.9 s passes; result arriving after cancel is ignored; tick size pinned.
- **AC-2** `Handshake.TdcInvariantRejects`
  - Given: the TDC invariant validator.
  - When: fed (8 s, 5), (7.5 s, 5), (7.51 s, 5).
  - Then: (8, 5) rejected (48 > 45); (7.5, 5) accepted (45 <= 45); (7.51, 5) rejected (45.06); each rejection names the three keys.
  - Edge cases: budget at its safe-range edges 45 and 60.
- **AC-3** `Fallback.ReseedSequence`
  - Given: always-failing Generated stub; then a stub failing twice; then first-attempt pass.
  - When: attempts run.
  - Then: LayoutAttempt 0..5 on PublicSubSeed(Layout, k), Seed unchanged, 6 `FLayoutAttemptRecord`s with failed ids, publish counter 0; fail-twice gives level 1 and publish counter 1; first-attempt gives level 0.
  - Edge cases: failing on the 6th attempt goes to R8.3.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Layout/AttemptLoopTest.cpp` — `StealthGame.Layout.Handshake.AttemptTimeout`, `.TotalBudget`, `.TdcInvariantRejects`, `StealthGame.Layout.Fallback.ReseedSequence` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-002-service-state-machine-and-time-slicing.md`
- Cross-epic: `production/epics/tuning-data-config/EPIC.md` (invariant validator), `production/epics/session-travel/EPIC.md` (`PublicSubSeed`)
- Unlocks: `production/epics/level-socket-contract/story-004-abort-paths-and-event-log-header.md`, `production/epics/level-socket-contract/story-005-fallback-travel-and-host-loss.md`, `production/epics/level-socket-contract/story-012-navmesh-lifecycle-and-freeze.md`
