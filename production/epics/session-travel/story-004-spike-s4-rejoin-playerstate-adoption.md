# Story 004: Spike S4: retained PlayerState adopted on rejoin with no visible change to other clients

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-006`, `TR-session-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-006 = Liveness silence clock, rejoin window, supersede, client auto-rejoin; TR-session-007 = Session never spawns/frees pawns or PlayerStates on connection events; default login pawn disabled)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0001: Crowd identity rules; ADR-0008 (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `spike-s4-rejoin-playerstate-adoption` as one buildable behaviour: spike S4: retained PlayerState adopted on rejoin with no visible change to other clients. It is traced to the GDD criterion below and to TR-session-006, TR-session-007. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rules 15, 16, 22; Open Questions 2, 11. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From ADR-0005 Validation Criteria ('S4: a mid-round rejoin produces no PlayerState actor-channel open/close, no player-array change and no ClientReceiveLocalizedMessage on any non-owner client (raw-bunch capture), and no client ever receives an engine entered/left/renamed message in any phase; two clients on one machine with distinct -IdentityKey= never supersede each other'), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a mid-round rejoin with 3 clients, WHEN a raw-bunch capture is taken on a non-owner client, THEN the rejoin produces no PlayerState actor-channel open or close, no player-array change and no ClientReceiveLocalizedMessage, no client ever receives an engine entered, left or renamed message in any phase, and two clients on one machine with distinct -IdentityKey= values never supersede each other; the report states whether PlayerState adoption in InitPlayerState works or the documented fallback (spawn then destroy in the same frame) is needed.

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- ADR-0005 section 5: Login stashes the retained PlayerState as a pending reclaim before Super spawns the controller; AStealthPlayerController::InitPlayerState adopts it (SetPlayerState, SetOwner(this), OnReactivated). Fallback if adoption fails: spawn a fresh PlayerState and destroy it in the same frame (see ADR-0005 section 5 for the exact wording).
- Raw-bunch capture mechanism is ADR-0008's; the S1/S2/S14 report (production/qa/spikes/2026-10-25_S1-S2-S14.md) records that rejoin was not testable in the prototype, which is why this spike exists.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Reference only: Source/StealthGame/Private/Spikes/StealthIdentitySpike.* (S1/S2/S14 harness, cvar stealth.Spike.IdentityTrims) and production/qa/spikes/2026-10-25_S1-S2-S14.md. Do not promote; reuse the probe approach.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024 (`story-024-no-default-pawn-spawn-on-login.md`): no default pawn spawn
- Story 025 (`story-025-engine-welcome-messages-suppressed.md`): welcome broadcast suppression
- Story 060 (`story-060-rejoin-keeps-same-record-and-role.md`): rejoin same record

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/spikes/<date>_S4-rejoin-adoption.md`
- `Source/StealthTestHarness/Private/Tests/MP/Session/RejoinAdoptionMpTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a mid-round rejoin with 3 clients, WHEN a raw-bunch capture is taken on a non-owner client, THEN the rejoin produces no PlayerState actor-channel open or close, no player-array change and no ClientReceiveLocalizedMessage, no client ever receives an engine entered, left or renamed message in any phase, and two clients on one machine with distinct -IdentityKey= values never supersede each other; the report states whether PlayerState adoption in InitPlayerState works or the documented fallback (spawn then destroy in the same frame) is needed.
  - Given: A mid-round rejoin with 3 clients; two clients on one machine use distinct -IdentityKey= values.
  - When: A raw-bunch capture is taken on a non-owner client.
  - Then: No PlayerState actor-channel open or close, no player-array change and no ClientReceiveLocalizedMessage. No client receives an engine entered, left or renamed message in any phase, and the two clients never supersede each other. The report states whether PlayerState adoption in InitPlayerState works or the fallback (spawn then destroy in the same frame) is needed.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/MP/Session/RejoinAdoptionMpTest.cpp` — `StealthGame.Session.MP.RejoinAdoption` (multi-process Gauntlet node), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.MP.RejoinAdoption`, T3 multi-process node named `StealthGame.Session.MP.RejoinAdoption` (Gauntlet); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-001-harness-connection-event-hook.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-024-no-default-pawn-spawn-on-login.md`, `production/epics/session-travel/story-056-roster-rebind-both-paths.md`, `production/epics/session-travel/story-060-rejoin-keeps-same-record-and-role.md`, `production/epics/session-travel/story-072-pawn-shape-stable-across-rejoin.md`, `production/epics/session-travel/story-077-silent-drop-and-rejoin-presentation.md`
