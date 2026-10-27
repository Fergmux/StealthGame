# AC-ASN-30 `ReversalReadsAsStrikerFell` — protocol and results sheet

> **Story:** `production/epics/assassination-defence-reversal/story-025-reversal-readability-and-brace-tell-playtest.md` (AC1)
> **Status:** PROTOCOL READY — NOT RUN (needs ≥ 6 human testers and a human-captured clip set; prepared 2026-10-27 by the
> unattended build, which ran no session and recorded no answer)
> **Clips under test:** stand-ins (D54 placeholder montages `AM_ASN_Strike_D54Placeholder`, `AM_ASN_Counter_D54Placeholder`,
> `AM_ASN_DeathFall_D54Placeholder`). A pass on these is **provisional** and must be repeated with the authored clips before
> the vertical slice (story Implementation Notes).

## 1. What is measured

Trained testers (they know the vocabulary "striker", "target", "countered", "struck"; they do not know the answer sheet and
did not build the feature) watch 24 silent greyscale clips, 3 s each around the contact frame, filmed from 20 m in a
baseline crowd of 50 NPCs, and for each clip say **who fell** (the striker or the target) and **why** ("the striker was
countered" / "the target was struck").

- **Pass (QA rewrite, AC1):** for kills and for reversals separately, correct "who fell" answers given within 2 s of the
  clip's contact frame ÷ (testers × 12) ≥ 0.85 (with 6 testers: ≥ 62 of 72 per kind).
- **Not gated:** the "why" answers, recorded per bearing (front, side, behind) as input to OQ-ASN-10 (closed for MVP on
  2026-10-25: the counter plays toward body facing; reopening it needs a user decision).
- **Escalation:** if reversals fail, the creative director (story Implementation Notes).

## 2. Exclusions

- No tester who built the encounter, wrote this sheet or has seen the answer key.
- Silent clips only (a tester cannot answer from sound; the audio contract VA-ASN-5 is verified separately).
- A tester who reports recognising a clip from an earlier session is noted; their answers are kept and flagged.

## 3. Clip set (24) and how to capture it

IDs: `<outcome>-<bearing>-<n>`; outcome K = kill, R = reversal; bearing = camera position relative to the **striker**
(F in front of it, S beside it, B behind it), 20 m away, eye height; n = 1-4 (four victim placements per cell; vary the
victim kind across them: NPC, NPC, player, informant for kills; player, player, NPC brace, informant brace for reversals).

Capture (a human operator; the build has no movie capture of its own):

1. `tools/dev/cycle.ps1` (launch the editor), open `L_Prototype`, PIE listen server + 2 clients, `NpcCount` 50
   (`UStealthSettings`), round Active.
2. Greyscale: desaturate the recording afterwards in the video editor (saturation 0), or add an unbound post-process
   volume with Global Saturation 0 for the session only (never save it into `L_Prototype`); silent: record without audio.
3. Drivers (every command goes through the production command path; quote the action):
   - kill an NPC: `StealthBot <i> "strike_npc 150 0"`
   - kill a rival: `StealthBot <i> "strike_enemy 150 0"` (the rival unbraced)
   - kill an informant: `StealthBot <i> "strike_informant 150 0"`
   - reversal: `StealthBot <j> "defend"` on the defender, then within 4 s `StealthBot <i> "strike_enemy 150 0"`
     (an NPC brace for the NPC reversal cells: wait for a crowd brace, or brace a possessed informant with `"defend"`)
   - `StealthBot 0 status` lists the indices; `log LogStealthGameplay Verbose` prints every presentation row (the contact
     frame's server time is the ClipStart row's `due` of the striker's strike clip).
4. Frame the camera 20 m from the striker (F/S/B) with the editor's PIE eject camera; record the PIE viewport with the
   OS recorder at 30 fps or higher; cut 3 s around the contact frame (1.5 s before, 1.5 s after).
5. Save the clips as `production/qa/evidence/asn-30-clips/<ID>.mp4` (not committed if large; record their checksum here)
   and one still at the contact frame per bearing and outcome as `production/qa/evidence/asn-30-<K|R>-<F|S|B>.png`.

## 4. Fixed shuffled order (shown to every tester in this order)

Generated once with Python `random.Random(25030).shuffle` over the 24 IDs in canonical order:

| # | Clip | # | Clip | # | Clip | # | Clip |
|---|---|---|---|---|---|---|---|
| 1 | K-B-2 | 7 | R-S-4 | 13 | R-F-2 | 19 | R-S-1 |
| 2 | R-B-3 | 8 | R-S-2 | 14 | R-F-1 | 20 | R-B-1 |
| 3 | K-B-3 | 9 | K-F-2 | 15 | K-B-4 | 21 | K-F-1 |
| 4 | K-B-1 | 10 | R-B-4 | 16 | R-B-2 | 22 | K-F-3 |
| 5 | R-F-3 | 11 | R-S-3 | 17 | K-S-3 | 23 | K-S-4 |
| 6 | R-F-4 | 12 | K-S-1 | 18 | K-F-4 | 24 | K-S-2 |

## 5. Briefing script (read verbatim)

> "You will see 24 short silent clips of a crowd. In each, one person attacks another. Some attacks succeed and the
> target falls; some are countered and the attacker falls. After each clip, say as fast as you can who fell, the
> striker or the target, and then why: 'the striker was countered' or 'the target was struck'. There is no trick and no
> clip repeats. Ready?"

The operator starts a stopwatch at each clip's contact frame (marked on the operator's copy only) and records the time of
the "who fell" answer.

## 6. Results (fill in)

| Tester | Clip # | Clip | Who fell (answer) | Correct | Time from contact (s) | Why (answer) |
|---|---|---|---|---|---|---|
| T1 | 1 | K-B-2 | | | | |
| … | | | | | | |

Per kind (correct within 2 s ÷ testers × 12):

| Kind | Correct within 2 s | Total | Share | ≥ 0.85 |
|---|---|---|---|---|
| Kills | | | | |
| Reversals | | | | |

"Why" answers per bearing (not gated; OQ-ASN-10 input):

| Bearing | Kills: "struck" | Kills: "countered" | Reversals: "countered" | Reversals: "struck" |
|---|---|---|---|---|
| Front | | | | |
| Side | | | | |
| Behind | | | | |

## 7. Sign-off

| Role | Name | Date | Verdict |
|---|---|---|---|
| Art director (lead sign-off) | | | |
| Session operator | | | |

Provisional (stand-in clips): yes until the authored clips are captured and the session is repeated.
