# Story 014: S8 Traversal points

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The Generator emits LSC `TraversalPoint` records from coping-tagged kit variants only (LSC D5 `N_T`); traversal heights equal the LSC keys `TraversalVaultHeightCm` / `TraversalClimbHeightCm`, which Shared Character & Locomotion supplies.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; plain integer C++)
**Engine Notes**: No engine API. The traversal capability heights belong to SCL (defaults 80 / 160 cm); the kit must match them (`shared-character-locomotion/story-046-traversal-heights-match-kit.md`).

**Control Manifest Rules (this layer)**:
- Required: Pure data, integer streams — source: ADR-0014
- Forbidden: A shortcut must never be needed for reachability (a traversal-only route is a V-REACH failure) — source: GDD S8
- Guardrail: part of the 0.12 s candidate budget

Provisional: this stage is marked † in the GDD (pending the traversal spike of Shared Character & Locomotion); the heights are LSC key values, so a spike result changes data, not code.

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (R-G4 S8), scoped to this story:*

- [ ] **AC1** (R-G4 S8, `Traversal.CopingOnlyVariants`): **GIVEN** 50 seeds **THEN** traversal exists only at coping-tagged M01 variants (vault 0.8 m at quays and market edges, climb 1.6 m and platform edges at the temple), with heights equal to LSC `TraversalVaultHeightCm` / `TraversalClimbHeightCm`; M10 carries no traversal tag (its 0.4 m kerb is stepped, art bible M10, and the water is blocked by the pawn-only volume, AC-G15).
- [ ] **AC2** (`Traversal.OccurrenceAndStairs`): **GIVEN** 50 seeds **THEN** every traversal variant occurs 0 or >= 3 times (quay variants are 0 at tier G0, canals off) and temple stairs exist on all 4 faces.
- [ ] **AC3** (`Traversal.NeverNeeded`): **GIVEN** a layout with all vault and climb points removed (temple stairs kept) **THEN** every court is still reachable within the D2 budget, so a shortcut saves path but is never needed.

---

## Implementation Notes

*Derived from GDD R-G4 S8, art bible 6.2 (M10) and the QL-STORY-READY review:*

The GDD lists "M10 quay walls" among the coping-tagged variants, but the art bible defines M10 as the acequia edge, a 0.4 m kerb above the quay, and AC-G15 says that kerb is stepped (below SCL's 45 cm step height) and not blocking. A 0.4 m kerb is therefore not a vault: this story reads the GDD's quay traversal as the M01 coping variants along quay walls, tags nothing on M10, and flags the GDD wording for the level-designer to ratify. Vault 0.8 m at quays and market edges; climb 1.6 m and platform edges at the temple; temple stairs on 4 faces. Every traversal variant occurs 0 or >= 3 times (the same occurrence rule S11.4 enforces, story 016, since a variant used 1-2 times is replaced). A traversal point must save path but is never needed: remove all of them and V-REACH must still pass for every court (the negative control is a fixture reachable only through a vault, which must fail). Emit LSC `TraversalPoint` records (position, kind, height) in story 017.

Prototype replaced: the prototype's "sample traversal" (Space = climb/vault) used the Game Animation Sample course; the generated kit has its own coping variants.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-016-s11-repairs-and-occurrence-rule.md`: the occurrence fix.
- `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`: `TraversalPoint` records.
- LSC V-TRAV and V-REACH on the navmesh: `level-socket-contract/story-017-predicates-navigation-area-sight-route-traversal.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenStageTraversal.cpp`
- `Source/StealthGameplay/Private/Tests/CityGen/CopingOnlyVariantsTest.cpp` (class `FCityGenTraversalCopingOnlyVariantsTest`, name `StealthGame.CityGen.Traversal.CopingOnlyVariants`)
- `Source/StealthGameplay/Private/Tests/CityGen/OccurrenceAndStairsTest.cpp` (class `FCityGenTraversalOccurrenceAndStairsTest`)
- `Source/StealthGameplay/Private/Tests/CityGen/NeverNeededTest.cpp` (class `FCityGenTraversalNeverNeededTest`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: AC2 and AC3 ADEQUATE; AC1 was GAPS on the M10 height and is revised above per the art bible).*

- **AC1** `Traversal.CopingOnlyVariants`
  - Given: 50 seeds at G0 plus a canal-enabled G1 fixture.
  - When: S8 runs.
  - Then: every traversal point sits on a coping-tagged M01 variant; vault height is the LSC vault key (80 cm default) and climb height the climb key (160 cm default); no traversal point is on an M10 piece.
  - Edge cases: a traversal tag on M10 fails (negative control). Heights are compared against the LSC key values at their hard-range extremes as well as the defaults.
- **AC2** `Traversal.OccurrenceAndStairs`
  - Given: 50 seeds at G0, plus a canal-enabled G1 fixture.
  - When: S8 runs.
  - Then: every traversal variant count is 0 or at least 3. Quay variants are 0 at G0. Temple stairs exist on all 4 faces.
  - Edge cases: a count of 1 or 2 fails. The stairs check applies on every seed.
- **AC3** `Traversal.NeverNeeded`
  - Given: 50 seeds.
  - When: all vault and climb points are removed (temple stairs kept).
  - Then: V-REACH passes, so every court is reachable from every start cell within the D2 budget. In at least one fixture, the path with traversal is strictly shorter.
  - Edge cases: a fixture reachable only via a vault fails (negative control).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Logic
**Required evidence**:
- Logic: the three test files under `Source/StealthGameplay/Private/Tests/CityGen/` — must exist and pass (`/story-done` checks that it EXISTS; pass/fail is established by `/gate-check` and `/smoke-check`, both later)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-009-s3-area-types.md`, `story-004-kit-tables-and-generator-kit-version.md`; cross-epic `shared-character-locomotion/story-046-traversal-heights-match-kit.md` (heights)
- Unlocks: `story-016-s11-repairs-and-occurrence-rule.md`, `story-017-s12-sockets-socket-ids-slab-cover-and-skeleton-transform.md`
