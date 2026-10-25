# Story 067: World-scoped accessor reads its own world in multi-client PIE

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-007` (text lives in `docs/architecture/tr-registry.yaml`); GDD CR-10 (one access path, world-scoped); ADR-0007 Validation 8

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-10 requires the tuning accessor to be world-scoped so that multi-client PIE reads the right world: the listen server, and each client, run in their own world inside one editor process, and a service that cached a pointer in a static or global would hand every world the same object. This story proves, with a listen server and two clients, that each world's `UStealthTuningService` returns data from its own world. Single-process PIE shares one asset object, so identical values alone would pass even if a world read another world's data: the amendment therefore asserts object addresses, requiring the server world's frozen set and each client world's pointer into its own GameState snapshot to be three pairwise distinct addresses.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The accessor is a `UWorldSubsystem` (one per world), while pipeline state lives in the game-instance store; each PIE world reads its own GameInstance and its own GameState snapshot.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: ADR-0004 is LOW risk. The T2 harness it rides is [U] for headless operation (ADR-0008 Verification 1; `FPIENetworkComponent` calls `GetFirstActiveViewport`, `PIENetworkComponent.cpp:121-127`, settled by story 019). CQTest facts [V]: `Build()` stops PIE and creates an empty map before every test, the default GameMode is `AGameModeBase` unless `WithGameMode` is set, and PIE always runs in one process (`PIENetworkComponent.cpp:33-36, 119, 124-131`). The exact `FPIENetworkComponent` template form is [U] (ADR-0008 Key Interfaces).

**Control Manifest Rules (this layer)**:
- Required: T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map; T2 and T3 code lives in `StealthTestHarness` — source: ADR-0008
- Required: T2/T3 tests bound by frames or server time and assert and record the active replication system — source: ADR-0008
- Forbidden: never give a tuning accessor a pawn, controller, team, viewer or participant parameter — source: ADR-0004
- Forbidden: never disable, skip or reseed a failing test to pass a gate; wall-clock only as a failure ceiling — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` (CR-10), with the amendment to the wording, scoped to this story:*

- [ ] **CR-10 (test `StealthGame.Tuning.PIE.WorldScopedAccessor`)**: in a listen server plus 2 clients PIE, each world's `UStealthTuningService` returns the address and values from its own world (the server world's frozen set; each client world's pointer into its own GameState snapshot), and the three addresses are pairwise distinct (a per-world discriminator, since single-process PIE shares one asset object).

---

## Implementation Notes

*Derived from ADR-0004 section 6 and ADR-0008 sections 1 (T2) and 2 (naming):*

- **Test shape.** `Source/StealthTestHarness/Private/Tests/PIE/WorldScopedAccessorTest.cpp`, declared `NETWORK_TEST_CLASS(WorldScopedAccessor, "StealthGame.Tuning.PIE")` giving `StealthGame.Tuning.PIE.WorldScopedAccessor.<Method>` (CQTest classes are named by scenario and are exempt from the `F...Test` convention, `.claude/rules/test-standards.md`). Inside the CQTest guard (`ENABLE_PIE_NETWORK_TEST`) and `#if WITH_DEV_AUTOMATION_TESTS`.
- **Setup.** `AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass())`, then a first step loading the fixture map. Start a round far enough that the server world's service is Frozen (`BeginResolve`, via the stub round of story 052 or a direct call) and the replicated snapshot of story 073 is valid on both clients. This is why story 073 is a dependency: `GetClientTuning()` returns a pointer into the GameState snapshot only once that snapshot exists.
- **The discriminator.** The server world returns the address of its frozen `FStealthServerTuning` (via `GetServerTuning()`); each client world returns the address from `GetClientTuning()` (a pointer into its own GameState's `TuningSnapshot.Values`). Assert the three addresses are pairwise distinct. To make a wrong-world read detectable beyond addresses, use a non-Default test preset or distinct per-world `RoundIndex` / `Revision` where the harness allows it; the QA spec phrases this as the per-world discriminator, and the means of making each world's data distinct is open (the assets are shared in single-process PIE).
- **Values.** Also assert each world's values equal what that world should hold: the server's frozen set, and each client's own snapshot values.
- **Edge case.** A global or static cache inside the service makes two addresses equal and fails the test: this is the failure the test exists to catch. The test is its own negative evidence only if the failing case was watched once: temporarily introduce a static cache (or a fake service that caches) in a local build and record that the test fails, per ADR-0008 ("a gate you have not watched fail is not a gate"). The QA spec's earlier edge case about reads after a world shuts down was dropped in the revised spec and is not required.
- **Bounds and fallback.** Bound steps by frames or server time (wall clock only a ceiling); record the active replication system via the harness assertion (story 018). Per amendment 019 (G4), if headless T2 fails (story 019), this story carries the fallback: run T2 in a non-headless editor on the dev machine with an ADR-0008 amendment request, never skip.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`: the service and its signature parity
- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated snapshot and its public-only content
- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: the T2 harness proof
- `production/epics/tuning-data-config/story-076-no-pawn-before-ready.md`: `GetClientTuning()` invalid before ready
- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the game-instance store

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/WorldScopedAccessorTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: each PIE world's accessor returns its own world's data at three distinct addresses (test `NETWORK_TEST_CLASS(WorldScopedAccessor, "StealthGame.Tuning.PIE")`)
  - Given: a listen server plus 2 client PIE worlds after a snapshot (story 073)
  - When: each world's `UStealthTuningService` is read
  - Then: the server world returns its frozen set; each client returns a pointer into its own GameState snapshot with its values; the three addresses are pairwise distinct
  - Edge cases: a global or static cache makes two addresses equal and fails

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `Source/StealthTestHarness/Private/Tests/PIE/WorldScopedAccessorTest.cpp` (`StealthGame.Tuning.PIE.WorldScopedAccessor`) must exist and pass, or, if story 019 closes FAIL, the documented non-headless run per the G4 fallback

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-066-tuning-service-and-parity.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`, `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. It inherits ADR-0008 Verification 1 (headless CQTest PIE, [U], story 019) and the `FPIENetworkComponent` template form ([U]). Open item found while writing: how each world's data is made distinct in single-process PIE.
