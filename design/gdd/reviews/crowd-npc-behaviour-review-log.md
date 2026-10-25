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
