# Review Log: Information Visibility Policy

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: XL (cross-cutting Core system, 8 formulas, 20+ dependents, ADR-001 plus spikes S9-S15; producer should verify before sprint planning)
Specialists: ue-replication-specialist, security-engineer, qa-lead, creative-director (senior verdict)
Blocking items: 7 | Recommended: 4
Summary: The structure is sound (one default-deny query, seven viewer classes, one carrier per audience, a full engine-default override table and leak catalogue), but the newer sibling GDDs pulled it in three unsafe directions: Crowd asked to loosen F5 to k = 2.0, Objectives asked for a `RivalStageStep` Team fact whose timing could confirm a watched tend, and evidence playtests leaned on a competing listen host. The creative-director ruled (A) k stays 1.5 and Crowd raises r_N, (B) the rival signal is denied as specified and allowed only stage-level with a fresh per-event delay and a global release quantum, (C) listen-host trusted-host rule now, headless server a pending user decision, (D) `SiteKnownToTeam` is presentation only and ingress accepts any court. Revisions were applied in place across two sessions (the first was interrupted mid-R7); an independent re-review is required.
Prior verdict resolved: First review
Findings:
- [BLOCKING] F5 / Tuning Knobs: k range 1.0-2.0 invited Crowd's k = 2.0 request; the cull exposes every hide city-wide -> fixed: k range 1.0-1.5, 1.5 is the ceiling, rationale paragraph under F5, AC-IVP-43 harness check (ruling A).
- [BLOCKING] R7.6 / Edge Cases / Tuning Knobs / AC-IVP-30: automatic client-side-hide fallback if Crowd misses F5 -> fixed: no automatic fallback, escalation to creative-director and user, client-side hide internal dev builds only (L29 CRITICAL), AC-IVP-30 and AC-IVP-44 gate external builds.
- [BLOCKING] R3 / L34 / OQ4: Objectives OC-12 `RivalStageStep` (Team fact) leaks sub-stage timing and confirms feints -> fixed: denied as specified; R3 row and OQ4 state the allowed shape (preliminaries only, never the final step, fresh per-event delay, one global release quantum, no member identity); AC-IVP-40; change request to Objectives in OQ4 and Cross-References item 8.
- [BLOCKING] R7.4a / R3 `SiteKnownToTeam`: gating IIF court ingress on site status is an oracle -> verified applied: ingress accepts any court fixture, `SiteKnownToTeam` drives presentation and progress credit only; AC-IVP-41 added (ruling D).
- [BLOCKING] R10 / OQ8: evidence playtests with a competing listen host contaminate F5/F8 data -> fixed: non-competing trusted host or host-team exclusion now, AC-IVP-36; headless server logged as a pending user decision (scope_changes, ruling C).
- [BLOCKING] Acceptance Criteria: rules cited AC-IVP-37, 38, 39, 41, 42, 45 which were never defined -> fixed: AC-IVP-36..45 added (possession canary, spectator ban, strike-id oracle parity, court ingress parity, allowlist content, net-config runtime differential, rival-signal shape, k ceiling, external-build critical gate, evidence host).
- [BLOCKING] Dependencies: Crowd, Concealment and Objectives listed as "Not written" while their GDDs exist and conflict -> fixed: statuses updated, conflicts named (Crowd k = 2.0 declined per C7; Objectives OC-12 denied), Concealment change request 12 (narrow its k range to 1.0-1.5).
- [RECOMMENDED] Edge Cases / AC-IVP-12 / Visual table: provisional `BushTransition` name -> fixed: `BushEnter` / `BushExit` per Concealment CON-R5.
- [RECOMMENDED] AC-IVP-33: catalogue range L1-L28 stale after L29-L34 were added -> fixed: L1-L34, L29 listed as dev-build Acc.
- [RECOMMENDED] OQ3: restated against k = 1.5 and the Concealment version of bush use (occupancy-blind, no dwell cap, r_N about 0.19/min); change request to Crowd T5, CR 1 and OQ1.
- [RECOMMENDED] AC-IVP-34: no longer deferred; fails until Crowd T5 cites k = 1.5.
Specialist disagreements: Crowd (k = 2.0, P_hide 0.20, game-designer wanting 0.30-0.40) vs Policy/Concealment (k = 1.5); creative-director ruled for 1.5 and orchestrator conflict C7 chose the Concealment version. Objectives (OC-12 approved at its CD-GDD-ALIGN with a team-labelled count) vs this review's ruling B (denied as specified); Objectives must revise OC-12 before the IVP adds a row.
Change requests not applied (other owners' GDDs): crowd-npc-behaviour.md T5, change request 1, OQ1 to k = 1.5; objectives-chain.md OC-12, F4, AC-OC-09/10 and its IVP change request 3; concealment-bushes.md F5 table k range 1.0-1.5.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 74cc103f40a86310e995f9d20cb3cadadc047bbc
Reviewed-Content-Hash: design/gdd/objectives-chain.md 3611facfb79f5c98605dfcf5314126da5602c6c3
Reviewed-Content-Hash: design/gdd/concealment-bushes.md ed2d2ba1c4cfd9081cd01d62ce6b2a592668b0c5
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: XL (cross-cutting Core system, 8 formulas, 20+ dependents; producer should verify before sprint planning)
Specialists: none (lean, single-session second-round review)
Blocking items: 2 (both fixed in place) | Recommended: 4
Summary: All seven blocking items of the 2026-10-24 review are confirmed resolved in the text (k ceiling 1.5 with AC-IVP-43; no automatic client-side-hide fallback; `RivalPrelimCount` replaces `RivalStageStep`; court ingress accepts any court; evidence-host rule AC-IVP-36; AC-IVP-36..46 defined; dependency statuses). All eight formulas recompute correctly at NpcCount 50 (F5 0.633 and 3/22, F2 SE 0.0175, F3/F4 250 ms, F6 104 B/s, F7 n = 1236, F8 h_fail = 16), the engine claim `RemoteViewPitch16` was verified in the installed 5.8.4 `Pawn.h`, and no section is missing at `full`. The two new blockers were stale text that contradicted a user decision or a formula, left over from the 2026-10-25 decision pass; they are fixed here, so an independent re-review should be able to approve.
Prior verdict resolved: Yes (7 of 7)
Findings:
- [BLOCKING] R10: said the headless-server option "is a pending user decision (OQ8)" while OQ8 is closed (deferred to Alpha, user 2026-10-25) -> fixed: R10 now states the deferral and that the non-competing-host rule stands until the Alpha dedicated server.
- [BLOCKING] AC-IVP-03: required n2 ≥ 2400 pawn-rounds while F2 requires ≥ 3000 (60 matches × 50 NPCs; 2400 was the NpcCount-40 figure), so the gate could pass on fewer samples than F2's SE assumes -> fixed: n2 ≥ 3000 (F2).
- [RECOMMENDED] AC-IVP-11: said each cull lands at exactly `t0 + 250 ms`, contradicting R7.2 and Concealment AC-CON-01b (first global grid point at or after `t0 + ConcealCullDelayMs`; the 66.7 ms grid is not a multiple of 50 ms) -> fixed: AC rewritten to the grid-point wording, ±1 server tick.
- [RECOMMENDED] Cross-References item 4: required `r_N / r_P ≥ 0.625` (the NpcCount-40 value); F5 and AC-IVP-25 give 0.633 at 50 NPCs -> fixed, with a recompute note.
- [RECOMMENDED] Interactions, Downstream, Cross-References: "killed-concealed interim rule" wording survived although R7.5 adopted the `bDiedConcealed` target rule on 2026-10-25; the Edge Case for a killed concealed pawn also omitted `bDiedConcealed` and the outcome-slot rule -> fixed in five places.
- [RECOMMENDED] Dependencies: Round, TDC, LSC and Informant Pings statuses were stale ("Designed", "pending review") -> updated to their current review states.
Cross-doc follow-ups (main session):
- `design/registry/entities.yaml` `team_position_fallback_feed`: `output_range` is [0, 480] B/s (12 B entries); IVP F6 is now 13 B per entry (`Seq` added 2026-10-25), so 0-520 B/s and 104 B/s worst case at 2 Hz. Update the range and the note.
- `design/registry/entities.yaml` `npc_bush_use_rate_target`: notes say "0.625 * r_P at defaults"; at NpcCount 50 it is 0.633 (Concealment F2 already says 0.633; Crowd F2 lists 0.633 at 50 and 0.625 at 40). Update the note.
- `design/gdd/systems-index.md`: IVP stays Needs Revision (this verdict) until the independent re-review; mark it In Review if the main session treats applied revisions as awaiting re-review.
- Outstanding from the first review, still owner-side: confirm Crowd T5 and Objectives OC-12 text are in sync (both now cite k = 1.5 and `RivalPrelimCount`; no conflict found in this pass).
- Not a defect, for the producer: CRITICAL leak rows L1, L2, L4, L13, L30, L31 remain `Spk`/`Req` until spikes S1, S2, S9, S10, S14 report, which blocks every build leaving the team (AC-IVP-44).
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 242977e3dcc0185347fd1053741e6f305c1e42fa
Reviewed-Content-Hash: design/gdd/objectives-chain.md 97f7640460f661ff7b7c2b4241a31d26ed6de168
Reviewed-Content-Hash: design/gdd/concealment-bushes.md d1594d8133f63e58ae044906a5762f652d66c0cf
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 409b5d7c0de4b7484fed1a6e4ad5fac1fedce466
Reviewed-Content-Hash: design/gdd/hud.md 224977e0b36ac631f3ab286857b5b707178bc4dd
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 89d049074c81e71b16f1269736cbe2016d977f9c

## Closeout cross-doc pass 2026-10-25
- Formula table NpcCount range set to safe 35-50, hard 6-100, default 50; `TeamPing` quantisation corrected from "10 cm" to 0.1 cm (`FVector_NetQuantize10`); status cells set to In Review.
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.

## Cross-review fix 2026-10-25
- C-W4: peer-contract table restated: the Policy's own edge to every peer is Soft; the second column records each implementer's own label (SCL Hard, IIF Hard, CPA Soft, LSC Soft, Session Soft/unrecorded upstream); the implementation obligation binds whichever label.
- C-W9: "ADR-001 ... not yet written" (Overview), `-> ADR-001` (R13), S11 and the Concealment / Teammate Markers "(not written)" change requests updated: ADR-0001..ADR-0023 are accepted and both GDDs exist.
- C-W19: new invariant I-IVP-1 `ConcealCullDelayMs mod 50 = 0` (reject, never repair; for TDC to register).
- C-W26: F7 example names its channel (ActionPose `FActionState` delay U, defined once in IIF F2, uniform over 0 to one server frame; sigma 9.6 ms at 30 Hz, 4.8 ms at 60 Hz) and L8 cites IIF F2.
- D-W6 / D-W8 (D26, D28): the TeamPing row states a ping plays no pawn animation and changes no replicated pawn state, and a concealed sender is rejected with an owner-only tone and no other-visible trace.
- S-B1 (D23): new Team-audience fact row `TeammateReconnectEndCause` (`ReconnectEndCause` uint8 {None, Rejoined, GraceExpired, Cleared}, C-TeamView teammate entry, written with every `bReconnecting` clear; 1 byte per teammate entry, change-only); AC-IVP-46 includes it.

### Cross-review fix 2026-10-25 — main-session follow-ups
- C-W4 follow-up: peer table now records the implementers' final labels (Session, CPA, LSC all Hard as implementers; Policy side Soft), reconciling edits made concurrently in Session/CPA/LSC.
