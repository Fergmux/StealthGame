# S10 re-open frame sequence — evidence and sign-off

**Story:** `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`
**Report:** `production/qa/spikes/2026-10-25_S10.md`
**Captured:** 2026-10-25, run `s10-l1b` (StealthLeak1), render client C3 (640×360, 60 fps cap), UE 5.8.4 Development editor binary in `-game`

## Frames

Folder: `production/qa/evidence/story-005-spike-s10-reopen-behaviour-and-netguid/frames/`

| File | Content |
|---|---|
| `strip_252_585.png` | crop of the left subject: culled frame (absent) → re-open frame f00 → f01 → f02 → f05 |
| `strip_252_673.png`, `strip_270_686.png` | the same sequence for two more re-opens (left and right subjects) |
| `reopen_252_516_gone.png` | full frame after the relevancy destroy (subject absent, left of the player) |
| `reopen_252_585_f00.png` | full frame of the re-open: the re-created pawn renders collapsed (un-evaluated pose) |
| `reopen_252_585_f01.png`, `reopen_252_585_f05.png` | next frames: the pawn stands at its held position |

## Observation

The re-created pawn appears at the right place (root error ≤ 0.5 cm) but its first rendered frame shows an un-evaluated, collapsed pose; it is correct from the next frame. This is a one-frame visible pop on every re-open (S10 Q3b: FAIL on the prototype AnimBP).

## Sign-off

| Role | Name | Decision | Date |
|---|---|---|---|
| Lead (network/visual) | — | [ ] Approved | pending |

*Captured unattended; the lead has not reviewed it yet.*
