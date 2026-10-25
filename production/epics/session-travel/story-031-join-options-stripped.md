# Story 031: Client-supplied Team, SpectatorOnly and Seed options are never read

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-001`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-001 = Listen-server direct join; ordered admission gates at PreLogin; capacity 2 x TeamSize)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `join-options-stripped` as one buildable behaviour: client-supplied Team, SpectatorOnly and Seed options are never read. It is traced to the GDD criterion below and to TR-session-001. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 7. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 6, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN join options `Team=`, `SpectatorOnly=` and `Seed=`, WHEN the client is admitted, THEN no system reads them, and the participant is neither a spectator nor on a client-chosen team. `StripsUnknownOptions` [U] + [PIE-2]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- The engine cannot strip join options before login (PreLogin's Options is const), so the new-player initialisation never calls Super and parses only IdentityKey, Name and RejoinToken (E17); SpectatorOnly, SplitscreenCount and every other key are ignored. The [U] half is the option parser, the [PIE-2] half joins a client with those options.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype AStealthGameMode (Source/StealthGame/Private/StealthGameMode.cpp): PostLogin calls Super, which spawns the pawn through the engine start-player path, and bUseSeamlessTravel is false (line 28). Nothing in it is promoted.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 032 (`story-032-display-name-sanitise-and-dedupe.md`): display name sanitising

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/StealthGameMode.h`
- `Source/StealthGame/Private/StealthGameMode.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Session/StripsUnknownOptionsPieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS — 'No system reads them' is not observable as written; restate as: the option parser output holds only IdentityKey, Name and RejoinToken (rule 7), and team and spectator state are not client-chosen. (accepted and written as is in the unattended run; revise the criterion or run `/qa-plan` before `/dev-story`).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/StripsUnknownOptionsPieTest.cpp` — `StealthGame.Session.PIE.StripsUnknownOptions` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.StripsUnknownOptions`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(StripsUnknownOptions, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-023-game-mode-skeleton-agamemodebase.md`, `production/epics/session-travel/story-001-harness-connection-event-hook.md`
- Unlocks: None
