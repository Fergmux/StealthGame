# Story 035: The prototype concealment code is retired and the networked behaviours it proved still hold

> **Epic**: Concealment (Bushes)
> **Status**: Blocked — BLOCKED: depends on stories still open (028, 033, 034 now Ready; 012 Complete 2026-10-29); nothing is retired until the production system it replaces is observed working. Note 2026-10-29: the integration batch already retired the prototype's bush overlap concealment and replicated bConcealed (concealment-001-026 evidence); this story keeps the regression sweep and the remaining retirement checks.
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-26

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R9, Player Fantasy), `production/first-playable-report.md` (tests E and F, known issues 2 and 8)
**Requirement**: `TR-con-001`, `TR-con-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (Migration Plan; Alternative 1 rejected: replicated `bConcealed` with client-side hide), with ADR-0003 (allowlist audit of every replicated property)
**ADR Decision Summary**: The prototype proved per-viewer concealment is feasible (PIE test E and F PASS) but leaks: it replicates `bConcealed` to all clients, keys concealment on overlap, and hides with a client `SetVisibility` driven by that flag. Production replaces it with the non-replicated component, the server cull and the fail-closed latch; the prototype's behaviours remain as the regression reference.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Removal changes the replicated-property set of `AStealthCharacter`; the ADR-0003 allowlist audit and the SCL class-parity scan must pass after it. Per `production/never-resave-assets.md` no `.uasset` is re-saved by this story's pipeline run.

**Control Manifest Rules (this layer)**:
- Required: Prototype code is replaced, never promoted; the allowlist audit shows zero concealment rows on the pawn — source: ADR-0017, ADR-0003
- Forbidden: Never leave a replicated concealed flag on the pawn — source: ADR-0017
- Guardrail: The three prototype behaviours must still hold in the production build

---

## Acceptance Criteria

*From the epic's Prototype Disposition and `production/first-playable-report.md`, scoped to this story:*

- [ ] **AC1** (derived, epic Prototype Disposition): **GIVEN** the production system built **THEN** `Source/StealthGame/Private/StealthBush.cpp` and `Public/StealthBush.h` and `StealthCharacter`'s `SetInBush`, `UpdateConcealmentVisibility`, `RevealFromConcealment` and the replicated `bConcealed`/`bRevealedUntilExit` are removed, no file includes or references the removed symbols (source scan), and the replicated-property allowlist audit shows zero concealment rows on the pawn.
- [ ] **AC2** (prototype tests E and F as regression, [PIE-2]): **GIVEN** 2 clients on `L_Prototype` (or the production graybox map) **THEN** a teammate sees the hatched marker and an enemy sees an empty bush (E), a blind strike at the bush kills the hidden enemy (F), and striking from a bush reveals the attacker until it leaves (CON-R9); retained screenshots in `production/qa/evidence/`.

---

## Implementation Notes

*Derived from the epic's Prototype Disposition and ADR-0017 Migration Plan:*

Remove in this order: first the replicated flags and `UpdateConcealmentVisibility` (the leak), then `RevealFromConcealment`/`SetInBush`, then the `AStealthBush` placeholder spheres once the Layout Service places the production bush mesh. The first-playable report's networked tests E and F are the behavioural reference; rerun them against the production build as regression, using the production strike resolver and cull. The marker assertion needs the Teammate Markers epic. Compare the replicated-property allowlist before and after: the only intended change is the removal of the two concealment properties.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Every production story it follows (this story adds no new behaviour).
- Camera `story-025-retire-prototype-camera.md`: the camera prototype retirement.

---

## Files

- `Source/StealthGame/Private/StealthBush.cpp` and `Source/StealthGame/Public/StealthBush.h` (removed)
- `Source/StealthGame/Private/StealthCharacter.cpp` and `Public/StealthCharacter.h` (concealment members removed)
- `Source/StealthTestHarness/Private/Tests/PIE/PrototypeConcealmentRegressionTest.cpp` (`NETWORK_TEST_CLASS(PrototypeConcealmentRegression, "StealthGame.Concealment.PIE")`)
- `production/qa/evidence/con-regression-*.png`, `con-regression-evidence.md`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: PrototypeRemoved
  - Given: the production system is built.
  - When: the source is scanned and the replication allowlist audited.
  - Then: `StealthBush.cpp`/`.h`, `SetInBush`, `UpdateConcealmentVisibility`, `RevealFromConcealment` and `bConcealed`/`bRevealedUntilExit` are gone, no file includes them, and the allowlist has zero concealment rows on the pawn.
- **AC2**: PrototypeRegression
  - Given: 2 clients on `L_Prototype`.
  - When: scenarios E and F and the strike-reveal case run.
  - Then: E: a teammate sees the marker and an enemy sees an empty bush. F: a blind strike kills the hidden enemy. A strike from a bush reveals the attacker until it leaves (CON-R9). Screenshots are retained in `production/qa/evidence/`.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the PIE `PrototypeConcealmentRegressionTest.cpp` plus retained screenshots in `production/qa/evidence/` — must exist and pass, OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-012-owner-warp-no-snap.md`, `story-013-inside-the-bush-stillness-and-view.md`, `story-020-fresh-actor-latch-and-ally-hidden-read.md`, `story-022-presentation-hide-application-and-host-parity.md`, `story-025-cull-push-wiring.md`, `story-026-per-viewer-visibility-pie.md`, `story-027-uncull-before-leaf-plane.md`, `story-028-enter-swap-hides-body-per-viewer.md`, `story-029-rustle-parity.md`, `story-032-ownership-change-while-concealed.md`, `story-033-blind-strike-parity-and-kill-timing.md`, `story-034-r5-leak-matrix.md`
- Unlocks: None
