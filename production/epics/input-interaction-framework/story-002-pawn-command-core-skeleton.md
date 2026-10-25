# Story 002: FPawnCommandCore runs headless with an injected clock and stub queue

> **Epic**: Input & Interaction Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/input-interaction-framework.md`
**Requirement**: `TR-iif-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Pawn command interface and input pipeline
**ADR Decision Summary**: The command logic (slot state machine, legality table, token buckets, debounce, reply decision) lives in a plain-C++ `FPawnCommandCore` with no `UWorld` dependency, wrapped by a non-replicated `UPawnCommandComponent` with seven entry points, so ADR-0008's headless specs cover it.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Command path is MEDIUM (pre-cutoff component and RPC mechanisms), the epic as a whole HIGH. No post-cutoff API is used here. The relevant constraint is E7 of ADR-0002 (cited by ADR-0011 for weak interface pointers): `TWeakInterfacePtr<T>` requires a UHT `UINTERFACE`, so the injected clock and queue seams of this story are plain C++ abstract classes held by raw reference or `TSharedRef`, not `TWeakInterfacePtr`. No V1-V6 item gates this story.

**Control Manifest Rules (this layer)**:
- Required: Every pawn carries exactly one non-replicated `UPawnCommandComponent`; every discrete command from any source enters through its entry points and the queue; only `UPawnCommandComponent::WriteActionState` writes `FActionState` — source: ADR-0011
- Required: Hold ends are `ScheduleAt` queue items; the release debounce is server-side and identical for every pawn — source: ADR-0011
- Forbidden: Never carry caller identity (`IsAI`, `IsPlayer`, controller, PlayerState, team) in an entry point, payload, event or listener; the command component never calls `IsPlayerControlled`, `IsLocallyControlled`, `IsBotControlled` or a Roster binding query — source: ADR-0011
- Forbidden: Never time a hold on the IIF's own tick, `FTimerManager` or a client timestamp — source: ADR-0011
- Guardrail: Command interface stage 2 a few microseconds per frame at the IIF F3 worst case (about 5 items per frame), inside the 0.5 ms queue line — source: ADR-0011

This story lays the logic skeleton the rest of the server-side command stories grow into. It builds `FPawnCommandCore` as a plain C++ class (no `UWorld`, `AActor`, `UActorComponent` or `FTimerManager` reference) that takes its notion of time from an injected clock interface and submits work to an injected queue interface that mirrors ADR-0002's `Submit` and `ScheduleAt`. The point is testability: every later Logic story in this epic (ingress, legality, slot priority, hold timers, debounce) asserts on this class directly under ADR-0008 headless specs with no PIE world.

It sits second because it needs the types of story 001 and everything server-side needs it. The real round queue is a cross-epic dependency (round-match-framework, stories not yet written); until it lands the stub queue is the only queue. The skeleton's behaviour is deliberately minimal (an `InteractBegin` then `InteractEnd` submits exactly two items, in order); slot arbitration, legality and buckets arrive in stories 020 and 027-039. GDD relation: R1 (the single boundary), R2 (no caller identity, so the core has no controller or team parameter), R8 (hold timers come from server time, hence the injected clock). ADR-0011 sections 1 (boundary), 3.2 (stage 2 order, filled in by later stories), 4 (slot) and 5 (hold timers).

Per the plan's additions, this story also owns only the *interface declarations* of the cross-epic seams (clock, queue, shared pawn state view, Roster legality query); the named stub doubles live in story 095.

---

## Acceptance Criteria

*ADR-derived (no numbered GDD AC). Source: ADR-0011 Decision §1 ("a plain-C++ `FPawnCommandCore` ... with no `UWorld` dependency, so ADR-0008's headless specs cover it"):*

- [ ] **GIVEN** an `FPawnCommandCore` constructed with an injected clock and a stub queue and no `UWorld` **WHEN** `InteractBegin` is called, the clock advances 0.5 s, and `InteractEnd` is called **THEN** the stub queue has recorded exactly two items (Begin then End) with the given payload, every time read went through the injected clock, and no file of the core includes a `UWorld`, `AActor`, `UActorComponent` or engine-time header.

---

## Implementation Notes

*Derived from ADR-0011 Decision §1, §3, §4, §5 and Implementation Guidelines:*

`FPawnCommandCore` "holds one slot state per pawn: `Idle | Interacting(Target, Start) | Bracing(Start) | Striking | Countering | HideTransition(Enter|Exit)`" (§4) and the legality table, four token buckets, one pending End and hold timer handles (Performance Implications: "tens of bytes" per pawn). This story creates the class, its construction seam and the recording-stub test only; the slot states exist as an enum so later stories fill the transitions. `UPawnCommandComponent` is a thin non-replicated `UActorComponent` created in the pawn's constructor (never added per kind) that owns one `FPawnCommandCore` and calls through; the pawn header forward-declares it and never includes its header (ADR-0007 lint rule (a), architecture-review fix 2026-10-25).

