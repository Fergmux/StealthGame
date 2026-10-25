# Story 078: AC 35 RingMonotonicClamp

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
**ADR Decision Summary**: ADR-0011 makes owner prediction presentation only: the interact ring (F4) is predicted from the local press and re-based on `FHoldStatus`, while the server alone decides completion from queue-scheduled hold timers. ADR-0002 section 9 supplies the only server-time source presentation may use, `EstimatedServerNow`, which is monotonic (slewed at most 50 ms/s, stepped only on first sync or error above 250 ms).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: The ring model is plain arithmetic with no post-cutoff API. ADR-0011 Decision section 9 ("Owner prediction is presentation only") and ADR-0002's knowledge risk (MEDIUM; the client clock carries [U] items) apply. The model must be a plain-C++ function with an injected clock so the headless ADR-0008 harness can run it.

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — source: ADR-0011
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp; never debounce releases client-side — source: ADR-0011
- Required (Foundation, ADR-0002 Implementation Guidelines): Client presentation schedules only from `EstimatedServerNow()`; never `GetServerWorldTimeSeconds()`, the arrival time of a replicated value or a client wall clock — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 35 (F4, Logic, U `RingMonotonicClamp`): **GIVEN** RTT_est 150 ms on a 30 Hz host **WHEN** no confirmation arrives **THEN** the ring hides at T_ack = 0.483 s; **AND** it never exceeds 0.98 before `Outcome=Completed`; **AND** it never decreases, including at the moment of confirmation.

---

## Implementation Notes

*Derived from GDD F4 (Formulas, "F4. Owner ring prediction and confirmation timeout") and ADR-0011 Decision section 9:*

**What this story builds.** A plain-C++ owner ring model implementing F4: `ring_local = clamp((t_local - t_press) / D, 0, 0.98)`; before confirmation `ring = ring_local`; after confirmation `ring = max(ring_local, clamp((ServerNowEst - ServerStartTime) / D, 0, 0.98))`; `ring = 1.0` only on `FHoldStatus.Outcome = Completed`; the ring hides if still unconfirmed after `T_ack = 2 x RTT_est + dt_max + AckSlackSec` (which covers one reliable resend). Example in the GDD: RTT 150 ms on a 30 Hz host gives T_ack = 0.30 + 0.033 + 0.15 = 0.483 s, and an Interact pressed at t = 0 shows 0.16 at 0.483 s. The model is monotonic by construction (the server estimate can only correct it forward) and never reaches 1.0 on prediction alone.

**Why here, and where it lives.** The ring is client presentation only (GDD UI Requirements item 1; ADR-0011 section 9: owner prediction is presentation only, the client never decides completion). Putting the formula in a headless Core function lets this Logic story pin the clamp and the T_ack rule without PIE; the widget (HUD epic, ADR-0021 L0 leaf) only draws the number. Planned location: a pure function in `Source/StealthGameplay/Public/View/Core/` beside ADR-0021's `OwnerContextGate.h` convention (name planned: `HoldRingModel`). The model takes `t_local` and `ServerNowEst` as injected values and must not read a wall clock.

**Test notes (qa-lead).** Use an injected clock; read `AckSlackSec` from tuning data (the example in F4 gives T_ack 0.483). Assert the ring is hidden at the first tick with t >= 0.483 (within one injected dt), that progress never exceeds 0.98 before `Outcome=Completed`, and that the sequence is non-decreasing, including across a confirmation tick. Edge cases: confirmation at t = 0.2 with a server start earlier than the client estimate (the clamp holds, the ring does not drop); confirmation at 0.482 versus 0.484; dt of 1/60 and 1/30.

**Prototype.** New code, no prototype counterpart (the prototype `StealthCharacter` interaction shows progress from replicated state with no prediction model).

**Tuning keys (defaults).** `AckSlackSec` 0.15 (hard 0.05-0.5, safe 0.1-0.25; Audience All, Latch Use). `ObjectiveInteractSec` D = 3.0 is Objectives-owned and only consumed.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-079-ring-resync-and-late-confirm.md`: server-time drift resync, late `FHoldStatus` after the ring hid, and "no predicted ring branch" for Prepare.
- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: delivery of `FHoldStatus`.
- `production/epics/input-interaction-framework/story-074-brace-protected-only-on-confirm.md`: the brace indicator, which never uses the predicted branch.
- `production/epics/input-interaction-framework/story-075-prompt-legality.md`: the ring colour rule (Bone, then Marigold after `TendCueDelaySec`).

---

## Files

- `Source/StealthGameplay/Public/View/Core/HoldRingModel.h` (planned)
- `Source/StealthGameplay/Private/Core/HoldRingModel.cpp` (planned)
- `Source/StealthGameplay/Private/Tests/Input/RingMonotonicClampTest.cpp` — class `FInputRingMonotonicClampTest`, name `StealthGame.Input.RingMonotonicClamp`

---

## QA Test Cases

**Automated test specs:**

- **AC-1**: Ring hides at T_ack and never exceeds 0.98 or decreases (`StealthGame.Input.RingMonotonicClamp`).
  - Given: F4 ring model, RTT_est 0.150, host 30 Hz (dt_max 0.0333), `AckSlackSec` from tuning (the F4 example gives T_ack 0.483), injected clock.
  - When: a Begin is sent at t = 0 and no confirmation arrives.
  - Then: the ring is hidden at the first tick with t >= 0.483 (within one injected dt).
  - Given: progress sampled every injected tick.
  - Then: progress never exceeds 0.98 before `Outcome=Completed`; the sequence is non-decreasing, including across a confirmation tick.
  - Edge cases: confirmation at t = 0.2 with server start earlier than the client estimate (clamp holds, no drop); confirmation at 0.482 versus 0.484; dt of 1/60 and 1/30.

*QL-STORY-READY: ADEQUATE (qa-lead).*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/RingMonotonicClampTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; cross-epic: round-match-framework (stories not yet written; ADR-0002 `EstimatedServerNow` estimator); cross-epic: tuning-data-config (stories not yet written; module skeleton, `AckSlackSec` knob)
- Unlocks: `production/epics/input-interaction-framework/story-079-ring-resync-and-late-confirm.md`
