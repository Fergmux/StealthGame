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
