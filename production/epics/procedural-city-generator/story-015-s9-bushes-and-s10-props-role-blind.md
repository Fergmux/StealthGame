# Story 015: S9 Bushes and S10 props, role-blind

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The Generator emits geometry and LSC socket records only and never sees a role; bush sites are LSC `BushSite` records every machine builds locally from the Manifest. Pillar 1 is protected by construction: S9 and S10 read no zone, court, sun-pool or socket data beyond the LSC exclusion distances.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. The `BushSite` schema is the Concealment epic's input: planter 2.0 x 2.0 m, 0.35 m curb, 1-3 entry sides; LSC V-BUSH-1's >= 2 open sides is the stricter rule and governs.

**Control Manifest Rules (this layer)**:
- Required: Bushes are built locally from the Manifest on every machine — source: ADR-0017, ADR-0014
- Forbidden: Never let a bush or prop be chosen because of a socket's role; never use a per-role bush exclusion (a bush-free court would be a final tell: LSC request 11a, one role-blind `BushCourtExclusionCm` for every court if adopted) — source: GDD Edge Cases, LSC OQ 14
- Guardrail: part of the 0.12 s candidate budget

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G7, R-G4 S9 and S10), scoped to this story:*

- [ ] **AC1** (AC-G7, `Leak.BushPropStagesRoleBlind`): **GIVEN** one seed with its zone and court records perturbed between two runs **THEN** bush and prop placements are byte-identical, and an injected role-field read in either stage fires the assert.
- [ ] **AC2** (R-G4 S9, `Bushes.RoleBlindPoisson`): **GIVEN** 50 seeds **THEN** bushes are a role-blind Poisson-disc over eligible planter cells (2.0 x 2.0 m, 0.35 m curb, >= 2 open sides per LSC V-BUSH-1), N_B = round(A / 1,000 m^2) (8-10), emitted as exact LSC `BushSite` records, and the stage reads no zone, court, sun-pool or socket data beyond the LSC exclusion distances.
- [ ] **AC3** (R-G4 S10, `Props.FamilyOccurrence`): **GIVEN** 50 seeds **THEN** waist and eye-band props follow art 6.4 densities, every family occurs >= 3 times or not at all, ground-class props (<= 0.5 m) are left to decoration, and props are Manifest entries.

---

## Implementation Notes

*Derived from GDD R-G4 S9 and S10 and LSC V-BUSH-1/2/3:*

S9: role-blind Poisson-disc sampling (integer, canonical order over eligible planter cells) with spacing `BushSpacingCm` (LSC V-BUSH-1, 12 m default) and the LSC exclusion distances as the only external reads; N_B = round(A / 1,000 m^2) per LSC D5 (8-10 over the band; A = 8,500 rounds to 9). The behaviour when eligible cells number fewer than N_B is unspecified in the GDD: take "fail the candidate in G-PRE (V-BUSH-1 count)" and record it. V-BUSH-3 (backdrop) has no grid form and is left to LSC Validate. S10: waist and eye-band props with collision or sight-blocking, art 6.4 densities, every family >= 3 times or not at all (G0 has 2-3 ground props as primitive placeholders), and they are Manifest entries; ground-class props (<= 0.5 m) are decoration (R-G8, story 029). The role-blind test perturbs every zone and court field (not one), compares placement hashes, and injects a role-field read into each stage to prove the assert fires (a seam, compiled out of Shipping).

Prototype replaced: the prototype moved bushes to seeded sites (`StealthCityGenerator`) with no spacing, density or role-blind rule; none is carried over.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-016-s11-repairs-and-occurrence-rule.md`: the occurrence fix for props and variants.
- `story-029-client-decoration.md`: ground-class props and foliage as decoration.
- `story-022-g-pre-predicates-b-and-role-counts.md`: V-BUSH-1/2 pre-checks.
- Concealment epic (client presentation of bushes): `concealment-bushes`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageBushes.cpp`
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageProps.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/BushPropStagesRoleBlindTest.cpp` (class `FCityGenLeakBushPropStagesRoleBlindTest`, name `StealthGame.CityGen.Leak.BushPropStagesRoleBlind`)
- `Source/StealthGameplay/Private/Tests/CityGen/RoleBlindPoissonTest.cpp` (class `FCityGenBushesRoleBlindPoissonTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/FamilyOccurrenceTest.cpp` (class `FCityGenPropsFamilyOccurrenceTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `Leak.BushPropStagesRoleBlind`
  - Given: one seed, run once as a baseline and once with every zone and court record perturbed.
  - When: S9 and S10 run.
  - Then: bush and prop placement hashes are identical. An injected role-field read in S9 and one in S10 each fire the assert.
  - Edge cases: perturb all fields, not one.
- **AC2** `Bushes.RoleBlindPoisson`
  - Given: 50 seeds.
  - When: S9 runs.
  - Then: sites are only in eligible planter cells (2.0x2.0 m, 0.35 m curb, at least 2 open sides). The count is round(A / 1,000 m^2), which is 8-10. Pairwise spacing is at least `BushSpacingCm` (12 m, LSC V-BUSH-1). Records match the exact `BushSite` schema. The stage reads 0 zone, court, pool or socket fields beyond the exclusion-distance query.
  - Edge cases: A = 8,500 rounds to 9. The behaviour when eligible cells number fewer than N_B fails the candidate (recorded choice).
- **AC3** `Props.FamilyOccurrence`
  - Given: 50 seeds.
  - When: S10 runs.
  - Then: densities per area type match the art-bible 6.4 table. Each family count is 0 or at least 3. No ground-class prop (0.5 m or less) is a Manifest entry. Every placed prop is a Manifest entry.
  - Edge cases: a family count of 1 or 2 fails. A 0.5 m prop counts as ground-class.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-009-s3-area-types.md`, `story-004-kit-tables-and-generator-kit-version.md`
- Unlocks: `story-016-s11-repairs-and-occurrence-rule.md`, `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`, `story-022-g-pre-predicates-b-and-role-counts.md`
