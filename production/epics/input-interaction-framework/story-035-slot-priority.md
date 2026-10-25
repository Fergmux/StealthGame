# Story 035: AC 20 SlotPriority

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story implements and verifies the one-action-slot rule (GDD R9, ADR-0011 section 4): a pawn performs at most one of {Interact, Prepare, Attack, Hide transition} at a time, with priority Attack > Prepare > Interact > Hide. A higher-priority command preempts an active lower one (cancel reason `Superseded`); a lower-priority command is rejected `Busy`; same-type Interact or Prepare is a no-op; same-type Attack (during Striking) or Hide (during a transition) is `Busy`; any command during a Hide transition, Attack included, is `Busy`; nothing auto-resumes. The AC enumerates the 16 ordered pairs.

It follows story 033 (stage 2 steps 1-2) and is the base of the lockout story (036), the hold stories (037 onward) and the round-queue stories. The slot state machine lives in `FPawnCommandCore` (ADR-0011 section 1): `Idle | Interacting(Target, Start) | Bracing(Start) | Striking | Countering | HideTransition(Enter|Exit)`. ADR-0002 governs the queue ordering that makes "the second command arrives during the first" deterministic.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; one action slot per pawn with fixed priority Attack > Prepare > Interact > Hide, preemption cancelling with `Superseded`, atomic Hide transitions and nothing auto-resuming.
**ADR Version**: 2026-10-24

**Other ADRs**: ADR-0002: Server resolution queue, server clock and round-end staging (one total-order queue keyed (ServerTickIndex, Seq); resolvers register per item type; follow-ons run inline in the slot).

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011 HIGH for the input layer; ADR-0002 MEDIUM)
**Engine Notes**: No V1-V6 item gates this story. `FPawnCommandCore` is plain C++ with no `UWorld` dependency so the headless specs cover it (ADR-0011 section 1, ADR-0008). ADR-0002 E4: a CMC pawn spawned in `TG_PostPhysics` begins ticking in the same frame; not relevant here because the test uses stubs.

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; only `UPawnCommandComponent::WriteActionState` writes `FActionState` — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case (about 5 items/frame), inside the 0.5 ms queue line — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 20 (R9, Logic, U `SlotPriority`): **GIVEN** each of the 16 ordered pairs from {Attack, Prepare, Interact, Hide} **WHEN** the second arrives during the first **THEN** the outcomes are:
    - a higher-priority command preempts (`Superseded`);
    - a lower-priority command is rejected `Busy`;
    - same-type Interact or Prepare is a no-op;
    - same-type Attack (during Striking) or Hide (during a transition) is rejected `Busy`;
    - any command during a Hide transition, Attack included, is rejected `Busy`;
    - nothing auto-resumes.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 4 and Implementation Guidelines:*

`FPawnCommandCore` holds one slot state per pawn: `Idle | Interacting(Target, Start) | Bracing(Start) | Striking | Countering | HideTransition(Enter|Exit)`. Priority **Attack > Prepare > Interact > Hide**: a higher command preempts a lower active one (cancel reason `Superseded`); a lower one is rejected `Busy`. Hide transitions are atomic (rejected while any action is active; every discrete command `Busy` during one, Attack included). Prepare and Interact inside a bush are rejected `NotNow` (story 052). Same-type rules: `InteractBegin` while interacting and `PrepareBegin` while bracing are no-ops with no reply; `Attack` while `Striking` (including recovery and lockout) and `Hide` during a transition are `Busy`. Nothing auto-resumes. `Countering` is pose-locked and rejects every discrete command `Busy`. A legality-class change or phase change cancels the active action with `Illegal` in the same slot; pawn removal cancels with `PawnRemoved`.

**Rationale.** A single slot with a fixed priority lets one pawn do one visible thing at a time, so the replicated pose (one `FActionState`) is unambiguous and the resolution is deterministic for human and AI alike (ADR-0011 section 4). Hide transitions are atomic because Concealment owns the transition length and the slot must never be half-entered.

