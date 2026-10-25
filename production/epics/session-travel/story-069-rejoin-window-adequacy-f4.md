# Story 069: RejoinWindowSec satisfies F4 against measured load and restart times

> **Epic**: Session & Travel
> **Status**: Ready — unblocked 2026-10-25: the net-emulation measurement spike (story 002) reported (production/qa/spikes/2026-10-25_session-net-emulation.md): Soft 3, Hard 12, RejoinWindow 120, TravelLoad 60 and AckGrace 15 confirmed from measured R_c, RTT_p99, bursts and load times (Wi-Fi bursts remain a playtest input).
> **Layer**: Foundation
> **Type**: Config/Data
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

**Why this story**: This story delivers `rejoin-window-adequacy-f4` as one buildable behaviour: rejoinWindowSec satisfies F4 against measured load and restart times. It is traced to the GDD criterion below and to TR-session-006. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Formulas F4; Tuning Knobs. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD `design/gdd/session-travel.md` Acceptance Criteria no. 37, scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] **F4** GIVEN the spike's measured `t_load` and `T_srvload` and the playtest's `t_restart`, WHEN F4 is evaluated, THEN `RejoinWindowSec >= 1.5 * (t_restart + t_connect + t_load + A) + T_busy`. `RejoinWindowF4` [S] [SPIKE] [PLAYTEST]

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- RejoinWindowSec >= 1.5 * (t_restart + t_connect + t_load + A) + T_busy, with t_load and T_srvload from the spike and t_restart from the playtest; A is ClientAutoRejoinIntervalSec. Also must satisfy Round's DisconnectGraceSec <= RejoinWindowSec - 30. Evidence is the [S] formula check plus a [PLAYTEST] record in production/qa/evidence/.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None: the prototype has no session handling, identity key, admission, liveness or travel, so there is nothing to replace or promote.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002 (`story-002-net-emulation-measurement-spike.md`): the measurement

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/rejoin-window-f4-evidence.md`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE). Implement against these; do not invent new test cases during implementation.*

**Config/Data smoke check:**

- **AC-1**: **F4** GIVEN the spike's measured `t_load` and `T_srvload` and the playtest's `t_restart`, WHEN F4 is evaluated, THEN `RejoinWindowSec >= 1.5 * (t_restart + t_connect + t_load + A) + T_busy`. `RejoinWindowF4` [S] [SPIKE] [PLAYTEST]
  - Smoke: Take t_load and T_srvload from the story-002 spike and t_restart from the playtest record. Evaluate F4: RejoinWindowSec >= 1.5 * (t_restart + t_connect + t_load + A) + T_busy, with A = ClientAutoRejoinIntervalSec and T_busy = T_srvload (0 if no travel overlaps). Pass when the configured RejoinWindowSec (default 120) satisfies it. Also check Round's DisconnectGraceSec <= RejoinWindowSec - 30. Record inputs, result and the playtest in production/qa/evidence/rejoin-window-f4-evidence.md.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Config/Data
**Required evidence**:
- Config/Data: smoke check pass in `production/qa/smoke-*.md` plus the evidence file `production/qa/evidence/rejoin-window-f4-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/session-travel/story-002-net-emulation-measurement-spike.md`
- Cross-epic: Epic `round-match-framework` (`production/epics/round-match-framework/EPIC.md`) consumes Session's events and pushes the admission mode; use a fake Round behind the consumer-owned interfaces here.
- Unlocks: None
- Unblock when: the spike named in the Status line has reported and its report is linked here
