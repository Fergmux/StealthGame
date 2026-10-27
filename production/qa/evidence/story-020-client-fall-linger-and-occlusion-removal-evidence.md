# Story asn-020 evidence: one fall, one linger, one cut-at-occlusion removal (AC-ASN-22), 2026-10-27

Story: `production/epics/assassination-defence-reversal/story-020-client-fall-linger-and-occlusion-removal.md`
Type: Visual/Feel (retained captures + lead sign-off, BLOCKING).

## Lead sign-off

**PENDING.** The implementing agent cannot give the Visual/Feel lead sign-off. Reviewer: art-director / technical-artist
lead. Sign-off line to complete:

`Signed off by: ____________  Date: ________  Verdict: APPROVED / CHANGES REQUESTED`

What the reviewer judges: the placeholder `Death_Fall` (the D54 spike clip `A_D54_Fall_LibShotFallForward`, retargeted to
SK_UEFN_Mannequin; the authored clip is the D54 engine half) reads as "struck, then down" within 1.2 s for every kind; the
static linger; the one-frame cut at grace expiry with no fade, dissolve, sink or effect.

## Captures (retained, `production/qa/evidence/story-020-client-fall-linger-and-occlusion-removal/`)

3-player PIE (listen server + 2 clients, floating windows, L_Prototype, 50 NPCs), driven through the Unreal MCP
(`StealthBot`), windows captured with a burst variant of `tools/dev/capture-window.ps1` (one PowerShell process,
~0.15-0.3 s per capture; the editor runs ~3 fps with three PIE windows, so each "moment" is the nearest captured frame).
Every image was looked at; none is black (mean luminance 42-85).

| Kind | Camera | Files | Moments |
|---|---|---|---|
| NPC | attacker (client 2) | `asn-22-npc-1..4-*.png` | contact (graded frame), 0.8 s (falling), 1.2 s (down), linger end 7.2 s (static pose) |
| Player (reversed attacker) | defender (client 1) | `asn-22-player-1..6-*.png` | contact (lunge into the brace), 0.8 s, 1.2 s (down at the defender's feet), linger end, the frame before and the frame after the grace cut (one-frame cut, no fade) |
| Informant | attacker (client 1) | `asn-22-informant-1..6-*.png` | contact, 0.8 s, 1.2 s, linger end, before / after the grace cut (the body lies partly behind the attacker) |

Deviation from the AC's "same camera": the three kinds were filmed from three participants' cameras (the bot driver
teleports the attacker to its target, so a fixed bystander camera could not frame all three); the NPC's removal frame was
not captured (its client cut it 0.8 s after the burst ended; the presentation log records it). The timing comparison is
the PIE test's (below), not the captures'.

Presentation log excerpt: `pie-presentation-log-excerpt.txt` (verbose `LogStealthGameplay`: ClipStart / CollisionOff /
BodyHidden rows per machine). Examples: NPC C_15 (DeathServerTime 104.547): collision off at 105.812 on both clients,
hidden out of frustum at 111.812 (+7.27) on one client, cut at grace at 113.813 / 113.814 (+9.27) on the other client and
the host; reversed player (146.237): hidden out of frustum at +7.33 on two machines, cut at grace at +9.33 on the third;
informants (196.630, 246.963): occluded at +7.27 / cut at grace at +9.27. No `LogStealthFault` line in the session.

## Tests

- `StealthGame.Assassination.PIE.OneBodySequence.NpcPlayerAndInformantFallLingerAndGoAlike` (AC1, AC2): on all three
  machines, for an AI-driven NPC, a reversed player and an informant: Death_Fall starts at DeathServerTime (+0.000 to
  +0.006 s), collision off at +1.200 to +1.206 s, hidden inside [+7.2 s, +9.2 s + one frame] where a viewer hid it,
  identical across kinds within one frame; a walking pawn's capsule swept through each body after the fall meets no
  blocking hit from it; no particle / Niagara / decal component and no material change from the kill to the linger end.
- `StealthGame.Assassination.BodyRemovalEvaluator` (supporting, headless): the VA-ASN-2 rule's timing and one-frame cut.
- `StealthGame.Assassination.EncounterPresentationTimeline` (supporting): fall 1.2 s, linger 6.0 s, window +7.2 s, cut +9.2 s,
  late fall sought, never shown after the linger.
