# Story 057: AC 44 RosterLegalityClass

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`, `TR-iif-011`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story verifies the single Roster seam of stage 2: the command component asks `Roster.GetLegalityClass(Pawn)` for a class in {Active, Informant, RoundEnded}, and an unbound pawn resolves to Active, so the component never learns the binding (an NPC and a player are indistinguishable below this call). When a pawn's class changes to Informant mid-hold (a `RequestTransition(Informant)` resolving in the queue), the active hold cancels `Illegal` within the same drain's resolution pass, there is no `OnActionCompleted` and no reply, later Informant commands are rejected `NotNow`, and a return to Active is immediate.

It depends on story 039 (hold cancel plumbing, including the `Illegal` reason). In ADR-0011 it is section 3.2 (step 1, legality class, Unbound -> Active) and section 4 ("a legality-class change or phase change cancels the active action with `Illegal` in the same slot"). The Roster legality-class source comes from the participant-state-roster epic. GDD rules: R7 (legality by class), R8 (cancel table: class change), R13 (contexts follow owner state, UX only).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Stage 2 step 1 resolves the legality class from Roster (Unbound resolves to Active, so the component never learns the binding) against the R7 table; a legality-class change cancels the active action with `Illegal`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM in ADR-0011's Engine Compatibility; input-layer HIGH does not apply. No V1-V6 gate (the owner-side `IMC_Informant` swap is story 071, which is gated on V1 and V2). U-form automation test with a Roster stub.

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source enters through its entry points and the queue — ADR-0011
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order — ADR-0011
- Required: Action-hub events fire only inside queue slots — ADR-0011
- Forbidden: Never carry caller identity; the command component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query (only the legality-class function, whose Unbound result is Active) — ADR-0011
- Forbidden: Never let the condition sweep cancel a hold itself — ADR-0011
- Forbidden: Never validate everything at ingress (single stage) — ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case, inside the 0.5 ms queue line — ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 44 (Integration, U `RosterLegalityClass`): **GIVEN** an unbound pawn **THEN** it resolves to Active; **GIVEN** `RequestTransition(Informant)` mid-hold **THEN** the hold cancels `Illegal` in the same resolution.

---

## Implementation Notes

*Derived from ADR-0011 Decision sections 3.2 and 4:*

- **Legality class call.** Stage 2 step 1 calls `Roster.GetLegalityClass(Pawn)` and uses the result against the R7 table (Informant: Move walk and Look only; Run clamped to Walk; all discrete commands `NotNow`). Unbound -> Active is Roster's contract; the stub returns Active for an unbound pawn. A code scan (AC 2 grep, story 006) must find no binding query in the component.
- **Mid-hold class change.** `RequestTransition(Informant)` resolves as its own queue item; the hold's `Illegal` cancel is scheduled by the condition sweep or the transition notification as a `HoldTimer(ConditionCancel)`-style item and resolves in a later slot of the same drain pass (R8: the cancel resolves in a queue slot, not in the transition's own slot). `OnActionCancelled(Interact, Illegal)` fires after the transition item and before the drain's end-of-resolution reply step, with no `OnActionCompleted` and no reply.
- **Known weakness of the GDD criterion (QL-STORY-READY GAPS).** "In the same resolution" is ambiguous: the hold-cancel is a `ConditionCancel` item scheduled by the sweep (R8), so it resolves in a later slot of the same drain, not in the `RequestTransition` slot itself. The qa-lead's proposed rewrite is in QA Test Cases. The GDD AC text stays the authority.
- **After the change.** Informant commands are rejected `NotNow` (legality row); a return to Active is immediate (no cool-off).
- **Test seam.** A Roster stub exposing a settable legality class and `RequestTransition`-style notification; Interact predicate = Allow. Named doubles come from story 095.
- **Prototype.** New code, no prototype counterpart (the prototype had no legality classes).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 033 (`story-033-legality-matrix.md`): the full class by locomotion legality table.
- Story 039 (`story-039-interact-cancel-matrix.md`): the other cancel causes.
- Story 071 (`story-071-context-swap-on-state-and-possession.md`): the owner's input context swap to `IMC_Informant`.
- Story 072 (`story-072-informant-plate.md`): the informant plate UI.
- participant-state-roster epic (cross-epic): `GetLegalityClass`, `RequestTransition` and the unbound rule.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- Source/StealthGameplay/Private/Core/PawnCommandCore.cpp (legality-class read, `Illegal` cancel path)
- Source/StealthGameplay/Private/Core/PawnCommandConditionSweep.cpp (class-change condition scheduling a cancel item)
- Source/StealthGameplay/Private/Tests/Input/RosterLegalityClassTest.cpp (`FInputRosterLegalityClassTest`, `StealthGame.Input.RosterLegalityClass`; U-form automation test)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "In the same resolution" is ambiguous: the hold-cancel is a `ConditionCancel` item scheduled by the sweep (R8), so it resolves in a later slot of the same drain, not in the `RequestTransition` slot itself.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "...THEN OnActionCancelled(Interact, Illegal) fires within the same drain's resolution pass, after the transition item and before the drain's end-of-resolution reply step."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Input/RosterLegalityClassTest.cpp` (U-form automation test `StealthGame.Input.RosterLegalityClass`) OR a playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-039-interact-cancel-matrix.md`; cross-epic: participant-state-roster (stories not yet written) for `GetLegalityClass` and `RequestTransition`; cross-epic: round-match-framework (stories not yet written) for the resolution queue
- Unlocks: None
