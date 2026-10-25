# Story 016: Gaze catalogue (G1-G7): glances, holds, goods and salient motion

> **Epic**: Crowd & NPC Behaviour
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-002 = StateTree per NPC (~4 Hz) + follower emitting Move/Look through the IIF; no path-following driving.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: shared-character-locomotion GDD (R8 head filter, R8.8 T13 definition, R8.9 idle gaze layer, D27), ADR-0003 (`RemoteViewPitch16`)
**ADR Decision Summary**: All gaze goes through `Look` via the virtual camera and the control-rotation write; the SCL head filter makes every NPC glance look like a human's. Glance targets come from the spatial hash filtered by the perception filter (never concealed, always with line of sight); holds and reaction delays are due-time entries.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The pitch values for G3 (-10 to -20 degrees) ride the story 006 pitch path; no engine focus system is used.

**Why this story**: Long head holds on a passer-by (T13) are a published advantage tell, and the crowd must produce them at exactly the target rate, while context holds (goods, partner, idle scans) must not be counted as T13. Read GDD CNB-R6 (G1-G7), CNB-R7 head-hold clause and AC-CNB-16.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: the crowd AIController writes `Look` yaw and pitch into the control rotation; perception excludes concealed pawns identically for every kind; every draw via `FCrowdDraws` (ADR-0016)
- Forbidden: `SetFocus`/`SetFocalPoint`/`FaceRotation`; an NPC-only idle scan (Crowd adds none, G1 is SCL's shared layer) (ADR-0016, GDD G1)
- Guardrail: gaze updates run every controller tick; target selection inside `Crowd.Perception`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-16 and CNB-R6, scoped to this story:*

- [ ] (AC-CNB-16a) GIVEN 10 min of NPC `Look` streams, WHEN `HeadLook` is logged, THEN head rate never exceeds `LookMaxRateDegPerSec`, every G2 glance holds 0.4-2.5 s unless it is a G6 hold of 2.5-6 s, and no G2 target is concealed or out of line of sight.
- [ ] (AC-CNB-16b) The G6 rate is within the 99 % Poisson interval of 0.10 per NPC-minute over a 600-minute run (G1 idle-scan sweeps of 3-7 s are context holds, excluded from this limit and from the G6 count, SCL R8.8, D27), and G3 holds reach a pitch of -10 to -20 degrees.
- [ ] G4: over at least 400 eligible salient stimuli (run, traversal landing or strike within 15 m with line of sight) the glance share lies in the 99 % binomial interval of 0.5, and the reaction delay measured from the stimulus time (not the decision tick) lies in [0.3, 1.2] s and passes a KS test at p > 0.01 against U(0.3, 1.2).

---

## Implementation Notes

*Derived from GDD CNB-R6 and ADR-0016 section 4:*

- G1 scan is SCL's idle gaze layer (SCL R8.9): sweeps of 30-60 degrees yaw every 3-7 s when the look input has been unchanged for more than `IdleGazeDelaySec` (3 s); Crowd adds no NPC-only scan. A `Look` that re-issues the same value counts as unchanged, so the virtual look layer cannot defeat the trigger.
- G2: nearest moving pawn within 6 m with line of sight (any kind, never concealed), hold 0.4-2.5 s, every 4-10 s; while walking, the direction obeys the motion-parity profile (story 010).
- G3: facing a stall or prop within 3 m: pitch -10 to -20 degrees for 1-4 s (contextual).
- G4: salient motion glance after a reaction delay drawn from logged player reaction times (provisional U(0.3, 1.2) s), timed from the stimulus.
- G6: an independent clock with mean `CrowdHeadHoldIntervalSec` (600 s) turns the next G2 glance into a 2.5-6 s hold (T13). The sampler and chain-gap guard come from story 018; this story implements the behaviour.
- G5 and G7 are owned with the conversation story (015) and turn-in-place; this story provides the shared glance machinery.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; prototype NPCs had no gaze behaviour (they moved their heads only through the sample AnimBP defaults). New code.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 015: G5 partner look and C6 body re-centre
- Story 018: the tell clock and chain-gap guard for G6
- Story 024: strike glances (R8)
- The SCL epoch: the head filter and idle gaze layer themselves

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdGazeTasks.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdGazeTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25). AC-3 revised after the GAPS verdict (sample size, interval and delay test added).*

- **AC-1**: `StealthGame.Crowd.GazeWithinHeadLimits`
  - Given: 10 min of NPC `Look` streams
  - When: `HeadLook` is logged
  - Then: head rate never exceeds `LookMaxRateDegPerSec`; every G2 glance holds 0.4-2.5 s unless G6 2.5-6 s; no G2 target concealed or out of line of sight
  - Edge cases: target becomes concealed mid-glance
- **AC-2**: `StealthGame.Crowd.HeadHoldRateAndGoodsPitch`
  - Given: a 600-minute run
  - When: G6 holds and G3 pitches are counted
  - Then: G6 rate in the 99 % Poisson interval of 0.10 per NPC-minute (G1 sweeps excluded); G3 holds reach -10 to -20 degrees
  - Edge cases: G6 clock firing while no passer-by is in range
- **AC-3**: `StealthGame.Crowd.SalientMotionGlance`
  - Given: at least 400 eligible salient stimuli
  - When: glances and delays are logged
  - Then: glance share in the 99 % binomial interval of 0.5; delay from stimulus time in [0.3, 1.2] s and KS p > 0.01 against U(0.3, 1.2)
  - Edge cases: delayed ray (story 008) keeps the stimulus time

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdGazeTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-010-virtual-input-layer.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md`
