# Story 031: Emergence — kill decoupling and exit-cancel rate bound

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-006`
*(At write time: TR-lsc-006 = Shared emergence sockets; `PickRespawnPawn` of an unseen crowd pawn)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: A replacement is never served at the instant of a kill: it takes the next scheduled ambient entry (Crowd's `AmbientSchedule` stream), and one upcoming exit is cancelled, chosen uniformly among upcoming exits at all sockets; cancellations are capped at `EmergenceExitCancelMaxPct` (25%) of scheduled exits in any 60 s window and the deficit at `EmergenceDeficitMaxCount` (3).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Fixed-step simulations on the injected clock; the `AmbientSchedule` stream is Crowd's, so a Crowd stub supplies the pinned schedule until the real Crowd exists.

**Why this story**: An emergence must never mean "a kill just happened" (creative-director guard 1). Read first: R5 items 6-7, D9, AC-12, AC-13.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Draw sites use ADR-0006 shapes; the cancel draw is counter-based (ADR-0014)
- Forbidden: Never serve a replacement at the instant of a kill (GDD R5.7)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-12 and AC-13 (revised after QL-STORY-READY: the GDD's "at least 2,000 cancellations" is unreachable at the 25% cap, so the pooled sample is at least 300), scoped to this story:*

- [ ] GIVEN a pinned ambient schedule, WHEN an NPC kill resolves at time t, THEN the replacement emerges at the next scheduled entry after t (never at t), one upcoming exit is cancelled, and the NPC count is constant after the slot (`Emergence.KillDecoupled`)
- [ ] GIVEN 20 pinned 600 s fixed-step simulations with at least 20 kills each (add runs until pooled cancellations reach at least 300), THEN the entry schedule with kills equals the no-kill schedule exactly; cancelled exits never exceed 25% in any 60 s window and the deficit never exceeds 3; and the cancelled exit's socket is uniform (chi-square < 26.217, df 12, p = 0.01, at N_E 13) (`Leak.EmergenceRateBound`)

---

## Implementation Notes

- Entry times follow the schedule, never the kill; exit times differ only by the cancelled exits. Excess replacements beyond the 25% window cap are deferred (the population runs short by at most 3 until the window allows).
- Expected cancellations per socket at least 5 (300 / 13 = 23). The GDD AC-13 figure (at least 2,000 cancellations across 20 runs) cannot be reached at the 25% cap (at most about 325); flagged to the GDD owner. A kill exactly on a scheduled entry tick uses the next entry.
- Kills of NPCs, players and informants all count (`Replacement` kind covers the replacement for a killed informant's body).
- The cancellation draw uses the `EmergencePick` stream counter-based by request sequence; a Crowd stub provides `AmbientSchedule`.

---

## Out of Scope

- Story 030: the request/clear check; Crowd's actual scheduler (`production/epics/crowd-npc-behaviour/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/Layout/EmergenceKillDecoupling.cpp`
- `Source/StealthTestHarness/Private/EmergenceRateBoundTest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/EmergenceKillTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE after revision).*

- **AC-1** `Emergence.KillDecoupled`
  - Given: a pinned ambient schedule; an NPC kill at t.
  - When: it resolves.
  - Then: the replacement emerges at the next scheduled entry after t (not at t); one upcoming exit cancelled; NPC count constant after the slot.
  - Edge cases: a kill exactly on a scheduled entry tick uses the next one.
- **AC-2** `Leak.EmergenceRateBound`
  - Given: 20 pinned 600 s fixed-step runs with >= 20 kills each (add runs until pooled cancellations >= 300).
  - When: schedules and cancellations are compared.
  - Then: entry schedule with kills equals the no-kill schedule exactly; cancelled exits <= 25% in any 60 s window; deficit <= 3; cancelled-socket chi-square < 26.217 (df 12, N_E 13).
  - Edge cases: expected >= 5 per socket (300/13 = 23).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Layout/EmergenceKillTest.cpp` and `Source/StealthTestHarness/Private/EmergenceRateBoundTest.cpp` — `StealthGame.Layout.Emergence.KillDecoupled`, `StealthGame.Layout.Leak.EmergenceRateBound` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-030-emergence-request-clear-check-uniform-pick.md`, `production/epics/level-socket-contract/story-007-formulas-d5-d6-d6b-d9.md`
- Cross-epic: `production/epics/crowd-npc-behaviour/EPIC.md` (scheduler; stub until it exists)
- Unlocks: `production/epics/level-socket-contract/story-036-crowd-and-objectives-contract-tests.md`
