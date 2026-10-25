# Story 085: AC-14a HotReloadPie

> **Epic**: Tuning Data & Config
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/tuning-data-config.md`
**Requirement**: `TR-tdc-010`, `TR-tdc-008` (text lives in `docs/architecture/tr-registry.yaml`); GDD AC-14a (CR-11, CR-12)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0004: Tuning data pipeline and validator (section 9, PIE hot-reload; section 8, Latch classes)
**ADR Decision Summary**: In an editor build only (`WITH_EDITOR`), the store subscribes to `FCoreUObjectDelegates::OnObjectPropertyChanged`, accepts only `ValueSet` events with a non-null property on the preset in use, debounces 0.25 s on the editor ticker and routes re-validation to the PIE server world's store. The live set is the frozen `Round` values plus the edited `Action`/`Use` values; a passing set publishes with `Revision + 1`, a failing one is rejected, and the round is flagged `HotReloaded`. `Action`-latched values are captured into the action record at start and never re-read.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: LOW
**Engine Notes**: `FCoreUObjectDelegates::OnObjectPropertyChanged(UObject*, FPropertyChangedEvent&)` and `EPropertyChangeType::Interactive`/`ValueSet` are verified [V] (`UObjectGlobals.h:3293`, `UnrealType.h:6957-6961`). This story IS ADR-0004 Verification Required (3): `OnObjectPropertyChanged` fires with `ValueSet` once per committed edit on a data asset open in a details panel during PIE. A headless automation test cannot open a details panel, so the part of that claim that depends on a real panel edit stays [U] unless observed once by hand in the editor; the automated part exercises the same path with committed-edit events. Also gated by ADR-0008 Verification 1 [U] (CQTest PIE, story 019). Undo arrives as an `Unspecified` event with a null property [V `Obj.cpp:885-889`] and is excluded by the filter (verified in story 086).

**Control Manifest Rules (this layer)**:
- Required: hot-reload, `?Tuning=` and console-command code sits inside `WITH_EDITOR` / `!UE_BUILD_SHIPPING` blocks (also `!UE_BUILD_TEST` for `stealth.tuning.preset`) — source: ADR-0004
- Required: `Action`-latched values are captured into the action record at start on server and owning client — source: ADR-0004
- Forbidden: never use wall-clock time as a test pass/fail predicate (only as a failure ceiling); never use wall-clock waits in tests — source: ADR-0008
- Required: T2 tests set `WithGameMode(AStealthGameMode)` and load their fixture map; T2 bounded by frames or server time — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/tuning-data-config.md` AC-14a, scoped to this story:*

- [ ] **AC-14a (CR-11, CR-12)**: GIVEN PIE with 3 worlds and an injected clock, with a defence started at t=0 and `DefenceDurationSec` 4.0, WHEN a designer commits 3.0 at t=1, THEN all of the following hold:
  - the in-flight defence ends at t=4.0 and the next lasts 3.0;
  - `Revision` rises by exactly 1;
  - clients recompute `ClientHash` under the new epoch and stay `Ready`;
  - the log is flagged `HotReloaded`;
  - a committed `NpcCount` edit leaves the live value and the snapshot unchanged until `Reset`, and `Revision` does not rise for it.
  (`StealthGame.Tuning.PIE.HotReloadPie`)

*Per amendment (story 085 REVISED): the "defence started at t=0" is the stub action consumer of story 052, and the 0.25 s debounce is measured on the injected clock through an injectable debounce timer, never wall time. G4 applies (spike/verification story).*

---

## Implementation Notes

*Derived from ADR-0004 sections 8-9 and Key Interfaces (`OnTuningChanged` under `WITH_EDITOR`), GDD CR-11, CR-12 and Edge Cases (hot-reload), ADR-0008 sections 1, 2, 4, and global amendments G1, G4:*

