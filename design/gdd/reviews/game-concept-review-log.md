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
