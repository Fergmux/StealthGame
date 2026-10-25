# Story 070: A kicked Active participant's window expiry is ignored and conversion happens at DisconnectedAt + G_eff

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 26c; rules R17.2 (host kick path), R15 (c) (kick is `Void(KickedLastActive)`, decision D32 ordering), R17.6.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies Session's kick behaviour: `KickParticipant` sends an in-round participant straight to `Expired` and raises `OnParticipantDisconnected(Kicked)` and `OnRejoinWindowExpired` in the same tick, while Round (ADR-0002 section 8.7 (2)) treats the kick as a normal disconnect: the pawn is AI-driven at once, the grace record carries `Kicked = true`, and only the grace timer converts, so a kicked pawn keeps the same AI-driven walk for `G_eff` as any dropped player.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. Session's kick is faked by the story 003 gateway; no engine API, post-cutoff API or verification item (spikes S3, S4, S5) is exercised.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules, section Round phase flow, Event Log and public round struct):
- Required: At most one grace timer exists per participant (a kick sets `Kicked` and keeps the expiry); expiry calls `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)` (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer (ADR-0002); the grace timer is the `TimerFired(GraceExpiry)` item.
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 26c, scoped to this story:*

- [ ] GIVEN an Active participant the host kicks (Session raises `OnParticipantDisconnected(Kicked)` and `OnRejoinWindowExpired` in the same tick, connection `Expired`), THEN the window expiry is ignored (no conversion, no `WindowBeforeGrace` entry), the pawn is AI-driven at once, the grace record carries `Kicked = true`, and the participant converts at `DisconnectedAt + G_eff` with the transition `Applied` (not `Rejected_NotDisconnected`) and R15 (c) applied to the zeroing event.

---

## Implementation Notes

*Derived from GDD R17.2, R15 (c) and ADR-0002 section 8.7 (2) and (4):*

- **Kick path.** `Disconnected(Kicked)` for an `Active` participant takes the normal disconnect path: take the pawn over at once if it is not already AI-driven (same blend), start the grace timer as `TimerFired(GraceExpiry)` with `GraceExpiry = max(DisconnectedAt, ActivePhaseStart) + G_eff`, and write the record `{P, GraceExpiry, ExpectedEpoch, ExpectedConnectionGeneration, Kicked = true}`. An instant conversion would be a timing tell, so the kick never converts at once.
- **Same-tick window expiry.** Session raises `OnRejoinWindowExpired` in the same tick as the kick (connection `Expired`). Round ignores it when the record has `Kicked = true`: no conversion and no `WindowBeforeGrace` entry (the contrast with story 069, where a non-kicked still-Active participant converts).
- **Conversion at expiry.** At `DisconnectedAt + G_eff` the resolver calls `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)`. The Roster accepts the `Disconnect` request for a connection Session has marked `Expired` (change request 21, confirmed by Roster R11, decision D9): `Expired` projects `Disconnected` and the move into it does not bump `ConnectionGeneration`, so the carried generation matches and the result is `Applied`, not `Rejected_NotDisconnected`. The test's fake Roster must model that rule; do not weaken the assertion.
- **R15 (c).** The conversion's event is evaluated with the reason partition: a `Disconnect` caused by a host kick (`Kicked = true`) that zeroes a team's `Active` count is `Void(KickedLastActive)` (the host cannot win by kicking the last rival); a conversion that does not zero a team's Active count produces no outcome. The AC says only "R15 (c) applied to the zeroing event", so the fixture must state whether the kicked participant is the last Active of its team; per the QA spec, freeze the expected effect as a fixture constant (story 005). The void path itself (phase `Reset`, `bCalledOff`) is story 065.
- **Related same-frame cases (not this AC).** A kick for a participant that already has a live grace timer starts no second timer and no second takeover: it sets `Kicked = true` on the existing record and keeps that `GraceExpiry`; and a kick and a grace expiry for one participant in the same frame process the kick first, the expiry then being the Roster's idempotent no-op (decision D32). Do not implement these here beyond not breaking them.
- **Teammates' marker.** The Roster clears teammates' `bReconnecting` for a kicked participant silently (`ReconnectEndCause = Cleared`) and a later connection is refused; that is story 079.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline (this story's dependency).
- Story 069 (`production/epics/round-match-framework/story-069-window-expiry-while-active.md`): the non-kicked window expiry that converts with a defect entry.
- Story 079 (`production/epics/round-match-framework/story-079-kicked-cannot-return.md`): `bKicked` notify, teammates' flag cleared, connection refused, R18 never entered.
- Story 065 (`production/epics/round-match-framework/story-065-forfeit-void-cases.md`): `Void(KickedLastActive)` outcome (phase `Reset`, no score).
- Story 071 (`production/epics/round-match-framework/story-071-grace-generation-guard.md`): stale-generation timers.
- Session's kick-list and key-refusal internals and the Roster's `Expired` projection.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Disconnect/GraceRecord.h`
- `Source/StealthGameplay/Private/Round/Disconnect/RejoinWindowPolicy.cpp` (shared with story 069)
- `Source/StealthGameplay/Private/Tests/Round/KickWindowExpiryIgnoredTest.cpp` (class `FRoundKickWindowExpiryIgnoredTest`, test name `StealthGame.Round.KickWindowExpiryIgnored`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A kicked Active participant's window expiry is ignored, and conversion happens at DisconnectedAt + G_eff.
  - Given: an Active participant kicked by the host (Session raises `OnParticipantDisconnected(Kicked)` and `OnRejoinWindowExpired` in the same tick, connection `Expired`).
  - When: the tick drains and the clock reaches `DisconnectedAt + G_eff`.
  - Then: the window expiry is ignored, with no conversion and no `WindowBeforeGrace` entry.
  - Then: the pawn is AI-driven at once, and the grace record carries `Kicked = true`.
  - Then: the participant converts at `DisconnectedAt + G_eff` with the transition `Applied` (not `Rejected_NotDisconnected`).
  - Then: R15 (c) is applied to the zeroing event.
  - Edge cases: R15 (c) is a GDD rule outside the range the qa-lead read, so freeze its expected effect as a fixture. Open: confirm against GDD R15 (c) the fixture's expected effect for the kicked participant (zeroing versus non-zeroing).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/KickWindowExpiryIgnoredTest.cpp` (automation test `StealthGame.Round.KickWindowExpiryIgnored`, class `FRoundKickWindowExpiryIgnoredTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: `production/epics/round-match-framework/story-079-kicked-cannot-return.md`
- Cross-epic: `production/epics/session-travel` (kick, `OnRejoinWindowExpired`, `Expired` state; stories not yet created), `production/epics/participant-state-roster` (`Disconnect` accepted for an `Expired` connection; stories not yet created)
