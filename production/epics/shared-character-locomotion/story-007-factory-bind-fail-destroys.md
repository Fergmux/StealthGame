# Story 007: A rejected bind destroys the pawn before FinishSpawning

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R2 step 3; AC-SCL-03)
**Requirement**: `TR-scl-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 3.1 step 3)
**ADR Decision Summary**: If `Roster.BindPawn` rejects, the factory calls `Destroy()` on the deferred actor before `FinishSpawning` so no client ever receives the actor, and returns the rejection code (Round aborts `BindFailed`; Crowd follows its retry rule).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: A deferred actor that never reaches `FinishSpawning` is never registered for replication, so destroying it there is invisible on the wire. Verify with a client-side actor-channel count in the test, not by inspection.

**Control Manifest Rules (this layer)**:
- Required: a bind rejection destroys the pawn before `FinishSpawning`; a rejected `ApplyAppearance` (returns `false`) does the same and sets `bAppearanceRejected` (ADR-0010, ADR-0012).
- Forbidden: never destroy a pawn inside a resolver except via `RequestTeardown`; this destroy happens inside the factory call stack before the actor is finished, which is the one allowed path.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-03** [P] `FactoryBindFailDestroys`: GIVEN `BindPawn` stubbed to return `Rejected_AlreadyBound`, WHEN the factory runs, THEN the pawn is destroyed before `FinishSpawning`, the code is returned, and a connected client never receives the actor.

*QA clarification (create-stories 2026-10-25, not a GDD change):* an injected binder and a spy on `FinishSpawning` establish the call order; the connected client runs in the harness and must show 0 channel opens for the actor.

---

## Implementation Notes

*Derived from ADR-0010 section 3.1:*

- Extend the factory from story 006: after `ApplyAppearance` and before `FinishSpawning`, call `Roster.BindPawn(Participant, Pawn, ExpectedEpoch)`; on any rejection, `Destroy()` and return the code in `FSpawnResult.Bind`. Use a stubbed Roster facade returning `Rejected_AlreadyBound`; the real Roster arrives with its own epic.
- Assert the destroy happened before `FinishSpawning` by recording call order in the stub, and assert the connected client's actor count did not change across the call.
- Round maps the returned code to `BindFailed`; Crowd's retry rule is its own: do not implement either caller here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 006: the happy-path factory sequence.
- Round's `BindFailed` abort and Crowd's retry (their epics).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawnFactory.cpp` (bind-failure branch)
- `Source/StealthGameplay/Private/Tests/Locomotion/FactoryBindFailDestroysTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.FactoryBindFailDestroys` (harness).*

Test: rejected bind destroys the pawn before `FinishSpawning` (AC-SCL-03)
  Given: `BindPawn` stubbed to return `Rejected_AlreadyBound`, a spy on `FinishSpawning`, and one connected client
  When: the factory runs
  Then: the pawn is destroyed before `FinishSpawning`; the code is returned to the caller; the client never creates or receives the actor (0 channel opens for it)
  Edge cases: also run with `ApplyAppearance` stubbed to return `false` (same destroy path, `bAppearanceRejected` set)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/FactoryBindFailDestroysTest.cpp`, test name `StealthGame.Locomotion.FactoryBindFailDestroys` — must exist and pass (not player-observable, so no screenshot)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-006-single-spawn-site.md`; `production/epics/participant-state-roster/EPIC.md` (stubbed until built)
- Unlocks: None
