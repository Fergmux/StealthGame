# Story 014: Every audience row sits on its one carrier kind

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

R4 says each audience has exactly one kind of carrier and a carrier holds only its audience: `All` on the pawn, C-Public and GameState public structs; `Team` in C-TeamView on the receiving teammate's own PlayerController; `Owner` on the owner's PlayerController; `ServerOnly` in game-instance stores, never replicated. This story cross-checks the allowlist's audience column against the R4 carrier table so that a row on the wrong carrier (a Team fact on the pawn, an Owner gameplay field on the pawn, a ServerOnly fact as a replicated property) fails before it ever replicates.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-002` (One carrier kind per audience)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 5, the carrier model)
**ADR Decision Summary**: `All` = pawn / C-Public / GameState public structs; `Team` = C-TeamView components on the teammate's own PlayerController (no team-filtered actor, no team relevancy); `Owner` = the owner's PlayerController (the pawn only for the three `COND_OwnerOnly` engine pointers); `ServerOnly` = game-instance and world stores, never a replicated property.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Pure data check over the allowlist and the R4 fixture; the PlayerController is relevant only to its owner (E6 in ADR-0001), which is why Team and Owner data on it reach exactly one connection.

**Control Manifest Rules (this layer)**:
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- Forbidden: **Never use a team-filtered actor, a per-team replica actor or a team relevancy rule** — source: ADR-0001
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-35):*

- [ ] **AC-IVP-35** [Unit] `StealthGame.Visibility.CarrierAudienceConsistency`: GIVEN the allowlist's audience column and the R4 table, WHEN cross-checked, THEN the pawn carries only `All` rows, every `Team` fact is in C-TeamView, no `Owner` fact is a gameplay field on the pawn, and no `ServerOnly` fact is a replicated property. (R4)

---

## Implementation Notes

*Derived from the GDD rule R4 and ADR-0001 section 5:*

- This story creates the R4 carrier-table fixture itself (stories 011 and 013 do not): audience to allowed carrier kinds, as data. The check walks every allowlist row (story-013) and the matrix's fact rows (story-011) and verifies: pawn rows are `All` only; every `Team` fact's carrier is a C-TeamView part; no `Owner` fact is a gameplay field on the pawn (only the three `COND_OwnerOnly` engine pointers are allowed on the pawn); no `ServerOnly` fact appears as a replicated property.
- Negative control (added by the story, ADR-0008 section 3): an allowlist copy with one `Team` fact placed on the pawn must make the check fail.
- Files in `StealthFoundation`; reuses the parser from story-013.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-013: the allowlist content; story-015: the reflection audit of real classes
- story-021: runtime verification that no opponent connection receives Team data

---

## Files

- `Source/StealthFoundation/Private/Tests/Data/CarrierTable.h` (the R4 fixture)
- `Source/StealthFoundation/Private/Tests/Visibility/CarrierAudienceConsistencyTest.cpp` (test `StealthGame.Visibility.CarrierAudienceConsistency`, class `FVisibilityCarrierAudienceConsistencyTest`)

*(The module folders do not exist yet; tuning-data-config Exit A creates them.)*

---

## QA Test Cases

- **AC-1**: CarrierAudienceConsistency (AC-IVP-35)
  - Given: the allowlist audience column and the R4 carrier-table fixture.
  - When: the two are cross-checked.
  - Then: the pawn carries only All rows; every Team fact is in C-TeamView; no Owner fact is a gameplay field on the pawn; no ServerOnly fact is a replicated property.
  - Edge cases: the R4 fixture has no other owning story; story-014 creates it. Negative control (added): an allowlist copy with one Team fact on the pawn must fail. (Tier: T1.)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Visibility/CarrierAudienceConsistencyTest.cpp` (the studio docs' `tests/unit/visibility/`) — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`, `production/epics/information-visibility-policy/story-013-allowlist-content.md`
- Unlocks: None
