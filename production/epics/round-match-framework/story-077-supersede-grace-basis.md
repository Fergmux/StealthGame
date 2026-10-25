# Story 077: A superseded Connected participant is AI-driven at t_s, hands back at rejoin, or converts at t_s + G_eff

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 62 (ruling D24, cross-review S-B2); rules R17.2 (supersede is a disconnect at the supersede instant), R17.3 (`OnParticipantDisconnected(Superseded)` and `OnParticipantRejoined(Superseded)`), R18, formula F4.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, `IdentityKey` rejoin and a Session that reports facts while Round decides policy. ADR-0010: kind-blind possession and a hand-over pass that commits in a slot, bumps the control epoch and executes at the next frame start. This story applies both: a supersede raised at the supersede instant is a loss that takes the normal grace path, the pawn stays AI-driven for the whole load of the new client, and control is handed back only at `OnParticipantRejoined(Superseded)` (ADR-0002 section 8.7 (2) and (3)).
**ADR Version**: ADR-0005 2026-10-24; ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 and ADR-0010 knowledge risk)
**Engine Notes**: None touched by the unit test. It runs on a fake clock with fake Session, Roster and Crowd gateways. ADR-0005 verification S4 (reconnect: retained PlayerState to a new controller, two clients on one machine with `-IdentityKey=`, supersede) and ADR-0010 H1 (AI-to-player hand-over gap) and S14 are real-engine items that this story does not exercise; no post-cutoff API is used.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules, section Round phase flow, Event Log and public round struct):
- Required: A hand-over is committed in the drain as the apply-only hand-over pass, bumps the pawn's control epoch, takes effect at the next frame start and is never committed during a traversal (ADR-0002, ADR-0010); the grace timer is `TimerFired(GraceExpiry)` with `G_eff = min(DisconnectGraceSec, RejoinWindowSec - 30)`.
- Forbidden: Never execute a hand-over inline in the queue slot (ADR-0010).
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line (ADR-0005).

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 62, scoped to this story:*

- [ ] GIVEN a `Connected` Active participant whose key is superseded at `t_s` (fake clock; Session raises `OnParticipantDisconnected(Superseded)` at `t_s`), THEN the pawn is AI-driven from `t_s` by the normal blend (no second blend if the Soft hand-over had begun), the grace record carries `GraceExpiry = t_s + G_eff` and the generation the Roster bumped; WHEN `OnParticipantRejoined(Superseded)` arrives at `t_s + G_eff - 1`, THEN control is handed back at that event and not before and the participant stays Active; WHEN instead the rejoin has not completed at `t_s + G_eff`, THEN the participant converts at exactly that time (`Applied`, `ReleaseToCrowd`) and a later rejoin takes R18's informant path; a failed rejoin starts no new timer and the original one still matches its generation.

---

## Implementation Notes

*Derived from GDD R17.2, R17.3 and ADR-0002 section 8.7 (1) to (4):*

