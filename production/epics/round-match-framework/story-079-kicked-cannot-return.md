# Story 079: A kicked Active participant clears teammates' reconnecting flag, cannot reconnect and never enters R18

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 66; rules R17 (step 2, host kick), R18.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: Four game-instance stores are the only authoritative match state, with seamless `ServerTravel`, a single round acknowledgement, admission modes and host commands on the game instance. This story applies the Session/Round split for a kick: Session reports the fact (`Disconnected(Kicked)`, sends the in-round participant straight to `Expired`, refuses the key for the rest of the session), the Roster derives and clears `bReconnecting`, and Round only records `Kicked = true` on the grace record and never writes `bReconnecting` itself (ruling D23).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. The story is a [U] test against fake Session and Roster gateways, so no engine API runs. ADR-0005 uses only long-standing `AGameModeBase`/`APlayerController`/`UNetDriver` hooks (the 5.6 `DispatchPostLogin` deprecation is not touched), and its behavioural spikes S3, S4 (reconnect, retained PlayerState, supersede) and S5 are not exercised: the real connection refusal of a kicked key is Session's (Session rule 7 gate 3, Session AC 59), not asserted here.

**Control Manifest Rules (this layer)** (Round-relevant disconnect rules sit in the Foundation Layer Rules; the pawn rule is Core):
- Required: Disconnect policy: the grace timer is the queue item `TimerFired(GraceExpiry)`; at most one grace timer per participant (a kick sets `Kicked` and keeps the expiry); expiry calls `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)` (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never destroy the pawn in `PawnLeavingGame`; never destroy-and-respawn on disconnect or takeover - a vanishing/appearing pawn identifies a human (ADR-0010).
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: None (no prototype equivalent: the prototype has no disconnect policy, kick path or Roster). New Core code under ADR-0007 modules.

**Where it sits**: Exit B (Core). It closes the kick branch of the disconnect policy built by stories 060 and 070: story 070 proves the window expiry of a kicked participant is ignored and conversion happens at `DisconnectedAt + G_eff`; this story proves the other three consequences of a kick (teammates' flag cleared silently, no return, R18 never entered).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 66, scoped to this story:*

- [ ] GIVEN an Active participant the host kicks, THEN Session's notify carries `bKicked`, the Roster clears the teammates' `bReconnecting` with `ReconnectEndCause = Cleared` and no "reconnecting" line follows, a later connection with that key is refused, and R18 is never entered for it.

---

## Implementation Notes

*Derived from ADR-0005 sections 4-5 and Implementation Guidelines 8-9, and GDD R17.2 and R18:*

- **Kick path (R17.2).** A host kick reaches Round as `OnParticipantDisconnected(Kicked)`. Session sends a kicked in-round participant straight to `Expired` and raises `OnRejoinWindowExpired` in the same tick. Round takes the normal disconnect path with the record flagged `Kicked = true` (story 070 owns the grace record, the ignored window expiry and the `Applied` conversion). If a live grace timer already exists, a kick sets `Kicked = true` on it and starts no second timer and no second takeover.
- **What this story asserts.** (1) The notify Round receives from the fake Session gateway carries `bKicked`, and Round forwards `Kicked` to the Roster transition path unchanged. (2) Round never writes `bReconnecting`: the Roster derives it from Session's notify and from Round's transitions (ruling D23), so assert the absence of any reconnecting write in the fake Roster gateway's ordered call log, and that the Roster's own derivation yields `bReconnecting` cleared with `ReconnectEndCause = Cleared` for the teammates. (3) A later connection attempt with the kicked key is refused: the refusal is Session's (kicked key refused for the rest of the session, Session rule 7 gate 3), so the fake Session gateway models it and Round must make no admission or re-pawn call. (4) R18 is never entered: no `ShouldAdmitExpired` readmission path, no `OnInformantReadmitted` call and no re-pawn request is recorded for the kicked key.
- **Seams.** Use the fake Session and Roster gateways of story 003 (dependency injection, no singletons), the fake `IClock` of story 001 and the fixture constants of story 005 (team layouts, codes). Do not inline fixture strings.
- **Boundaries.** Session reports facts and never calls `RequestTransition` (ADR-0005 guideline 8); ack records, kick list and connection maps are keyed by `ParticipantId` and never replicated (guideline 9). The kicked pawn keeps its AI-driven walk for `G_eff` like any dropped player (an instant conversion would be a timing tell); do not change that here.
- **Language rule.** The "reconnecting" line is presentation. The assertion in this story is on the Roster model (`bReconnecting`, `ReconnectEndCause`), not on UI text.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 070 (`production/epics/round-match-framework/story-070-kick-window-expiry-ignored.md`): the ignored window expiry, `Kicked = true` on the grace record, conversion at `DisconnectedAt + G_eff` and R15 (c).
- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and the disconnect policy skeleton.
- Story 065 (`production/epics/round-match-framework/story-065-forfeit-void-cases.md`): `Void(KickedLastActive)` when a kick is the last conversion.
- Story 062 (`production/epics/round-match-framework/story-062-rejoin-after-grace.md`): R18's informant path for a dropped (not kicked) player.
- Session internals (the real key refusal, Session AC 59) and Roster internals (how `bReconnecting` is derived) belong to the session-travel and participant-state-roster epics; the "reconnecting" text is the HUD epic's.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Round/Disconnect/RoundDisconnectPolicy.cpp` (created by story 060, extended by 070; this story adds at most the kicked-path forwarding)
- `Source/StealthGameplay/Private/Tests/Round/KickedCannotReturnTest.cpp` (class `FRoundKickedCannotReturnTest`, test name `StealthGame.Round.KickedCannotReturn`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A kicked Active participant clears teammates' reconnecting flag, cannot reconnect and never enters R18.
  - Given: an Active participant the host kicks.
  - When: Session notifies Round, and a later connection with that key is attempted.
  - Then: Session's notify carries `bKicked`.
  - Then: the Roster clears the teammates' `bReconnecting` with `ReconnectEndCause = Cleared`, and no "reconnecting" line follows.
  - Then: the later connection is refused, and R18 is never entered for it.
  - Edge cases: the clearing is Roster behaviour, so assert Round's forwarding of `Kicked` and the absence of any further `bReconnecting` setting. Open: confirm against GDD R17 step 2 and the Roster GDD (R6) that the "reconnecting" line is presentation, so it is asserted as a Roster-model assertion.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/KickedCannotReturnTest.cpp` (class `FRoundKickedCannotReturnTest`, test name `StealthGame.Round.KickedCannotReturn`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-070-kick-window-expiry-ignored.md`
- Unlocks: None
- Cross-epic: `production/epics/participant-state-roster` (`bReconnecting` derivation, `ReconnectEndCause = Cleared`; stories not yet created), `production/epics/session-travel` (kick notify with `bKicked`, key refusal; stories not yet created), `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
