# Story 035: F6 T_needed is computed at freeze with the specified values and warning boundaries

> **Epic**: Round & Match Framework
> **Status**: Ready
> **Layer**: Core
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: — (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/round-match-framework.md`
**Requirement**: `TR-round-006` (Preparing readiness sequence, spawn gate and watchdog); GDD AC 34a; formula F6, rule R6 (`PreparingWatchdogSec`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Session, seamless travel and match-scoped state stores
**ADR Decision Summary**: `AGameModeBase` with four game-instance stores (Session, Roster, Round, Tuning preset) as the only authoritative match state, seamless `ServerTravel` with `?Seed=`, single round acknowledgement, admission modes, host commands on the game instance, no host migration. This story applies ADR-0002 section 8.6 (Preparing watchdog) over ADR-0005's Session F7 deadline: Round computes `PreparingWatchdogEff = max(PreparingWatchdogSec, T_needed)` once, when tuning freezes, so no knob combination makes the watchdog fire before the owners' own deadlines.
**ADR Version**: ADR-0005 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0005 knowledge risk)
**Engine Notes**: None touched. F6 is pure arithmetic over tuning values and one logged warning; no engine API, post-cutoff API or verification item is exercised.

**Control Manifest Rules (this layer)** (Round-relevant rules sit in the Foundation Layer Rules, section Session and travel):
- Required: Every wait is bounded by a server-measured clock (ADR-0005); the watchdog is a Round-owned deadline compared with `ServerNow()` in the Preparing poll, not an engine timer or a queue item.
- Forbidden: Never let a client message start, extend or restart a deadline (ADR-0005).
- Guardrail: Travel load is bounded by `TravelLoadTimeoutSec` (60 s) (ADR-0005); F6 adds one evaluation at freeze and no per-frame cost.

**Prototype disposition**: none (new Round Core code).

---

## Acceptance Criteria

*From GDD `design/gdd/round-match-framework.md` AC 34a, scoped to this story:*

- [ ] GIVEN F6 inputs, THEN `T_needed` 122.25 at defaults with a generated layout (effective 150), 161.25 with `TravelLoadTimeoutSec` 120 (effective 161.25, warning logged), and the boundary `PreparingWatchdogSec = T_needed` gives no warning while `T_needed - 0.01` does. Round evaluates F6 at freeze.

---

## Implementation Notes

*Derived from GDD F6 and ADR-0002 section 8.6 (Preparing watchdog):*

- **Formula (GDD F6).** `T_prep_max = max(TravelLoadTimeoutSec, T_srvload + LayoutTotalBudgetSec + T_tuning + TravelAckGraceSec)` (Session F7); `T_needed = T_prep_max + T_spawn + WorldReadyTimeoutSec + StartLeadSec + 30`; `PreparingWatchdogEff = max(PreparingWatchdogSec, T_needed)`, computed once when tuning freezes, with a logged warning whenever `T_needed` wins.
- **Fixture inputs (from the F6 variable table).** `TravelLoadTimeoutSec` 60, `TravelAckGraceSec` 15, `T_srvload` 20 (bound), `LayoutTotalBudgetSec` 45, `T_tuning` 1 (bound), `T_spawn` 1 (bound), `WorldReadyTimeoutSec` 10, `StartLeadSec` 0.25, `PreparingWatchdogSec` 150. Worked: `T_prep_max = max(60, 20 + 45 + 1 + 15) = 81`; `T_needed = 81 + 1 + 10 + 0.25 + 30 = 122.25 <= 150`, so the effective watchdog is 150. With `TravelLoadTimeoutSec = 120`: `T_needed = 120 + 1 + 10 + 0.25 + 30 = 161.25`, so the effective watchdog is 161.25 s and a warning is logged. Open: confirm against GDD F6 and the TDC defaults that the fixture values are the shipped defaults (the AC does not restate them); freeze them as test fixtures.
- **Warning boundary.** With `PreparingWatchdogSec = T_needed`, `max` is a tie and no warning is logged; with `PreparingWatchdogSec = T_needed - 0.01`, `T_needed` wins and one warning is logged. Compare with a tolerance for the 0.01 boundary (half the last printed digit).
- **When evaluated.** Round evaluates F6 once at freeze (on `OnTuningReady`, ADR-0002 section 8.6), not per frame. The watchdog itself (firing `PreparingWatchdog`) is stamped with `RoundEpoch`, compared against `ServerNow()` in Round's `Preparing` poll, cancelled at Active entry, abort and `Reset`; the abort is story 034.
- **Output range for reference (GDD F6).** `T_needed` from 52.1 s (all minima) to 242 s (all maxima); the hard maximum of `PreparingWatchdogSec` is 300.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 028 (`production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`): the phase machine (this story's dependency).
- Story 034 (`production/epics/round-match-framework/story-034-preparing-aborts.md`): the `PreparingWatchdog` abort when the deadline fires.
- Story 032 (`production/epics/round-match-framework/story-032-preparing-barrier.md`): the barrier the watchdog bounds.
- TDC internals (schema ranges, `PreparingWatchdogSec` hard range 60 to 300, tuning freeze) and Session internals (F7 deadline values).

---

## Files

*Planned files only. Tests live under the engine test roots (`tests/README.md`), not under `tests/`. `/dev-story` replaces this list with the files it actually changed.*

- `Source/StealthGameplay/Public/Round/Preparing/PreparingWatchdog.h`
- `Source/StealthGameplay/Private/Round/Preparing/PreparingWatchdog.cpp`
- `Source/StealthGameplay/Private/Tests/Round/WatchdogBoundTest.cpp` (class `FRoundWatchdogBoundTest`, test name `StealthGame.Round.WatchdogBound`)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: F6 T_needed is computed at freeze with the specified values and warning boundaries.
  - Given: F6 inputs at defaults with a generated layout, then with `TravelLoadTimeoutSec` 120.
  - When: Round freezes Preparing and evaluates F6.
  - Then: `T_needed` = 122.25 at defaults (effective watchdog 150).
  - Then: `T_needed` = 161.25 with `TravelLoadTimeoutSec` 120 (effective 161.25, warning logged).
  - Then: `PreparingWatchdogSec = T_needed` logs no warning, and `T_needed - 0.01` logs one.
  - Then: F6 is evaluated at freeze.
  - Edge cases: the F6 default component values come from the GDD Formulas section (not restated in the AC) and must be frozen as fixtures (Open: confirm against GDD F6); use a tolerance for the 0.01 boundary.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Round/WatchdogBoundTest.cpp` (automation test `StealthGame.Round.WatchdogBound`, class `FRoundWatchdogBoundTest`) — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/round-match-framework/story-028-phase-admission-and-illegal-transitions.md`
- Unlocks: None
- Cross-epic: `production/epics/tuning-data-config` (F6 tuning keys `PreparingWatchdogSec`, `WorldReadyTimeoutSec`, `StartLeadSec`; module skeleton and StealthTestHarness bootstrap; stories not yet created), `production/epics/session-travel` (`TravelLoadTimeoutSec`, `TravelAckGraceSec` from Session F7; stories not yet created), `production/epics/level-socket-contract` (`LayoutTotalBudgetSec`; stories not yet created)
