# Story 024: Disconnect and rejoin spawn no pawn and leave the player array unchanged

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-007 = Session never spawns/frees pawns or PlayerStates on connection events; default login pawn disabled)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `no-default-pawn-spawn-on-login` as one buildable behaviour: disconnect and rejoin spawn no pawn and leave the player array unchanged. It is traced to the GDD criterion below and to TR-session-007. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 22. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 55, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **Rule 22, no pawn at login**: GIVEN a round in Active with 2 clients, WHEN one disconnects and rejoins, THEN the server's pawn count and the player array are identical before, during and after, and no pawn is spawned by login. `NoDefaultPawnSpawn` [PIE-2]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- HandleStartingNewPlayer is a no-op: it runs after PostLogin and at the end of HandleSeamlessTravelPlayer (E4) and would call RestartPlayer. Pawns come only from Round's mixed spawn through UStealthPawnFactory::SpawnSharedPawn (ADR-0010).
- AStealthPlayerState::OnDeactivated is phase-conditional (ADR-0005 section 1): while admission is RejoinOnly it does not destroy the PlayerState, so the player array is unchanged; in Open or Closed it destroys as normal. Never toggle bIsInactive or remove a PlayerState between Preparing and the next travel.
- Needs the harness hook (story 001) and the adoption result of spike S4 (story 004).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype AStealthGameMode (Source/StealthGame/Private/StealthGameMode.cpp): PostLogin calls Super, which spawns the pawn through the engine start-player path, and bUseSeamlessTravel is false (line 28). Nothing in it is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 025 (`story-025-engine-welcome-messages-suppressed.md`): welcome broadcast suppression
- Story 060 (`story-060-rejoin-keeps-same-record-and-role.md`): same record on rejoin

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/StealthGameMode.h`
- `Source/StealthGame/Private/StealthGameMode.cpp`
- `Source/StealthGame/Public/StealthPlayerController.h`
- `Source/StealthGame/Private/StealthPlayerController.cpp`
- `Source/StealthGame/Public/StealthPlayerState.h`
- `Source/StealthGame/Private/StealthPlayerState.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Session/NoDefaultPawnSpawnPieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **Rule 22, no pawn at login**: GIVEN a round in Active with 2 clients, WHEN one disconnects and rejoins, THEN the server's pawn count and the player array are identical before, during and after, and no pawn is spawned by login. `NoDefaultPawnSpawn` [PIE-2]
  - Given: A round in Active with 2 clients.
  - When: One disconnects and rejoins.
  - Then: The server's pawn count and the player array are identical before, during and after, and no pawn is spawned by login.
  - Edge cases: The PlayerState is not destroyed while admission is RejoinOnly (rule 22).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/NoDefaultPawnSpawnPieTest.cpp` — `StealthGame.Session.PIE.NoDefaultPawnSpawn` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.NoDefaultPawnSpawn`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(NoDefaultPawnSpawn, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-023-game-mode-skeleton-agamemodebase.md`, `production/epics/session-travel/story-001-harness-connection-event-hook.md`, `production/epics/session-travel/story-004-spike-s4-rejoin-playerstate-adoption.md`
- Cross-epic: Epic `round-match-framework` (`production/epics/round-match-framework/EPIC.md`) consumes Session's events and pushes the admission mode; use a fake Round behind the consumer-owned interfaces here.
- Unlocks: `production/epics/session-travel/story-077-silent-drop-and-rejoin-presentation.md`
