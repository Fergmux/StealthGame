# Story 030: Pawn replication settings are equal for NPC and player and pinned to the parity values

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: S
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R13 Network parity configuration; AC-SCL-16)
**Requirement**: `TR-scl-009` (net parity config: 15 Hz fixed, priority 3.0, `DORM_Never`, no cull distance, no pawn multicast)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (section 1)
**ADR Decision Summary**: `AStealthPawn`'s constructor sets, from the frozen tuning snapshot, `NetUpdateFrequency = MinNetUpdateFrequency = PawnNetUpdateHz` (15), adaptive frequency off, `NetPriority = 3.0`, `DORM_Never`, no cull distance, `bAlwaysRelevant = false`, `bOnlyRelevantToOwner = false`, `bNetUseOwnerRelevancy = false`; `SimulatedProxy` on every non-owner for both kinds. No per-class or per-instance override exists.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: `net.UseAdaptiveNetUpdateFrequency = 0` (engine default, E6) is asserted at startup by the harness boot test; `NetClientTicksPerSecond` is pinned to 2400 in `Config/DefaultEngine.ini` (ADR-0003 E15) so the listen host updates every client each frame. On a listen server the server tick is the host's render frame, so the 15 Hz rule is stated on host frame time (I-SCL-10, checked by the harness frame-time capture at p99).

**Control Manifest Rules (this layer)**:
- Required: pawn net configuration set once from the tuning snapshot in the constructor and asserted by the audit on class defaults and live instances; identical replicated components and subobjects (ADR-0003 section 1, section 8).
- Forbidden: no net dormancy, `bOnlyRelevantToOwner`, cull distance, per-class or per-instance priority or relevancy override; no `ForceNetRelevant`/`ForceActorRelevantNextUpdate` on a pawn.
- Guardrail: pawn network at most 36 KB/s per client (story 034).

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-16** [P] `NetConfigParity`: GIVEN an NPC pawn and a player pawn at runtime, WHEN their replication settings are dumped (update frequency, min frequency, priority, relevancy flags, dormancy, components, subobjects, condition table), THEN every field is equal, AND absolutely `NetUpdateFrequency` = `MinNetUpdateFrequency` = 15, `NetPriority` = 3.0, `NetDormancy` = `DORM_Never`, `bOnlyRelevantToOwner` = false, adaptive frequency off, and this stays true after possession, unpossession, a takeover and a state transition.

---

## Implementation Notes

*Derived from ADR-0003 section 1 and 8:*

- Read `PawnNetUpdateHz` from the tuning snapshot (never a literal) in the `AStealthPawn` constructor path that runs after `OnTuningReady`; apply `SetNetUpdateFrequency` and `SetMinNetUpdateFrequency`, `NetPriority = 3.0f`, `SetNetDormancy(DORM_Never)` in the class default only (no call elsewhere, see story 031), and relevancy flags false.
- The dump covers a class default, an NPC pawn, a player pawn, a just-taken-over pawn and a host pawn, on live instances in a test world. Re-dump after each of the four events in the AC. Wire the dump into the harness boot audit as well (the allowlist audit of story 032 reuses it).
- **Prototype replaced**: prototype pawns used engine defaults for net configuration; not ported.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 031: dormancy and class-priority bans (source scan, 50 instances over 60 s).
- Story 032: the replicated-property allowlist.
- The concealment cull hook (ADR-0003 section 5; Visibility Policy epic).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (net config)
- `Source/StealthGameplay/Private/Tests/Locomotion/NetConfigParityTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE. Test `StealthGame.Locomotion.NetConfigParity`.*

Test: net config parity (AC-SCL-16)
  Given: an NPC pawn and a player pawn at runtime
  When: their replication settings are dumped (update frequency, min frequency, priority, relevancy flags, dormancy, components, subobjects, condition table), and again after possession, unpossession, a takeover and a state transition
  Then: every field is equal between the two; `NetUpdateFrequency` = `MinNetUpdateFrequency` = 15, `NetPriority` = 3.0, `NetDormancy` = `DORM_Never`, `bOnlyRelevantToOwner` = false, adaptive frequency off; all of this stays true after each of the four events
  Edge cases: none beyond the four events named

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthGameplay/Private/Tests/Locomotion/NetConfigParityTest.cpp`, test name `StealthGame.Locomotion.NetConfigParity` — must exist and pass (not player-observable)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`, `story-003-tuning-keys-registered.md`, `story-008-spawn-bunch-parity.md`
- Unlocks: `production/epics/shared-character-locomotion/story-031-no-dormancy-owner-only-or-class-priority.md`, `story-033-replication-delay.md`
