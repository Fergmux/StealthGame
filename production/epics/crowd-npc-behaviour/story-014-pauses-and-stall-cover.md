# Story 014: Pauses, stall-cover regulator and the stationary-stretch bound

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: session-travel GDD (rule 25f, connection-stall cover), shared-character-locomotion GDD (R5.5)
**ADR Decision Summary**: Pauses are StateTree states with lengths drawn per pawn and timed on the per-frame due-time list; the Director runs the F4 stall-cover regulator and the stationary-stretch watchdog, forcing a `Routing` leg when a stretch reaches `CrowdMaxIdleSec`; the stretch clock is suspended while a pawn is concealed.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: No post-cutoff API beyond the StateTree stack of story 012. Stationary state is derived from movement state, not from the StateTree state, so a conversation or reaction pause counts too.

**Why this story**: A connection stall by a real player looks like a stationary body; the crowd must contain enough long-stationary NPCs that this is not a tell (Tell Matrix T9), while no NPC may stand longer than a player legitimately could be seen to (T14). Read GDD CNB-R5 (C2, watchdog), F4 and AC-CNB-12, AC-CNB-13.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: behaviour delays live in the controller's per-frame due-time list; the stationary-stretch watchdog is kept by the Director; values from the frozen tuning snapshot (ADR-0016)
- Forbidden: timing delays in decision-tick counts or the engine timer manager; adapting rates at runtime (ADR-0016)
- Guardrail: the regulator uses public positions and velocities only; cost inside `Crowd.Director`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-12, AC-CNB-13 and CNB-R5 (C2), scoped to this story:*

- [x] (AC-CNB-12) GIVEN 10 min of the reference scene sampled every 1 s, WHEN NPCs in a stationary stretch already at least 4 s long are counted (the F4 metric), THEN the city-wide count is at least `ceil(0.1 * N_live)` in at least 98 % of samples, and the regulator never sets the long-pause share above 90 %; the per-viewer share (10 bot viewers, samples with at least 10 NPCs visible) is reported, not gated.
- [x] (AC-CNB-13) GIVEN 60 min of simulation, WHEN every visible stationary stretch of every NPC and restricted-driver pawn is measured, including chains of pause, conversation and reaction pause, THEN none exceeds `CrowdMaxIdleSec` (20 s); the stretch clock is suspended while the pawn is concealed.
- [x] C2 pauses: over at least 1,000 pauses the short share lies in the 99 % binomial interval of 0.55 with lengths in U(1, 3) s, and the long share in 0.45 with lengths in U(4, 16) s; a long pause ends facing a stall, mural or view within 3 m when one exists (within 30 degrees, a provisional QA tolerance), with no facing requirement when none exists; a pawn idle for `CrowdMaxIdleSec` receives a `Routing` leg in that tick.

---

## Implementation Notes

*Derived from ADR-0016 section 3 (watchdog) and GDD CNB-R5, F4:*

- After each leg: 55 % short pause U(1, 3) s, 45 % long U(4, 16) s (`CrowdLongPausePct` 45, `CrowdIdleMinSec`/`MaxSec` 4/16). Long pauses face a context within 3 m with head pitch down 10-20 degrees allowed (the pitch goes through the story 006 look path).
- F4 regulator: raise `CrowdLongPausePct` up to 90 % when the city-wide share of NPCs in a stretch at least 4 s old falls below `CrowdStallCoverMinPct` (10 %). It reads only the Director's activity data; it never reads a pawn's kind.
- Watchdog: the Director tracks each driven pawn's visible stationary stretch from movement state; at `CrowdMaxIdleSec` it forces a `Routing` leg through a due-time wake. The stretch budget runs from the start of the whole stationary chain (pause, conversation, reaction pause), so chains cannot exceed the single-entry maximum.
- The 30 degree facing tolerance in the third criterion is a QA tolerance added at review (the GDD gives no angle); record it in the evidence note and raise it to the GDD author if it needs to become a key.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): Replaces the prototype's random wander pauses in `Source/StealthGame/Private/StealthNpcController.cpp`; none of its timing is reused.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 015: conversations and turn-in-place (a stretch component, but owned there)
- Story 028: restricted-driver takeover (this story only supplies the idle bound it reuses)
- Story 038: exit-time re-run of AC-CNB-12/13 on the full catalogue

