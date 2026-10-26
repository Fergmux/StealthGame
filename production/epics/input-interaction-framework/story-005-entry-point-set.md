# Story 005: AC 1 EntryPointSet

> **Epic**: Input & Interaction Framework
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Every `AStealthPawn` carries exactly one non-replicated `UPawnCommandComponent` whose entry points (Move, Look, Traverse, InteractBegin/End, PrepareBegin/End, Attack, Hide) are the same functions for every source; discrete entry points build an `FCommandPayload` and submit it rather than executing anything.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path MEDIUM (pre-cutoff component and reflection mechanisms). `UFUNCTION()` with no specifiers is pre-cutoff; the only engine check here is that no entry point carries `FUNC_Net*` flags. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source enters through its entry points and the queue; only `UPawnCommandComponent::WriteActionState` writes `FActionState` — source: ADR-0011
- Forbidden: Never add a Move/Look RPC or a Look event; never add a gameplay RPC on the pawn — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener — source: ADR-0011
- Guardrail: Command interface stage 2 a few microseconds per frame; entry points are O(1) builders — source: ADR-0011

This story pins the public face of the command component: the exact list of entry points. It turns GDD rule R1 ("It exposes exactly these server-side entry points") into a test that fails when someone adds a public command function, removes one, or turns one into an RPC. That matters because the whole parity boundary rests on there being one small, fixed set of doors into a pawn (R2, R17): a sneaky eighth entry point is where a caller-identity or per-kind shortcut would enter.

It sits after the core skeleton (002) because the component wraps the core. Story 006 (AC 2) applies the identity check to these same entry points and so depends on this one. The component shell was created in story 002; this story fills in the nine declared functions as thin builders that submit through the core.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 1 (R1, Logic, U `EntryPointSet`): **GIVEN** the command component **WHEN** its server entry points are listed by reflection **THEN** they are exactly Move, Look, Traverse, Interact(Begin/End), Prepare(Begin/End), Attack and Hide(Enter/Exit).

---

## Implementation Notes

*Derived from ADR-0011 Decision §1, Key Interfaces and Implementation Guidelines:*

Entry points (ADR-0011 §1 table): `Move(FVector2D Direction, EGait Gait)` (continuous, never a queue item), `Look(FRotator Rotation)` (continuous, never a queue item), `Traverse(const FTraversalHint&)`, `InteractBegin(FCourtFixtureId)` / `InteractEnd()`, `PrepareBegin()` / `PrepareEnd()`, `Attack(const FAttackPayload&)`, `Hide(const FHideCommand&)` (`Enter(BushId)` / `Exit(SideIndex)`). "Discrete entry points do not execute anything: they build an `FCommandPayload` and submit it." `WriteActionState` and `QueryCanBegin` are public but are not command entry points.

**Known GDD/ADR defect (not fixed here; ADR governs):** the AC says the entry points are "listed by reflection", but ADR-0011 Key Interfaces declare them as plain C++ methods, not UFUNCTIONs, so UHT reflection lists nothing. The AC also says "Hide(Enter/Exit)" (two) where the ADR has one `Hide(FHideCommand)`. QL-STORY-READY proposes: declare every entry point `UFUNCTION()` with no specifiers (so none is a Server/Client/NetMulticast RPC); the UFUNCTIONs declared on `UPawnCommandComponent` are exactly Move, Look, Traverse, InteractBegin, InteractEnd, PrepareBegin, PrepareEnd, Attack, Hide (nine), and `WriteActionState` and `QueryCanBegin` are not UFUNCTIONs. This departs from the ADR snippet, so the author must confirm it, or choose the alternative the qa-lead offers: an exhaustive `ECommandKind` switch plus a `constexpr` list of entry-point names. Whichever is chosen, record it in the implementation and in the test comment. The mapping from `ECommandKind` (story 001) to the seven discrete entry points is 1:1 with Hide = HideEnter + HideExit.

The component is non-replicated (`SetIsReplicated(false)`, no replicated property) and exactly one exists per pawn, created in the pawn's constructor; the test also asserts that.

