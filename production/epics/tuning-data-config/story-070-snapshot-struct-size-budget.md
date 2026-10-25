# Story 070: AC-19 SnapshotSizeBudget: FStealthTuningSnapshot and NetSerialize

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-19 (F-4 snapshot size budget, CR-8)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7, Snapshot replication)
**ADR Decision Summary**: The public half of a frozen tuning set replicates as one atomic struct, `FStealthTuningSnapshot`, with a custom `NetSerialize` that writes a header and then every public field in schema-table order at full precision. It declares `WithNetSerializer` and `WithIdenticalViaEquality` so the legacy change check sends it as one unit and a changed `Revision` triggers a resend.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0004 rates the snapshot's atomic replication MEDIUM (it uses only the legacy driver's long-standing custom `NetSerialize` path). Verified [V]: `TStructOpsTypeTraits<>::WithNetSerializer` drives custom struct serialisation (`Class.h:1221`, `:5670`); a custom-serialised struct is sent as one unit, not field by field (`RepLayout.cpp:5720, 5858-5860`); without native identity support the legacy change check compares only `UPROPERTY` members (`Class.cpp:3672-3692`); `FName` serialises without a package map (`CoreNet.cpp:430-438`), so an `FNetBitWriter(nullptr, ...)` round trip is valid; partial bunches have a 64 KB cap (`DataChannel.cpp:105`). Unverified [U]: that the legacy replicator compares and sends the ~2 KB struct as one unit and re-sends it after a lost partial bunch. That is ADR-0004 Verification Required (1), settled by spike story 071, not by this story: this story proves size and round trip only.

**Control Manifest Rules (this layer)**:
- Guardrail: snapshot at most 2,048 bytes per client per round; validation under 1 ms at `Preparing`; under 100 KB memory — source: ADR-0004
- Forbidden: never deliver the tuning snapshot by reliable client RPC — source: ADR-0004
- Required: every header in a `Public/` folder sits under `View/` (client-safe) or `Server/`; the snapshot is a `View/` type — source: ADR-0007
- Required: every gameplay number is a field of a `_Public` or `_Server` tuning sub-struct; the snapshot carries only the `_Public` face — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-19, scoped to this story:*

- [ ] **AC-19 (F-4)**: GIVEN the public snapshot struct with a maximum-length `PresetId`, WHEN serialised through the replication path in an automation test, THEN it is at most 16,384 bits (2,048 bytes) with every registered public key present. A net-profile capture of a 10-client join is advisory evidence only. (`StealthGame.Tuning.SnapshotSizeBudget`)

---

## Implementation Notes

*Derived from ADR-0004 section 7 and Key Interfaces, ADR-0008 sections 2 and 4:*

