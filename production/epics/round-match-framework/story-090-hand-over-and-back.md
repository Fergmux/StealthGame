# Story 090: Unresponsive at 3 s hands over once without a Roster transition, cancels holds correctly and hands back on Responsive

> **Epic**: Round & Match Framework
> **Status**: Blocked — BLOCKED: ADR-0010 spikes S14 (per-tick deceleration limit) and H1 have not reported. Unblocks when S14 reports and `HandOverPerTickLimit` is filled and H1 reports; then re-check the story with `/story-readiness`.
> **Layer**: Core
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 50 ([U] [SPIKE]); rules R17.1, R17.2, R17.3, R13.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity; ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0010: a possession change of a live pawn is committed in a queue slot (Round's disconnect policy item), bumps the control epoch at commit and executes in the apply-only hand-over pass (`FPawnHandoverTickFunction`, `TG_PrePhysics`) at the next frame start, never inline in the slot and never during a traversal; Player to AI uses `ICrowdControl::DriveBoundPawn` so a running pawn slows through the baseline deceleration. ADR-0005: Session reports the liveness facts (`OnParticipantUnresponsive`, `OnParticipantResponsive`, `OnParticipantRejoined(Superseded)`); it never calls `RequestTransition`, and Round decides.
**ADR Version**: ADR-0010 2026-10-24; ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 knowledge risk)
**Engine Notes**: Touches ADR-0010 Verification items **S14** (shared with ADR-0001: owner-only pointers survive every possession path; the GDD ties the per-tick deceleration limit to it) and **H1** (AI to player hand-over gap). The Round-side assertions run on a fake clock and fake gateways (no engine API); only the speed clause may need a PIE run (see Implementation Notes). `FPawnHandoverTickFunction` is a custom tick function in `TG_PrePhysics`, registered on the authority in `OnWorldBeginPlay`; its behaviour inside the engine is not exercised by the [U] clauses. No post-cutoff API is called directly.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules; the possession rules are Core):
- Required: Soft silence (`SessionSoftTimeoutSec`): with `bHandOverOnUnresponsive`, an `Active` participant's bound pawn is handed to Crowd by the normal blend and handed back the same way on `OnParticipantResponsive`; a hand-over is committed in the drain as the apply-only hand-over pass, bumps the pawn's control epoch, takes effect at the next frame start and is never committed during a traversal (ADR-0002).
- Forbidden: Never execute a hand-over inline in the queue slot (ADR-0010).
- Guardrail: Movement shaping: well under 5 % of the 3.0 ms CMC game-thread line (40 server-simulated NPCs + proxies) (ADR-0010).

**Prototype disposition**: None (new Core code; the prototype has no hand-over or Unresponsive handling).

**Where it sits**: Exit B (Core), part of the disconnect policy (R17.1 to R17.3). It builds on story 060 (grace timeline) and its Round-side assertions are unit-level; the one speed clause is spike-gated and may move to PIE. It is Blocked only because the per-tick limit and the hand-over gap are spike outputs.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 50 (revised in `rmf-ac-overrides.md`), scoped to this story:*

- [ ] Round-side assertions: one hand-over and no Roster transition at Unresponsive 3 s with `bHandOverOnUnresponsive`; a held Interact is cancelled; a held Prepare is cancelled with the cooldown not refunded; Responsive hands back; a supersede does not hand back until `Rejoined(Superseded)` at rejoin completion; with the flag false no hand-over occurs; and the speed decreases monotonically to crowd pace with each per-tick decrease at most `HandOverPerTickLimit` (fixture constant filled when spike S14 reports; if the blend lives only inside Crowd/pawn movement, this one clause runs in PIE).

---

## Implementation Notes

*Derived from ADR-0010 sections 4.3-4.4 and Implementation Guidelines, ADR-0005 sections 5-6 and GDD R17:*

