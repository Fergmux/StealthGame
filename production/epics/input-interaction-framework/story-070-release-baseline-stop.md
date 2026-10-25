# Story 070: AC 24 ReleaseBaselineStop

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Every body in the city is one `AStealthPawn : ACharacter` on the CharacterMovementComponent. `UStealthMovementComponent` implements the SCL movement model once, for every pawn: one input path into the CMC, one pace quantiser, one acceleration and braking rule (including the stop floor `d_eff = min(d, v / StopMinSec)`), identical on the server and in owner prediction.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0010 HIGH; ADR-0011 input layer HIGH)
**Engine Notes**: ADR-0010 E8 lists the CMC override points this story exercises: `GetMaxSpeed`, `GetMaxAcceleration`, `GetMaxBrakingDeceleration`, `ScaleInputAcceleration`. Prediction state (spike P1, owner-versus-server divergence at 100 ms lag) and starvation braking (spike ST1) are ADR-0010 open items that can affect how the owner's release looks, but the AC measures the server pawn's stop; neither P1 nor ST1 is an IIF verification item V1-V6, and neither blocks this story.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand` — source: ADR-0011
- Required: The Shared Pawn uses `UStealthMovementComponent` (a CMC subclass) — source: ADR-0010
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn — source: ADR-0011
- Guardrail: Movement shaping well under 5 % of the 3.0 ms CMC game-thread line (40 server-simulated NPCs + proxies) — source: ADR-0010

This story is the movement-parity check of IIF R3: a human who releases every movement key must come to rest exactly as an NPC does, with Locomotion's baseline deceleration and no sudden stop. A digital WASD release is a step input (the key is either down or up), so unless the movement component shapes it, a human's release would be an instant stop that no NPC ever performs, which is a perfect identity tell (art bible 5.5 "sudden stop"). The criterion therefore compares the time from the first all-zero-input frame to rest with Locomotion's NPC baseline stop time (about 0.5 s provisional, from the SCL GDD), within one frame, and requires that peak deceleration does not exceed the NPC baseline.

It depends on story 017 (walk-slower scaling) because the human Move path (client scaling of the keyboard Move vector before `ServerMove`) must exist, and the full-walk release in this story is the baseline against which story 018's walk-slower accel/brake clauses are compared. It relates to GDD R3 and R15a, to F5's latency note (Look is local) and to ADR-0010 section 6 (acceleration, braking, turning, the stop floor); ADR-0011 section 2 fixes the transport (Move only in `ServerMove`). The cross-epic dependency on shared-character-locomotion's NPC baseline stop value does not block the story, but a missing value must make the test fail with a named message, not pass.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 24 (R3, Integration, P `ReleaseBaselineStop`): **GIVEN** walking at full walk speed **WHEN** all movement keys are released **THEN** time to zero speed equals Locomotion's NPC baseline stop time (±1 frame; numeric baseline from the Locomotion GDD, about 0.5 s provisional); **AND** peak deceleration does not exceed the NPC baseline.

---

## Implementation Notes

*Derived from ADR-0010 Decision section 6 and Implementation Guidelines, and ADR-0011 Decision sections 2 and 11:*

- One input path into the CMC. Human: the owner's `Move` produces the CMC input vector and the gait bit (`FLAG_Custom_0`, `WantsRun`). AI: `UPawnCommandComponent::Move` on the server writes the same input vector. The CMC consumes both identically. `RequestDirectMove` and `RequestPathMove` fault and do nothing.
- Acceleration, braking, turning: `GetMaxAcceleration` and `GetMaxBrakingDeceleration` give constant acceleration to gait speed in `WalkStartSec` / `RunStartSec`, one walk deceleration set from the slowest pace (`WalkStopSec`) and run braking from `RunStopSec`. Stop floor (closeout D2, SCL OQ15): the effective deceleration is `d_eff = min(d, v / StopMinSec)` with `StopMinSec` = 0.5 s (safe 0.4-0.7, hard 0.2-1.0), so every stop lasts at least 0.5 s; `GetMaxBrakingDeceleration` returns `d_eff` for every pawn (human, AI, starved, hand-over). That is why the human and NPC stop times can match: both come from the same function. The criterion's "about 0.5 s provisional" baseline is therefore read from tuning (`StopMinSec`, `WalkStopSec`), never hard-coded.
- Measurement per the qa-lead (gap 1): "time from the first frame with all movement input zero to the first frame with horizontal speed < 1 cm/s, sampled on the server pawn each frame". The qa-lead's proposed rewrite is unconfirmed; the GDD AC stays the authority. Gap 2: the NPC baseline stop time is provisional; read it from SCL tuning at test time. Gap 3: assume the walk-slower modifier is not held (full walk speed 140 cm/s at defaults).
- Test per the qa-lead: 2-client PIE, owner walking `W` at full walk speed in steady state; release all movement keys; assert stop time equals the SCL NPC baseline stop time within 1 frame; sample deceleration per frame and assert the peak is at most the NPC baseline deceleration. Run 3 repeats, all within tolerance. Releasing at walk-slower pace (0.8) is out of scope here (story 018). If the shared-character-locomotion baseline value is missing, the test fails with a named message.
- Prototype disposition: the prototype's movement input in `StealthCharacter.cpp` (`SetupPlayerInputComponent` Move binds) and its own `UStealthMovementComponent` (the `bUseAccelerationForPaths` path following) are replaced, not ported (ADR-0010 Implementation Guidelines). This test is new code.
- Test seams: the 2-client PIE harness, a scripted key release (`IA_Move` to zero) and the per-frame server speed sampler; the SCL baseline value comes from Locomotion's tuning face (cross-epic).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-017-walk-slower-scaling.md`: the client-side keyboard scaling of Move (0.8 / 0.9 / 0.8 / 1.0).
- `production/epics/input-interaction-framework/story-018-walk-slower-two-client.md`: server-observed walk-slower paces and the accel/brake clauses at those paces.
- `production/epics/input-interaction-framework/story-081-look-latency.md`: Look latency (AC 58).
- `production/epics/input-interaction-framework/story-085-look-parity-observed.md`: observed look parity.
- `production/epics/input-interaction-framework/story-048-planted-brace-ignores-move.md`: Move ignored while bracing.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandComponent.cpp` (the `Move` entry point feeding the CMC input vector; changed only if the test exposes a defect)
- `Source/StealthTestHarness/Private/ReleaseBaselineStopPieTest.cpp` (CQTest `NETWORK_TEST_CLASS(ReleaseBaselineStop, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.ReleaseBaselineStop.<Method>`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Gap 1: "time to zero speed" lacks a zero threshold and a measuring frame (server pawn, or owner client).
- Gap 2: the NPC baseline stop time is provisional (about 0.5 s, SCL GDD); read from SCL tuning at test time, not hard-coded.
- Gap 3: state whether the walk-slower modifier is held (assume none, 140 cm/s full walk).

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "time from the first frame with all movement input zero to the first frame with horizontal speed < 1 cm/s, sampled on the server pawn each frame".

QA-lead test sketch (unconfirmed): `StealthGame.Input.PIE.ReleaseBaselineStop`; Given a 2-client PIE with the owner walking W at full walk speed in steady state, When all movement keys are released, Then the stop time equals the SCL NPC baseline stop time within 1 frame; Given the same run with per-frame deceleration sampled, Then peak deceleration is at most the NPC baseline deceleration. Edge cases: release at walk-slower pace (0.8) is out of scope (story 018); 3 repeats, all within tolerance; a missing shared-character-locomotion baseline value makes the test fail with a named message, not pass.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/ReleaseBaselineStopPieTest.cpp` (CQTest `StealthGame.Input.PIE.ReleaseBaselineStop`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-017-walk-slower-scaling.md`; cross-epic: shared-character-locomotion (stories not yet written; NPC baseline stop time, `UStealthMovementComponent` braking)
- Unlocks: None
