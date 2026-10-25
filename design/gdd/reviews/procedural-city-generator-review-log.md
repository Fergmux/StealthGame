# Review Log: Procedural City Generator

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
(Lean depth, requested by the orchestrator as `--depth lean`; `--depth` is the pre-1.1 name for `--review`. The Generator is built last and is off the critical path. No creative-director gate ran.)
Scope signal: L (multi-system Feature: 7 formulas, 6 upstream dependencies, 2 ADRs pending (off-thread generation and RNG; runtime ISM and nav APIs); producer should verify before sprint planning)
Specialists: one combined sub-agent (level-designer, systems-designer and technical-artist lenses), per the pipeline's one-agent fan-out rule
Blocking items: 10 | Recommended: 8
Summary: The architecture holds: a pure integer-seeded skeleton, G-PRE before any geometry, a Manifest-only client build and a deterministic failure ladder all match the revised Level Socket Contract. It had not caught up with the contract's MAJOR REVISION. Checked against the revised V-SEP (every start cell at least 60 m from every final-eligible court), corner zones mean no temple-ring court can ever be final-eligible. That made the court-class audit unsatisfiable and left the S5 anchor, the zone-fill and respawn predicates, the CrowdRoute node density, the canal water accounting and the Z-alley geometry under-specified or wrong. All blocking items were fixed in place (autonomous mode). A fresh re-review is required, full depth recommended once the LSC re-review lands.
Prior verdict resolved: First review
Findings:
- [BLOCKING] S4 / R-G11.4 / Edge Cases / AC-G34: with anti-diagonal corner zones, every temple-ring junction is about 58 m or less of path from one anchor, below V-SEP's 60 m, so finals fall only in the two axis corners. The "temple ring vs rest ≤ 50%" audit could never pass. Fixed: stratified courts (6 axis-corner final pool, 6 zone-corner prelims, 2-4 ring prelims); the audit becomes "axis-corner share ≤ 60%"; `CityGenFinalEligibleTargetCount` 6; OQ5 extended for creative-director confirmation.
- [BLOCKING] S5: "about 12 c in from a corner" landed inside a block, and V-ZONEFILL, V-RESPAWN and V-SEP were not pre-checked. Fixed: the anchor is a plain street cell on the second lattice corridor (A_z about 1,950 m² walkable, a 33% margin); named zone predicates in G-PRE; start cells are route nodes.
- [BLOCKING] R-G5.2: the guard band covered only ratio predicates, and the absolute path floors (the tight ones) were unguarded. Fixed: `CityGenGPrePathGuardPct` 8% on floors and band edges; a named G-PRE predicate list (R-G5.3); AC-G39.
- [BLOCKING] F1 / S2: walkable accounting ignored canal water. At a 20% canal share N_walk is about 2,024, below the 2,046 floor. Fixed: `CityGenCanalShareMaxPct` 12%, quays carved 1 c, an N_walk formula with the canal term.
- [BLOCKING] S7: a 3+2+3 c Z did not fit 7 c or 9-10 c blocks, and ExitPoint was unplaced (the mouth is about 9-10 m from O against LSC's 2-6 m). Fixed: Z in 7-8 c blocks with 2-3 c legs, an L-alley in 9-10 c blocks (exempt from the S11.2 prune), ExitPoint 5.0 m inside the leg; AC-G6 extended.
- [BLOCKING] S12: CrowdRoute nodes only at junctions and courts starved the LSC R6.1 NPC pool and the StartCell rule; Crowd CR-17 (10 m coverage) was unmet. Fixed: nodes every ≤ 3 c (`CityGenRouteNodeSpacingCount`), a second edge in courts and plazas wider than 6 c; AC-G37.
- [BLOCKING] R-G7: a runtime navmesh rebuild after `OnLayoutReady` was not forbidden (Crowd CR-18; the prototype rebuilds dynamically). Fixed: the navmesh is frozen from hand-off to teardown; AC-G36.
- [BLOCKING] S4: Penalty CR-13 (no single-pawn chokepoint) and Objectives CR-8 (4 fixture sides, ≥ 3 ingresses on final-eligible courts) were unaddressed. Fixed: ingress and fixture clearance rule; AC-G38.
- [BLOCKING] F5 vs R-G9 / AC-G33: F5 summed spawn 1.0 s plus nav 3.0 s, but art R4 and R-G9 allow 3.0 s combined. Fixed: one combined budget, worst case 5.76 s; the R4 report decides the reading.
- [BLOCKING] Dependencies: the D8 target was stated as 33/50 (the LSC says 34/50), and the Onboarding row offered golden seeds for practice, contradicting R-G11.5 (and citing R-G10). Fixed.
- [RECOMMENDED] F7: measured against a 220 m chain, superseded by D4's 120-240 m (target 170). Fixed: worst case 9.4-13% stated; per-tercile histogram.
- [RECOMMENDED] Cross-References: Round's row says the Generator may read `RoundIndex`. Change request 9 added.
- [RECOMMENDED] Cross-References: Session OQ6 (do clients run the generator?) is answered here. Change request 10 added.
- [RECOMMENDED] Cross-References: LSC-owned parts of the received requests (a role-blind bush exclusion, the `ClueTag` predicate, the CR-13 clause in V-NAV-2). Change request 11 added, plus a "Requests received" table.
- [RECOMMENDED] OQ5: "the final is in one of the two axis corners" is a new structural read. Creative-director to confirm it for MVP; 4-zone default as the lever (not applied).
- [RECOMMENDED] F6: p_g ≥ 0.30 is unverified now that the zone, path-guard and final-target pre-checks are stricter. The first 50-seed batch measures it (no change).
- [RECOMMENDED] OQ7: the canal-width question is still open and can move F1 again.
- [RECOMMENDED] AC-G17: "a reviewer sees no joint structure" is subjective. Pair it with the VG-3 autocorrelation threshold when the art pass sets one (not changed).
Not re-raised (already a pending user decision): pre-merged facade bays, paving slabs and G0 placeholder modules as derived assets.
Receipt note: the target GDD was revised after this review, so its hash is intentionally not stamped; the next /design-review must run in full.
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/level-socket-contract.md f318f06c15dd6194b5816961985e04c18cd1e5ce
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md d5494369a7ed46d882c863563dd3228f5d2281ab
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md f06247d43d0cf957747d3c8f7003659075ce1f30
Reviewed-Content-Hash: design/gdd/objectives-chain.md 986f8de4aca918e560156ceb328862adad93f3e5
Reviewed-Content-Hash: design/gdd/concealment-bushes.md dae06ab7f91d65fbb7e2c82c6d4fa3e7b710d877
Reviewed-Content-Hash: design/gdd/round-match-framework.md 5d1f316101eefb952dc5c9f42e9247d2cbcd613d
Reviewed-Content-Hash: design/gdd/session-travel.md 5a2f3d67fb9453d71eb9602429781de31d79777c
Reviewed-Content-Hash: design/art/art-bible.md 23373eb6ce1b1161988b4d931c1bbd545bd33f34
Reviewed-Content-Hash: design/gdd/systems-index.md d8783feb3e49846488003b4c5c6860e64372e36b

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
(Second round, requested by the orchestrator as `--review lean`. No specialists and no creative-director gate ran. Checked against the LSC, Registry and TDC as they stand after the 2026-10-25 user decisions, `NpcCount` 50 included.)
Scope signal: L (unchanged: 7 formulas, 6 upstream dependencies; producer should verify before sprint planning)
Specialists: none (lean)
Blocking items: 2 | Recommended: 6
Summary: All 10 blocking items of the first round are resolved in the text and re-verified (F1-F7 arithmetic re-computed and correct, including the 50-NPC V-AREA band 2,000-2,700 cells, F6 table, and the corner-zone rules). Two new blocking items come from checking S4 against the revised LSC: the court count was below the LSC N_C of 16, and the axis-corner final-eligible pool cannot reach 6 under V-SEP's 60 m start-to-final floor. The court count is fixed in the text; the pool geometry needs a design decision (Open Question 9). Several counts that depend on A (sockets, bushes, V-ZONEFILL floor) were refreshed for the widened band.
Prior verdict resolved: Yes (all 10 prior blocking items; no regression found)
Findings:
- [BLOCKING] S4 / `CityGenFinalEligibleTargetCount` / AC-G34: zone anchors at about (31, 97) m and (97, 31) m, start cells up to 18 m nearer, and a 65 m guarded floor confine final-eligible courts to about x + y ≤ 45 m from an axis corner. Only the two junctions where the first interior corridors meet the perimeter lane qualify (about 2 per corner, 4 in all); the lane's corner turn has 2 ingresses (< 3 required) and is 22 m from both; 3 courts at 24 m spacing cannot fit the stated "30-40 m path" band (about 7 m × 40 m). The target of 6 (= LSC `FinalCandidatesMinCount`) is unreachable, so G-PRE would always fail `FinalEligible`. NOT fixed (design decision): caveat added to S4, Open Question 9 with four levers and a grid-only feasibility script.
- [BLOCKING] S4 / `CityGenCourtTargetCount` / D5: courts 14-16 (default 15) vs LSC D5 N_C ≥ 16 (FinalCandidatesMinCount 6 + 4 × 3 − 2). Fixed: strata 6 + 6 + 4-6 = 16-18, default 16, hard 16-20, safe 16-18; F2 N_fixture and its worked example updated (2,615 entries, 23,543 B).
- [RECOMMENDED] S7 / F2: socket quota "12-13" and N_bush "8-9" were stale for the band (A up to 9,676 m²: N_E 12-14, N_B 8-10 per LSC D5). Fixed.
- [RECOMMENDED] S5 / R-G5.2: the V-ZONEFILL floor 1,467 m² holds only at A = 8,800 m²; D6's ρ_p = pawns / A raises it to about 1,610 m² at the band top (margin falls from 33% to about 21%, still passing). Fixed: G-PRE computes the floor from the candidate's own A.
- [RECOMMENDED] R-G5.3: the named G-PRE list omitted V-NAV-3 (has a grid form), V-REACH and V-CLUE (now an LSC predicate, previously a change request). Fixed.
- [RECOMMENDED] R-G11.4 / AC-G10 / Edge Cases: false-pass ceiling was 20% in two places and 15% in F6 (p_v ≥ 0.85) and the Edge Cases. At p_v = 0.80 the per-attempt p is 0.754, below the 0.78 design target. Fixed: 15% everywhere.
- [RECOMMENDED] AC-G17: "a reviewer sees no joint structure" was untestable. Fixed: p99 per-pixel |ΔL*| ≤ 3 (provisional) plus lead sign-off.
- [RECOMMENDED] S5: "a plain street cell (never a junction) on the second lattice corridor from the grid edge on both axes" is ambiguous, since a cell on both second corridors is their crossing (a junction). Not changed: needs the level-designer's intent (suggested wording: the corner-facing arm cell 1-2 c from that crossing).
- [RECOMMENDED] F5 / Open Questions: `t_cand` ≤ 0.12 s is unmeasured, and AC-G30's p95 ≤ 10 s holds only if p ≳ 0.78 (at the LSC floor 0.565 the p95 is about 16 s). Open Question 10 added; `CityGenFinalEligibleTargetCount` safe range raised to 6-8 (never below the LSC floor).
Cross-doc follow-ups (main session):
- Registry: `CityGenFinalEligibleTargetCount` safe range 4-6 becomes 6-8; register `CityGenCourtTargetCount` (16, 16-20, 16-18); TDC invariant `CityGenCourtTargetCount ≥ FinalCandidatesMinCount + 4 · PrelimCandidatesPerStageMinCount − 2`.
- LSC: decide which lever for Generator OQ 9 it accepts (`StartToFinalMinPathCm` toward 45-48 m, or `FinalCandidatesMinCount` toward `TeamSize` with a creative-director check of Objectives SP-4); LSC OQ 11 (16 courts at 24 m) should use the same grid script.
- Systems index: row 16 stays In Review / Needs Revision; the risk table still says "integer-grid FRandomStream" (ADR-0006: PCG32).
- Objectives OQ 9 still says "LSC floor still 4" (it is 6).
Receipt note: the target GDD was revised after this review, so its hash is intentionally not stamped; the next /design-review must run in full.
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/level-socket-contract.md af334d6d7daf805031824f58026b65822f1b670e
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 72cdc3c5219df10a4e38f30e08b767f4fff9d79d
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/objectives-chain.md ca652db3c9bb041fb11d0af194650394ce5140b0
Reviewed-Content-Hash: design/gdd/concealment-bushes.md a7075ec56f19ea43b29d27c845e98d280ed6797f
Reviewed-Content-Hash: design/gdd/round-match-framework.md ab477269caa8bacab048c8719d3029a90a7ce89e
Reviewed-Content-Hash: design/gdd/session-travel.md 1e185cf338c1b17880a81bc7057dcb11fa8a0ee4
Reviewed-Content-Hash: design/art/art-bible.md ba9f28821285a7933cad48bee9f4ccf28499ac5e
Reviewed-Content-Hash: design/gdd/systems-index.md b195f89efa215d57e7630933f75bbbc02bdd9ba8
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 63db04c802c6db06eb0ba3dac93f65fb9fa98068
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 0ecd59c878842ff54ca7fb1da7c30e07497e7efd
