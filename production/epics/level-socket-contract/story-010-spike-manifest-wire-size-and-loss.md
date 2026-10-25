# Story 010: Spike — Manifest wire size and behaviour under loss

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-002`
*(At write time: TR-lsc-002 = Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash`)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: The Manifest travels as one bunch of about 31.5 KB (3,500 entries x 9 bytes) under the 64 KB cap; if the measured size or loss behaviour is unacceptable, the documented fallback is a staged publish of chunks over successive net updates (section 8.2). Risk R-4 owns this [U].
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Network emulation profiles and raw-bunch capture come from the ADR-0008 harness (`StealthTestHarness`). The graybox Manifest has 0 entries, so this spike uses **synthetic** 1,500- and 3,500-entry Manifests; the numbers matter chiefly for the Generator epic (LSC Open Question 2, ADR-0014 8.2).

**Why this story**: This is the "Manifest-wire-size spike" named in the epic's Build Order and Gates. It converts the provisional 15 KB figure of AC-36 and the loss behaviour of AC-34 into measured facts before the Generator depends on them. Read first: AC-34, AC-36, Open Question 2; ADR-0014 8.2 and Risks R-4.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Manifest plus pending GameState payload stays at most 48 KB at publish (ADR-0014 R-4 mitigation)
- Forbidden: Never replicate the Manifest as a single `TArray` or by reliable RPCs (ADR-0014)
- Guardrail: Manifest 13.5-31.5 KB once per round, about 0.3-0.7 s of a 50 KB/s budget (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-34 and AC-36 (advisory half) and ADR-0014 Risks R-4:*

- [ ] GIVEN a scripted loss pattern (the first 3 Manifest packets dropped) and 20 pinned network-emulation seeds at 10% loss and 200 ms, THEN the Manifest arrives, hashes match and the ack succeeds before `TravelLoadTimeoutSec` in every run (`Network.ManifestUnderLoss`)
- [ ] GIVEN synthetic Manifests of 1,500 and 3,500 entries, THEN wire size and publish time are recorded; about 1,500 entries is at most 15 KB (provisional, advisory); a publish-time assertion enforces Manifest plus pending GameState payload at most 48 KB (`Network.ManifestWireSize`, proposed)
- [ ] THEN the spike report in `production/qa/spikes/` states whether single-bunch chunked publish stands or the staged-publish fallback of ADR-0014 8.2 is adopted, with the measured numbers

---

## Implementation Notes

- Reuse the multi-client PIE harness and net-emulation profiles; log the dropped-packet sequence to prove the pinned emulation seed fixes the RNG.
- Measure raw-bunch bytes with the harness capture at 1,500, 3,500 and 3,501 entries (3,501 is rejected by `CityGenManifestEntriesMaxCount`).
- Write the report as `production/qa/spikes/lsc-manifest-wire.md`: the machine, profile, numbers, the decision and, if staged publish is adopted, a follow-up change request on ADR-0014 8.2 (superseding ADR rule: a failed spike is handled by a superseding ADR, not by editing this one).
- A failure here does not block this epic's graybox stories (the graybox Manifest is empty); it gates the Generator epic.

---

## Out of Scope

- Story 009: the replication implementation under test
- Generator-side entry production (procedural-city-generator epic)

---

## Files

- `Source/StealthTestHarness/Private/ManifestUnderLossTest.cpp`
- `Source/StealthGameplay/Private/Tests/Layout/ManifestWireSizeTest.cpp`
- `production/qa/spikes/lsc-manifest-wire.md`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Network.ManifestUnderLoss`
  - Given: first 3 Manifest packets dropped; 20 pinned emulation seeds at 10% loss, 200 ms.
  - When: a client joins a layout with a synthetic Manifest.
  - Then: in all 20 runs the Manifest arrives, hashes match and the ack succeeds before `TravelLoadTimeoutSec`.
  - Edge cases: verify the pinned seed actually fixes the emulation RNG (log the dropped-packet sequence).
- **AC-2** `Network.ManifestWireSize` (proposed)
  - Given: synthetic Manifests of 1,500 and 3,500 entries.
  - When: published over the real net driver.
  - Then: wire bytes and publish time are recorded; 1,500 entries <= 15 KB (advisory); an assertion fires if Manifest + pending GameState payload > 48 KB.
  - Edge cases: 3,500 (max) and 3,501 (rejected by `CityGenManifestEntriesMaxCount`).
- **AC-3** spike report (manual check)
  - Setup: `production/qa/spikes/lsc-manifest-wire.md` exists.
  - Verify: it states "single-bunch chunked publish stands" or "staged publish (ADR-0014 8.2) adopted".
  - Pass condition: the statement cites the AC-1 and AC-2 measured numbers.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/ManifestUnderLossTest.cpp` and `Source/StealthGameplay/Private/Tests/Layout/ManifestWireSizeTest.cpp` (tests above) plus the spike report in `production/qa/spikes/`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-009-manifest-chunked-replication-late-join.md`
- Unlocks: Generator epic (`production/epics/procedural-city-generator/EPIC.md`) Manifest-size assumptions
