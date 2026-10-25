# Story 019: The per-commit possession canary shows no PlayerState, Controller or Owner in any non-owner changelist

> **Epic**: Information Visibility Policy
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The most dangerous Pillar 1 leak is the engine default that replicates `APawn::PlayerState`, `Controller` and `Owner` to every client: a packet dump then separates player pawns from NPC pawns, and possession (takeover, rejoin, `PawnRecovery`) adds an off-cadence update on top. This story is the canary that runs on every commit, does not wait for spikes, and fails the build if any non-owner changelist for either pawn kind ever contains one of the three pointers or a bunch at the possession frame. It is the AC-IVP-37 result that story-049's external-build gate requires, and it is the first test that must be watched to fail (ADR-0008 section 3) before any other leak gate is trusted.

**Scope note (story-level):** the canary runs parameterised over the pawn class under test. Initially that is the harness stand-in pawn that implements the ADR-0001 section 4.1 resets (plus its `COND_None` negative-control variant); it is re-pointed to `AStealthPawn` when shared-character-locomotion lands, and the shared-character-locomotion pawn story must run this canary in its Definition of Done. Possession uses controller `Possess` calls in the test world; rejoin and `PawnRecovery` are simulated by the harness until the participant-state-roster and session-travel rejoin stories land. Until story-001 chooses the capture mechanism, the canary uses the single-process property-level capture described in `production/qa/spikes/2026-10-25_S1-S2-S14.md`; it may be reworked if story-001 picks a different mechanism.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator)); also `TR-ivp-007` (Leak catalogue with CRITICAL ship gate; per-commit canaries)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (sections 4.1, 4.2, 8); also ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3, 5, 6, 8); ADR-0003: Replication, relevancy and the concealment cull hybrid (section 2)
**ADR Decision Summary**: `AStealthPawn::GetLifetimeReplicatedProps` resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly` on the class itself; possession is invisible to non-owners because the possession `ForceNetUpdate` is dropped and the next fixed-cadence update carries the change. The test is absence from every non-owner changelist, not "equal null".
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 E1-E4, E13 [V] (defaults, private `APawn::PlayerState` reset by name, condition-only overload keeps `Owner`'s push-based flag); E8 [V] (possession paths call `ForceNetUpdate`); E12 [V legacy] (a new owner receives current values; the old owner keeps stale last values, hence the owner-local cleanup); E14 [U, S14] (no engine path breaks on an absent `PlayerState` on a simulated proxy). S1 result on file: identity links never sent to non-owner connections with trims on (0 vs 97 sends; single-process, property-level, not byte-exact). CQTest `FPIENetworkComponent` under `-nullrhi` with a listen server and clients is itself [U] (ADR-0008 Verification 1); a failure there moves the canary to T3 by amendment, never by disabling it.

**Control Manifest Rules (this layer)**:
- Required: **`AStealthPawn` resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly`** with the condition-only `ResetReplicatedLifetimeProperty` overload (`RESET_REPLIFETIME_CONDITION` for the public members) — source: ADR-0001
- Required: **`ForceNetUpdate()` on a pawn forwards only inside `FStealthForcedNetUpdate` with an allowed reason, at most once per pawn per frame**; the unscoped possession call is dropped and the 15 Hz cadence carries the change — source: ADR-0003, ADR-0001
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Forbidden: **Never use the per-actor active-override toggle for the owner-only pointers** — source: ADR-0001
- Guardrail: **Tests**: per-commit pipeline ≤ 10 min — source: ADR-0008

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-37** [Int] `StealthGame.Visibility.PossessionCanary` (runs per commit, does not wait for spikes): GIVEN a 2-client test world with one player pawn and one NPC pawn, WHEN a takeover, a rejoin and a `PawnRecovery` possession each occur, THEN the non-owner client's changelist for both pawns contains no `PlayerState`, `Controller` or `Owner` property at any frame, no bunch for the possessed pawn is sent at the possession frame, and `PlayerState->GetPawn()` is null on the non-owner. Negative control: `APawn::PlayerState` at `COND_None` fails. (R14, L1, L31)

---

## Implementation Notes

*Derived from ADR-0001 sections 4.1, 4.2, 8 and ADR-0008 sections 1, 3, 5, 6 and 8:*

