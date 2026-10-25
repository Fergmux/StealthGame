# Story 088: A silent-then-rejoining player's pawn shows no zero-velocity run or speed drop beyond the crowd baseline on a non-team client

> **Epic**: Round & Match Framework
> **Status**: Blocked — BLOCKED: ADR-0010 spikes S14, H1 and P1 and the OQ1(d) takeover-blend spike have not reported. Unblocks when those spikes report (S14: owner-only pointers survive every possession path; H1: AI to player hand-over gap; P1: shaping state restored on client prediction; OQ1(d): the takeover blend) and `CrowdStopDecel` is filled from Crowd's derived constant; then re-check the story with `/story-readiness`.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 28a ([PIE-3] [SPIKE]); rules R17.1-R17.3, R23 (e).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: One `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator and possessor, kind-blind possession and hand-over pass, disconnect never destroys a pawn. This story applies section 4.3 and section 5: a Player to AI hand-over is committed in a queue slot, executes at the next frame start, and the crowd controller issues `Move(current heading, Walk)` for `HandoverHoldSec` so a running pawn slows through the baseline run-to-walk deceleration (never a dead stop); AI to player keeps simulating the pawn with the held input until the owner's first `ServerMove`, without touching the CMC's client timestamps.
**ADR Version**: ADR-0010 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 knowledge risk)
**Engine Notes**: Touches ADR-0010 Verification items **S14** (owner-only pointers survive every possession path), **H1** (the AI to player hand-over gap: the server keeps simulating with the held input until the new owner's first `ServerMove` (E7), with no dropped first moves, no owner correction above `PawnSmoothSnapThresholdCm` and no visible stop for observers), **P1** (shaping state restored on client prediction, `FSavedMove_Stealth`) and **ST1** (starvation braking cadence, part of the net-emulation spike, measured by AC-SCL-23). The OQ1(d) takeover-blend spike is the GDD's. `AGameNetworkManager` forced-update values come from `Config/DefaultGame.ini` (CDO), and `ForcePositionUpdate` advances `CurrentClientTimestamp`; neither is called by this story. No post-cutoff API is called directly.

**Control Manifest Rules (this layer)**:
- Required: A live pawn's possession change is committed in a queue slot, bumps the control epoch, waits for landing if `IsInTraversal`, and executes in the hand-over pass at the next frame start; the last-command hold covers every frame until the new controller's first command or `HandoverHoldSec` (ADR-0010).
- Forbidden: Never advance the CMC's client timestamps or call `ForcePositionUpdate` during the AI to player hold - the server would reject the owner's first moves (ADR-0010).
- Guardrail: Movement shaping: well under 5 % of the 3.0 ms CMC game-thread line (40 server-simulated NPCs + proxies) (ADR-0010).

**Prototype disposition**: None (new integration test; no prototype equivalent).

**Where it sits**: Exit B (Core), the kinematic half of the Pillar 1 takeover proofs (R23 (e)): a human who loses connection must not be distinguishable by how their pawn moves. It feeds story 089 (the human blind-observer test on the same clips). Spike-gated.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 28a (revised in `rmf-ac-overrides.md`), scoped to this story:*

- [ ] GIVEN the net profile 100 ms RTT / 1% loss (fixture constant, as 1a), with a baseline of crowd pawns sampled at 30 Hz for 60 s per run over 3 runs, WHEN a player goes silent for 15 s and then rejoins (same sampling on a non-team client), THEN the player's pawn shows no zero-velocity run longer than the baseline's 99th percentile and no one-tick speed drop above `CrowdStopDecel` (fixture constant filled from Crowd's derived constant; an unfilled constant fails the test).

---

## Implementation Notes

*Derived from ADR-0010 sections 4.3 and 5, ADR-0002 section 8 and GDD R17, R23 (e):*

- **Two traces, same sampling.** (1) Baseline: crowd (NPC) pawns sampled at 30 Hz for 60 s per run over 3 runs, on a non-team client. (2) Subject: the player's pawn on the same non-team client while the player goes silent for 15 s (drop the client's packets; the Soft timeout at 3 s hands the pawn to Crowd by the normal blend, R17.1) and then rejoins (inside grace, R17.3: hand back by blend). Sample at 30 Hz.
- **Predicates.** From the baseline compute the 99th percentile of the longest zero-velocity run; the subject must have no zero-velocity run longer than that. Per-tick speed drops are compared at the 30 Hz sample interval: no one-tick drop above the per-tick deceleration allowed by `CrowdStopDecel` (Crowd's derived constant). QA asks to pool the three baseline runs before taking the p99; the AC does not state the pooling. Open: confirm the pooling against GDD AC 28a.
- **Fixtures.** The net profile (100 ms RTT, 1% loss) is a fixture constant, as in story 084 (AC 1a). Open: confirm whether it is the ADR-0008 `LeakNetProfile` (ADR-0002 section 9 describes it as symmetric PktLag 100 ms, PktLoss 2 %) or a separate fixture. `CrowdStopDecel` is filled from Crowd's derived constant when the spike reports; an unfilled value must fail the test, not pass.
- **What this exercises.** The R17.1 blend (no dead stop mid-stride, no sprint into a wall, no single-frame snap), the grace walk, and the AI to player hold on rejoin (H1). A player who gives no input after hand-back sees the pawn brake by the baseline once the hold ends (`WalkStopSec`); that braking must also stay under the `CrowdStopDecel` per-tick limit.
- **Test shape.** [PIE-3]: host plus a non-team client plus the dropped client; CQTest `NETWORK_TEST_CLASS(TakeoverKinematics, "StealthGame.Round.PIE")` in `StealthTestHarness`.
- **Gate.** Do not start until the spikes report and `CrowdStopDecel` is filled; `/story-readiness` re-confirms.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline this run exercises (this story's dependency).
- Story 089 (`production/epics/round-match-framework/story-089-takeover-blind-observer.md`): the human blind-observer test on takeover clips (unlocked by this story).
- Story 087 (`production/epics/round-match-framework/story-087-takeover-no-pawn-property-delta.md`): property deltas rather than kinematics.
- Story 090 (`production/epics/round-match-framework/story-090-hand-over-and-back.md`): the Round-side hand-over assertions on a fake clock.
- The movement model, hand-over pass and hold (Shared Character epic) and Crowd's blend and `CrowdStopDecel` (crowd-npc-behaviour epic).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/TakeoverKinematicsTest.cpp` (CQTest `NETWORK_TEST_CLASS(TakeoverKinematics, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.TakeoverKinematics.<Method>`; the GDD scenario name is `StealthGame.Round.TakeoverKinematics`)
- `Source/StealthFoundation/Private/Tests/Fixtures/StealthRoundTestConstants.h` (fixture constants file from story 005; the net profile and `CrowdStopDecel` added)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A silent-then-rejoining player shows no zero-velocity run beyond the baseline p99 and no one-tick speed drop above `CrowdStopDecel`.
  - Given: the net profile 100 ms RTT / 1% loss (fixture constant), and a baseline of crowd pawns sampled at 30 Hz for 60 s per run over 3 runs.
  - When: a player goes silent for 15 s, then rejoins, with the same sampling on a non-team client.
  - Then: no zero-velocity run is longer than the baseline's 99th percentile.
  - Then: no one-tick speed drop exceeds `CrowdStopDecel`.
  - Edge cases: pool the baseline runs before taking the p99 (Open: confirm against GDD AC 28a, since the AC does not state the pooling); `CrowdStopDecel` is a fixture filled from Crowd's derived constant, and an unfilled one must fail the test; compare the per-tick drop at the 30 Hz sample interval; the story stays blocked on ADR-0010 spikes S14, H1, P1 and the OQ1(d) spike.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/TakeoverKinematicsTest.cpp` (CQTest `NETWORK_TEST_CLASS(TakeoverKinematics, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: `production/epics/round-match-framework/story-089-takeover-blind-observer.md`
- Cross-epic: `production/epics/shared-character-locomotion` (hand-over hold, movement model, spikes S14, H1, P1, ST1; stories not yet created), `production/epics/crowd-npc-behaviour` (blend path, `CrowdStopDecel`, baseline crowd pawns; stories not yet created), `production/epics/session-travel` (silence detection and rejoin; stories not yet created)
