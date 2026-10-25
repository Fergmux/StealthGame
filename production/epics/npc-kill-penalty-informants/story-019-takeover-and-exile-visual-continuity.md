# Story 019: Takeover return, exile body and exiled owner's view stay visually continuous

> **Epic**: NPC-Kill Penalty & Informants
> **Status**: Ready
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/npc-kill-penalty-informants.md`
**Requirement**: `TR-pen-002`, `TR-pen-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0019: Informant lifecycle and crowd-pawn takeover (§2.3 step 4, §4 note on the executing hand-over), with ADR-0010 (hand-over blend, possession parity) and ADR-0001
**ADR Decision Summary**: The takeover changes nothing on the taken pawn on the wire or on screen: no spawn effect, fade, flash or material change; velocity, `AppearanceId` and head-look are preserved; with no input the body slows at or below the B4 baseline rate. The exile releases the surviving body to the crowd's restricted driver for its strike lockout and the owner's role switches from autonomous to simulated proxy mid-clip without a snap. The informant's own client cuts to the new body with the informant grade on from the cut frame and fades in over `NoPawnFadeSec`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Risk is the highest of the governing ADRs (ADR-0001 HIGH, ADR-0010 HIGH for possession parity). Spike S14 (`production/qa/spikes/2026-10-25_S1-S2-S14.md`) passed every possession path the prototype has (rejoin and PawnRecovery not testable); evidence screenshots from the prototype exist in `production/qa/evidence/2026-10-25_S14_*`. The mid-clip role switch and the first owner `ServerMove` correction below `PawnSmoothSnapThresholdCm` are ADR-0010 H1 and are exactly what AC3 observes; the sample's motion matching is also sensitive to role-dependent setup (spike note 6: re-run sample setup that depends on `GetLocalRole` after a takeover).

