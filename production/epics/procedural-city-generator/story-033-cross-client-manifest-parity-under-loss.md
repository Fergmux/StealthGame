# Story 033: Cross-client Manifest parity under loss

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0008 for net emulation)
**ADR Decision Summary**: Every client builds from the same Manifest and must report the same `LayoutHash` (LSC D7); the Manifest travels as chunked bunches, clients recompute the hash after building and send it in Session's single ack; a client `GeneratorKitVersion` mismatch is resolved by LSC (resend once, then `ParamsMismatch`) and the server never regenerates.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; net-emulation semantics under the legacy NetDriver are partly [U], ADR-0008)
**Engine Notes**: Packet simulation per ADR-0008: `FNetworkComponentBuilder::WithPacketSimulationSettings` in CQTest, or the `-PktLag/-PktLoss/-PktEmulationProfile` command line; lag applies to each side's outgoing packets [U semantics], so state whether "200 ms" is one-way `PktLag` or a round trip (assumed one-way `PktLag` 200 on the affected side). Loss may slow delivery but must never change the hash.

**Control Manifest Rules (this layer)**:
- Required: Clients build only from a complete `FLayoutManifest` and recompute `LayoutHash` — source: ADR-0014
- Forbidden: Never replicate the Manifest as a single `TArray` or by reliable RPCs; never regenerate for a client mismatch — source: ADR-0014
- Guardrail: Manifest <= 4 chunks, 13.5-31.5 KB once per round; plus the pending GameState payload stays <= 48 KB (ADR-0014 R-4)

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G24, R-G11.1) and LSC R4.7, scoped to this story:*

- [ ] **AC1** (AC-G24, `Determinism.CrossClientManifest`): **GIVEN** a 3-client PIE on 3 golden seeds (one under 10% loss and 200 ms emulation) **THEN** every client reports the server's `LayoutHash` and an identical collision hash.
- [ ] **AC2** (LSC R4.7, `Determinism.MismatchNegativeControl`): **GIVEN** a forced `GeneratorKitVersion` mismatch on one client **THEN** LSC resends once then reports `ParamsMismatch`, and the server never regenerates.

---

## Implementation Notes

*Derived from GDD R-G11.1 and LSC R4.7:*

T2 CQTest PIE in `StealthTestHarness` (`StealthGame.CityGen.PIE.CrossClientManifest.HashesEqualUnderLoss` and `...MismatchNegativeControl`): listen server plus 3 clients on 3 golden seeds; on one seed one client runs with `PktLoss` 10% and `PktLag` 200. Each client reports `GetLocalLayoutHash()` (the `IStealthLayoutClientView` interface) and a collision hash (a hash of the built collision body setups, computed by a non-Shipping test hook); both must equal the server's on every seed. The mismatch control bumps `GeneratorKitVersion` by 1 on client 2: the LSC resends exactly once, then reports `ParamsMismatch` for client 2; the Generator's `Build` call count on the server stays 1; other clients are unaffected. The overlap with story 025 AC3 (a T1 plain-core check of the same rule) is deliberate: that one proves the Generator side with a fake service, this one proves it end to end.

Prototype replaced: the prototype's clients each ran the generator from the replicated seed; clients now build only from the Manifest.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-030-pure-function-determinism-and-stage-hashes.md`: server-side determinism.
- `story-032-manifest-fixture-provider.md`: the fixture-provider path.
- LSC Manifest replication and the loss spike: `level-socket-contract/story-009-manifest-chunked-replication-late-join.md`, `story-010-spike-manifest-wire-size-and-loss.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/CrossClientManifestPieTest.cpp` (CQTest PIE, two test methods)
- `Source/StealthGameplay/Private/Features/CityGenerator/CityGenCollisionHash.cpp` (non-Shipping collision hash hook)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `StealthGame.CityGen.PIE.CrossClientManifest.HashesEqualUnderLoss`
  - Given: 3 clients on 3 golden seeds. On one seed, one client has PktLoss 10% and 200 ms lag via `WithPacketSimulationSettings`.
  - When: each seed's round reaches Active.
  - Then: every client reports the server's `LayoutHash` and an identical collision hash, on every seed.
  - Edge cases: "200 ms" is stated as one-way `PktLag` for the affected side. Loss may slow delivery but must never change the hash.
- **AC2** `StealthGame.CityGen.PIE.CrossClientManifest.MismatchNegativeControl`
  - Given: client 2's `GeneratorKitVersion` forced to +1.
  - When: the Manifest is delivered.
  - Then: LSC resends exactly once, then reports `ParamsMismatch` for client 2. The server's `Build` count stays 1. The other clients are unaffected.
  - Edge cases: the overlap with story 025 AC3 is deliberate (T1 versus T2).

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the T2 PIE test file (two methods) OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-030-pure-function-determinism-and-stage-hashes.md`, `story-031-golden-seeds-and-bump-rule.md`, `story-032-manifest-fixture-provider.md`, `story-026-server-kit-spawn-from-manifest.md`; cross-epic `level-socket-contract/story-009-manifest-chunked-replication-late-join.md`, `story-024-preparing-handshake-and-hash-ack.md`, `tuning-data-config/story-017-net-emulation-profiles.md`
- Unlocks: `story-046-ten-connection-gauntlet-integration-pass.md`
