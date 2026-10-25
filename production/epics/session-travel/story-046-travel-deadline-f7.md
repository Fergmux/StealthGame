# Story 046: Travel deadline is absolute: max(t_travel + 60, t_ready + 15), unmoved by disconnect or rejoin

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
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `travel-deadline-f7` as one buildable behaviour: travel deadline is absolute: max(t_travel + 60, t_ready + 15), unmoved by disconnect or rejoin. It is traced to the GDD criterion below and to TR-session-005. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 11.7; Formulas F7. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 40, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **F7** GIVEN a fake clock, readiness data published at t = 18.2 s and one client that never acknowledges, WHEN time advances, THEN that client is `Disconnected(TravelTimeout)` at t ∈ [60, 60 + `LivenessCheckIntervalSec`]; AND GIVEN readiness published at t = 55.2 s, THEN the deadline is 70.2 s for every participant; AND a disconnect plus rejoin during Preparing does not move the deadline. `TravelDeadlineF7` [U]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- Measured on the server from two events only (travel start, publication of readiness data); no client message starts, extends or restarts it. A participant leaving Traveling for Disconnected leaves Round's readiness wait set. TravelLoadTimeoutSec 60 and TravelAckGraceSec 15 come from tuning. Guardrail: travel load bounded by TravelLoadTimeoutSec.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Supersedes the prototype's in-place round restart (StealthGameMode.cpp StartRound/EndRound/auto-restart timer, lines 238-349) and the replicated StealthGameState::RoundSeed (StealthGameState.h:36-51). None of it is promoted. The prototype city seeds (29755, 8924, 25482, 17906, 13609, 19972, 13135) are smoke references only.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 048 (`story-048-travel-timeout-isolated.md`): the per-participant timeout isolation

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Session/StealthSessionCore.h`
- `Source/StealthFoundation/Private/Session/StealthSessionCore.cpp`
- `Source/StealthFoundation/Private/Tests/Session/TravelDeadlineF7Test.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **F7** GIVEN a fake clock, readiness data published at t = 18.2 s and one client that never acknowledges, WHEN time advances, THEN that client is `Disconnected(TravelTimeout)` at t ∈ [60, 60 + `LivenessCheckIntervalSec`]; AND GIVEN readiness published at t = 55.2 s, THEN the deadline is 70.2 s for every participant; AND a disconnect plus rejoin during Preparing does not move the deadline. `TravelDeadlineF7` [U]
  - Given: A fake clock, readiness data published at t = 18.2 s, and one client that never acknowledges; separately, readiness published at t = 55.2 s.
  - When: Time advances.
  - Then: The silent client is Disconnected(TravelTimeout) at t in [60, 60 + LivenessCheckIntervalSec]. With readiness at 55.2 s the deadline is 70.2 s for every participant. A disconnect plus rejoin during Preparing does not move the deadline.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/TravelDeadlineF7Test.cpp` — automation test `StealthGame.Session.TravelDeadlineF7` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.TravelDeadlineF7`, T1 unit test, class `FSessionTravelDeadlineF7Test` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-015-connection-state-machine-illegal-transitions.md`, `production/epics/session-travel/story-042-ack-arrival-order-independent.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): knob registry (sub-struct `Session`), `TuningSchemaVersion` and `ClientHash`.
- Cross-epic: Epic `round-match-framework` (`production/epics/round-match-framework/EPIC.md`) consumes Session's events and pushes the admission mode; use a fake Round behind the consumer-owned interfaces here.
- Unlocks: `production/epics/session-travel/story-048-travel-timeout-isolated.md`, `production/epics/session-travel/story-051-fallback-travel-reacknowledges.md`
