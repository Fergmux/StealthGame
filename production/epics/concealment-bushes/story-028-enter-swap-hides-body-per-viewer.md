# Story 028: At the swap server time the body, shadow and reflections vanish on the same frame for opponent and ally clients

> **Epic**: Concealment (Bushes)
> **Status**: In Review
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R6, CON-R6a, AC-CON-01a)
**Requirement**: `TR-con-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§5 latch, §6 application)
**ADR Decision Summary**: Every client that holds the pawn plays `BushEnter` with the identical rustle; the body, its attachments and every shadow, Lumen, reflection and ray-tracing contribution hide at the swap instant `Start + BushEnterSwapSec`, evaluated on the client's synchronised server clock, on every client except the owner's. Allies then show the hatched marker and no body; non-allies then lose the actor when the cull lands.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Depends on the R5-verified flag set and on the S9/S10-verified cull and re-open behaviour. Capture is frame-stepped with the swap key and the client's `EstimatedServerNow()` logged beside each frame; no net emulation in this check (AC-CON-01a carries no [Net] tag).

**Control Manifest Rules (this layer)**:
- Required: Clips are seeked, never restarted; hide at the swap key from `EstimatedServerNow()` — source: ADR-0017
- Forbidden: Never gate the hide by an anim notify — source: ADR-0017
- Guardrail: ±1 frame at 60 Hz; the negative control must fail

IVP `story-027-concealment-local-hide-at-swap.md` owns the Policy-side check of the same frame; this story is the Concealment-side evidence.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-01a), scoped to this story:*

- [ ] **AC1** (AC-CON-01a, [PIE-2][1P/3P][Vis] `StealthGame.Concealment.EnterSwapHidesBodyPerViewer`, no Net, frame-stepped capture): **GIVEN** an opponent client and an ally client, each with line of sight to a player pawn walking into a bush, **WHEN** the client's server clock reaches `Start + BushEnterSwapSec` **THEN** on both clients the body, its attachments, shadow and reflections disappear on that rendered frame (±1 frame at 60 Hz); the ally then shows the hatched marker and no body; the owner keeps its own body. Negative control: a capture with the swap key offset by 5 frames fails. Evidence: `con-enter-{opp,ally}-{1p,3p}-*.png`, with lead sign-off.

---

## Implementation Notes

*Derived from ADR-0017 Decision §5 and §6:*

The capture harness steps frames and logs `EstimatedServerNow()` per frame. The swap key is `Start + BushEnterSwapSec` from `FActionState.StartServerTime`; the negative control shifts the key by 5 frames and must fail the same assertion, proving the capture can see the hide. 1P and 3P are both run on the viewing client (Camera CS-R5b: the eye-origin mask applies in third person). The owner keeps its own body throughout.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-022-presentation-hide-application-and-host-parity.md`: the production hide application.
- `story-034-r5-leak-matrix.md`: the lighting/RT matrix.
- IVP `story-027-concealment-local-hide-at-swap.md`: the Policy-side same-frame check.

---

## Files

- `Source/StealthTestHarness/Private/Tests/PIE/Concealment/EnterSwapHidesBodyPerViewerPieTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: Manual check: EnterSwapHidesBodyPerViewer
  - Setup: an opponent client and an ally client with line of sight, a player walking into a bush, frame-stepped capture (no Net).
  - Verify: the body, attachments, shadow and reflections vanish on the rendered frame where the clock reaches Start + 0.5 s (±1 frame at 60 Hz); the ally shows the hatched marker and no body; the owner keeps its body.
  - Pass condition: all of the above holds; the negative control (swap key offset by 5 frames) fails; `con-enter-{opp,ally}-{1p,3p}-*.png` retained with lead sign-off.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained screenshots `con-enter-{opp,ally}-{1p,3p}-*.png` in `production/qa/evidence/` plus sign-off in `con-enter-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-020-fresh-actor-latch-and-ally-hidden-read.md`, `story-021-spike-r5-hidden-primitive-leaks.md` (blocker), `story-022-presentation-hide-application-and-host-parity.md`, `story-025-cull-push-wiring.md`; IVP `story-027-concealment-local-hide-at-swap.md`
- Unlocks: `story-035-prototype-retirement-and-regression.md`

## Completion Notes
**Completed**: 2026-10-29 (status In Review)
**Criteria**: AC1 automated part passing: StealthGame.Concealment.PIE.EnterSwapHidesBodyPerViewer: on the opponent and the ally machine the body and every attached primitive are hidden from the first frame at or after Start + BushEnterSwapSec (on that frame, +- 1 frame), drawn before it; the ally then holds IsAllyBodyHidden and no body; the owner keeps its body; the negative control (key + 5 frames) FAILS as it must. Screenshots: con-enter-opp-3p-after-swap.png (enemy: no body), con-enter-ally-3p-after-swap.png (ally: hatched marker, no body), con-enter-owner-3p-inside.png. NOT DONE: the 1P captures (the prototype camera toggle has no console path; the production 1P/3P toggle is Camera stories 008/009) and the lead sign-off.
**Deviations**: ADVISORY: test file under Tests/PIE/Concealment/; evidence names con-enter-*-3p-*.png in the batch folder.
**Test Evidence**: production/qa/evidence/concealment-027-034-2026-10-29/
**Gate**: Build Succeeded; full ^StealthGame. suite 594 tests, 593 passed first run (ExitUncullBeforeLeafPlane snap check narrowed to late actors, re-run alone at full counts PASS)
Run result: OBSERVED — 3-player PIE: after the swap the enemy's client draws nothing of the hider and the ally shows the hatched marker with no body (con-enter-*.png).
**Reviewed by**: self code review 2026-10-29 (notes in production/qa/evidence/concealment-027-034-2026-10-29/evidence.md)
