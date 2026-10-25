# Story 014: An Attack from inside breaks concealment, un-culls in the same step and ends outside the bush

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R9 table, F3(c), AC-CON-13 server part)
**Requirement**: `TR-con-001`, `TR-con-008`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§3 HideExit execution / `OnConcealmentBroken`), with ADR-0018 (outcome slot, `BushStrike` variant)
**ADR Decision Summary**: An Attack from inside (any target, a co-occupant, or a whiff) breaks concealment at execution: `OnConcealmentBroken(Pawn)` fires, `SetCullState(Pawn, false)` is called in the same slot, state becomes Emerging with pose Strike (the `BushStrike` variant owned by Assassination); the leaf crossing is at `ContactServerTime − StrikeContactSec` (0.35 s), the pawn leaves the record there and ends outside when the strike recovery ends.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ with the cull behind a seam)
**Engine Notes**: Un-cull in the same server step is the ADR-0003 §5.6 rule; the real cull source is story 025. The observed timing on an opponent client (0.35 s body, 0.75 s contact) is story 027.

**Control Manifest Rules (this layer)**:
- Required: Un-cull happens in the same slot as exit execution or break — source: ADR-0003, ADR-0017
- Forbidden: Never read a hide-pose start from `ContactServerTime`; read `FActionState.StartServerTime` — source: ADR-0017, ADR-0018
- Guardrail: The crossing uses the same formula clients use so server and clients cannot round differently

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (CON-R9, AC-CON-13), scoped to this story:*

- [ ] **AC1** (AC-CON-13 server part): **GIVEN** a concealed pawn whose Attack executes at t0 (any target including a co-occupant, or a whiff) **THEN** `OnConcealmentBroken(Pawn)` fires at execution, `SetCullState(Pawn, false)` is called in the same slot, state = Emerging, pose Strike with the `BushStrike` variant, the emerge side is the open side nearest the aim yaw, and concealment breaks identically for a whiff (P3).
- [ ] **AC2** (CON-R9, F3(c)): **GIVEN** Emerging **THEN** the leaf crossing is scheduled at `BushStrikeEmergeSec` (0.35 s) = `ContactServerTime` − `StrikeContactSec`, the pawn leaves the occupancy record at the crossing and the Owner/Team flags clear, and it ends Outside when the strike's recovery ends; the victim outcome is decided at execution (a Prepare during the preface changes nothing).
- [ ] **AC3** (derived, CON-R9 table): **GIVEN** Prepare/Interact inside **THEN** they are rejected `NotNow` and are not a break; **GIVEN** Run inside **THEN** no effect and no dash-out.

---

## Implementation Notes

*Derived from ADR-0017 Decision §3 and CON-R9:*

Subscribe to IIF's `OnActionStarted(Pawn, Attack)` (IIF `story-054-concealment-hears-attack.md`); at the Attack execution slot call `OnConcealmentBroken`, `SetCullState(Pawn, false, ...)`, set state Emerging and schedule `Cross` at `ContactServerTime − StrikeContactSec` (the same formula clients use, so server and clients cannot round differently); the emerge side is the open side nearest the aim yaw (same tie rule as story 005 AC3). The victim outcome is decided at execution by Assassination (IIF R10), including a concealed pawn reversing is Assassination's. `BushStrike` clip and the `FActionState` carrier are owned by Assassination; use a fake clip length in the unit test (the real clip is deferred). A kill in the same tick follows queue order.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-025-cull-push-wiring.md`: the real cull source.
- `story-027-uncull-before-leaf-plane.md`: the observed 0.35 s / 0.75 s timing on an opponent client.
- `story-017-kill-while-concealed-server-rules.md`: the victim side.
- assassination-defence-reversal epic: the strike resolver and `BushStrike` clip.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentCore.cpp` (break path)
- `Source/StealthGameplay/Private/Tests/Concealment/AttackBreaksConcealmentTest.cpp` (classes `FConcealmentBreakAtExecutionTest`, `FConcealmentEmergeTimingTest`, `FConcealmentInsideInputsTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: BreakAtExecution
  - Given: a Concealed pawn whose Attack executes at t0 against a passer-by, a co-occupant, or nothing.
  - When: the slot runs.
  - Then: `OnConcealmentBroken` at t0 and `SetCullState(false)` in the same slot; Emerging with pose Strike (BushStrike) on the open side nearest the aim yaw; the whiff case is identical.
  - Edge cases: an aim yaw equidistant between sides uses the same tie rule as story 005 AC3.
- **AC2**: EmergeTiming
  - Given: Emerging from t0 and a stub strike resolver.
  - When: the crossing item fires.
  - Then: the crossing is at t0 + 0.35 and equals ContactServerTime (t0 + 0.75) minus StrikeContactSec (0.4); the pawn leaves the record and the flags clear at the crossing; it is Outside when the strike recovery ends; a Prepare during the preface does not change the outcome.
  - Edge cases: the recovery end is an Assassination input, so inject a fake clip length; the real clip is deferred.
- **AC3**: InsideInputs
  - Given: a Concealed pawn.
  - When: Prepare, Interact and Run are issued.
  - Then: Prepare and Interact give NotNow and do not break concealment; Run has no effect and no dash-out.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `AttackBreaksConcealmentTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-003-conceal-state-machine-and-occupancy-record.md`, `story-004-hide-enter-execution-and-commit.md`, `story-005-hide-exit-execution-and-blocked-side.md`; IIF `production/epics/input-interaction-framework/story-054-concealment-hears-attack.md`; assassination-defence-reversal epic (`BushStrike` clip, ASN-R21; stories not yet written)
- Unlocks: `story-027-uncull-before-leaf-plane.md`, `story-033-blind-strike-parity-and-kill-timing.md`
