# Story 003: Tests inject fake Session, Roster, TDC, Character Pool, Level Socket and Crowd gateways

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-001` (five-phase machine in a game-instance store surviving travel; nearest TR, approximate: the seam is the dependency-injection boundary of every Round rule, and TR-round-001 is the Round phase machine that consumes these gateways). GDD AC: Test seams (binding on implementation), item 3. Rules: R1, R2 (R1 lists what Round owns and the systems it calls).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0007: Code module layout, composition root and include boundaries
**ADR Decision Summary**: Four runtime modules (StealthFoundation, StealthGameplay, StealthPresentation, StealthGame) plus Editor and StealthTestHarness; `Public/View/` vs `Public/Server/` with a compile guard; consumer-owned interfaces wired by the composition root; plain-C++ cores behind thin facades; logging policy. This story applies the consumer-owned-interface and registration rules: the consumer declares the interface, the composition root registers the implementation (weakly, never from `StartupModule`), and plain cores take explicitly registered pointers so headless tests inject fakes without UObjects.
**ADR Version**: ADR-0007 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Touches `TWeakInterfacePtr` only if a gateway is declared as a `UINTERFACE` (verified fact: `TWeakInterfacePtr<T>` requires a UHT `UINTERFACE`, ADR-0002 E7 / ADR-0007 verified facts). The plain-core route (raw pointers registered by the facade) avoids it. No post-cutoff API is called. `UE_LOGF` (5.8) is the only post-cutoff item in ADR-0007 and the fakes do not log. Verification items of ADR-0007 (PrivateDefinitions guard, `TargetConfigurationDenyList` for the harness, include lint) are not touched.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25f)**:
- Required: Queue, Roster, tuning validator, seed mixer and appearance deck are plain C++ cores (`F...Core`) with no `UWorld` dependency, behind a subsystem facade; interface implementations are registered only by the composition root, weakly, and cleared in `Deinitialize`/`EndPlay` (ADR-0007)
- Forbidden: Never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only) (ADR-0008)
- Guardrail: Tests: per-commit pipeline at most 10 min (ADR-0008); the fakes are in-memory and allocation-light

**Why this story**: Round decides, and six other systems act: Session (travel, admission, connection facts), Roster (teams, states, transitions), Tuning/TDC (the frozen snapshot), Character Pool (appearance allocation), Level Socket (placement sockets) and Crowd (`ICrowdControl`). Every Core test (stories 018 onward) needs to drive Round against fakes and read back what Round called, in order. This story builds the six recording fakes and the shared call-order log once. It is the largest seam story (M) because six interfaces are involved. Exit A seam story; the Core stories consume it.

**Prototype disposition**: none (new Foundation code). The prototype calls its collaborators directly from `StealthGameMode.cpp`; there is nothing to promote.

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md`, "Test seams (binding on implementation)" item 3, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a [U] test, THEN fake Session, Roster, TDC, Character Pool, Level Socket and Crowd gateways are supplied by dependency injection (no singleton access) and record their calls in order. `StealthGame.Round.FakeGatewaysSeam` [U]

---

## Implementation Notes

*Derived from ADR-0007 Implementation Guidelines and Key Interfaces (consumer-owned interfaces, registration shape):*

