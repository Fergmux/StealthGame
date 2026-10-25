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

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: XL (unchanged: one pawn class for every body, 12 formulas, 14+ dependents, ADR-0001/0003/0010/0013/0016/0019 interplay; producer should verify before sprint planning)
Specialists: none (lean, single-session; formulas F1-F12 re-computed by hand at default and boundary values; registry and the sibling GDDs/ADRs it cites consulted by grep)
Blocking items: 2 | Recommended: 7
Summary: All 38 first-round blockers are verified resolved in the text (R2a/R2b, R5.7, R8, R13, F1-F12 arithmetic, planted brace, AC rewrites 38-46); every formula example recomputes correctly. New problems were stale figures left behind by the revision pass (8-bit pitch quantisation in F6 and AC-09, a PENDING-USER row contradicting the applied walk-slower decision, invariant ranges I-SCL-1..12/14 against a list of 15) plus a missing `CheckTakeOverEligibility` that the Penalty GDD and ADR-0010/0019 already call. Two items need a ruling or a measurement and remain blocking: sub-pace stops contradict the "no sudden stop" claim, and the listen-host animation cost is not bounded by F8.
Prior verdict resolved: Yes (38 of 38 first-round blockers; 3 first-round OPEN items carried forward as recommended)
Findings:
- [BLOCKING] Core Rules R5.3 / Formulas F2 / Edge Cases: "every walk stop takes ≥ 0.5 s, no input produces a sudden stop" is false for a stop begun below a pace (t = v / d: under 0.5 s below 94 cm/s, under 0.2 s below 37 cm/s, i.e. a key tap in the first 0.27 s of a start). Wording narrowed to "from a steady pace"; OQ15 opened. Needs a creative-director ruling (braking-time floor vs accepted NPC-shown start-abort) and a Crowd check.
- [BLOCKING] Core Rules R11.5 / R12 / Formulas F8 / AC-SCL-29: the listen host poses every pawn at rate 1, so F8's 3.94 ms client-tiering worst case does not bound host animation cost (60 pawns at about 120 µs would be about 7 ms by the formula's own c_A); the GDD had no host line. Scope note added to R12 and F8, host capture added to AC-SCL-29 (4.0 ms summed / 2.0 ms game thread, ADR-0013 §5), OQ16 opened. Remains blocking until spike R3 gives a host cost row or the uniform TickPose escalation is taken.
- [RESOLVED] Formulas F6 Output Range: pitch quantisation "≈ 1.41°" contradicted R8.4 and AC-18 (16-bit, ≈ 0.0055°). Fixed; F9 b_i updated (yaw 1 B + pitch 2 B).
- [RESOLVED] AC-SCL-09 vs AC-SCL-18: pitch bounds ±21.5° / step 10.7° (8-bit era) against ±20.1° / 9.4°. AC-09 aligned to AC-18.
- [RESOLVED] Core Rules R2a.1: eligibility lacked `IsPendingTeardown` and the side-effect-free `CheckTakeOverEligibility(Pawn)` (ADR-0010 §3.2a, ADR-0019 §4; Penalty PEN-R9 step 3 calls it). Added.
- [RESOLVED] Cross-References change-request table: Crowd CR 3 walk-slower row read PENDING-USER while the 2026-10-25 row says APPLIED. Marked superseded/applied.
- [RESOLVED] Cross-References CR 2 "I-SCL-1 to I-SCL-12" and AC-SCL-35 "I-SCL-1 to I-SCL-14" against 15 invariants. Both now 1-15.
- [RESOLVED] Core Rules R13 / OQ5: legacy-versus-Iris left open; config decision accepted 2026-10-25 pins Iris off and `NetClientTicksPerSecond` 2400 (applied per ADR-0003). R13 and OQ5 updated; Iris clauses kept as ADR-0024 contingency.
- [RESOLVED] Core Rules R5.2: the planted-brace entry was "one authored clip" for 34-53 cm of root motion within ±10 %, which one fixed clip cannot meet and which left idle entry undefined. Restated as a distance-matched pose-database entry (R10).
- [RESOLVED] Acceptance Criteria: AC-04, 22, 30 had no sample size or pose tolerance; AC-19 name said 49 pawns; AC-32 said "until the Crowd GDD exists" (it does); R5.8 contact reaction and R17.1 body constants had no AC. Tightened; AC-SCL-47 and AC-SCL-48 added.
- [RECOMMENDED] Formulas F1 / Core Rules R5.1: the pace set {0.8, 0.9, 1.0} and the ±0.03 quantisation hysteresis are written as literals; the set belongs to Crowd's pace keys. Name the consumed key (SCL should read the set, not restate it) and register the hysteresis as a knob or constant.
- [RECOMMENDED] Acceptance Criteria: no AC for the ±0.03 pace hysteresis, the R18.2 start/settle/pivot steps, or the R4.3 last-command hold.
- [RECOMMENDED] Dependencies: Diegetic Audio (22, VS) and Anti-Cheat (27, Alpha) have no GDD yet; expected by tier, no action before their authoring.
- [RECOMMENDED] Acceptance Criteria: first-round OPEN qa-lead tolerance/capture-method refinements remain for AC-12, 13, 15, 17, 20, 25, 36, 37 (AC-04, 22, 30 done this round); AC ids still not re-sequenced.
- [RECOMMENDED] Formulas F7: first-round OPEN tier C boundary hysteresis 12 % (art-director item 8) still not applied; I-SCL-3 enforces non-overlap only.
- [RECOMMENDED] Formulas F8: C_fixed 0.9 ms leaves a 0.06 ms margin at defaults; all c_t are ASSUMED. Acceptable only while R3 stays a gate.
- [RECOMMENDED] Cross-References: change requests 1-15 remain the main session's to propagate; confirm art bible 11(a)-(h) wording changes landed.
Cross-doc follow-ups (main session):
- design/registry/entities.yaml `pawn_network_budget`: output_range "about 26 KB/s worst case" is stale; SCL F9 gives 31.1 KB/s worst case against a 36 KB/s line (NpcCount 50).
- Systems index: set Shared Character & Locomotion to "Needs Revision" (second review 2026-10-25, lean); add Round and Session as dependents and LSC as soft peer (CR 12, still open).
- NPC-Kill Penalty & Informants and ADR-0010/0019: already call `CheckTakeOverEligibility`; SCL R2a now owns it. No edit needed there beyond confirming the `IsPendingTeardown` term is in Penalty's PEN-R9 step 3 wording.
- Crowd & NPC Behaviour: needs to answer OQ15 (do route-leg starts ever abort below pace?) and name the pace-set key SCL should consume.
- Technical-artist / performance-analyst: host capture for spike R3 (OQ16) must include the listen host with 9 remote clients.
- Art bible: confirm CR 11 (a)-(h); in particular B4 wording interacts with OQ15.
Revisions applied after review: 24 (by the reviewing agent, autonomous mode). Third review pending; do not treat the revisions as an approval.
Receipt note: the target GDD is deliberately not stamped, because it was revised after review; the next run must perform a full re-review.
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 72cdc3c5219df10a4e38f30e08b767f4fff9d79d
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 252003e5936635dd0a55fb062f10b42713cbf39d
Reviewed-Content-Hash: docs/architecture/adr-0010-shared-pawn-cmc-possession.md 58f59f4168c6ada0eefe565a0586cf51f56c7497
Reviewed-Content-Hash: docs/architecture/adr-0013-animation-motion-matching-significance.md ba39985e11bb5cc3cfbb53a09c61b00db85fc8ea
Reviewed-Content-Hash: docs/architecture/adr-0003-replication-relevancy-concealment-cull.md ab5abc0c7a57a15709c7c92762a2ca4590f67b13

## Closeout cross-doc pass 2026-10-25
- OQ15 closed by D2 (stop floor `d_eff = min(d, v0 / StopMinSec)`, `StopMinSec` 0.5 s); AC-SCL-50 vectors include 93 cm/s, the floor boundary. OQ16 closed by D3 (uniform TickPose escalation, confirm in spike R3).
- Bidirectionality paragraph and change request 12 marked met (index rows 5, 6, 9, 12 list SCL / LSC peer); status cells set to In Review.
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.
