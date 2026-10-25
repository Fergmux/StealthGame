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
