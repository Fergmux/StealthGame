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

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (unchanged: multi-system integration, 9 formulas plus a statistical gate, ADRs for pawn replication parity and the stream primitive; producer should verify before sprint planning)
Specialists: none (lean, single session)
Blocking items: 4 | Recommended: 5
Summary: Second-round review. All 26 round-1 blockers are confirmed resolved in the text (knob names and TDC I-3/I-5, single Session acknowledgement, server-private streams, adoption-only informants, pawn-parity rules, no late join, Preparing disconnect, own-pawn exclusion, swatch floor by uniform valid-subset draw, T7 and calibration corrections, uniform replacement draw, goodness-of-fit AC-CP-12). Recomputed and confirmed: F-5 hypergeometric tables (D=60 and D=50), twin/shared-look/teammate-pair figures, F-8 T1 to T7 null expectations and the 0.8 / 4.7 / 0.15 percent calibration figures, 32 of 64 valid MVP subsets at K=6, F-2/F-3 examples and every AC-CP-01 triple. New blockers are cross-document drift introduced by the same-day sibling revisions and the 2026-10-25 user decisions, plus two untestable or inconsistent ACs. All four were revised in this session; they need an independent re-check, so the verdict stays NEEDS REVISION.
Prior verdict resolved: Yes (26 of 26 blocking, 20 of 20 recommended verified present)
Findings:
- [BLOCKING] Core Rules CP-12b / States I-C5 / Edge Cases / AC-CP-12 / AC-CP-14 / OQ-14: the candidate set was described as "all unobserved crowd pawns" and the likelihood-ratio-1 claim as pawn-level; Penalty PEN-R7 and LSC R5b (user-confirmed 2026-10-25) restrict the pick to unseen pawns inside the team's respawn region. Revised: eligible set defined, claims narrowed to look level, AC-CP-12 now asserts uniformity over the eligible set with appearance-free predicates, pawn-level region effect handed to Penalty and LSC (OQ-14) [main]
- [BLOCKING] Edge Cases (replacement backlog) and CP-12a: CPA invented a Crowd "backlog exceeds 3" rule and a `VacancyBacklog` log; Crowd CNB-R9.2 has neither (first scheduled entry at or after kill + 10 s, served by 60 s). Revised to match Crowd; OQ-10 closed [main]
- [BLOCKING] Acceptance Criteria AC-CP-17: expected mean 255 and variance 10 * 208.25 * 40/49 hold only at D=50 but the test named no configuration and the shipping default is D=60. Revised to state the default (mean 305, variance 2541.7) and the F-8 reference values [main]
- [BLOCKING] Acceptance Criteria AC-CP-32 vs F-9: AC capped the property at 150 B per client while F-9 and the Replication section give up to 180 B at D=60. Revised to 180 B [main]
- [RECOMMENDED] Formulas F-8: "checked-in file" for the seed set contradicted the AC preamble (constexpr array, no file I/O); aligned, ADR-0008 to follow [main]
- [RECOMMENDED] Formulas F-8: T2 to T6 at D=60 need recomputed null expectations; stated [main]
- [RECOMMENDED] Acceptance Criteria AC-CP-24a: a 65 percent lower bound at 60 trials needs 47 of 60 (78.3 percent), so the 75 percent point estimate was not the binding rule; stated, per-tester clustering noted [main]
- [RECOMMENDED] Core Rules CP-13c: "effective NPC count falls by 1" read as contradicting conservation after a kill-driven adoption; clarified [main]
- [RECOMMENDED] Replication: "60 to 180 bytes" mixed payload-only and with-handles; aligned with F-9 (120 to 180) [main]
Cross-doc follow-ups (main session):
- NPC-Kill Penalty & Informants and Level Socket Contract: own the pawn-level Pillar-1 argument for the respawn-region candidate set (CPA OQ-14); CPA now claims look-level independence only.
- systems-index.md: Character Pool & Appearance stays Needs Revision until an independent re-check of the four revised items (index not edited here).
- ADR-0008 (test harness) should align with the constexpr seed array (no seed file); ADR-0012 already follows this GDD.
- Crowd GDD: no change needed (CNB-R9.2 is the authority; CPA now cites it).
Revisions: all four blocking and five recommended items revised in this session. The content hash of design/gdd/character-pool-appearance.md is deliberately not stamped below (it changed after review), so the next check reads NEW and a re-review runs.
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 63db04c802c6db06eb0ba3dac93f65fb9fa98068
Reviewed-Content-Hash: design/gdd/session-travel.md 49b874ce7cbf748247815232fc587337df2e82e2
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/round-match-framework.md ab477269caa8bacab048c8719d3029a90a7ce89e
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
Reviewed-Content-Hash: design/gdd/systems-index.md b195f89efa215d57e7630933f75bbbc02bdd9ba8
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 88399334ca7f9623f4f828a7ba51075673f039f6
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