Prototype replaced: `StealthCharacter.cpp` `SetupPlayerInputComponent`, `OnAssassinatePressed`, `OnDefencePressed/Released`, `OnInteractPressed/Released` and the server RPCs `ServerAssassinate`, `ServerSetDefence`, `ServerSetInteracting` (the per-feature entry points this single set supersedes). Tuning keys: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-002-pawn-command-core-skeleton.md`: the core and component shell this story fills.
- `story-006-no-caller-identity-parameter.md`: AC 2, the identity-name check on these entry points, hub events and payloads.
- `story-020-ingress-clamp-and-drop.md`: stage-1 ingress that feeds these entry points for humans.
- `story-027-stage-two-at-resolution.md`: what happens at the slot for each submitted command.
- `story-093-static-scan-and-prototype-retirement.md`: removal of the prototype per-feature input and RPCs.

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `Source/StealthGameplay/Public/Server/Core/StealthPawnCommandComponent.h`
- `Source/StealthGameplay/Private/Core/StealthPawnCommandComponent.cpp`
- `Source/StealthGameplay/Public/Server/Core/PawnCommandTypes.h`
- `Source/StealthGameplay/Public/View/Core/StealthGait.h`
- `Source/StealthGameplay/Private/Tests/Input/EntryPointSetTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "Listed by reflection" cannot work as written: the ADR Key Interfaces declare the entry points as plain C++ methods, not UFUNCTIONs, so UHT reflection lists nothing. The AC also says "Hide(Enter/Exit)" (two) where the ADR has one `Hide(FHideCommand)`.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Every entry point is declared `UFUNCTION()` with no specifiers (so none is a Server/Client/NetMulticast RPC). The UFUNCTIONs declared on UPawnCommandComponent are exactly Move, Look, Traverse, InteractBegin, InteractEnd, PrepareBegin, PrepareEnd, Attack, Hide (9); WriteActionState and QueryCanBegin are not UFUNCTIONs." Author must confirm (departs from the ADR snippet) or choose an exhaustive ECommandKind switch plus a constexpr list.
- Scenario sketch from the qa file (`EntryPointSet`, AC 1): `TFieldIterator<UFunction>(EIncludeSuper::ExcludeSuper)` names equal the 9 names exactly and none has FUNC_Net/FUNC_NetServer/FUNC_NetClient/FUNC_NetMulticast; `ECommandKind` maps 1:1 to the 7 discrete entry points; a new public method added later fails the test (intended); inherited `UActorComponent` UFUNCTIONs excluded by ExcludeSuper; component is non-replicated and exactly one per pawn.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/EntryPointSetTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (`StealthGame.Input.EntryPointSet`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-002-pawn-command-core-skeleton.md`; cross-epic: shared-character-locomotion (stories not yet written; `AStealthPawn` carrying the component)
- Unlocks: `production/epics/input-interaction-framework/story-006-no-caller-identity-parameter.md`

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 passing (reflected entry points exactly Move, Look, Traverse, InteractBegin, InteractEnd, PrepareBegin, PrepareEnd, Attack, Hide; none an RPC; one non-replicated component per pawn; each discrete entry point submits its kind, Move and Look submit nothing)
**Deviations**: Decision (logged): every entry point is a UFUNCTION() with no specifiers (the QL-proposed resolution of the 'listed by reflection' defect); Hide is one UFUNCTION with FHideCommand covering HideEnter/HideExit. WriteActionState and QueryCanBegin are not declared yet (FActionState and the dry run arrive with stories iif-069 and iif-027+). Human commands reach the same entry points through FScopedCommandOrigin opened by ingress (opaque mailbox/sink/epoch; no identity). The Move intent is re-applied by the component every tick (tick enabled only while an intent is held).
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Input/EntryPointSetTest.cpp`; full suite in `production/qa/evidence/iif-command-path-2026-10-26/full-suite.txt`
Run result: N/A — reflection set; the entry points in play are observed through story iif-094's run
**Reviewed by**: lead-programmer focused review (sonnet; 3 medium + 6 low findings, all fixed except the documented limits) + self-review, IIF batch 13
