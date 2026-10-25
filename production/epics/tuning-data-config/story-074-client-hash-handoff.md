# Story 074: AC-10 ClientHashHandoff

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-011` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-10 (CR-9)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7, Snapshot replication; section 5, Self-check)
**ADR Decision Summary**: TDC has no RPC, timer or kick rule of its own. When a client holds a valid snapshot for the current epoch and its PlayerController exists (either may arrive first), it recomputes `ClientHash` from the values it received and hands it to Session's single acknowledgement; the listen host's direct call after the server write plays the same role because the host gets no RepNotify.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Builds on the snapshot property and RepNotify of story 073, so ADR-0004 Verification Required (1) [U] (legacy atomic delivery, spike story 071) is assumed, not settled. PIE test shape is ADR-0008 T2 (Verification 1 [U], story 019). Seamless travel replaces the PlayerController and PlayerState in UE 5.8.4 (ADR-0005 E5, GDD CR-9), so "the controller exists" means the new controller; single-process PIE cannot represent seamless travel, so only the order of arrival is exercised here.

**Control Manifest Rules (this layer)**:
- Required: server tuning-ready bookkeeping is keyed by `ParticipantId`, never replicated, cleared at every epoch — source: ADR-0004
- Forbidden: never deliver the tuning snapshot by reliable client RPC — source: ADR-0004
- Required: every Integration story ships a T2 or T3 test or a documented playtest; T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map — source: ADR-0008
- Forbidden: never use wall-clock time as a test pass/fail predicate (only as a failure ceiling) — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-10, scoped to this story:*

- [ ] **AC-10 (CR-9)**: GIVEN `Preparing` with a Session stub, WHEN each client holds the snapshot, THEN each client hands Session exactly one `ClientHash` per epoch, equal to the server's. The value is recomputed from the received fields: the test corrupts one received field through the hook and asserts the handed hash differs. AND the snapshot-first and controller-first orders (forced by the hook) both produce exactly one hand-off. (`StealthGame.Tuning.PIE.ClientHashHandoff`)

---

## Implementation Notes

*Derived from ADR-0004 sections 5 and 7, ADR-0008 sections 1-2 and 4, and global amendment G1:*

- **Production code.** `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`: the client `OnRep_TuningSnapshot` path and the host direct call. Each computes `ClientHash` only when both a valid snapshot for the current epoch (`bValid` and `RoundIndex` equal to the current epoch) and the local PlayerController exist, in either arrival order, then performs exactly one hand-off per epoch (`Epoch` = `RoundIndex` plus `Revision`). A repeated `OnRep` for the same epoch must not hand off again (QA edge case: duplicate OnRep). `GetClientHash()` on `IStealthClientTuning` returns the value recomputed from received fields, never a stored server value.
- **The hand-off interface (open).** Session is not built in this epic; the receiver is `FStubSession` (story 052), which records the single acknowledgement per participant. The inputs do not name the interface through which the service hands the hash to Session. Per ADR-0007 section 4 the consumer owns its interface (lower layers declare the interfaces they consume), so a small interface declared in Foundation with `FStubSession` as the test implementation is the natural reading; its name and shape are open and must be recorded here at implementation. TDC adds no RPC and no timer (CR-9).
- **Hook usage (story 072).** The test forces both arrival orders (snapshot-first, controller-first) and corrupts one received field through the hook. After corruption the handed hash must differ from the server's; this proves the hash is recomputed from received fields and not copied.
- **Test.** `NETWORK_TEST_CLASS(ClientHashHandoff, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/ClientHashHandoffTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST`; full name `StealthGame.Tuning.PIE.ClientHashHandoff.<Method>`. `AsListenServer().WithClients(N).WithGameMode(AStealthGameMode::StaticClass())`, fixture map in the first step, bounded by frames or server time. Run three scenarios: snapshot-first, controller-first, corrupted field. Assert per client: exactly one hand-off recorded by `FStubSession` per epoch, equal to the server's `ClientHash` (read through `UStealthTuningStore::GetServerClientHash(RoundIndex, Revision)`, story 058, which Session compares against), different hash after corruption, still exactly one hand-off.
- **Stubs and seams.** `FStubSession` and `FStubRound` come from story 052 in `Source/StealthFoundation/Public/Server/Testing/` (G1: server-only headers under `WITH_STEALTH_TEST_SEAMS`). The hook comes from story 072. A fault sink (story 008) asserts zero unexpected faults at teardown.
- **Host path.** The host's own hand-off goes through the accessor path with no RPC; its test is story 077. This story checks remote clients in the listen-server PIE session; whether the host also appears as a participant in the stub's records is open and need not be asserted here.
- **Replication system.** The T2 run asserts and records that the game driver is legacy (story 018).
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated property, the public-only checks and server/client hash equality
- `production/epics/tuning-data-config/story-075-stale-epoch-ignored.md`: server-side comparison and stale-epoch handling (AC-11)
- `production/epics/tuning-data-config/story-077-host-path.md`: the host's no-RPC ready path (AC-12a)
- `production/epics/tuning-data-config/story-072-snapshot-holdback-test-hook.md`: the hook itself
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the Round and Session stubs

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningService.cpp`
- `Source/StealthTestHarness/Private/Tests/PIE/ClientHashHandoffTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: each client hands Session exactly one `ClientHash` per epoch, equal to the server's, recomputed from received fields
  - Given: `Preparing` with an `FStubSession`, the hold-back/order hook (`NETWORK_TEST_CLASS(ClientHashHandoff, "StealthGame.Tuning.PIE")`)
  - When: each client holds the snapshot; run snapshot-first and controller-first orders; corrupt one received field
  - Then: each client hands Session exactly one `ClientHash` per epoch equal to the server's; with the corrupted field the handed hash differs; both orders give exactly one hand-off
  - Edge cases: a duplicate `OnRep` does not duplicate the hand-off

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/ClientHashHandoffTest.cpp` (CQTest `StealthGame.Tuning.PIE.ClientHashHandoff`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-072-snapshot-holdback-test-hook.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: `production/epics/tuning-data-config/story-075-stale-epoch-ignored.md`, `production/epics/tuning-data-config/story-076-no-pawn-before-ready.md`
- Open verification claims: ADR-0004 Verification Required (1) [U] and ADR-0008 Verification 1 [U] are assumed settled by stories 071 and 019. Open: the name and shape of the Session hand-off interface; whether the host appears in the stub's records.
