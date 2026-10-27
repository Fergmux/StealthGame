# Story cnb-025 — strike reaction look (AC-CNB-19): evidence and sign-off

- **Story**: `production/epics/crowd-npc-behaviour/story-025-strike-reaction-look.md` (Visual/Feel)
- **Date**: 2026-10-27
- **Build**: main after the crowd batch cnb-019..032 (309/309 `StealthGame.*` tests), L_Prototype graybox, 50 NPCs,
  3-player PIE (listen server + 2 clients), strikes through the prototype bot driver (`StealthBot <n> strike_npc`), which
  announces `OnActionStarted(Attack)` so the crowd's R8 fan-out runs.

## Captures (`production/qa/evidence/crowd-strike-reaction-look-2026-10-27/`)

| Take | Files | What |
|---|---|---|
| 1 | `clip1/frame01..16.png`, `strike2-*.png`, `strike3-*.png` | bystander at 20 m, frames every ~0.85 s; 1 / 5 / 15 s stills of two more strikes. Reviewed and **not approved** (framing: strike point out of view, crowd too far). |
| 2 | `take2-strike1/frame01..20.png` (bystander at 11 m facing the strike, ~0.3 s apart); `take2-strike2/striker01..16.png` and `bystander01..16.png` (~0.35 s apart) and `bystander-15s.png` | re-capture following the review |
| 2 | `take2-server-log-r8.txt` | server log with the dev-only `Crowd.LogStrikeReactions` cvar: each strike's responders and drawn reaction: strike 0 Pause @ 7.5 m, Glance @ 13.0 m, None @ 2.9 m; strike 1 None @ 11.0 m, Glance @ 13.8 m, Glance @ 1.6 m |

The headless spec `StealthGame.Crowd.Strike.SharesAndLimits` (4,000 strikes) shows every R8 share within 20 % of its
target in both distance strata, panic runs only within 6 m and never longer than `CrowdPanicRunMaxSec` (3 s), no
responder twice within 8 s, and no glance at the attacker afterwards.

## Thresholds (provisional QA tolerances of the story)

| Threshold | Result |
|---|---|
| No gathering (<= 3 NPCs stationary facing the strike point within 5 m for > 5 s) | Pass (take 2: at most 3 NPCs ever within 5 m, none packed into a ring; reaction pauses last 2-5 s by rule) |
| No flight from the district | Pass (the crowd is still in the market at +15 s after both take-2 strikes) |
| No NPC run longer than 3 s | Pass (no Run / Brace / Reroute drawn in take 2; no sprint pose at 0.3 s sampling; the spec bounds panic runs at 3 s) |
| At least 3 visible glance or pause reactions | 4 in the server log (1 Pause, 3 Glance); 1 corroborated visibly (the NPC beside the striker stays turned to the body ~4.5 s while the others walk on); the farther glances are not readable in stills at that range |

## Sign-off

| Role | Who | Date | Verdict |
|---|---|---|---|
| Art-director (Visual/Audio lens) | art-director agent, delegated review (`crowd-strike-reaction-look-signoff-2026-10-27.md`) | 2026-10-27 | [x] Approved (take 2), conditional on the lead's confirmation |
| Lead (creative-director or the human art-director) | — | — | [ ] Approved — pending: a human lead should confirm, or ask for a re-capture with 8-10 NPCs within 10 m of the strike so 3 reactions are visible in the stills |

Deviations: the "clip" is a 0.3 s still sequence (the capture tool takes window images, not video); the three seeds are
three strike positions in one PIE seed per take (L_Prototype re-seeds per PIE session); the crowd is sparse near the
strike points in this graybox.
