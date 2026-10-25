# Story 011: Concealed-press refusal end to end

> **Epic**: Informant Pings & Team Communication
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/informant-pings-team-comms.md` (PNG-R2, PNG-R3 exception, PNG-R5 step 2, ruling D26 and D33, edge case on concealed sender; AC-PNG-17)
**Requirement**: `TR-png-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (Technical Setup 2026-10-25: server `IsInBushOrTransition` and owner `IsOwnPawnConcealedForPing()`), with ADR-0011 §9 (gate) and ADR-0021 (refusal plate and tone through `PingView`)
**ADR Decision Summary**: The ping concealment gate uses Concealment's server `IsInBushOrTransition` (true for every state but `Outside`, so `Emerging` counts) and, on the owning client, `IsOwnPawnConcealedForPing()` (owner `bIsConcealed`, own hide or bush-strike `FActionState` before recovery ends, or an unanswered `Hide(Enter)` with a 1.0 s backstop); the owner query is fail-closed and adds no replicated field. Only a concealed refusal gives owner feedback (the HUD's "Not now" plate and the 2D `PingConcealedRefused` tone), observed by Presentation only through `ConcealedRefusalSeq`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; the predicates are plain gameplay state, no post-cutoff API)
**Engine Notes**: ADR-0017 spikes S-CON-1 (owner-side warp) and R5 (hidden-primitive leak matrix) gate the concealment mechanics themselves, not this predicate use; the ping tests substitute a concealment fixture that sets each bush state directly, so they do not wait for those spikes.

**Control Manifest Rules (this layer)**:
- Required: the ping concealment gate uses the server `IsInBushOrTransition` and the owner `IsOwnPawnConcealedForPing()`; the owner query is fail-closed and adds no replicated field — source: ADR-0017
- Required: `FHudInputs.PingConcealedRefusalSeq` is sampled every frame from `PingView`; a change submits one `NotNow` refusal to the existing `RejectPlateCooldownSec` limiter, which shows the plate and plays the owner-only 2D cue together or neither — source: ADR-0021
- Forbidden: Never answer a dropped `ServerTeamPing` with a reply, client RPC, replicated field or client-visible log row; the presenter never reads concealment state — source: ADR-0011, ADR-0021

---

## Acceptance Criteria

*From GDD `design/gdd/informant-pings-team-comms.md` (AC-PNG-17, PNG-R2, PNG-R3, PNG-R5), scoped to this story:*

- [ ] **AC1** (AC-PNG-17 a-d, `[Integration]` `StealthGame.Pings.PIE.ConcealedRefusal`): **GIVEN** an `Active` sender in (a) `Concealed`, (b) `Entering`, (c) `Exiting` before the outward crossing, (c2) `Emerging`, (d) outside any bush **WHEN** it presses `IA_Ping` aimed at ground 20 m away **THEN** (a)-(c2) send nothing, spend no token, play the owner-only refusal tone and show the "Not now" plate (once per `RejectPlateCooldownSec`); (d) is sent and accepted.
- [ ] **AC2** (AC-PNG-17 forged, server step 2): **GIVEN** a forged `ServerTeamPing` from a concealed sender in each of those states (`Emerging` included) **WHEN** it arrives **THEN** it is dropped at step 2 with drop reason `Concealed`, no reply and no token spent, and the client pre-check and the server step 2 return the same answer for every bush state.
- [ ] **AC3** (AC-PNG-17 observability): **GIVEN** refused presses **WHEN** observed **THEN** `IPingClientView::GetConcealedRefusalSeq()` rises by one per `RefusedConcealed` press (monotonic, non-replicated) and the HUD consumes it through `FStealthClientViewHandles::PingView`, the press never changes concealment, and the team's other clients' and the opponents' replicated-property logs, RPC lists, HUD dumps and audio logs equal a control run without the presses.

---

## Implementation Notes

*Derived from ADR-0017 Technical Setup 2026-10-25, ADR-0011 Technical Setup 2026-10-25b and ADR-0021 Technical Setup 2026-10-25b:*

