# Story 028: An attack on a culled pawn resolves against capsules identically by spot or id, and the victim re-opens only at the death write

> **Epic**: Information Visibility Policy
> **Status**: Blocked — BLOCKED: [Dep:Assassination] needs the assassination-defence-reversal Attack(TargetSpot/TargetPawn) resolver; spike S15 (blind-attack oracle) has not reported. Update 2026-10-26: the S10 gate is cleared — story-056 (S10 fix + re-run) reported **PASS** (`production/qa/spikes/2026-10-26_S10-rerun.md`: re-open guard with `ReopenAckTimeoutMs` 350, lost-pawn repair, warm-up and the ADR-0002 clock budget all met; S9 PASS 2026-10-25).
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: L
> **Manifest Version**: 2026-10-25e
> **Last Updated**: 2026-10-26

## Context

A culled pawn does not exist on a non-ally client, yet an attacker can still strike where it stands. If the attack path behaved differently for a culled target (a rejected id, a different reply, a prompt, an early re-open in the outcome slot), the attacker would learn that something hidden is there, which defeats concealment. This story proves the blind-spot attack path: an attack on the culled pawn's capsule, as `Attack(TargetSpot)` and separately as `Attack(TargetPawn)` with its id and the same `WorldPoint`, resolves on the server against capsules with identical replies and timing class and no prompt beforehand, and the victim re-opens only at the death write (carrying `bDiedConcealed` when killed while concealed) and is never rendered on any non-owner. It is the integration test of ADR-0003 section 5.7 and of the user's 2026-10-25 killed-concealed decision.

**GDD**: `design/gdd/information-visibility-policy.md`
**Requirement**: `TR-ivp-004` (Concealment hybrid: quantised server cull for non-allies + client presentation hide)
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0003: Replication, relevancy and the concealment cull hybrid (sections 5.7, 5.8, 4); also ADR-0008: Test and automation strategy for leak and parity tests (sections 1, 3, 5); ADR-0002: Resolution queue and round-end staging (section 8, the death write staging pass)
**ADR Decision Summary**: Nothing re-opens in the strike's outcome slot. A victim culled for a reason other than concealment re-opens at Assassination's death write (staging pass), already falling and carrying `DeathServerTime`; a victim killed while concealed returns to `Visible` at the death write too, carrying `FDeathState.bDiedConcealed = true` so every non-owner client keeps the body presentation-hidden.
**ADR Version**: 2026-10-24

**Engine**: Unreal Engine 5.8 (installed 5.8.4) | **Risk**: HIGH
**Engine Notes**: ADR-0003 E4 [V]: a legacy forced update does not change relevancy, so a death write's forced update on a culled victim does not re-open it (re-open is the un-cull in the same server step); E10 [V] / [U, S10]: in-flight cull/un-cull race; E11 [V]/[U, S10]: re-open creates a new client actor, NetGUID reuse and AnimBP initialisation unknown; E12 [U]: fixed-width `NetSerialize` byte identity (the byte-capture spike, story-008). Open spikes: S9 (story-004), S10 (story-005), and S15 (the blind-attack oracle, owned by assassination-defence-reversal).

**Control Manifest Rules (this layer)**:
- Forbidden: **Never re-open a culled pawn in a strike's outcome slot; a killed-concealed victim re-opens only at the death write, with `bDiedConcealed = true`** — source: ADR-0003
- Required: **The concealment cull lands on the global boundary grid and closes the channel explicitly per denied connection in the cull tick**; un-cull happens in the same server step as the state change — source: ADR-0003
- Forbidden: **Never skip `FActionState.StartServerTime` in an `FActionState` write, and never send a feed entry without advancing its `Seq`** — source: ADR-0003, ADR-0017, ADR-0018
- Required: **Outcome-bearing replicated structs serialise to a fixed bit width** — source: ADR-0003

---

## Acceptance Criteria

*From GDD `design/gdd/information-visibility-policy.md`, scoped to this story:*

- [ ] **AC-IVP-13** [PIE-2][S9][S15][Dep:Assassination] `StealthGame.Visibility.BlindSpotAttackOnCulled`: GIVEN a pawn culled for the attacker, WHEN the attacker sends `Attack(TargetSpot)` at its capsule, and separately `Attack(TargetPawn)` with its id and the same `WorldPoint`, THEN both resolve on the server against capsules and kill it, with identical reply codes and timing class and no prompt beforehand; no non-ally connection receives any bunch for the victim in the outcome slot; a victim culled for a non-concealment reason re-opens already falling at the death write (`ContactServerTime − StrikeNetLeadSec`, ±1 tick) with the death time set in that step, and a victim killed while concealed re-opens at the same death write carrying `FDeathState.bDiedConcealed` = true and is never rendered on any non-owner client (user decision 2026-10-25). (R7.4, R7.5) Clarification (story-level, QL-STORY-READY): "never rendered on a non-owner" means on every non-owner client the victim's primitives are not registered with the scene or are hidden (component assertion) plus a retained screenshot; "no prompt beforehand" means no outbound reply or RPC to the attacker before the resolver reply.

