# Story 003: The per-pawn Concealment state machine and per-bush occupancy record exist as headless core

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R2, CON-R3 occupancy, States and Transitions, AC-CON-30)
**Requirement**: `TR-con-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§1, §3, §7)
**ADR Decision Summary**: One non-replicated `UConcealmentComponent` on every pawn (same class and flags, never replicated) and one server-only `UConcealmentService` over a plain C++ `FConcealmentCore` hold the `EConcealState {Outside, Entering, Concealed, Exiting, Emerging}`, `BushId`, `TransitionGeneration`, occupancy records per `BushId` with commit sequence ids, and the `IsInBushOrTransition` predicate.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; this story is plain C++ with no post-cutoff API)
**Engine Notes**: The core is headless-testable plain C++; only the thin `UConcealmentComponent` and `UConcealmentService` shells touch UObject. Class-parity of the pawn's components is scanned by SCL story-004, not here.

**Control Manifest Rules (this layer)**:
- Required: `UConcealmentComponent` exists on every pawn with identical class and flags and never replicates — source: ADR-0017
- Forbidden: Never add a concealment field to the pawn's replicated set; never replicate a concealed flag — source: ADR-0017
- Guardrail: Negligible server CPU inside the 1.0 ms misc line

This is the foundation every other story builds on: the legal state moves, the occupancy record that blind strikes read, and the takeover and ping predicate.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (States and Transitions, CON-R2, AC-CON-30), scoped to this story:*

- [ ] **AC1** (derived, States and Transitions, CON-R2): **GIVEN** `FConcealmentCore` **WHEN** transitions are driven **THEN** the only legal moves are Outside→Entering (HideEnter execution), Entering→Concealed (commit), Entering→Outside (killed before commit, no record), Concealed→Exiting, Concealed→Emerging, Concealed→removed (kill), Exiting→Outside, Emerging→Outside; every other move is refused and reported to the fault sink; each accepted transition bumps `TransitionGeneration`; the replicated-property allowlist has zero rows for `UConcealmentComponent`.
- [ ] **AC2** (derived, CON-R2 record, CON-R3): **GIVEN** pawns committing, exiting and emerging **WHEN** the record is read **THEN** it holds exactly the pawns in Concealed or in Exiting/Emerging before their leaf crossing, each with a strictly increasing `CommitSeq`; a pawn leaves it at the crossing, at the kill slot, or at reset; there is no capacity limit (0, 1, 4 and 20 occupants all accepted).
- [ ] **AC3** (AC-CON-30, [U] `StealthGame.Concealment.TakeoverEligibility`): **GIVEN** pawns in each of Outside, Entering, Concealed, Exiting and Emerging **THEN** `IsInBushOrTransition` is false only for Outside; the same predicate is the Pings consumer's (PNG-R2, PNG-R5 step 2, D33), so an Emerging pawn counts as concealed for the ping refusal until fully out.

---

## Implementation Notes

*Derived from ADR-0017 Decision §1, §3 and §7:*

`UConcealmentComponent` (`SetIsReplicatedByDefault(false)`) holds server-side `State`, `BushId`, `TransitionGeneration`, `TransitionStartServerTime`, `CommitSeq` and `LastExitCompleteServerTime`; none replicate (allowlist: zero rows). `UConcealmentService` (`UWorldSubsystem`) is a facade over `FConcealmentCore`, which holds the occupancy records per `BushId` (the set of pawns in `Concealed`, or `Exiting`/`Emerging` before their crossing, each with its commit sequence id). Every transition is atomic and carries a transition generation id; later timer items are dropped unless the pawn is alive and still in that transition (story 006). `IsInBushOrTransition(Pawn)` is true for every state except `Outside` (consumed by Crowd eligibility, Penalty/takeover IVP R9c and Pings). A kill in Entering gives Outside with no record; a kill in Exiting/Emerging is not in the AC1 move list, so the implementer confirms the intended record handling against CON-R9 (record leaves at the kill slot).

The `ICrowdPerceptionFilter` interface is story 009; the client latch is story 019.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-004-hide-enter-execution-and-commit.md` and `story-005-hide-exit-execution-and-blocked-side.md`: the resolvers that drive the moves.
- `story-006-round-phase-lifecycle-and-no-dwell.md`: Ended, Reset and stale-item dropping.
- `story-024-is-concealed-from-truth-table.md`: `IsConcealedFrom`.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentCore.h`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentService.h`
- `Source/StealthGameplay/Features/Concealment/Public/View/ConcealmentComponent.h`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentService.cpp`
- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentComponent.cpp`
- `Source/StealthGameplay/Private/Tests/Concealment/StateMachineTest.cpp` (class `FConcealmentStateMachineTest`, name `StealthGame.Concealment.StateMachine`)
- `Source/StealthGameplay/Private/Tests/Concealment/OccupancyRecordTest.cpp` (class `FConcealmentOccupancyRecordTest`, name `StealthGame.Concealment.OccupancyRecord`)
- `Source/StealthGameplay/Private/Tests/Concealment/TakeoverEligibilityTest.cpp` (class `FConcealmentTakeoverEligibilityTest`, name `StealthGame.Concealment.TakeoverEligibility`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: StateMachine
  - Given: `FConcealmentCore` in each state.
  - When: all 25 (from, to) moves are driven.
  - Then: exactly the legal moves listed are accepted; the others, including self-moves, are refused and reported to the fault sink; generation increases by 1 only on accepted moves; the allowlist has 0 rows for `UConcealmentComponent`.
  - Edge cases: a kill from Concealed removes the pawn; a kill in Entering gives Outside with no record.
- **AC2**: OccupancyRecord
  - Given: pawns commit, exit and emerge in a scripted order.
  - When: the record is read.
  - Then: it holds exactly the Concealed pawns plus Exiting/Emerging pawns before their crossing, with strictly increasing `CommitSeq`; pawns leave at the crossing, the kill slot or reset; 0, 1, 4, 20 occupants accepted.
  - Edge cases: a re-entering pawn gets a new, larger `CommitSeq`.
- **AC3**: TakeoverEligibility
  - Given: one pawn per state.
  - When: `IsInBushOrTransition` is evaluated.
  - Then: false only for Outside; the Pings seam calls the same symbol; Emerging counts as concealed.
  - Edge cases: the Pings-side refusal test is deferred until the Pings epic exists.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: the three tests above under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: IIF `production/epics/input-interaction-framework/story-002-pawn-command-core-skeleton.md` (component host)
- Unlocks: `story-004-hide-enter-execution-and-commit.md`, `story-005-hide-exit-execution-and-blocked-side.md`, `story-006-round-phase-lifecycle-and-no-dwell.md`, `story-014-attack-breaks-concealment.md`, `story-015-spot-zone-and-occupants.md`, `story-017-kill-while-concealed-server-rules.md`, `story-024-is-concealed-from-truth-table.md`
