# Story 050: AC 69 HoldTimerKeyOrder

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

This story proves that a hold timer which falls due in the same frame as an Attack that kills the tending pawn is decided by queue key order and nothing else. If the Attack has the lower key, the hold cancels `PawnRemoved` and there is no completion; if the `HoldTimer(Complete)` is stamped first, there is exactly one completion (Objectives advances once) and then the kill. Exactly one of the two outcomes occurs. There is no "completion wins" rule (P3: one strike, one deterministic outcome).

It depends on story 039 (the cancel plumbing, `PawnRemoved` and the hub). The stamping rule comes from ADR-0002 section 4 (the governing ADR here): an item with `DueTime <= DrainStartTime` is stamped before that drain's intake items, and one that becomes due during the drain is stamped after the intake; equal `DueTime`s order by the key of the hold's Begin. ADR-0011 section 5 states that the IIF has no tick of its own for holds. GDD rules: R8 (Hold timers are Round queue items, same-frame order, order against commands) and F1.

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: One total-order queue keyed `(ServerTickIndex, Seq)` with a due-time heap: items due at drain start are stamped before intake, items that become due during the drain after it, with equal due times ordered by the hold's Begin key.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH, ADR-0002 MEDIUM)
**Engine Notes**: Command path is MEDIUM in ADR-0011; input-layer HIGH does not apply. No V1-V6 gate. Server time is `double`; the due test uses Round R12's 1 ms epsilon (cited, not redefined by the IIF).

**Control Manifest Rules (this layer)**:
- Required: Hold ends are `ScheduleAt` queue items; a stale `ActionEpoch` voids a timer silently — ADR-0011
- Required: Action-hub events fire only inside queue slots, in key order — ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — ADR-0011
- Forbidden: Never relay action-hub events to a client except through the Visibility Policy — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 69 (R8, Logic, U `HoldTimerKeyOrder`): **GIVEN** an Interact whose `HoldTimer(Complete)` falls due in the same frame as an Attack that kills the tending pawn **WHEN** the drain resolves **THEN** the outcome follows key order: Attack first means the hold cancels `PawnRemoved` and there is no completion, while Complete first means one completion and then the kill; **AND** exactly one of the two outcomes occurs.

---

## Implementation Notes

*Derived from ADR-0002 section 4 and ADR-0011 Decision section 5:*

- **Case 1, Attack first.** The `Complete` timer becomes due during the drain, so it is stamped after intake (F1) and the Attack has the lower key. The kill removes the tender: `OnActionCancelled(T, Interact, PawnRemoved)` once, no `OnActionCompleted`, no Objectives effect (the `HoldTimer(Complete)` slot finds the pawn removed or the `ActionEpoch` voided).
- **Case 2, Complete first.** The timer is due at or before `DrainStartTime`, so it is stamped before intake: `OnActionCompleted` once, Objectives advances once (in the Complete slot, story 055), then the kill; no cancel.
- **Exactly one outcome per run.** Assert event counts for both cases. Equal `DueTime` between `Complete` and `DebounceExpiry` of the same hold: Complete first (Objectives change request 1b).
- **No identity.** Order is key order; the stub queue stamps keys, and the IIF reads none of the participant ordering (Round's R12 sorting by `ParticipantId` is a change request, not used here).
- **Test seam.** Queue stub that stamps keys per ADR-0002 (pre-intake group and intake group), an Assassination stub that kills the tender when its Attack executes, an Objectives stub counting advances. Named doubles from story 095.
- **Tuning keys.** `ObjectiveInteractSec` 3.0 (Objectives-owned, D); no IIF key is read beyond the epsilon from Round.
- **Prototype.** Replaces `OnInteractPressed/Released`, `ServerSetInteracting` and `ServerAssassinate`; the prototype resolved holds and strikes in RPC arrival order. Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 037 (`story-037-hold-completion-epsilon.md`): the completion epsilon and stamping of one `Complete` item.
- Story 055 (`story-055-objectives-completion.md`): the completion slot applying the Objectives effect once.
- Story 039 (`story-039-interact-cancel-matrix.md`): the other cancel causes.
- Story 058 (`story-058-round-queue-order.md`): the key-order outcomes for Prepare versus Attack.
- Assassination and Objectives epics (cross-epic): the real kill and effect.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (`HoldTimer(Complete)` resolver re-check of hold intactness)
- Source/StealthGameplay/Private/Tests/Input/HoldTimerKeyOrderTest.cpp (`FInputHoldTimerKeyOrderTest`, `StealthGame.Input.HoldTimerKeyOrder`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Logic story — automated test spec (`StealthGame.Input.HoldTimerKeyOrder`):**

- **AC-1**: Hold timer versus same-frame kill follows key order
  - Given: Interact due at D = 3.0 for tender T; attacker Attack whose Assassination stub kills T; stub queue stamps keys.
  - When (case 1, Attack first): the timer becomes due during the drain, so it is stamped after intake (F1) and the Attack has the lower key.
  - Then: `OnActionCancelled(T, Interact, PawnRemoved)` once; no `OnActionCompleted`; no Objectives effect.
  - When (case 2, Complete first): the timer is due at or before `DrainStartTime` so it is stamped before intake.
  - Then: `OnActionCompleted` once, Objectives advances once, then the kill; no cancel.
  - Edge cases: exactly one of the two outcomes per run (assert counts); Complete versus `DebounceExpiry` with the same `DueTime`: Complete first.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/HoldTimerKeyOrderTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`; cross-epic: round-match-framework (stories not yet written) for the due-time heap and key stamping; cross-epic: assassination (stories not yet written) and objectives (stories not yet written) for the real kill and completion effect
- Unlocks: None
