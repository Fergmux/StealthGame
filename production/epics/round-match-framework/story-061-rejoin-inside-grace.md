# Story 061: A rejoin 1 s before grace expiry keeps the participant Active and repossesses the same pawn

> **Epic**: Round & Match Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void). Also `GDD AC 23a`, `R17` step 3 (rejoin inside grace), `R18` (first bullet, `Active` within grace).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores; ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0005: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration; section 5: a rejoiner adopts its retained PlayerState in `InitPlayerState`, is `Rejoining` until its acknowledgement, and the pawn survives through `PawnLeavingGame`. ADR-0010: one `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator and possessor, a kind-blind possession and hand-over pass, disconnect never destroys a pawn. This story applies the Round side: cancel the grace timer and hand control back by the reverse blend, so the participant keeps pawn and state.
**ADR Version**: ADR-0005 2026-10-24; ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0005's post-cutoff item (5.6 `DispatchPostLogin` deprecation) is not touched. The rejoin is a fake Session event and the hand-back a fake Crowd and possession gateway call, so ADR-0005 S4 (retained PlayerState re-attached to a new controller) and ADR-0010 S8, S14 and H1 (hand-over gap and owner-only pointer survival) are not touched; the real repossession is verified by those spikes and by the PIE evidence in the Blocked stories 087 to 090.

**Control Manifest Rules (this layer)** (the Round-relevant rule sits in the manifest's Foundation Layer Rules, Round phase flow; the Forbidden rule is from the Core Layer Rules, Shared Pawn; the Core Layer Rules hold no other Round-specific rule):
- Required: Disconnect policy: at most one grace timer per participant; the grace timer is the `TimerFired(GraceExpiry)` item carrying the captured `ConnectionGeneration` (ADR-0002).
- Forbidden: Never destroy the pawn in `PawnLeavingGame`; never destroy-and-respawn on disconnect or takeover, since a vanishing or appearing pawn identifies a human (ADR-0010).
- Guardrail: Queue drain + staging + teardown + Roster flush: at most 0.5 ms listen-host game thread (ADR-0002, ADR-0022).

**Prototype disposition**: none (no prototype equivalent: the prototype has no rejoin).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 23a, scoped to this story:*

- [x] GIVEN a rejoin 1 s before grace expiry (fake clock), THEN the participant stays Active and repossesses the same pawn id.

---

## Implementation Notes

*Derived from GDD R17.3 and R18 and ADR-0005 section 5:*

- Rule (R17.3): on `OnParticipantRejoined` for a participant still `Active`, Round cancels the grace timer and hands control back to the human by the reverse blend; the participant keeps their pawn and their state (`Active`, no transition requested). A rejoiner passes Session's acknowledgement first and sees only the loading screen; the rejoin event arrives at rejoin completion, so control never returns before the rejoiner is ready (R17.3 for a supersede; story 077 covers that case).
- Test setup: fake clock; an Active participant disconnects (story 060's grace record exists with `GraceExpiry = max(DisconnectedAt, ActivePhaseStart) + G_eff`); the fake Session raises `OnParticipantRejoined` at `GraceExpiry - 1 s`. Assert: the participant's Roster state is still `Active`; no `RequestTransition` was sent; the fake possession gateway recorded exactly one hand-back request for the same `FPawnHandle` (same pawn id) and no spawn, destroy or card move; the grace timer is cancelled (no live grace item).
- Same pawn id: "repossesses the same pawn id" is asserted on the pawn handle the Round core passes to the hand-back request, which must equal the handle that was AI-driven during grace (the Roster binding is untouched). The actual possession call is the pawn factory's hand-over pass (ADR-0010: `Possess`/`UnPossess` only by the factory, always through `FScopedPossessionParity`), committed in the drain, bumping the pawn's control epoch and taking effect at the next frame start; Round does not possess.
- A rejoin bumps the participant's `ConnectionGeneration` (every `Connection` change does, ADR-0005 section 5), so even if the cancelled timer item were still in the heap it could only return `Rejected_StaleConnection`; that guard is asserted in story 071. Here assert only the cancelled state and the `Active` outcome (QA edge case).
- Boundary: the 1 s margin is the fixture; the AC does not ask for the exact-expiry boundary. A rejoin completing at or after `GraceExpiry` is story 062's case.
- Where it lives: the rejoin handler in the Round Core disconnect policy (planned `Source/StealthGameplay/Private/Round/Disconnect/`), extending the grace record and handlers created by story 060.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and conversion machinery.
- Story 062 (`production/epics/round-match-framework/story-062-rejoin-after-grace.md`): a rejoin after expiry (the informant path).
- Story 071 (`production/epics/round-match-framework/story-071-grace-generation-guard.md`): the stale-generation guard for a timer that survives a rejoin.
- Story 077 (`production/epics/round-match-framework/story-077-supersede-grace-basis.md`): a rejoin that completes a supersede.
- Story 090 (`production/epics/round-match-framework/story-090-hand-over-and-back.md`) (Blocked) and stories 087 to 089 (Blocked): the hand-back blend kinematics and observer evidence.
- Session's acknowledgement and PlayerState adoption (session-travel epic); the Roster's `bReconnecting` derivation (participant-state-roster epic).

---

## Files

*Planned files. Tests live under the engine test roots (`Source/<Module>/Private/Tests/`, see `tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Private/Tests/Round/RejoinInsideGraceTest.cpp` (class `FRoundRejoinInsideGraceTest`, test name `StealthGame.Round.RejoinInsideGrace`)
- `Source/StealthGameplay/Private/Round/Disconnect/` rejoin handler (new, shared with stories 062, 063 and 077)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A rejoin 1 s before grace expiry keeps the participant Active and repossesses the same pawn.
  - Given: the fake clock and a participant in grace.
  - When: it rejoins at `GraceExpiry - 1 s`.
  - Then: the participant stays Active.
  - Then: it repossesses the same pawn id.
  - Edge cases: the stale grace timer later fires and is rejected (see 071).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/RejoinInsideGraceTest.cpp` — must exist and pass (automation test `StealthGame.Round.RejoinInsideGrace`, class `FRoundRejoinInsideGraceTest`)

**Status**: [x] Created and passing (`StealthGame.Round.RejoinInsideGrace`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None

Cross-epic: `production/epics/session-travel` (`OnParticipantRejoined`, acknowledgement; stories not yet created); `production/epics/participant-state-roster` (state stays `Active`, bindings; stories not yet created); `production/epics/crowd-npc-behaviour` (hand-back request, faked here; stories not yet created); `production/epics/shared-character-locomotion` (hand-over pass and possession; stories not yet created); `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)

## Completion Notes
**Completed**: 2026-10-26 (RMF round-end and disconnect batch: stories 048-065, two-step round end, TeamSummary, grace/forfeit/void)
**Criteria**: 1/1 passing (Active, no transition, timer cancelled, one hand-back for the same pawn id, no spawn/bind/card)
**Deviations**: None
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Round/RejoinInsideGraceTest.cpp`; full suite: `production/qa/evidence/rmf-round-end-disconnects-2026-10-26/full-suite.txt`
**Run result**: PASS - PIE rejoin inside grace keeps the Active role (see evidence)
**Reviewed by**: lead-programmer focused review (sonnet) + self-review, RMF round-end batch; findings and fixes in production/qa/evidence/rmf-round-end-disconnects-2026-10-26/evidence.md
