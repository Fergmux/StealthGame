# Story 027: AC 8 StageTwoAtResolution

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

This story builds stage 2, the command check, as the component's `IQueueResolver` for the `PawnCommand` item type (GDD R5 item 2, ADR-0011 section 3.2). Stage 2 runs only at the command's queue slot, for every source, against live state: any earlier-keyed event in the same drain (a kill, a state transition, a hold timer) is already reflected when the command is checked. AC 8 is the defining case: an Interact sits in a mailbox, an earlier-keyed transition of the pawn to Informant resolves first in the same drain, and the Interact is then rejected `NotNow` and `OnActionStarted` never fires.

Stories 028 to 033 and most of the later hold and slot stories build on this resolver, so it carries the ordered step list (0 to 5) and the Voided-with-reply mechanism. It depends on the action hub and predicate registry (story 003, needed for step 4 and for `OnActionStarted`) and the ingress function (story 020). The queue itself (Submit, ScheduleAt, keys, drain) is cross-epic (round-match-framework, ADR-0002); this story registers a resolver with it.

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Seven server-validated entry points with no caller identity; two-stage validation where stage 2 (legality class, locomotion, slot arbitration, feature predicate, per-pawn bucket) runs only at the command's queue slot; a failed command is voided and gets exactly one reply.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0011: HIGH for the input layer, MEDIUM for the command path)
**Engine Notes**: No V1-V6 item gates this story. The resolver registers with `RegisterResolver(EQueueItemType, TWeakInterfacePtr<IQueueResolver>)` from the composition root; `TWeakInterfacePtr` requires a UHT `UINTERFACE` (ADR-0002 E7). The drain is a `TG_PostPhysics` tick function on the authority (ADR-0002 section 5), after movement has settled, so the resolver validates against this frame's positions.

**Control Manifest Rules (this layer)**:
- Required: Ingress drops are silent; stage 2 runs only at the command's queue slot and in the stated order; the per-pawn token bucket is consumed only on execution — source: ADR-0011
- Required: A resolution-voided command produces exactly one reply, flushed at the end of the drain's resolution step in one owner-only RPC per connection; `Rejected_Latched` replies `NotNow` through the held-reply buffer — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Forbidden: Never validate everything at ingress (single stage) — breaks source parity and the total order — source: ADR-0011
- Forbidden: Never carry caller identity in an entry point, payload, event or listener; the command component never calls a Roster binding query — source: ADR-0011
- Guardrail: Command interface: stage 2 a few microseconds per frame at the IIF F3 worst case (about 5 items/frame), inside the 0.5 ms queue line — source: ADR-0011

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 8 (R5, Logic, U `StageTwoAtResolution`): **GIVEN** an Interact in a mailbox and an earlier-keyed transition to Informant in the same drain **WHEN** the queue resolves **THEN** the Interact is rejected `NotNow` and `OnActionStarted` does not fire.

---

## Implementation Notes

*Derived from ADR-0011 Decision section 3.2 and Implementation Guidelines:*

Stage 2, in order, against live state at the slot: (0) pawn still valid, not `IsPendingTeardown`, and the payload's control epoch equals the pawn's current one (else `Voided(StaleControl)`; ADR-0010 section 4.4); (1) legality class `Roster.GetLegalityClass(Pawn)` in {Active, Informant, RoundEnded} (Unbound resolves to Active, so the component never learns the binding), against the R7 table; (2) locomotion from `ISharedPawnStateView` (`IsRunning`, `IsInTraversal`); (3) action-slot arbitration (ADR-0011 section 4); (4) the owning feature's predicate `ICommandPredicate::CanBegin` giving `Allow | AllowNoEffect | Reject(Code)`; (5) the per-pawn, per-type token bucket, consumed only when 0-4 pass and the command executes. A command that passes executes inside its slot (writes `FActionState`, starts hold timers, emits `OnActionStarted`, runs follow-ons inline). A command that fails is `Voided` and gets its reply, appended to the sink's per-pass buffer and flushed at the end of the drain's resolution step.

