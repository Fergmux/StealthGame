# Story 069: A rejoin-window expiry for a still-Active participant converts in the same slot with a WindowBeforeGrace defect, and none in Preparing

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 26b; rule R17.4 (window expiry paragraph), R17.7, R18, formula F4 (Preparing case).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies the Session fact `OnRejoinWindowExpired` (ADR-0005 Key Interfaces) together with Round's policy in ADR-0002 section 8.7 (4): in `Active` for a still-`Active`, non-kicked participant it is an immediate grace expiry logged `WindowBeforeGrace` as a defect (it can only happen through a defect, since `G_eff` precedes the window); in `Preparing` it is ignored.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. Session's delegate is faked by the story 003 gateway; no engine API, post-cutoff API or verification item (spikes S3, S4, S5) is exercised.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules, section Round phase flow, Event Log and public round struct):
- Required: The grace conversion calls `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)` and on `Applied` `ReleaseToCrowd` in the same call stack (ADR-0002, Technical Setup 2026-10-25b); a window expiry in `Active` reuses that path.
- Forbidden: Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source (ADR-0002). Session's delegate must enter the queue, not convert inline.
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 26b, scoped to this story:*

- [ ] GIVEN a window expiry in `Active` for a still-Active participant, THEN conversion in the same slot (`Applied`) and a `WindowBeforeGrace` defect entry; in `Preparing`, THEN no conversion.

---

## Implementation Notes

*Derived from GDD R17.4 and ADR-0002 section 8.7 (4):*

- **Entry.** Session raises `OnRejoinWindowExpired(P)` on the game thread outside the drain; Round must turn it into a queue item (the closed `ConnectionTransition` item type carries `{Participant, ConnectionGeneration, Transition}`, ADR-0002 section 2a; Open: confirm the item kind and its mapping against ADR-0002 section 2a) so the conversion runs in a slot with a key and the epoch and generation guards apply.
- **In `Active`, still-Active, not kicked.** Treat it as an immediate grace expiry in that slot: run the same conversion routine as story 060/064 (`RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)`, `ReleaseToCrowd` in the same call stack on `Applied`, then R15 partition (b) on the resulting event) and append a `WindowBeforeGrace` defect entry to the Event Log through `IRoundLogWriter::Append` (a defect entry, never `GraceExpired`). The pending grace timer for that participant becomes a logged no-op when it later fires (stale answer, story 071's family).
- **In `Preparing`.** Ignore the event: the grace has not started (it starts at `ActivePhaseStart`) and R18 still admits the participant; no transition, no entry. GDD F4 "Preparing case": a participant that timed out loading still converts at `ActivePhaseStart + G_eff` even if the Session window meanwhile expired.
- **Kick is not this path.** A host kick makes Session raise `OnRejoinWindowExpired` in the same tick as `Disconnected(Kicked)`; that expiry is expected and ignored (story 070). The test here uses a non-kicked participant.
- **Why a defect.** `G_eff` is clamped so that `RejoinWindowSec - G_eff >= 30` (story 068), so a window expiry can only precede the grace expiry through a defect; the log entry makes it visible to playtest.
- **Other phases.** Behaviour in `Lobby`, `Ended` and `Reset` is not part of the AC; connection events there change only the connection (R9, R17.2).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and the conversion routine this story reuses (this story's dependency).
- Story 070 (`production/epics/round-match-framework/story-070-kick-window-expiry-ignored.md`): the kicked participant's window expiry, which is ignored.
- Story 068 (`production/epics/round-match-framework/story-068-grace-clamp.md`): the `G_eff` clamp that makes this a defect.
- Story 071 (`production/epics/round-match-framework/story-071-grace-generation-guard.md`): stale-generation grace timers.
- Session internals (window timer, `Expired` connection state) and Roster internals (`RequestTransition` rules).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Disconnect/RejoinWindowPolicy.h`
- `Source/StealthGameplay/Private/Round/Disconnect/RejoinWindowPolicy.cpp`
- `Source/StealthGameplay/Private/Tests/Round/WindowExpiryWhileActiveTest.cpp` (class `FRoundWindowExpiryWhileActiveTest`, test name `StealthGame.Round.WindowExpiryWhileActive`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A rejoin-window expiry for a still-Active participant converts in the same slot with a WindowBeforeGrace defect, and none in Preparing.
  - Given: a window expiry in `Active` for a still-Active participant; separately, one in `Preparing`.
  - When: the expiry is drained.
  - Then: in `Active`, the participant converts in the same slot (`Applied`) and a `WindowBeforeGrace` defect entry is logged.
  - Then: in `Preparing`, no conversion occurs.
  - Edge cases: no defect entry in `Preparing`.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/WindowExpiryWhileActiveTest.cpp` (automation test `StealthGame.Round.WindowExpiryWhileActive`, class `FRoundWindowExpiryWhileActiveTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (`OnRejoinWindowExpired`; stories not yet created), `production/epics/participant-state-roster` (`RequestTransition`; stories not yet created)
