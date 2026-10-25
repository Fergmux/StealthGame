# Story 014: Motion-warp targets are never replicated, and the server lunge is outcome-blind

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (ASN-R16, ASN-R21 uniform network treatment)
**Requirement**: `TR-asn-012`, `TR-asn-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§2.2 last bullet, §7.2, E5, E8)
**ADR Decision Summary**: `UMotionWarpingComponent` replicates `WarpTargets` by default (`COND_SimulatedOnly`), which would send a second, variable-content copy of the strike target to every simulated proxy; every `AStealthPawn`'s component is therefore a subclass that disables that replication identically for every pawn, and strike warp targets are set locally on every machine from replicated state. The server's lunge warps to the fixed `WarpPoint` for every outcome and never re-tracks the victim.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (MotionWarping is Beta in 5.8.4: `MotionWarping.uplugin` `IsBetaVersion: true`; the protected `WarpTargets` disable is on the engine-upgrade checklist)
**Engine Notes**: `UStealthMotionWarpingComponent` calls `Super::GetLifetimeReplicatedProps` and then `DISABLE_REPLICATED_PROPERTY_FAST(UMotionWarpingComponent, WarpTargets)`, because the member is `protected` so the non-FAST macro's name check does not compile from a subclass; the lifetime row becomes `COND_Never` (engine precedent `ClusterUnionComponent.cpp:1406`) and is audited. `AddOrUpdateWarpTargetFromLocation(FName, FVector)` sets a target locally ([V] E4). `UMotionWarpingComponent::GetCharacterOwner` is removed in 5.8: use `GetOwnerAdapter()`. Whether the sample's Blueprint traversal needs server-set replicated targets is [U] and owned by the traversal net-emulation spike (SCL story 045): if it does, traversal moves to the same local-from-replicated-state pattern before this story merges.

**Control Manifest Rules (this layer)**:
- Required: `UMotionWarpingComponent` replicates `WarpTargets` by default; disable it in the subclass with `DISABLE_REPLICATED_PROPERTY_FAST` (leaves a `COND_Never` row) — source: ADR-0018
- Forbidden: Never set a strike or counter warp target through `UMotionWarpingComponent`'s replicated `WarpTargets`; never re-track the victim's capsule during the server lunge — source: ADR-0018
- Required: The reflection audit (ADR-0003 allowlist) shows the `WarpTargets` row as `COND_Never` for every pawn — source: ADR-0018, ADR-0003
- Guardrail: Removing `WarpTargets` replication saves bytes on every warped action

Two commitments: (1) wire: no motion-warping bunch is produced by a strike or counter, and every pawn's component is the same class with the same lifetime rows (identical for NPC and player); (2) lunge: the server's root-motion montage warps toward the fixed `WarpPoint` (story 012) in every outcome, so the attacker's `RepRootMotion` and replicated movement up to contact are identical for a hit, a whiff and a reversal; contact residuals are reconciled by ordinary movement smoothing.

---

## Acceptance Criteria

*From ADR-0018 Validation Criteria (`WarpTargetsNotReplicated`, `LungeOutcomeBlind`) and GDD ASN-R16, scoped to this story (the GDD has no numbered AC for this requirement; AC-ASN-21's pre-contact parity is story 018):*

- [ ] **AC1** (ADR-0018 `WarpTargetsNotReplicated`, [Logic/BLOCKING] reflection audit plus [Integration/BLOCKING]): **GIVEN** every `AStealthPawn`'s motion-warping component **THEN** its `WarpTargets` lifetime row is `COND_Never` and the component class is the same for every pawn; **AND WHEN** a strike and a counter are executed under a bystander capture **THEN** no motion-warping bunch or property record appears on the wire.
- [ ] **AC2** (ADR-0018 `LungeOutcomeBlind`, GDD ASN-R16, [Integration/BLOCKING][Net]): **GIVEN** a hit, a whiff and a reversal with the same `WarpPoint` and attacker path **WHEN** the attacker's `RepRootMotion` and replicated movement are captured from the slot to contact **THEN** they are identical across the three runs, and the server's lunge never re-tracks the victim's capsule (victim moved in the window in one run: no change in the attacker's warp).

---

## Implementation Notes

*Derived from ADR-0018 §2.2, §7.2, E5, E8:*

Create `UStealthMotionWarpingComponent` (subclass on the one pawn class; ADR-0010's migration reparents the sample Blueprint, so the sample's `UMotionWarpingComponent` is replaced when the Blueprint is reparented; the sample assets stay on the never-resave list until then, `production/never-resave-assets.md`). Add the `COND_Never` row to the ADR-0003 allowlist as the audited exception. Client and server both set the local `"StrikeContact"` target from `P_attacker_local + WarpPoint` (story 018 on clients; the server does the same on its own lunge). Re-set, not re-track: the target is computed once at the slot.

Keep the reflection audit generic: it walks the registered lifetime properties of every component on `AStealthPawn` and fails on a `WarpTargets` row not `COND_Never`, which also guards the engine-upgrade checklist (a changed `protected` layout breaks the macro at compile time, by design).

Risk register: motion warping is Beta; if the sample's traversal depends on replicated targets, coordinate with SCL story 045 (traversal under emulation) and ADR-0010 before merging.

**Prototype:** the prototype has no C++ use of motion warping; nothing is promoted or replaced.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-012-slot-write-outcome-blind-and-warp-point.md`: deriving `WarpPoint` and writing `FActionState`.
- `story-018-strike-clip-variants-warp-and-contact-scheduling.md`: client-side clip and local warp target.
- SCL epic: traversal's own use of warp targets (story 045 spike, 047 to 049).

