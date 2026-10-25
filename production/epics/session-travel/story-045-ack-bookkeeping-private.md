# Story 045: Acknowledgement state is keyed by ParticipantId, cleared per epoch and never replicated

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-005 = Single round acknowledgement (seed, layout hash, tuning hash, pool hash) within the travel deadline)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0001: Crowd identity rules (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `ack-bookkeeping-private` as one buildable behaviour: acknowledgement state is keyed by ParticipantId, cleared per epoch and never replicated. It is traced to the GDD criterion below and to TR-session-005. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 11.6. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthPlayerState replicates only PublicHandle and PlayerNamePrivate; AStealthPlayerState::ShouldBroadCastWelcomeMessage returns false in every phase; a rejoiner adopts its retained PlayerState in InitPlayerState and OnDeactivated does not destroy it while admission is RejoinOnly; team-audience data travels only in a C-TeamView on the receiving teammate's own PlayerController (ADR-0001, ADR-0005)
- Forbidden: Toggling bIsInactive or removing a PlayerState from the array between Preparing and the next travel; spawning a fresh PlayerState for a reclaim; any per-instance, per-kind or on-possession change to a pawn's net config (ADR-0001, ADR-0003, ADR-0005)
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget (ADR-0001); Session silence clocks inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 50, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **Rule 11.6, keying and privacy**: GIVEN 3 clients mid-acknowledgement, WHEN every replicated property of the GameState, PlayerStates and PlayerControllers is reflected, THEN none holds acknowledgement state, `ConnectionState` or `TuningHash`, AND the acknowledgement record is keyed by `ParticipantId` and cleared at the next travel start. `AckBookkeepingPrivate` [S] + [U]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- A replicated acknowledged flag would publish which entries are human (TDC AC-11). Records {FAckEpoch outstanding, MismatchCount} live in FSessionCore keyed by ParticipantId, cleared at every epoch start (travel start and each TDC Revision change); [S] half reflects GameState, PlayerStates and PlayerControllers for ack state, ConnectionState and TuningHash.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 049 (`story-049-world-ready-second-stage.md`): world-ready bookkeeping

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Session/StealthSessionCore.h`
- `Source/StealthFoundation/Private/Session/StealthSessionCore.cpp`
- `Source/StealthFoundation/Private/Tests/Session/AckBookkeepingPrivateTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **Rule 11.6, keying and privacy**: GIVEN 3 clients mid-acknowledgement, WHEN every replicated property of the GameState, PlayerStates and PlayerControllers is reflected, THEN none holds acknowledgement state, `ConnectionState` or `TuningHash`, AND the acknowledgement record is keyed by `ParticipantId` and cleared at the next travel start. `AckBookkeepingPrivate` [S] + [U]
  - Given: 3 clients mid-acknowledgement.
  - When: Every replicated property of the GameState, PlayerStates and PlayerControllers is reflected.
  - Then: None holds acknowledgement state, ConnectionState or TuningHash. The acknowledgement record is keyed by ParticipantId and cleared at the next travel start.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/AckBookkeepingPrivateTest.cpp` — automation test `StealthGame.Session.AckBookkeepingPrivate` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.AckBookkeepingPrivate`, T1 unit test, class `FSessionAckBookkeepingPrivateTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-042-ack-arrival-order-independent.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Cross-epic: Epic `participant-state-roster` (`production/epics/participant-state-roster/EPIC.md`): the registration API (RegisterParticipant, RebindView, NotifyConnectionChanged, IsRoundScopeReset, InvalidateAllBindings) is a peer epic built in the same window; use a fake Roster behind the interface for Logic tests.
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): knob registry (sub-struct `Session`), `TuningSchemaVersion` and `ClientHash`.
- Unlocks: `production/epics/session-travel/story-049-world-ready-second-stage.md`
