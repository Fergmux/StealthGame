# Story 088: AC 53 OwnerStatusBandwidth

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0011 sections 7 and 8 keep every owner-only datum (`FHoldStatus`, rejection codes, owner results) off the pawn and deliver it in at most one reliable owner-only `ClientCommandReplies` RPC per connection per drain; uplink is bounded to 20 reliable RPCs per second per connection by the transport bucket, so downlink reply traffic is bounded too.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Network Insights on the owner connection. Reliable resend bytes under the 1 percent loss default count towards the figure (qa-lead). The budget is derived from the 50 KB/s per-client network line: the GDD AC states it keeps the IIF at or below 3 percent of that line.

**Control Manifest Rules (this layer)**:
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection — source: ADR-0011
- Forbidden: Never put owner-only data on the pawn; never deliver replies as replicated owner-only properties — source: ADR-0011
- Guardrail: Command interface: uplink at most 20 reliable RPCs/s per connection (transport bucket); at most one reply RPC per connection per drain — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 53 (Integration, N `OwnerStatusBandwidth`): **GIVEN** a player mashing every action key at the bucket limit **WHEN** measured for 60 s **THEN** owner status plus rejection replies average ≤ 0.5 KB/s. This keeps the IIF ≤ 3 % of the 50 KB/s per-client budget.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 7 and 8 and Performance Implications; GDD Acceptance Criteria section G (provisional budgets, technical-director to confirm):*

**What this story builds.** The owner-side downlink measurement: a scripted player mashes every action key at the transport bucket limit while Network Insights records, on the owner connection for 60 s, the owner `FHoldStatus` entries plus the `ClientCommandReplies` RPC bytes (rejections included). The average must be at most 0.5 KB/s. This is the cost of ADR-0011's choice to batch replies per drain into one reliable RPC (Alternative 5 there, owner-only replicated properties, was rejected partly on this cost and on timing-leak grounds).

**Definitions (qa-lead).** KB is defined once, as in story 087 (1000 or 1024 bytes, stated in the report). Include reliable resend bytes under the 1 percent loss default of net profile N (PktLag 50 ms per direction, 1 percent loss). Log the bucket rate during the run to prove the limit was actually reached (mashing below the bucket would under-measure). The threshold is a provisional budget read from test config.

**Where it sits.** Stories 004 (the batched flush) and 029 (the codes) provide the traffic; this story measures worst-case honest-looking abuse. It is a measurement, so the evidence is a trace and a report rather than a pure unit test.

**Prototype.** New code, no prototype counterpart.

**Tuning keys (defaults).** `TransportBucketSize` 30, `TransportBucketPerSec` 20 (the limit being mashed against); per-pawn buckets `BucketInteractSize` 3 / 2.0 per s, `BucketPrepareSize` 2 / 1.0, `BucketAttackSize` 2 / 1.0, `BucketHideSize` 4 / 2.0. The 0.5 KB/s threshold is read from test config.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: the batching and flush mechanism.
- `production/epics/input-interaction-framework/story-087-action-pose-bandwidth.md`: observer-side ActionPose bandwidth.
- `production/epics/input-interaction-framework/story-086-command-cost-per-50-pawns.md`: server CPU cost.
- `production/epics/input-interaction-framework/story-022-transport-bucket-and-abuse-log.md`: the transport bucket and abuse log behaviour.

---

## Files

- `Source/StealthTestHarness/Private/OwnerStatusBandwidthTest.cpp` — CQTest `NETWORK_TEST_CLASS(OwnerStatusBandwidth, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.OwnerStatusBandwidth.<Method>`; net-emulation profile N (planned)
- `production/qa/evidence/iif-owner-status-bandwidth.md` plus the retained Network Insights trace and the logged bucket rate (planned)

---

## QA Test Cases

**Automated test specs (measurement):**

- **AC-1**: Owner status plus rejection replies average at most 0.5 KB/s (`StealthGame.Input.PIE.OwnerStatusBandwidth`).
  - Method: Network Insights on the owner connection, 60 s; a scripted player mashing every action key at the transport bucket limit; profile N.
  - Scopes: owner `FHoldStatus` plus `ClientCommandReplies` RPC bytes (rejections included, and reliable resend bytes under the 1 percent loss default). KB defined as in story 087.
  - Threshold: average at most 0.5 KB/s.
  - Evidence: trace file retained; bucket rate logged to prove the limit was reached.

*QL-STORY-READY: ADEQUATE (qa-lead).*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a performance report with the Network Insights trace and the logged bucket rate in `production/qa/evidence/iif-owner-status-bandwidth.md`, plus the CQTest `Source/StealthTestHarness/Private/OwnerStatusBandwidthTest.cpp` (`StealthGame.Input.PIE.OwnerStatusBandwidth`) which must exist (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; cross-epic: assassination-defence-reversal (stories not yet written; owner results and Prepare/Attack predicates); cross-epic: tuning-data-config (stories not yet written; module skeleton, config-read threshold)
- Unlocks: None
