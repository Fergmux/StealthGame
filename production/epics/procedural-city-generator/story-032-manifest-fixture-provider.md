# Story 032: Manifest fixture provider

> **Epic**: Procedural City Generator
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25f
> **Last Updated**: not yet set (set by /dev-story)

## Context

**GDD**: `design/gdd/procedural-city-generator.md`
**Requirement**: `TR-citygen-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. The fixture provider lets the client instantiation, hash and ack path be tested before the Generator is finished.)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh (ADR-0008 for the CQTest PIE tier)
**ADR Decision Summary**: The contract's client path (`FLayoutGeometryBuilder`, `LayoutHash` recompute, Session's single ack) is provider-agnostic; a non-Shipping fixture provider that publishes a committed golden Manifest exercises it end to end with a real Manifest size and content.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM (ADR-0014; CQTest `FPIENetworkComponent` network tests are MEDIUM, ADR-0008)
**Engine Notes**: CQTest PIE network tests (`NETWORK_TEST_CLASS`, `WithGameMode(AStealthGameMode)`, listen server plus clients) per ADR-0008; the full test name is `StealthGame.CityGen.PIE.<Scenario>.<Method>`. The Shipping symbol probe is DEFERRED until a Shipping build exists.

**Control Manifest Rules (this layer)**:
- Required: T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map; T2/T3 tests bound by frames or server time — source: ADR-0008
- Forbidden: Never compile test seams or negative-control types into Shipping; never use wall-clock as a pass predicate — source: ADR-0008
- Guardrail: the fixture Manifest is a realistic size (a few thousand entries) so wire and build paths see real load

---

## Acceptance Criteria

*From GDD `design/gdd/procedural-city-generator.md` (AC-G25, R-G12.4), scoped to this story:*

- [ ] **AC1** (AC-G25, `Fixture.ManifestPath`): **GIVEN** the fixture provider with a committed golden Manifest **WHEN** a 2-client PIE runs **THEN** both clients build, hash, ack and reach `Active`.
- [ ] **AC2** (R-G12.4, `Fixture.NonShippingOnly`): **GIVEN** a Shipping build **THEN** the fixture provider symbols are absent, and the committed fixture's `LayoutHash` equals its committed constant.

---

## Implementation Notes

*Derived from GDD R-G12.4:*

`FManifestFixtureProvider : ILayoutProvider` (non-Shipping, behind `WITH_STEALTH_TEST_SEAMS` or in `StealthTestHarness`) returns a candidate built from a committed golden Manifest (generated once from a golden seed by the rebaseline tool, story 031, or hand-built until then) and a minimal socket set sufficient for the service to publish. It selects through `LayoutProviderMode` only in non-Shipping test configurations. The PIE test (`StealthGame.CityGen.PIE.ManifestFixture.TwoClientsReachActive`) asserts each client's recomputed hash equals the committed constant and the round reaches `Active` within a frame/server-time failure ceiling; a one-byte-corrupted fixture must not reach `Active` (recommended negative control). Use the LSC's client-builder tests (`level-socket-contract/story-013-client-geometry-builder-and-decoration.md`) as the unit-level proof and this as the integration proof.

Prototype replaced: none.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `story-033-cross-client-manifest-parity-under-loss.md`: parity under packet loss on Generator output.
- LSC chunked replication and the ack: `level-socket-contract/story-009-manifest-chunked-replication-late-join.md`, `story-024-preparing-handshake-and-hash-ack.md`.

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/CityGen/ManifestFixtureProvider.h`
- `Source/StealthTestHarness/Private/CityGen/ManifestFixtureProvider.cpp`
- `Source/StealthGameplay/Private/Tests/Data/CityGenFixtureManifest.generated.inl` (committed golden Manifest and its `LayoutHash` constant)
- `Source/StealthTestHarness/Private/CityGen/ManifestFixturePieTest.cpp` (CQTest PIE, `StealthGame.CityGen.PIE.ManifestFixture.TwoClientsReachActive`)
- `Source/StealthGameplay/Private/Tests/CityGen/FixtureNonShippingOnlyTest.cpp` (class `FCityGenFixtureNonShippingOnlyTest`, name `StealthGame.CityGen.Fixture.NonShippingOnly`)

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC1** `StealthGame.CityGen.PIE.ManifestFixture.TwoClientsReachActive`
  - Given: the fixture provider with the committed golden Manifest, and 2 clients.
  - When: the round starts.
  - Then: each client builds, its hash equals the committed `LayoutHash`, and it acks. The round reaches Active within the failure-ceiling timeout.
  - Edge cases: a one-byte corrupted fixture must not reach Active (recommended control).
- **AC2** `Fixture.NonShippingOnly`
  - Given: the fixture Manifest.
  - When: `LayoutHash` is computed (T1), and a Shipping binary is probed for symbols (T0).
  - Then: the hash equals the committed constant and the Shipping binary has 0 fixture-provider symbols.
  - Edge cases: the symbol probe is DEFERRED until a Shipping build exists.

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived** (advisory, never "must exist and pass"), but a Visual/Feel or UI story's retained screenshot is not.*

**Story Type**: Integration
**Required evidence**:
- Integration: the T2 PIE test and the T1 hash test OR a documented playtest

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/procedural-city-generator/story-031-golden-seeds-and-bump-rule.md`, `story-019-placement-bay-stacks-and-manifest-emission.md`; cross-epic `level-socket-contract/story-009-manifest-chunked-replication-late-join.md`, `story-013-client-geometry-builder-and-decoration.md`, `story-024-preparing-handshake-and-hash-ack.md`
- Unlocks: `story-033-cross-client-manifest-parity-under-loss.md`, `story-041-client-build-time-and-first-frame-completeness.md`
