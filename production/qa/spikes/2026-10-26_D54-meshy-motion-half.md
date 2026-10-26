# Spike D54, route B (Meshy motion) half: strike, victim fall, brace

> **Story**: `production/epics/shared-character-locomotion/story-056-animation-sourcing-spike.md` (Status In Progress)
> **Decision**: D54 (`production/design-closeout/2026-10-25_rereview-followups.md`, last entry)
> **Date**: 2026-10-26 (unattended run, non-UE tools only: Meshy MCP, Blender 5.2.3 headless, Python)
> **Scope of this half**: route 2 of D54 (Meshy text-to-motion and Meshy library motion, retargeted onto the UEFN mannequin skeleton in Blender). Route 1 (Game Animation Sample edits in Sequencer/Control Rig), the UE import and the same-scene comparison are the UE half, listed at the end.

## Summary

| Clip type | Best route-B candidate | Verdict (route B only) | Main gap |
|---|---|---|---|
| Strike | `A_D54_Strike_T2MSwiftFast` (swift, 3 cr) | Usable shape, strongest silhouette of the run | Contact 0.67 s after onset (target 0.40 s); authored root translation at contact 55 cm, so a 1.6 warp cap reaches about 88 cm, short of the about 150 cm F1/I-ASN-1 assumes; the clip steps back to its start |
| Victim fall | `A_D54_Fall_T2MSwift` (swift, 3 cr) | Usable after a trim | The clip starts in a hunched flinch (1.2 s preface), not from a stand; collapse itself 1.27 s (inside `DeathFallSec` 1.1-1.6 s); body lands 1.49 m ahead of the capsule; lying body 8 cm into the ground |
| Brace | `A_D54_Brace_T2MPrime` (prime, 10 cr) | Matches the art bible 5.5 signature | Stature 0.64-0.66 of standing (target about 0.60), held still (pelvis 2.8 cm, yaw 1.0°); the step-out is a foot slide, not a step; very wide stance (110 cm) may be louder than needed |