---

## Files

- `Source/StealthGameplay/Public/View/Core/StealthMotionWarpingComponent.h` (`UStealthMotionWarpingComponent`)
- `Source/StealthGameplay/Private/Core/StealthMotionWarpingComponent.cpp` (`GetLifetimeReplicatedProps` with `DISABLE_REPLICATED_PROPERTY_FAST`)
- `Source/StealthGameplay/Private/Tests/Assassination/WarpTargetsNotReplicatedTest.cpp` (class `FAssassinationWarpTargetsNotReplicatedTest`)
- `Source/StealthTestHarness/Private/Assassination/LungeOutcomeBlindTest.cpp` (`StealthGame.Assassination.PIE.LungeOutcomeBlind`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: the wire half has no defined capture fixture and no definition of "motion-warping bunch or property record"; proposed rewrite (unconfirmed): using the story 012 capture rig, across a strike and a reversal-counter, 0 property records for `UStealthMotionWarpingComponent` and 0 bunches on its component channel are logged for any connection.
- AC2: "identical" has no unit and the victim's displacement is unstated; proposed rewrite (unconfirmed): per-tick `RepRootMotion` and `ReplicatedMovement` wire values are byte-equal across the hit, whiff and reversal runs; in a fourth run the victim moves ≥ 100 cm during the window and the attacker's values stay byte-equal to the baseline.

- **AC-1**: `WarpTargets` not replicated
  - Given: every `AStealthPawn` motion-warping component (player and NPC) and the story 012 capture rig
  - When: the lifetime properties are reflected, and a strike and a counter are executed
  - Then: the `WarpTargets` row is `COND_Never`, the component class is identical for all pawns, and 0 warp records appear on the wire
  - Edge cases: a changed engine layout fails at compile time by design; a pawn spawned later has the same row
- **AC-2**: Lunge outcome-blind
  - Given: a hit, a whiff and a reversal with the same `WarpPoint` and path
  - When: `RepRootMotion` and replicated movement are captured from the slot to contact
  - Then: values are identical across runs and no change follows a moving victim
  - Edge cases: the victim moves in only one run; contact residuals reconcile by smoothing and are not asserted

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: `WarpTargetsNotReplicatedTest.cpp` and `LungeOutcomeBlindTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-004-wire-structs-fixed-width.md`, `story-012-slot-write-outcome-blind-and-warp-point.md`; cross-epic `production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md` (traversal's reliance on replicated warp targets, [U]), `story-001-pawn-class-default-subobjects.md`; `production/epics/information-visibility-policy/story-015-replicated-property-audit.md`
- Unlocks: `story-018-strike-clip-variants-warp-and-contact-scheduling.md`
