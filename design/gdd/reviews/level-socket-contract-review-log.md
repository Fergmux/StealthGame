# Design Review Log: Level Socket Contract

## Review — 2026-10-24 — Verdict: MAJOR REVISION NEEDED
Review mode: full · Workflow: full
Scope signal: XL (cross-cutting contract, 10 formulas, 12+ dependents, several ADRs; producer should verify)
Specialists: level-designer, systems-designer, qa-lead, creative-director (senior verdict)
Blocking items: 14 | Recommended: 14
Summary: Informant respawn and rejoin were modelled as a new pawn emerging from an alley socket, contradicting the decided takeover rule (C1) and Roster's ban on mid-round fresh player pawns; and several validators were unsatisfiable or false at the locked ~40 NPC defaults (V-SEP vs T_contact, D6 window collapsing to a constant 8, V-DENSITY failing the typical layout, a 27-cell placement pool for 50 pawns). The creative-director judged the pillars and fantasy sound once respawn becomes an unseen takeover; 26 revisions were applied in the same session (autonomous mode) and a fresh re-review is required.
Prior verdict resolved: First review
Findings:
- [BLOCKING] R3/R5/R6/Edge/AC: respawn and rejoin as emergence of a new pawn (fixed: R5b takeover, RespawnRegion, V-RESPAWN/D6b, AC-14..18, AC-53)
- [BLOCKING] Edge Cases: disconnected pawn "leaves through AmbientExit" vs Roster/Session hand-over (fixed)
- [BLOCKING] R9.1/R5: one SpawnOrder stream shared by Round shuffle, emergence tie-break and ambient schedule; edge case named PlayerPlacement (fixed: per-domain counter-based streams; CR to Session F5)
- [BLOCKING] V-SEP: 90/120 m anchor separation + 35 m disc vs T_contact >= 15 s unsatisfiable (fixed: start-set based, StartCellMaxRadiusCm 18 m, ZoneSeparationPathCm 70 m)
- [BLOCKING] V-SEP/R11: "final candidates in a centre band" vs StartToFinalMinPathCm (fixed: band rule removed)
- [BLOCKING] D6: window collapses to exactly 8 NPCs (a constant tell); A_z feasibility unstated (fixed: d_max 1.8, q 60%, NpcZoneMax, V-ZONEFILL, uniform draw in window, NpcCount >= 35 invariant)
- [BLOCKING] V-DENSITY vs V-AREA: typical 10,500 m2 layout always warns (fixed: V-AREA upper bound derived from pawn count, Reject; examples moved to 8,800 m2)
- [BLOCKING] R6.1: placement pool too small (fixed: route-adjacent cells >= 3 m apart; start cells are player slots)
- [BLOCKING] R9.4/D8: marginal role share does not bound final inference given a team's own chain (fixed: prefix-ambiguity predicate, FinalAmbiguityMinCount)
- [BLOCKING] R5.7: rate invariant false in expectation (fixed: exit-cancel bound 25%/60 s, deficit cap 3, ambient floor 0.5/socket/min)
- [BLOCKING] States: no any-state -> Aborted, so AC-26 contradicted AC-39/48 (fixed)
- [BLOCKING] AC-32: "NpcCount 32 passes" contradicted the density cap (fixed in AC-30 D6 vectors)
- [BLOCKING] ACs: wall-clock timing, hardware-dependent AC-40, flaky AC-24/38, powerless AC-51/53/54, stub-dependent AC-15/57, invalid AC-63 chi-square (fixed: injected clock, pinned seeds, work-unit cap, revised sample sizes, stub list)
- [BLOCKING] Cross-refs: Round listed as unwritten; Session T_layout CR already applied; Generator->Round dependency in systems-index (fixed in GDD; index CR listed)
- [RECOMMENDED] D8: 33/50 gives ~1% not <= 0.8% (fixed: 34/50); 6 x 10 s attempts exceed the 45 s budget (fixed: 7 s + invariant)
- [RECOMMENDED] D3: n range wrong, 8 m stride aliases 4 m alleys (fixed: 4 m stride)
- [RECOMMENDED] D2: cap/agreement claims false above 125 m (fixed)
- [RECOMMENDED] D4: 150-300 m chain band likely unreachable (fixed provisionally: 120-240 m, set from graybox enumeration, OQ 11)
- [RECOMMENDED] R5.4: LRU socket choice predictable (fixed: uniform among clear sockets)
- [RECOMMENDED] R3/R11: EligibleRoles hint and 2 fixed graybox zones teach the map (fixed: hint removed, 2-4 candidate zones, 2 drawn per round)
- [RECOMMENDED] R3/D5/R11: court and socket counts stated three ways (fixed: D5 single source)
- [RECOMMENDED] Condensation: R10, Interactions, Edge Cases, VA-1..6, Cross-References duplicated other sections or GDDs (condensed; 1,067 -> ~810 lines)
- [RECOMMENDED] AC coverage gaps: R1/R2.7, R4.1 isolation, alley shape, W(O) algorithm, LayoutFallbackLevel 1 (added)
- [RECOMMENDED] Formula ACs restated worked examples (merged into one test-vector AC-30)
- [RECOMMENDED] AC-62 fairness playtest underpowered (fixed: >= 60 rounds, Wilson interval)
- [RECOMMENDED] AC-45 batch audit lacked an effect size (fixed)
- [RECOMMENDED] D1 mean over start cells hides a +-35 m single-cell spread (open; start radius now 18 m)
- [RECOMMENDED] NpcCount ~40 leaves little slack (pending user decision; Open Question 6)
Specialist disagreements (resolved by creative-director): respawn starvation 10 s (LD) vs 30 s (SD) -> 10 s new key; draw stream SpawnOrder (LD/QA) vs new RespawnPick (SD) -> RespawnPick; V-SEP blocking (LD) vs recommended (SD) -> blocking; D6 raise NpcCount (SD) vs loosen caps -> loosen (NpcCount locked); RoleShare per-role cap (LD) vs prefix predicate (SD) -> prefix predicate; LRU vs random -> random.
Receipt note: the target GDD was revised after this review, so its hash is intentionally not stamped; the next /design-review must run in full.
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3
Reviewed-Content-Hash: design/gdd/round-match-framework.md b710a1c57960995a95c305b58642d529c8f33c93
Reviewed-Content-Hash: design/gdd/session-travel.md 5a2f3d67fb9453d71eb9602429781de31d79777c
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 8bf1dcd1acd4e451528d64eb7491c82b2479c26e
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md a0f62833f301166c838c3a9537e3cf1309486fde
Reviewed-Content-Hash: design/gdd/systems-index.md d6cf3c42850f899304bd30a0c7b7f3b9420cc8d1

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: XL (unchanged: cross-cutting contract, 10 formulas, 12+ dependents, several ADRs; producer should verify)
Specialists: none (lean, single-session)
Blocking items: 6 | Recommended: 7
Summary: All 14 prior blocking items are resolved in the text (respawn is an unseen takeover, streams are per-domain, V-SEP/V-ZONEFILL/V-AREA are satisfiable at NpcCount 50, States has any-state Aborted, ACs are deterministic). New problems are localised, not structural: the two teams' respawn regions can overlap (anchors may be about 34 m apart while regions are 45 m), the graybox LayoutHash format cannot detect a 1 cm move, AC-6 and the AC-30 D5/D8 vectors contradict the formulas, and two keys duplicate or mis-own registry entries. 17 revisions were applied in this session; a fresh re-review is still required.
Prior verdict resolved: Yes (14 of 14 blocking items from 2026-10-24 verified fixed; 2 recommended items still open: D1 single-cell spread, NpcCount slack now answered by the 50 decision)
Findings:
- [BLOCKING] R3/R5b/V-RESPAWN/Edge Cases: Edge Case claimed regions cannot overlap ("70 m + both radii"), but V-SEP only bounds nearest start cells (anchors can be about 34 m apart), so a 45 m region can reach the enemy zone, contradicting "at their team's spawn" (fixed: RespawnRegion(zone, partner) cut by nearer-anchor rule; V-RESPAWN per usable pair; AC-14 adds RegionsDisjoint; AC-18 widening cut)
- [BLOCKING] D7/AC-3: 9-byte grid record cannot express a graybox mesh at cm precision, so "hash changes when a mesh moves 1 cm" was impossible (fixed: 40-byte graybox record; ADR-0014 §6 owns the extract)
- [BLOCKING] AC-6: "reaches R8.3 at 45 s whatever attempts remain" contradicts the invariant (6 x 7 = 42 s) (fixed: 42 s at defaults; 45 s only with an invariant-bypassing fixture)
- [BLOCKING] AC-30 D5/D8 vectors: "N_C 14 passes, 13 fails" contradicts D5 (N_C >= 16); D8 vector used the pre-reconciliation 14/4 courts; D6 vector omitted A_z (fixed)
- [BLOCKING] V-CLUE/Tuning Knobs: `ClueTagCourtsMaxCount` duplicates Objectives-owned `ClueMatchMaxCount` (registry, 3, ServerOnly) (fixed: V-CLUE reads `ClueMatchMaxCount`; LSC key dropped)
- [BLOCKING] V-POOL/Tuning Knobs: `SunPoolSamplesMinCount` default 120 (30 m2) was below the 6 x 6 m rectangle (144 samples) and outside its own safe range 144-176 (fixed: 144, 36 m2; art 6.4 prop-free 6 x 6)
- [RECOMMENDED] Tuning Knobs: `TraversalLandingClearCm` lumped with SCL-owned heights, but the registry names LSC as source (fixed: split row)
- [RECOMMENDED] AC-55: >= 60 rounds gives about 30 appearances per zone, Wilson half-width 0.17, so a fair map fails 0.35-0.65 (fixed: >= 180 rounds, >= 45 per zone)
- [RECOMMENDED] R5.1/AC-7/AC-50: PawnRecovery had no AC; "every NPC disappearance" and AC-50's "despawns" would include corpse cleanup (fixed)
- [RECOMMENDED] Cross-References: row still "Provisional (not yet written)" for GDDs that now exist; CR list heading stale (fixed)
- [RECOMMENDED] D3/D6: sample and pair ranges stale after the 10,800 m2 band; A_z bound used the NpcCount-35 share term (fixed)
- [RECOMMENDED] OQ 11: T_contact >= 15 s implies start-set separation of about 70 m plus sight reach, the tightest zone constraint (noted; graybox plan to confirm first)
- [RECOMMENDED] Dependencies: Telemetry (#26) and Anti-Cheat (#27) GDDs do not exist yet (Alpha tier); both soft (noted)
Dependency graph: all hard and soft upstream/downstream GDDs exist except telemetry-analytics.md and anti-cheat-security.md (not started, Alpha).
Registry values: searched the knob names, NpcCount 50, TeamSize 5, walk 140 / run 450, traversal heights, ZoneDensityMaxMult 1.5, InformantRespawnPathCm 4500, EmergenceWitnessRangeCm 3200; no value conflicts. Name/ownership conflicts: ClueTagCourtsMaxCount (fixed), TraversalLandingClearCm (fixed).
Cross-doc follow-ups (main session):
- design/registry/entities.yaml: team_spawn_zone.respawn_region should read "walkable cells within InformantRespawnPathCm (45 m path) of the anchor and nearer to it than to the other used anchor (cut per used pair)"; register `SunPoolSamplesMinCount` 144 if keys are registered later; clue key stays `ClueMatchMaxCount` (Objectives).
- docs/architecture/adr-0014 §7.3: RespawnRegion cell set is precomputed per usable zone pair (nearer-anchor cut), not per zone; §6: define the graybox `GrayboxLayoutHash` extract record (LSC D7, 40 bytes: MeshIdHash, XYZ cm, rotation 0.01 deg, scale per mille).
- design/gdd/npc-kill-penalty-informants.md and ADR-0019 §4.3: "respawn region" is now the partner-cut region; widening steps are cut by the same rule (no value change).
- design/gdd/objectives-chain.md: V-CLUE now rejects a layout whose clue matches more than `ClueMatchMaxCount` courts; the edge case "layout logged ClueTooBroad, round proceeds" then covers only a slipped-through layout.
- design/gdd/systems-index.md: row 8 status Needs Revision (set by main session); telemetry-analytics and anti-cheat-security GDDs absent.
Receipt note: the target GDD was revised after this review, so its hash is intentionally not stamped; the next /design-review must run in full.
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/round-match-framework.md ab477269caa8bacab048c8719d3029a90a7ce89e
Reviewed-Content-Hash: design/gdd/session-travel.md 1e185cf338c1b17880a81bc7057dcb11fa8a0ee4
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 252003e5936635dd0a55fb062f10b42713cbf39d
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 0ecd59c878842ff54ca7fb1da7c30e07497e7efd
Reviewed-Content-Hash: design/gdd/systems-index.md b195f89efa215d57e7630933f75bbbc02bdd9ba8
Reviewed-Content-Hash: design/gdd/objectives-chain.md ca652db3c9bb041fb11d0af194650394ce5140b0
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910

## Closeout cross-doc pass 2026-10-25
- D4 applied in V-SEP/D5: `StartToFinalMinPathCm` 4,400 on the generator.
- Dependencies and Cross-References status cells set to In Review.
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.

## Cross-review fix 2026-10-25
Source: design/gdd/gdd-cross-review-2026-10-25.md (review-all-gdds, FAIL); fixes applied unattended under the user's authorisation.
- C-B3 / 2E-B2 / 2F-B1: R11.5 graybox checklist now says final-eligible courts are at least `StartToFinalMinPathCm` (44 m, D4) from every start cell; no other final-eligibility "60 m" remains (the remaining 60 m hits are the historical D4 note and the D6 contact example).
- C-W16 / 2B-W8: `StartToFinalMinPathCm` knob row now carries owner, audience/latch, invariants and the C-W24 OPEN note, for the registry mirror (default 4,400; hard 4,000-10,000; safe 4,000-4,400).
- C-W15 / 2B-W7: `LayoutWalkableCellsMinCount` / `MaxCount` split into two rows; default 3,250 kept (it does not bind below `NpcCount` 50: density term 2,700 cells), Max safe 2,700-3,600, Min safe 2,000-2,025. The safe range, not the default, was wrong.
- C-W17 (LSC parts): reseeds safe 3-6 -> 3-5; `LayoutTotalBudgetSec` safe 30-60 -> 45-60 (I-LSC-2); Prelim / Final candidate counts safe 3-5 / TeamSize+1-8 -> 3 / 6 (I-PCG-6 at the Generator default 16).
- C-W19: `ReachDetourMult` documented as derived (= `DetourMaxMult`, read-only alias; I-LSC-4 true by construction); `ZoneCandidatesMinCount` vs `CityGenZoneCandidatesCount` stated as proposed invariant I-PCG-7 (reject), safe range 2 (3-4 only with the Generator at 4).
- C-W23: D6 requirement at `NpcCount` 35 corrected (about 1,800 m2, not 1,467 m2) with the general formula `10 A / (NpcCount + 2 TeamSize)`, consequence (about half of unconstrained layouts fail at 35-40; Generator disc margin 8-11 %), fix (two-term floor statement, R11.5 2,000 m2 disc rule below `NpcCount` 45, recommended NpcCount safe floor 45 to TDC; NpcCount itself unchanged); D4 walk share restated against Objectives F1 T_round about 9 min (S 0.16-0.32; quarter threshold reached at L about 190 m), D29.
- C-W24 / 2C-W10: recorded as OPEN verification in D5, the `StartToFinalMinPathCm` row and new Open Question 15 (200-skeleton enumeration not run; no result claimed).
- C-W27: "63 pawns" -> NPCs (about 63 NPCs at TeamSize 5 = 72 pawns).
- C-W38: AC-14, R5b.4 cross-reference and the CPA change-request wording now say `DrawReplacement()` (CP-12(a), uniform over vacant cards; the victim's own card only when it is the sole vacancy); AC-14 asserts vacant-set membership.
- C-W40 (LSC side): R5b.2 now states explicitly the respawn region is the team's own anchor disc, strictly nearer than the partner's, never a union (registry wording is the main session's fix).
- C-W4: Information Visibility Policy relabelled Hard on the LSC upstream row (aligned with IVP's peer table); index row 8 to follow.


## Cross-review 2026-10-25b (/review-all-gdds since-last-review)
- ADR-0014 framing line; ReachDetourMult D2 variable row (derived alias); D6 recommendation recorded as adopted (NpcCount safe 45-50); LayoutWalkableCellsMinCount note cites the I-LSC-1 floor 35.
Report: design/gdd/gdd-cross-review-2026-10-25b.md. Status line unchanged.

## Approved 2026-10-25 (D35)
- Approved 2026-10-25 (D35): main-session decision under user authorisation after /review-all-gdds 2026-10-25b (CONCERNS, 0 blocking); remaining warnings stay listed in the GDD and design/gdd/gdd-cross-review-2026-10-25b.md.
