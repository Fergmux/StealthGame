# Review Log: Concealment (Bushes)

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system integration, 4 formulas, 9 upstream and 8 downstream dependencies, ADRs for owner component, MPC rustle and owner-side warp plus spikes S-CON-1 and R5; producer should verify before sprint planning)
Specialists: game-designer + systems-designer + qa-lead (one combined brief), ue-replication-specialist, creative-director (senior verdict)
Blocking items: 13 | Recommended: 9
Summary: The core design holds and no locked ruling moved: occupancy-blind bush use with no dwell cap and k = 1.5 (C7), full invisibility with blind-attackability, no informant x-ray. The problems were missing specification on the leak and timing paths (fresh actors visible by default, an arrival-based clock, kill-while-concealed re-open, host gating, avoidance as an occupancy oracle, owner-side warp), stale text contradicting the IVP and Assassination rulings, and acceptance criteria that fail at compliant values (the 10 % multi-occupant trigger is exceeded by the F1 defaults; AC-CON-07's sample size equals the design p99). Creative director: NEEDS REVISION; all fixes are additions or corrections.
Prior verdict resolved: First review
Findings:
- [BLOCKING] F2 / Tuning Knobs: k range 1.0-2.0 and k = 2.0 prose -> fixed: k 1.0-1.5, 1.5 the ceiling, Crowd's 0.20 posterior declared invalid input, r_N,req evaluated at measured n_P/n_N (IVP change request 12 applied).
- [BLOCKING] CON-R14 / Edge Cases / AC-CON-21 / Dependencies: automatic cull-off fallback contradicted IVP R7.6 and L29 -> fixed: no automatic fallback, escalation to creative director and user, client-side hide internal dev builds only; Crowd now a hard upstream for any external build.
- [BLOCKING] F3 / Tuning Knobs: `BushStrikeEmergeSec` range mismatch (0.25-0.5 vs 0.25-0.6) and zero-margin hard minimum -> fixed: safe 0.3-0.5, hard 0.30-0.6, F3 (f) = I-ASN-3; `BushExitCrossSec` hard minimum 0.30 (Assassination change request 7 applied).
- [BLOCKING] CON-R12 / F4: Assassination reconciliation -> fixed: OQ1/OQ2 closed, ally exclusion confirmed, filter by full F1 reach (Δz and warp included) at each candidate's server position then pick, terminal whiff (empty, ally-only or out-of-reach bush never falls through to the open spot), hash salted server-side (`SpotSalt`, not `RoundSeed`).
- [BLOCKING] CON-R12 / AC-CON-24: the ≤ 10 % multi-occupant trigger is exceeded at defaults (F1 baseline m0 = 1 − e^(−λ) ≈ 29 %) -> fixed per creative-director ruling: review when m > 35 % or m > m0 + 10 points; inconclusive below 30 strikes.
- [BLOCKING] Player Fantasy / AC-CON-13: sequence backwards (striker crosses at 0.35 s, contact at 0.75 s) -> fixed in anchor 3, CON-R9, Game Feel and AC-CON-13 (with braced-victim control and whiff-from-inside case).
- [BLOCKING] CON-R6 / AC-CON-07 / AC-CON-08: "from pose arrival" clock -> fixed: CON-R6a schedules hide clips from a replicated start server time (change request to `FActionState`), seek-never-snap.
- [BLOCKING] CON-R6 / CON-R16: fresh actors visible by default; per-frame pose-driven hide pops the ally body at +0.9 s -> fixed: CON-R6b fail-closed latch `IsAllyBodyHidden(Pawn)` evaluated in `PostNetInit`, explicit set/clear/precedence rules (Teammate Markers CR-6 applied).
- [BLOCKING] CON-R9: a re-created actor cannot know it died concealed; "kill step" instant undefined -> fixed: record removal at the outcome slot, un-cull plus `bDiedConcealed` at Assassination's death write (change request), identical for NPC and player victims, no companion data.
- [BLOCKING] CON-R4 / CON-R4a: owner-side warp unspecified (server-applied source hitches every hide); transition pawns blocked by their own `HideVolume` -> fixed: two mechanisms named for spike S-CON-1 (AC-CON-31), `MoveToForce`, collision state derived from state and elapsed server time, `HideVolume` ignored in transitions, input zeroed.
- [BLOCKING] CON-R4 / CON-R14: NPC avoidance around invisible occupants is an occupancy oracle; Crowd LOD could change replication -> fixed: avoidance exclusion, shared net frequency and quantised cull boundary.
- [BLOCKING] CON-R7 / VA-5 / AC-CON-09: 60 cm boom vs Camera's `ConcealedBoomCm` 100 (lens outside the dome) -> kept 60 per creative-director ruling; change request 12 to Camera (default 60, safe max 80); Camera OQ7 answered: body yaw locked, Look within the head clamp.
- [BLOCKING] Acceptance Criteria: AC-CON-07 statistically unsound (N = 200 at the design p99); AC-CON-01 bundled unmeasurable client timing; several ACs without pass bounds or controls; rules without ACs -> fixed: AC-07 N = 1,000/300 with ≤ 1.5 %/2.0 % late-actor bounds and 0 early draws; AC-01 split into 01a/01b; AC-02, 05, 06, 08-12, 14-21, 23, 24 tightened; AC-CON-25 to 32 added (kill while Entering, kill-while-concealed timing, manifest reject, collision and avoidance, guard fault, takeover eligibility, owner warp, rejoin); tags defined.
- [RECOMMENDED] CON-R2: cull predicate must replace the whole `IsNetRelevantFor` body; owned/attached actors follow owner relevancy -> applied.
- [RECOMMENDED] CON-R16: host gating (no `SetActorHiddenInGame`/`bHidden`, server-item-driven latch, foley/Niagara/decal gating, presentation trace channel mechanism) -> applied.
- [RECOMMENDED] States: queue items carry a transition generation id and are dropped when stale; due times in whole ticks -> applied.
- [RECOMMENDED] CON-R8: "standing pawn" undefined for transitioning pawns; depenetration cap had no value -> applied (`BushExitDepenetrationCapCm` 30).
- [RECOMMENDED] F1: expected E[O] range wrong (0.12-0.54, not 0.2-0.4); m0 baseline added -> applied.
- [RECOMMENDED] F3: missing cross-knob invariants (Swap < EnterDur, Cross < ExitDur) and calm-gait warp-speed bound -> applied as F3 (d)-(e); safe ranges tightened (Reach 30-70, Swap 0.45-0.7).
- [RECOMMENDED] Player Fantasy: two posteriors conflated (unknown entrant ≤ 1/6 vs tracked player ≈ 85 %); anchor 5 sanctioned a long-dwell spotter -> applied per rulings G2/G3; long dwell acknowledged as a P2 tell (G4).
- [RECOMMENDED] CON-R15: ambush metric (kills from `Emerging`, review at 25 %) -> applied per ruling G5; CON-R3 cooldown prose corrected (G7).
- [RECOMMENDED] Dependencies / Cross-References: stale statuses (Assassination, Teammate Markers, Objectives "Not written"; Crowd "In design") -> updated; Assassination, Teammate Markers and Objectives rows added.
Specialist disagreements: (1) systems-designer read the 0.35 m curb as a width outside the footprint and called F4's 1.35 m example unreachable; main review and creative director: the curb is the planter's height inside the 2.0 m footprint, the example holds; wording clarified. (2) Creative director wanted the killed-concealed record removal and un-cull at one instant (the outcome slot); the revision splits them (removal at the slot, un-cull at Assassination's death write) because an un-cull at execution would make a kill and a whiff differ on the wire before contact, breaking ASN-R21 / AC-ASN-20. Logged as an autonomous decision; the creative director's re-review should confirm.
Change requests raised (not applied to other GDDs): Concealment Cross-References items 10-17 (Assassination `bDiedConcealed`, `StartServerTime`, filter-then-pick and salted hash; IVP R7.5/R7.8 death-write timing and relevancy scope; Camera `ConcealedBoomCm` 60 and OQ7; Crowd avoidance exclusion and LOD replication; Teammate Markers CR-6 closed; SCL/IIF owner warp and presentation gating; TDC knob and invariants; systems index Crowd hard).
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md d8fd90df42d37734c67ecba262aefd4ca1d353b8
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 92b2933d7e7100547848bb6450f1aea75ece2cae
Reviewed-Content-Hash: design/gdd/teammate-markers.md fa6747053069b90ca709dd4b0b3aa5ab56eb2efa
Reviewed-Content-Hash: design/gdd/camera-system.md f9e69a811e48da816bbeb7424f626b61d966634b
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md d5494369a7ed46d882c863563dd3228f5d2281ab
Reviewed-Content-Hash: design/gdd/level-socket-contract.md f318f06c15dd6194b5816961985e04c18cd1e5ce
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (unchanged: 4 formulas, 9 upstream and 8 downstream dependencies, ADR-0017 owner component, spikes S-CON-1 and R5; producer should verify before sprint planning)
Specialists: none (lean, single-session second-round review)
Blocking items: 3 (all fixed in place; the verdict stands until a further review) | Recommended: 5
Summary: All 13 blockers of the 2026-10-24 full review are confirmed resolved in the text (leak and timing paths, CON-R6a/R6b/R9, k range, host gating, ACs with bounds), and the user decisions of 2026-10-25 (`bDiedConcealed`, `StartServerTime`) are consistently applied. The new problems are stale text left by later syncs: AC-CON-24 (c) still used 35 % although the NpcCount 50 baseline moved m0 to 34 % (CON-R12 had already been raised to 40 %), and F4 still described the replaced stateless-hash pick in several places. One open design question remains for the creative director (the absolute cap on m against the F1 range).
Prior verdict resolved: Yes (13 of 13 blockers; the full-review note that this re-review must run in full was honoured: the whole document was read)
Findings:
- [BLOCKING] Acceptance Criteria / AC-CON-24 (c): `m` ≤ 35 % sat 1 point above the F1 baseline m0 = 34 % at defaults (a compliant build fails it) and contradicted CON-R12's 40 % -> fixed: AC now reads ≤ 40 % and ≤ m0 + 10 points, same as CON-R12.
- [BLOCKING] F4 / CON-R12 / Interactions / Dependencies / AC-CON-14: pick still specified as a "stateless hash" with `SpotSalt`, variable row `H`, "hash value 7 → 7 mod 2" example and "1,000 fixed salts", contradicting the F4 formula (secret stream, `UniformBelow`, ADR-0006 domain 14, registry revision note) -> fixed: rows, example, rules and AC reworded to the stream draw.
- [BLOCKING] Open design question for the creative director: CON-R12 / AC-CON-24 (c) apply an absolute cap (m > 40 %) AND m > m0 + 10. At the F1 upper range (N_B 8, n_N 60: λ 0.54, m0 ≈ 42 %) the absolute cap trips on a compliant build, so the rule is only coherent at or near defaults. Not changed (a CD ruling): either drop the absolute cap or make it max(40 %, m0 + 10).
- [RECOMMENDED] F3: the F3 variable table gave `BushEnterSwapSec` 0.35-0.7 against the Tuning Knobs and registry safe range 0.45-0.7 -> fixed to 0.45-0.7. Also (d) is joint: Swap 0.45 at Reach 60 gives 211 cm/s against the 210 limit -> noted in F3 (the validator, not the ranges, binds; practical floor 0.46); registry safe range left to the main session.
- [RECOMMENDED] AC-CON-11: the failing data set "swap 0.9 with dur 0.8" violates (a) and (e), not one constraint -> expected validator output now names the exact set for each row.
- [RECOMMENDED] AC-CON-23: "above the noise floor" undefined -> defined as the 99th percentile of the temporal-mean difference between two empty bushes at matched sway.
- [RECOMMENDED] CON-R2 / ACs: the owned/attached-actor relevancy rule had no AC -> AC-CON-33 added.
- [RECOMMENDED] CON-R4: garbled "(or the warp is stopped by its own bush)" -> reworded.
Registry values: searched the bush entries, thresholds and knob names against design/registry/entities.yaml (NpcCount 50, k 1.5, ConcealCullDelayMs 250, ConcealedBoomCm 60/70, BushSpotToleranceCm 75, BushEnterSwapSec 0.5, BushExitCrossSec 0.45, StrikeVerticalReachCm, BushSpacingCm 1200): no conflict with the GDD. One stale registry note the other way (below). F1/F2 arithmetic re-derived (E[O] 0.410, m0 0.336, pick-on-player 0.82, r_N,req 0.633 r_P, P_hide 0.112, T_uncull 250 ms): all correct.
Dependency graph: all 14 named GDDs exist; Diegetic Audio, Telemetry and Anti-Cheat are listed "Not written" (no file, correctly marked).
Cross-doc follow-ups (main session):
- design/gdd/assassination-defence-reversal.md ASN-R8.2, Dependencies row (line ~318), change-request rows and AC-ASN-39 still say "stateless hash salted with `SpotSalt`" / "BushVictimHash" / "1,000 fixed hash inputs"; Concealment F4, ADR-0017 §7 and the registry now use the `BushSpotPick` stream (`UniformBelow`).
- design/registry/entities.yaml `npc_bush_use_rate_target` notes say "0.625 * r_P" (NpcCount 40); at NpcCount 50 it is 0.633 * r_P (Concealment F2, Crowd F2). Also `exit_occlusion_timing_chain` expression lists only (a)-(c) while F3 now has invariants (a)-(f).
- Registry `BushEnterSwapSec` safe range 0.45-0.7: at Reach 60 the lower end fails F3 (d) by 1 cm/s at the provisional walk speed; consider 0.5-0.7 or a joint (d) note in Tuning Data & Config.
- Creative director: ruling on the CON-R12 / AC-CON-24 (c) absolute cap (see third blocker above).
- Systems index: Concealment stays Needs Revision until the CD ruling and a clean re-review.
Reviewed-Content-Hash: design/gdd/concealment-bushes.md a7075ec56f19ea43b29d27c845e98d280ed6797f
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md c56bbe20ab9e274be42989aac11f4cc6b797f818
Reviewed-Content-Hash: design/gdd/teammate-markers.md 8b45501b2783ca0601c2f3832fa3aba2cd85e2bc
Reviewed-Content-Hash: design/gdd/camera-system.md 6c124b4dd555a8157b58cca2a38167582a200cdd
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 242977e3dcc0185347fd1053741e6f305c1e42fa
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8

## Closeout cross-doc pass 2026-10-25
- CON-R12 / AC-CON-24(c): multi-occupant share cap is max(40 %, m0 + 10 pp) (D1), replacing the double cap.
- `BushEnterSwapSec` safe lower bound is now 0.46 s in F3 and the knob table, matching the registry (0.45 failed F3(d) by 1 cm/s at Reach 60).
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.
