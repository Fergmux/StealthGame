# Story 037: Player and NPC pawns are indistinguishable to a remote client

> **Epic**: Character Pool & Appearance
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-006`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-006 = One look path for every viewer and pawn kind; no keyed material parameters.)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (with ADR-0012)
**ADR Decision Summary**: Every UE default that reveals player-ness is overridden: one pawn class, one archetype, one replicated subobject list; the pawn's `PlayerState` reference never reaches non-owners; `Owner` and `Controller` resolve identically for NPC and player pawns on non-owners; no appearance property exists on `PlayerState` or `GameState`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0001, ADR-0012)
**Engine Notes**: Stock `APawn` replicates `PlayerState` to everyone: suppression is ADR-0001's. Multi-client CQTest naming `StealthGame.CharacterPool.PIE.<Scenario>.<Method>`. **Readiness:** the GDD marks AC-CP-16 "Blocked by Shared Character & Locomotion (pawn class)"; this story is Ready only in the sense that the shared-character-locomotion stories (001, 004, 032) are written, and it cannot pass before they are implemented (see Dependencies).

**Why this story**: This is CPA's own check that possession changes nothing a non-owning client can see (CP-10). The test must prove it can detect the tell: it fails on an unmodified default `ACharacter` setup. Read GDD CP-10, AC-CP-16 and ADR-0001 §§1 to 2.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every `AStealthPawn` carries the same default subobjects; one pawn class for players and NPCs (ADR-0010, ADR-0001)
- Forbidden: Never add a component on possession; never carry a `PlayerState` reference to non-owners
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-16:*

- [ ] **AC-1** GIVEN a remote client with an NPC pawn, a player pawn and a pawn adopted by an informant mid-round, THEN `GetClass()`, the replicated subobject list, `PlayerState` (null for all), `Owner` and `Controller` (null for all) and net role (simulated proxy) are identical across all three (test `StealthGame.CharacterPool.PIE.PlayerNpcIndistinguishableToRemote`).
- [ ] **AC-2** GIVEN `GameState` and `PlayerState` class reflection on the client, THEN no appearance-related replicated property exists; a seeded property fails the test (test `...PIE.NoParticipantMapOnClients`).
- [ ] **AC-3** GIVEN an unmodified default `ACharacter` fixture class named in the test, THEN AC-1's assertions fail on it (the test detects the tell); the suite requires that failure to pass.

---

## Implementation Notes

- The informant-adopted pawn is checked after takeover, so the stale `PlayerState` and `Owner` on the old owner's former body (spike S14 finding) are the traps: assert on non-owner clients only.
- Overlaps shared-character-locomotion `story-004-class-parity-scan.md`, `story-032-replicated-allowlist.md` and participant-state-roster `story-049-pie-pawn-replication-parity.md`: CPA owns the look-side statement; share the harness, do not duplicate the capture.

**Prototype disposition**: none.

---

## Out of Scope

- Story 038: view-pitch and cadence parity and spawn/export order; SCL's class-parity scan

---

## Files

- `Source/StealthTestHarness/Private/CharacterPool/PlayerNpcIndistinguishableTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; CQTest naming; readiness note above).*

**Test: remote tells identical**
- Given: a remote client with an NPC pawn, a player pawn and an adopted pawn.
- When: they are inspected.
- Then: `GetClass()`, the replicated subobject list, `PlayerState` (null), Owner and Controller (null) and net role (simulated proxy) are identical across all three.
- Edge cases: the informant-adopted pawn after takeover.

**Test: no participant map on clients**
- Given: `GameState` and `PlayerState` class reflection on the client.
- When: scanned.
- Then: no appearance-related replicated property.
- Edge cases: a seeded property fails.

**Test: detector self-check**
- Given: an unmodified default `ACharacter` fixture class.
- When: the first test's assertions run on it.
- Then: they fail; the failure is required so the tell is detectable.
- Edge cases: the fixture class is named in the test.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CharacterPool/PlayerNpcIndistinguishableTest.cpp` — automation tests `StealthGame.CharacterPool.PIE.PlayerNpcIndistinguishableToRemote`, `...PIE.NoParticipantMapOnClients`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-001-pawn-class-default-subobjects.md`, `production/epics/shared-character-locomotion/story-004-class-parity-scan.md`, `production/epics/shared-character-locomotion/story-032-replicated-allowlist.md`, `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`
- Unlocks: None
