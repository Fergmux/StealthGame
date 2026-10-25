# Story 009: Chunked Manifest replication, idempotent handler and late join

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-002`, `TR-lsc-003`
*(At write time: TR-lsc-002 = Integer Layout Manifest; clients build with no RNG and recompute `LayoutHash`; TR-lsc-003 = Zero replicated actors; bush/court components built locally)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (with ADR-0003 for the allowlist row)
**ADR Decision Summary**: `FLayoutManifest` is one `ReplicatedUsing` property on `AStealthGameState`, audience All, **not** `COND_InitialOnly`, serialised as fixed-width 9-byte chunk entries (one bunch of about 31.5 KB at the 3,500-entry maximum, under the 64 KB constructed-bunch cap); the client handler is idempotent on `LayoutId`. ADR-0003: every replicated property is on the audited allowlist.
**ADR Version**: 2026-10-24 (ADR-0014); 2026-10-24 (ADR-0003)

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0003 networking 5.4-5.8)
**Engine Notes**: The wire limit is the 64 KB `net.MaxConstructedPartialBunchSizeBytes` cap, not `DefaultMaxRepArraySize` (unenforced in 5.8.4 [V]); a bunch of 8 or more partials is sent reliable [V]. Legacy NetDriver only (ADR-0001). `WithNetSerializer` and `WithIdentical` struct traits carry the chunk.

**Why this story**: The Manifest must reach connected and late-joining clients intact and be built exactly once. Read first: R4 items 4-5, AC-32; ADR-0014 sections 8.1-8.4.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Clients build only from a complete `FLayoutManifest`; every layout-derived component is `bReplicates = false` (ADR-0014)
- Forbidden: Never replicate the Manifest as a single `TArray`, or send it by reliable RPCs (ADR-0014)
- Guardrail: Manifest 13.5-31.5 KB once per round (at most 4 chunks); zero layout traffic in `Active` (ADR-0014)

---

## Acceptance Criteria

*From GDD AC-32, R4.4 and ADR-0014 8.3, scoped to this story:*

- [ ] GIVEN a client that joins after `Published`, THEN it receives the same Manifest (not initial-only) and its ack succeeds (`Network.LateJoinManifest`)
- [ ] GIVEN a repeated or partial `OnRep_LayoutManifest` notify (missing chunk indices, or entry total != `EntryCount`), THEN the client build starts only once, only when chunks 0..N-1 are present and the total matches, keyed on `LayoutId` (`Manifest.OnRepIdempotent`)
- [ ] GIVEN the replicated-property allowlist audit (ADR-0003), THEN `FLayoutManifest` appears on `AStealthGameState` with audience All and no `COND_InitialOnly`, the audit fails if the row is removed, and each chunk serialises fixed-width 9-byte entries (`Replication.ManifestAllowlistRow`)

---

## Implementation Notes

*Derived from ADR-0014 section 8:*

- `FLayoutManifestChunk` (`ChunkIndex`, `TArray<FLayoutManifestEntry> Entries`, at most `LayoutManifestChunkEntriesMax` 1,024) with `NetSerialize` (fixed-width 9-byte entries, count-prefixed) and `Identical`; `TStructOpsTypeTraits` with `WithNetSerializer`/`WithIdentical`.
- Publish order on the server (`Selecting -> Published`): freeze sockets and selection, write `FLayoutManifest` on the GameState, write `LayoutAttempt` and `LayoutHash` into Session's `FRoundReadiness`, then broadcast `OnLayoutReady(LayoutId, Seed)`.
- `OnRep_LayoutManifest` starts a build only when indices `0..N-1` are all present and the entry total equals `EntryCount`; a repeat for the same `LayoutId` is ignored; a new `LayoutId` (reseed) starts a fresh build.
- A size assertion at publish: Manifest plus pending GameState payload at most 48 KB (spike story 010 measures; enforce here).
- Add the allowlist row for `FLayoutManifest` (audience All, carrier GameState) so the ADR-0003 audit passes; remove it in a test to prove the audit catches it.
- Graybox Manifests have 0 entries, so the late-join test also runs with a synthetic multi-chunk fixture.

---

## Out of Scope

- Story 010: loss/latency behaviour and wire-size measurement (spike)
- Story 013: the geometry build the notify triggers
- Story 024: the Preparing ack ordering

---

## Files

- `Source/StealthGameplay/Public/View/Core/StealthLayoutManifest.h` (chunk serializer)
- `Source/StealthGame/Public/StealthGameState.h` and `Private/StealthGameState.cpp` (property, `OnRep`, allowlist row; composition-root class)
- `Source/StealthGameplay/Private/Tests/Layout/ManifestReplicationTest.cpp`
- `Source/StealthTestHarness/Private/ManifestLateJoinTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Network.LateJoinManifest`
  - Given: a PIE host with Published state and a client joining afterwards.
  - When: the late client connects.
  - Then: it receives a Manifest with the same LayoutId and LayoutHash as the server's, builds, and its ack is accepted.
  - Edge cases: join during the same tick as Published; join while the client has 0 chunks.
- **AC-2** `Manifest.OnRepIdempotent`
  - Given: client state with chunks 0..N-1 delivered in permuted order and with a missing index.
  - When: `OnRep_LayoutManifest` fires repeatedly and with partial data.
  - Then: 0 builds while any index is missing or the entry total != EntryCount; exactly 1 build once complete and matching; repeats for the same LayoutId start 0 more builds.
  - Edge cases: a new LayoutId (reseed) starts a fresh build; EntryCount 0 (graybox).
- **AC-3** `Replication.ManifestAllowlistRow`
  - Given: the ADR-0003 allowlist audit.
  - When: it runs with and without the FLayoutManifest row.
  - Then: with the row, FLayoutManifest is on AStealthGameState, audience All, no COND_InitialOnly; without it, the audit fails; each chunk entry serialises to 9 bytes.
  - Edge cases: audience changed to Owner-only or InitialOnly fails.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Layout/ManifestReplicationTest.cpp` and `Source/StealthTestHarness/Private/ManifestLateJoinTest.cpp` — `StealthGame.Layout.Network.LateJoinManifest` (CQTest PIE, name `StealthGame.Layout.PIE.<Scenario>` per ADR-0008 exemption if CQTest is used), `StealthGame.Layout.Manifest.OnRepIdempotent`, `StealthGame.Layout.Replication.ManifestAllowlistRow` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-008-manifest-struct-and-layout-hash.md`
- Cross-epic: ADR-0003 allowlist audit and the ADR-0008 multi-client harness (`production/epics/tuning-data-config/EPIC.md`, `production/epics/session-travel/EPIC.md`)
- Unlocks: `production/epics/level-socket-contract/story-010-spike-manifest-wire-size-and-loss.md`, `production/epics/level-socket-contract/story-024-preparing-handshake-and-hash-ack.md`
