# Review Log: NPC-Kill Penalty & Informants

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system integration: 5 formulas, 11 upstream and 7 downstream systems, 20 change requests to siblings; ADR homes for the server-only store, the `FStrikeResult` extension, the informant timer item and the card-entrance event; producer should verify before sprint planning)
Specialists: systems-designer (with game-designer, ue-replication-specialist and qa-lead lenses), creative-director (senior verdict, rulings R1-R12). Run sequentially, one sub-agent at a time (pipeline budget).
Blocking items: 9 | Recommended: 15
Summary: The core holds: strike accounting, the single takeover path (C1), the kill-by-kill informant invariant and the Owner-only tally are sound, and the tally decision is leak-sound on the attacker's connection. The defects were (1) the exile moment had no single clock (Penalty, HUD and Camera each timed it from a different anchor, so a late RPC could black the screen before the card) and a killed informant had no own presentation; (2) the document claimed more secrecy than it has: a witness-side residual built into the locked rules was unnamed, and "never learns" overclaimed; (3) sibling drift (Crowd, Character Pool, Round, Teammate Markers, HUD statuses). The creative director ruled every fix a local amendment or a change request.
Prior verdict resolved: First review
Findings:
- [BLOCKING] PEN-R14 / AC-21: witness-side residual (a rival's strike on the just-exiled released body returns `WrongTarget`; its prompt exit) reveals an enemy removal, unnamed; AC-21 "byte-identical opponent capture" could never pass -> fixed: PEN-R14 names three residuals (attacker removal timing, witness side, informant-side "Returning" overrun), CR-9 R10 text and CR-20 to IVP L24; AC-21 rewritten to class/condition/cadence parity (CD R1).
- [BLOCKING] PEN-R16 / Game Feel / UI-NKP-3 / AC-38: three different exile anchors (contact frame vs HUD plate show time vs Camera contact timer) -> fixed: HUD's card-entrance event is the only clock; Camera `ViewRelease(StrikeLimit)` and the exile cue fire from it; `ExileCardTimeoutSec` (2.0 s) fallback; CR-10 amended (Camera's applied timer wording superseded); AC-38 injects 0/300/900 ms lateness and a dropped RPC (CD R3).
- [BLOCKING] Game Feel: "cross-fade" contradicted HUD-R9 push-off (CR-HUD-6) -> fixed: plate pushed off over 0.25 s stepped; timings stated relative to the card entrance.
- [BLOCKING] VA-NKP-1 / VA-NKP-5 / AC-26: exile cue "never reads bExiled" yet fires with the bExiled-gated card -> fixed: exile cue triggered by the card-entrance event; per-kill cue selection reads only `TallyAfter` and L.
- [BLOCKING] PEN-R16: killed informant's own presentation unspecified (no `State` change; a binding-keyed fade would black before the fall) -> fixed: `InformantKilled` release at `DeathServerTime + DeathFallSec`, no card, countdown after the fade; CR-18 to Camera/HUD; AC-NKP-44 (CD R2).
- [BLOCKING] Cross-References CR-4/6/7: Crowd edge case and CP-12b table row "adopt first, then release", Round R18 "no TakeNpcCard" contradict PEN-R4/R9 -> fixed: CR-4, CR-6, CR-7 extended with exact replacement text (CD R4).
- [BLOCKING] Teammate Markers OQ10 unanswered -> fixed: CR-17 (released at exile; marker exits with ReleaseToCrowd timing; reappears on the adopted pawn).
- [BLOCKING] PEN-R3 / HUD-R7: a per-strike C-TeamView tally write could beat the plate RPC -> fixed: per-strike tally only in `FStrikeResult`; owner re-push `{tally, LastResultSeq}` only on reclaim/rejoin/reset; HUD gate and monotonic; CR-19, CR-12(b), AC-NKP-46 (CD R6).
- [BLOCKING] AC-35: R = 23.82 failed "23.8 ± 0.002" -> fixed with F5 restatement: unrounded inputs, ± 0.01.
- [RECOMMENDED] F2: floor max 4.7 s with the bush variant (I-ASN-3), not 4.3 -> fixed; TDC cross-field check added to Penalty CR-11.
- [RECOMMENDED] F3/F5: `T_out` an upper bound, walking out of the own quarter is not denial; F5 "poor trade" not robust -> fixed: `T_deny = D + L_pick + d_back/v`, R as exchange rate defined for c_N > 0 (high R near c_N -> 0 is P2 working), playtest metric informant share vs 2 × q (CD R9).
- [RECOMMENDED] Player Fantasy / F5 / Edge Cases: "never learns" overclaims -> fixed: "never from the kill itself; at most, later, that one of their wrong targets was not a strike, never which" (CD R10).
- [RECOMMENDED] Player Fantasy: 15-35 s black screen and home-quarter re-entry unstated (largest P3 cost) -> fixed: fantasy rewritten, Pillar service states the cost (CD R8); optional VS ambient bed left as a pending user decision.
- [RECOMMENDED] F4: range given only at defaults -> fixed: L = 5 gives 0.73, NpcCount floor 6 gives 0.996; P_res a lower bound (graced bodies).
- [RECOMMENDED] Edge Cases: L = 1 reachable through the TDC clamp -> hard minimum 2 NOT applied (schema change, always-ask; OQ-NKP-11); edge case added.
- [RECOMMENDED] UI-NKP-4: "Returning…" overrun is an informant-side sensor -> accepted as residual 3; static "Returning" with no count-up (CD R7).
- [RECOMMENDED] PEN-R2: listen-server host memory caveat (IVP R10/L21) -> added.
- [RECOMMENDED] AC-36 / new AC-43: owner-side autonomous -> simulated proxy switch mid-clip -> AC-NKP-43 added; AC-17, 36, 37 marked provisional on spikes S1/S14; AC-37 gains a control pawn cadence check.
- [RECOMMENDED] AC rewrites: AC-01 NPC-killer column; AC-03 field-wise normalised compare incl. allies + AC-39 residual boundary; AC-04 differential wire test; AC-05/08/34 split (AC-40/41/42); AC-09 scope; AC-10 F2 example case and bush cap; AC-13 grace vs expiry; AC-18 three new cross-field warnings; AC-19 widget state; AC-20 reconnect; AC-22 `PawnLost`; AC-23 report-only with minimum sample; AC-29 trace split to AC-47; AC-32 tally unchanged; coverage map rebuilt.
- [RECOMMENDED] PEN-R5 / CR-4: exile exit adds to the city exit rate -> fixed: takes the next scheduled exit slot; AC-NKP-45; OQ-NKP-9 closed (prompt exit kept, CD R1c).
- [RECOMMENDED] Dependencies / Cross-References: stale statuses (HUD exists and answers CR-12 via CR-HUD-6; Crowd and Camera In Review; Generator applied CR-13 via AC-G38) -> fixed; OQ-NKP-6 timing half closed.
- [RECOMMENDED] Edge Cases: disconnect as informant conversion, informant dropping to bait a strike, the 2 s strike-free window -> added and accepted.
- [RECOMMENDED] Naming: Penalty CR-11 collided with TDC's own rule CR-11 Latching -> every reference disambiguated.
Specialist disagreements: (1) panel proposed a pure HUD-event trigger for the exile; creative director added the `ExileCardTimeoutSec` fallback so a lost RPC cannot hang the view. (2) Panel offered "hold the countdown at Returning in 1" or accept; creative director chose accept-without-amplifying (a held "1" would read as a bug). (3) Panel framed c_N -> 0 in F5 as a modelling flaw; creative director ruled it intended (Pillar 2) and documented, not fixed. (4) Panel's cadence-parity AC extended by the creative director into an exit-budget requirement (CR-4, AC-45).
Pending user decisions (not applied): OQ-NKP-11 `NpcKillStrikeLimit` hard minimum 2 (schema change); OQ-NKP-12 `InformantKilled` reason value in Camera/HUD contracts (possible schema change) and an optional VS ambient bed while awaiting (scope change); OQ-NKP-1 tally decision (re-affirmed by the creative director, R12).
New change requests (Cross-References): CR-4, 6, 7, 9, 10, 11, 12, 13 amended; CR-17 Teammate Markers, CR-18 Camera + HUD, CR-19 Roster, CR-20 IVP added.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/hud.md 00414a034133f413de29ba689b03b83c155fcb16
Reviewed-Content-Hash: design/gdd/camera-system.md 8c0dbdd6b248a1b4df5a7a96d0f35ae366d57528
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 7d51824a23e99b89b1cd389e93c7ed087fd943bd
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md d5494369a7ed46d882c863563dd3228f5d2281ab
Reviewed-Content-Hash: design/gdd/teammate-markers.md 22764362d4196ffde99920c0983ae1b0f92cf719
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md d8fd90df42d37734c67ecba262aefd4ca1d353b8
Reviewed-Content-Hash: design/gdd/objectives-chain.md 986f8de4aca918e560156ceb328862adad93f3e5
Reviewed-Content-Hash: design/gdd/procedural-city-generator.md b1cf4a2523f0fabae99c1d6091a8a57e515cffd3
Reviewed-Content-Hash: design/gdd/round-match-framework.md 5d1f316101eefb952dc5c9f42e9247d2cbcd613d
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/level-socket-contract.md f318f06c15dd6194b5816961985e04c18cd1e5ce
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 8bf1dcd1acd4e451528d64eb7491c82b2479c26e
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 13f87f4eae08c99906862f29de8e94cc59796e3d

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (unchanged: 5 formulas, 11 upstream and 8 downstream systems, 20 change requests)
Specialists: none (lean mode, single-session analysis; GDD read in full, registry and TDC/ASN/HUD facts grepped)
Blocking items: 4 | Recommended: 3
Summary: Second-round review. All nine first-round blockers are resolved in the text (one exile clock with timeout fallback, `InformantKilled` presentation, three named residuals, tally re-push gated by `LastResultSeq`, CR-4/6/7/17-20 wording, AC-35 tolerance), the user's 2026-10-25 decisions are reflected (tally confirmed, hard minimum 2, `InformantKilled`, `T_due` carrier, ping MVP, audio bed deferred to VS, `NpcCount` 50), and F1-F5 and AC-NKP-35 recompute correctly. Four small defects remained (testability and consistency); all were revised in place, but a revision is not an approval, so the verdict stands until a re-review of the revised text.
Prior verdict resolved: Yes (NEEDS REVISION 2026-10-24: 9 blocking items all resolved; recommended items resolved or deliberately left as pending user decisions that were then decided)
Findings:
- [BLOCKING] AC-NKP-18: the F2 floor-warning case (D = 5 with DeathFallSec 2.0, NoPawnFadeSec 1.5) cannot warn: 1.2 + 2.0 + 1.5 = 4.7 <= 5, and the hard ranges can never violate the predicate -> revised: that input now expects no warning, and the predicate is tested directly with an injected out-of-range D = 4.0.
- [BLOCKING] AC-NKP-09: "one card move per TryReleaseNpc attempt" contradicted PEN-R9 step 6 and AC-NKP-30 (no card move for a failed takeover) -> revised: exactly one card move over the whole respawn, one pick per commit attempt.
- [BLOCKING] Cross-References CR-11: still stated `ExileCardTimeoutSec >= ExileBeatSec + 0.5`, superseded by W-NKP-5 (TDC and the knob row: StrikeContactSec + BushStrikeEmergeSec + ExileBeatSec + 0.3) -> revised to the W-NKP-5 formula; W-NKP-4 noted as a guard that cannot fire inside the hard ranges.
- [BLOCKING] Edge Cases: "P's 3rd NPC kill is reversed by a bracing NPC" is not possible (a strike on a braced NPC resolves Reversal, not Kill; ASN-R7) -> rewritten as k = L - 1 striking a bracing NPC: Reversal, no tally, no strike, P eliminated ReversedAgainst.
- [RECOMMENDED] AC-NKP-31: "Returning…" (ellipsis) contradicted UI-NKP-4's static "Returning" -> revised.
- [RECOMMENDED] AC-NKP-05 parameterised L = 1 to 5, but L = 1 exists only behind `bTestAllowsStrikeLimitOne` -> revised (L = 2 to 5 plus the flagged L = 1); coverage map gained AC-NKP-48; F4 labels L = 5 as outside the 2-4 safe range.
- [RECOMMENDED] Header and Quick reference: Informant Pings still "(VS)" and Status/Last Updated stale -> refreshed (MVP since 2026-10-25; Pings In Review).
Registry values: searched NpcKillStrikeLimit, InformantRespawnDelaySec, ExileBeatSec, StrikeOutReleaseGraceSec, ExileCardTimeoutSec, NpcCount; no conflict (3 / 2-5 / 2-4, 15 / 5-60 / 10-20, 0.8, 2.0, 2.0, N = 50 in F4).
Dependency graph: every named GDD exists (informant-pings-team-comms.md included); Event Feedback, Post-Round Reveal and Telemetry are Not designed (VS/Alpha, by design).
Cross-doc follow-ups (main session):
- design/registry/entities.yaml `informant_respawn.chain` still reads "PickRespawnPawn -> TakeNpcCard -> TryReleaseNpc -> TakeOverCrowdPawn"; the GDD, ADR-0019 and ASN follow pick, pre-check, TryReleaseNpc, TakeOverCrowdPawn, then TakeNpcCard (only after Applied). Update the registry chain.
- design/gdd/tuning-data-config.md W-NKP-4 (`1.2 + DeathFallSec + NoPawnFadeSec <= InformantRespawnDelaySec`) can never fire inside the hard ranges (max 4.7 <= min D 5); keep as a guard or drop it, and align its test with AC-NKP-18.
- design/gdd/systems-index.md: row 14 status should read Needs Revision (second review) until the revised GDD is re-reviewed; Informant Pings row is In Review.
- Dependency-row statuses in this GDD for sibling GDDs ("Reviewed, NEEDS REVISION") will go stale as parallel re-reviews land; refresh them at the next pass.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 89d049074c81e71b16f1269736cbe2016d977f9c
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 2ae4886c85a5145d50d8cab36f30cc374c722621
Reviewed-Content-Hash: design/gdd/hud.md 224977e0b36ac631f3ab286857b5b707178bc4dd
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 6bf80120e5a3d8ace5b1ae83ad802eef537b16a2

