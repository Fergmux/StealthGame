# Story 009: Final completion and win declaration

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-8, OC-10 final branch; Edge Cases on same-tick ordering; AC-OC-05, AC-OC-06 final rows, AC-OC-07)
**Requirement**: `TR-oc-005` (completion gate inside the hold-timer slot; `DeclareObjectiveWin`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision section 5.4)
**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging (single-assignment latch, `Rejected_Latched`, inline follow-ons inherit the key)
**ADR Decision Summary**: When a counted completion is the `Final`, the core appends `FinalCompleted(team, Key)` and calls `Round.DeclareObjectiveWin(team, Key)` **in the same slot**, with `CurrentKey()` as the key; nothing else. The final's completion is not written to the view (the round-end step presents it). A completion never cancels another pawn's hold. Items after the latch return `Rejected_Latched`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0002 MEDIUM; the declaration and ordering rules are plain C++ over the queue stub)
**Engine Notes**: None beyond the queue. Tie-breaking is by the Round key `(ServerTickIndex, Seq)` and, for two holds due in one frame, by `(DueTime, Begin key)` (ADR-0020 and GDD Edge Cases); equal integer-time due values resolve in one drain.

**Control Manifest Rules (this layer)**:
- Required: The final calls `DeclareObjectiveWin(Team, CurrentKey())` in the `HoldTimer(Complete)` slot — source: ADR-0020
- Forbidden: Never let a completion cancel another pawn's hold; never judge completion from an `OnActionCompleted` subscription — source: ADR-0020
- Forbidden: Never put the final before the team's preliminaries are complete in the view — source: ADR-0020
- Guardrail: win declaration O(1) inside the queue line — source: ADR-0020

The win is a single call. What makes it correct is everything around it: it happens inside the very slot that completed the hold (so no other queue item can interleave and the order of a same-tick strike on the tender is decided by the key, not by luck), a second tender at a completed site is never cancelled (a correlation tell if every tender at a court stopped on the same frame), and the latch makes the loser of a same-frame double final a no-effect tend that is still logged (so F3's tend counts and F5's `ObjectiveEndShare` do not under-count it).

---

## Acceptance Criteria

*From GDD `design/gdd/objectives-chain.md` (AC-OC-05, AC-OC-06 final rows, AC-OC-07, Edge Cases), scoped to this story:*

- [ ] **AC1** (AC-OC-06 [U, table-driven] `CompletionGate`, final rows): the same six-row table (human on an eligible team; AI-driven hand-over; Unresponsive; Disconnected; Informant; Active on a team for which the final is not `Live`) applied to a **final** `HoldTimer(Complete)` slot yields `DeclareObjectiveWin` exactly once for row 1 and 0 times for rows 2-6, with site state and team views unchanged for rows 2-6.
- [ ] **AC2** (AC-OC-07 [U] `FollowOnInSlot`): **GIVEN** a stub queue **WHEN** a counted final completes **THEN** `DeclareObjectiveWin` is called with the `HoldTimer(Complete)` key inside that slot and no other item interleaves; `FinalCompleted(team, Key)` is logged; no team-view write contains the final.
- [ ] **AC3** (AC-OC-05 [U] `NoStackNoCancel`): **GIVEN** two teammates holding at their live site 0.5 s apart **WHEN** the first completes **THEN** the stage counts once, the second hold is not cancelled, completes at its own `D` and changes nothing; no `TargetInvalid` is emitted.
- [ ] **AC4** (GDD Edge Cases [U] `FinalSameFrameOrdering`): Complete before a strike on the tender in one tick counts (the strike returns `Rejected_Latched` for a final); a strike first cancels the hold (`PawnRemoved`/`Illegal`, no completion); both teams' final holds due in one frame are stamped by `(DueTime, Begin key)`, the first latches `ObjectiveWin` and the second returns `Rejected_Latched` and is logged `TendCompleted(..., NoEffect)`; a completer killed later in the same drain does not reverse the win (the kill item returns `Rejected_Latched`).

---

## Implementation Notes

*Derived from ADR-0020 Decision section 5.4 and the GDD Edge Cases:*

This story adds the `Final` branch to the gate built in story 008: after a counted completion of the `Final` row, append `FinalCompleted(team, Key)` to the Round Event Log and call `IRoundObjectiveHooks::DeclareObjectiveWin(Team, CurrentKey())`. No view write, no release scheduled (the final is never counted in `RivalPrelimCount`), no further effect. After the latch the queue voids pending items; `Dropped` is only `WrongPhase` or `StaleEpoch` (Round).

No-stack and no-cancel (OC-8): a completion releases only its own claim (story 006) and never cancels another pawn's hold; a second completion at a `Completed` site finds the site `Completed` and has no effect, with no log entry for the stage (gate (e)); the second tender's card changes at once (view push) so they can release by choice, but nothing is forced. Two teammates tending do not tend faster.

