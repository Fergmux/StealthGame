# Story 003: Mutation from a worker thread is rejected and faulted

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**Also governed by**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: ADR-0002 resolves every interacting state change in one server-side total order: a per-round queue drained once per frame on the game thread, with nothing outside the drain allowed to mutate gameplay state, and with the Roster's event flush counted inside the 0.5 ms drain budget. ADR-0005 adds that every mutating Roster API asserts authority and the game thread, and that misuse is reported to the fault sink instead of crashing. Together they make "game-thread only" a property the core must check on every mutator, with the thread check injected so it is testable without a real worker thread.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0002 (MEDIUM): no post-cutoff APIs; `UE_LOGF` is preferred for diagnostic text (and queue Event Log content never passes through logging). The verification items (drain spawn behaviour, client estimator accuracy, drain once per server frame on the listen host) concern the queue and clock, not this story. The thread check is an injected predicate; the production adapter (`IsInGameThread()`) is facade work.

**Context paragraph**: Roster invariant I7 says no mutation happens off the game thread. This story adds the injected thread check to `FRosterCore`, wires it into the shared precedence check used by `RequestTransition` and `BindPawn` (R8 step 2, R9 precedence) and proves a worker-thread call returns `Rejected_WrongThread`, records exactly one fault and changes nothing. It realises GDD R2 (authority and thread rule), R8 step 2 and I7.

**Control Manifest Rules (this layer)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade (ADR-0007)
- Required: Registrations are weak (`TWeakInterfacePtr`/`TWeakObjectPtr`), cleared in `Deinitialize`/`EndPlay`; a dead registration is a fault-sink report, never a crash (ADR-0007)
- Forbidden: Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source (ADR-0002)
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests (ADR-0008)
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread (ADR-0002)

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-03** [Core] `StealthGame.Roster.OffGameThreadMutationAsserts`: GIVEN a worker thread, WHEN it calls `RequestTransition` or `BindPawn`, THEN the result is `Rejected_WrongThread`, the fault sink records one fault, and nothing changes (R2, R8, I7).

---

## Implementation Notes

*Derived from ADR-0002 Implementation Guidelines, ADR-0005 section 2 ("every mutating API asserts authority and the game thread") and GDD R2, R8, R9:*

- **Injected thread check.** `IRosterThreadCheck` (declared in `StealthRosterDeps.h`, story 001) answers "is this the game thread". The core calls it through one private precheck helper, `CheckCallPreconditions`, that all mutators will share: step 1 authority (`Rejected_NotAuthority`), step 2 thread (`Rejected_WrongThread`). Build the helper now so stories 004, 014 and 010 reuse it and the precedence order cannot drift.
- **Fault sink, not `ensure`.** A wrong-thread call reports exactly one fault with a distinct fault code to the injected fault sink (R2: the sink raises an `ensure` in development builds). Tests never rely on `ensure` firing because it fires once per call site per process (GDD Tags); assert the sink's recorded faults only.
- **Precedence.** `Rejected_NotAuthority` outranks `Rejected_WrongThread` (R8 precedence 1 vs 2; R9 BindPawn precedence lists the same two first), so the test keeps authority true. Do not test the reverse here (story 004).
- **Which calls.** AC-PSR-03 names `RequestTransition` and `BindPawn`. Neither exists yet: this story introduces only the precheck and minimal entry points whose first step is the precheck. Story 010 (`BindPawn`) and story 014 (`RequestTransition`) add their behaviour behind it. The entry points added here must return `Rejected_WrongThread` before reading any argument, so a worker thread never touches the record table.
- **Fixture seam.** The QA spec needs an Active participant with a bound pawn, but registration lands in story 002 and binding and transitions in stories 010 and 014. Build that fixture through a core test seam (compiled only under `WITH_STEALTH_TEST_SEAMS`, never into Shipping, per ADR-0008) and remove nothing when the real paths land. Open point: the plan lists only story 001 as a dependency although the test also needs registration for ids; the seam avoids that ordering problem.
- **Test shape.** A fake thread check that reports "not game thread" is sufficient and deterministic; an optional real `FRunnable` worker adds coverage but must not be needed for pass/fail (no wall-clock waits).
- **State snapshot.** Add a core-side `Snapshot()` (or reuse the test fixture's) so "nothing changes" is a value comparison of the record table, binding table, `EventSeq`, `RoundEpoch` and `MatchId`; later stories reuse it.
- **Prototype disposition:** No prototype counterpart (the prototype `AStealthGameMode` has no thread or authority guard on its team and life bookkeeping).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004 (Every roster call on a client is rejected or answers not-targetable and faults): `Rejected_NotAuthority` and the client-side query answers
- Story 010 (BindPawn rejects an already-bound participant, an in-use pawn and a round-ended participant): the `BindPawn` body and binding table
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the `RequestTransition` body and the full R8 precedence
- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): the event queue and `EventSeq` machinery that Roster events use

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (changed: shared precheck, thread-check use, state snapshot helper)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (changed)
- `Source/StealthFoundation/Private/Roster/StealthRosterDeps.h` (changed: thread-check and fault-code definitions if not already complete from story 001)
- `Source/StealthFoundation/Private/Tests/Roster/StealthOffGameThreadMutationAssertsTest.cpp` (new: class `FRosterOffGameThreadMutationAssertsTest`, test name `StealthGame.Roster.OffGameThreadMutationAsserts`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**For Logic / Integration stories — automated test specs:**

- **AC-1**: worker-thread RequestTransition and BindPawn rejected and faulted.
  - Given: injected thread check reporting "not game thread" (optionally a real FRunnable worker), authority true, one Active participant with a pawn, fault sink.
  - When: RequestTransition and BindPawn each called from that thread.
  - Then: each returns Rejected_WrongThread, sink holds exactly one fault per call with the expected code, state snapshot unchanged.
  - Edge cases: Rejected_NotAuthority outranks WrongThread (R8 precedence 1 vs 2), so keep authority true; never rely on ensure.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: automated test `Source/StealthFoundation/Private/Tests/Roster/StealthOffGameThreadMutationAssertsTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is full, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-001-record-schema-whitelist.md`
- Unlocks: `production/epics/participant-state-roster/story-004-client-calls-rejected.md`