## Closeout cross-doc pass 2026-10-25
- OQ-NKP-13 added: Penalty owns the Pillar-1 review of the informant respawn-region restriction (taken over from Character Pool OQ-14, shared with LSC R5b).
- D10 (InformantKilled reason) lives on `RetireBinding` in the Roster log, not as a transition reason; Penalty passes it, nothing replicates.
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.

## Cross-review fix 2026-10-25
- C-W6: Concealment added to Upstream (Hard, as Concealment states it); Bidirectionality note lists it; index amendment requested of the main session.
- C-W8: Crowd & NPC Behaviour added to Downstream (soft, mutual with the upstream row).
- C-W29: PEN-R8 limited to the applied `Active -> Informant` causes; the informant-kill respawn is scheduled by Penalty itself in the kill slot.
- C-W17 / C-W18: `ExileBeatSec` safe range 0.75-1.2 narrowed to 0.75-0.95 (cites I-HUD-6 and W-NKP-5); `ExileCardTimeoutSec` safe 1.5-2.5 narrowed to 2.0-2.5; `StrikeOutReleaseGraceSec` W-NKP-3 pairing stated.
- D-W10 (D29): Player Fantasy now names the card ("Informant") and keeps "exile" as the design term for this removal alone; "voice and text" changed to voice only (text chat is Vertical Slice); OQ-NKP-4 notes the rival-kill plate is "Target eliminated".
- C-W9: "no ADR exists yet" corrected (ADR-0018, ADR-0019 exist); status cells untouched.

## Approved 2026-10-25 (D35)
- Approved 2026-10-25 (D35): main-session decision under user authorisation after /review-all-gdds 2026-10-25b (CONCERNS, 0 blocking); remaining warnings stay listed in the GDD and design/gdd/gdd-cross-review-2026-10-25b.md.
