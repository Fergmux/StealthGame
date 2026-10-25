# Story 067: A bound AI-driven pawn's Interact, Prepare, Attack and Hide are rejected with BoundAiForbidden and nothing is enqueued

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 25; rule R17.1 (participant-bound AI constraint, binding on Crowd), R23 (e).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity; ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0010: one `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator and possessor, kind-blind possession and hand-over pass, disconnect never destroys a pawn. ADR-0005: four game-instance stores and Session reporting facts while Round decides policy. This story applies the rule that a pawn bound to a participant and AI-driven (hand-over or grace) issues only `Move` and `Look`, so an absent human is never struck out, reversed or exposed by a script (Round R17.1; ADR-0002 section 8.7 (1)).
**ADR Version**: ADR-0010 2026-10-24; ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 knowledge risk)
**Engine Notes**: None touched by the unit test. It uses a fake bound AI-driven pawn and a recording command sink; the ADR-0010 verification items (traversal, S8 `PawnLeavingGame`, S14 owner-only pointers, H1 AI-to-player hold, ST1, P1) concern real possession and movement and are not exercised here.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules; the pawn-side rules in the Core Layer Rules):
- Required: A participant-bound AI-driven pawn (hand-over or grace) issues only `Move` and `Look` (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never give a participant-bound AI-driven pawn `Interact`, `Prepare`, `Attack` or `Hide`: it would complete objectives or change concealment for an absent player (ADR-0002).
- Guardrail: Command interface: stage 2 a few microseconds per frame inside the 0.5 ms queue line, at most one reply RPC per connection per drain (ADR-0011); the rejection path adds no queue item and no reply.

**Prototype disposition**: none (new Round Core code). The prototype has no participant-bound AI driving.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 25, scoped to this story:*

- [ ] GIVEN a bound AI-driven pawn, WHEN its controller submits `Interact`, `Prepare`, `Attack` or `Hide` through the pawn command interface, THEN each is rejected with `BoundAiForbidden` and nothing is enqueued (the 120 s soak with a fixed seed moves to the Crowd GDD's criteria).

---

## Implementation Notes

*Derived from GDD R17.1 and ADR-0002 section 8.7 (1), with the pawn-side rules of ADR-0010:*

- **The rule.** A pawn that is bound to a participant and AI-driven (a hand-over after `OnParticipantUnresponsive`, or the grace window after `OnParticipantDisconnected`, informant-bound pawns included) accepts only `Move` and `Look`. `Interact`, `Prepare`, `Attack` and `Hide` are refused; the pawn never completes an objective and never starts or ends concealment. If the pawn is concealed when control passes it stays hidden until killed or released to the crowd (that half is Crowd's behaviour and Concealment's, not tested here).
- **Refusal code.** `BoundAiForbidden` is the refusal the AC names; add it to the closed refusal set Round and the pawn command interface share. Refusal is not an enqueue: the rejected command must not reach `UResolutionQueue::Submit`, so the queue (key counter, mailboxes, `Seq`) is unchanged. `Move` and `Look` are continuous inputs and never queue items (R12), so they are accepted without any queue effect.
- **Predicate.** "Bound and AI-driven" comes from the pawn's participant binding (Roster) and its control kind (the factory's `EControlKind`, ADR-0010), never from the controller class or PlayerState (kind-blind rule, Round R23 (a)). The test supplies both through the fake Roster and a fake pawn; no singleton access.
- **Control epoch.** A hand-over bumps the pawn's control epoch and commands stamped under the old epoch void at re-validation (ADR-0010); this story does not test that path (story 090 does), only the AI-driven refusal.
- **Open: confirm against ADR-0011 and ADR-0016 where `BoundAiForbidden` is produced.** ADR-0011 (pawn command interface) states that the participant-bound `Move`/`Look` rule is "not enforced by a kind check here" but is Crowd's behaviour constraint, and ADR-0016 enforces it by construction with the restricted `FCrowdMotionSink` (Move and Look only). AC 25 asks for a rejection with a named code at the pawn command interface. Resolve this at story readiness: either the interface gains a defensive guard that returns `BoundAiForbidden` (this story's reading, the guard being a second line behind Crowd's restricted sink), or the AC is re-expressed against the sink. The story must not guess; no ADR amendment is made here.
- **Out of the soak.** The 120 s fixed-seed soak of a bound AI-driven pawn moves to the Crowd GDD's criteria (AC text); do not add a soak here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 042 (`production/epics/round-match-framework/story-042-active-gate.md`): Active entry and the `NotNow` reply for commands in Preparing (this story's dependency).
- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline that makes a pawn AI-driven.
- Story 090 (`production/epics/round-match-framework/story-090-hand-over-and-back.md`): hand-over and hand-back blending (Blocked on spike S14 and H1).
- The restricted `FCrowdMotionSink` and the StateTree behaviour (crowd-npc-behaviour epic) and the IIF validation stages and reply batching (input-interaction-framework epic).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Commands/BoundAiCommandGuard.h`
- `Source/StealthGameplay/Private/Round/Commands/BoundAiCommandGuard.cpp`
- `Source/StealthGameplay/Private/Tests/Round/BoundAiMoveLookOnlyTest.cpp` (class `FRoundBoundAiMoveLookOnlyTest`, test name `StealthGame.Round.BoundAiMoveLookOnly`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A bound AI-driven pawn's Interact, Prepare, Attack and Hide are rejected with BoundAiForbidden and nothing is enqueued.
  - Given: a bound AI-driven pawn.
  - When: its controller submits `Interact`, `Prepare`, `Attack` and `Hide` through the pawn command interface.
  - Then: each is rejected with `BoundAiForbidden`.
  - Then: the queue is unchanged, with nothing enqueued.
  - Edge cases: `Move` and `Look` are still accepted; one case per command (four cases). Open: confirm against ADR-0011 / ADR-0016 where the refusal is produced (see Implementation Notes).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/BoundAiMoveLookOnlyTest.cpp` (automation test `StealthGame.Round.BoundAiMoveLookOnly`, class `FRoundBoundAiMoveLookOnlyTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-003-test-seam-fake-gateways.md`, `production/epics/round-match-framework/story-042-active-gate.md`
- Unlocks: None
- Cross-epic: `production/epics/input-interaction-framework` (pawn command interface and its refusal codes; stories not yet created), `production/epics/crowd-npc-behaviour` (restricted driver for bound pawns; stories not yet created), `production/epics/shared-character-locomotion` (`EControlKind` and the factory; stories not yet created), `production/epics/participant-state-roster` (participant binding query; stories not yet created)
