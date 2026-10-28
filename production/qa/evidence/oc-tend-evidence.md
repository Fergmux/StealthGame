# Tend-cue ring evidence (Objectives story oc-020 AC1 / oc-016 AC1) - 2026-10-28

Story: `production/epics/objectives-chain/story-020-pie2-hidden-site-oracle-and-world-signature.md`. Run: 3-player listen-server
PIE on L_Prototype (50 NPCs), driven through the Unreal MCP (`StartPIE` floating; `StealthBot <n> interact <court>` on the host).
Team 0 banked Stage 1 first (court 1 becomes a completed court). Team 1's client (window "Client 1") then tended five courts with
its pawn placed at each; the client was captured in a burst (one frame per ~0.23 s; a background PIE window refreshes at about
4 fps, so the early frame is the first frame that shows the bar, about 0.67 s after Begin, and the late frame about 1.66 s).

The ring stand-in is the HUD's tend bar (`AStealthHUD::DrawInteractBar`): progress from the server's FHoldStatus, colour Bone until
`RingIsMarigold`, Marigold only at an own live site after TendCueDelaySec (1.0 s). Bar fill measured on each image (row y = 348,
x 194-457; 260 px = D = 3 s):

| Court (team 1's view) | Court id | Early frame | Late frame |
|---|---|---|---|
| completed (team 0's banked P1) | 1 | `oc-tend-completed-early.png`: 57 px, Bone | `oc-tend-completed.png`: 144 px, Bone |
| hidden (the final, locked for team 1) | 5 | `oc-tend-hidden-early.png`: 58 px, Bone | `oc-tend-hidden.png`: 144 px, Bone |
| opponent (team 0's live P2) | 2 | `oc-tend-opponent-early.png`: 58 px, Bone | `oc-tend-opponent.png`: 144 px, Bone |
| locked (team 1's own P2) | 4 | `oc-tend-locked-early.png`: 58 px, Bone | `oc-tend-locked.png`: 144 px, Bone |
| live (team 1's own P1) | 3 | `oc-tend-live-early.png`: 58 px, Bone | `oc-tend-live.png`: 144 px, **Marigold** (first Marigold frame at 116 px = 1.34 s; 87 px = 1.0 s still Bone) |

Pass: all five early frames Bone; the late frame Marigold only in `oc-tend-live.png`. Every image was looked at; none is black.
The automated half (byte-exact owner replies, D within one frame, Bone / Marigold read from the client's own controller at 0.3 s
and 1.5 s) is `StealthGame.Objectives.PIE.HiddenSiteOracle`.

Sign-off: agent run-and-observe 2026-10-28; art / UX lead review of the ring colours PENDING (human).
