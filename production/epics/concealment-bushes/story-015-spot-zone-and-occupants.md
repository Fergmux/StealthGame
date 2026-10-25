# Story 015: A strike point targets a bush within 75 cm and GetSpotOccupants returns the non-ally record

> **Epic**: Concealment (Bushes)
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/concealment-bushes.md` (CON-R12, CON-R10, F4 zone, AC-CON-14 zone and terminal parts)
**Requirement**: `TR-con-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0017: Concealment component and per-viewer presentation (§7 server queries)
**ADR Decision Summary**: `TargetsBush(WorldPoint)` is true iff the horizontal distance to a bush's planter footprint is ≤ `BushSpotToleranceCm` (75; zones never overlap at `BushSpacingCm` 12 m). `GetSpotOccupants(BushId, Attacker)` returns the occupancy record minus the attacker and its allies (Roster `GetRelation`), including pawns in Exiting/Emerging before their crossing, ordered by `CommitSeq`. Assassination filters by its F1 reach and picks one (story 016).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0017 overall; plain C++ geometry and set operations)
**Engine Notes**: No engine API. The zone is the footprint grown by the tolerance with rounded corners (about 11.8 m²): a point 75 cm diagonally off a corner, (1.53, 1.53) m from the anchor, is in; (1.55, 1.55) m is out. The bush rule is terminal (ASN-R8).

**Control Manifest Rules (this layer)**:
- Required: `GetSpotOccupants` and `TargetsBush` are server-only queries; the occupancy record is the only source — source: ADR-0017
- Forbidden: Never fall through from an empty or ally-only bush to open-spot candidates — source: ADR-0017, ADR-0018
- Guardrail: O(1) per bush

Co-occupants who are not allies do not see each other but may blind-strike their own bush; the striker emerges (story 014) and the victim is drawn from the non-ally candidates.

---

## Acceptance Criteria

*From GDD `design/gdd/concealment-bushes.md` (AC-CON-14, CON-R10, CON-R12), scoped to this story:*

- [ ] **AC1** (AC-CON-14 zone part, [U] `StealthGame.Concealment.SpotZoneAndVictim`): **GIVEN** `WorldPoint`s at 74 and 76 cm from a footprint side and at (1.53, 1.53) and (1.55, 1.55) m from the anchor **THEN** only the first of each pair targets the bush (`TargetsBush`, F4); the zone is the footprint grown by `BushSpotToleranceCm` with rounded corners.
- [ ] **AC2** (derived, CON-R12, CON-R10): **GIVEN** a bush record **WHEN** `GetSpotOccupants(b, Attacker)` is read **THEN** it returns the record minus the attacker and its allies (Roster `GetRelation`), including pawns in Exiting/Emerging before their crossing, ordered by `CommitSeq`; co-occupants who are not allies are included.
- [ ] **AC3** (AC-CON-14 terminal, [U]): **GIVEN** an empty bush with a visible pawn 50 cm beside the planter and no `PawnRef` **THEN** the strike is a whiff (the bush rule is terminal, never falls through to open spots); an ally-only bush gives an empty set (identical whiff).

---

## Implementation Notes

*Derived from ADR-0017 Decision §7 and GDD F4/CON-R12:*

F4: `TargetsBush(b, W) = d_xy(W, Footprint_b) ≤ BushSpotToleranceCm`. The aim ray's `WorldPoint` includes its intersection with a `HideVolume` (a bush-spot query channel), projected to the ground, so a strike aimed into the leaves always lands inside the footprint; the tolerance only matters for ground aims beside the planter. The tolerance must stay below half of `BushSpacingCm` and is kept equal to `OpenSpotToleranceCm` and `PawnRefAgreementCm` (Assassination). The exact 75 cm boundary is not stated inclusive or exclusive in the GDD; the implementer confirms and adds the case. Assassination first applies its F1 reach filter (story 016 helper and Assassination stories) then picks; this story provides only the queries. Occupants out of reach are filtered by Assassination, not here.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-016-spot-pick-secret-stream.md`: the unbiased pick.
- assassination-defence-reversal epic: F1 reach filter, the strike resolver.
- `story-033-blind-strike-parity-and-kill-timing.md`: the integration parity run.

---

## Files

- `Source/StealthGameplay/Features/Concealment/Private/ConcealmentSpot.cpp`
- `Source/StealthGameplay/Features/Concealment/Public/Server/ConcealmentService.h` (`TargetsBush`, `GetSpotOccupants`)
- `Source/StealthGameplay/Private/Tests/Concealment/SpotZoneTest.cpp` (class `FConcealmentSpotZoneTest`, name `StealthGame.Concealment.SpotZoneAndVictim`)
- `Source/StealthGameplay/Private/Tests/Concealment/SpotOccupantsTest.cpp` (class `FConcealmentSpotOccupantsTest`)

---

## QA Test Cases

*Written by qa-lead at story creation (QL-STORY-READY: ADEQUATE).*

- **AC1**: SpotZone
  - Given: footprint 2.0 × 2.0 m, tolerance 75 cm.
  - When: `TargetsBush` is evaluated at 74 and 76 cm from a side, and at (1.53, 1.53) and (1.55, 1.55) m from the anchor.
  - Then: only the first of each pair targets the bush (corner distances 0.7496 and 0.778 m against a 0.75 m radius).
  - Edge cases: the exact 75 cm boundary is not stated; confirm whether inclusive.
- **AC2**: SpotOccupants
  - Given: a record with the attacker A, ally B, rival C, and rival D in Exiting before crossing.
  - When: `GetSpotOccupants(b, A)` is read.
  - Then: it returns C and D ordered by `CommitSeq`; an ally-only bush returns an empty set.
  - Edge cases: D is excluded after its crossing.
- **AC3**: TerminalBushRule
  - Given: an empty bush, a visible pawn 50 cm beside the planter, and no PawnRef.
  - When: the strike resolves.
  - Then: a whiff, never falling through to open spots; an ally-only bush gives the identical whiff.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `SpotZoneTest.cpp` and `SpotOccupantsTest.cpp` under `Source/StealthGameplay/Private/Tests/Concealment/` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `story-002-bush-runtime-from-manifest.md`, `story-003-conceal-state-machine-and-occupancy-record.md`
- Unlocks: `story-016-spot-pick-secret-stream.md`, `story-033-blind-strike-parity-and-kill-timing.md`