- **Production code.** `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp`: a `WITH_EDITOR` block that (1) subscribes to `FCoreUObjectDelegates::OnObjectPropertyChanged`; (2) accepts only events on the preset in use whose `ChangeType` is `EPropertyChangeType::ValueSet` and whose property is non-null (this filter is what makes the committed edit reach the pipeline; the rejection paths for `Interactive`, undo and null-property events and the I-1 rejection are tested in story 086); (3) debounces 0.25 s; (4) routes the re-validation to the store of the PIE server world's game instance (the PIE world context whose net mode is not `NM_Client`); (5) applies the live-set rule (frozen `Round` values plus edited `Action`/`Use` values; edits to `Round`-latched keys are a pending set applied at the next `Reset`, no `Revision` change); (6) on a passing set whose live-set hash changed, publishes a new frozen set with `Revision + 1`, re-replicates the snapshot, fires `OnTuningChanged(Revision)` (`#if WITH_EDITOR`), and flags the round `HotReloaded` for the log header (header data is story 084). A PIE re-check compares, in-process and with no RPC, the PIE server world's hash with each PIE client world's recomputed `ClientHash`; a mismatch logs an error and never demotes a client from Ready or removes a pawn.
- **Injected clock and debounce (amendment).** The debounce must be measured on an injected `IStealthClock` (story 007) through an injectable debounce-timer interface, so a test advances the fake clock past 0.25 s instead of waiting; the production implementation uses the editor ticker. The interface name and shape are not given in the inputs: open, declare it in Foundation (ADR-0007 section 4: lower layers declare the interfaces they consume). No wall-clock waits anywhere in the test.
- **Stub action consumer (story 052).** `FStubActionConsumer` captures `DefenceDurationSec` into an action record at start and ends the action at start plus the captured value, never re-reading it. Start it at t=0 with `DefenceDurationSec` 4.0, commit 3.0 at t=1 (clock advanced past the debounce), and assert the in-flight action ends at t=4.0 and the next lasts 3.0.
- **Test.** `NETWORK_TEST_CLASS(HotReloadPie, "StealthGame.Tuning.PIE")` in `Source/StealthTestHarness/Private/Tests/PIE/HotReloadPieTest.cpp`, inside `#if ENABLE_PIE_NETWORK_TEST`; full name `StealthGame.Tuning.PIE.HotReloadPie.<Method>`. "3 worlds" is a listen server plus 2 clients: `AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass())`, fixture map in the first step. Assertions: action end times as above; `Revision` rises by exactly 1; each client's recomputed `ClientHash` under the new epoch equals the server's and each client stays Ready (never re-requested, never demoted); the log header (`FStubRound`) shows `HotReloaded` true; then a committed `NpcCount` edit leaves the live value and the snapshot unchanged and `Revision` does not rise (it is `Round`-latched, pending until `Reset`). The `ValueSet` events can be produced by committing the edit on the preset in use and broadcasting the delegate with a `ValueSet` event; the spec does not say whether the edit is a real property edit or a synthetic `FPropertyChangedEvent` (story 086's spec uses synthetic events): open, record the choice.
- **Verification (3) outcome.** Record in this story the result of ADR-0004 Verification Required (3) and, separately, that the "open in a details panel" half was or was not observed by hand in the editor. Do not claim the panel half from synthetic events.
- **G4 closure (verification story).** PASS, or FAIL with a written ADR-0004 amendment request and the named fallback; never by skipping or disabling the test. The fallback to name is not stated in the inputs for this story: open; the only stated fallback is the T2 non-headless editor run on the dev machine if story 019 fails.
- **Prototype.** None.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- `production/epics/tuning-data-config/story-086-hot-reload-rejected.md`: AC-14b, the rejection of an I-1-violating edit, and the 20 interactive slider ticks, undo and null-property events
- `production/epics/tuning-data-config/story-087-hot-reload-absent-packaged.md`: AC-14c, the `WITH_EDITOR` guard and absence in packaged builds
- `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`: sentinel literals for the back-door code, including the hot-reload hook
- `production/epics/tuning-data-config/story-084-round-log-header.md`: the header contents including the `HotReloaded` flag
- `production/epics/tuning-data-config/story-052-round-session-stubs.md`: `FStubActionConsumer` and the Round/Session stubs
- `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`: `IStealthClock` and the fake clock

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tuning/StealthTuningStore.cpp` (`WITH_EDITOR` block: `OnObjectPropertyChanged` filter, 0.25 s debounce, routing to the PIE server world)
- `Source/StealthTestHarness/Private/Tests/PIE/HotReloadPieTest.cpp`

---

## QA Test Cases

*Written by qa-lead at story creation. The developer implements against these — do not invent new test cases during implementation.*

- **AC-1**: a committed `DefenceDurationSec` edit retimes only the next action and raises `Revision` once; a committed `NpcCount` edit changes nothing live
  - Given: PIE with 3 worlds, an injected clock, `FStubActionConsumer` started at t=0 with `DefenceDurationSec` 4.0, an injectable debounce timer on the injected clock (`NETWORK_TEST_CLASS(HotReloadPie, "StealthGame.Tuning.PIE")`)
  - When: the designer commits 3.0 at t=1 (clock advanced past the 0.25 s debounce); a committed `NpcCount` edit
  - Then: the in-flight action ends at t=4.0 and the next lasts 3.0; `Revision` rises by exactly 1; clients recompute `ClientHash` under the new epoch and stay Ready; the log is flagged `HotReloaded`; the `NpcCount` edit leaves the live value and snapshot unchanged until `Reset` and `Revision` does not rise
  - Edge cases: no wall-clock waits; a FAIL closes with an amendment request; records ADR-0004 Verification Required (3)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/HotReloadPieTest.cpp` (CQTest `StealthGame.Tuning.PIE.HotReloadPie`) must exist and pass, OR the recorded FAIL plus ADR-0004 amendment request per G4 (the Verification Required (3) outcome note is in this story in both cases)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/tuning-data-config/story-073-gamestate-snapshot-public-only.md`, `production/epics/tuning-data-config/story-007-stealth-clock-interface.md`, `production/epics/tuning-data-config/story-019-cqtest-headless-proof.md`, `production/epics/tuning-data-config/story-052-round-session-stubs.md`
- Unlocks: `production/epics/tuning-data-config/story-086-hot-reload-rejected.md`, `production/epics/tuning-data-config/story-087-hot-reload-absent-packaged.md`, `production/epics/tuning-data-config/story-088-backdoor-code-sentinels.md`
- Open verification claims: this story verifies ADR-0004 Verification Required (3) (plan Note); the details-panel half is [U] unless observed by hand. ADR-0008 Verification 1 [U] (story 019). Open: real versus synthetic edit events, the debounce-timer interface, the fallback to name on a FAIL.
