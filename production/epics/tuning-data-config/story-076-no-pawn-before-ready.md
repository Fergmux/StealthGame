# Story 076: AC-9 NoPawnBeforeReady

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-011`, `TR-tdc-006` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-9 (CR-8)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 7)
**ADR Decision Summary**: Until the snapshot is valid the client holds its participant in a loading state: `GetClientTuning()` returns an explicit invalid result (never a local default), and a participant entering a round for the first time has no pawn observable by other clients until it is tuning-Ready. Round owns pawn release, at a common moment, so per-client readiness timing is never visible. This is the Pillar 1 rule for the loading state (GDD CR-8).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Uses the snapshot replication of stories 070-073 (ADR-0004 Verification Required (1) [U], spike story 071) and CQTest PIE (ADR-0008 Verification 1 [U], story 019). B's tick-by-tick sampling of its replicated actors needs the hold-back hook (story 072) to withhold the snapshot to client A only. Pawn spawn and release belong to Round, which does not exist in this epic: the test runs against `FStubRound` and `FStubSession` and proves TDC's half only.

**Control Manifest Rules (this layer)**:
- Forbidden: never let Session spawn, free, hide or possess a pawn — source: ADR-0005 (Session reports rejection; Round owns the pawn)
- Required: T2/T3 tests are bounded by frames or server time; T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map — source: ADR-0008
- Forbidden: never use wall-clock time as a test pass/fail predicate (only as a failure ceiling) — source: ADR-0008
- Forbidden: never deliver the tuning snapshot by reliable client RPC — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-9, scoped to this story:*

- [ ] **AC-9 (CR-8)**: GIVEN snapshot replication to client A is held back by the test hook, WHEN client B samples its replicated actors every tick from A's connect until the server marks A tuning-`Ready`, THEN B sees zero pawns bound to A, and A's `GetClientTuning()` returns *invalid* and no default value throughout. A's pawn first appears on B no earlier than the common release tick of the round. AND a rejected acknowledgement on Session's deadline or mismatch path (stub) never produces a pawn for A. (`StealthGame.Tuning.PIE.NoPawnBeforeReady`)

---

## Implementation Notes

*Derived from ADR-0004 sections 6-7, GDD CR-8/CR-9, ADR-0008 sections 1, 2 and 4, and global amendment G1:*

- **Test.** `NETWORK_TEST_CLASS(NoPawnBeforeReady, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/NoPawnBeforeReadyTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST`; full name `StealthGame.Tuning.PIE.NoPawnBeforeReady.<Method>`. `AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass())` (A, B), fixture map in the first step. Every step is bounded by frames or server time.
- **Sequence.** (1) Hold back snapshot replication to A with the hook (story 072; B and the server are unaffected). (2) A connects; from that tick B samples its own replicated actors every tick (no sampling gaps; QA edge case) until the server marks A tuning-Ready, recording any pawn bound to A. (3) Throughout, assert A's `GetClientTuning()` returns invalid (nullptr) and never a default value. (4) Release the hold-back; the stub Round releases pawns at a common tick; assert A's pawn first appears on B no earlier than that tick. (5) Rejection path: with a fresh participant, drive `FStubSession` to reject the acknowledgement (deadline path, then mismatch path); assert no pawn is ever produced for that participant.
- **Control run.** A run without the hold-back must spawn the pawn at the release tick (QA edge case); without it the "zero pawns" assertion could pass vacuously.
- **What a "pawn bound to A" is (open).** `AStealthPawn` is not built in this epic, and the inputs do not define the pawn class or the binding the sampler looks for. The stub Round (`FStubRound`, story 052) "releases pawns at a common tick"; the actor class it spawns and how a pawn is recognised as bound to participant A are open and must be recorded at implementation (a harness-only pawn class is the likely shape).
- **Where the gate lives.** The "release no earlier than the common tick" and the rejection-never-spawns behaviours are properties of the stub Round and stub Session; this test therefore proves TDC's half (tuning-Ready gates pawn release and a missing snapshot reads as invalid); the real Round and Session epics replace the stubs with integration tests (plan Note: runs against the Round/Session stubs until those epics land). The QA spec states the same.
- **Invalid read.** `GetClientTuning()` returns nullptr before the snapshot is valid (`bValid` false or stale `RoundIndex`), never a local default or asset value (ADR-0004 Key Interfaces; story 066 owns the service, story 073 the snapshot).
- **Seams.** Hook (072), `FStubRound` and `FStubSession` (052, G1 paths under `Source/StealthFoundation/Public/Server/Testing/`), fault sink (008): zero unexpected faults at teardown.
- **Replication system.** The T2 run asserts and records that the game driver is legacy (story 018).
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-074-client-hash-handoff.md`: the client's hand-off of `ClientHash`
- `production/epics/tuning-data-config/story-075-stale-epoch-ignored.md`: stale-epoch comparison on the server
- `production/epics/tuning-data-config/story-080-rejoin-snapshot.md`: the rejoining participant keeps its pawn and issues no command before its snapshot is valid (multi-process)
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: the stubs used here
- `production/epics/tuning-data-config/story-072-snapshot-holdback-test-hook.md`: the hold-back hook

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/NoPawnBeforeReadyTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: no pawn is bound to a participant before it is tuning-Ready, and its client tuning reads invalid throughout
  - Given: snapshot replication to client A held back; client B sampling replicated actors every tick from A's connect (`NETWORK_TEST_CLASS(NoPawnBeforeReady, "StealthGame.Tuning.PIE")`)
  - When: until the server marks A tuning-Ready; then a Session deadline/mismatch rejection via the stub
  - Then: B sees zero pawns bound to A; A's `GetClientTuning()` is invalid and no default throughout; A's pawn first appears on B no earlier than the common release tick; a rejected acknowledgement never yields a pawn
  - Edge cases: sampling gaps (sample every tick); a control run without hold-back spawns the pawn. Runs against `FStubRound`/`FStubSession` and proves TDC's half only

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/NoPawnBeforeReadyTest.cpp` (CQTest `StealthGame.Tuning.PIE.NoPawnBeforeReady`) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-074-client-hash-handoff.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: None
- Open verification claims: the plan Note says Round owns pawn release and the test runs against stubs until those epics land (a scope limit, not an unverified engine claim). ADR-0004 Verification Required (1) [U] and ADR-0008 Verification 1 [U] are assumed settled by stories 071 and 019. Open: the stub pawn class and how "bound to A" is recognised.
