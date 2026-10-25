# Story 079: AC-12c SelfCheckFault

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-005` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-12c (CR-9)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 5, Freeze, hashes, self-check)
**ADR Decision Summary**: After freezing, the server serialises the `FStealthTuningSnapshot` it is about to replicate through the same `NetSerialize` into an `FNetBitWriter`, reads it back into a fresh struct, recomputes `ClientHash` and compares. A difference raises `TuningSnapshotFault`, the pipeline enters `Failed`, Round returns to Lobby, and no client is disconnected. This is the only server-fault path.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: `FName` serialises without a package map [V `CoreNet.cpp:430-438`], so `FNetBitWriter(nullptr, ...)` is a valid round-trip carrier for the self-check. The behaviour of the legacy replicator itself is not exercised (this is a plain-C++ check, headless under `-nullrhi`). The snapshot struct is story 070.

**Control Manifest Rules (this layer)**:
- Forbidden: never compile negative-control types or test seams into Shipping (`StealthTestHarness` or `WITH_STEALTH_TEST_SEAMS` only); the forcing hook and the connection recorder are seams — source: ADR-0008
- Required: every Logic story ships T1 tests; T1 tests live in the owning module's `Private/Tests/<System>/` — source: ADR-0008
- Forbidden: never disable, skip or reseed a failing test to pass a gate — source: ADR-0008
- Required: new code logs with `UE_LOGF` and a `LogStealth...` category (`LogStealthFoundation`) — source: ADR-0007

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-12c, scoped to this story:*

- [ ] **AC-12c (CR-9)**: GIVEN the server self-check is forced to see a differing serialised struct (test hook), WHEN freezing, THEN `TuningSnapshotFault` is logged, the pipeline is `Failed`, the Round stub receives `Failed`, and no client is disconnected. (`StealthGame.Tuning.SelfCheckFault`)

*Per amendment (story 079 REVISED): uses the self-check forcing hook and disconnect recorder of story 099.*

---

## Implementation Notes

*Derived from ADR-0004 sections 5-7 and Key Interfaces, GDD CR-9 and Edge Cases, ADR-0008 sections 2 and 4, and global amendment G1:*

- **Production code.** `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`: at freeze, write the snapshot with `NetSerialize` into an `FNetBitWriter(nullptr, ...)`, read it back with an `FNetBitReader` into a fresh `FStealthTuningSnapshot`, recompute `ClientHash` from the read-back public values, and compare with the hash of the validated set. A difference logs `TuningSnapshotFault` with `UE_LOGF(LogStealthFoundation, Error, ...)`, moves the pipeline to `Failed`, raises `OnTuningFailed(ETuningFailReason::SnapshotFault)`, and writes nothing to the GameState snapshot property. No connection is touched: the server never disconnects or kicks on a self-check fault (GDD Edge Cases).
- **Forcing hook (story 099).** `Source/StealthFoundation/Public/Server/Testing/StealthSelfCheckHook.h` (G1 path, under `WITH_STEALTH_TEST_SEAMS`) makes the self-check see a differing serialised struct at freeze. This story consumes the hook; it does not define it.
- **Disconnect recorder (story 099).** `FConnectionEventRecorder` (`Source/StealthFoundation/Public/Server/Testing/StealthConnectionEventRecorder.h`) records every disconnect and kick on a fake connection set; the test asserts it holds zero disconnects or kicks.
- **Round stub.** `FStubRound` (story 052) receives `OnTuningFailed`; the test asserts it received `Failed` with the snapshot-fault reason. In this epic "Round returns to Lobby" is Round's behaviour and is not tested here.
- **Log capture (story 009).** `FStealthLogCapture` captures `LogStealthFoundation` and the test asserts `TuningSnapshotFault` appears (exactly once for the forced fault).
- **Test.** `Source/StealthFoundation/Private/Tests/Tuning/SelfCheckFaultTest.cpp`, class `FTuningSelfCheckFaultTest`, name `StealthGame.Tuning.SelfCheckFault`, flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. Steps: arrange a frozen set; force the self-check; freeze; assert (1) `TuningSnapshotFault` captured, (2) pipeline state `Failed`, (3) `FStubRound` got `Failed`, (4) recorder holds zero disconnects or kicks, (5) no snapshot was written to the GameState (QA edge case; a fake or test GameState target as the write sink). Include the control: with the hook not forced, the same freeze completes, the pipeline is Frozen and the snapshot is written (otherwise the failure assertions prove nothing).
- **Dependency gaps to flag.** The plan lists `StealthTuningService.cpp` as the production file and the test needs a pipeline that can reach `Failed` and notify a Round stub, which come from the pipeline core (stories 050-053) and the service (story 066). The dependency list is 070, 052, 099 only. Open: which of these must already exist; record the actual prerequisites at implementation and add them to this story's dependencies if they are needed. The snapshot write that the self-check gates is story 073.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-099-selfcheck-and-disconnect-seams.md`: the forcing hook and the connection event recorder
- `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`: the struct and its `NetSerialize`
- `production/epics/tuning-data-config/story-053-fallback-failed.md`: the load-failure path (`TuningLoadFailed`), not the snapshot fault
- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: writing the snapshot to the GameState
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the Round stub

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/SelfCheckFaultTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a forced self-check difference logs `TuningSnapshotFault`, fails the pipeline, notifies the Round stub and disconnects nobody
  - Given: the self-check forcing hook and `FConnectionEventRecorder` (story 099), `FStubRound`, log capture (test `StealthGame.Tuning.SelfCheckFault`)
  - When: freezing with the self-check forced to see a differing serialised struct
  - Then: `TuningSnapshotFault` logged; pipeline `Failed`; the stub receives `Failed`; the recorder holds zero disconnects or kicks
  - Edge cases: no snapshot written to the GameState

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/SelfCheckFaultTest.cpp` (`StealthGame.Tuning.SelfCheckFault`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`, `production/epics/tuning-data-config/story-099-selfcheck-and-disconnect-seams.md`
- Unlocks: None
- Open verification claims: none from plan Note lines (the plan has none for 079). Open: the actual prerequisites in the pipeline and service for the `Failed` path (see Implementation Notes).
