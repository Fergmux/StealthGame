# Story 064: Each Rejoining origin ends at exactly its RejoinDeadline with the listed events

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-006 = Liveness silence clock, rejoin window, supersede, client auto-rejoin)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `rejoining-origins-and-deadlines` as one buildable behaviour: each Rejoining origin ends at exactly its RejoinDeadline with the listed events. It is traced to the GDD criterion below and to TR-session-006. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 21 (origins table); States and Transitions. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 66, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **Rule 21, `Rejoining` origins and bounds** (2026-10-25 re-review): GIVEN a fake clock and a rejoin handshake admitted from each origin in the rule-21 table (`Disconnected` with 10 s of window left, `Disconnected` with 100 s left, `Expired` via `ShouldAdmitExpired` true, supersede of `Connected`, supersede of `Traveling` mid round travel, supersede of `Connected` in Lobby) whose client then never acknowledges, WHEN time advances, THEN each becomes terminal at exactly its `RejoinDeadline` (10 s; `t_admit + TravelLoadTimeoutSec`; `t_admit + TravelLoadTimeoutSec`; same; the travel's F7 `Deadline`; same as the second row) and moves to the table's failure target with exactly the listed events after the handshake began (one `OnRejoinWindowExpired`, none, none, none for the supersede of `Connected` (its `OnParticipantDisconnected(Superseded)` was raised at the supersede instant, before the handshake, with `DisconnectedAt` equal to that instant; the failure raises no second one and changes neither `DisconnectedAt` nor `ConnectionGeneration`), none for the supersede of `Traveling` (same), and `OnParticipantDisconnected(..., InLobby)` into `Dormant`), a third superseding connection never moves `t_admit` or the deadline, and a `Connecting` handshake admitted in `Open` still completes after Round pushes `RejoinOnly`. `RejoiningOriginsBounded` [U]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- Implement the rule 21 origin table as data: Disconnected with 10 s left (RejoinDeadline 10 s), Disconnected with 100 s left (t_admit + TravelLoadTimeoutSec), Expired via ShouldAdmitExpired (same), supersede of Connected (same as row 2), supersede of Traveling (the travel's F7 Deadline), supersede of Connected in Lobby. t_admit is never moved by a third superseding connection; a Connecting handshake admitted in Open still completes after Round pushes RejoinOnly.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Anything not named in the acceptance criterion above.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Session/StealthSessionCore.h`
- `Source/StealthFoundation/Private/Session/StealthSessionCore.cpp`
- `Source/StealthFoundation/Private/Tests/Session/RejoiningOriginsBoundedTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **Rule 21, `Rejoining` origins and bounds** (2026-10-25 re-review): GIVEN a fake clock and a rejoin handshake admitted from each origin in the rule-21 table (`Disconnected` with 10 s of window left, `Disconnected` with 100 s left, `Expired` via `ShouldAdmitExpired` true, supersede of `Connected`, supersede of `Traveling` mid round travel, supersede of `Connected` in Lobby) whose client then never acknowledges, WHEN time advances, THEN each becomes terminal at exactly its `RejoinDeadline` (10 s; `t_admit + TravelLoadTimeoutSec`; `t_admit + TravelLoadTimeoutSec`; same; the travel's F7 `Deadline`; same as the second row) and moves to the table's failure target with exactly the listed events after the handshake began (one `OnRejoinWindowExpired`, none, none, none for the supersede of `Connected` (its `OnParticipantDisconnected(Superseded)` was raised at the supersede instant, before the handshake, with `DisconnectedAt` equal to that instant; the failure raises no second one and changes neither `DisconnectedAt` nor `ConnectionGeneration`), none for the supersede of `Traveling` (same), and `OnParticipantDisconnected(..., InLobby)` into `Dormant`), a third superseding connection never moves `t_admit` or the deadline, and a `Connecting` handshake admitted in `Open` still completes after Round pushes `RejoinOnly`. `RejoiningOriginsBounded` [U]
  - Given: A fake clock and a rejoin handshake from each rule-21 origin: Disconnected with 10 s left; Disconnected with 100 s left; Expired via ShouldAdmitExpired true; supersede of Connected; supersede of Traveling; supersede of Connected in Lobby. Each client never acknowledges.
  - When: Time advances.
  - Then: Each is terminal at its RejoinDeadline: 10 s; t_admit + TravelLoadTimeoutSec; same; same; the travel's F7 Deadline; same as row 2. Events: one OnRejoinWindowExpired, none, none, none, none, and OnParticipantDisconnected(..., InLobby) into Dormant.
  - Edge cases: A third superseding connection never moves t_admit. A Connecting handshake admitted in Open still completes after Round pushes RejoinOnly.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/RejoiningOriginsBoundedTest.cpp` — automation test `StealthGame.Session.RejoiningOriginsBounded` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.RejoiningOriginsBounded`, T1 unit test, class `FSessionRejoiningOriginsBoundedTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-015-connection-state-machine-illegal-transitions.md`, `production/epics/session-travel/story-058-supersede-raises-disconnect-immediately.md`, `production/epics/session-travel/story-059-supersede-requires-acknowledgement.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-061-rejoin-window-not-extended.md`
