# Story 027: On a BushEnter swap the body and its shadow, reflection, Lumen and decals hide on the same rendered frame on ally and opponent clients

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Concealment] needs the concealment-bushes BushEnter swap and client latch built; also S9/S10 not reported
> **Layer**: Foundation
> **Type**: Visual/Feel
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The quantised server cull takes at least `ConcealCullDelayMs` to land, so the concealing pawn must disappear for everyone the instant it enters a bush by a purely client-local hide, triggered by the replicated `ActionPose`, not by the cull. If any residual (shadow, reflection, Lumen contribution, footstep decals) remains for even a frame, an opponent sees a ghost that gives the bush entry away and, worse, differs between a player and an NPC. This story verifies by frame-stepped screenshot evidence that on both an ally and an opponent client, including one at high RTT, the hide lands on the same rendered frame as the swap. It is the visual half of the concealment hybrid; the server half is stories 024 and 025.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.4, 5.6, 5.8); also ADR-0017: Concealment per-viewer presentation (section 5, the server-time swap); ADR-0008: Test and automation strategy for leak and parity tests (section 1: visual evidence)
**ADR Decision Summary**: A `CullPending` pawn is stationary with its body already hidden locally at the server-time swap (ADR-0017 section 5; no anim notify gates the hide), so the client-local hide, not the cull, covers the delay window; the cull then removes the pawn from non-ally connections at the grid boundary.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 section 5.4 and the 2026-10-24 amendment read "swap notify" as the server-time swap (no notify gates the hide). The hidden-primitive render flags (`bCastHiddenShadow`, `bAffectIndirectLightingWhileHidden`, `bRayTracingFarField` = false) are pinned by the ADR-0003 section 8 audit on pawn primitive class defaults (ADR-0022 section 2.1, ADR-0017 section 6); this story checks the rendered result, not the flag values. Rendering checks are Visual evidence, not T2 (ADR-0008 section 1). Open spikes: S9 (story-004) and S10 (story-005) have not reported.

**Control Manifest Rules (this layer)**:
- Forbidden: **Never hide concealment client-side only or replicate a concealed flag** (internal development switch only); never use dormancy or distance culling for concealment — source: ADR-0003
- Required: **Client code tolerates a relevancy destroy and initialises a re-created actor from replicated state only** — source: ADR-0003
- Required: **The concealment cull lands on the global boundary grid and closes the channel explicitly per denied connection in the cull tick** — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-12** [PIE-2][Net][Dep:Concealment] `StealthGame.Visibility.ConcealmentLocalHideAtSwap`: GIVEN an ally and an opponent client watching a `BushEnter`, one with RTT above `HudLatencyWarnRttSec` (the IIF duplicate `LatencyWarnRttSec` was retired by user decision 2026-10-25), WHEN the swap notify fires, THEN on both clients body, shadow, reflection, Lumen contribution and footstep decals hide on that rendered frame (±1 frame at 60 Hz), triggered by `ActionPose` not the cull; the ally then shows the hatched marker. Retained screenshot sequence in `production/qa/evidence/` with lead sign-off. (R7.3)

---

## Implementation Notes

*Derived from ADR-0003 sections 5.4, 5.6, 5.8, ADR-0017 sections 5 and 6, and the coding standards for Visual/Feel evidence:*

- **What to do.** Run PIE with one ally client and one opponent client watching a `BushEnter`, with one of the two clients delayed above `HudLatencyWarnRttSec` (use the engine packet-simulation profile `StealthLeak1` or `StealthLeak2` from ADR-0008 section 5, or a per-client emulation setting). Capture frame-stepped screenshots on each client at frames N-1, N and N+1 around the swap, showing body, shadow, reflection, Lumen contribution and footstep decals; capture the ally's hatched marker after the hide.
- **What must be true.** The hide comes from `ActionPose` (the replicated server-time swap), not from the cull: verify by inspecting that the pawn is still present (relevant) on the opponent client at the swap frame while hidden, and only removed later at the cull boundary.
- **Evidence.** A retained screenshot sequence in `production/qa/evidence/` and the sign-off file `production/qa/evidence/concealment-local-hide-at-swap-evidence.md` recording the setup, the build and commit, the RTT used, the frames inspected, and the lead's sign-off. Retained means the files are still on disk when the story closes.
- **Prototype disposition.** Replaced, never promoted: the prototype's `UpdateConcealmentVisibility` client-side toggle and replicated `bConcealed`/`bRevealedUntilExit` (`Source/StealthGame/Private/StealthCharacter.cpp:63-64`). The production hide is Concealment's presentation (ADR-0017).
- **Spike gates.** story-005 (S10): re-created actors and registries; story-004 (S9) through story-024.
- **Unblocks when.** concealment-bushes delivers the `BushEnter` swap and the client latch; stories 024 and 005 are done.
- **Timing.** The cull-side timing is story-025; this story does not assert server timing.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-024: server cull audience
- story-025: cull timing on the global grid
- story-029: un-cull lead bound
- concealment-bushes epic: the swap and client latch themselves, the exit transition asset
- teammate-markers epic: marker widget behaviour (story-033 verifies the concealed-ally marker)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `production/qa/evidence/concealment-local-hide-at-swap-evidence.md` (setup, frames inspected, sign-off)
- `production/qa/evidence/` frame-stepped screenshots (frames N-1, N, N+1 on the ally client and on the opponent client)

---

## QA Test Cases

- **AC-1**: Concealment local hide at swap (AC-IVP-12)
  - Setup: PIE with an ally client and an opponent client watching a `BushEnter`, one at RTT above `HudLatencyWarnRttSec`.
  - Verify: on the swap-notify frame body, shadow, reflection, Lumen contribution and footstep decals are hidden on both clients; the hide comes from `ActionPose`, not the cull; the ally then shows the hatched marker.
  - Pass condition: the hide lands within ±1 frame at 60 Hz on both clients; a frame-stepped screenshot sequence (frames N-1, N, N+1) is retained in `production/qa/evidence/`; the lead signs off.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: a retained screenshot sequence in `production/qa/evidence/` + sign-off in `production/qa/evidence/concealment-local-hide-at-swap-evidence.md`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`, `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`; concealment-bushes epic stories delivering the `BushEnter` swap and the client latch
- Unlocks: None
