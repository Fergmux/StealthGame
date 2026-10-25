# Story 084: Lobby clients see 'Name joined' and 'Name left' and team only for their own teammates

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-010 = Leak-surface checklist (PlayerState fields, ping, join timing) enforced via the allowlist)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules
**ADR Decision Summary**: Four audience carriers (C-Public allowlist on PlayerState, owner-only C-TeamView on PlayerController, C-Server, GameState public structs); every UE default that reveals player-ness is overridden; one pawn class, one look path, identity blindness. Conditionally accepted: spikes S1, S2, S14 must have reported before implementation depends on them (S1 reported FAIL for whole-pawn parity on 2026-10-25).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Networking is a HIGH-risk domain for UE 5.4-5.8; legacy NetDriver with Iris pinned off. APawn::RemoteViewPitch16 (5.6) replaces the 8-bit RemoteViewPitch. Verification owned by spikes S1, S2, S14 (report: production/qa/spikes/2026-10-25_S1-S2-S14.md).

**Why this story**: This story delivers `lobby-join-leave-notices` as one buildable behaviour: lobby clients see 'Name joined' and 'Name left' and team only for their own teammates. It is traced to the GDD criterion below and to TR-session-010. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 24; UI Requirements. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: Session status text is a display-only layer in the HUD's L4 (ZOrder 60); controls (Cancel, OK) live in a Session-owned modal pushed at ZOrder 100 through the menu root (GDD UI Requirements, D15, ADR-0021 L4 split); every Session RPC is owner-only (ADR-0005)
- Forbidden: No in-round element may show joins, leaves, connection states, ping values or counts of other participants, except the teammate-only reconnecting lines; Session never replicates connection data of other participants (GDD rule 24; ADR-0001, ADR-0005)
- Guardrail: Status text appears within 1 RTT plus one frame of the gate decision; every waiting state shows exactly one plain status line and ends in bounded time (GDD Game Feel)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 61, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **Rule 24, lobby notices**: GIVEN 3 clients in Lobby on two teams, WHEN one joins and one leaves, THEN every client logs "Name joined" / "Name left", and each client's lobby list shows team only for its own teammates. `LobbyNotices` [PIE-3]

---

## Implementation Notes

*Derived from ADR-0001 Implementation Guidelines and the GDD rules named above:*

- Notices come from the Lobby UI reading Session events, never from engine messages (story 025). In Preparing, Active and Ended there are none for non-teammates; teammates see only the reconnecting lines (HUD-R10a, driven by the Team-audience bReconnecting bit). No UX spec for the lobby panel exists yet (main-menu.md MM-11): a retained screenshot is needed as UI evidence.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no menu, lobby panel or status overlay (AStealthHUD draws only in-round debug text).

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- the lobby's Start control (round-match-framework epic)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthPresentation/Private/StealthLobbyPanel.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/Session/LobbyNoticesPieTest.cpp`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Automated test spec:**

- **AC-1**: **Rule 24, lobby notices**: GIVEN 3 clients in Lobby on two teams, WHEN one joins and one leaves, THEN every client logs "Name joined" / "Name left", and each client's lobby list shows team only for its own teammates. `LobbyNotices` [PIE-3]
  - Given: 3 clients in Lobby on two teams.
  - When: One joins and one leaves.
  - Then: Every client logs 'Name joined' / 'Name left', and each client's lobby list shows team only for its own teammates.
  - Edge cases: Notices come from the Lobby UI reading Session events. A retained screenshot of the lobby list is also needed as UI evidence.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/Session/LobbyNoticesPieTest.cpp` — `StealthGame.Session.PIE.LobbyNotices` (CQTest PIE network test), OR a documented playtest in `production/qa/evidence/`

**Status**: [ ] Not yet created

Test naming follows `.claude/rules/test-standards.md` and ADR-0008: automation name `StealthGame.Session.PIE.LobbyNotices`, T2 CQTest PIE network test declared with `NETWORK_TEST_CLASS(LobbyNotices, "StealthGame.Session.PIE")` (exempt from the F[System][Scenario]Test class-name rule, ADR-0008); wrap the file in `#if WITH_DEV_AUTOMATION_TESTS`; tests use an injected clock and entropy, no wall-clock time as a pass predicate, no shared mutable state, and assert zero unexpected faults at teardown.

---

## Dependencies

- Depends on: `production/epics/session-travel/story-001-harness-connection-event-hook.md`, `production/epics/session-travel/story-025-engine-welcome-messages-suppressed.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): the ADR-0007 module skeleton (StealthFoundation, StealthGameplay, StealthPresentation, StealthTestHarness), test harness and CI must exist before the files below can be created at those paths. Until it lands, write the code and tests under `Source/StealthGame/` (Private/Tests/) and move them when the skeleton arrives.
- Unlocks: None
