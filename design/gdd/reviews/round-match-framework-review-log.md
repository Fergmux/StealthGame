# Review Log: Round & Match Framework

## Review — 2026-10-24 — Verdict: MAJOR REVISION NEEDED
Review mode: full · Workflow: full
Scope signal: XL (cross-cutting Core system, 8 formulas, 9+ dependencies, several new ADRs: Round authority object, scheduled start, identity-free pawn replication, spawn-gate relevancy)
Specialists: systems-designer, qa-lead, ue-replication-specialist, creative-director (senior verdict)
Blocking items: 14 | Recommended: 12
Summary: The fantasy ("one breath, one verdict", no clock, no rival count) is right, but the mechanics broke both decided cross-GDD rules (C1 informant respawn takeover, C2 ResetAll order), contradicted Roster F4 (an EliminationWin on a Disconnect, which also let a host win by kicking the last rival), omitted the Roster's ExpectedConnectionGeneration guard, had no recovery for an Active participant left without a pawn, and rested on an engine pawn that replicates PlayerState/Controller to every client (a permanent Pillar-1 tell). Most acceptance criteria were compound or vague and about 25 were missing (qa-lead was not consulted at authoring). Revisions were applied in the same session (26 items); an independent re-review is required.
Prior verdict resolved: First review
Findings:
- [BLOCKING] R15/R17.6/F2: EliminationWin on last grace expiry contradicts Roster F4; host-kick exploit -> fixed: Reason partition, ForfeitElimination (log-only), Void(Abandoned|KickedLastActive), TryTerminate.
- [BLOCKING] R17.4: grace transition lacked ExpectedConnectionGeneration -> fixed (timer record carries it).
- [BLOCKING] Tuning/F4: DisconnectGraceSec ranges inconsistent with Roster/Session; R=30 gives G<=0 -> fixed: hard 3-120, G_eff = min(G, R-30); Session/Roster change requests.
- [BLOCKING] R18: ShouldAdmitExpired excluded Preparing/expired returners -> fixed: true for every InRound record in Preparing/Active/Ended.
- [BLOCKING] R18: re-admission used emergence + TakeNpcCard, contradicting C1 -> fixed (C1 takeover, provisional OQ10).
- [BLOCKING] R17: no recovery for an Active participant without a pawn -> fixed: R17.9 own-card respawn via emergence, cap 2, then Void(PawnLost).
- [BLOCKING] R5: order contradicted C2; ResetAll lacked RoundIndex; no rollback -> fixed.
- [BLOCKING] R23: APawn PlayerState/Controller replication flips on possession (Pillar 1) -> precondition R23(f) + AC 27b; Shared Character change request; ADR.
- [BLOCKING] R7/R20: "same instant" reveal overclaimed; pawns arrive over several net ticks -> fixed: P7 world-ready + scheduled ActiveAtServerTime; ExpectedPawnCount gate.
- [BLOCKING] R9: a client could miss Ended on an immediate reset -> fixed: EndedMinDwellSec, LastResult.
- [BLOCKING] F6: wrong shape; default failed above TravelLoadTimeoutSec 105; retired acks -> fixed: Session F7-based T_needed and computed effective watchdog.
- [BLOCKING] R6 P5: used retired TDC/pool acks -> fixed: Session single acknowledgement (C3).
- [BLOCKING] Acceptance Criteria: compound/vague ACs, random-seed flake, ~25 missing, 3 AC-rule contradictions -> rewritten with test seams; ACs 36-61 added.
- [BLOCKING] F3/R5: TeamsInvalid(Unassigned) unreachable -> removed.
- [RECOMMENDED] R12: server timers stamped after intake extended braces by a frame; fixed TieRank -> fixed (due-before-drain first, per-tick TieRank, hold completions via heap).
- [RECOMMENDED] R12: stale-connection items, mailbox flood cap, canonical base order -> fixed.
- [RECOMMENDED] F5: 0/0 for empty zones; NpcCount floor -> fixed.
- [RECOMMENDED] F7: forfeits distort ObjectiveEndShare; zero denominators -> fixed.
- [RECOMMENDED] F8/AC33: entry size underestimated; truncation count ambiguous -> fixed.
- [RECOMMENDED] R19: Voided flood could evict the kill timeline -> rate limit added.
- [RECOMMENDED] F3: MinPlayersPerTeamCount <= TeamSize invariant -> added.
- [RECOMMENDED] R3: TeamSize ownership (TDC key, Round rule, injection) and tie team at round 0 -> added.
- [RECOMMENDED] R9: human pawns must stop by the same path as AI -> added.
- [RECOMMENDED] R20: clock-leak claim overstated -> softened to the presentation rule.
- [RECOMMENDED] R23(g): listen-server host-view smoothing tell -> accepted MVP exposure pending spike (OQ11).
- [RECOMMENDED] R22: abort before P1 relies on watchdog -> Session travel cancel.
Specialist disagreements: forfeit as a third public outcome vs log-only EndReason (CD: log-only, banner unchanged); pawn-loss recovery own-card respawn (systems-designer, CD) vs C1 takeover reuse or hold-then-Informant (replication specialist; rejected by CD); scheduled start vs zeroed Active start time (CD: accepted, field zeroed at Active entry, no countdown).
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 7090ac55a320a45ab62d598f7ec0661d468cc8e3
Reviewed-Content-Hash: design/gdd/session-travel.md 5a2f3d67fb9453d71eb9602429781de31d79777c
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 8bf1dcd1acd4e451528d64eb7491c82b2479c26e
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/level-socket-contract.md e07cedf53b842c906dcf4a5d51fc302c1569de22
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: XL (unchanged: cross-cutting Core system, 8 formulas, 9+ dependencies, several ADRs already accepted)
Specialists: none (lean, single-session analysis)
Blocking items: 2 (both fixed in text; one needs Roster/Session confirmation) | Recommended: 5
Summary: All 14 first-round blockers are verified resolved in the text (Reason partition and forfeit/void, `ExpectedConnectionGeneration`, G_eff clamp, `ShouldAdmitExpired`, C1 informant re-pawn, pawn-loss own-card respawn, C2 start order, R23(f) identity-free replication precondition, scheduled start with world-ready, `EndedMinDwellSec`, F6 computed watchdog, single acknowledgement, rewritten ACs, unreachable `TeamsInvalid(Unassigned)` removed). No registry value conflicts were found, and all 8 sections are present. New blockers: an acceptance criterion that contradicts the P5 rule, and a host-kick path that Session sends to `Expired` in the same tick, which R17.4 would have treated as a defect and converted instantly. The revisions are applied; a revision is not an approval, so a further re-review decides.
Prior verdict resolved: Yes (MAJOR REVISION NEEDED 2026-10-24; all 14 blockers and 12 recommendations verified present; 2 new blockers found)
Findings:
- [BLOCKING] Acceptance Criteria 7: still asserted a "half of the clients mismatching aborts" quorum for `TuningSnapshotFault`, contradicting R6 P5 / TDC CR-9 (no quorum abort) -> fixed (mismatch only disconnects the client; the fault is a server self-check).
- [BLOCKING] R17.2/R17.4/F2/AC 26b: a host kick is sent by Session straight to `Expired` with `OnRejoinWindowExpired` in the same tick; R17.4 treated that as a `WindowBeforeGrace` defect with immediate conversion (a timing tell, and false defect noise), and the Roster transition may return `Rejected_NotDisconnected` for an `Expired` connection -> fixed on Round's side (kick keeps the grace, same-tick window expiry ignored, AC 26c added); Roster/Session confirmation is change request 21.
- [RECOMMENDED] F6: `LayoutTotalBudgetSec` range "0 to 45" contradicted the registry hard range 10-90; the "~0 for the graybox" note contradicted the minimum -> fixed (range 10-90; maxima and minima recomputed, T_needed 52.1 to 242 unchanged).
- [RECOMMENDED] R6 slow-loader text: "up to 70.2 s" is Session's example, not the bound; F6's bound is 81 s -> fixed.
- [RECOMMENDED] F4 / change request 13(a): `RejoinWindowSec` "Session's table says 30" is stale (Session and registry now 60-600) -> fixed.
- [RECOMMENDED] Acceptance Criteria: duplicate label 12d; placeholder `<StopBlendSec>` (no such key; uses `RunStopSec`); "Crowd-owned limit" in 28a (named `CrowdStopDecel`) -> fixed.
- [RECOMMENDED] Dependencies: Informant Pings & Team Comms (MVP since 2026-10-25) clears its parts in the R9 round-end step but was missing from Downstream -> row added.
- [RECOMMENDED] R10 step 4 (OQ14): destroying only the player pawns before lobby travel is an uncontrolled player/NPC difference on an abort or void -> added as OQ14, not changed (needs the spike and AC 13a).
Specialist disagreements: none (no specialists in lean mode).
Cross-doc follow-ups (main session):
- Roster/Session (change request 21): confirm the Roster accepts `RequestTransition(Informant, Disconnect)` for a connection Session has marked `Expired` after a host kick, or keep a kicked in-round participant `Disconnected` until grace ends.
- Registry/TDC (change request 22): `StartLeadSec`, `PreparingWatchdogSec`, `MaxTeamImbalanceCount`, `PawnLossRecoveryMaxCount` and `RoundLogMaxEntriesCount` are not in `design/registry/entities.yaml` (TDC counts 17 Round keys; the registry has 12).
- Systems index: set round-match-framework to "In Review" (revisions applied, re-review pending), not Approved.
- Penalty/HUD: `TeamSummary.WrongTargetCount` is read "from the Event Log"; confirm it is the protected `KillResolved` entries (VictimKind Npc or Informant, by the killer's team), not Penalty's non-protected `StrikeCounted` entries.
- Informant Pings & Team Comms GDD: confirm its Dependencies row for Round matches the new Round Downstream row.
Note: the target document was revised after this review, so it is deliberately not stamped below (a re-review must run in full).
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/participant-state-roster.md 11106ca55e071c8e8f1ab67926f678d5c3ecf306
Reviewed-Content-Hash: design/gdd/session-travel.md 0da6609c7da397b7225feb73fde9b303ac01a320
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 4d10b5f8eb2a9a711422f9e2955e2fbf3aa164d6
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 63db04c802c6db06eb0ba3dac93f65fb9fa98068
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
Reviewed-Content-Hash: design/gdd/informant-pings-team-comms.md 6f5397e9f8ef2526f8d2c6c107100ba7a2db673b
Reviewed-Content-Hash: design/gdd/crowd-npc-behaviour.md 242977e3dcc0185347fd1053741e6f305c1e42fa

## Closeout cross-doc pass 2026-10-25
- CR-21 marked confirmed by Roster R11 (kick path, D9): `Disconnect` for an `Expired` connection is accepted and `Applied`.
- Interactions row for Session now lists `OnRoundTravelStarted` and `OnLobbyTravelComplete` semantics; R17.2 gains the failed-supersede-rejoin `OnParticipantDisconnected` handling (stale generation voids the old grace timer, new timer from the new loss).
- Status cells in Dependencies set to In Review.
- Ref: see production/design-closeout/2026-10-25_rereview-followups.md and 2026-10-25_gdd-pass-remaining.md.

## Cross-review fix 2026-10-25
- C-B5 (D22): R11 and R12 define the due rule `DueServerTime <= NowServerTime + DueEpsilonMs` with `DueEpsilonMs` = 1 ms (a constant); AC 45 tests 0.5 ms (fires), 1 ms (fires), 2 ms (does not).
- S-B2 (D24): R17.1 (a supersede does not hand back), R17.2 (supersede is a disconnect, grace basis, kick cite corrected, kicked participant cannot return, Round never writes bReconnecting), R17.3 sub-bullet rewritten, F4 DisconnectedAt, Edge Cases, AC 50 and new AC 62.
- C-W30: R17.2 cites the transition-table kick row and edge case, not rule 13; new AC 66.
- C-W3: Downstream gains CPA and IIF rows; Crowd and SCL resolved as mutual peer contracts (contained cycle); IIF-Round cycle labelled in Upstream and Downstream.
- C-W9: Cross-References items 1, 20, 21 (closed by D9), 22 updated; Bidirectionality note; "no ADR exists yet" corrected.
- C-W19: F5 `NpcCount` row cites I-LSC-1 (35 at the LSC defaults).
- C-W27: new warn invariant W-RMF-5 `EndedMinDwellSec >= HudBannerMaxDelaySec + 1.0` (`EndedMinDwellSec` safe 2-5 to 3-5); `StartLeadSec` bound L99 + 1/f_net = 217 ms (safe 0.2-0.5 to 0.25-0.5, default 0.25 kept); F4 `L_soft` 3.0-3.3 s and `L_hard` 12.0-12.3 s equal to Session F2.
- C-W35: R19 logs a latched final Complete as `TendCompleted(NoEffect, Latched = true)`; new AC 63.
- D-W9 (D29): F7 re-based on Objectives F1 (~9 min): new knob `StallFlagMinutes` 18 (stall flag, abort flag at half), `ObjectiveEndShare` per team-size bucket with the concept's 0.70 gate over 20 rounds for 4v4 and larger; AC 35 updated.
- S-W1: R9 (f) also stops pawns in hand-over (pawn-level Halt); AC 12b, new AC 64, Edge Cases.
- S-W3: R9 round-end step budget (2.0 ms added host game-thread time, about 3.1 KB burst for 9 clients); new AC 65 [SPIKE]; constants list.

## Cross-review 2026-10-25b (/review-all-gdds since-last-review)
- R17.2: a Disconnected(Kicked) for a participant with a live grace timer sets Kicked on the existing record, no second timer or takeover (S8). AC 66 wording. CR 22 note: StallFlagMinutes is registered. F5 NpcCount: safe minimum 45 (follow-up b).
Report: design/gdd/gdd-cross-review-2026-10-25b.md. Status line unchanged.

## Approved 2026-10-25 (D35)
- Approved 2026-10-25 (D35): main-session decision under user authorisation after /review-all-gdds 2026-10-25b (CONCERNS, 0 blocking); remaining warnings stay listed in the GDD and design/gdd/gdd-cross-review-2026-10-25b.md.
