# Penalty visual-continuity evidence and sign-off (story nkp-019)

Story: `production/epics/npc-kill-penalty-informants/story-019-takeover-and-exile-visual-continuity.md` (Visual/Feel,
BLOCKING, needs an art-lead sign-off). Observer: dev-story agent (unattended run), 2026-10-27. Build: the working tree of the
NKP 016-022 commit (StealthGameEditor Development, UE 5.8.4). Map L_Prototype, 3-player listen-server PIE, 50 crowd NPCs,
no fixed seed (the PIE session has none). Images: `production/qa/evidence/npc-kill-penalty-informants-016-022-2026-10-27/`.

## AC1 / AC-NKP-12: takeover return (the informant's own client)

| Capture | Shows |
|---|---|
| `nkp-12-1-before-cut.png` | the awaiting informant one capture before the takeover: "Returning in 1", the view on its released body |
| `nkp-12-2-after-cut-new-body.png` | the next refreshed frame: the view cut to the new body (control epoch 2), no fade, no spawn effect, the countdown gone |
| `nkp-12-owner-new-body.png`, `nkp-12-3-owner-after-takeover-round1.png` | the informant in its new body, frozen tally, INFORMANT panel |
| `nkp-12-ally-marker-on-new-body.png` | the ally's (host's) view: its teammate marker "(informant)" on the new body |

Observed: the cut happens on one frame (no blend of the view between the two bodies), the new body walks. NOT observable
yet: the informant grade on from the cut frame (Camera's `SetOwnerGrade(Informant)`, camera-system story 019, not built)
and the fade-in over NoPawnFadeSec from the cut frame (camera-system story 017, not built): both are Camera's, recorded as
dependent and pending.

## AC2 / AC-NKP-36: exile body continuity seen by an opponent

Not captured: the per-frame log of an opponent client around the AI hand-over (root, head-look, LOD, animation rate) needs
a placed observer and a per-frame dump that the run did not have. Related measured evidence: on the opponent's machine the
released pawn keeps the same actor, actor channel and NetGUID and its replicated non-movement properties change exactly as
in an NPC kill by the same attacker except `Actor.PlayerState` (IVP story 018), in
`StealthGame.Penalty.PIE.RemovalAudiences` (story nkp-016). PENDING.

## AC3 / AC-NKP-43: the exiled owner's own view at the release

| Capture | Shows |
|---|---|
| `nkp-43-1-released-body-held-view.png` | during the card: the view held on the released body |
| `nkp-43-2-released-body-walks-off-under-ai.png` | after the strike lockout: the released body walks away under the crowd's AI, the view following it, no snap visible |

Found and fixed while measuring (story nkp-017): before the fix the owning client's view jumped to the PlayerController the
moment the pawn replicated null (`APlayerController::SetPawn`'s auto-managed camera target); `AStealthPlayerController::SetPawn`
now keeps the view on the released body (ADR-0019 section 4.4). The per-frame bounds (<= 0.5 cm / 0.5 deg root, <= 1 deg
head-look across the role switch) were not measured.

## Sign-off

| Role | Verdict | Date |
|---|---|---|
| Evidence captured (dev-story, unattended) | Captured (AC1 partial, AC2 not captured, AC3 partial) | 2026-10-27 |
| Art lead | [ ] PENDING: no human art lead in the unattended run; the bounds of AC2/AC3 also await the art lead's confirmation | |
