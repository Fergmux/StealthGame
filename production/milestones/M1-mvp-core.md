# Milestone: M1 MVP-Core

> Authored by hand from `.claude/docs/templates/milestone-definition.md` (no skill writes this directory).
> Defined 2026-10-25 (Pre-Production, unattended run, user authorisation in force).
> Sources: `production/estimates/2026-10-25_mvp-rebaseline.md` (MVP-core checkpoint, week 13-15),
> `production/design-closeout/2026-10-25_rereview-followups.md` (D36-D45, esp. **D38**),
> `design/gdd/game-concept.md` (MVP Definition, Scope Tiers), `production/epics/index.md`,
> `production/gate-checks/*.md` (accepted risks).

## Overview

- **Target Date**: 2027-01-29 (end of week 14 counted from Sprint 1 start 2026-10-26). Planning window 2027-01-22 (week 13) to 2027-02-05 (week 15). Low confidence: no velocity data exists; Sprint 1 is the calibration (see below).
- **Type**: Prototype-to-production checkpoint ("MVP-core"): the first build on the production architecture (ADR-0007 four-module layout, ADR-0008 harness) that is playable by 10 players. Not the Vertical Slice (that is M2) and not the full MVP.
- **Duration**: 14 weeks (2026-10-26 to 2027-01-29)
- **Number of Sprints**: 7 (Sprints 1-7, two weeks each). Sprint 1 is "Calibration + bootstrap" (`production/sprints/sprint-01.md`). Sprint boundaries after Sprint 1 are indicative and are re-planned from measured velocity at the end of each sprint.

## Milestone Goal

Land the Foundation and Core epics on the production architecture and run a playable 10-player round on it, so that the MVP hypothesis ("reading beats chance": a human who watches a suspect before striking hits a rival on the first strike materially more often than the base rate) can be measured on the real stack and a PROCEED / PIVOT decision made under **D38**. M1 also replaces the single-module prototype's role as the build everything else stands on: the include-boundary lint, the leak-parity harness and the CI gate are live, and the Pillar 1 identity tells (ADR-0013, `IsLocallyControlled` branch) are removed.

At the end of M1 we can demonstrate: a 10-connection round (listen host plus clients, in-process PIE and a Gauntlet multi-process run) on a graybox map with the mannequin-variant character pool, the crowd, strikes and the objective chain, with the PROCEED/PIVOT measurement taken from at least one documented human playtest block.

## Success Criteria

The milestone is complete ONLY when all of these are met.

- [ ] **Foundation epics landed (all stories Complete or explicitly deferred with a logged reason):** Tuning Data & Config (Exit A bootstrap 001-028, doc-lint 029-037, Exit B pipeline 038-100), Participant State & Roster, Session & Travel, Information Visibility Policy.
- [ ] **Core epics landed:** Input & Interaction Framework, Shared Character & Locomotion, Camera System, Round & Match Framework, Level Socket Contract (graybox map), Character Pool & Appearance (mannequin variants; the Meshy swap is M2 scope).
- [ ] **Minimum Feature slice for a measurable round landed:** Crowd & NPC Behaviour (50 NPCs, `NpcCount` 50), Assassination & Defence Reversal, Objectives Chain (one chain on the graybox map). Without these there are no first strikes to measure. (NPC-Kill Penalty & Informants and Concealment are Should Ship, below.)
- [ ] **Playable 10-player round on the production architecture:** 10 connections (listen host plus 9 clients) complete a round to `Ended` on the graybox map, observed in a retained run (screenshot set in `production/qa/evidence/` and the harness result JSON), in in-process PIE and in one Gauntlet multi-process run.
- [ ] **PROCEED/PIVOT measurement per D38 recorded:** informed first-strike hit rate on a player computed from the Round Event Log over **>= 40 first strikes** across playtest rounds, written to `production/qa/evidence/` with the raw counts and the verdict: **>= 0.27 PROCEED** (3x the ~0.09 base rate at 5v5, 50 NPCs); **0.15-0.27 PIVOT** (tune tells and crowd density, then re-measure); **< 0.15 KILL-or-rethink review** (creative-director plus producer). The operational definition of an "informed" strike and the per-team-size treatment (see Risk Register) are fixed in the playtest plan before the first counted round.
- [ ] **Pillar 1 parity gates green:** the multi-client leak-parity diff (ADR-0008) shows no identity tell on any opponent-client capture; ADR-0013 `IsLocallyControlled` removal verified by the class-parity scan with its negative control; spike S1 (multi-process raw-bunch capture) reported PASS or with a recorded ADR amendment.
- [ ] **Conditioning spikes reported:** every ADR accepted "conditional on spikes" has its spike report in `production/qa/spikes/` with PASS, FAIL plus the taken fallback, or an amendment request: S1, S2, S3, S4, S9, S10, S12, S13, S14, V1, V2, V3, V5, camera mask OQ5/OQ10, net-emulation (traversal and session), and the 3060-class captures R1-R3, RC-1, RC-2 (ADR-0022).
- [ ] **Velocity re-baselined:** the Sprint 1 calibration is applied (measured planning-days per day replaces the 0.6 factor) and the remaining-MVP estimate in `production/estimates/` is reissued after Sprint 1 and again at M1 close.
- [ ] All S1 and S2 bugs resolved
- [ ] Performance within budget on target hardware: ADR-0022 budgets at the reference preset (RTX 3060-class PC, 1080p, `sg.*` groups at 3), see Quality Gates. Until a 3060-class capture exists the figures stay labelled ASSUMED and the criterion is NOT ASSESSED, not passed.
- [ ] Build stable for 5 consecutive working days (per-commit gate green, nightly green)

