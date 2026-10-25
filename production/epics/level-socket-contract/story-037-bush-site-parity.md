# Story 037: BushSite derivation and server/client parity

> **Epic**: Level Socket Contract
> **Status**: Blocked — BLOCKED: design gap, not an ADR or spike. GDD R2.7 gives the graybox an empty Manifest (0 entries) while R2.6 and ADR-0014 say clients derive `BushSite` records from the Manifest; ADR-0014 section 6 lets graybox clients read only the baked blob header. Where graybox BushSite records reach clients is undefined. Resolve with an ADR-0014 / GDD R2.7 amendment (`/architecture-decision`), then re-run `/qa-plan` for this story.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-003`
*(At write time: TR-lsc-003 = Zero replicated actors; bush/court components built locally)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: Bush hide volumes and entry axes are built locally from the Manifest as non-replicated components; each bush's `BushId` is its `SocketId`; `DeriveBushSites(const FLayoutManifest&)` is pure and used by server and clients (AC-52).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Plain derivation function plus 2-client PIE parity check; no new engine API.

**Why this story**: Concealment's hide volumes must be identical on server and clients or players would hide in different places. Read first: R2.6, R2.7, R3 BushSite row, AC-52.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Clients build only from a complete `FLayoutManifest`; layout-derived components are `bReplicates = false` (ADR-0014)
- Forbidden: Never add a field to `FLayoutManifest` beyond section 8.1 (always-ask change) (ADR-0014)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-52, with the canonical serialisation proposed here (QL-STORY-READY GAPS: serialisation was undefined and the graybox input is open, so these criteria wait for the design amendment; QA specs are not yet defined):*

- [ ] GIVEN a 2-client PIE, THEN the FNV-1a 64 hash over BushSites sorted by `BushId`, each serialised little-endian as (u16 `BushId`, i32 `HideVolume` min x,y,z and max x,y,z in cm, u8 `EntryAxis` bitset), is identical on the server and both clients (`Cross.Concealment.BushSiteParity`)
- [ ] GIVEN the derivation input, THEN `DeriveBushSites` is pure over it: for the Generated provider the Manifest entries, and for the Graybox provider the baked, versioned records blob of the opaque layout-data asset (clients hold that blob in the cook; AC-38 allows it as an opaque blob); identical calls give identical records and `BushId` equals the bush's `SocketId`

---

## Implementation Notes

- The design gap to close first (owner: technical-director with the unreal-specialist): either graybox bush planters/fixtures become Manifest entries, or the opaque blob (not only its header) is read by clients to derive `BushSite`s; both keep `FLayoutManifest` unchanged. Record the decision as an ADR-0014 amendment and update GDD R2.7.
- Hash input excludes occupancy (Concealment's, runtime). The client derivation uses the same builder rule as story 013 (non-replicated components).
- Once unblocked, ask `/qa-plan` for the specs; the Concealment epic consumes the records (`production/epics/concealment-bushes/EPIC.md`).

---

## Out of Scope

- Concealment's occupancy and presentation (`production/epics/concealment-bushes/EPIC.md`); story 013: the geometry builder

---

## Files

- `Source/StealthGameplay/Public/View/Core/StealthLayoutManifest.h` (`DeriveBushSites`)
- `Source/StealthGameplay/Private/View/DeriveBushSites.cpp`
- `Source/StealthTestHarness/Private/BushSiteParityTest.cpp`

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them once the design gap above is closed (QL-STORY-READY: GAPS, accepted).*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/BushSiteParityTest.cpp` — `StealthGame.Layout.Cross.Concealment.BushSiteParity` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-013-client-geometry-builder-and-decoration.md`, `production/epics/level-socket-contract/story-026-graybox-provider-markers-and-bake.md`, and the design amendment above
- Cross-epic: `production/epics/concealment-bushes/EPIC.md`
- Unlocks: Concealment epic bush-site stories
