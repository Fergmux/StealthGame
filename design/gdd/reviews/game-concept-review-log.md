# Review Log: game-concept.md

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: XL (cross-cutting foundation document; 18 downstream GDDs, 23 ADRs. Rough scope signal, producer should verify.)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist; senior reviewer creative-director
Blocking items: 0 hard blockers after the creative-director ruling (specialists raised 10 BLOCKING; the director downgraded them to revisions that change text only) | Recommended: 17
Summary: The concept is a sound foundation: pillars, confirmed rules and downstream GDDs are coherent, and no user-confirmed rule is contradicted. It had drifted from the project: the NPC count (40 to 50), the perf budgets (superseded by ADR-0022), the hidden final objective, session arithmetic, and an unfalsifiable MVP hypothesis. The creative director ruled NEEDS REVISION, all fixable by text edits. Revisions were applied in the same run; an independent re-review in a fresh session is still required before Approved.
Prior verdict resolved: First review
Review standard: game-concept.md is a concept document, not a system GDD. The 8-section GDD checklist (gdd-structure-check.sh reports 0 of 8 present) does not apply and was not scored; the review used the concept standard (pillars, MDA, loop, scope, feasibility, testable validation). Logged via log_decision.
Registry values: grep of the concept against design/registry/entities.yaml and tuning-data-config.md found one conflict (NpcCount 40 in the concept vs 50 registry/TDC, 3 lines plus 4 character-count lines); fixed.
Dependency graph: the concept has no Dependencies section; all systems named in design/gdd/systems-index.md have GDD files on disk (18 GDDs).
Specialist disagreements: none between specialists; the creative director declined to act on QA's detailed test protocols (belong in the test plan), any match-end rule or timer (confirmed no time limit), informant changes, Fellowship demotion, and ADR-number pointers in the concept.
Findings:
- [BLOCKING-as-raised, applied] Core Mechanics/MVP/Risks: NpcCount still ~40 (user changed to 50 on 2026-10-25); character count now 60 [systems-designer, unreal-specialist]
- [BLOCKING-as-raised, applied] Core Identity/Core Loop: 3-5 rounds x ~12 min != 45-60 min; "30-120 minutes" header; no match-end rule exists [systems-designer, creative-director]
- [BLOCKING-as-raised, applied] MVP: hypothesis "tense and fun" unfalsifiable; split into an MVP (reading beats chance) and a Vertical Slice (5v5 race) hypothesis; "2-client verified" reworded to mechanics verified [game-designer, qa-lead, systems-designer]
- [BLOCKING-as-raised, applied] Technical Considerations: perf budget superseded by ADR-0022 (listen-host GT 12.0 ms p95, 3060-class assumed, ~0.8 ms headroom at 50 characters on the 5090) [unreal-specialist]
- [BLOCKING-as-raised, applied] Technical Risks: listen-host secrecy (host holds ServerOnly state) was missing [unreal-specialist]
- [BLOCKING-as-raised, deferred] Pillar 1: no verification path for parity; one-line gate recorded under Review Follow-ups 3, detail belongs in the test plan [qa-lead]
- [BLOCKING-as-raised, deferred] Pillar 4 validation "most playtest rounds": measurable form added to the VS hypothesis; pillar text untouched (user-confirmed) [qa-lead]
- [RECOMMENDED, applied] Core Loop: "both teams converge ... contact is unavoidable" contradicted the hidden final (Objectives Chain SP-1) [creative-director via cross-review D-W3f]
- [RECOMMENDED, applied] Key Dynamics: final-site watching/camping presented as intended play [game-designer]
- [RECOMMENDED, applied] Core Mechanics: added the tell inventory pointer to CNB-R16 (the "head one beat too late" example is backed by T13) [game-designer]
- [RECOMMENDED, applied] Core Loop: post-round reveal is a Vertical Slice item, not MVP [game-designer]
- [RECOMMENDED, applied] Networking/Seed/Mover/MotionWarping: legacy NetDriver with Iris off, public geometry seed only, nav build lock, plugins disabled, Motion Warping Beta [unreal-specialist]
- [RECOMMENDED, applied] Open Questions: resolved the hidden-final question; added NPC bush use and minimum team size; Scope Tiers deltas versus cumulative totals [game-designer, systems-designer]
- [RECOMMENDED, applied] Next Steps checklist brought up to date (setup-engine, art-bible, map-systems, design-system, design-review)
- [RECOMMENDED, deferred] Pillar 3 "every mistake sticks" overclaims; confirmed pillar text, left for the user [game-designer, systems-designer]
- [RECOMMENDED, deferred] Fellowship ranked #2 with thin MVP delivery; informant agency; pitch sells a hunt; measurable rewrites of visual tests and latency claims (art bible and test plan) [game-designer, qa-lead]
- [NICE, not applied] Blind-guess odds, NpcKillStrikeLimit range and pool numbers belong to downstream GDDs [systems-designer]
Senior verdict [creative-director]: NEEDS REVISION; sound foundation, no design or pillar change required; endorse nine text revisions (applied except the Pillar 3 wording and the post-MVP items noted above).
Note: the stamped hashes below are of the REVISED concept (revisions were applied after the specialists read it); the verdict applies to the pre-revision text, so a re-review must still run in full.
Reviewed-Content-Hash: design/gdd/game-concept.md f151e88c358acddbf5b6378bb528ad89ecdad297
Reviewed-Content-Hash: design/registry/entities.yaml 42ae044c911d2cf4c07781c5e657160895a6e4c0
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md a4e4f1f3d0cd05517e898e0caf158e438d6244c7
Reviewed-Content-Hash: design/gdd/objectives-chain.md 97f7640460f661ff7b7c2b4241a31d26ed6de168
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 03781c1be07f58166bdefb418a0dea31a992aa9a
Reviewed-Content-Hash: design/gdd/gdd-cross-review-2026-10-24.md 883fe8df5607c356f672029991ef4ac2214858dc
Reviewed-Content-Hash: docs/architecture/adr-0022-rendering-performance-budgets.md c90801c9524c12cc3459b32ce823f811c8fbcbb3
Reviewed-Content-Hash: docs/architecture/adr-0014-layout-service-generation-navmesh.md ad20cc195c72eb51ad28a102236ee2c378056203
Reviewed-Content-Hash: docs/architecture/adr-0006-seeds-random-streams.md 6132980d0c0b5bed4f47e14a96e5cd2680e45293

