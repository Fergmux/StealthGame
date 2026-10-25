# Story 026: IdentityKey is per install, overridable and never replicated or logged beyond 4 bytes

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-002 = Persistent 128-bit IdentityKey per install with launch/PIE override)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0001: Crowd identity rules (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `identity-key-persistent-and-private` as one buildable behaviour: identityKey is per install, overridable and never replicated or logged beyond 4 bytes. It is traced to the GDD criterion below and to TR-session-002. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 6. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD AC 58, first part (the RejoinToken part is story 027), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN 3 PIE instances, THEN each has a distinct IdentityKey, AND no replicated property or log line contains more than a 4-byte prefix of any key (`IdentityKeyPrivate` [PIE-2] + [S]).

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- UStealthIdentitySubsystem (game instance) loads or creates a 128-bit key in a local file under Saved/ on first launch, honours -IdentityKey=, and derives a per-instance key in PIE from the PIE instance index; it is sent only as a login option (ADR-0005 section 3). The key is never derived from or exposed as the engine PlayerId.
- Persist the cached host address with the key when the client joins so both survive a crash (GDD rule 23).

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 027 (`story-027-rejoin-token-gate.md`): RejoinToken
- Story 065 (`story-065-client-auto-rejoin-after-crash.md`): auto-rejoin that reads the cached address

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/Session/StealthIdentitySubsystem.h`
- `Source/StealthFoundation/Private/Session/StealthIdentitySubsystem.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Session/IdentityKeyPrivatePieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS — The title says per install and overridable, but the criterion tests only distinct keys across 3 PIE instances and the 4-byte limit; it has no check of persistence across relaunch or of the -IdentityKey= override (rule 6). (accepted and written as is in the unattended run; revise the criterion or run `/qa-plan` before `/dev-story`).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/IdentityKeyPrivatePieTest.cpp` — `StealthGame.Session.PIE.IdentityKeyPrivate` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.IdentityKeyPrivate`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(IdentityKeyPrivate, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-014-session-service-lifecycle-and-empty-stores.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: `production/epics/session-travel/story-016-host-record-seeded-without-handshake.md`, `production/epics/session-travel/story-027-rejoin-token-gate.md`, `production/epics/session-travel/story-065-client-auto-rejoin-after-crash.md`, `production/epics/session-travel/story-087-menu-join-navigation.md`, `production/epics/session-travel/story-091-menu-remembered-address.md`, `production/epics/session-travel/story-092-menu-busy-retry-status.md`
