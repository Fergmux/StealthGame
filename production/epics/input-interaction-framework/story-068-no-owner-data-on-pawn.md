# Story 068: AC 27 NoOwnerDataOnPawn

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
**ADR Decision Summary**: Every pawn is one class with one replication configuration; ADR-0003 fixes the pawn net parity configuration, the forced-update policy, fixed-width serialisers, and a checked-in replicated-property allowlist audited at build time. ADR-0011 puts owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) on the PlayerController carrier family and nothing but `FActionState` on the pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 HIGH; ADR-0011 input layer HIGH)
**Engine Notes**: ADR-0003 E12 (unverified, byte-capture spike): a `USTRUCT` with a custom `NetSerialize` (`WithNetSerializer`) is sent as a whole when any field changes, with exactly the bits it writes, on the legacy driver; the fixed bit width of `FActionState` is what makes this story's serialisation check meaningful. Replicated properties are enumerated through reflection (`CPF_Net` on the class default object and on live instances); this is pre-cutoff but must include Blueprint replication lists and component sub-objects (ADR-0003 section 8).

**Control Manifest Rules (this layer)**:
- Required: Owner-only data (`FHoldStatus`, rejection codes, owner results, cooldown end) lives on the PlayerController carrier family — source: ADR-0011
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; only `UPawnCommandComponent::WriteActionState` writes `FActionState` — source: ADR-0011
- Forbidden: Never put owner-only data on the pawn — source: ADR-0011
- Required (Foundation Layer Rules): Every new replicated field, fact, audience, condition or replicated class ships with a Visibility Policy row and a signed-off allowlist row; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Forbidden: Never replicate `IsRunning`, `IsInTraversal` or `CommandedDisplacementCm` (owner-local copies for prompts only) — source: ADR-0010

This story is the structural audit that keeps the Shared Pawn's replicated surface clean. The pawn is observed by every opponent client, so anything replicated on it is public: a hold progress, start time, cooldown, rejection code or cancel reason on the pawn would leak what the owner is doing and thinking. IIF R12 therefore allows exactly one IIF-written replicated value on the pawn, `FActionState` (one RepNotify), and requires that it serialise to the same number of bits for every `Pose`, so that kill, whiff and reversal cannot be told apart by packet size. `FHoldStatus`, cooldown and rejection data exist only on `AStealthPlayerController` (the owner-only carrier).

It depends on story 004 (the reply sink and drain-end flush that defines the owner-side carriers) and unlocks story 069 (the forced-net-update behaviour of the same write). It relates to GDD R12 (what replicates) and to ADR-0003 sections 4 (fixed-width serialisers), 6 (owner and team data on the wire) and 8 (allowlist and audit), and ADR-0011 section 7.

---

## Acceptance Criteria

*From GDD `design/gdd/input-interaction-framework.md`, scoped to this story:*

- [ ] AC 27 (R12, Logic, U `NoOwnerDataOnPawn`): **GIVEN** the Shared Pawn class and its components **WHEN** replicated properties are listed **THEN** the only IIF-written one is `FActionState` (fixed serialised size for every `Pose`); **AND** `FHoldStatus`, cooldown and rejection data exist only on the PlayerController.

---

## Implementation Notes

*Derived from ADR-0003 Decision sections 4, 6 and 8 and Implementation Guidelines, and ADR-0011 Decision section 7:*

