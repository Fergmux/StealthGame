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

## Review — 2026-10-25 — Verdict: NEEDS REVISION
Review mode: lean · Workflow: full
Scope signal: L (multi-system integration, 7 formulas, ADR-0001/0005/0010 dependencies; producer should verify before sprint planning)
Specialists: none (lean, single-session analysis)
Blocking items: 1 | Recommended: 8
Summary: All 17 blockers of the 2026-10-24 review are confirmed resolved in the text (identity key honesty, frozen PlayerStates, one clock model, ConnectionGeneration guard, F4 hardening, Dormant records, binding/spawn paths, parity, informant-kill invariant, ACs). The 2026-10-25 user-decision additions (owner tally and `T_due`, teammate `RetireAtServerTime` and `bReconnecting`, Pings part) introduced one real gap: the Roster calls that write the owner fields were named in R6 but never specified or tested. That is the one blocker. It, and eight smaller inconsistencies, were revised in this pass; the verdict is not upgraded by those revisions, and a re-review decides. Pillar 1's network claim remains provisional on spikes S1/S2 (no stated fallback; added as OQ16).
Prior verdict resolved: Yes (17 of 17 blockers; recommended-open items partly closed: positive event ACs and `RosterTransitionLogDepth` AC added, S1/S2 fallback logged as OQ16, language-rule lint and S7 Iris matrix still open)
Findings:
- [BLOCKING] R6 (C-TeamView owner part): `SetOwnerTally` / `SetOwnerRespawnDue` were named but had no signature, result precedence, authority rule, event behaviour, cache scope or AC. APPLIED: owner-view feed paragraph added to R6 plus AC-PSR-54.
- [RECOMMENDED] R6 `bReconnecting`: window used `DisconnectGraceSec` rather than F6's `GraceExpiryMs` / `G_eff`. APPLIED.
- [RECOMMENDED] Interactions table: Teammate Markers and HUD rows listed a stale C-TeamView subset; Informant Pings and Camera rows missing; Summary quick-reference deps. APPLIED.
- [RECOMMENDED] F6: `RejoinWindowSec` range stated 60-300; registry hard range is 60-600 (safe 60-300). APPLIED.
- [RECOMMENDED] F7: `bReconnecting` byte accounting and AC-PSR-33's 96 B headroom. APPLIED.
- [RECOMMENDED] Edge Cases: stale-respawn `BindPawn` epoch check attributed to the timer's owner instead of `BindPawn` itself. APPLIED.
- [RECOMMENDED] Acceptance Criteria: no positive test for `OnParticipantRegistered` / `OnTeamsChanged` / `OnParticipantRemoved`, none for `RosterTransitionLogDepth`. APPLIED (AC-PSR-55, AC-PSR-56).
- [RECOMMENDED] Open Questions: no plan B if spikes S1/S2 fail. APPLIED as OQ16 (needs ADR-0001 text).
- [RECOMMENDED] (open) Language-rule localization lint; S7 Iris AC matrix; AC-PSR-43 and AC-PSR-37 depend on human sign-off for part of their evidence.
Cross-doc follow-ups (main session):
- npc-kill-penalty-informants.md PEN-R3/PEN-R15 should cite the Roster calls `SetOwnerTally(Id, Tally, LastResultSeq, ExpectedEpoch)` and `SetOwnerRespawnDue(Id, T_due, ExpectedEpoch)` by name (currently only "carried as an Owner-only field"); it must pass `ExpectedEpoch` on `BindPawn` and both calls.
- design/registry/entities.yaml `connection_state.teammate_exposure` says the window is `DisconnectGraceSec`; the Roster now states it as F6 `GraceExpiryMs` (`G_eff`). Registry wording should follow. The registry's `c_team_view` content line should note the owner-view feed result codes if it lists API names.
- design/gdd/systems-index.md: row 2 stays In Review / Needs Revision (not Approved).
- ADR-0001: needs a stated fallback if spikes S1/S2 fail (Roster OQ16).
- Pending decision 18 (an `InformantKilled` Reason value) is not applied; if accepted later, R8's entering-reason list, the registry `entering_reasons_informant` and AC-PSR-20's fixture all change.
Note on the receipt: the target document was revised after this review, so it is deliberately **not** stamped; the next `/design-review` reads it as NEW.
Reviewed-Content-Hash: design/registry/entities.yaml 28f7a489149e3fba702986d089b7799719796b39
Reviewed-Content-Hash: .claude/skills/design-review/SKILL.md a63d8fba9b7142eeb43791f061e2e1533c429b43
Reviewed-Content-Hash: design/gdd/session-travel.md 83f0974bf4ff3e8d49fcf4df038e6ce6fba4bc6d
Reviewed-Content-Hash: design/gdd/round-match-framework.md f019aa4e04092b43aee6b74f2462438418205597
Reviewed-Content-Hash: design/gdd/npc-kill-penalty-informants.md 2777d58294cd124c53ce663e2ee82349b5da9aaf
Reviewed-Content-Hash: design/gdd/hud.md 224977e0b36ac631f3ab286857b5b707178bc4dd
Reviewed-Content-Hash: design/gdd/teammate-markers.md 8b45501b2783ca0601c2f3832fa3aba2cd85e2bc
Reviewed-Content-Hash: design/gdd/information-visibility-policy.md 9ce1d9677b38048e71176b7dd0b44959ce5c99f5
Reviewed-Content-Hash: design/gdd/character-pool-appearance.md 2cb51cb76bc64b4827a0f7c131fc8d0fea64b377
Reviewed-Content-Hash: design/gdd/tuning-data-config.md 89d049074c81e71b16f1269736cbe2016d977f9c
Reviewed-Content-Hash: design/gdd/input-interaction-framework.md d2cffcaa5a506020e748425c86fcdfe18b61761a
Reviewed-Content-Hash: design/gdd/level-socket-contract.md 170946af3b310140bd789eda4906874349fc59f9
Reviewed-Content-Hash: design/gdd/systems-index.md b195f89efa215d57e7630933f75bbbc02bdd9ba8
Reviewed-Content-Hash: design/gdd/game-concept.md 6dbca7dc538ed44d89a5f009dbc8eda18782a2c8
