# Review Log: Participant State & Roster

## Review — 2026-10-24 — Verdict: NEEDS REVISION
Review mode: full · Workflow: full
Scope signal: L (multi-system integration, 7 formulas, ADR-001 plus store/spawn-site ADRs; producer should verify before sprint planning)
Specialists: game-designer, systems-designer, qa-lead, network-programmer, ue-replication-specialist, creative-director (senior)
Blocking items: 17 | Recommended: 25
Summary: The binding model and structure are sound, but the GDD contradicted the now-written Session & Travel GDD (identity key, Lobby Dormant records, connection ownership, frozen PlayerStates, API names, reset timing). It also had real defects: F4 awarded wins on disconnect, to empty teams, and to already-eliminated teams; there was a stale-grace-timer race; the pawn-parity audit was too narrow; informants respawned as fresh pawns, which is a network tell; and many acceptance criteria were non-deterministic. All 17 required revisions were applied to the GDD in the same session by the independent reviewer (unattended pipeline), and a re-review is pending.
Prior verdict resolved: First review
Findings:
- [BLOCKING] R3/R4: IdentityKey is client-asserted in MVP; "never client-supplied" was false; AGameModeBase vs AGameMode inactive-PlayerState facts corrected
- [BLOCKING] R6: adopt Session's frozen PlayerStates and name list; closed PlayerState allowlist (PlayerId, ping, bIsInactive, bFromPreviousLevel...)
- [BLOCKING] Player Fantasy: opponent team derivability stated honestly
- [BLOCKING] R11/F6: one clock model (hand-over at Unresponsive, grace from Disconnected, DisconnectGraceSec <= RejoinWindowSec); returning players never refused
- [BLOCKING] F6/R8: ConnectionGeneration guard against stale grace timers; epoch token (MatchId, RoundEpoch)
- [BLOCKING] F4: no win on Disconnect reason; winner side must have Active >= 1; both-zero and all-disconnected go to Round adjudication; IsTeamEliminated phase-gated
- [BLOCKING] R3/R11: Connection is a Session-written projection (ServerOnly), mapping table, not replicated to teammates
- [BLOCKING] R4/R5/R7: Lobby departure leaves record Dormant; SetTeamsLocked then ResetAll; Team=None never Active; capacity ignores Dormant
- [BLOCKING] R8/table: R8 precedence authoritative over "not listed" rule
- [BLOCKING] R9: PawnLeavingGame override (spike S8); single named spawn site; ClearBinding fault only on Destroyed in Active
- [BLOCKING] R6/R9: pawn replication parity (RemoteViewPitch, net frequency, dormancy, subobjects, RPCs); informants adopt crowd pawns (no fresh mid-round spawn)
- [BLOCKING] R10: informant-kill invariant (attacker perception identical to NPC kill); ResolveVictimKind evaluation order
- [BLOCKING] R3/R6: ParticipantId ServerOnly; random opaque PublicHandle for clients
- [BLOCKING] R3/R12/I6: EventSeq is one sequence over all event types
- [BLOCKING] R4: Session API mapping; ResetAll before RequestRoundTravel; RoundEpoch vs RoundIndex; RebindView / InvalidateAllBindings
- [BLOCKING] Acceptance Criteria: PIE determinism, fault sink, pawn-liveness provider, AC-28/33/37/38/39/40/42 rewritten, coverage AC-44..52 added
- [BLOCKING] Dependencies/Cross-References: stale "not yet written" statuses; TeamSize owner Round; AppearanceHistoryDepth 0 log-only; GetLegalityClass; LSC dependent
- [RECOMMENDED] (applied) BindPawn precedence, GetRelation pair table, AC-10 values, I1 restated, flapping budget (Round), rejoin/expiry same-tick order, supersede race, EndMatch on every exit, presentation include check, C-TeamView coalescing, id/epoch widths, R2 fault sink
- [RECOMMENDED] (open) language-rule localization lint; RosterTransitionLogDepth AC; positive event ACs for Registered/TeamsChanged; S1/S2 fallback plan; S7 Iris AC matrix; deterministic F7 serializer fixture details

Note on the receipt: the target document was revised after this review, so it is deliberately **not** stamped. The next `/design-review` therefore reads it as NEW and runs a full re-review, as intended.
Reviewed-Content-Hash: design/registry/entities.yaml dfeacb02917f751cb1dfd88cf490c7ec3d8d2d0f
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/session-travel.md 79ddc592523e462c830516106a94c8ff29fdfdc1
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 6c09a02eb045d68fab387ad4e614d0e2e450fd3d
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 4c00659ec2fded9575197cc975bcde9241b96b93
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md 5e6f769a977b74d4ce1571d24f3142e68dc28512
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 262eb5056ce08175a3eebf6cac899bb141684101
Reviewed-Content-Hash: design/gdd/systems-index.md 2adb098db430a1b7d1c77e1238171c3948bf08eb
Reviewed-Content-Hash: design/gdd/game-concept.md 8dccb622e17a7f1dc58349dd2baa54d52fc36d32
