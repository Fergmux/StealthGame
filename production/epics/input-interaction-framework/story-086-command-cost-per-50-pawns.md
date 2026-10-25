# Story 086: AC 51 CommandCostPer50Pawns

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-012`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: ADR-0011 section 3 keeps ingress O(1) per RPC and stage 2 O(1) per item plus one predicate call; at the IIF F3 worst case (300 admitted commands per second across 50 pawns, about 5 per frame at 60 Hz) it is a few microseconds per frame inside ADR-0002's 0.5 ms queue budget. The condition sweep touches only pawns with an active hold.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: A profiling story: Unreal Insights with named CPU scopes `IIF.Validate`, `IIF.HoldTimers` and `IIF.Hub`. The listen host's server tick is its render frame (ADR-0003 E17: `NetServerMaxTickRate` has no effect unless `bClampListenServerTickRate`), so "30 Hz" must be produced by the harness (for example a frame-rate cap) and logged. The GDD marks the budgets provisional pending the 50-character spike and technical-director confirmation.

**Control Manifest Rules (this layer)**:
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case (about 5 items per frame), inside the 0.5 ms queue line; uplink at most 20 reliable RPCs/s per connection; at most one reply RPC per connection per drain — source: ADR-0011
- Required: AI commands skip ingress (stage 1), submit to the server-local mailbox with a null sink, and are subject to every stage-2 step including the per-pawn buckets — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 51 (Integration, N `CommandCostPer50Pawns`): **GIVEN** 50 pawns (40 AI issuing at bucket maximum, 10 humans) on a 30 Hz listen server **WHEN** profiled for 60 s **THEN** the summed IIF scopes cost ≤ 0.15 ms per server tick at p95.

---

## Implementation Notes

*Derived from ADR-0011 Performance Implications and GDD Acceptance Criteria section G ("Performance", proposed budgets provisional pending the 50-character spike; technical-director to confirm):*

**What this story builds.** The server CPU cost measurement for the IIF at the target population: a 50-pawn run (40 AI issuing at the token-bucket maximum through the null-sink path, 10 scripted humans) on a 30 Hz listen server for 60 s after a 10 s warm-up, with the IIF Insights scopes `IIF.Validate`, `IIF.HoldTimers` and `IIF.Hub` summed per server tick. Pass: p95 of the summed per-tick cost is at most 0.15 ms. The scopes are part of the production code this story adds (the three `TRACE_CPUPROFILER_EVENT_SCOPE` markers around ingress and stage-2 validation, hold-timer slot work and hub fan-out). The F3 worst case it reproduces is 50 pawns x sum R 6/s = 300 admitted commands per second sustained plus the burst of 11 per pawn.

**Measurement method (qa-lead).** Unreal Insights trace, Development build, 1080p, RTX 3060-class reference PC. Humans cannot be 10 PIE clients on the reference PC: the 10 humans are scripted clients (an RPC injector calling `ServerPawnCommand`) and the budget is provisional, so the 0.15 ms threshold is read from a config value, not hard-coded. Net profile N (PktLag 50 ms per direction, 1 percent loss). Repeat 3 runs and report the worst p95. Evidence: trace file plus a summary in `production/qa/evidence/`.

**Why this is Integration, not Logic.** The criterion is a measured run on a real listen server with replicated pawns; it needs the null-sink AI path (crowd epic) and the transport RPC path (story 094).

**Prototype.** New code, no prototype counterpart (the prototype has no ingress, buckets or scopes).

**Tuning keys (defaults).** `BucketInteractSize` 3 / `BucketInteractPerSec` 2.0; `BucketPrepareSize` 2 / `BucketPreparePerSec` 1.0; `BucketAttackSize` 2 / `BucketAttackPerSec` 1.0; `BucketHideSize` 4 / `BucketHidePerSec` 2.0; `TransportBucketSize` 30 / `TransportBucketPerSec` 20. The 0.15 ms threshold is a provisional budget read from test config.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-031-token-bucket-defaults.md`: per-pawn bucket arithmetic (F3).
- `production/epics/input-interaction-framework/story-087-action-pose-bandwidth.md`: ActionPose network bandwidth.
- `production/epics/input-interaction-framework/story-088-owner-status-bandwidth.md`: owner status and rejection reply bandwidth.
- `production/epics/input-interaction-framework/story-080-latency-budget.md`: latency, not CPU cost.

---

## Files

- `Source/StealthGameplay/Private/Core/` — `IIF.Validate`, `IIF.HoldTimers` and `IIF.Hub` Insights scopes in the ingress, stage-2 and hub code (planned)
- `Source/StealthTestHarness/Private/CommandCostPer50PawnsTest.cpp` — CQTest `NETWORK_TEST_CLASS(CommandCostPer50Pawns, "StealthGame.Input.PIE")`, full name `StealthGame.Input.PIE.CommandCostPer50Pawns.<Method>`; net-emulation profile N (planned)
- `production/qa/evidence/iif-command-cost-per-50-pawns.md` plus the retained `.utrace` (planned)

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

*QL-STORY-READY: GAPS (qa-lead):*
- Gap: "10 humans" cannot be 10 PIE clients on the reference PC; state that humans are scripted clients (an RPC injector through `ServerPawnCommand`) and that budgets are provisional pending technical-director confirmation (the AC says so), so the threshold is read from a config value.

*Draft measurement from the qa-lead (unconfirmed), `CommandCostPer50Pawns`:* Unreal Insights trace, Development build, 1080p, RTX 3060-class reference PC; 50 pawns (40 AI at bucket maximum via the null sink, 10 scripted humans), listen server at 30 Hz, 60 s run after a 10 s warm-up, net profile N. Scopes `IIF.Validate`, `IIF.HoldTimers`, `IIF.Hub`, per-tick sum. Threshold: p95 of the summed per-tick cost at most 0.15 ms (provisional). Evidence: trace file plus summary in `production/qa/evidence`; repeat 3 runs, worst p95 reported.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a performance report with the Unreal Insights trace and the worst-of-3 p95 summary in `production/qa/evidence/iif-command-cost-per-50-pawns.md`, plus the CQTest `Source/StealthTestHarness/Private/CommandCostPer50PawnsTest.cpp` (`StealthGame.Input.PIE.CommandCostPer50Pawns`) which must exist (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-031-token-bucket-defaults.md`; cross-epic: crowd-npc-behaviour (stories not yet written; 40 AI issuing at bucket maximum via the null sink); cross-epic: round-match-framework (stories not yet written; resolution queue and drain); cross-epic: tuning-data-config (stories not yet written; module skeleton, config-read threshold)
- Unlocks: None
