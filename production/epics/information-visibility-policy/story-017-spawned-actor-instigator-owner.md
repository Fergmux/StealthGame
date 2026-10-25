# Story 017: Every pawn spawn path sets Instigator to Self and no replicated spawned actor carries a replicated Owner or Instigator

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: needs UStealthPawnFactory and every pawn spawn path (round start, NPC replacement, takeover, cull re-open) from shared-character-locomotion, round-match-framework and npc-kill-penalty-informants; not a spike
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

An actor's `Owner` and `Instigator` replicate by default (ADR-0001 E3) and export NetGUIDs whose resolvability can differ by controller kind, so a replicated child actor or effect that carries either one can tell an observer who controls a pawn (leak L15). The pawn factory is therefore required to give every pawn `Instigator == Self` through every spawn path and to spawn no replicated child actor carrying `Owner` or `Instigator`. This story is the test that proves it across all four spawn paths and across every replicated effect or projectile class, so a new spawn path or a new effect class cannot reintroduce the leak. It feeds the leak-catalogue L15 row.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator))
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 1, pawn factory); also ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.8, 8); ADR-0008: Test and automation strategy for leak and parity tests
**ADR Decision Summary**: Pawn creation goes only through the pawn factory (`UStealthPawnFactory::SpawnSharedPawn` / `TakeOverCrowdPawn`); every spawn path is identical for both kinds, asserts `Instigator == Self`, and spawns no replicated child actor carrying `Owner` or `Instigator` (ADR-0001 section 1, IVP R5).
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 E3 [V]: `AActor::Owner` and `Instigator` replicate push-based with no condition (`ActorReplication.cpp:563-569`); E16 [V]: `SetOwner` marks `Owner` dirty. ADR-0003 section 5.8 and E11: a cull re-open creates a new client actor, which is why the cull re-open is one of the four spawn paths under test. No unreported spike owns a claim here.

**Control Manifest Rules (this layer)**:
- Required: **Every body is an `AStealthPawn`**: one class, one component set, no per-instance replication or net-config difference — source: ADR-0001
- Required: **`HandleStartingNewPlayer` is a no-op; every pawn comes from `UStealthPawnFactory`** — source: ADR-0005, ADR-0010
- Forbidden: **Never call `RestartPlayer` or `SpawnDefaultPawnFor`** — source: ADR-0005, ADR-0010
- Forbidden: **Never read `Owner` or `PlayerState` on a pawn the owner does not currently possess** — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-06** [Int] `StealthGame.Visibility.SpawnedActorInstigatorOwner`: GIVEN every pawn spawn path (round start, NPC replacement, takeover, cull re-open) and every replicated spawned effect or projectile class, WHEN spawned in a test world, THEN `Instigator == Self` on every pawn and no replicated spawned actor carries a non-null replicated `Owner` or `Instigator`. (R5, L15)

---

## Implementation Notes

*Derived from ADR-0001 section 1 and ADR-0008 sections 1 and 2:*

- **What to build.** A test that spawns a pawn through each of the four paths (round start, NPC replacement, takeover, cull re-open) in a test world and asserts `Instigator == Self`; and a reflection-driven sweep of every replicating `AActor` subclass that can be spawned as an effect or projectile, spawning one of each and asserting that its replicated `Owner` and `Instigator` are null. "Every class" comes from reflection discovery of replicating `AActor` subclasses, not a hand list, so a new effect class is covered automatically.
- **Placement.** Test name `StealthGame.Visibility.SpawnedActorInstigatorOwner` (QA tier T1/T2): the reflection sweep is a T1 test in `Source/StealthFoundation/Private/Tests/Visibility/`; the spawn-path half needs a test world with the factory and is a harness test under `Source/StealthTestHarness/`. Decide the exact split when the factory exists; if the spawn-path half becomes a CQTest, ADR-0008 section 2 names it `NETWORK_TEST_CLASS(SpawnedActorInstigatorOwner, "StealthGame.Visibility.PIE")`.
- **Prototype disposition.** Replaced, never promoted: the prototype spawns characters without a factory and with the engine's default `Instigator`/`Owner` handling (`Source/StealthGame/Private/StealthCharacter.cpp`); the production factory replaces it.
- **Spike gates.** None. The cull re-open path depends on story-024's cull, but the `Instigator == Self` assertion on a re-opened (newly created) client actor is made on the server pawn.
- **Unblocks when.** shared-character-locomotion delivers `UStealthPawnFactory` with `SpawnSharedPawn`/`TakeOverCrowdPawn`; round-match-framework delivers the round start spawn path; npc-kill-penalty-informants delivers NPC replacement and takeover.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-018: effective replication settings on pawn defaults (AC-IVP-09)
- story-019: possession canary (changelist absence of the three pointers)
- story-015: reflection audit over the allowlist (the allowlist separately forbids replicated attachments)
- shared-character-locomotion epic: the factory itself

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthFoundation/Private/Tests/Visibility/SpawnedActorInstigatorOwnerTest.cpp` (class `FVisibilitySpawnedActorInstigatorOwnerTest`, test `StealthGame.Visibility.SpawnedActorInstigatorOwner`; reflection sweep)
- `Source/StealthTestHarness/Private/Tests/PIE/` (spawn-path half in a test world, if it needs the replicated world)

---

## QA Test Cases

- **AC-1**: Spawned actor Instigator and Owner (AC-IVP-06)
  - Given: every pawn spawn path (round start, NPC replacement, takeover, cull re-open) and every replicated spawned effect or projectile class, in a test world.
  - When: each is spawned.
  - Then: `Instigator == Self` on every pawn; no replicated spawned actor carries a non-null replicated `Owner` or `Instigator`.
  - Edge cases: "every class" comes from reflection discovery of replicating `AActor` subclasses, not a hand list. (Tier: T1/T2.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test under `Source/StealthTestHarness/` (with the T1 reflection sweep under `Source/StealthFoundation/Private/Tests/Visibility/`) OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: shared-character-locomotion epic story delivering `UStealthPawnFactory`; round-match-framework epic story delivering the round-start spawn path; npc-kill-penalty-informants epic story delivering NPC replacement and takeover; tuning-data-config Exit A (harness)
- Unlocks: None
