# Story 051: Network cost of AppearanceId recorded in a 10-client run

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: `AppearanceId` costs about 1 byte of payload plus 1 to 2 bytes of handle per pawn per channel open, roughly 120 to 180 bytes once for 60 pawns and 0 B/s in steady state (F-9); the larger indirect cost is identical update rate for all pawns, against the 50 KB/s budget owned by Shared Character & Locomotion.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Network Insights trace of a 10-client round (multi-process harness). The 50 KB/s total is **record only** here.

**Why this story**: AC-CP-32 / F-9 is a measurement the GDD wants recorded, not a CPA budget gate. Read GDD F-9, Replication "Bandwidth" and AC-CP-32.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `AppearanceId` about 100 to 150 B per client per round and 0 B/s steady state (ADR-0012 guardrail)
- Forbidden: n/a
- Guardrail: at most 180 B per client per round

---

## Acceptance Criteria

*From GDD AC-CP-32 (`PT-CP-NETBUDGET`); steady-state window made explicit per QL-STORY-READY:*

- [ ] **AC-1** GIVEN a 10-client run of one round with Network Insights, THEN `AppearanceId` costs at most 180 B per client per round (including the client that joins last) and 0 B/s over a recorded steady-state window (start and length recorded).
- [ ] **AC-2** GIVEN the same run, THEN total pawn replication per client is recorded against the 50 KB/s budget (record only) and the CSV is retained in `production/qa/evidence/`.

---

## Implementation Notes

- `F-9: D * (1 + h)` with `h` = 2 at `D = 60` gives the 180 B bound.
- Documented profiling run (not an automation test): name `PT-CP-NETBUDGET`.

**Prototype disposition**: none.

---

## Out of Scope

- The 50 KB/s budget gate (shared-character-locomotion story 034)

---

## Files

- `production/qa/evidence/pt-cp-netbudget-<date>.csv` and `production/qa/evidence/pt-cp-netbudget-<date>.md`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead). Test `PT-CP-NETBUDGET`.*

**Test: appearance id cost**
- Given: a 10-client run of one round with Network Insights.
- When: `AppearanceId` bytes are read per client.
- Then: at most 180 B per client per round and 0 B/s over a recorded steady-state window.
- Edge cases: the client that joins last.

**Test: total pawn replication**
- Given: the same run.
- When: total pawn replication is recorded.
- Then: the value is recorded against 50 KB/s (record only) and the CSV is retained.
- Edge cases: none.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: documented profiling run with CSV in `production/qa/evidence/` (`PT-CP-NETBUDGET`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: None
