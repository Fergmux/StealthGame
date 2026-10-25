# Story 060: Silent, quit, informant and debug-kill timelines resolve at the specified times on a fake clock (plus one PIE smoke)

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void). Also `GDD AC 22`, `R17` steps 1, 2, 4 and 5, `F4` (disconnect timing), `R15` partition (b).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration; section 5 states that every `Connection` change bumps the participant's `ConnectionGeneration`, that Round's grace expiry carries it, and that the pawn survives disconnect through `PawnLeavingGame` and is handed to Crowd by Round (R17), never by Session. ADR-0010: one `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator and possessor, kind-blind possession and hand-over pass, disconnect never destroys a pawn. This story builds Round's side of the policy: the `TimerFired(GraceExpiry)` item, `G_eff`, the takeover request and the conversion.
**ADR Version**: ADR-0005 2026-10-24; ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. The unit part uses a fake Session, fake Crowd and fake clock, so none of ADR-0005's spikes (S3 travel, S4 reconnect, S5 listen-host loss) and none of ADR-0010's (traversal, S8, S14, H1, P1) is needed. The one [PIE-2] smoke exercises the real disconnect path, which relies on ADR-0010 S8 (the `PawnLeavingGame` override keeps the pawn and a crowd AI controller possesses it in the same server frame on a real connection close) and ADR-0005 S5 (detection bounds); that smoke cannot be trusted until those spikes have reported, so run it once they have and re-check the story if either fails. MotionWarping (Beta in 5.8.4) and `GetOwnerAdapter()` are not touched.

**Control Manifest Rules (this layer)** (the Round-relevant rules sit in the manifest's Foundation Layer Rules, Round phase flow and Forbidden; the Core Layer Rules hold no Round-specific rule):
- Required: Disconnect policy: the grace timer is the queue item `TimerFired(GraceExpiry)` (`Subject` participant, `Generation` the captured `ConnectionGeneration`, `Expected` round epoch) with `G_eff = min(DisconnectGraceSec, RejoinWindowSec - 30)`; expiry calls `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)` and on `Applied` `ReleaseToCrowd` in the same call stack (ADR-0002).
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer (ADR-0002); the grace timer is a `ScheduleAt` item in the due-time heap.
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (ADR-0002, ADR-0022).

**Prototype disposition**: none (no prototype equivalent: the prototype has no disconnect policy, no grace timer and no takeover; a disconnected prototype player simply leaves the game).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 22, scoped to this story:*

- [ ] GIVEN a fake clock and an Active participant, WHEN its client goes silent at t, THEN takeover occurs in [t + 3.0, t + 3.3] s (Session F2's `L_min` to `L_max`); WHEN it quits at t, THEN takeover at t; Informant (`Disconnect`) at exactly `DisconnectedAt + G_eff` with `ReleaseToCrowd` in the same slot; a debug kill on the pawn before expiry resolves `Player`, after expiry `Npc`. One [PIE-2] smoke run checks the same timeline within ±1 s.

---

## Implementation Notes

*Derived from GDD R17 steps 1, 2, 4, 5, F4 and ADR-0005 section 5:*

- This story creates the disconnect policy core that stories 061 to 072 and 077 extend: an event handler set on the Round store for Session's facts (`OnParticipantUnresponsive`, `OnParticipantResponsive`, `OnParticipantDisconnected`, `OnParticipantRejoined`) and the grace record `{P, GraceExpiry, ExpectedEpoch = (MatchId, RoundEpoch), ExpectedConnectionGeneration}`, with at most one live grace timer per participant. Keep the record shape and the `Kicked` flag slot ready for stories 070 and 077 but implement only what this AC needs.
- Silence (R17.1, F4): `OnParticipantUnresponsive` (silence at least `SessionSoftTimeoutSec`, 3 s). With `bHandOverOnUnresponsive` and the participant `Active` with a bound pawn, Round asks Crowd to drive the pawn through the pawn command interface by the normal blend path; the hand-over is committed in the drain as the apply-only hand-over pass, bumps the pawn's control epoch and takes effect at the next frame start (never committed during a traversal). The fake Session raises the silence at its `L_min` and `L_max` endpoints (3.0 s and 3.3 s after t) and the test asserts the takeover request falls in [t + 3.0, t + 3.3] s. The AC measures only when the takeover occurs; the blend kinematics, hold cancels and hand-back are story 090 (Blocked on spikes S14 and H1), so this story uses a fake Crowd that records the request and its server time. Open: confirm against R17.1 and F4 which instant is "takeover occurs" (the hand-over commit in the drain or its effect at the next frame start); the difference is under one frame and inside the 0.3 s window.
- Quit (R17.2, F4 `T_takeover = 0`): a socket close, quit or kick skips `Unresponsive`; `OnParticipantDisconnected` arrives at t and Round takes the pawn over at once if it is not already AI-driven, and starts the grace timer with `GraceExpiry = max(DisconnectedAt, ActivePhaseStart) + G_eff`, `G_eff = min(DisconnectGraceSec, RejoinWindowSec - GraceMarginSec)` with `GraceMarginSec = 30` (the clamp and its warning are story 068).
- Conversion (R17.4, F4 `T_informant = L_detect + G_eff`): at `GraceExpiry` the `TimerFired(GraceExpiry)` item resolves, calls `RequestTransition(P, Informant, Disconnect, ExpectedEpoch, ExpectedConnectionGeneration)` and, if `Applied`, `ReleaseToCrowd(pawn)` in the same call stack, so the pawn becomes an NPC with the same look; then Round evaluates R15 on the resulting event (partition (b), stories 064 and 065). The AC's "Informant (`Disconnect`) at exactly `DisconnectedAt + G_eff`" is this grace-expiry conversion of an Active participant. Open: the manifest title says "informant" timeline; confirm against R17.4 and R17.5 that the AC does not mean R17.5 (an Informant who disconnects is released to the crowd at once; story 072).
- Debug kill (R17.2, AC 22): before expiry the pawn is `BoundLive` and AI-driven, so a kill on it is a `Player` kill: the participant becomes `Informant` (Penalty puts it in Absent with no respawn timer, PEN-R8) and the grace timer then fires as a logged no-op (R17.4: `NoOp_AlreadyInState` is logged `Voided(TimerFired, code)`, story 071); after the expiry the pawn was released to the crowd, so a kill resolves `Npc`. Open: confirm which hook the AC's "debug kill on the pawn" uses (`stealth.round.debugeliminate` is a `DebugRequestTransition(Informant, Assassinated)`, a Roster transition, not a pawn kill; a `DebugItem` kill that goes through the real resolver may be needed) against R22 and the `FDebugPayload` definition in ADR-0002 section 2a.
- Fake clock only: all times come from the injected fake `IClock` (story 001); the test advances it and never reads the wall clock. Fake Session raises events by the fake clock (story 003).
- PIE smoke: one [PIE-2] run (host plus one client) drops the client silently and quits another run's client, and checks the same timeline within +/-1 s on the real clock. It is declared with `NETWORK_TEST_CLASS(GraceTimeline, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.GraceTimeline.<Method>` per the CQTest exemption in `.claude/rules/test-standards.md`; the unit test keeps the GDD name `StealthGame.Round.GraceTimeline`.
- Where it lives: the disconnect policy in the Round Core (planned `Source/StealthGameplay/Private/Round/Disconnect/`), registering `TimerFired(GraceExpiry)` through the queue's timer resolver (`RegisterTimerResolver(ETimerKind::GraceExpiry, ...)`, ADR-0002 Key Interfaces) from the composition root; Round never calls `BindPawn` or `Possess` itself (ADR-0010 and the factory).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 049 (`production/epics/round-match-framework/story-049-completion-needs-human.md`): a final completion needs a human, Connected, Active completer (the `Disconnected` completer case).
- Story 061 (`production/epics/round-match-framework/story-061-rejoin-inside-grace.md`), 062 (`production/epics/round-match-framework/story-062-rejoin-after-grace.md`), 063 (`production/epics/round-match-framework/story-063-rejoin-in-ended.md`): the three rejoin cases.
- Story 064 (`production/epics/round-match-framework/story-064-last-disconnect-forfeits.md`) and 065 (`production/epics/round-match-framework/story-065-forfeit-void-cases.md`): what the conversion does to the round (forfeit and void).
- Story 068 (grace clamp), 069 (window expiry while Active), 070 (kick window expiry), 071 (grace generation guard), 072 (informant disconnect releases), 077 (supersede grace basis): the variants of the grace record.
- Story 090 (`production/epics/round-match-framework/story-090-hand-over-and-back.md`) (Blocked): the hand-over blend kinematics, hold cancels and hand-back.
- Stories 087 to 089 (Blocked): property-delta, kinematic and blind-observer evidence for takeover.
- Session's detection latency (`L_min`, `L_max`), Roster transition internals, and Crowd's blend and constraint (the participant-bound AI constraint is story 067).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/GraceTimelineTest.cpp` (CQTest `NETWORK_TEST_CLASS(GraceTimeline, "StealthGame.Round.PIE")`, the PIE-2 smoke; scenario `StealthGame.Round.GraceTimeline`)
- `Source/StealthGameplay/Private/Tests/Round/GraceTimelineTest.cpp` (class `FRoundGraceTimelineTest`, test name `StealthGame.Round.GraceTimeline`, the fake-clock timeline)
- `Source/StealthGameplay/Public/Round/` disconnect-policy header (new; Open: name set when implemented)
- `Source/StealthGameplay/Private/Round/Disconnect/` grace record, takeover request and conversion (new)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Silent, quit, informant and debug-kill timelines resolve at the specified times (plus one PIE smoke).
  - Given: the fake clock and an Active participant.
  - When: its client goes silent at t; it quits at t; an Informant disconnects; a debug kill hits the pawn before and after expiry.
  - Then: for silence, takeover occurs in [t + 3.0, t + 3.3] s.
  - Then: for quit, takeover is at t.
  - Then: the Informant's conversion is at exactly `DisconnectedAt + G_eff`, with `ReleaseToCrowd` in the same slot.
  - Then: a debug kill before expiry resolves `Player`, and after expiry `Npc`.
  - Then: one [PIE-2] smoke run shows the same timeline within +/-1 s.
  - Edge cases: the fake Session raises the silence at its `L_min` and `L_max` endpoints (3.0 and 3.3 s).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/GraceTimelineTest.cpp` — must exist and pass (CQTest network test, scenario `StealthGame.Round.GraceTimeline`, full name `StealthGame.Round.PIE.GraceTimeline.<Method>`), together with the fake-clock automation test `Source/StealthGameplay/Private/Tests/Round/GraceTimelineTest.cpp` (`StealthGame.Round.GraceTimeline`, class `FRoundGraceTimelineTest`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-001-test-seam-fake-clock.md`, `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-042-active-gate.md`, `production/epics/round-match-framework/story-049-completion-needs-human.md`
- Unlocks: `production/epics/round-match-framework/story-061-rejoin-inside-grace.md`, `production/epics/round-match-framework/story-062-rejoin-after-grace.md`, `production/epics/round-match-framework/story-063-rejoin-in-ended.md`, `production/epics/round-match-framework/story-064-last-disconnect-forfeits.md`, `production/epics/round-match-framework/story-068-grace-clamp.md`, `production/epics/round-match-framework/story-069-window-expiry-while-active.md`, `production/epics/round-match-framework/story-070-kick-window-expiry-ignored.md`, `production/epics/round-match-framework/story-071-grace-generation-guard.md`, `production/epics/round-match-framework/story-072-informant-disconnect-releases.md`, `production/epics/round-match-framework/story-077-supersede-grace-basis.md`, `production/epics/round-match-framework/story-087-takeover-no-pawn-property-delta.md`, `production/epics/round-match-framework/story-088-takeover-kinematics.md`, `production/epics/round-match-framework/story-090-hand-over-and-back.md`

Cross-epic: `production/epics/session-travel` (disconnect, unresponsive and rejoin events, `ConnectionGeneration`; stories not yet created); `production/epics/participant-state-roster` (`RequestTransition`, `Disconnect` reason; stories not yet created); `production/epics/crowd-npc-behaviour` (takeover request and `ReleaseToCrowd`, faked here; stories not yet created); `production/epics/shared-character-locomotion` (kind-blind possession and hand-over pass; stories not yet created); `production/epics/tuning-data-config` (`DisconnectGraceSec`, `bHandOverOnUnresponsive`, module skeleton and StealthTestHarness bootstrap; stories not yet created)
