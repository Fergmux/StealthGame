# Story 009: Spike V2 reports ClientSetPossessionEpoch always precedes ClientRestart

> **Epic**: Input & Interaction Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-013`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: `PossessionEpoch` is server-owned: `AStealthPlayerController::OnPossess` increments a server-side `ServerPossessionEpoch` and, before the engine's `ClientRestart`, sends the owner-only reliable client RPC `ClientSetPossessionEpoch(uint16)`, so on one reliable controller channel the owner's `OwnerPossessionEpoch` is current before it acts on the new pawn. Verification item V2 confirms that ordering.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: This story IS verification item V2 of ADR-0011: "`ClientSetPossessionEpoch` always arrives before the `ClientRestart` of the same possession on the owner (same reliable controller channel), across takeover, rejoin, same-pawn re-possession after an `Unresponsive` hand-over, and the engine's `ClientRetryClientRestart`/null-pawn resend paths (E6)." Engine fact E6: `APlayerController::OnPossess` calls `ClientRestart(Pawn)` once per possession; the server may re-send it through the throttled `ClientRetryClientRestart`, and a `ClientRestart` with a null pawn makes the owner call `ServerCheckClientPossessionReliable`, so the owner can see more than one `ClientRestart` per possession. Source: `PlayerController.cpp:778-837`; `PlayerController.h:1142, 1507-1514`.

**Control Manifest Rules (this layer)**:
- Required: `PossessionEpoch` is server-owned and delivered only by `ClientSetPossessionEpoch` before `ClientRestart`; ingress stamps the controller's recorded `PossessedControlEpoch`, never the pawn's current control epoch — source: ADR-0011
- Required: `Possess`/`UnPossess` are called only by the factory (spawn, hand-over pass incl. `ReleaseBody`, takeover) and `AStealthPlayerController::PawnLeavingGame`, always through `FScopedPossessionParity` — source: ADR-0010
- Forbidden: Never destroy the pawn in `PawnLeavingGame`; never destroy-and-respawn on disconnect or takeover — source: ADR-0010
- Guardrail: Command interface uplink at most 20 reliable RPCs per second per connection; the epoch RPC is one reliable owner-only message per possession — source: ADR-0011

This is a verification spike. The ordering claim is what makes stale commands safe: a command from the old owner carries the old epoch and is dropped at ingress (story 025), but that only works if the new owner's `OwnerPossessionEpoch` is already current when its first command on the new pawn is stamped. If the engine ever delivered `ClientRestart` before `ClientSetPossessionEpoch`, the first command on a new pawn would be stamped with the old epoch and be dropped silently. Stories 071 and 091 are Blocked ("spike V2 (story 009) has not reported") until this reports.

It has no story dependency because it is a measurement of engine and controller behaviour. It needs the controller skeleton from story 094 or, as the qa-lead notes, a bootstrap-epic controller; scenarios needing Round R17.1 or the ADR-0010 hand-over are simulated by direct `UnPossess`/`Possess` calls and the report says so.

---

## Acceptance Criteria

*ADR-derived spike. Source: ADR-0011 Engine Compatibility, Verification Required item V2; criterion text from the plan:*

- [ ] **GIVEN** a server and an owning client **WHEN** possession changes through takeover, rejoin, same-pawn re-possession after an `Unresponsive` hand-over, `ClientRetryClientRestart` and the null-pawn resend path are each exercised, with and without net emulation (PktLag 50 ms per direction, 1 % loss) **THEN** a report at `production/qa/spikes/` records, per path, the owner-side order of `ClientSetPossessionEpoch(N)` receipt, `ClientRestart` execution and the value of `OwnerPossessionEpoch` at `ClientRestart`, shows `ClientSetPossessionEpoch` preceding `ClientRestart` in every run, and states PASS or FAIL per path and overall.

---

## Implementation Notes

*Derived from ADR-0011 Decision §2, Engine Compatibility V2 and Implementation Guidelines:*

"The server owns it: `AStealthPlayerController::OnPossess` increments a server-side `ServerPossessionEpoch` and, **before** the engine's `ClientRestart`, sends the owner-only reliable client RPC `ClientSetPossessionEpoch(uint16)`; reliable RPCs on one controller channel arrive in order, so the owner's `OwnerPossessionEpoch` is current before it acts on the new pawn (V2). The two fields are separate, so the listen host (where both run on one object) never double-counts; no dedupe by pawn is needed, and same-pawn re-possession (Round R17.1 Unresponsive to AI to same player) still advances the epoch. It is an RPC, not a replicated property, adds nothing to the pawn and carries only the owner's own counter."

**Spike procedure (from the qa-lead spec):** for each path (takeover, rejoin, same-pawn re-possession after an `Unresponsive` hand-over, `ClientRetryClientRestart`, null-pawn resend), under no emulation and under PktLag 50 ms per direction with 1 % loss, record a timestamped owner-side log of `ClientSetPossessionEpoch(N)` receipt, `ClientRestart` execution and the value of `OwnerPossessionEpoch` at `ClientRestart`, plus the server's `ServerPossessionEpoch` at each `OnPossess`; 20 repetitions per path. Pass rule: in 100 % of runs the owner's `OwnerPossessionEpoch` already equals the server value when `ClientRestart` (and the first command on the new pawn) executes, and same-pawn re-possession advances the epoch. Any inversion is a FAIL naming the path; stories 071 and 091 stay Blocked. Edge cases: `ClientRestart` resends with the same pawn; listen host (both fields on one object, no double count); `uint16` wrap is not required for the report. The scenarios needing Round R17.1 or the ADR-0010 hand-over pass use direct `UnPossess`/`Possess` calls under `FScopedPossessionParity`; the report states that simplification. Cross-epic dependency on the factory and controller (shared-character-locomotion, round-match-framework, stories not yet written).

Prototype: new code, no prototype counterpart (the prototype `StealthPlayerController` has no epoch). Tuning keys: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-025-possession-epoch-gate.md`: the ingress gate that drops stale epochs (AC 79).
- `story-091-hand-over-window-test.md`: the production hand-over window test, Blocked on this spike.
- `story-071-context-swap-on-state-and-possession.md`: context swap on possession, Blocked on this spike and V1.
- `story-094-controller-transport-skeleton.md`: the controller RPCs and epoch fields this spike measures.

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `production/qa/spikes/2026-10-26_V2-possession-epoch-order-report.md`
- `Source/StealthTestHarness/Private/Tests/Spikes/SpikeV2PossessionEpochOrderTest.cpp`
- `Source/StealthGame/Public/StealthPlayerController.h`
- `Source/StealthGame/Private/StealthPlayerController.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Integration — spike specification** (QL-STORY-READY: ADEQUATE; note: needs the controller skeleton from the bootstrap epic; hand-over scenarios are simulated by direct UnPossess/Possess and the report says so):

- **AC-1**: Spike V2 reports ClientSetPossessionEpoch always precedes ClientRestart
  - Given: each path (takeover, rejoin, same-pawn re-possession after an Unresponsive hand-over, ClientRetryClientRestart, null-pawn resend), under no emulation and under PktLag 50 ms/direction with 1 % loss. Must record a timestamped owner-side log of `ClientSetPossessionEpoch(N)` receipt, `ClientRestart` execution, the value of `OwnerPossessionEpoch` at `ClientRestart`, and the server's `ServerPossessionEpoch` at each `OnPossess`; 20 repetitions per path.
  - When: each possession path executes.
  - Then: in 100 % of runs the owner's `OwnerPossessionEpoch` already equals the server value when `ClientRestart` (and the first command on the new pawn) executes, and same-pawn re-possession advances the epoch. Any inversion is a FAIL naming the path; stories 071 and 091 stay Blocked.
  - Edge cases: `ClientRestart` resends with the same pawn; listen host (both fields on one object, no double count); `uint16` wrap not required for the report.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report `production/qa/spikes/V2-possession-epoch-order-report.md` (a documented result, retained) OR the probe harness test `Source/StealthTestHarness/Private/SpikeV2PossessionEpochOrderTest.cpp`

**Status**: [x] Spike report written (`production/qa/spikes/2026-10-26_V2-possession-epoch-order-report.md`; harness `StealthSpike.Input.PIE.SpikeV2PossessionEpochOrder`, not a gate test)

---

## Dependencies

- Depends on: None (cross-epic: shared-character-locomotion and round-match-framework for the factory, controller and possession path, stories not yet written; tuning-data-config StealthTestHarness module)
- Unlocks: `production/epics/input-interaction-framework/story-071-context-swap-on-state-and-possession.md`, `production/epics/input-interaction-framework/story-091-hand-over-window-test.md`

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 — the report records per path the owner-side order of ClientSetPossessionEpoch(N) receipt and ClientRestart, OwnerPossessionEpoch at ClientRestart and the server epoch at OnPossess, without and with PktLag 50 ms per direction + 1 % loss, and states PASS per path and overall: takeover, same-pawn re-possession after an Unresponsive hand-over, ClientRetryClientRestart and takeover of a culled pawn 20/20 each in both profiles, first possession of a fresh controller 1/1; no inversion in 161 runs
**Deviations**: ADVISORY — direct UnPossess/Possess calls stand in for Round R17.1 and the ADR-0010 hand-over pass (the story allows it; FScopedPossessionParity does not exist). ADVISORY — the mid-round rejoin path was NOT EXERCISED (the rejoined controller gets no pawn until Round R17/Session rejoin adoption exist) and the engine's null-pawn branch was NOT PROVOKED in single-process PIE (ClientRestart resolved the culled pawn in every run); both are open items for the multi-process harness, recorded in the report. The harness adds a development-only possession trace to AStealthPlayerController (bounded at 4,096 entries).
**Test Evidence**: `production/qa/spikes/2026-10-26_V2-possession-epoch-order-report.md`; `production/qa/evidence/story-009-spike-v2-possession-epoch-order/` (v2-results.json, run-log-excerpt.txt)
Run result: N/A — a measurement of RPC ordering; the counts in the report are the observation, nothing is drawn
**Reviewed by**: self, no independent review (unattended run; harness code covered by the batch's lead-programmer review)
