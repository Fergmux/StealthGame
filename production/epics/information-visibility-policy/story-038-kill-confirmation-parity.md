# Story 038: Kill confirmation reaches only the attacker and the fall, removal and replacement sequence is identical for NPC, Active and informant victims

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Assassination][Dep:Penalty] needs assassination-defence-reversal and npc-kill-penalty-informants kill resolution built.
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: M
> **Manifest Version**: 2026-10-25e
> **Last Updated**: (set by /dev-story when implementation begins)

## Context

After a successful strike the attacker learns what they killed (an NPC, an opposing player or an opposing informant); that `VictimKind` is the whole point of the wrong-target penalty and must reach nobody else. Everyone else must see the same fall, the same removal and the same replacement for all three victim kinds, and an informant kill must present to the attacker as an `Npc`. If any connection other than the attacker's PlayerController received a kind-dependent field, or if the observable sequence differed by kind, a bystander could tell players from NPCs by watching kills. This story proves that parity end to end in a multi-client run.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-001` (`CanViewerKnow` server query, default deny, seven viewer classes)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0001: Crowd identity rules (sections 1, 4.2 and 5, owner carrier); ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.7 and 6)
**ADR Decision Summary**: Owner-audience data travels only on the owner's PlayerController (an owner reply), never as a gameplay field on the pawn; a pawn carries only `All` facts, identical for every pawn kind. A victim's death presentation (`FDeathState`) is identical for every victim kind and carries no kind-dependent field.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: A PlayerController is relevant only to its owner (ADR-0001 E6) [V], which is why an owner reply reaches exactly one connection. `FDeathState` is a fixed-width serialised struct whose bit identity is E12 [U], owned by the fixed-width byte-capture spike (story-008). The legacy NetDriver is asserted by the harness.

**Control Manifest Rules (this layer)**:
- Required: **Team-audience data travels only in a `UTeamViewComponentBase` subclass on the receiving teammate's own PlayerController; owner-audience data only on the owner's PlayerController** — source: ADR-0001
- Required: **Outcome-bearing replicated structs serialise to a fixed bit width** — source: ADR-0003
- Forbidden: **Never branch on `IsPlayerControlled()`, `IsBotControlled()`, controller class, `PlayerState` presence, team or Roster state** in pawn, component, AnimBP or audio code for anything another client can observe — source: ADR-0001
- Forbidden: **Never re-open a culled pawn in a strike's outcome slot; a killed-concealed victim re-opens only at the death write, with `bDiedConcealed = true`** — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md` (AC-IVP-14), scoped to this story:*

- [ ] **AC-IVP-14** [PIE-3][Dep:Assassination][Dep:Penalty] `StealthGame.Visibility.KillConfirmationParity`: GIVEN kills of an NPC, an Active opponent and an opponent informant, WHEN each resolves, THEN only the attacker's PlayerController receives `VictimKind`, the informant kill presents as `Npc`, no other connection receives a kind-dependent field, and the fall, removal and replacement sequence is identical for all three. (R3, R9b, L10)

---

## Implementation Notes

*Derived from ADR-0001 sections 1, 4.2, 5 and ADR-0003 sections 5.7, 6, with ADR-0008 sections 2-3:*

- Three kills in one fixture (an NPC, an Active opponent, an opponent informant), each resolved through the real strike path of assassination-defence-reversal and npc-kill-penalty-informants. For each, record per connection every replicated datum received in the outcome and replacement window: the attacker's PlayerController must receive `VictimKind` (owner reply, never on the pawn); the informant kill presents as `Npc` (the informant is a crowd body to the attacker); no other connection (owner of the victim, allies, opponents, bystander clients) receives any kind-dependent field.
- "Identical" is defined as the same event order with server timestamps equal within +-1 server tick (QL-STORY-READY note; the author confirms the tolerance with qa-lead before implementation). Compare the fall, removal and replacement sequence across the three victim kinds on a non-attacker client; the `FDeathState`/`FActionState` writes, removal timing and replacement spawn must match in order and timing class.
- Negative control (added by the story, ADR-0008 section 3): a harness-only variant that sends `VictimKind` to a bystander must be detected by the suite; the suite passes only if the control is detected. The control type lives only in `StealthTestHarness`.
- Use the R3 matrix rows for the kill-related facts (story-011 encodes them: `VictimKind` is an Owner-audience reply fact) and story-009's viewer classes for who counts as attacker, bystander and victim owner. A killed-concealed victim and a victim culled for another reason follow ADR-0003 section 5.7 (re-open only at the death write); include them as edge cases of the sequence comparison only if the Assassination resolver is available.
- T2 test: `NETWORK_TEST_CLASS(KillConfirmationParity, "StealthGame.Visibility.PIE")` with `WithGameMode(AStealthGameMode)`, at least 3 clients, a fixture map, bound by server time and frames; record the active replication system (legacy).
- Prototype: Replaces: nothing directly (the prototype's `AStealthCharacter::IsNpc()` kind detection, `PlayerState == null`, is replaced by story-009 and the pawn work in the SCL epic). Promotes: nothing.
- Cross-epic: needs assassination-defence-reversal's strike resolver, death write and `FDeathState` (ADR-0018); npc-kill-penalty-informants' kill resolution, informant presentation and replacement flow.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-009 and story-011: the viewer classes and the R3 fact table (`VictimKind` row) this story relies on
- story-036: the post-round latch release
- story-052: strike-id oracle parity (`Attack(TargetPawn)` against `Attack(TargetSpot)`)
- story-008: spike for the `FDeathState` byte identity (E12)
- assassination-defence-reversal and npc-kill-penalty-informants epics: the kill resolution and informant flows

---

## Files

*The code and test files this story creates or changes (planned):*

- `Source/StealthTestHarness/Private/Tests/PIE/KillConfirmationParityTest.cpp` (T2, `NETWORK_TEST_CLASS(KillConfirmationParity, "StealthGame.Visibility.PIE")`, plus the harness-only `VictimKind`-to-bystander control)

*Note: the `StealthTestHarness` module folder is created by tuning-data-config Exit A (it does not exist yet).*

---

## QA Test Cases

- **AC-1**: Kill confirmation parity (AC-IVP-14)
  - Given: kills of an NPC, an Active opponent and an opponent informant.
  - When: each resolves.
  - Then: only the attacker's PlayerController receives `VictimKind`; the informant kill presents as `Npc`; no other connection receives a kind-dependent field; the fall, removal and replacement sequence is identical for all three.
  - Edge cases: define "identical" as the same event order with server timestamps equal within +-1 tick (author to confirm); negative control (added): a variant that sends `VictimKind` to a bystander must be detected.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: `Source/StealthTestHarness/Private/Tests/PIE/KillConfirmationParityTest.cpp` (T2 test, must exist and pass) OR a documented playtest or spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-009-viewer-class-invariants.md`, `production/epics/information-visibility-policy/story-011-matrix-per-fact.md`; assassination-defence-reversal epic (strike resolver, death write, `FDeathState`); npc-kill-penalty-informants epic (kill resolution, informant presentation)
- Unlocks: None
