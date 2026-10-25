# Story 012: Clearing a live binding during Active faults, but level-teardown clears are silent

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity
**ADR Decision Summary**: ADR-0010 forbids destroying a Shared Pawn inside a resolver except through `RequestTeardown`, makes `FellOutOfWorld`/`OutsideWorldBounds` use it, and says `Reset` must never destroy a pawn and `PawnLeavingGame` only unpossesses during world teardown or seamless travel. Pawn `EndPlay` is therefore the one place a binding can be lost without a resolved kill, and the Roster must tell a genuine loss (a pawn destroyed during Round `Active`) from a teardown (travel, PIE end, level unload) so Round can restore a body or report a fault.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0010 (HIGH): post-cutoff `UMotionWarpingComponent::GetOwnerAdapter()` (MotionWarping is Beta in 5.8.4); outstanding verification S8, S14, H1, ST1, P1, the traversal spike and the ADR-0002 E4 spike. The `EEndPlayReason` values the pawn passes (`Destroyed`, `LevelTransition`, `EndPlayInEditor`, ...) are long-standing engine API; the core receives them as a plain enum from the facade, so nothing here is engine-version sensitive.

**Context paragraph**: This story adds `ClearBinding(Pawn, EndPlayReason)`, `InvalidateAllBindings()` and `GetUnboundActiveParticipants()`. Clearing a live binding for an `Active` participant during Round phase `Active` with reason `Destroyed` must record one fault and null the participant's live pawn while leaving state unchanged, whereas `EndPlayInEditor`, `LevelTransition` and `InvalidateAllBindings` are silent. It realises GDD R9 (`ClearBinding`, `InvalidateAllBindings`, results on wrong input), R10 (`GetUnboundActiveParticipants`) and invariants I4 and I5.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Registrations are weak (`TWeakInterfacePtr`/`TWeakObjectPtr`), cleared in `Deinitialize`/`EndPlay`; a dead registration is a fault-sink report, never a crash (ADR-0007)
- Required: Resolvers call `RequestTeardown` and treat actors pending teardown as gone (ADR-0002)
- Forbidden: Never destroy actors inline in a resolver (ADR-0002)
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests (ADR-0008)
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-25** [Core] `StealthGame.Roster.ClearBindingWhileLive`: GIVEN an Active participant's BoundLive pawn, WHEN `ClearBinding(pawn, Destroyed)` is called during phase `Active`, THEN the fault sink records one fault, `GetPawn(Id)` = null, the state is unchanged, and `GetUnboundActiveParticipants` contains Id. WHEN instead `ClearBinding(pawn, EndPlayInEditor / LevelTransition)` is called, or `InvalidateAllBindings` runs, THEN no fault is recorded and no event fires (R9, I4).

---

## Implementation Notes

*Derived from ADR-0010 Implementation Guidelines (`Reset` and `PawnLeavingGame` never destroy a pawn; teardown only via `RequestTeardown`), ADR-0005 section 2 and GDD R9, R10, I4, I5:*

