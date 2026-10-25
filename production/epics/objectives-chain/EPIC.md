# Epic: Objectives Chain

> **Layer**: Feature
> **GDD**: design/gdd/objectives-chain.md
> **Architecture Module**: Objectives in `StealthGameplay/Features` (chain data asset, per-team progress, `UObjectiveTeamViewComponent`)
> **Status**: Ready
> **Stories**: Not yet created — run `/create-stories objectives-chain`
> **Engine Risk**: MEDIUM (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25e
> **Build Order**: 15 of 19 (layer order; the risk-first sequence is in the Build Order and Gates section)

## Overview

Objectives Chain turns the city's identical stone courts into a race. It owns the objective definition (a reusable `FObjectiveDef` record: stage, team eligibility, interaction requirements, completion state) in a chain data asset that strings definitions together per team (two team-specific preliminary stages, then one shared final), the court binding from the server-secret `FSiteAssignment` at `OnLayoutReady`, the site audiences (which team may know which court is live and when), the Interact predicate that decides what a 3-second tend at a court does (reading only the site, public occupancy and legality class), the completion gate inside the hold-timer slot that calls `Round.DeclareObjectiveWin`, and the stall-pressure rules (no timer, no reveal) that keep waiting from ever winning.

The team view (`FObjectiveTeamView`: stage card, own steps, `RivalPrelimCount`) rides C-TeamView and is pushed at Active entry; `RivalPrelimCount` is published with a fresh per-publication delay and one global release grid so the timing is not a tell (secret stream `StageSignal`). Without it the round has no clock but the other team's knives and Pillar 4 has no engine.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| ADR-0002: Server resolution queue, server clock and round-end staging | One total-order queue keyed (ServerTickIndex, Seq) drained in TG_PostPhysics, due-time heap, typed staged writes, single-assignment latch, client server-time estimator. | MEDIUM |
| ADR-0006: Seeds and deterministic/secret random streams | Public sub-seeds for layout vs SipHash-2-4 keyed server-secret streams; integer PCG32 streams so layout is bit-identical across machines. | LOW |
| ADR-0020: Objectives data and team-view sub-structs | Chain data asset of FObjectiveDef rows, court binding from FSiteAssignment, FObjectiveTeamView on C-TeamView. | LOW |

All governing ADRs are Accepted (user, 2026-10-25). Several are conditionally accepted; the outstanding gates are in Build Order and Gates below.

## GDD Requirements

7 of 7 requirements are covered by an Accepted ADR. Untraced requirements: None.

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-oc-001 | Chain data asset of `FObjectiveDef` rows | ADR-0020 ✅ |
| TR-oc-002 | Court binding from `FSiteAssignment` at `OnLayoutReady`, server-only | ADR-0020 ✅ |
| TR-oc-003 | `FObjectiveTeamView` rides C-TeamView; pushed at Active entry | ADR-0020 ✅ |
| TR-oc-004 | Tend predicate reads only site, public occupancy and legality class | ADR-0020 ✅ |
| TR-oc-005 | Completion gate inside the hold-timer slot; `DeclareObjectiveWin` | ADR-0002, ADR-0020 ✅ |
| TR-oc-006 | `RivalPrelimCount` with fresh per-publication delay and one global release grid | ADR-0006, ADR-0020 ✅ |
| TR-oc-007 | Stall-pressure rules (no timer, no reveal) | ADR-0020 ✅ |

## Prototype Disposition

The code in `Source/StealthGame` is marked PROTOTYPE (`production/first-playable-report.md`). Production code follows ADR-0007 modules, and nothing is promoted until the ADR that governs it is built against.

**Replaces** `Source/StealthGame/Private/StealthObjective.cpp` / `Public/StealthObjective.h` (172 + 75 lines: two team-specific preliminary stages then a shared final revealed after the preliminaries; interacting visibly works the site), `StealthGameMode::OnObjectiveCompleted`/`HandleInteractRequest`, and `FStealthRules::CanCompleteObjective` (pure rule with a stage-gating unit test, the reference). Networked prototype tests D and D2 (full chain 1 -> 2 -> final -> objective win; on a generated city) are the behavioural reference. The prototype tracks stage per team without a data asset, a view struct or stall-pressure rules.

## Build Order and Gates

- **Depends on (epics)**: input-interaction-framework (hold timers), round-match-framework (queue, `DeclareObjectiveWin`), level-socket-contract (`FSiteAssignment`), participant-state-roster, information-visibility-policy, tuning-data-config; concealment-bushes (hard peer for P4), crowd-npc-behaviour (decoy rate, soft)
- **Gates and risks**: ADR-0020 conditional gate: Objectives Chain GDD re-review and game-designer sign-off on the two GDD interpretations (ADR section 2 and 5.3); no spike. LOW risk where it is rules and data, MEDIUM where the view rides the C-TeamView mechanism. The design risk is a camping incentive (systems-index): the final site stays hidden until the preliminaries are done; tune interaction time by playtest.
- **Estimate**: about 6 bottom-up days (human-paced yardstick, `production/estimates/2026-10-25_mvp-rebaseline.md`, Method B work packages); planning figure about 3.6 days after the uniform 0.6 calibration. Low confidence: no sprint history exists, so run the 2-week calibration spike and re-baseline.
- **Acceptance criteria in the GDD**: about 32 (counted as GIVEN-form criteria in the Acceptance Criteria section).
- **MVP build order**: risk-first per `design/gdd/systems-index.md`: graybox map and mannequin-variant pool first, then camera toggle and traversal replication, then the Meshy pool swap, then the generator.

## Definition of Done

- Every story implemented, reviewed and closed with `/story-done`
- Every acceptance criterion of design/gdd/objectives-chain.md verified
- Logic and Integration stories have passing tests in the test root (`Source/<Module>/Private/Tests/`)
- Visual/Feel and UI stories have retained screenshots in `production/qa/evidence/`
  (a lead sign-off too for Visual/Feel)
- Stories embed Control Manifest version 2026-10-25e and every governing ADR's conditional gate has reported before dependent work is marked Done
