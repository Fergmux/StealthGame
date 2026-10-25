# Story 043: A first hash mismatch triggers one resend with AckSeq + 1; a second disconnects with ParamsMismatch

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
**Also relevant**: ADR-0004 (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `ack-mismatch-resend-once` as one buildable behaviour: a first hash mismatch triggers one resend with AckSeq + 1; a second disconnects with ParamsMismatch. It is traced to the GDD criterion below and to TR-session-005. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 11.6; Core Rules rule 23. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 14, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN a client whose (a) `LocalLayoutHash`, (b) `ClientHash` (one public tuning field corrupted through the test hook) or (c) `PoolHash` differs from the server's, WHEN it acknowledges, THEN the server sends exactly one further `ClientRequestRoundAck` with `AckSeq + 1` and no replicated struct is re-sent; a second mismatch gives `Disconnected(ParamsMismatch)` with auto-rejoin stopped, a manual rejoin inside the window is admitted, and the round is not aborted. `AckMismatchOnce` [U]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- A replicated property that has not changed is never re-sent, so the resend is a new ClientRequestRoundAck with AckSeq + 1 to the owner only (a counter on a replicated struct would tell everyone someone failed). Second mismatch gives Disconnected(ParamsMismatch), client auto-rejoin stops, a manual rejoin inside the window is admitted, and the round is never aborted by client reports. The test corrupts one public tuning field through the test hook.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Supersedes the prototype's in-place round restart (StealthGameMode.cpp StartRound/EndRound/auto-restart timer, lines 238-349) and the replicated StealthGameState::RoundSeed (StealthGameState.h:36-51). None of it is promoted. The prototype city seeds (29755, 8924, 25482, 17906, 13609, 19972, 13135) are smoke references only.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 065 (`story-065-client-auto-rejoin-after-crash.md`): client auto-rejoin stop rule

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/Server/Session/StealthSessionCore.h`
- `Source/StealthFoundation/Private/Session/StealthSessionCore.cpp`
- `Source/StealthGame/Public/StealthPlayerController.h`
- `Source/StealthGame/Private/StealthPlayerController.cpp`
- `Source/StealthFoundation/Private/Tests/Session/AckMismatchOnceTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN a client whose (a) `LocalLayoutHash`, (b) `ClientHash` (one public tuning field corrupted through the test hook) or (c) `PoolHash` differs from the server's, WHEN it acknowledges, THEN the server sends exactly one further `ClientRequestRoundAck` with `AckSeq + 1` and no replicated struct is re-sent; a second mismatch gives `Disconnected(ParamsMismatch)` with auto-rejoin stopped, a manual rejoin inside the window is admitted, and the round is not aborted. `AckMismatchOnce` [U]
  - Given: A client whose (a) LocalLayoutHash, (b) ClientHash (one public tuning field corrupted through the test hook) or (c) PoolHash differs from the server's.
  - When: It acknowledges.
  - Then: The server sends exactly one further ClientRequestRoundAck with AckSeq + 1 and re-sends no replicated struct. A second mismatch gives Disconnected(ParamsMismatch) with auto-rejoin stopped. A manual rejoin inside the window is admitted, and the round is not aborted.
  - Edge cases: none beyond the criterion

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/AckMismatchOnceTest.cpp` — automation test `StealthGame.Session.AckMismatchOnce` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.AckMismatchOnce`, T1 unit test, class `FSessionAckMismatchOnceTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-042-ack-arrival-order-independent.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): knob registry (sub-struct `Session`), `TuningSchemaVersion` and `ClientHash`.
- Unlocks: None