- **ClearBinding(Pawn, EndPlayReason).** Called by the pawn at `EndPlay`. If the binding was still live (`BoundLive`), the participant's live pawn becomes null and the entry is removed. A `Retired` pawn's tombstone also ends at `EndPlay` (R9: "The tombstone lasts until the pawn's `EndPlay` (`ClearBinding`) or `ResetAll`"). The fault is reported **only** when the reason is `Destroyed` and the Round phase is `Active`; every other reason (`EndPlayInEditor`, `LevelTransition`, and travel, PIE end or level unload paths) clears silently. Define a core-side mirror enum of the reasons used (planned `ERosterEndPlayReason`) rather than including engine headers in the core.
- **Fault, not ensure.** Report through the injected fault sink with the clear-binding-while-live code; the sink raises the dev `ensure` (R9). The test asserts the sink only; never rely on `ensure` (GDD Tags).
- **OnBindingLost.** R9 and R12 say that for an `Active` participant the Roster raises `OnBindingLost(Id, PawnDestroyed)` through the story 008 queue (consuming one `EventSeq`) so Round can run its own-card respawn (Round R17.9). The AC does not assert it. Implement it for the `Destroyed` + `Active` case only, keep it out of the teardown paths, and see the open point. The teardown cases must fire **no** event, which the AC does assert.
- **Wrong input.** `ClearBinding` on an `Unbound` pawn returns `NoOp_NotBound` and changes nothing (R9).
- **InvalidateAllBindings().** Called by Session at travel start (`OnTravelStarting`). It clears every live binding and tombstone, touches no lifecycle state, and emits no event (R9, R12). Also clears the `PawnRef` entries of C-TeamView once the push step exists (story 041); nothing to do for that yet.
- **GetUnboundActiveParticipants().** Round's pre-`Active` check (R10, I4): returns the ids of `Active`, `InRound` participants with no live pawn. Return an out-parameter array or a small fixed container; it is Round's cold path, so allocation is allowed but must not be required on the hot-path queries (story 033).
- **Round phase seam.** The fault condition depends on Round phase `Active`, but the GDD's injected-dependency list has no Round-phase input. Add a minimal injected phase provider (planned `IRosterRoundPhase`, or a single `SetRoundPhaseForTests` core seam compiled only under `WITH_STEALTH_TEST_SEAMS`) and flag the missing dependency; stories 025 and 035 need the same input.
- **Open point:** the QL-STORY-READY note that R9/R12 raise `OnBindingLost` for the Destroyed case but the AC does not assert it (no AC covers it, see the QA report's uncovered clauses): record whether it is implemented here or deferred, and add a test via `/qa-plan` if kept. The Round-phase input is also not named in the GDD's dependency list.
- **Prototype disposition:** No prototype counterpart (the prototype ignored a pawn being destroyed under a live player; `AStealthCharacter::EndPlay` had no roster call).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): the binding table and `BindPawn`
- Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): tombstone generation matching and `ReleaseToCrowd`
- Story 020 (Killing an informant retires its binding without a state event): `RetireBinding`
- Story 022 (ResetAll resets every non-dormant record with exactly one roster-reset event): `ResetAll` clearing bindings and tombstones
- Story 025 (Undersized rosters count correctly and lock and reset preconditions hold): further use of the Round-phase input
- Story 041 (A replaced controller or reclaim re-pushes C-TeamView within one server frame): `PawnRef` clearing in C-TeamView
- Story 043 (Player pawns are bound before BeginPlay and a failed bind destroys the pawn before FinishSpawning): the spawn site and its failure handling

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (changed: `ClearBinding`, `InvalidateAllBindings`, `GetUnboundActiveParticipants`, `OnBindingLost` event, end-play reason mirror enum)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (changed)
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (changed: Round-phase input seam)
- `Source/StealthFoundation/Private/Tests/Roster/StealthClearBindingWhileLiveTest.cpp` (new: class `FRosterClearBindingWhileLiveTest`, test name `StealthGame.Roster.ClearBindingWhileLive`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: clearing a live binding during Active faults; teardown clears are silent.
  - Given: an Active participant with a BoundLive pawn, Round phase set to Active through a phase seam (the GDD dependency list names none), fault sink, event spy.
  - When: ClearBinding(pawn, Destroyed); separately ClearBinding(pawn, EndPlayInEditor), ClearBinding(pawn, LevelTransition) and InvalidateAllBindings.
  - Then: the Destroyed case records one fault, GetPawn(Id) null, state unchanged, GetUnboundActiveParticipants contains Id. The teardown paths record no fault and fire no event.
  - Edge cases: R9/R12 also raise OnBindingLost for the Destroyed case; the AC does not assert it. Clearing an Unbound pawn returns NoOp_NotBound.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthClearBindingWhileLiveTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-010-bind-pawn-guards.md`
- Unlocks: None