- **At `t_s`.** Session raises `OnParticipantDisconnected(Superseded)` at the supersede instant. Round treats it as a normal disconnect of a `Connected` participant: if the pawn is not already AI-driven, take it over at once by the normal blend; if the Soft hand-over (`OnParticipantUnresponsive`) had already begun it simply continues, with no second takeover blend. The grace timer starts at `t_s` with `GraceExpiry = max(DisconnectedAt, ActivePhaseStart) + G_eff` where `DisconnectedAt = t_s` (the conversion counts from the supersede instant whatever the rejoin does), carrying the generation the Roster bumped for the supersede.
- **Hand-over rules.** The takeover is a hand-over committed in a slot (`UStealthPawnFactory::RequestHandOver`), bumps the control epoch, and executes in the hand-over pass at the next frame start; it waits for landing if the pawn is in traversal (ADR-0010). The test asserts through the Crowd/factory spies: exactly one drive-start for the participant (none extra when a Soft hand-over had begun).
- **Rejoin at `t_s + G_eff - 1`.** The event `OnParticipantRejoined(Superseded)` is raised only at rejoin completion (Session rule 15), so control never returns before the rejoiner is ready. On that event Round cancels the grace timer and hands control back by the reverse blend: the hand-back is observed at that event and not earlier, and the participant stays `Active` with the same pawn. Open: confirm the unit of the "1" in `t_s + G_eff - 1` (seconds, matching AC 23a's "1 s before grace expiry") against GDD AC 62 and story 061.
- **No rejoin by `t_s + G_eff`.** The grace item fires at exactly `t_s + G_eff`: `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)` is `Applied`, `ReleaseToCrowd(pawn)` runs in the same call stack and R15 is evaluated on the event. A rejoin that completes after that finds the participant already an `Informant` and takes R18's informant path (re-pawned by the C1 takeover, story 062). Open: confirm against GDD R18 that this is the full effect of "R18's informant path" for the assertion (the QA spec marks R18 as a rule outside the range qa-lead read; the R18 text states: `Informant` with no live pawn is re-pawned through Penalty's informant respawn path).
- **Failed rejoin.** A failed rejoin raises no second `OnParticipantDisconnected`, changes neither `DisconnectedAt` nor `ConnectionGeneration`, and starts no new timer; the one grace timer keeps running and still matches its generation (repeated failed rejoins cannot extend grace). An old timer is stale only for a genuine second loss after a completed rejoin (story 071).
- **Lobby.** A supersede in `Lobby` raises no disconnect at all (Session rule 15); not tested here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and records (this story's dependency).
- Story 061 (`production/epics/round-match-framework/story-061-rejoin-inside-grace.md`): ordinary rejoin inside grace.
- Story 062 (`production/epics/round-match-framework/story-062-rejoin-after-grace.md`): the C1 informant re-pawn after expiry.
- Story 071 (`production/epics/round-match-framework/story-071-grace-generation-guard.md`): stale-generation timers after a genuine second loss.
- Story 090 (`production/epics/round-match-framework/story-090-hand-over-and-back.md`): the Soft hand-over and hand-back blending (Blocked on spikes S14 and H1).
- Session's supersede acceptance and rejoin completion (session-travel epic), Roster's generation bump, and Crowd's blend (other epics).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Round/Disconnect/DisconnectPolicy.cpp` (created by story 060; extended here for the supersede instant and rejoin hand-back)
- `Source/StealthGameplay/Private/Tests/Round/SupersedeGraceBasisTest.cpp` (class `FRoundSupersedeGraceBasisTest`, test name `StealthGame.Round.SupersedeGraceBasis`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A superseded Connected participant is AI-driven at t_s, hands back at rejoin, or converts at t_s + G_eff.
  - Given: a Connected Active participant superseded at `t_s` (Session raises `OnParticipantDisconnected(Superseded)`).
  - When: it rejoins at `t_s + G_eff - 1`; in a second run it has not rejoined by `t_s + G_eff`.
  - Then: the pawn is AI-driven from `t_s` by the normal blend (no second blend if the Soft hand-over had begun).
  - Then: the grace record carries `GraceExpiry = t_s + G_eff` and the generation the Roster bumped.
  - Then: with the rejoin, control is handed back at that event and not before, and the participant stays Active.
  - Then: without it, the participant converts at exactly `t_s + G_eff` (`Applied`, `ReleaseToCrowd`), and a later rejoin takes R18's informant path.
  - Then: a failed rejoin starts no new timer, and the original still matches its generation.
  - Edge cases: R18 is a GDD rule outside the range the qa-lead read. Open: confirm against GDD R18.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/SupersedeGraceBasisTest.cpp` (automation test `StealthGame.Round.SupersedeGraceBasis`, class `FRoundSupersedeGraceBasisTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None
- Cross-epic: `production/epics/session-travel` (supersede, `OnParticipantDisconnected(Superseded)`, `OnParticipantRejoined(Superseded)`; stories not yet created), `production/epics/participant-state-roster` (generation bump, `RequestTransition`; stories not yet created), `production/epics/crowd-npc-behaviour` (drive and release; stories not yet created), `production/epics/shared-character-locomotion` (hand-over pass; stories not yet created)
