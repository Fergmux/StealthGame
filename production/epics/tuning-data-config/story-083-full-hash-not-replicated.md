# Story 083: AC-16 FullHashNotReplicated (provisional on Round)

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-005` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-16 (CR-7)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 5, Freeze, hashes, self-check; section 7)
**ADR Decision Summary**: `TuningHash` covers all keys and is never replicated: no client consumes it. `ClientHash` covers public keys only and is recomputed by the client. The Round Event Log header (server-only) carries `TuningHash`, and clients read it only after `Ended`, under the Visibility Policy.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Reflection over the replicated state of the GameState and every PlayerState (class reflection and instance values in a PIE world); no post-cutoff API. Rests on the replicated snapshot of story 073 (ADR-0004 Verification Required (1) [U], story 071) and CQTest PIE (ADR-0008 Verification 1 [U], story 019). Provisional on Round: the real Round Event Log header does not exist in this epic, so the header is the stub's buffer.

**Control Manifest Rules (this layer)**:
- Required: Event Log types are plain C++ (no `USTRUCT`/`UCLASS`/`UPROPERTY`) with no text conversion besides the `Ended`/`Reset` disk writer; the stub header buffer follows the same rule — source: ADR-0007
- Forbidden: never pass Event Log content, `DealDigest`, server sub-seeds, bindings or mailbox order to `UE_LOG`/`UE_LOGF`/`ensureMsgf`/`checkf`/`ClientMessage`/`TRACE_*`/`CSV_*` — source: ADR-0007
- Required: the hash is the project's own FNV-1a-64 over the F-2 byte stream (the stub records the frozen value, not a recomputation) — source: ADR-0004
- Required: T2/T3 tests are bounded by frames or server time and assert and record the active replication system — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-16, scoped to this story:*

- [ ] **AC-16 (CR-7)**: GIVEN a round in `Active`, WHEN the replicated state of the GameState and every PlayerState is reflected, THEN no `TuningHash` value is present. AND WHEN the round reaches `Ended`, THEN the Round Event Log header (stub until the Round GDD) contains `TuningHash`. (`StealthGame.Tuning.PIE.FullHashNotReplicated`; provisional on Round)

---

## Implementation Notes

*Derived from ADR-0004 sections 5 and 7, GDD CR-7, ADR-0007 section 6, ADR-0008 sections 1-2 and 4, and global amendment G1:*

- **Test.** `NETWORK_TEST_CLASS(FullHashNotReplicated, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/FullHashNotReplicatedTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST`; full name `StealthGame.Tuning.PIE.FullHashNotReplicated.<Method>`. `AsListenServer().WithClients(N).WithGameMode(AStealthGameMode::StaticClass())`, fixture map in the first step, bounded by frames or server time. Provisional on Round: it runs against `FStubRound` (story 052).
- **Active-state scan (names and values).** Reflect every replicated property of the GameState and of every PlayerState in the world (instances, not only class defaults), walking nested structs, and (a) assert none is named `TuningHash`, and (b) scan the values: assert no replicated 64-bit property, and no 8-byte window in a replicated struct image, equals the server's frozen `TuningHash` value (QA edge case: scan values as well as names). Scan on the server instance and on each client instance. The scan must also work with a negative control: a test-only replicated property seeded with the hash value is detected (ADR-0008 section 3; helper of story 010).
- **Ended-state check.** When the stub Round reaches `Ended`, the stub's Round Event Log header buffer must contain `TuningHash` equal to the frozen value. The stub (`FStubRound`, story 052) "holds a Round Event Log header buffer" and receives `OnTuningReady(TuningHash)`; that the stub writes the received `TuningHash` into its buffer is not stated by story 052's AC, so who fills the buffer (the stub on `OnTuningReady`, or the service's header code of story 084) is open and is recorded at implementation. The full header content (`PresetId`, `SchemaVersion`, `FallbackLevel`, `Revision`, warnings, `HotReloaded`) is story 084. This story asserts only that `TuningHash` is present at `Ended` and absent from replicated state in Active.
- **Why this is not story 073's check.** Story 073 asserts no replicated field named `TuningHash`. This story widens it to the Active-phase value scan of every PlayerState and the GameState and adds the Ended-time contrast, which only makes sense with a Round stub.
- **Event Log rule.** The stub header buffer is a plain C++ struct (no `USTRUCT`/`UPROPERTY`) so reflection-based dumps cannot reach it, and it exposes no text conversion; do not route the hash through `UE_LOGF` or any trace channel.
- **Fallback.** If story 019 closes FAIL (T2 not headless), this story runs in a non-headless editor on the dev machine per G4 and the revised story 019.
- **Replication system.** The T2 run asserts and records that the game driver is legacy (story 018).
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the public-only snapshot and the field-name check
- `production/epics/tuning-data-config/story-084-round-log-header.md`: the full header contents (AC-20)
- `production/epics/tuning-data-config/story-045-hasher-golden.md`: the hash function and its golden fixture
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the Round stub that holds the header buffer

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/FullHashNotReplicatedTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: no `TuningHash` value is replicated in Active, and the Round log header stub contains it at Ended
  - Given: a round in `Active` with the Round stub (`NETWORK_TEST_CLASS(FullHashNotReplicated, "StealthGame.Tuning.PIE")`, provisional on Round)
  - When: the replicated state of the GameState and every PlayerState is reflected (names and the 64-bit hash value); the round reaches `Ended`
  - Then: no `TuningHash` present in Active; the stub Round Event Log header contains `TuningHash` at `Ended`
  - Edge cases: scan values as well as names

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/FullHashNotReplicatedTest.cpp` (CQTest `StealthGame.Tuning.PIE.FullHashNotReplicated`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: None
- Open verification claims: provisional on Round (the real header arrives with the Round epic; the plan title and Note flag it). ADR-0004 Verification Required (1) [U] and ADR-0008 Verification 1 [U] are assumed settled by stories 071 and 019.
