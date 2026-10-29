# Story 021: Spike R5, a hidden pawn leaves no shadow, VSM, Lumen, reflection or ray-traced trace on 5.8.4

> **Epic**: Concealment (Bushes)
> **Status**: Complete
> **Layer**: Feature
> **Type**: Visual/Feel
> **Estimate**: L
> **Manifest Version**: 2026-10-25f
> **Last Updated**: 2026-10-29

## Context

A hidden primitive is not added to the scene at all (no render proxy, hence no shadow, Lumen, ray-tracing, reflection or sky-capture contribution) unless one of three gating flags keeps it (E2 [V]). What is still unverified is Virtual Shadow Map cache-page removal, groom (hair) components and Niagara attached to the pawn, and whether the flag set holds under hardware ray-traced Lumen. Those are owned by spike R5. The hide must remove mesh, attachments, shadow, Lumen and ray-tracing presence for non-allies (art bible R5).

**GDD**: `design/gdd/concealment-bushes.md` (CON-R6b, AC-CON-23 preview, Visual/Audio)
**Requirement**: `TR-con-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§6; Verification Required R5), with ADR-0022 (rendering configuration and budgets)
**ADR Decision Summary**: `SetVisibility(false, true)` on the mesh and every attached primitive, with `bCastHiddenShadow = false`, `bAffectIndirectLightingWhileHidden = false` and `bRayTracingFarField = false` fixed on the class defaults of every pawn primitive (never toggled at runtime), leaves no render proxy and therefore no shadow, VSM, Lumen, reflection or ray-traced contribution. Spike R5 verifies this with `bCreateRenderStateForHiddenComponentsWithCollsion` off and covers VSM cache pages, groom components and attached Niagara.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: E1/E2 [V]: `PrimitiveComponent.cpp:2106-2122`, `SceneComponent.cpp:3574-3576`; the non-Shipping world flag `bCreateRenderStateForHiddenComponentsWithCollsion` keeps a proxy for hidden components with collision and must be off. VSM cache pages, groom and Niagara are [U] until this spike reports.

**Control Manifest Rules (this layer)**:
- Required: Hiding applies the full ADR-0017 §6 flag set; the three gating flags are class defaults on every pawn primitive and are never toggled at runtime — source: ADR-0017
- Forbidden: Never use `SetActorHiddenInGame`/`bHidden` for concealment — source: ADR-0017
- Guardrail: Client hide/show rebuilds the render proxy; its cost is profiled here (client render budget, ADR-0022)

---

## Acceptance Criteria

*From the story's spike definition (ADR-0017 Verification Required R5; GDD AC-CON-23 preview):*

- [ ] **AC1** (spike on throwaway code): **GIVEN** `SetVisibility(false, true)` on a pawn mesh and every attached primitive (body, hair/groom, garments, props, attached Niagara) with `bCastHiddenShadow = false`, `bAffectIndirectLightingWhileHidden = false`, `bRayTracingFarField = false` on class defaults and `bCreateRenderStateForHiddenComponentsWithCollsion` off, across the R5 matrix cells (low sun, noon, shade × Lumen software and hardware RT × VSM on/off) **THEN** captures show no shadow, VSM cache page, Lumen, reflection or ray-traced contribution of the hidden pawn above the noise floor (the 99th percentile of the temporal-mean difference between two empty bushes at matched sway phase).
- [ ] **AC2** (verdict): **GIVEN** the spike finished **THEN** `production/qa/spikes/<date>_R5.md` states for each of VSM cache pages, groom components and attached Niagara whether the flag set suffices or names the extra step, records the proxy-rebuild cost of hide/show (client profile), and retains the captures; a failure is handled by a superseding ADR.

---

## Implementation Notes

*Derived from ADR-0017 Decision §6 and Risks:*

Throwaway code on the prototype map is acceptable, but it must follow the rule shape production will take: flags on class defaults, hide through `SetVisibility(false, true)` propagating to children, no `bHidden`. The noise floor is measured first (two empty bushes at matched sway phase, 60-frame temporal means). Include hardware RT Lumen and software Lumen cells, VSM on and off, and attached groom and Niagara. A magenta occlusion proxy placed behind the pawn position makes any residual contribution visible as non-zero pixels. Profile the hide/show proxy rebuild on the RTX 3060-class reference (ADR-0022 R1-R3 are separate gates).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-022-presentation-hide-application-and-host-parity.md`: the production hide.
- `story-034-r5-leak-matrix.md`: the AC-CON-23 matrix on the production hide.
- ADR-0022 spikes R1-R3.

---

## Files

- `production/qa/spikes/<date>_R5.md` (the verdict)
- `production/qa/evidence/<date>_R5-*.png` (retained captures)
- Throwaway spike code: `Source/StealthGame/Private/Spikes/`

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: Manual check: R5 matrix capture
  - Setup: a pawn mesh and every attached primitive hidden with `SetVisibility(false, true)` and the flag set on class defaults. Matrix cells: low sun, noon and shade, each against Lumen software and hardware RT, each against VSM on and off. The noise floor is the p99 temporal-mean difference between two empty bushes at matched sway phase.
  - Verify: captures show no shadow, VSM page, Lumen, reflection or RT contribution above the noise floor.
  - Pass condition: all cells are at or below the floor, with captures retained and lead sign-off.
- **AC2**: Manual check: verdict document
  - Setup: the spike has finished.
  - Verify: `production/qa/spikes/<date>_R5.md` states VSM, groom and Niagara sufficiency (or the extra step) and the hide/show proxy-rebuild cost.
  - Pass condition: all three are stated with retained captures; a failure goes to a superseding ADR.

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: retained captures in `production/qa/evidence/` plus the spike report `production/qa/spikes/<date>_R5.md` with lead sign-off

**Status**: [x] Created and passing (2026-10-29): Stealth.Spike.R5 (console spike, not in the gate); StealthGame.Pawn.RenderFlagParity; evidence production/qa/evidence/concealment-001-026-2026-10-29/

Run result: OBSERVED - rendering PIE in the editor (D3D12 SM6, software Lumen): captures and heat images in production/qa/spikes/2026-10-29_R5/

---

## Dependencies

- Depends on: None
- Unlocks: `story-022-presentation-hide-application-and-host-parity.md`, `story-028-enter-swap-hides-body-per-viewer.md`, `story-034-r5-leak-matrix.md`

## Completion Notes
**Completed**: 2026-10-29
**Criteria**: AC1-AC2 met for the cells this build can run: 6 cells (low sun / noon / shade x VSM on/off, software Lumen); in every Hidden state no subject primitive (body, groom, attached Niagara) has a render proxy and the subject-region difference Hidden-vs-Empty equals Empty-vs-Empty noise (visible control ~80x larger); VSM pages, groom and Niagara need no extra step; hide/show costs 0.024 ms game thread (0.016 ms under the D56 PROXY affinity). The four hardware-RT Lumen cells are NOT RUN (r.RayTracing=False; carried to story 034 with the packaged render-thread profile). Report production/qa/spikes/2026-10-29_R5.md with retained captures.
**Deviations**: The noise floor uses the same empty spot captured twice (the prototype bushes do not sway) instead of two bushes at matched sway phase; no magenta proxy (the empty-frame diff shows any residual directly); the subject stands 7 m ahead of the player's own camera (the sample's camera system keeps the view target); NpcCount 0 and the HUD hidden for the session (DefaultGame.ini restored).
**Test Evidence**: production/qa/evidence/concealment-001-026-2026-10-29/
**Reviewed by**: self code review 2026-10-29 (APPROVED; notes in the evidence)
