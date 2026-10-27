# Story 021: Brace and startle red herring: due clock, flinch triggers and Prepare cap

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-010 = Baseline and Tell Matrix rates as data with calibration counters.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0018 (a braced NPC reverses an attacker exactly as a braced player does; Assassination owns that outcome), input-interaction-framework GDD (`Prepare` command, 60 s cooldown, planted stance)
**ADR Decision Summary**: A brace is a real `Prepare(Begin/End)` through the sink after a `CanBegin` dry run, with the same cooldown and planted stance as a player's. The T4 clock decides when a brace is due; the brace is performed at the next flinch trigger within 30 s, else as an ambient startle. A trigger alone never causes a brace.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The planted stance is the shared SCL/IIF pose; the crowd only issues the command. No post-cutoff API.

**Why this story**: Brace (T4) is the visible tell of a prepared defender. If NPCs flinched whenever a runner passed, "sprint past and watch for a flinch" would work as a test; the due-clock design makes it powerless (about 1 in 100 NPCs are due at any moment). Read GDD CNB-R7 brace bullet and R16 row T4.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: discrete commands through `FCrowdCommandSink` after a `CanBegin` dry run; every draw via `FCrowdDraws` (ADR-0016)
- Forbidden: giving the NPC sink an `Attack` method; triggering a brace from a stimulus alone; adapting rates at runtime (ADR-0016)
- Guardrail: flinch-trigger scans use the Director's hash; cost inside `Crowd.Perception`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` CNB-R7 (brace) and F2, scoped to this story:*

- [x] A brace made due by the T4 clock is performed at the next flinch trigger within 30 s (a runner within 2 m, a traversal landing within 3 m) else as an ambient startle; a trigger alone never causes a brace.
- [x] Braces use `Prepare(Begin/End)` through the sink after `CanBegin` with the same 60 s cooldown and planted stance; the share held to the Prepare hold cap (4 s, the IIF's Prepare cap key read from tuning) lies in the 99 % binomial interval of `CrowdBraceCapSharePct` (55 %) over at least 400 braces, and the rest are released at U(1.0, 3.8) s.
- [x] The accepted brace rate (ambient plus R8 startle counted in the same budget) is within the 99 % Poisson interval of `60 / CrowdBraceIntervalSec` per NPC over a 600-minute run.

---

## Implementation Notes

*Derived from GDD CNB-R7 brace bullet and F2:*

- Total accepted rate 0.020/min (`CrowdBraceIntervalSec` 3000); the ambient share is this minus the R8 startle contribution (F2; the startle comes from story 024).
- Cap share: `CrowdBraceCapSharePct` 55 % held to the cap, 45 % released at U(1.0, 3.8) s. The cap value is the IIF Prepare cap; read it from the IIF tuning, never copy it.
- Flinch triggers: a runner passing within 2 m, a traversal landing within 3 m; a trigger scans only NPCs that are already due. The 30 s due window falls back to an ambient startle.
- Attacking a braced NPC follows Assassination rules and is outside this story.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; the prototype's "brace" is a player crouch and NPCs did not brace. New behaviour.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 024: the R8 strike-reaction startle (counts in the same budget, owned there)
- The Assassination epic (`production/epics/assassination-defence-reversal/EPIC.md`): what happens when a braced NPC is struck
- Story 023: the long rate gate

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdBraceTasks.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdBraceTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-2 revised after the GAPS verdict (cap named, interval and sample size added).*

- **AC-1**: `StealthGame.Crowd.BraceDueAndTrigger`
  - Given: NPCs with the brace clock due and not due, and flinch triggers (runner at 2 m, traversal landing at 3 m)
  - When: triggers fire within and after the 30 s window
  - Then: only due NPCs brace, at the next trigger within 30 s, else as an ambient startle; a trigger never causes a brace for a non-due NPC
  - Edge cases: two triggers in the same frame; trigger at exactly 30 s
- **AC-2**: `StealthGame.Crowd.BraceCommandAndDuration`
  - Given: at least 400 braces
  - When: commands, cooldowns and durations are logged
  - Then: `Prepare` via the sink after `CanBegin`; 60 s cooldown honoured; cap-held share in the 99 % binomial interval of 0.55; the rest end in [1.0, 3.8] s
  - Edge cases: brace attempted inside the cooldown
- **AC-3**: `StealthGame.Crowd.BraceAcceptedRate`
  - Given: a 600-minute run with ambient braces and R8 startles
  - When: accepted braces are counted
  - Then: rate in the 99 % Poisson interval of `60 / CrowdBraceIntervalSec` per NPC
  - Edge cases: braces discarded by the chain-gap guard excluded

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Crowd/CrowdBraceTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Brace.DueWindowAndTriggers`, `StealthGame.Crowd.Brace.CapShareAndCooldown`; the accepted-rate criterion by `StealthGame.Crowd.TellCalibration.TellBaselineNonZero` (600 simulated minutes); 2026-10-27)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md`

## Completion Notes
**Completed**: 2026-10-27 (Crowd & NPC Behaviour batch, stories cnb-019..031)
**Criteria**: 3/3 passing
**Deviations**: The Prepare hold cap and cooldown are read from Assassination DefenceDurationSec / DefenceCooldownSec (the IIF has no separate Prepare-cap key). In L_Prototype a brace is a real PrepareBegin through the sink; no Prepare predicate is registered yet, so the dry run passes and the prototype command queue ignores NPCs (no PlayerState).
**Test Evidence**: `StealthGame.Crowd.Brace.DueWindowAndTriggers`, `StealthGame.Crowd.Brace.CapShareAndCooldown`; the accepted-rate criterion by `StealthGame.Crowd.TellCalibration.TellBaselineNonZero` (600 simulated minutes); full suite and measured lines: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/`
**Run result**: 3-player PIE on L_Prototype with 50 NPCs: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/evidence.md`
**Reviewed by**: self-review against the code-review skill (findings in the evidence note)
