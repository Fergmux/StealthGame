# Story 006: Spike S12, the cull delay and un-cull lead are recomputed from measured L99

> **Epic**: Information Visibility Policy
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-25

## Context

IVP F3 fixes the concealment cull delay at `D_cull = q * ceil((L99 + 1/f_net) / q)` with a provisional `L99 = 150 ms`, giving 250 ms, and F4 requires Concealment's exit transition to keep the body hidden for at least `T_uncull = L99 + 1/f_net + T_reopen` (also 250 ms). Both depend on the real p99 one-way latency under the leak net profile. If the measured L99 is larger, `ConcealCullDelayMs`, `T_uncull` and Concealment's `BushExitCrossSec`/`BushStrikeEmergeSec` must be raised together (cross-GDD invariant). This spike measures it.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 5.3, Risks); GDD formulas F3 and F4
**ADR Decision Summary**: The cull deadline lies on a global grid at or after `CommitServerTime + ConcealCullDelayMs` (IVP F3, default 250 ms at `LeakNetProfile`); the risk row "Measured L99 exceeds the F3 assumption (S12)" is mitigated by recomputing `ConcealCullDelayMs` and `T_uncull` together.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `PktLag 100` is one-way on the client's outgoing side [U semantics]; the harness story measures the resulting RTT (ADR-0008 section 5). E14 (random per-actor update offset) and E17 (listen-host frame time) shape the observed latency. This spike owns S12.

**Control Manifest Rules (this layer)**:
- Required: **The concealment cull lands on the global boundary grid** ... with no client acknowledgement wait — source: ADR-0003
- Guardrail: **Server frame**: listen-host frame time ≤ 1/60 s (p99 under the profiled load), measured by the harness frame-time capture — source: ADR-0002, ADR-0003
- Guardrail: **Replication**: fixed 15 Hz pawns within 36 KB/s worst case — source: ADR-0003

---

## Acceptance Criteria

*From the story's spike definition (ADR-0003 Verification Required, S12):*

- [ ] GIVEN the S9 cull rig under StealthLeak1 and StealthLeak2, WHEN the p99 one-way server-to-client latency L99 is measured over at least 10 minutes with 3 clients, THEN F3 is recomputed (D_cull = 50 ms x ceil((L99 + 1/f_net)/50 ms)) and F4's T_uncull = L99 + 1/f_net + T_reopen is recomputed with story-005's T_reopen, and the result either confirms ConcealCullDelayMs = 250 ms and T_uncull = 250 ms or records the larger pair for TDC and Concealment (cross-GDD invariant) in production/qa/spikes/.

---

## Implementation Notes

*Derived from IVP F3/F4 and ADR-0003 Risks:*

- Reuse story-004's rig and story-005's measured `T_reopen`. Record both profiles separately; the result is binary (confirm 250/250, or a larger pair).
- If the pair grows, raise a TDC change request (`ConcealCullDelayMs` range 100-1000, multiple of 50, at least `L99 + 1/f_net`; story-026) and a Concealment change request for `BushExitCrossSec`/`BushStrikeEmergeSec`; do not edit the GDDs here.
- Measure latency server to client at the packet level (not frame level) over at least 10 minutes with 3 clients.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-004: the cull rig; story-005: `T_reopen`
- story-025: the cull timing acceptance test; story-029: the un-cull lead bound test
- Concealment epic: exit-transition timings

---

## Files

*Changed by /dev-story (2026-10-25).*

- `production/qa/spikes/2026-10-25_S12.md` (the verdict)
- Measurement on story 005's rig (`Source/StealthTestHarness/Private/Scenarios/StealthHarnessScenarioS10.cpp`) with packet timing (`Spikes/Session/StealthSpikeMeasure.{h,cpp}`, `StealthTimingIpConnection.h`); analyser `tools/ci/mp/analyze_net.py`

---

## QA Test Cases

- **AC-1**: S12 cull delay against measured L99
  - Given: the S9 rig, StealthLeak1 and StealthLeak2, 3 clients.
  - When: p99 one-way server-to-client latency L99 is measured for at least 10 minutes.
  - Then: F3 recomputed as D_cull = 50 ms x ceil((L99 + 1/f_net)/50 ms); F4 T_uncull = L99 + 1/f_net + T_reopen recomputed with story-005's T_reopen; the report confirms ConcealCullDelayMs = 250 and T_uncull = 250, or records the larger pair for TDC and Concealment.
  - Edge cases: the result is binary (confirm or larger pair); record both profiles separately; depends on story-005's T_reopen. (Tier: T3.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a written spike report `production/qa/spikes/<date>_S12.md` with the measurements (a documented measurement).

**Status**: [x] Report with the measurements — `production/qa/spikes/2026-10-25_S12.md`

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`
- Unlocks: `production/epics/information-visibility-policy/story-025-concealment-cull-timing.md`, `production/epics/information-visibility-policy/story-029-uncull-lead-bound.md`

## Completion Notes
**Completed**: 2026-10-25
**Verdict**: CONFIRMED 250/250 — L99 (packet-level p99 one-way server→client, 3 clients, 640 s per profile on the cycling cull rig) = 133.4 ms under StealthLeak1 and StealthLeak2; F3 D_cull = 50·ceil((133.4 + 66.7)/50) = 250 ms; F4 T_uncull = 133.4 + 66.7 + 20 (story 005's T_reopen p99) = 220 ms ≤ 250 ms. No TDC/Concealment change request. CONCERN: at the data level 1-2 % of re-opens are delayed one resend (end-to-end p99 ≈ 0.4 s under StealthLeak2); recorded for story 029.
**Criteria**: 1/1 (AC-1)
**Decisions**: L99 measured at the client's packet processing time (includes 60 fps frame quantisation); T_reopen taken as story 005's measured p99 (≈ 20 ms) — story 005 reported FAIL on other questions, but its T_reopen measurement stands.
**Deviations**: none.
**Test Evidence**: `production/qa/evidence/story-006-spike-s12-cull-delay-vs-measured-l99/` (per-run packet analysis JSON, launch lines).
**Run result**: N/A — headless latency measurement; nothing player-visible.
**Reviewed by**: focused self-review (unattended run).
**Unblocks**: IVP story 025 → Ready (S12 was its only open gate); stories 029 and concealment 027 keep their S10/Concealment gates.
