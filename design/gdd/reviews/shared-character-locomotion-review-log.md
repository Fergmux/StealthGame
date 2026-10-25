# Review Log: Shared Character & Locomotion

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: XL (cross-cutting: one pawn class for every body, 12 formulas, 10+ dependents, several ADRs: ADR-001 pawn parity and cull hook, Look-parity mechanism, step clock authority, CMC mapping; producer should verify before sprint planning)
Specialists: systems-designer, qa-lead, art-director, creative-director (senior). Panel capped at 3 specialists by the pipeline budget rule; game-designer, gameplay-programmer and ai-programmer were not spawned.
Blocking items: 38 (main review 6, systems-designer 14, art-director 6, qa-lead 11; overlaps merged) | Recommended: 32
Summary: The creative-director judged the pillars, fantasy and structure sound; the problems were correctness bugs in the formulas and parity mechanisms that were asserted but not written down. The decided informant-takeover rule (C1) was not yet reflected, the IIF R3a Look-parity mechanism and relevancy bans were not owned explicitly, and the Visibility Policy's concealment-cull amendment contradicted R13. All blockers were fixable in place; a full (not lean) re-review is required, focused on F1-F12, R13, R2a and ACs 38-46.
Rulings (creative-director): D1 walk braking is one constant deceleration set from the slowest pace (187 cm/s²; 0.60 s/34 cm at 112, 0.75 s/53 cm at 140; stop ≥ 0.5 s at every pace), time-based braking rejected. D2 tier D split into D-culled (no pose) and D-visible (≥ 8 Hz minimal pose), C cap kept, no rendered pawn ever frozen. D3 brace freezes translation and yaw; entry is a step-into-stance clip. D4 no owner exemption from tiering (art bible 5.6). D5 one contact reaction for every pawn; players never auto-yield; Crowd gives NPCs a 10-30 % non-yield rate. D6 first-person camera pitch −60/+40 local only; replicated head pitch stays ±20 (OQ8 adjusted).
Prior verdict resolved: First review
Revisions applied after review: 41 (by the reviewing agent, autonomous mode). Re-review pending.
Findings:
- [BLOCKING] Core Rules R2 / OQ6 / AC-SCL-02: informant respawn could create a pawn, contradicting decided rule C1. FIXED: R2a `TakeOverCrowdPawn` (eligibility, one-frame bind + possess, no new actor); factory callers Round and Crowd only; OQ6 resolved; AC-02 rewritten, AC-38 added.
- [BLOCKING] Core Rules (Roster dependency): no `PawnLeavingGame` override. FIXED: R2b; AC-42.
- [BLOCKING] Core Rules R8: IIF R3a Look-parity mechanism not owned; no observer-side HeadLook interpolation. FIXED: R8 rewritten (one control-rotation path, one body-yaw rule, one filter per server tick, one pitch path, proxy interpolation); AC-09, AC-18 rewritten, AC-39 added.
- [BLOCKING] Core Rules R5: AI path-following not routed through Move. FIXED: R5.7; AC-40.
- [BLOCKING] Core Rules R13: dormancy/owner-only/per-class priority bans not explicit; "relevancy can never be a tell" contradicted Visibility Policy R6/R7. FIXED: explicit bans, concealment-cull exception and hook (legacy and Iris), L27, `Instigator == Self`; AC-16 absolute values, AC-41 added.
- [BLOCKING] Dependencies / Cross-References: Visibility Policy listed as unwritten. FIXED.
- [BLOCKING] Formulas F1: Run evaluated before the dead zone; RoundEnded never 0; dead zone not a server input. FIXED.
- [BLOCKING] Formulas F2 / R5.3: "never < 0.5 s" false at the 0.8 pace; F2 ranges disagreed with the knob table. FIXED per D1; I-SCL-4; ranges aligned.
- [BLOCKING] Formulas F3: B5 radius ≥ 100 cm failed inside the safe range. FIXED: I-SCL-6, safe range 50-64.
- [BLOCKING] States / R5.4: Pivot entry for idle pawns undefined, phantom 60° threshold, no reversal hysteresis, 180° tie. FIXED: `PivotExitAngleDeg`, `ReversalHysteresisDeg`, latch, tie rule; AC-07 extended.
- [BLOCKING] Formulas F6: divide by zero at LookEaseSec 0, unsigned velocity flip, overshoot, 198 °/s diagonal, frame-rate dependence. FIXED: 2D signed-velocity filter at 1/60 s sub-steps; torso-relative rates stated.
- [BLOCKING] Formulas F7 / R11: promotion hold froze newly visible pawns; z ≤ 0; η units; I-SCL-3 ignored hysteresis. FIXED: demotion-only hold, z_near, η = pct/100, I-SCL-3 extended; AC-26 table, AC-46.
- [BLOCKING] Formulas F8 / R11.4: owner pawn costed outside the caps (art bible 5.6 bans the exemption); hard caps exceed 4 ms; overflow froze visible pawns. FIXED per D2/D4; I-SCL-11.
- [BLOCKING] Formulas F11 / R18: cadence 1.6 steps/s at 112 broke B1; step clock unsynchronised with visible plants; steps in pivots/falls/traversal undefined. FIXED: cadence by pace, AnimBP follows the clock (±50 ms), start/settle/pivot steps, suppression; AC-31 rewritten; OQ12.
- [BLOCKING] Formulas F4 / R19: IsRunning speed source could be flipped by root motion, falling or catch-up. FIXED: commanded-locomotion velocity, forced false states, I-SCL-7.
- [BLOCKING] Formulas F9 / R13: f = 30 exceeded the budget; tick quantisation broke U ≤ 0.083 s. FIXED: range 12-17, U_eff, I-SCL-10.
- [BLOCKING] Formulas F10, F12, F5: snap vs run speed, τ < 1/f, starvation hard minimum below the heartbeat, Δt undefined, discard vs consume; climb 40 cm vs 100 cm landing clearance, 45-50 cm gap, selection order, server lag. FIXED: I-SCL-8/9/12, hard min 0.25 s, discard, landing cell definition, `TraversalMinHeightCm` = step height 45, filter-then-choose, F5 after saved moves (verify).
- [BLOCKING] Core Rules R5.2 / States: planted brace still turned at 120 °/s (wide-stance slide). FIXED per D3; AC-11 rewritten.
- [BLOCKING] Acceptance Criteria: AC-14, 21, 24, 26, 28, 32, 33 untestable, underpowered, flaky or mis-tagged; no allowlist AC. FIXED: rewritten with statistics, fixed seeds, sample sizes; AC-43 added.
- [RECOMMENDED] Core Rules R5.6 / B6, B8: players never auto-yield; no touch. APPLIED per D5 (R5.8 contact reaction; Crowd non-yield rate, OQ10).
- [RECOMMENDED] R9 / OQ8: first-person pitch ±20° too restrictive. APPLIED per D6.
- [RECOMMENDED] R5.5 turn-to-look exit, R5.1 pace hysteresis, States missing exits, R9 vs FOV tiering wording, B2 idle set, tier C cadence and foot-placement threshold, pivot rate range. APPLIED.
- [RECOMMENDED] Acceptance Criteria: missing ACs for R17, R5.6, R10, R5.5, planted braking; tolerances for AC-05 to AC-08, AC-23. APPLIED (AC-44, AC-45; AC-05 to 08, 23 rewritten).
- [OPEN] Acceptance Criteria: AC-04, 12, 13, 15, 17, 19, 20, 22, 25, 29, 30, 36, 37 tolerance and capture-method refinements (qa-lead items 14-24) not applied; AC numbering not re-sequenced (ids are referenced by other GDDs).
- [OPEN] Tier C boundary hysteresis 12 % (art-director item 8) not applied; I-SCL-3 enforces non-overlap only.
- [OPEN] Cross-GDD change requests 1-14 in the GDD's Cross-References (IIF, TDC, LSC, Round, Crowd, Camera, Assassination, Penalty, Visibility Policy, Diegetic Audio, art bible, systems index, Roster, Character Pool).
Receipt note: the target GDD is deliberately not stamped, because it was revised after review; the next run must perform a full re-review.
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 90b88857c2a98e951356a90c5878a80f231c47ca
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3
Reviewed-Content-Hash: design/gdd/round-match-framework.md b710a1c57960995a95c305b58642d529c8f33c93
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 8bf1dcd1acd4e451528d64eb7491c82b2479c26e
Reviewed-Content-Hash: design/gdd/session-travel.md 5a2f3d67fb9453d71eb9602429781de31d79777c
Reviewed-Content-Hash: design/gdd/level-socket-contract.md f318f06c15dd6194b5816961985e04c18cd1e5ce
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md ec783f844405983ceb5bb1344d9d82cfcb68f236
Reviewed-Content-Hash: design/art/art-bible.md 23373eb6ce1b1161988b4d931c1bbd545bd33f34
