# Story 041: A non-empty GetUnboundActiveParticipants() at Active entry aborts BindFailed and never enters Active

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 42; rules R6 (P6 `BindFailed`), R7 (last sentence), R8 (abort in Preparing), Edge Cases ("If `GetUnboundActiveParticipants()` is non-empty at Active entry").
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies ADR-0005's split of duties (Session reports facts, Round decides, Round owns the abort-to-lobby path) together with ADR-0002 section 8.6, which states that `BindFailed` also covers the Active-entry check that `GetUnboundActiveParticipants()` is empty (Roster invariant I4).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk; residual risk is behavioural, spikes S3 to S5)
**Engine Notes**: None touched. The check is a call on the fake Roster gateway (story 003) from a plain C++ phase core; no `UWorld`, travel, controller or post-cutoff API is exercised, so ADR-0005 verification items S3, S4 and S5 do not apply here.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the Foundation Layer Rules, Round phase flow and Forbidden Approaches):
- Required: the `Preparing` abort codes are the closed set `BadParams`, `TuningLoadFailed`, `TuningSnapshotFault`, `DeckInvalid`, `AllocatorMisuse`, `LayoutAborted`, `LayoutSeedMismatch`, `TeamAbsent`, `BindFailed`, `PlacementInvariant`, `PlacementInfeasible`, `NpcCountTooLow`, `PreparingWatchdog`; an abort runs `Reset` with `PreparingFailed(code)` and returns to the lobby (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never let a `Preparing` abort call `UResolutionQueue::Latch` or stage a `RoundEndStep`; the queue is not open in `Preparing` (ADR-0002).
- Guardrail: Roster queries are O(1), at most 2 hash probes and 0 allocations per call (AC-PSR-28); `GetUnboundActiveParticipants()` is one such query and adds no per-frame work here (ADR-0005).

**Prototype disposition**: None (new Core code). The prototype `StartRound` in `Source/StealthGame/Private/StealthGameMode.cpp` has no unbound-pawn check and no Roster binding; it is retired as a whole by stories 024 and 042, not by this story.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 42, scoped to this story:*

- [ ] GIVEN `GetUnboundActiveParticipants()` non-empty at Active entry, THEN abort `BindFailed` and `Active` is never entered.

---

## Implementation Notes

*Derived from GDD R6, R7, R8 and ADR-0002 sections 8.6 and 8.8:*

- **Where the check sits (R7).** At the first drain whose server time is at or after `ActiveAtServerTime`, Round invokes `GetUnboundActiveParticipants()` immediately before the Active-entry step. A non-empty result means Roster invariant I4 is broken (an Active participant has no live bound pawn) and aborts `BindFailed`. The check runs before any of story 042's effects, so nothing of Active entry has happened: no `ActivePhaseStart` stamp, no `RoundsPlayedCount` increment, no queue open, no `OnRoundActive`, no Crowd start.
- **The abort is a Preparing abort (R8).** Round logs the code, runs the `Reset` cleanup (R10) with reason `PreparingFailed(BindFailed)` and calls `RequestLobbyTravel`. No result, no score change, `RoundsPlayedCount` unchanged, the Event Log slice archived and never released. It never calls `UResolutionQueue::Latch` and stages no `RoundEndStep` (the queue opens only at Active entry), so none of the latch machinery of stories 043 and 052 is involved.
- **The same code at P6.** `BindFailed` is also raised in P6 when `BindPawn` is rejected (the pawn is destroyed and the round aborts); that path belongs to story 039. This story covers only the Active-entry guard.
- **Fake gateway.** The story 003 fake Roster gateway lets the test return an unbound set (one `ParticipantId`) or an empty set. The abort is observed through the phase core's abort entry (a spy recording the code); the full abort effects (archive, lobby travel, nothing released) are asserted by story 034, which covers all 13 codes. Open: stories 034 and 041 do not depend on each other, so confirm at /dev-story that the phase core exposes a single abort entry both reuse; whichever lands first creates it.
- **A defect, not a state.** The GDD calls this case a defect (Roster I4 broken). Log it as such through the Event Log writer and the fault sink; there is no recovery attempt and no retry.
- **Where the code goes.** A small guard in the Active-entry path under `Source/StealthGameplay/Private/Round/Preparing/`, calling the consumer-owned Roster gateway interface; plain C++, no `UObject` (ADR-0007).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): the Active-entry step that runs when the guard passes.
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): the effects of every abort code (Lobby travel, `PreparingFailed` archived, 0 pawns, nothing released).
- Story 039 (`production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`): P6 spawning and the `BindFailed` raised there when `BindPawn` is rejected.
- Story 032 (`production/epics/round-match-framework/story-032-preparing-barrier.md`): the P5 barrier that precedes P6 and Active entry.
- Roster internals (how `GetUnboundActiveParticipants()` is computed, invariant I4 and `OnBindingLost`) belong to the participant-state-roster epic; the in-round pawn-loss recovery is stories 075 and 076.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Round/Preparing/ActiveEntryGuard.cpp`
- `Source/StealthGameplay/Public/Round/Preparing/ActiveEntryGuard.h`
- `Source/StealthGameplay/Private/Tests/Round/ActiveEntryUnboundAbortsTest.cpp` (class `FRoundActiveEntryUnboundAbortsTest`, test name `StealthGame.Round.ActiveEntryUnboundAborts`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A non-empty `GetUnboundActiveParticipants()` at Active entry aborts `BindFailed` and `Active` is never entered.
  - Given: Preparing with a fake gateway leaving one participant unbound.
  - When: Active entry is attempted.
  - Then: the abort is `BindFailed`.
  - Then: the phase never becomes `Active`, and `OnRoundActive` is never fired.
  - Edge cases: with an empty unbound set, Active is entered normally.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/ActiveEntryUnboundAbortsTest.cpp` (automation test `StealthGame.Round.ActiveEntryUnboundAborts`, class `FRoundActiveEntryUnboundAbortsTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-032-preparing-barrier.md`, `production/epics/round-match-framework/story-039-spawn-one-frame-and-gate.md`
- Unlocks: `production/epics/round-match-framework/story-042-active-gate.md`
- Cross-epic: `production/epics/participant-state-roster` (`GetUnboundActiveParticipants()`, invariant I4, via the fake Roster gateway; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
