# Story 024: Strike stimulus fan-out and outcome-blind reactions (R8)

> **Epic**: Crowd & NPC Behaviour
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-27

## Context

**GDD**: `design/gdd/crowd-npc-behaviour.md`
**Requirement**: `TR-crowd-001`, `TR-crowd-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-crowd-001 = Server-only Crowd Director subsystem. TR-crowd-004 = Identity blindness: no crowd code reads player/binding/team/state.)*

**ADR Governing Implementation**: ADR-0016: Crowd AI architecture
**Also relevant**: ADR-0018 (outcome timing: strike stimulus only from `OnActionStarted(Attack)`; an NPC victim is driven as if the strike had whiffed until its death write), ADR-0001
**ADR Decision Summary**: Strike stimuli arrive from the IIF action hub `OnActionStarted(Pawn, Action)` and only from the outcome-blind `OnActionStarted(Attack)`, never `OnPawnKilled`. The Director fans a strike out to at most `CrowdStrikeResponderLimit` unbound NPCs within `CrowdStrikeReactRadiusCm` with line of sight, applying refractory and chain-gap rules; the stimulus never carries an outcome or `VictimKind`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: The action hub delegate is server-only (ADR-0011). Rays for line of sight come from the ray budget of story 008; deferred rays keep the stimulus time so the reaction delay is measured from the strike, not the decision tick.

**Why this story**: How NPCs react to a strike must not reveal whether the victim was an NPC, a player, or whether the strike whiffed (GDD CNB-R8, T6). That is the single most dangerous information leak in the crowd. Read GDD CNB-R8, G4 and AC-CNB-18.

**Control Manifest Rules (Feature layer, manifest 2026-10-25e)**:
- Required: strike stimuli come only from `OnActionStarted(Attack)`; Director fan-out limited by responder cap and refractory; delays on the due-time list (ADR-0016, ADR-0018)
- Forbidden: reading `VictimKind` or the strike outcome; reacting from `OnPawnKilled`; any NPC-only reaction not reproducible by a player (ADR-0016, GDD CNB-R4)
- Guardrail: fan-out bounded by 8 responders per strike; cost inside `Crowd.Director`/`Crowd.Perception`

---

## Acceptance Criteria

*From GDD `design/gdd/crowd-npc-behaviour.md` AC-CNB-18 and CNB-R8, scoped to this story (the visual check is story 025):*

- [x] (AC-CNB-18a) GIVEN 500 scripted strikes each of kill-NPC, kill-player, whiff and reversal in a fixed crowd, WHEN the reactions of NPCs within 15 m with line of sight are logged, THEN the reaction distribution does not differ by outcome (chi-square p > 0.01).
- [x] (AC-CNB-18b) Stratified by distance (<= 6 m, > 6 m), each R8 share is within +-20 % relative of its target (panic runs and startle braces only <= 6 m, glances in their place beyond), no reaction starts earlier than the reaction-time distribution's minimum after the stimulus, at most `CrowdStrikeResponderLimit` (8) NPCs respond per strike, no NPC reacts twice within `CrowdReactRefractorySec`, and no responder's gaze tracks the attacker after its glance ends.
- [x] Strike stimuli come only from `OnActionStarted(Attack)`, never `OnPawnKilled`; the Director never reads the outcome or `VictimKind`; an NPC victim keeps its driver and behaviour as if the strike had whiffed until its death write.

---

## Implementation Notes

*Derived from GDD CNB-R8 and ADR-0016 section 5:*

- Reaction shares (R8, provisional constants registered in story 003): 40 % none, 30 % glance at the victim or strike point (never tracking the attacker afterwards) 1-3 s, 12 % pause 2-5 s, 7 % reroute away at walk, 3 % short panic run up to 3 s, 8 % startle brace; the last two only within `CrowdStartleRadiusCm` (6 m); beyond 6 m the shares fold into glances.
- Delay: drawn from logged player reaction times (provisional U(0.3, 1.2) s), timed from the stimulus on the controller's due-time list (`ECrowdDecision::ReactionKind`, `ReactionDelay`).
- Panic runs and startle braces that would be a tell inside the responder's chain-gap become glances (story 018); the startle uses the brace behaviour of story 021.
- `OnPawnKilled` handling for ledger and exits is separate (story 027) and must not produce any observable effect before the death write.
- The Director picks responders uniformly among eligible unbound NPCs within radius with line of sight, up to the cap.

**Prototype disposition** (`production/first-playable-report.md`, `Source/StealthGame`): None; prototype NPCs ignored strikes. New behaviour.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 025: the visual read of the reaction
- Story 021: the brace behaviour itself
- Story 027: kill handling for the ledger and exit cancellation
- The Assassination epic: the strike itself and its outcome writes

---

## Files

- `Source/StealthGameplay/Features/Crowd/Private/CrowdStrikeStimulus.cpp`
- `Source/StealthGameplay/Features/Crowd/Private/CrowdReactionTasks.cpp`
- `Source/StealthGameplay/Private/Tests/Crowd/CrowdStrikeReactionTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY, 2026-10-25).*

