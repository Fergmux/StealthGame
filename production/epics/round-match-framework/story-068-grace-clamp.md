# Story 068: DisconnectGraceSec is clamped to RejoinWindowSec with a warning above it and refused by the schema below 3 s

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-007` (disconnect policy: blend hand-over, guarded grace timer, forfeit/void); GDD AC 26a; rule R17.4 (the `G_eff` constraint), formula F4, Tuning Knobs (`DisconnectGraceSec`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies the Session window it depends on: Session's `RejoinWindowSec` reserves the slot, and Round's effective grace `G_eff = min(DisconnectGraceSec, RejoinWindowSec - GraceMarginSec)` with the constant `GraceMarginSec = 30` always leaves at least 30 s of window after the conversion (ADR-0002 section 8.7 (2), which carries the policy).
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. The clamp is pure arithmetic over two tuning values plus one recorded warning; no engine API, post-cutoff API or verification item (spikes S3, S4, S5) is exercised.

**Control Manifest Rules (this layer)** (Round policy rules sit in the Foundation Layer Rules, section Round phase flow, Event Log and public round struct):
- Required: The grace timer's effective value is `G_eff = min(DisconnectGraceSec, RejoinWindowSec - 30)` (ADR-0002, Technical Setup 2026-10-25b).
- Forbidden: Never use `FTimerManager`, a feature tick or a latent action for a gameplay timer (ADR-0002); the clamp only computes the value the queue's `ScheduleAt` uses.
- Guardrail: Tuning: 0 per frame, validation under 1 ms at `Preparing` (ADR-0004); the clamp is evaluated once at the tuning freeze.

**Prototype disposition**: none (new Round Core code; the prototype has no disconnect grace).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 26a, scoped to this story:*

- [ ] GIVEN `RejoinWindowSec` 120, THEN `DisconnectGraceSec` 90 gives `G_eff` 90 with no warning and 90.1 gives `G_eff` 90 with a TDC clamp warning; `DisconnectGraceSec` 2 is refused by the schema (hard range 3-120).

---

## Implementation Notes

*Derived from GDD F4 and R17.4, and ADR-0002 section 8.7 (2):*

- **Formula (GDD F4).** `G_eff = min(G, RejoinWindowSec - GraceMarginSec)` with `GraceMarginSec = 30` (a constant, not a knob); `GraceExpiry(p) = max(DisconnectedAt(p), ActivePhaseStart) + G_eff`. Guaranteed `RejoinWindowSec - G_eff >= 30`. At `RejoinWindowSec = 120` the clamp point is 90, so 90 passes unchanged and 90.1 clamps to 90 (it is a clamp to `RejoinWindowSec - 30`, not to the hard maximum of 120).
- **Key.** `DisconnectGraceSec` is a Round key: default 30, hard range 3 to 120, safe 15 to 60, `ServerOnly`, below 10 only in dev presets (`Solo` uses 3), 0 not allowed (ADR-0002 section 8.7 (2); GDD Tuning Knobs). A value below 3 is refused by the schema (hard range), not clamped.
- **Where the pieces live.** Round computes `G_eff` through one pure function (inputs `DisconnectGraceSec` and `RejoinWindowSec`, output `G_eff` and a "clamp applied" flag) evaluated once when tuning freezes; the grace timer item (story 060) reads the result. The warning is the TDC's ("TDC warns when the clamp applies", ADR-0002 section 8.7; AC text "TDC clamp warning"): the fake TDC gateway from story 003 records it. Open: confirm against GDD F4 and the Tuning Data & Config epic which side raises the warning text (TDC validation or Round's freeze) and expose a single recorded call either way; and confirm the schema refusal entry point (key registration with hard range 3-120, GDD Cross-System change request 14).
- **Boundaries.** QA adds 3 (accepted, hard minimum) and 120 (accepted by the schema, then clamped to 90 with the warning at `RejoinWindowSec` 120).
- **Open: confirm against GDD F4 / R17.4** the clamp rule before coding the 90.1 case, as the QA spec requires (the rule above is the R17.4 text; F4 states `RejoinWindowSec - G_eff >= 30` and "at `RejoinWindowSec = 60` the clamp limits `G_eff` to 30").

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 060 (`production/epics/round-match-framework/story-060-grace-timeline.md`): the grace timeline that consumes `G_eff` (this story's dependency).
- Story 069 (`production/epics/round-match-framework/story-069-window-expiry-while-active.md`): a rejoin-window expiry arriving for a still-Active participant.
- Story 071 (`production/epics/round-match-framework/story-071-grace-generation-guard.md`): the generation guard on the timer.
- TDC internals (schema machinery, presets, hot reload) and Session's `RejoinWindowSec` range (60 to 600, Session knob table).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Disconnect/GraceDuration.h`
- `Source/StealthGameplay/Private/Round/Disconnect/GraceDuration.cpp`
- `Source/StealthGameplay/Private/Tests/Round/GraceClampTest.cpp` (class `FRoundGraceClampTest`, test name `StealthGame.Round.GraceClamp`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: DisconnectGraceSec is clamped to RejoinWindowSec with a warning above it and refused by the schema below 3 s.
  - Given: `RejoinWindowSec` 120.
  - When: `DisconnectGraceSec` is 90, 90.1 and 2.
  - Then: 90 gives `G_eff` 90 with no warning.
  - Then: 90.1 gives `G_eff` 90 with a TDC clamp warning.
  - Then: 2 is refused by the schema (hard range 3-120).
  - Edge cases: add boundary values 3 (accepted) and 120 (accepted); 90.1 against `RejoinWindowSec` 120 shows the clamp is not to the hard max. Open: confirm the clamp rule against GDD F4 before coding the 90.1 case.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/GraceClampTest.cpp` (automation test `StealthGame.Round.GraceClamp`, class `FRoundGraceClampTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-060-grace-timeline.md`
- Unlocks: None
- Cross-epic: `production/epics/tuning-data-config` (module skeleton and StealthTestHarness bootstrap; `DisconnectGraceSec` key and schema range, clamp warning; stories not yet created), `production/epics/session-travel` (`RejoinWindowSec`; stories not yet created)
