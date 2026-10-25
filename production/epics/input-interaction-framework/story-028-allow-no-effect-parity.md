# Story 028: AC 9 AllowNoEffectParity

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story pins the `AllowNoEffect` rule of stage 2 step 4 (GDD R5 item 2d, ADR-0011 section 6). A feature predicate may answer `Allow`, `AllowNoEffect` or `Reject(Code)`. `AllowNoEffect` runs exactly like `Allow` (same pose, same hold timer, same events) and only its completion changes nothing. It exists so that a decoy action, such as an NPC tending a site it has no side for, is indistinguishable from a real one; the choice between `AllowNoEffect` and `Reject` may depend only on the site and the legality class, never on who controls the pawn.

It follows story 027 (the stage 2 resolver). The IIF-side assertion is that the IIF itself treats the two verdicts identically; whether `Allow` actually advances the objective is Objectives' behaviour and is verified by the Objectives epic (AC 42, story 055).

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; feature predicates are consumer-owned interfaces registered weakly per `EActionType`, and `AllowNoEffect` runs exactly like `Allow` so decoy actions are indistinguishable.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. `ICommandPredicate` is a `UINTERFACE(meta=(CannotImplementInterfaceInBlueprint))` so `TWeakInterfacePtr` works (ADR-0002 E7); `CanBegin` must be `const` and side-effect-free because `QueryCanBegin` (AI dry run) calls the same predicates.

**Control Manifest Rules (this layer)**:
- Required: Feature predicates are registered weakly from the composition root per `EActionType` — source: ADR-0011
- Required: Hold ends are `ScheduleAt` queue items — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Forbidden: Never let `AllowNoEffect` vs `Reject` depend on anything but the site and legality class — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 9 (R5d, Logic, U `AllowNoEffectParity`): **GIVEN** predicates returning `Allow` and `AllowNoEffect` **WHEN** an Interact runs to D **THEN** ActionPose writes, timers and events are identical; **AND** only `Allow` changes objective state.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 6 (Feature predicates) and Implementation Guidelines:*

`AllowNoEffect` runs exactly like `Allow` (same pose, timer, events) and only its completion changes nothing. In the IIF this means a single execution path keyed on "verdict is not Reject"; the verdict value must not select any different pose write, `HoldTimer` scheduling, hub event or `IInteractTargetOwner` call. `IInteractTargetOwner::OnInteractExecuted` and `OnInteractEnded(Completed)` are called identically under both; the owner (Objectives) is what decides to apply or not apply its effect.

**Rationale.** If the framework treated the verdicts differently in any observable way (pose, timing, events), NPC decoy tends would differ from player tends and become an identity tell (GDD R5 item 2d). The qa-lead notes that the "only `Allow` changes objective state" clause has no observable inside the IIF, because the `IInteractTargetOwner` callbacks carry no verdict.

**GDD references.** Core Rules R5 item 2d; R8 Interact effect; Edge Cases (a modified client probing a hidden court resolves as `AllowNoEffect`, AC 71). No tuning keys beyond `ObjectiveInteractSec` (Objectives-owned, 3.0) which supplies D.

**Rewrite note.** The qa-lead proposes scoping this story's observable to the IIF: under `Allow` and `AllowNoEffect` the IIF produces identical `ActionPose` write sequences, scheduled `HoldTimer` items, hub events and `IInteractTargetOwner` call sequences; the objective-state difference is verified by the Objectives epic (AC 42), not here. The GDD AC text stays the authority.

**Prototype superseded.** New code, no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-027-stage-two-at-resolution.md`: the stage 2 resolver this story exercises
- `production/epics/input-interaction-framework/story-055-objectives-completion.md`: that only `Allow` advances objective state (AC 42, Objectives owns the effect)
- `production/epics/input-interaction-framework/story-060-hidden-site-probe.md`: the hidden-site oracle guard (AC 71)
- `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`: predicate registration

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp` (single execution path for `Allow` and `AllowNoEffect`)
- `Source/StealthGameplay/Private/Tests/Input/AllowNoEffectParityTest.cpp` (class `FInputAllowNoEffectParityTest`, name `StealthGame.Input.AllowNoEffectParity`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "Only `Allow` changes objective state" has no observable inside the IIF. The `IInteractTargetOwner` callbacks carry no verdict, so a stub owner cannot distinguish the cases and the check is either tautological or Objectives' (cross-epic).

QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Under Allow and AllowNoEffect the IIF produces identical ActionPose write sequences, scheduled HoldTimer items, hub events and IInteractTargetOwner call sequence; the objective-state difference is verified by the Objectives epic (AC 42), not here."

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/AllowNoEffectParityTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-027-stage-two-at-resolution.md`
  - cross-epic: objectives-chain (`IInteractTargetOwner` implementation and its effect; stories not yet written)
- Unlocks:
  - None
