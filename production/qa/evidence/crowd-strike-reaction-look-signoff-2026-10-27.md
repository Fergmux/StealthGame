# Sign-off: Crowd strike reaction look (story CNB-025, AC-CNB-19)

- **Reviewer role**: art-director (delegated agent review, not a human lead sign-off)
- **Date**: 2026-10-27
- **Scope**: the crowd's look only. Excluded as known prototype artifacts: the flung ragdoll body (Assassination epic), graybox sky, placeholder huts and bush blobs, HUD text.

## Frames reviewed

Evidence folder: `production/qa/evidence/crowd-strike-reaction-look-2026-10-27/`

- `clip1/frame01, 03, 05, 08, 12, 16` (6 of 16 read; strike 1, Client 1 "Eagle")
- `strike2-01s-mid-reaction.png`, `strike2-05s.png`, `strike2-15s.png` (Client 1)
- `strike3-01s-mid-reaction.png`, `strike3-05s.png`, `strike3-15s.png` (Client 2 "Jaguar")

## What a still sequence can and cannot show

Stills sampled about 0.85 s apart (clip1) or at 1/5/15 s (strikes 2 and 3) cannot measure run duration, stationary dwell time, or head and gaze direction at 20 m. Glances are a head turn of a few degrees on a figure a few dozen pixels tall. Facing the strike point is only judgeable if the strike point is in frame.

Framing is also a problem. In clip1 the strike point is not identifiable (only the flung body is visible, top of frame) and the bystander is looking across the market, not at it. In strike2 the camera looks away from the crowd (an almost empty field, one NPC near the bystander). In strike3 the bystander is beside a large bush blob with the crowd as 6-10 tiny figures at the left edge.

## Per-threshold verdict

| Threshold | Verdict | What I saw |
|---|---|---|
| No gathering (no more than 3 NPCs stationary and facing the strike point within 5 m for more than 5 s) | NOT DEMONSTRATED (no counter-evidence) | clip1 frames 12 and 16 show 3-4 NPCs close to the camera, spread out and in walking or mid-gesture poses, not packed into a ring. Frames 01-08 show a dispersed crowd. strike3-05s and 15s show the distant crowd spread out. No frame shows a cluster, but stationary dwell time and facing cannot be read from stills, and the strike point is not visible. |
| No flight from the district | WEAK PASS | Across strikes 1-3 NPCs stay in the market. In clip1 they are still present in frame 16 and in the strike2 and strike3 15 s shots. Nothing shows a mass exit, but the views cover a narrow slice of the market and do not show its boundary. |
| No NPC run longer than 3 s | NOT DEMONSTRABLE | The one NPC at the right of clip1 frame08 shows a wide-stride pose with arms out, and the strike3-15s NPC at the left is mid-stride. At 0.85 s sampling I cannot tell walk from run, and no NPC is tracked across 4 or more consecutive frames. |
| At least 3 visible glance or pause reactions | FAIL (not shown) | I cannot identify 3 reactions. Possible candidates: the strike2-05s NPC near the bystander with one arm held out (could be a brace, a pause or a normal walk pose) and the clip1 frame12/16 NPCs near the camera. None can be attributed to the strike (no gaze direction, no before/after pair of the same NPC, no strike point in frame). |

## Overall

The captures show nothing that contradicts calm: there is no stampede, no pile-up and no visible mass exit. But the criterion requires a visible positive, at least 3 glance or pause reactions, and this capture set does not show it. Three of the four thresholds cannot be confirmed from these views. The spec `StealthGame.Crowd.Strike.SharesAndLimits` supports the shares headlessly, but it is not a substitute for the visual evidence this story exists to retain.

## Sign-off

- [ ] Approved — not approved because the evidence does not show at least 3 glance or pause reactions, and run length and gathering dwell time cannot be judged from the framing and sampling. No defect in the crowd's behaviour was seen; this is a capture problem, not a tuning failure.

## Recommended re-capture

