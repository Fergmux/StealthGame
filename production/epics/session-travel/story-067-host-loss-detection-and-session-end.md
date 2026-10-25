# Story 067: Hosts quitting or crashing end the session for every client with no host migration

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-009`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-009 = No host migration; session-ending notice; client host-loss detection)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `host-loss-detection-and-session-end` as one buildable behaviour: hosts quitting or crashing end the session for every client with no host migration. It is traced to the GDD criterion below and to TR-session-009. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 4; Formulas F6. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 39, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **F6** GIVEN 3 clients at 100 ms RTT on one machine (timestamps from the shared platform clock), WHEN the host quits, THEN every client shows "The session has ended." within 0.55 s plus one frame; WHEN the host process is killed, THEN within 12.3 s; AND GIVEN a client loading a map for 20 s with no server traffic, THEN it does not declare the session ended. No client becomes host or stays in the transition map. This covers anchor 3, second part. `HostLossF6` [MP] [SPIKE]; sign-off [10P]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- This story is ADR-0005's spike S5. Deliberate quit: ClientSessionEnding(Reason) owner-only reliable RPC on every controller, wait up to ShutdownFlushSec (0.5) or until every client acknowledges, then EndMatch; clients show 'The session has ended.' within 0.55 s plus one frame. Crash: each client's host-loss clock (the same clamped accumulator, paused from travel start until FRoundParams or the lobby ack request arrives) fires within 12.3 s.
- A client loading a map for 20 s with no server traffic must not declare the session ended. No client becomes host or stays in the transition map. Sign-off needs [10P].

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 090 (`story-090-menu-session-end-ok-returns.md`): the OK control and menu return

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Session/StealthSessionService.h`
- `Source/StealthFoundation/Private/Session/StealthSessionService.cpp`
- `Source/StealthGame/Public/StealthPlayerController.h`
- `Source/StealthGame/Private/StealthPlayerController.cpp`
- `Source/StealthFoundation/Public/View/Session/StealthIdentitySubsystem.h`
- `Source/StealthFoundation/Private/Session/StealthIdentitySubsystem.cpp`
- `Source/StealthTestHarness/Private/Tests/MP/Session/HostLossF6MpTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **F6** GIVEN 3 clients at 100 ms RTT on one machine (timestamps from the shared platform clock), WHEN the host quits, THEN every client shows "The session has ended." within 0.55 s plus one frame; WHEN the host process is killed, THEN within 12.3 s; AND GIVEN a client loading a map for 20 s with no server traffic, THEN it does not declare the session ended. No client becomes host or stays in the transition map. This covers anchor 3, second part. `HostLossF6` [MP] [SPIKE]; sign-off [10P]
  - Given: 3 clients at 100 ms RTT on one machine, timestamped from the shared platform clock.
  - When: The host quits; separately the host process is killed; separately a client loads a map for 20 s with no server traffic.
  - Then: After a quit, every client shows 'The session has ended.' within 0.55 s plus one frame. After a kill, within 12.3 s. The loading client does not declare the session ended. No client becomes host or stays in the transition map.
  - Edge cases: Sign-off needs [10P]; full-capacity behaviour is untested until then.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/MP/Session/HostLossF6MpTest.cpp` — `StealthGame.Session.MP.HostLossF6` (multi-process Gauntlet node), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.MP.HostLossF6`, T3 multi-process node named `StealthGame.Session.MP.HostLossF6` (Gauntlet); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-002-net-emulation-measurement-spike.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-090-menu-session-end-ok-returns.md`
