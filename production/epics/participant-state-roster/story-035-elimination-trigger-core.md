# Story 035: The elimination trigger fires only for a strike-out win and never for disconnect or empty sides

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also governed by**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: ADR-0005 keeps the Roster's records, bindings and `EventSeq` in the plain-C++ `FRosterCore`, whose state changes are applied immediately and announced through events carrying their own count snapshots. ADR-0002 owns the terminal latch and the two-step round end: Round's `TryTerminate` is the only caller of `Latch`, the win decision is Round's, and a win is evaluated once per applied event in `EventSeq` order. The Roster's part is to supply correct payloads (`ActiveCountAfter`) and a phase-aware `IsTeamEliminated`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005 uses no new post-cutoff API (`DispatchPostLogin` deprecated in 5.6; `UE_LOGF`); verification items are spikes S3, S4, S5 (not yet reported) and do not gate this pure-core story. ADR-0002 (Risk MEDIUM) has no post-cutoff APIs; its verification items (drain-spawn spike, estimator net emulation, drain once per frame on the listen host) are Round-side and not exercised here.

**Control Manifest Rules (this layer)**:
- Required: Only Round's `TryTerminate` calls `Latch`; the latch step replicates and presents nothing — source: ADR-0002
- Required: Resolvers re-validate against live state at the slot; listeners act on applied events only (payload counts are authoritative for ordering) — source: ADR-0002
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Forbidden: Never end the round in one step at the latch, or through an `OnRoundEnded` subscriber chain — source: ADR-0002
- Forbidden: Never order events with a feature-local clock, timestamp, tick-function order or `FTimerManager` — source: ADR-0002
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread — source: ADR-0002

This story delivers the Roster side of the elimination rule F4: the trigger predicate evaluated once per applied single-participant transition event, and the phase-aware `IsTeamEliminated`. It exists so that a strike-out ends the round but a network drop, a host kick or an empty opposing side never does, and so that no draw is possible. It realises GDD F4 and R8/R12 payload semantics, and re-expresses the prototype's `FStealthRules::EvaluateEliminationWinner` in the pure core.

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-50** [Core] `StealthGame.Roster.EliminationTriggerCore`: GIVEN payloads from the Roster core, WHEN F4's trigger is evaluated in phase `Active`, THEN:
  - (a) the last Active participant's `StrikeLimit` with the opponent's `ActiveCount` 3 gives a win for the opponent;
  - (b) the same with Reason `Disconnect` gives no win;
  - (c) a 1v0 strike-out gives no win, because the winner side is empty;
  - (d) two same-tick events driving both teams to 0 give a win for the first event only, because the second event's opposing `ActiveCountAfter` is 0;
  - (e) `IsTeamEliminated` is false for both teams after `EndRoundAll`.

---

## Implementation Notes

*Derived from ADR-0005 section 2, ADR-0002 section 8 and GDD F4, R8, R12:*