- `FActionState {Pose, Seq, Variant, StartServerTime, ContactServerTime, WarpPoint}` is declared once in ADR-0018's Key Interfaces (Foundation `Public/View`); this story consumes it. It implements `NetSerialize` with `WithNetSerializer` and writes the same number of bits for every value: `Pose` 8 bits, `Seq` 16, `Variant` 8, `StartServerTime` 64, `ContactServerTime` 64, and `WarpPoint` as three fixed-width signed offsets. No magnitude-packed vector, no optional field, no conditional bit. `StartServerTime` is written in every write, for every pawn kind.
- Owner-only data: `FHoldStatus {ClientCommandSeq, ActionType, Outcome, ServerStartTime, ServerEndTime}`, `ClientCommandRejected` codes and owner results travel in the reply step on the owner's controller (ADR-0011 section 7). Reply structs live in `StealthFoundation/Public/View`. Nothing else replicates: no hold progress, start time, duration, cooldown, target or team to anyone but the owner.
- Gap resolution the qa-lead asks the author to confirm: "the only IIF-written one" cannot be reflected, because `ACharacter` and the CMC carry their own replicated properties. Test it as: the replicated-property set of `AStealthPawn` and its components equals the allowlist (the engine-inherited set from ADR-0003 and ADR-0010) plus `FActionState`, and `FActionState` is the only IIF type among them. The allowlist is the checked-in JSON at `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (ADR-0003 section 8; owned by the Visibility Policy, pawn rows authored by Shared Character). Do not edit that file here without both owners' sign-off.
- "Every Pose": `EActionPose` is owned by ADR-0018 (cross-epic); read its values by reflection (`UEnum`) rather than listing them, so a new pose is covered automatically. Serialise an `FActionState` for each value into a bit writer and require an identical bit count; also compare maximum-value field contents against zero contents.
- Positive control: `FHoldStatus` and the cooldown/rejection carriers exist on `AStealthPlayerController`. A new replicated property added later must fail loudly.
- Prototype disposition: the prototype's `ServerSetDefence` and `ServerSetInteracting` state in `StealthCharacter.cpp`, and the cooldown fields the ADR-0011 Migration Plan names on the PlayerState, are replaced, not ported. This audit is new code.
- Test seams: reflection over `AStealthPawn` class defaults and components, a bit-writer fixture for `NetSerialize`, and the `ReplicationAllowlist.json` loader (staged for cooked Development runs per ADR-0008).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`: the forced net update per `FActionState` write and the dormancy and `bOnlyRelevantToOwner` ban.
- `production/epics/input-interaction-framework/story-084-pawn-replicated-value-parity.md`: replicated value parity between a bound pawn and an NPC pawn, observed on a client.
- `production/epics/input-interaction-framework/story-087-action-pose-bandwidth.md`: `ActionPose` bandwidth.
- `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`: the owner reply carrier and its flush.
- `production/epics/input-interaction-framework/story-093-static-scan-and-prototype-retirement.md`: the static scan for gameplay RPCs on the pawn.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/` (`FActionState` NetSerialize, owned by the animation/assassination work; changed here only if the bit-count check fails)
- `Source/StealthGameplay/Private/Tests/Input/NoOwnerDataOnPawnTest.cpp` (class `FInputNoOwnerDataOnPawnTest`, name `StealthGame.Input.NoOwnerDataOnPawn`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- Gap 1: "the only IIF-written one" cannot be reflected: `ACharacter` and the CMC carry their own replicated properties. Rewrite: "the replicated-property set of `AStealthPawn` and its components equals the allow-list {engine-inherited set from ADR-0003/0010} plus `FActionState`, and `FActionState` is the only IIF type among them".
- Gap 2: "every Pose" depends on the pose enum (ADR-0018, cross-epic); read it by reflection.

QA-lead test sketch (unconfirmed): `StealthGame.Input.NoOwnerDataOnPawn`; Given the `AStealthPawn` CDO and component classes, When replicated FProperties (`CPF_Net`) are listed, Then the set equals the allow-list plus `FActionState` and no property of type `FHoldStatus`, cooldown or rejection data is on the pawn. Given each `EActionPose` value written into an `FActionState`, When serialised with its NetSerialize into a bit writer, Then the bit count is identical for every Pose. Given `AStealthPlayerController`, Then `FHoldStatus`/cooldown/rejection carriers exist there (positive control). Edge cases: a new replicated property added later fails loudly; max-value field contents versus zero contents give the same size.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/NoOwnerDataOnPawnTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`; cross-epic: assassination-defence-reversal (stories not yet written; ADR-0018 `FActionState` and `EActionPose`), cross-epic: information-visibility-policy (stories not yet written; the replication allowlist file and audit), cross-epic: shared-character-locomotion (stories not yet written; pawn rows)
- Unlocks: `production/epics/input-interaction-framework/story-069-force-net-update-on-write.md`
