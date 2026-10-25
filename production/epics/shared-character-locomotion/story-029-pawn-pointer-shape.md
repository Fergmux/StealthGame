# Story 029: NPC and player pawns have the same Owner, Controller and PlayerState shape on the wire

> **Epic**: Shared Character & Locomotion
> **Status**: Ready
> **Layer**: Core
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: [set by /dev-story when implementation begins]

## Context

**GDD**: `design/gdd/shared-character-locomotion.md` (R3 Possession parity; AC-SCL-17; Roster AC-PSR-39/45; Session 25d; OQ1)
**Requirement**: `TR-scl-003`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (section 4: owner-only pointers `Owner`, `Controller`, `PlayerState`; `COND_OwnerOnly` on all three), with ADR-0003 section 6
**ADR Decision Summary**: No non-owner connection ever receives the pawn's `Owner`, `Controller` or `PlayerState`, for player or NPC pawns, across spawn, restart, takeover, AI hand-over and kill; the three are owner-only conditions, and the owner-local stale-pointer cleanup handles the previous owner's former body.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: Spike **S1 reported 2026-10-25** (`production/qa/spikes/2026-10-25_S1-S2-S14.md`): PASS for the three pointers under trims 7 (0 non-owner receipts), FAIL for whole-pawn parity (`RemoteViewPitch16` and the sample's `CharacterInputState` are differentiators, handled by stories 022 and 032). S14 PASS for the prototype's possession paths, but rejoin, travel and `PawnRecovery` were not testable and still re-run when the code exists. The previous owner's former body keeps `PlayerState` and `Owner` on that client with `IsPlayerControlled()` true (ADR-0001 section 4.1 owner-local cleanup keeps it from being observable).

**Control Manifest Rules (this layer)**:
- Required: owner-only pointers on every pawn; the audit enumerates `GetLifetimeReplicatedProps` through each class's rep layout.
- Forbidden: no client code derives player-ness from these pointers; never read another pawn's `PlayerState`, `Controller` or `Owner` on a client.
- Guardrail: none.

---

## Acceptance Criteria

*From GDD `design/gdd/shared-character-locomotion.md`, scoped to this story:*

- [ ] **AC-SCL-17** [N][S1] `PawnPointerShape`: GIVEN a modified client dumping raw received bunches, WHEN it observes NPC and player pawns for 60 s including a disconnect handover, THEN `Owner`, `Controller` and `PlayerState` are null or identical in kind for both, with no unresolvable NetGUID on either (Roster AC-PSR-39/45, Session 25d).

*QA clarification (create-stories 2026-10-25, not a GDD change):* Pass means 0 receipts of `Owner`, `Controller` and `PlayerState` on any non-owner connection (the S1 result), and, for the modified client that previously owned a handed-over body, the former body's three pointers are null and `IsPlayerControlled()` is false on that client after the handover (the stale state spike S14 confirmed). Both checks are required.

---

## Implementation Notes

*Derived from ADR-0001 section 4 and the S1/S14 spike report:*

- Apply the owner-only conditions through the by-name reset in `AStealthPawn::GetLifetimeReplicatedProps` (`DOREPLIFETIME_CONDITION` with `COND_OwnerOnly`) for `Owner`, `Controller` and `PlayerState`, as the trims-7 configuration in the spike did, and keep the owner-local cleanup for the previous owner's former body. `bShouldUpdateReplicatedPing = false` goes in the `AStealthPlayerState` constructor (the setter is protected, spike section 6 item 1; the PlayerState class belongs to the Roster epic).
- The test is a harness raw-bunch capture over 60 s that includes the disconnect handover of story 026; reuse the spike analysers in `production/qa/spikes/2026-10-25_tools/` as a starting point and the `stealth.Spike.RepMap` readback in `Source/StealthGame/Private/Spikes/StealthIdentitySpike.cpp` for the boot assertion form.
- **Prototype replaced**: the prototype's `AStealthCharacter` replicates all three pointers to non-owners (R0 baseline in the spike); the spike's trims configuration is the production behaviour.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 032: the full replicated-property allowlist (whole parent set).
- Story 026: the disconnect hook (consumed here).
- Rejoin and travel pointer cases (Session & Travel and Roster epics re-run S14).

---

## Files

- `Source/StealthGameplay/Private/Core/SharedPawn/StealthPawn.cpp` (`GetLifetimeReplicatedProps` conditions)
- `Source/StealthTestHarness/Private/Locomotion/PawnPointerShapeTest.cpp` (planned)

---

## QA Test Cases

*QL-STORY-READY: ADEQUATE (after clarification). Test `StealthGame.Locomotion.PIE.PawnPointerShape.NpcVsPlayer`.*

Test: pawn pointer shape (AC-SCL-17, spike S1)
  Given: a modified client dumping raw received bunches, observing NPC and player pawns for 60 s including a disconnect handover
  When: the dump is analysed and the former body's client-side state is read on the modified client that previously owned a handed-over body
  Then: `Owner`, `Controller` and `PlayerState` are received 0 times on any non-owner connection for both kinds; no unresolvable NetGUID on either; the former body's three pointers are null and `IsPlayerControlled()` is false on that client after the handover
  Edge cases: needs the harness raw-bunch hook; Iris is off

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Locomotion/PawnPointerShapeTest.cpp` — must exist and pass (multi-client PIE, modified-client dump); not player-observable

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/shared-character-locomotion/story-008-spawn-bunch-parity.md`, `story-026-disconnect-keeps-pawn-under-crowd-ai.md`; `production/epics/information-visibility-policy/EPIC.md`, `production/epics/participant-state-roster/EPIC.md`
- Unlocks: `production/epics/shared-character-locomotion/story-032-replicated-allowlist.md`
