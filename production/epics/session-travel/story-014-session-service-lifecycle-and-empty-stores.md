# Story 014: Session lifecycle: OpenSession seeds the host, EndSession clears every store on every exit path

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-001`, `TR-session-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-001 = Listen-server direct join; ordered admission gates at PreLogin; capacity 2 x TeamSize; TR-session-009 = No host migration; session-ending notice; client host-loss detection)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0007 (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `session-service-lifecycle-and-empty-stores` as one buildable behaviour: session lifecycle: OpenSession seeds the host, EndSession clears every store on every exit path. It is traced to the GDD criterion below and to TR-session-001, TR-session-009. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rules 1, 16. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From ADR-0005 Validation Criteria ('A two-sessions-in-one-process test shows empty stores at the second OpenSession') and Decision section 2 (Lifetime); GDD rules 1 and 16, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN two sessions opened one after the other in one process, WHEN the second OpenSession runs, THEN the Session, Roster, Round and Tuning stores are empty at its start, and EndMatch has run on every exit path (host quit, host return to the main menu, OnSessionEnding, UGameInstance::Shutdown and a last networked world cleanup) so a former host keeps nothing.

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- USessionService is a UGameInstanceSubsystem facade over the plain-C++ FSessionCore (no UWorld dependency, ADR-0007 section 5); four stores are the only authoritative match state (Session, Roster, Round, Tuning preset). Mutating APIs assert authority and the game thread; on a client process the stores exist but stay empty.
- OpenSession draws ServerSecret and MatchNonce (story 009), validates session-latched tuning (ADR-0004), calls Roster.BeginMatch(TeamSize) and checks engine timeouts (story 021). Stores hold TWeakObjectPtr to world objects, never strong pointers across travel.
- Roster, Round and Tuning stores belong to their own epics; this story owns Session's half and the EndMatch fan-out contract.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype AStealthGameMode (Source/StealthGame/Private/StealthGameMode.cpp): PostLogin calls Super, which spawns the pawn through the engine start-player path, and bUseSeamlessTravel is false (line 28). Nothing in it is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 016 (`story-016-host-record-seeded-without-handshake.md`): the host record seeded by OpenSession
- Story 021 (`story-021-engine-timeout-config-validated.md`): engine timeout validation
- the Roster/Round/Tuning stores' internals

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Session/StealthSessionCore.h`
- `Source/StealthFoundation/Private/Session/StealthSessionCore.cpp`
- `Source/StealthFoundation/Public/Server/Session/StealthSessionService.h`
- `Source/StealthFoundation/Private/Session/StealthSessionService.cpp`
- `Source/StealthFoundation/Private/Tests/Session/StoresEmptyAtSecondOpenTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN two sessions opened one after the other in one process, WHEN the second OpenSession runs, THEN the Session, Roster, Round and Tuning stores are empty at its start, and EndMatch has run on every exit path (host quit, host return to the main menu, OnSessionEnding, UGameInstance::Shutdown and a last networked world cleanup) so a former host keeps nothing.
  - Given: Two sessions opened one after the other in one process.
  - When: The second OpenSession runs.
  - Then: The Session, Roster, Round and Tuning stores are empty at its start. EndMatch has run on every exit path (host quit, host return to the main menu, OnSessionEnding, UGameInstance::Shutdown, last networked world cleanup), so a former host keeps nothing.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthFoundation/Private/Tests/Session/StoresEmptyAtSecondOpenTest.cpp` — `StealthGame.Session.StoresEmptyAtSecondOpen` (automation test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.StoresEmptyAtSecondOpen`, T1 unit test, class `FSessionStoresEmptyAtSecondOpenTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: None within this epic
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Cross-epic: Epic `participant-state-roster` (`production/epics/participant-state-roster/EPIC.md`): the registration API (RegisterParticipant, RebindView, NotifyConnectionChanged, IsRoundScopeReset, InvalidateAllBindings) is a peer epic built in the same window; use a fake Roster behind the interface for Logic tests.
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): knob registry (sub-struct `Session`), `TuningSchemaVersion` and `ClientHash`.
- Unlocks: `production/epics/session-travel/story-009-os-entropy-and-session-secret.md`, `production/epics/session-travel/story-015-connection-state-machine-illegal-transitions.md`, `production/epics/session-travel/story-016-host-record-seeded-without-handshake.md`, `production/epics/session-travel/story-021-engine-timeout-config-validated.md`, `production/epics/session-travel/story-023-game-mode-skeleton-agamemodebase.md`, `production/epics/session-travel/story-026-identity-key-persistent-and-private.md`, `production/epics/session-travel/story-032-display-name-sanitise-and-dedupe.md`, `production/epics/session-travel/story-056-roster-rebind-both-paths.md`, `production/epics/session-travel/story-086-menu-host-navigation.md`
