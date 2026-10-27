# Smoke check: Crowd Tell Matrix row citations (AC-CNB-30)

- **Date**: 2026-10-27
- **Story**: `production/epics/crowd-npc-behaviour/story-037-tell-matrix-row-citations.md` (TR-crowd-010)
- **Gate**: Advisory (Review/ADVISORY at the cross-GDD review)
- **Source of rows**: `design/gdd/crowd-npc-behaviour.md`, CNB-R16 Baseline and Tell Matrix (lines 114-135), Owner column.
- **Result**: 18 rows read (T1-T18). 17 required (GDD, row) pairs across the 8 named GDDs; 5 found cited by number inside an acceptance criterion; 12 MISSING. T17 and T18 recorded as no-deviation rows.

Counting rule: an AC counts only if its own text literally contains the row number (for example "T4"). Prose elsewhere in the GDD (rules, change-request logs, dependency tables) does not count.

## Citation table

| GDD | Tell Matrix row(s) owned (R16 Owner column) | AC id that cites the row by number | Where (file:line / section) |
|---|---|---|---|
| shared-character-locomotion.md (SCL) | T1 Run, T2 Traversal, T10 AI handoff, T11 Sudden stop, T12 Abrupt reversal, T13 Head hold, T16 Slow walk | MISSING for all seven. AC-SCL-32 (`TellMatrixRows`) exists but matches rows by name ("Run, Traversal, sudden stop, abrupt reversal and head hold") and Owner cell, not by T-number | AC-SCL-32 at line 738 (Acceptance Criteria). T13 appears only in R8 prose (line 106) and a dependency row (line 636); T-numbers in no AC |
| input-interaction-framework.md (IIF) | T3 Objective interaction, T4 Brace, T16 Slow walk | MISSING for all three. AC 50 (`TellMatrixRows`) lists "brace, objective interaction, bush enter/leave and assassination" by name only, and does not name T16 at all | AC 50 at line 805 (Acceptance Criteria). T3-T6 mentioned only in Open Question 5 (line 848) |
| level-socket-contract.md (LSC) | T8 Alley emergence / exit | MISSING. AC-61 does not exist in this file (ACs run AC-50..AC-55 in that block). The Tell Matrix AC is AC-54, which names the row by description ("emerging from or leaving by an alley"), not "T8" | AC-54 at line 835 (Acceptance Criteria) |
| concealment-bushes.md | T5 Bush enter / leave | MISSING. AC-CON-15 (`BlindStrikeParity`) is a packet-trace parity test and does not contain "T5". T5 is cited only outside an AC | AC-CON-15 at line 450. Non-AC citations: CON-R14 (line 99), change-request note line 384, CR table line 413, OQ3 (line 478) |
| assassination-defence-reversal.md | T4 Brace, T6 Assassination strike | AC-ASN-26 (T4); AC-ASN-27 (T6). Both found | AC-ASN-26 line 581 ("Crowd T4"); AC-ASN-27 line 582 ("Crowd T6"); both Acceptance Criteria |
| objectives-chain.md | T3 Objective interaction | AC-OC-24 (T3). Found | Line 463, Acceptance Criteria ("AC-OC-24 (Crowd T3)", id `TellMatrixRowT3`) |
| information-visibility-policy.md (IVP) | T5 Bush enter / leave | AC-IVP-34 (T5). Found, via "(Crowd T5 cites k = 1.5 ...)" inside the AC text | Line 588, Acceptance Criteria |
| npc-kill-penalty-informants.md | T7 Informant restrictions | AC-NKP-17 (T7). Found | Line 543, Acceptance Criteria ("Tell Matrix T7"); PEN-R13 (line 131) owns the rule |

Owners in R16 not named in AC-CNB-30 (listed for completeness, not counted): T9 and T10 Session + Round (Round GDD), T15 CPA (T10 also SCL, counted above); T3/T4/T5/T8/T12/T13/T16 also list CNB itself.

## Missing entries (change requests; nothing was edited here)

