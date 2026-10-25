# Story 071: Per-player connection fields are owner-only or constant and player and NPC pawns export the same references

> **Epic**: Session & Travel
> **Status**: Blocked — BLOCKED: ADR-0001 spike S1 reported FAIL for whole-pawn parity (production/qa/spikes/2026-10-25_S1-S2-S14.md: RemoteViewPitch16 and CBP_SandboxCharacter.CharacterInputState still differentiate player pawns); ADR-0001 is conditionally accepted and a failed spike is handled by a superseding ADR, which does not exist yet
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
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

**Why this story**: This story delivers `leak-allowlist-connection-fields` as one buildable behaviour: per-player connection fields are owner-only or constant and player and NPC pawns export the same references. It is traced to the GDD criterion below and to TR-session-010. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 25(b), 25(d). Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthPlayerState replicates only PublicHandle and PlayerNamePrivate; AStealthPlayerState::ShouldBroadCastWelcomeMessage returns false in every phase; a rejoiner adopts its retained PlayerState in InitPlayerState and OnDeactivated does not destroy it while admission is RejoinOnly; team-audience data travels only in a C-TeamView on the receiving teammate's own PlayerController (ADR-0001, ADR-0005)
- Forbidden: Toggling bIsInactive or removing a PlayerState from the array between Preparing and the next travel; spawning a fresh PlayerState for a reclaim; any per-instance, per-kind or on-possession change to a pawn's net config (ADR-0001, ADR-0003, ADR-0005)
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget (ADR-0001); Session silence clocks inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 29, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN the PlayerState, pawn and game-state classes, WHEN reflected, THEN the per-player connection fields in rule 25b are owner-only or constant, and player and NPC pawns export the same replicated reference set. `LeakAllowlist` [S]

---

## Implementation Notes

*Derived from ADR-0001 Implementation Guidelines and the GDD rules named above:*

- Rule 25b field list: ping (and compressed ping), sequential player id, start time, inactive, bot, spectator and only-spectator flags, from-previous-level flag, score, unique net id (check a null-subsystem id does not embed the address), and any post-Lobby name change. AStealthPlayerState replicates only PublicHandle and PlayerNamePrivate (ADR-0001 section 4.3). The pawn-reference half cannot pass until whole-pawn parity (S1) is resolved.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Reference only: Source/StealthGame/Private/Spikes/StealthIdentitySpike.* (S1/S2/S14 harness, cvar stealth.Spike.IdentityTrims) and production/qa/spikes/2026-10-25_S1-S2-S14.md. Do not promote; reuse the probe approach.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 076 (`story-076-visibility-allowlist-coverage.md`): allowlist coverage diff

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Session/LeakAllowlistTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: GIVEN the PlayerState, pawn and game-state classes, WHEN reflected, THEN the per-player connection fields in rule 25b are owner-only or constant, and player and NPC pawns export the same replicated reference set. `LeakAllowlist` [S]
  - Given: The PlayerState, pawn and game-state classes.
  - When: They are reflected.
  - Then: The per-player connection fields in rule 25b (ping including compressed ping, sequential player id, start time, inactive, bot, spectator and only-spectator flags, from-previous-level flag, score, unique net id, post-Lobby name change) are owner-only or constant. Player and NPC pawns export the same replicated reference set.
  - Edge cases: A null-subsystem unique net id must not embed the address.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Session/LeakAllowlistTest.cpp` — automation test `StealthGame.Session.LeakAllowlist` must exist and pass (`/story-done` checks it exists; pass/fail is established by `/gate-check` and `/smoke-check`)

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.LeakAllowlist`, T1 unit test, class `FSessionLeakAllowlistTest` (one class per `IMPLEMENT_SIMPLE_AUTOMATION_TEST`), flags `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`; wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: None within this epic
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: None
- Unblock when: the spike named in the Status line has reported and its report is linked here
