# Story 087: AC 52 ActionPoseBandwidth

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-009`, `TR-iif-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid
**ADR Decision Summary**: ADR-0003 gives every pawn a fixed 15 Hz update and forwards a forced net update only inside the scoped `ActionStateWrite` reason, at most once per pawn per frame; `FActionState` is serialised at a fixed bit width, so its cost per change is the same for every pose and every pawn kind. The pawn network line is at most 36 KB/s worst case (60 pawns).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 (networking, HIGH). The project uses the legacy NetDriver (ADR-0003); Iris is production-ready in 5.8 but is not the replication system here, so the Network Insights per-property counter used for `FActionState` must be named in the report (qa-lead gap 2). Fixed-width `NetSerialize` byte identity (ADR-0003 section 4) is a precondition for the average being the same for every pose.

**Control Manifest Rules (this layer)**:
- Required: Outcome-bearing replicated structs serialise to a fixed bit width with no value-dependent size — source: ADR-0003
- Required: `ForceNetUpdate()` on a pawn forwards only inside `FStealthForcedNetUpdate` with an allowed reason, at most once per pawn per frame — source: ADR-0003
- Required: Only `UPawnCommandComponent::WriteActionState` writes `FActionState` — source: ADR-0011
- Guardrail: Replication: pawns at a fixed 15 Hz within the 36 KB/s worst-case line (60 pawns); total within the 50 KB/s per-client budget — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 52 (Integration, N `ActionPoseBandwidth`): **GIVEN** 50 pawns cycling poses at their maximum rate **WHEN** Network Insights measures one client for 60 s **THEN** ActionPose traffic averages ≤ 1.0 KB/s.

---

## Implementation Notes

*Derived from ADR-0003 Decision sections 2 and 4, GDD Acceptance Criteria section G (provisional budgets, technical-director to confirm) and GDD R12:*

**What this story builds.** A bandwidth measurement of the only per-action replicated data, `FActionState` on the pawn: one observing client, 60 s after warm-up, 50 pawns cycling poses at the defined maximum rate, ActionPose traffic averaging at most 1.0 KB/s. This keeps the IIF's replicated action surface a small fraction of the 36 KB/s pawn line and the 50 KB/s per-client budget, and guards against a future per-pose payload growing with action count.

**Definitions the test must fix (qa-lead gaps).** (1) "Maximum rate" is the rate the token buckets, slot priority and animation durations allow: a scripted worst-case cycle of Prepare, Interact and Hide at the bucket limit, plus one forced net update per `ActionPose` write (ADR-0003 section 2). (2) Define KB as 1000 bytes (or 1024) once, state it in the report, and use the same definition in story 088. (3) The observing client is one that is relevant to all 49 other pawns (no concealment culling in the scenario). (4) Name the counter: the Network Insights per-property bytes for `FActionState` on the legacy NetDriver, with the raw bytes retained so the average can be recomputed.

**Why N-form.** Profile N (PktLag 50 ms per direction, 1 percent loss). Loss affects retransmission cost; the average must be reported with that profile, and the budget is provisional.

**Prototype.** New code, no prototype counterpart (the prototype replicated per-feature pawn state and RPCs).

**Tuning keys (defaults).** `BucketInteractSize` 3 / `PerSec` 2.0; `BucketPrepareSize` 2 / `PerSec` 1.0; `BucketHideSize` 4 / `PerSec` 2.0 (these bound the maximum rate). The 1.0 KB/s threshold is a provisional budget read from test config.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`: one forced net update per write (the mechanism this bandwidth depends on).
- `production/epics/input-interaction-framework/story-088-owner-status-bandwidth.md`: owner-only `FHoldStatus` and reply bandwidth.
- `production/epics/input-interaction-framework/story-084-pawn-replicated-value-parity.md`: property set and per-property size parity.
- `production/epics/input-interaction-framework/story-086-command-cost-per-50-pawns.md`: server CPU cost.

---

## Files

- `Source/StealthTestHarness/Private/ActionPoseBandwidthTest.cpp` — CQTest `NETWORK_TEST_CLASS(ActionPoseBandwidth, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.ActionPoseBandwidth.<Method>`; net-emulation profile N (planned)
- `production/qa/evidence/iif-action-pose-bandwidth.md` plus the retained Network Insights trace and raw byte counts (planned)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap 1: "maximum rate" is undefined; define as the rate the token buckets, slot priority and animation durations allow (scripted worst-case cycle: Prepare/Interact/Hide at bucket limit), plus a forced net update per write (ADR-0003).
- Gap 2: define KB as 1000 bytes (or 1024) and the observing client (one relevant to all 49 other pawns, no culling).

*Draft measurement from the qa-lead (unconfirmed), `ActionPoseBandwidth`:* Network Insights, one client, 60 s after warm-up, 50 pawns cycling poses at the defined maximum, profile N. Scopes: per-property bytes for `FActionState` (name the Iris or legacy counter used). Threshold: average at most 1.0 KB/s. Evidence: trace plus the average computed from raw bytes retained.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a performance report with the Network Insights trace and raw byte counts in `production/qa/evidence/iif-action-pose-bandwidth.md`, plus the CQTest `Source/StealthTestHarness/Private/ActionPoseBandwidthTest.cpp` (`StealthGame.Input.PIE.ActionPoseBandwidth`) which must exist (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`; cross-epic: assassination-defence-reversal, objectives-chain and concealment-bushes (stories not yet written; Prepare, Interact and Hide predicates for the pose cycle); cross-epic: crowd-npc-behaviour (stories not yet written; 50-pawn population); cross-epic: tuning-data-config (stories not yet written; module skeleton, config-read threshold)
- Unlocks: None