## Review — 2026-10-25 — Verdict: APPROVED
Review mode: lean · Workflow: full
Scope signal: XL (unchanged; cross-cutting foundation document. Rough scope signal, producer should verify.)
Specialists: none (lean mode, single-session analysis)
Blocking items: 0 | Recommended: 3
Summary: Second-round review of the revised concept. Every first-round revision was checked against the live registry, Tuning Data, Crowd, Objectives Chain, Penalty, HUD, Assassination GDDs and ADR-0001/0010/0022 and holds; no user-confirmed rule or pillar wording is contradicted. Five stale or missing text-sync items were found and fixed in place (MVP estimate and item count, session-length header, rival-signal omission, ADR pointer, NPC bush flag). Remaining open items are downstream (test plan, art bible, producer re-estimate) or user creative calls, none of which blocks the concept.
Prior verdict resolved: Yes (first-round NEEDS REVISION: all applied revisions verified present and consistent; deferred items remain recorded under Review Follow-ups, with follow-up 3 now pointing at ADR-0008 and ADR-0023)
Review standard: concept document, not a system GDD. The 8-section checklist (gdd-structure-check.sh reports 0 of 8 present) was not scored, as in the first round; the concept standard (pillars, MDA, loop, scope, feasibility, testable validation) was used. The Approved verdict applies to the text as stamped below: the second-round edits were text-sync only and were made before the hash was taken.
Registry values: searched NpcCount, TeamSize, TargetRoundMinutes, defence and strike terms; the one line giving numbers (base rate 5/55 = 0.0909) matches Tuning Data F-3; no remaining NpcCount 40 except the historical note. Registry and Tuning Data changed since round one; no conflict.
Dependency graph: no Dependencies section; all 18 system GDDs named by the systems index exist on disk. Cross-references cited in the concept resolve (CNB-R16, T5/T13, SP-1, OC-12, HUD-R14, PEN-R15, ADR-0001, 0003, 0006, 0007, 0010, 0014, 0022).
Findings:
- [FIXED] MVP/Scope Risks/Scope Tiers: estimate still read 8-10 weeks for "7 items" (producer note in the systems index says 10-12 weeks) and the user-added items 8-11 were not reflected; text now says 10-12 weeks for the 7 core items and that items 8-11 are unestimated
- [FIXED] Core Loop: Session-Level header said 40-60 minutes while the body (3-5 rounds x 12 min) gives 36-60
- [FIXED] Core Mechanics 4: user-confirmed rival-progress signal (OC-12) was absent from the concept; one sentence added
- [FIXED] Technical Considerations: "To be ADR-001" (now ADR-0001, Proposed) and the NPC bush design flag (resolved in design by T5)
- [FIXED] Review Follow-ups 3 / Next Steps: now cite ADR-0008 and ADR-0023 and the second-round result
- [RECOMMENDED] Pillar 3 wording "every mistake sticks" overclaims given the free wrong strikes; confirmed pillar text, left for the user
- [RECOMMENDED] Pillar 4 validation "most playtest rounds" is unmeasured in the pillar text; the VS hypothesis carries the measurable form; left for the user
- [RECOMMENDED] Pitch (10-second test) sells a hunt, Pillar 4 says race; revise when the pitch is next touched
- [NOTED, unchanged] Performance at 60 characters is unproven (RTX 5090 only, about 0.8 ms headroom); the concept states this plainly and keeps the NpcCount fallback explicit
Cross-doc follow-ups (main session):
- systems-index.md: set game-concept to Approved if the index tracks it; the Producer Note (line ~114) says all 7 MVP items and needs the user-added items 8-11 in the estimate
- producer: re-estimate the MVP with items 8-11 before /sprint-plan
- test plan / art bible: measurable rewrites of the Visual Identity tests and latency claims (concept Review Follow-ups 3-4)
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 242977e3dcc0185347fd1053741e6f305c1e42fa
Reviewed-Content-Hash: design/gdd/objectives-chain.md 97f7640460f661ff7b7c2b4241a31d26ed6de168
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 89d049074c81e71b16f1269736cbe2016d977f9c
Reviewed-Content-Hash: design/gdd/gdd-cross-review-2026-10-24.md 883fe8df5607c356f672029991ef4ac2214858dc
Reviewed-Content-Hash: docs/architecture/adr-0022-rendering-performance-budgets.md c90801c9524c12cc3459b32ce823f811c8fbcbb3
Reviewed-Content-Hash: docs/architecture/adr-0014-layout-service-generation-navmesh.md ad20cc195c72eb51ad28a102236ee2c378056203
Reviewed-Content-Hash: docs/architecture/adr-0006-seeds-random-streams.md 6132980d0c0b5bed4f47e14a96e5cd2680e45293
Reviewed-Content-Hash: docs/architecture/adr-0008-test-automation-leak-parity.md 4c9c47caf2e0b10ba63f10085b97e2d646c3ebea
Reviewed-Content-Hash: docs/architecture/adr-0023-playtest-telemetry-hooks.md 3480d97ec0f1dc482562e2d96bcc3c4090d5b072
Reviewed-Content-Hash: design/gdd/systems-index.md b195f89efa215d57e7630933f75bbbc02bdd9ba8

## Closeout cross-doc pass 2026-10-25
- Cross-doc alignment only (NpcCount 50, objective duration near Objectives Chain F1 ~9 min median); no design change.
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.
