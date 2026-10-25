# Story 006: Fixture claims and the interact-target owner

> **Epic**: Objectives Chain
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/objectives-chain.md` (OC-7 predicate, OC-8 several tenders at one fixture, OC-14 interaction time)
**Requirement**: `TR-oc-004` (the predicate reads public occupancy, which this story defines as active-hold claims), `TR-oc-005` (the owner callbacks are the completion gate's entry)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0020: Objectives data and team-view sub-structs (Decision sections 5.1 to 5.3)
**Other ADRs**: ADR-0011: Pawn command interface and input pipeline (the consumer-owned `IInteractTargetOwner` amendment this story implements; applied by its owner)
**ADR Decision Summary**: One interact-target owner is registered for `Interact`. `GetInteractHoldSec()` returns the frozen `ObjectiveInteractSec` with no court parameter, so `D` cannot depend on a court's role. `OnInteractExecuted` claims one of the fixture's `FixtureInteractSideCount` faces (the free face nearest the pawn, ties by face index) and records the pawn's control epoch; `OnInteractEnded` releases it. A body that merely stands at a face holds no claim. Called outside a slot (invalid key) it only releases the claim.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW (plain C++ claim table over an engine-free interface)
**Engine Notes**: None. `UINTERFACE(CannotImplementInterfaceInBlueprint)` and weak registration are pre-cutoff patterns owned by the IIF story that declares the interface.

**Control Manifest Rules (this layer)**:
- Required: The hold duration is one value for every court (`GetInteractHoldSec()` has no court parameter) — source: ADR-0020
- Required: An interact-end call outside a slot only releases the claim — source: ADR-0020
- Forbidden: Never set a per-court hold duration; never branch on player vs NPC in Objectives except the gate's server-only queries — source: ADR-0020
- Guardrail: Claim operations O(1) over at most four faces per fixture — source: ADR-0020

"Physically occupied" in OC-8 had two readings; ADR-0020 section 5.3 chose the second and the game-designer signed it off: a stand point is occupied only by an active Interact hold, because that is what every observer can see (who is tending where), the same for every pawn and every court, so the full-fixture reject is no oracle. A body standing at a face without holding blocks it physically (collision, and the IIF's `OutOfReach` reach and facing check at Begin) but holds no claim, so nobody can lock a fixture by parking at it. This story builds the claim table and the owner interface; the predicate that reads the claim count is story 007 and the gate that reads the recorded control epoch is story 008.

---

## Acceptance Criteria

*From ADR-0020 sections 5.1-5.3 and GDD OC-7/OC-8/OC-14, scoped to this story (criteria are ADR-derived because the GDD states no AC for the claim table):*

- [ ] **AC1** (ADR 5.2 [U] `InteractHoldSecOneValue`): `GetInteractHoldSec()` returns the frozen `ObjectiveInteractSec`, takes no court parameter and is equal for every court; `D` is read once at execution, so a later tuning publish never changes a running hold and the next Begin reads the new frozen value.
- [ ] **AC2** (ADR 5.1, 5.3 [U] `ClaimLifecycle`): `OnInteractExecuted` claims the free face nearest the pawn (ties to the lower face index) and records the pawn's control epoch with the claim; `OnInteractEnded` (`Completed` or `Cancelled`) releases only that pawn's own claim; a pending debounced End keeps the claim until the `DebounceExpiry` slot ends the hold.
- [ ] **AC3** (ADR 5.1 outside a slot, OC-8 [U] `ClaimReleaseOutsideSlot`): `OnInteractEnded` with an invalid `FQueueKey` (round-end staging, pawn teardown) is a claim release only (gate not entered, log call count 0); a body standing at a face with no hold holds no claim and cannot lock a fixture.
- [ ] **AC4** (ADR 5.1 [U] `ClaimConsistencyCheck`): in a non-Shipping build, at drain end each fixture's claim count equals the number of IIF `Interacting` slots targeting it; a seeded mismatch reports to the fault sink exactly once.

---

## Implementation Notes

*Derived from ADR-0020 Decision sections 5.1-5.3:*

The interface is declared by the IIF epic in Core (`StealthGameplay/Public/Server/Core/InteractTargetOwner.h`): `GetInteractHoldSec() const`, `OnInteractExecuted(const AStealthPawn&, FCourtFixtureId, FQueueKey)`, `OnInteractEnded(const AStealthPawn&, FCourtFixtureId, EInteractEnd End /* Completed | Cancelled */, FQueueKey)`. Objectives implements it on `UObjectivesService`; do not redeclare it. The IIF calls `OnInteractExecuted` inline in the Begin's slot after executing the command and `OnInteractEnded` inline in the slot that ends the hold, before `OnActionCompleted`/`OnActionCancelled` fan out, for every pawn kind and with no caller identity. Lifecycle rules of the amendment, all asserted here: the round-end staging pass cancels frozen holds where `CurrentKey()` is invalid, so an invalid key means claim release only; pawn teardown likewise releases the claim; `D` is read once at execution to schedule `HoldTimer(Complete)`; a pending debounced End keeps the claim until the `DebounceExpiry` slot ends the hold (the hold is still running until then, IIF R8); exactly one owner is registered for `Interact` (if a second interact-target kind is ever added the amendment must route per kind).

The claim table is server-only, kind-blind and keyed by fixture: face index to {pawn handle, recorded control epoch}. "Nearest the pawn's position in that slot" uses the pawn's position at the Begin slot; the face positions come from the court's fixture geometry (LSC request: four faces, at least 3 clear by 1.2 m, all inside the sun pool; `FixtureInteractSideCount` 4). The recorded control epoch is what gate (c) compares in story 008 (`UStealthPawnFactory::GetControlEpoch`, ADR-0010 section 4.4).

The non-Shipping drain-end consistency check reads the IIF's count of `Interacting` slots per fixture through a test seam, and reports through `IStealthFaultSink`. Wrap it in `#if !UE_BUILD_SHIPPING`; a grep of the guard is part of the edge-case test.

