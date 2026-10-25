# Story 069: AC 56 ForceNetUpdateOnWrite

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: Every pawn is one class with one replication configuration. `AStealthPawn::ForceNetUpdate()` forwards to `Super` only inside a scoped reason token `FStealthForcedNetUpdate(EForcedUpdateReason)` with reasons `{ActionStateWrite, TraversalStateWrite, DeathWrite}`; every other call is dropped and counted. `UPawnCommandComponent::WriteActionState` is the only writer of `FActionState`, and it does so inside `FStealthForcedNetUpdate(ActionStateWrite)`, identically for NPC and player.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 HIGH; ADR-0011 input layer HIGH)
**Engine Notes**: ADR-0003 E8 and E16 (verified in source): `AActor::ForceNetUpdate` is virtual and every engine caller of `ForceNetUpdate` goes through it; engine callers that reach a pawn (possession paths, `APawn::BecomeViewTarget`, the channel-saturation retry in `ServerReplicateActors`, the PlayerController pause loop) are dropped by the override. Plugin callers are unchecked (a [U] item). `NetDormancy` (`DORM_Never`) and `bOnlyRelevantToOwner` are pre-cutoff `AActor` settings; in 5.5+ the net-config members use setters rather than direct access (ADR-0003 post-cutoff APIs).

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; only `UPawnCommandComponent::WriteActionState` writes `FActionState` — source: ADR-0011
- Required (Foundation Layer Rules, the Core rules do not restate it): `ForceNetUpdate()` on a pawn forwards only inside `FStealthForcedNetUpdate` with an allowed reason, at most once per pawn per frame; the unscoped possession call is dropped and the 15 Hz cadence carries the change — source: ADR-0003, ADR-0001
- Forbidden (Foundation Layer Rules): Never give a pawn dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy; never change net config per instance, kind or on possession — source: ADR-0003
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011

This story proves the replication-delay parity requirement behind IIF R12: every `FActionState` write forces a net update, identically on a bound pawn and an NPC pawn, so the replication delay `U` between a server write and the observer's RepNotify is the same for both. If NPC writes waited for the fixed 15 Hz cadence while player writes forced an update (or the reverse), the observer would see Brace onsets with different latencies and could tell humans from NPCs (AC 46 observes this on a client; this Logic test fixes it at the source). It also asserts that neither pawn uses dormancy or `bOnlyRelevantToOwner`, the two engine mechanisms that would make an idle NPC cheaper than a player and visibly different on the wire.

It depends on story 068 (the `FActionState` layout and the single writer) and unlocks stories 082, 084 and 087 (the observed pose parity, replicated value parity and bandwidth checks, which rely on one forced update per write). It relates to GDD R12 and ADR-0003 section 2 (forced-update policy) and ADR-0011 section 7.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 56 (R12, Logic, U `ForceNetUpdateOnWrite`): **GIVEN** a spy on the net-update request **WHEN** `ActionPose` is written on a bound pawn and on an NPC pawn **THEN** exactly one forced net update is requested per write on each; **AND** neither pawn uses dormancy or `bOnlyRelevantToOwner`.

---

## Implementation Notes

*Derived from ADR-0003 Decision section 2 and Implementation Guidelines, and ADR-0011 Decision section 7:*

- `AStealthPawn` overrides `ForceNetUpdate()` and forwards to `Super` only inside `FStealthForcedNetUpdate(EForcedUpdateReason)` with reasons `{ActionStateWrite, TraversalStateWrite, DeathWrite}`; every other call is dropped and counted in development builds. Cap: at most one forwarded forced update per pawn per server frame, for every pawn kind; extra scoped requests in the same frame coalesce.
- `UPawnCommandComponent::WriteActionState` is the only writer of `FActionState` and runs inside `FStealthForcedNetUpdate(ActionStateWrite)`, identically for NPC and player; Assassination's death-write `Counter` goes through it too (ADR-0011 section 7). "ActionPose" in the AC means `FActionState.Pose` (GDD R12, reconciliation 2026-10-24).
- Clarification from the qa-lead: the spy sits at the request (the scoped token), not at the downstream coalesced forward, so two writes in one drain give two requests (one per write, as the AC is written). The per-frame cap of one forwarded update is ADR-0003's coalescing and is observable only in the forced-update log (story 087 and ADR-0003 Validation Criteria), not in this test.
- Second assertion: `NetDormancy` is `DORM_Never` and `bOnlyRelevantToOwner` is false on both pawns. Read the values from the class defaults and from live instances (a spawned NPC pawn and a bound pawn), because ADR-0003's audit insists the config is identical on defaults and on live instances; this Logic test only needs the two instances. Never set them per instance or per kind.
- A write by any caller other than `WriteActionState` is impossible by construction (a compile-time or static check, see story 093's scope); do not add a runtime guard here.
- Prototype disposition: new code, no prototype counterpart (the prototype's per-feature Server RPCs on `StealthCharacter` — `ServerSetDefence`, `ServerSetInteracting` — are replaced, not ported).
- Test seams: a spy on `ForceNetUpdate` / `FStealthForcedNetUpdate(ActionStateWrite)` (a test double of the scoped token or a counter in development builds), one bound pawn fixture and one NPC pawn fixture created through the same factory path (ADR-0010 factory; cross-epic).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-068-no-owner-data-on-pawn.md`: the replicated-property set and the fixed serialised size.
- `production/epics/input-interaction-framework/story-082-action-pose-parity-observed.md`: observed onset-to-offset parity on an opponent client.
- `production/epics/input-interaction-framework/story-084-pawn-replicated-value-parity.md`: per-property byte parity between a bound pawn and an NPC pawn.
- `production/epics/input-interaction-framework/story-087-action-pose-bandwidth.md`: ActionPose bandwidth.
- `production/epics/input-interaction-framework/story-093-static-scan-and-prototype-retirement.md`: the static scan that no other code writes `FActionState`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Core/PawnCommandComponent.cpp` (`WriteActionState` wrapped in `FStealthForcedNetUpdate(ActionStateWrite)`)
- `Source/StealthGameplay/Private/Tests/Input/ForceNetUpdateOnWriteTest.cpp` (class `FInputForceNetUpdateOnWriteTest`, name `StealthGame.Input.ForceNetUpdateOnWrite`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: AC 56 ForceNetUpdateOnWrite (`StealthGame.Input.ForceNetUpdateOnWrite`)
  - Given: a spy on `FStealthForcedNetUpdate(ActionStateWrite)` / `ForceNetUpdate`; one bound pawn and one NPC pawn.
  - When: `WriteActionState` is called once on each.
  - Then: exactly 1 forced update is requested per write per pawn.
  - Given: the same two pawns.
  - Then: `NetDormancy` is `DORM_Never` and `bOnlyRelevantToOwner` is false on both.
  - Edge cases: two writes in one drain give 2 requests (one per write, as written); a write by any other caller is impossible (compile-time or static check, see story 093's scope).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/ForceNetUpdateOnWriteTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-068-no-owner-data-on-pawn.md`; cross-epic: shared-character-locomotion (stories not yet written; `AStealthPawn` and the `FStealthForcedNetUpdate` scope)
- Unlocks: `production/epics/input-interaction-framework/story-082-action-pose-parity-observed.md`, `production/epics/input-interaction-framework/story-084-pawn-replicated-value-parity.md`, `production/epics/input-interaction-framework/story-087-action-pose-bandwidth.md`