## Feature List

Sprint targets are indicative (no velocity yet). Owners are agent roles; the single human developer reviews, decides, runs the editor and plays.

### Must Ship (Milestone Fails Without These)

| Feature | Design Doc | Owner | Sprint Target | Status |
|---------|-----------|-------|--------------|--------|
| Tuning Data & Config: Exit A bootstrap (module skeleton, harness, lints, CI, VCS checks) | `design/gdd/tuning-data-config.md`; `production/epics/tuning-data-config/EPIC.md` | lead-programmer, devops-engineer, qa-lead | 1 (stories 001-028; spills into 2) | In Sprint 1 plan |
| Tuning Data & Config: doc-lint 029-037 and Exit B pipeline 038-100 | same | lead-programmer, unreal-specialist | 2-4 | Not Started |
| Information Visibility Policy (audiences, culling, leak tests, spikes S1/S9/S10/S12) | `design/gdd/information-visibility-policy.md` | network-programmer, ue-replication-specialist | 1-4 | Spikes in Sprint 1 plan |
| Participant State & Roster (stores, C-TeamView carriers) | `design/gdd/participant-state-roster.md` | gameplay-programmer, ue-replication-specialist | 2-4 | Not Started |
| Session & Travel (seamless travel, rejoin grace, spikes S3/S4) | `design/gdd/session-travel.md` | network-programmer | 1-4 | Spikes in Sprint 1 plan (stretch) |
| Input & Interaction Framework (pawn command interface, spikes V1/V2/V3/V5) | `design/gdd/input-interaction-framework.md` | gameplay-programmer | 2-4 | Spikes in Sprint 1 plan (stretch) |
| Shared Character & Locomotion (one pawn class, ADR-0013 removal, traversal under net emulation) | `design/gdd/shared-character-locomotion.md` | gameplay-programmer, ue-replication-specialist | 1-5 | ADR-0013 stories in Sprint 1 plan |
| Round & Match Framework (round states, victories, Event Log) | `design/gdd/round-match-framework.md` | gameplay-programmer | 3-5 | Stories not yet created |
| Level Socket Contract plus hand-built graybox map | `design/gdd/level-socket-contract.md` | level-designer, gameplay-programmer | 3-4 | Stories not yet created |
| Character Pool & Appearance (mannequin-variant pool; uniformity contract) | `design/gdd/character-pool-appearance.md` | technical-artist, art-director | 3-5 | Stories not yet created |
| Camera System (first/third-person toggle, eye mask) | `design/gdd/camera-system.md` | gameplay-programmer | 4-6 | Mask spike in Sprint 1 plan (stretch) |
| Crowd & NPC Behaviour (50 NPCs, gaze catalogue, Tell Matrix) | `design/gdd/crowd-npc-behaviour.md` | ai-programmer | 4-6 | Stories not yet created |
| Assassination & Defence Reversal | `design/gdd/assassination-defence-reversal.md` | gameplay-programmer | 5-6 | Stories not yet created |
| Objectives Chain (one chain, graybox) | `design/gdd/objectives-chain.md` | gameplay-programmer | 5-6 | Stories not yet created |
| 10-player integration pass and PROCEED/PIVOT measurement (D38) | `design/gdd/game-concept.md` (MVP hypothesis); D38 | producer, qa-lead | 7 | Not Started |

