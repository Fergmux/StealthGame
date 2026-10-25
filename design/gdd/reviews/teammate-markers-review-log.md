# Review Log: Teammate Markers

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: M (one presentation system, 8 formulas but all local geometry and timing, 10 upstream and 6 downstream dependencies, needs the UI ADR (OQ6) and the ADR-001 carrier decisions; producer should verify before sprint planning — matches pending OQ12 S->M)
Specialists: ue-replication-specialist + security-engineer + ux-designer / ue-umg-specialist + systems-designer + qa-lead + game-designer (one combined panel brief), creative-director (senior verdict)
Blocking items: 9 | Recommended: 9
Summary: The design is sound and keeps all three locked rules (markers only on teammates' clients, enemies cannot obtain them, informants keep allies' markers); security found no opponent-observable path. The problems were local: a phase/state contradiction at round end, a death hold mistimed against Assassination's death write, a broken behind-camera clamp and corner overlap, a null-`PawnRef` ambiguity a UE client cannot resolve, a hatch source that broke the same-frame claim, and leak ACs that could not prove the locked rule. All fixed in place without schema changes; three schema requests are pending user decisions.
Prior verdict resolved: First review
Findings:
- [BLOCKING] TM-R5 / TM-R8 / AC-TMK-10, 16: Roster `EndRoundAll` makes every record `RoundEnded`, so "RoundEnded → no marker" contradicted markers staying in `Ended` -> fixed: keyed on round phase (last fill frozen in `Ended`; no marker outside `Ended`); AC-10 and new AC-16b.
- [BLOCKING] TM-R6 / F6 / AC-TMK-12: death-state extension fired ~half the time at defaults (death write at slot + 0.25 s, bush strike + 0.60 s, hold 0.25 s) -> fixed per CD P2: extension, `TeammateMarkerDeathHoldCapSec` and its invariant removed; marker "leaves as the blow lands"; Player Fantasy anchor 3, Game Feel, AC-12/13 rewritten; exact `RetireAtServerTime` is pending user decision (OQ3, schema).
- [BLOCKING] F3 / AC-TMK-17: behind-camera `d` has magnitude ≤ 1 so `min(1, …)` put the marker at screen centre; exactly-behind went top-centre for a high anchor -> fixed: bottom-biased direction (`k_b` 0.25), no cap, divide guard, hysteresis; examples recomputed.
- [BLOCKING] TM-R4 / TM-R6 / States: a UE client reads an unmapped reference as null, so "none" vs "unresolved" was unimplementable -> fixed per CD P1: TM-R4a (feed entry ⇒ `Fed`; null without feed ⇒ hold that absorbs transient mapping; resolved beats feed), stated atomic-delivery invariant (CR-11 to IVP); `bPawnBound` pending (OQ13); AC-TMK-30.
- [BLOCKING] TM-R5 / AC-TMK-10, 11: `IsAllyBodyHidden ∨ IsConcealed` let the server-timed flag show the hatch before the body hid -> fixed: tracked entries use the CON-R6b latch alone; `IsConcealed` only for fed entries.
- [BLOCKING] TM-R6.4 / TM-R7: a different pawn returning during the hold would slide the marker -> fixed: same pawn cancels the hold; different pawn exits and a new marker enters; AC-12(e), AC-32.
- [BLOCKING] AC-TMK-05: 32-bit handle memory scan gives chance matches and legitimate C-Public copies -> rewritten as reflection walk plus 64-bit test canary with positive control.
- [BLOCKING] AC-TMK-01, 02, 04, 07: 2v1 left B's module inert; AC-02 had no method; AC-04 too narrow; AC-07 moving ally -> 2v2 with exact set equality; reflection dump with partition rule and allowlist; any-channel search plus host-on-team-B run; stationary allies.
- [BLOCKING] F4 / AC-TMK-18: perimeter sort had a seam at the top-left corner and arc spacing under-separated corner glyphs -> fixed: cyclic order opened at the largest gap, symmetric relaxation, `L = W + H + e`; examples and AC cases added.
- [RECOMMENDED] TM-R8: re-defined "live view" -> now defers to Camera `GetViewState()` (CR-3 applied by Camera).
- [RECOMMENDED] CR-HUD-7 -> applied: `TeammateMarkerEdgeInsetPct` removed, F3 reads `HudSafeMarginPct`; new TM-R14 `OnAllyMarkerRetired(PublicHandle)` (fires once, also undrawn, immediate if no marker) with AC-TMK-31.
- [RECOMMENDED] F5: receive-time staleness hides a stationary fed ally under change-only replication; invariant fails below 0.6 Hz -> keyed on a feed sequence (CR-11c, pending OQ14), inactive until then; invariant bounded.
- [RECOMMENDED] VA-2: zero-lag projection under-specified -> paint-time projection, rendered world FOV, single DPI scaling, glyph size includes keyline (outline hollow 6 px at the floor, checked by AC-21); AC-22 uses an engine-rendered reference sphere.
- [RECOMMENDED] F8 / UI: label focus hysteresis and edge placement -> added; edge markers under HUD plates recorded as OQ15 (HUD UX spec).
- [RECOMMENDED] AC-TMK-29 / game-designer radar and clustering concern -> per CD P3 kept edge markers; telemetry (centre dwell, edge time, ally pair distance vs baseline) and a CD trigger; development-only console-variable distance cap for the A/B.
- [RECOMMENDED] Acceptance Criteria: missing direct pawn switch, draw order and F7 numerics; AC-16 and AC-25 chained -> AC-32, AC-33 added; AC-16 split into 16a/16b, AC-25 into 25a/25b; AC-13 given a repeated early-exit measure.
- [RECOMMENDED] Edge Cases / OQ10: exile handling -> resolved by Penalty PEN-R4.2 (release, no death); edge case rewritten; OQ10 closed.
- [RECOMMENDED] Dependencies / CRs / Cross-References: stale statuses (Penalty, HUD, Camera, Concealment, IVP), CR-3 and CR-6 applied elsewhere, Assassination no longer read at run time -> updated; CR-11 to CR-15 added; systems index row 17 Depends On gains Concealment (Bushes), Camera System, Tuning Data & Config (CR-8).
Specialist disagreements: (1) ue-replication preferred a mandatory `bPawnBound` schema flag; CD ruled the feed-inferred rule (TM-R4a) sufficient for MVP with the flag as a pending robustness option. (2) Main review proposed raising the hold to 0.40 s; systems-designer showed it fails for bush strikes and `StrikeContactSec` > 0.55; CD chose to cut the extension. (3) game-designer called all-time edge markers radar-like and proposed a distance cap; CD kept them (anchor 2, art bible 7.x) pending playtest telemetry. (4) CD noted the extension cut could be read as a scope change; the pipeline classified it as a design fix (timing detail of a broken formula, no milestone content moved) and logged it; the user can reverse it via OQ3.
Change requests raised (not applied to other GDDs): CR-1 (Roster `IsConcealed` in R6), CR-2 (IVP feed `Pos` = feet), CR-4 (IIF `ShowTeamNames`, pending OQ11), CR-5 (TDC keys and feed-stale invariant), CR-7 (art bible outline keyline), CR-9 (Assassination row 17 status; no run-time read), CR-10 (SCL), CR-11 (IVP feed trigger, atomicity, sequence), CR-12 (`bPawnBound`, pending), CR-13 (`RetireAtServerTime`, pending), CR-14 (HUD dependency row wording).
Pending user decisions added to active.md: `RetireAtServerTime` (OQ3), `bPawnBound` (OQ13), feed sequence (OQ14) — all schema_changes.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md d8fd90df42d37734c67ecba262aefd4ca1d353b8
Reviewed-Content-Hash: design/gdd/concealment-bushes.md dae06ab7f91d65fbb7e2c82c6d4fa3e7b710d877
Reviewed-Content-Hash: design/gdd/camera-system.md 8c0dbdd6b248a1b4df5a7a96d0f35ae366d57528
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md f06247d43d0cf957747d3c8f7003659075ce1f30
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md d71a926c02dcf13bee80b0c553b724807e51952d
Reviewed-Content-Hash: design/gdd/hud.md 00414a034133f413de29ba689b03b83c155fcb16
Reviewed-Content-Hash: design/gdd/round-match-framework.md 5d1f316101eefb952dc5c9f42e9247d2cbcd613d
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 90b88857c2a98e951356a90c5878a80f231c47ca

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: M (unchanged: one presentation system, 8 formulas of local geometry and timing, 10 upstream and 6 downstream dependencies; producer should verify before sprint planning)
Specialists: none (lean, single-session second-round re-review)
Blocking items: 1 | Recommended: 6
Summary: All 9 blockers of the 2026-10-24 review are resolved and the user decisions of 2026-10-25 (`RetireAtServerTime` and feed `Seq` adopted, `bPawnBound` declined, `ShowTeamNames` to VS, reconnecting label suffix in MVP) are applied consistently with the Roster, IVP, registry and HUD. The re-review found one new formula blocker, an F3 discontinuity at the camera plane that contradicted the GDD's own continuity claim and AC-TMK-17, plus AC/rule mismatches introduced by the `RetireAtServerTime` revision. All were revised in place; the verdict stays NEEDS REVISION because a revision is not an approval and F3 needs a fresh read.
Prior verdict resolved: Yes (9 of 9 blocking items verified fixed; F1, F2, F3 examples, F4 examples, F5, F7 recomputed and correct)
Findings:
- [BLOCKING] F3 / TM-R9 / AC-TMK-17: the behind-camera bias `max(−D_u, k_b)` applied from the camera plane onward, so a level ally crossing the side plane jumped about `k_b · h_x` = 212 px (top or middle of the right edge to below centre), contradicting "moves continuously" and the AC's 180°/s bound -> fixed: bias ramp `σ = clamp(−D_f / ε_b, 0, 1)`, `ε_b` 0.5, `d_y = −D_u + σ·max(0, k_b + D_u)`, zero-vector guard, crossing example (1810, 540/650/785), AC-17 extended (F3 behind-left and exactly-behind examples unchanged at σ = 1).
- [RECOMMENDED] TM-R6 step 2 / F6: step 2 omitted the `T_Rmax` guard that F6 has, named no clock, and left a stale or leftover `RetireAtServerTime` able to extend a later hold -> fixed: guard added, clock named (`EstimatedServerNow()`, ADR-0002 §9), value latched once at `t_r` from the retiring update only; AC-TMK-12 gained cases (g) zero, (h) +3.0 s cap, (i) past value, (j) stale value.
- [RECOMMENDED] AC-TMK-31 / TM-R14: AC said `OnAllyMarkerRetired` fires at `t_r + T_hold`, wrong for a bush strike (`t_end` of F6) -> fixed to `t_end`.
- [RECOMMENDED] States and Transitions: `Entering → Exiting` skipped the TM-R6 hold, and `Stale` had no exit for a retirement while undrawn although TM-R14 says the event still fires -> fixed (`Entering → Held`, `Stale → Held`).
- [RECOMMENDED] AC-TMK-23: F8 hysteresis (8 px) and "edge markers never take focus" had no test -> added to the AC.
- [RECOMMENDED] Cross-References / Dependencies counts / header: stale text (session-travel OQ7 "not added", "4 pending user, 0 declined", Status and Last Updated) -> refreshed.
- [RECOMMENDED] Tuning Knobs / registry: five of the seven `TeammateMarker*` / `TeammateLabelFocusSize` keys are not in `design/registry/entities.yaml` (only `TeammateMarkerFeedStaleSec` and `TeammateMarkerRetireHoldSec` are; both match the GDD) -> cross-doc follow-up, not edited here.
- [NICE] AC-TMK-21 "a tester names all three states correctly": state a tester count and threshold at the first playtest; `T_Rmax` (1.5 s) is a constant derived from I-ASN-3 and should be re-derived if that bound moves.
Cross-doc follow-ups (main session): (1) register `TeammateMarkerAnchorCm`, `TeammateMarkerEdgeGapSize`, `TeammateMarkerFadeSec`, `TeammateLabelFocusSize`, `TeammateMarkerLargeMult` in the registry (TDC row 119 counts 7 keys); (2) the systems index row for Teammate Markers should read Needs Revision with this review's date; (3) Informant Pings reuses the F3 clamp: its GDD (`informant-pings-team-comms.md`) should pick up the camera-plane ramp (`σ`, `ε_b`) if it copies the formula; (4) HUD UX spec (`/ux-design`) still owns OQ15 (edge markers under plates).
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/concealment-bushes.md a7075ec56f19ea43b29d27c845e98d280ed6797f
Reviewed-Content-Hash: design/gdd/camera-system.md d304d3eb9b8ca59389014b2fd64e1318e1200f5a
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md d89bac971a0a37d20024e0b32fb89559467cf5ab
Reviewed-Content-Hash: design/gdd/hud.md 05f2d8dac61a985f97a45a83f56d07c421369931
Reviewed-Content-Hash: design/gdd/round-match-framework.md ab477269caa8bacab048c8719d3029a90a7ce89e
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 63db04c802c6db06eb0ba3dac93f65fb9fa98068
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 6f5397e9f8ef2526f8d2c6c107100ba7a2db673b
Reviewed-Content-Hash: design/gdd/shared-character-locomotion.md 6df3690b6fb980c603a0a0be65d9a6224c0b57b9
Reviewed-Content-Hash: design/gdd/session-travel.md a0a052e3e4fbdc5dea8e841e14d1bc2d52c88e45
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md dfb73369b11821c543d7841b5cec4d3965988d04
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8

