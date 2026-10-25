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
