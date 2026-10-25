# Story 087: Takeover, hand-over, hand-back, ReleaseToCrowd, re-pawn and pawn-loss respawn show no identity-bearing pawn property delta

> **Epic**: Round & Match Framework
> **Status**: Blocked — BLOCKED: ADR-0010 spikes S8 and H1 and the OQ11(a) PlayerState/Controller replication spike have not reported. Unblocks when those spikes report (S8: `PawnLeavingGame` keeps the pawn; H1: AI to player hand-over gap; OQ11(a): the shared pawn does not replicate `PlayerState` and `Controller` to non-owners) and fill `CrowdBaselinePawnDeltaSet`; then re-check the story with `/story-readiness`.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (Disconnect policy: blend hand-over, guarded grace timer, forfeit/void), `TR-round-009` (`FRoundPublicState` identical bytes for every viewer; `ActiveAtServerTime` zeroed in Active; bounded late-loader reveal); GDD AC 27b ([PIE-3] [SPIKE]); rules R17, R18, R23 (e) and (f).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity; ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0010: one `AStealthPawn` on CMC, `UStealthPawnFactory` as the only creator and possessor, kind-blind possession and hand-over pass, disconnect never destroys a pawn; possession may change only the controller pointer, `Owner`, `PlayerState` (all three `COND_OwnerOnly`), the control rotation and owner-only setup (section 4.1). ADR-0003: the allowlist audit lists `PlayerState`/`Controller`/`Owner` as `COND_OwnerOnly` pawn rows, so a non-team client sees none of them change. This story proves that on the wire across the six pawn-control transitions.
**ADR Version**: ADR-0010 2026-10-24; ADR-0003 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 knowledge risk)
**Engine Notes**: Touches ADR-0010 Verification items **S8** (the `PawnLeavingGame` override keeps the pawn and a crowd AI controller possesses it in the same server frame on a real connection close, including whether spawning a controller is safe while the connection is being cleaned up), **H1** (the AI to player hand-over gap), and **S14** (owner-only pointers survive every possession path, shared with ADR-0001). Also depends on the GDD's OQ11(a) spike (whether `Owner`/`Controller` export differs on the wire; GDD R23 (f)). The engine pawn replicates `PlayerState` and `Controller` without a condition (verified in the 5.8.4 source per the GDD), so the shared class disables or conditions them. `UMotionWarpingComponent` is Beta in 5.8.4 (ADR-0010) but is not exercised here.

**Control Manifest Rules (this layer)**:
- Required: `Possess`/`UnPossess` are called only by the factory (spawn, hand-over pass incl. `ReleaseBody`, takeover) and `AStealthPlayerController::PawnLeavingGame`, always through `FScopedPossessionParity` (ADR-0010).
- Forbidden: Never destroy the pawn in `PawnLeavingGame`; never destroy-and-respawn on disconnect or takeover - a vanishing/appearing pawn identifies a human (ADR-0010).
- Guardrail: Pawn network: `TraversalState` 7 bytes per change; total within the <= 36 KB/s pawn line (SCL F9) (ADR-0010).

**Prototype disposition**: None (new integration test; no prototype equivalent: the prototype creates and possesses pawns through `RestartPlayer`, which ADR-0010 replaces, and has no hand-over).

**Where it sits**: Exit B (Core), one of the spike-gated Pillar 1 proofs (R23 (e) and (f): possession is invisible on the wire). It depends on the disconnect policy built by story 060; it cannot be built or closed until the ADR-0010 spikes and the OQ11(a) spike report.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 27b (revised in `rmf-ac-overrides.md`), scoped to this story:*

- [ ] The captured property deltas for the pawn (through takeover, hand-over, hand-back, `ReleaseToCrowd`, a C1 re-pawn and a pawn-loss respawn, each captured separately) are a subset of the allowed delta set `CrowdBaselinePawnDeltaSet` (fixture constant filled by the spike: location, rotation, velocity, animation state; an unfilled set fails the test) and contain none of `PlayerState`, `Controller`, `Owner`.

---

## Implementation Notes

*Derived from ADR-0010 sections 3-5 and Implementation Guidelines, ADR-0003 section 8 and GDD R17, R18, R23:*