**Expected outcomes by pair (QL-STORY-READY, ADEQUATE).** Attack to any other: `Busy`; Attack to Attack: `Busy`. Prepare to Attack: `Superseded` (`OnActionCancelled` with Superseded, Attack starts); Prepare to Prepare: no-op, no event, no reply; Prepare to Interact or Hide: `Busy`. Interact to Attack or Prepare: `Superseded`; Interact to Interact: no-op; Interact to Hide: `Busy`. Hide (transition) to every command including Attack and Hide: `Busy`. The Hide-transition rule overrides the priority rule (Hide is the lowest priority yet blocks Attack). Nothing auto-resumes after the preemptor ends: advance past recovery and lockout and assert the preempted action is not active.

**Known GDD defect (do not fix here).** GDD R8's cancel table and AC 16 list Hide among `Superseded` causes for an Interact, but R9 and AC 20 reject Hide against an active Interact as `Busy`. For this story AC 20 and R9 govern (Interact to Hide is `Busy`); see story 039.

**GDD references.** Core Rules R9 (priority, atomic transitions, same-type rules, Countering); States and Transitions; Edge Cases "Ordering and simultaneity". Tuning Knobs: `AttackAttemptLockoutSec` (Assassination-owned, default 1.75) and `CounterDurationSec` (Assassination-owned) occupy the slot; `BushReenterCooldownSec` (Concealment-owned, 6).

**Prototype superseded.** New code, no prototype counterpart (the prototype had independent per-feature flags such as `ServerSetDefence` and `ServerSetInteracting`, so a pawn could interact while defending).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-036-lockout-occupies-slot.md`: the Attack lockout keeping the slot occupied (AC 62)
- `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`: Interact cancel reasons including `Superseded` (AC 16)
- `production/epics/input-interaction-framework/story-052-no-brace-or-tend-in-bush.md`: Prepare and Interact rejected inside a bush (AC 39)
- `production/epics/input-interaction-framework/story-049-reversal-consumes-brace.md`: the `Countering` slot after a reversal (AC 77)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h` (slot state machine and arbitration)
- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp`
- `Source/StealthGameplay/Private/Tests/Input/SlotPriorityTest.cpp` (class `FInputSlotPriorityTest`, name `StealthGame.Input.SlotPriority`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 20 `SlotPriority` (`StealthGame.Input.SlotPriority`): the 16 ordered pairs from {Attack, Prepare, Interact, Hide} resolve per R9
  - Given: a fresh pawn per pair, predicates Allow, buckets full; the first command executed and still occupying the slot (Attack in Striking, Hide in transition).
  - When: the second command resolves in a later slot.
  - Then (first to second): Attack to any other: `Busy`; Attack to Attack: `Busy`. Prepare to Attack: `Superseded` (`OnActionCancelled` Superseded, Attack starts); Prepare to Prepare: no-op, no event, no reply; Prepare to Interact/Hide: `Busy`. Interact to Attack/Prepare: `Superseded`; Interact to Interact: no-op; Interact to Hide: `Busy`. Hide (transition) to every command including Attack and Hide: `Busy`.
  - Edge cases: the Hide-transition rule overrides the priority rule (Hide is lowest priority yet blocks Attack); nothing auto-resumes after the preemptor ends (advance past recovery and lockout, assert the preempted action is not active); `Busy` replies = one each, no state change.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/SlotPriorityTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-033-legality-matrix.md`
  - cross-epic: round-match-framework (queue ordering; stories not yet written)
  - cross-epic: concealment-bushes (Hide transition owner; stories not yet written)
  - cross-epic: assassination-defence-reversal (Attack, Prepare and Countering predicates; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-036-lockout-occupies-slot.md`
  - `production/epics/input-interaction-framework/story-037-hold-completion-epsilon.md`