- Each gateway is an interface owned by its consumer (Round, in StealthGameplay) with the signature set owned by the producing system's ADR/GDD. The six fakes implement those interfaces and are passed by constructor or `Register<I>` injection; no fake is found through `GetSubsystem`, a global or a static. The system under test must construct and run with no singleton lookup (the AC's "no singleton access").
- One shared call-order log (a single append-only list of `{GatewayId, CallName, arguments summary}`) is owned by the test and handed to all six fakes, so interleaved calls across gateways are ordered by one sequence. Each fake also keeps its own recorded list.
- A call to a fake that has no scripted behaviour fails the test (strict fakes), so a silent unexpected call cannot pass.
- The fakes record only what the call carried (ids, enums, counts); they never store or print a server secret, sub-seed, mailbox order or Event Log content (ADR-0007 logging policy; control manifest Forbidden).
- Open: confirm against ADR-0007 Key Interfaces (consumer-owned interfaces list names `IQueueResolver`, `IStagedWriteApplier`, `ITerminalDecision`, `IEventFlushBarrier`, `ICrowdControl`, `ILayoutProvider`, `ITeamPingReset` but no `IRoundSessionGateway`, `IRoundRosterGateway` and so on) the exact interface names and method sets of the Session, Roster, TDC, Character Pool and Level Socket gateways, and which of them Round declares itself. This story's layer is Foundation but the gateway interfaces are Core types (StealthGameplay); Foundation cannot include StealthGameplay (ADR-0007 include rules). The location of the fake gateway headers therefore needs a decision (for example a test-only header reachable by both modules). Do not guess: resolve at `/dev-story` start, and where a collaborator's interface does not exist yet, declare the minimal Round-side gateway for the calls Round's GDD rules make (for example R5 steps for Roster and Session) and record it.
- Where a producing epic already declares the interface (participant-state-roster, session-travel, character-pool-appearance, level-socket-contract, crowd-npc-behaviour, tuning-data-config), reuse that declaration; the fake is then a recording implementation of it.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Stories 018 (`production/epics/round-match-framework/story-018-team-assignment.md`), 022 (`story-022-start-validity.md`), 024 (`story-024-start-ordering.md`), 031 (`story-031-allocate-once.md`), 038 (`story-038-placement-invariant.md`), 040, 060, 067 and 075: the Core rules that call these gateways and assert the recorded sequences.
- The real implementations of the six systems; they belong to their own epics. Only recording fakes are built here.
- Story 001 (fake clock), story 002 (fixed secret), story 004 (stub resolvers), story 005 (fixture constants): the other seams.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Fakes/StealthFakeGateways.h` (new; six recording fakes and the shared call-order log; placement subject to the Open item above)
- `Source/StealthFoundation/Private/Tests/Fakes/FakeGatewaysSeamTest.cpp` (new; class `FRoundFakeGatewaysSeamTest`)
- Tests live under the engine test roots (`tests/README.md`), never under `tests/`. `/dev-story` replaces this planned list with the real one. `Source/StealthFoundation` does not exist yet (tuning-data-config epic creates the skeleton); write under `Source/StealthGame/` until it lands.

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE). The developer implements against these — do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a [U] test, THEN fake Session, Roster, TDC, Character Pool, Level Socket and Crowd gateways are supplied by dependency injection (no singleton access) and record their calls in order.
  - Given: the six fake gateways passed by constructor/DI, with no singleton access.
  - When: a stub scenario calls each gateway in a known sequence.
  - Then: each fake's recorded call list equals the call sequence in the order made.
  - Then: the system under test constructs and runs with no global/singleton lookup.
  - Edge cases: interleaved calls across gateways are ordered by one shared call-order log; a call with no recorded fake fails the test.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Fakes/FakeGatewaysSeamTest.cpp` — automation test `StealthGame.Round.FakeGatewaysSeam` must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later). Class `FRoundFakeGatewaysSeamTest`, T1 per ADR-0008 and `.claude/rules/test-standards.md`; wrap in `#if WITH_DEV_AUTOMATION_TESTS`.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (within this epic)
- Cross-epic (interface contracts only, the real implementations are not needed to build the fakes): `production/epics/participant-state-roster`, `production/epics/session-travel`, `production/epics/character-pool-appearance`, `production/epics/level-socket-contract`, `production/epics/crowd-npc-behaviour` (`ICrowdControl`; mutual peer), `production/epics/tuning-data-config`
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; stories not yet created)
- Unlocks: `production/epics/round-match-framework/story-018-team-assignment.md`, `production/epics/round-match-framework/story-022-start-validity.md`, `production/epics/round-match-framework/story-024-start-ordering.md`, `production/epics/round-match-framework/story-031-allocate-once.md`, `production/epics/round-match-framework/story-038-placement-invariant.md`, `production/epics/round-match-framework/story-040-disconnected-at-start-spawned.md`, `production/epics/round-match-framework/story-060-grace-timeline.md`, `production/epics/round-match-framework/story-067-bound-ai-move-look-only.md`, `production/epics/round-match-framework/story-075-active-pawn-lost-recovery.md`