- **What to build.** A T2 CQTest: `NETWORK_TEST_CLASS(PossessionCanary, "StealthGame.Visibility.PIE")` with `AsListenServer().WithClients(2).WithGameMode(AStealthGameMode::StaticClass())`, a first step that loads the fixture map (CQTest otherwise runs in an empty map with `AGameModeBase`), a capture of each non-owner client's received property changes for both pawns across the whole possession frame window (not a single frame), and assertions on the three pointers, on bunch absence at the possession frame, and on `PlayerState->GetPawn()` being null on the non-owner. T2 classes are named by scenario and are exempt from the `F...Test` class-name convention (ADR-0008 section 2).
- **Pawn under test.** Parameterise the test over the pawn class. First target: a harness stand-in pawn implementing the ADR-0001 section 4.1 resets (`PlayerState` by name with `ResetReplicatedLifetimeProperty`, `Controller` and `Owner` with `RESET_REPLIFETIME_CONDITION`), kept in `Source/StealthTestHarness/`. Re-point to `AStealthPawn` when it lands; the shared-character-locomotion pawn story runs this canary in its Definition of Done. A misspelled `PlayerState` name is a fatal startup error, so the harness boot test also asserts the reset took effect (ADR-0001 section 4.1).
- **Possession paths.** Takeover and AI hand-over use controller `Possess` calls in the test world. Rejoin and `PawnRecovery` are simulated by the harness (re-adopt of a retained PlayerState and a re-pawn) until participant-state-roster and session-travel land the real paths; real multi-process coverage of rejoin and `PawnRecovery` is story-003, not this canary.
- **Negative control (in the GDD criterion; ADR-0008 section 3).** A pawn variant with `APawn::PlayerState` at `COND_None` must fail the inner check; the suite passes only when that failure is detected. The variant lives only in `StealthTestHarness` or a `WITH_DEV_AUTOMATION_TESTS` block, never in runtime production code. Before any parity result is trusted, the capture mechanism must first detect this control (ADR-0008 section 6).
- **Capture mechanism.** Until story-001 records its choice as an ADR-0008 amendment, use the single-process property-level capture described in `production/qa/spikes/2026-10-25_S1-S2-S14.md`. If story-001 selects NetTrace analysis or a harness-only receive hook, rework the capture and keep the assertions.
- **Run record.** The run writes its result with the active replication system (legacy, asserted at start per ADR-0008 section 5) and the commit; story-049's external-build gate reads a passing record for the commit.
- **Prototype disposition.** Replaced, never promoted: unconditional `PlayerState`/`Controller`/`Owner` replication on `AStealthCharacter` (`Source/StealthGame/Private/StealthCharacter.cpp`, inherited `APawn` defaults, `IsNpc()` at line 138). Reference only: `Source/StealthGame/Private/Spikes/StealthIdentitySpike.*` and the `stealth.Spike.IdentityTrims` pattern.
- **Spike gates.** None: this canary does not wait for spikes (that is its purpose).
- **Cross-epic.** shared-character-locomotion (re-point to `AStealthPawn`, run in its Definition of Done); participant-state-roster and session-travel (real rejoin and `PawnRecovery` paths replace the simulations).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-001: multi-process, byte-level capture mechanism (S1)
- story-003: S14 rejoin and `PawnRecovery` possession multi-process, owner-side possession correctness
- story-018: effective replication settings (AC-IVP-09)
- story-020: frozen player array
- story-054: net-config runtime differential (AC-IVP-45)
- story-044: raw-bunch schema parity over 100 pawn lifetimes (AC-IVP-02)

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/PossessionCanaryTest.cpp` (`NETWORK_TEST_CLASS(PossessionCanary, "StealthGame.Visibility.PIE")`)
- `Source/StealthTestHarness/` (harness stand-in pawn with the ADR-0001 section 4.1 resets, its `COND_None` negative-control variant, simulated rejoin and `PawnRecovery`, property-level capture)

---

## QA Test Cases

- **AC-1**: Possession canary (AC-IVP-37)
  - Given: a 2-client listen world, 1 player pawn and 1 NPC pawn, run on the pawn variant under test.
  - When: a takeover, a rejoin and a `PawnRecovery` possession each occur.
  - Then: the non-owner changelist for both pawns contains no `PlayerState`, `Controller` or `Owner` property at any frame; no bunch for the possessed pawn is sent at the possession frame; `PlayerState->GetPawn()` is null on the non-owner; negative control: a pawn variant with `APawn::PlayerState` at `COND_None` must fail the inner check or the suite fails.
  - Edge cases: assert over the whole frame window, not one frame; rejoin and `PawnRecovery` are harness-simulated for now. (Tier: T2 CQTest.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test at `Source/StealthTestHarness/Private/Tests/PIE/PossessionCanaryTest.cpp` OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: tuning-data-config Exit A (CQTest harness, module skeleton, `Config/DefaultEngine.ini` net profiles from the harness story)
- Unlocks: `production/epics/information-visibility-policy/story-018-engine-default-overrides.md`, `production/epics/information-visibility-policy/story-020-frozen-player-array.md`, `production/epics/information-visibility-policy/story-021-team-view-never-to-opponents.md`, `production/epics/information-visibility-policy/story-049-external-build-critical-gate.md`, `production/epics/information-visibility-policy/story-054-net-config-runtime-differential.md`; shared-character-locomotion epic pawn story (runs this canary in its Definition of Done)