**Control Manifest Rules (this layer)**:
- Required: the hand-over executes at the next frame start (AI to player with the held input until the owner's first `ServerMove`) with no snap, re-blend or head-look jump — source: ADR-0010, ADR-0019
- Forbidden: never keep the participant's old look; no spawn effect, fade or material change on a taken body — source: ADR-0019, GDD VA-NKP-3
- Guardrail: AppearanceId unchanged across release and takeover — source: ADR-0012

Three visual continuity checks: the informant's own return view (AC-NKP-12), what an opponent sees of the exile (AC-NKP-36) and what the exiled player's own client sees at the release (AC-NKP-43). They are retained screenshot/clip evidence with art-lead sign-off, per the Visual/Feel evidence rule.

**Prototype replaced:** the prototype possession change on exile and respawn (`ExilePlayer`, `RespawnInformant` call `Possess` directly); the prototype S14 evidence shows it works for the pointers but with a cut and no blend. Nothing is promoted; the production pawn's hand-over blend (shared-character-locomotion) is the thing under test.

---

## Acceptance Criteria

*From GDD `design/gdd/npc-kill-penalty-informants.md` (AC-NKP-12, AC-NKP-36, AC-NKP-43), scoped to this story:*

- [ ] **AC1** (AC-NKP-12 as clarified at QL-STORY-READY, [Visual/BLOCKING][PIE-3] takeover return, PEN-R9, PEN-R16): **GIVEN** an informant takeover **WHEN** the informant's own client is recorded **THEN** it cuts to the new body on one frame with the informant grade on from that cut frame (never eased in), fades in over `NoPawnFadeSec` starting on the cut frame, walks only, and allies see an outline marker on the new body (retained screenshots `nkp-12-*.png` at the cut frame, +1 frame and the fade end, with art-lead sign-off).
- [ ] **AC2** (AC-NKP-36 as clarified at QL-STORY-READY, [Visual/BLOCKING][Provisional: spikes S1/S14] exile body continuity, VA-NKP-2): **GIVEN** an exile seen from an opponent client **WHEN** the strike clip and the AI hand-over play and a per-frame log covers +/- 0.5 s around the hand-over (root position and rotation, head-look yaw and pitch, LOD index, animation tick rate) **THEN** the delta at the hand-over frame stays within the delta of a control pawn or a no-hand-over run (proposed bound: <= 0.5 cm and <= 0.5 degrees root, <= 1 degree head-look; the art lead confirms the bounds), LOD index, tick rate and `AppearanceId` are equal across the hand-over, and a -2..+2 frame strip is retained (`nkp-36-*.png`, art-lead sign-off).
- [ ] **AC3** (AC-NKP-43 as clarified at QL-STORY-READY, [Visual/BLOCKING][PIE-3][Provisional: spikes S1/S14] exiled owner's own view at the release, PEN-R4.2, PEN-R16): **GIVEN** an exile **WHEN** the exiled player's own client is recorded from the contact frame to the card entrance with the same per-frame log as AC2 **THEN** the released body shows no snap, re-blend, warp jump or head-look jump as its role switches from autonomous to simulated proxy mid-clip (same proposed bounds as AC2, art-lead confirmed; retained -2..+2 frame strip around the role switch `nkp-43-*.png`, art-lead sign-off).

---

## Implementation Notes

*Derived from GDD Visual/Audio VA-NKP-2 to VA-NKP-4 and ADR-0019 §4.1:*

Evidence procedure per `.claude/docs/run-and-observe.md` for Unreal: a 2-client or 3-client PIE session with a fixed seed, driven by the dev bot driver (`StealthBot <player#> <strike_npc|strike_enemy|strike_informant|...>`). For AC1 record the informant's client across a takeover: capture the cut frame (informant grade already on, never eased), the fade-in over `NoPawnFadeSec`, control on the cut frame with the head not moving (CS-R12.2), and an ally's client showing the outline marker on the new body (C-TeamView `PawnRef`, TM-R7). The grade is applied through Camera's `SetOwnerGrade(Informant)` (CS-R15, camera-system `story-019-grade-hooks.md`).

For AC2 capture an opponent client watching an exile through the strike clip (`AttackAttemptLockoutSec`, 1.75 s) and the AI hand-over: compare frame sequences for pose snap, blend restart, head-look jump, LOD or animation-rate change; the released body's `AppearanceId` equals its value before the exile. For AC3 capture the exiled owner's own client from the contact frame to the card entrance. The role switch (autonomous proxy to simulated proxy mid-clip) is ADR-0010 H1's concern; any snap is recorded as a defect against shared-character-locomotion, with the failing frames retained.

Retain as `production/qa/evidence/nkp-12-*.png`, `nkp-36-*.png`, `nkp-43-*.png` plus a sign-off file `production/qa/evidence/penalty-visual-evidence.md` recording the observer, build, seed, and the art-lead sign-off (Visual/Feel requires a lead sign-off). A parse or build check is not a run: the evidence is the observation (project coding standard).

The `[Provisional: spikes S1/S14]` tag on AC2 and AC3 is read as "re-baseline on production code", not Blocked: both spikes reported 2026-10-25. Re-baseline when the production pawn replaces the prototype.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- shared-character-locomotion `story-027-handover-blend.md`, `story-011-release-baseline-stop.md`: the blend and braking implementations (this story verifies them in Penalty's flows).
- `story-017-owner-presentation-timing.md`: timestamps of the owner's presentation.
- `story-014-one-respawn-path-and-takeover-wire-parity.md`: wire parity of the takeover.
- OQ-NKP-5: the AI-to-player handover braking AC lives in SCL (shared-character-locomotion).

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/nkp-12-*.png`, `nkp-36-*.png`, `nkp-43-*.png`
- `production/qa/evidence/penalty-visual-evidence.md` (sign-off)
- No production source is planned: defects found are routed to the owning epic (shared-character-locomotion, camera-system, crowd-npc-behaviour)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC1** (AC-NKP-12) Manual check: takeover return
  - Setup: PIE-3 with an informant awaiting a body and a takeover of an unseen pawn. Record the informant's client with a per-frame log (view target, grade weight, fade alpha, gait) and an ally's client.
  - Verify: capture the cut frame F, F + 1 and the fade end (`nkp-12-*.png`).
  - Pass condition: the view target switches on frame F alone; the grade weight is 1.0 at F with no ramp; the fade alpha starts at F and ends at `NoPawnFadeSec` (+/- 1 frame); speed stays at walk; the ally sees an outline marker on the new body; art-lead sign-off recorded.
- **AC2** (AC-NKP-36, Provisional S1/S14) Manual check: exile body continuity
  - Setup: PIE-3 with an opponent client watching an exile; a control NPC or a no-hand-over run; a per-frame log for +/- 0.5 s (root position and rotation, head-look yaw and pitch, LOD index, animation tick rate).
  - Verify: compare the hand-over frame's delta against the control's delta; retain the -2..+2 frame strip (`nkp-36-*.png`).
  - Pass condition: the difference from the control is within the proposed bounds (<= 0.5 cm and <= 0.5 degrees root, <= 1 degree head-look); LOD index, tick rate and `AppearanceId` are equal across the hand-over; art-lead sign-off recorded and the art lead confirms the bounds.
- **AC3** (AC-NKP-43, Provisional S1/S14) Manual check: exiled owner's own view
  - Setup: PIE-3 recording the exiled player's own client from the contact frame to the card entrance, with the AC2 per-frame log plus the role-switch frame (autonomous to simulated proxy).
  - Verify: the -2..+2 strip around the role switch (`nkp-43-*.png`).
  - Pass condition: the AC2 bounds hold across the role switch with no snap, re-blend, warp jump or head-look jump; art-lead sign-off recorded.

QL-STORY-READY: ADEQUATE

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: a retained screenshot in `production/qa/evidence/` (`nkp-12-*`, `nkp-36-*`, `nkp-43-*`) + sign-off in `production/qa/evidence/penalty-visual-evidence.md` (art-lead sign-off)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/npc-kill-penalty-informants/story-006-exile-releases-body-to-crowd.md`, `story-014-one-respawn-path-and-takeover-wire-parity.md`, `story-017-owner-presentation-timing.md`; cross-epic: shared-character-locomotion `story-027-handover-blend.md`, `story-011-release-baseline-stop.md`; camera-system `story-019-grade-hooks.md`, `story-017-view-state-neutral-view-fade.md`; teammate-markers (outline marker on the new body)
- Unlocks: None