**Provisional route-B pick** (the story's AC-3 decision waits for route A): strike → Meshy swift lunge *only if* route A cannot produce a 0.40 s contact with about 150 cm of authored root translation (the Meshy clip needs both fixed, so route A is favoured for the strike); fall → Meshy swift, trimmed to start from a stand or blended from locomotion; brace → Meshy prime. Library motions (Meshy animate) were tried for fall and brace and are not better than text-to-motion for either.

## Credits

Balance 3,029 before, **3,001 after: 28 credits spent** (cap 30). Unattended Meshy budget: 281 + 28 = 309 / 2,000 used, 1,691 left.

| Task | Type | Mode | Credits | Output |
|---|---|---|---|---|
| `01a17683-54ed-72b0-b1f7-1257ca28fa8c` | text-to-motion strike | swift | 3 | `source/T2M_Swift_Strike.bvh` |
| `01a1768b-fc2b-7173-8ec4-f535a4e4b304` | text-to-motion strike (faster prompt) | swift | 3 | `source/T2M_Swift_StrikeFast.bvh` |
| `01a17683-58f3-77d6-8451-6006904cb00a` | text-to-motion fall | swift | 3 | `source/T2M_Swift_Fall.bvh` |
| `01a17683-5ea6-74f7-ac75-069c44ee014e` | text-to-motion brace | swift | 3 | `source/T2M_Swift_Brace.bvh` |
| `01a1768d-1a71-774e-a048-8dfe33629483` | text-to-motion brace | prime | 10 | `source/T2M_Prime_Brace.fbx` |
| `01a17683-6345-776d-82d7-5f2291b832cf` | library animate, actions 184 Shot and Fall Forward + 138 Block1, on the CPA-003 auto-rig `01a171e9-...` (no new rig) | — | 6 | `source/Lib_MeshyRig_FallForward_Block1.glb` |

Prompts and settings: `assets/animations/spike-d54/D54_meshy.prov.json`. The prime was spent on the brace because the swift brace failed the art bible's defining trait (no compression, no step-into-stance), i.e. it was unusable for the brace spec; the strike and fall swift clips were usable.

**Library check (free, `meshy_list_animations`):** no library motion is a standing lunge to contact (closest: Thrust Slash, Elbow Strike, punches from a boxing guard). Falls: Dead, Shot and Fall Forward/Backward, Strangled and Fall Forward (7.5 s, too long), Knock Down, Fall Dead from Abdominal Injury. Brace-like: Block1-Block10 (boxing guards), Two-handed Parry. The two tried were chosen from their preview GIFs.

## Method

`tools/asset-pipeline/retarget_motion_to_mannequin.py` (headless; opens `assets/characters/body_base/Body_Base.blend` read-only, never saves it; the user's StealthPipeline/StealthKit scenes were not touched):

1. Retarget pose: every mapped limb bone of the mannequin is turned so its chain direction matches the source's rest pose (Meshy text-to-motion T-pose, SMPL-H T-pose, Meshy auto-rig A-pose); hands also match the across-the-palm vector. Spine, neck and head keep the mannequin's rest.
2. Per frame, mannequin world rotation = source world delta from rest × retarget pose. Pelvis translation = mannequin rest + leg-length ratio (0.95 swift, 1.05 prime, 1.07 auto-rig) × source hip displacement; one vertical offset puts frame 1's lower ankle at the mannequin's standing ankle height.
3. Root motion: strike clips put the forward (−Y) pelvis displacement on the `root` node (the armature object, Unreal's root bone); lateral sway and height stay on the pelvis. Fall and brace are in place.
4. `ik_foot_*`/`ik_hand_*` follow `foot_*`/`hand_*`; twist, weapon and attach bones keep rest relative to their parent; fingers are mapped for the swift skeleton only (prime and auto-rig fingers stay at the mannequin's rest).
5. Export animation-only FBX with the frozen `build_body_base.FBX_EXPORT_PRESET` axes, 30 fps, every bone baked; **re-import check: 88 skeleton nodes, no Null, 0.000-0.001 cm position error** for pelvis, head, right hand, left foot, right ball at five frames on all 7 files. The FBX take re-imports one frame late in Blender (frames 2..N+1); the UE half checks Unreal's frame range.
6. Metrics (`<clip>_metrics.json`) and renders: 3/4 close-ups for review; greyscale profile at 20 m and 30 m and front at 20 m with a 60° vertical FOV at 1920×1080, cropped to the figure at true pixel size (figure ≈ 77 px tall at 20 m, ≈ 52 px at 30 m). Contact sheets: `production/qa/evidence/story-056-animation-sourcing-spike/sheet_<clip>.png`.

Foot sliding is the largest horizontal drift of the planted ball bone over any planted run (ankle and ball both within 2.5 cm of their clip minimum height for ≥ 3 frames). The source's own drift, scaled to the mannequin, is shown beside the target's: where they match, the drift is in the generated motion, not added by the retarget.

## Per-clip results

### Strike

| | `Strike_T2MSwift` | `Strike_T2MSwiftFast` |
|---|---|---|
| Length | 2.5 s (75 f) | 2.0 s (60 f) |
| Motion onset | 0.13 s | 0.00 s (moves from frame 1) |
| Arm at 90 % of peak reach | 1.07 s (0.93 s after onset) | **0.67 s** |
| Peak reach | 1.23 s; hand 138 cm ahead of the start pelvis | 0.90 s; 150 cm |
| Root translation at 90 % reach / at peak / at end | 68 / 74 / 97 cm (ends forward) | **55 / 63 / 10 cm** (steps back to the start) |
| Peak root speed | 156 cm/s | 146 cm/s |
| Planted drift L / R, target (source scaled) | 8.6 (12.0) / 2.2 (1.7) cm | 3.1 (3.3) / 2.8 (2.4) cm |
| Lowest mesh point | −1.9 cm | −2.1 cm |
| Striking hand | right | right |

- **Readability.** Fast clip: a deep fencing lunge with the right arm fully extended and a wide leg gap; at 20 m and 30 m in profile it is the most distinctive silhouette of the run and plainly not a baseline pose. Front view: the arm is foreshortened and it reads mostly as a widening stance plus a torso turn, so the front read is weaker (art bible T3 tests both). First clip: a walking step then a punch; the extension reads in profile but the lead-in looks like a B1 walk step.
- **Foot contact.** The first clip's left-foot 8.6 cm drift is in the source (12 cm), so it is a generator defect, not the retarget; the fast clip is within 3 cm.
- **Root motion (SCL-045 E12).** Root translation is on the `root` node and extractable; proxies follow replicated movement, so the clip's root curve only has to agree with the server's warp. Two contract gaps: (1) **timing** — contact 0.67 s vs `StrikeContactSec` 0.40 s needs a 0.6× retime of the pre-contact segment (outside the 0.75-1.33× advisory band; a lunge sped up that much may read as a teleport, I-ASN-7); (2) **authored distance** — F1/I-ASN-1 assumes about 150 cm of authored root motion so a 1.6 warp reaches `StrikeWarpLimitCm` 240; 55-74 cm authored gives 88-118 cm. Either the clip is re-authored with a longer closing step (a two-step lunge prompt, or a route-A edit), or game-designer revisits `StrikeWarpLimitCm` / the warp-scale assumption.
- **Shape gap.** The fast clip returns to its start (recovery steps back); the contract wants the attacker to stay where the warp put it. Trim the recovery or use a forward recovery.

### Victim fall

| | `Fall_T2MSwift` | `Fall_LibShotFallForward` |
|---|---|---|
| Length | 2.5 s | 2.2 s |
| First frame | hunched flinch (mesh top 126 cm) | standing recoil (147 cm) |
| Collapse start → settled | 1.20 s → 2.47 s: **1.27 s** | 0.07 s → 2.17 s: **2.10 s** |
| `DeathFallSec` 1.2 (safe 1.1-1.6) | inside the safe range (collapse only) | 0.57-0.76× retime needed |
| Final pose | prone, top 30 cm, pelvis 11.5 cm | prone, top 40 cm, pelvis 18.5 cm |
| Pelvis travel (body lands ahead of capsule) | **149 cm** | 92 cm |
| Lowest mesh point | **−8.1 cm** (lying body in the ground) | −4.0 cm |
| Planted drift L / R, target (source) | 3.0 (1.0) / 0.8 (1.1) cm | 5.3 (6.6) / 2.9 (2.8) cm |

- **Readability.** Both read unmistakably as a fall at 20 m and 30 m in profile and front. The library clip's arms-flung backward arch reads as a gunshot, not a stab, and clashes with the composed style (art bible 5.4). The swift clip's knees-then-prone collapse fits a stab.
- **Start.** ASN-R16/R17: the victim walks or stands until the death write, then brakes and falls; the swift clip's 1.2 s hunched preface must be cut (start at about 1.1 s) or replaced by a blend from locomotion — after the cut the clip is 1.4 s long, so `DeathFallSec` holds without a retime.
- **Body position.** A 0.9-1.5 m forward travel means the body comes to rest away from the capsule; ASN-R16 says "the body falls from that position", and the body's collision/linger footprint is ADR-0018's. In-place root was used here; the UE half decides whether the fall carries root motion (the capsule follows the body) or the travel is cut.
- **Ground.** The −4 to −8 cm lying penetration comes from the ankle-based height offset; the UE half needs a pelvis-height correction near the end of the clip or a ground-alignment curve.

### Brace

| | `Brace_T2MSwift` | `Brace_LibBlock1` | `Brace_T2MPrime` |
|---|---|---|---|
| Length | 4.0 s | 3.5 s | 4.0 s |
| Entry from standing | none (frame 1 is already a guard) | none (guard loop) | **yes**, step-out and sink in about 0.6 s |
| Stature in hold (min / mean, of standing) | 0.96 / 0.97 (a 0.73 dip at 1.7 s, then rises) | 0.98 / 1.00 of its first frame (an already bent guard, 155 cm) | **0.64 / 0.66** (target about 0.60) |
| Feet apart, first → hold | 61 → 62 cm | 73 → 73 cm | 48 → **110 cm** |
| Hold drift: pelvis / yaw / feet L, R | 4.2 cm / 2.0° / 0.3, 0.4 cm | 2.9 cm / 0.5° / 0.7, 1.0 cm | 2.8 cm / 1.0° / 1.8, 1.5 cm |
| Planted drift L / R, target (source) | 3.0 (0.6) / 1.8 (1.5) cm | 6.4 (4.9) / 5.9 (2.7) cm | 6.5 (6.5) / 9.3 (9.5) cm |
| Lowest mesh point | −1.1 cm | −3.5 cm | −5.0 cm |

- **Readability.** The prime clip is the only one with the art bible signature: a compact low crouch, feet wide, forearms crossed in front of the face; at 20 m and 30 m in profile and front it is unmistakable. Both other clips read as an upright boxing guard: a tell, but missing the 60 % compression that makes the brace readable at 30 m, and the library loop is close to a "fighter" stance from the style the art bible rejects.
- **Foot contact.** The prime clip's 6.5/9.3 cm planted drift is the step-out: the feet slide outward while still near the ground (source identical, peak 66-83 cm/s), so the "step-into-stance" entry (SCL story 015) is a slide, not a step. Once planted the feet hold within 1.8 cm.
- **Root and hold (story 015: no translation, no yaw while held).** All in place; prime hold drift 2.8 cm pelvis and 1.0° yaw (inside the advisory 3 cm / 2°). For a loop the UE half picks a hold segment (about 1.0-4.0 s) and measures the seam pose difference.
- **Loudness.** The 110 cm stance may be louder than needed ("must not pass at the exaggerated level", art bible 5.5); a Control Rig narrowing on top of the prime clip is a cheap fix.

## Pillar 1 and cost

- **Pillar 1.** Every candidate is one asset on the one skeleton; nothing in the pipeline knows the pawn kind. The parity check that matters (the same asset on an AI-driven and a player-driven Shared Pawn through the one AnimBP, side by side) is the UE half.
- **Cost (route B).** Credits: 3 per swift clip, 10 per prime clip, 3 per library action. Authoring time: the retarget tool is a one-off (about 2 hours including this spike); after it each clip takes about 5 s to retarget, export, verify and render, plus one or two prompt iterations (the strike needed a second, faster prompt). The fixes each Meshy clip still needs (strike retime and longer closing step, fall preface trim and ground correction, brace stance narrowing) are UE-side edits, i.e. route-A work on top of route B.

## What the UE half must still check (DEFERRED: engine run pending)

1. **Import** each `assets/animations/spike-d54/A_D54_*.fbx` as an animation on `/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin`'s skeleton: no 89th bone, no missing-bone warnings, scale 1 (cm), facing matches the sample's clips, and the sequence length (75/60/75/66/120/105/120 frames at 30 fps) — Blender re-imports the take one frame late, so confirm Unreal's first frame is the clip's frame 1.
2. **Root motion**: on the strike clips, the `root` bone track carries the forward translation (Blender: armature-object animation exported as the FBX Root node); enable root motion and confirm the extracted curve matches the metrics (55 cm at 0.67 s for the fast clip). Fall and brace: root bone static.
3. **Route A**: author the same three clips from Game Animation Sample clips in Sequencer/Control Rig and render them through the same 20 m / 30 m camera (60° vertical FOV, 1080p, greyscale) so the comparison is like for like.
4. **Foot sliding in UE** with the production AnimBP's foot IK on and off; compare with the Blender numbers above.
5. **Pillar 1**: play each candidate on an AI-driven and a player-driven Shared Pawn side by side (no per-kind asset or branch; AC-SCL-01 must not regress).
6. **Strike contract**: retime/warp the pre-contact segment to `StrikeContactSec` 0.40 s, check the motion-warping window and scale cap against `StrikeWarpLimitCm` 240, and whether the clip reads as a teleport (I-ASN-7). Raise the authored-distance gap (55-74 cm versus about 150 cm) with game-designer if route A cannot close it.
7. **Fall**: cut the preface, fix the lying penetration, decide the 0.9-1.5 m body travel with ADR-0018's owner.
8. **Brace**: pick and measure the loop segment; narrow the stance if the art-director finds it exaggerated; the step-out slide versus story 015's step-into-stance.
9. **Licence**: producer to confirm the Meshy plan's terms cover motion output (art bible 8.9 rule 2) before any of these ship.
10. **AC-3 decision** per clip type, logged with `log_decision`, and the route the other D54 custom clips start from.

## Evidence

- Metrics: `production/qa/evidence/story-056-animation-sourcing-spike/renders/<clip>_metrics.json` (7 files)
- Renders: `production/qa/evidence/story-056-animation-sourcing-spike/renders/<clip>_<close|profile20|profile30|front20>_f<frame>.png`
- Contact sheets: `production/qa/evidence/story-056-animation-sourcing-spike/sheet_<clip>.png` (7)
- Clips: `assets/animations/spike-d54/A_D54_*.fbx` (7), Meshy originals in `assets/animations/spike-d54/source/`, provenance `assets/animations/spike-d54/D54_meshy.prov.json`
- Tools: `tools/asset-pipeline/retarget_motion_to_mannequin.py`, `tools/asset-pipeline/motion_contact_sheet.py`