**Rationale.** Stage 2 moved from pre-submit to resolution in the 2026-10-24 design review so that net-receive and host-local commands have no frame-position dependence, and so that same-drain events are visible (GDD R5 design-review note). A "submit then re-validate" model would let an earlier kill or transition in the same drain go unseen.

**GDD references.** Core Rules R5 (stage 2 a-e and the Ordering paragraph), R6 (reply tier), R7 (legality table), R9; Edge Cases "Ordering and simultaneity" (an earlier event invalidates a later command; an Attack whose `PawnRef` died earlier resolves as a strike, never a rejection). Tuning Knobs (defaults): `BucketInteractSize` 3, `BucketInteractPerSec` 2.0 (the bucket seen at step 5).

**Test seam.** Needs a stub queue modelling Submit with keys and drain order (round-match-framework owned; shared doubles in story 095), a Roster stub whose `GetLegalityClass` returns Informant after the transition item, and an `ISharedPawnStateView` stub. The control case (transition keyed after the Interact) must execute and fire `OnActionStarted(Interact)`.

**Prototype superseded.** Supersedes the server RPC bodies `ServerAssassinate(Target, StrikePoint)`, `ServerSetDefence` and `ServerSetInteracting` in `StealthCharacter.cpp`, which validated (if at all) and acted immediately in the RPC body with no queue slot. Not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-028-allow-no-effect-parity.md`: `Allow` versus `AllowNoEffect` parity at step 4
- `production/epics/input-interaction-framework/story-029-rejection-code-set.md`: the reply code per stage and the drain-end enqueue
- `production/epics/input-interaction-framework/story-033-legality-matrix.md`: the full R7 table at step 1-2
- `production/epics/input-interaction-framework/story-035-slot-priority.md`: slot arbitration at step 3
- `production/epics/input-interaction-framework/story-051-wrong-phase-commands.md`: phase and latch handling (`WrongPhase`, `Rejected_Latched`)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h` (stage 2 resolver)
- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp`
- `Source/StealthGameplay/Private/Tests/Input/StageTwoAtResolutionTest.cpp` (class `FInputStageTwoAtResolutionTest`, name `StealthGame.Input.StageTwoAtResolution`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

**Logic — automated test specs (QL-STORY-READY: ADEQUATE):**

- **AC-1**: AC 8 `StageTwoAtResolution`
  - Given: a stub queue, an Interact in the pawn's mailbox with key K2, and a transition of the pawn to Informant with earlier key K1 in the same drain (Roster stub returns Informant afterwards).
  - When: the drain resolves.
  - Then: the Interact is Voided with exactly one `NotNow` reply (enqueued at drain end); `OnActionStarted` not fired; the slot stays Idle.
  - And (control): with the transition keyed after the Interact, the Interact executes and `OnActionStarted(Interact)` fires.
  - Edge cases: transition and Interact from the same drain but different mailboxes; a kill earlier in the drain (pawn removed) voids with `NotNow`; no ingress-time legality check exists (stage 1 passes the Interact).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/StageTwoAtResolutionTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on:
  - `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`
  - `production/epics/input-interaction-framework/story-020-ingress-clamp-and-drop.md`
  - cross-epic: round-match-framework (resolution queue, mailbox and drain; stories not yet written)
  - cross-epic: participant-state-roster (`GetLegalityClass` and `RequestTransition`; stories not yet written)
  - cross-epic: shared-character-locomotion (`ISharedPawnStateView` adapter; stories not yet written)
- Unlocks:
  - `production/epics/input-interaction-framework/story-028-allow-no-effect-parity.md`
  - `production/epics/input-interaction-framework/story-029-rejection-code-set.md`
  - `production/epics/input-interaction-framework/story-030-attack-never-rejected-by-target.md`
  - `production/epics/input-interaction-framework/story-031-token-bucket-defaults.md`
  - `production/epics/input-interaction-framework/story-033-legality-matrix.md`
