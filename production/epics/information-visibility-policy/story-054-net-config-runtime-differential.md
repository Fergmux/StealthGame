# Story 054: Pawn net config is equal across NPC, remote, host and just-taken-over pawns

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs AStealthPawn with the ADR-0003 section 1 net config (shared-character-locomotion, ADR-0010) and a just-taken-over pawn path; negative control: a test-only NetPriority override
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

Cadence, priority, dormancy and relevancy flags are Pillar-1 tells if they differ by pawn kind or change on possession. ADR-0003 section 1 fixes one net configuration for every pawn, set once in the constructor from tuning. This story proves it at runtime: the net-config fields read on class defaults and on four live instances (an NPC pawn, a remote-client pawn, the host's pawn and a pawn that was just taken over) must be equal everywhere. A test-only `NetPriority` override on one pawn must make the check fail, so the gate is known to see a single deviating pawn. It covers leak row L5.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-006` (Single allowlist file, reflection audit and net-config audit on defaults and live instances)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 1, 8); ADR-0008: Test and automation strategy for leak and parity tests (sections 1 T2, 3)
**ADR Decision Summary**: `AStealthPawn` sets `NetUpdateFrequency = MinNetUpdateFrequency = PawnNetUpdateHz` (15), `NetPriority = 3.0`, `DORM_Never`, `bAlwaysRelevant = bOnlyRelevantToOwner = bNetUseOwnerRelevancy = false` once from the frozen tuning snapshot, with no per-class or per-instance override; the section 8 audit asserts net config on class defaults and on live instances of an NPC pawn, a remote-client pawn, a host pawn and a just-taken-over pawn.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `NetUpdateFrequency`, `MinNetUpdateFrequency` and `NetCullDistanceSquared` are setter-only since 5.5 (E7) [V]; `net.UseAdaptiveNetUpdateFrequency` defaults to 0 (E6) [V]; `p.EnableCharacterAccelerationReplication=1` is asserted by the boot test and `bRepAcceleration` by the pawn audit (ADR-0003 section 1). E13 (CMC cadence and corrections identical for PC- and AI-possessed pawns) is [U] and owned by S13 (story-007). T2 uses CQTest `FPIENetworkComponent` (ADR-0008 Verification 1, [U] headless).

**Control Manifest Rules (this layer)**:
- Required: **Pawn net config is set only from tuning values, through the 5.5+ setters (`SetNetUpdateFrequency`, `SetMinNetUpdateFrequency`, `SetNetCullDistanceSquared`), identically for every pawn** — source: ADR-0003
- Forbidden: **Never give a pawn dormancy, a cull distance, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, a per-instance priority or team relevancy; never change net config per instance, kind or on possession** — source: ADR-0003
- Required: **Every leak, parity, statistical and lint gate includes a negative control the suite detects** — source: ADR-0008
- Guardrail: **Replication**: pawns at a fixed 15 Hz within 36 KB/s worst case (60 pawns) — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, AC-IVP-45, scoped to this story:*

- [ ] **AC-IVP-45** [PIE-3][Int] `StealthGame.Visibility.NetConfigRuntimeDifferential`: GIVEN an NPC pawn, a remote-client pawn, a host pawn and a just-taken-over pawn, WHEN the R11 net-config fields are read on class defaults and on the live instances, THEN every field is equal across all four. Negative control: a test-only override of `NetPriority` on one pawn fails. (R11, L5)

---

## Implementation Notes

*Derived from ADR-0003 sections 1 and 8 and ADR-0008 sections 1 and 3:*

- T2 test `NETWORK_TEST_CLASS(NetConfigRuntimeDifferential, "StealthGame.Visibility.PIE")` (full name `StealthGame.Visibility.PIE.NetConfigRuntimeDifferential.<Method>`) in `Source/StealthTestHarness/Private/Tests/PIE/`, inside `#if ENABLE_PIE_NETWORK_TEST`, with `WithGameMode(AStealthGameMode::StaticClass())` and the fixture map loaded in a first step. Spawn or take the four pawns, read the section 1 fields (update frequency and minimum frequency, priority, dormancy, cull distance, `bAlwaysRelevant`, `bOnlyRelevantToOwner`, `bNetUseOwnerRelevancy`, role on non-owners, `bRepAcceleration`, adaptive frequency off) on the class default object and on each live instance, and compare every field across the four.
- Read the just-taken-over pawn after the takeover step (the QA edge case): the takeover must not change net config (ADR-0003 section 1, "never change net config per instance, kind or on possession"). Use the harness takeover path from story-019 until the npc-kill-penalty-informants takeover exists.
- Reuse the net-config reader and field list from the audit in story-015 (the section 8 audit that covers class defaults and live instances); this story supplies the runtime four-pawn differential.
- Negative control (ADR-0008 section 3, from the criterion): a harness-only override of `NetPriority` on one pawn must fail the check; the suite passes only if the control is detected. The control lives only in `StealthTestHarness`, never Shipping.
- Wall-clock only as a failure ceiling; assert and record the legacy replication system.
- Prototype: Replaces default net config on `AStealthCharacter` (ADR-0003 Migration Plan: the prototype uses default net config and no relevancy override); nothing promoted.
- Cross-epic needs: shared-character-locomotion epic (`AStealthPawn` with the ADR-0003 section 1 net config from the frozen tuning snapshot; pawn net config ADR-0010), npc-kill-penalty-informants epic (a real takeover path, optional until it exists), tuning-data-config Exit A (T2 harness).
- Unblocks when: `AStealthPawn` with the section 1 config exists and a takeover can be performed in the test world.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-015: the reflection audit of replicated properties and the net-config reader
- story-019: the possession canary (three pointers on takeover, rejoin, `PawnRecovery`)
- story-007: S13 CMC cadence measurement
- story-044: raw-bunch schema parity
- shared-character-locomotion epic: setting the net config itself

---

## Files

*Planned (module folders are created by tuning-data-config Exit A and do not exist yet):*

- `Source/StealthTestHarness/Private/Tests/PIE/NetConfigRuntimeDifferentialTest.cpp` (T2 test, including the `NetPriority` override negative control)

---

## QA Test Cases

- **AC-1**: AC-IVP-45 `StealthGame.Visibility.PIE.NetConfigRuntimeDifferential` (T2)
  - Given: an NPC pawn, a remote-client pawn, a host pawn and a just-taken-over pawn.
  - When: the R11 net-config fields are read on class defaults and on the live instances.
  - Then: every field is equal across all four pawns; negative control: a test-only `NetPriority` override on one pawn must fail.
  - Edge cases: read the just-taken-over pawn after the takeover step; compare defaults and live values for each pawn.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: T2 test under `Source/StealthTestHarness/Private/Tests/PIE/`, OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-015-replicated-property-audit.md`, `production/epics/information-visibility-policy/story-019-possession-canary.md`; shared-character-locomotion epic (`AStealthPawn` with the ADR-0003 section 1 net config, ADR-0010); tuning-data-config Exit A (T2 harness)
- Unlocks: None
