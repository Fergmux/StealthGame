# Story 002: Net-emulation spike measures RTT, packet rate, burst outages and map load time

> **Epic**: Session & Travel
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/session-travel.md`
**Requirement**: `TR-session-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-session-006 = Liveness silence clock, rejoin window, supersede, client auto-rejoin)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**Also relevant**: ADR-0008 (read the listed section only; the governing ADR above decides the mechanism)
**ADR Decision Summary**: AGameModeBase with no match-state machine; four game-instance stores (Session, Roster, Round, Tuning preset) are the only authoritative match state; engine seamless ServerTravel with ?Seed=/?Preset=/?Round= server options and a re-bind of the replaced PlayerController and PlayerState on arrival; Session's single server-requested round acknowledgement; admission gates in PreLogin; Session's own clamped silence clock with engine timeouts as a backstop; rejoin and supersede that re-attach the retained PlayerState; default login pawn disabled; no host migration.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Uses only long-standing AGameModeBase, APlayerController and UNetDriver hooks, each re-read in the 5.8.4 source, so residual risk is behavioural (spikes S3, S4, S5), not API names. Seamless travel is disabled in single-process PIE unless net.AllowPIESeamlessTravel=1 (E7), so every travel acceptance test runs multi-process standalone. DispatchPostLogin is deprecated since 5.6: override PostLogin/OnPostLogin. The listen host never passes PreLogin (E2). Use UE_LOGF for logging.

**Why this story**: This story delivers `net-emulation-measurement-spike` as one buildable behaviour: net-emulation spike measures RTT, packet rate, burst outages and map load time. It is traced to the GDD criterion below and to TR-session-006. Read these GDD sections before starting (use `bash .claude/scripts/md-section.sh extract design/gdd/session-travel.md "<heading>"` or Grep `^#` then Read a range; the GDD is 150 KB, never read it whole): Formulas F3, F4, F7; Tuning Knobs; Open Questions 4. Where this story overlaps a neighbour, the Out of Scope list says which story owns the other half.

**Control Manifest Rules (Foundation layer, manifest 2026-10-25e)**:
- Required: AStealthGameMode derives AGameModeBase; authoritative match state lives only in the four game-instance stores, keyed by ParticipantId, never by controller or PlayerState pointer; every Session RPC is owner-only and the participant is derived from the receiving connection; every wait is bounded by a server-measured clock; HandleStartingNewPlayer is a no-op (ADR-0005)
- Forbidden: AGameMode, AddInactivePlayer/FindInactivePlayer or address-keyed reclaim; anything authoritative on a PlayerController, PlayerState, GameMode or GameState; a secret in a travel URL or login option, or client code parsing travel URL options; a multicast Session RPC; a client message that starts, extends or restarts a deadline; Session spawning, freeing, hiding or possessing a pawn or calling RequestTransition; overriding DispatchPostLogin (ADR-0005)
- Guardrail: Session silence clocks and ack bookkeeping inside the 0.5 ms line; travel load bounded by TravelLoadTimeoutSec (60 s) (ADR-0005)

---

## Acceptance Criteria

*From GDD Open Question 4 ('Measured client packet rate, burst-outage distribution on Wi-Fi, and map load time: they confirm or revise the Soft, Hard, window and travel defaults (F3, F4, F7)'; owner network-programmer, spike report) and the epic's Build Order and Gates ('run the 2-week calibration spike and re-baseline'), scoped to this story (one criterion, `story_granularity: fine`):*

- [ ] GIVEN 3 clients on one machine under the StealthLeak1 packet-simulation profile (PktLag 100 ms, PktLoss 1 percent) and the StealthClean profile, WHEN a 30 minute session with a lobby phase, one round travel and an active round runs, THEN a report in production/qa/spikes/ records the measured client packet rate R_c, the burst-outage distribution, RTT_p99, map load time t_load and server load T_srvload, and states for each of SessionSoftTimeoutSec, SessionHardTimeoutSec, RejoinWindowSec, TravelLoadTimeoutSec and TravelAckGraceSec whether the GDD default is confirmed or revised (F3, F4, F7).

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines and the GDD rules named above:*

- Profiles are engine [PacketSimulationProfile.StealthLeak1|StealthLeak2|StealthClean] sections in Config/DefaultEngine.ini applied with -PktEmulationProfile= (ADR-0008 section on net profiles); the config change is made by this story if the harness story has not.
- Measure with the multi-process harness (T3). Wall-clock appears only as a failure ceiling, never as a pass predicate (control manifest, ADR-0008).
- Outputs feed F3 (SessionSoftTimeoutSec >= 2*RTT_p99 + 1 and >= 3/R_c), F4 (RejoinWindowSec formula) and F7 (T_srvload range). If a default is revised, record it as a TDC knob change request; do not edit the GDD here.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Reference only: Source/StealthGame/Private/Spikes/StealthIdentitySpike.* (S1/S2/S14 harness, cvar stealth.Spike.IdentityTrims) and production/qa/spikes/2026-10-25_S1-S2-S14.md. Do not promote; reuse the probe approach.

**Tuning values**: read every threshold, timeout and default from the `Session` knob sub-struct in tuning (session-latched, ADR-0004); never hardcode a gameplay value.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 068 (`story-068-false-timeout-validation-f3.md`): F3 soak and config validation
- Story 069 (`story-069-rejoin-window-adequacy-f4.md`): F4 evaluation
- any default change in the GDD

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/spikes/<date>_session-net-emulation.md`
- Module placement follows ADR-0007 section 2 (`Public/Server/` for server-only headers with the guard include first, `Public/View/` for client-safe ones); confirm before creating a header.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

QL-STORY-READY: GAPS — No pass/fail threshold and no run-validity condition (for example, that all 3 clients stay connected for the 30 minutes); the criterion only asks for a report with a confirmed-or-revised verdict per knob and no test is named, so the report is the only evidence. (accepted and written as is in the unattended run; revise the criterion or run `/qa-plan` before `/dev-story`).

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report named in the Files list, retained in `production/qa/spikes/`, plus any harness test it produces

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/session-travel/story-001-harness-connection-event-hook.md`
- Cross-epic: Epic `tuning-data-config` (`production/epics/tuning-data-config/EPIC.md`): knob registry (sub-struct `Session`), `TuningSchemaVersion` and `ClientHash`.
- Unlocks: `production/epics/session-travel/story-067-host-loss-detection-and-session-end.md`, `production/epics/session-travel/story-068-false-timeout-validation-f3.md`, `production/epics/session-travel/story-069-rejoin-window-adequacy-f4.md`, `production/epics/session-travel/story-077-silent-drop-and-rejoin-presentation.md`, `production/epics/session-travel/story-078-connection-event-baseline-stationary-bound.md`, `production/epics/session-travel/story-080-sub-threshold-lag-smooth-no-snap.md`, `production/epics/session-travel/story-085-join-latency-targets.md`
