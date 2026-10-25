# Story 020: The in-round player array stays frozen through disconnect, rejoin, elimination and informant conversion

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: spike story-002 (S2 travel/rejoin) has not reported, and the participant-state-roster/session-travel rejoin and informant-conversion stories are not built
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

The engine's player array is rebuilt on every client from the PlayerStates it receives, so a PlayerState created, destroyed or deactivated in a round tells every client who just joined, left, was eliminated or turned informant, and an NPC pawn that had a PlayerState would be told apart from a player pawn at once. ADR-0001 therefore freezes the array between `Preparing` and the next travel: a rejoiner re-adopts its retained PlayerState rather than getting a new one. This story proves the freeze on every client through the four events that tempt the engine to change it, and it is the test that makes the retained-PlayerState rejoin design (ADR-0005) verifiable. It extends the S2 verdict (story-002) to production classes.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-003` (UE default overrides of R5 (pawn links, Instigator, possession ForceNetUpdate, RemoteViewPitch16, spectator))
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (sections 4.3, 4.4); also ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3); ADR-0005: Session, seamless travel and state stores (rejoin mechanism)
**ADR Decision Summary**: No PlayerState is created, destroyed, deactivated or renamed between `Preparing` and the next travel; a rejoiner is re-associated with its retained PlayerState. The array reveals only the human count, which is public by design.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0001 E9 [V] (`APlayerState` is always relevant; `PlayerArray` is not replicated and is rebuilt on each client from received PlayerStates); E16 [V] (`SetOwner` marks `Owner` dirty, so re-owning a retained PlayerState to a rejoiner's controller would broadcast unless `Owner` is `COND_OwnerOnly` on `AStealthPlayerState`); E17 [V] (the 5.8.4 PlayerState replicated set; `OnRep_bIsInactive` re-sorts player lists). Seamless travel is disabled in single-process PIE unless `net.AllowPIESeamlessTravel=1`, so the rejoin leg needs multi-process (T3). Open spike: S2 extension to seamless travel and rejoin is story-002.

**Control Manifest Rules (this layer)**:
- Forbidden: **Never change the in-round player array; never enter the engine spectator state** — source: ADR-0001
- Required: **A rejoiner adopts its retained PlayerState in `InitPlayerState`**; `OnDeactivated` does not destroy the PlayerState while admission is `RejoinOnly` — source: ADR-0005
- Forbidden: **Never toggle `bIsInactive` or remove a PlayerState from the array between `Preparing` and the next travel; never spawn a fresh PlayerState for a reclaim** — source: ADR-0005
- Required: **`AStealthPlayerState` replicates only `PublicHandle` and `PlayerNamePrivate`** — source: ADR-0001

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-22** [PIE-3] `StealthGame.Visibility.FrozenPlayerArray`: GIVEN an `Active` round, WHEN a player disconnects, rejoins, is eliminated and becomes an informant, THEN on every client the count, order and names of the player array are unchanged, no PlayerState is created or destroyed, and no NPC pawn has a PlayerState. (R5, L3)

---

## Implementation Notes

*Derived from ADR-0001 sections 4.3, 4.4 and 8 (S2 fallback), ADR-0005 and ADR-0008 sections 1, 3:*

- **What to build.** A test that runs an `Active` round and, after each of the four events (disconnect, rejoin, elimination, informant conversion) and not only at the end, asserts on every client that the player array count, order and names are unchanged, that no PlayerState was created or destroyed, and that no NPC pawn has a PlayerState (NPCs are possessed by the crowd AIController class with `bWantsPlayerState = false`, ADR-0001 section 1).
- **Tiers.** The disconnect, elimination and informant legs can run in-process as T2: `NETWORK_TEST_CLASS(FrozenPlayerArray, "StealthGame.Visibility.PIE")` with `WithGameMode(AStealthGameMode)`. The rejoin leg needs seamless travel and a retained PlayerState across a real reconnect, which single-process PIE cannot represent: it is the T3 scenario `StealthGame.Visibility.MP.FrozenPlayerArray` (Gauntlet, one cooked Development listen host plus client processes, legacy NetDriver asserted).
- **Negative control (added by the story, ADR-0008 section 3).** A harness-only variant that re-creates a PlayerState on rejoin (a fresh PlayerState for the reclaim) must be detected by the same checks; the suite passes only when the control fails the inner assertion. The criterion text stays the GDD's.
- **Fallback awareness.** ADR-0001 section 8 records the S2 fallback paths (pinned-constant re-enable, sanitised ids, a no-`SetOwner` rejoin); if story-002 triggers one, this test's assertions stay the same and only the allowed constant rows change.
- **Prototype disposition.** None promoted; the prototype has no rejoin path and replicates `Team`/`LifeState` on `AStealthPlayerState` at `COND_None` (`Source/StealthGame/Private/StealthPlayerState.cpp:12-13`), replaced by the section 4.3 trims.
- **Spike gates.** story-002 (S2 travel and rejoin). story-019 (possession canary harness) is the in-process base.
- **Unblocks when.** story-002 reports; participant-state-roster delivers rejoin and informant conversion; session-travel delivers seamless-travel and retained-PlayerState rejoin.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-002: the S2 spike on travel and rejoin PlayerState trims
- story-019: possession canary
- story-016: spectator state ban
- story-021: C-TeamView never reaching opponents
- participant-state-roster and session-travel epics: the rejoin and informant mechanisms themselves

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/FrozenPlayerArrayTest.cpp` (`NETWORK_TEST_CLASS(FrozenPlayerArray, "StealthGame.Visibility.PIE")`)
- `Source/StealthTestHarness/Private/Scenarios/` and `Build/Scripts/` (T3 scenario `StealthGame.Visibility.MP.FrozenPlayerArray` for the rejoin leg)
- Harness-only negative-control variant (a PlayerState re-created on rejoin), inside `StealthTestHarness`

---

## QA Test Cases

- **AC-1**: Frozen player array (AC-IVP-22)
  - Given: an `Active` round, on every client.
  - When: a player disconnects, rejoins, is eliminated and becomes an informant.
  - Then: player array count, order and names unchanged; no PlayerState created or destroyed; no NPC pawn has a PlayerState.
  - Edge cases: check after each of the four events, not only at the end; rejoin needs the T3 path (T2 cannot represent seamless travel); negative control (added): a variant that re-creates a PlayerState on rejoin must be detected. (Tier: T2 and T3.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test under `Source/StealthTestHarness/Private/Tests/PIE/` plus the T3 scenario under `Source/StealthTestHarness/Private/Scenarios/` OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-002-spike-s2-seamless-travel-and-rejoin-playerstate-trims.md`, `production/epics/information-visibility-policy/story-019-possession-canary.md`; participant-state-roster epic stories delivering rejoin and informant conversion; session-travel epic stories delivering seamless travel and the retained-PlayerState rejoin (ADR-0005)
- Unlocks: None
