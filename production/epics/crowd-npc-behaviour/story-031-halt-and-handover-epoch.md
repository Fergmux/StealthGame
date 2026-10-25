# Story 031: Halt at the round-end step and handover epoch discipline

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-009 = Dormant until OnRoundActive; Halt in the round-end step.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0002 (latch, round-end step, step (f) calls `Halt()`, step (g) cancels frozen holds), ADR-0010 (stop-and-blend for every live pawn, hand-over pass, control epoch)
**ADR Decision Summary**: `ICrowdControl::Halt()` is called by Round inside the round-end step; synchronously the Director latches every sink (only `Move(0)` and `Look` pass), clears every AI-driven pawn's held `Move`, stops every tree directly (not by event alone) and stops the ambient schedule, replacements and reactions. Halt does not end holds and does not brake pawns: Round's step (g) and ADR-0010 do that for every pawn kind. Every handover bumps the control epoch at commit, so every sink holding the old epoch is dead from that instant; possession happens at the start of the next server frame, never during a traversal.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: An event sent to a stopped StateTree is dropped (E15), so Halt and handover are direct calls that latch the sink first and then stop or switch the tree. The GDD's AC-CNB-22 says Crowd "ends its holds"; ADR-0016 section 7 (accepted later) moves hold cancellation to Round's step (g) and braking to ADR-0010, so the ADR wins and the story's first criterion tests the end state, not who performs it.

**Why this story**: At round end nothing may move except by the same braking every pawn gets, and a late async callback from a previous driver must never move a pawn that has changed hands. Both are the sort of one-frame race that appears as an observable tell. Read GDD CNB-R12, R10 item 5 and AC-CNB-22, AC-CNB-38.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: Halt and handover are direct calls that latch the sink and stop or switch the tree; Halt latches every sink synchronously inside the round-end step before any further crowd tick; every sink call checks the control epoch (ADR-0016)
- Forbidden: relying on a StateTree event alone for Halt or handover; Halt ending holds or braking pawns; `Possess`/`UnPossess` from Crowd code (ADR-0016)
- Guardrail: Halt is O(N) over AI-driven pawns inside the round-end step

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-22 and AC-CNB-38 and ADR-0016 section 7, scoped to this story:*

- [ ] (AC-CNB-22) GIVEN 50 pawns moving, running, tending and traversing, WHEN `Ended` latches, THEN in Round's round-end step (for a strike win, the deciding strike's death write; before it no AI-driven pawn changes speed or ends a hold because of the latch) every AI-driven pawn's sink is latched and `Move(0)` issued, every pawn reaches idle within `RunStopSec` plus the traversal remainder with no deceleration above the baseline (holds are cancelled by Round's step and braking is ADR-0010's; Crowd's Halt only latches), and no emergence, absorption or reaction occurs afterwards.
- [ ] (AC-CNB-38a) GIVEN 100 takeovers and 100 hand-backs with injected late path callbacks and debounced Ends, THEN 0 old-epoch commands reach any pawn and every possession change happens at a frame start, never during a traversal.
- [ ] (AC-CNB-38b) GIVEN 50 Halts with commands pending, THEN 0 non-`Move(0)` motion commands reach any pawn after Halt, and Halt latches every sink synchronously inside the round-end step before any further crowd tick.

---

## Implementation Notes

*Derived from ADR-0016 section 7:*

- Halt in one call: (1) set the Halt latch on every sink, (2) clear every AI-driven pawn's held `Move` intent (`Move(0)`), (3) stop every tree directly (terminal `Halted`; the per-frame look keeps the head idle), (4) stop the ambient schedule (its `TimerFired(CrowdSchedule)` items are already `Rejected_Latched`), replacements and reactions, abandoning open cycles without emergence. A pawn released during `Ended` stays halted.
- Between the latch and the round-end step the crowd runs unchanged; its discrete commands resolve `Rejected_Latched` silently (null sink), locomotion continues (ADR-0002 section 8.1).
- Handover order (CNB-R10.5): Round or Penalty calls `RequestHandOver` (bumps the epoch at commit in the drain); ADR-0010's hand-over pass possesses at the start of the next frame, before the Director's tick, so the new controller is driven in the same frame. Until the new driver's first command or `HandoverHoldSec`, ADR-0010 holds the last `Move`.
- Debounced Ends and async path callbacks are injected by the harness; every one must be dropped by the epoch gate (story 005).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype's end-of-round handling in `Source/StealthGame/Private/StealthGameMode.cpp` (objective win sets a flag; NPCs keep wandering). Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: the sink epoch and halt filter types
- Story 028 and 029 and 030: the driver registrations
- The Round epoch (`production/epics/round-match-framework/EPIC.md`) and the queue: step ordering, hold cancels, braking

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdHalt.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdHaltTest.cpp`
- `Source/StealthTestHarness/Private/CrowdHandoverEpochTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.EndedHold`
  - Given: 50 pawns moving, running, tending and traversing
  - When: `Ended` latches
  - Then: in Round's round-end step every AI-driven pawn's sink is latched and `Move(0)` issued; every pawn reaches idle within `RunStopSec` plus traversal remainder with no deceleration above baseline; no emergence, absorption or reaction afterwards; nothing changes before the step
  - Edge cases: a pawn mid-traversal at latch
- **AC-2**: `StealthGame.Crowd.HandoverEpoch`
  - Given: 100 takeovers and 100 hand-backs with injected late path callbacks and debounced Ends
  - When: the sink is instrumented
  - Then: 0 old-epoch commands reach any pawn; every possession change at a frame start, never during a traversal
  - Edge cases: a debounced End landing in a traversal frame
- **AC-3**: `StealthGame.Crowd.HaltLatchSynchronous`
  - Given: 50 Halts with commands pending
  - When: commands are logged after each Halt
  - Then: 0 non-`Move(0)` motion commands reach any pawn; Halt latches every sink synchronously inside the round-end step before any further crowd tick
  - Edge cases: a command queued in the same frame as Halt

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdHaltTest.cpp` plus `Source/StealthTestHarness/Private/CrowdHandoverEpochTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-005-command-sinks-epoch-and-halt-filter.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`, `production/epics/crowd-npc-behaviour/story-028-bound-pawn-restricted-driver.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-038-performance-pass-50-60-character.md`
