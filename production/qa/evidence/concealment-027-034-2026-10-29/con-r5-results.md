# R5 leak matrix on the production hide — results sheet (story con-034, AC-CON-23, VA-4)

Date: 2026-10-29 · Build: UE 5.8.4 Development editor, standalone PIE, L_Prototype, D3D12 SM6, Lumen software tracing,
`r.RayTracing=False` · Rig: `Stealth.R5Matrix 60` (`Source/StealthTestHarness/Private/Scenarios/R5LeakMatrixCapture.cpp`) ·
Analysis: `python tools/analysis/r5_matrix.py Saved/Harness/R5Matrix` (observations only) · Retained: the `con-r5-*` files in
this folder (the full capture set is regenerable under `Saved/Harness/R5Matrix`).

**Verdict for the lead: PARTIAL — the production hide removes every render proxy and leaves no body pixel in the measured
frames, but several lighting cells are NOT ASSESSED (their two empty captures differ globally) and the 25 m magenta row has no
valid control. Lead sign-off and a re-run of the open cells are required before the story closes.**

## What the rig does (production path only)

The subject is the game's character class spawned as an unbound NPC (a non-ally of the viewer) with a groom on its head and an
active Niagara system under the mesh. It hides only through `UPawnCommandComponent::Hide(Enter)` -> Round's queue ->
`UConcealmentService` -> the commit, and the local `UConcealmentComponent` latch applies `ApplyHidden` at
`Start + BushEnterSwapSec`; nothing in the rig calls `SetVisibility` on it. The crowd is hidden for the session (people walking
through the frame are not renderer noise), auto exposure is fixed (`r.EyeAdaptationQuality 0`) and restored after.

Per cell (sun low -10 deg / noon -80 deg / shade -55 deg under a 6 x 6 m slab x VSM on / off): EmptyA (subject 200 m away),
Visible (subject at the bush's entry), Hidden (subject concealed), EmptyB, six captures each; every frame of the enter (press
to swap + 0.25 s) and of the exit (press to crossing + 0.15 s). LOD rows: the concealed subject at every LOD of its mesh (the
mesh has **one** LOD). Magenta rows: the subject's own materials tinted pure magenta through its appearance colour parameter,
viewer at 5, 15 and 25 m, every frame of one enter and exit. Screenshot timing: a request is served by the next rendered frame,
so each single frame is analysed at the next logged frame's server time.

## Results

### Render proxies (engine level), every cell and LOD

Hidden state: **no proxy** on any subject primitive (CharacterMesh0, R5Groom, R5Niagara) in all six cells and at LOD0 (the only
LOD); `bCreateRenderStateForHiddenComponentsWithCollsion` 0. (`con-r5-r5matrix.jsonl`, `"primitives"` of every `*_Hidden`
state.)

### Pixels

| Cell (run 4) | Converged? (EmptyA vs EmptyB) | ROI px | ROI mean Hidden-EmptyA | ROI mean noise | Verdict |
|---|---|---|---|---|---|
| LowSun_VSM1 | no (floor p99 46.8: the sky light / GI was still settling) | 0 | - | - | NOT ASSESSED |
| LowSun_VSM0 | yes (0.95) | 3,486 | 0.402 | 0.214 | residual above noise: see ghost below; lead to judge |
| Noon_VSM1 | no (36.5) | 0 | - | - | NOT ASSESSED |
| Noon_VSM0 | yes (0.79) | 3,910 | 0.387 | 0.271 | within ~0.1 of noise |
| Shade_VSM1 | yes (0.91) | 6,184 | 0.322 | 0.258 | within ~0.1 of noise |
| Shade_VSM0 | yes (0.94) | 5,980 | 0.296 | 0.286 | equal to noise |

Run 3 (same rig, before the long settle) had Noon_VSM1, Noon_VSM0 and Shade_VSM1 converged with ROI Hidden 0.25 / 0.247 / 0.244
against noise 0.212 / 0.212 / 0.227 (`con-r5-analysis-run4.jsonl` keeps run 4; run 3's numbers are quoted here). The visible
control is 18-19 luma levels in the same ROIs, so a leak of the body or its shadow would read 50-100x the residuals above.

### Single frames around the swap and the crossing

In every converged cell the frames inside the hidden window are clean except **2-3 frames right after the swap**, which show a
faint temporal-AA ghost of the body inside the bush silhouette (150-270 pixels above 10 luma levels against ~25 for an empty
frame; `con-r5-taa-ghost-noon-vsm1-run3.png`, heat x8 on the right). It is TSR history of the frame before the swap, inside the
rustle window that AC-CON-23 excludes ("no difference outside the rustle window"); it is recorded for the lead, not as a pass.
No frame before the crossing shows the body on the exit side.

### Magenta occlusion proxy

| Distance | Visible control (body px) | Frames swap -> crossing | Magenta px in them |
|---|---|---|---|
| 5 m (run 3) | 1,707 / 1,728 | 6 enter + 7 exit | **0** |
| 15 m (run 3) | 266 / 266 | 6 enter + 7 exit | **0** |
| 25 m | 0 (no valid control: the viewer direction chosen had the bush between it and the entry) | - | NOT ASSESSED |

Run 4 moved the subject's entry side to face the viewer, but its magenta captures read 0 in the controls too (the tint did not
register under the fixed exposure); run 3's 5 and 15 m rows are the valid ones (`con-r5-magenta-proxy-run3-grid.png`).

### Hardware ray tracing

The four HW-RT Lumen cells: **N/A by configuration** — the project ships with `r.RayTracing=False` (ADR-0022, the rendering
configuration decision); no RT build exists to capture. Not a pass.

## Open for the re-run (one editor session, about 10 minutes)

1. Settle each cell until EmptyA's mean luma stops moving (or lock the sky light recapture) before capturing; re-run LowSun_VSM1
   and Noon_VSM1.
2. Re-run the 25 m magenta row with a framed control, and run 4's magenta controls under auto exposure.
3. Lead decision on the 2-3 frame TSR ghost inside the rustle window (accept as in-window, or hide one frame earlier).

## Sign-off

- [ ] lead (technical-artist / art-lead) — pending (human)
