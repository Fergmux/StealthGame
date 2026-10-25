# Milestone: M2 Vertical Slice (full MVP loop)

> Authored by hand from `.claude/docs/templates/milestone-definition.md` (no skill writes this directory).
> Defined 2026-10-25 (Pre-Production, unattended run, user authorisation in force).
> Sources: `production/estimates/2026-10-25_mvp-rebaseline.md`, `production/design-closeout/2026-10-25_rereview-followups.md`
> (D36-D45), `design/gdd/game-concept.md` (MVP Definition, Scope Tiers), `.claude/skills/vertical-slice/SKILL.md`,
> `production/gate-checks/*.md` (accepted risks), predecessor [M1 MVP-Core](M1-mvp-core.md).
>
> **Naming note.** This M2 is the Pre-Production validation slice that gates Pre-Production to Production (`/vertical-slice`, then `/gate-check production`): the full MVP loop of all 11 MVP items, played by humans, with a REPORT. The concept's later "Vertical Slice" scope tier (one polished district, 3 objective variants, Lobby UI, Post-Round Reveal, Diegetic Audio, Event Feedback, Settings, "+2-3 months after MVP") is production content that comes after this milestone and is not part of M2.

## Overview

- **Target Date**: 2027-04-16 (end of week 25 counted from Sprint 1 start 2026-10-26: week 24 is the re-baselined expected MVP finish, 2027-04-09, plus one week for the playtest session and the report). Optimistic 2027-03-05 (18.5 weeks, plus the same week); pessimistic 2027-07-02 (36 weeks). Low confidence; re-issue after each sprint from measured velocity.
- **Type**: Vertical Slice (Pre-Production validation)
- **Duration**: 11 weeks (2027-02-01 to 2027-04-16), starting when M1 closes (target 2027-01-29)
- **Number of Sprints**: 5 two-week sprints (Sprints 8-12, weeks 15-24) plus a one-week playtest-and-report window (week 25). If M1 closes late, M2 shifts by the same amount; the 24-week MVP total is the anchor, not the sprint count.

## Milestone Goal

