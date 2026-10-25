# Story 018: Silence clock fires Soft and Hard inside the F2 bounds and survives host hitches

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
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

**Why this story**: This story delivers `silence-clock-detection-bound-f2` as one buildable behaviour: silence clock fires Soft and Hard inside the F2 bounds and survives host hitches. It is traced to the GDD criterion below and to TR-session-006. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rules 18, 19; Formulas F2; Tuning Knobs. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 35, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **F2** GIVEN a fake clock and a client silent from t = 0, WHEN liveness is evaluated at the defaults, THEN Soft fires in [3.00, 3.30] s and Hard in [12.00, 12.30] s; with a single 2.0 s host frame, Hard fires by 14.2 s; with 20 consecutive 0.5 s frames, no other client is marked Unresponsive. `DetectionBoundF2` [U]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- Session's own accumulator, not a timestamp subtraction: every server tick adds min(dt, LivenessMaxCountedFrameSec) and zeroes when the connection's LastReceiveRealtime advanced since the previous tick (ADR-0005 section 6, E12); evaluation every LivenessCheckIntervalSec; suspended while Traveling. Tick function in TG_PrePhysics, after the net driver receive dispatch.
- Defaults: Soft 3.0, Hard 12.0, check 0.25, clamp 0.1. Soft fires in [3.00, 3.30] s and Hard in [12.00, 12.30] s; a single 2.0 s host frame still gives Hard by 14.2 s; 20 consecutive 0.5 s frames mark no other client Unresponsive.
- Read every value from tuning (session-latched, ADR-0004); never hardcode. Guardrail: silence clocks inside the 0.5 ms line.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 068 (`story-068-false-timeout-validation-f3.md`): F3 false-timeout validation against measured RTT

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Session/StealthSessionCore.h`
- `Source/StealthFoundation/Private/Session/StealthSessionCore.cpp`
- `Source/StealthFoundation/Private/Tests/Session/DetectionBoundF2Test.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **F2** GIVEN a fake clock and a client silent from t = 0, WHEN liveness is evaluated at the defaults, THEN Soft fires in [3.00, 3.30] s and Hard in [12.00, 12.30] s; with a single 2.0 s host frame, Hard fires by 14.2 s; with 20 consecutive 0.5 s frames, no other client is marked Unresponsive. `DetectionBoundF2` [U]
  - Given: A fake clock and a client silent from t = 0, at the defaults (Soft 3.0, Hard 12.0, check 0.25, clamp 0.1).
  - When: Liveness is evaluated.
  - Then: Soft fires in [3.00, 3.30] s and Hard in [12.00, 12.30] s. With a single 2.0 s host frame, Hard fires by 14.2 s. With 20 consecutive 0.5 s frames, no other client is marked Unresponsive.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/DetectionBoundF2Test.cpp` — automation test `StealthGame.Session.DetectionBoundF2` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.DetectionBoundF2`, T1 unit test, class `FSessionDetectionBoundF2Test` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-015-connection-state-machine-illegal-transitions.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): knob registry (sub-struct `Session`), `TuningSchemaVersion` and `ClientHash`.
- Unlocks: `production/epics/session-travel/story-019-same-tick-arrival-wins.md`, `production/epics/session-travel/story-020-responsive-again-event.md`, `production/epics/session-travel/story-068-false-timeout-validation-f3.md`
