# Story 071: Spike T-1: atomic snapshot delivery under loss and for a rejoiner

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`); GDD CR-8 (the snapshot must arrive as one atomic unit) and ADR-0004 Validation Criteria (`SnapshotAtomic`)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7, Verification Required (1), Validation Criteria)
**ADR Decision Summary**: The snapshot is a single custom-`NetSerialize` struct on the GameState that is sent as one unit and re-sent on change; at about 2 KB it travels as partial bunches, and losing one partial drops that update and relies on the legacy resend. Spike T-1 measures that behaviour under `LeakNetProfile` loss, and the ADR records the outcome as an amendment.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: This story IS ADR-0004 Verification Required (1): a ~2 KB `NetSerialize` struct on `AGameStateBase` replicates atomically and re-sends on change under the legacy driver, including to a rejoiner. Unverified [U] claims it settles: the legacy property replicator compares and sends the custom-serialised struct as one unit, and losing one partial bunch is recovered by the legacy resend. Verified [V] background: custom-`NetSerialize` structs are sent as one unit (`RepLayout.cpp:5720, 5858-5860`); partial bunches have a 64 KB cap (`DataChannel.cpp:105`). It also depends on ADR-0008 Verification 1 [U] (CQTest PIE headless, story 019) and on packet-simulation semantics [U]: `PktLag` one-way on the outgoing side, `LoadEmulationProfile` name [U] (ADR-0008 section 5). The PIE application of the profile must be recorded in this story (QA spec).

**Control Manifest Rules (this layer)**:
- Forbidden: never deliver the tuning snapshot by reliable client RPC (the fallback for a failed spike is a periodic whole-struct resend, not an RPC) — source: ADR-0004
- Forbidden: never disable, skip or reseed a failing test to pass a gate; never mark a gate Done that was never watched to fail — source: ADR-0008
- Required: T2/T3 tests are bounded by frames or server time and assert and record the active replication system; T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map — source: ADR-0008
- Engine API constraint: spikes owning [U] claims include T-1 snapshot atomicity (ADR-0004) — source: ADR-0004

---

## Acceptance Criteria

*From ADR-0004 Validation Criteria (`StealthGame.Tuning.SnapshotAtomic`), as amended (G4), scoped to this story:*

- [ ] with the hold-back hook (072) releasing mid-update under the `StealthLeak2` profile, the client's applied snapshot at every observed tick equals either the complete old value vector or the complete new one (never a mixture of old and new public keys), and a rejoiner receives the current Revision; per G4 a FAIL closes with an ADR-0004 amendment request and the fallback "deliver as a periodic whole-struct resend", never by skipping.

---

## Implementation Notes

*Derived from ADR-0004 section 7 and Validation Criteria, ADR-0008 sections 2 and 5, and global amendment G4 (spike stories):*

- **Test.** `NETWORK_TEST_CLASS(SnapshotAtomic, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/SnapshotAtomicTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST`. The full name is `StealthGame.Tuning.PIE.SnapshotAtomic.<Method>` (CQTest appends the method; the AC text names it `StealthGame.Tuning.PIE.SnapshotAtomic`). T2 classes are exempt from the `F...Test` class-name rule. Flags come from the macro (`EditorContext | ProductFilter`). Shape: `AsListenServer().WithClients(N).WithGameMode(AStealthGameMode::StaticClass()).WithPacketSimulationSettings(...)`; the first step loads the fixture map.
- **Method.** Build an old value vector and a new one (differing in several public keys, including keys serialised early and late in schema order). Use the hold-back hook of story 072 to hold delivery to the client, update the server value (Revision + 1), and release mid-update under `StealthLeak2`. Observe the client's applied snapshot every tick (server-time or frame bounded) and compare each observation with the old and new vectors by `operator==` or `ClientHash` (never field by field only, so a mixture is detected). Then add a rejoiner (a client that connects after the updates) and assert it receives the current `Revision`.
- **Profile.** `StealthLeak2` is `PktLag 100`, `PktLoss 2` (story 017). T2 loads the same profile into `FPacketSimulationSettings` (`LoadEmulationProfile` [U name]) and passes it to `WithPacketSimulationSettings`. The QA spec asks that how the profile was applied in PIE be recorded: write it into the outcome record. Story 017 is not in this story's dependency list although the profile comes from it: flag; if the profile does not yet exist the story is not runnable as specified.
- **Carrier is open.** The plan names no replicated carrier for the spike and no dependency on story 073 (the `AStealthGameState` property). Options: a harness-only replicated actor holding an `FStealthTuningSnapshot` with the same `ReplicatedUsing`/`COND_None` shape, or the GameState once story 073 exists. Decide at implementation and record the choice in the outcome note; do not assume it.
- **Rejoiner mechanics are open.** The inputs do not say how a rejoiner is produced inside single-process PIE (a client added after build, or a disconnect and reconnect). The full Session rejoin path is story 080 (multi-process); here a rejoiner is a client that joins after updates have already happened.
- **G4 closure (spike).** PASS: record the observation (command, date, profile, number of updates observed, no mixture seen) in the story as an ADR-0004 amendment note, as the plan states ("the measured result under the `StealthLeak2` profile is recorded in the story as an ADR-0004 amendment note"). FAIL: record the failing observation, file an ADR-0004 amendment request in this story naming the fallback "deliver as a periodic whole-struct resend", and close FAIL. The test is never skipped, disabled or `#if`-ed out to close the story, and the fallback is never a reliable client RPC.
- **Fallback inheritance.** If story 019 closes FAIL (T2 not headless), this story runs in a non-headless editor on the dev machine (the fallback named in G4 and the revised story 019).
- **Outcome record (filled by `/dev-story`).** Verification Required (1) result: `PASS | FAIL`, date, command line, profile application, carrier chosen, amendment note or request reference.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`: the struct and `NetSerialize` under test
- `production/epics/tuning-data-config/story-072-snapshot-holdback-test-hook.md`: the hold-back and arrival-order hook this spike uses
- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the real GameState property, public-only checks and `ClientHash` equality
- `production/epics/tuning-data-config/story-078-remote-loopback-hash.md`: serialisation across real processes
- `production/epics/tuning-data-config/story-080-rejoin-snapshot.md`: the multi-process rejoin through Session's window
- `production/epics/tuning-data-config/story-017-net-emulation-profiles.md`: the packet-simulation profiles

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/SnapshotAtomicTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the client's applied snapshot is always the whole old or the whole new vector, and a rejoiner receives the current Revision
  - Given: the hold-back hook (072) releasing mid-update, the `StealthLeak2` profile (record how it was applied in PIE), a client, then a rejoiner
  - When: updates run with the release mid-update
  - Then: the client's applied snapshot at every observed tick equals the complete old or complete new value vector (compare `ClientHash` or `operator==`), never a mixture; the rejoiner receives the current Revision
  - Edge cases: FAIL closes with an ADR-0004 amendment request and the fallback "periodic whole-struct resend", never by skipping

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/SnapshotAtomicTest.cpp` (CQTest `StealthGame.Tuning.PIE.SnapshotAtomic`) must exist and pass, OR the recorded FAIL plus ADR-0004 amendment request per G4 (the Verification Required (1) outcome note is in this story in both cases)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`, `production/epics/tuning-data-config/story-072-snapshot-holdback-test-hook.md`
- Unlocks: None (no story's dependency list names 071)
- Open verification claims: spike T-1 is the [U] verification of the legacy driver's atomic custom-`NetSerialize` struct and partial-bunch resend (plan Note); it is exactly what this story settles. Also open: `PktLag` semantics [U] and the `LoadEmulationProfile` name [U] (ADR-0008 section 5); the carrier actor; the rejoiner mechanics; the missing dependency on story 017.