---

## Files

- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdStallRegulator.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdStallRegulator.cpp`
- `Source/StealthGameplay/Features/Crowd/Public/Server/CrowdAgent.h`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdAgent.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdStallCoverTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-3 revised after the GAPS verdict (sample size, interval and facing tolerance added).*

- **AC-1**: `StealthGame.Crowd.StallCoverShare`
  - Given: 10 min reference scene sampled every 1 s
  - When: NPCs in a stationary stretch already at least 4 s long are counted
  - Then: count at least `ceil(0.1 * N_live)` in at least 98 % of samples; the regulator never sets the long-pause share above 90 %; per-viewer share reported, not gated
  - Edge cases: N_live drops after kills; samples with fewer than 10 visible NPCs excluded from the viewer report
- **AC-2**: `StealthGame.Crowd.MaxIdleBound`
  - Given: a 60 min simulation including pause, conversation and reaction chains
  - When: each visible stationary stretch is measured
  - Then: none exceeds `CrowdMaxIdleSec`; the clock is suspended while concealed
  - Edge cases: chain of three entries; a pawn that enters a bush mid-stretch
- **AC-3**: `StealthGame.Crowd.PauseKinds`
  - Given: at least 1,000 pauses with fixed seeds, some with a stall within 3 m and some without
  - When: lengths and facing are logged
  - Then: short share in the 99 % binomial interval of 0.55, each length in its U range; long pause with a target within 3 m ends facing it within 30 degrees; a pawn idle at `CrowdMaxIdleSec` gets a Routing leg in that tick
  - Edge cases: target exactly at 3 m; two targets

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdStallCoverTest.cpp` — must exist and pass (AC-1 and AC-2 run as headless accelerated simulations)

**Status**: [x] Created and passing (`StealthGame.Crowd.Pauses.StallCover`, `StealthGame.Crowd.Pauses.StretchBound`, `StealthGame.Crowd.Pauses.PauseMix`, `StealthGame.Crowd.Pauses.Watchdog`; 2026-10-26)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-013-identity-and-command-path-gate.md`

## Completion Notes
**Completed**: 2026-10-26 (Crowd & NPC Behaviour batch, stories cnb-001..018; the production crowd drives L_Prototype, D53)
**Criteria**: 3/3 passing
**Deviations**: F4 regulator amended (decision logged): it raises the long share while the covered count OR the P_th look-ahead (pawns whose rest still runs at Now + P_th) is below ceil(f_min N) + 3 (CoverMarginNpcs); the GDD rule alone gave 85.7 % of samples. Result 99.2 %, mean 9.3 covered, never above 90 %. Watchdog lead 2.5 s (a forced leg turns and clears a partner first). 30-degree facing tolerance recorded as a provisional QA tolerance; long pauses that became a conversation are reported, not checked. Raise the margin/look-ahead to the GDD author (F4).
**Test Evidence**: `StealthGame.Crowd.Pauses.StallCover`, `StealthGame.Crowd.Pauses.StretchBound`, `StealthGame.Crowd.Pauses.PauseMix`, `StealthGame.Crowd.Pauses.Watchdog`; full suite 275/275: `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/full-suite.txt`
**Run result**: 3-player PIE on L_Prototype with the production crowd (40 NPCs, the NpcCount default): `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
**Reviewed by**: self-review against the code-review skill during the batch; findings and fixes in `production/qa/evidence/crowd-npc-behaviour-001-018-2026-10-26/evidence.md`
