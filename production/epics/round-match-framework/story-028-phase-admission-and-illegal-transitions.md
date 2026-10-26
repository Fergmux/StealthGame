# Story 028: Each phase pushes the specified admission mode and every illegal transition is refused

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (Five-phase machine in a game-instance store surviving travel); GDD AC 40; rules R4, R8, States and Transitions (phase table, transition table, "Illegal and asserted in tests").
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies ADR-0005 section 2: the five-phase machine lives in `FRoundPhaseCore` behind `URoundMatchStore`, and Round is the only caller of `SetAdmissionMode` (ADR-0005 Key Interfaces).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk; residual risk is behavioural, spikes S3 to S5)
**Engine Notes**: None touched. The phase core is plain C++ and `SetAdmissionMode(EAdmissionMode)` is called on a fake Session gateway (story 003), so no engine API, post-cutoff API or ADR-0005 verification item (S3, S4, S5) is exercised.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules, section Session and travel):
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005).
- Forbidden: Never store anything authoritative on a PlayerController, PlayerState, GameMode or GameState; never carry match state across travel on them (ADR-0005).
- Guardrail: Session bookkeeping stays inside the 0.5 ms line and travel load is bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005); the phase core adds no per-frame work.

**Prototype disposition**: Replaces the round state held in `Source/StealthGame/Private/StealthGameMode.cpp` and `Source/StealthGame/Public/StealthGameMode.h` (`StartRound`, `AbortRound`, `EndRound(WinningTeam, Reason)`, `TryScheduleRoundStart`, `CleanupRoundActors`, the auto-restart) and the replicated round fields in `StealthGameState.*`. Prototype resolution is first-come order with no total-order key; the production machine is `FRoundPhaseCore` in the game-instance store.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 40, scoped to this story:*

- [ ] GIVEN each phase, THEN the admission mode pushed is `Open`, `RejoinOnly` (x3), `Closed` in table order, and each illegal transition of the States section is refused (table-driven).

---

## Implementation Notes

*Derived from ADR-0005 section 2 and Key Interfaces, and GDD R4 and States and Transitions:*

- **Admission push on every phase change (R4, Session rule 20).** On entering a phase Round pushes through the Session gateway `SetAdmissionMode`: `Lobby` -> `Open`; `Preparing`, `Active`, `Ended` -> `RejoinOnly`; `Reset` -> `Closed`. In phase order Lobby, Preparing, Active, Ended, Reset the spy therefore records `Open`, `RejoinOnly`, `RejoinOnly`, `RejoinOnly`, `Closed`, which is the AC's "`Open`, `RejoinOnly` (x3), `Closed`". Session forces `Closed` itself during travel; that is Session's behaviour, not tested here.
- **Table-driven transition guard.** Encode the GDD transition table as a fixture and drive `FRoundPhaseCore` through it. Legal transitions (GDD States and Transitions): `Lobby -> Preparing` (Start accepted and travel accepted), `Lobby -> Lobby` (Start refused, or travel refused at step 8), `Preparing -> Active`, `Preparing -> Reset` (abort), `Active -> Ended` (win), `Active -> Reset` (host abort if allowed, or Void), `Ended -> Reset`, `Reset -> Lobby`, any phase on `OnSessionEnding` (log flushed, nothing released).
- **Illegal and asserted in tests (GDD States and Transitions, last paragraph):** `Active -> Lobby` and `Active -> Preparing` directly; `Ended -> Active`; `Ended -> Preparing`; any phase change by a client; two latches; a queue item executed outside `Active`; `Lobby -> Active` without `Preparing`. A refused transition leaves the phase, the pushed admission mode and `PhaseChangeCount` unchanged. Open: confirm against GDD States and Transitions whether "any phase change by a client", "two latches" and "a queue item executed outside Active" are covered by this story's table or by stories 043 and 008; this story's table covers the phase-to-phase pairs, and the latch and queue items are asserted where those mechanisms are built.
- **Where the code goes.** Phase transitions are methods of `FRoundPhaseCore` (plain C++, no `UObject`); `URoundMatchStore` is the thin facade (ADR-0007 plain-C++ core behind a facade). The Session gateway is a consumer-owned interface injected by the composition root; tests inject the story 003 fake that records calls in order.
- **State that must survive travel** (`PhaseChangeCount` is held in the phase core so it never moves backward across travel) is exercised in stories 029 and 030, not here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024 (`production/epics/round-match-framework/story-024-start-ordering.md`): the accepted-Start call order and entering `Preparing` after travel acceptance.
- Story 029 (`production/epics/round-match-framework/story-029-public-state-replication.md`): `FRoundPublicState` replication and `Phase` 0 = `Unknown`.
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): the abort codes and the Preparing -> Reset cleanup.
- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the Active entry step.
- Story 057 and 058: host abort in Active and what Reset clears.
- Story 073 (`production/epics/round-match-framework/story-073-should-admit-expired-table.md`): the `ShouldAdmitExpired` phase x state table.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- Source/StealthGameplay/Public/Server/Core/Round/StealthRoundPhaseCore.h, Private/Core/Round/StealthRoundPhaseCore.cpp (new: FRoundPhaseCore)
- Source/StealthGame/Private/StealthGameMode.cpp (prototype round state now follows Round's phase listener)
- Source/StealthGameplay/Private/Tests/Round/PhaseAdmissionAndIllegalTransitionsTest.cpp (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Each phase pushes the specified admission mode and every illegal transition is refused.
  - Given: a fake Session admission spy and the transition table.
  - When: each phase is entered, and each illegal transition from the States table is attempted.
  - Then: the pushed admission modes are `Open`, `RejoinOnly` (x3), `Closed`, in table order.
  - Then: every illegal transition is refused and the phase does not change.
  - Edge cases: the illegal-transition table lives in the GDD States section and must be frozen as a table-driven fixture (Open: confirm against GDD States and Transitions that the fixture lists exactly the "Illegal and asserted in tests" set).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/PhaseAdmissionAndIllegalTransitionsTest.cpp` (automation test `StealthGame.Round.PhaseAdmissionAndIllegalTransitions`, class `FRoundPhaseAdmissionAndIllegalTransitionsTest`) — must exist and pass.

**Status**: [x] Created and passing (`StealthGame.Round.PhaseAdmissionAndIllegalTransitions`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-024-start-ordering.md`
- Unlocks: `production/epics/round-match-framework/story-029-public-state-replication.md`, `production/epics/round-match-framework/story-034-preparing-aborts.md`, `production/epics/round-match-framework/story-035-watchdog-bound.md`, `production/epics/round-match-framework/story-057-host-abort.md`, `production/epics/round-match-framework/story-073-should-admit-expired-table.md`
- Cross-epic: `production/epics/session-travel` (`SetAdmissionMode`, `EAdmissionMode`, via the fake Session gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

---

## Completion Notes
**Completed**: 2026-10-26 (D53 integration batch: PSR 061-062, RMF 018-029, SCL 002/006)
**Criteria**: 1/1 passing
**Deviations**: 'two latches' / 'queue item outside Active' stay with stories 043/008; 'phase change by a client' is covered by 026 (a client holds no Round core).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/PhaseAdmissionAndIllegalTransitionsTest.cpp`; full suite: `production/qa/evidence/integration-spine-2026-10-26/full-suite.txt` (120/120 Success)
**Run result**: OBSERVED: full phase cycle in PIE (Unknown->Lobby->Preparing->Active->Ended->Reset->Lobby->Preparing->Active)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, D53 batch
