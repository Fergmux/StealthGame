# GDD closeout pass — remaining items (handoff 2026-10-25)

## PARTIAL
- participant-state-roster.md: Dependencies status cells (section ~line 493) -> "In Review".
- session-travel.md: kick wording matching Roster D9 (Disconnect accepted for Expired); Lobby Rejoining projection / failed-supersede ConnectionGeneration bump wording; status cells.
- round-match-framework.md: CR-21 (~lines 136,148) marked confirmed by Roster; Interactions rows OnLobbyTravelComplete / OnRoundTravelStarted; handling of OnParticipantDisconnected (from failed supersede-rejoin); status cells.
- shared-character-locomotion.md: bidirectionality paragraph (~line 512) still calls CR-12 open — mark met.
- procedural-city-generator.md, level-socket-contract.md: status cells.

## REMAINING
- information-visibility-policy.md: stale NpcCount ranges in formula tables (NpcCount default 50; safe 35-50, hard 6-100 as in concealment F1); TeamPing quantisation "10 cm" -> 0.1 cm (FVector_NetQuantize10); status cells.
- informant-pings-team-comms.md: state pings carry no stage/card/court data; copy Teammate Markers F3 bias ramp if the ping edge clamp reuses the clamp formula; status cells.
- Status cells not started: input-interaction-framework.md, teammate-markers.md, character-pool-appearance.md.
- design/art/art-bible.md: section 7.x row 9 (informant pings: rings to all allies, non-positional tick, no feed line); B4 restated for D2 (every stop >= 0.5 s; ~0.35-0.55 m from crowd pace).
- Replication verification: spawn ue-replication-specialist (model sonnet) to adversarially verify AC-ASN-05, 20, 21, 44, 45 in assassination-defence-reversal.md (already rewritten once); apply its fixes.
- Concealment BushEnterSwapSec safe lower bound: registry now says 0.46; make concealment-bushes.md F3 / knob table say 0.46 too.
- Decision log entries (log_decision, 6 positional args): D11 removal wording (title "Informant", subtitles "Too many wrong targets" / "Eliminated for the round"); AC-ASN-45 rewrite; D4 value StartToFinalMinPathCm = 44 m (<= 44.4 m for 6 courts); Penalty OQ-NKP-13 ownership; D10 applied on RetireBinding not as transition reason; AC-SCL-50 test values (93 cm/s).
- Review-log notes: append "Closeout cross-doc pass 2026-10-25" (2-5 lines: what changed and why, ref production/design-closeout/) to design/gdd/reviews/<stem>-review-log.md for: concealment-bushes, assassination-defence-reversal, hud, npc-kill-penalty-informants, character-pool-appearance, round-match-framework, game-concept, shared-character-locomotion, camera-system, crowd-npc-behaviour, procedural-city-generator, level-socket-contract, objectives-chain, participant-state-roster, session-travel, information-visibility-policy, informant-pings-team-comms (only those changed).
