# Story 034: Appearance resolved before first replication; no default-look frame

> **Epic**: Character Pool & Appearance
> **Status**: Blocked — BLOCKED: spike CPA-V1 (story 026) has not reported; the first-visible-frame guarantee rests on the null-mesh pawn taking its look from the initial OnRep.
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/character-pool-appearance.md`
**Requirement**: `TR-cpa-004`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time. At write time: TR-cpa-004 = Appearance before first replication, immutable per pawn; deck conserved; `DrawReplacement` same tick; `TakeNpcCard` on adoption.)*

**ADR Governing Implementation**: ADR-0012: Character pool data and appearance allocation
**ADR Decision Summary**: `AppearanceId` is set between `SpawnActorDeferred` and `FinishSpawning` so the first replication bunch carries it, and the pawn draws nothing until it resolves from resident assets (no default or placeholder look, not for a frame, for any pawn class).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH (ADR-0012)
**Engine Notes**: Multi-client tests use CQTest `NETWORK_TEST_CLASS(<Scenario>, "StealthGame.CharacterPool.PIE")`, giving `StealthGame.CharacterPool.PIE.<Scenario>.<Method>` (ADR-0008 exemption; QL-STORY-READY finding 2); test names in this story use that form. Every Pillar-1 assertion is made on the **remote** client (host trust limit).

**Why this story**: CP-18 and AC-CP-15a, 15b, 15e. Read GDD CP-10, CP-18, AC-CP-15 and ADR-0012 §4 and §6.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: The pawn draws nothing until `AppearanceId` has resolved; `PostNetInit` asserts `AppearanceId != 255` in non-Shipping (ADR-0012)
- Forbidden: Never show a default or placeholder look for even one frame
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-CP-15a, 15b, 15e:*

- [ ] **AC-1** GIVEN a listen server and one remote client, WHEN a round is dealt and all pawns spawn (including pawns spawned in the last tick before the reveal), THEN on the remote client every pawn's `AppearanceId != 255` at `PostNetInit`, NPC and player alike (test `StealthGame.CharacterPool.PIE.IdSetBeforeFirstReplication`).
- [ ] **AC-2** GIVEN the same setup, THEN on every frame from first visibility (the first frame `WasRecentlyRendered` is true, including a pawn entering relevancy mid-round) every visible pawn has a non-null mesh and a resolved id (test `...PIE.NoDefaultLookFrame`).
- [ ] **AC-3** GIVEN 200 ms latency, 5 % loss and the named constant `EmulationSeed` (logged for reproduction), THEN AC-1 and AC-2 hold (nightly) (test `...PIE.IdParityUnderEmulation`).

---

## Implementation Notes

- Per-frame check through a hook on each remote pawn's mesh component; do not poll with screenshots.
- Net emulation profile per ADR-0008 (`session-travel` story 002, net-emulation measurement spike, provides the profile).

**Prototype disposition**: replaces the prototype's `OnRep_AppearanceIndex` (default look until the notify); nothing promoted.

---

## Out of Scope

- Story 027: the property and apply path; Story 035: asset load at reveal; Story 036: re-entry and takeover

---

## Files

- `Source/StealthTestHarness/Private/CharacterPool/IdBeforeFirstReplicationTest.cpp`

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (qa-lead; CQTest naming).*

**Test: id before first replication**
- Given: a listen server, one remote client and a dealt round.
- When: all pawns spawn.
- Then: every remote pawn's `AppearanceId` is not 255 at `PostNetInit`, NPC and player alike.
- Edge cases: pawns spawned in the last tick before the reveal.

**Test: no default-look frame**
- Given: the same setup.
- When: each frame is checked from the first frame `WasRecentlyRendered` is true.
- Then: every visible pawn has a non-null mesh and a resolved id on every frame.
- Edge cases: a pawn entering relevancy mid-round.

**Test: parity under emulation (nightly)**
- Given: 200 ms latency, 5 % loss and `EmulationSeed`.
- When: the two tests repeat.
- Then: both pass.
- Edge cases: the seed is logged.

---

## Test Evidence

*Governed by `qa.level: full`: tests are required evidence for Logic and Integration; a Visual/Feel or UI story's retained screenshot is required at every `qa.level`.*

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/CharacterPool/IdBeforeFirstReplicationTest.cpp` — automation tests `StealthGame.CharacterPool.PIE.IdSetBeforeFirstReplication`, `...PIE.NoDefaultLookFrame`, `...PIE.IdParityUnderEmulation` must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/character-pool-appearance/story-027-appearance-id-property-and-apply-path.md`, `production/epics/character-pool-appearance/story-026-spike-cpa-v1-null-mesh-initial-onrep.md`, `production/epics/session-travel/story-002-net-emulation-measurement-spike.md`
- Unlocks: None