1. Bystander at 20 m but with the camera aimed so that both the strike point and the 5 m ring around it are in frame (face the strike, wide FOV), not looking away.
2. Use a closer bystander (10-12 m, inside the 15 m responder radius) for at least one take, so glances and pauses are readable as head turns and halted walks.
3. Record the real clip (video) or capture at 0.25-0.33 s intervals for the first 6 s, so run length (3 s limit) is countable. Alternatively slow the clip to 0.25x.
4. Tint or label the struck NPC and the strike point in a debug overlay (a ring on the strike point, a per-NPC state tag such as Glance, Pause, Reroute or Run), so reactions are attributable.
5. Capture a top-down or high-angle view of the market at +15 s showing the district bounds, to evidence "no flight".
6. Keep the three seeds and the 1 s, 5 s and 15 s stills per the story's capture note, then re-submit for sign-off. The creative-director or a human art-director should record the final name and date in `crowd-strike-reaction-look-evidence.md`.

---

# Take 2 (re-capture, same day)

- **Reviewer role**: art-director (delegated agent review, not a human lead sign-off)
- **Date**: 2026-10-27
- **Capture note**: a camera debug overlay was not available (crowd debug draw is banned by story 036). Instead a dev-only cvar, `Crowd.LogStrikeReactions`, logs each strike's R8 responders and the reaction each drew.

## Frames and log reviewed

In `production/qa/evidence/crowd-strike-reaction-look-2026-10-27/`:

- `take2-strike1/frame03, 08, 12, 20` (bystander at 11 m, facing the strike)
- `take2-strike2/striker03, 06, 10, 16` (the striker's view at the strike point)
- `take2-strike2/bystander08, bystander16, bystander-15s`
- `take2-server-log-r8.txt` (seed 209190993, 50 NPCs, two strikes)

## What the server log says

| Strike | Responder | Distance | Reaction |
|---|---|---|---|
| 0 | 43 | 746 cm | Pause |
| 0 | 25 | 1301 cm | Glance |
| 0 | 7 | 294 cm | None |
| 1 | 38 | 1095 cm | None |
| 1 | 7 | 1383 cm | Glance |
| 1 | 18 | 159 cm | Glance |

Six responders, four glance or pause reactions, two None. There is no Reroute, Run or Brace entry in this sample, so no NPC was ordered to run. The sample is small: with a sparse crowd near the strike points it says little about the R8 shares, which the headless spec covers.

## Per-threshold verdict

| Threshold | Verdict | What I saw |
|---|---|---|
| No gathering | PASS (with limits) | take2-strike1 frames 03/08/12/20: apart from the bystander, 2-3 NPCs are visible at 8-20 m and drift around, none clustered. The striker frames show one NPC beside the striker (not a ring) and 5-6 NPCs spread across the background. bystander08/16/15s show 1-3 NPCs walking in from the right, in walking poses, none stationary and facing the strike. At no point do more than 3 NPCs stand within 5 m. |
| No flight from the district | PASS | The crowd stays in the market in every frame, including the +15 s shot. NPCs are still in view in both strikes after 15 s, and no one is shown leaving. |
| No NPC run longer than 3 s | PASS (weak, but supported by the log) | The log has no Run or Brace outcome. At 0.3-0.35 s sampling, no NPC is in a sprint pose in any frame I read, and the walkers keep walking-pose gaits across consecutive frames. |
| At least 3 glance or pause reactions | PASS on the log, 1 corroborated by stills | Log: 4 reactions (1 Pause, 3 Glance). Stills: in striker03/06/10/16 (about 4.5 s) the NPC beside the striker stands still on one spot, turned toward the fallen body and with an arm held out, while the other NPCs around it keep walking. That is a clear pause or glance and is consistent with the 159 cm Glance (or the closest of the logged reactions). The three more distant reactions (746, 1301 and 1383 cm) are not distinguishable in the stills: at that range a 1-3 s head turn is a few pixels. |

## Residual limits

- Only 1 of the 4 logged reactions is visibly corroborated. A reader who requires 3 visible reactions strictly would not be satisfied. I have counted the log under the allowance given in the re-capture request.
- Run length and the 5 s dwell limit rest on the absence of Run, Brace and Reroute in the log and on stills, not on a tracked clip.
- Strike 3 and the third seed were not re-shot in take 2.

## Take 2 sign-off

- [x] Approved (take 2, art-director delegated agent review, 2026-10-27). The captured crowd reads calm: no gathering, no flight, no run, and a corroborated stationary reaction beside the striker, with 4 reactions in the server log. The Approved status is conditional on the lead (creative-director or the human art-director) confirming this in `crowd-strike-reaction-look-evidence.md`, since only one reaction is visibly confirmed. If the lead wants 3 visible reactions, re-capture with 8-10 NPCs seeded within 10 m of the strike point.
