# Story 036: AC 62 LockoutOccupiesSlot

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies the slot-occupancy rule for the Attack lockout (GDD R9: "Attack recovery and lockout (`AttackAttemptLockoutSec`, Assassination-owned) occupy the slot for their duration"). After an executed Attack, the pawn's slot stays occupied through recovery and the lockout, so Interact, Prepare and Hide issued during the lockout are rejected `Busy`, and once the lockout has passed each is accepted. The AC stubs the lockout to 1.5 s and tests at +1.0 s (rejected) and +1.6 s (accepted).

It follows story 035 (slot arbitration), which provides the occupancy mechanism; the lockout duration itself is Assassination's value and the seam that tells the IIF the slot is still occupied is a stub in this story. The `Striking` state includes recovery and lockout (ADR-0011 section 4), which is why same-type Attack during that period is also `Busy` (AC 20).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; one action slot per pawn where Attack while Striking (including recovery and lockout) is rejected `Busy` and nothing auto-resumes.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. The lockout expiry is an Assassination-owned time item in the queue (due-time heap, ADR-0002 section 4); here it is a stub reporting the slot occupied until `t_exec + AttackAttemptLockoutSec`.

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source enters through its entry points and the queue — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply (here `Busy`), flushed at the end of the drain's resolution step — source: ADR-0011
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — source: ADR-0011
- Forbidden: Never let a reply code depend on a target pawn's identity, team, concealment or brace state — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 62 (R9, Logic, U `LockoutOccupiesSlot`): **GIVEN** an executed Attack with `AttackAttemptLockoutSec` stubbed to 1.5 **WHEN** Interact, Prepare and Hide are issued at +1.0 s **THEN** each is rejected `Busy`; **AND** at +1.6 s each is accepted.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 4 and GDD R9:*

The Attack slot state is `Striking`, which includes recovery and lockout. While it persists every discrete command that is not a higher-priority preemptor is rejected `Busy`, and Attack itself is `Busy` too. When the lockout ends the slot returns to `Idle`, and commands are then judged on their own merits (stage 2 steps 3-5). The lockout length is `AttackAttemptLockoutSec`, an Assassination-owned key (GDD default 1.75 s; the AC stubs 1.5 s). The IIF does not own or read it directly: the occupancy comes from the Attack's slot state, which Assassination's follow-on ends.

**Rationale.** Occupying the slot during the lockout is what makes the lockout a real action cost: without it a player could Brace or hide in the middle of a failed strike's recovery (GDD R9, "Nothing auto-resumes... recovery and lockout occupy the slot").

**Test wording and seam (QL-STORY-READY).** "Each is accepted at +1.6 s" is ambiguous: issued in sequence on one pawn, Interact then Prepare (preempts, accepted) then Hide would be `Busy`. The qa-lead's rewrite: at +1.6 s each of Interact, Prepare and Hide, issued on its own fresh pawn after an identical Attack, is accepted; lockout is measured from Attack execution. The seam telling the IIF the slot is occupied by lockout is not named; use an Assassination stub that reports the slot occupied until `t_exec + AttackAttemptLockoutSec` (stubbed 1.5). The GDD AC text stays the authority.

**GDD references.** Core Rules R9 (last paragraphs); AC 20 for Attack while Striking. Tuning Knobs: `AttackAttemptLockoutSec` is consumed here, owned by Assassination (GDD default 1.75; test with the 1.5 stub, read from the stub, not hard-coded).

**Prototype superseded.** New code, no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-035-slot-priority.md`: the 16-pair priority table and the slot state machine this story relies on
- `production/epics/input-interaction-framework/story-056-assassination-cooldown-no-reset.md`: the Prepare cooldown not resetting (AC 43)
- `production/epics/input-interaction-framework/story-049-reversal-consumes-brace.md`: the `Countering` slot after a reversal (AC 77)
- `production/epics/input-interaction-framework/story-030-attack-never-rejected-by-target.md`: Attack never rejected by target

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (slot occupancy through recovery and lockout)
- `Source/StealthGameplay/Private/Tests/Input/LockoutOccupiesSlotTest.cpp` (class `FInputLockoutOccupiesSlotTest`, name `StealthGame.Input.LockoutOccupiesSlot`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "Each is accepted at +1.6 s" is ambiguous: issued in sequence on one pawn, Interact then Prepare (preempts, accepted) then Hide would be Busy.
- The seam telling the IIF the slot is occupied by lockout is not named.

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "...at +1.6 s each of Interact, Prepare and Hide, issued on its own fresh pawn after an identical Attack, is accepted. Lockout is measured from Attack execution." Seam: Assassination stub reports slot occupied until t_exec + AttackAttemptLockoutSec (stubbed 1.5).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/LockoutOccupiesSlotTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-035-slot-priority.md`
  - cross-epic: assassination-defence-reversal (`AttackAttemptLockoutSec` and the lockout slot occupancy; stories not yet written)
- Unlocks:
  - None
