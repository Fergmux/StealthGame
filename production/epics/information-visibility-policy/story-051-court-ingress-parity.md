# Story 051: Interact ingress is identical at live sites, non-site courts, rival sites and decoys

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Objectives] needs objectives-chain court fixtures and input-interaction-framework Interact ingress
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

If the game answered `Interact` differently at a real objective site than at a plain court or a decoy (a different reply code, hold duration, timing, or a replicated "busy" or "in use" flag), a modified client could probe courts to find the objectives and see who is working on them. This story proves that the input-interaction-framework ingress treats the four cases identically, charges budgets to the sender only, and that no court fixture carries a replicated busy, occupied or in-use field. It covers leak rows L24 and L33.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5, carrier model; R7.4a); ADR-0008: Test and automation strategy for leak and parity tests (sections 1 T2, 3, 5)
**ADR Decision Summary**: Opponent and objective state has no replicated carrier: `ServerOnly` facts live in game-instance and world stores, never as a replicated property, and a public carrier holds no per-team or per-participant value. ADR-0008 runs T2 multi-client parity tests on CQTest `FPIENetworkComponent` with an optional packet-simulation profile and requires a negative control on every parity gate.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: T2 uses CQTest `FPIENetworkComponent` with `AsListenServer().WithClients(N).WithGameMode(AStealthGameMode)` and `WithPacketSimulationSettings` loaded from an engine profile (`StealthLeak1`) [V macro names; U exact component template form, ADR-0008 Verification 1]. CQTest step timeouts are wall-clock and are a failure ceiling only; the pass or fail predicates are server-time windows and frame counts. Packet loss is not seedable, so assert bounds.

**Control Manifest Rules (this layer)**:
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Required: **T2/T3 tests bound by frames or server time and assert and record the active replication system** — source: ADR-0008
- Forbidden: **Never use a team-filtered actor, a per-team replica actor or a team relevancy rule** — source: ADR-0001
- Forbidden: **Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source** — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-41, scoped to this story:*

- [ ] **AC-IVP-41** [Int][Net][Dep:Objectives] `StealthGame.Visibility.CourtIngressParity`: GIVEN `Interact` at an own live site, an own non-site court, a rival site and a decoy court, WHEN IIF ingress handles each, THEN acceptance, reply code, hold duration and timing class are identical (F7 bounds), budgets are charged to the sender only, only presentation and progress credit differ, and no fixture carries a replicated busy, occupied or in-use field. (R7.4a, L24, L33)

---

## Implementation Notes

*Derived from ADR-0001 section 5, ADR-0008 sections 1, 3, 5 and the GDD R7.4a / R12 rows L24, L33:*

- T2 test `NETWORK_TEST_CLASS(CourtIngressParity, "StealthGame.Visibility.PIE")` (full name `StealthGame.Visibility.PIE.CourtIngressParity.<Method>`), in `Source/StealthTestHarness/Private/Tests/PIE/`, inside `#if ENABLE_PIE_NETWORK_TEST`. T2 classes are named by scenario and are exempt from the `F...Test` class-name convention (ADR-0008 section 2). Set `WithGameMode(AStealthGameMode::StaticClass())`, load the fixture map in a first step, run under the `StealthLeak1` profile, and record the active replication system (legacy).
- Four `Interact` cases through the IIF ingress: own live site, own non-site court, rival site, decoy court. For each, compare acceptance, reply code, hold duration and timing class within the F7 bounds (the F7 bounds and sample sizes are defined in the GDD Formulas; do not restate or loosen them here). Charged budgets must land on the sender only. Only presentation and progress credit may differ between cases.
- Field audit on every fixture: no replicated busy, occupied or in-use property on any court or site fixture (the QA edge case). Reuse the reflection audit machinery from story-015 for the fixture classes.
- Negative control (added by the story, ADR-0008 section 3): a harness-only fixture with a replicated in-use flag must be detected by the audit; the suite passes only if the control is detected. The control type lives only in `StealthTestHarness`, never Shipping.
- Wall-clock time is a failure ceiling only; bound steps by server time and frames (ADR-0008 section 4).
- Prototype: not applicable (no court or IIF code exists). Replaces: none (new).
- Cross-epic needs: objectives-chain epic (court fixtures: live site, non-site court, rival site, decoy court, selected from `ServerSecret` sub-seeds only), input-interaction-framework epic (`Interact` ingress, reply codes, hold timers as queue items), round-match-framework epic (the queue), tuning-data-config Exit A (T2 harness, net profiles).
- Unblocks when: the objectives-chain court fixtures and the IIF `Interact` ingress exist.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-052: strike id oracle parity (`Attack(TargetPawn)` against `Attack(TargetSpot)`)
- story-050: rival signal shape
- story-042: timing parity of rejection replies, `ActionPose` delay and kill-confirmation delay
- story-015: the replicated-property audit (this story reuses it for fixtures)
- input-interaction-framework and objectives-chain epics: ingress and fixture behaviour themselves

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `Source/StealthTestHarness/Private/Tests/PIE/CourtIngressParityTest.cpp` (T2 `NETWORK_TEST_CLASS(CourtIngressParity, "StealthGame.Visibility.PIE")`, including the replicated in-use flag fixture used as the negative control)

---

## QA Test Cases

- **AC-1**: AC-IVP-41 `StealthGame.Visibility.PIE.CourtIngressParity` (T2 under a lossy profile)
  - Given: `Interact` at an own live site, an own non-site court, a rival site and a decoy court.
  - When: IIF ingress handles each.
  - Then: acceptance, reply code, hold duration and timing class are identical (F7 bounds); budgets are charged to the sender only; only presentation and progress credit differ; no fixture carries a replicated busy, occupied or in-use field.
  - Edge cases: the busy/occupied/in-use check is a field audit on every fixture; negative control (added): a fixture with a replicated in-use flag must be detected.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: T2 test under `Source/StealthTestHarness/Private/Tests/PIE/`, OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`; objectives-chain epic (court fixtures); input-interaction-framework epic (`Interact` ingress); tuning-data-config Exit A (T2 harness, net profiles)
- Unlocks: None
