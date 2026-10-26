# Story 063: A rejoin during Ended is admitted with no pawn, no transition and bIsSpectator false

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void). Also `GDD AC 23c`, `R18` (last bullet, `Ended`: admitted, view only), `R9` ("A disconnect or rejoin during `Ended` changes only the connection").
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies section 3 (admission mode times known key, with the `ShouldAdmitExpired` callback into Round) and section 5 (rejoin by retained PlayerState; `Reset` of rounds by travel): in `Ended` the admission mode is `RejoinOnly`, so a returning known key is admitted, and Round creates no pawn and requests no transition.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. The rejoin is a fake Session event, so spike S4 (retained PlayerState adoption, no player-array change on any client) is not touched; the claim that the returner is not in the engine spectator state (`bIsSpectator` false, which replicates to every client) is asserted on Round's gateway call and not on a real PlayerState.

**Control Manifest Rules (this layer)** (the Round-relevant required rule sits in the manifest's Foundation Layer Rules, Session and travel; the Forbidden rule is from the Core Layer Rules, Camera; the Core Layer Rules hold no other Round-specific rule):
- Required: Authoritative match state lives only in the four game-instance stores, keyed by `ParticipantId`, never by controller or PlayerState pointer (ADR-0005).
- Forbidden: Never use spectator pawns, spectator state or spectator RPCs (ADR-0015, ADR-0001); the returner in `Ended` sees a neutral view, not the engine spectator state.
- Guardrail: Session: silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005).

**Prototype disposition**: none (no prototype equivalent: the prototype has no rejoin and no Ended-phase admission).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 23c, scoped to this story:*

- [x] GIVEN a rejoin during `Ended`, THEN admitted, no pawn, no transition, `bIsSpectator` false.

---

## Implementation Notes

*Derived from GDD R18 and R9:*

- Rule (R18, last bullet): in `Ended` the returner is "admitted, view only, no pawn, no transition, until `Reset` (neutral camera, not the engine spectator state)". R9: "A disconnect or rejoin during `Ended` changes only the connection (no transitions are requested)." `ShouldAdmitExpired(P) = P.InRound AND Phase in {Preparing, Active, Ended}` (R18, story 073) is true here for an `InRound` participant, so Session admits even a record whose rejoin window has expired.
- Setup: a participant disconnected before the round ended (its grace not yet expired, or already converted to `Informant`); the round enters `Ended` (an objective-win latch, story 052); the fake Session raises the rejoin. Assert (1) admission: Round's `ShouldAdmitExpired` answer is true and the fake Session records the admission, (2) no pawn: the fake pawn factory recorded no spawn, no takeover and no `BindPawn`, and no Crowd call, (3) no transition: the fake Roster recorded no `RequestTransition` (the Roster's `EndRoundAll` batch already made records `RoundEnded`; the rejoin adds nothing), (4) `bIsSpectator` is false on the participant's view carrier (Round's gateway does not set the spectator state; the neutral camera is a presentation matter).
- Edge case (QA): no log entry beyond the admission. Open: confirm against GDD R19 (entry types include `Rejoined`) whether Round writes a `Rejoined` entry for a rejoin in `Ended`, or only the admission; the AC does not say.
- A pending grace timer in `Ended`: grace timers are cancelled at `Reset` and cannot change the outcome after the latch (`TryTerminate` refuses, R9 latch step (i)); the test does not assert them.
- Where it lives: the rejoin handler in the Round Core disconnect policy (planned `Source/StealthGameplay/Private/Round/Disconnect/`), extending stories 060 to 062; the `ShouldAdmitExpired` policy implementation is story 073's.
- Manifest 2026-10-25f: `CanViewerKnow` and `GetViewerClass` default to deny for an unknown viewer or unlisted fact, and a new viewer-cache entry (rejoin) keeps the spawn gate closed until Session moves the connection to `Connected`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline.
- Story 061 (`production/epics/round-match-framework/story-061-rejoin-inside-grace.md`) and story 062 (`production/epics/round-match-framework/story-062-rejoin-after-grace.md`): the rejoin cases in `Active`.
- Story 073 (`production/epics/round-match-framework/story-073-should-admit-expired-table.md`): the `ShouldAdmitExpired` phase by state table.
- Story 052 (`production/epics/round-match-framework/story-052-ended-latch-effects.md`): the round-end step that produces `Ended`.
- The neutral camera view for the returner (Camera and HUD epics) and Session's acknowledgement and PlayerState adoption (session-travel epic).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/RejoinInEndedTest.cpp` (class `FRoundRejoinInEndedTest`, test name `StealthGame.Round.RejoinInEnded`)
- `Source/StealthGameplay/Private/Round/Disconnect/` rejoin handler (Ended branch; shared with stories 060 to 062)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A rejoin during Ended is admitted with no pawn, no transition and bIsSpectator false.
  - Given: a participant disconnected before the round ended, and a round in `Ended`.
  - When: it rejoins.
  - Then: it is admitted.
  - Then: no pawn is created and no state transition occurs.
  - Then: `bIsSpectator` is false.
  - Edge cases: no log entry beyond the admission (Open: confirm against GDD R19).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/RejoinInEndedTest.cpp` — must exist and pass (automation test `StealthGame.Round.RejoinInEnded`, class `FRoundRejoinInEndedTest`)

**Status**: [x] Created and passing (`StealthGame.Round.RejoinInEnded`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None

Cross-epic: `production/epics/session-travel` (rejoin admission and `ShouldAdmitExpired` call; stories not yet created); `production/epics/participant-state-roster` (`RoundEnded` records, no transition; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing (ShouldAdmitExpired true; no pawn, transition, Crowd or spectator call; no log entry)
**Deviations**: Round writes no Rejoined entry in Ended (Open item)
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/RejoinInEndedTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: N/A - Logic, covered headless
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
