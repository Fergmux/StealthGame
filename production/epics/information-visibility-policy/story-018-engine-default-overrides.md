# Story 018: Effective replication settings override the engine defaults for player and NPC pawns alike (provisional on S1, S13, S14)

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: spikes story-001 (S1 multi-process), story-003 (S14 rejoin/PawnRecovery) and story-007 (S13) have not reported, and AStealthPawn (shared-character-locomotion) is not built. AC is tagged [S1][S13][S14] and provisional pending those spikes
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Unreal's defaults leak player-ness in several quiet ways: a player pawn replicates a non-null `PlayerState` to everyone, possession forces an off-cadence update, adaptive frequency makes cadence depend on activity, and `RemoteViewPitch` is written only when a controller exists. This story reads the effective replication settings of a player pawn and an NPC pawn, on class defaults and on live instances, and asserts that every R5 override is in force identically for both. It is the runtime check that the ADR-0001 and ADR-0003 overrides took effect, and its result is provisional until the three spikes it names report on the production pawn.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator))
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (sections 4.1, 4.2); also ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 1, 2, 3); ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: `AStealthPawn` resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly` on the class; pawn net config is set once from tuning values (15 Hz fixed, adaptive off, priority 3.0, `DORM_Never`, no cull distance, no owner relevancy) identically for every pawn; `PreReplication` writes `RemoteViewPitch16` from `HeadLook` for every pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 E1-E4 and E13 [V] (the pointer defaults and the condition-only `ResetReplicatedLifetimeProperty` overload; `APawn::PlayerState` is private, reset by name, a misspelled name is a fatal startup error); ADR-0003 E6 (`net.UseAdaptiveNetUpdateFrequency` defaults to 0) [V], E7 (setter-only net config since 5.5) [V], E8 (`APawn::SetRemoteViewPitch(double)` writes `RemoteViewPitch16`, 5.6) [V], E13 (CMC cadence identical for PC and AI pawns) [U, S13]. Open spikes: S1 (story-001) multi-process byte-level capture; S14 (story-003) possession, `OnRep_Controller`, `ClientRestart` and CMC routing across rejoin and PawnRecovery; S13 (story-007) CMC cadence parity.

**Control Manifest Rules (this layer)**:
- Required: **`AStealthPawn` resets `PlayerState` (by name), `Controller` and `Owner` to `COND_OwnerOnly`** with the condition-only `ResetReplicatedLifetimeProperty` overload — source: ADR-0001
- Required: **Pawn net config is set only from tuning values, through the 5.5+ setters, identically for every pawn** — source: ADR-0003
- Required: **`PreReplication` writes `RemoteViewPitch16` from `HeadLook` for every pawn, after `Super`** — source: ADR-0003
- Forbidden: **Never give a pawn dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy; never change net config per instance, kind or on possession** — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-09** [Int][S1][S13][S14] `StealthGame.Visibility.EngineDefaultOverrides`: GIVEN a player pawn and an NPC pawn, WHEN effective replication settings are read, THEN `PlayerState`, `Controller`, `Owner` are owner-only and `PlayerState->GetPawn()` is null on non-owners; net frequency is fixed (SCL F9) with adaptive off; `RemoteViewPitch` comes from the `Look()` path; non-owners see `SimulatedProxy`; demo recording is disabled. (R5; PlayerState rows are AC-PSR-44)

---

## Implementation Notes

*Derived from ADR-0001 sections 4.1-4.2 and ADR-0003 sections 1-3, ADR-0008 sections 1 and 2:*

- **What to build.** A test that reads effective replication settings on the class defaults and on live instances (a player-possessed pawn and an NPC pawn) and asserts: the three pointers are `COND_OwnerOnly` and `PlayerState->GetPawn()` is null on every non-owner client; `NetUpdateFrequency = MinNetUpdateFrequency = PawnNetUpdateHz` with adaptive frequency off (`net.UseAdaptiveNetUpdateFrequency = 0`); `RemoteViewPitch16` is written from the `Look()`/`HeadLook` path for both pawns; both pawns are `SimulatedProxy` on non-owners; demo recording is disabled. The frequency value is read from the shared-character-locomotion tuning key (SCL F9), never duplicated here.
- **Result is provisional.** The criterion carries [S1][S13][S14]; record in the test report which of the three spikes had reported when it ran, and re-run once story-001, story-003 and story-007 report on `AStealthPawn`.
- **Placement.** Spec name `StealthGame.Visibility.EngineDefaultOverrides`; QA tier T2 (in-process multi-client). If written as a CQTest, ADR-0008 section 2 gives the full name `StealthGame.Visibility.PIE.EngineDefaultOverrides.<Method>` via `NETWORK_TEST_CLASS(EngineDefaultOverrides, "StealthGame.Visibility.PIE")` with `WithGameMode(AStealthGameMode)`. The test asserts and records the active replication system (legacy, ADR-0008 section 5).
- **Prototype disposition.** Replaced, never promoted: `AStealthCharacter` inherits unconditional `APawn::PlayerState`/`Controller` replication and `IsNpc()` is `PlayerState == null` (`Source/StealthGame/Private/StealthCharacter.cpp:138`).
- **PlayerState rows.** The AActor/PlayerState trims are AC-PSR-44 (participant-state-roster); this story does not re-test the PlayerState allowlist.
- **Spike gates.** story-001 (S1), story-003 (S14), story-007 (S13). Unblocks when all three have reported and `AStealthPawn` exists (shared-character-locomotion pawn story with the ADR-0003 section 1 net config, ADR-0010).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-019: possession canary (per-commit changelist absence check)
- story-054: net-config runtime differential across NPC, remote-client, host and just-taken-over pawns (AC-IVP-45)
- story-015: reflection audit against the allowlist
- story-043: transport classifier AUC (cadence statistics)
- story-001, story-003, story-007: the spikes this story is provisional on

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/EngineDefaultOverridesTest.cpp` (test `StealthGame.Visibility.EngineDefaultOverrides`; reads settings on CDOs and live instances)

---

## QA Test Cases

- **AC-1**: Engine default overrides (AC-IVP-09)
  - Given: a player pawn and an NPC pawn, on class defaults and live instances.
  - When: effective replication settings are read.
  - Then: `PlayerState`, `Controller` and `Owner` are owner-only; `PlayerState->GetPawn()` is null on non-owners; net frequency is fixed per SCL F9 with adaptive off; `RemoteViewPitch` comes from the `Look()` path; non-owners see `SimulatedProxy`; demo recording is disabled.
  - Edge cases: the SCL F9 value is read from SCL, not duplicated; result is provisional until the spikes report. (Tier: T2.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test under `Source/StealthTestHarness/Private/Tests/PIE/` OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md`, `production/epics/information-visibility-policy/story-003-spike-s14-s1-rejoin-and-pawnrecovery-possession.md`, `production/epics/information-visibility-policy/story-007-spike-s13-cmc-cadence-pc-vs-ai-parity.md`, `production/epics/information-visibility-policy/story-019-possession-canary.md`; shared-character-locomotion epic story delivering `AStealthPawn` with the ADR-0003 section 1 net config (ADR-0010)
- Unlocks: None
