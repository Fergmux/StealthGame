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
