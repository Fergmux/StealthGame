# Story 078: AC-12b RemoteLoopbackHash (multi-process)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-005`, `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-12b (CR-8)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7); the multi-process tier is ADR-0008 section 1 (T3)
**ADR Decision Summary**: The snapshot's `NetSerialize` path is only exercised when a real remote client receives it; the listen host reads it locally and never serialises. A loopback test with a real remote client therefore recomputes `ClientHash` from the received fields and compares it with the host's, exercising the wire format. T3 is a Gauntlet-launched cooked Development listen host plus client processes running a named harness scenario that writes a result JSON the node asserts.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Depends on the multi-process smoke of story 020 and so on ADR-0008 Verification 3 [U] (Gauntlet launches one cooked Development listen host plus N clients on one machine; Gauntlet ships with 5.8 [V], the topology is [U]) and Verification 7 [U] (whether `WITH_DEV_AUTOMATION_TESTS` is defined in cooked Development game builds; the seam types here compile under `WITH_STEALTH_TEST_SEAMS`, which is `!UE_BUILD_SHIPPING`). Also rests on ADR-0004 Verification Required (1) [U] (legacy atomic delivery, story 071). The packet-simulation profile applies with `-PktEmulationProfile=<Name>` [V switch]; `PktLag` semantics are [U].

**Control Manifest Rules (this layer)**:
- Forbidden: never prove multi-process criteria in in-process PIE only; never select tiers with `Automation RunFilter` — source: ADR-0008
- Required: T2/T3 tests are bounded by frames or server time and assert and record the active replication system (result JSON `replicationSystem`) — source: ADR-0008
- Required: T2 and T3 code lives in `StealthTestHarness`; T3 names are `StealthGame.<System>.MP.<Scenario>` — source: ADR-0008
- Required: every leak and parity gate includes a negative control the suite detects — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-12b, scoped to this story:*

- [ ] **AC-12b (CR-8)**: GIVEN a listen host and one remote multi-process client, WHEN the snapshot replicates, THEN the remote client's recomputed `ClientHash` equals the host's. This loopback test exercises serialisation, which the host path never does. (`StealthGame.Tuning.MP.RemoteLoopbackHash`)

---

## Implementation Notes

*Derived from ADR-0004 sections 5 and 7, ADR-0008 sections 1, 2, 5 and Key Interfaces (harness scenario entry and result JSON), and global amendments G1, G4:*

- **Scenario.** `Source/StealthTestHarness/Private/Scenarios/RemoteLoopbackHashScenario.cpp`: a named harness scenario run with `-StealthHarnessScenario=RemoteLoopbackHash -StealthHarnessOut=<path>`. The host process runs the stub Round and the TDC pipeline, writes the snapshot, and records the host's `ClientHash`; the remote client process recomputes `ClientHash` from the fields it received and records it. Both write the result JSON `{scenario, build, replicationSystem, netProfile, passed, negativeControlDetected, faults[], captures[]}` (story 018), the hashes appear in `captures[]`, and the node reads the hash through the result JSON captures (QA spec).
- **Gauntlet node (open).** The plan lists only the scenario file for this story, while sibling multi-process stories list their own node scripts (063, 064, 080 name `Build/Scripts/StealthTuning*Node.cs`). Whether this scenario is driven by the generic node of story 020 (`Build/Scripts/StealthMultiProcessSmoke.cs`, `-StealthHarnessScenario=<Name>`) or needs its own node is not stated: open, record at implementation. The test name `StealthGame.Tuning.MP.RemoteLoopbackHash` follows the T3 convention `StealthGame.<System>.MP.<Scenario>`. Build/ must not be ignored by `.gitignore` (story 024).
- **Replication system.** Both result files carry `replicationSystem=legacy`, and the node asserts it (stories 018 and 020).
- **Hash comparison.** Compare as unsigned 64-bit values serialised in the JSON without loss (for example as a hex string); the JSON field format is not fixed by the inputs: open. The host's value comes from the store (`GetServerClientHash(RoundIndex, Revision)`) and the client's from `IStealthClientTuning::GetClientHash()`, recomputed from received fields.
- **Negative control (QA edge case).** A corrupted received field must make the client hash differ. The mechanism is not specified and story 072's hook is not in this story's dependency list although it can corrupt a received field and exists in cooked Development (`WITH_STEALTH_TEST_SEAMS` is `!UE_BUILD_SHIPPING`): open. If the hook is used, depend on story 072 and record it; do not weaken the control. The scenario sets `negativeControlDetected` in the result.
- **Profile.** Run once under `StealthClean` and once under `StealthLeak2` (profiles from story 017, applied with `-PktEmulationProfile=<Name>`); the hash must match in both. Whether the plan requires the loss run is not stated for this AC: open, the clean run is the minimum.
- **Bounds.** The scenario is bounded by frames or server time; wall clock is only a failure ceiling for a hung process.
- **Fallback and deferral.** The QA spec marks this DEFERRED: multi-process, so it runs in the nightly job once story 020 is Done. It is not proven in single-process PIE.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the in-process 2-client equality of `ClientHash` and the public-only checks
- `production/epics/tuning-data-config/story-077-host-path.md`: the host's own no-RPC path
- `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`: the Gauntlet host and client smoke and the `replicationSystem` assertion
- `production/epics/tuning-data-config/story-079-self-check-fault.md`: the server self-check round trip
- `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`: atomic delivery under loss

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Scenarios/RemoteLoopbackHashScenario.cpp`
- `Build/Scripts/` node script, only if the story-020 node cannot drive this scenario (open)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the remote client's recomputed `ClientHash` equals the host's
  - Given: a listen host plus one remote client process (DEFERRED: multi-process; test `StealthGame.Tuning.MP.RemoteLoopbackHash`)
  - When: the snapshot replicates
  - Then: the remote client's recomputed `ClientHash` equals the host's
  - Edge cases: the hash is read through the result JSON captures; a corrupted field makes it differ (control)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the T3 scenario `Source/StealthTestHarness/Private/Scenarios/RemoteLoopbackHashScenario.cpp` (`StealthGame.Tuning.MP.RemoteLoopbackHash`) run by the Gauntlet node must exist and pass (nightly, deferred multi-process), with its result JSON retained

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-020-gauntlet-multiprocess-smoke.md`
- Unlocks: None
- Open verification claims: ADR-0008 Verification 3 (Gauntlet one-machine topology) [U] and Verification 7 (`WITH_DEV_AUTOMATION_TESTS` in cooked Development) [U], both settled by story 020 or later; ADR-0004 Verification Required (1) [U] (story 071). Open: node reuse, the JSON hash format, the corruption mechanism, and whether the loss profile is required.
