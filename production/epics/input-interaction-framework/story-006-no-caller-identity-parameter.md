# Story 006: AC 2 NoCallerIdentityParameter

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
**ADR Decision Summary**: No parameter, event, payload or listener of the command interface carries `IsAI`, `IsPlayer`, a controller, a PlayerState or a team, and the component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query; the only attachment ingress makes is an opaque reply sink.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path MEDIUM. The check uses UE reflection (`FProperty` names and CPP types) plus compile-time `static_assert` on native delegate signatures, because native (non-dynamic) multicast delegates are invisible to reflection. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener; the command component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query — source: ADR-0011
- Forbidden: Never special-case `NM_ListenServer` in input ingress — source: ADR-0011
- Required: Action-hub events fire only inside queue slots and are native (not Blueprint-assignable) — source: ADR-0011
- Required: The listen host's input goes through `IngestCommand(HostLocal)` with the same ingress and reply flush — source: ADR-0011
- Guardrail: The checker is a static/reflection test with no runtime cost in the game build — source: ADR-0011

This story is the parity boundary's tripwire (ADR-0001 parity boundary, GDD R2 "No caller identity, anywhere"). It turns the rule into two automated checks: a reflection scan of every entry-point parameter, hub-event signature and payload struct for identity-shaped names or types, and a CI grep of the command component source for the forbidden identity calls. If any later story (or a hurried feature) adds a `bool bIsAI` to a payload or calls `IsPlayerControlled` in the component, this test fails.

It depends on 003 (the hub's delegates exist to be checked) and 005 (the entry points exist). The scan covers the types of story 001 as well. GDD relation: R2 and R17 (AI subject to the same path); ADR-0011 section 1 and Implementation Guidelines.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 2 (R2, Logic, U `NoCallerIdentityParameter`): **GIVEN** every entry point, hub event and payload struct **WHEN** parameters and fields are reflected **THEN** none has a type or name matching Controller, PlayerState, Team, IsAI or IsPlayer; **AND** a CI grep of the component source for `IsPlayerControlled|IsLocallyControlled|IsNpc|GetTeam|NM_ListenServer` returns 0 hits.

---

## Implementation Notes

*Derived from ADR-0011 Decision §1, §2 and Implementation Guidelines:*

"No parameter, event, payload or listener carries `IsAI`, `IsPlayer`, a controller, a PlayerState or a team; the component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query (IIF R2; ADR-0001 §2)." The one allowed opaque item is `FCommandOrigin { FMailboxId Mailbox; FReplySinkId Sink; /* opaque; null for AI */ }` and the payload's control epoch (`PossessedControlEpoch`): "ControlEpoch" is not "Controller", so neither matches.

**Test design (QL-STORY-READY, to be pinned in the implementation):** "matching" is undefined in the AC, so use a case-insensitive substring match on field names, parameter names and type names against {Controller, PlayerState, Team, IsAI, IsPlayer}. Reflection cannot see the three hub delegates (native, non-dynamic), so their signatures are checked at compile time with `static_assert` on parameter types, exactly `(AStealthPawn*, EActionType[, ECancelReason])`, none deriving from `AController` or `APlayerState`. Reflection covers entry points only if story 005's UFUNCTION choice stands. The CI grep in the AC lists `IsPlayerControlled|IsLocallyControlled|IsNpc|GetTeam|NM_ListenServer`; the ADR also forbids `IsBotControlled` and the Roster binding query, so extend the grep with `IsBotControlled` and the Roster binding query name (the qa-lead's recommendation). Scope the grep to `StealthGameplay/Public/Server/Core` and `Private/Core`. Decide and document whether comments are excluded or a hit is a documented false positive; exclude `.gen.h`. The checker needs a seeded negative fixture (a payload with `bool bIsAI`) that must fail it.

Structs to scan: `FNetPawnCommand`/`FNetCommandArgs` (story 001), `FCommandPayload` (ADR-0002 §2a, Foundation), `FAttackPayload`, `FHideCommand`, `FTraversalHint`.

