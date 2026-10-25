# Story 056: Seamless arrival calls RebindView and fresh-login rejoin calls RegisterParticipant(Reclaimed) exactly once

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-003 = Round travel by seamless ServerTravel ?Seed= with the Roster reset ordering)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0001: Crowd identity rules (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `roster-rebind-both-paths` as one buildable behaviour: seamless arrival calls RebindView and fresh-login rejoin calls RegisterParticipant(Reclaimed) exactly once. It is traced to the GDD criterion below and to TR-session-003. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 16. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 43, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **Roster rebind**: GIVEN a seamless-travel arrival and a fresh-login rejoin, WHEN each binds, THEN the first calls `RebindView` and the second `RegisterParticipant` (returning `Reclaimed`) exactly once, both reach the same record, and the player array has the same entries before and after. `RosterRebindBothPaths` [PIE-2]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- BindConnection(PeerKey, PlayerState) is the one Session operation on every arrival path: RegisterParticipant(IdentityKey, DisplayName) on fresh login (Registered or Reclaimed), RebindView(Id, NewPlayerState) on seamless arrival, which rebuilds view fields and C-TeamView from the record without re-running login. RebindView writes only view fields and does not depend on world wiring, so the host's rebind in PostSeamlessTravel is safe before BeginPlay (E19). Raises URosterStore::OnParticipantViewRebound(FParticipantId).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype AStealthGameMode (Source/StealthGame/Private/StealthGameMode.cpp): PostLogin calls Super, which spawns the pawn through the engine start-player path, and bUseSeamlessTravel is false (line 28). Nothing in it is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- the Roster's API implementation (participant-state-roster epic)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/StealthGameMode.h`
- `Source/StealthGame/Private/StealthGameMode.cpp`
- `Source/StealthFoundation/Public/Server/Session/StealthSessionCore.h`
- `Source/StealthFoundation/Private/Session/StealthSessionCore.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Session/RosterRebindBothPathsPieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **Roster rebind**: GIVEN a seamless-travel arrival and a fresh-login rejoin, WHEN each binds, THEN the first calls `RebindView` and the second `RegisterParticipant` (returning `Reclaimed`) exactly once, both reach the same record, and the player array has the same entries before and after. `RosterRebindBothPaths` [PIE-2]
  - Given: A seamless-travel arrival and a fresh-login rejoin.
  - When: Each binds.
  - Then: The first calls RebindView and the second calls RegisterParticipant (returning Reclaimed), each exactly once. Both reach the same record, and the player array has the same entries before and after.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/RosterRebindBothPathsPieTest.cpp` — `StealthGame.Session.PIE.RosterRebindBothPaths` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.RosterRebindBothPaths`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(RosterRebindBothPaths, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-001-harness-connection-event-hook.md`, `production/epics/session-travel/story-004-spike-s4-rejoin-playerstate-adoption.md`, `production/epics/session-travel/story-014-session-service-lifecycle-and-empty-stores.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Cross-epic: Epic `participant-state-roster` (`production/epics/participant-state-roster/EPIC.md`): the registration API (RegisterParticipant, RebindView, NotifyConnectionChanged, IsRoundScopeReset, InvalidateAllBindings) is a peer epic built in the same window; use a fake Roster behind the interface for Logic tests.
- Unlocks: `production/epics/session-travel/story-054-field-scope-across-travel.md`, `production/epics/session-travel/story-057-anchor-new-city-together.md`, `production/epics/session-travel/story-060-rejoin-keeps-same-record-and-role.md`