- **Observation point.** A non-team client (third viewer) captures the replicated property deltas for one pawn (its replication property-change stream, via the harness capture of ADR-0008/ADR-0003 S1/S6 tooling) while the pawn goes through the six transitions, each captured as a separate run so one transition's delta cannot hide another's: (1) takeover (an informant takeover through `TakeOverCrowdPawn`, Penalty's C1 path), (2) hand-over (Player to AI at `Unresponsive`, R17.1), (3) hand-back (AI to player at `Responsive`/rejoin), (4) `ReleaseToCrowd` (grace expiry, R17.4), (5) a C1 re-pawn (R18 informant path), (6) a pawn-loss respawn (R17.9, `RequestEmergence(PawnRecovery)`).
- **Allowed set.** `CrowdBaselinePawnDeltaSet` is a fixture constant in the test constant files (story 005), filled by the spike: location, rotation, velocity, animation state (the deltas an ordinary NPC pawn produces). The subset check is per transition. An unfilled set must fail the test, never pass vacuously.
- **Forbidden deltas.** The captured deltas must contain none of `PlayerState`, `Controller`, `Owner` (nor any other identity-bearing change: class, components, net config, `AppearanceId` re-send, `FActionState` or `HeadLook` jump). ADR-0010 section 4.1 limits possession to the controller pointer, `Owner`, `PlayerState` (all three `COND_OwnerOnly`), control rotation and the server-only control epoch; the control epoch is never replicated. A takeover keeps the `COND_InitialOnly` `AppearanceId` already on the client (no re-send, no `OnRep`; CP AC-CP-18b asserts both on the Character Pool side).
- **Not here.** `FScopedPossessionParity` (development-build capture and verify of velocity, movement mode, `HeadLook` and the last commanded input around `Possess`) is Shared Character's check; this story observes the wire result only.
- **Test shape.** [PIE-3]: host plus at least two clients, one of them the non-team observer; CQTest `NETWORK_TEST_CLASS(TakeoverNoPawnPropertyDelta, "StealthGame.Round.PIE")` in `StealthTestHarness`. Run under the harness `LeakNetProfile` where available.
- **Gate.** Do not start implementation until `/story-readiness` re-confirms the three spikes have reported and the fixture set is filled; record the spike results in the story's Files/notes when unblocking.
- Manifest 2026-10-25f: a takeover keeps the `COND_InitialOnly` `AppearanceId` already on the client (no re-send, no `OnRep`); AC-CP-18b asserts both.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline and disconnect policy this test drives (this story's dependency).
- Story 088 (`production/epics/round-match-framework/story-088-takeover-kinematics.md`): velocity and speed traces through a silent-then-rejoining takeover.
- Story 089 (`production/epics/round-match-framework/story-089-takeover-blind-observer.md`): the human blind-observer test.
- Story 090 (`production/epics/round-match-framework/story-090-hand-over-and-back.md`): Round-side hand-over assertions.
- Story 080 (`production/epics/round-match-framework/story-080-no-leak-before-ended.md`): the allowlist diff and `FRoundPublicState` identity.
- The pawn possession mechanics (Shared Character epic), Crowd's takeover and release behaviour (crowd-npc-behaviour epic) and the allowlist rows for the pawn.

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthTestHarness/Private/Round/TakeoverNoPawnPropertyDeltaTest.cpp` (CQTest `NETWORK_TEST_CLASS(TakeoverNoPawnPropertyDelta, "StealthGame.Round.PIE")`, full name `StealthGame.Round.PIE.TakeoverNoPawnPropertyDelta.<Method>`; the GDD scenario name is `StealthGame.Round.TakeoverNoPawnPropertyDelta`)
- `Source/StealthFoundation/Private/Tests/Fixtures/StealthRoundTestConstants.h` (fixture constants file from story 005; `CrowdBaselinePawnDeltaSet` added when the spike reports)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: A pawn's captured property deltas are a subset of `CrowdBaselinePawnDeltaSet` and contain none of `PlayerState`, `Controller` or `Owner`.
  - Given: a non-team client capturing property deltas for a pawn, and `CrowdBaselinePawnDeltaSet` (location, rotation, velocity, animation state).
  - When: the pawn goes through takeover, hand-over, hand-back, `ReleaseToCrowd`, a C1 re-pawn and a pawn-loss respawn.
  - Then: the captured deltas for that pawn are a subset of `CrowdBaselinePawnDeltaSet`.
  - Then: they contain none of `PlayerState`, `Controller`, `Owner`.
  - Edge cases: the set is filled by the spike, and an unfilled set must fail the test, not pass vacuously; each of the six transitions is captured separately; the story stays blocked on ADR-0010 spikes S8 and H1 and the OQ11(a) spike.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Round/TakeoverNoPawnPropertyDeltaTest.cpp` (CQTest `NETWORK_TEST_CLASS(TakeoverNoPawnPropertyDelta, "StealthGame.Round.PIE")` per the CQTest exemption in `.claude/rules/test-standards.md`) - must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None
- Cross-epic: `production/epics/shared-character-locomotion` (possession, hand-over pass, the ADR-0010 spikes S8, H1; stories not yet created), `production/epics/crowd-npc-behaviour` (takeover blend, `ReleaseToCrowd`, `CrowdBaselinePawnDeltaSet` source; stories not yet created), `production/epics/participant-state-roster` (bindings and transitions; stories not yet created), `production/epics/session-travel` (disconnect and rejoin facts; stories not yet created)
