# Story 029: Spike CPA-V5: PoolHash is identical across cooked client, cooked host and PIE

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-005`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-005 = All pool assets resident and precached before leaving `Preparing`; no first-draw load.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation (conditional gate CPA-V5, risk R7)
**ADR Decision Summary**: `PoolHash` hashes only authored data and editor-written content digests so it is byte-identical on a cooked client, a cooked listen host and PIE for the same content, and differs after any pool row or referenced package changes.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Editor and cooked `GetPackageSavedHash` may differ: record the finding either way; a mismatch means the digest must be computed from something both builds carry (a superseding decision, not an edit of ADR-0012).

**Why this story**: Story 033 (acknowledgement) is Blocked until this reports. It needs a cooked build, so the cooked half is DEFERRED to the first cooked run per QL-STORY-READY. Read ADR-0012 Verification Required CPA-V5 and risk R7.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: `PoolHash` computed identically on server, listen host and clients (ADR-0012)
- Forbidden: Never hash runtime pointers or cooked offsets
- Guardrail: n/a

---

## Acceptance Criteria

*From ADR-0012 Verification Required CPA-V5:*

- [ ] **AC-1** GIVEN the same content, THEN `PoolHash` is byte-identical on a cooked client, a cooked listen host and PIE.
- [ ] **AC-2** GIVEN a changed pool row, then a changed referenced package (including a whitespace-only change to a referenced text asset), THEN the hash differs each time, and `production/qa/spikes/<date>_CPA-V5.md` states PASS or FAIL.

---

## Implementation Notes

- Log the `PoolHash` at `UCharacterPoolSubsystem` initialise in each process; compare as hex text.
- If PIE and cooked differ because of `GetPackageSavedHash`, recommend the alternative digest in the report.

**Prototype disposition**: none.

---

## Out of Scope

- Story 028: the hash; Story 033: the acknowledgement field

---

## Files

- `production/qa/spikes/<date>_CPA-V5.md`
- `production/qa/logs/<date>_CPA-V5-poolhash-*.txt`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; cooked half DEFERRED to a cooked run); spike report.*

**Test: cross-process hash**
- Given: the same content on a cooked client, a cooked listen host and PIE.
- When: `PoolHash` is read in each.
- Then: the three values are byte-identical.
- Edge cases: editor and cooked saved hashes may differ; record either way.

**Test: change detection**
- Given: one changed pool row, then one changed referenced package.
- When: `PoolHash` is recomputed.
- Then: it differs in each case and the report states PASS or FAIL.
- Edge cases: whitespace-only change to a referenced text asset.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `production/qa/spikes/<date>_CPA-V5.md` (documented spike report with logs)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-028-poolhash-and-content-digest.md`, `production/epics/character-pool-appearance/story-025-pool-subsystem-residency.md`
- Unlocks: `production/epics/character-pool-appearance/story-033-poolhash-in-session-ack.md`