- **AC-1**: `StealthGame.Crowd.StrikeReactionOutcomeBlind`
  - Given: a fixed crowd and 500 scripted strikes each of kill-NPC, kill-player, whiff and reversal
  - When: reactions of NPCs within 15 m with line of sight are logged
  - Then: the reaction distribution does not differ by outcome (chi-square p > 0.01)
  - Edge cases: NPCs without line of sight excluded; expected counts at least 5
- **AC-2**: `StealthGame.Crowd.StrikeReactionShares`
  - Given: the same log stratified at <= 6 m and > 6 m
  - When: shares, delays and responder counts are analysed
  - Then: each R8 share within +-20 % relative; panic runs and startle braces only <= 6 m; no reaction before the minimum delay; at most `CrowdStrikeResponderLimit` respond; refractory respected; no gaze tracking after the glance
  - Edge cases: exactly 6 m
- **AC-3**: `StealthGame.Crowd.StrikeStimulusSource`
  - Given: Attack actions, kills, and an NPC victim
  - When: `OnActionStarted(Attack)` and `OnPawnKilled` fire with Director reads instrumented
  - Then: stimuli come only from `OnActionStarted(Attack)`; the Director never reads the outcome or `VictimKind`; the victim is driven as a whiff until its death write
  - Edge cases: `OnPawnKilled` with no prior strike

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthGameplay/Private/Tests/Crowd/CrowdStrikeReactionTest.cpp` — must exist and pass

**Status**: [x] Created and passing (`StealthGame.Crowd.Strike.OutcomeBlind`, `StealthGame.Crowd.Strike.SharesAndLimits`, `StealthGame.Crowd.Strike.PriorityKeyed`; 2026-10-27)

---

## Dependencies

- Depends on: `production/epics/crowd-npc-behaviour/story-008-spatial-hash-and-perception-filter.md`, `production/epics/crowd-npc-behaviour/story-012-statetree-schema-and-decision-grid.md`, `production/epics/crowd-npc-behaviour/story-018-tell-sampler-and-calibration-counters.md`
- Unlocks: `production/epics/crowd-npc-behaviour/story-023-tell-calibration-gate.md`, `production/epics/crowd-npc-behaviour/story-025-strike-reaction-look.md`

## Completion Notes
**Completed**: 2026-10-27 (Crowd & NPC Behaviour batch, stories cnb-019..031)
**Criteria**: 3/3 passing
**Deviations**: The Director listens to UActionHub::OnActionStarted(Attack) only; the prototype strike in L_Prototype announces its start on the hub (a bridge in AStealthGameMode::HandleAssassinationRequest until the Assassination slot exists). Responders are picked by the counter-based priority keyed by (ordinal, strike index), read without advancing any counter.
**Test Evidence**: `StealthGame.Crowd.Strike.OutcomeBlind`, `StealthGame.Crowd.Strike.SharesAndLimits`, `StealthGame.Crowd.Strike.PriorityKeyed`; full suite and measured lines: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/`
**Run result**: 3-player PIE on L_Prototype with 50 NPCs: `production/qa/evidence/crowd-npc-behaviour-019-031-2026-10-27/evidence.md`
**Reviewed by**: self-review against the code-review skill (findings in the evidence note)
