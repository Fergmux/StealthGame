# VA-ASN-3(b) brace tell — protocol and results sheet

> **Story:** `production/epics/assassination-defence-reversal/story-025-reversal-readability-and-brace-tell-playtest.md` (AC2)
> **Status:** PROTOCOL READY — NOT RUN (needs the same ≥ 6 human testers as `asn-30-results.md`; prepared 2026-10-27 by the
> unattended build, which ran no session)
> **Blocker for a final verdict:** the authored brace clips (`Brace_In` 0.25 s, `Brace_Hold`, `Brace_Out` 0.40 s) do not
> exist yet: the brace is presented by the prototype's crouch stance (`AStealthCharacter::SetDefenceStance`, retired by story
> 027) and the SCL Brace pose. A session on that stand-in is **provisional** and is repeated when the clips land.

## 1. What is measured

Testers watch short silent clips of a crowd and mark every pawn they believe is **bracing**, among pawns that are bracing,
standing, or crouching. Two conditions, shown in counterbalanced order (half the testers default first):

| Condition | Brace_In | Hold | Brace_Out |
|---|---|---|---|
| Default (art bible 5.5) | 0.25 s | 4.0 s (`DefenceDurationSec`) | 0.40 s |
| 2× compression | 0.125 s | 2.0 s | 0.20 s |

**Pass (QA rewrite, AC2):** at the default, ≥ 85 % of bracing pawns correctly identified **and** a false-positive rate on
standing and crouching pawns ≤ 15 %; at 2× the identification **must fail** (correct identification < 85 %). Both results
are recorded; the guardrail is "passes at the default and fails at 2×".

How to set the conditions: `DefenceDurationSec` is the tuning key of the hold (4.0 default; 2.0 for the 2× condition, set
in a test tuning preset, never in the shipped defaults); the in/out times are the clips' own lengths (the 2× clips are the
same clips at play rate 2, a montage rate change in the capture session only).

## 2. Clip set and fixed order

12 vignettes per condition, 4 of each stance (BR bracing, ST standing, CR crouching), each a 6 s clip at 20 m in the
50-NPC crowd with the subject pawn and three ordinary crowd pawns in frame. Fixed order (Python
`random.Random(2505).shuffle` over BR1-4, ST1-4, CR1-4), the same for both conditions:

`BR3, ST2, BR4, ST3, BR2, CR3, CR4, BR1, CR1, CR2, ST4, ST1`

Drivers: `StealthBot <i> "defend"` raises the brace of player `<i>`'s pawn (a possessed informant reads as a crowd pawn);
`StealthBot <i> "release"` lowers it; crouch with the sample's crouch input on the client. Capture as in
`asn-30-results.md` section 3 (greyscale, silent, 20 m).

## 3. Results (fill in)

| Tester | Order (D first / 2× first) | Condition | Bracing pawns marked correctly (of 4) | False positives (standing + crouching marked) |
|---|---|---|---|---|
| T1 | | Default | | |
| T1 | | 2× | | |
| … | | | | |

| Condition | Identification share | False-positive share | Passes (≥ 85 %, ≤ 15 %) |
|---|---|---|---|
| Default | | | |
| 2× | | | must be "no" |

## 4. Sign-off

| Role | Name | Date | Verdict |
|---|---|---|---|
| Art director (lead sign-off) | | | |
| Session operator | | | |
