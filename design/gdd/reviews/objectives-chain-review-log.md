# Review Log: Objectives Chain

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system Feature: 5 formulas, 9 upstream/peer dependencies, one new IVP R3 row, ADR for the chain asset class and C-TeamView field; producer should verify before sprint planning)
Specialists: combined panel (game-designer, systems-designer, level-designer, qa-lead lenses; one sub-agent per pipeline fan-out rule), creative-director (senior verdict)
Blocking items: 8 | Recommended: 12
Summary: The chain's structure (OC-4 to OC-11, SP-1 to SP-8) holds; the blocking items were all local. OC-12 still described the `RivalStageStep` fact the IVP review denied (ruling B), F3 set the Crowd decoy rate from a 12-minute round that F1 contradicts, and SP-4 overstated what camping costs. The creative-director approved the rewritten rival signal (`RivalPrelimCount`, fresh per-completion delay U(30, 165) s keyed by the round-global completion ordinal, one global 30 s release grid, on by default) as the CD approval IVP R3/OQ4 requires, reopened Crowd CR6 (r_N about 0.0057), and adopted `TendCueDelaySec` (1.0 s) to close an instant-cue probe tell. Revisions were applied in place; an independent re-review is required (Re-review policy).
Prior verdict resolved: First review
Findings:
- [BLOCKING] OC-12 / OC-5 / States / UI 3 / Game Feel / Edge Cases: `RivalStageStep` (keyed by team and step, monotonic `max()` delivery) denied by IVP ruling B -> fixed: `RivalPrelimCount` 0-2, fresh per-completion delay on `StageSignal` keyed by completion ordinal, release only on the global grid `RivalSignalReleaseQuantumSec` 30 s from `Active`, unordered count (may jump 0 -> 2), timer only while a release is pending; "pending CD-GDD-ALIGN" text removed.
- [BLOCKING] F4 / Tuning Knobs / AC-OC-22: bound ignored the quantum and failed at the slow-round rate (old 40-140 s: LR 3.56 at λ_c 0.0033) -> fixed: `W_eff = Max − Min + Q`, requirement `W_eff ≥ 163 s`; defaults Min 30, Max 165, Q 30 (LR 2.38); I-OC2 rewritten (warn < 163, clamp < 100, `Max ≥ Min + 30`).
- [BLOCKING] F3 / Crowd CR6 / AC-OC-21: `R_P` 0.30 came from the non-binding 12-min target; with F1's ~9-min round, Crowd's 0.0032 gives P 0.81 -> fixed: F3 bands full-length tends with `T_round` from F1 (`R_full` 0.40), `P_short` reported separately (alarm > 0.90); CR6 reopened to r_N ≈ 0.0057 (interval ≈ 10,500 s).
- [BLOCKING] Edge Cases / Dependencies: `Dropped(PostLatch)` conflicts with Round's queue-item lifecycle -> fixed: `Rejected_Latched` throughout; IIF wording change requested (CR 1f).
- [BLOCKING] Dependencies (HUD row): "live-site ids for the site plate" contradicted OC-15/UI 5 (no plate) -> fixed: ids used only for the ring colour after the cue delay.
- [BLOCKING] SP-4: "camping guesses 1 in 3-4" false (5 players can cover ~4 candidates) -> fixed: blanket camping stated as an unprofitable hunt strategy with its costs, no penalty/timer/reveal; LSC change request (`FinalCandidatesMinCount` > `TeamSize`); OQ 9.
- [BLOCKING] AC-OC-16: "no Objectives timer exists" conflicted with a release quantum -> fixed: timer only while a release is pending.
- [BLOCKING] AC-OC-09/10: written for the denied signal -> fixed: grid release, ordinal-keyed delay independence (satisfies AC-IVP-40).
- [RECOMMENDED] OC-6 / Tuning Knobs: instant Marigold/Bone cue let probes release ~0.3 s after Begin, unlike any decoy -> fixed: `TendCueDelaySec` 1.0 s, invariant I-OC3, AC-OC-32 (creative-director approved; flagged for the user).
- [RECOMMENDED] OC-7 / OC-8 / Edge Cases: fifth Begin at a fully occupied fixture unspecified -> fixed: `Reject(NotNow)` from public occupancy, same for NPCs; IIF and Crowd change requests.
- [RECOMMENDED] SP-5 / OQ 1: rival count 2 may cue a hunt -> fixed: telemetry named, SP-7 (1) fallback.
- [RECOMMENDED] F2 / F1: probe exposure ignored -> fixed: `N_probe`, `t_probe` terms; `o` note.
- [RECOMMENDED] F5: "about 6 completions" (max is 5); no per-team gap -> fixed: text corrected, `G_team` reported, team-labelled example.
- [RECOMMENDED] AC-OC-08 / AC-OC-12: "byte-identical ± one frame", "identical in kind and size class" untestable -> fixed: byte-exact owner trace ± 1 server tick; zero Objectives-related properties/RPCs to opponents.
- [RECOMMENDED] AC-OC-06 / AC-OC-20 / AC-OC-21 / AC-OC-23 / AC-OC-26: vague or under-powered -> fixed: table-driven counts, stop-to-Complete metric, ≥ 60 rounds or simulation, team-labelled fixtures, AC-IVP-41 fixture shared.
- [RECOMMENDED] Acceptance Criteria: missing coverage -> fixed: AC-OC-28 (ClueTooBroad), 29 (no team view before Active), 30 (feint = decoy path), 31 (decoys include tended courts), 32 (cue delay).
- [RECOMMENDED] Dependencies / Cross-References statuses stale -> fixed: statuses updated to the 2026-10-24 review states; index row 15 Depends On gains Crowd and Session (CR 9).
- [RECOMMENDED] OC-5 / OC-8 / Interactions: ruling D consistency -> verified and stated: the team view is presentation only; ingress accepts any court (OC-6, OC-11 already treat locked sites as ordinary courts).
- [RECOMMENDED] Stall Pressure: SP-8 listed before SP-7 -> fixed: reordered.
- [RECOMMENDED] Player Fantasy: rival tick absent -> fixed: one line added.
Specialist disagreements: the panel and the main review proposed keeping the applied Crowd rate (0.0032) pending data; the systems lens and creative-director chose to reopen CR6 because it was computed from a wrong denominator (resolved: reopened). The level and qa lenses pushed for structural anti-camping; the game-designer lens and creative-director keep it as corrected text plus an LSC change request (no mechanic). The earlier CD-GDD-ALIGN approval of `RivalStageStep` is superseded by IVP ruling B and this review's CD approval of the revised shape.
Change requests not applied (other owners' GDDs; listed in the GDD's Cross-References): IVP R3 `RivalPrelimCount` row, close OQ4, L34 Mit (CR 3); Crowd r_N ≈ 0.0057 and safe range, full-fixture reject, expose early-release keys (CR 6); IIF `TendCueDelaySec`, full-fixture reject, `Rejected_Latched` wording (CR 1 d-f); LSC `FinalCandidatesMinCount` > `TeamSize` (CR 8); Round `StageSignalReleased` log entry (CR 4); HUD/art bible rival bar may fill two steps at once (CR 7).
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md d8fd90df42d37734c67ecba262aefd4ca1d353b8
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md d5494369a7ed46d882c863563dd3228f5d2281ab
Reviewed-Content-Hash: design/gdd/round-match-framework.md 5d1f316101eefb952dc5c9f42e9247d2cbcd613d
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 92b2933d7e7100547848bb6450f1aea75ece2cae
Reviewed-Content-Hash: design/gdd/teammate-markers.md fa6747053069b90ca709dd4b0b3aa5ab56eb2efa
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md f06247d43d0cf957747d3c8f7003659075ce1f30
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 90b88857c2a98e951356a90c5878a80f231c47ca
Reviewed-Content-Hash: design/gdd/level-socket-contract.md f318f06c15dd6194b5816961985e04c18cd1e5ce
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/concealment-bushes.md dae06ab7f91d65fbb7e2c82c6d4fa3e7b710d877
Reviewed-Content-Hash: design/gdd/session-travel.md 5a2f3d67fb9453d71eb9602429781de31d79777c
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (unchanged: 5 formulas, 9 upstream/peer dependencies; producer should verify before sprint planning)
Specialists: none (lean, single session)
Blocking items: 1 | Recommended: 8
Summary: Second-round review. All 8 blocking and 12 recommended items of the 2026-10-24 full review are verified resolved in the text (rival signal in IVP ruling-B shape, F3 re-based on F1 and `NpcCount` 50, F4 `W_eff`, `Rejected_Latched`, SP-4 wording, AC rewrites); the F1, F3 and F4 arithmetic was recomputed and holds. One new blocking item: OC-14 still said the hold is `ObjectiveInteractSec` "unless a definition overrides it", which contradicts the user decision of 2026-10-25 that removed `InteractSecOverride` (a per-definition hold time is a timing oracle). It and the recommended items were fixed in place; a fix is not an approval, so an independent re-review decides.
Prior verdict resolved: Yes (all 8 blocking items confirmed resolved)
Findings:
- [BLOCKING] OC-14: "unless a definition overrides it" contradicted the confirmed removal of `InteractSecOverride` -> fixed: no per-definition hold time exists, with the oracle rationale; AC-OC-01 gains a static check that `FObjectiveDef` has no hold-time field.
- [RECOMMENDED] F2 Output Range: `E_court` minimum 6.5 s is wrong at the stated minima (1.5 + 0.5 + 2.0 + 1.5 = 5.5) -> fixed to 5.5.
- [RECOMMENDED] F4 variable table: `W_eff` "≥ 100 (clamp)" contradicted I-OC2, AC-OC-22 and TDC CR-6 (reject, never repair) -> fixed.
- [RECOMMENDED] Tuning Knobs `RivalSignalReleaseQuantumSec`: "must exceed 15.5 s" while the hard minimum is 10 s -> reworded "should exceed", safe range 20-45 respects it.
- [RECOMMENDED] OC-2 `CompletionsRequiredCount`: no rule for values above 1 (cancel, second completer) -> values above 1 rejected at `Preparing` like `ObjectivesPerStageCount`; AC-OC-01 extended.
- [RECOMMENDED] F3 N_npc: range 32-60 outside TDC safe range 35-50, and `r_N` is tuned for 50 (at 35 NPCs the same interval gives `P_full` 0.77) -> range corrected, rescale-with-`NpcCount` rule stated in F3 and Tuning Knobs interactions.
- [RECOMMENDED] Cross-References change request 6: still ended at r_N 0.0057 / 10,500 s -> added the 2026-10-25 rescale to 13,125 s (r_N 0.0046).
- [RECOMMENDED] Open Question 8: written before the MVP team ping (user scope decision 2026-10-25) -> rewritten (ping is a team-only place ring; Pings GDD to confirm it carries no stage or role data); Dependencies note lists Pings as a peer.
- [RECOMMENDED] Acceptance Criteria: informants' share of the team view and rival count untested -> AC-OC-33 added.
- [RECOMMENDED] Dependencies statuses are 2026-10-24 snapshots -> labelled, index named authoritative.
Specialist disagreements: none (no specialists).
Cross-doc follow-ups (main session, not applied): (1) systems-index "Depends On" list item 15 (line about Objectives Chain in the numbered dependency list) still omits Crowd and Session, which the table row 15 already lists; (2) entity registry has no entries for Objectives keys `ClueMatchMaxCount`, `ObjectivesPerStageCount`, `bRivalSignalEnabled`, `ObjectiveStallGapDefectSec`, `ObjectiveFinalDwellDefectSec` (TDC says 11 Objectives keys); (3) Informant Pings & Team Comms GDD: confirm a ping carries no stage, card or court-role data and no rule keyed to a court's role (Objectives OQ 8); (4) game-concept.md lines about a "median duration near the ~12 min target" (VS hypothesis) conflict with F1's about 9 min round, which Objectives treats as the non-binding target; consider rewording to "about 9 min, 7-11".
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 242977e3dcc0185347fd1053741e6f305c1e42fa
Reviewed-Content-Hash: design/gdd/round-match-framework.md 97e464757d41252679f8163bd3f90f68d509d23b
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md d89bac971a0a37d20024e0b32fb89559467cf5ab
Reviewed-Content-Hash: design/gdd/teammate-markers.md f7107192fea0ee4dd407278a981b33fb8c31ac93
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 63db04c802c6db06eb0ba3dac93f65fb9fa98068
Reviewed-Content-Hash: design/gdd/concealment-bushes.md a7075ec56f19ea43b29d27c845e98d280ed6797f
Reviewed-Content-Hash: design/gdd/session-travel.md f6e4a60b76f7e3e99045662d1636bb3e226926f1
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 6f5397e9f8ef2526f8d2c6c107100ba7a2db673b
Reviewed-Content-Hash: design/gdd/hud.md 224977e0b36ac631f3ab286857b5b707178bc4dd
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8

## Closeout cross-doc pass 2026-10-25
- Cross-doc follow-ups from the lean re-review applied (ClueMatchMaxCount use in the ClueTooBroad edge case, Generator OQ9 closure by D4, concept median vs F1 ~9 min).
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.
