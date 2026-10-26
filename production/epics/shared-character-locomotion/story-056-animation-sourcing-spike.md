# Story 056: Animation sourcing spike: strike, victim fall and brace from sample edits versus Meshy motion (D54)

> **Epic**: Shared Character & Locomotion
> **Status**: In Progress
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R10 one animation set); `design/gdd/assassination-defence-reversal.md` (ASN-R16 strike, warp and contact contract; ASN-R17 one fall clip; F1 and I-ASN-1 lunge reach; tuning `StrikeContactSec` 0.40 s, `StrikeWarpLimitCm` 240, `DeathFallSec` 1.2 s); `design/art/art-bible.md` 5.5 (tell vocabulary: brace and strike signatures), 1.3.7 T3 (greyscale silhouette at 20 m and 30 m)
**Requirement**: `TR-scl-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0013: Animation: motion matching and significance budget (§1 one animation asset set; action poses and the fall)
**ADR Decision Summary**: One AnimBP, one PoseSearch database set and one chooser set for every pawn; Strike/Counter clips scheduled from `ContactServerTime`/`WarpPoint` and seeked by `EstimatedServerNow()`; **one** fall montage from `FDeathState.DeathServerTime` for every victim kind. Nothing is selected by controller kind (Pillar 1).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Decision D54 (`production/design-closeout/2026-10-25_rereview-followups.md`, last entry) sources locomotion and traversal from the Game Animation Sample (shared UEFN skeleton, no retarget) and the custom clips in order (1) edits of sample clips in UE Sequencer/Control Rig, (2) Meshy text-to-motion retargeted via Blender/IK Retargeter (at most 30 credits for this spike), (3) free Fab/Epic UE5-mannequin content, (4) user video mocap, (5) Blender hand-key. This spike compares routes 1 and 2 on three test clips. SCL-045 decided proxies **follow replicated movement** (E12): root-motion clips play cosmetically from replicated state and the capsule path is the server's, so a candidate's root motion must be extractable on the `root` bone and usable by motion warping (`UMotionWarpingComponent` is Beta in 5.8.4).

**Control Manifest Rules (this layer)**:
- Required: every Shared Pawn uses the one AnimBP, the one PoseSearch database set and the one chooser set (ADR-0013); on authority every pawn updates its pose every frame (montage time, root motion and notifies never skipped).
- Forbidden: never select an animation asset, layer or graph by controller, team, participant state or `AppearanceId`; never let gameplay rules read anim notifies, bone/socket transforms or curves (ADR-0013).
- Guardrail: animation ≤ 4 ms at 60 characters (ADR-0013 §5); the spike adds no runtime cost.

---

## Acceptance Criteria

*Derived from D54, ADR-0013 §1 and the art bible 5.5 / 1.3.7 T3 (the spike has no pass or fail per candidate; it ends in a decision):*

- [ ] **AC-1 Candidates.** GIVEN the three test clip types — strike (standing lunge to contact), victim fall, brace (defensive hold pose loop) — WHEN the spike runs, THEN for each type at least one route-A candidate (an edit of Game Animation Sample clips in Sequencer/Control Rig) and at least one route-B candidate (Meshy text-to-motion or a Meshy library motion, retargeted onto the UEFN mannequin skeleton in Blender) exist as `UAnimSequence` assets on `SK_UEFN_Mannequin`'s skeleton; every route-B FBX is in `assets/animations/spike-d54/` and carries exactly 88 skeleton nodes and no Null.
- [ ] **AC-2 Same-skeleton comparison.** GIVEN every candidate on the same skeleton and the same mannequin mesh, WHEN each is measured, THEN the report in `production/qa/spikes/` records per candidate, each as a number or a stated observation: (a) silhouette readability — greyscale renders at 20 m and at 30 m (60° vertical FOV, 1920×1080, profile and front) at the key frames, and whether the tell signature of art bible 5.5 is present (strike: closing lunge with arm extension to the torso; brace: compression toward about 60 % of stature, feet planted wide, forearms raised, no translation or turn while held); (b) foot sliding — maximum horizontal drift of the planted ball bone over any planted run, in cm; (c) root motion per the SCL-045 findings — translation on the `root` bone (strike) or in place (brace, fall), the strike's time from motion onset to contact against `StrikeContactSec` 0.40 s and its authored root translation at contact against the about 150 cm F1/I-ASN-1 assume (warp scale ≤ 1.6), the fall's duration against `DeathFallSec` 1.2 s (safe 1.1-1.6 s), and the brace's pelvis and yaw drift during the hold; (d) Pillar 1 — the same asset plays on an AI-driven and a player-driven pawn through the one AnimBP with no per-kind selection, with a side-by-side capture; (e) cost — Meshy credits and authoring time per clip.
- [ ] **AC-3 Decision.** GIVEN the comparison, WHEN the report is closed, THEN it names one source route per clip type (strike, fall, brace) with the reason, states which route the remaining D54 custom clips start from (strike variants `Strike_L60`/`Strike_R60`, reversal, objective tend, bush enter/exit, crowd conversation), and the decision is logged with `log_decision`.
- [x] **AC-4 Budget.** GIVEN the D54 cap, WHEN route B is produced, THEN Meshy spend is at most 30 credits and is recorded with every task id and prompt (provenance file beside the FBX files).

*QA clarification (story authoring 2026-10-26, not a GDD change):* advisory usability thresholds for AC-2, used only to explain the decision: planted-foot drift ≤ 3 cm per run; brace hold pelvis drift ≤ 3 cm and yaw drift ≤ 2°; strike onset-to-contact within ±0.1 s of `StrikeContactSec` after at most a 0.75-1.33× retime; fall duration inside the `DeathFallSec` safe range after at most the same retime.

---

## Implementation Notes

*Derived from D54 and ADR-0013 §1:*

- **Route B (non-UE half):** Meshy text-to-motion (`swift` first, one `prime` only if swift is unusable) plus a library check (`meshy_list_animations`, `meshy_animate` on the existing CPA-003 auto-rig, so no new rig credits). Retarget headless in Blender with `tools/asset-pipeline/retarget_motion_to_mannequin.py` onto the armature `root` of `assets/characters/body_base/Body_Base.blend` (opened read-only), export animation-only FBX with the frozen `build_body_base.FBX_EXPORT_PRESET` axes, re-import and compare. The user's live Blender scenes (StealthPipeline, StealthKit) are never touched.
- **Route A (UE half):** in the editor, build the same three clips from Game Animation Sample clips with Sequencer and Control Rig (for example a lunge from a sample start or pivot with an arm-extension layer, a fall from a sample stop with a pose-to-ground key, a brace from a sample idle with a crouch and forearm pose key held). Same skeleton, so no retarget.
- **Comparison scene:** one test map (not `L_Prototype`, and no sample camera asset or `PC_Sandbox` is resaved: `production/never-resave-assets.md`) with the mannequin played on an AI-driven and a player-driven Shared Pawn side by side; renders through the same 20 m / 30 m reference camera the Blender half used.
- **Prototype replaced**: none (spike; the chosen clips are promoted by the Assassination and SCL stories that consume them).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Production strike/fall/brace playback, scheduling and warp (Assassination epic, ADR-0018). Story 015 (planted brace behaviour), story 041 (one body and animation set), story 044 (the 4 ms gate).
- The other D54 clip types (decided by AC-3, produced by their own stories).

---

## Files

- `production/qa/spikes/2026-10-26_D54-meshy-motion-half.md` (route-B report; the UE comparison report is added beside it)
- `tools/asset-pipeline/retarget_motion_to_mannequin.py`, `tools/asset-pipeline/motion_contact_sheet.py`
- `assets/animations/spike-d54/` (route-B FBX clips, `source/` Meshy originals, `D54_meshy.prov.json`)
- Evidence: `production/qa/evidence/story-056-animation-sourcing-spike/` (metrics JSON, renders, contact sheets)
- No game code changed

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. No automated test; the evidence is the report, the metrics and the renders.*

Test: route-B candidates and measurements (AC-1 part B, AC-2 a-c and e for route B, AC-4)
  Given: Meshy clips for strike, fall and brace and the Body_Base armature
  When: `retarget_motion_to_mannequin.py` retargets and exports each clip
  Then: each FBX has 88 skeleton nodes and no Null, re-imports with zero position error at the sampled frames, and its metrics JSON and 20 m / 30 m renders exist; credits ≤ 30
  Edge cases: a source whose rest pose is not on the ground (SMPL prime output); a clip that starts mid-action (no standing first frame)

Test: route-A candidates and the same-skeleton comparison (AC-1 part A, AC-2 all, AC-3) — DEFERRED (engine run pending)
  Given: route-A and route-B candidates imported on `SK_UEFN_Mannequin`
  When: each plays on an AI-driven and a player-driven Shared Pawn through the one AnimBP in the comparison map
  Then: the report carries (a)-(e) for every candidate and names a route per clip type
  Edge cases: root motion on the `root` bone imported from the FBX root node; frame offset of the FBX take; ground penetration of the lying body

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the spike report(s) in `production/qa/spikes/` with the metrics and renders in `production/qa/evidence/story-056-animation-sourcing-spike/`; no automated test (a measurement and a decision)

**Status**: [~] Route-B half: `production/qa/spikes/2026-10-26_D54-meshy-motion-half.md` with metrics, renders and contact sheets. Route-A half and the UE comparison: DEFERRED (engine run pending)

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-003-meshy-test-character-generate-normalise.md` (Complete: the Meshy auto-rig reused for the library motions, and the Body_Base pipeline); `production/epics/shared-character-locomotion/story-045-net-emulation-traversal-spike.md` (Complete: the E12 root-motion decision)
- Unlocks: the Assassination epic's strike, fall and brace clip stories; story 015 (brace clip); the D54 custom-clip production order

---

## Progress Notes

**2026-10-26 (unattended run, route-B half):** AC-4 done (28 credits). AC-1 route-B and AC-2 (a)(b)(c)(e) measured for route B on 7 candidates (2 strike, 2 fall, 3 brace) in Blender; see the report. **DEFERRED (engine run pending):** AC-1 route A, the UE import of the route-B FBX files, AC-2 (d) Pillar 1 side-by-side and the same-scene UE renders, AC-3 the decision (the report gives a provisional route-B pick per clip type only). Status stays In Progress.