## Closeout cross-doc pass 2026-10-25
- Dependencies and Cross-References status cells set to In Review; F3 bias ramp is mirrored in Informant Pings VA-PNG-2.
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.

## Cross-review fix 2026-10-25
Source: design/gdd/gdd-cross-review-2026-10-25.md; rulings D23. Status lines and status cells untouched.
- C-W2: Dependencies Upstream gains the HUD row (`HudSafeMarginPct`, UI scale, L0 layer); the HUD lists Teammate Markers as a dependent. Index row 17 stays with the main session.
- C-W33: TM-R1 now lists the six teammate-entry fields `{PublicHandle, State, PawnRef, IsConcealed, RetireAtServerTime, bReconnecting}` (also in the Overview data layer, the Roster Interactions row and the Roster Upstream row); `ReconnectEndCause` is HUD-only and the label suffix follows the flag alone.
- S-W2: TM-R6 step 6 (markers in `Held` or `Exiting`, or retired after `Ended`, complete at once; `OnAllyMarkerRetired` fires once; other markers keep their frozen fills, which is the existing Ended rule); Edge Cases `Ended`; AC-TMK-34.
- S-B1 (TMK side): TM-R11 suffix persists while the flag stays true (informant still away, `Active` ally killed while disconnected, superseding rejoin loading) and drops on any clear; AC-TMK-26 extended; the stale edge case that said grace expiry creates no HUD removal line was corrected (an `Active` ally's expiry converts it, so HUD-R10 shows the removal line; an informant ally's expiry shows "— disconnected").
- C-W19: `TeammateMarkerLargeMult` presented as a two-step setting ({1.0, 1.5}), outside TDC F-1's continuous clamp; TDC/registry row to follow (cross-file need).
- C-W9: "-> ADR-001 / UI ADR" replaced by ADR-0001 and ADR-0021; "Informant Pings (VS)" in TM-R13 corrected to MVP.

## Cross-review 2026-10-25b (/review-all-gdds since-last-review)
- Ally-disconnects edge case: clear events listed exactly as Roster R6; grace-expiry edge case no longer promises a "disconnected" line.
Report: design/gdd/gdd-cross-review-2026-10-25b.md. Status line unchanged.

## Approved 2026-10-25 (D35)
- Approved 2026-10-25 (D35): main-session decision under user authorisation after /review-all-gdds 2026-10-25b (CONCERNS, 0 blocking); remaining warnings stay listed in the GDD and design/gdd/gdd-cross-review-2026-10-25b.md.
