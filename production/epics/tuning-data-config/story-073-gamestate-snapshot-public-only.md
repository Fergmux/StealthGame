# Story 073: AC-7 SnapshotPublicOnly

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-006`, `TR-tdc-005` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-7 (CR-8)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7, Snapshot replication)
**ADR Decision Summary**: The public snapshot replicates as a property of `AStealthGameState` with `ReplicatedUsing = OnRep_TuningSnapshot` and condition `COND_None` (not initial-only: hot-reload revisions and rejoiners need it), written only after the freeze and self-check, never on a PlayerState; `TuningHash` is never a replicated field. The client's RepNotify (and the listen host's direct call) recomputes `ClientHash` from the received fields.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: ADR-0004 Verification Required (1) [U] (atomic replication of the ~2 KB custom-`NetSerialize` struct under the legacy driver, spike story 071) underlies this story; this story assumes the legacy driver (ADR-0001) and does not settle that claim. Iris is out of scope: if the project ever moves to Iris the struct would need an Iris `NetSerializer` (ADR-0024, not written). ADR-0008 Verification 1 [U] (CQTest PIE headless, story 019) applies to the T2 test. `DOREPLIFETIME_CONDITION` is the registration macro (not deprecated, `docs/engine-reference/unreal/VERSION.md`).

**Control Manifest Rules (this layer)**:
- Required: every new replicated field ships with a Visibility Policy row and a signed-off allowlist row; the reflection audit fails the build otherwise — source: ADR-0001, ADR-0003
- Required: the game NetDriver runs the legacy replication system and the harness asserts it at startup — source: ADR-0001
- Forbidden: never put team, state, connection, appearance or pawn data on a PlayerState — source: ADR-0001 (the snapshot is on the GameState, on no PlayerState, per AC-7)
- Forbidden: never deliver the tuning snapshot by reliable client RPC — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-7, scoped to this story:*

- [ ] **AC-7 (CR-8)**: GIVEN a 2-client round using a non-`Default` test preset, WHEN `Preparing` completes, THEN all of the following hold:
  - each client's snapshot contains exactly the public keys plus `SchemaVersion`, `PresetId`, `RoundIndex`, `Revision` and `bValid`;
  - reflection shows that struct membership equals the schema audience and that no server-only key is in the replicated struct;
  - the snapshot is on the GameState and on no PlayerState;
  - no field named `TuningHash` is replicated;
  - each client's recomputed `ClientHash` equals the server's.
  (`StealthGame.Tuning.PIE.SnapshotPublicOnly`)

---

## Implementation Notes

*Derived from ADR-0004 sections 6-7 and Key Interfaces, ADR-0007 sections 2 and 8, ADR-0008 sections 1-2:*

- **Replicated property.** Add `UPROPERTY(ReplicatedUsing = OnRep_TuningSnapshot) FStealthTuningSnapshot TuningSnapshot;` to `Source/StealthGame/Public/StealthGameState.h` with `OnRep_TuningSnapshot()`, registered in `GetLifetimeReplicatedProps` as `DOREPLIFETIME_CONDITION(AStealthGameState, TuningSnapshot, COND_None)`. The prototype `AStealthGameState` already has `GetLifetimeReplicatedProps` and several `Replicated` members; only the snapshot property is added and the rest of the prototype GameState is untouched. The prototype stays in the composition root `StealthGame` (ADR-0007 section 8: nothing is promoted before its ADR is built against; this story extends the composition-root class and does not move it).
- **Write rule.** The server writes `TuningSnapshot` only after the freeze (and after the self-check once story 079 lands; until then the write follows the freeze only: open, the self-check function is story 079 and is not in this story's dependency list). A new GameState starts with `bValid` false; values from a previous GameState are invalid from travel start (travel itself is story 081).
- **Client side.** `OnRep_TuningSnapshot` (and the listen host's direct call after the server write, since the host gets no RepNotify) recomputes `ClientHash` from the received fields and hands it to Session's acknowledgement path. The hand-off is story 074; this story verifies equality of the recomputed hash with the server's. Open: the inputs show `FTuningHasher::Hash(const FStealthServerTuning&, int32, bool bPublicOnly)` taking the server struct, while a client holds only `FStealthClientTuning`; how the client recomputes from the client face is not stated. Resolve at implementation (for example a client-face overload over the public schema rows) and record it; the result must equal the server's public-only hash (story 045 owns the hasher).
- **Where the non-Default preset comes from (open).** The inputs do not say how a non-Default test preset reaches the PIE server store. Candidate: the `ITuningPresetSource` seam (story 050) through the store (story 058), as AC-2a uses a test catalog entry. The preset must differ from `Default` in at least one public and at least one server-only key (QA spec edge case) so that both the membership and the hash checks can fail.
- **Test.** `NETWORK_TEST_CLASS(SnapshotPublicOnly, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/SnapshotPublicOnlyTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST`; full name `StealthGame.Tuning.PIE.SnapshotPublicOnly.<Method>`. `AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass())`, fixture map loaded in the first step, every step bounded by frames or server time (wall clock only as a failure ceiling). Assertions: per client, the received snapshot holds exactly the schema's public rows plus the five header fields; reflection over `FStealthClientTuning` shows its members equal the set of schema rows with `bPublic` true and that no `_Server` key is a member; the property `TuningSnapshot` exists on the GameState class and no `PlayerState` class lists a snapshot property; no replicated property named `TuningHash` exists on GameState or PlayerState; recomputed `ClientHash` on each client equals the server's. Include a negative control seen to fail (for example a test-only struct with one server-only key added; ADR-0008 section 3) via the helper of story 010.
- **Allowlist row (open).** ADR-0004 states ADR-0003 registers the property on the replicated-property allowlist (audience All, carrier GameState); the allowlist lives at `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (ADR-0008 section 4, content owned by ADR-0003 and the IVP epic). Whether a reflection audit and that file exist when this story is built is not stated; if the audit exists this story must add the row and the Visibility Policy row so the audit does not fail, and if it does not exist this is recorded as open.
- **Replication system.** The T2 run asserts and records that the game driver is legacy (story 018 provides the harness assertion).
- **Prototype.** Extends `AStealthGameState` (composition root) with the snapshot property; the rest of the prototype GameState is untouched (plan Prototype line). Nothing is promoted.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`: the struct, `NetSerialize` and size budget
- `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`: the `UStealthTuningService` accessor
- `production/epics/tuning-data-config/story-071-snapshot-atomic-spike.md`: atomic delivery under loss
- `production/epics/tuning-data-config/story-074-client-hash-handoff.md`: the one-per-epoch hand-off to Session
- `production/epics/tuning-data-config/story-077-host-path.md`: the listen host's direct path
- `production/epics/tuning-data-config/story-078-remote-loopback-hash.md`: the same hash equality across real processes
- `production/epics/tuning-data-config/story-079-self-check-fault.md`: the server self-check and `TuningSnapshotFault`
- `production/epics/tuning-data-config/story-081-travel-epoch.md`: invalidity of the old snapshot across travel
- `production/epics/tuning-data-config/story-083-full-hash-not-replicated.md`: AC-16, no `TuningHash` value in Active state (this story checks the field name only)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthGame/Public/StealthGameState.h`
- `Source/StealthGame/Private/StealthGameState.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/SnapshotPublicOnlyTest.cpp`
- `Source/StealthFoundation/Private/Tests/Data/ReplicationAllowlist.json` (only if the allowlist and its audit exist; see Implementation Notes)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: the replicated snapshot holds exactly the public keys plus its header, on the GameState only, with no `TuningHash` field and an equal recomputed `ClientHash`
  - Given: a 2-client round with a non-Default test preset (`NETWORK_TEST_CLASS(SnapshotPublicOnly, "StealthGame.Tuning.PIE")`)
  - When: `Preparing` completes
  - Then: each client's snapshot has exactly the public keys plus `SchemaVersion`, `PresetId`, `RoundIndex`, `Revision`, `bValid`; reflection shows struct membership equals the schema audience and no server-only key; the snapshot is on the GameState and on no PlayerState; no replicated field named `TuningHash`; each client's recomputed `ClientHash` equals the server's
  - Edge cases: the preset must differ from Default in a public and a server-only key; the snapshot is written only after freeze plus self-check

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/SnapshotPublicOnlyTest.cpp` (CQTest `StealthGame.Tuning.PIE.SnapshotPublicOnly`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-070-snapshot-struct-size-budget.md`, `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`
- Unlocks: `production/epics/tuning-data-config/story-067-world-scoped-accessor-pie.md`, `production/epics/tuning-data-config/story-074-client-hash-handoff.md`, `production/epics/tuning-data-config/story-077-host-path.md`, `production/epics/tuning-data-config/story-078-remote-loopback-hash.md`, `production/epics/tuning-data-config/story-080-rejoin-snapshot.md`, `production/epics/tuning-data-config/story-081-travel-epoch.md`, `production/epics/tuning-data-config/story-082-local-tamper-ignored.md`, `production/epics/tuning-data-config/story-083-full-hash-not-replicated.md`, `production/epics/tuning-data-config/story-085-hot-reload-pie.md`
- Open verification claims: ADR-0004 Verification Required (1) (legacy atomic delivery, spike story 071) is [U] and not settled here; ADR-0008 Verification 1 (story 019) [U]. Open: how the client recomputes `ClientHash` from the client face, how the non-Default preset reaches the PIE server store, whether the allowlist audit exists yet, the self-check ordering (story 079 is not a dependency).
