# Story 028: At the swap server time the body, shadow and reflections vanish on the same frame for opponent and ally clients

> **Epic**: Concealment (Bushes)
> **Status**: Blocked — BLOCKED: spike R5 (`story-021-spike-r5-hidden-primitive-leaks.md`) and spikes S9/S10 (IVP `story-004-…`, `story-005-…`) have not reported Update 2026-10-25 (S10 resolution): the S10 FAIL is resolved by ADR-0003 amendment "S10 resolution 2026-10-25" (re-open guard with bounded timeout and fresh channel, lost-pawn repair, warm-up, ADR-0002 clock budget); the S10 gate is now **Blocked: story-056 (S10 fix + re-run)** (`production/epics/information-visibility-policy/story-056-s10-fix-implementation-and-rerun.md`); other gates unchanged.
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

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

- `production/qa/evidence/con-enter-opp-1p.png`, `con-enter-opp-3p.png`, `con-enter-ally-1p.png`, `con-enter-ally-3p.png`, `con-enter-evidence.md`
- `Source/StealthTestHarness/Private/Tests/PIE/EnterSwapHidesBodyPerViewerTest.cpp` (frame-stepped capture harness)

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