### Should Ship (Planned but Cuttable)

| Feature | Design Doc | Owner | Sprint Target | Cut Impact | Status |
|---------|-----------|-------|--------------|-----------|--------|
| NPC-Kill Penalty & Informants | `design/gdd/npc-kill-penalty-informants.md` | gameplay-programmer | 6-7 | Wrong-target consequences absent from the measured rounds; first-strike count unaffected. Moves to M2 | Not Started |
| Concealment (Bushes), per viewer | `design/gdd/concealment-bushes.md` | gameplay-programmer, ue-replication-specialist | 6-7 | Hiding tactic absent; weakens Pillar 1 coverage of the leak-parity diff (the cull path is only exercised by spikes S9/S10/S12). Moves to M2 | Not Started |
| 3060-class performance captures (R1-R3, RC-1, RC-2) | `docs/architecture/adr-0022-rendering-performance-budgets.md` | performance-analyst | 6-7 | Budgets stay ASSUMED; risk carries to M2 with no later cheap point to find a failure | Not Started |
| Gauntlet nightly with `StealthLeak2` and `StealthClean` profiles | `production/epics/tuning-data-config/EPIC.md` | devops-engineer | 2 | Leak-parity regressions found by hand instead of overnight | In Sprint 1 plan (stretch) |

### Stretch Goals (Only if Ahead of Schedule)

| Feature | Design Doc | Owner | Value Add |
|---------|-----------|-------|----------|
| Early start on Teammate Markers (core clamp and carrier) | `design/gdd/teammate-markers.md` | ui-programmer | De-risks M2 items 8 and 10 (ping and reconnect indicator depend on the marker clamp) |
| Early start on keyboard walk-slower (IIF story 017) | `design/gdd/input-interaction-framework.md` | gameplay-programmer | Cheapest MVP item (about 1.5 days); good early win, relevant to crowd pace feel in the D38 measurement |
| Remove the Game Animation Sample Ctrl jog toggle (D37) | `production/design-closeout/2026-10-25_rereview-followups.md` D37 | gameplay-programmer | Frees Ctrl for walk-slower before any playtest |

## Quality Gates

| Gate | Threshold | Measurement Method |
|------|-----------|-------------------|
| Crash rate | 0 crashes in any retained 10-player round; < 1 per 10 hours of soak | Harness result JSON, editor and Gauntlet logs |
| Frame rate | >= 60 FPS (16.67 ms) at the reference preset; CPU game thread <= 12.0 ms p95 (listen host and client), render/RHI <= 12.0 ms p95, GPU 14.4 ms with >= 2.0 ms headroom (ADR-0022 s4). ASSUMED until measured on 3060-class | `stat unit` / Unreal Insights captures, `production/qa/evidence/`; NOT ASSESSED until the 3060-class capture exists |
| Load time | Seeded round start to `Playing` under the Session & Travel budget (TDC `Session` knobs); no number is set in M1 beyond "measured and reported" | Net-emulation spike (Session story 002) records map load time; automated timing in the harness |
| Critical bugs | 0 open S1 | Bug tracker (`production/qa/bugs/` when it exists; until then the sprint QA sign-off reports) |
| High-severity bugs | 0 open S2 in delivered features | Same |
| Test coverage | Every Logic and Integration story has a passing automated test; every BLOCKING leak/parity AC passes (`testing.strict` defaults, `.claude/docs/coding-standards.md`); coverage % is not a gate | Per-commit gate script, nightly T3 suites, `/smoke-check`, `/team-qa` sign-off |
| Pillar 1 leak diff | 0 identity tells on opponent-client captures (opponent-as-host run included) | ADR-0008 leak-parity harness with seeded-violation negative controls |
| D38 sample size | >= 40 first strikes on players | Round Event Log `KillResolved` export |

