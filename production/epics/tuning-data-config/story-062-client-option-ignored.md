# Story 062: AC-2d ClientOptionIgnored

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-003` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-2d, CR-4

*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

Exit B-2 of the epic. CR-4 states that only the server chooses the preset and that per-client login options are never read for it: a client may append `?Tuning=Playtest` to its own login URL, and that is the one place an attacker or a tester could try to talk the server into a different tuning set. This story proves the negative with a real client connection: a listen server running `Default`, a client logging in with `?Tuning=Playtest`, and the server's preset unchanged. It is an Integration story because the claim is about the server's login path, which a plain-C++ test of the store cannot reach.

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator
**ADR Decision Summary**: The preset name lives in a server-side store written once at session open from the host's own option (non-Shipping), and per-client login options are never read; the store wins over a travel-URL `?Preset=`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: ADR-0004 is LOW risk, but this story rides the T2 harness whose headless run is [U] (ADR-0008 Verification 1: `FPIENetworkComponent` calls `GetFirstActiveViewport`, `PIENetworkComponent.cpp:121-127`; settled by story 019). The CQTest declaration shape `NETWORK_TEST_CLASS`, `FPIENetworkComponent`, `AsListenServer().WithClients(N).WithGameMode(...)` is [V macro names; U exact component template form] (ADR-0008 Key Interfaces). How a CQTest PIE client is given extra login options is not stated in the inputs: open (see the vacuous-pass edge case).

**Control Manifest Rules (this layer)**:
- Required: T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map; travel scenarios are T3 — source: ADR-0008
- Required: T2/T3 tests bound by frames or server time and assert and record the active replication system — source: ADR-0008
- Forbidden: never disable, skip or reseed a failing test to pass a gate; wall-clock time is only a failure ceiling — source: ADR-0008
- Forbidden: never read a gameplay value from ini, console variable or command line (Shipping); per-client login options are never read — source: ADR-0004

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md`, scoped to this story:*

- [ ] **AC-2d (CR-4)**: GIVEN the server runs `Default`, WHEN a client logs in with `?Tuning=Playtest` in its own login options, THEN the active preset is unchanged. (`StealthGame.Tuning.ClientOptionIgnored`)

---

## Implementation Notes

*Derived from ADR-0004 section 6 (Preset store precedence) and ADR-0008 sections 1 (T2) and 2 (naming):*

- **Test shape.** `Source/StealthTestHarness/Private/Tests/PIE/ClientOptionIgnoredTest.cpp`, declared `NETWORK_TEST_CLASS(ClientOptionIgnored, "StealthGame.Tuning.PIE")`, giving the full name `StealthGame.Tuning.PIE.ClientOptionIgnored.<Method>`. The GDD names the criterion `StealthGame.Tuning.ClientOptionIgnored`; by ADR-0008 section 2 a T2 test takes the PIE segment, and CQTest classes are exempt from the `F...Test` naming rule. The method name states the result. Whole file inside the CQTest guard (`ENABLE_PIE_NETWORK_TEST`, which implies `WITH_EDITOR`) and `#if WITH_DEV_AUTOMATION_TESTS`; the test is under the harness module because T2 and T3 code live in `StealthTestHarness`.
- **Setup.** `AsListenServer().WithClients(1).WithGameMode(AStealthGameMode::StaticClass())` and a first step that loads the fixture map (CQTest otherwise runs in an auto-created empty map with `AGameModeBase`). The server runs `Default` (no `?Tuning=`).
- **Vacuous-pass guard (QA edge case).** A harness that fails to carry the option to the server would pass trivially. The test must assert that the option actually reached the server's login URL (for example by reading the options string the server received on login) before asserting that the preset did not change. How to inject and observe the option through the CQTest component is open; if the component cannot carry client login options, the story records that and uses the nearest supported way, noted in the story.
- **Assertions.** After the client's login: the server's active preset and `PresetId` are still `Default` (read from the store, story 058). The QA spec also asserts the client snapshot's `PresetId` is `Default`; the snapshot property is built in story 073, which is not a declared dependency of this story (declared: 058, 019). Either add 073 as a dependency through `/dev-story`, or limit the assertion to the server store here and leave the client-snapshot half to story 073's `SnapshotPublicOnly`. This is flagged as an inconsistency in the plan.
- **Both catalog cases.** Run the test with `Playtest` HostSelectable and with it not HostSelectable (QA edge case), so the result does not depend on the selectability check of story 059. Changing the catalog needs the injection seam from story 059 (open there).
- **Store change.** The plan lists `FND/Private/Tuning/StealthTuningStore.cpp` as a file this story may touch. The store already never reads client login options; touch it only if the test finds the opposite, and record that as a finding.
- **Bounds and fallback.** Bound every step by frames or server time; wall clock only as a ceiling. Assert and record the active replication system (legacy) through the harness assertion of story 018. Per amendment 019 (G4), if the headless T2 proof fails, this story carries the same fallback (T2 in a non-headless editor on the dev machine) with an ADR-0008 amendment request, never a skip.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`: the store and the host's own `?Tuning=` path
- `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`: the T2 harness proof and the headless verification
- `production/epics/tuning-data-config/story-059-preset-not-selectable.md`: the selectability check on the host's option
- `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`: the replicated snapshot and its `PresetId`
- `production/epics/tuning-data-config/story-082-local-tamper-ignored.md`: client-side tampering of assets, ini and console (multi-process)
- `production/epics/tuning-data-config/story-063-preset-survives-travel.md`: the store across ServerTravel

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/ClientOptionIgnoredTest.cpp`
- `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp` (only if the test finds the client option is read)

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a client's own `?Tuning=Playtest` login option does not change the server's active preset (test `NETWORK_TEST_CLASS(ClientOptionIgnored, "StealthGame.Tuning.PIE")`, full name `StealthGame.Tuning.PIE.ClientOptionIgnored.*`)
  - Given: a listen server running `Default`, one client
  - When: the client logs in with `?Tuning=Playtest` in its own login options
  - Then: the server's active preset and `PresetId` are unchanged (`Default`); the client snapshot `PresetId` is `Default`
  - Edge cases: `Playtest` HostSelectable and not HostSelectable; a PIE client login-option injection that fails to carry the option is a vacuous pass, so assert the option reached the server login URL

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: the CQTest `Source/StealthTestHarness/Private/Tests/PIE/ClientOptionIgnoredTest.cpp` (`StealthGame.Tuning.PIE.ClientOptionIgnored`) must exist and pass, or, if story 019 closes FAIL, the documented non-headless run per the G4 fallback

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-058-tuning-store-default-preset.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`
- Unlocks: None
- Open verification claims: the plan has no Note line for this story. It inherits ADR-0008 Verification 1 (headless CQTest PIE, [U], story 019) and the `NETWORK_TEST_CLASS` exact template form ([U]). Open items found while writing: how the PIE client's login options are injected and observed, and the undeclared dependency on story 073 for the client-snapshot assertion.
