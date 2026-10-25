# Review Log: Crowd & NPC Behaviour

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: XL (cross-cutting Core system, 6 formulas, 12 upstream and 11 downstream systems, a Crowd ADR (StateTree/follower, motion parity) plus the 50-character cost spike; producer should verify before sprint planning)
Specialists: systems-designer (with game-designer and qa-lead lenses), ai-programmer (with unreal-specialist and performance-analyst lenses), creative-director (senior verdict). Run sequentially, one sub-agent at a time (pipeline budget).
Blocking items: 14 | Recommended: 12
Summary: The core holds: identity-blind NPCs, red-herring rates set from the evidence each tell should carry, and the player-reproducibility rule. The problems were leaks and broken arithmetic, not concept. The C7/IVP rulings had not been applied (k = 2.0, free bush, capped dwell), and the 20 s chain-gap guard silently cut every accepted tell rate to about 80 %, which would break `P_hide ≤ 1/6` even after r_N was raised. Several implementation paths leaked players (handover braking, NPC-only slow walking, steering around and glancing at concealed pawns, one-sided conversations, non-human motion texture), and AC-10, 12, 15 and 29 could never pass. The creative-director ruled every fix a local amendment (NEEDS REVISION, not MAJOR); all were applied in place; a full re-review is required because many BLOCKING ACs changed.
Prior verdict resolved: First review
Findings:
- [BLOCKING] R7 bush bullet / T5 / F2 / Hiding state / Edge Cases / Tuning / AC-15 / OQ1 / CR 1: still the rejected design (k = 2.0, P* 0.20, free bush, dwell capped by Concealment) -> fixed per conflict C7 and IVP ruling A: occupancy- and player-position-blind choice, same re-entry cooldown and exit-side rule, uncapped log-normal stay (no cap, no timer on hiders), r_N 0.19/min (`CrowdBushIntervalSec` 316), `P_hide ≤ 1/6`, bush row exempt from NpcCount scaling, `CrowdBushDwellSigma` added, OQ1 and CR 1 recorded as resolved/denied.
- [BLOCKING] R7 / F2: chain-gap guard drops draws, accepted rates fall to ~80 % (bush 0.152, P_hide 0.198) -> fixed: discarded draw restarts the tell's clock at gap end (no pile-up), F2 compensation `a_j = r_j / (1 − (Λ − r_j)·T_gap)` with a validity bound; R8 tells inside the gap become glances; one window everywhere (creative-director ruling a).
- [BLOCKING] R7 decoy / T3 / F2 / Tuning / Edge Cases: conflict with Objectives CR6 (occupied fixtures skipped, r_P doubled, band 0.7-0.85, interval 14,286, missing `CrowdAmbientTouchMaxSec`) -> fixed: occupied courts included (free stand point), r_N 0.0032 (18,750 s), T3 0.65-0.75, touch keys 0.8/1.5 s added.
- [BLOCKING] R3.1 / Player Fantasy / OQ3: 0.8/0.9 pace unreproducible on keyboard (NPC-only behaviour, classifier BA ~0.85) -> fixed: interim NPC pace 1.0 only, C4 inactive, Fantasy line rewritten, T16 row; keyboard walk-slower modifier left as a pending user decision (ruling c).
- [BLOCKING] R3 / R6 G3 / R15: engine AIController zeroes pitch unless focus is a pawn [verified 5.8.4]; G3 pitch impossible -> fixed: crowd AIController writes Look yaw and pitch into control rotation; SCL change request for 16-bit `RemoteViewPitch16`.
- [BLOCKING] R3: same commands give legality, not human-looking motion (smooth pursuit curves, free-angle walking glances) -> fixed: R3.4 motion-parity rule (device-weighted human input profiles), AC-27/28 gate, emulation vs SCL smoothing -> ADR (OQ11) (ruling d).
- [BLOCKING] R10 / R11 / States: handover frame order contradictory; stale commands after commit; ~0.25 s brake after possession -> fixed: R10.5 control epoch, possession at next frame start, SCL holds last Move until first ServerMove; AC-23 and new AC-38.
- [BLOCKING] F5 / AC-26: omitted tick overhead, server CMC and 70-pawn case; aligned 4 Hz decisions; GPU named for a CPU metric -> fixed: c_t term, N ≤ 70, phase offsets (R12), CMC assigned to SCL budget, named Insights scopes, Test build, recorded CPU, 70-pawn 30 fps run; host budget OQ10; no cut lowers NpcCount without CD review.
- [BLOCKING] R14 / AC-29: draw counts world-dependent, AC-29 would fail -> fixed: counter-based draws keyed by (ordinal, decision type, index), streams in the Director, fixed-timestep harness; AC-34 added.
- [BLOCKING] F4 / AC-12 / T9: F4 counted whole pauses (15.4 %) but stall cover needs stretches already ≥ 4 s (8.6 %); per-viewer clause infeasible; regulator could exceed 100 % -> fixed: metric corrected, defaults 45 % / U(4, 16) (f 12.5 %), clamp 0.9, per-viewer reported not gated.
- [BLOCKING] AC-10: 10 min too short (decoy count zero in ~20 % of runs) -> fixed: 600 simulated minutes, 99 % Poisson intervals on accepted rates.
- [BLOCKING] AC-02: zero rejections unachievable under same-drain contention -> fixed: contention exempted and logged; rejection handling rule in R3; MoveTo/path-following bans instrumented.
- [BLOCKING] R2 / R3.3 / G2: concealed pawns used as steering obstacles and glance targets (occupancy leak); G2 lacked line of sight -> fixed: perception excludes concealed pawns and requires LOS; bush HideVolume is the obstacle; AC-36.
- [BLOCKING] C3 / AC-17: one-sided conversation with a still player is player-only -> fixed: reciprocal conversation (start/reply probabilities from logged face-back rates, both face within 60°), AC-17 at 1,100 trials with TOST (ruling e).
- [RECOMMENDED] R5 / AC-13: pause → conversation chains exceed `CrowdMaxIdleSec` -> fixed: stationary-stretch watchdog.
- [RECOMMENDED] R6 G6 / T13 / AC-16: 4.5 % of glances gave 0.39/min not 0.125; contextual holds counted -> fixed: independent `CrowdHeadHoldIntervalSec` 480 s clock; G3/G5 excluded from T13.
- [RECOMMENDED] R7 brace: flinch per trigger unspecified ("sprint past" test) -> fixed: the T4 clock decides when; triggers only time a due brace.
- [RECOMMENDED] R8 / F2: startle and panic budgets had no formula; 0.3 s floor -> fixed: offline residual formula; reaction delays from logged human reaction times, timed from the stimulus.
- [RECOMMENDED] F1: Market cap claim false at range edges; ρ_obs = 0 divides by zero -> fixed: layout-load ratio check (TDC CR), density floor, live N.
- [RECOMMENDED] F3 / Tuning: hard 30 s broke the socket ceiling, low end broke the LSC floor after cancels -> fixed: hard 40-90 s plus two cross-field constraints; table ranges aligned with knobs.
- [RECOMMENDED] Edge Cases: missing nav failure, stuck, stale callbacks, Halt with pending command, same-drain rejection, off-route pawns, yield-test claim -> fixed (progress monitor, frozen navmesh, Halt latch, LSC route-coverage CR).
- [RECOMMENDED] AC-03, 07, 14, 18, 24, 27, 30, 31: tolerances, kill rate over cancel capacity, uniform test, distance strata, numeric handover tolerances, run length, list-existence check, AC-31 split and moved -> fixed.
- [RECOMMENDED] Missing ACs (yield rate, phase gating, determinism, exit cancel, runtime non-adaptation) -> AC-32 to AC-37 added.
- [RECOMMENDED] Dependencies: Concealment, Objectives, Assassination listed "Not written"; IVP "In Design" -> fixed statuses.
- [RECOMMENDED] Game Feel: emergence every ~8 s (is ~6 s), reaction 0.3-0.8 s, standing share -> fixed.
- [RECOMMENDED] F6: SE ignores participant clustering -> fixed: larger of closed form and cluster-bootstrap SE.
Specialist disagreements: (1) ai-programmer "0.3 s is faster than any human" vs creative-director "overstated (primed humans 0.2-0.25 s); the issue is remote-observer delay": resolved by drawing delays from logged player reaction times, no raised floor. (2) ai-programmer's keyboard-and-mouse virtual-input fix vs creative-director: match the real input-device mix, and weigh an SCL turn-smoothing filter on all pawns in the ADR (OQ11). (3) Proposed chain-gap deferral (main review) rejected by the creative-director as a new spike tell; replaced by clock restart plus F2 compensation. (4) Earlier game-designer wish for a bush posterior of 0.30-0.40 stays declined (C7, IVP ruling A).
Pending user decision (not applied, scope): keyboard "walk slower" modifier (adds a player control); until decided, NPCs walk only at full pace.
New change requests (Cross-References 1, 5, 7, 15-20): IVP (reconcile "F7 on dwell" with the F5 context note); Concealment (cite T5/AC-CNB-15 in CON-R14); SCL (16-bit pitch figures, hold last Move after possession, budget AI CMC, turn-smoothing option); Round (epoch, next-frame possession, mailbox cap); LSC (route coverage, pre-baked edge paths); Generator (no runtime navmesh rebuild); TDC (new keys and constraints); Telemetry (one shared pipeline).
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/concealment-bushes.md ed2d2ba1c4cfd9081cd01d62ce6b2a592668b0c5
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md d8fd90df42d37734c67ecba262aefd4ca1d353b8
Reviewed-Content-Hash: design/gdd/objectives-chain.md 3611facfb79f5c98605dfcf5314126da5602c6c3
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 13f87f4eae08c99906862f29de8e94cc59796e3d
Reviewed-Content-Hash: design/gdd/level-socket-contract.md f318f06c15dd6194b5816961985e04c18cd1e5ce
Reviewed-Content-Hash: design/gdd/session-travel.md 5a2f3d67fb9453d71eb9602429781de31d79777c

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: XL (unchanged: cross-cutting Core system, 6 formulas, 14 upstream and 11 downstream systems, ADR-0016; producer should verify before sprint planning)
Specialists: none (lean mode, single-session analysis; arithmetic of F1-F6 re-derived and the F2 chain-gap compensation checked by a Monte Carlo of the eight tell clocks)
Blocking items: 5 | Recommended: 6
Summary: All 14 first-round blockers are resolved in the text (C7 bush rules, chain-gap compensation, Objectives CR6, pace rule lifted by the user decision, pitch path, motion parity, handover epoch, F5, counter-based streams, F4, AC-10/02, perception exclusions, reciprocal conversation) and the document honours every confirmed design rule (informant respawn by takeover near the spawn, blind-attack of bush occupants, NpcCount 50, brace pose shared, walk-slower modifier). The remaining problems are a pair of statistical ACs that cannot reliably pass, one fact that contradicts SCL F2, and two unspecified rules F3 and AC-18 depend on. All were revised in place; a verdict of NEEDS REVISION stands until a third review confirms them.
Prior verdict resolved: Yes (14 of 14 blocking items from the 2026-10-24 full review verified fixed; the 12 recommended items verified applied)
Findings:
- [BLOCKING] Acceptance Criteria AC-CNB-15: required the measured NPC bush rate to be >= r_N,req, but the shipped 0.1905/min sits 0.25 % above the requirement while 1,140 expected entries give about 3 % noise, so a fixed seed fails about half the time -> fixed: (a) deterministic config check `60 / CrowdBushIntervalSec >= r_N,req`, (b) measured rate inside the 99 % Poisson interval of the configured rate.
- [BLOCKING] Acceptance Criteria AC-CNB-17: 1,100 trials per arm gives about 76 % TOST power at p = 0.5 for the 5-point margin, so a correct build fails about a quarter of fixed seeds (the rest of the document sizes at 1,236) -> fixed: 1,236 per arm.
- [BLOCKING] Core Rules R3.2: arrival stop distance "42 cm at 140 cm/s" contradicts SCL F2 (53 cm at 140, 42 cm at 126, 34 cm at 112) -> fixed: stop distance of the current pace, SCL figures quoted.
- [BLOCKING] Core Rules R8: which 8 of the qualifying NPCs respond when more than `CrowdStrikeResponderLimit` qualify was unspecified (AC-18's distance strata depend on it) -> fixed: lowest counter-based priority draw keyed by (ordinal, strike index), never distance, appearance or kind.
- [BLOCKING] Core Rules R9.1 / F3: the ambient schedule's arrival law was unstated while F3's replacement delay (Exp(lambda), mean 16.2 s, p95 28.4 s) and AC-CNB-08 assume Poisson -> fixed: Poisson process at lambda city-wide, slot given to a uniformly drawn socket, cooldown-blocked sockets take the 1 s retry.
- [RECOMMENDED] Formulas F5: T_disp 3 ms/s in the table vs 3.6 in the example (60 pawns x 60 calls x 1 us); 30 fps average stated 1.24 ms but the model gives 40.6 ms/s = 1.35 ms -> fixed (both under the 1.5 ms budget).
- [RECOMMENDED] Formulas F2 example: uncompensated bush rate stated 0.159 / P_hide 0.158; simulation gives about 0.16 / 0.155 -> fixed; the linear compensation `a_j = r_j / (1 − (Λ − r_j)·T_gap)` verified by simulation (accepted/target 0.998-1.013).
- [RECOMMENDED] Edge Cases: NpcCount extreme said every red-herring rate scales by 50/NpcCount; the bush row is exempt (F2) -> fixed.
- [RECOMMENDED] Tuning Knobs: R5 says "parameters are knobs", but R8 shares, pause shares, leg length, gaze timings, avoidance distances, lane offset and reaction-delay range have no key (coding standard: gameplay values data-driven) -> listed at the head of Tuning Knobs, change request 22 and OQ13 added; registration is a TDC schema edit (not applied here).
- [RECOMMENDED] Formulas F2 / Tuning Knobs `CrowdBushIntervalSec`: the bush target is met with zero margin (P_hide = k·b exactly at r_N,req, r_P 0.3); if the measured r_P exceeds 0.3 the ceiling fails until the interval is retuned. A margin default (for example 300 s, 0.20/min) would absorb about 5 % of r_P drift; not applied (the default is registered at 315 and referenced by IVP and Concealment; decide in the main session).
- [RECOMMENDED] Header: Status, Last Updated and Design Review lines updated for this round.
Cross-doc follow-ups (main session):
- systems-index.md row 11: keep status Needs Revision / In Review (this review is not an approval); Camera System (soft) still absent from the Depends On cell (CR 21).
- design/registry/entities.yaml: `npc_bush_use_rate_target` notes say "0.625 * r_P at defaults", which is the 40-NPC figure; at NpcCount 50 it is 0.633 * r_P (IVP F5 and Concealment F2 already say 0.633). Consider a bush-interval margin (see above) and registering the unkeyed constants (CR 22).
- shared-character-locomotion.md: change-request table row "Crowd CR 3 / Keyboard walk slower: PENDING-USER — not added" (line ~656) is stale beside the APPLIED row for the 2026-10-25 user decision (line ~677).
- tuning-data-config.md: register the keys from CR 22 and the `CrowdMotionProfileAsset`, `CrowdFullStateTreeAsset`, `CrowdRestrictedStateTreeAsset` rows already in the registry; confirm the cross-field constraints listed in Tuning Knobs are in its validator.
- ADR-0002: still needs the `CrowdSchedule` timer kind (ADR-0016 §6), which the Poisson schedule in R9.1 relies on.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/concealment-bushes.md a7075ec56f19ea43b29d27c845e98d280ed6797f
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/objectives-chain.md ca652db3c9bb041fb11d0af194650394ce5140b0
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md b16480784d0fc81db34f2d01c988e88b0e5f7134
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
Reviewed-Content-Hash: design/gdd/session-travel.md 4fd55e704b2b7660cd0b3f4f9294f63523bd578a
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/round-match-framework.md ab477269caa8bacab048c8719d3029a90a7ce89e
Reviewed-Content-Hash: docs/architecture/adr-0016-crowd-ai-architecture.md a555315844f9e5c7aa96ad4602c8ae35d69c4e05

## Closeout cross-doc pass 2026-10-25
- D7: bush target default 300 s (from 315 s) to absorb r_P drift (registry updated); Crowd consumes the SCL `StopMinSec` key (D2).
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.

## Cross-review fix 2026-10-25
- C-W8: Concealment is Hard in the Concealment-needs-Crowd direction, Soft in the other; Downstream gets its own Concealment row.
- C-W10: `CrowdDecoyEarlyReleaseMaxSec < ObjectiveInteractSec` named I-CNB-6 (reject); Max safe 2.2-3.0 -> 2.2-2.9.
- C-W17(f): `CrowdMaxIdleSec` safe 15-25 -> 20-25 (I-CNB-2).
- C-W23: F3 `N_E` range 12-16 with the outputs at 12 (lambda 0.150/s, D mean 16.7 s, p95 30.0 s).
- C-W36: G1 uses SCL R8.9's single trigger definition.
- D-W7 / D27: G1 hold accounting (idle-scan holds are not T13), camping accepted, T13 row cites SCL R8.8's definition, AC-CNB-16 excludes G1 holds.
- D-W8 / D28: Tell Matrix row T18 (Ping: no animation, no replicated pawn state, no tell); AC-CNB-30 range and Cross-References updated; Pings listed Downstream (Soft).
- R3.1 notes the pace set is an SCL R5.1 constant held by no key.

## Cross-review 2026-10-25b (/review-all-gdds since-last-review)
- NpcCount safe range 45-50 in F-table variables (follow-up b).
Report: design/gdd/gdd-cross-review-2026-10-25b.md. Status line unchanged.

## Approved 2026-10-25 (D35)
- Approved 2026-10-25 (D35): main-session decision under user authorisation after /review-all-gdds 2026-10-25b (CONCERNS, 0 blocking); remaining warnings stay listed in the GDD and design/gdd/gdd-cross-review-2026-10-25b.md.