## Risk Register

Includes the risks the gate reports accepted (see Dependencies > Gate reports). "Accepted" means accepted under the user's standing gate policy, not retired.

| Risk | Probability | Impact | Mitigation | Owner | Status |
|------|------------|--------|-----------|-------|--------|
| Velocity uncalibrated; the 0.6 agent-assist factor is a judgement (rebaseline "Risk Factors") | High | Whole schedule +/- 50 % | Sprint 1 records planned vs actual per story; re-baseline at Sprint 1 close and again at M1 close | producer | Open; measured in Sprint 1 |
| Traversal replication under 100 ms / 1 % loss unproven (SCL High-Risk row) | Medium | +1 to +2 weeks | SCL story 045 spike before traversal is built on it | gameplay-programmer | Spike in Sprint 1 plan (Should) |
| 50 NPCs on 3060-class budget unmeasured; prototype 50 characters at 89.6 FPS / 11.2 ms game thread on an RTX 5090 only (gate: concept to systems, accepted) | Medium | +1 to +3 weeks | Measure early on target hardware; significance LOD levers in ADR-0013; captures R1-R3 | performance-analyst | Accepted; Open |
| 10 humans over a real network behave differently from in-process PIE (rebaseline, pessimistic case) | Medium | Reopens netcode work | Gauntlet multi-process run in M1 plus one real-LAN session; RC-1 10-connection capture | network-programmer | Open |
| ADRs 0003/0013/0022 conditional on spikes; a failed spike forces ADR rework (gate: technical setup, accepted) | Medium | Rework of dependent stories | Run conditioning spikes before dependent stories (Sprint 1 plan covers the first wave) | technical-director | Accepted; Open |
| Pillar 1 identity leak open until ADR-0001 spikes complete (gate: concept to systems, accepted) | Medium | High (core pillar) | S1 multi-process raw-bunch capture in Sprint 1 plan (Must); ADR-0013 removal (Must) | network-programmer | Accepted; Open |
| 5 Foundation and about 16 Core TR partials open at the technical-setup gate (accepted) | Medium | Gaps found mid-build | Core partials closed before Core stories start (technical-director concern) | technical-director | Accepted; Open |
| GDD approval rests on main-session decision D35, not independent APPROVED verdicts (gate: systems design to technical setup, accepted) | Medium | Design rework when stories expose gaps | Story-readiness check per story; log ruling in the decision log | producer | Accepted; Open |
| 7 cross-review warnings open or ruled (TD-REG-1 registry gap, C-W24 final-court margin script, others in 25b) | Low | Registry and doc-lint gaps | TD-REG-1 closed by TDC stories 029-037 | lead-programmer | Accepted; Open |
| Stories exist for only 8 of 19 epics today (Round, Level Socket, Character Pool, Crowd, Concealment, Assassination, NPC-Kill, Objectives, Procgen, Pings, Markers, HUD have none) | High | Sprints 3+ cannot be planned from Ready stories | `/create-stories` per epic one sprint ahead (balanced granularity, D41) | producer | Open |
| D38 states an absolute margin (>= 0.27) but the base rate varies by team size (`TeamSize / (TeamSize + NpcCount)`: 0.09 at 5v5, higher at smaller teams); "informed first strike" has no operational definition | Medium | The measurement could pass or fail for reasons unrelated to reading | Fix the definition and the team-size treatment in the playtest plan before the first counted round; do not change D38 itself without the user | creative-director, producer | Open (flagged, not changed) |
| >= 40 first strikes on players needs human testers; a solo developer cannot supply them unaided; no recruitment owner or date exists | High | D38 cannot be measured; M1 closes without its key criterion | Schedule a playtest block by M1 week 10 (Sprint 5); smaller teams (2v2+) are allowed; fresh-context agents do not count as D38 players | producer | Open |
| No human sensitivity read of the Mexica framing before an art-commit story (D40) | Medium | Cultural-framing rework | Desk check by a fresh agent scheduled in Pre-Production (D40); human read before M2 | art-director, narrative-director | Open |
| Single machine: UE builds, PIE and multi-process runs are serial, single editor instance, asset-resave hazards (`production/never-resave-assets.md`) | High | Throughput capped regardless of agent count | Batch adjacent stories per session (D41); script-only stories run without the editor; never-resave pre-commit check (Sprint 1 plan) | producer | Open |
| Walk-slower collides with the sample's Ctrl jog toggle (D37) | Medium | +1 day | Remove the sample toggle at implementation | gameplay-programmer | Ruled (D37); implementation pending |