- **Predicate (F4).** `EliminationTrigger(e) = e.Applied AND e.Old = Active AND e.New = Informant AND e.Reason in {Assassinated, ReversedAgainst, StrikeLimit} AND Phase = Active AND NOT EndedLatched AND e.ActiveCountAfter[e.Team] = 0 AND RosterCount(e.Team) >= 1 AND e.ActiveCountAfter[1 - e.Team] >= 1`, yielding `EliminationWin(1 - e.Team)`. Implement it as a pure function over the `OnParticipantStateChanged` payload (`Id, Team, Old, New, Reason, EventSeq, RoundEpoch, ActiveCountAfter[2], InformantCountAfter[2]`, GDD R12) plus injected `Phase` and `EndedLatched`. It is edge-triggered: evaluated once per applied event in `EventSeq` order and never re-evaluated from live state. Round calls it (story 052); the Roster only supplies the payload and the function.
- **`IsTeamEliminated(T) = (RosterCount(T) >= 1) AND (ActiveCount(T) = 0)`**, defined only in Round phase `Active`; in every other phase it returns false. So an empty team (`RosterCount = 0`) is never eliminated, and after `EndRoundAll` (phase `Ended`) neither team reports eliminated.
- **Reason partition.** `Disconnect` never triggers (a network drop is not a decision, P3), and neither does a host kick, which reaches the Roster as a disconnect. Batch events (`OnRosterRoundEnded`, `OnRosterReset`) are not single transitions and are never evaluated, even though they drive both counts to 0 (covered by (e) here and by story 052 with Round).
- **No draw is possible.** Each single transition changes one participant on one team, so one event drives at most one team to 0. For two same-tick events that each drive their own team to 0, the second event's `ActiveCountAfter[1 - e.Team]` is 0, so it awards nothing; the first has already latched a valid win. Evaluate each payload with `EndedLatched = false` so the count rule, not the latch, is what rejects the second (case d).
- **Phase seam.** The GDD's injected-dependency list has no Round-phase input, yet F4 and `IsTeamEliminated` need "phase Active". Add one minimal consumer-owned seam in `StealthRosterDeps.h` (a round-phase provider interface, test-settable) shared with stories 012 and 025; whichever of those lands first creates it. ADR-0007 section 4: lower layers declare the interfaces they consume. Do not take a dependency on Round types.
- **Open point:** the phase seam is not named in the GDD dependency list (qa-lead cross-cutting gap). The seam name and signature are an implementer decision to record in the PR; flag to the lead-programmer.
- **Prototype disposition:** re-expresses `FStealthRules::EvaluateEliminationWinner` (`Source/StealthGame/Private/StealthRules.cpp`, with its current tests in `Source/StealthGame/Private/Tests/StealthRulesTests.cpp`: team 1 wins when team 0 has no active players, no winner while both teams have an active player, a single-team session never ends by elimination) in the pure core and re-tests those cases against the new payload-based predicate, adding the Reason, phase and latch conditions the prototype lacks. The prototype function is reference logic, not promoted (StealthRules is replaced per the manifest).
- **Header naming (ADR-0007).** Stealth prefix, project-unique names: `StealthRosterCore.h`, `StealthRosterDeps.h`, `StealthRosterTypes.h`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 052 (Round declares an elimination win exactly once from the transition payload): the Round-side consumer and its stub.
- Story 053 (Two last-active attacks in one tick latch exactly one elimination win): queue-order latching with Round and Assassination stubs.
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): owns `RequestTransition` and the payload's production.
- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event) and Story 023 (EndRoundAll ends all records once and is idempotent): own the batch calls whose events are never evaluated.
- Story 034 (A scripted 200-step sequence keeps F1-F3 and I1-I3 equal to a brute-force recount): owns the count invariants.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (add `EvaluateEliminationTrigger`, phase-aware `IsTeamEliminated`)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp`
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (round-phase provider seam, shared with stories 012 and 025)
- `Source/StealthFoundation/Public/View/StealthRosterTypes.h` (payload and `FEliminationWin` result types, if not already present)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterEliminationTriggerCoreTest.cpp` (class `FRosterEliminationTriggerCoreTest`, test name `StealthGame.Roster.EliminationTriggerCore`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs (Logic):**

- **AC-1**: the elimination trigger fires only for a strike-out win.
  - Given: payloads from the core, Round phase Active through the phase seam, EndedLatched = false per evaluation.
  - When: F4's trigger evaluated.
  - Then: (a) last Active's StrikeLimit with opponent ActiveCountAfter 3 gives a win for the opponent. (b) Same with Disconnect gives no win. (c) A 1v0 strike-out gives no win. (d) Two same-tick events driving both teams to 0 give a win for the first event only. (e) IsTeamEliminated false for both teams after EndRoundAll.
  - Edge cases: in (d) evaluate each payload with the latch off so the count rule rejects the second.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterEliminationTriggerCoreTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `production/epics/participant-state-roster/story-022-reset-all-batch.md`, `production/epics/participant-state-roster/story-023-end-round-all-batch-idempotent.md`
- Unlocks: `production/epics/participant-state-roster/story-052-elimination-trigger-from-payload.md`, `production/epics/participant-state-roster/story-053-no-draw-same-tick.md`