Prototype: new code, no prototype counterpart (the prototype's `ServerAssassinate(Target, StrikePoint)` named a victim and the controller; replaced). Tuning keys: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-005-entry-point-set.md`: the exact entry-point list (AC 1) this check reads.
- `story-003-action-hub-and-predicate-registry.md`: the hub delegates being checked.
- `story-026-bound-unbound-same-events.md`: the behavioural parity test (bound versus unbound pawn produce the same events).
- `story-093-static-scan-and-prototype-retirement.md`: the wider static scan (no gameplay RPC on the pawn, no banned triggers).

---

## Files

*The code and test files this story changed (written by /dev-story; /story-done checks scope against it).*

- `Source/StealthGameplay/Private/Tests/Input/NoCallerIdentityParameterTest.cpp`
- `Source/StealthGameplay/Private/Tests/Input/IdentityNameChecker.h`
- `tools/ci/check-no-caller-identity.sh`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Reflection cannot see the hub events (native, non-dynamic delegates) and covers entry points only if story 005's UFUNCTION choice stands. "Matching" is undefined (substring? case?). The grep pattern omits `IsBotControlled`, which ADR 1 forbids.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "Case-insensitive substring match on field/param names and type names against {Controller, PlayerState, Team, IsAI, IsPlayer}; hub delegate signatures are checked at compile time; the CI grep adds IsBotControlled and the Roster binding query."
- Scenario sketch from the qa file (`NoCallerIdentityParameter`, AC 2): all entry-point UFUNCTION params and every USTRUCT payload (FNetPawnCommand/Args, FCommandPayload, FAttackPayload, FHideCommand, FTraversalHint) list 0 matches; `FCommandOrigin`'s opaque Sink id and `PossessedControlEpoch` do not match; the 3 hub delegates `static_assert` exact parameter types; CI grep pattern `IsPlayerControlled|IsLocallyControlled|IsBotControlled|IsNpc|GetTeam|NM_ListenServer` returns 0 hits; the checker has a seeded `bool bIsAI` fixture that must fail it.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/NoCallerIdentityParameterTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [x] Created and passing (`StealthGame.Input.NoCallerIdentityParameter`, 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md`, `production/epics/input-interaction-framework/story-005-entry-point-set.md`
- Unlocks: None

---

## Completion Notes
**Completed**: 2026-10-26 (IIF batch 13: command/reply types, core, hub, reply step, transport, seams, spikes V2/V3/V5, input ACs; D53 integration of the production input path)
**Criteria**: 1/1 passing (reflection of every entry-point parameter and payload struct finds no Controller/PlayerState/Team/IsAI/IsPlayer name or type; the CI grep through tools/ci/lint/gate.sh returns 0 hits over the command source; seeded negatives fail)
**Deviations**: Pinned: case-insensitive substring match on names and types plus any AController/APlayerState-derived object class; comments are stripped by the grep (a doc comment is not a hit), generated headers excluded; scope = top-level files of StealthGameplay Public/Server/Core and Private/Core plus Public/Server/ActionHub.h, excluding StealthPawnFactory.h (ADR-0010 factory, binds through the Roster by design); the grep is extended with IsBotControlled and Roster binding queries (FRosterCore, URosterStore, Server/Roster/ includes). FCommandPayload is not reflected: its fields are mirrored and a size check (80 bytes) is the tripwire. Native hub delegates are checked by static_assert.
**Test Evidence**: `Source/StealthGameplay/Private/Tests/Input/NoCallerIdentityParameterTest.cpp`, `tools/ci/check-no-caller-identity.sh`; full suite in `production/qa/evidence/iif-command-path-2026-10-26/full-suite.txt`
Run result: N/A — a static/reflection check
**Reviewed by**: lead-programmer focused review (sonnet; 3 medium + 6 low findings, all fixed except the documented limits) + self-review, IIF batch 13