- **Struct.** `Source/StealthFoundation/Public/View/StealthTuningSnapshot.h`: `USTRUCT() struct FStealthTuningSnapshot { FStealthClientTuning Values; int32 SchemaVersion; FName PresetId; int32 RoundIndex; int32 Revision; bool bValid; }`, every member a `UPROPERTY`, plus `bool NetSerialize(FArchive&, UPackageMap*, bool& bOutSuccess)` and `bool operator==(const FStealthTuningSnapshot&) const` over every member. `TStructOpsTypeTraits<FStealthTuningSnapshot>` sets `WithNetSerializer = true` and `WithIdenticalViaEquality = true` (the second is required: the legacy change check calls `Identical`, and without it a bare `Revision` change would not trigger a resend). It lives in `Public/View/` because the client face must be able to name it (ADR-0007 section 2); it never includes a `Server/` header.
- **NetSerialize.** Writes the header (`SchemaVersion`, `PresetId`, `RoundIndex`, `Revision`, `bValid`) then every public field in schema-table order at full precision: floats as 32-bit, ints as 32-bit, enums as their declared `uint8`. No quantisation (CR-8). `PresetId` is an `FName` of at most 16 characters; this resolves the GDD F-4 wording "a catalog name of at most 16 bytes, or an index": the ADR fixes `FName`, at most 16 characters, so the QA spec's "PresetId or index choice must be fixed" is answered by ADR-0004 section 7.
- **Implementation file.** `Source/StealthFoundation/Private/Tuning/StealthTuningSnapshot.cpp` holds `NetSerialize` and `operator==`.
- **Dependency gap to flag.** "Schema-table order" and "every registered public key" come from `GetTuningSchema()` and the generated table (stories 038 and 041), but this story's plan dependency list is story 040 only. Until the generated table exists the schema is empty under `STEALTH_TUNING_SCHEMA_BOOTSTRAP`, so the order source is open: use the table once stories 038/041 are Done, and do not invent a second ordering. Resolve at implementation and record it here.
- **Test.** `Source/StealthFoundation/Private/Tests/Tuning/SnapshotSizeBudgetTest.cpp`, class `FTuningSnapshotSizeBudgetTest`, name `StealthGame.Tuning.SnapshotSizeBudget`, flags `EditorContext | ProductFilter` inside `#if WITH_DEV_AUTOMATION_TESTS`. Steps: build a snapshot with a 16-character `PresetId`, set every public key found in the schema (not a hard-coded count), serialise through an `FNetBitWriter` (null package map), assert the bit count is at most 16,384; read it back through an `FNetBitReader` into a fresh struct and assert `==` the original. Also assert `operator==` is sensitive to a one-field change (so `WithIdenticalViaEquality` is meaningful). Log the measured bit count with `UE_LOGF(LogStealthFoundation, ...)` as advisory output.
- **Re-run as owners register (spec edge case).** Only keys registered at the time are counted, so the test must derive the key set from the schema at run time and keep passing as owner epics register their public keys (the GDD expects about 150 public keys and about 600 bytes of values). It must never assert a fixed key count.
- **Advisory only.** The GDD's net-profile capture of a 10-client join is advisory evidence, not part of this story's pass condition.
- **No RPC.** The snapshot is delivered by property replication (story 073), never by a reliable client RPC.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: replicating the struct on `AStealthGameState` (`ReplicatedUsing`, `COND_None`) and the public-only checks
- `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`: spike T-1, atomic delivery under loss and for a rejoiner (ADR-0004 Verification Required (1))
- `production/epics/tuning-data-config/story-072-snapshot-holdback-test-hook.md`: the hold-back and arrival-order test hook
- `production/epics/tuning-data-config/story-079-self-check-fault.md`: the server self-check round trip and `TuningSnapshotFault`
- `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`: `FStealthClientTuning` and `FStealthServerTuning` (this story consumes the client face)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Public/View/StealthTuningSnapshot.h`
- `Source/StealthFoundation/Private/Tuning/StealthTuningSnapshot.cpp`
- `Source/StealthFoundation/Private/Tests/Tuning/SnapshotSizeBudgetTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the public snapshot struct with a maximum-length `PresetId` serialises to at most 16,384 bits with every registered public key present
  - Given: public snapshot with `PresetId` at maximum length (at most 16 characters) and every registered public key present
  - When: serialised via `FNetBitWriter`, then a `NetSerialize` round trip
  - Then: at most 16,384 bits (2,048 bytes); the round-tripped struct equals the original
  - Edge cases: only the registered keys are counted, so the test must re-run as owners register; the `PresetId` choice is fixed (`FName`, at most 16 characters)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/StealthFoundation/Private/Tests/Tuning/SnapshotSizeBudgetTest.cpp` (`StealthGame.Tuning.SnapshotSizeBudget`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-040-canonical-keys-and-faces.md`, `production/epics/tuning-data-config/story-038-schema-row-types.md`, `production/epics/tuning-data-config/story-041-schema-generator-commandlet.md` (added after review: schema-table order and the registered key list)
- Unlocks: `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`, `production/epics/tuning-data-config/story-072-snapshot-holdback-test-hook.md`, `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-079-self-check-fault.md`, `production/epics/tuning-data-config/story-099-selfcheck-and-disconnect-seams.md`
- Open verification claims: the legacy replicator sending the custom-`NetSerialize` struct as one unit and re-sending after a lost partial bunch is [U] (ADR-0004 Verification Required (1)); it is settled by story 071, not here. Open: the schema-order source while stories 038/041 are not in this story's dependency list.
