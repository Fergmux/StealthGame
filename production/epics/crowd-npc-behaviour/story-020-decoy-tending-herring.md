# Story 020: Decoy tending: uniform court choice, early releases and the T3 posterior

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-010`, `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters. TR-crowd-002 = StateTree per NPC + follower emitting Move/Look through the IIF.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0020 (Objectives: `Interact` predicate with `AllowNoEffect`, one hold duration for every court, `Reject(NotNow)` at a full fixture)
**ADR Decision Summary**: NPC decoy tending is a real `Interact(Begin/End)` through the sink after a `CanBegin` dry run. Courts are chosen uniformly over all courts (live, decoy, used, tended), never weighted by the assignment; a refused `NotNow` begin abandons the court and chooses another.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Court fixtures are local non-replicated layout components (ADR-0020); the stand-point interface comes from Objectives (`FixtureInteractSideCount`). No post-cutoff API.

**Why this story**: Objective tending (T3) is the strongest honest tell in the game (`P_full` 0.65-0.75 in Objectives F3). If NPC decoy tends were weighted by assignment or never short, the posterior would drift outside its design band. Read GDD CNB-R7 decoy bullet and R16 row T3 and AC-CNB-14.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: discrete commands through `FCrowdCommandSink` after a `CanBegin` dry run; the hold duration is one value for every court (`GetInteractHoldSec()` has no court parameter); every draw via `FCrowdDraws` (ADR-0016, ADR-0020)
- Forbidden: reading `FSiteAssignment` or any objective-progress state in Crowd; per-court hold durations; branching on player versus NPC in Objectives (ADR-0016, ADR-0020)
- Guardrail: decoy decisions are due-time entries; no per-frame cost when idle

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-14, scoped to this story:*

- [ ] (AC-CNB-14a) GIVEN 2,000 decoy tends over 50 layouts (headless accelerated, about 8,750 simulated minutes at r_N 0.0046), WHEN court choice is tabulated, THEN a chi-square goodness-of-fit test against the uniform distribution over all courts shows p > 0.01, and so does a test of live versus decoy versus unused versus currently-tended courts.
- [ ] (AC-CNB-14b) 75 +-5 % of decoy holds complete at exactly `ObjectiveInteractSec`, every early release lies in [`CrowdDecoyEarlyReleaseMinSec`, `CrowdDecoyEarlyReleaseMaxSec`], and every decoy Begin refused `NotNow` at a full fixture is followed by a different court choice.
- [ ] (AC-CNB-14c) With Objectives' `R_full` 0.40 (F3 at `T_round` 9 min) the full-length T3 posterior `P_full` computed from the measured NPC rate lies in 0.65-0.75 (AC-OC-21).

---

## Implementation Notes

*Derived from GDD CNB-R7 decoy bullet and Objectives F3:*

- Rate: r_N about 0.0046/min (`CrowdDecoyTendIntervalSec` 13125); 75 % held to completion (`CrowdDecoyCompletePct`), early release uniform in 0.8-2.8 s (`CrowdDecoyEarlyReleaseMinSec`/`MaxSec`), Objectives change request 6 (b).
- Stand point: the NPC takes a free stand point (Objectives `FixtureInteractSideCount`) and picks another court only when every stand point is physically occupied; no avoidance beyond physical space.
- A refused Begin (`Reject(NotNow)`, a fifth Begin at a full fixture) abandons that court and chooses another, as after any rejection (R3).
- The test needs the Objectives court fixtures and `AllowNoEffect`; until the objectives-chain epic provides them, build the harness against a fixture court set and run the real gate at exit. `P_full` computation uses Objectives' formula (AC-OC-21), not a copy.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype crowd never touched objectives (`StealthObjective.cpp` is player-only). Do not port its replicated `bCompleted`/`InteractorCount`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 018: the clock and guard that decide when a tend is due
- Story 023: the 600-minute rate calibration
- The Objectives epic (`production/epics/objectives-chain/EPIC.md`): fixtures, the Interact predicate and F3

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdDecoyTendTasks.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdDecoyTendTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.DecoyTendAssignmentBlind`
  - Given: 2,000 decoy tends over 50 layouts, headless accelerated, fixed seeds
  - When: court choice is tabulated
  - Then: chi-square against uniform over all courts gives p > 0.01; so does live versus decoy versus unused versus currently-tended
  - Edge cases: expected cell counts at least 5; a failing seed is a defect, never reseeded
- **AC-2**: `StealthGame.Crowd.DecoyHoldOutcomes`
  - Given: decoy holds, and a full fixture that refuses Begin with `NotNow`
  - When: hold outcomes are logged
  - Then: 75 +-5 % complete at exactly `ObjectiveInteractSec`; every early release in [`CrowdDecoyEarlyReleaseMinSec`, `MaxSec`]; each `NotNow` refusal followed by a different court choice
  - Edge cases: only one court free
- **AC-3**: `StealthGame.Crowd.DecoyPosterior`
  - Given: Objectives `R_full` 0.40 (F3 at `T_round` 9 min) and the measured NPC decoy rate
  - When: `P_full` is computed using the AC-OC-21 formula
  - Then: `P_full` in 0.65-0.75
  - Edge cases: depends on the Objectives formula being implemented

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdDecoyTendTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md`
