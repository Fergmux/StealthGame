# Story 010: BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant

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
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0010 makes `UStealthPawnFactory` the only creator and possessor of pawns and fixes the spawn sequence as deferred spawn, `ApplyAppearance`, `Roster.BindPawn` (if a participant), `FinishSpawning`, possess, in one call stack, with a bind rejection destroying the pawn before `FinishSpawning` so no client ever receives it. `TakeOverCrowdPawn` owns the eligibility check and calls `BindPawn` itself for crowd adoption. This means `BindPawn` is the single gate through which a participant gets a body, and its guards must be exact. ADR-0005 keeps the binding table in the Roster store.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0010 (HIGH): post-cutoff `UMotionWarpingComponent::GetOwnerAdapter()` (MotionWarping is Beta in 5.8.4); outstanding verification S8 (`PawnLeavingGame` hand-over), S14 (owner-only pointers survive possession), H1, ST1, P1, the traversal spike and the ADR-0002 E4 spawn-in-drain spike. None is exercised here: the core sees pawns only as an opaque handle and a liveness provider. ADR-0005 (HIGH): S3, S4, S5 outstanding, untouched.

**Context paragraph**: This story introduces the binding table (each pawn is `Unbound`, `BoundLive(Id)` or `Retired(Id)`) and `BindPawn(Id, Pawn [, ExpectedEpoch])` with its three state guards. It proves the guards return `Rejected_AlreadyBound`, `Rejected_PawnInUse` and `Rejected_WrongState` and leave the table unchanged. It realises GDD R9 (`BindPawn` legality and precedence) and invariant I3 (one live pawn per participant, one participant per pawn).

