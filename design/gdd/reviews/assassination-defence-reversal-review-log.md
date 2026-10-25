# Review Log: Assassination & Defence Reversal

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system integration, 5 formulas, 8 upstream and 8+ downstream dependencies, ADR work for the defence component, fixed-width action-state serialiser, pending-write queue and presentation dispatch; producer should verify before sprint planning)
Specialists: systems-designer + game-designer + qa-lead (one combined brief), ue-replication-specialist, creative-director (senior verdict)
Blocking items: 9 | Recommended: 12
Summary: The core holds and no locked rule moved: the outcome is settled in the Attack's slot, one outcome-blind action struct goes on the wire, and contact is presentation only. The blocking defects were timing and data leaks at the slot (un-cull in the slot, victim `Disabled` at execution, `WarpPoint` reading a hidden occupant), an unsatisfiable no-warning rule (Owner/Team outcome data arrives before contact with no presentation gating), F1 lacking a lower bound and bearing limit, a missing `Dead` exclusion that let a spot strike at a body kill a bystander and charge a strike, and sibling change requests not yet applied. Creative director: NEEDS REVISION (overruling the replication specialist's MAJOR); every fix is local to this GDD or a change request.
Prior verdict resolved: First review
Findings:
- [BLOCKING] ASN-R10 / Edge Cases / AC-ASN-44-45: un-cull "in the same step" as the slot contradicted ASN-R21 and CON-R9 -> fixed: nothing observable changes on the victim in the slot; non-concealment culls re-open at the death write; a victim killed while concealed stays relevancy-closed to non-allied viewers (interim, field-free, while `bDiedConcealed` is pending; OQ-ASN-11); AC-44/45 rescoped.
- [BLOCKING] ASN-R16 / F1 / AC-ASN-02, 38, 48: victim `Disabled` at execution leaked hit vs whiff to honest bystanders 0.4 s early -> fixed: victim untouched until the death write; F1 extrapolates at constant velocity for `t_d = T_v − λ`, then brakes; runner-fleeing cutoff 193 -> 151.75 cm (CD-accepted, stated in R12); reversing defender never `Disabled`; clients warp to the fixed `WarpPoint`.
- [BLOCKING] ASN-R21 / AC-ASN-47: `WarpPoint` read a concealed or culled victim's position (occupied-vs-empty bush and anchor leak) -> fixed: victim position only for an honoured `PawnRef`; spot strikes derive it from `WorldPoint` (bush: footprint edge), hit and whiff alike; accepted tell: blind open-spot body may fall up to `r_c + OpenSpotToleranceCm` away.
- [BLOCKING] ASN-R6 / ASN-R23 (new) / UI-ASN-3-5 / AC-ASN-05, 35, 49: Owner/Team outcome data (Roster state, `FHoldStatus`, `FDefenceResult`, replies, round-end broadcast) arrived before contact -> fixed: R6 is a presentation rule (strike pose and positional audio exempt); R23 gating via `OnRoleChangePresented` at `DeathServerTime`, brace end at `ServerEndTime = ContactServerTime`, defence plate from the defender's Counter write, round-end broadcast staged (Round change request); residuals restated (bystander ≤ `StrikeNetLeadSec`, Owner/Team ≤ `T_v`, IVP L10 change request); AC-05 rewritten.
- [BLOCKING] F1: no lower bound (negative X for a victim touching or walking toward) and no bearing term -> fixed: `X_eff = max(X, 0)` (arm strike in place), `|θ_T| ≤ StrikeBearingLimitDeg` (new knob, 90°, I-ASN-8).
- [BLOCKING] ASN-R8 / F4 / AC-ASN-17: `Dead` exclusion only in F4 -> fixed: R8 excludes `Dead` (incl. pending death write); a `PawnRef` at a body is a terminal whiff; AC-17 adds the bystander-beside-body case.
- [BLOCKING] ASN-R19 / UI-ASN-2 / OQ-ASN-2 / AC-ASN-16, 46: Penalty CR-2 not applied -> applied provisionally on Penalty OQ-NKP-1 (`FStrikeResult {…, TallyAfter, bExiled}`, "Wrong targets" tally, OQ-ASN-2 closed provisionally).
- [BLOCKING] ASN-R8 / AC-ASN-39: Concealment CR-10 (c)(d) not applied -> applied (range reconciliation confirmed; filter-then-pick with `SpotSalt`). CR-10 (a)(b) fields NOT added (pending user decision, schema_changes; OQ-ASN-11).
- [BLOCKING] AC hygiene: AC-02/38 numbers, AC-07 preconditions, AC-14/42 tick tolerance, AC-20 bit-count capture under loss untestable -> fixed (server-side dirty-property and dry-run size diff plus a loss soak), AC-31 extended (I-ASN-6 note, I-ASN-7/8 cases).
- [RECOMMENDED] Tuning Knobs: `StrikeWarpLimitCm` hard 150-300 vs warp scale ≤ 1.6 -> hard 150-240, safe 200-240; I-ASN-1 now `R − r_c − g ≤ X_max`; I-ASN-7 speed warning; `StrikeNetLeadSec` safe 0.10-0.15 (I-ASN-4 at the safe corner); F3 duty range and TDC I-1 note; F5 `b_N` range 0-0.015 and startle-clustering note.
- [RECOMMENDED] ASN-R21: fixed-width serialiser, uniform net frequency and forced updates, idempotent `RefreshPresentation`, sentinel defaults, host calls the handler, double server time -> applied as rules; implementation homes added to OQ-ASN-9 (ADR).
- [RECOMMENDED] Edge Cases: victim acting between slot and death write; listen-host wall-clock wording; late-actor cross-fade ≤ 0.1 s -> added.
- [RECOMMENDED] ASN-R11: Penalty's own strike conditions (PEN-R2) -> noted.
- [RECOMMENDED] Dependencies / Cross-References: stale statuses (Penalty, Teammate Markers "Not written"; upstreams "Designed") -> updated; Penalty and Teammate Markers rows added; Teammate Markers CR-9 applied.
- [RECOMMENDED] Change requests 13-19 raised (IVP R7.5/L10, Round staged broadcast, Roster presentation dispatch, IIF `ServerEndTime`, SCL `Disabled` timing and uniform net settings, TDC knobs/invariants, Camera `Dying` dispatch); IVP CR-5 answered.
- [RECOMMENDED] AC-ASN-30: add "why did the striker fall" per bearing (evidence for OQ-ASN-10) -> applied, not gated.
- [RECOMMENDED] AC-ASN-33: reversal-frequency review target 0.5-2 per match (bait fantasy) -> applied (advisory).
- [RECOMMENDED] OQ-ASN-3: 1.75 s pose lock after a kill and shorter runner reach tagged for playtest -> applied.
- [RECOMMENDED][ALWAYS-ASK] OQ-ASN-10 rear-strike counter turn: still pending the user (unchanged).
Specialist disagreements: (1) ue-replication-specialist rated MAJOR REVISION NEEDED; creative director ruled NEEDS REVISION (no redesign, all fixes local). (2) Replication tagged moving `Disabled` to the death write as always-ask; creative director ruled it applicable now (no locked value changes; the 2 m range against a standing target is unchanged) and accepted that running now buys a little reach (193 -> ~152 cm) against R12's "speed is never protection". (3) game-designer proposed releasing movement after a kill near contact + 0.4 s; creative director rejected it (the full lock is the P2/P3 consequence), kept as a playtest question. (4) Creative director considered Penalty CR-2 (`TallyAfter`/`bExiled`, tally replacing the concept's strike counter) always-ask; the orchestrating brief explicitly instructed applying it, so it is applied **provisionally**, tied to the already-pending user decision Penalty OQ-NKP-1, and flagged in active.md. (5) Concealment wants an un-cull at the death write plus a client hide (needs `bDiedConcealed`); with the field pending, the creative director chose the field-free route (stay relevancy-closed); change requests raised to IVP R7.5 and CON-R9.
Re-review: per the Re-review policy (decision log 2026-10-24T04:24:20Z) this was the one independent review+fix pass; the creative director asks that a later re-review have the replication specialist re-verify AC-ASN-05, 20, 21, 44 and 45.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md c551d1e5f1562149082a455b1f09648242c588c1
Reviewed-Content-Hash: design/gdd/concealment-bushes.md dae06ab7f91d65fbb7e2c82c6d4fa3e7b710d877
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md d5494369a7ed46d882c863563dd3228f5d2281ab
Reviewed-Content-Hash: design/gdd/objectives-chain.md 986f8de4aca918e560156ceb328862adad93f3e5
Reviewed-Content-Hash: design/gdd/teammate-markers.md 22764362d4196ffde99920c0983ae1b0f92cf719
Reviewed-Content-Hash: design/gdd/camera-system.md 8c0dbdd6b248a1b4df5a7a96d0f35ae366d57528
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md d8fd90df42d37734c67ecba262aefd4ca1d353b8
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 90b88857c2a98e951356a90c5878a80f231c47ca
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (unchanged: 5 formulas, 8 upstream and 8+ downstream dependencies, ADR work for replication; producer should verify before sprint planning)
Specialists: none (lean mode; single-session second-round review)
Blocking items: 3 | Recommended: 4
Summary: All 9 blocking items of the 2026-10-24 review are resolved in the text, and the 2026-10-25 user decisions (tally, `bDiedConcealed`, `StartServerTime`, `ContactServerTime`, `RetireAtServerTime`, victim `Disabled` at the death write, rear-counter declined) are integrated. Registry values (all `Assassination*`/`Strike*`/`Defence*`/`Counter*`/`Body*` keys) match; F1, F2, F3, F5 and the worked examples were recomputed by hand and hold. Remaining defects were stale text left by the decision integration and one scope gap in an AC; all are corrected in the GDD in this pass, but they were found in a lean single-session review and the creative director's request that the replication specialist re-verify AC-ASN-05, 20, 21, 44 and 45 is still outstanding, so this is not an approval.
Prior verdict resolved: Yes (9 of 9 blocking items verified fixed; 12 recommended items verified applied)
Findings:
- [BLOCKING, fixed] VA-ASN-6: still said relevancy stays closed for non-allied viewers for a body killed while concealed, contradicting ASN-R10/R17/AC-ASN-44 and the user decision (re-open at the death write with `bDiedConcealed`, hidden on every non-owner client) -> rewritten to the decided route.
- [BLOCKING, fixed] ASN-R16: said `FActionState` has no start-time field and derived the `BushStrike` start from `ContactServerTime`; `StartServerTime` was adopted in ASN-R21 -> now reads `StartServerTime`, derivation kept only as a consistency check; Cross-References row for ADR-0017 sync annotated as superseded.
- [BLOCKING, fixed as clarification] ASN-R16/R21: the death write applied `Disabled`/death state to a reversed attacker at `ContactServerTime − StrikeNetLeadSec` while its lunge warp runs to contact, so server and client could disagree on where the attacker drops (up to roughly 0.15 s of lunge, tens of cm) -> stated that the death write never interrupts the lunge: for a reversed attacker it sets `FDeathState` and cancels input only, the warp to the fixed `WarpPoint` runs to `ContactServerTime`, then the body falls. Needs SCL / ADR-0018 confirmation (cross-doc follow-up).
- [RECOMMENDED, fixed] AC-ASN-20: "same set of dirty replicated properties" was unsatisfiable across Owner/Team channels (Roster state differs for a kill versus a reversal) -> scoped to the All-audience (bystander) channel, Owner/Team data deferred to the ASN-R21 residuals and AC-ASN-05.
- [RECOMMENDED, fixed] ASN-R21 Residuals: named "the target's team" only; on a reversal the attacker's team receives the Team-audience data -> now "the team of whichever pawn is eliminated".
- [RECOMMENDED, fixed] F1 Output Range: "caps reach at 260 cm" was wrong for the default 240 cm warp limit (240 + 35 + 15 = 290) -> corrected with the values at the safe and hard minimum warp limits; TDC hard maximum 250 unchanged.
- [RECOMMENDED, fixed] Edge Cases (strike spam) and OQ-ASN-3 still quoted the `NpcCount` 40 figures (0.34 rival kills, 8.6 NPC kills per rival kill); F5 and AC-ASN-32/33 use `NpcCount` 50 (0.276 / 10.7) -> aligned.
- [RECOMMENDED, fixed] AC-ASN-35: ally marker line described an exit fade from the arrival of the retire update; ASN-R23 and TM-R6 now schedule it from `RetireAtServerTime` -> aligned.
- [OPEN] Replication-specialist re-verification of AC-ASN-05, 20, 21, 44, 45 (creative-director request, 2026-10-24 log) is not done in lean mode; run it in the next full-mode pass or at ADR-0003/0018 acceptance.
Cross-doc follow-ups (main session):
- Systems index: set Assassination & Defence Reversal to Needs Revision (second review) with the date; this agent did not edit it.
- SCL / ADR-0018: confirm the "death write never interrupts the lunge for a reversed attacker" clarification (ASN-R16) against the SCL `Disabled` timing and the server-side motion warp.
- Dependencies/Cross-References carry sibling statuses ("Reviewed, NEEDS REVISION", Round and TDC "MAJOR REVISION NEEDED") as of 2026-10-24; refresh once the parallel second reviews land.
- IVP L10 and Round staged-broadcast change requests (items 3, 13, 14) remain the siblings' to confirm; Roster R6 `RetireBinding`/`RetireAtServerTime` and TM-R6 are assumed adopted as the GDD states.
- TDC: confirm I-ASN-1 reach cap wording (290 cm at default warp limit) when it restates the invariant.
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/assassination-defence-reversal.md d89bac971a0a37d20024e0b32fb89559467cf5ab
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 12fe9795d9b3f3a6a34b36063e17bccc2f3ad910
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md ed42829b7c7aea9557521fb4c45d124224835244
Reviewed-Content-Hash: design/gdd/concealment-bushes.md a7075ec56f19ea43b29d27c845e98d280ed6797f
