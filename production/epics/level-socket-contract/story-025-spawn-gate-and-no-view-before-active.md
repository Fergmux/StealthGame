# Story 025: Spawn gate and no world view before Active

> **Epic**: Level Socket Contract
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: not yet started (set by /dev-story when implementation begins)

## Context

**GDD**: `design/gdd/level-socket-contract.md`
**Requirement**: `TR-lsc-007`
*(At write time: TR-lsc-007 = Round-start placement and spawn gate)*

**ADR Governing Implementation**: ADR-0014: Layout service, seeded generation and runtime navmesh
**ADR Decision Summary**: No pawn is spawned for, possessed by or made relevant to a client until Session has moved it to `Connected` for this round (geometry built and verified); placement happens while clients are in the briefing state and no world view is rendered until `Active`. The service never spawns a pawn; Round owns spawn order and uses `UStealthPawnFactory`.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: MEDIUM
**Engine Notes**: Works under `-nullrhi`; relevance is asserted with `IsNetRelevantFor` and the absence of an actor channel. Without collision a client's movement corrections spiral, which is why the gate exists.

**Why this story**: It protects clients from corrections against missing geometry and guarantees the first frame shows everyone in place. Read first: R4 item 6, R6 item 3, AC-21, AC-33.

**Control Manifest Rules (Core layer, manifest 2026-10-25e)**:
- Required: Every pawn is created by `UStealthPawnFactory::SpawnSharedPawn`; clients build only from a complete Manifest (ADR-0010, ADR-0014)
- Forbidden: Never create or possess a pawn with `RestartPlayer` or `SpawnDefaultPawnFor` outside the factory (ADR-0010)
- Guardrail: n/a

---

## Acceptance Criteria

*From GDD AC-21 and AC-33, scoped to this story:*

- [ ] GIVEN a 2-client PIE (works under `-nullrhi`), THEN each client's `WorldViewEnabled` event is ordered after its `Active` transition (`Placement.NoViewBeforeActive`)
- [ ] GIVEN client B's load delayed by 5 s on the injected clock, THEN no pawn is spawned for, possessed by or net-relevant to B before B is `Connected` (`Cross.Round.SpawnGate`)

---

## Implementation Notes

- The gate is a requirement on Round that the service documents and the test enforces: spawn only for participants Session reports `Connected` for this round.
- `WorldViewEnabled` is a client-local event; assert its index is greater than the `Active` transition index per client; a late joiner is included.
- For B before `Connected`: 0 pawns spawned for B, 0 possessed by B, 0 net-relevant (`IsNetRelevantFor` false, no actor channel); after `Connected` they follow. B disconnecting while loading is an edge case.
- Round, Session and the pawn factory are stubs/cross-epic; this story supplies the test and the gate hook the service exposes.

---

## Out of Scope

- Story 029: the placement function; story 024: the handshake; the factory and Round themselves (`production/epics/round-match-framework/EPIC.md`, `production/epics/shared-character-locomotion/EPIC.md`)

---

## Files

- `Source/StealthGameplay/Private/Core/LayoutSpawnGate.cpp`
- `Source/StealthTestHarness/Private/SpawnGateTest.cpp`

---

## QA Test Cases

*Written by qa-lead (QL-STORY-READY: ADEQUATE).*

- **AC-1** `Placement.NoViewBeforeActive`
  - Given: a 2-client PIE under `-nullrhi`.
  - When: the round runs Preparing to Active.
  - Then: each client's `WorldViewEnabled` event index is greater than its Active transition index.
  - Edge cases: late-joiner.
- **AC-2** `Cross.Round.SpawnGate`
  - Given: client B's load delayed by 5 s on the injected clock.
  - When: the round spawns pawns.
  - Then: for B before Connected: 0 pawns spawned for B, 0 possessed by B, 0 net-relevant to B; after Connected they follow.
  - Edge cases: B disconnects while loading.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/SpawnGateTest.cpp` — `StealthGame.Layout.Placement.NoViewBeforeActive`, `StealthGame.Layout.Cross.Round.SpawnGate` (CQTest PIE naming per ADR-0008 if used) must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/level-socket-contract/story-024-preparing-handshake-and-hash-ack.md`
- Cross-epic: `production/epics/round-match-framework/EPIC.md`, `production/epics/session-travel/EPIC.md`, `production/epics/shared-character-locomotion/EPIC.md` (stubs until they exist)
- Unlocks: `production/epics/level-socket-contract/story-029-start-placement-same-function-zone-fill.md`
