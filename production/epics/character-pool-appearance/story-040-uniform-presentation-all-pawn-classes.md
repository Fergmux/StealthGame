# Story 040: One presentation for every pawn class; no appearance-keyed audio or variation

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
**ADR Decision Summary**: The same `AppearanceId` yields the same mesh, material instance, LOD selection by screen size, skin-cache setting, physics asset, animation fidelity and shadow behaviour for every viewer and every pawn class (player, NPC, informant, concealed, dead); `AppearanceId` is never an input to any audio selection; two copies of one appearance differ in nothing.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Concealed and dead pawn states belong to Shared Character & Locomotion and Concealment (not coded yet): those cases are DEFERRED until those states exist; the audio-event module does not exist yet, so the reflection scan must fail on an empty module rather than pass vacuously (QL-STORY-READY).

**Why this story**: CP-16 and AC-CP-20a to 20c. **AC-2 and AC-3 revised per QL-STORY-READY.** Read GDD CP-16, AC-CP-20a/b/c and art bible 5.2.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Footsteps and every audio event come from per-class sound with no per-pawn parameter; one look for everyone (ADR-0012, ADR-0013)
- Forbidden: Never key audio, scale, material parameters or VFX by `AppearanceId`
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-20a, 20b, 20c; AC-2 and AC-3 revised per QL-STORY-READY:*

- [ ] **AC-1** GIVEN each pawn class (player, NPC, informant, concealed, dead), THEN the same `AppearanceId` yields the same mesh, MI pointer, LOD, shadow and animation-tick settings and no pawn has a runtime MID or a player-only component (test `StealthGame.CharacterPool.PIE.UniformPresentationAllClasses`; the concealed and dead states are DEFERRED until Shared Character and Concealment exist).
- [ ] **AC-2** GIVEN every `UCLASS` and `USTRUCT` in the audio-event module, THEN a reflection test finds no property of type `FAppearanceId` and **fails if it finds nothing to scan** (an empty module is a failure, not a pass), a seeded property makes it fail, and two copies of one appearance have identical scale and material parameters (test `...PIE.NoAppearanceKeyedAudioOrInstanceVariation`).
- [ ] **AC-3** GIVEN pawns of three different looks in assassination range in turn from a fixed camera and time, THEN the prompt crops are pixel-identical (engine comparison at `Zero` tolerance) and retained in `production/qa/evidence/`; DEFERRED until the HUD and Assassination systems exist.

---

## Implementation Notes

- Compare through the harness's render-state reader (story 039's tuple); add anim-tick settings.
- AC-3 needs the HUD prompt (hud and assassination-defence-reversal epics); keep the test written and mark it deferred in the evidence note until they land.

**Prototype disposition**: none.

---

## Out of Scope

- Story 039: owner-hidden mesh; the HUD prompt's own tests (hud epic)

---

## Files

- `Source/StealthTestHarness/Private/CharacterPool/UniformPresentationTest.cpp`
- `production/qa/evidence/assassination-prompt-three-looks-*.png`

---

## QA Test Cases

*QL-STORY-READY: GAPS → AC-2 and AC-3 revised with the qa-lead's proposed text; not re-run (unattended; decision logged).*

**Test: uniform presentation**
- Given: player, NPC, informant, concealed and dead pawns with the same `AppearanceId`.
- When: their state is read.
- Then: mesh, MI pointer, LOD, shadow and animation-tick settings are equal; no runtime MID; no player-only component.
- Edge cases: DEFERRED for concealed and dead until Shared Character.

**Test: audio and instance variation**
- Given: the audio module and two copies of one appearance.
- When: scanning and comparing.
- Then: no `AppearanceId` parameter or selector; scale and material parameters identical.
- Edge cases: the empty-module guard.

**Test: prompt screenshots (DEFERRED)**
- Given: three different looks in assassination range in turn.
- When: capturing the prompt.
- Then: pixel-identical crops.
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CharacterPool/UniformPresentationTest.cpp` — automation tests `StealthGame.CharacterPool.PIE.UniformPresentationAllClasses`, `...PIE.NoAppearanceKeyedAudioOrInstanceVariation`; retained prompt screenshots when HUD exists

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`, `production/epics/shared-character-locomotion/story-041-body-and-anim-set-parity.md`, `production/epics/shared-character-locomotion/story-043-footstep-parity.md`
- Unlocks: None
