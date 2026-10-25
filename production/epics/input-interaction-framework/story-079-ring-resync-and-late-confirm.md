# Story 079: AC 57 RingResyncAndLateConfirm

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: ADR-0011 keeps owner prediction (the F4 ring, prompt legality) as presentation only, re-based on `FHoldStatus`. ADR-0002 section 9 defines the client server-time estimator: NTP four-timestamp samples, minimum-delay offset, monotonic output slewed at most 50 ms per second, with a step allowed only on first sync or when the error exceeds 250 ms.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: No post-cutoff API. ADR-0002's knowledge risk is MEDIUM (the client clock carries [U] items, accuracy budget under `LeakNetProfile`). The F4 resync threshold (0.25 s) matches ADR-0002's step threshold (250 ms). The model must run headless with injected time.

**Control Manifest Rules (this layer)**:
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — source: ADR-0011
- Required (Foundation, ADR-0002 Implementation Guidelines): Client presentation schedules only from `EstimatedServerNow()`, never from the arrival time of a replicated value or a client wall clock — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 57 (F4, Logic, U `RingResyncAndLateConfirm`): **GIVEN** a server-time drift of 0.3 s **THEN** the estimate re-syncs and the ring does not move backward; **GIVEN** a hidden ring and a late `FHoldStatus` with the latest `ClientCommandSeq` while the key is held **THEN** the ring shows Confirmed; **GIVEN** Prepare **THEN** no predicted ring branch is used.

---

## Implementation Notes

*Derived from GDD F4 and ADR-0011 Decision section 9 ("Owner prediction is presentation only"), ADR-0002 Decision section 9:*

**What this story builds.** The three remaining F4 behaviours on top of the ring model of story 078: (1) `ServerNowEst` is re-synced if drift exceeds 0.25 s, and the ring is monotonic across the resync (the post-confirmation term `max(ring_local, clamp((ServerNowEst - ServerStartTime) / D, 0, 0.98))` can correct forward only); (2) a late `FHoldStatus` carrying the latest `ClientCommandSeq` while the key is still held moves the ring `Hidden -> Confirmed` (after `T_ack` the ring hid, but the reliable resend can still arrive); (3) Prepare does not use the predicted branch: the brace indicator shows only a pending outline until `FHoldStatus` confirms it (GDD UI Requirements item 2), so the ring state for a Prepare must never take a predicted value.

**Why this is its own story.** Story 078 pins the pure clamp and T_ack rule. This story pins the interaction with the clock estimator and with reply timing, which is where a ring can visibly jump backward or stay hidden after a delayed reliable resend. The behaviours hang off ADR-0002's estimator semantics (monotonic, slewed, step above 250 ms), so the test injects drift rather than waiting for a real exchange.

**Test notes and gaps (qa-lead).** "The estimate re-syncs" needs an observable: assert that after an injected drift of 0.3 s (above the 0.25 s threshold) the next sample sets `ServerNowEst` to the true server time within `RTT_est / 2 + dt`, and that ring progress is non-decreasing across the resync. "No predicted ring branch" needs a state-level assertion: the ring state enum never takes the predicted value for Prepare; the first visible state is Pending or Confirmed. Edge cases: drift exactly 0.25 s (no resync) versus 0.26; a late status with an older `Seq` is ignored; negative drift (the estimate moves back, the ring does not). A seam to inject the RTT estimate and clock drift is not owned by another story; add it to the ring model's inputs.

**Prototype.** New code, no prototype counterpart.

**Tuning keys (defaults).** `AckSlackSec` 0.15 (hard 0.05-0.5). The 0.25 s resync threshold is an F4 constant tied to ADR-0002's step threshold, not a knob.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-078-ring-monotonic-clamp.md`: the T_ack hide time and the 0.98 clamp (this story builds on its model).
- `production/epics/input-interaction-framework/story-074-brace-protected-only-on-confirm.md`: the on-screen brace indicator (pending outline, then Protected); this story asserts only the ring-model state.
- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: how `FHoldStatus` reaches the owner.
- `production/epics/input-interaction-framework/story-080-latency-budget.md`: measured latency under net emulation.

---

## Files

- `Source/StealthGameplay/Public/View/Core/HoldRingModel.h` (planned; created by story 078, extended here)
- `Source/StealthGameplay/Private/Core/HoldRingModel.cpp` (planned)
- `Source/StealthGameplay/Private/Tests/Input/RingResyncAndLateConfirmTest.cpp` — class `FInputRingResyncAndLateConfirmTest`, name `StealthGame.Input.RingResyncAndLateConfirm`

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap 1: "the estimate re-syncs" needs an observable.
- Gap 2: "no predicted ring branch" needs a state-level assertion.

*QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority):* after an injected drift of 0.3 s (> 0.25 s threshold) the next sample sets `ServerNowEst` to the true server time within `RTT_est / 2 + dt`, and ring progress is non-decreasing across the re-sync; the ring state enum never takes the predicted value for Prepare, and the first visible state is Pending or Confirmed.

*Draft spec from the qa-lead (unconfirmed), test `RingResyncAndLateConfirm`:* Given a ring with `ServerNowEst` in sync and 0.3 s drift injected, when the next server time sample arrives, then the estimate is within tolerance of server time and progress is non-decreasing. Given a ring hidden after T_ack with the key still held, when a late `FHoldStatus` with the latest `ClientCommandSeq` (Active) arrives, then the ring state is Confirmed and visible. Given a Prepare Begin sent, then the ring state is never Predicted before confirmation. Edge cases: drift exactly 0.25 s (no resync) versus 0.26; late status with an older `Seq` (ignored); negative drift (estimate moves back, ring does not).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/RingResyncAndLateConfirmTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-078-ring-monotonic-clamp.md`; cross-epic: round-match-framework (stories not yet written; ADR-0002 `EstimatedServerNow` estimator)
- Unlocks: None