**Control Manifest Rules (this layer)**:
- Required: `HandleStartingNewPlayer` is a no-op; every pawn comes from `UStealthPawnFactory` (ADR-0005, ADR-0010)
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Every queue item payload, staged-write payload, `FPawnHandle` and `FSubjectRef` is declared in `StealthFoundation/Public/Server/` (ADR-0002, ADR-0007)
- Forbidden: Never call `RestartPlayer` or `SpawnDefaultPawnFor` (ADR-0005, ADR-0010)
- Forbidden: Never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*`/`SCOPE_CYCLE_COUNTER` names (ADR-0007)
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-22** [Core] `StealthGame.Roster.BindPawnGuards`: GIVEN, in turn, a participant that already has a live pawn, a pawn that is BoundLive or Retired, and a RoundEnded participant, WHEN `BindPawn` is called, THEN `Rejected_AlreadyBound` / `Rejected_PawnInUse` / `Rejected_WrongState` and the binding table is unchanged (R9, I3).

---

## Implementation Notes

*Derived from ADR-0010 Implementation Guidelines (spawn sequence, `TakeOverCrowdPawn`), ADR-0005 section 2 and GDD R9, I3:*

- **Opaque pawn handle.** The core sees a pawn only as `{ObjectKey, Generation}` plus answers from the injected pawn-liveness provider (valid, destroyed, pending kill) from `StealthRosterDeps.h` (story 001). The generation is what keeps a recycled pawn from inheriting a tombstone later (story 011, I5). The facade converts `FPawnHandle` (ADR-0002 `TWeakObjectPtr<APawn>`, declared in `Public/Server/`) to that handle; keep the core free of `UObject`.
- **Binding table.** Map `ObjectKey` -> `{Status: BoundLive|Retired, ParticipantId, Generation}`; a pawn absent from the table is `Unbound` (an NPC). Each participant keeps at most one live pawn reference (I3) and each pawn at most one participant. Every lookup is O(1); allocation-free lookups are asserted in story 033.
- **BindPawn legality and precedence** (R9): `Rejected_NotAuthority`, `Rejected_WrongThread` (shared precheck from story 003), `Rejected_UnknownParticipant`, `Rejected_StaleEpoch` (optional `ExpectedEpoch`; asserted in story 024), `Rejected_WrongState` (participant not `Active` or `Informant`), `Rejected_AlreadyBound` (participant already has a live pawn), `Rejected_PawnInUse` (pawn BoundLive or Retired for anyone, **or** null, destroyed or pending kill), then `Applied`. Implement the whole order now so later stories only add tests; this story's cases each violate exactly one rule so that precedence does not mask them.
- **Round-ended participants.** `RoundEnded` participants cannot hold a pawn (R9, States table), so `Rejected_WrongState` for them.
- **No state change on rejection.** Table and participant record are unchanged on every rejection; compare snapshots (helper from story 003).
- **Fixtures through a core test seam.** The lifecycle states (`Active`, `Informant`, `RoundEnded`) cannot yet be reached through `RequestTransition`, `ResetAll` or `EndRoundAll` (stories 014, 022, 023), and a `Retired` binding has no production path until `RetireBinding` (story 020). Set both through a core test seam compiled only under `WITH_STEALTH_TEST_SEAMS` (never into Shipping, ADR-0008) until those stories land.
- **Success path.** Only the minimum `Applied` path is needed (write a `BoundLive` entry). Events: `BindPawn` emits none (R12 lists no bind event; `OnBindingLost` is story 012). Do not add one.
- **Spawn-site coupling (not this story).** The factory call that runs `BindPawn` before `FinishSpawning` and destroys on rejection is story 043. Do not reference `UStealthPawnFactory` from the core.
- **Open point:** R9 gives a null, destroyed or pending-kill pawn `Rejected_PawnInUse`, which the QA spec leaves outside this AC; add it as an extra assertion only if it costs nothing, since it shares the same table lookup.
- **Prototype disposition:** No prototype counterpart for the binding table. The prototype tied a player to a pawn through `AStealthPlayerState` and the possessed `AStealthCharacter`, and spawned via `Super::PostLogin` in `StealthGameMode.cpp`; both are replaced by this table and by factory-only spawning (story 043).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 003 (Mutation from a worker thread is rejected and faulted): the thread and authority precheck for `BindPawn`
- Story 011 (Tombstones respect generations and ReleaseToCrowd needs a disconnected participant): tombstone generation matching and `ReleaseToCrowd`
- Story 012 (Clearing a live binding during Active faults, but level-teardown clears are silent): `ClearBinding` and `InvalidateAllBindings`
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): real state transitions that retire or release a binding
- Story 020 (Killing an informant retires its binding without a state event): `RetireBinding`
- Story 024 (Calls carrying an old round epoch or match id are rejected): the `Rejected_StaleEpoch` assertions for `BindPawn`
- Story 031 (GetLegalityClass answers per pawn state and BindPawn adopts an unbound pawn): the adoption success path and its assertion
- Story 043 (Player pawns are bound before BeginPlay and a failed bind destroys the pawn before FinishSpawning): the spawn site

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (changed: binding table, `BindPawn`, result codes, core test seam for lifecycle and Retired fixtures)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (changed)
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (changed: pawn handle and liveness provider if not complete from story 001)
- `Source/StealthFoundation/Private/Tests/Roster/StealthBindPawnGuardsTest.cpp` (new: class `FRosterBindPawnGuardsTest`, test name `StealthGame.Roster.BindPawnGuards`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant.
  - Given: injected pawn-liveness provider, binding table snapshot, lifecycle states set through a core seam until story 014.
  - When: BindPawn called with an Active participant who already has a live pawn; with a pawn that is BoundLive or Retired for another participant; for a RoundEnded participant.
  - Then: Rejected_AlreadyBound, Rejected_PawnInUse, Rejected_WrongState respectively; binding table unchanged.
  - Edge cases: R9 precedence is WrongState, AlreadyBound, then PawnInUse; make each case violate one rule only. A null/destroyed/pending-kill pawn gives PawnInUse per R9 but is not in this AC.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthBindPawnGuardsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-008-registration-and-team-events.md`
- Unlocks: `production/epics/participant-state-roster/story-011-tombstone-generation-and-release.md`, `production/epics/participant-state-roster/story-012-clear-binding-while-live.md`, `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`, `production/epics/participant-state-roster/story-024-stale-epoch-rejected.md`, `production/epics/participant-state-roster/story-029-reclaim-core.md`, `production/epics/participant-state-roster/story-031-legality-class-and-adoption.md`, `production/epics/participant-state-roster/story-043-player-pawn-bound-before-visible.md`