- **Unresponsive hand-over (R17.1).** `OnParticipantUnresponsive` (silence >= `SessionSoftTimeoutSec`, 3 s) with `bHandOverOnUnresponsive` (default on) and an `Active` participant with a bound pawn: Round asks Crowd to drive the pawn through `ICrowdControl::DriveBoundPawn` (entering by the normal blend) and calls `RequestHandOver(Pawn, Ai, ExpectedEpoch)`. Commit happens in the drain; execution is at the next frame start; the control epoch is bumped at commit (commands stamped under the old epoch void at re-validation); never during a traversal. **No Roster transition**: the participant stays `Active` (assert zero `RequestTransition` calls on the fake Roster gateway). The same applies to an `Unresponsive` informant with a bound pawn.
- **Holds.** An interaction (`Interact`) held at hand-over is cancelled with its ordinary cancel animation; a held `Prepare` is cancelled and its 60 s cooldown is already spent (not refunded). Assert on the fake IIF/hold gateway's call log.
- **Hand-back.** `OnParticipantResponsive` hands control back by the reverse blend (`RequestHandOver(Pawn, Player(Participant), ExpectedEpoch)`). A **supersede** does not hand back: it is a disconnect (R17.2), so the pawn stays AI-driven until the rejoin completes, and control returns at `OnParticipantRejoined(Superseded)` at rejoin completion (R17.3). Assert no hand-back call before that event.
- **Flag false.** With `bHandOverOnUnresponsive` false, no hand-over occurs at `Unresponsive` (a Hard-timeout disconnect still takes over at once).
- **Speed clause.** Speed decreases monotonically to crowd pace with each per-tick decrease at most `HandOverPerTickLimit`, a fixture constant filled when spike S14 reports; an unfilled constant must fail the test. QA's condition: this assertion is valid in [U] only if the test drives the real hand-over blend (a pure speed-limiter object) on the fake clock. If the blend exists only inside Crowd or pawn movement, a fake would make the assertion tautological, so move that one Then line to a PIE run (for example into story 088's harness run). Open: confirm against GDD AC 50 and ADR-0010 section 4.3 where the blend lives once spikes S14 and H1 report.
- **Seams.** Fake `IClock` (story 001), fake Session, Roster, Crowd gateways (story 003), fixture constants (story 005). Do not inline codes or the limit.
- **Gate.** Do not start until S14 and H1 report; `/story-readiness` re-confirms.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and disconnect policy skeleton (this story's dependency).
- Story 077 (`production/epics/round-match-framework/story-077-supersede-grace-basis.md`): the supersede grace basis, the conversion at `t_s + G_eff` and the no-second-blend rule.
- Story 067 (`production/epics/round-match-framework/story-067-bound-ai-move-look-only.md`): the Move-and-Look-only constraint on a bound AI-driven pawn.
- Story 088 (`production/epics/round-match-framework/story-088-takeover-kinematics.md`): observer-side kinematics under net emulation.
- The blend, the hand-over pass and the hold (Shared Character epic), Crowd's `DriveBoundPawn` (crowd-npc-behaviour epic) and hold-cancel mechanics (input-interaction-framework epic).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Disconnect/RoundHandOverPolicy.h`
- `Source/StealthGameplay/Private/Round/Disconnect/RoundHandOverPolicy.cpp`
- `Source/StealthGameplay/Private/Tests/Round/HandOverAndBackTest.cpp` (class `FRoundHandOverAndBackTest`, test name `StealthGame.Round.HandOverAndBack`)
- `Source/StealthFoundation/Private/Tests/Fixtures/StealthRoundTestConstants.h` (fixture constants file from story 005; `HandOverPerTickLimit` added when S14 reports)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: Unresponsive at 3 s hands over once with no Roster transition, cancels holds correctly, hands back on Responsive, and speed decreases within `HandOverPerTickLimit`.
  - Given: an Active participant, the fake clock, and `bHandOverOnUnresponsive` true, then false.
  - When: the client goes Unresponsive at 3 s, then Responsive; separately, a supersede with a later `Rejoined(Superseded)`.
  - Then: with the flag true there is one hand-over and no Roster transition.
  - Then: a held Interact is cancelled.
  - Then: a held Prepare is cancelled with the cooldown not refunded.
  - Then: `Responsive` hands back.
  - Then: a supersede does not hand back until `Rejoined(Superseded)` at rejoin completion.
  - Then: with the flag false no hand-over occurs.
  - Then: speed decreases monotonically to crowd pace, with each per-tick decrease at most `HandOverPerTickLimit`.
  - Edge cases: `HandOverPerTickLimit` is a fixture filled when S14 reports, and an unfilled one must fail the test. The speed assertion is only valid in [U] if the test drives the real hand-over blend (a pure speed-limiter object) on the fake clock. If the blend exists only inside Crowd or pawn movement, a fake would make the assertion tautological, so move that one Then line to a PIE run. The story stays blocked on ADR-0010 spikes S14 and H1.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/HandOverAndBackTest.cpp` (class `FRoundHandOverAndBackTest`, test name `StealthGame.Round.HandOverAndBack`) - must exist and pass. If the speed clause moves to PIE, that clause's evidence is the harness run it moves to (CQTest `NETWORK_TEST_CLASS` per the exemption in `.claude/rules/test-standards.md`).

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None
- Cross-epic: `production/epics/shared-character-locomotion` (hand-over pass, the blend, spikes S14 and H1; stories not yet created), `production/epics/crowd-npc-behaviour` (`DriveBoundPawn`, blend path; stories not yet created), `production/epics/session-travel` (`OnParticipantUnresponsive`/`Responsive`/`Rejoined(Superseded)`; stories not yet created), `production/epics/participant-state-roster` (no-transition assertion; stories not yet created), `production/epics/input-interaction-framework` (hold cancel on hand-over; stories not yet created)
