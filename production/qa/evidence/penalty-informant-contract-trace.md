# Informant contract trace (AC-NKP-47, story nkp-015 AC3)

Review, ADVISORY. Source: PEN-R12 in `design/gdd/npc-kill-penalty-informants.md` (the nine rows, user-locked) and the
"Enforced by" owners' Acceptance Criteria sections, read 2026-10-27. Each row maps to at least one acceptance criterion in
its owner's GDD. Ids were checked by search in the owner GDD; the Input & Interaction Framework and Round & Match Framework
GDDs number their criteria (no `AC-<SYS>-` prefix), so those are cited as "IIF AC n" / "RMF AC n".

| # | PEN-R12 row | Enforced by (rule) | Owner GDD | Owner AC id(s) | Built today |
|---|---|---|---|---|---|
| 1 | walks, looks and toggles the camera (Run clamped to Walk) | IIF R7, SCL R6 | `input-interaction-framework.md`, `shared-character-locomotion.md`, `camera-system.md` | IIF AC 13 `LegalityMatrix` (Run clamped to Walk for an Informant); AC-SCL-10 `InformantRestrictions`; AC-CS-21 `InformantSameRules` | SCL clamp yes (SCL-014); legality source still the prototype PlayerState adapter (iif-071) |
| 2 | cannot run, traverse, interact, brace, attack or hide (`NotNow`) | IIF R7 | `input-interaction-framework.md` | IIF AC 13 `LegalityMatrix`; IIF AC 44 `RosterLegalityClass`; IIF AC 28 `InformantPlate` | Attack and Prepare yes (the slot's legality re-check); Traverse, Interact, Hide no (IIF story 057) |
| 3 | cannot see concealed non-allies (no x-ray) | IVP R9a, Camera CS-R11.4 | `information-visibility-policy.md`, `concealment-bushes.md`, `camera-system.md` | AC-IVP-04 `ViewerClassInvariants` (`OpponentInformant ≡ Opponent`); AC-CON-04 `InformantNoXray`; AC-CON-05 `IsConcealedFromTruthTable`; AC-CS-21 | no (viewer class not built) |
| 4 | keeps every ally's marker, including a concealed ally's | IVP R9a, Teammate Markers | `information-visibility-policy.md`, `teammate-markers.md`, `concealment-bushes.md` | AC-IVP-04; AC-TMK-16a `ViewGateFollowsCamera` (an informant viewer shows a concealed ally's hatched marker); AC-TMK-13 `InformantReturn`; AC-CON-04 | no (Teammate Markers not built; prototype HUD marker) |
| 5 | looks exactly like an NPC to every opponent | IVP R9b, Roster R10 | `information-visibility-policy.md`, `participant-state-roster.md`, `npc-kill-penalty-informants.md` | AC-IVP-01 `ReplicatedPropertyAudit`; AC-IVP-02 `RawBunchSchemaParity`; AC-PSR-52 `InformantKillLooksLikeNpcKill`; AC-NKP-17, AC-NKP-37 | partly: no channel or NetGUID change (nkp-014 AC2 clause); the owner-only pointers are IVP story 018 (Blocked) |
| 6 | does not count for elimination | Roster F1-F4, Round R15 | `participant-state-roster.md`, `round-match-framework.md` | AC-PSR-10 `UndersizedRostersAndLock` (F1-F4 counts); RMF AC 18 (a lone informant: no outcome); RMF AC 48 (F2 zeroing clauses) | yes |
| 7 | can be killed; the kill is not a strike; the killer's feedback is an NPC kill's | PEN-R2, PEN-R3, ASN-R19 | `npc-kill-penalty-informants.md`, `assassination-defence-reversal.md`, `participant-state-roster.md` | AC-NKP-01 `StrikeRuleMatrix`; AC-NKP-03 `InformantKillParity`; AC-ASN-06 `NoTargetDependentRejection`; AC-PSR-52 | yes |
| 8 | respawns near its team's spawn, still an informant, never Active again this round | PEN-R7-R10, Roster (no revival) | `npc-kill-penalty-informants.md`, `participant-state-roster.md`, `shared-character-locomotion.md` | AC-NKP-09 `OneRespawnPath`; AC-NKP-10 `RespawnDueTime`; AC-NKP-14 `StarvationWidening`; AC-PSR-17 `NoRevivalNoLateEvents`; AC-SCL-38 `InformantRespawnTakesOverUnseenCrowdPawn` | yes, through the documented stand-ins (LSC 033 pick, SCL 028 takeover) |
| 9 | talks to its team by the in-game team ping | Informant Pings PNG-R1..R11 | `informant-pings-team-comms.md` | AC-PNG-01 `OpponentSeesNothing`; AC-PNG-02 `PawnParity`; AC-PNG-06 `InputContexts` | not built (pings epic) |

Result: no blank entry; every row has at least one owner AC id that exists in the owner's GDD.

Findings (reported to the owning epics, never patched in Penalty):

- **input-interaction-framework**: its criteria have ordinal numbers only (no stable `AC-IIF-` ids), so a renumbering silently
  breaks this trace (it happened once: "Criteria 1-53 were renumbered"). Suggest stable ids.
- **input-interaction-framework story 057**: rows 1-2 rely on the `Informant` legality class; today the clamp reads the
  prototype's replicated PlayerState (iif-071 / ADR-0011) and Traverse, Interact and Hide have no Informant rejection.
- **information-visibility-policy story 018** (Blocked): row 5's owner-only `PlayerState` / `Controller` / `Owner` reset on
  `AStealthPawn` is not built; until it is, a takeover is visible in replicated content (spike S1 baseline R0).
- **round-match-framework**: R15's "Active count" rule is exercised by AC 18 / AC 48 but no criterion names an informant's
  embodied pawn being killed while its team's last Active player lives (covered from the Penalty side by AC-NKP-08).
