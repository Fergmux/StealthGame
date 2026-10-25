# Story 077: AC-12a HostPath

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-011` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-12a (CR-9, CR-10)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7, Snapshot replication)
**ADR Decision Summary**: The listen-server host is both server and client but receives no RepNotify for the property it just wrote, so the service calls the client-side handler directly after the server write; the host is marked tuning-Ready locally through the same accessor path, with no RPC. A loopback test with a real remote client covers serialisation, which the host path never exercises.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Rests on the snapshot property of story 073 (ADR-0004 Verification Required (1) [U], story 071) and CQTest PIE (ADR-0008 Verification 1 [U], story 019). The "no reliable RPC" assertion needs the reliable-RPC counter of story 100, which carries its own positive control against a real PIE connection; whether that counter works under headless CQTest is therefore also gated by story 019.

**Control Manifest Rules (this layer)**:
- Required: server code reads only `GetServerTuning()`/`FValidatedTuning`; client and presentation code reads only `GetClientTuning()` (the host's client-side code uses the snapshot path too, CR-10) — source: ADR-0004
- Forbidden: never deliver the tuning snapshot by reliable client RPC — source: ADR-0004
- Required: T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map; T2/T3 are bounded by frames or server time — source: ADR-0008
- Forbidden: never use wall-clock time as a test pass/fail predicate — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-12a, scoped to this story:*

- [ ] **AC-12a (CR-9, CR-10)**: GIVEN a listen-server host, WHEN `Preparing` runs, THEN the host is tuning-`Ready` through the accessor path without any RPC and reads public values identical to the snapshot. (`StealthGame.Tuning.PIE.HostPath`)

*Per amendment (story 077 REVISED): "without any RPC" is asserted with the reliable-RPC counter of story 100.*

---

## Implementation Notes

*Derived from ADR-0004 section 7, GDD CR-9, CR-10 and Edge Cases (listen host), ADR-0008 sections 1-2, and global amendments G1, G4 (fallback list):*

- **Production code.** `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`: after the server writes `TuningSnapshot` on the GameState (story 073) the service invokes the same handler the client's `OnRep_TuningSnapshot` uses, so the host recomputes `ClientHash` and reaches tuning-Ready through the accessor path with no RPC and no RepNotify. `GetClientTuning()` on the host reads the snapshot from the GameState (CR-10: the host's client-side code uses the snapshot path too); it is never a shortcut to the frozen server set.
- **Test.** `NETWORK_TEST_CLASS(HostPath, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/HostPathTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST`; full name `StealthGame.Tuning.PIE.HostPath.<Method>`. `AsListenServer().WithClients(N).WithGameMode(AStealthGameMode::StaticClass())`, fixture map in the first step, bounded by frames or server time. Steps: run `Preparing` against the stub Round (story 052); assert the host is tuning-Ready; read the host's reliable-RPC counter (`FReliableRpcCounter`, story 100, `Source/StealthFoundation/Public/Server/Testing/StealthReliableRpcCounter.h`, G1 path) on the host controller and assert zero reliable RPCs; compare each public value read through `GetClientTuning()` on the host with the replicated snapshot and assert equality.
- **Counter must be proven on a real connection.** The QA spec requires that the counter be shown to count on a real connection; that proof is story 100's positive control (`StealthGame.Harness.PIE.RpcCounterCountsRealRpc`, count exactly 1 for a known reliable RPC). Do not duplicate it here; depend on it. Story 100 is a dependency per the amendments (the plan lists 073; amendments add 100).
- **What this test does not cover.** The host path never exercises serialisation: `FNetBitWriter` round-trip equality is story 078 (remote multi-process client) and the self-check of story 079. The host is not a remote client, so a host-ready result says nothing about the wire format.
- **Fallback.** If story 019 closes FAIL (T2 not headless), this story runs in a non-headless editor on the dev machine per G4 and the revised story 019.
- **Seams.** `FStubRound` (story 052, G1 path), fault sink (story 008) with zero unexpected faults at teardown.
- **Replication system.** The T2 run asserts and records that the game driver is legacy (story 018).
- **Prototype.** None. The plan lists `FND/Private/Tuning/StealthTuningService.cpp` as the production file.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated property and public-only checks
- `production/epics/tuning-data-config/story-074-client-hash-handoff.md`: remote clients' hand-off of `ClientHash`
- `production/epics/tuning-data-config/story-078-remote-loopback-hash.md`: serialisation across a real remote client
- `production/epics/tuning-data-config/story-100-rpc-counter-and-property-crc-helper.md`: the reliable-RPC counter and its real-connection positive control
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the Round and Session stubs

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/HostPathTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the host is tuning-Ready through the accessor with zero reliable RPCs and reads public values equal to the snapshot
  - Given: a listen-server host and `FReliableRpcCounter` (story 100) (`NETWORK_TEST_CLASS(HostPath, "StealthGame.Tuning.PIE")`)
  - When: `Preparing` runs
  - Then: the host is marked tuning-Ready through the accessor; the counter reads zero reliable RPCs; host public values equal the snapshot
  - Edge cases: the counter must be shown to count on a real connection (see story 100); the host path never exercises serialisation (story 078)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/HostPathTest.cpp` (CQTest `StealthGame.Tuning.PIE.HostPath`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-100-rpc-counter-and-property-crc-helper.md`
- Unlocks: None
- Open verification claims: ADR-0004 Verification Required (1) [U] and ADR-0008 Verification 1 [U] are assumed settled by stories 071 and 019 (story 100 depends on 019). No plan Note line applies to 077.