Prototype replaced: the prototype counted interactors with a replicated `InteractorCount` on `AStealthObjective` and timed the hold from client-set state; neither is ported (ADR-0020 Alternative 1; story 022).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-007-interact-predicate-and-feint-parity.md`: `CanBegin` reading the claim count.
- `story-008-completion-gate-and-stage-effects.md`: the gate (a)-(e) inside `OnInteractEnded(Completed)`.
- IIF `story-003-action-hub-and-predicate-registry.md` and `story-055-objectives-completion.md`: the interface declaration, registration and the IIF-side call order (this story is the Objectives implementation, verified by those stories' stubs).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesClaims.h` (claim table)
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesClaims.cpp`
- `Source/StealthGameplay/Private/Features/Objectives/ObjectivesService.cpp` (`IInteractTargetOwner` bodies)
- `Source/StealthGameplay/Private/Tests/Objectives/InteractHoldSecOneValueTest.cpp` (class `FObjectivesInteractHoldSecOneValueTest`, name `StealthGame.Objectives.InteractHoldSecOneValue`)
- `Source/StealthGameplay/Private/Tests/Objectives/ClaimLifecycleTest.cpp` (`StealthGame.Objectives.ClaimLifecycle`)
- `Source/StealthGameplay/Private/Tests/Objectives/ClaimReleaseOutsideSlotTest.cpp` (`StealthGame.Objectives.ClaimReleaseOutsideSlot`)
- `Source/StealthGameplay/Private/Tests/Objectives/ClaimConsistencyCheckTest.cpp` (`StealthGame.Objectives.ClaimConsistencyCheck`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

- **AC-1** (`InteractHoldSecOneValue`): single hold value
  - Given: tuning `ObjectiveInteractSec` 3.0 frozen; stub courts with different ids
  - When: `GetInteractHoldSec()` is read per court; a hold begins; tuning publishes 5.0 mid-hold
  - Then: same value for every court and the API has no court parameter; the running hold keeps 3.0; the next Begin reads the new frozen value
  - Edge cases: read exactly at the execution tick; publish in the same tick as execution.
- **AC-2** (`ClaimLifecycle`): claim on execute, own release only
  - Given: fixture with `FixtureInteractSideCount` faces at known positions; two pawns; stub IIF
  - When: `OnInteractExecuted` for pawn A then B; `OnInteractEnded(A, Completed)`; pawn B Ended(Cancelled); a debounced End case
  - Then: each claim is the free face nearest the pawn, ties to the lower face index, with control epoch recorded; Ended releases only that pawn's claim; a pending debounced End keeps the claim until the `DebounceExpiry` slot
  - Edge cases: all faces taken (no claim, no corruption); pawn equidistant from two faces.
- **AC-3** (`ClaimReleaseOutsideSlot`): invalid key End and non-holding body
  - Given: a claim held; stub pawn teardown
  - When: `OnInteractEnded` with an invalid `FQueueKey`; a body stands at a face with no hold
  - Then: invalid-key End is a claim release only (gate not entered, log call count 0); the standing body holds no claim and cannot lock a fixture
  - Edge cases: End for a pawn with no claim is a no-op; teardown during Active versus at round end.
- **AC-4** (`ClaimConsistencyCheck`): drain-end consistency check
  - Given: non-Shipping build; stub IIF reporting Interacting slots per fixture; fault sink
  - When: the queue drain ends
  - Then: per-fixture claim count equals Interacting slots targeting it; a seeded mismatch reports to the fault sink exactly once
  - Edge cases: check absent in Shipping (verified by guard grep); zero slots and zero claims passes.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the four test files under `Source/StealthGameplay/Private/Tests/Objectives/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/objectives-chain/story-001-objectives-tuning-rows-and-invariants.md`, `production/epics/objectives-chain/story-005-service-shell-and-composition-root.md`; cross-epic: `production/epics/input-interaction-framework/story-003-action-hub-and-predicate-registry.md` (declares `IInteractTargetOwner`), `production/epics/input-interaction-framework/story-095-command-test-seams.md`; cross-epic: level-socket-contract (fixture face geometry; stories not yet written)
- Unlocks: `production/epics/objectives-chain/story-007-interact-predicate-and-feint-parity.md`, `production/epics/objectives-chain/story-008-completion-gate-and-stage-effects.md`, `production/epics/objectives-chain/story-009-final-completion-and-win-declaration.md`
