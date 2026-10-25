# Story 052: Retire the prototype pool, dealer and per-pawn tint

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-001`, `TR-cpa-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-001 = Pool as one cooked data asset of appearance rows; 6-8 models, ≤ 24 appearances, stable `AppearanceId`. TR-cpa-002 = `AllocateAppearances` once per round; balanced deck, model-stratified subset, unbiased Fisher-Yates.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: The prototype path (`UStealthSettings` Config arrays, MID tint, `FMath::RandRange`, `LoadSynchronous`) is rejected: ini back door (TDC CR-13), invertible engine-versioned RNG, no conservation, a per-pawn parameter surface and a pop correlated with first use; nothing is promoted until the ADR that governs it is built against.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Deleting or editing `Content/` assets is subject to `production/never-resave-assets.md`: remove references first, then delete only the prototype's own assets (`SK_Citizen02-05`, `M_CrowdTint`, `BP_CrowdCharacter` when unreferenced), never sample content; diff `Config/` after any save.

**Why this story**: The epic's Prototype Disposition says the prototype pool and dealer are replaced; this story removes them once the production path runs, so two appearance paths never coexist. The prototype Meshy citizens are throwaway (arms about 17 % short, extra root bone, per the Art Director's note). Read `production/first-playable-report.md` (known issue 3) and ADR-0012 Alternative 1.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Only the factory writes `AppearanceId`; the allocator is the only dealer (ADR-0012)
- Forbidden: Never keep a second pool or deal path; never use `FMath::RandRange` for a deal (ADR-0006)
- Guardrail: n/a

---

## Acceptance Criteria

*From the epic's Prototype Disposition and ADR-0012 Alternative 1:*

- [ ] **AC-1** GIVEN the production path, THEN the prototype pool entries (`StealthSettings.h`, `DefaultGame.ini`), the deal inside `StealthGameMode.cpp` and `AStealthCharacter::SetAppearanceIndex`, `ApplyAppearance` and `OnRep_AppearanceIndex` are removed and a grep (patterns listed in the story's evidence note) finds no reference.
- [ ] **AC-2** GIVEN a 2-client PIE round, THEN it starts through the production allocator with a retained screenshot in `production/qa/evidence/`, and `SK_Citizen02` to `05`, `BP_CrowdCharacter` and `M_CrowdTint` are unreferenced by the production pool (Reference Viewer).
- [ ] **AC-3** GIVEN the repository, THEN `tools/asset-pipeline/blender_bind_to_mannequin.py` and the `assets/characters/meshy` sources still exist.

---

## Implementation Notes

- Retire in this order: switch the game mode to the allocator and factory; delete the prototype arrays and ini keys; then delete the prototype assets.
- Keep the prototype's balanced-deck idea only as history in the commit message (it is the origin of the GDD rule).
- Any prototype console command or spike probe that depends on the old pool (the `stealth.Spike.*` commands noted in the 2026-10-25 spike report) must be re-pointed or removed here; record which.

**Prototype disposition**: this story removes `Source/StealthGame` appearance code and the four citizens; it promotes nothing.

---

## Out of Scope

- Retiring other prototype systems (their epics); Story 054: swapping in Tier B or C content

---

## Files

- `Source/StealthGame/Public/StealthSettings.h`, `Source/StealthGame/Private/StealthGameMode.cpp`, `Source/StealthGame/Public/StealthCharacter.h` and `.cpp` (appearance parts only)
- `Config/DefaultGame.ini` (pool keys only)
- `Content/StealthGame/Characters/Pool/SK_Citizen02..05`, `M_CrowdTint` (delete when unreferenced)
- `production/qa/evidence/retire-prototype-pool-evidence.md` and the PIE screenshot

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; blocked in practice until story 027 lands, which is why it depends on it).*

**Test: prototype removal**
- Given: the production source.
- When: grepping for the prototype pool entries, the deal in `StealthGameMode.cpp`, `SetAppearanceIndex`, `ApplyAppearance` (prototype) and `OnRep_AppearanceIndex`.
- Then: zero references.
- Edge cases: the grep patterns are listed in the evidence note.

**Test: 2-client round**
- Given: a 2-client PIE round.
- When: it starts.
- Then: it uses the production allocator with a retained screenshot; the citizens, `BP_CrowdCharacter` and `M_CrowdTint` are not referenced by the production pool.
- Edge cases: Reference Viewer is the check.

**Test: preserved tools**
- Given: the repo.
- When: checked.
- Then: `tools/asset-pipeline/blender_bind_to_mannequin.py` and `assets/characters/meshy` exist.
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: grep record and a retained 2-client PIE screenshot in `production/qa/evidence/` (documented playtest) OR an automation test

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-013-allocator-facade-signature-and-call-order.md`, `production/epics/character-pool-appearance/story-018-ledger-core-commit-vacate-invariants.md`, `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`, `production/epics/character-pool-appearance/story-046-tier-a-pool-asset-authoring-and-validation.md`
- Unlocks: None
