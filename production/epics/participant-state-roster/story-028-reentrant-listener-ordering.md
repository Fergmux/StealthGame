# Story 028: Re-entrant listener calls see live state and events dispatch FIFO with strict EventSeq

> **Epic**: Participant State & Roster
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

This story proves the Roster's event discipline holds when a listener calls back into the Roster, which is exactly what Round does when it ends the round from inside an `OnParticipantStateChanged` handler. State is mutated immediately, so the nested call gets an accurate result against live state; its events queue behind the current ones; listeners therefore see events strictly FIFO with `EventSeq` strictly +1 across all event types; and each payload's counts are that event's own snapshot. It realises GDD rules R12 (events queued and dispatched FIFO after the outermost call, payload counts authoritative for ordering), I6 and the Edge Case "a listener calls `RequestTransition` re-entrantly".

**GDD**: `design/gdd/participant-state-roster.md`
**Requirement**: `TR-roster-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0002: Server resolution queue, server clock and round-end staging contract
**ADR Decision Summary**: Every interacting state change resolves in one server-side total order, and resolvers re-validate against live state at the slot and run follow-ons inline. After each `Resolve` the queue calls the injected `IEventFlushBarrier::FlushPending()`, which the Roster implements as FIFO dispatch of the events queued during the outermost call (Roster R12), and asserts nothing remains queued before the next item. The Roster applies calls in the order received and never orders requests itself.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Core (game-thread scheduling, ticking) and Networking (server clock on clients). Tick groups and `FTickFunction` are pre-cutoff and re-verified; no post-cutoff API is used (`UE_LOGF` is preferred for diagnostics, ADR-0007). Verification required by the ADR (drain spawn inside `TG_PostPhysics`, client estimator under net emulation, drain once per frame) concerns the queue, not this plain-C++ core, so none of it gates this story.

**Control Manifest Rules (this layer)**:
- Required: Resolvers re-validate against live state at the slot and run follow-ons inline; a deferred outcome is a new item with its own key and a logged `CauseKey` — source: ADR-0002
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F…Core`) with no `UWorld` dependency, behind a subsystem facade — source: ADR-0007
- Required: Test names: `StealthGame.<System>.<Scenario>` (T1); T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: Never mutate gameplay state outside the drain; never resolve inline in an RPC body or AI source — source: ADR-0002
- Forbidden: Never use wall-clock time as a test pass/fail predicate; never use wall-clock waits, unseeded randomness or shared mutable state in tests — source: ADR-0008
- Guardrail: Queue drain + staging + teardown + Roster flush: <= 0.5 ms listen-host game thread — source: ADR-0002

---

## Acceptance Criteria

*From GDD `design/gdd/participant-state-roster.md`, scoped to this story:*

- [ ] **AC-PSR-34** [Core] `StealthGame.Roster.ReentrantListenerOrdering`: GIVEN a listener that calls `RequestTransition`, WHEN the first transition applies, THEN the nested call returns an accurate result against live state, and events dispatch FIFO after the outermost call with `EventSeq` strictly +1 across all event types. Each payload's counts are its own snapshot (R12, I6).

---

## Implementation Notes

*Derived from ADR-0002 section 5 (Roster event flush, `IEventFlushBarrier`) and GDD rules R12, I6, F1, F2:*

- The FIFO event queue and dispatch-after-outermost-call infrastructure is owned by story 008; this story pins its re-entrancy behaviour in `FRosterCore` (`Source/StealthFoundation/Private/Roster/StealthRosterCore.h/.cpp`). Keep one call-depth counter that spans both the mutating call and the dispatch loop, so a mutating call made from inside a listener is treated as nested: it mutates state immediately and appends its events to the queue instead of dispatching recursively.
- `EventSeq` is assigned when an event is emitted (queued), +1 per event of any type (state, batch, registration, reclaim, connection, teams: I6). So with an outer event E1 and two nested events E2 and E3 raised inside the listener, the listeners observe E1, E2, E3 in that order with consecutive sequence numbers.
- Payload snapshots: `OnParticipantStateChanged` carries `ActiveCountAfter[2]` and `InformantCountAfter[2]` computed at the moment of its own transition. The outer payload therefore excludes the nested change even though live state already includes it, and the nested payload includes both changes. Listeners act on applied events only, never on a request's return value (R12).
- The test listener, on the outer `OnParticipantStateChanged`, calls `RequestTransition` for a second participant and one other event-producing call (for example `EndRoundAll`, story 023). The nested `RequestTransition` must return `Applied` against live state (not a stale-state result), and no nested dispatch happens during the outer dispatch step: the nested events are delivered only after the current listener returns, in queue order.
- The queue's `IEventFlushBarrier::FlushPending()` entry point (ADR-0002) is the facade-level wrapper; the core should expose a flush that the barrier calls, and assert nothing remains queued after it. The barrier registration itself belongs to the composition root, not this core.
- Test construction: `FRosterCore` with injected fakes from `Private/Roster/StealthRosterDeps.h` (authority true, game thread, seeded random source, fault sink) and a recording listener. Participants set up through stories 002, 005 and 014; at least two Active participants on a team so the count snapshots differ.
- Prototype disposition: no prototype counterpart.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 008 (Registration, team, removal and reclaim each fire exactly one sequenced event): the event queue and one-event-per-operation rules.
- Story 014 (Active to Informant applies for each entering reason with the correct pawn disposition): the transition and its payload fields.
- Story 023 (EndRoundAll ends all records once and is idempotent): the nested batch call used by the listener.
- Story 055 (Same-tick assassinations and strike-outs resolve in queue order): ordering of same-tick requests by the Round resolution queue.
- Story 035 (The elimination trigger fires only for a strike-out win and never for disconnect or empty sides): the listener-side F4 trigger evaluation using payloads.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthFoundation/Private/Roster/StealthRosterCore.h` (only if re-entrancy handling needs correction)
- `Source/StealthFoundation/Private/Roster/StealthRosterCore.cpp` (only if re-entrancy handling needs correction)
- `Source/StealthFoundation/Private/Tests/Roster/StealthRosterReentrantListenerOrderingTest.cpp` (class `FRosterReentrantListenerOrderingTest`, test name `StealthGame.Roster.ReentrantListenerOrdering`)
- ADR-0007 header naming: no new header is added; any new one uses the `Stealth` prefix and a project-unique file name.

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation. (At tiers where the QL-STORY-READY gate is skipped — `minimal`, or `lean`/`solo` review mode — no qa-lead specs exist, and this section reads: N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above.)*

**Automated test specs:**

- **AC-1**: re-entrant calls see live state; events dispatch FIFO with strict EventSeq.
  - Given: a listener on OnParticipantStateChanged that calls RequestTransition for a second participant and one other event-producing call (e.g. EndRoundAll).
  - When: the first transition applies.
  - Then: the nested call returns Applied against live state. Recorded order is the outer event, then the nested ones, EventSeq strictly +1 across event types. The outer payload's ActiveCountAfter excludes the nested change; the nested payload includes both.
  - Edge cases: no nested dispatch during the outer call.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Roster/StealthRosterReentrantListenerOrderingTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). `qa.level` is `full`, so the test is required.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/participant-state-roster/story-008-registration-and-team-events.md`, `production/epics/participant-state-roster/story-014-transition-active-to-informant.md`
- Cross-epic: none directly; the `StealthFoundation` module skeleton from `production/epics/tuning-data-config/EPIC.md` reaches this story through story 001.
- Unlocks: `production/epics/participant-state-roster/story-055-simultaneous-resolution.md`