---

## Implementation Notes

*Derived from ADR-0003 sections 5.7, 5.8, 4 and 2, ADR-0002 section 8 (staging) and ADR-0008 sections 1 and 3:*

- **What to build.** An integration test (PIE with at least 2 clients) with a pawn culled for the attacker. Run both attack forms for both victim kinds (culled for a non-concealment reason; killed while concealed): `Attack(TargetSpot)` at the capsule, and `Attack(TargetPawn)` with the victim's id and the same `WorldPoint`. Assert on the server that both resolve against capsules and kill the victim; that the reply codes and timing class are identical across the two forms; that no outbound reply or RPC reaches the attacker before the resolver reply; and that no non-ally connection receives a bunch for the victim in the outcome slot.
- **Re-open at the death write.** For a victim culled for a non-concealment reason: the re-open happens already falling at the death write, at `ContactServerTime - StrikeNetLeadSec` (±1 tick), with the death time set in that step (`DeathServerTime`) so clients fast-forward. For a victim killed while concealed: Concealment pushes `bConcealed = false` in the staging pass in the same server step, and the victim re-opens at the same death write carrying `FDeathState.bDiedConcealed = true`.
- **Never rendered on a non-owner.** On every non-owner client assert by component check that the victim's primitives are not registered with the scene or are hidden, and retain a screenshot (evidence under `production/qa/evidence/`). The interim `KilledConcealed` cull reason is retired (user decision 2026-10-25); do not use it.
- **Placement.** T2: `NETWORK_TEST_CLASS(BlindSpotAttackOnCulled, "StealthGame.Visibility.PIE")`, `WithGameMode(AStealthGameMode)`; timing assertions in server time and frame bounds only (ADR-0008 section 4). Rendering assertions are component checks; the screenshot is evidence, not a T2 predicate.
- **Prototype disposition.** Replaced, never promoted: `bConcealed` replication and `UpdateConcealmentVisibility` (`Source/StealthGame/Private/StealthCharacter.cpp:63-64`); the prototype has no blind-attack resolver.
- **Spike gates.** story-004 (S9), story-005 (S10), and S15 (blind-attack oracle, owned by assassination-defence-reversal); no story file exists yet for S15.
- **Cross-epic.** assassination-defence-reversal delivers the `Attack(TargetSpot)` / `Attack(TargetPawn)` resolver and the death write; input-interaction-framework delivers reply codes; concealment-bushes delivers `bDiedConcealed` handling; npc-kill-penalty-informants delivers kill resolution.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- story-024: cull audience
- story-029: un-cull lead bound
- story-038: kill-confirmation parity across NPC, Active opponent and informant (AC-IVP-14)
- story-052: strike-id oracle parity across id classes (AC-IVP-39)
- story-005: S10 re-open behaviour spike
- assassination-defence-reversal epic: the resolver and death write mechanisms

---

## Files

*The code and test files this story creates or changes. `/create-stories` lists the planned ones; `/dev-story` replaces the list with the files it changed; `/story-done` checks scope against it.*

- `Source/StealthTestHarness/Private/Tests/PIE/BlindSpotAttackOnCulledTest.cpp` (`NETWORK_TEST_CLASS(BlindSpotAttackOnCulled, "StealthGame.Visibility.PIE")`)
- `production/qa/evidence/` retained screenshot of the victim not rendered on a non-owner client

---

## QA Test Cases

- **AC-1**: Blind-spot attack on a culled pawn (AC-IVP-13)
  - Given: a pawn culled for the attacker, PIE with ≥ 2 clients.
  - When: the attacker sends `Attack(TargetSpot)` at its capsule, and separately `Attack(TargetPawn)` with its id and the same `WorldPoint`.
  - Then: both resolve server-side against capsules and kill it, with identical reply codes and timing class; no outbound reply or RPC reaches the attacker before the resolver reply; no non-ally connection receives a bunch for the victim in the outcome slot; a victim culled for a non-concealment reason re-opens already falling at the death write (`ContactServerTime - StrikeNetLeadSec`, ±1 tick); a victim killed while concealed re-opens at the death write with `bDiedConcealed = true`; on every non-owner the victim's primitives are unregistered or hidden, and a screenshot is retained.
  - Edge cases: depends on S9, S10 and S15; run both attack forms for both victim kinds. (Tier: T2.)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Integration: a T2 test at `Source/StealthTestHarness/Private/Tests/PIE/BlindSpotAttackOnCulledTest.cpp` plus a retained screenshot in `production/qa/evidence/`, OR a documented playtest/spike report

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: `production/epics/information-visibility-policy/story-004-spike-s9-cull-latency-and-explicit-channel-close.md`, `production/epics/information-visibility-policy/story-005-spike-s10-reopen-behaviour-and-netguid.md`, `production/epics/information-visibility-policy/story-024-concealment-cull-audience.md`; assassination-defence-reversal epic story delivering the `Attack(TargetSpot/TargetPawn)` resolver and the death write, and its S15 blind-attack oracle spike; concealment-bushes epic (`bDiedConcealed` handling)
- Unlocks: None