## Dependencies

### Internal Dependencies

| Feature | Depends On | Owner of Dependency | Status |
|---------|-----------|-------------------|--------|
| Every other epic | Tuning Data & Config Exit A (module skeleton, harness, include lint, CI) | lead-programmer | Sprint 1 (stories 001-028) |
| Shared Character & Locomotion (keys, class parity key-uniqueness half) | TDC Exit B key pipeline (SCL story 003 depends on it) | lead-programmer | Not Started (Sprint 2-3) |
| Traversal stories SCL 047-051, 053, 055 | SCL story 045 spike (needs TDC net-emulation profiles, story 017) | gameplay-programmer | Sprint 1 plan |
| Concealment, IVP cull stories | IVP spikes S9, S10, S12 (stories 004-006) | network-programmer | Sprint 1 plan (004 Should; 005, 006 stretch) |
| Session & Travel build, Roster rebind and rejoin | Session spikes S3, S4 and the harness connection-event hook (story 001) | network-programmer | Sprint 1 plan (stretch) |
| IIF context swap and hand-over stories | IIF spikes V1, V2, V3, V5 | gameplay-programmer | Sprint 1 plan (stretch) |
| Camera mask stories 015, 022 | Camera mask spike (story 011) which needs the 60-character graybox street (Character Pool and Level Socket Contract) | gameplay-programmer | Missing prerequisite, flagged in Sprint 1 plan |
| Round & Match | Roster registration API, Session & Travel, TDC Exit B | gameplay-programmer | Not Started |
| Crowd, Assassination, Objectives | Round & Match, SCL, IIF, Level Socket Contract graybox map | ai-programmer, gameplay-programmer | Not Started |
| 10-player integration pass | Everything above | qa-lead | Not Started |

### External Dependencies

| Dependency | Provider | Status | Risk if Delayed |
|-----------|---------|--------|----------------|
| Unreal Engine 5.8.4 installed at `C:/Program Files/Epic Games/UE_5.8` | Epic | Installed | Version drift between reference docs and engine; re-verify before relying on [U] claims |
| Self-hosted Windows CI runner (`.github/workflows/tests.yml`) | The developer (registering the runner is a user action, TDC story 022) | Workflow exists; real CI run unobserved | Per-commit gate runs locally only |
| Git LFS lock behaviour on the GitHub remote | GitHub (TDC story 027) | [U] until observed once | Binary-asset collisions |
| 3060-class reference hardware | The developer | Not confirmed to exist | ADR-0022 budgets stay ASSUMED; Quality Gates stay NOT ASSESSED |
| Human playtesters (>= 40 first strikes) | The developer | Not recruited | D38 criterion unmeasurable |
| Common UI / Game Input plugins (5.8) | Epic | Common UI enabled in `StealthGame.uproject` 2026-10-25 | IIF V5 spike |

## Review Schedule

| Date | Review Type | Attendees |
|------|-----------|-----------|
| 2026-11-06 (Sprint 1 close) | Calibration review: measured velocity, re-baseline, go/no-go on Sprint 2 scope | Producer, technical-director |
| 2026-11-09 (week 3) | Early progress check | Producer, Directors |
| 2026-12-11 (week 7, Sprint 4 close) | Mid-milestone review (`/milestone-review`): Foundation landed? spike results vs ADR conditions | Full agent team, user |
| 2027-01-15 (week 12) | Pre-milestone review: 10-player integration readiness, playtest block confirmed | Full agent team, user |
| 2027-01-29 | Milestone review: success criteria, D38 verdict (PROCEED / PIVOT / KILL-or-rethink) | Full agent team, user |