Same-tick cases to assert with the stub queue: (a) Complete then strike on the tender: the lower key acts first, Complete is judged by the gate and counts, and the strike item returns `Rejected_Latched` (owner reply `NotNow`); (b) strike first: the hold cancels, no completion; (c) both teams' final holds due in the same frame: the first counted completion latches, the second returns `Rejected_Latched`, its hold ends by the normal blend, and Round's server-only `TendCompleted(ParticipantId, court, NoEffect)` entry is written for it (cross-review C-W35; Round adds the log rule, change request 4); (d) the final completes and its completer is killed later in the same drain: the earlier win stands.

Prototype replaced: the final in the prototype ended the round from `AStealthGameMode::OnObjectiveCompleted` after client-triggered state; network test D2 (full chain on a generated city) is the behavioural reference. Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-008-completion-gate-and-stage-effects.md`: gate (a)-(e) and the stage branch.
- `story-017-stall-metrics-and-tend-posterior.md`: F5 handling of a latch-rejected final in the metrics.
- `story-021-pie2-end-to-end-cacao-chain.md`: the networked chain to `ObjectiveWin`.
- Round epic: the latch, `DeclareObjectiveWin` itself and the round-end step.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesCore.cpp` (the final branch)
- `Source/StealthGameplay/Private/Tests/Objectives/CompletionGateFinalTest.cpp` (class `FObjectivesCompletionGateFinalTest`, name `StealthGame.Objectives.CompletionGateFinal`)
- `Source/StealthGameplay/Private/Tests/Objectives/FollowOnInSlotTest.cpp` (`StealthGame.Objectives.FollowOnInSlot`)
- `Source/StealthGameplay/Private/Tests/Objectives/NoStackNoCancelTest.cpp` (`StealthGame.Objectives.NoStackNoCancel`)
- `Source/StealthGameplay/Private/Tests/Objectives/FinalSameFrameOrderingTest.cpp` (`StealthGame.Objectives.FinalSameFrameOrdering`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`CompletionGate` final rows): six-row final table
  - Given: stub Round queue; both teams' finals Live as needed; rows (1) Active+Connected+human eligible; (2) AI-driven; (3) Unresponsive; (4) Disconnected; (5) Informant; (6) Active on a team whose final is not Live
  - When: a final `HoldTimer(Complete)` slot executes per row (fresh fixture)
  - Then: `DeclareObjectiveWin` called exactly once for row 1, 0 times for rows 2-6; site state and team views unchanged for rows 2-6
  - Edge cases: row 1 completing twice declares once; table is order independent.
- **AC-2** (`FollowOnInSlot`): win declared inside the slot
  - Given: stub queue recording items and keys
  - When: a counted final completes
  - Then: `DeclareObjectiveWin` carries the `HoldTimer(Complete)` key inside that slot with no other item interleaved; `FinalCompleted(team, Key)` logged; no team view write contains the final
  - Edge cases: another item enqueued before the slot still runs after; final id absent from every pushed view.
- **AC-3** (`NoStackNoCancel`): teammates holding together
  - Given: two teammates holding at their live site, Begin 0.5 s apart; fake clock; D = `ObjectiveInteractSec`
  - When: the first completes at its D, then the second at its own D (0.5 s later)
  - Then: the stage counts once; the second hold is not cancelled and completes at its own D with no effect; no `TargetInvalid` emitted
  - Edge cases: Begin gap exactly 0 (same tick, order by Begin key); second tender cancels voluntarily before D.
- **AC-4** (`FinalSameFrameOrdering`): same-slot ordering
  - Given: stub queue; strike stub that returns `Rejected_Latched` against a latched final; both teams' final holds due in one frame
  - When: (a) Complete then strike on the tender in one tick; (b) strike then Complete; (c) both final holds due the same frame; (d) completer killed later in the same drain
  - Then: (a) completion counts; (b) hold cancelled, no win; (c) ordered by (DueTime, Begin key), first latches `ObjectiveWin`, second returns `Rejected_Latched` and logs `TendCompleted(..., NoEffect)`; (d) win not reversed
  - Edge cases: equal DueTime tie broken by Begin key; kill arriving before the drain finishes versus after.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-006-fixture-claims-and-interact-target-owner.md`, `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`; cross-epic: round-match-framework (`DeclareObjectiveWin`, latch, `Rejected_Latched`; stories being written in parallel), `production/epics/input-interaction-framework/story-050-hold-timer-key-order.md`
- Unlocks: `production/epics/objectives-chain/story-017-stall-metrics-and-tend-posterior.md`, `production/epics/objectives-chain/story-018-stall-pressure-and-cross-system-checks.md`, `production/epics/objectives-chain/story-021-pie2-end-to-end-cacao-chain.md`
