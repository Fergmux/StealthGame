# Epic: Objectives Chain

> **Layer**: Feature
> **GDD**: design/gdd/objectives-chain.md
> **Architecture Module**: Objectives in `StealthGameplay/Features` (chain data asset, per-team progress, `UObjectiveTeamViewComponent`)
> **Status**: Ready
> **Stories**: 23 stories (see Stories below)
> **Engine Risk**: MEDIUM (highest among governing ADRs; engine 5.8, installed 5.8.4)
> **Control Manifest**: 2026-10-25f
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
- Stories embed Control Manifest version 2026-10-25f and every governing ADR's conditional gate has reported before dependent work is marked Done

## Stories

Written by `/create-stories objectives-chain` (story_granularity `balanced`, 2-4 acceptance criteria per story). All 23 are Ready: the epic has no spike, and ADR-0020's conditional gate (Objectives Chain GDD re-review; game-designer sign-off on the two GDD interpretations) is recorded as cleared in the GDD (Approved 2026-10-25; OC-7 carries the 2026-10-24 sign-off; `InteractSecOverride` removed by the user 2026-10-25). ADR-0020's verification items OBJ-V1, OBJ-V2 and OBJ-V3 are acceptance criteria of stories 011 and 002, so they report when those stories close. Cross-epic upstream gates (for example ADR-0001 spikes S1, S2, S14) still apply at Done, per the Definition of Done above.

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Objectives tuning rows and invariants I-OC1 to I-OC3 | Logic | Complete | ADR-0020 |
| 002 | Chain data types and the Cacao chain asset | Logic | Complete | ADR-0020 |
| 003 | Core chain validation and court binding | Logic | Complete | ADR-0020 |
| 004 | Core site state and per-team progress | Logic | Complete | ADR-0020 |
| 005 | Service shell and composition-root wiring | Integration | Complete | ADR-0020 |
| 006 | Fixture claims and the interact-target owner | Logic | Complete | ADR-0020 |
| 007 | Interact predicate and feint/decoy parity | Logic | Complete | ADR-0020 |
| 008 | Completion gate and stage effects | Logic | Complete | ADR-0020 |
| 009 | Final completion and win declaration | Logic | Complete | ADR-0020 |
| 010 | Team view struct and builder | Logic | Ready | ADR-0020 |
| 011 | Team view component and replication | Integration | Ready | ADR-0020 |
| 012 | Rejoin, view rebind and round reset | Logic | Ready | ADR-0020 |
| 013 | Clue function and the too-broad clue log | Logic | Ready | ADR-0020 |
| 014 | Rival signal delay draw and release grid | Logic | Ready | ADR-0020 |
| 015 | Rival signal release scheduling and idle behaviour | Logic | Ready | ADR-0020 |
| 016 | Presentation view helpers and the tend-cue ring rule | Logic | Ready | ADR-0020 |
| 017 | Stall metrics (F5) and the tend posterior band (F3) | Logic | Ready | ADR-0020 |
| 018 | Stall-pressure and cross-system checks | Integration | Ready | ADR-0020 |
| 019 | PIE-2 team view contents, ingress and informant parity | Integration | Ready | ADR-0020 |
| 020 | PIE-2 hidden-site oracle and completion world signature | Integration | Ready | ADR-0020 |
| 021 | PIE-2 end-to-end Cacao chain | Integration | Ready | ADR-0020 |
| 022 | Prototype retirement and static scans | Logic | Ready | ADR-0020 |
| 023 | Playtest advisory metrics (pacing, exposure, tend share) | Integration | Ready | ADR-0020 |

Story files: `production/epics/objectives-chain/story-NNN-<slug>.md`. ADR-0002 (queue, `TimerFired`, latch) and ADR-0006 (secret stream, integer draws) are secondary ADRs of stories 005, 008, 009, 014 and 015.