Server: bind the injected concealment predicate of story 005 to `UConcealmentService::IsInBushOrTransition(Pawn)` at the composition root; the step-2 drop reason is `Concealed`. Client: the controller passes `IsOwnPawnConcealedForPing()` into the gate (story 009); on `RefusedConcealed` it increments `ConcealedRefusalSeq` on the owner's `UTeamPingViewComponent` and sends nothing. The HUD side (`FHudInputs.PingConcealedRefusalSeq` sampled each frame, `LastSeenRefusalSeq` set at `Bind`, the shared `RejectPlateCooldownSec` limiter, the `PingConcealedRefused` cue) is built by the HUD epic; this story supplies the counter and tests the end-to-end result against the HUD's real limiter when the HUD stories have landed, and against a fake `PingView` consumer until then. The refusal tone and plate are owner-only; verify with the AC-PNG-01-style dumps that nothing reaches any other machine. A ping pressed just before entering a bush is accepted and its ring stays (PNG-R10); assert that too as an edge case of AC1 (d).

The predicate is the same function on both machines (fail-closed on the client, so within a transition the client may refuse a ping the server would accept; it can never send one the server would drop as `Concealed`).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005 and 009: the core ordering and the gate function.
- concealment-bushes epic: `IsInBushOrTransition`, `IsOwnPawnConcealedForPing()` and the bush state machine.
- hud epic: the "Not now" plate, the `RejectPlateCooldownSec` limiter and the `PingConcealedRefused` tone asset (`production/epics/input-interaction-framework/story-076-rejection-plate.md` is the IIF analogue).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingService.cpp` (bind the concealment predicate)
- `Source/StealthGame/Private/StealthPlayerController.cpp` (pass `IsOwnPawnConcealedForPing()`; increment the seq on `RefusedConcealed`)
- `Source/StealthGameplay/Private/Features/Pings/StealthTeamPingView.cpp` (`ConcealedRefusalSeq`)
- `Source/StealthTestHarness/Private/Pings/ConcealedRefusalTest.cpp` (CQTest PIE, `NETWORK_TEST_CLASS(ConcealedRefusal, "StealthGame.Pings.PIE")`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Test: AC1 client refusal in every bush state (AC-PNG-17 a-d)**
- Given: an `Active` sender in (a) `Concealed`, (b) `Entering`, (c) `Exiting` before the outward crossing, (c2) `Emerging`, (d) outside any bush.
- When: press `IA_Ping` aimed at ground 20 m away.
- Then: (a)-(c2) send nothing, spend no token, trigger the owner-only refusal tone event and show the "Not now" plate once per cooldown; (d) is sent and accepted.
- Edge cases: a second refused press inside `RejectPlateCooldownSec` (3.0 s, read from the schema) gives the tone but no new plate; a press at 3.0 s gives a plate.

**Test: AC2 forged RPC and predicate parity**
- Given: a forged `ServerTeamPing` from a concealed sender in each of the four states.
- When: it arrives.
- Then: dropped at step 2, reason `Concealed`, no reply, no token spent; the client pre-check and server step 2 agree for every bush state.
- Edge cases: `Emerging` included; a state added to the bush enum must fail the parity test until classified.

**Test: AC3 refusal sequence and no leak**
- Given: repeated refused presses against a control run.
- When: observe `IPingClientView::GetConcealedRefusalSeq()`, concealment state, and teammates' and opponents' logs.
- Then: the seq rises by exactly one per `RefusedConcealed` press (monotonic, non-replicated); the HUD reads it via `FStealthClientViewHandles::PingView`; concealment unchanged; other clients' property logs, RPC lists, HUD dumps and audio logs equal the control run.
- Edge cases: a refused press does not raise the seq when the phase fails (`RefusedSilent`).

QL-STORY-READY: ADEQUATE (the "Not now" plate's retained screenshot is captured in story 021 AC4)

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Pings/ConcealedRefusalTest.cpp` OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/informant-pings-team-comms/story-005-server-validation-core.md`, `story-009-client-gate-and-bucket-mirror.md`, `story-010-ia-ping-action-and-send-path.md`; cross-epic concealment-bushes epic (predicates), hud epic (plate and tone consumer)
- Unlocks: `production/epics/informant-pings-team-comms/story-013-opponent-sees-nothing.md` (concealed-press control)
