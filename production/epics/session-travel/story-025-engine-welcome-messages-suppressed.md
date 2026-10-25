# Story 025: No client ever receives an engine entered, left or renamed message

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-010`, `TR-session-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-010 = Leak-surface checklist (PlayerState fields, ping, join timing) enforced via the allowlist; TR-session-007 = Session never spawns/frees pawns or PlayerStates on connection events; default login pawn disabled)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0001: Crowd identity rules (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `engine-welcome-messages-suppressed` as one buildable behaviour: no client ever receives an engine entered, left or renamed message. It is traced to the GDD criterion below and to TR-session-010, TR-session-007. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 25(c); Core Rules rule 24. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthPlayerState replicates only PublicHandle and PlayerNamePrivate; AStealthPlayerState::ShouldBroadCastWelcomeMessage returns false in every phase; a rejoiner adopts its retained PlayerState in InitPlayerState and OnDeactivated does not destroy it while admission is RejoinOnly; team-audience data travels only in a C-TeamView on the receiving teammate's own PlayerController (ADR-0001, ADR-0005)
- Forbidden: Toggling bIsInactive or removing a PlayerState from the array between Preparing and the next travel; spawning a fresh PlayerState for a reclaim; any per-instance, per-kind or on-possession change to a pawn's net config (ADR-0001, ADR-0003, ADR-0005)
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget (ADR-0001); Session silence clocks inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From ADR-0005 Validation Criteria (S4 bullet: 'no client ever receives an engine entered/left/renamed message in any phase') and GDD rule 25(c), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN 3 clients across Lobby, a round travel, an active round, a mid-round disconnect and rejoin and a Lobby leave, WHEN each non-owner client's received messages are captured, THEN no client receives an engine 'entered the game', 'left the game' or 'renamed' message in any phase, and AStealthPlayerState::ShouldBroadCastWelcomeMessage returns false always.

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- E10: APlayerState broadcasts entered, left and renamed messages to every PlayerController regardless of game mode unless ShouldBroadCastWelcomeMessage returns false. Suppression must be server-side; client-side suppression alone is bypassable (rule 25c). The lobby's join and leave lines come from the Lobby UI reading Session events (story 084), not from engine messages.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 084 (`story-084-lobby-join-leave-notices.md`): the lobby join/leave lines

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/StealthPlayerState.h`
- `Source/StealthGame/Private/StealthPlayerState.cpp`
- `Source/StealthGame/Public/StealthGameMode.h`
- `Source/StealthGame/Private/StealthGameMode.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Session/NoEngineJoinLeaveMessagesPieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN 3 clients across Lobby, a round travel, an active round, a mid-round disconnect and rejoin and a Lobby leave, WHEN each non-owner client's received messages are captured, THEN no client receives an engine 'entered the game', 'left the game' or 'renamed' message in any phase, and AStealthPlayerState::ShouldBroadCastWelcomeMessage returns false always.
  - Given: 3 clients across Lobby, a round travel, an active round, a mid-round disconnect and rejoin, and a Lobby leave.
  - When: Each non-owner client's received messages are captured.
  - Then: No client receives an engine 'entered the game', 'left the game' or 'renamed' message in any phase, and AStealthPlayerState::ShouldBroadCastWelcomeMessage returns false always.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/NoEngineJoinLeaveMessagesPieTest.cpp` — `StealthGame.Session.PIE.NoEngineJoinLeaveMessages` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.NoEngineJoinLeaveMessages`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(NoEngineJoinLeaveMessages, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-023-game-mode-skeleton-agamemodebase.md`, `production/epics/session-travel/story-001-harness-connection-event-hook.md`
- Unlocks: `production/epics/session-travel/story-077-silent-drop-and-rejoin-presentation.md`, `production/epics/session-travel/story-084-lobby-join-leave-notices.md`
