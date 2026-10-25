# Review Log: Character Pool & Appearance (design/gdd/character-pool-appearance.md)

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system integration with Round, Session, Roster, Crowd and Shared Character; 9 formulas plus a statistical gate; ADRs needed for pawn replication parity and the stream primitive; producer should verify before sprint planning)
Specialists: systems-designer, qa-lead, ue-replication-specialist (economical roster per orchestrator instruction after a usage-limit reset); senior: creative-director
Blocking items: 26 | Recommended: 20
Summary: The core design holds: a balanced, shuffled, team-blind, conserved deck plus the CP-12b look re-roll delivers Pillar 1, and every statistic in F-5/F-8 recomputed correctly. The blockers were contradictions with sibling GDDs written in parallel (a second pool acknowledgement and private entropy against Session & Travel; a fresh informant spawn against Roster R9 crowd adoption; late join against Session/TDC; the Preparing-disconnect case against Round P6; key names and I-3/I-5 against TDC), statistical edge cases (swatch re-draw failing 0.39 percent of small rounds, T7 null, an AC-CP-12 test that fails a correct allocator 35-40 percent of the time) and 11 untestable acceptance criteria. The creative-director ruled the informant-look correlation is not a leak (pawn-level likelihood ratio 1) and made crowd adoption the only materialisation path.
Prior verdict resolved: First review
Findings:
- [BLOCKING] Tuning Knobs: `DeckMinCopiesCount` breaks TDC CR-15 (unpaired Min); CPA "I-5" collides with TDC I-5; `AppearancePalette` ownership unacknowledged [main, systems-designer]
- [BLOCKING] Core Rules CP-18: pool-residency acknowledgement and `PoolResidentTimeoutSec` duplicate Session rule 11.6 and `TravelLoadTimeoutSec` [main, ue-replication-specialist]
- [BLOCKING] Core Rules CP-9: private entropy conflicts with Session rule 14/F5; PCG32 shared across observable draws is invertible [main, ue-replication-specialist]
- [BLOCKING] Core Rules CP-12b/CP-13c: retire-and-spawn informant pawn at an emergence socket contradicts Roster R9 and is a wire-visible destroy/create pair [main, ue-replication-specialist, creative-director]
- [BLOCKING] Core Rules CP-10 / Replication: possession flips stock `PlayerState` replication on non-owners; one-factory claim untrue; no RemoteViewPitch/class/subobject parity rule [ue-replication-specialist]
- [BLOCKING] Core Rules CP-13c: late join contradicts Session RejoinOnly and TDC CR-8 [main]
- [BLOCKING] Edge Cases: disconnect during Preparing contradicts Round P6 (bound AI-driven pawn) [main, ue-replication-specialist]
- [BLOCKING] Tuning Knobs: I-3 `>= 24` breaks TDC NpcCount min 6, AC-17 and Solo, and makes CPA's dev path unreachable [main, systems-designer]
- [BLOCKING] Core Rules CP-12b: strike removal could adopt the participant's own released pawn [systems-designer]
- [BLOCKING] Interactions/Edge Cases: informant emergence at team TeamSpawn and two emergences per participant kill [systems-designer]
- [BLOCKING] Formulas F-4: swatch re-draw cap fails 1/256 rounds at the MVP pool K=6; K<=4 can never reach 5 swatches [systems-designer]
- [BLOCKING] Formulas F-8: T7 null distribution wrong (needs (K-1)/(K-r)); calibration threshold <=2 fails a correct harness ~4.7 percent [systems-designer]
- [BLOCKING] Edge Cases: backlog "oldest first" contradicts the uniform replacement draw [systems-designer]
- [BLOCKING] Acceptance Criteria AC-CP-12: independence chi-square fails a correct allocator on ~35-40 percent of seeds [systems-designer, qa-lead]
- [BLOCKING] Acceptance Criteria AC-CP-17: NetGUIDs are assigned at first export in priority order; test measures the wrong thing [ue-replication-specialist]
- [BLOCKING] Acceptance Criteria: seed file path and file I/O; non-deterministic smoke set; Shipping checks impossible in automation; AC-01/08 dev K<6 path; AC-06 non-deterministic; AC-10 check() untestable; AC-14 late join; AC-19 timeout; AC-18 pixel diff 0; AC-13/16/20/26/27 not marked blocked; AC-27 label (11 items) [qa-lead]
- [RECOMMENDED] Formulas F-1 to F-3: range errors, unreachable `Error_DeckTooSmall`, K=A ignores the cap [systems-designer]
- [RECOMMENDED] Formulas F-8: T3b without-replacement inflation; AC-CP-17 finite-population correction [systems-designer, qa-lead]
- [RECOMMENDED] Core Rules CP-12: `DrawReplacement` before spawn leaves a pawnless `NpcLive` card; simultaneous adoptions [systems-designer]
- [RECOMMENDED] Tuning Knobs: CR-15.8 tell review per key [systems-designer]
- [RECOMMENDED] CP-18: residency is not first-draw readiness (warm-up of all A entries); PostNetInit check; BeginPlay independence; dormancy [ue-replication-specialist]
- [RECOMMENDED] Replication: bandwidth is marginal only (~18-30 KB/s for 50 pawns at the player rate); listen host can enumerate PlayerControllers; archetype vs actor name [ue-replication-specialist]
- [RECOMMENDED] Acceptance Criteria: test-name prefixes and procedure IDs; split bundled ACs (08, 11, 15, 20); AC-02 inclusion law; nightly calibration; AC-07; G-VAR and T5 procedures; coverage gaps CP-5, CP-6, CP-8, CP-10, CP-13b/d, CP-15, CP-17, F-9 [qa-lead]
Specialist disagreements (adjudicated by creative-director): main review suspected the informant's new look leaked through a look-level correlation (P(same look) ~0.061 vs ~0.082); systems-designer showed the dip is the forced-NPC replacement, identical after an NPC kill (pawn-level likelihood ratio 1) -> no leak, claims rewritten as "per-pawn uniform except the forced-NPC replacement". Materialisation (adoption vs retire-and-spawn) -> adoption only. QA read AC-CP-01 (2,10) as contradicting F-2; resolved by an explicit Enforce/Warn floor policy (AC-CP-01b).
Revisions: applied by the reviewer in the same session (autonomous pipeline): all 26 blocking and 20 recommended items; independent re-review required. Content hash below is of the document as reviewed (before revisions), so a re-review reads CHANGED.
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 6c09a02eb045d68fab387ad4e614d0e2e450fd3d
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/session-travel.md 5a2f3d67fb9453d71eb9602429781de31d79777c
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3
Reviewed-Content-Hash: design/gdd/round-match-framework.md 0a20ed3e6deebf0dcc36a11ec171861efbd91583
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
Reviewed-Content-Hash: design/gdd/systems-index.md 2adb098db430a1b7d1c77e1238171c3948bf08eb