1. **To the Shared Character & Locomotion author**: AC-SCL-32 (line 738) must cite the rows it covers by number. Quote of the gap: "rows exist for Run, Traversal, sudden stop, abrupt reversal and head hold (5 of 5)". Please add T1, T2, T11, T12, T13 to it, and add T10 (AI handoff) and T16 (slow walk), which R16 also assigns to SCL but the AC does not mention. 7 pairs missing: (SCL, T1), (SCL, T2), (SCL, T10), (SCL, T11), (SCL, T12), (SCL, T13), (SCL, T16).
2. **To the Input & Interaction Framework author**: AC 50 (line 805) must cite T3 and T4 by number. Quote: "rows exist for brace, objective interaction, bush enter/leave and assassination". Also add T16 (slow walk, IIF R15a), which R16 assigns to IIF and AC 50 omits. 3 pairs missing: (IIF, T3), (IIF, T4), (IIF, T16).
3. **To the Level Socket Contract author**: AC-CNB-30 refers to "LSC (AC-61)" but no AC-61 exists; the Tell Matrix AC is AC-54 (line 835). Either correct the reference in AC-CNB-30 (Crowd author) or renumber. AC-54 must also cite T8 by number. Quote: "rows 'emerging from or leaving by an alley' (deviation none; NPCs do it: yes; owners LSC + Crowd)". 1 pair missing: (LSC, T8). AC-54 also mentions a "takeover of a crowd pawn" row, which is T10 in R16 (owner list there is SCL + Round + CNB, not LSC): confirm the mapping.
4. **To the Concealment & Bushes author**: AC-CON-15 (line 450, `BlindStrikeParity`) does not cite T5. Quote of what exists elsewhere: "Tell Matrix T5 references AC-CON-15" (line 384, a change-request note, not an AC). Add "(Crowd T5)" to AC-CON-15 or a dedicated AC. 1 pair missing: (Concealment, T5).

Total missing pairs: 7 + 3 + 1 + 1 = 12.

Note to the Crowd author (not a missing citation): AC-CNB-30 names "LSC (AC-61)" and "IIF (AC 50)"; R16's T3 Owner cell names AC-OC-21 and AC-OC-24 while the Objectives GDD only carries AC-OC-24 as the T3 row AC (AC-OC-21 not checked, out of scope).

## No-deviation rows (recorded)

| Row | Subject | Deviation | Sections that argue it |
|---|---|---|---|
| T17 | Camera mode / toggle (1P/3P, shoulder swap) | None; client-local, changes nothing on the pawn; not an advantage, not visible | `camera-system.md`: CS-R2 "Two modes, one body" (line 68); dependency rows lines 302 and 536; Camera AC-CS-23 `TellMatrixRow` (lines 876-880, Doc review/ADVISORY: expects a "camera mode / toggle" row reading "no visible deviation"; it does not cite "T17" by number either). Crowd R16 line 134 |
| T18 | Ping (`IA_Ping`, team-only place ring) | None; no pawn animation, no replicated pawn state change | `informant-pings-team-comms.md`: PNG-R2 (line 51), PNG-R9 (line 76) and its body-correlate bullet (line 80), Tell Matrix ruling D28 bullet (line 81), OQ8 informant-only fallback (line 366), dependency row line 312. Crowd R16 line 135 |

Observation: neither Camera nor Pings has an AC that cites "T17"/"T18" by number; Camera AC-CS-23 argues the row without the number. Pings has no Tell Matrix AC (AC-PNG-01/-02 test invisibility). Both are no-deviation rows so AC-CNB-30 does not require them; reported only as an advisory note.

## Method

Read the story and R16 (lines 114-135 of the Crowd GDD) only. For each named GDD, grepped for `\bT(1[0-8]?|[1-9])\b` on lines, then for the AC ids named in AC-CNB-30 (AC-SCL-32, IIF 50, AC-61, AC-CON-15, AC-OC-24, AC-IVP-34) and for AC-ASN / AC-NKP lines citing T4/T6/T7, reading only matched lines and small ranges. No GDD was read whole or edited. A hit counts only when the AC line itself contains the T-number. Line numbers are from the working tree on 2026-10-27.
