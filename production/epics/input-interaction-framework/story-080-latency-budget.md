# Story 080: AC 36 LatencyBudget

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0011 section 11 states the budget by construction: a human discrete command costs one-way uplink, at most one server frame (the RPC body lands before the same frame's drain) and the drain-end reply flush at the next net update, so `L_confirm = RTT + dt_server + dt_client`; nothing waits for a timer, a second RPC or a client acknowledgement. AC 36 and AC 58 measure it and are the report for verification item V4.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This story reports **V4** (latency under the ADR-0008 net-emulation profiles), together with story 081. Engine fact E10 (server RPC bodies run at the top of the world tick, before the tick groups) is why the command is in its mailbox before the same frame's drain. ADR-0003 E17: on a listen host `NetServerMaxTickRate` has no effect unless `bClampListenServerTickRate` is set, so "host fixed at 30 Hz" must be produced by the harness (for example a frame-rate cap) and the achieved host frame time must be logged in the report.

**Control Manifest Rules (this layer)**:
- Required: Human Move/Look travel only in `ServerMove`; discrete commands only in `AStealthPlayerController::ServerPawnCommand` — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — source: ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection (transport bucket); at most one reply RPC per connection per drain — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 36 (F5, Integration, N `LatencyBudget`): **GIVEN** PktLag 75 ms per direction (RTT 150 ms), a host fixed at 30 Hz and t_interp 0.05 **WHEN** Prepare is pressed 100 times **THEN**, measured on the PIE process clock, the owner's `FHoldStatus` arrives in ≤ 220 ms at p95; **AND** the observer's ActionPose OnRep occurs in ≤ 400 ms at p95.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 11 and Validation Criteria, GDD F5 (Formulas, "F5. Input latency budget"):*

**What this story builds.** A multi-client CQTest network test that measures F5 on the real path (`ServerPawnCommand` -> mailbox -> drain -> reply flush; `FActionState` replication to an observer). F5's outputs: `L_confirm` is 183-200 ms at 150 ms RTT (30 Hz host) and `L_observer = RTT_a/2 + RTT_o/2 + 2 x dt_server + t_interp + U` is 0.18-0.35 s at 150 ms RTT; the AC budgets (220 ms and 400 ms at p95) carry headroom over those. ADR-0011 lists AC 36 in its Validation Criteria and treats V4 as passed only when this story and story 081 report PASS (the V1-V6 gate).

**Test shape (qa-lead).** Host at fixed 30 Hz, PktLag 75 ms per direction, `t_interp` 0.05, an owner client and an observer client. Inject 100 Prepare Begin/End cycles. Measurement origin ("pressed") is the injected Begin send time on the owner client; end points are the `FHoldStatus` receipt on the owner and the ActionPose OnRep on the observer, both read on the PIE process clock. Record the maximum and any loss resends, discard no sample, and write the V4 report (raw samples included) to `production/qa/spikes/`.

**Known gaps (qa-lead).** (1) Prepare 100 times is illegal under `DefenceCooldownSec` (60 s): a real second press replies `Cooldown` and never produces an `FHoldStatus`. The setup must override the cooldown to 0 for the test (document the override in the report) or reset it between presses, and must release between presses (release debounce 0.10 s, per-pawn Prepare bucket B2 R1, transport bucket) so every press executes. (2) The AC states lag but not packet loss; the profile default is 1 percent. State the loss used (p95 under 1 percent loss is a different measurement from p95 at 0 percent). (3) The numeric limits come from F5; thresholds are read from tuning or test config, not hard-coded in several places.

**Why N-form here.** The net-emulation profile for this story is PktLag 75 ms per direction (RTT 150 ms, the AC's own value) with the loss stated per gap (2). The timing rule behind the numbers is ADR-0011 section 8: a reply is enqueued at the end of the resolution step of the drain that resolved the command and flushed once per connection at the next net flush.

**Prototype.** New code, no prototype counterpart (the prototype has no latency measurement).

**Tuning keys (defaults).** `DefenceCooldownSec` 60 (Assassination-owned; overridden for the test only), `ReleaseDebounceSec` 0.10, `BucketPrepareSize` 2, `BucketPreparePerSec` 1.0, `TransportBucketSize` 30, `TransportBucketPerSec` 20, `HudLatencyWarnRttSec` 0.20 (HUD-owned; not asserted here).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-081-look-latency.md`: Look latency, the second half of the V4 report.
- `production/epics/input-interaction-framework/story-077-latency-icon.md`: the owner-visible latency warning icon.
- `production/epics/input-interaction-framework/story-082-action-pose-parity-observed.md`: NPC versus player ActionPose duration parity.
- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: the reply flush this measurement rides.

---

## Files

- `Source/StealthTestHarness/Private/LatencyBudgetTest.cpp` — CQTest `NETWORK_TEST_CLASS(LatencyBudget, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.LatencyBudget.<Method>` (planned)
- `production/qa/spikes/iif-v4-latency-report.md` — V4 report with raw samples (planned; shared with story 081)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap 1: pressing Prepare 100 times is illegal under `DefenceCooldownSec` 60 s (replies `Cooldown`, never `FHoldStatus`); state the setup (override the cooldown to 0 for the test, or reset the cooldown between presses) and release between presses (debounce 0.10 s, token bucket).
- Gap 2: loss is not stated beside the 75 ms lag (profile default 1 percent); state it (p95 under 1 percent loss is a different test).
- Gap 3: define the measurement origin and end points: "pressed" = injected Begin send time on the owner client; end points = `FHoldStatus` receipt on the owner and ActionPose OnRep on the observer client, both on the PIE process clock.

*Draft spec from the qa-lead (unconfirmed), test `LatencyBudget` (reports V4 with 081):* Given a host fixed at 30 Hz, PktLag 75 ms per direction, `t_interp` 0.05, an owner and an observer client, cooldown overridden (documented in the report), when 100 Prepare Begin/End cycles are injected, then p95(owner `FHoldStatus` arrival minus send) <= 220 ms and p95(observer OnRep minus send) <= 400 ms. Edge cases: discard none; record the max and loss resends; V4 report written to `production/qa/spikes` with raw samples.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/LatencyBudgetTest.cpp` (CQTest `StealthGame.Input.PIE.LatencyBudget`) must exist and pass, plus the V4 report in `production/qa/spikes/` (`/story-done` checks that the test EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; cross-epic: tuning-data-config (stories not yet written; module skeleton, StealthTestHarness net-emulation profiles); cross-epic: assassination-defence-reversal (stories not yet written; Prepare predicate and `DefenceCooldownSec`); cross-epic: round-match-framework (stories not yet written; resolution queue and drain)
- Unlocks: None
