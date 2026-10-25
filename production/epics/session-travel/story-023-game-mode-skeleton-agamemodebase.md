# Story 023: AStealthGameMode derives AGameModeBase with seamless travel and Session hook overrides

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-007`, `TR-session-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-007 = Session never spawns/frees pawns or PlayerStates on connection events; default login pawn disabled; TR-session-001 = Listen-server direct join; ordered admission gates at PreLogin; capacity 2 x TeamSize)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0001: Crowd identity rules (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `game-mode-skeleton-agamemodebase` as one buildable behaviour: aStealthGameMode derives AGameModeBase with seamless travel and Session hook overrides. It is traced to the GDD criterion below and to TR-session-007, TR-session-001. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rules 7, 22; Detailed Design G (Implementation notes). Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From ADR-0005 Decision section 1 (hook override table) and control manifest Required/Forbidden (Session and travel); no GDD criterion names the skeleton, TR-session-007 is the traced requirement, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN the game mode, session class and PlayerState classes, WHEN reflected and statically checked, THEN AStealthGameMode derives AGameModeBase (never AGameMode), bUseSeamlessTravel is true and bStartPlayersAsSpectators is false, AStealthGameSession::ApproveLogin always returns success, HandleStartingNewPlayer_Implementation is a no-op, PlayerCanRestart_Implementation returns false, ShouldBroadCastWelcomeMessage returns false in every phase, and DispatchPostLogin is not overridden (login overrides use PostLogin or OnPostLogin).

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- The prototype class already derives AGameModeBase; keep it, set bUseSeamlessTravel true, and replace the prototype's PostLogin Super-spawn path with the table in ADR-0005 section 1: PreLogin (remote only, no Super), Login (stash reclaim before Super), InitNewPlayer without Super (parse only IdentityKey, Name, RejoinToken), InitGame (parse ?Seed ?Preset ?Round, no GameState write), InitGameState (write FRoundParams), PostLogin (BindConnection).
- Every game mode stays on the legacy replication system because seamless travel ensure()s when source and destination differ (E22, ADR-0001 section 7).
- Behaviour of each hook is proven by its own story (admission 028-038, travel 039-057); this story fixes the class shape.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype AStealthGameMode (Source/StealthGame/Private/StealthGameMode.cpp): PostLogin calls Super, which spawns the pawn through the engine start-player path, and bUseSeamlessTravel is false (line 28). Nothing in it is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024 (`story-024-no-default-pawn-spawn-on-login.md`): no default pawn at login
- 028-037: gate logic
- Story 040 (`story-040-seed-url-parse-rejects-bad-params.md`): ?Seed parsing

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/StealthGameMode.h`
- `Source/StealthGame/Private/StealthGameMode.cpp`
- `Source/StealthGame/Public/StealthPlayerState.h`
- `Source/StealthGame/Private/StealthPlayerState.cpp`
- `Source/StealthGame/Public/StealthGameSession.h`
- `Source/StealthGame/Private/StealthGameSession.cpp`
- `Source/StealthFoundation/Private/Tests/Session/GameModeSkeletonTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN the game mode, session class and PlayerState classes, WHEN reflected and statically checked, THEN AStealthGameMode derives AGameModeBase (never AGameMode), bUseSeamlessTravel is true and bStartPlayersAsSpectators is false, AStealthGameSession::ApproveLogin always returns success, HandleStartingNewPlayer_Implementation is a no-op, PlayerCanRestart_Implementation returns false, ShouldBroadCastWelcomeMessage returns false in every phase, and DispatchPostLogin is not overridden (login overrides use PostLogin or OnPostLogin).
  - Given: The game mode, session class and PlayerState classes.
  - When: They are reflected and statically checked.
  - Then: AStealthGameMode derives AGameModeBase (never AGameMode). bUseSeamlessTravel is true and bStartPlayersAsSpectators is false. AStealthGameSession::ApproveLogin always succeeds. HandleStartingNewPlayer_Implementation is a no-op, PlayerCanRestart_Implementation returns false, and ShouldBroadCastWelcomeMessage returns false in every phase. DispatchPostLogin is not overridden.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/GameModeSkeletonTest.cpp` — automation test `StealthGame.Session.GameModeSkeleton` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.GameModeSkeleton`, T1 unit test, class `FSessionGameModeSkeletonTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-014-session-service-lifecycle-and-empty-stores.md`
- Unlocks: `production/epics/session-travel/story-024-no-default-pawn-spawn-on-login.md`, `production/epics/session-travel/story-025-engine-welcome-messages-suppressed.md`, `production/epics/session-travel/story-028-admission-gate-order.md`, `production/epics/session-travel/story-031-join-options-stripped.md`, `production/epics/session-travel/story-040-seed-url-parse-rejects-bad-params.md`, `production/epics/session-travel/story-041-round-params-replicated-atomically.md`