Why headless: the ADR makes the core the unit-testable seam (coding standard: "all public methods must be unit-testable, dependency injection over singletons"). The injected clock is a `double Now()` interface standing in for `ServerNow()`; "no client timestamp is accepted and nothing is back-dated" (§5), so the core must never read `FPlatformTime`, `FApp::GetCurrentTime` or a timer manager. The injected queue mirrors ADR-0002 `UResolutionQueue::Submit(Mailbox, FQueueItem{PawnCommand, FCommandPayload})` and `ScheduleAt(DueTime, HoldTimer{...})`. The queue and mailbox types themselves are cross-epic (round-match-framework, ADR-0002 §2a payload types in Foundation `Public/Server`); declare only the minimal interface the core needs.

QL-STORY-READY gaps to resolve in the implementation (author to state, not invent): clock moving backwards (undefined in the spec: state fault versus clamp); a null queue is a construction error, not a crash. "Runs headless" is unobservable by running (automation runs inside an editor with a world), so the include-scan lint over `Public/Server/Core` and `Private/Core/PawnCommandCore*` is part of the test, with a seeded negative fixture that must fail it.

Prototype: new code, no prototype counterpart (the prototype's logic sits in `StealthRules.cpp` and the character's RPC handlers, which are replaced under ADR-0011 Migration Plan; the pure-logic test style of `StealthRulesTests.cpp`, names `StealthGame.Rules.*`, is the template for this test). Tuning keys: none consumed yet (buckets and debounce keys arrive in later stories).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-001-command-reply-types.md`: the value types this class uses.
- `story-003-action-hub-and-predicate-registry.md`: the action hub and predicate registry the core calls at its slots.
- `story-004-reply-sink-and-drain-end-flush.md`: the reply buffer and drain-end flush.
- `story-005-entry-point-set.md`: the reflected entry-point set of `UPawnCommandComponent`.
- `story-020-ingress-clamp-and-drop.md`: stage-1 ingress (this core covers stage 2 only, and only its skeleton).
- `story-095-command-test-seams.md`: the named stub doubles (Round queue, Roster legality, state view, camera, predicates) used across the IIF tests.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the
planned ones; `/dev-story` replaces the list with the files it changed; `/story-done`
checks scope against it.*

- `Source/StealthGameplay/Public/Server/Core/PawnCommandCore.h`
- `Source/StealthGameplay/Private/Core/PawnCommandCore.cpp`
- `Source/StealthGameplay/Public/Server/Core/StealthPawnCommandComponent.h` (thin component shell; behaviour added by later stories)
- `Source/StealthGameplay/Private/Tests/Input/PawnCommandCoreHeadlessTest.cpp` (class `FInputPawnCommandCoreHeadlessTest`, name `StealthGame.Input.PawnCommandCoreHeadless`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS
- "Runs headless" is unobservable by running (automation runs in an editor with a world); the injected-clock and stub-queue seam types are unnamed and the skeleton's minimal behaviour is unstated.
- QA-lead proposed rewrite (unconfirmed; the GDD AC stays the authority): "FPawnCommandCore is constructed with an injected clock interface (double Now()) and a queue interface mirroring ADR-0002 Submit/ScheduleAt. InteractBegin then InteractEnd submit exactly two items to a recording stub, in order. No file under Public/Server/Core or Private/Core/PawnCommandCore* includes a UWorld/AActor/UActorComponent/Engine-time header."
- Scenario sketch from the qa file (`PawnCommandCoreHeadless`, derived): core built with stub clock (Now() = 100.0) and recording queue; InteractBegin(Fixture1), advance 0.5, InteractEnd; queue records exactly 2 items with the given payload, spy count of clock reads > 0 and `FPlatformTime`/`FApp::GetCurrentTime` never called, advancing the clock alone changes no core state; include-scan lint 0 hits for UWorld, AActor, UActorComponent, FTimerManager, GetWorld; two cores in one test are independent.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Input/PawnCommandCoreHeadlessTest.cpp` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/input-interaction-framework/story-001-command-reply-types.md`; cross-epic: round-match-framework (stories not yet written; the real resolution queue, until then only the stub queue is used); cross-epic: tuning-data-config (stories not yet written; module skeleton)
- Unlocks: `production/epics/input-interaction-framework/story-004-reply-sink-and-drain-end-flush.md`, `production/epics/input-interaction-framework/story-005-entry-point-set.md`, `production/epics/input-interaction-framework/story-095-command-test-seams.md`
