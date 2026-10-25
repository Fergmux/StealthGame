# Story 001: PIE harness hook forces client disconnect, rejoin, supersede and host kick

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-006 = Liveness silence clock, rejoin window, supersede, client auto-rejoin)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0008 (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `harness-connection-event-hook` as one buildable behaviour: pIE harness hook forces client disconnect, rejoin, supersede and host kick. It is traced to the GDD criterion below and to TR-session-006. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Acceptance Criteria preamble (test tags); Open Questions 2; Core Rules rule 15, 21. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From Epic overview (production/epics/session-travel/EPIC.md): 'the PIE disconnect/rejoin/supersede/kick test hook that the reconnect-indicator criteria (AC-HUD-28, AC-PSR-53) need (rebaseline: build the harness hook here and time-box a spike)'; GDD Open Question 2 and AC 17, 20, 28, 30, 31, 55, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a PIE listen server with 2 or more clients, WHEN a test calls the harness hook, THEN it can (a) drop a chosen client's connection, (b) make the same IdentityKey reconnect as a rejoin, (c) make a second connection with the same key supersede a live one, and (d) have the host kick a chosen participant, each deterministically and without editing the game code under test. Time-box: one spike, reported in production/qa/spikes/.

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- Lives in StealthTestHarness (non-Shipping only, ADR-0007 TargetConfigurationDenyList). It must act through the real net driver (close the UNetConnection, open a fresh join with -IdentityKey), never by editing Session state.
- Two clients on one dev machine need distinct IdentityKeys or they supersede each other (GDD rule 6, ADR-0005 S4): drive keys through the IdentityKey launch/PIE override of story 026.
- Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (ADR-0005 E7); this hook covers connection events only, not travel.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Reference only: Source/StealthGame/Private/Spikes/StealthIdentitySpike.* (S1/S2/S14 harness, cvar stealth.Spike.IdentityTrims) and production/qa/spikes/2026-10-25_S1-S2-S14.md. Do not promote; reuse the probe approach.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 026 (`story-026-identity-key-persistent-and-private.md`): the IdentityKey override itself
- every [PIE-n] criterion story that uses the hook

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Public/StealthConnectionEventHook.h`
- `Source/StealthTestHarness/Private/StealthConnectionEventHook.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Session/ConnectionEventHookPieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS — 'Each deterministically' has no measurable form (no repeat count or pass rate across runs); the criterion does not say which of (a)-(d) must pass to close the spike. (accepted and written as is in the unattended run; revise the criterion or run `/qa-plan` before `/dev-story`).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/ConnectionEventHookPieTest.cpp` — `StealthGame.Session.PIE.ConnectionEventHook` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.ConnectionEventHook`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(ConnectionEventHook, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: None within this epic
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-002-net-emulation-measurement-spike.md`, `production/epics/session-travel/story-003-spike-s3-seamless-travel-persistence.md`, `production/epics/session-travel/story-004-spike-s4-rejoin-playerstate-adoption.md`, `production/epics/session-travel/story-013-secret-never-exported.md`, `production/epics/session-travel/story-024-no-default-pawn-spawn-on-login.md`, `production/epics/session-travel/story-025-engine-welcome-messages-suppressed.md`, `production/epics/session-travel/story-031-join-options-stripped.md`, `production/epics/session-travel/story-047-travel-drop-isolated.md`, `production/epics/session-travel/story-056-roster-rebind-both-paths.md`, `production/epics/session-travel/story-060-rejoin-keeps-same-record-and-role.md`, `production/epics/session-travel/story-072-pawn-shape-stable-across-rejoin.md`, `production/epics/session-travel/story-073-rejection-private-to-rejected-client.md`, `production/epics/session-travel/story-077-silent-drop-and-rejoin-presentation.md`, `production/epics/session-travel/story-078-connection-event-baseline-stationary-bound.md`, `production/epics/session-travel/story-084-lobby-join-leave-notices.md`
