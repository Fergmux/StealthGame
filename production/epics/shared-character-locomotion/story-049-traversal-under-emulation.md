# Story 049: Traversal converges on every client under 100 ms lag and 1 % loss

> **Epic**: Shared Character & Locomotion
> **Status**: Blocked — BLOCKED: the net-emulation / traversal spike (story 045; ADR-0010 "traversal under net emulation at 100 ms / 1 % loss") has not reported; AC-SCL-25 is tagged [SPIKE] and this story is the epic's Exit B proof (Exit B is gated on that spike so a failed spike cannot hold up Exit A)
> **Layer**: Core
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R7.6 During traversal, F10; AC-SCL-25; OQ2/OQ3)
**Requirement**: `TR-scl-004`, `TR-scl-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Shared Pawn: CMC locomotion, spawn factory and possession parity (section 7; Verification Required "traversal under net emulation")
**ADR Decision Summary**: Proxy end-position error must stay within `PawnSmoothSnapThresholdCm / 4` (64 cm) with no duplicated clip, using one proxy root-motion policy for every pawn (`RepRootMotion` or follow replicated movement); the late-arrival offset by `EstimatedServerNow` skips a clip that already finished.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Net-emulation profile `PktLag 100`, `PktLoss 1 %`. If root-motion end positions on simulated proxies slide more than 64 cm, the fallback is to follow the replicated path on proxies and ignore root motion there (spike decision, GDD R7.6). Traversal holds the pawn at animation rate 1 on every client.

**Control Manifest Rules (this layer)**:
- Required: one proxy root-motion policy for every pawn; `TraversalState` written in the scoped forced update.
- Forbidden: never use a multicast for traversal; never use the sample's replicated `TraversalResult`.
- Guardrail: `TraversalState` 7 bytes per change.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-25** [N] `TraversalUnderEmulation` [SPIKE]: GIVEN PktLag 100, PktLoss 1 %, WHEN 100 vaults and climbs are performed by players and 100 by AI, THEN every traversal ends within `PawnSmoothSnapThresholdCm / 4` (64 cm) of the server landing on all clients, and no observer sees a replayed or duplicated clip.

*QA clarification (create-stories 2026-10-25, not a GDD change):* 100 traversals per kind (vault, climb) per controller type (400 in total); "ends" is the landing frame after smoothing settles; a duplicate is more than one clip start per traversal on an observer.

---

## Implementation Notes

*Derived from ADR-0010 section 7 and GDD R7.6:*

- Script traversals through the harness with fixed seeds for approach jitter; log the server landing position and each observer's landing position after smoothing settles, and the number of clip starts per traversal on each observer.
- Fix failures in the production traversal of story 048, not in this test: a failure at 64 cm switches proxies to the follow-replicated-movement policy for every pawn (never for some).
- **Prototype replaced**: n/a.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 048: the traversal implementation. Story 055: the combined exit pass over this and other profiles.

---

## Files

- `Source/StealthTestHarness/Private/Locomotion/TraversalUnderEmulationTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification; BLOCKED on spike 045). Test `StealthGame.Locomotion.PIE.TraversalUnderEmulation.PlayersAndAi`.*

Test: traversal under net emulation (AC-SCL-25)
  Given: PktLag 100 / PktLoss 1 % and 100 traversals per kind (vault, climb) per controller type (400 in total)
  When: the traversals are performed by players and by AI
  Then: every traversal ends (landing frame after smoothing settles) within 64 cm of the server landing on all clients; no observer sees a replayed or duplicated clip (a duplicate is more than one clip start per traversal on an observer)
  Edge cases: fixed seeds for approach jitter

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/TraversalUnderEmulationTest.cpp` — must exist and pass (multi-client PIE under net emulation); retained capture of observer landings in `production/qa/evidence/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-048-traversal-start-parity.md`, `story-045-net-emulation-traversal-spike.md`
- Unlocks: `production/epics/shared-character-locomotion/story-055-net-emulation-pass-exit.md`
