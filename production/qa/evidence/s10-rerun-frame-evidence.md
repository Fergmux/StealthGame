# S10 re-run first-visible-frame strips — evidence and sign-off

**Story:** `production/epics/information-visibility-policy/story-056-s10-fix-implementation-and-rerun.md`
**Report:** `production/qa/spikes/2026-10-26_S10-rerun.md`
**Captured:** 2026-10-26, run `s10r-l1` (StealthLeak1, warm-up on), render client C3 (640×360, 60 fps cap, HUD hidden), UE 5.8.4 Development editor binary in `-game`
**Baseline for comparison:** `production/qa/evidence/s10-reopen-evidence.md` (S10, no warm-up: a collapsed first frame on every re-open)

## Frames

Folder: `production/qa/evidence/story-056-s10-fix-implementation-and-rerun/`

| File | Content |
|---|---|
| `strips/strip_150_*.png` (16) | crops of one subject per re-open: culled frame (absent) → first visible frame f00 → f01 → f05 → f10 |
| `frames/s10r_150_gone_2610.png` | full frame after the relevancy destroy (subject absent, right of the player) |
| `frames/s10r_150_f00_2611.png` | full frame of the re-open: the re-created pawn's first visible frame, standing, posed |
| `frames/s10r_150_f01_2612.png`, `frames/s10r_150_f05_2616.png` | next frames: unchanged pose |

## Observation

Every captured re-open shows the pawn standing and posed in its first visible frame (silhouette 94-102 % of the f10 silhouette over the 15 assessable captures; the head-bone probe found 0 un-posed first frames in 3,747 first visible frames of the gate runs). On f00 and f01 the body is slightly translucent: that is the renderer's temporal history (TSR) fading a newly visible primitive in over ~3 frames, which is the same for every newly visible object and is not a pose defect. Compared with S10's strip, the collapsed frame is gone: the hidden-until-first-pose warm-up (ADR-0003 S10-R5) works on the prototype AnimBP.

## Sign-off

| Role | Name | Decision | Date |
|---|---|---|---|
| Lead (network/visual) | Lead sign-off under the user's unattended authorisation (recorded by the story-056 run) | [x] Approved — no collapsed or un-posed first frame on re-open | 2026-10-26 |

*Captured unattended and signed off under the user's blanket authorisation for this unattended run. The production re-proof with the production AnimBP is stories 027/044.*