Complete all 11 MVP items on the production architecture (the Feature and Presentation epics on top of M1's Foundation and Core), run the full loop (lobby-less direct join, 10-player round, objective chain on the seeded city, strikes, informants, pings, end-of-round summary) with humans, document at least one playtest session, and write the Vertical Slice report that answers whether the full loop is achievable and fun enough to commit to Production.

At the end of M2 we can demonstrate: a 10-player round played by humans on a seeded procedural city, with the Meshy-swapped character pool, every MVP item working, a playtest session report with the D38 measurement carried over from M1 and the first read on the Vertical Slice hypothesis, and a REPORT.md with a PROCEED / PIVOT / KILL / NOT ASSESSED recommendation plus the day-by-day velocity log.

## Success Criteria

The milestone is complete ONLY when all of these are met.

- [ ] **M1 closed:** every M1 success criterion met or explicitly carried here with a logged reason, including the D38 PROCEED/PIVOT verdict. A D38 verdict of KILL-or-rethink blocks M2 until the creative-director review decides.
- [ ] **All 11 MVP items shipped on the production architecture** (`design/gdd/game-concept.md` MVP Definition): (1) networked round framework; (2) motion-matched locomotion with the small traversal set, first/third-person toggle, replicated; (3) shared character pool with 50 NPCs and teammate-only markers, Meshy pool swapped in; (4) assassination, prepared-defence reversal, NPC-kill removal, informants; (5) one objective chain; (6) seeded modular city layout from a small kit, identical on all clients; (7) bush concealment per viewer; (8) informant ping; (9) text-only end-of-round summary; (10) teammate-reconnecting indicator; (11) keyboard walk-slower.
- [ ] **Epics landed (all stories Complete or deferred with a logged reason):** Character Pool & Appearance (Meshy swap), Crowd & NPC Behaviour, Concealment (Bushes), Assassination & Defence Reversal, NPC-Kill Penalty & Informants, Objectives Chain, Procedural City Generator, Informant Pings & Team Communication, Teammate Markers, HUD, plus any M1 carryover (Round, Level Socket Contract, Camera, SCL traversal stories 046-055).
- [ ] **Human playtest session documented:** at least 1 documented session with a REPORT.md showing PROCEED (hard requirement of the Production gate), via `/playtest-report`; 3 or more sessions recommended. At least one session is a 10-player round with human players over a real network (LAN or internet), not in-process PIE.
- [ ] **Vertical Slice report written:** `prototypes/smoking-mirror-vertical-slice/REPORT.md` (concept slug "smoking-mirror" for the working title "Smoking Mirror"), with its row in `prototypes/index.md`, produced by `/vertical-slice`: validation question, what was built, observations, velocity log from actual days, lessons learned, recommendation. The creative-director CD-PLAYTEST review (review mode full) is recorded in the report.
- [ ] **Vertical Slice hypothesis evaluated** (concept MVP Definition): at the 10-player test, >= 70 % of >= 20 rounds end by objective with median duration near Objectives Chain F1's ~9 min (a median under 6 min triggers AC-OC-19 pacing levers), `ObjectiveEndShare` reported per team-size bucket (only 4v4 and above count for Pillar 4), plus a post-session survey on tension and wanting another round. The exact thresholds are fixed in the test plan before the first counted session. If 20 rounds are not reached, the report states the count and the verdict for this criterion is NOT ASSESSED, not PASS.
- [ ] **Pillar 1 parity gates green on the full build:** leak-parity diff (opponent-as-host run included) shows no identity tell, with the Pings carrier covered (AC-PNG-01, -02, -03 BLOCKING), bush culling covered (IVP S9/S10/S12 conditions met), and the Tell Matrix rows verified.
- [ ] **Performance within the ADR-0022 budgets on 3060-class hardware** with 50 NPCs plus 10 players (ASSUMED figures replaced by measured ones; captures R1-R9, RC-1, RC-2 recorded). Without a 3060-class capture the criterion is NOT ASSESSED.
- [ ] **Accessibility tier Standard (plus the committed elevations)** (`design/accessibility-requirements.md`) verified for the screens touched: HUD, main menu, pause menu, end-of-round summary; retained screenshots in `production/qa/evidence/`.
- [ ] **UX specs approved for HUD, interaction patterns, main menu, pause menu** (the `production/design-closeout/ux-review-*.md` items closed) and AC-PNG-10 (blind sort) run by human reviewers at the first playtest, moving from ADVISORY back to BLOCKING (D36).
- [ ] **D40 cultural-framing desk check done before any art-commit story** (the Meshy swap and the Body_Base character); the human sensitivity read is scheduled (it is a named exit gate of the later Vertical Slice scope tier).
- [ ] All S1 and S2 bugs resolved
- [ ] Performance within budget on target hardware (see Quality Gates)
- [ ] Build stable for 10 consecutive working days (per-commit gate and nightly green, no unexplained flake in the leak-parity suite)
- [ ] `/gate-check production` run on the result, and its verdict (PASS / CONCERNS / FAIL / NOT ASSESSED) recorded in `production/gate-checks/`.

## Feature List

Sprint targets are indicative (no velocity yet) and assume M1 closes at week 14.

### Must Ship (Milestone Fails Without These)

| Feature | Design Doc | Owner | Sprint Target | Status |
|---------|-----------|-------|--------------|--------|
| NPC-Kill Penalty & Informants (if not done in M1) | `design/gdd/npc-kill-penalty-informants.md` | gameplay-programmer | 8 | Not Started (M1 Should) |
| Concealment (Bushes), per viewer (if not done in M1) | `design/gdd/concealment-bushes.md` | gameplay-programmer, ue-replication-specialist | 8-9 | Not Started (M1 Should) |
| Teammate Markers (clamp, labels, `OnAllyMarkerRetired`) | `design/gdd/teammate-markers.md` | ui-programmer | 8-9 | Stories not yet created |
| HUD (L0 layer, F8 feed queue, F10 banner timing, canvas-drawn prototype HUD replaced) | `design/gdd/hud.md` | ue-umg-specialist, ui-programmer | 8-10 | Stories not yet created |
| Informant ping (MVP item 8; carrier, validation, point rule, L0 presenter, 21 ACs) | `design/gdd/informant-pings-team-comms.md` | network-programmer, ui-programmer | 9-10 (after HUD L0, Markers clamp, Camera aim ray) | Stories not yet created; about 7.5 planning days |
| Text-only end-of-round summary (MVP item 9; `FRoundResult.TeamSummary`, HUD-E21) | `design/gdd/round-match-framework.md`, `design/gdd/hud.md` | gameplay-programmer | 10 (after Round Event Log and HUD banner) | Not Started; about 3 planning days |
| Teammate-reconnecting indicator (MVP item 10; Roster `bReconnecting`, HUD feed, marker suffix, AC-HUD-28 a-h) | `design/gdd/participant-state-roster.md`, `design/gdd/hud.md`, `design/gdd/teammate-markers.md` | network-programmer | 10 (after Session grace and HUD feed) | Not Started; about 5 planning days |
| Keyboard walk-slower (MVP item 11; `IA_WalkSlower`, crowd pace rule) | `design/gdd/input-interaction-framework.md` | gameplay-programmer | 8 (cheapest; may pull into M1) | Not Started; about 1.5 planning days |
| Procedural City Generator (item 6; built last) | `design/gdd/procedural-city-generator.md` | gameplay-programmer, technical-artist | 9-11 | Stories not yet created; about 8.5 planning days |
| Character Pool: Meshy swap (item 3) once one Meshy character retargets end to end | `design/gdd/character-pool-appearance.md` | technical-artist | 10-11 | Not Started; prototype Meshy citizens fail the uniformity contract and are not carried forward |
| Traversal replication and camera toggle verified under net emulation (SCL 046-055, camera stories) if not complete in M1 | `design/gdd/shared-character-locomotion.md`, `design/gdd/camera-system.md` | ue-replication-specialist | 8-9 | Not Started |
| 10-player human playtest session(s), `/playtest-report`, `/vertical-slice` REPORT | this milestone | producer, qa-lead | 12 and week 25 | Not Started |

### Should Ship (Planned but Cuttable)

| Feature | Design Doc | Owner | Sprint Target | Cut Impact | Status |
|---------|-----------|-------|--------------|-----------|--------|
| Three or more playtest sessions (instead of the 1-session minimum) | `.claude/skills/playtest-report` | producer | 12 | Less reliable PROCEED signal (the gate accepts 1 session) | Not Started |
| >= 20 rounds for the Vertical Slice hypothesis | `design/gdd/game-concept.md` | producer | 12 | Hypothesis verdict is NOT ASSESSED | Not Started |
| Human sensitivity read of the Mexica framing (D40 follow-through) | `production/design-closeout/2026-10-25_rereview-followups.md` D40 | art-director, narrative-director | 11 | Becomes a hard exit gate of the later Vertical Slice scope tier | Not Started |
| Soak test (extended 10-player play for slow leaks) via `/soak-test` | `.claude/skills/soak-test` | qa-lead | 12 | Slow leaks surface later | Not Started |
| OI-1/OI-10 test character, OI-2 bush value band, OI-4/OI-6 graybox checks (art bible open items) | `design/art/art-bible.md` | art-director, technical-artist | 8-10 | Visual measurements missing at the Production gate | Not Started |

### Stretch Goals (Only if Ahead of Schedule)

| Feature | Design Doc | Owner | Value Add |
|---------|-----------|-------|----------|
| Lobby UI (direct join stays the MVP path) | `design/gdd/systems-index.md` row 19 (no GDD yet) | ux-designer | Smoother playtest onboarding; this is Vertical Slice scope-tier work |
| Post-Round Reveal (the "one more round" hook) | `design/gdd/systems-index.md` row 21 (no GDD yet) | ux-designer | Improves the survey read on wanting another round; scope-tier work |
| Second district or a third objective variant | `design/gdd/objectives-chain.md` | level-designer | Not needed to validate the loop; scope-tier work |

## Quality Gates

| Gate | Threshold | Measurement Method |
|------|-----------|-------------------|
| Crash rate | 0 crashes in any retained 10-player round; < 1 per 10 hours of soak | Harness result JSON, Gauntlet logs, `/soak-test` |
| Frame rate | >= 60 FPS (16.67 ms) with 50 NPCs plus 10 players at the reference preset; CPU game thread <= 12.0 ms p95, render/RHI <= 12.0 ms p95, GPU 14.4 ms with >= 2.0 ms headroom (ADR-0022 s4); texture pool <= 1000 MB; non-Nanite draw calls <= 1500 | Unreal Insights / `stat unit` on 3060-class hardware (`/perf-profile`); NOT ASSESSED without it |
| Load time | Seeded round start to `Playing` within the Session & Travel budget measured in M1 (Session story 002); city generation within the Generator's stated budget | Harness timing; generator test |
| Critical bugs | 0 open S1 | Bug tracker |
| High-severity bugs | 0 open S2 in delivered features | Bug tracker (`/bug-triage`) |
| Test coverage | Every Logic and Integration story has a passing test; all BLOCKING leak/parity ACs pass (Pings 21 ACs, AC-HUD-27, AC-HUD-28 a-h, Round AC 12e, AC-PSR-53, AC-SCL-36); Visual/UI stories each have a retained screenshot | Nightly suites, `/smoke-check`, `/team-qa`, `production/qa/evidence/` |
| Seeded layout identity | City layout byte-identical on host and every client for the same seed | Procedural City Generator tests (determinism) |
| Playtest evidence | >= 1 documented session, REPORT.md verdict PROCEED | `/playtest-report`, `/vertical-slice` |

## Risk Register

Includes the risks the gate reports accepted. "Accepted" means accepted under the user's standing gate policy, not retired. M1 risks that persist are not repeated unless they change.

| Risk | Probability | Impact | Mitigation | Owner | Status |
|------|------------|--------|-----------|-------|--------|
| MVP total is low confidence: 24 weeks expected, 18.5 optimistic, 36 pessimistic; the Vertical Slice and Alpha increments were not re-estimated (rebaseline) | High | Date moves by months | Re-baseline after Sprint 1 and at M1 close; re-estimate M2 from measured velocity at M1 close | producer | Open |
| Items 8-11 depend on carriers built late (C-TeamView, HUD L0, Markers F3 clamp, `OnAllyMarkerRetired`, Session grace): +4 days on the ping if not ready | Medium | +4 days to +1 week | Schedule items 8 and 10 after HUD and Markers (as ordered above) | producer | Open |
| Reconnect indicator needs a PIE disconnect/rejoin/supersede/kick harness (AC-HUD-28 a-h) | Medium | +1 to +2 weeks on item 10 | Session story 001 (the harness hook) is in Sprint 1 as a pulled-in prerequisite (stretch) | network-programmer | Open |
| AC-PNG-10 blind-sort needs 3 reviewers who did not author the UI; a solo developer cannot supply them | High | Cannot close item 8 as written | D36: three fresh-context agents as pre-screen, criterion ADVISORY until human reviewers at the first playtest, then BLOCKING | qa-lead | Ruled (D36); open until the playtest |
| Pings could expose Pillar 1 leaks through the controller carrier (accepted risk, Pillar 1 identity leak) | Low | High | AC-PNG-01/-02/-03 BLOCKING with the opponent-as-host run | network-programmer | Accepted; Open |
| Meshy character pipeline: arms 17 % short, uniformity contract failures; prototype citizens not production-grade (gate: technical setup, accepted) | Medium | Meshy swap slips; mannequin pool stays | Swap only after one Meshy character retargets end to end; keep the mannequin variants as the fallback; the swap is behind D40 | technical-artist | Accepted; Open |
| No visual measurements yet (OI-1/OI-10 test character, OI-2 bush band) (gate: technical setup, accepted) | Medium | Art-bible numbers fail on first measurement | Schedule OI-1/OI-10, OI-2, OI-4/OI-6 early in M2 (Should Ship above) | art-director | Accepted; Open |
| Procedural generator is built last and its spikes are provisional on results (GDDs not held for the three spikes) | Medium | City generation slips; graybox map remains | Keep the hand-built graybox map as the playtest fallback; seeded layout identity test early | gameplay-programmer | Open |
| 10 human players cannot be recruited or hosted; the first 10-player test would be the first time 10 real clients connect | High | Gate evidence missing | Schedule recruiting at M2 week 4; M1's Gauntlet and LAN runs reduce the technical risk; smaller team sizes still give a PROCEED/PIVOT read | producer | Open |
| Cultural-framing review is desk-only before the Meshy swap; a human sensitivity read is not scheduled (D40) | Medium | Rework of costumes, names or text | Human read scheduled in Sprint 11 (Should Ship) before any public build | narrative-director | Open |
| PROCEED/PIVOT result is PIVOT or KILL at M1 or M2 | Medium | Plan changes; Vertical Slice cannot proceed | M2 does not start Feature work that the PIVOT would invalidate (tells, crowd density) until the M1 verdict is in; PIVOT note per `/vertical-slice` | creative-director | Open |
| Estimate for the later Vertical Slice and Alpha tiers was set against the old MVP pace and is probably understated (rebaseline) | High | Post-M2 plan too optimistic | Re-estimate those tiers at the M2 milestone review | producer | Open |
| Single machine: serial UE builds and PIE; asset-resave hazards | High | Throughput capped | Batch stories (D41); never-resave checks; snapshots before sample-asset edits | producer | Open |

## Dependencies

### Internal Dependencies

| Feature | Depends On | Owner of Dependency | Status |
|---------|-----------|-------------------|--------|
| Everything in M2 | M1 closed: Foundation and Core epics landed, 10-player round on the production architecture | producer | Planned (target 2027-01-29) |
| Informant ping | HUD L0 layer, Teammate Markers clamp and `OnAllyMarkerRetired`, Camera `GetAimRay`, Concealment `IsInBushOrTransition`, Roster C-TeamView carrier, ADR amendments (0001 carrier, 0011 input and RPC, 0021 L0 slot) | ui-programmer, network-programmer | Not Started; confirm the amendments are applied |
| End-of-round summary | Round Event Log and `FRoundResult`, HUD banner timing (F10) | gameplay-programmer | Not Started |
| Reconnect indicator | Session grace and rejoin (`NotifyConnectionChanged`), Roster `bReconnecting`, HUD F8 feed queue | network-programmer | Not Started |
| Walk-slower | IIF `IMC_Common`, SCL pace quantisation (R5.1), Crowd pace set, D37 (remove the sample Ctrl jog toggle) | gameplay-programmer | Not Started |
| Meshy swap | Mannequin pool working, one Meshy character retargeted end to end, D40 desk check | technical-artist | Not Started |
| Procedural City Generator | Level Socket Contract and kit, Round seed domain (ADR-0006, MatchId row D46), TDC pipeline | gameplay-programmer | Not Started |
| Vertical Slice report | A playable 10-player build, test plan with exact thresholds, recruited players | producer, qa-lead | Not Started |

### External Dependencies

| Dependency | Provider | Status | Risk if Delayed |
|-----------|---------|--------|----------------|
| Human playtesters (10 for a full round; recruiting plan owner and date TBD) | The developer | Not recruited | No PROCEED evidence; the Production gate fails |
| 3060-class reference hardware | The developer | Unconfirmed | Performance gates NOT ASSESSED |
| Meshy account credits and API for character generation | Meshy | Pipeline proven in the prototype; credit cost needs user confirmation per generation | Pool swap stalls |
| Voice chat for playtests (external team voice is a stated MVP playtest requirement; no in-game voice) | The playtesters | Not arranged | Playtest conditions differ from design assumptions |
| Human sensitivity read of the Mexica framing | A cultural reviewer to be named by the user | Not arranged | Becomes a hard gate of the later Vertical Slice tier |
| UE 5.8 hotfixes (5.8.4 installed) | Epic | Ongoing | Revalidate [U] items after any engine upgrade |

## Review Schedule

| Date | Review Type | Attendees |
|------|-----------|-----------|
| 2027-02-19 (M2 week 3) | Early progress check: M1 verdict applied, Feature epic stories created | Producer, Directors |
| 2027-03-05 (week 19, mid-milestone) | Mid-milestone review (`/milestone-review`): items 8-11 status, generator status, playtest recruiting confirmed | Full agent team, user |
| 2027-04-02 (week 23) | Pre-milestone review: playtest build frozen, test plan thresholds fixed | Full agent team, user |
| 2027-04-16 | Milestone review: REPORT.md verdict, CD-PLAYTEST review, `/gate-check production` | Full agent team, user |
