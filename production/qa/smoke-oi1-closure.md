# Smoke check: OI-1 closure (CPA story 001)

**Date**: 2026-10-25
**Story**: `production/epics/character-pool-appearance/story-001-oi1-uefn-reference-measurements-recorded.md`
**Type**: Config/Data (document and golden-data change; no code, no engine run)
**Verdict**: PASS

## Checks

| # | Check | Method | Result |
|---|---|---|---|
| 1 | Golden chains equal the editor measurements | Script: recompute each `chain.*` path in `Source/StealthGameEditor/Private/Tests/CharacterPool/Golden/UefnReferenceChains.txt` from the `SKM_UEFN_Mannequin` rows of `production/qa/logs/2026-10-25_A_mannequin-bone-dump.txt` | 10 of 10 equal to 0.01 cm (thigh 39.05, shin 42.56, foot 15.15, spine 49.47, clavicle to upper arm 15.51, upper arm 26.65, forearm 22.35, hand 16.23, hand straight 15.44, neck 9.95); `bone_count=88` equals the 88 dump rows |
| 2 | Blender re-measurement agrees | `tools/asset-pipeline/body_measure.py` on `assets/characters/reference/SKM_UEFN_Mannequin.fbx` (Blender 5.2.3, headless) | Chains identical to check 1; stature 165.57 cm; FBX skeleton nodes 88 (`root` + 87 `LimbNode`); reference mesh head 19.32, shoulder 50.95, chest 27.62, hip 34.76, foot 26.25 cm |
| 3 | Art bible 1.3.2 / 1.3.5 name the reference and record the values | Read 1.3.2 table and OI-1 block, 1.3.5 Skeleton / Skinning / Rest pose rows | `SK_UEFN_Mannequin`, 165.6 cm (band 163.6 to 167.6), 88 bones, the nine chains; 175 cm and Manny 180.5 cm marked superseded |
| 4 | Every dependent value carries a number and a derivation line | Read the 1.3.2 re-derivation table, 8.6 Axes row, 8.7 step 3 | head 19.3, shoulder 51.0, chest 27.6, hip 34.8, foot 26.3, hem 52.0 +/- 7.6, envelope front 64.2 +/- 3 and side 37.1, leg gap 13.2, 8.6 import 165.6 cm at scale 1.0, 8.7 step 3 165.6 +/- 2; each row states its derivation |
| 5 | Validator tolerance stated | 1.3.2 chain row and golden file | +/- 3 percent per chain (`chain_tolerance_pct=3.0`), confirmed at the re-sign |
| 6 | OI-1 closed, re-sign recorded | 8.x.17 register and blocking-gates table, 1.3.2 OI-1 block | OI-1 CLOSED 2026-10-25; art-director re-sign of 1.3 dated 2026-10-25 (decision log) |
| 7 | Downstream change-request list complete | 1.3.2 CR table | CR-OI1-1 `TeammateMarkerAnchorCm` 195 + teammate-markers OQ7; CR-OI1-2 8.5 reference-camera table (+ SCL `H_fig`, ADR-0013 F7); CR-OI1-3 T2/T3 distances (+ CPA CP-15, G-TWIN); CR-OI1-4 to -6 in-document and GDD text; each with owning document and owner |
| 8 | No GDD edited | `git diff --stat -- design/gdd docs/` | empty |

## Notes

- The re-derived dimensions follow "chains win": the UEFN mannequin is not a scaled Manny, so scaling the
  old 175 cm figures by 0.9463 disagreed with the chain-bound body by more than the tolerance for head,
  shoulder, chest, hip and foot (decision logged 2026-10-25 in `production/session-logs/decision-log.md`).
- The 1.3.3 table itself still shows the old envelope, hem and leg-gap numbers; the 1.3.2 OI-1 block
  supersedes them until CR-OI1-4 is applied by the art-director.
