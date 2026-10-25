# Story 039: The owner's hidden first-person body has no effect on what others see

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: No client-side or per-viewer appearance exists; the owner's hidden first-person body is a local render flag (owner-no-see) on the owner's client and changes nothing others see; the same `AppearanceId` yields the same mesh, material instance, LOD and shadow flags for every viewer.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Reclassified from Visual/Feel to Integration per QL-STORY-READY (the GDD lists AC-CP-18 under Integration and the automated render-state comparison is the real gate); the engine screenshot comparison remains required evidence and is retained. Anti-aliasing off, fixed time, both pawns pinned to animation frame 0.

**Why this story**: CP-16 and AC-CP-18: the owner's own pawn must not be recognisable by a render difference. Read GDD CP-16, Game Feel ("the camera is the identifier") and AC-CP-18.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `ApplyAppearanceVisuals` reads no local-ness or viewer; owner-no-see is a local flag only (ADR-0012)
- Forbidden: Never add an own-pawn highlight, outline or tint; no per-viewer appearance
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-18; type reclassified per QL-STORY-READY:*

- [ ] **AC-1** GIVEN the owning client in first person and a remote client at a fixed distance and angle, with both the owner's pawn and an NPC of the same `AppearanceId` pinned to animation frame 0, anti-aliasing off and fixed time, THEN a render-state comparison (mesh, material instance pointer, LOD, shadow flags) is equal (test `StealthGame.CharacterPool.PIE.OwnerHiddenMeshNoRemoteEffect`).
- [ ] **AC-2** GIVEN the same capture, THEN an engine screenshot comparison of the two pawn crops passes at `Low` tolerance and the screenshots are retained in `production/qa/evidence/`; owner-no-see is a local flag only (no replicated or remote-visible effect).

---

## Implementation Notes

- The remote client's crop of the owner's pawn is compared with the crop of the NPC twin from the same viewpoint.
- If a lead sign-off is wanted on the screenshots, record it in the evidence note; it is not a gate here.

**Prototype disposition**: none.

---

## Out of Scope

- Story 040: all pawn classes and audio; Camera System's eye mask (camera-system epic)

---

## Files

- `Source/StealthTestHarness/Private/CharacterPool/OwnerHiddenMeshTest.cpp`
- `production/qa/evidence/owner-hidden-mesh-*.png`

---

## QA Test Cases

*QL-STORY-READY: GAPS → reclassified to Integration with the qa-lead's recommended fix; not re-run (unattended; decision logged).*

**Test: render-state equality**
- Given: an owner client in first person and a remote client at a fixed distance and angle, both pawns pinned to frame 0, anti-aliasing off, fixed time.
- When: render state is read.
- Then: mesh, MI pointer, LOD and shadow flags are equal.
- Edge cases: none.

**Test: screenshot comparison**
- Given: the same setup; crops of the owner's pawn and the NPC.
- When: the engine screenshot comparison runs at `Low` tolerance.
- Then: it passes, and the files are retained in `production/qa/evidence/`.
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CharacterPool/OwnerHiddenMeshTest.cpp` — automation test `StealthGame.CharacterPool.PIE.OwnerHiddenMeshNoRemoteEffect` + retained screenshots in `production/qa/evidence/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`, `production/epics/camera-system/story-004-camera-manager-shell-remote-pov.md`
- Unlocks: None
