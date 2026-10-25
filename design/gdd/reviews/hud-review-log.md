# Review Log: HUD

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (one presentation system with 10 formulas, 12 upstream and 5 downstream dependencies; needs the UI ADR (OQ8) shared with Teammate Markers and Camera, and a shared server-clock offset service; producer should verify before sprint planning; the index's M estimate looks low)
Specialists: ux-designer + ue-umg-specialist + accessibility-specialist + qa-lead + systems-designer + ue-replication-specialist (one combined panel brief), creative-director (senior verdict)
Blocking items: 13 | Recommended: 22
Summary: The structure and pillar fit are sound: three carriers only, no element about a non-ally, no clock, and a leak suite to prove it. What failed was timing. In several places an outcome reached the screen before the player's own contact frame, which breaks ASN-R6/ASN-R23 and P3's verdict moment: the `Eliminating` freeze, hold cancels, a stale contact time in F2, dropped late results, and a victim's banner. All of these were fixed in place. One schema item is now a pending user decision. Creative director: NEEDS REVISION, not MAJOR; every fix is local.
Prior verdict resolved: First review
Findings:
- [BLOCKING] HUD-R6 / OQ3 / CR-HUD-11: the defender plate used the interim "arrival + hold-back" rule and asked Assassination for a new field, but ASN-R19/R23 had already settled it. Fixed: the plate is held Pending until the defender's own Counter write and shown at its contact time; brace End at `FHoldStatus.ServerEndTime`; arrival + hold-back is kept only as the fallback; OQ3 closed; CR-HUD-11 withdrawn.
- [BLOCKING] States / HUD-R8 / HUD-R9 / HUD-R13: `Eliminating` "froze" elements when the slot-time own State arrived, and hint, prompt and ring read the raw state. This was a pre-contact tell for victims and a pre-plate tell for an exiling attacker. Fixed: new HUD-R6a `PresentedOwnState` flips only at `OnRoleChangePresented` or the exile card; outcome-caused owner data (State, hold cancels, `NotNow`, tally, `bExiled`) is held to that moment; interactive elements hide at the fall start (CD D1).
- [BLOCKING] F2: assumed `FActionState.ContactServerTime` is present when the `FStrikeResult` RPC arrives (UE does not order the two) and could read the previous strike's contact time; "own pawn" breaks after an exile release. Fixed: the result stays Pending until the matching strike write on the pawn captured at the press (CD D10); hold-back fallback; `ContactServerTime` in `FStrikeResult` is pending a user decision (OQ15).
- [BLOCKING] F2 / HUD-R9: late results were dropped after `HudReplayWindowSec`, losing the verdict, the tally step and the exile card (contradicts PEN-R16). Fixed: never dropped in live play; the replay window applies to rebuilds only; the card waits; `OnRemovalCardEntered` is exposed for Camera (CD D9).
- [BLOCKING] F10 / States: a victim whose death ends the round saw the banner before its own fall; the States table and F10 disagreed (card hold vs entrance). Fixed: F10 floor (own plate, own fall) plus a `HudBannerMaxDelaySec` 1.6 cap on the extras; no removal card when the round ends (CD D8, D12); E04's final fill shown before the banner (CD D2).
- [BLOCKING] HUD-R5 / HUD-R13 / UI table: the Result slot hosted both the returning countdown and the informant plate in IA. Fixed: the countdown moved to the Notice slot; the priority list was completed.
- [BLOCKING] HUD-E13: the prompt read team live-site data, contradicting HUD-R13 and Objectives OC-6. Fixed: public geometry plus `PresentedOwnState` only; CR-HUD-12 sent to IIF UI-3.
- [BLOCKING] HUD-R4: the camera fade between L0 and L1 cannot be an engine camera fade (UMG composites after it). Fixed: HUD-hosted fade layer driven by Camera, with named ZOrders 10-100; CommonUI only for menus; CR-HUD-10 updated.
- [BLOCKING] HUD-R13 / VA-HUD-1: the live-site Marigold ring was a colour-only cue. Fixed: four cardinal ticks plus a ≥ 3:1 contrast requirement (CD D6); art bible CR item 11.
- [BLOCKING] F1: the area budget assumed 16:9, and the knob floors sat below the default layout. Fixed: checked at 16:10 (7.6 %), lint `limit ≥ A_persist`, hard range 7-12; 4:3 and 5:4 stated as exceeding the cap (OQ9); `y_notice` corrected.
- [BLOCKING] AC-HUD-03 / -05 / -13: asserted "unchanged" while a cooldown sweep moves, and used a circular F2 oracle. Rewritten with a determinism rule, control runs, anim-notify oracles, ±1-tick tolerances, and the victim mid-tend and hint cases.
- [BLOCKING] AC-HUD-01: the struck enemy could be the last rival, which ends the round and changes the dumps. Fixed: 2v2 with E2 alive.
- [BLOCKING] Tuning Knobs / Dependencies: stale `TeammateMarkerDeathHoldCapSec` (removed by the Teammate Markers review) and "F6 `t_end`". Fixed: TM-R14 `OnAllyMarkerRetired`; dependency statuses refreshed; Round `EndRoundAll` `RoundEnded` explicitly ignored.
- [RECOMMENDED] F4 / F5 / F7 / F8 output ranges: corrected the card lower bound, the "2:00" maximum, the countdown's first visible value, and the overflow wait `ceil((b−c)/c)·dwell` (with a c = 1 example and AC).
- [RECOMMENDED] HUD-R12: "Brace" label plus bound glyph (D3); End presented at `ServerEndTime`; sweep not drawn while Protected; the numeral holds at 0:01 until server end + RTT/2 (D5).
- [RECOMMENDED] HUD-R7 / F6: an informant's frozen tally drops the danger triangle (D4); `L` = 5 and n = 255 cases added; the tally is noted as pending OQ-NKP-1.
- [RECOMMENDED] HUD-R16: contrast ratios 4.5:1 and 3:1; floors kept for PC-first (D7, OQ14); `HudReadHoldScale` knob (D11); subtitles, screen-reader and toggle scope stated; AC-HUD-18 made BLOCKING at the VS gate and the tool named.
- [RECOMMENDED] VA-HUD-6: a distinct result cue per outcome (after contact) for players who cannot see the plate (CR-HUD-15, Event Feedback VS).
- [RECOMMENDED] HUD-R13: latency icon reads own net-connection lag with hysteresis (0.8×, ≥ 2 s); the third-person hint flag is consumed only when shown; an informant press before the card collapses is dropped.
- [RECOMMENDED] HUD-R10: wording corrected (the line enters with TM-R14's retire, close to the blow, not after it); follows TM's pending `RetireAtServerTime` if adopted.
- [RECOMMENDED] HUD-R17: duplicate-seq ring cleared on reconnect, travel and epoch change; `ClientCommandSeq` compared as `(PossessionEpoch, Seq)`.
- [RECOMMENDED] Edge Cases: one slewed server-clock offset shared with animation, Camera and Markers replaces the "re-sync > 0.25 s" rule (CR-HUD-14, OQ8); bystander banner residual (I-ASN-4) stated.
- [RECOMMENDED] Tuning Knobs: invariants I-HUD-5 (`ExileBeatSec ≥ plate entrance + 0.5`), I-HUD-6 (card in before black), I-HUD-7; I-HUD-3 raised to reject; `HudBannerMinPlateSec` hard max 1.5.
- [RECOMMENDED] Acceptance Criteria: AC-HUD-08 boundaries; AC-HUD-15 split into a Logic resolver test plus screenshots across aspects; AC-HUD-20 settled and ordered-event comparison; new AC-HUD-24 (layers and fade), -25 (prompt parity and ring ticks), -26 (brace states).
- [RECOMMENDED] OQ8 / engine notes: DPI `ShortestSide` rule, per-style text-floor resolver, `ProjectWorldLocationToWidgetPosition` after the camera tick, `Collapsed` not `Hidden`, C++-sampled timelines (all flagged unverified against 5.8.4).
- [RECOMMENDED] Change requests: CR-HUD-6 (Penalty Game Feel cross-fade, informant triangle), CR-HUD-7 and CR-HUD-9 marked applied, new CR-HUD-12 (IIF UI-3, UI-2, UI-7), CR-HUD-13 (Concealment row), CR-HUD-14 (`FActionState.Seq` mapping, hold-cancel gating, clock service), CR-HUD-15 (Cues), CR-HUD-16 (Round staged `Ended`); art bible CR items 11-14.
Specialist disagreements:
- The panel suggested raising the text floors to 24 px for TV. The CD kept 18/22/28 px for a PC-first MVP and opened OQ14.
- Replication preferred `ContactServerTime` in `FStrikeResult`. The CD chose Pending-until-matching for MVP and recorded the field as a pending user decision (schema).
- The CD scoped the per-outcome cue and the shared clock service as change requests to their owners (Cues VS, technical-director), not HUD MVP scope.
- The CD qualified the fade layer: HUD owns the widget, Camera keeps the timing.
Change requests raised (not applied to other GDDs): CR-HUD-1 to -6, -8, -10, -12 to -16 (see hud.md Dependencies).
Sibling requests applied in hud.md:
- Penalty: tally and card copy; late-or-never case.
- Assassination: ASN-R23 gating.
- Camera: `GetViewState()`, the 3P hint on the first live frame, the countdown on the neutral view.
- Teammate Markers: `OnAllyMarkerRetired`, `HudSafeMarginPct`.
- Objectives: `RivalPrelimCount`; no site plate.
- Concealment: prompt verb only.
- IVP: three carriers.
- Round: no clock; `FRoundResult` banner; `EndRoundAll`.
- IIF: prompt and brace rules.
Pending user decisions added to active.md: `ContactServerTime` in `FStrikeResult` (schema_changes; OQ15).
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md 7d51824a23e99b89b1cd389e93c7ed087fd943bd
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md f3b8f1457ac5ab28d647fa1943ff043b7587c6bf
Reviewed-Content-Hash: design/gdd/teammate-markers.md 22764362d4196ffde99920c0983ae1b0f92cf719
Reviewed-Content-Hash: design/gdd/camera-system.md d8d88307900d60f7a48bc103c344ad88c27c402c
Reviewed-Content-Hash: design/gdd/objectives-chain.md d9d026202feea7e41786c2a2f762cf046fe29e22
Reviewed-Content-Hash: design/gdd/concealment-bushes.md 8340b1dca731c7a25bcbf91d81ec6dc4c1ccca05
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 1277ef682327977500e3667b7efde48695c916b5
Reviewed-Content-Hash: design/gdd/round-match-framework.md 5d1f316101eefb952dc5c9f42e9247d2cbcd613d
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 4f420c05e0a8e4119ade3ef7fff68f39eaff6e78

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (unchanged: 10 formulas, 12 upstream and 5 downstream dependencies; needs the UI ADR already written as ADR-0021; the index's M estimate still looks low)
Specialists: none (lean mode, single-session analysis)
Blocking items: 4 (3 fixed in place, 1 cross-document and open) | Recommended: 5
Summary: All 13 blockers of the 2026-10-24 full review are confirmed resolved in the text (PresentedOwnState, F2 pending and never-drop rules, F10 floor and cap, Notice-slot countdown, fade layer, ring ticks, F1 at 16:10, rewritten ACs). The user decisions of 2026-10-25 (`ContactServerTime` in `FStrikeResult`, tally re-push, `T_due`, round summary, reconnecting lines, ping slot) are applied consistently with the sibling GDDs, and every formula example was recomputed and matches. What remained were stale remnants of the pre-decision text and one test gap, plus registry drift. Verdict stays NEEDS REVISION because the entity registry holds only 15 of the 21 HUD keys that TDC says are registered; no design-level defect was found.
Prior verdict resolved: Yes (NEEDS REVISION 2026-10-24; all 13 blocking items verified against the current text)
Findings:
- [BLOCKING, fixed] Edge Cases (Timing): "FStrikeResult arrives before the matching FActionState: the plate stays Pending" contradicted F2 and ASN-R19, where the attacker's plate reads `FStrikeResult.ContactServerTime` and never waits; rewritten so only the defender's `FDefenceResult` and a time-less result wait.
- [BLOCKING, fixed] HUD-R9 (Informant killed): said the user's confirmation of `InformantKilled` "stays open" although Penalty OQ-NKP-12 and CR-18 were approved 2026-10-25 (the Cross-References table and the status line already said so).
- [BLOCKING, fixed] Acceptance Criteria: HUD-R9's `InformantKilled` path (no card, countdown after Camera's fade), the lost-death-state cap path (F4) and the round-ends-during-removal path had no AC; added AC-HUD-29 and the coverage line.
- [BLOCKING, open, cross-doc] Entity registry: only 15 HUD keys are registered (`HudPersistentAreaLimitPct`, `HudResultPlateOutSec`, `HudRemovalCardHoldSec`, `HudRemovalCardOutSec`, `HudFeedLineCount`, `HudObjectiveStepSec` are missing) while TDC lists 21; the `result_show_time` formula entry still shows only the pre-decision expression (no result-carried `ContactServerTime` / fallback split).
- [RECOMMENDED, fixed] States table: HUD-E22 (ping rings) was missing from `ActiveAlive` and `InformantBodied`, and HUD-E17 (first third-person hint, listed for IB in UI Requirements) from `InformantBodied`.
- [RECOMMENDED, fixed] HUD-R13 priority list: the strike hint was "dropped, never queued" while a higher notice held the Notice slot, which is ambiguous for a level-triggered predicate; stated as level-triggered with priority-only slot choice (kind-blind, HUD-R3).
- [RECOMMENDED, fixed] Dependencies and Cross-References: Informant Pings listed as "Designed (pending independent review)"; it is In Review (NEEDS REVISION 2026-10-25, revisions applied). Also noted that the HUD reads Pings' content (two-way link, not a cycle). Tuning Knobs: `HudLatencyWarnRttSec` note updated (IIF retired its duplicate key).
- [RECOMMENDED, noted open] F1: the A_persist lint sums only the three reference clusters; informant brackets (E09) and the latency icon (E19) have no footprint, and the 16:10 margin is 0.4 points (7.6 % of 8 %). Open item added to F1 for the UX spec; no numbers invented.
- [RECOMMENDED, open] Hard ranges for `HudResultPlateMaxHoldBackSec` and `HudOwnRemovalHoldCapSec` start at 1.2 while I-HUD-1 and I-HUD-2 need at least sum + 1/60 (up to 1.217 when `StrikeContactSec + BushStrikeEmergeSec` is at Assassination's 1.2 cap); harmless because the invariants reject, but TDC may want the hard minimum raised to 1.25.
- [ADVISORY, pre-existing] OQ5 (exile word collision: "Exiled" card vs "Target exiled" plate) stays a blocker for the HUD UX spec, and OQ11 conditions (qa-lead pass, art-director sign-off on CR-HUD-8, audio cue list) stay before HUD epics.
Specialist disagreements: none (no specialists).
Registry values: searched the document for the HUD key names and for the neighbour keys it restates (`DefenceCooldownSec` 60, `DefenceDurationSec` 4, `StrikeContactSec` 0.4, `BushStrikeEmergeSec` 0.35, `AttackAttemptLockoutSec` 1.75, `DeathFallSec` 1.2, `NoPawnFadeSec` 0.5, `StrikeNetLeadSec` 0.15, `ExileBeatSec` 0.8, `InformantRespawnDelaySec` 15, `NpcKillStrikeLimit` 3, rival-signal 30-195 s): no conflicting value found against the registry or the owner GDDs.
Dependency graph: all 12 upstream GDD files exist; downstream Event Feedback & Cues, Settings & Accessibility, Onboarding and Post-Round Reveal are not written (stated in the GDD).
Cross-doc follow-ups (main session):
- Registry (`design/registry/entities.yaml`): add the six missing HUD keys (values, ranges and units as in hud.md Tuning Knobs); update `result_show_time` to the result-carried `ContactServerTime` primary input with the matching-write fallback for `FDefenceResult`.
- ADR-0021 layer table (`docs/architecture/adr-0021*.md`): L0 content should list the team ping rings and edge rings (HUD-E22, Informant Pings) and L3 the round summary (HUD-E21); its L1 content should list the stage card by its element split if the UX spec requires it.
- TDC: optionally raise the hard minimum of `HudResultPlateMaxHoldBackSec` and `HudOwnRemovalHoldCapSec` to 1.25 (see the recommended item above).
- Systems index row 18: HUD status stays In Review / Needs Revision; the main session decides the string. The row's Depends On could add Informant Pings only if Pings' own row stops listing HUD (avoid a two-way hard cycle in the index).
- UX spec (`/ux-design`, design/ux/hud.md): footprints for E09 and E19 and a re-run of the F1 budget; OQ5 wording.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md d89bac971a0a37d20024e0b32fb89559467cf5ab
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/teammate-markers.md a99e945ffb425c4da6bd1094f01b6fd9ced99ff6
Reviewed-Content-Hash: design/gdd/camera-system.md d304d3eb9b8ca59389014b2fd64e1318e1200f5a
Reviewed-Content-Hash: design/gdd/objectives-chain.md ca652db3c9bb041fb11d0af194650394ce5140b0
Reviewed-Content-Hash: design/gdd/concealment-bushes.md a7075ec56f19ea43b29d27c845e98d280ed6797f
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/round-match-framework.md ab477269caa8bacab048c8719d3029a90a7ce89e
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 6f5397e9f8ef2526f8d2c6c107100ba7a2db673b
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 63db04c802c6db06eb0ba3dac93f65fb9fa98068
Reviewed-Content-Hash: design/gdd/session-travel.md 1e185cf338c1b17880a81bc7057dcb11fa8a0ee4

## Closeout cross-doc pass 2026-10-25
- D11: removal card wording fixed to title "Informant" for every cause, subtitles "Too many wrong targets" (exile) and "Eliminated for the round" (assassinated/reversed), so OQ5 no longer collides with the Roster language rule.
- D8: ranges of `HudResultPlateMaxHoldBackSec` and `HudOwnRemovalHoldCapSec` now start at 1.25 s (TDC hard minimum raised to match).
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.
