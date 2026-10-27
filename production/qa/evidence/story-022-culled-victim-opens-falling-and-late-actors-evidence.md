# Evidence: story asn-022 A culled victim re-opens already falling, and late actors seek and never snap

Story: `production/epics/assassination-defence-reversal/story-022-culled-victim-opens-falling-and-late-actors.md`
Date: 2026-10-27. Build: `tools/dev/cycle.ps1 -Tests -NoLaunch`, `Result: Succeeded`; full `StealthGame.` run 391 pass, 2 fail
(this story's two PIE tests, first run; `story-022-.../cycle-tests-first-run.txt`). After the two test fixes below the
`StealthGame.Assassination.` set ran 71 / 71 (`assassination-rerun.txt`). Files stamped in `stamp.txt`.

## Automated

| AC | Test | Result and measurement |
|---|---|---|
| AC1 (AC-ASN-45) | `StealthGame.Assassination.PIE.CulledVictimOpensFalling.NpcPlayerAndInformantReopenAtTheDeathWriteAlreadyFalling` | NPC, player, informant: 0 open channel frames to the witness from the deny's settle to the death-write tick; the channel opens at death-write +1 frame (seen after the server's tick flush) for all three; each re-opened instance carries FDeathState from its first sight (0.157 s before DeathServerTime: the write leads it by StrikeNetLeadSec); its first clip is Death_Fall at DeathServerTime + 9 ms (one frame), sought to 0.009 s = now - death, blend 0.100 s, montage weight 0.000 on its first frame reaching 1 after 7 frames; identical across the kinds. Hidden from creation until its first bone finalisation (WarmUpBegin and WarmUpShown in the creation frame). CONTROL: deny then hand release with no strike: the witness gets the NPC back alive, shown, with no clip. NEGATIVE CONTROL: release in the slot: 13-14 open frames before the death-write tick, caught. |
| AC2 (AC-ASN-25) | `StealthGame.Assassination.PIE.LateActorSeeksNeverSnaps.MidFallSeeksWithoutPopAndPastLingerNeverShows` | +0.6 s: released at +0.617 s (server), Death_Fall at +0.593 s on the bystander's clock (the engine clock stand-in lags ~24 ms, within the 0.05 s ADR-0002 stand-in tolerance), sought to exactly that, blend 0.100 s, warmed up in its creation frame; max per-frame root-relative bone delta over the first 10 shown frames 35.6 cm against the bound 37.8 cm (clip's own 21.6 cm + blend span 79.2 cm / 6 frames + 3 cm). +7.5 s: 62 frames sampled, never shown, hidden in its first frame (NeverShow), no clip. Host and the other client: Death_Fall at ContactServerTime +0.000 s / +0.010 s; DeathServerTime == ContactServerTime. |
| AC2 edges | `StealthGame.Assassination.LateActorBoundaries` | On time 0.15 s entry; +0.6 s sought, late, <= 0.1 s; +1.2 s held and collision off; +7.19 s shown; +7.2 / +7.5 / +8.0 s NeverShow. |
| Warm-up negative control | both PIE tests with the warm-up disabled (temporary local edit, reverted) | Both fail on the warm-up check (`warmup-negative-control.txt`). The pose bound alone does NOT fail without the warm-up under -nullrhi: the prototype AnimBP's collapsed first frame (S10 Q3b) is a render-client effect, so the mechanism is asserted directly. |

Fixes found by the first run (test-side): the release record was matched to the control's manual release of the same NPC;
the late-actor window compared the bystander's lagging clock with the server's release time without the clock tolerance;
a blocked NPC spawn now retries other spots and fails cleanly.

## Run and observe

Run result: N/A — nothing in ordinary play takes this path: the MVP has no non-concealment pawn cull (SCL R13, ADR-0003
section 3), so a re-open at the death write happens only through the test-only deny seam; its observation is the PIE
presentation log and per-frame samples above (warm-up, fall seek, blend weight, bones).

## Deviations

General fresh-pawn warm-up (every creation path) remains ADR-0013's significance subsystem; this story warms up the fresh
actors carrying a death or a playing clip. `WarmUpMaxFrames` (3) stands in for the IVP key `ReopenWarmupMaxFrames`.
