# Story 079: Observers pick the connection-event pawn no better than chance over 20 trials

> **Epic**: Session & Travel
> **Status**: Blocked — BLOCKED: story 078 has not been built (the net-emulation spike, story 002, reported 2026-10-25: production/qa/spikes/2026-10-25_session-net-emulation.md); the playtest needs the baseline behaviour in place
> **Layer**: Foundation
> **Type**: Visual/Feel
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

**Why this story**: This story delivers `connection-event-playtest-signoff` as one buildable behaviour: observers pick the connection-event pawn no better than chance over 20 trials. It is traced to the GDD criterion below and to TR-session-010. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Core Rules rule 25(f). Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthPlayerState replicates only PublicHandle and PlayerNamePrivate; AStealthPlayerState::ShouldBroadCastWelcomeMessage returns false in every phase; a rejoiner adopts its retained PlayerState in InitPlayerState and OnDeactivated does not destroy it while admission is RejoinOnly; team-audience data travels only in a C-TeamView on the receiving teammate's own PlayerController (ADR-0001, ADR-0005)
- Forbidden: Toggling bIsInactive or removing a PlayerState from the array between Preparing and the next travel; spawning a fresh PlayerState for a reclaim; any per-instance, per-kind or on-possession change to a pawn's net config (ADR-0001, ADR-0003, ADR-0005)
- Guardrail: C-TeamView pushes change-only, at most 1 per frame, within the 50 KB/s per-client budget (ADR-0001); Session silence clocks inside the 0.5 ms line (ADR-0005)

---

## Acceptance Criteria

*From GDD AC 46, part (b); implementation sign-off stays blocked until this passes, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] (b) Playtest sign-off: 20 trials, each showing an observer 5 candidate pawns of which one had the connection event; pass if <= 7 correct picks (P(X >= 8 | p = 0.2) ~ 0.03), recorded in `production/qa/evidence/`. [PLAYTEST]

---

## Implementation Notes

*Derived from ADR-0001 Implementation Guidelines and the GDD rules named above:*

- Record who observed, the trial order and each pick; keep the screenshots or recordings.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 078 (`story-078-connection-event-baseline-stationary-bound.md`): owns the other half of this criterion (see its file)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/connection-event-playtest-evidence.md`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Manual verification steps:**

- **AC-1**: (b) Playtest sign-off: 20 trials, each showing an observer 5 candidate pawns of which one had the connection event; pass if <= 7 correct picks (P(X >= 8 | p = 0.2) ~ 0.03), recorded in `production/qa/evidence/`. [PLAYTEST]
  - Setup: Run 20 trials. Each shows an observer 5 candidate pawns, one of which had the connection event. Record who observed, the trial order and each pick; keep screenshots or recordings.
  - Verify: Count correct picks across the 20 trials.
  - Pass condition: 7 or fewer correct picks (P(X >= 8 | p = 0.2) is about 0.03), recorded in production/qa/evidence/ with lead sign-off.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained recordings or screenshots in `production/qa/evidence/` and the lead sign-off record `production/qa/evidence/connection-event-playtest-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/session-travel/story-078-connection-event-baseline-stationary-bound.md`
- Unlocks: None
- Unblock when: the spike named in the Status line has reported and its report is linked here
