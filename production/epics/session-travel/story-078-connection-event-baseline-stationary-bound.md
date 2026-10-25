# Story 078: A pawn whose player went silent 15 s stays within the Crowd baseline

> **Epic**: Session & Travel
> **Status**: Blocked — BLOCKED: the net-emulation measurement spike (story 002) has not reported; the GDD (Open Question 4, F3/F4/F7) holds the Soft, Hard, window and travel defaults provisional until it does
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-010 = Leak-surface checklist (PlayerState fields, ping, join timing) enforced via the allowlist)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**Also relevant**: ADR-0005: Session, seamless travel and match-scoped state stores (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. Conditionally accepted: spikes S1, S2, S14 must have reported before implementation depends on them (S1 reported FAIL for whole-pawn parity on 2026-10-25).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking is a HIGH-risk domain for UE 5.4-5.8; legacy NetDriver with Iris pinned off. APawn::RemoteViewPitch16 (5.6) replaces the 8-bit RemoteViewPitch. Verification owned by spikes S1, S2, S14 (report: production/qa/spikes/2026-10-25_S1-S2-S14.md).

**Why this story**: This story delivers `connection-event-baseline-stationary-bound` as one buildable behaviour: a pawn whose player went silent 15 s stays within the Crowd baseline. It is traced to the GDD criterion below and to TR-session-010. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 25(f). Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthPlayerState replicates only PublicHandle and PlayerNamePrivate; AStealthPlayerState::ShouldBroadCastWelcomeMessage returns false in every phase; a rejoiner adopts its retained PlayerState in InitPlayerState and OnDeactivated does not destroy it while admission is RejoinOnly; team-audience data travels only in a C-TeamView on the receiving teammate's own PlayerController (ADR-0001, ADR-0005)
- Forbidden: Toggling bIsInactive or removing a PlayerState from the array between Preparing and the next travel; spawning a fresh PlayerState for a reclaim; any per-instance, per-kind or on-possession change to a pawn's net config (ADR-0001, ADR-0003, ADR-0005)
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget (ADR-0001); Session silence clocks inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD AC 46, part (a); part (b) is story 079, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **Rule 25f, binding; the mechanism is now defined by Round R17.1 (hand-over at Soft) and the Crowd baseline, and implementation sign-off stays blocked until the playtest in (b) passes**: (a) GIVEN an Active round with 3 clients under net emulation, WHEN a player's connection goes silent for 15 s and then rejoins, THEN in a non-owner client's recording that pawn's longest stationary stretch is ≤ Crowd's `CrowdMaxIdleSec` (20 s), no frame-to-frame position change exceeds the NPC smoothing threshold, and its visibility never toggles. `ConnectionEventBaseline` [PIE-3] [SPIKE].

---

## Implementation Notes

*Derived from ADR-0001 Implementation Guidelines and the GDD rules named above:*

- Mechanism is Round R17.1 (hand-over at Soft) and the Crowd baseline (CrowdMaxIdleSec 20 s); Session raises OnParticipantUnresponsive at Soft and owns none of the visible behaviour.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 079 (`story-079-connection-event-playtest-signoff.md`): the playtest

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/Session/ConnectionEventBaselinePieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS — 'The NPC smoothing threshold' has no numeric value here (owned by the Shared Character GDD); the recording method for 'longest stationary stretch' and 'visibility never toggles' is not defined; sign-off stays blocked on story 079. (accepted and written as is in the unattended run; revise the criterion or run `/qa-plan` before `/dev-story`).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/ConnectionEventBaselinePieTest.cpp` — `StealthGame.Session.PIE.ConnectionEventBaseline` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.ConnectionEventBaseline`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(ConnectionEventBaseline, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-001-harness-connection-event-hook.md`, `production/epics/session-travel/story-002-net-emulation-measurement-spike.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Cross-epic: Epic `round-match-framework` (`production/epics/round-match-framework/EPIC.md`) consumes Session's events and pushes the admission mode; use a fake Round behind the consumer-owned interfaces here.
- Unlocks: `production/epics/session-travel/story-079-connection-event-playtest-signoff.md`
- Unblock when: the spike named in the Status line has reported and its report is linked here
