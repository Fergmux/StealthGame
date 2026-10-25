# Story 025: A reversal reads as "the striker fell" and the brace reads as a stance (playtest protocol)

> **Epic**: Assassination & Defence Reversal
> **Status**: Ready
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/assassination-defence-reversal.md` (VA-ASN-3, Game Feel "Reversal (bystander)"; OQ-ASN-10; AC-ASN-30)
**Requirement**: `TR-asn-007`, `TR-asn-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0018: Encounter data and outcome presentation timing (§5.1 one fall and one outcome-blind strike), with ADR-0013 (one clip set for every pawn)
**ADR Decision Summary**: A bystander tells a kill from a reversal only by who falls and who stays standing, because the attacker's clip up to contact, the (absent) bystander grade, the sound and the timing are identical for every outcome; one `Death_Fall` serves every victim.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (animation presentation; the protocol is a human study, not an engine feature)
**Engine Notes**: Clips are captured from the running game (cooked Development standalone or PIE with fixed framing) in greyscale at 20 m in a baseline crowd of 50 NPCs, 3 s around contact, filmed from in front of, beside and behind the striker. This story builds no new engine behaviour; it runs the study and records the result, and feeds OQ-ASN-10 (the rear-strike counter lands on air by user decision 2026-10-25, a closed question whose evidence is gathered here).

**Control Manifest Rules (this layer)**:
- Required: Identical presentation up to contact for every outcome and victim kind; loud by readability, not by effects — source: GDD Game Feel, ADR-0018
- Forbidden: No VFX, particles, decals, blood, light, emissive or material change; no hit-stop, slow motion or shake; no kill cam — source: GDD VA-ASN-6, CS-R11.1
- Guardrail: Brace tell test passes at the default 4.0 s hold and fails at 2× compression (art bible 5.5)

The pass condition is a human one: trained testers shown a fixed shuffled set of 24 clips (12 kills and 12 reversals across three bearings) name who fell, and say why ("the striker was countered" / "the target was struck"). The "why" answers per bearing (front, side, behind) are recorded as input to OQ-ASN-10 and are **not** gated. The brace tell test (VA-ASN-3(b), art bible 5.5) checks that the held stance is distinguishable from ordinary standing and crouching at default compression and not at 2×.

---

## Acceptance Criteria

*From GDD `design/gdd/assassination-defence-reversal.md` (AC-ASN-30, VA-ASN-3(b)), scoped to this story:*

- [ ] **AC1** (AC-ASN-30, [Visual/BLOCKING] `ReversalReadsAsStrikerFell`, manual protocol): **GIVEN** 3 s greyscale clips at 20 m in a baseline crowd, 12 kills and 12 reversals from three bearings **WHEN** ≥ 6 trained testers, each shown all 24 clips in one fixed shuffled order, name who fell and say why **THEN** ≥ 85 % of "who fell" answers are correct within 2 s, for kills and for reversals separately; the "why" answers are recorded per bearing (front, side, behind) as input to OQ-ASN-10, not gated; evidence: results sheet, `asn-30-*.png` stills and lead sign-off.
- [ ] **AC2** (VA-ASN-3(b), [Visual/BLOCKING] brace tell): **GIVEN** the brace clips (`Brace_In` 0.25 s, `Brace_Hold`, `Brace_Out` 0.40 s) at default compression and at 2× compression **WHEN** the same testers identify bracing pawns among crowd stances **THEN** identification passes at the default and fails at 2×, recorded on the results sheet.

---

## Implementation Notes

*Derived from GDD VA-ASN-3, Game Feel and art bible 5.5:*

Set-up: run after stories 018, 020 and 010 produce the full kill and reversal sequences (stand-in clips are acceptable for the first pass; a pass on stand-ins is provisional and must be repeated with final clips before the vertical slice). Fixed shuffle order and tester briefing script are stored with the results; testers are "trained" in the sense of knowing the vocabulary, not the answer. A tester who answered from sound is excluded by capturing silent clips for the reversal-readability test (audio contract VA-ASN-5 is verified separately).

Record: per-tester answers, response time, per-bearing "why" counts, percentage per kind; attach stills at contact. If AC1 fails for reversals, the escalation path is the creative director (OQ-ASN-10 declined for MVP on 2026-10-25: the counter plays toward body facing; reopening it needs a user decision).

This is a manual protocol: `/story-done` requires the retained evidence, not an automated test.

**Prototype:** none; first-playable's networked prototype tests (client reversal screenshot `production/qa/evidence/2026-10-24_client1-reversal-elimination-win.png`) show the outcome but had no clips.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-018`, `story-020`, `story-010`: producing the sequences under test.
- The counter-turns-toward-attacker change (OQ-ASN-10, declined for MVP).
- Audio mix review (audio-director; VA-ASN-5).

---

## Files

- `production/qa/evidence/asn-30-results.md` (protocol, tester count, results table, per-bearing "why" counts, sign-off)
- `production/qa/evidence/asn-30-*.png` (stills at contact for each bearing and outcome)
- `production/qa/evidence/asn-25-brace-tell-results.md` (VA-ASN-3(b) results and sign-off)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

QL-STORY-READY: GAPS
- AC1: "within 2 s" has no start point and the 85 % computation is unspecified; proposed rewrite (unconfirmed): ≥ 85 % of answers are correct and given within 2 s of the clip's contact frame, computed per kind as correct answers ÷ (testers × 12) ≥ 0.85 (with 6 testers, ≥ 62 of 72), for kills and reversals separately.
- AC2: neither "passes" nor "fails at 2×" has a threshold and "2× compression" is undefined; proposed rewrite (unconfirmed): identification passes if ≥ 85 % of bracing pawns are correctly identified with a false-positive rate on standing and crouching pawns ≤ 15 % at the default 4.0 s hold; at 2× (hold 2.0 s, `Brace_In` 0.125 s, `Brace_Out` 0.20 s) it must fail (correct identification < 85 %); the two conditions are shown in counterbalanced order.
- "Trained testers" and exclusions should be written into the protocol: no tester who built or knows the answer sheet; silent clips only.

- **AC-1**: Reversal reads as striker fell
  - Setup: 24 silent greyscale 3 s clips at 20 m in a 50-NPC crowd (12 kills and 12 reversals across front, side and behind), one fixed shuffled order, briefing script stored, ≥ 6 testers
  - Verify: record each tester's answer, response time and "why", plus per-kind percentages and per-bearing "why" counts
  - Pass condition: ≥ 85 % correct within 2 s, for kills and for reversals separately; "why" counts recorded but not gated; `asn-30-results.md`, the `asn-30-*.png` stills and the art-director sign-off present; a pass on stand-in clips is marked provisional
- **AC-2**: Brace tell
  - Setup: the brace clips at default and at 2× compression among crowd stances, with the same testers
  - Verify: tally brace identifications and false positives at each compression
  - Pass condition: the default condition passes and the 2× condition fails against the thresholds above; results recorded in `asn-25-brace-tell-results.md` with sign-off

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained results sheets and stills in `production/qa/evidence/` (`asn-30-results.md`, `asn-30-*.png`, `asn-25-brace-tell-results.md`) with a lead sign-off (art-director) — AC-ASN-30 is `[Visual/BLOCKING]`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-010-reversal-counter-and-chained-strikes.md`, `story-011-npc-brace-parity-never-strike-planted.md`, `story-018-strike-clip-variants-warp-and-contact-scheduling.md`, `story-020-client-fall-linger-and-occlusion-removal.md`
- Unlocks: Vertical Slice gate evidence for reversal readability (P2); input to OQ-ASN-10 post-MVP review
